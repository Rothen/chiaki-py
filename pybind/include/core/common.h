#ifndef CHIAKI_PY_CORE_COMMON_H
#define CHIAKI_PY_CORE_COMMON_H

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_common(py::module &m);

#endif // CHIAKI_PY_CORE_COMMON_H