#!/bin/bash

# Build wamrc AOT compiler for macOS
cd include/wamr/wamr-compiler

# Build LLVM components if needed
if [ ! -d "build" ]; then
    echo "Building LLVM components for wamrc..."
    ./build_llvm.sh
fi

# Build wamrc
mkdir -p build
cd build
cmake .. -DWAMR_BUILD_PLATFORM=darwin
make -j$(sysctl -n hw.ncpu)

echo "✓ Built wamrc AOT compiler"