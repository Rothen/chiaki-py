#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "nv12_to_rgb_ptx.h"

// Constants for converting YCbCr to RGB, in the form the conversion kernel
// (pybind/src/cuda/nv12_to_rgb.cl) takes them. With samples in their raw integer
// units: luma = (Y - y_offset) * y_scale, cb/cr = (C - c_mid) * c_scale, then
// R = luma + rv * cr, G = luma - gu * cb - gv * cr, B = luma + bu * cb.
struct YuvToRgbParams
{
    float y_offset;
    float y_scale;
    float c_mid;
    float c_scale;
    float rv;
    float gu;
    float gv;
    float bu;

    // `kr` and `kb` are the luma weights of the colour matrix (BT.709: 0.2126 and
    // 0.0722). `bytes_per_sample` is 1 for 8-bit video and 2 for 10/16-bit video
    // stored in 16 bits (P010/P016), where every level is scaled up by 2^8.
    static YuvToRgbParams make(double kr, double kb, bool full_range, int bytes_per_sample)
    {
        const double kg = 1.0 - kr - kb;
        const double unit = bytes_per_sample == 2 ? 256.0 : 1.0;

        YuvToRgbParams p;
        p.y_offset = static_cast<float>(full_range ? 0.0 : 16.0 * unit);
        p.y_scale = static_cast<float>(1.0 / ((full_range ? 255.0 : 219.0) * unit));
        p.c_mid = static_cast<float>(128.0 * unit);
        p.c_scale = static_cast<float>(1.0 / ((full_range ? 255.0 : 224.0) * unit));
        p.rv = static_cast<float>(2.0 * (1.0 - kr));
        p.bu = static_cast<float>(2.0 * (1.0 - kb));
        p.gv = static_cast<float>(2.0 * kr * (1.0 - kr) / kg);
        p.gu = static_cast<float>(2.0 * kb * (1.0 - kb) / kg);
        return p;
    }
};

// The few CUDA driver API entry points needed to convert a decoded frame to RGB
// into a caller-supplied CUDA array. They are loaded at runtime rather than
// linked, and the conversion kernel is shipped as PTX for the driver to
// JIT-compile, so the module builds without the CUDA toolkit and still imports on
// machines without an NVIDIA driver.
class CudaDriver
{
public:
    using CUresult = int;
    using CUcontext = void *;
    using CUmodule = void *;
    using CUfunction = void *;
    using CUdeviceptr = unsigned long long;

    static const CudaDriver &get()
    {
        static const CudaDriver driver;
        return driver;
    }

    bool available() const
    {
        return ctx_push && ctx_pop && stream_synchronize && module_load_data && module_get_function && launch_kernel &&
               init && device_get && primary_ctx_retain;
    }

    // The primary context of `device`, retained for the rest of the process: the
    // context PyTorch and CuPy work in, so it is the one to create FFmpeg's CUDA
    // device in for its frames to be usable from them.
    CUcontext primary_context(int device = 0) const
    {
        if (!available())
            throw std::runtime_error("CUDA driver library (nvcuda.dll / libcuda.so.1) not found");

        std::lock_guard<std::mutex> lock(kernels_mutex);
        const auto found = primary_contexts.find(device);
        if (found != primary_contexts.end())
            return found->second;

        check(init(0), "cuInit");
        int handle;
        check(device_get(&handle, device), "cuDeviceGet");
        CUcontext context;
        check(primary_ctx_retain(&context, handle), "cuDevicePrimaryCtxRetain");
        primary_contexts[device] = context;
        return context;
    }

    // Makes `ctx` the calling thread's current context until it goes out of scope.
    class ScopedContext
    {
    public:
        ScopedContext(const CudaDriver &driver, CUcontext ctx) : driver(driver)
        {
            driver.check(driver.ctx_push(ctx), "cuCtxPushCurrent");
        }
        ~ScopedContext()
        {
            CUcontext popped;
            driver.ctx_pop(&popped);
        }
        ScopedContext(const ScopedContext &) = delete;
        ScopedContext &operator=(const ScopedContext &) = delete;

    private:
        const CudaDriver &driver;
    };

    // Converts a semi-planar 4:2:0 frame - a Y plane followed by an interleaved
    // UV plane, as NV12 (`bytes_per_sample` 1) and P010/P016 (2) are - to packed
    // RGB24 in `dst`, which must have room for `height` rows of `width * 3` bytes.
    // Runs in `ctx` and returns once the conversion has finished (so the source can
    // be released straight away); it is ordered with work on the legacy default
    // stream, other streams have to be synchronised by the caller.
    void nv12_to_rgb(CUcontext ctx, uintptr_t y, size_t y_pitch, uintptr_t uv, size_t uv_pitch, uintptr_t dst,
                     unsigned width, unsigned height, unsigned bytes_per_sample, const YuvToRgbParams &color) const
    {
        if (!available())
            throw std::runtime_error("CUDA driver library (nvcuda.dll / libcuda.so.1) not found");

        const ScopedContext scope(*this, ctx);
        const CUfunction kernel = kernel_for(ctx);

        CUdeviceptr y_ptr = y, uv_ptr = uv, dst_ptr = dst;
        unsigned y_pitch32 = static_cast<unsigned>(y_pitch), uv_pitch32 = static_cast<unsigned>(uv_pitch);
        YuvToRgbParams c = color;
        void *params[] = {&y_ptr, &uv_ptr, &dst_ptr, &y_pitch32, &uv_pitch32, &width, &height, &bytes_per_sample,
                          &c.y_offset, &c.y_scale, &c.c_mid, &c.c_scale, &c.rv, &c.gu, &c.gv, &c.bu};

        constexpr unsigned kBlock = 16; // any size would do: the kernel bounds-checks every thread
        check(launch_kernel(kernel, (width + kBlock - 1) / kBlock, (height + kBlock - 1) / kBlock, 1, kBlock, kBlock, 1,
                            0, nullptr, params, nullptr),
              "cuLaunchKernel");
        check(stream_synchronize(nullptr), "cuStreamSynchronize");
    }

private:
    CUresult (*ctx_push)(CUcontext) = nullptr;
    CUresult (*ctx_pop)(CUcontext *) = nullptr;
    CUresult (*stream_synchronize)(void *) = nullptr;
    CUresult (*module_load_data)(CUmodule *, const void *) = nullptr;
    CUresult (*module_get_function)(CUfunction *, CUmodule, const char *) = nullptr;
    CUresult (*launch_kernel)(CUfunction, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, void *,
                              void **, void **) = nullptr;
    CUresult (*get_error_name)(CUresult, const char **) = nullptr;
    CUresult (*init)(unsigned) = nullptr;
    CUresult (*device_get)(int *, int) = nullptr;
    CUresult (*primary_ctx_retain)(CUcontext *, int) = nullptr;

    // A module belongs to the context it was loaded in, so each gets its own.
    mutable std::map<CUcontext, CUfunction> kernels;
    mutable std::map<int, CUcontext> primary_contexts;
    mutable std::mutex kernels_mutex;

    CudaDriver()
    {
#ifdef _WIN32
        HMODULE library = LoadLibraryA("nvcuda.dll");
        auto load = [library](const char *name) -> void * {
            return library ? reinterpret_cast<void *>(GetProcAddress(library, name)) : nullptr;
        };
#else
        void *library = dlopen("libcuda.so.1", RTLD_NOW);
        auto load = [library](const char *name) -> void * { return library ? dlsym(library, name) : nullptr; };
#endif
        ctx_push = reinterpret_cast<decltype(ctx_push)>(load("cuCtxPushCurrent_v2"));
        ctx_pop = reinterpret_cast<decltype(ctx_pop)>(load("cuCtxPopCurrent_v2"));
        stream_synchronize = reinterpret_cast<decltype(stream_synchronize)>(load("cuStreamSynchronize"));
        module_load_data = reinterpret_cast<decltype(module_load_data)>(load("cuModuleLoadData"));
        module_get_function = reinterpret_cast<decltype(module_get_function)>(load("cuModuleGetFunction"));
        launch_kernel = reinterpret_cast<decltype(launch_kernel)>(load("cuLaunchKernel"));
        get_error_name = reinterpret_cast<decltype(get_error_name)>(load("cuGetErrorName"));
        init = reinterpret_cast<decltype(init)>(load("cuInit"));
        device_get = reinterpret_cast<decltype(device_get)>(load("cuDeviceGet"));
        primary_ctx_retain = reinterpret_cast<decltype(primary_ctx_retain)>(load("cuDevicePrimaryCtxRetain"));
    }

    void check(CUresult result, const char *call) const
    {
        if (result == 0)
            return;
        const char *name = nullptr;
        if (get_error_name)
            get_error_name(result, &name);
        throw std::runtime_error(std::string(call) + " failed: " + (name ? std::string(name) : "CUDA error " + std::to_string(result)));
    }

    // Must be called with `ctx` current. The first call JIT-compiles the PTX.
    CUfunction kernel_for(CUcontext ctx) const
    {
        std::lock_guard<std::mutex> lock(kernels_mutex);
        const auto found = kernels.find(ctx);
        if (found != kernels.end())
            return found->second;

        CUmodule module;
        check(module_load_data(&module, kNv12ToRgbPtx), "cuModuleLoadData (colour conversion kernel)");
        CUfunction function;
        check(module_get_function(&function, module, "nv12_to_rgb"), "cuModuleGetFunction");
        kernels[ctx] = function;
        return function;
    }
};
