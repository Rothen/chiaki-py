#include "placebo_vulkan.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#ifndef _WIN32

// Only Windows so far: VulkanRenderer needs a window system integration per platform.
AVBufferRef *PlaceboVulkan::create_device(std::string &error)
{
    error = "Creating a Vulkan device with libplacebo is only supported on Windows so far";
    return nullptr;
}
PlaceboVulkan *PlaceboVulkan::from_device(const AVHWDeviceContext *) { return nullptr; }
bool PlaceboVulkan::is_supported() { return false; }

#else

extern "C"
{
#include <libavutil/hwcontext_vulkan.h>
}

namespace
{
    // libplacebo talks to stderr by default only if asked to; its warnings and errors say why a device or a
    // shader could not be made, which is worth having. CHIAKI_PY_PLACEBO_LOG=debug (or info, trace) shows more.
    pl_log_level log_level_from_environment()
    {
        char name[16];
        DWORD length = GetEnvironmentVariableA("CHIAKI_PY_PLACEBO_LOG", name, sizeof(name));
        if (length == 0 || length >= sizeof(name))
            return PL_LOG_WARN;
        if (std::strcmp(name, "trace") == 0)
            return PL_LOG_TRACE;
        if (std::strcmp(name, "debug") == 0)
            return PL_LOG_DEBUG;
        if (std::strcmp(name, "info") == 0)
            return PL_LOG_INFO;
        if (std::strcmp(name, "error") == 0)
            return PL_LOG_ERR;
        if (std::strcmp(name, "none") == 0)
            return PL_LOG_NONE;
        return PL_LOG_WARN;
    }

    void log_callback(void *, pl_log_level level, const char *message)
    {
        static const char *const names[] = {"", "fatal", "error", "warning", "info", "debug", "trace"};
        std::fprintf(stderr, "[libplacebo %s] %s\n", names[level], message);
    }

    // Called by FFmpeg once it is done with the device, after it has uninitialised its own parts of it.
    void free_device(AVHWDeviceContext *device)
    {
        delete static_cast<PlaceboVulkan *>(device->user_opaque);
        device->user_opaque = nullptr;
    }

    // FFmpeg's threads and libplacebo's share the queues: both go through libplacebo's locks.
    void lock_queue(AVHWDeviceContext *device, uint32_t family, uint32_t index)
    {
        pl_vulkan vulkan = static_cast<PlaceboVulkan *>(device->user_opaque)->vulkan;
        vulkan->lock_queue(vulkan, family, index);
    }

    void unlock_queue(AVHWDeviceContext *device, uint32_t family, uint32_t index)
    {
        pl_vulkan vulkan = static_cast<PlaceboVulkan *>(device->user_opaque)->vulkan;
        vulkan->unlock_queue(vulkan, family, index);
    }

    // The first queue family that can decode video: its index, and what codecs it can decode.
    bool find_decode_queue_family(VkPhysicalDevice physical_device, uint32_t &family, VkVideoCodecOperationFlagsKHR &codecs)
    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &count, nullptr);
        std::vector<VkQueueFamilyVideoPropertiesKHR> video(count, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR});
        std::vector<VkQueueFamilyProperties2> properties(count, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
        for (uint32_t i = 0; i < count; i++)
            properties[i].pNext = &video[i];
        vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &count, properties.data());

        for (uint32_t i = 0; i < count; i++)
        {
            if ((properties[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) && video[i].videoCodecOperations)
            {
                family = i;
                codecs = video[i].videoCodecOperations;
                return true;
            }
        }
        return false;
    }
} // namespace

PlaceboVulkan::~PlaceboVulkan()
{
    if (vulkan)
        pl_vulkan_destroy(&vulkan); // blocks until the GPU is done with everything that was submitted
    if (instance)
        pl_vk_inst_destroy(&instance);
    if (log)
        pl_log_destroy(&log);
}

bool PlaceboVulkan::is_supported() { return true; }

PlaceboVulkan *PlaceboVulkan::from_device(const AVHWDeviceContext *device)
{
    if (!device || device->type != AV_HWDEVICE_TYPE_VULKAN || device->free != &free_device)
        return nullptr;
    return static_cast<PlaceboVulkan *>(device->user_opaque);
}

AVBufferRef *PlaceboVulkan::create_device(std::string &error)
{
    auto placebo = std::make_unique<PlaceboVulkan>();

    pl_log_params log_params = pl_log_default_params;
    log_params.log_cb = log_callback;
    log_params.log_level = log_level_from_environment();
    placebo->log = pl_log_create(PL_API_VER, &log_params);
    if (!placebo->log)
    {
        error = "Failed to create libplacebo's log";
        return nullptr;
    }

    // Twitch Studio installs a Vulkan layer into every Vulkan process, which prints its whole module list to stdout every
    // time the swapchain is recreated, and that is at every step of a drag-resize. The layer's manifest has a switch to
    // turn it off; it has to be set before the instance is made. Left alone if it is set already.
    if (GetEnvironmentVariableA("DISABLE_TWITCH_VULKAN_OVERLAY", nullptr, 0) == 0)
        SetEnvironmentVariableA("DISABLE_TWITCH_VULKAN_OVERLAY", "1");

    const char *instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    pl_vk_inst_params instance_params = pl_vk_inst_default_params;
    instance_params.extensions = instance_extensions;
    instance_params.num_extensions = 2;
    placebo->instance = pl_vk_inst_create(placebo->log, &instance_params);
    if (!placebo->instance)
    {
        error = "Failed to create a Vulkan instance with the surface extensions";
        return nullptr;
    }

    // Video decoding is optional here, to be able to say what is missing rather than that no device was found.
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const char *optional_device_extensions[] = {
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
    };
    pl_vulkan_params vulkan_params = pl_vulkan_default_params;
    vulkan_params.instance = placebo->instance->instance;
    vulkan_params.get_proc_addr = placebo->instance->get_proc_addr;
    vulkan_params.extra_queues = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
    vulkan_params.extensions = device_extensions;
    vulkan_params.num_extensions = 1;
    vulkan_params.opt_extensions = optional_device_extensions;
    vulkan_params.num_opt_extensions = 4;
    placebo->vulkan = pl_vulkan_create(placebo->log, &vulkan_params);
    if (!placebo->vulkan)
    {
        error = "Failed to create a Vulkan device (libplacebo needs Vulkan 1.2 with timeline semaphores)";
        return nullptr;
    }
    const pl_vulkan vulkan = placebo->vulkan;

    uint32_t decode_family = 0;
    VkVideoCodecOperationFlagsKHR decode_codecs = 0;
    if (!find_decode_queue_family(vulkan->phys_device, decode_family, decode_codecs))
    {
        error = "The GPU has no Vulkan video decode queue";
        return nullptr;
    }

    AVBufferRef *device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
    if (!device_ref)
    {
        error = "Failed to allocate the hardware device context";
        return nullptr;
    }
    auto *device = reinterpret_cast<AVHWDeviceContext *>(device_ref->data);
    auto *hw = static_cast<AVVulkanDeviceContext *>(device->hwctx);

    hw->get_proc_addr = vulkan->get_proc_addr;
    hw->inst = vulkan->instance;
    hw->phys_dev = vulkan->phys_device;
    hw->act_dev = vulkan->device;
    hw->device_features = *vulkan->features;
    hw->enabled_inst_extensions = placebo->instance->extensions;
    hw->nb_enabled_inst_extensions = placebo->instance->num_extensions;
    hw->enabled_dev_extensions = vulkan->extensions;
    hw->nb_enabled_dev_extensions = vulkan->num_extensions;
    hw->lock_queue = lock_queue;
    hw->unlock_queue = unlock_queue;

#if FF_API_VULKAN_FIXED_QUEUES
    // FFmpeg turns these into the list of queue families it uses.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    hw->queue_family_index = vulkan->queue_graphics.index;
    hw->nb_graphics_queues = vulkan->queue_graphics.count;
    hw->queue_family_tx_index = vulkan->queue_transfer.index;
    hw->nb_tx_queues = vulkan->queue_transfer.count;
    hw->queue_family_comp_index = vulkan->queue_compute.index;
    hw->nb_comp_queues = vulkan->queue_compute.count;
    hw->queue_family_encode_index = -1;
    hw->nb_encode_queues = 0;
    hw->queue_family_decode_index = static_cast<int>(decode_family);
    hw->nb_decode_queues = 1;
#pragma clang diagnostic pop
#else
    auto add_queue_family = [hw](uint32_t index, uint32_t count, VkQueueFlagBits flags, VkVideoCodecOperationFlagsKHR codecs = 0) {
        hw->qf[hw->nb_qf++] = {static_cast<int>(index), static_cast<int>(count), flags, static_cast<VkVideoCodecOperationFlagBitsKHR>(codecs)};
    };
    add_queue_family(vulkan->queue_graphics.index, vulkan->queue_graphics.count, VK_QUEUE_GRAPHICS_BIT);
    add_queue_family(vulkan->queue_compute.index, vulkan->queue_compute.count, VK_QUEUE_COMPUTE_BIT);
    add_queue_family(vulkan->queue_transfer.index, vulkan->queue_transfer.count, VK_QUEUE_TRANSFER_BIT);
    add_queue_family(decode_family, 1, VK_QUEUE_VIDEO_DECODE_BIT_KHR, decode_codecs);
#endif

    // From here on the device context owns libplacebo's device, and gives it up when it is freed.
    device->user_opaque = placebo.release();
    device->free = free_device;
    int result = av_hwdevice_ctx_init(device_ref);
    if (result < 0)
    {
        av_buffer_unref(&device_ref); // frees the PlaceboVulkan too
        char message[AV_ERROR_MAX_STRING_SIZE];
        error = std::string("FFmpeg could not use the device for Vulkan video decoding: ") + av_make_error_string(message, sizeof(message), result);
        return nullptr;
    }
    return device_ref;
}

#endif
