set(SOURCE
    include/chiaki-pybind.h
    src/py_discover.c
    src/py_wakeup.c)

add_library(chiaki-pybind-lib STATIC ${SOURCE})
target_include_directories(chiaki-pybind-lib PUBLIC "include")
target_link_libraries(chiaki-pybind-lib chiaki)

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
    include/py_controllermanager.h
    include/py_elapsed_timer.h
    include/py_exception.h
    include/py_host.h
    include/py_sessionlog.h
    include/py_settings.h
    include/py_streamsession.h
    include/py_timer.h
    include/py_av_frame.h
    src/py_host.cpp
    src/py_controllermanager.cpp
    src/py_sessionlog.cpp
    src/py_settings.cpp
    src/py_streamsession.cpp
    src/py_av_frame.cpp
    src/bindings.cpp
)

# Create the shared library (Python module)
pybind11_add_module(chiaki_py ${SOURCES})

set(FFMPEG_LIBRARIES
    ${CMAKE_SOURCE_DIR}/deps/lib/avcodec.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avformat.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/avutil.lib
    ${CMAKE_SOURCE_DIR}/deps/lib/swscale.lib
)

set(FFMPEG_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/deps/include
)

add_definitions(-D__STDC_CONSTANT_MACROS)
include_directories(SYSTEM BEFORE "${CMAKE_SOURCE_DIR}/libs/chiaki-ng/lib/include")

# Ensure that CMake correctly finds FFmpeg
add_compile_definitions(_HAS_STD_BYTE=0)
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fms-runtime-lib=static ")
include_directories(" C:/Users/benir/Documents/Projects/chiaki-py/libs/chiaki-ng/lib/include ")
link_directories(" C:/Users/benir/Documents/Projects/chiaki-py/libs/chiaki-ng/build-debug/lib ")

# Link against chiaki
target_link_libraries(chiaki_py PRIVATE chiaki-pybind-lib fmt::fmt ${FFMPEG_LIBRARIES})
target_include_directories(chiaki_py PRIVATE ${FFMPEG_INCLUDE_DIRS})

find_library(CHIAKI_LIB NAMES chiaki PATHS " C:/Users/benir/Documents/Projects/chiaki-py/libs/chiaki-ng/build-debug/lib ")

if(CHIAKI_LIB)
    target_link_libraries(chiaki_py PRIVATE chiaki)
else()
    message(FATAL_ERROR " Chiaki library not found! ")
endif()

if(WIN32)
    target_link_libraries(chiaki-pybind-lib ws2_32)
    target_link_libraries(chiaki_py PRIVATE wsock32 ws2_32 bcrypt iphlpapi)
endif()

add_custom_command(TARGET chiaki_py POST_BUILD
    VERBATIM COMMAND python -c " import os; import sys; sys.path.insert(1, os.getcwd()) ; from pybind11_stubgen import main; import chiaki_py; sys.argv[0] = 'pybind11-stubgen'; sys.argv.append('--ignore-all-errors') ; sys.argv.append('-o') ; sys.argv.append('.') ; sys.argv.append('chiaki_py') ; main() " ||(exit 0)
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT " Creating stubs for MC3D-TRECSIM "
)

