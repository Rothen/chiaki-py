set_env.ps1

cmake `
    -S . `
    -B build `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_BUILD_TYPE=Release `
    -DCHIAKI_ENABLE_TESTS=OFF `
    -DCHIAKI_ENABLE_CLI=OFF `
    -DCHIAKI_ENABLE_GUI=OFF `
    -DCHIAKI_ENABLE_ANDROID=OFF `
    -DCHIAKI_ENABLE_BOREALIS=OFF `
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF `
    -DCHIAKI_ENABLE_STEAM_SHORTCUT=OFF `
    -DCHIAKI_ENABLE_FFMPEG_DECODER=ON `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe" `
    -DCMAKE_PREFIX_PATH="${env:workplace}\${env:dep_folder}; ${env:VULKAN_SDK}"

# Build chiaki-ng
cmake --build build --config Release --clean-first --target chiaki-lib