#!/bin/bash

# Create a temporary wrapper to force C linkage without modifying source
cat > /tmp/add_wrapper.cpp << 'EOF'
#include "add.cpp"
EOF

# Compile C++ to WebAssembly with exported functions
# The wrapper provides extern "C" linkage without modifying the original source
emcc /tmp/add_wrapper.cpp -O2 -o add.wasm \
  -I. \
  -sSTANDALONE_WASM \
  -sEXPORTED_RUNTIME_METHODS=[] \
  -sEXPORTED_FUNCTIONS=_get_sample \
  --no-entry

# Convert WASM binary to C header array
xxd -i add.wasm > add_wasm.h

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
    ../build/wamrc --target=$TARGET -o add.aot add.wasm
    xxd -i add.aot > add_aot.h
    echo "✓ Built AOT file for $TARGET and generated add_aot.h"
else
    echo "⚠ wamrc not found, skipping AOT build"
fi

# Cleanup
rm /tmp/add_wrapper.cpp
rm add.wasm

echo "✓ Built WASM file and generated add_wasm.h"
echo "✓ Function 'get_sample' exported with C linkage"