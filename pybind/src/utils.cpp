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

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::vector<uint8_t> fromBase64(const std::string &base64Str)
{
    std::vector<uint8_t> decoded;
    int val = 0, valb = -8;

    for (char c : base64Str)
    {
        if (c == '=')
            break;
        int pos = base64_chars.find(c);
        if (pos == std::string::npos)
            continue;

        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0)
        {
            decoded.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return decoded;
}

std::vector<unsigned char> fromHex(const std::string &hex)
{
    if (hex.size() % 2 != 0)
    {
        throw std::invalid_argument("Hex string length must be even");
    }

    std::vector<unsigned char> result;
    result.reserve(hex.size() / 2);

    auto hexCharToInt = [](char c) -> unsigned char
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character");
    };

    for (size_t i = 0; i < hex.size(); i += 2)
    {
        unsigned char high = hexCharToInt(hex[i]);
        unsigned char low = hexCharToInt(hex[i + 1]);
        result.push_back((high << 4) | low);
    }

    return result;
}