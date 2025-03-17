#ifndef CHIAKI_PY_CORE_BASE64_H
#define CHIAKI_PY_CORE_BASE64_H

#include "core/common.h"

#include <chiaki/base64.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_base64(py::module &m);

ChiakiErrorCode base64_encode(const std::vector<uint8_t> &in, std::vector<char> &out);
ChiakiErrorCode base64_decode(const std::vector<char> &in, std::vector<uint8_t> &out);

#endif // CHIAKI_PY_CORE_BASE64_H