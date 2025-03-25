New-Variable os -Value 'Windows' -Option Constant
  
New-Variable linux_host -Value ($os -eq 'Linux') -Option Constant
New-Variable cygwin_host -Value ('0' -eq '1') -Option Constant
New-Variable windows_host -Value ($os -eq 'Windows' -and !$cygwin_host) -Option Constant

New-Variable version -Value ('18') -Option Constant
New-Variable latest -Value ($version -eq 'latest') -Option Constant
New-Variable x64 -Value ('x64' -eq 'x64') -Option Constant

New-Variable cc -Value ('1' -eq '1') -Option Constant

New-Variable clang -Value 'clang' -Option Constant
New-Variable clangxx -Value 'clang++' -Option Constant

function Locate-Choco {
    $path = Get-Command 'choco' -ErrorAction SilentlyContinue
    if ($path) {
        $path.Path
    }
    else {
        Join-Path ${env:ProgramData} 'chocolatey' 'bin' 'choco'
    }
}

function Install-Package {
    param(
        [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
        [string[]] $Packages
    )

    if ($script:linux_host) {
        sudo apt-get update
        sudo DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends $Packages
    }
    elseif ($script:cygwin_host) {
        $choco = Locate-Choco
        & $choco install $Packages -y --no-progress --source=cygwin
    }
    elseif ($script:windows_host) {
        $choco = Locate-Choco
        & $choco upgrade $Packages -y --no-progress --allow-downgrade
    }
    else {
        throw "Sorry, installing packages is unsupported on $script:os"
    }
}

function Format-UpstreamVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Version
    )

    switch -Exact ($Version) {
        # Since version 7, they dropped the .0 suffix. The earliest
        # version supported is 3.9 on Bionic; versions 4, 5 and 6 are
        # mapped to LLVM-friendly 4.0, 5.0 and 6.0.
        '4' { '4.0' }
        '5' { '5.0' }
        '6' { '6.0' }
        default { $Version }
    }
}

function Format-AptLine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Version
    )

    if (!(Get-Command lsb_release -ErrorAction SilentlyContinue)) {
        throw "Couldn't find lsb_release; LLVM only provides repositories for Debian/Ubuntu"
    }
    $codename = lsb_release -sc

    "deb http://apt.llvm.org/$codename/ llvm-toolchain-$codename-$Version main"
}

function Add-UpstreamRepo {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Version
    )

    Invoke-WebRequest -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    $apt_line = Format-AptLine $Version
    sudo add-apt-repository --yes --update $apt_line
}

$clang = 'clang'
$clangxx = 'clang++'

if ($linux_host) {
    $pkg_clang = 'clang'
    $pkg_llvm = 'llvm'
    $pkg_gxx = 'g++'

    if (!$latest) {
        $pkg_version = Format-UpstreamVersion $version
        Add-UpstreamRepo $pkg_version

        $pkg_clang = "$pkg_clang-$pkg_version"
        $pkg_llvm = "$pkg_llvm-$pkg_version"

        $clang = "$clang-$pkg_version"
        $clangxx = "$clangxx-$pkg_version"
    }
    if (!$x64) {
        $pkg_gxx = 'g++-multilib'
    }
    $packages = $pkg_clang, $pkg_llvm, $pkg_gxx

    Install-Package $packages
}
elseif ($cygwin_host) {  
    # IDK why, but without libiconv-devel, even a "Hello, world!"
    # C++ app cannot be compiled as of December 2020. Also, libstdc++
    # is required; it's simpler to install gcc-g++ for all the
    # dependencies.
    Install-Package clang gcc-g++ libiconv-devel llvm
}
elseif ($windows_host) {
    Install-Package llvm

    $bin_dir = Join-Path $env:ProgramFiles LLVM bin
    Write-Output $bin_dir >> $env:GITHUB_PATH
}
else {
    throw "Sorry, installing Clang is unsupported on $os"
}

function Link-Exe {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Exe,
        [Parameter(Mandatory = $true)]
        [string] $LinkName
    )

    $exe_path = (Get-Command $Exe).Path
    $link_dir = if ($script:windows_host) { Split-Path $exe_path } else { '/usr/local/bin' }
    $link_name = if ($script:windows_host) { "$LinkName.exe" } else { $LinkName }
    $link_path = if ($script:cygwin_host) { "$link_dir/$link_name" } else { Join-Path $link_dir $link_name }
    Write-Output "Creating link $link_path -> $exe_path"
    if ($script:linux_host) {
        sudo ln -f -s $exe_path $link_path
    }
    elseif ($script:cygwin_host) {
        ln.exe -f -s $exe_path $link_path
    }
    elseif ($script:windows_host) {
        New-Item -ItemType HardLink -Path $link_path -Value $exe_path -Force | Out-Null
    }
}

if ($cc) {
    Link-Exe $clang cc
    if ($clang -ne 'clang') {
        Link-Exe $clang 'clang'
    }
    Link-Exe $clangxx c++
    if ($clangxx -ne 'clang++') {
        Link-Exe $clangxx 'clang++'
    }
}

set_env.ps1
$ProgressPreference = 'SilentlyContinue'

# Install Vulkan SDK
$ver = (Invoke-WebRequest -Uri "https://vulkan.lunarg.com/sdk/latest.json" | ConvertFrom-Json).windows
Write-Host "Installing Vulkan SDK Version ${ver}"
Invoke-WebRequest -Uri "https://sdk.lunarg.com/sdk/download/1.4.304.1/windows/VulkanSDK-1.4.304.1-Installer.exe" -OutFile "VulkanSDK.exe"
Start-Process -Verb RunAs -Wait -FilePath "VulkanSDK.exe" -ArgumentList "--root ${env:VULKAN_SDK} --accept-licenses --default-answer --confirm-command install"
Remove-Item "VulkanSDK.exe"

# Install Python and dependencies
python -m pip install --upgrade pip setuptools wheel scons protobuf grpcio-tools pyinstaller meson

# Invoke-WebRequest -Uri "https://github.com/r52/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip" -OutFile ".\ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip"
Expand-Archive -LiteralPath "ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip" -DestinationPath "."
Rename-Item "ffmpeg-n7.1-latest-win64-gpl-shared-7.1" "${env:dep_folder}"
Remove-Item "ffmpeg-n7.1-latest-win64-gpl-shared-7.1.zip"

# Install QT
pip install setuptools wheel py7zr==0.20.*
pip install aqtinstall==3.1.*
python -m aqt version
python -m aqt install-qt windows desktop 6.8.2 win64_msvc2022_64 --autodesktop --outputdir ${env:workplace}\Qt --modules qtwebengine qtpositioning qtwebchannel qtwebsockets qtserialport

# Build SPIRV-Cross
if (!(Test-Path "SPIRV-Cross")) {
    git clone https://github.com/KhronosGroup/SPIRV-Cross.git
}
Set-Location SPIRV-Cross
cmake `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX="${env:workplace}\${env:dep_folder}" `
    -DSPIRV_CROSS_SHARED=ON `
    -S . `
    -B build `
    -G Ninja
cmake --build build --config Release
cmake --install build
Set-Location ..

# Setup shaderc
$url = ((Invoke-WebRequest -UseBasicParsing -Uri "https://storage.googleapis.com/shaderc/badges/build_link_windows_vs2019_release.html").Content | Select-String -Pattern 'url=(.*)"').Matches.Groups[1].Value
Invoke-WebRequest -UseBasicParsing -Uri ${url} -OutFile .\shaderc.zip
Expand-Archive -LiteralPath "shaderc.zip" -DestinationPath "."
Copy-Item "./install/*" "./${env:dep_folder}" -Force -Recurse
Remove-Item "./install" -r -force
Remove-Item "shaderc.zip"


# Setup vcpkg
if (!(Test-Path "vcpkg")) {
    git clone https://github.com/microsoft/vcpkg.git
    Set-Location vcpkg
    git checkout ${env:vcpkg_baseline}
    .\bootstrap-vcpkg.bat
    Set-Location ..
}
vcpkg\vcpkg.exe install --recurse --clean-after-build --x-install-root ./vcpkg_installed/ --triplet=${env:triplet}

# Build libplacebo
if (!(Test-Path "libplacebo")) {
    git clone --recursive https://github.com/haasn/libplacebo.git
}
Set-Location libplacebo
git checkout --recurse-submodules v7.349.0
wsl dos2unix ../scripts/flatpak/0002-Vulkan-use-16bit-for-p010.patch
git apply --ignore-whitespace --verbose ../scripts/flatpak/0002-Vulkan-use-16bit-for-p010.patch
meson setup `
    --prefix "${env:workplace}\${env:dep_folder}" `
    --native-file ../meson.ini `
    "--pkg-config-path=['${env:workplace}\vcpkg_installed\x64-windows\lib\pkgconfig','${env:workplace}\vcpkg_installed\x64-windows\share\pkgconfig','${env:workplace}\${env:dep_folder}\lib\pkgconfig']" `
    "--cmake-prefix-path=['${env:workplace}\vcpkg_installed\x64-windows', '${env:VULKAN_SDK}', '${env:workplace}\${env:dep_folder}']" `
    -Dc_args="/I ${env:VULKAN_SDK}Include" `
    -Dcpp_args="/I ${env:VULKAN_SDK}Include" `
    -Dc_link_args="/LIBPATH:${env:VULKAN_SDK}Lib" `
    -Dcpp_link_args="/LIBPATH:${env:VULKAN_SDK}Lib" `
    -Ddemos=false `
    ./build
ninja -C./build
ninja -C./build install
Set-Location ..

# Apply Patches
git submodule update --init --recursive
git apply --ignore-whitespace --verbose --directory=third-party/gf-complete/ scripts/windows-vc/gf-complete.patch
git apply --ignore-whitespace --verbose scripts/windows-vc/libplacebo-pc.patch

# Configure chiaki-ng
cmake `
    -S . `
    -B build-debug `
    -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE:STRING="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCHIAKI_ENABLE_PYBIND=ON `
    -DPYTHON_EXECUTABLE="${env:python_path}\python.exe" `
    -DCMAKE_PREFIX_PATH="${env:workplace}\${env:dep_folder}; ${env:VULKAN_SDK}"

# Build chiaki-ng
cmake --build build-debug --config Debug --clean-first --target chiaki_py

Copy-Item "${env:workplace}\${env:dep_folder}\bin\avcodec-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\avutil-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swscale-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\${env:dep_folder}\bin\swresample-*.dll" build-debug\pybind
Copy-Item "${env:workplace}\vcpkg_installed\x64-windows\bin\zlib1.dll" build-debug\pybind