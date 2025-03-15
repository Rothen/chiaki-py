git submodule update --init --recursive

pip install --user protobuf grpcio-tools setuptools --break-system-packages
conda install -c conda-forge gcc=12.1.0

export PATH=$(echo $PATH | tr ':' '\n' | grep -v '/mnt/c/' | tr '\n' ':')

sudo apt install -y ninja-build \
    protobuf-compiler \
    libopus0 \
    libopus-dev \
    libjson-c-dev \
    libminiupnpc-dev \
    libpsl-dev \
    libevdev-dev \
    nasm \
    libplacebo-dev \
    libgf-complete-dev \
    libspeexdsp-dev \
    libnanopb-dev \
    libidn2-0-dev \
    libnghttp2-dev \
    libssh2-1-dev \
    libfmt-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavdevice-dev

cmake -S . -B build -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCHIAKI_ENABLE_PYBIND=ON \
    -DCHIAKI_ENABLE_CLI=OFF \
    -DCHIAKI_ENABLE_GUI=OFF \
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF \
    -DCHIAKI_ENABLE_STEAM_SHORTCUT=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

cmake --build build --config Debug --clean-first --target chiaki_py