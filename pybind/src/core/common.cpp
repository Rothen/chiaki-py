#include "core/common.h"

#include <chiaki/common.h>

namespace py = pybind11;

void init_core_common(py::module &m)
{
    py::enum_<ChiakiTarget>(m, "Target")
        .value("PS4_UNKNOWN", CHIAKI_TARGET_PS4_UNKNOWN)
        .value("PS4_8", CHIAKI_TARGET_PS4_8)
        .value("PS4_9", CHIAKI_TARGET_PS4_9)
        .value("PS4_10", CHIAKI_TARGET_PS4_10)
        .value("PS5_UNKNOWN", CHIAKI_TARGET_PS5_UNKNOWN)
        .value("PS5_1", CHIAKI_TARGET_PS5_1)
        .export_values();

    py::enum_<ChiakiCodec>(m, "Codec")
        .value("H264", CHIAKI_CODEC_H264)
        .value("H265", CHIAKI_CODEC_H265)
        .value("H265_HDR", CHIAKI_CODEC_H265_HDR)
        .export_values();
}
