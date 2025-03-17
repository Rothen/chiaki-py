#include "core/common.h"

#include <chiaki/common.h>

namespace py = pybind11;

void init_core_common(py::module &m)
{

    py::enum_<ChiakiErrorCode>(m, "ErrorCode")
        .value("SUCCESS", CHIAKI_ERR_SUCCESS)
        .value("UNKNOWN", CHIAKI_ERR_UNKNOWN)
        .value("PARSE_ADDR", CHIAKI_ERR_PARSE_ADDR)
        .value("THREAD", CHIAKI_ERR_THREAD)
        .value("MEMORY", CHIAKI_ERR_MEMORY)
        .value("OVERFLOW", CHIAKI_ERR_OVERFLOW)
        .value("NETWORK", CHIAKI_ERR_NETWORK)
        .value("CONNECTION_REFUSED", CHIAKI_ERR_CONNECTION_REFUSED)
        .value("HOST_DOWN", CHIAKI_ERR_HOST_DOWN)
        .value("HOST_UNREACH", CHIAKI_ERR_HOST_UNREACH)
        .value("DISCONNECTED", CHIAKI_ERR_DISCONNECTED)
        .value("INVALID_DATA", CHIAKI_ERR_INVALID_DATA)
        .value("BUF_TOO_SMALL", CHIAKI_ERR_BUF_TOO_SMALL)
        .value("MUTEX_LOCKED", CHIAKI_ERR_MUTEX_LOCKED)
        .value("CANCELED", CHIAKI_ERR_CANCELED)
        .value("TIMEOUT", CHIAKI_ERR_TIMEOUT)
        .value("INVALID_RESPONSE", CHIAKI_ERR_INVALID_RESPONSE)
        .value("INVALID_MAC", CHIAKI_ERR_INVALID_MAC)
        .value("UNINITIALIZED", CHIAKI_ERR_UNINITIALIZED)
        .value("FEC_FAILED", CHIAKI_ERR_FEC_FAILED)
        .value("VERSION_MISMATCH", CHIAKI_ERR_VERSION_MISMATCH)
        .value("HTTP_NONOK", CHIAKI_ERR_HTTP_NONOK)
        .export_values();

    m.def("chiaki_error_string", &chiaki_error_string, py::arg("error_code"), "Get the error string.");
    m.def("chiaki_aligned_alloc", &chiaki_aligned_alloc, py::arg("alignment"), py::arg("size"), "Allocate aligned memory.");
    m.def("chiaki_aligned_free", &chiaki_aligned_free, py::arg("ptr"), "Free aligned memory.");

    py::enum_<ChiakiTarget>(m, "Target")
        .value("PS4_UNKNOWN", CHIAKI_TARGET_PS4_UNKNOWN)
        .value("PS4_8", CHIAKI_TARGET_PS4_8)
        .value("PS4_9", CHIAKI_TARGET_PS4_9)
        .value("PS4_10", CHIAKI_TARGET_PS4_10)
        .value("PS5_UNKNOWN", CHIAKI_TARGET_PS5_UNKNOWN)
        .value("PS5_1", CHIAKI_TARGET_PS5_1)
        .export_values();

    m.def("chiaki_lib_init", &chiaki_lib_init, "Initialize the Chiaki library.");

    py::enum_<ChiakiCodec>(m, "Codec")
        .value("H264", CHIAKI_CODEC_H264)
        .value("H265", CHIAKI_CODEC_H265)
        .value("H265_HDR", CHIAKI_CODEC_H265_HDR)
        .export_values();

    m.def("chiaki_codec_name", &chiaki_codec_name, py::arg("codec"), "Get the codec name.");
}