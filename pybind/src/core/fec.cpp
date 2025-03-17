#include "core/fec.h"
#include "core/common.h"

#include <chiaki/fec.h>

namespace py = pybind11;

void init_core_fec(py::module &m)
{
    m.attr("WORDSIZE") = CHIAKI_FEC_WORDSIZE;

    m.def("chiaki_fec_decode", &chiaki_fec_decode,
          py::arg("frame_buf"),
          py::arg("unit_size"),
          py::arg("stride"),
          py::arg("k"),
          py::arg("m"),
          py::arg("erasures"),
          py::arg("erasures_count"),
          "Decode a frame using Forward Error Correction.");
    m.def("chiaki_fec_encode", &chiaki_fec_encode,
          py::arg("frame_buf"),
          py::arg("unit_size"),
          py::arg("stride"),
          py::arg("k"),
          py::arg("m"),
          "Encode a frame using Forward Error Correction.");
}