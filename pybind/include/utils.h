#ifndef CHIAKI_PY_UTILS_H
#define CHIAKI_PY_UTILS_H

#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <locale>
    #include <codecvt>
#endif

std::string fromLocal8Bit(const std::string &localStr);
std::vector<uint8_t> fromBase64(const std::string &base64Str);
std::vector<unsigned char> fromHex(const std::string &hex);

#endif // CHIAKI_PY_UTILS_H