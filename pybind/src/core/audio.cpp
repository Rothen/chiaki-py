#include "core/audio.h"

#include <chiaki/audio.h>

namespace py = pybind11;

void init_core_audio(py::module &m)
{
    m.attr("HEADER_SIZE") = CHIAKI_AUDIO_HEADER_SIZE;

    py::class_<AudioHeaderWrapper>(m, "AudioHeader")
        .def(py::init<uint8_t, uint8_t, uint32_t, uint32_t>(),
             py::arg("channels"),
             py::arg("bits"),
             py::arg("rate"),
             py::arg("frame_size"))
        .def("load", &AudioHeaderWrapper::load, py::arg("buf"), "Load audio header.")
        .def("save", &AudioHeaderWrapper::save, py::arg("buf"), "Save audio header.")
        .def("frame_buf_size", &AudioHeaderWrapper::frame_buf_size, "Get frame buffer size.");
}