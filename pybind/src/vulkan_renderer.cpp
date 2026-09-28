// The platform macros have to be defined before the first Vulkan header.
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include "vulkan_renderer.h"

#include <stdexcept>
#include <string>

#ifndef CHIAKI_PY_HAS_PLACEBO

// Built without libplacebo (see placebo_vulkan.cpp): the class exists, so that the module has the same API, but
// says so when used.
static constexpr const char *kUnsupported = "This build of chiaki_py can't draw with Vulkan: it was built without libplacebo";

struct VulkanRenderer::Impl
{
};

VulkanRenderer::VulkanRenderer(ChiakiPySession &, uintptr_t, uintptr_t, VulkanWindowSystem) { throw std::runtime_error(kUnsupported); }
VulkanRenderer::~VulkanRenderer() {}
void VulkanRenderer::render(const VulkanFrame &) { throw std::runtime_error(kUnsupported); }
void VulkanRenderer::set_size(int, int) {}
void VulkanRenderer::set_overlay(const uint8_t *, int, int, int) { throw std::runtime_error(kUnsupported); }
void VulkanRenderer::clear_overlay() {}
void VulkanRenderer::close() {}
bool VulkanRenderer::is_supported() { return false; }

#else

#include <atomic>
#include <cstdint>
#include <deque>

#include "placebo_vulkan.h"

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
}

#include <libplacebo/renderer.h>
#include <libplacebo/swapchain.h>
// The implementation of what this header offers is in libav_impl.c.
#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

#ifndef _WIN32
// Last, as Xlib defines macros (None, Bool, Status, ...) that would break the headers above. vulkan_wayland.h needs no
// Wayland header: it only names wl_display and wl_surface as incomplete structs.
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#endif

struct VulkanRenderer::Impl
{
    // Frames that stay referenced after they were drawn. libplacebo lets the decoder reuse a frame's image as soon as
    // the GPU is done with it (through the frame's semaphores), but an image must not be freed while it is in use,
    // and that could happen if the stream ends. So the latest frames are held on to, as many as the swapchain has in
    // flight (one) plus the one being drawn; each takes a slot in the decoder's frame pool, so no more than that.
    static constexpr size_t kFramesKept = 2;

    AVBufferRef *device_ref = nullptr;
    AVHWDeviceContext *device = nullptr;
    PlaceboVulkan *placebo = nullptr;
#ifdef _WIN32
    HWND window = nullptr;
#else
    Display *x11_display = nullptr; // only set if the renderer opened the connection itself, to close it again
#endif
    std::atomic<int> requested_width{0}, requested_height{0}; // from set_size(), for surfaces without a size of their own
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    pl_swapchain swapchain = nullptr;
    pl_renderer renderer = nullptr;
    std::deque<AVFrame *> drawn;
    pl_tex overlay_tex = nullptr; // the overlay's pixels, or nullptr while there is none
    int overlay_margin = 0;
    bool closed = false;

    ~Impl()
    {
        try
        {
            close();
        }
        catch (...)
        {
        }
    }

    void init(ChiakiPySession &session, uintptr_t native_window, uintptr_t native_display, VulkanWindowSystem window_system);
    void create_surface(uintptr_t native_window, uintptr_t native_display, VulkanWindowSystem window_system);
    bool window_size(int &width, int &height);
    void render(const VulkanFrame &frame);
    void set_overlay(const uint8_t *rgba, int width, int height, int margin);
    void clear_overlay();
    void close();
};

void VulkanRenderer::Impl::init(ChiakiPySession &session, uintptr_t native_window, uintptr_t native_display, VulkanWindowSystem window_system)
{
    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();
    if (!decoder || !decoder->hw_device_ctx)
        throw std::runtime_error("Session is not using a hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");
    auto *device_context = reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data);
    if (device_context->type != AV_HWDEVICE_TYPE_VULKAN)
        throw std::runtime_error(std::string("Session is using the ") + av_hwdevice_get_type_name(device_context->type) +
                                 " hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");
    placebo = PlaceboVulkan::from_device(device_context);
    if (!placebo)
        throw std::runtime_error("The session's Vulkan device was not created by chiaki_py (libplacebo could not make one on this "
                                 "machine, or the session was given a device of its own), so it can't draw on it");

    device_ref = av_buffer_ref(decoder->hw_device_ctx);
    device = device_context;
    create_surface(native_window, native_display, window_system);

    pl_vulkan_swapchain_params swapchain_params = {};
    swapchain_params.surface = surface;
    swapchain_params.present_mode = VK_PRESENT_MODE_FIFO_KHR; // vsync
    swapchain_params.swapchain_depth = 1;                     // one frame at a time, for the lowest latency
    swapchain = pl_vulkan_create_swapchain(placebo->vulkan, &swapchain_params);
    if (!swapchain)
        throw std::runtime_error("libplacebo failed to create a swapchain for the window");

    renderer = pl_renderer_create(placebo->log, placebo->vulkan->gpu);
    if (!renderer)
        throw std::runtime_error("libplacebo failed to create its renderer");
}

#ifdef _WIN32

void VulkanRenderer::Impl::create_surface(uintptr_t native_window, uintptr_t, VulkanWindowSystem window_system)
{
    if (window_system != VulkanWindowSystem::Default && window_system != VulkanWindowSystem::Win32)
        throw std::runtime_error("Only Win32 windows can be drawn into on Windows");
    window = reinterpret_cast<HWND>(native_window);

    VkWin32SurfaceCreateInfoKHR surface_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_info.hinstance = GetModuleHandleW(nullptr);
    surface_info.hwnd = window;
    if (vkCreateWin32SurfaceKHR(placebo->instance->instance, &surface_info, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Vulkan surface for the window");
}

// The window's drawable size; false if the window is gone.
bool VulkanRenderer::Impl::window_size(int &width, int &height)
{
    RECT area;
    if (!GetClientRect(window, &area))
        return false;
    width = area.right - area.left;
    height = area.bottom - area.top;
    return true;
}

#else

void VulkanRenderer::Impl::create_surface(uintptr_t native_window, uintptr_t native_display, VulkanWindowSystem window_system)
{
    const VkInstance instance = placebo->instance->instance;
    switch (window_system)
    {
    case VulkanWindowSystem::Default:
    case VulkanWindowSystem::X11:
    {
        if (!placebo->has_instance_extension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
            throw std::runtime_error("The Vulkan driver can't draw into X11 windows (it has no " VK_KHR_XLIB_SURFACE_EXTENSION_NAME ")");
        // A window belongs to the X server, not to a connection: one of the renderer's own draws into a window Qt made
        // on its connection just as well, which is all that Qt's winId() leaves to go on.
        Display *display = reinterpret_cast<Display *>(native_display);
        if (!display)
        {
            x11_display = XOpenDisplay(nullptr);
            if (!x11_display)
                throw std::runtime_error("Failed to connect to the X server (is DISPLAY set?)");
            display = x11_display;
        }
        VkXlibSurfaceCreateInfoKHR surface_info = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
        surface_info.dpy = display;
        surface_info.window = static_cast<Window>(native_window);
        if (vkCreateXlibSurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create a Vulkan surface for the X11 window");
        break;
    }
    case VulkanWindowSystem::Wayland:
    {
        if (!placebo->has_instance_extension(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
            throw std::runtime_error("The Vulkan driver can't draw into Wayland windows (it has no " VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME ")");
        if (!native_window || !native_display)
            throw std::runtime_error("Drawing into a Wayland window needs its wl_surface (window) and wl_display (display)");
        VkWaylandSurfaceCreateInfoKHR surface_info = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
        surface_info.display = reinterpret_cast<wl_display *>(native_display);
        surface_info.surface = reinterpret_cast<wl_surface *>(native_window);
        if (vkCreateWaylandSurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create a Vulkan surface for the Wayland window");
        break;
    }
    case VulkanWindowSystem::Win32:
        throw std::runtime_error("Win32 windows can only be drawn into on Windows");
    }
}

// The window's drawable size as the surface reports it (X11), or as set_size() said where the surface leaves it to the
// swapchain (Wayland); false if the window is gone.
bool VulkanRenderer::Impl::window_size(int &width, int &height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(placebo->vulkan->phys_device, surface, &capabilities) != VK_SUCCESS)
        return false;
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        width = static_cast<int>(capabilities.currentExtent.width);
        height = static_cast<int>(capabilities.currentExtent.height);
    }
    else
    {
        width = requested_width.load();
        height = requested_height.load();
    }
    return true;
}

#endif

void VulkanRenderer::Impl::render(const VulkanFrame &frame)
{
    if (closed)
        throw std::runtime_error("The renderer has been closed");

    const AVFrame *source = frame.frame;
    if (!source || source->format != AV_PIX_FMT_VULKAN || !source->hw_frames_ctx)
        throw std::domain_error("Only frames from the Vulkan hardware decoder can be drawn");
        // throw std::runtime_error("Only frames from the Vulkan hardware decoder can be drawn");
    auto *frames = reinterpret_cast<AVHWFramesContext *>(source->hw_frames_ctx->data);
    if (frames->device_ctx != device)
        throw std::runtime_error("The frame was decoded on another Vulkan device than this renderer draws on");

    int width = 0, height = 0;
    if (!window_size(width, height))
        throw std::runtime_error("The window is gone");
    if (width <= 0 || height <= 0)
        return; // minimised, or on Wayland no size set yet
    if (!pl_swapchain_resize(swapchain, &width, &height))
        throw std::runtime_error("libplacebo failed to resize the swapchain");

    const pl_gpu gpu = placebo->vulkan->gpu;

    // Wraps the frame's images, to be sampled directly. The frame's semaphores make the drawing wait for the decoder
    // and the decoder for the drawing.
    pl_avframe_params map_params = {};
    map_params.frame = source;
    pl_frame image = {};
    if (!pl_map_avframe_ex(gpu, &image, &map_params))
        throw std::runtime_error("libplacebo could not use the frame (" + VulkanFrame::format_name(frames->sw_format) + ")");
    struct Unmap
    {
        pl_gpu gpu;
        pl_frame *image;
        bool done = false;
        void run()
        {
            if (!done)
                pl_unmap_avframe(gpu, image);
            done = true;
        }
        ~Unmap() { run(); }
    } unmap{gpu, &image};

    // HDR streams are passed on to displays that can show them; on others libplacebo tone maps them.
    pl_swapchain_colorspace_hint(swapchain, &image.color);

    pl_swapchain_frame swapchain_frame = {};
    if (!pl_swapchain_start_frame(swapchain, &swapchain_frame))
    {
        // Probably out of date (the window was resized in between); once more with the size it has now.
        pl_swapchain_resize(swapchain, &width, &height);
        if (!pl_swapchain_start_frame(swapchain, &swapchain_frame))
            throw std::runtime_error("libplacebo failed to start a frame on the window's swapchain");
    }

    // The picture is scaled to fit the window with its aspect ratio kept, centred; the rest of the window is left black.
    pl_frame target = {};
    pl_frame_from_swapchain(&target, &swapchain_frame);
    pl_rect2df_aspect_copy(&target.crop, &image.crop, 0.0f);

    // The overlay (the frame rate) sits in the corner of the window, not of the picture, so it is addressed relative to
    // the whole target rather than its crop. Whole pixels, for it to be drawn 1:1.
    pl_overlay overlay = {};
    pl_overlay_part overlay_part = {};
    if (overlay_tex)
    {
        const float overlay_width = static_cast<float>(overlay_tex->params.w), overlay_height = static_cast<float>(overlay_tex->params.h);
        const float right = static_cast<float>(swapchain_frame.fbo->params.w - overlay_margin);
        overlay_part.src = {0.0f, 0.0f, overlay_width, overlay_height};
        overlay_part.dst = {right - overlay_width, static_cast<float>(overlay_margin), right, static_cast<float>(overlay_margin) + overlay_height};
        overlay.tex = overlay_tex;
        overlay.mode = PL_OVERLAY_NORMAL;
        overlay.coords = PL_OVERLAY_COORDS_DST_FRAME;
        overlay.repr = pl_color_repr_rgb;
        overlay.repr.alpha = PL_ALPHA_PREMULTIPLIED;
        overlay.color = pl_color_space_srgb;
        overlay.parts = &overlay_part;
        overlay.num_parts = 1;
        target.overlays = &overlay;
        target.num_overlays = 1;
    }

    const bool rendered = pl_render_image(renderer, &image, &target, &pl_render_default_params);
    unmap.run(); // the drawing is recorded, the frame's images are handed back to the decoder once it is done
    const bool submitted = pl_swapchain_submit_frame(swapchain);
    if (!rendered)
        throw std::runtime_error("libplacebo failed to draw the frame");
    if (!submitted)
        throw std::runtime_error("libplacebo failed to submit the frame to the swapchain");
    pl_swapchain_swap_buffers(swapchain);

    AVFrame *kept = av_frame_clone(source);
    if (!kept)
        throw std::runtime_error("Failed to reference the frame");
    drawn.push_back(kept);
    while (drawn.size() > kFramesKept)
    {
        av_frame_free(&drawn.front());
        drawn.pop_front();
    }
}

void VulkanRenderer::Impl::set_overlay(const uint8_t *rgba, int width, int height, int margin)
{
    if (closed)
        throw std::runtime_error("The renderer has been closed");
    if (!rgba || width < 1 || height < 1)
        throw std::runtime_error("The overlay has no pixels");

    const pl_gpu gpu = placebo->vulkan->gpu;
    pl_fmt format = pl_find_fmt(gpu, PL_FMT_UNORM, 4, 8, 8, PL_FMT_CAP_SAMPLEABLE);
    if (!format)
        throw std::runtime_error("The GPU has no 8 bit RGBA texture format to sample");
    pl_tex_params params = {};
    params.w = width;
    params.h = height;
    params.format = format;
    params.sampleable = true;
    params.host_writable = true;
    if (!pl_tex_recreate(gpu, &overlay_tex, &params)) // reuses the texture if it has the right size already
        throw std::runtime_error("libplacebo failed to create the overlay's texture");

    pl_tex_transfer_params upload = {};
    upload.tex = overlay_tex;
    upload.ptr = const_cast<uint8_t *>(rgba);
    if (!pl_tex_upload(gpu, &upload))
        throw std::runtime_error("libplacebo failed to upload the overlay");
    overlay_margin = margin;
}

void VulkanRenderer::Impl::clear_overlay()
{
    if (!closed && overlay_tex)
        pl_tex_destroy(placebo->vulkan->gpu, &overlay_tex);
}

void VulkanRenderer::Impl::close()
{
    if (closed)
        return;
    closed = true;

    if (placebo && placebo->vulkan)
        pl_gpu_finish(placebo->vulkan->gpu);
    if (overlay_tex)
        pl_tex_destroy(placebo->vulkan->gpu, &overlay_tex);
    for (AVFrame *&frame : drawn)
        av_frame_free(&frame);
    drawn.clear();
    if (renderer)
        pl_renderer_destroy(&renderer);
    if (swapchain)
        pl_swapchain_destroy(&swapchain);
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(placebo->instance->instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
#ifndef _WIN32
    if (x11_display)
        XCloseDisplay(x11_display); // after the surface, which uses the connection
    x11_display = nullptr;
#endif
    av_buffer_unref(&device_ref); // the device stays alive for as long as the decoder has it
}

VulkanRenderer::VulkanRenderer(ChiakiPySession &session, uintptr_t window, uintptr_t display, VulkanWindowSystem window_system)
    : impl(std::make_unique<Impl>())
{
    impl->init(session, window, display, window_system); // if this throws, the impl frees what was made so far
}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::render(const VulkanFrame &frame) { impl->render(frame); }

void VulkanRenderer::set_size(int width, int height)
{
    impl->requested_width = width;
    impl->requested_height = height;
}

void VulkanRenderer::set_overlay(const uint8_t *rgba, int width, int height, int margin) { impl->set_overlay(rgba, width, height, margin); }

void VulkanRenderer::clear_overlay() { impl->clear_overlay(); }

void VulkanRenderer::close() { impl->close(); }

bool VulkanRenderer::is_supported() { return PlaceboVulkan::is_supported(); }

#endif
