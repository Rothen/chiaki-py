if(NOT DEPS_FOLDER)
    set(DEPS_FOLDER ${CMAKE_SOURCE_DIR}/deps)
endif(NOT DEPS_FOLDER)

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
        $<$<CONFIG:RELWITHDEBINFO>:-Ofast>
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

pybind11_add_module(chiaki-py ${SOURCES})
set_target_properties(chiaki-py PROPERTIES OUTPUT_NAME "chiaki_py")

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
    set_target_properties(chiaki-py PROPERTIES SKIP_BUILD_RPATH TRUE)
endif()

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

if(WIN32)
    find_package(FFTW3 REQUIRED)
    set(FFTW3_TARGET FFTW3::fftw3)
else()
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
    find_library(VULKAN_LIB vulkan-1 REQUIRED)
else()
    find_package(Vulkan REQUIRED)
    set(VULKAN_LIB Vulkan::Vulkan)
endif()
find_library(ZLIB_LIB NAMES z zlib REQUIRED)
find_library(HIDAPI_LIB NAMES hidapi hidapi-hidraw hidapi-libusb REQUIRED)

if(WIN32)
    target_link_libraries(chiaki-py PRIVATE CURL::libcurl_static)
endif()
target_link_libraries(chiaki-py PRIVATE ${FFMPEG_LIBRARIES})
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
target_link_libraries(chiaki-py PRIVATE chiaki-lib OpenSSL::Crypto)

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

    set(LIBEVENT_RUNTIME_DLL "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/event_core.dll")
    if(EXISTS "${LIBEVENT_RUNTIME_DLL}")
        add_custom_command(TARGET chiaki-py POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBEVENT_RUNTIME_DLL}" "${CMAKE_SOURCE_DIR}/chiaki_py/lib"
            COMMENT " Copying libevent runtime DLL into chiaki_py/lib/ "
        )
    endif()
endif()

add_custom_command(TARGET chiaki-py POST_BUILD
    VERBATIM COMMAND "${PYTHON_EXECUTABLE}" -c "with __import__('contextlib').suppress(BaseException): import sys, os; sys.path.insert(1, os.getcwd()); from pybind11_stubgen import main; import chiaki_py; sys.argv = ['pybind11-stubgen', '--ignore-all-errors', '-o', '${CMAKE_SOURCE_DIR}/chiaki_py/lib', 'chiaki_py']; main()"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/chiaki_py/lib
    COMMENT " Generating chiaki_py .pyi stubs into chiaki_py/lib/ "
)

