git submodule update --init third-party --recursive

pip install --user protobuf --break-system-packages

brew update
brew install --force ffmpeg \
    pkgconfig \
    opus \
    openssl \
    cmake \
    ninja \
    nasm \
    sdl2 \
    protobuf \
    speexdsp \
    libplacebo \
    wget \
    python-setuptools \
    json-c \
    miniupnpc

cmake -S . -B build-debug -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCHIAKI_ENABLE_TESTS=OFF \
    -DCHIAKI_ENABLE_CLI=OFF \
    -DCHIAKI_ENABLE_GUI=OFF \
    -DCHIAKI_ENABLE_ANDROID=OFF \
    -DCHIAKI_ENABLE_BOREALIS=OFF \
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF \
    -DCHIAKI_ENABLE_STEAM_SHORTCUT=OFF \
    -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/@openssl@3"

export CPATH=$(brew --prefix)/opt/ffmpeg/include
cmake --build build-debug --config Debug --clean-first --target chiaki-lib