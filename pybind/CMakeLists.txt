function(link_third_party_libs target)
    if(WIN32)
        target_link_libraries(${target} PRIVATE
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/jerasure.lib
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/gf_complete.lib
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/curl/lib/libcurl-d.lib
            wsock32 ws2_32 bcrypt iphlpapi
        )
    else()
        target_link_libraries(${target} PRIVATE
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/libjerasure.a
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/libgf_complete.a
            ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/curl/lib/libcurl-d.a
        )
    endif()
endfunction()

if(NOT DEPS_FOLDER)
    set(DEPS_FOLDER ${CMAKE_SOURCE_DIR}/deps)
endif(NOT DEPS_FOLDER)

# Include directories
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(fmt REQUIRED)
find_package(OpenSSL REQUIRED)

if(MSVC)
    add_compile_options(
        /std:c++17
        $<$<CONFIG:RELEASE>:/O2>
        $<$<CONFIG:RELWITHDEBINFO>:/O2>
        $<$<CONFIG:DEBUG>:/Od>
    )
else()
    add_compile_options(
        -std=c++17
        $<$<CONFIG:RELEASE>:-Ofast>
        $<$<CONFIG:RELEASE>:-march=native>
        $<$<CONFIG:RELWITHDEBINFO>:-Ofast>
        $<$<CONFIG:RELWITHDEBINFO>:-march=native>
        $<$<CONFIG:DEBUG>:-O0>
        $<$<CONFIG:DEBUG>:-ggdb3>
    )
endif(MSVC)


set(PYBIND11_PYTHON_VERSION 3.11)
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    SOURCE_DIR "${DEPS_FOLDER}/pybind11"
    GIT_TAG v2.13.6
)
FetchContent_MakeAvailable(pybind11)
include_directories(${DEPS_FOLDER}/pybind11)
include_directories(${CMAKE_SOURCE_DIR}/pybind/include)

# Source files
set(SOURCES
    include/core/common.h
    include/core/audio.h
    include/core/base64.h
    include/core/bitstream.h
    include/core/controller.h
    # include/core/discovery_service.h
    include/core/ecdh.h
    include/core/fec.h
    include/core/feedback.h
    include/core/log.h
    include/backend.h
    include/controllermanager.h
    include/elapsed_timer.h
    include/exception.h
    include/host.h
    include/event_source.h
    include/sessionlog.h
    include/settings.h
    include/streamsession.h
    include/discovery_manager.h
    include/timer.h
    include/utils.h
    src/core/audio.cpp
    src/core/base64.cpp
    src/core/bitstream.cpp
    src/core/common.cpp
    src/core/controller.cpp
    # src/core/discovery_service.cpp
    src/core/ecdh.cpp
    src/core/fec.cpp
    src/core/feedback.cpp
    src/core/log.cpp
    src/backend.cpp
    src/host.cpp
    src/event_source.cpp
    src/controllermanager.cpp
    src/sessionlog.cpp
    src/settings.cpp
    src/streamsession.cpp
    src/discovery_manager.cpp
    src/utils.cpp
    src/bindings.cpp
)

# Create the shared library (Python module)
pybind11_add_module(chiaki-py ${SOURCES})
set_target_properties(chiaki-py PROPERTIES OUTPUT_NAME "chiaki_py")

# Build straight into chiaki_py/lib/ - the source-tree package that
# re-exports this compiled module (see chiaki_py/lib/__init__.py) - instead
# of the default build*/pybind/, so that plain `from .chiaki_py import *`
# there resolves it as an ordinary same-directory submodule. Set both
# LIBRARY_ (used for the .so on Linux/macOS) and RUNTIME_OUTPUT_DIRECTORY
# (used for the .pyd on Windows, where extension modules link like DLLs),
# plus their per-config variants for multi-config generators (e.g. the
# Visual Studio generator, which otherwise appends a Debug/Release
# subdirectory) - single-config generators like Ninja ignore the latter.
set_target_properties(chiaki-py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
)
foreach(_config_type ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${_config_type} _config_type_upper)
    set_target_properties(chiaki-py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${_config_type_upper} "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
        RUNTIME_OUTPUT_DIRECTORY_${_config_type_upper} "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
    )
endforeach()

set(FFMPEG_LIBRARIES
    ${CMAKE_SOURCE_DIR}/deps/lib/avcodec.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avformat.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avutil.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/swscale.lib
)

set(FFMPEG_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/deps/include
)

# Find dependencies from vcpkg
find_package(FFTW3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)

find_library(JSON_C_LIB json-c REQUIRED)
find_library(MINIUPNPC_LIB miniupnpc REQUIRED)
find_library(OPUS_LIB opus REQUIRED)
find_library(LCMS2_LIB lcms2 REQUIRED)
find_library(VULKAN_LIB vulkan-1 REQUIRED)
find_library(ZLIB_LIB zlib REQUIRED)
find_library(HIDAPI_LIB hidapi REQUIRED)

target_link_libraries(chiaki-py PRIVATE CURL::libcurl_static)
target_link_libraries(chiaki-py PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/lib/avcodec.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avformat.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avutil.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/swscale.lib
)
# Link against everything
target_link_libraries(chiaki-py PRIVATE
    FFTW3::fftw3
    OpenSSL::Crypto
    OpenSSL::SSL
    protobuf::libprotobuf
    SDL2::SDL2
    ${JSON_C_LIB}
    ${MINIUPNPC_LIB}
    ${OPUS_LIB}
    ${LCMS2_LIB}
    ${VULKAN_LIB}
    ${ZLIB_LIB}
    ${HIDAPI_LIB}
)
# include_directories(SYSTEM BEFORE "${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include")

# Ensure that CMake correctly finds FFmpeg
# include_directories(" C:/Users/benir/Documents/Projects/chiaki-py/libs/chiaki-ng/lib/include ")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    link_directories("${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib")
else()
    link_directories("${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build/lib")
endif()

# Link against chiaki
target_link_libraries(chiaki-py PRIVATE fmt::fmt)
target_link_libraries(chiaki-py PRIVATE ${FFMPEG_LIBRARIES})
target_include_directories(chiaki-py PRIVATE ${FFMPEG_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_search_module(json-c REQUIRED json-c IMPORTED_TARGET)
target_include_directories(chiaki-py PRIVATE ${json-c_INCLUDE_DIRS})
target_link_libraries(chiaki-py PRIVATE PkgConfig::json-c)

find_package(Opus REQUIRED)
include_directories(${Opus_INCLUDE_DIRS})

find_package(Threads REQUIRED)
target_link_libraries(chiaki-py PRIVATE Threads::Threads)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_library(chiaki-lib STATIC IMPORTED)
set_target_properties(chiaki-lib PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(chiaki-lib PROPERTIES
            IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/chiaki.lib
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
        )
    else()
        set_target_properties(chiaki-lib PROPERTIES
            IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build/lib/chiaki.lib
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
        )
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_library(chiaki-lib STATIC IMPORTED)
        set_target_properties(chiaki-lib PROPERTIES
            IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/libchiaki.a
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
        )
    else()
        add_library(chiaki-lib STATIC IMPORTED)
        set_target_properties(chiaki-lib PROPERTIES
            IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build/lib/libchiaki.a
            INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
        )
    endif()
endif()

target_link_libraries(chiaki-py PRIVATE chiaki-lib OpenSSL::Crypto)
link_third_party_libs(chiaki-py)
target_link_libraries(chiaki-py PRIVATE Nanopb::nanopb)
target_link_libraries(chiaki-py PRIVATE Jerasure::Jerasure)

# The FFmpeg .dlls this module needs at runtime (it decodes video via them
# directly - see streamsession.cpp) aren't vcpkg-managed: FFMPEG_LIBRARIES
# above links them from vendored import libs under deps/lib/, so vcpkg's own
# POST_BUILD step (which copies a target's DLL dependencies by scanning only
# vcpkg_installed/.../bin) can't find or copy them. Copy them here instead,
# next to the built extension - without this, linking succeeds but importing
# chiaki_py fails with "DLL load failed".
file(GLOB FFMPEG_RUNTIME_DLLS
    "${CMAKE_SOURCE_DIR}/deps/bin/avcodec-*.dll"
    "${CMAKE_SOURCE_DIR}/deps/bin/avformat-*.dll"
    "${CMAKE_SOURCE_DIR}/deps/bin/avutil-*.dll"
    "${CMAKE_SOURCE_DIR}/deps/bin/swscale-*.dll"
    "${CMAKE_SOURCE_DIR}/deps/bin/swresample-*.dll"
)
add_custom_command(TARGET chiaki-py POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG_RUNTIME_DLLS} "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
    COMMENT " Copying FFmpeg runtime DLLs into chiaki_py/lib/ "
)

# Regenerate the .pyi stubs into chiaki_py/lib/ (the source-tree package
# that re-exports this compiled module - see chiaki_py/lib/__init__.py), so
# `from .chiaki_py import *` there resolves against real stub coverage for
# static type checkers/IDE autocomplete instead of the dynamic star-import
# being opaque to them. Uses ${PYTHON_EXECUTABLE} (the exact interpreter
# this extension was built for, discovered by pybind11 itself) rather than
# a bare `python` - depending on PATH order, `python` can resolve to
# Windows' "install from the Store" app-execution-alias stub instead of a
# real interpreter, which silently no-ops here without `${PYTHON_EXECUTABLE}`.
add_custom_command(TARGET chiaki-py POST_BUILD
    VERBATIM COMMAND "${PYTHON_EXECUTABLE}" -c "import os; import sys; sys.path.insert(1, os.getcwd()) ; from pybind11_stubgen import main; import chiaki_py; sys.argv[0] = 'pybind11-stubgen'; sys.argv.append('--ignore-all-errors') ; sys.argv.append('-o') ; sys.argv.append('${CMAKE_SOURCE_DIR}/chiaki_py/lib') ; sys.argv.append('chiaki_py') ; main() " || (exit 0)
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/chiaki_py/lib
    COMMENT " Generating chiaki_py .pyi stubs into chiaki_py/lib/ "
)

