#pragma once
#include <wasm_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wasm_module_t module;
    wasm_module_inst_t instance;
    wasm_exec_env_t exec_env;
    wasm_function_inst_t get_sample_func;
} WamrAotEngine;

WamrAotEngine* wamr_aot_engine_new(void);
void wamr_aot_engine_delete(WamrAotEngine* engine);
bool wamr_aot_engine_load_module(WamrAotEngine* engine, const uint8_t* aot_bytes, uint32_t size);
float wamr_aot_engine_get_sample(WamrAotEngine* engine);

#ifdef __cplusplus
}
#endif
