brew update
brew install --force fmt

cmake -S . -B build-debug  -G Ninja \
    -DCHIAKI_ENABLE_PYBIND=ON \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++
export CPATH=$(brew --prefix)/opt/ffmpeg/include

cmake --build build-debug --config Debug --clean-first --target chiaki_py