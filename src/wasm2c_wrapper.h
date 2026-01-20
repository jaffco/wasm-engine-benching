#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare the generated wasm2c module struct
struct w2c_module;

typedef struct {
    struct w2c_module* instance;
} Wasm2cEngine;

Wasm2cEngine* wasm2c_engine_new(void);
void wasm2c_engine_delete(Wasm2cEngine* engine);
float wasm2c_engine_get_sample(Wasm2cEngine* engine);

#ifdef __cplusplus
}
#endif
