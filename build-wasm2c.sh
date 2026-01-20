#!/bin/bash
set -e

echo "=== Building wasm2c from add.wasm ==="

WASM_MODULE_DIR="wasm-module"
BUILD_DIR="build"

# Check if add.wasm exists
if [ ! -f "$WASM_MODULE_DIR/add.wasm" ]; then
    echo "Error: add.wasm not found. Building it first..."
    cd "$WASM_MODULE_DIR"
    
    # Create a temporary wrapper
    cat > /tmp/add_wrapper.cpp << 'EOF'
#include "add.cpp"
EOF

    # Compile to WASM
    emcc /tmp/add_wrapper.cpp -O2 -o add.wasm \
      -I. \
      -sSTANDALONE_WASM \
      -sEXPORTED_RUNTIME_METHODS=[] \
      -sEXPORTED_FUNCTIONS=_get_sample \
      --no-entry
    
    rm /tmp/add_wrapper.cpp
    cd ..
fi

# Convert WASM to C using wasm2c
echo "Converting add.wasm to C using wasm2c..."
wasm2c "$WASM_MODULE_DIR/add.wasm" -o "$BUILD_DIR/add.c"

echo "✓ Generated $BUILD_DIR/add.c and $BUILD_DIR/add.h"
echo "✓ wasm2c build complete"
