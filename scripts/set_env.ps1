New-Variable os -Value 'Windows' -Option Constant

New-Variable version -Value ('18') -Option Constant
New-Variable latest -Value ($version -eq 'latest') -Option Constant
New-Variable x64 -Value ('x64' -eq 'x64') -Option Constant

# Load MSVC environment
$vcvarsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
$arch = "x64"  # Change to your desired architecture (e.g., x86, x64, arm, etc.)

cmd.exe /c " `"$vcvarsPath`" $arch && set " | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:\$($matches[1])" -Value "$($matches[2])"
    }
}

# Set environment variables
$ErrorActionPreference = "Stop"
$env:CC = "clang-cl.exe"
$env:CXX = "clang-cl.exe"
$env:VULKAN_SDK = "C:\VulkanSDK\"
$env:triplet = "x64-windows"
$env:vcpkg_baseline = "42bb0d9e8d4cf33485afb9ee2229150f79f61a1f"
$env:VCPKG_INSTALLED_DIR = "./vcpkg_installed/"
$env:dep_folder = "deps"
$env:libplacebo_tag = "v7.349.0"
$env:workplace = "C:\Users\benir\Documents\Projects\chiaki-py"
$env:python_path = "C:\Users\benir\anaconda3\envs\chiaki-ng"

# Set Qt environment variables
$env:QT_ROOT_DIR = "${env:workplace}\Qt\6.8.2\msvc2022_64"
$env:QT_PLUGIN_PATH = "${env:workplace}\Qt\6.8.2\msvc2022_64\plugins"
$env:QML2_IMPORT_PATH = "${env:workplace}\Qt\6.8.2\msvc2022_64\qml"
$env:Qt6_DIR = "${env:workplace}\Qt\6.8.2\msvc2022_64\lib\cmake\Qt6"
$env:QT_DIR = "${env:workplace}\Qt\6.8.2\msvc2022_64\lib\cmake\Qt6"

# Setup vcpkg
$env:RUNVCPKG_VCPKG_ROOT = "${env:workplace}\vcpkg"
$env:VCPKG_ROOT = "${env:workplace}\vcpkg"
$env:RUNVCPKG_VCPKG_ROOT_OUT = "${env:workplace}\vcpkg"
$env:VCPKG_DEFAULT_TRIPLET = "x64-windows"
$env:RUNVCPKG_VCPKG_DEFAULT_TRIPLET_OUT = "x64-windows"