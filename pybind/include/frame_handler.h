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

struct GpuFrame
{
    AVFrame *frame;
    double pts;
    double duration;

    GpuFrame(AVFrame *frame, double pts, double duration);
    GpuFrame(const GpuFrame &) = delete;
    GpuFrame &operator=(const GpuFrame &) = delete;
    ~GpuFrame();

    void reset(AVFrame *new_frame, double new_pts, double new_duration);

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
    // would have produced it. For trying out consumers of GpuFrames without a console. The frame is
    // `visible_width` x `visible_height` (the whole picture by default), like a decoded picture that is
    // smaller than the image it is stored in.
    static std::unique_ptr<GpuFrame> upload_nv12(StreamSession &session, const py::array_t<uint8_t, py::array::c_style> &nv12,
                                                 std::optional<int> visible_width = std::nullopt,
                                                 std::optional<int> visible_height = std::nullopt);
};

struct CudaPlane
{
    AVFrame *frame;
    uintptr_t ptr;
    std::vector<py::ssize_t> shape;
    std::vector<py::ssize_t> strides; // in bytes
    std::string typestr;

    CudaPlane(const AVFrame *source, uintptr_t ptr, std::vector<py::ssize_t> shape,
              std::vector<py::ssize_t> strides, std::string typestr);
    CudaPlane(const CudaPlane &) = delete;
    CudaPlane &operator=(const CudaPlane &) = delete;
    ~CudaPlane();

    py::dict cuda_array_interface() const;
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

struct CudaFrame
{
    int width;
    int height;
    double pts;
    double duration;
    std::string sw_format;
    std::shared_ptr<CudaPlane> y;
    std::shared_ptr<CudaPlane> uv;

    static std::unique_ptr<CudaFrame> from_frame(const AVFrame *frame, double pts, double duration);
};

static YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, const CudaFrameLayout &layout);

// The colour conversion (matrix and range) of `frame`, for samples of `bytes_per_sample` bytes (1: NV12, 2: P010/P016).
YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, int bytes_per_sample);

struct CudaArrayDestination
{
    uintptr_t ptr;
    std::vector<py::ssize_t> shape;

    static CudaArrayDestination parse(const py::object &out);

    void check_fits(const CudaFrameLayout &layout) const;
};

class FrameHandler
{
public:
    FrameHandler(StreamSession *streamSession);
    virtual ~FrameHandler() = default;

    virtual py::object get_frame(const py::object &out) = 0;

protected:
    StreamSession *streamSession;

    struct AVFrameGuard
    {
        AVFrame *&frame;
        ~AVFrameGuard() { av_frame_free(&frame); }
    };

    ChiakiFfmpegFrame pull_decoded_frame();
};

class CPUFrameHandler : public FrameHandler
{
public:
    CPUFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);

private:
    AVFrame *frame = nullptr;
    int width = 0;
    int height = 0;
    AVPixelFormat src_format = AV_PIX_FMT_NONE;
    uint8_t *dst_data[1] = {nullptr};
    int dst_linesize[1] = {0};

    py::array_t<uint8_t> output_array(const py::object &out, int height, int width);
};

class CUDAFrameHandler : public FrameHandler
{
public:
    CUDAFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);
};

class GPUFrameHandler : public FrameHandler
{
public:
    GPUFrameHandler(StreamSession *streamSession) : FrameHandler(streamSession) {}
    py::object get_frame(const py::object &out);
};

#endif // CHIAKI_PY_FRAME_HANDLER_H
