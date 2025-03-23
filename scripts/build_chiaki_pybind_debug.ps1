set_env.ps1

# Configure chiaki-ng
cmake `
    -S . `
    -B build-debug `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Debug `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe"

# Build chiaki-ng
cmake --build build-debug --config Debug --clean-first --target chiaki-py

Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libcrypto-*-x64.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libssl-*-x64.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\SDL2d.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\hidapi.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\fftw3.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\opus.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libspeexdsp.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\lcms2.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\miniupnpc.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\json-c.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\zlibd1.dll" build-debug\pybind\zlibd1.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\zlibd1.dll" build-debug\pybind\zlib1.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\fmtd.dll" build-debug\pybind\fmtd.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\libssh2.dll" build-debug\pybind\libssh2.dll
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swscale*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avformat-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\libplacebo-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\shaderc_shared.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\spirv-cross-c-shared.dll" build-debug\pybind