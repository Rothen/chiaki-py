#ifndef CHIAKI_PY_VULKAN_RENDERER_H
#define CHIAKI_PY_VULKAN_RENDERER_H

#include <cstdint>
#include <memory>

#include "frame_handler.h"

// Draws VulkanFrames of the Vulkan hardware decoder into a native window without them leaving the
// GPU: libplacebo converts the frames' NV12/P010 planes to RGB (SDR or HDR), scales them and presents
// them on the same Vulkan device the decoder decodes on. Windows only so far. Call from one thread
// (the GUI thread), and close() before the window is destroyed.
//
// The Vulkan device the decoder decodes on (see the ChiakiPySession constructor, which has libplacebo create
// it) is also the one drawn on, so a decoded frame is used where it is: libplacebo samples its NV12/P010
// planes, converts them to RGB with the frame's own colour space (SDR or HDR), scales them to the window and
// presents the result on the window's swapchain. Nothing is copied.
//
// All calls must come from one thread (not necessarily the GUI thread - the Python binding uses a
// dedicated thread of its own, since a call here can block for a while - see VulkanRenderThread),
// and close() before the window is destroyed.
class VulkanRenderer
{
public:
    // `window` is the native window to draw into: an HWND on Windows, the only platform supported so far.
    VulkanRenderer(ChiakiPySession &stream_session, uintptr_t window);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    // Draw `frame`, a VulkanFrame from the Vulkan decoder, scaled to fit the window with its aspect
    // ratio kept, and present it. Does nothing while the window has no area (is minimised). Returns
    // once the drawing is submitted, not finished; the GPU is waited for when the next frame needs it.
    void render(const VulkanFrame &frame);

    // Shows `rgba` - width x height pixels, 8 bit RGBA rows with premultiplied alpha, tightly packed - on top of the
    // video, in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared.
    // The image is copied. libplacebo composites it in the same pass that draws the video.
    void set_overlay(const uint8_t *rgba, int width, int height, int margin);

    // Remove the overlay, if there is one.
    void clear_overlay();

    // Wait for the GPU to be done with everything drawn so far and free everything. Call before the
    // window is destroyed; safe to call twice.
    void close();

    // Whether windows of this platform can be drawn into (only Windows so far).
    static bool is_supported();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CHIAKI_PY_VULKAN_RENDERER_H
