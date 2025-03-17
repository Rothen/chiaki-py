#include "core/takion.h"

#include <chiaki/takion.h>

namespace py = pybind11;

void init_core_takion(py::module &m)
{
    py::enum_<ChiakiDisableAudioVideo>(m, "DisableAudioVideo")
        .value("NONE", CHIAKI_NONE_DISABLED)
        .value("AUDIO", CHIAKI_AUDIO_DISABLED)
        .value("VIDEO", CHIAKI_VIDEO_DISABLED)
        .value("AUDIO_VIDEO", CHIAKI_AUDIO_VIDEO_DISABLED)
        .export_values();
}