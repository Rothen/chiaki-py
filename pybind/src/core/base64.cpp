#include "core/base64.h"

#include <pybind11/stl.h>

namespace py = pybind11;

ChiakiErrorCode base64_encode(const std::vector<uint8_t> &in, std::vector<char> &out)
{
    return chiaki_base64_encode(in.data(), in.size(), out.data(), out.size());
}

ChiakiErrorCode base64_decode(const std::vector<char> &in, std::vector<uint8_t> &out)
{
    size_t out_size = out.size();
    return chiaki_base64_decode(in.data(), in.size(), out.data(), &out_size);
}

void init_core_base64(py::module &m)
{
    m.def("chiaki_base64_encode", &base64_encode, py::arg("in"), py::arg("out"), "Encode data to base64.");
    m.def("chiaki_base64_decode", &base64_decode, py::arg("in"), py::arg("out"), "Decode base64 data.");
}