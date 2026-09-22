#ifndef CHIAKI_PY_VULKAN_RENDERER_H
#define CHIAKI_PY_VULKAN_RENDERER_H

#include <cstdint>
#include <memory>

#include "frame_handler.h"

// Draws the frames of a VulkanFrameHandler into a window without them ever leaving the GPU, using libplacebo.
//
// The Vulkan device the decoder decodes on (see the StreamSession constructor, which has libplacebo create
// it) is also the one drawn on, so a decoded frame is used where it is: libplacebo samples its NV12/P010
// planes, converts them to RGB with the frame's own colour space (SDR or HDR), scales them to the window and
// presents the result on the window's swapchain. Nothing is copied.
//
// All calls must come from one thread (the GUI thread), and close() before the window is destroyed.
class VulkanRenderer
{
public:
    // `window` is the native window to draw into: an HWND on Windows, the only platform supported so far.
    VulkanRenderer(StreamSession &session, uintptr_t window);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;

    // Draws `frame` scaled to fit the window, letterboxed, and presents it. Returns without drawing if
    // the window has no area (is minimised). Waits for the frame's decoding to have finished on the GPU,
    // not on the CPU.
    void render(const VulkanFrame &frame);

    // Shows `rgba` - width x height pixels, 8 bit RGBA rows with premultiplied alpha, tightly packed - on top of the
    // video, in the top-right corner of the window, `margin` pixels from its edges, until it is replaced or cleared.
    // The image is copied. libplacebo composites it in the same pass that draws the video.
    void set_overlay(const uint8_t *rgba, int width, int height, int margin);
    void clear_overlay();

    // Waits for the GPU to be done with everything drawn so far and frees all resources. Safe to call twice.
    void close();

    // Whether a window of this platform can be drawn into at all.
    static bool is_supported();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CHIAKI_PY_VULKAN_RENDERER_H
