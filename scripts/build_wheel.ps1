<#
.SYNOPSIS
    Builds the native extension in Release mode and packages a wheel.

.DESCRIPTION
    Run from a shell with the MSVC dev environment loaded (this repo's
    VS Code integrated terminal does this automatically - see
    .vscode/settings.json - otherwise run Launch-VsDevShell.ps1 yourself
    first) and $env:VCPKG_ROOT pointing at your vcpkg checkout.

    The wheel this produces is tagged for exactly the Python this script
    runs under (cp<major><minor>-win_amd64) - see setup.py for why that
    tagging happens at all, and pyproject.toml's requires-python for why
    that has to be the interpreter pyproject.toml declares support for.

.PARAMETER PythonExecutable
    Python to build the extension against and to invoke `build` with. Must
    have `protobuf` installed (nanopb's code generator needs it) and match
    the requires-python constraint in pyproject.toml.
#>
param(
    [string]$PythonExecutable = (Get-Command python).Source,
    [string]$BuildDir = "build-release"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $RepoRoot

if (-not $env:VCPKG_ROOT) {
    throw "`$env:VCPKG_ROOT is not set - point it at your vcpkg checkout first."
}

Write-Host "==> Configuring ($BuildDir, Release)"
cmake --fresh -S . -B $BuildDir -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
    -DCMAKE_BUILD_TYPE=Release `
    -DPYTHON_EXECUTABLE="$PythonExecutable"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "==> Building chiaki-py"
cmake --build $BuildDir --config Release --target chiaki-py -- -j4
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

Write-Host "==> Building wheel"
& $PythonExecutable -m pip install --upgrade build
& $PythonExecutable -m build --wheel
if ($LASTEXITCODE -ne 0) { throw "wheel build failed" }

Write-Host "==> Checking the built wheel"
& $PythonExecutable -m pip install --upgrade twine
& $PythonExecutable -m twine check dist/*
if ($LASTEXITCODE -ne 0) { throw "twine check failed" }

Write-Host "==> Done. Wheel(s) in dist/. Upload with:"
Write-Host "    python -m twine upload dist/*"
