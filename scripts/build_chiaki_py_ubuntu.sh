sudo apt install -y libssh-dev

cmake -S . -B build-debug  -G Ninja \
    -DCHIAKI_ENABLE_PYBIND=ON \
    -DPython_EXECUTABLE=$(which python) \
    -DPython3_EXECUTABLE=$(which python3) \
    -DCMAKE_C_COMPILER=/usr/bin/gcc \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++

cmake --build build-debug --config Debug --clean-first --target chiaki_py