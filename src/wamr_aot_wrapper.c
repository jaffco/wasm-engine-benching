#include "wamr_aot_wrapper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HEAP_SIZE (512 * 1024)
#define STACK_SIZE 8192

static char global_heap[HEAP_SIZE];

WamrAotEngine* wamr_aot_engine_new(void) {
    WamrAotEngine* engine = calloc(1, sizeof(WamrAotEngine));
    if (!engine) return NULL;

    RuntimeInitArgs init_args = {0};
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = global_heap;
    init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap);

    if (!wasm_runtime_full_init(&init_args)) {
        free(engine);
        return NULL;
    }

    return engine;
}

void wamr_aot_engine_delete(WamrAotEngine* engine) {
    if (!engine) return;
    if (engine->exec_env) wasm_runtime_destroy_exec_env(engine->exec_env);
    if (engine->instance) wasm_runtime_deinstantiate(engine->instance);
    if (engine->module) wasm_runtime_unload(engine->module);
    wasm_runtime_destroy();
    free(engine);
}

bool wamr_aot_engine_load_module(WamrAotEngine* engine, const uint8_t* aot_bytes, uint32_t size) {
    char error_buf[128];

    engine->module = wasm_runtime_load(aot_bytes, size, error_buf, sizeof(error_buf));
    if (!engine->module) return false;

    engine->instance = wasm_runtime_instantiate(engine->module, STACK_SIZE, HEAP_SIZE,
                                                error_buf, sizeof(error_buf));

    if (!engine->instance) return false;

    engine->exec_env = wasm_runtime_create_exec_env(engine->instance, STACK_SIZE);
    if (!engine->exec_env) return false;

    engine->get_sample_func = wasm_runtime_lookup_function(engine->instance, "get_sample");
    return engine->get_sample_func != NULL;
}

float wamr_aot_engine_get_sample(WamrAotEngine* engine) {
    if (!engine->get_sample_func) {
        printf("ERROR: get_sample_func is NULL!\n");
        return 0.0f;
    }

    // Initialize WAMR thread environment for the calling thread (e.g., audio thread)
    // This is safe to call multiple times - it will return true if already initialized
    static __thread bool thread_env_initialized = false;
    if (!thread_env_initialized) {
        if (!wasm_runtime_init_thread_env()) {
            printf("ERROR: Failed to initialize WAMR thread environment!\n");
            return 0.0f;
        }
        thread_env_initialized = true;
        printf("Initialized WAMR thread environment for audio processing thread\n");
    }

    // Use the older argv-based call API instead of wasm_val_t
    uint32_t argv[2];  // Input arg (float as uint32) + return value slot
    argv[0] = 0;       // dummy float argument (0.0f as uint32_t)
    
    // Call the function using the simpler API
    if (wasm_runtime_call_wasm(engine->exec_env, engine->get_sample_func, 1, argv)) {
        // Result is in argv[0] as uint32_t representation of float
        float result = *(float*)&argv[0];
        
        static int debug_count = 0;
        if (debug_count < 3) {
            printf("WAMR call succeeded, result = %f\n", result);
            debug_count++;
        }
        return result;
    } else {
        static int error_count = 0;
        if (error_count < 1) {
            const char* exception = wasm_runtime_get_exception(engine->instance);
            printf("ERROR: WAMR call failed! Exception: %s\n", exception ? exception : "none");
            error_count++;
        }
        return 0.0f;
    }
}
