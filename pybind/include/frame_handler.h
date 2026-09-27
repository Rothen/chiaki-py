#ifndef CHIAKI_PY_FRAME_HANDLER_H
#define CHIAKI_PY_FRAME_HANDLER_H

#include "chiakipysession.h"
#include "cuda_driver.h"

#include <string>
#include <vector>

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

// A decoded video frame still resident in GPU memory. Keeps the decoder's frame pool slot and
// device alive until dropped, so release it promptly. The integers are raw handles for interop;
// what they point to depends on hw_type ('vulkan': data[0] is an AVVkFrame*, 'cuda': one device
// pointer per plane, 'd3d11va': data[0] is an ID3D11Texture2D* and data[1] its array slice).
struct VulkanFrame
{
    AVFrame *frame = nullptr;
    // Presentation time in seconds.
    double pts = 0.0;
    // Frame duration in seconds.
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

    // Upload an NV12 picture from system memory - a C-contiguous uint8 array of shape
    // (height * 3 / 2, width): the luma rows, then the interleaved chroma rows - into a new
    // VulkanFrame on the session's Vulkan hardware device (RuntimeError if it does not use one),
    // as the decoder would have produced it. The frame shows only the top left
    // visible_width x visible_height pixels if given, like a decoded picture that is smaller
    // than the image it is stored in. For trying out consumers of VulkanFrames, such as
    // VulkanRenderer, without a console.
    static std::unique_ptr<VulkanFrame> upload_nv12(ChiakiPySession &stream_session, const py::array_t<uint8_t, py::array::c_style> &nv12,
                                                 std::optional<int> visible_width = std::nullopt,
                                                 std::optional<int> visible_height = std::nullopt);

    // A new all-black width x height VulkanFrame on the session's Vulkan hardware device
    // (RuntimeError if it does not use one), e.g. as a placeholder picture. Unlike
    // VulkanFrameHandler.empty_frame(), which holds no frame and does no GPU work.
    static std::unique_ptr<VulkanFrame> black(ChiakiPySession &stream_session, int width, int height);
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

// Base class of the frame handlers, which pull decoded frames out of a ChiakiPySession in
// different forms (CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler). Not instantiable itself.
class FrameHandler
{
public:
    FrameHandler(ChiakiPySession *streamSession);
    virtual ~FrameHandler() = default;

    // Pull the next decoded video frame, or None if none was available yet. What is returned, and
    // what `out` may be, depends on the subclass. Raises RuntimeError on decoding failure.
    virtual py::object get_frame(const py::object &out = py::none()) = 0;

    // Not implemented on the base class; call empty_frame() on a concrete subclass instead.
    // (Static, so not overridable: each subclass hides it with an empty_frame() of its own, returning
    // the empty buffer/frame that its own get_frame() knows how to fill.)
    static py::object empty_frame(int width = 0, int height = 0);

protected:
    ChiakiPySession *streamSession;

    struct AVFrameGuard
    {
        AVFrame *&frame;
        ~AVFrameGuard() { av_frame_free(&frame); }
    };

    ChiakiFfmpegFrame pull_decoded_frame();
};

// Decodes and converts frames to RGB entirely on the CPU. Works with any decoder, needs no
// hardware decoder configured, and is the default FrameHandler `chiaki_py.Session` uses.
class CpuFrameHandler : public FrameHandler
{
public:
    CpuFrameHandler(ChiakiPySession *streamSession) : FrameHandler(streamSession) {}

    // Pull the next decoded video frame as a (height, width, 3) uint8 RGB array, or None if
    // none was available yet. If `out` is given it must already have the frame's exact shape
    // (C-contiguous, uint8, writable); the frame is written into it and `out` is returned,
    // otherwise a new array is allocated. Raises RuntimeError on decoding failure and
    // TypeError/ValueError for an unusable `out`.
    py::object get_frame(const py::object &out = py::none()) override;

    // A (height, width, 3) uint8 numpy array, ready to be reused as `out` for get_frame() of frames
    // of this size.
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

// Converts decoded frames to RGB on the GPU with CUDA and returns them as a CuPy (or PyTorch)
// array, without a round trip through system memory. Requires an NVIDIA GPU and
// Settings.set_hardware_decoder('cuda') before connecting, which chiaki_py.Session does itself
// for frame_handler_cls=CudaFrameHandler.
class CudaFrameHandler : public FrameHandler
{
public:
    CudaFrameHandler(ChiakiPySession *streamSession) : FrameHandler(streamSession) {}

    // Pull the next decoded video frame on the GPU as a (height, width, 3) uint8 RGB CuPy array,
    // converted on the GPU, or None if none was available yet. If `out` is given - a writable
    // uint8 CUDA array such as a torch tensor or cupy array, exposing __cuda_array_interface__,
    // shaped (height, width, 3) and C-contiguous ('channels last'), or (3, height, width) as a
    // transpose/permute view over that same interleaved memory ('channels first', e.g. from
    // empty_frame(channels_last=False)) - the frame is converted into it instead and `out` is
    // returned; otherwise a new CuPy array is allocated. The conversion has finished when this
    // returns. Requires hardware_decoder='cuda' (RuntimeError otherwise); a bad `out` raises
    // TypeError/ValueError.
    py::object get_frame(const py::object &out = py::none()) override;

    // A uint8 CUDA array, ready to be reused as `out` for get_frame() of frames of this size.
    // `backend` is 'cupy' (default) or 'torch', selecting what allocates it - torch requires a
    // CUDA build of PyTorch and returns a tensor on 'cuda'. With `channels_last` true (default)
    // it is shaped (height, width, 3); with it false, (3, height, width) instead - a
    // transpose/permute view of that same interleaved RGB memory, for a model that wants CHW,
    // with no extra copy.
    static py::object empty_frame(int width, int height, const std::string &backend = "cupy", bool channels_last = true);
};

// Hands out frames as VulkanFrame objects that stay resident on the GPU device the decoder
// decoded them on - no conversion, no copy. Normally used with Settings.set_hardware_decoder('vulkan')
// (chiaki_py.Session sets it for frame_handler_cls=VulkanFrameHandler), which VulkanRenderer needs to
// draw them, e.g. via chiaki_py.gui.VulkanVideoWidget; with another hardware decoder the frames are
// that decoder's (see VulkanFrame.hw_type).
class VulkanFrameHandler : public FrameHandler
{
public:
    VulkanFrameHandler(ChiakiPySession *streamSession) : FrameHandler(streamSession) {}

    // Pull the next decoded video frame without leaving GPU memory. Returns a VulkanFrame, or None
    // if none was available yet. If `out` is a VulkanFrame it takes over the new frame (releasing
    // the one it held) and is returned instead of a new VulkanFrame being created. Raises
    // RuntimeError if the session doesn't use a hardware decoder and TypeError if `out` isn't a VulkanFrame.
    py::object get_frame(const py::object &out = py::none()) override;

    // An empty VulkanFrame holding no decoded frame yet, ready to be reused as `out` for get_frame().
    // `width`/`height` are accepted for a uniform empty_frame(width, height) signature but otherwise
    // unused, since a VulkanFrame takes on whatever size the next decoded frame actually has. Does no
    // GPU work, unlike VulkanFrame.black(): use that directly if you need a real placeholder picture
    // and can guarantee it won't run concurrently with the hardware decoder's own Vulkan submissions.
    static std::unique_ptr<VulkanFrame> empty_frame(int width = 0, int height = 0);
};

#endif // CHIAKI_PY_FRAME_HANDLER_H
