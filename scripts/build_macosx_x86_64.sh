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

cmake -S . -B build-debug  -G Ninja \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_PREFIX_PATH="$(brew --prefix openssl@3)"

export CPATH=$(brew --prefix ffmpeg)/include
cmake --build build-debug --config Debug --clean-first --target chiaki-py