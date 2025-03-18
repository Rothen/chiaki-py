#include "utils.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <locale>
    #include <codecvt>
#endif

std::string fromLocal8Bit(const std::string &localStr)
{
#ifdef _WIN32
    // Convert ANSI (Local Code Page) → Wide String (UTF-16)
    int wideLen = MultiByteToWideChar(CP_ACP, 0, localStr.c_str(), -1, nullptr, 0);
    if (wideLen == 0)
        return ""; // Conversion failed

    std::wstring wideStr(wideLen, 0);
    MultiByteToWideChar(CP_ACP, 0, localStr.c_str(), -1, &wideStr[0], wideLen);

    // Convert Wide String (UTF-16) → UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len == 0)
        return ""; // Conversion failed

    std::string utf8Str(utf8Len - 1, 0); // -1 to exclude the null terminator
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &utf8Str[0], utf8Len, nullptr, nullptr);

    return utf8Str;
#else
    // On Linux/macOS, local "8-bit" encoding is generally UTF-8 already
    return localStr;
#endif
}