#ifndef CHIAKI_PY_CORE_ECDH_H
#define CHIAKI_PY_CORE_ECDH_H

#include "core/struct_wrapper.h"

#include <chiaki/ecdh.h>

#include <vector>

#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_core_ecdh(py::module &m);

class ECDHWrapper : public StructWrapper<ChiakiECDH>
{
public:
    using StructWrapper::StructWrapper;

    ~ECDHWrapper()
    {
        chiaki_ecdh_fini(ptr());
    }

    ChiakiErrorCode init()
    {
        return chiaki_ecdh_init(ptr());
    }

    ChiakiErrorCode get_local_pub_key(std::vector<uint8_t> &key_out, const std::vector<uint8_t> &handshake_key, std::vector<uint8_t> &sig_out)
    {
        size_t key_out_size = key_out.size();
        size_t sig_out_size = sig_out.size();
        return chiaki_ecdh_get_local_pub_key(ptr(), key_out.data(), &key_out_size, handshake_key.data(), sig_out.data(), &sig_out_size);
    }

    ChiakiErrorCode derive_secret(std::vector<uint8_t> &secret_out, const std::vector<uint8_t> &remote_key, const std::vector<uint8_t> &handshake_key, const std::vector<uint8_t> &remote_sig)
    {
        // size_t remote_key_size = remote_key.size();
        return chiaki_ecdh_derive_secret(ptr(), secret_out.data(), remote_key.data(), remote_key.size(), handshake_key.data(), remote_sig.data(), remote_sig.size());
    }

    ChiakiErrorCode set_local_key(const std::vector<uint8_t> &private_key, const std::vector<uint8_t> &public_key)
    {
        return chiaki_ecdh_set_local_key(ptr(), private_key.data(), private_key.size(), public_key.data(), public_key.size());
    }
};

#endif // CHIAKI_PY_CORE_ECDH_H