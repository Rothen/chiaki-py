git submodule update --init --recursive

pip install --user protobuf grpcio-tools setuptools --break-system-packages

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

cmake -S . -B build-debug -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCHIAKI_ENABLE_TESTS=OFF \
    -DCHIAKI_ENABLE_CLI=OFF \
    -DCHIAKI_ENABLE_GUI=OFF \
    -DCHIAKI_ENABLE_ANDROID=OFF \
    -DCHIAKI_ENABLE_BOREALIS=OFF \
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF \
    -DCHIAKI_ENABLE_STEAM_SHORTCUT=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_C_COMPILER=/usr/bin/gcc \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++

cmake --build build-debug --config Debug --clean-first --target chiaki-lib