
cmake_minimum_required(VERSION 3.15)

project(chiaki-py)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PythonInterp 3 REQUIRED)

set(CHIAKI_PY_VERSION_MAJOR 0)
set(CHIAKI_PY_VERSION_MINOR 0)
set(CHIAKI_PY_VERSION_PATCH 0)
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE CHIAKI_PY_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE CHIAKI_PY_GIT_RESULT
        ERROR_QUIET)
    if(CHIAKI_PY_GIT_RESULT EQUAL 0 AND CHIAKI_PY_GIT_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(CHIAKI_PY_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(CHIAKI_PY_VERSION_MINOR ${CMAKE_MATCH_2})
        set(CHIAKI_PY_VERSION_PATCH ${CMAKE_MATCH_3})
    endif()
endif()
set(CHIAKI_PY_VERSION ${CHIAKI_PY_VERSION_MAJOR}.${CHIAKI_PY_VERSION_MINOR}.${CHIAKI_PY_VERSION_PATCH})

set(CHIAKI_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_CLI OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_GUI OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_ANDROID OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_BOREALIS OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_STEAMDECK_NATIVE OFF CACHE STRING "" FORCE)
set(CHIAKI_ENABLE_STEAM_SHORTCUT OFF CACHE BOOL "" FORCE)
set(CHIAKI_ENABLE_FFMPEG_DECODER ON CACHE STRING "" FORCE)
set(CHIAKI_IS_SWITCH ${NSWITCH})
set(CHIAKI_USE_SYSTEM_CURL OFF CACHE STRING "" FORCE)

if(WIN32)
    list(APPEND CMAKE_REQUIRED_LIBRARIES "ws2_32" "crypt32")
endif()

add_definitions(-DCHIAKI_PY_VERSION_MAJOR=${CHIAKI_PY_VERSION_MAJOR} -DCHIAKI_PY_VERSION_MINOR=${CHIAKI_PY_VERSION_MINOR} -DCHIAKI_PY_VERSION_PATCH=${CHIAKI_PY_VERSION_PATCH} -DCHIAKI_PY_VERSION=\"${CHIAKI_PY_VERSION}\")

set(CHIAKI_NG_VERSION "v1.10.0" CACHE STRING "chiaki-ng git tag/branch/commit to fetch")
set(CHIAKI_NG_REPOSITORY "https://github.com/streetpea/chiaki-ng" CACHE STRING "chiaki-ng git repository to fetch")
mark_as_advanced(CHIAKI_NG_REPOSITORY)

set(CHIAKI_NG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng")
if(NOT EXISTS "${CHIAKI_NG_SOURCE_DIR}/CMakeLists.txt")
    include(FetchContent)
    FetchContent_Populate(
        chiaki-ng
        GIT_REPOSITORY ${CHIAKI_NG_REPOSITORY}
        GIT_TAG ${CHIAKI_NG_VERSION}
        GIT_SUBMODULES "third-party/nanopb;third-party/jerasure;third-party/gf-complete;third-party/curl;third-party/cpp-steam-tools"
        GIT_PROGRESS TRUE
        SOURCE_DIR "${CHIAKI_NG_SOURCE_DIR}"
    )
endif()

if(WIN32)
    set(GF_COMPLETE_DIR "${CHIAKI_NG_SOURCE_DIR}/third-party/gf-complete")
    set(GF_COMPLETE_PATCH "${CHIAKI_NG_SOURCE_DIR}/scripts/windows-vc/gf-complete.patch")
    if(EXISTS "${GF_COMPLETE_DIR}/src/gf_cpu.c" AND EXISTS "${GF_COMPLETE_PATCH}")
        file(READ "${GF_COMPLETE_DIR}/src/gf_cpu.c" _gf_cpu_c)
        if(NOT _gf_cpu_c MATCHES "intrin\\.h")
            find_package(Git REQUIRED)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace --verbose "${GF_COMPLETE_PATCH}"
                WORKING_DIRECTORY "${GF_COMPLETE_DIR}"
                RESULT_VARIABLE _gf_patch_result
            )
            if(NOT _gf_patch_result EQUAL 0)
                message(FATAL_ERROR "Failed to apply ${GF_COMPLETE_PATCH} to ${GF_COMPLETE_DIR}")
            endif()
        endif()
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng/cmake;${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng/setsu/cmake")

if(WIN32)
    set(CHIAKI_PY_FFMPEG_URL "https://github.com/r52/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip"
        CACHE STRING "Prebuilt shared Windows FFmpeg release (zip) to unpack into deps/")
    set(CHIAKI_PY_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
    if(NOT EXISTS "${CHIAKI_PY_DEPS_DIR}/lib/avcodec.lib")
        set(_ffmpeg_zip "${CMAKE_BINARY_DIR}/ffmpeg.zip")
        set(_ffmpeg_extract "${CMAKE_BINARY_DIR}/ffmpeg-extract")

        message(STATUS "Downloading FFmpeg into ${CHIAKI_PY_DEPS_DIR}: ${CHIAKI_PY_FFMPEG_URL}")
        file(DOWNLOAD "${CHIAKI_PY_FFMPEG_URL}" "${_ffmpeg_zip}" SHOW_PROGRESS STATUS _ffmpeg_status)
        list(GET _ffmpeg_status 0 _ffmpeg_status_code)
        if(NOT _ffmpeg_status_code EQUAL 0)
            file(REMOVE "${_ffmpeg_zip}")
            message(FATAL_ERROR "Failed to download FFmpeg from ${CHIAKI_PY_FFMPEG_URL}: ${_ffmpeg_status}")
        endif()

        file(REMOVE_RECURSE "${_ffmpeg_extract}")
        file(MAKE_DIRECTORY "${_ffmpeg_extract}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_ffmpeg_zip}"
            WORKING_DIRECTORY "${_ffmpeg_extract}"
            RESULT_VARIABLE _ffmpeg_extract_result
        )
        if(NOT _ffmpeg_extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${_ffmpeg_zip}")
        endif()

        file(GLOB _ffmpeg_roots LIST_DIRECTORIES true "${_ffmpeg_extract}/*")
        list(GET _ffmpeg_roots 0 _ffmpeg_root)
        file(COPY "${_ffmpeg_root}/" DESTINATION "${CHIAKI_PY_DEPS_DIR}")
        file(REMOVE_RECURSE "${_ffmpeg_extract}")
        file(REMOVE "${_ffmpeg_zip}")

        if(NOT EXISTS "${CHIAKI_PY_DEPS_DIR}/lib/avcodec.lib")
            message(FATAL_ERROR "FFmpeg archive ${CHIAKI_PY_FFMPEG_URL} did not contain lib/avcodec.lib")
        endif()
    endif()
endif()

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng)

add_subdirectory(pybind)