#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE:-$(command -v python3)}"

"$PYTHON_EXECUTABLE" -m pip install --upgrade protobuf pybind11_stubgen

sudo apt-get update
sudo apt-get install -y \
    build-essential ninja-build cmake git pkg-config nasm \
    python3-dev \
    protobuf-compiler libprotobuf-dev \
    libopus-dev libjson-c-dev libminiupnpc-dev libpsl-dev libevdev-dev libevent-dev \
    libgf-complete-dev libspeexdsp-dev libidn2-dev libnghttp2-dev libssh2-1-dev \
    libfmt-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
    libsdl2-dev libhidapi-dev libssl-dev libfftw3-dev liblcms2-dev libvulkan-dev zlib1g-dev

CMAKE_ARGS=(
    -S . -B build -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DPYTHON_EXECUTABLE="$PYTHON_EXECUTABLE"
    -DCMAKE_C_COMPILER=/usr/bin/gcc
    -DCMAKE_CXX_COMPILER=/usr/bin/g++
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

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
