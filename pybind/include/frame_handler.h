#ifndef CHIAKI_PY_FRAME_HANDLER_H
#define CHIAKI_PY_FRAME_HANDLER_H

#include <time.h>
#include "core/common.h"
#include "core/audio.h"
#include "core/base64.h"
#include "core/bitstream.h"
#include "core/controller.h"
#include "core/ecdh.h"
#include "core/fec.h"
#include "core/feedback.h"
#include "core/log.h"
#include "event_source.h"
#include "settings.h"
#include "streamsession.h"
#include "discovery_manager.h"
#include "backend.h"
#include "cuda_driver.h"

#include <stdlib.h>
#include <stdio.h>
#include <optional>
#include <stdexcept>
#include <string>

#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

namespace py = pybind11;
struct ThreadSwsContext
{
    SwsContext *ctx = nullptr;
    ~ThreadSwsContext();
};

struct VulkanFrame
{
    AVFrame *frame = nullptr;
    double pts = 0.0;
    double duration = 0.0;

    VulkanFrame();
    VulkanFrame(AVFrame *frame, double pts, double duration);
    VulkanFrame(const VulkanFrame &) = delete;
    VulkanFrame &operator=(const VulkanFrame &) = delete;
    ~VulkanFrame();

    void reset(AVFrame *new_frame, double new_pts, double new_duration);

    // Throws if `frame` is null (a default-constructed VulkanFrame that nothing has reset() into yet):
    // every accessor below needs this, since dereferencing a null AVFrame* would otherwise segfault
    // the whole interpreter instead of raising a catchable Python exception.
    void require_frame() const;

    const AVHWFramesContext *frames_ctx() const;

    static std::string format_name(int format);

    std::string hw_type() const;
    std::string format() const;
    std::string sw_format() const;

    std::vector<uintptr_t> data() const;

    std::vector<int> linesize() const;

    uintptr_t device_hwctx() const;

    // Uploads an NV12 picture from system memory (a (height * 3 / 2, width) uint8 array: the luma rows, then
    // the interleaved chroma rows) into a new frame on the session's Vulkan hardware device, as the decoder
    // would have produced it. For trying out consumers of VulkanFrames without a console. The frame is
    // `visible_width` x `visible_height` (the whole picture by default), like a decoded picture that is
    // smaller than the image it is stored in.
    static std::unique_ptr<VulkanFrame> upload_nv12(StreamSession &session, const py::array_t<uint8_t, py::array::c_style> &nv12,
                                                 std::optional<int> visible_width = std::nullopt,
                                                 std::optional<int> visible_height = std::nullopt);

    // A new all-black `width` x `height` frame uploaded to the session's Vulkan hardware device, for
    // VulkanFrameHandler::empty_frame() to hand out before any frame has been decoded yet.
    static std::unique_ptr<VulkanFrame> black(StreamSession &session, int width, int height);
};

struct CudaFrameLayout
{
    py::ssize_t bytes_per_sample;
    const char *typestr;
    std::string sw_format;
    py::ssize_t width;
    py::ssize_t height;
    py::ssize_t uv_width;
    py::ssize_t uv_height;

    static CudaFrameLayout of(const AVFrame *frame);
};

static YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, const CudaFrameLayout &layout);

// The colour conversion (matrix and range) of `frame`, for samples of `bytes_per_sample` bytes (1: NV12, 2: P010/P016).
YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, int bytes_per_sample);

struct CudaArrayDestination
{
    uintptr_t ptr;
    std::vector<py::ssize_t> shape;
    std::vector<py::ssize_t> strides;   // empty if the source reported none (implies C-contiguous)

    static CudaArrayDestination parse(const py::object &out);

    void check_fits(const CudaFrameLayout &layout) const;
};

class FrameHandler
{
public:
    FrameHandler(StreamSession *streamSession);
    virtual ~FrameHandler() = default;

    virtual py::object get_frame(const py::object &out) = 0;

    // Not overridable (static): concrete subclasses hide this with their own empty_frame() of the
    // same name, each returning the empty buffer/frame that its own get_frame() knows how to fill.
    static py::object empty_frame(int width = 0, int height = 0);

protected:
    StreamSession *streamSession;

    struct AVFrameGuard
    {
        AVFrame *&frame;
        ~AVFrameGuard() { av_frame_free(&frame); }
    };

    ChiakiFfmpegFrame pull_decoded_frame();
};

class CpuFrameHandler : public FrameHandler
{
public:
    CpuFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);

    // A (height, width, 3) uint8 numpy array, suitable as `out` for get_frame() of this size.
    static py::array_t<uint8_t> empty_frame(int width, int height);

private:
    AVFrame *frame = nullptr;
    int width = 0;
    int height = 0;
    AVPixelFormat src_format = AV_PIX_FMT_NONE;
    uint8_t *dst_data[1] = {nullptr};
    int dst_linesize[1] = {0};

    py::array_t<uint8_t> output_array(const py::object &out, int height, int width);
};

class CudaFrameHandler : public FrameHandler
{
public:
    CudaFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);

    // A (height, width, 3) uint8 CUDA array, suitable as `out` for get_frame() of this size: a CuPy
    // array by default, or a torch tensor if `backend` is "torch". With `channels_last` false, the
    // array is instead shaped (3, height, width) - a transposed/permuted view of that same
    // interleaved memory, so it is still a valid `out` (get_frame() writes RGB interleaved either way).
    static py::object empty_frame(int width, int height, const std::string &backend = "cupy", bool channels_last = true);
};

class VulkanFrameHandler : public FrameHandler
{
public:
    VulkanFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);

    // A new all-black VulkanFrame of this size, uploaded to the session's Vulkan hardware device -
    // suitable to show before the first decoded frame arrives, and equally usable as `out` for
    // get_frame() (which replaces its contents regardless of size once a real frame lands). Unlike
    // the other handlers' empty_frame(), this needs the session's device, so it isn't static.
    std::unique_ptr<VulkanFrame> empty_frame(int width, int height);
};

#endif // CHIAKI_PY_FRAME_HANDLER_H
