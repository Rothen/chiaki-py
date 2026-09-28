#ifndef CHIAKI_PY_PLACEBO_VULKAN_H
#define CHIAKI_PY_PLACEBO_VULKAN_H

#include <string>

extern "C"
{
#include <libavutil/hwcontext.h>
}

// CHIAKI_PY_HAS_PLACEBO is set by the build where libplacebo is linked: always on Windows, on Linux if it was found.
#ifdef CHIAKI_PY_HAS_PLACEBO
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
#endif
// No window system headers on Linux (Xlib's macros would leak into everything that includes this): the files
// that create surfaces include them.

#include <vulkan/vulkan.h>
#include <libplacebo/log.h>
#include <libplacebo/vulkan.h>
#endif

struct PlaceboVulkan
{
#ifdef CHIAKI_PY_HAS_PLACEBO
    pl_log log = nullptr;
    pl_vk_inst instance = nullptr;
    pl_vulkan vulkan = nullptr;

    PlaceboVulkan() = default;
    PlaceboVulkan(const PlaceboVulkan &) = delete;
    PlaceboVulkan &operator=(const PlaceboVulkan &) = delete;
    ~PlaceboVulkan();

    // Whether the instance was created with the instance extension `name` (e.g. a window system's surface extension).
    bool has_instance_extension(const char *name) const;
#endif

    static AVBufferRef *create_device(std::string &error);

    static PlaceboVulkan *from_device(const AVHWDeviceContext *device);

    static bool is_supported();
};

#endif // CHIAKI_PY_PLACEBO_VULKAN_H
