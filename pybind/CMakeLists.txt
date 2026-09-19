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

if(NOT WIN32)
    # By default CMake bakes an RPATH/RUNPATH into the built .so pointing at
    # the directory of every linked library it found via find_package/
    # find_library - including Python's own lib/ directory (wherever
    # PYTHON_EXECUTABLE's interpreter lives, e.g. a venv or conda env). That
    # directory search happens *before* the system's standard library paths,
    # so if that Python environment happens to also ship its own copies of
    # things like libssl (conda's `python` package commonly does, often an
    # older one than the system's), the extension silently loads the
    # env-bundled version instead of the system one it was actually linked
    # against and built for - observed as e.g. "version `OPENSSL_3.2.0' not
    # found" at import time despite the build itself succeeding.
    #
    # None of that RPATH is actually needed here: extension modules don't
    # need their own path to libpython on Linux (the symbols it needs are
    # already present in the running interpreter process; the loader reuses
    # that instance by SONAME rather than searching RPATH for it), and every
    # other dependency (FFmpeg, OpenSSL, SDL2, ...) is a normal system
    # library resolved through the standard ldconfig search path. Skipping
    # RPATH generation entirely leaves resolution to that standard search
    # path, avoiding the shadowing above.
    set_target_properties(chiaki-py PROPERTIES SKIP_BUILD_RPATH TRUE)
endif()

# FFmpeg: on Windows this repo uses prebuilt import libs/headers under
# deps/ (downloaded there at configure time by the top-level CMakeLists.txt
# when missing - vcpkg doesn't manage FFmpeg here). On
# Linux/macOS, link against the system/distro FFmpeg dev packages instead
# (e.g. libavcodec-dev, libavformat-dev, libavutil-dev, libswscale-dev on
# Ubuntu/Debian) via pkg-config, which is how chiaki-ng itself finds FFmpeg.
if(WIN32)
    set(FFMPEG_LIBRARIES
        ${CMAKE_SOURCE_DIR}/deps/lib/avcodec.lib
        ${CMAKE_SOURCE_DIR}/deps/lib/avformat.lib
        ${CMAKE_SOURCE_DIR}/deps/lib/avutil.lib
        ${CMAKE_SOURCE_DIR}/deps/lib/swscale.lib
    )
    set(FFMPEG_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/deps/include
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)
    set(FFMPEG_LIBRARIES PkgConfig::FFMPEG)
    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
endif()

# Find dependencies from vcpkg (Windows) / the system package manager (Linux/macOS)
if(WIN32)
    # vcpkg's fftw3 port ships a CMake package config.
    find_package(FFTW3 REQUIRED)
    set(FFTW3_TARGET FFTW3::fftw3)
else()
    # Debian/Ubuntu's libfftw3-dev only ships a pkg-config file, not a CMake
    # package config - vcpkg's fftw3 port is the only place FFTW3Config.cmake
    # comes from.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
    set(FFTW3_TARGET PkgConfig::FFTW3)
endif()
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)

find_library(JSON_C_LIB json-c REQUIRED)
find_library(MINIUPNPC_LIB miniupnpc REQUIRED)
find_library(OPUS_LIB opus REQUIRED)
find_library(LCMS2_LIB lcms2 REQUIRED)
if(WIN32)
    # vcpkg's vulkan port names the import lib "vulkan-1" (matching the
    # Windows vulkan-1.dll), not "vulkan".
    find_library(VULKAN_LIB vulkan-1 REQUIRED)
else()
    find_package(Vulkan REQUIRED)
    set(VULKAN_LIB Vulkan::Vulkan)
endif()
# vcpkg's zlib port names the built library "z"/"zd" (Unix convention), not
# "zlib" - "zlib" alone (matching a non-vcpkg Windows zlib build) is kept as
# a fallback name.
find_library(ZLIB_LIB NAMES z zlib REQUIRED)
# vcpkg's hidapi port builds a single "hidapi" library. Debian/Ubuntu's
# libhidapi-dev instead splits it into a hidraw backend and a libusb
# backend (no plain "hidapi") - prefer the hidraw one, matching how
# chiaki-ng itself picks a HIDAPI backend.
find_library(HIDAPI_LIB NAMES hidapi hidapi-hidraw hidapi-libusb REQUIRED)

if(WIN32)
    target_link_libraries(chiaki-py PRIVATE CURL::libcurl_static)
endif()
target_link_libraries(chiaki-py PRIVATE ${FFMPEG_LIBRARIES})
# Link against everything
target_link_libraries(chiaki-py PRIVATE
    ${FFTW3_TARGET}
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
# Link against chiaki
target_link_libraries(chiaki-py PRIVATE fmt::fmt)
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

# chiaki-lib is a real in-tree target (libs/chiaki-ng is add_subdirectory'd
# from the top-level CMakeLists.txt), so linking it also pulls in its own
# transitive deps (Nanopb, Jerasure, curl, OpenSSL::Crypto, json-c,
# miniupnpc, FFmpeg, Threads, Winsock, ...) and public include dirs
# automatically - no manual path-based linking needed.
target_link_libraries(chiaki-py PRIVATE chiaki-lib OpenSSL::Crypto)

# The FFmpeg .dlls this module needs at runtime (it decodes video via them
# directly - see streamsession.cpp) aren't vcpkg-managed: FFMPEG_LIBRARIES
# above links them from vendored import libs under deps/lib/, so vcpkg's own
# POST_BUILD step (which copies a target's DLL dependencies by scanning only
# vcpkg_installed/.../bin) can't find or copy them. Copy them here instead,
# next to the built extension - without this, linking succeeds but importing
# chiaki_py fails with "DLL load failed". Windows-only: on Linux/macOS
# FFmpeg is linked from the system package manager's shared libraries
# (found via pkg-config above), which the dynamic linker resolves normally
# via the standard library search path - nothing to copy alongside the
# extension.
if(WIN32)
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

    # libevent (linked by chiaki-lib through pkg-config, which resolves to
    # vcpkg's *release* libs even in a Debug build) makes the extension
    # import event_core.dll. vcpkg's own POST_BUILD DLL copy only looks in
    # debug/bin for a Debug build, where the file is named event_cored.dll,
    # so the release DLL never lands next to the .pyd and importing fails
    # with "DLL load failed". Release builds already get it from vcpkg's
    # copy; this makes Debug work too (harmless no-op there).
    set(LIBEVENT_RUNTIME_DLL "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/event_core.dll")
    if(EXISTS "${LIBEVENT_RUNTIME_DLL}")
        add_custom_command(TARGET chiaki-py POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBEVENT_RUNTIME_DLL}" "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
            COMMENT " Copying libevent runtime DLL into chiaki_py/lib/ "
        )
    endif()
endif()

# Regenerate the .pyi stubs into chiaki_py/lib/ (the source-tree package
# that re-exports this compiled module - see chiaki_py/lib/__init__.py), so
# `from .chiaki_py import *` there resolves against real stub coverage for
# static type checkers/IDE autocomplete instead of the dynamic star-import
# being opaque to them. Uses ${PYTHON_EXECUTABLE} (the exact interpreter
# this extension was built for, discovered by pybind11 itself) rather than
# a bare `python` - depending on PATH order, `python` can resolve to
# Windows' "install from the Store" app-execution-alias stub instead of a
# real interpreter, which silently no-ops here without `${PYTHON_EXECUTABLE}`.
# The whole stub-generation call is wrapped in `with
# __import__('contextlib').suppress(BaseException): ...` (rather than a
# shell "|| (exit 0)"/"|| exit 0" fallback after this command) so that a
# failure here - e.g. pybind11_stubgen not being installed for
# ${PYTHON_EXECUTABLE} - never fails the overall build. BaseException,
# not just Exception, because pybind11-stubgen's main() calls sys.exit()
# on some failures, which raises SystemExit.
#
# Written as a single physical line/statement (no embedded newlines, no
# try/except block) on purpose: add_custom_command's COMMAND is not run
# through a real shell on every generator/platform combination, so shell
# operators like "||" aren't portable here, and generators that run custom
# commands through a native Makefile recipe treat an embedded newline as a
# second, unindented recipe line ("missing separator" errors) - a
# with-statement's suite can sit entirely after the ":" on one line
# (separated by ";"), unlike try/except, which sidesteps both problems.
# __import__('contextlib') avoids needing a preceding `import contextlib`
# statement (which would itself have to end the line before `with` could
# start). Python string literals below use single quotes throughout so none
# of them need escaping inside this double-quoted CMake argument.
add_custom_command(TARGET chiaki-py POST_BUILD
    VERBATIM COMMAND "${PYTHON_EXECUTABLE}" -c "with __import__('contextlib').suppress(BaseException): import sys, os; sys.path.insert(1, os.getcwd()); from pybind11_stubgen import main; import chiaki_py; sys.argv = ['pybind11-stubgen', '--ignore-all-errors', '-o', '${CMAKE_SOURCE_DIR}/chiaki_py/lib', 'chiaki_py']; main()"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/chiaki_py/lib
    COMMENT " Generating chiaki_py .pyi stubs into chiaki_py/lib/ "
)

