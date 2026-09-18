
cmake_minimum_required(VERSION 3.15)

project(chiaki-py)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PythonInterp 3 REQUIRED) # Make sure nanopb doesn't find Python 2.7 because Python 2 should just die.set(CHIAKI_PY_VERSION_MAJOR 1)

set(CHIAKI_PY_VERSION_MAJOR 1)
set(CHIAKI_PY_VERSION_MINOR 9)
set(CHIAKI_PY_VERSION_PATCH 6)
set(CHIAKI_PY_VERSION ${CHIAKI_PY_VERSION_MAJOR}.${CHIAKI_PY_VERSION_MINOR}.${CHIAKI_PY_VERSION_PATCH})

set(CHIAKI_ENABLE_STEAM_SHORTCUT OFF)
set(CHIAKI_IS_SWITCH ${NSWITCH})
set(CHIAKI_USE_SYSTEM_CURL OFF)

list(APPEND CMAKE_REQUIRED_LIBRARIES "ws2_32" "crypt32")

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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng/cmake;${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng/setsu/cmake")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/chiaki-ng/third-party)

add_subdirectory(pybind)