#ifndef CHIAKI_PY_UTILS_H
#define CHIAKI_PY_UTILS_H

#include <string>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <locale>
    #include <codecvt>
#endif

std::string fromLocal8Bit(const std::string &localStr);

#endif // CHIAKI_PY_UTILS_H