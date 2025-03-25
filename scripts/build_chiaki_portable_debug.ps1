set_env.ps1

cmake `
    -S . `
    -B build-portable-debug `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCHIAKI_ENABLE_TESTS=OFF `
    -DCHIAKI_ENABLE_CLI=OFF `
    -DCHIAKI_ENABLE_GUI=ON `
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe" `
    -DCMAKE_PREFIX_PATH="${env:workplace}\${env:dep_folder}; ${env:VULKAN_SDK}"

# Build chiaki-ng
cmake --build build-portable-debug --config Debug --clean-first --target chiaki

# Prepare Qt deployment package
if (!(Test-Path "chiaki-ng-Win-debug")) {
    mkdir chiaki-ng-Win-debug
    Copy-Item build-portable-debug\third-party\cpp-steam-tools\cpp-steam-tools.dll chiaki-ng-Win-debug
    Copy-Item scripts\qtwebengine_import.qml gui\src\qml\
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\libcrypto-*-x64.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\libssl-*-x64.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\SDL2d.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\hidapi.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\fftw3.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\opus.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\libspeexdsp.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\lcms2.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\miniupnpc.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\json-c.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\zlibd1.dll" chiaki-ng-Win-debug/zlib.dll
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\libssh2.dll" chiaki-ng-Win-debug/libssh2.dll
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avformat-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\libplacebo-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\shaderc_shared.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\spirv-cross-c-shared.dll" chiaki-ng-Win-debug
}
Copy-Item build-portable-debug\gui\chiaki.exe chiaki-ng-Win-debug
.\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe --no-translations --qmldir=gui\src\qml --debug chiaki-ng-Win-debug\chiaki.exe