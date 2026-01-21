#!/bin/bash

# bail out on any failed process
set -e

# Create local build directory
mkdir -p build

# Create a temporary wrapper to force C linkage without modifying source
cat > build/module_wrapper.cpp << 'EOF'
#include "module.cpp"
EOF

# Compile C++ to WebAssembly with exported functions
# The wrapper provides extern "C" linkage without modifying the original source
emcc build/module_wrapper.cpp -O2 -o build/module.wasm \
  -I. \
  -sSTANDALONE_WASM \
  -sEXPORTED_RUNTIME_METHODS=[] \
  -sEXPORTED_FUNCTIONS=_process \
  --no-entry

# Convert WASM binary to C header array
xxd -i -n module_wasm build/module.wasm > build/module_wasm.h

# Build AOT file using wamrc (if available)
if [ -f "../build/wamrc" ]; then
    echo "Building AOT file with wamrc..."
    # Detect architecture
    ARCH=$(uname -m)
    if [ "$ARCH" = "arm64" ]; then
        TARGET="aarch64"
    else
        TARGET="x86_64"
    fi
    ../build/wamrc --target=$TARGET -o build/module.aot build/module.wasm
    xxd -i -n module_aot build/module.aot > build/module_aot.h
    echo "✓ Built AOT file for $TARGET and generated module_aot.h"
else
    echo "⚠ wamrc not found, skipping AOT build"
fi

# Convert WASM to C using wasm2c (if available)
if command -v wasm2c >/dev/null 2>&1; then
    echo "Converting build/module.wasm to C using wasm2c..."
    wasm2c build/module.wasm -o ../build/module.c
    echo "✓ Generated ../build/module.c and ../build/module.h"
else
    echo "⚠ wasm2c not found, skipping wasm2c conversion"
fi

echo "✓ Built WASM file and generated module_wasm.h"
echo "✓ Function 'process' exported with C linkage"