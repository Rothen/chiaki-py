#include "core/log.h"

#include <chiaki/log.h>

namespace py = pybind11;

void init_core_log(py::module &m)
{
    py::enum_<ChiakiLogLevel>(m, "LogLevel")
        .value("DEBUG", CHIAKI_LOG_DEBUG)
        .value("VERBOSE", CHIAKI_LOG_VERBOSE)
        .value("INFO", CHIAKI_LOG_INFO)
        .value("WARNING", CHIAKI_LOG_WARNING)
        .value("ERROR", CHIAKI_LOG_ERROR)
        .export_values();
}
