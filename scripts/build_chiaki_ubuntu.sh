#!/usr/bin/env bash
# Builds chiaki_py's native extension (chiaki_py/lib/chiaki_py*.so) on
# Ubuntu/Debian, linking chiaki-ng and its dependencies from the system
# package manager instead of vcpkg (which is what scripts/build_wheel.ps1 /
# the Windows CI use instead - see pybind/CMakeLists.txt for the WIN32 vs.
# non-WIN32 branches that make that split possible).
#
# Unlike this script's older revision, chiaki-ng itself no longer needs to
# be checked out by hand (e.g. via `git submodule`) - the top-level
# CMakeLists.txt fetches it into libs/chiaki-ng automatically on first
# configure (see CHIAKI_NG_VERSION there to pin a different tag). This
# script only needs to install this repo's own native dependencies and then
# drive that configure/build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# PYTHON_EXECUTABLE: the wheel this repo publishes is built against cp311
# specifically (see pyproject.toml's requires-python) - override this by
# exporting PYTHON_EXECUTABLE yourself (e.g. to a Python 3.11 venv/conda
# env's interpreter) before running this script if `python3` on PATH isn't
# 3.11. nanopb's code generator (used to build chiaki-ng's protocol
# buffers) also needs `google.protobuf` importable under that same
# interpreter - pip install it below rather than relying on it already
# being present.
PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE:-$(command -v python3)}"

"$PYTHON_EXECUTABLE" -m pip install --upgrade protobuf pybind11_stubgen

sudo apt-get update
sudo apt-get install -y \
    build-essential ninja-build cmake git pkg-config nasm \
    python3-dev \
    protobuf-compiler libprotobuf-dev \
    libopus-dev libjson-c-dev libminiupnpc-dev libpsl-dev libevdev-dev \
    libgf-complete-dev libspeexdsp-dev libidn2-dev libnghttp2-dev libssh2-1-dev \
    libfmt-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
    libsdl2-dev libhidapi-dev libssl-dev libfftw3-dev liblcms2-dev libvulkan-dev zlib1g-dev

CMAKE_ARGS=(
    -S . -B build -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    # chiaki-ng's own CMakeLists.txt pins cmake_minimum_required(VERSION
    # 3.2), which current CMake (>=4) rejects outright without this -
    # matches the flag scripts/build_wheel.ps1 / the CI workflow pass for
    # the same reason.
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DPYTHON_EXECUTABLE="$PYTHON_EXECUTABLE"
    -DCMAKE_C_COMPILER=/usr/bin/gcc
    -DCMAKE_CXX_COMPILER=/usr/bin/g++
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

# If a conda/mamba environment is active (its bin/ ahead of /usr/bin on
# PATH), CMake's find_package(fmt) can resolve to that environment's own
# fmt package (Config-mode package search also checks PATH-derived
# prefixes, and isn't fully suppressed by
# -DCMAKE_FIND_NO_SYSTEM_ENVIRONMENT_PATH=ON) instead of the apt package
# installed above - the two can be different sonames (e.g. libfmt.so.11 vs.
# this distro's libfmt.so.10), which fails at import time with something
# like "libfmt.so.11: cannot open shared object file", not at build time.
# Pin fmt_DIR to the system package's config explicitly when there's a
# conda prefix to shadow it.
if [ -n "${CONDA_PREFIX:-}" ]; then
    system_fmt_dir="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/cmake/fmt"
    if [ -d "$system_fmt_dir" ]; then
        CMAKE_ARGS+=(-Dfmt_DIR="$system_fmt_dir")
    fi
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config Release --target chiaki-py -- -j"$(nproc)"

echo
echo "Built chiaki_py/lib/$(basename "$(ls chiaki_py/lib/chiaki_py.*.so)")"
echo 'Run from the repo root with PYTHONPATH=. e.g.: PYTHONPATH=. "'"$PYTHON_EXECUTABLE"'" -c "import chiaki_py"'
