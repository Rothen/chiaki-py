// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_PYBIND_H
#define CHIAKI_PY_PYBIND_H

#include <chiaki/common.h>
#include <chiaki/log.h>
#ifdef __cplusplus
extern "C"
{
#endif

    CHIAKI_EXPORT ChiakiErrorCode chiaki_pybind_discover(ChiakiLog *log, const char *host, const float timeout);
    CHIAKI_EXPORT ChiakiErrorCode chiaki_pybind_wakeup(ChiakiLog *log, const char *host, const char *registkey, bool ps5);

#ifdef __cplusplus
}
#endif

#endif //CHIAKI_PY_PYBIND_H
