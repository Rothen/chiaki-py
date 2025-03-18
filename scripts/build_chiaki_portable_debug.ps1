set_env.ps1

cmake `
    -S . `
    -B build-debug `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCHIAKI_ENABLE_CLI=OFF `
    -DCHIAKI_ENABLE_PYBIND=OFF `
    -DCHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER=ON `
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe" `
    -DCMAKE_PREFIX_PATH="${env:workplace}\${env:dep_folder}; ${env:VULKAN_SDK}"

# Build chiaki-ng
cmake --build build-debug --config Debug --clean-first --target chiaki

# Prepare Qt deployment package
if (!(Test-Path "chiaki-ng-Win-debug")) {
    mkdir chiaki-ng-Win-debug
    Copy-Item build-debug\third-party\cpp-steam-tools\cpp-steam-tools.dll chiaki-ng-Win-debug
    Copy-Item scripts\qtwebengine_import.qml gui\src\qml\
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libcrypto-*-x64.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libssl-*-x64.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\SDL2d.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\hidapi.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\fftw3.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\opus.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\libspeexdsp.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\lcms2.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\miniupnpc.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\json-c.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\zlibd1.dll" chiaki-ng-Win-debug/zlibd1.dll
    Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\debug\bin\libssh2.dll" chiaki-ng-Win-debug\libssh2.dll
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\avformat-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\libplacebo-*.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\shaderc_shared.dll" chiaki-ng-Win-debug
    Copy-Item "${env:workplace}\${env:dep_folder}\bin\spirv-cross-c-shared.dll" chiaki-ng-Win-debug
}
Copy-Item build-debug\gui\chiaki.exe chiaki-ng-Win-debug
.\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe --no-translations --qmldir=gui\src\qml --debug chiaki-ng-Win-debug\chiaki.exe