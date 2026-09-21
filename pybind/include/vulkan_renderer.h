#ifndef CHIAKI_PY_VULKAN_RENDERER_H
#define CHIAKI_PY_VULKAN_RENDERER_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "frame_handler.h"

extern "C"
{
#include <libavutil/hwcontext_vulkan.h>
}

// Draws the frames of a GPUFrameHandler into a window without them ever leaving the GPU.
//
// The Vulkan device the decoder decodes on (see the StreamSession constructor, which creates it with
// the surface and swapchain extensions) is also the one drawn on, so a decoded frame is used where it
// is: its NV12/P010 planes are sampled by a fragment shader that converts them to RGB, and the result
// goes to the window's swapchain. Nothing is copied.
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
    void render(const GpuFrame &frame);

    // Waits for the GPU to be done with everything drawn so far and frees all resources. Safe to call twice.
    void close();

    // Whether a window of this platform can be drawn into at all.
    static bool is_supported();

private:
    static constexpr int kSlots = 2; // frames that may be in flight at once

    // Everything needed to have one frame drawn and presented while the next one is being prepared.
    struct Slot
    {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE; // signalled once the GPU is done with the slot; created signalled
        VkSemaphore acquired = VK_NULL_HANDLE;
        VkDescriptorSet descriptors = VK_NULL_HANDLE;
        std::array<VkImageView, 2> views = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        AVFrame *frame = nullptr; // keeps the decoder's pool slot (and so the image) alive while the GPU reads it
    };

    struct PushConstants;

    void create_surface(uintptr_t window);
    void pick_queue();
    void create_static_objects();
    bool create_swapchain(); // false if the window has no area
    void destroy_swapchain();
    void wait_for_slots();
    void release_slot(Slot &slot);
    bool swapchain_is_stale();

    AVBufferRef *device_ref = nullptr;
    AVHWDeviceContext *device = nullptr;
    AVVulkanDeviceContext *hw = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice vk_device = VK_NULL_HANDLE;

    uint32_t queue_family = 0;
    uint32_t queue_index = 0;
    VkQueue queue = VK_NULL_HANDLE;

    void *native_window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_views;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkSemaphore> render_done; // one per swapchain image, waited on by its present

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;

    std::array<Slot, kSlots> slots;
    int current_slot = 0;
    bool swapchain_outdated = false;
    bool closed = false;
};

#endif // CHIAKI_PY_VULKAN_RENDERER_H
