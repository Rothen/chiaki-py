#include "frame_handler.h"

#include <cstring>

namespace py = pybind11;

namespace
{
// Allocates a Vulkan-resident NV12 frame of `width` x `height` on `session`'s hardware device, ready
// for its pixels to be filled in and transferred with av_hwframe_transfer_data(). `frames_ref` receives
// the frames context, which the caller must av_buffer_unref() once the transfer is done (the frame
// holds its own reference, so the context itself isn't needed past that point). Throws if the session
// isn't using the Vulkan hardware decoder.
AVFrame *alloc_vulkan_nv12_frame(ChiakiPySession &session, int width, int height, AVBufferRef *&frames_ref)
{
    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();
    if (!decoder || !decoder->hw_device_ctx ||
        reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type != AV_HWDEVICE_TYPE_VULKAN)
        throw std::runtime_error("Session is not using the Vulkan hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");

    frames_ref = av_hwframe_ctx_alloc(decoder->hw_device_ctx);
    if (!frames_ref)
        throw std::runtime_error("Failed to allocate a frames context");
    auto *frames = reinterpret_cast<AVHWFramesContext *>(frames_ref->data);
    frames->format = AV_PIX_FMT_VULKAN;
    frames->sw_format = AV_PIX_FMT_NV12;
    frames->width = width;
    frames->height = height;
    if (av_hwframe_ctx_init(frames_ref) < 0)
    {
        av_buffer_unref(&frames_ref);
        throw std::runtime_error("Failed to initialise a Vulkan frames context");
    }

    AVFrame *hw = av_frame_alloc();
    if (!hw)
    {
        av_buffer_unref(&frames_ref);
        throw std::runtime_error("Failed to allocate a frame");
    }
    if (av_hwframe_get_buffer(frames_ref, hw, 0) < 0)
    {
        av_frame_free(&hw);
        av_buffer_unref(&frames_ref);
        throw std::runtime_error("Failed to allocate a Vulkan frame");
    }
    return hw;
}
}

ThreadSwsContext ::~ThreadSwsContext() { sws_freeContext(ctx); }

VulkanFrame::VulkanFrame() {}
VulkanFrame::VulkanFrame(AVFrame *frame, double pts, double duration) : frame(frame), pts(pts), duration(duration) {}
VulkanFrame::~VulkanFrame() { av_frame_free(&frame); }

void VulkanFrame::reset(AVFrame *new_frame, double new_pts, double new_duration)
{
    av_frame_free(&frame);
    frame = new_frame;
    pts = new_pts;
    duration = new_duration;
}

void VulkanFrame::require_frame() const
{
    if (!frame)
        throw std::runtime_error("VulkanFrame holds no decoded frame yet");
}

const AVHWFramesContext *VulkanFrame::frames_ctx() const
{
    require_frame();
    return reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
}

std::string VulkanFrame::format_name(int format)
{
    const char *name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? name : "unknown";
}

std::string VulkanFrame::hw_type() const { return av_hwdevice_get_type_name(frames_ctx()->device_ctx->type); }
std::string VulkanFrame::format() const { require_frame(); return format_name(frame->format); }
std::string VulkanFrame::sw_format() const { return format_name(frames_ctx()->sw_format); }

std::vector<uintptr_t> VulkanFrame::data() const
{
    require_frame();
    std::vector<uintptr_t> out;
    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; i++)
        out.push_back(reinterpret_cast<uintptr_t>(frame->data[i]));
    return out;
}

std::vector<int> VulkanFrame::linesize() const
{
    require_frame();
    return std::vector<int>(frame->linesize, frame->linesize + data().size());
}

uintptr_t VulkanFrame::device_hwctx() const { return reinterpret_cast<uintptr_t>(frames_ctx()->device_ctx->hwctx); }

std::unique_ptr<VulkanFrame> VulkanFrame::upload_nv12(ChiakiPySession &session, const py::array_t<uint8_t, py::array::c_style> &nv12,
                                                std::optional<int> visible_width, std::optional<int> visible_height)
{
    if (nv12.ndim() != 2 || nv12.shape(0) % 3 != 0 || nv12.shape(0) / 3 * 2 % 2 != 0 || nv12.shape(1) % 2 != 0)
        throw py::value_error("nv12 must have shape (height * 3 / 2, width) with an even height and width");
    const int width = static_cast<int>(nv12.shape(1));
    const int height = static_cast<int>(nv12.shape(0) / 3 * 2);

    AVBufferRef *frames_ref = nullptr;
    AVFrame *hw = alloc_vulkan_nv12_frame(session, width, height, frames_ref);
    AVFrame *sw = av_frame_alloc();
    auto cleanup = [&]() { av_frame_free(&hw); av_frame_free(&sw); av_buffer_unref(&frames_ref); };
    if (!sw)
    {
        cleanup();
        throw std::runtime_error("Failed to allocate a frame");
    }

    // The system memory frame only borrows the array's memory, for the duration of the transfer.
    sw->format = AV_PIX_FMT_NV12;
    sw->width = width;
    sw->height = height;
    uint8_t *data = const_cast<uint8_t *>(nv12.data());
    sw->data[0] = data;
    sw->linesize[0] = width;
    sw->data[1] = data + static_cast<size_t>(width) * height;
    sw->linesize[1] = width;
    if (av_hwframe_transfer_data(hw, sw, 0) < 0)
    {
        cleanup();
        throw std::runtime_error("Failed to upload the picture to the GPU");
    }
    // Decoders allocate images of the coded size, which can exceed the picture that is meant to be shown
    hw->width = visible_width.value_or(width);
    hw->height = visible_height.value_or(height);
    if (hw->width < 1 || hw->width > width || hw->height < 1 || hw->height > height)
    {
        cleanup();
        throw py::value_error("The visible size must be within the uploaded picture");
    }
    hw->colorspace = AVCOL_SPC_BT709;
    hw->color_range = AVCOL_RANGE_MPEG;

    av_frame_free(&sw);
    av_buffer_unref(&frames_ref); // the frame holds its own reference
    return std::make_unique<VulkanFrame>(hw, 0.0, 0.0);
}

std::unique_ptr<VulkanFrame> VulkanFrame::black(ChiakiPySession &session, int width, int height)
{
    if (width < 1 || height < 1)
        throw py::value_error("width and height must be positive");

    AVBufferRef *frames_ref = nullptr;
    AVFrame *hw = alloc_vulkan_nv12_frame(session, width, height, frames_ref);
    AVFrame *sw = av_frame_alloc();
    auto cleanup = [&]() { av_frame_free(&hw); av_frame_free(&sw); av_buffer_unref(&frames_ref); };
    if (!sw)
    {
        cleanup();
        throw std::runtime_error("Failed to allocate a frame");
    }

    sw->format = AV_PIX_FMT_NV12;
    sw->width = width;
    sw->height = height;
    if (av_frame_get_buffer(sw, 0) < 0)
    {
        cleanup();
        throw std::runtime_error("Failed to allocate a system memory frame");
    }
    // Limited-range (MPEG) black: luma 16, chroma 128 - matches the color_range set below.
    for (int y = 0; y < height; y++)
        memset(sw->data[0] + static_cast<size_t>(y) * sw->linesize[0], 16, width);
    for (int y = 0; y < (height + 1) / 2; y++)
        memset(sw->data[1] + static_cast<size_t>(y) * sw->linesize[1], 128, width);

    if (av_hwframe_transfer_data(hw, sw, 0) < 0)
    {
        cleanup();
        throw std::runtime_error("Failed to upload the picture to the GPU");
    }
    hw->colorspace = AVCOL_SPC_BT709;
    hw->color_range = AVCOL_RANGE_MPEG;

    av_frame_free(&sw);
    av_buffer_unref(&frames_ref); // the frame holds its own reference
    return std::make_unique<VulkanFrame>(hw, 0.0, 0.0);
}

CudaFrameLayout CudaFrameLayout::of(const AVFrame *frame)
{
    const auto *frames_ctx = reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
    if (frames_ctx->device_ctx->type != AV_HWDEVICE_TYPE_CUDA)
        throw std::runtime_error("Decoded frame is not a CUDA frame");

    CudaFrameLayout layout;
    switch (frames_ctx->sw_format)
    {
    case AV_PIX_FMT_NV12:
        layout.bytes_per_sample = 1;
        layout.typestr = "|u1";
        break;
    case AV_PIX_FMT_P010LE:
    case AV_PIX_FMT_P016LE:
        layout.bytes_per_sample = 2;
        layout.typestr = "<u2";
        break;
    default:
        throw std::runtime_error("Unsupported CUDA frame format: " + VulkanFrame::format_name(frames_ctx->sw_format));
    }
    if (!frame->data[0] || !frame->data[1])
        throw std::runtime_error("CUDA frame is missing a plane");

    layout.sw_format = VulkanFrame::format_name(frames_ctx->sw_format);
    layout.width = frame->width;
    layout.height = frame->height;
    layout.uv_width = (layout.width + 1) / 2;
    layout.uv_height = (layout.height + 1) / 2;
    return layout;
}

static YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, const CudaFrameLayout &layout)
{
    return yuv_to_rgb_params(frame, static_cast<int>(layout.bytes_per_sample));
}

YuvToRgbParams yuv_to_rgb_params(const AVFrame *frame, int bytes_per_sample)
{
    double kr = 0.2126, kb = 0.0722;
    switch (frame->colorspace)
    {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        kr = 0.299;
        kb = 0.114;
        break;
    case AVCOL_SPC_SMPTE240M:
        kr = 0.212;
        kb = 0.087;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        kr = 0.2627;
        kb = 0.0593;
        break;
    default:
        break;
    }
    return YuvToRgbParams::make(kr, kb, frame->color_range == AVCOL_RANGE_JPEG, bytes_per_sample);
}

CudaArrayDestination CudaArrayDestination::parse(const py::object &out)
{
    if (!py::hasattr(out, "__cuda_array_interface__"))
        throw py::type_error("out must be a CUDA array (torch tensor, cupy array, ...) exposing __cuda_array_interface__");

    const py::dict interface = out.attr("__cuda_array_interface__");
    const std::string typestr = interface["typestr"].cast<std::string>();
    if (typestr != "|u1")
        throw py::type_error("out must be a uint8 array, not '" + typestr + "'");

    CudaArrayDestination destination;
    destination.shape = interface["shape"].cast<std::vector<py::ssize_t>>();

    const py::tuple data = interface["data"].cast<py::tuple>();
    destination.ptr = data[0].cast<uintptr_t>();
    if (data[1].cast<bool>())
        throw py::value_error("out is read-only");

    // No strides means C-contiguous; keep whatever was reported (possibly none) for check_fits,
    // which knows the frame's width/height and so can tell a (height, width, 3) layout from a
    // (3, height, width) one and validate strides against the one that matches.
    if (interface.contains("strides") && !interface["strides"].is_none())
        destination.strides = interface["strides"].cast<std::vector<py::ssize_t>>();
    return destination;
}

void CudaArrayDestination::check_fits(const CudaFrameLayout &layout) const
{
    // Either the usual (height, width, 3) - "channels last" - or a (3, height, width) view over
    // that same memory, e.g. a transposed CuPy array or a permuted torch tensor: still physically
    // interleaved RGB, just presented CHW-first for a model that wants it that way.
    const std::vector<py::ssize_t> hwc_shape{layout.height, layout.width, 3};
    const std::vector<py::ssize_t> chw_shape{3, layout.height, layout.width};
    const bool is_hwc = shape == hwc_shape;
    const bool is_chw = shape == chw_shape;
    if (!is_hwc && !is_chw)
        throw py::value_error("out has the wrong shape for this frame: need (" + std::to_string(layout.height) + ", " +
                              std::to_string(layout.width) + ", 3) or (3, " + std::to_string(layout.height) + ", " +
                              std::to_string(layout.width) + ")");

    if (!strides.empty())
    {
        const std::vector<py::ssize_t> expected = is_hwc
            ? std::vector<py::ssize_t>{layout.width * 3, 3, 1}
            : std::vector<py::ssize_t>{1, layout.width * 3, 3};
        for (size_t i = 0; i < 3; i++)
            if (shape[i] != 1 && strides[i] != expected[i])
                throw py::value_error("out must be RGB-interleaved: (height, width, 3) C-contiguous, or a "
                                      "(3, height, width) transpose/permute view of that same memory");
    }
    else if (is_chw)
    {
        throw py::value_error("out has shape (3, height, width) but no strides: it must be a "
                              "transpose/permute view over interleaved (height, width, 3) memory, "
                              "not separately-allocated planar storage");
    }
}

FrameHandler::FrameHandler(ChiakiPySession *streamSession) : streamSession(streamSession) {}

py::object FrameHandler::empty_frame(int width, int height)
{
    throw std::runtime_error("FrameHandler has no frame representation of its own; call empty_frame() on "
                             "a concrete subclass (CpuFrameHandler, CudaFrameHandler, VulkanFrameHandler)");
}

ChiakiFfmpegFrame FrameHandler::pull_decoded_frame()
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");

    // The decoder's mutex is held by chiaki's receive thread while it decodes, and FFmpeg logs from
    // there (through ffmpeg_log_python), which takes the GIL: waiting for the mutex with the GIL held
    // would deadlock.
    GilReleaseIfHeld release;
    int32_t frames_lost;
    return chiaki_ffmpeg_decoder_pull_frame(decoder, &frames_lost);
}

py::object CpuFrameHandler::get_frame(const py::object &out = py::none())
{
    frame = pull_decoded_frame().frame;
    if (!frame)
        return py::none();

    AVFrameGuard frame_guard{frame};

    if (frame->hw_frames_ctx)
    {
        AVFrame *sw_frame = av_frame_alloc();
        if (!sw_frame)
            throw std::runtime_error("Failed to allocate software frame");

        if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0)
        {
            av_frame_free(&sw_frame);
            throw std::runtime_error("Failed to transfer frame from hardware");
        }

        av_frame_copy_props(sw_frame, frame);
        av_frame_free(&frame); // frame_guard.frame is now null, nothing double-freed
        frame = sw_frame;      // frame_guard now owns sw_frame
    }

    width = frame->width;
    height = frame->height;
    src_format = static_cast<AVPixelFormat>(frame->format);

    py::array_t<uint8_t> result = output_array(out, height, width);
    dst_data[0] = result.mutable_data();
    dst_linesize[0] = width * 3;

    {
        py::gil_scoped_release release;

        static thread_local ThreadSwsContext sws;
        sws.ctx = sws_getCachedContext(
            sws.ctx, width, height, src_format,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws.ctx)
        {
            const char *name = av_get_pix_fmt_name(src_format);
            throw std::runtime_error("Unsupported pixel format for RGB conversion: " + std::string(name ? name : "unknown"));
        }

        if (sws_scale(sws.ctx, frame->data, frame->linesize, 0, height, dst_data, dst_linesize) != height)
            throw std::runtime_error("Failed to convert frame to RGB");
    }

    return std::move(result);
}

py::object CudaFrameHandler::get_frame(const py::object &out = py::none())
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");
    if (!decoder->hw_device_ctx ||
        reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type != AV_HWDEVICE_TYPE_CUDA)
        throw std::runtime_error("Session is not using the CUDA hardware decoder; use Settings.set_hardware_decoder(\"cuda\")");

    ChiakiFfmpegFrame pulled = pull_decoded_frame();
    if (!pulled.frame)
        return py::none();

    AVFrameGuard frame_guard{pulled.frame};

    if (!pulled.frame->hw_frames_ctx)
    {
        throw std::runtime_error("Decoded frame is not GPU-resident");
    }

    const CudaFrameLayout layout = CudaFrameLayout::of(pulled.frame);

    // No out given: allocate the RGB destination ourselves, as a CuPy array.
    py::object result = out.is_none()
        ? empty_frame(static_cast<int>(layout.width), static_cast<int>(layout.height))
        : out;

    const CudaArrayDestination destination = CudaArrayDestination::parse(result);
    destination.check_fits(layout);

    // The first member of AVCUDADeviceContext is the CUcontext (its header needs
    // cuda.h, so it is read by hand): the device's primary one, as torch/cupy use.
    const auto *frames_ctx = reinterpret_cast<const AVHWFramesContext *>(pulled.frame->hw_frames_ctx->data);
    const auto cuda_context = *reinterpret_cast<CudaDriver::CUcontext *>(frames_ctx->device_ctx->hwctx);
    const YuvToRgbParams color = yuv_to_rgb_params(pulled.frame, layout);
    {
        py::gil_scoped_release release;
        CudaDriver::get().nv12_to_rgb(
            cuda_context,
            reinterpret_cast<uintptr_t>(pulled.frame->data[0]), pulled.frame->linesize[0],
            reinterpret_cast<uintptr_t>(pulled.frame->data[1]), pulled.frame->linesize[1],
            destination.ptr, static_cast<unsigned>(layout.width), static_cast<unsigned>(layout.height),
            static_cast<unsigned>(layout.bytes_per_sample), color);
    }
    return result;
}

py::object CudaFrameHandler::empty_frame(int width, int height, const std::string &backend, bool channels_last)
{
    py::object array;
    if (backend == "cupy")
    {
        array = py::module_::import("cupy").attr("empty")(py::make_tuple(height, width, 3), "uint8");
    }
    else if (backend == "torch")
    {
        py::module_ torch = py::module_::import("torch");
        array = torch.attr("empty")(py::make_tuple(height, width, 3), py::arg("dtype") = torch.attr("uint8"),
                                    py::arg("device") = "cuda");
    }
    else
    {
        throw py::value_error("backend must be 'cupy' or 'torch', not '" + backend + "'");
    }

    // A (3, height, width) transpose/permute: still the same interleaved memory underneath, just
    // presented channels-first, which get_frame()'s `out` handling understands (see CudaArrayDestination).
    if (!channels_last)
        array = backend == "torch" ? array.attr("permute")(2, 0, 1) : array.attr("transpose")(2, 0, 1);
    return array;
}

py::object VulkanFrameHandler::get_frame(const py::object &out = py::none())
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");
    if (!decoder->hw_device_ctx)
        throw std::runtime_error("Session is not using a hardware decoder, there are no GPU frames");

    VulkanFrame *reuse = nullptr;
    if (!out.is_none())
    {
        if (!py::isinstance<VulkanFrame>(out))
            throw py::type_error("out must be a VulkanFrame");
        reuse = out.cast<VulkanFrame *>();
    }

    ChiakiFfmpegFrame pulled = pull_decoded_frame();
    if (!pulled.frame)
        return py::none();

    if (!pulled.frame->hw_frames_ctx)
    {
        av_frame_free(&pulled.frame);
        throw std::runtime_error("Decoded frame is not GPU-resident");
    }

    if (reuse)
    {
        reuse->reset(pulled.frame, pulled.pts, pulled.duration);
        return out;
    }
    return py::cast(std::make_unique<VulkanFrame>(pulled.frame, pulled.pts, pulled.duration));
}

std::unique_ptr<VulkanFrame> VulkanFrameHandler::empty_frame(int width, int height)
{
    return std::make_unique<VulkanFrame>();
}

py::array_t<uint8_t> CpuFrameHandler::empty_frame(int width, int height)
{
    return py::array_t<uint8_t>(std::vector<py::ssize_t>{height, width, 3});
}

py::array_t<uint8_t> CpuFrameHandler::output_array(const py::object &out, int height, int width)
{
    if (out.is_none())
        return empty_frame(width, height);

    if (!py::isinstance<py::array_t<uint8_t, py::array::c_style>>(out))
        throw py::type_error("out must be a C-contiguous numpy array of dtype uint8");

    auto array = py::reinterpret_borrow<py::array_t<uint8_t, py::array::c_style>>(out);
    if (!array.writeable())
        throw py::value_error("out is read-only");
    if (array.ndim() != 3 || array.shape(0) != height || array.shape(1) != width || array.shape(2) != 3)
        throw py::value_error("out has the wrong shape for this frame: need (" + std::to_string(height) + ", " +
                              std::to_string(width) + ", 3)");
    return array;
}