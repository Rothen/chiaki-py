git submodule update --init --recursive

pip install --user protobuf grpcio-tools setuptools --break-system-packages

brew update
brew install --force streetpea/streetpea/chiaki-ng-qt@6 \
    ffmpeg \
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
    nanopb \
    libudev \
    libevdev \
    json-c miniupnpc

cmake -S . -B build -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCHIAKI_ENABLE_PYBIND=ON \
    -DCHIAKI_ENABLE_CLI=OFF \
    -DCHIAKI_ENABLE_GUI=OFF \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCHIAKI_ENABLE_STEAMDECK_NATIVE=OFF \
    -DCHIAKI_ENABLE_STEAM_SHORTCUT=OFF \
    -DCMAKE_PREFIX_PATH="$(brew --prefix openssl);$(brew --prefix chiaki-ng-qt)" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

export CPATH=$(brew --prefix)/opt/ffmpeg/include
cmake --build build --config Debug --clean-first --target chiaki_py