#include "core/ecdh.h"

#include <chiaki/ecdh.h>

namespace py = pybind11;

void init_core_ecdh(py::module &m)
{
    m.attr("SECRET_SIZE") = CHIAKI_ECDH_SECRET_SIZE;

    py::class_<ECDHWrapper>(m, "ECDH")
        .def(py::init<>())
        .def("init", &ECDHWrapper::init, "Initialize ECDH.")
        .def("get_local_pub_key", &ECDHWrapper::get_local_pub_key, py::arg("key_out"), py::arg("handshake_key"), py::arg("sig_out"), "Get local public key.")
        .def("derive_secret", &ECDHWrapper::derive_secret, py::arg("secret_out "), py::arg("remote_key"), py::arg("handshake_key"), py::arg("remote_sig"), "Derive secret.")
        .def("set_local_key", &ECDHWrapper::set_local_key, py::arg("private_key"), py::arg("public_key"), "Set local key.");
}