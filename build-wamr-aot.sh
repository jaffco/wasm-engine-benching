#!/bin/bash

# Store the project root directory
PROJECT_ROOT=$(pwd)

# Build WAMR runtime with AOT support for macOS
cd include/wamr/product-mini/platforms/darwin

mkdir -p build
cd build

cmake .. \
  -DWAMR_BUILD_PLATFORM=darwin \
  -DWAMR_BUILD_TARGET=AARCH64 \
  -DWAMR_BUILD_INTERP=0 \
  -DWAMR_BUILD_FAST_INTERP=0 \
  -DWAMR_BUILD_AOT=1 \
  -DWAMR_BUILD_LIBC_BUILTIN=1 \
  -DBUILD_SHARED_LIBS=OFF

make -j$(sysctl -n hw.ncpu)

# Copy the static library to the build directory using absolute path
mkdir -p "${PROJECT_ROOT}/build"
cp libiwasm.a "${PROJECT_ROOT}/build/libwamr_aot.a"

echo "✓ Built WAMR AOT runtime library"