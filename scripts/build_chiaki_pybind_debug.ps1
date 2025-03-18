set_env.ps1

# Configure chiaki-ng
cmake `
    -S . `
    -B build-pybind `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCHIAKI_ENABLE_PYBIND=ON `
    -DCHIAKI_ENABLE_CLI=OFF `
    -DCHIAKI_ENABLE_TESTS=OFF `
    -DCHIAKI_ENABLE_GUI=OFF `
    -DCHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER=OFF `
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF `
    -DENABLE_LIBPSL=OFF `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe" `
    -DCMAKE_PREFIX_PATH="${env:workplace}\${env:dep_folder};${env:VULKAN_SDK}"

# Build chiaki-ng
cmake --build build-pybind --config Debug --clean-first --target chiaki_py

Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libcrypto-*-x64.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libssl-*-x64.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\SDL2d.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\hidapi.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\fftw3.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\opus.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libspeexdsp.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\lcms2.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\miniupnpc.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\json-c.dll" build-pybind\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\zlibd1.dll" build-pybind\pybind\zlibd1.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\fmtd.dll" build-pybind\pybind\fmtd.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\libssh2.dll" build-pybind\pybind\libssh2.dll
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avformat-*.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\libplacebo-*.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\shaderc_shared.dll" build-pybind\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\spirv-cross-c-shared.dll" build-pybind\pybind