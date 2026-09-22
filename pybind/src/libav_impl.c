// libplacebo's conversion of libav frames (pl_map_avframe_ex and friends) is implemented in its header, and only
// when that is included from C. So this file exists to have it compiled once; C++ files include the header
// with PL_LIBAV_IMPLEMENTATION set to 0, to just use it.
#include <vulkan/vulkan.h> // before the header, for it to support Vulkan frames

#define PL_LIBAV_IMPLEMENTATION 1
#include <libplacebo/utils/libav.h>
