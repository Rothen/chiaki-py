# Builds libplacebo, which VulkanRenderer draws with, into <deps_dir> (headers in include/, libplacebo.lib in lib/, the
# DLL in bin/) unless it is there already. Windows only, like the renderer.
#
# libplacebo needs a SPIR-V compiler to make its shaders, and the prebuilt libplacebo builds around do not have one, so
# it is built here: with shaderc and the Vulkan loader, both from vcpkg (see vcpkg.json). Needs git, meson
# (pip install meson) and clang-cl, run from a Visual Studio dev shell like the rest of the build.
set(CHIAKI_PY_LIBPLACEBO_REPOSITORY "https://github.com/haasn/libplacebo.git" CACHE STRING "libplacebo git repository to build")
set(CHIAKI_PY_LIBPLACEBO_TAG "v7.349.0" CACHE STRING "libplacebo git tag to build")
mark_as_advanced(CHIAKI_PY_LIBPLACEBO_REPOSITORY)

function(chiaki_py_build_libplacebo deps_dir)
    if(EXISTS "${deps_dir}/lib/libplacebo.lib")
        return()
    endif()
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        message(FATAL_ERROR "Building libplacebo needs clang-cl (-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl): "
                            "MSVC's C compiler has no C11 atomics by default. Or put a libplacebo build with a shader compiler into "
                            "${deps_dir}, or point PLACEBO_ROOT at one.")
    endif()

    find_package(Git REQUIRED)
    find_program(CHIAKI_PY_MESON NAMES meson.exe meson)
    if(NOT CHIAKI_PY_MESON)
        message(FATAL_ERROR "Building libplacebo needs meson: pip install meson")
    endif()
    set(_vcpkg_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    find_program(CHIAKI_PY_PKGCONF NAMES pkgconf pkg-config HINTS "${_vcpkg_prefix}/tools/pkgconf")
    if(NOT CHIAKI_PY_PKGCONF)
        message(FATAL_ERROR "Building libplacebo needs pkgconf, which vcpkg installs (see vcpkg.json)")
    endif()
    if(NOT EXISTS "${_vcpkg_prefix}/lib/shaderc.lib" OR NOT EXISTS "${_vcpkg_prefix}/lib/vulkan-1.lib")
        message(FATAL_ERROR "Building libplacebo needs shaderc.lib and vulkan-1.lib in ${_vcpkg_prefix}/lib "
                            "(vcpkg's shaderc and vulkan, see vcpkg.json)")
    endif()

    set(_src "${CMAKE_BINARY_DIR}/libplacebo-src")
    set(_build "${CMAKE_BINARY_DIR}/libplacebo-build")
    # vcpkg's shaderc and vulkan-loader ports each install a shaderc.pc/vulkan.pc with paths already fixed up to
    # ${_vcpkg_prefix}, so meson finds both by pkg-config directly from there; shaderc is static, so it (and glslang
    # and SPIR-V Tools, which its .pc lists as extra libs) end up linked straight into libplacebo.dll.
    set(_pc "${_vcpkg_prefix}/lib/pkgconfig")

    if(NOT EXISTS "${_src}/meson.build")
        message(STATUS "Fetching libplacebo ${CHIAKI_PY_LIBPLACEBO_TAG}")
        file(REMOVE_RECURSE "${_src}")
        # The submodules hold the Vulkan headers and the code generators that libplacebo builds with
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --branch ${CHIAKI_PY_LIBPLACEBO_TAG} --depth 1 --recurse-submodules --shallow-submodules
                    ${CHIAKI_PY_LIBPLACEBO_REPOSITORY} "${_src}"
            RESULT_VARIABLE _result)
        if(NOT _result EQUAL 0)
            file(REMOVE_RECURSE "${_src}")
            message(FATAL_ERROR "Failed to fetch libplacebo from ${CHIAKI_PY_LIBPLACEBO_REPOSITORY}")
        endif()
    endif()

    # meson is told what to build with, and where pkg-config finds shaderc and the Vulkan loader, through the environment
    # (which is put back afterwards; not a `cmake -E env` command, since a PATH with its semicolons would be split into arguments)
    get_filename_component(_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    set(_environment
        "CC=clang-cl" "CXX=clang-cl" "CC_LD=lld-link" "CXX_LD=lld-link"
        "PKG_CONFIG=${CHIAKI_PY_PKGCONF}" "PKG_CONFIG_PATH=${_pc}")
    set(_saved_path "$ENV{PATH}")
    foreach(_entry IN LISTS _environment)
        string(REGEX MATCH "^([^=]+)=(.*)$" _unused "${_entry}")
        set(_saved_${CMAKE_MATCH_1} "$ENV{${CMAKE_MATCH_1}}")
        set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
    endforeach()
    set(ENV{PATH} "${_compiler_dir};$ENV{PATH}")

    message(STATUS "Building libplacebo ${CHIAKI_PY_LIBPLACEBO_TAG} with shaderc into ${deps_dir}")
    file(REMOVE_RECURSE "${_build}")
    execute_process(
        COMMAND ${CHIAKI_PY_MESON} setup "${_build}" "${_src}" --prefix "${deps_dir}" --buildtype release
                --default-library shared -Dshaderc=enabled -Dglslang=disabled -Dvk-proc-addr=enabled
                -Dopengl=disabled -Dd3d11=disabled -Dlcms=disabled -Dxxhash=disabled -Dunwind=disabled
                -Ddemos=false -Dtests=false
        RESULT_VARIABLE _result)
    if(_result EQUAL 0)
        execute_process(COMMAND ${CHIAKI_PY_MESON} install -C "${_build}" RESULT_VARIABLE _result)
    endif()

    set(ENV{PATH} "${_saved_path}")
    foreach(_entry IN LISTS _environment)
        string(REGEX MATCH "^([^=]+)=" _unused "${_entry}")
        set(ENV{${CMAKE_MATCH_1}} "${_saved_${CMAKE_MATCH_1}}")
    endforeach()

    if(NOT _result EQUAL 0 OR NOT EXISTS "${deps_dir}/lib/libplacebo.lib")
        message(FATAL_ERROR "Failed to build libplacebo, see the output above")
    endif()
endfunction()
