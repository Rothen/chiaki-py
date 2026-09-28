#ifndef CHIAKI_PY_VULKAN_RENDERER_H
#define CHIAKI_PY_VULKAN_RENDERER_H

#include <cstdint>
#include <memory>

#include "frame_handler.h"

// The kind of native window a VulkanRenderer draws into, which decides what its `window` and `display` are.
enum class VulkanWindowSystem
{
    Default, // Win32 on Windows, X11 elsewhere
    Win32,   // window: an HWND; display: unused
    X11,     // window: an X11 Window id (what Qt's winId() is on X11); display: an Xlib Display*, or 0 to open a connection of its own
    Wayland  // window: a wl_surface*; display: the wl_display* it belongs to (both from the toolkit, e.g. GLFW's or SDL's)
};

// Draws VulkanFrames of the Vulkan hardware decoder into a native window without them leaving the
// GPU: libplacebo converts the frames' NV12/P010 planes to RGB (SDR or HDR), scales them and presents
// them on the same Vulkan device the decoder decodes on. Windows (Win32) and Linux (X11 and Wayland).
//
// The Vulkan device the decoder decodes on (see the ChiakiPySession constructor, which has libplacebo create
// it) is also the one drawn on, so a decoded frame is used where it is: libplacebo samples its NV12/P010
// planes, converts them to RGB with the frame's own colour space (SDR or HDR), scales them to the window and
// presents the result on the window's swapchain. Nothing is copied.
//
// All calls must come from one thread (not necessarily the GUI thread - the Python binding uses a
// dedicated thread of its own, since a call here can block for a while - see VulkanRenderThread),
// except set_size(), and close() before the window is destroyed.
class VulkanRenderer
{
public:
    // `window` is the native window to draw into and `display` the connection it belongs to, as `window_system` says.
    VulkanRenderer(ChiakiPySession &stream_session, uintptr_t window, uintptr_t display = 0, VulkanWindowSystem window_system = VulkanWindowSystem::Default);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    // Draw `frame`, a VulkanFrame from the Vulkan decoder, scaled to fit the window with its aspect
    // ratio kept, and present it. Does nothing while the window has no area (is minimised), or on Wayland
    // before set_size() was called. Returns once the drawing is submitted, not finished; the GPU is waited
    // for when the next frame needs it.
    void render(const VulkanFrame &frame);

    // The size in pixels to draw at, for window systems that leave it to the renderer: Wayland, where a surface is
    // as big as what is drawn into it. Call it whenever the window's size changes, from any thread. Elsewhere the
    // window's own size is used and this is ignored.
    void set_size(int width, int height);

    // Shows `rgba` - width x height pixels, 8 bit RGBA rows with premultiplied alpha, tightly packed - on top of the
    // video, in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared.
    // The image is copied. libplacebo composites it in the same pass that draws the video.
    void set_overlay(const uint8_t *rgba, int width, int height, int margin);

    // Remove the overlay, if there is one.
    void clear_overlay();

    // Wait for the GPU to be done with everything drawn so far and free everything. Call before the
    // window is destroyed; safe to call twice.
    void close();

    // Whether this build can draw into windows of this platform (Windows, and Linux if it was built with libplacebo).
    static bool is_supported();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CHIAKI_PY_VULKAN_RENDERER_H
