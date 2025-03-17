// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_PY_EXCEPTION_H
#define CHIAKI_PY_EXCEPTION_H

#include <exception>
#include <string>

class Exception : public std::exception
{
private:
    std::string msg;

public:
    explicit Exception(const std::string &msg) : msg(msg) {}
    const char *what() const noexcept override { return msg.c_str(); }
};

#endif // CHIAKI_PY_EXCEPTION_H
