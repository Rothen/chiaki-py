New-Variable os -Value 'Windows' -Option Constant

New-Variable version -Value ('18') -Option Constant
New-Variable latest -Value ($version -eq 'latest') -Option Constant
New-Variable x64 -Value ('x64' -eq 'x64') -Option Constant

$clang = 'clang'
$clangxx = 'clang++'

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
$env:workplace = (Resolve-Path ${PSScriptRoot}\..)
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