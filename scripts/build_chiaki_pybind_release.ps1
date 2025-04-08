set_env.ps1

# Configure chiaki-ng
cmake `
    -S . `
    -B build `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Release `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe"

# Build chiaki-ng
cmake --build build --config Release --clean-first --target chiaki-py

Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libcrypto-*-x64.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libssl-*-x64.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\SDL2d.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\hidapi.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\fftw3.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\opus.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libspeexdsp.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\lcms2.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\miniupnpc.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\json-c.dll" build\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\zlibd1.dll" build\pybind\zlibd1.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\zlibd1.dll" build\pybind\zlib1.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\fmtd.dll" build\pybind\fmtd.dll
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libssh2.dll" build\pybind\libssh2.dll
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swscale*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avformat-*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\libplacebo-*.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\shaderc_shared.dll" build\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\spirv-cross-c-shared.dll" build\pybind