# set(SOURCE
#     include/chiaki-pybind.h
#     src/py_discover.c
#     src/py_wakeup.c)

# add_library(chiaki-pybind-lib STATIC ${SOURCE})
# target_include_directories(chiaki-pybind-lib PUBLIC "include")
# target_link_libraries(chiaki-pybind-lib chiaki)

if(NOT DEPS_FOLDER)
    set(DEPS_FOLDER ${CMAKE_SOURCE_DIR}/deps)
endif(NOT DEPS_FOLDER)

# Include directories
find_package(Python REQUIRED COMPONENTS Interpreter Development)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
# find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(fmt REQUIRED)


set(PYBIND11_PYTHON_VERSION 3.11)
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    SOURCE_DIR "${DEPS_FOLDER}/pybind11"
    GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)
include_directories(${DEPS_FOLDER}/pybind11)
include_directories(${CMAKE_SOURCE_DIR}/pybind/include)

# Source files
set(SOURCES
    include/core/log.h
    include/core/fec.h
    include/core/common.h
    include/controllermanager.h
    include/elapsed_timer.h
    include/exception.h
    include/host.h
    include/sessionlog.h
    include/settings.h
    include/streamsession.h
    include/timer.h
    include/av_frame.h
    src/core/log.cpp
    src/core/fec.cpp
    src/core/common.cpp
    src/host.cpp
    src/controllermanager.cpp
    src/sessionlog.cpp
    src/settings.cpp
    src/streamsession.cpp
    src/av_frame.cpp
    src/bindings.cpp
)

# Create the shared library (Python module)
pybind11_add_module(chiaki_py ${SOURCES})

set(FFMPEG_LIBRARIES
    ${CMAKE_SOURCE_DIR}/deps/lib/avcodec.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avformat.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avutil.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/swscale.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/chiaki.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/jerasure.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/gf_complete.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/curl/lib/libcurl-d.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/third-party/nanopb/protobuf-nanopbd.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/fftw3.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/fftw3f.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/fftw3l.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/hidapi.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/json-c-static.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/json-c.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/lcms2.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/libcrypto.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/libprotobuf-lite.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/libprotobuf.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/libprotoc.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/libssl.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/miniupnpc.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/opus.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/pkgconf.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/SDL2.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/speexdsp.lib
    ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/vcpkg_installed/x64-windows/lib/vulkan-1.lib
    ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/zlib.lib
)
find_package(Libssh2)
find_package(Libssh2 CONFIG REQUIRED)
target_link_libraries(chiaki_py PRIVATE $<IF:$<TARGET_EXISTS:Libssh2::libssh2_shared>,Libssh2::libssh2_shared,Libssh2::libssh2_static>)
target_link_libraries(chiaki_py PRIVATE crypt32.lib)
if(LIBSSH2_FOUND)
    list(APPEND CURL_LIBS ${LIBSSH2_LIBRARIES})
    list(APPEND LIBCURL_PC_REQUIRES_PRIVATE "libssh2")
    include_directories(SYSTEM ${LIBSSH2_INCLUDE_DIRS})
    set(USE_LIBSSH2 ON)
endif()

set(FFMPEG_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/deps/include
)

include_directories(SYSTEM BEFORE "${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include")

# Ensure that CMake correctly finds FFmpeg
include_directories(" C:/Users/benir/Documents/Projects/chiaki-py/libs/chiaki-ng/lib/include ")
link_directories("${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib")

# Link against chiaki
# target_link_libraries(chiaki_py PRIVATE chiaki-pybind-lib fmt::fmt ${FFMPEG_LIBRARIES})
target_link_libraries(chiaki_py PRIVATE fmt::fmt ${FFMPEG_LIBRARIES})
target_include_directories(chiaki_py PRIVATE ${FFMPEG_INCLUDE_DIRS})

# find_library(CHIAKI_LIB NAMES chiaki PATHS "${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/chiaki.lib")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(CHIAKI_LIB)
    # target_link_libraries(chiaki_py PRIVATE chiaki)
else()
    message(FATAL_ERROR " Chiaki library not found! ")
endif()

if(WIN32)
    # target_link_libraries(chiaki-pybind-lib ws2_32)
    target_link_libraries(chiaki_py PRIVATE wsock32 ws2_32 bcrypt iphlpapi)
endif()

add_custom_command(TARGET chiaki_py POST_BUILD
    VERBATIM COMMAND python -c "import os; import sys; sys.path.insert(1, os.getcwd()) ; from pybind11_stubgen import main; import chiaki_py; sys.argv[0] = 'pybind11-stubgen'; sys.argv.append('--ignore-all-errors') ; sys.argv.append('-o') ; sys.argv.append('.') ; sys.argv.append('chiaki_py') ; main() " ||(exit 0)
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT " Creating stubs for MC3D-TRECSIM "
)

