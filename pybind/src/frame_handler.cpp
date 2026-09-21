#include "frame_handler.h"

namespace py = pybind11;

ThreadSwsContext ::~ThreadSwsContext() { sws_freeContext(ctx); }

GpuFrame::GpuFrame(AVFrame *frame, double pts, double duration) : frame(frame), pts(pts), duration(duration) {}
GpuFrame::~GpuFrame() { av_frame_free(&frame); }

void GpuFrame::reset(AVFrame *new_frame, double new_pts, double new_duration)
{
    av_frame_free(&frame);
    frame = new_frame;
    pts = new_pts;
    duration = new_duration;
}

const AVHWFramesContext *GpuFrame::frames_ctx() const
{
    return reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data);
}

std::string GpuFrame::format_name(int format)
{
    const char *name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(format));
    return name ? name : "unknown";
}

std::string GpuFrame::hw_type() const { return av_hwdevice_get_type_name(frames_ctx()->device_ctx->type); }
std::string GpuFrame::format() const { return format_name(frame->format); }
std::string GpuFrame::sw_format() const { return format_name(frames_ctx()->sw_format); }

std::vector<uintptr_t> GpuFrame::data() const
{
    std::vector<uintptr_t> out;
    for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i]; i++)
        out.push_back(reinterpret_cast<uintptr_t>(frame->data[i]));
    return out;
}

std::vector<int> GpuFrame::linesize() const
{
    return std::vector<int>(frame->linesize, frame->linesize + data().size());
}

uintptr_t GpuFrame::device_hwctx() const { return reinterpret_cast<uintptr_t>(frames_ctx()->device_ctx->hwctx); }

std::unique_ptr<GpuFrame> GpuFrame::upload_nv12(StreamSession &session, const py::array_t<uint8_t, py::array::c_style> &nv12,
                                                std::optional<int> visible_width, std::optional<int> visible_height)
{
    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();
    if (!decoder || !decoder->hw_device_ctx ||
        reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type != AV_HWDEVICE_TYPE_VULKAN)
        throw std::runtime_error("Session is not using the Vulkan hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");
    if (nv12.ndim() != 2 || nv12.shape(0) % 3 != 0 || nv12.shape(0) / 3 * 2 % 2 != 0 || nv12.shape(1) % 2 != 0)
        throw py::value_error("nv12 must have shape (height * 3 / 2, width) with an even height and width");
    const int width = static_cast<int>(nv12.shape(1));
    const int height = static_cast<int>(nv12.shape(0) / 3 * 2);

    AVBufferRef *frames_ref = av_hwframe_ctx_alloc(decoder->hw_device_ctx);
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
    AVFrame *sw = av_frame_alloc();
    auto cleanup = [&]() { av_frame_free(&hw); av_frame_free(&sw); av_buffer_unref(&frames_ref); };
    if (!hw || !sw)
    {
        cleanup();
        throw std::runtime_error("Failed to allocate a frame");
    }
    if (av_hwframe_get_buffer(frames_ref, hw, 0) < 0)
    {
        cleanup();
        throw std::runtime_error("Failed to allocate a Vulkan frame");
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
    return std::make_unique<GpuFrame>(hw, 0.0, 0.0);
}

CudaPlane::CudaPlane(const AVFrame *source, uintptr_t ptr, std::vector<py::ssize_t> shape,
                   std::vector<py::ssize_t> strides, std::string typestr)
    : frame(av_frame_clone(source)), ptr(ptr), shape(std::move(shape)),
      strides(std::move(strides)), typestr(std::move(typestr))
{
    if (!frame)
        throw std::runtime_error("Failed to reference frame");
}
CudaPlane::~CudaPlane() { av_frame_free(&frame); }

py::dict CudaPlane::cuda_array_interface() const
{
    py::dict interface;
    interface["version"] = 3;
    interface["shape"] = py::tuple(py::cast(shape));
    interface["strides"] = py::tuple(py::cast(strides));
    interface["typestr"] = typestr;
    interface["data"] = py::make_tuple(ptr, false);
    return interface;
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
        throw std::runtime_error("Unsupported CUDA frame format: " + GpuFrame::format_name(frames_ctx->sw_format));
    }
    if (!frame->data[0] || !frame->data[1])
        throw std::runtime_error("CUDA frame is missing a plane");

    layout.sw_format = GpuFrame::format_name(frames_ctx->sw_format);
    layout.width = frame->width;
    layout.height = frame->height;
    layout.uv_width = (layout.width + 1) / 2;
    layout.uv_height = (layout.height + 1) / 2;
    return layout;
}

std::unique_ptr<CudaFrame> CudaFrame::from_frame(const AVFrame *frame, double pts, double duration)
{
    const CudaFrameLayout layout = CudaFrameLayout::of(frame);

    auto result = std::make_unique<CudaFrame>();
    result->width = frame->width;
    result->height = frame->height;
    result->pts = pts;
    result->duration = duration;
    result->sw_format = layout.sw_format;
    result->y = std::make_shared<CudaPlane>(
        frame, reinterpret_cast<uintptr_t>(frame->data[0]),
        std::vector<py::ssize_t>{layout.height, layout.width},
        std::vector<py::ssize_t>{frame->linesize[0], layout.bytes_per_sample}, layout.typestr);
    result->uv = std::make_shared<CudaPlane>(
        frame, reinterpret_cast<uintptr_t>(frame->data[1]),
        std::vector<py::ssize_t>{layout.uv_height, layout.uv_width, 2},
        std::vector<py::ssize_t>{frame->linesize[1], 2 * layout.bytes_per_sample, layout.bytes_per_sample},
        layout.typestr);
    return result;
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

    // No strides means C-contiguous.
    if (interface.contains("strides") && !interface["strides"].is_none())
    {
        const auto strides = interface["strides"].cast<std::vector<py::ssize_t>>();
        py::ssize_t expected = 1;
        for (size_t i = destination.shape.size(); i-- > 0;)
        {
            if (destination.shape[i] != 1 && strides[i] != expected)
                throw py::value_error("out must be C-contiguous");
            expected *= destination.shape[i];
        }
    }
    return destination;
}

void CudaArrayDestination::check_fits(const CudaFrameLayout &layout) const
{
    const std::vector<py::ssize_t> expected_shape{layout.height, layout.width, 3};
    if (shape != expected_shape)
        throw py::value_error("out has the wrong shape for this frame: need (" + std::to_string(layout.height) + ", " +
                              std::to_string(layout.width) + ", 3)");
}

FrameHandler::FrameHandler(StreamSession *streamSession) : streamSession(streamSession) {}

ChiakiFfmpegFrame FrameHandler::pull_decoded_frame()
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");

    int32_t frames_lost;
    return chiaki_ffmpeg_decoder_pull_frame(decoder, &frames_lost);
}

py::object CPUFrameHandler::get_frame(const py::object &out = py::none())
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

py::object CUDAFrameHandler::get_frame(const py::object &out = py::none())
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");
    if (!decoder->hw_device_ctx ||
        reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data)->type != AV_HWDEVICE_TYPE_CUDA)
        throw std::runtime_error("Session is not using the CUDA hardware decoder; use Settings.set_hardware_decoder(\"cuda\")");

    std::optional<CudaArrayDestination> destination;
    if (!out.is_none())
        destination = CudaArrayDestination::parse(out);

    ChiakiFfmpegFrame pulled = pull_decoded_frame();
    if (!pulled.frame)
        return py::none();

    AVFrameGuard frame_guard{pulled.frame};

    if (!pulled.frame->hw_frames_ctx)
    {
        throw std::runtime_error("Decoded frame is not GPU-resident");
    }

    if (!destination)
    {
        return py::cast(CudaFrame::from_frame(pulled.frame, pulled.pts, pulled.duration));
    }

    const CudaFrameLayout layout = CudaFrameLayout::of(pulled.frame);
    destination->check_fits(layout);

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
            destination->ptr, static_cast<unsigned>(layout.width), static_cast<unsigned>(layout.height),
            static_cast<unsigned>(layout.bytes_per_sample), color);
    }
    return out;
}

py::object GPUFrameHandler::get_frame(const py::object &out = py::none())
{
    ChiakiFfmpegDecoder *decoder = streamSession->GetFfmpegDecoder();
    if (!decoder)
        throw std::runtime_error("Session has no FFmpeg decoder");
    if (!decoder->hw_device_ctx)
        throw std::runtime_error("Session is not using a hardware decoder, there are no GPU frames");

    GpuFrame *reuse = nullptr;
    if (!out.is_none())
    {
        if (!py::isinstance<GpuFrame>(out))
            throw py::type_error("out must be a GpuFrame");
        reuse = out.cast<GpuFrame *>();
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
    return py::cast(std::make_unique<GpuFrame>(pulled.frame, pulled.pts, pulled.duration));
}

py::array_t<uint8_t> CPUFrameHandler::output_array(const py::object &out, int height, int width)
{
    if (out.is_none())
        return py::array_t<uint8_t>(std::vector<py::ssize_t>{height, width, 3});

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