
cmake_minimum_required(VERSION 3.15)

project(chiaki-py)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PythonInterp 3 REQUIRED) # Make sure nanopb doesn't find Python 2.7 because Python 2 should just die.set(CHIAKI_PY_VERSION_MAJOR 1)

set(CHIAKI_PY_VERSION_MAJOR 0)
set(CHIAKI_PY_VERSION_MINOR 1)
set(CHIAKI_PY_VERSION_PATCH 0)
set(CHIAKI_PY_VERSION ${CHIAKI_PY_VERSION_MAJOR}.${CHIAKI_PY_VERSION_MINOR}.${CHIAKI_PY_VERSION_PATCH})

# chiaki-ng build options, read by libs/chiaki-ng/CMakeLists.txt when it's
# add_subdirectory'd below - pre-set here (before that happens) so they take
# effect. chiaki-py only needs chiaki-lib itself, so everything that isn't
# the core library (GUI, CLI, tests, Android, Switch/Borealis, Steam Deck
# native input) is disabled.
#
# Forced as CACHE entries, not plain set() - chiaki-ng's own CMakeLists.txt
# pins cmake_minimum_required(VERSION 3.2), which predates policy CMP0077
# (CMake 3.13); under that policy's OLD behavior, the builtin option()
# command it uses for most of these *clears* a pre-existing plain variable
# and reasserts its own default instead of honoring it. A cache entry that
# already exists is left alone by option() regardless of that policy, so
# forcing these here is what actually sticks.
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

# chiaki-ng (https://github.com/streetpea/chiaki-ng) is the underlying
# Remote Play library this project binds against. Rather than requiring it
# to be cloned into libs/chiaki-ng by hand, fetch it there at configure
# time, pinned to CHIAKI_NG_VERSION (a tag, branch, or commit). Override
# either with -D on the cmake configure line, e.g.
# -DCHIAKI_NG_VERSION=v1.9.7.
#
# Only fetched when libs/chiaki-ng doesn't already have a CMakeLists.txt -
# FetchContent_Populate's git download step re-clones from scratch (wiping
# anything already there, including out-of-tree build directories like
# libs/chiaki-ng/build-debug) rather than updating an existing checkout in
# place, so an already-populated libs/chiaki-ng is left untouched. Delete
# the directory (or bump CHIAKI_NG_VERSION and re-fetch into a clean spot)
# to pick up a different version.
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

# chiaki-ng ships a patch for its vendored gf-complete
# (scripts/windows-vc/gf-complete.patch) that its own Windows CI applies
# right after cloning: upstream gf_cpu.c uses __cpuidex() in its _MSC_VER
# branch without including <intrin.h>, which clang-cl rejects as an implicit
# function declaration ("call to undeclared library function
# '__cpuidex'"). FetchContent above doesn't apply it, so do it here.
# Skipped when gf_cpu.c already mentions intrin.h (already patched, e.g. by
# an earlier configure or a hand-edited checkout) so this stays idempotent.
# chiaki-ng's other Windows patch, libplacebo-pc.patch, is not applied: it
# edits deps/lib/pkgconfig/libplacebo.pc, which only exists where libplacebo
# is built (chiaki-ng's GUI, disabled here).
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

# FFmpeg on Windows: not managed by vcpkg - chiaki-ng's find_package(FFMPEG)
# and pybind/CMakeLists.txt both use a prebuilt shared FFmpeg release
# unpacked into deps/ (headers in deps/include, import libs in deps/lib,
# DLLs in deps/bin). Download it there at configure time when it isn't
# already present (checked via deps/lib/avcodec.lib); a populated deps/ is
# left alone. Override the archive with -DCHIAKI_PY_FFMPEG_URL=<url>. On
# Linux/macOS FFmpeg comes from the system package manager instead.
# This has to happen before chiaki-ng is add_subdirectory'd below, since
# that already looks for FFmpeg.
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

        # The archive holds a single top-level folder (bin/, include/, lib/
        # inside); copy its contents into deps/, merging with whatever is
        # already there (e.g. a fetched deps/pybind11).
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

# chiaki-ng's own find_package(FFMPEG ...) (pkg-config based) needs to see
# this repo's vendored FFmpeg dev files.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps")

# Building chiaki-ng's own CMakeLists.txt here (rather than just its
# third-party/ subdirectory, as before) makes chiaki-lib an ordinary in-tree
# target: it gets built automatically as a dependency of chiaki-py below,
# with no separate out-of-tree configure/build step required.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng)

add_subdirectory(pybind)