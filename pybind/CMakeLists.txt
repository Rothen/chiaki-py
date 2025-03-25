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
    include/core/ecdh.h
    include/core/fec.h
    include/core/feedback.h
    include/core/log.h
    include/controllermanager.h
    include/elapsed_timer.h
    include/exception.h
    include/host.h
    include/sessionlog.h
    include/settings.h
    include/streamsession.h
    include/discovery_manager.h
    include/timer.h
    include/av_frame.h
    include/utils.h
    src/core/common.cpp
    src/core/audio.cpp
    src/core/base64.cpp
    src/core/bitstream.cpp
    src/core/controller.cpp
    src/core/ecdh.cpp
    src/core/fec.cpp
    src/core/feedback.cpp
    src/core/log.cpp
    src/host.cpp
    src/controllermanager.cpp
    src/sessionlog.cpp
    src/settings.cpp
    src/streamsession.cpp
    src/discovery_manager.cpp
    src/av_frame.cpp
    src/utils.cpp
    src/bindings.cpp
)

# Create the shared library (Python module)
pybind11_add_module(chiaki-py ${SOURCES})
set_target_properties(chiaki-py PROPERTIES OUTPUT_NAME "chiaki_py")

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
link_directories("${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib")

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
    set_target_properties(chiaki-lib PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/chiaki.lib
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
    )
else()
    add_library(chiaki-lib STATIC IMPORTED)
    set_target_properties(chiaki-lib PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/build-debug/lib/libchiaki.a
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include
    )
endif()

target_link_libraries(chiaki-py PRIVATE chiaki-lib OpenSSL::Crypto)
link_third_party_libs(chiaki-py)
target_link_libraries(chiaki-py PRIVATE Nanopb::nanopb)
target_link_libraries(chiaki-py PRIVATE Jerasure::Jerasure)

add_custom_command(TARGET chiaki-py POST_BUILD
    VERBATIM COMMAND python -c "import os; import sys; sys.path.insert(1, os.getcwd()) ; from pybind11_stubgen import main; import chiaki_py; sys.argv[0] = 'pybind11-stubgen'; sys.argv.append('--ignore-all-errors') ; sys.argv.append('-o') ; sys.argv.append('.') ; sys.argv.append('chiaki_py') ; main() " || (exit 0)
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT " Creating stubs for MC3D-TRECSIM "
)

