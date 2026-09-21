// The platform macros have to be defined before the first Vulkan header, which vulkan_renderer.h includes.
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include "vulkan_renderer.h"
#include "vulkan_shaders.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

#ifndef _WIN32

// Only Windows so far: drawing into a window needs a surface extension per window system. Elsewhere the
// class exists, so that the module has the same API, but says so when used. (The real implementation
// also relies on the queue family API of recent FFmpeg headers, which distributions do not all have.)
VulkanRenderer::VulkanRenderer(StreamSession &, uintptr_t)
{
    throw std::runtime_error("Rendering with Vulkan is only supported on Windows so far");
}
VulkanRenderer::~VulkanRenderer() {}
void VulkanRenderer::render(const GpuFrame &) { throw std::runtime_error("Rendering with Vulkan is only supported on Windows so far"); }
void VulkanRenderer::close() {}
bool VulkanRenderer::is_supported() { return false; }

#else

namespace
{
    void check(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Vulkan: ") + what + " failed (VkResult " + std::to_string(result) + ")");
    }

    bool has_extension(const char *const *names, int count, const char *wanted)
    {
        for (int i = 0; i < count; i++)
            if (std::strcmp(names[i], wanted) == 0)
                return true;
        return false;
    }

    // The frame decoders and this renderer share one VkQueue, which Vulkan wants used by one thread at a time.
    struct QueueLock
    {
        AVHWDeviceContext *device;
        AVVulkanDeviceContext *hw;
        uint32_t family, index;

        QueueLock(AVHWDeviceContext *device, AVVulkanDeviceContext *hw, uint32_t family, uint32_t index)
            : device(device), hw(hw), family(family), index(index)
        {
            if (hw->lock_queue)
                hw->lock_queue(device, family, index);
        }
        ~QueueLock()
        {
            if (hw->unlock_queue)
                hw->unlock_queue(device, family, index);
        }
    };

    // Holds a frame's properties (layout, semaphore values) still between reading and updating them.
    struct FrameLock
    {
        AVHWFramesContext *frames;
        AVVulkanFramesContext *vk_frames;
        AVVkFrame *frame;

        FrameLock(AVHWFramesContext *frames, AVVkFrame *frame)
            : frames(frames), vk_frames(static_cast<AVVulkanFramesContext *>(frames->hwctx)), frame(frame)
        {
            if (vk_frames->lock_frame)
                vk_frames->lock_frame(frames, frame);
        }
        ~FrameLock()
        {
            if (vk_frames->unlock_frame)
                vk_frames->unlock_frame(frames, frame);
        }
    };
} // namespace

struct VulkanRenderer::PushConstants
{
    YuvToRgbParams color;
    float sample_max;
    float uv_scale[2];
    float uv_max[2];
};

bool VulkanRenderer::is_supported()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

VulkanRenderer::VulkanRenderer(StreamSession &session, uintptr_t window)
{
    if (!is_supported())
        throw std::runtime_error("Rendering with Vulkan is only supported on Windows so far");

    ChiakiFfmpegDecoder *decoder = session.GetFfmpegDecoder();
    if (!decoder || !decoder->hw_device_ctx)
        throw std::runtime_error("Session is not using a hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");
    auto *device_context = reinterpret_cast<AVHWDeviceContext *>(decoder->hw_device_ctx->data);
    if (device_context->type != AV_HWDEVICE_TYPE_VULKAN)
        throw std::runtime_error(std::string("Session is using the ") + av_hwdevice_get_type_name(device_context->type) +
                                 " hardware decoder; use Settings.set_hardware_decoder(\"vulkan\")");

    device_ref = av_buffer_ref(decoder->hw_device_ctx);
    device = device_context;
    hw = static_cast<AVVulkanDeviceContext *>(device_context->hwctx);
    instance = hw->inst;
    physical_device = hw->phys_dev;
    vk_device = hw->act_dev;

    try
    {
        if (!has_extension(hw->enabled_inst_extensions, hw->nb_enabled_inst_extensions, "VK_KHR_surface") ||
            !has_extension(hw->enabled_dev_extensions, hw->nb_enabled_dev_extensions, "VK_KHR_swapchain"))
            throw std::runtime_error("The decoder's Vulkan device was created without the extensions needed to draw into a window");

        create_surface(window);
        pick_queue();
        create_static_objects();
        create_swapchain();
    }
    catch (...)
    {
        close();
        throw;
    }
}

VulkanRenderer::~VulkanRenderer()
{
    try
    {
        close();
    }
    catch (...)
    {
    }
}

void VulkanRenderer::create_surface(uintptr_t window)
{
#ifdef _WIN32
    native_window = reinterpret_cast<void *>(window);
    VkWin32SurfaceCreateInfoKHR info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    info.hinstance = GetModuleHandleW(nullptr);
    info.hwnd = reinterpret_cast<HWND>(window);
    check(vkCreateWin32SurfaceKHR(instance, &info, hw->alloc, &surface), "vkCreateWin32SurfaceKHR");
#else
    (void)window;
#endif
}

void VulkanRenderer::pick_queue()
{
    // The decoder lists the queue families it enabled; draw with the first that can do graphics and present.
    for (int i = 0; i < hw->nb_qf; i++)
    {
        const AVVulkanDeviceQueueFamily &family = hw->qf[i];
        if (!(family.flags & VK_QUEUE_GRAPHICS_BIT) || family.num < 1)
            continue;
        VkBool32 can_present = VK_FALSE;
        check(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, family.idx, surface, &can_present),
              "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (!can_present)
            continue;
        queue_family = static_cast<uint32_t>(family.idx);
        queue_index = 0;
        vkGetDeviceQueue(vk_device, queue_family, queue_index, &queue);
        return;
    }
    throw std::runtime_error("The decoder's Vulkan device has no queue that can draw into a window");
}

void VulkanRenderer::create_static_objects()
{
    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter = sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod = 0.0f;
    check(vkCreateSampler(vk_device, &sampler_info, hw->alloc, &sampler), "vkCreateSampler");

    VkDescriptorSetLayoutBinding bindings[2] = {};
    for (uint32_t i = 0; i < 2; i++)
    {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;
    check(vkCreateDescriptorSetLayout(vk_device, &layout_info, hw->alloc, &descriptor_layout), "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * kSlots};
    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.maxSets = kSlots;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    check(vkCreateDescriptorPool(vk_device, &pool_info, hw->alloc, &descriptor_pool), "vkCreateDescriptorPool");

    VkPushConstantRange push_range = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo pipeline_layout_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_range;
    check(vkCreatePipelineLayout(vk_device, &pipeline_layout_info, hw->alloc, &pipeline_layout), "vkCreatePipelineLayout");

    VkCommandPoolCreateInfo command_pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = queue_family;
    check(vkCreateCommandPool(vk_device, &command_pool_info, hw->alloc, &command_pool), "vkCreateCommandPool");

    for (Slot &slot : slots)
    {
        VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool = command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(vk_device, &allocate_info, &slot.command_buffer), "vkAllocateCommandBuffers");

        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(vk_device, &fence_info, hw->alloc, &slot.fence), "vkCreateFence");

        VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(vk_device, &semaphore_info, hw->alloc, &slot.acquired), "vkCreateSemaphore");

        VkDescriptorSetAllocateInfo set_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        set_info.descriptorPool = descriptor_pool;
        set_info.descriptorSetCount = 1;
        set_info.pSetLayouts = &descriptor_layout;
        check(vkAllocateDescriptorSets(vk_device, &set_info, &slot.descriptors), "vkAllocateDescriptorSets");
    }
}

bool VulkanRenderer::create_swapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    VkExtent2D new_extent = caps.currentExtent;
    if (new_extent.width == UINT32_MAX)
        new_extent = caps.minImageExtent; // the window does not say; not the case for Win32 windows
    if (new_extent.width == 0 || new_extent.height == 0)
    {
        destroy_swapchain(); // minimised: nothing to draw into until it has an area again
        return false;
    }

    uint32_t count = 0;
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> formats(count);
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR");
    // The video is already gamma encoded, so the target must not be an sRGB format, which would encode it again.
    VkSurfaceFormatKHR chosen = formats.at(0);
    for (const VkSurfaceFormatKHR &format : formats)
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM || (format.format == VK_FORMAT_R8G8B8A8_UNORM && chosen.format != VK_FORMAT_B8G8R8A8_UNORM))
            chosen = format;

    check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR");
    std::vector<VkPresentModeKHR> modes(count);
    check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, modes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR");
    // Mailbox never makes the (GUI) thread wait for the display; FIFO, which every implementation has, does.
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR available : modes)
        if (available == VK_PRESENT_MODE_MAILBOX_KHR)
            mode = available;

    wait_for_slots();
    {
        QueueLock lock(device, hw, queue_family, queue_index);
        check(vkQueueWaitIdle(queue), "vkQueueWaitIdle");
    }

    VkSwapchainKHR old_swapchain = swapchain;
    for (VkFramebuffer framebuffer : framebuffers)
        vkDestroyFramebuffer(vk_device, framebuffer, hw->alloc);
    framebuffers.clear();
    for (VkImageView view : swapchain_views)
        vkDestroyImageView(vk_device, view, hw->alloc);
    swapchain_views.clear();
    for (VkSemaphore semaphore : render_done)
        vkDestroySemaphore(vk_device, semaphore, hw->alloc);
    render_done.clear();

    VkSwapchainCreateInfoKHR info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        info.minImageCount = std::min(info.minImageCount, caps.maxImageCount);
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = new_extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                              ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                              : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = old_swapchain;
    VkResult created = vkCreateSwapchainKHR(vk_device, &info, hw->alloc, &swapchain);
    if (old_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(vk_device, old_swapchain, hw->alloc); // retired whether or not the new one was made
    if (created != VK_SUCCESS)
    {
        swapchain = VK_NULL_HANDLE;
        check(created, "vkCreateSwapchainKHR");
    }
    swapchain_format = chosen.format;
    extent = new_extent;
    swapchain_outdated = false;

    check(vkGetSwapchainImagesKHR(vk_device, swapchain, &count, nullptr), "vkGetSwapchainImagesKHR");
    swapchain_images.resize(count);
    check(vkGetSwapchainImagesKHR(vk_device, swapchain, &count, swapchain_images.data()), "vkGetSwapchainImagesKHR");

    if (render_pass == VK_NULL_HANDLE)
    {
        VkAttachmentDescription attachment = {};
        attachment.format = swapchain_format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // the bars around the video are the clear colour
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &reference;
        VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        pass_info.attachmentCount = 1;
        pass_info.pAttachments = &attachment;
        pass_info.subpassCount = 1;
        pass_info.pSubpasses = &subpass;
        check(vkCreateRenderPass(vk_device, &pass_info, hw->alloc, &render_pass), "vkCreateRenderPass");

        VkShaderModuleCreateInfo vert_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        vert_info.codeSize = sizeof(kVideoVertSpirv);
        vert_info.pCode = kVideoVertSpirv;
        VkShaderModuleCreateInfo frag_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        frag_info.codeSize = sizeof(kVideoFragSpirv);
        frag_info.pCode = kVideoFragSpirv;
        VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
        check(vkCreateShaderModule(vk_device, &vert_info, hw->alloc, &vert), "vkCreateShaderModule");
        VkResult frag_result = vkCreateShaderModule(vk_device, &frag_info, hw->alloc, &frag);

        VkResult pipeline_result = frag_result;
        if (frag_result == VK_SUCCESS)
        {
            VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                         {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vert;
            stages[0].pName = "main";
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = frag;
            stages[1].pName = "main";

            VkPipelineVertexInputStateCreateInfo vertex_input = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport_state.viewportCount = viewport_state.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo raster = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blend_attachment = {};
            blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = 1;
            blend.pAttachments = &blend_attachment;
            const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamic_states;

            VkGraphicsPipelineCreateInfo pipeline_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipeline_info.stageCount = 2;
            pipeline_info.pStages = stages;
            pipeline_info.pVertexInputState = &vertex_input;
            pipeline_info.pInputAssemblyState = &assembly;
            pipeline_info.pViewportState = &viewport_state;
            pipeline_info.pRasterizationState = &raster;
            pipeline_info.pMultisampleState = &multisample;
            pipeline_info.pColorBlendState = &blend;
            pipeline_info.pDynamicState = &dynamic;
            pipeline_info.layout = pipeline_layout;
            pipeline_info.renderPass = render_pass;
            pipeline_result = vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &pipeline_info, hw->alloc, &pipeline);
        }
        vkDestroyShaderModule(vk_device, vert, hw->alloc);
        if (frag != VK_NULL_HANDLE)
            vkDestroyShaderModule(vk_device, frag, hw->alloc);
        check(pipeline_result, "creating the video pipeline");
    }

    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (VkImage image : swapchain_images)
    {
        VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_format;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        check(vkCreateImageView(vk_device, &view_info, hw->alloc, &view), "vkCreateImageView");
        swapchain_views.push_back(view);

        VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &view;
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        check(vkCreateFramebuffer(vk_device, &framebuffer_info, hw->alloc, &framebuffer), "vkCreateFramebuffer");
        framebuffers.push_back(framebuffer);

        VkSemaphore semaphore = VK_NULL_HANDLE;
        check(vkCreateSemaphore(vk_device, &semaphore_info, hw->alloc, &semaphore), "vkCreateSemaphore");
        render_done.push_back(semaphore);
    }
    return true;
}

void VulkanRenderer::destroy_swapchain()
{
    if (swapchain == VK_NULL_HANDLE)
        return;
    wait_for_slots();
    {
        QueueLock lock(device, hw, queue_family, queue_index);
        vkQueueWaitIdle(queue);
    }
    for (VkFramebuffer framebuffer : framebuffers)
        vkDestroyFramebuffer(vk_device, framebuffer, hw->alloc);
    framebuffers.clear();
    for (VkImageView view : swapchain_views)
        vkDestroyImageView(vk_device, view, hw->alloc);
    swapchain_views.clear();
    for (VkSemaphore semaphore : render_done)
        vkDestroySemaphore(vk_device, semaphore, hw->alloc);
    render_done.clear();
    swapchain_images.clear();
    vkDestroySwapchainKHR(vk_device, swapchain, hw->alloc);
    swapchain = VK_NULL_HANDLE;
    extent = {0, 0};
}

void VulkanRenderer::wait_for_slots()
{
    constexpr uint64_t kTimeoutNs = 5'000'000'000ull;
    for (Slot &slot : slots)
    {
        if (slot.fence == VK_NULL_HANDLE)
            continue;
        VkResult result = vkWaitForFences(vk_device, 1, &slot.fence, VK_TRUE, kTimeoutNs);
        if (result == VK_TIMEOUT)
            throw std::runtime_error("Vulkan: the GPU did not finish drawing a frame within 5 seconds");
        check(result, "vkWaitForFences");
        release_slot(slot);
    }
}

void VulkanRenderer::release_slot(Slot &slot)
{
    for (VkImageView &view : slot.views)
    {
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(vk_device, view, hw->alloc);
        view = VK_NULL_HANDLE;
    }
    av_frame_free(&slot.frame);
}

bool VulkanRenderer::swapchain_is_stale()
{
    if (swapchain == VK_NULL_HANDLE || swapchain_outdated)
        return true;
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps) != VK_SUCCESS)
        return false;
    return caps.currentExtent.width != extent.width || caps.currentExtent.height != extent.height;
}

void VulkanRenderer::render(const GpuFrame &gpu_frame)
{
    if (closed)
        throw std::runtime_error("The renderer has been closed");

    const AVFrame *source = gpu_frame.frame;
    if (!source || source->format != AV_PIX_FMT_VULKAN || !source->hw_frames_ctx)
        throw std::runtime_error("Only frames from the Vulkan hardware decoder can be drawn");
    auto *frames = reinterpret_cast<AVHWFramesContext *>(source->hw_frames_ctx->data);
    if (frames->device_ctx != device)
        throw std::runtime_error("The frame was decoded on another Vulkan device than this renderer draws on");
    int bytes_per_sample = 0;
    switch (frames->sw_format)
    {
    case AV_PIX_FMT_NV12:
        bytes_per_sample = 1;
        break;
    case AV_PIX_FMT_P010LE:
    case AV_PIX_FMT_P016LE:
        bytes_per_sample = 2;
        break;
    default:
        throw std::runtime_error("Unsupported Vulkan frame format: " + GpuFrame::format_name(frames->sw_format));
    }
    const VkFormat *plane_formats = av_vkfmt_from_pixfmt(frames->sw_format);
    if (!plane_formats)
        throw std::runtime_error("Unsupported Vulkan frame format: " + GpuFrame::format_name(frames->sw_format));
    auto *vk_frame = reinterpret_cast<AVVkFrame *>(source->data[0]);
    if (!vk_frame)
        throw std::runtime_error("Frame has no Vulkan image");
    const bool one_image = vk_frame->img[1] == VK_NULL_HANDLE; // both planes in one multi-planar image, or one image each
    const uint32_t image_count = one_image ? 1 : 2;
    for (uint32_t i = 0; i < image_count; i++)
        if (vk_frame->queue_family[i] != VK_QUEUE_FAMILY_IGNORED && vk_frame->queue_family[i] != queue_family)
            throw std::runtime_error("The frame's image is owned exclusively by another queue family");

    if (swapchain_is_stale() && !create_swapchain())
        return;

    Slot &slot = slots[current_slot];
    constexpr uint64_t kTimeoutNs = 5'000'000'000ull;
    VkResult waited = vkWaitForFences(vk_device, 1, &slot.fence, VK_TRUE, kTimeoutNs);
    if (waited == VK_TIMEOUT)
        throw std::runtime_error("Vulkan: the GPU did not finish drawing a frame within 5 seconds");
    check(waited, "vkWaitForFences");
    release_slot(slot);

    uint32_t image_index = 0;
    VkResult acquired = vkAcquireNextImageKHR(vk_device, swapchain, UINT64_MAX, slot.acquired, VK_NULL_HANDLE, &image_index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (!create_swapchain())
            return;
        acquired = vkAcquireNextImageKHR(vk_device, swapchain, UINT64_MAX, slot.acquired, VK_NULL_HANDLE, &image_index);
    }
    if (acquired == VK_SUBOPTIMAL_KHR)
        swapchain_outdated = true; // still drawable; the swapchain is remade for the next frame
    else
        check(acquired, "vkAcquireNextImageKHR");

    try
    {
        slot.frame = av_frame_clone(source);
        if (!slot.frame)
            throw std::runtime_error("Failed to reference the frame");

        // A view of each plane, with the format of the plane rather than of the whole image.
        for (uint32_t plane = 0; plane < 2; plane++)
        {
            VkImageViewUsageCreateInfo usage = {VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
            usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.pNext = &usage;
            view_info.image = vk_frame->img[one_image ? 0 : plane];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = plane_formats[plane];
            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            if (one_image)
                aspect = plane == 0 ? VK_IMAGE_ASPECT_PLANE_0_BIT : VK_IMAGE_ASPECT_PLANE_1_BIT;
            view_info.subresourceRange = {aspect, 0, 1, 0, 1};
            check(vkCreateImageView(vk_device, &view_info, hw->alloc, &slot.views[plane]), "vkCreateImageView");
        }

        VkDescriptorImageInfo image_infos[2];
        VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        for (uint32_t plane = 0; plane < 2; plane++)
        {
            image_infos[plane] = {sampler, slot.views[plane], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            writes[plane].dstSet = slot.descriptors;
            writes[plane].dstBinding = plane;
            writes[plane].descriptorCount = 1;
            writes[plane].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[plane].pImageInfo = &image_infos[plane];
        }
        vkUpdateDescriptorSets(vk_device, 2, writes, 0, nullptr);

        // The picture is scaled to fit the window with its aspect ratio kept, centred.
        const float video_width = static_cast<float>(source->width), video_height = static_cast<float>(source->height);
        const float scale = std::min(extent.width / video_width, extent.height / video_height);
        const float width = std::max(1.0f, std::round(video_width * scale));
        const float height = std::max(1.0f, std::round(video_height * scale));
        VkViewport viewport = {std::floor((extent.width - width) / 2.0f), std::floor((extent.height - height) / 2.0f),
                               width, height, 0.0f, 1.0f};
        VkRect2D scissor = {{0, 0}, extent};

        PushConstants constants;
        constants.color = yuv_to_rgb_params(source, bytes_per_sample);
        constants.sample_max = bytes_per_sample == 2 ? 65535.0f : 255.0f;
        // The decoder's images can be larger than the picture (the coded size, padded); only sample the picture.
        constants.uv_scale[0] = video_width / static_cast<float>(frames->width);
        constants.uv_scale[1] = video_height / static_cast<float>(frames->height);
        constants.uv_max[0] = (video_width - 0.5f) / static_cast<float>(frames->width);
        constants.uv_max[1] = (video_height - 0.5f) / static_cast<float>(frames->height);

        VkCommandBuffer command_buffer = slot.command_buffer;
        check(vkResetCommandBuffer(command_buffer, 0), "vkResetCommandBuffer");

        // The frame's layout and semaphores are only valid while nobody else changes them: from reading them
        // for the barriers until they are updated after the submission.
        FrameLock frame_lock(frames, vk_frame);

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");

        VkImageMemoryBarrier2 barriers[2] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}, {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2}};
        for (uint32_t i = 0; i < image_count; i++)
        {
            barriers[i].srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            barriers[i].srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT; // whatever the decoder wrote
            barriers[i].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barriers[i].dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            barriers[i].oldLayout = vk_frame->layout[i];
            barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[i].srcQueueFamilyIndex = barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].image = vk_frame->img[i];
            barriers[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        }
        VkDependencyInfo dependency = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = image_count;
        dependency.pImageMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(command_buffer, &dependency);

        VkClearValue clear;
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo pass_begin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        pass_begin.renderPass = render_pass;
        pass_begin.framebuffer = framebuffers.at(image_index);
        pass_begin.renderArea = {{0, 0}, extent};
        pass_begin.clearValueCount = 1;
        pass_begin.pClearValues = &clear;
        vkCmdBeginRenderPass(command_buffer, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &slot.descriptors, 0, nullptr);
        vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(constants), &constants);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffer);
        check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

        // Wait for the decoder to be done with the frame, and let the next user wait for this drawing to be.
        VkSemaphoreSubmitInfo waits[3] = {{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                                          {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                                          {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO}};
        VkSemaphoreSubmitInfo signals[3] = {{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                                            {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO},
                                            {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO}};
        waits[0].semaphore = slot.acquired;
        waits[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        signals[0].semaphore = render_done.at(image_index);
        signals[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        for (uint32_t i = 0; i < image_count; i++)
        {
            waits[1 + i].semaphore = vk_frame->sem[i];
            waits[1 + i].value = vk_frame->sem_value[i];
            waits[1 + i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            signals[1 + i].semaphore = vk_frame->sem[i];
            signals[1 + i].value = vk_frame->sem_value[i] + 1;
            signals[1 + i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }
        VkCommandBufferSubmitInfo command_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        command_info.commandBuffer = command_buffer;
        VkSubmitInfo2 submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit.waitSemaphoreInfoCount = 1 + image_count;
        submit.pWaitSemaphoreInfos = waits;
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &command_info;
        submit.signalSemaphoreInfoCount = 1 + image_count;
        submit.pSignalSemaphoreInfos = signals;

        check(vkResetFences(vk_device, 1, &slot.fence), "vkResetFences");
        VkResult submitted;
        {
            QueueLock queue_lock(device, hw, queue_family, queue_index);
            submitted = vkQueueSubmit2(queue, 1, &submit, slot.fence);
        }
        if (submitted != VK_SUCCESS)
        {
            // Nothing was submitted, so nothing will signal the fence.
            VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkDestroyFence(vk_device, slot.fence, hw->alloc);
            slot.fence = VK_NULL_HANDLE;
            check(vkCreateFence(vk_device, &fence_info, hw->alloc, &slot.fence), "vkCreateFence");
            check(submitted, "vkQueueSubmit2");
        }

        for (uint32_t i = 0; i < image_count; i++)
        {
            vk_frame->layout[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vk_frame->access[i] = VK_ACCESS_SHADER_READ_BIT;
            vk_frame->sem_value[i] += 1;
        }
    }
    catch (...)
    {
        // The acquired image and its semaphore were never used: start over with a fresh swapchain and semaphore.
        swapchain_outdated = true;
        vkDestroySemaphore(vk_device, slot.acquired, hw->alloc);
        VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        slot.acquired = VK_NULL_HANDLE;
        vkCreateSemaphore(vk_device, &semaphore_info, hw->alloc, &slot.acquired);
        throw;
    }
    current_slot = (current_slot + 1) % kSlots;

    VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_done.at(image_index);
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &image_index;
    VkResult presented;
    {
        QueueLock queue_lock(device, hw, queue_family, queue_index);
        presented = vkQueuePresentKHR(queue, &present);
    }
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR)
        swapchain_outdated = true;
    else
        check(presented, "vkQueuePresentKHR");
}

void VulkanRenderer::close()
{
    if (closed)
        return;
    closed = true;

    if (vk_device != VK_NULL_HANDLE)
    {
        try
        {
            wait_for_slots();
            if (queue != VK_NULL_HANDLE)
            {
                QueueLock lock(device, hw, queue_family, queue_index);
                vkQueueWaitIdle(queue);
            }
        }
        catch (...)
        {
            // Destroy regardless; a GPU that stopped answering is not something to wait for again.
        }

        for (Slot &slot : slots)
        {
            release_slot(slot);
            if (slot.fence != VK_NULL_HANDLE)
                vkDestroyFence(vk_device, slot.fence, hw->alloc);
            if (slot.acquired != VK_NULL_HANDLE)
                vkDestroySemaphore(vk_device, slot.acquired, hw->alloc);
            slot = Slot();
        }
        for (VkFramebuffer framebuffer : framebuffers)
            vkDestroyFramebuffer(vk_device, framebuffer, hw->alloc);
        framebuffers.clear();
        for (VkImageView view : swapchain_views)
            vkDestroyImageView(vk_device, view, hw->alloc);
        swapchain_views.clear();
        for (VkSemaphore semaphore : render_done)
            vkDestroySemaphore(vk_device, semaphore, hw->alloc);
        render_done.clear();
        if (swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(vk_device, swapchain, hw->alloc);
        swapchain = VK_NULL_HANDLE;
        if (command_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_device, command_pool, hw->alloc); // frees the slots' command buffers
        command_pool = VK_NULL_HANDLE;
        if (pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(vk_device, pipeline, hw->alloc);
        pipeline = VK_NULL_HANDLE;
        if (pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(vk_device, pipeline_layout, hw->alloc);
        pipeline_layout = VK_NULL_HANDLE;
        if (render_pass != VK_NULL_HANDLE)
            vkDestroyRenderPass(vk_device, render_pass, hw->alloc);
        render_pass = VK_NULL_HANDLE;
        if (descriptor_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(vk_device, descriptor_pool, hw->alloc); // frees the slots' descriptor sets
        descriptor_pool = VK_NULL_HANDLE;
        if (descriptor_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(vk_device, descriptor_layout, hw->alloc);
        descriptor_layout = VK_NULL_HANDLE;
        if (sampler != VK_NULL_HANDLE)
            vkDestroySampler(vk_device, sampler, hw->alloc);
        sampler = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, surface, hw->alloc);
    surface = VK_NULL_HANDLE;
    av_buffer_unref(&device_ref); // the device stays alive for as long as the decoder has it
}

#endif // _WIN32
