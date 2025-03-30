#ifndef CHIAKI_PY_UTILS_H
#define CHIAKI_PY_UTILS_H

#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <tuple>
#include <type_traits>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <locale>
    #include <codecvt>
#endif

std::string fromLocal8Bit(const std::string &localStr);
std::vector<uint8_t> fromBase64(const std::string &base64Str);
std::vector<unsigned char> fromHex(const std::string &hex);

// Define a function type using typedef
typedef void (*ExampleFunctionType)(int, double, std::string);

// Helper to extract function parameter types
template <typename T>
struct FunctionTraits;

// Specialization for function pointers
template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t ArgCount = sizeof...(Args);
};

// Macro to generate a struct from a function typedef
#define GENERATE_STRUCT(NAME, FUNC_TYPE)                                 \
    struct NAME                                                          \
    {                                                                    \
        using ArgsTuple = typename FunctionTraits<FUNC_TYPE>::ArgsTuple; \
        static constexpr size_t ArgCount = std::tuple_size_v<ArgsTuple>; \
        template <std::size_t I>                                         \
        using ArgType = typename std::tuple_element<I, ArgsTuple>::type; \
        std::tuple<ArgType<0>, ArgType<1>, ArgType<2>> values;           \
    }

#endif // CHIAKI_PY_UTILS_H