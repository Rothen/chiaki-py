#ifndef CHIAKI_PY_PLACEBO_VULKAN_H
#define CHIAKI_PY_PLACEBO_VULKAN_H

#include <string>

extern "C"
{
#include <libavutil/hwcontext.h>
}

#ifdef _WIN32
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <vulkan/vulkan.h>
#include <libplacebo/log.h>
#include <libplacebo/vulkan.h>
#endif

struct PlaceboVulkan
{
#ifdef _WIN32
    pl_log log = nullptr;
    pl_vk_inst instance = nullptr;
    pl_vulkan vulkan = nullptr;

    PlaceboVulkan() = default;
    PlaceboVulkan(const PlaceboVulkan &) = delete;
    PlaceboVulkan &operator=(const PlaceboVulkan &) = delete;
    ~PlaceboVulkan();
#endif

    static AVBufferRef *create_device(std::string &error);

    static PlaceboVulkan *from_device(const AVHWDeviceContext *device);

    static bool is_supported();
};

#endif // CHIAKI_PY_PLACEBO_VULKAN_H
