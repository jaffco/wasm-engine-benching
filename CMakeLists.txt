cmake_minimum_required(VERSION 3.22)
set(PROJECT_NAME KICKSTART) # set project name with a variable
project(${PROJECT_NAME} VERSION 0.0.1)
add_subdirectory(include/JUCE)                    

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs on your
# system. This setup should be done before calling `juce_add_plugin`.
# juce_set_vst2_sdk_path(...)
# juce_set_aax_sdk_path(...)

# `juce_add_plugin` adds a static library target with the name passed as the first argument
# (AudioPluginExample here). This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.

juce_add_plugin(${PROJECT_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME JAFFCO                          # Specify the name of the plugin's author
    # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
    # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE JFCO               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Kstr                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "KICKSTART")        # The name of the final executable, which can differ from the target name

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

# juce_generate_juce_header(AudioPluginExample)

# `target_sources` adds source files to a target. We pass the target that needs the sources as the
# first argument, then a visibility parameter for the sources which should normally be PRIVATE.
# Finally, we supply a list of source files that will be built into the target. This is a standard
# CMake command.

target_sources(${PROJECT_NAME}
    PRIVATE
        src/PluginEditor.cpp
        src/PluginProcessor.cpp)

# Add wasmi-daisy include directory
target_include_directories(${PROJECT_NAME} PRIVATE include/wasmi-daisy)

# Add wasm-module include directory for generated header
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/build)

# Custom command to build wamrc AOT compiler
set(WAMRC_EXECUTABLE ${CMAKE_BINARY_DIR}/wamrc)
add_custom_command(
    OUTPUT ${WAMRC_EXECUTABLE}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build-wamrc.sh
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/include/wamr/wamr-compiler/build/wamrc-2.4.3 ${WAMRC_EXECUTABLE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building wamrc AOT compiler"
    VERBATIM
)

# Custom target for wamrc
add_custom_target(wamrc_tool ALL DEPENDS ${WAMRC_EXECUTABLE})

# Add wasm-module include directory for generated header
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/build)

# Custom command to build WASM module at compile time
set(WASM_OUTPUT_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/build/module_wasm.h)
set(WASM2C_GENERATED_C ${CMAKE_BINARY_DIR}/module.c)
set(WASM2C_GENERATED_H ${CMAKE_BINARY_DIR}/module.h)
add_custom_command(
    OUTPUT ${WASM_OUTPUT_HEADER} ${WASM2C_GENERATED_C} ${WASM2C_GENERATED_H}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/build-wasm.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/module.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/wasm-module/build-wasm.sh
    COMMENT "Building WASM module and converting to C using wasm2c"
    VERBATIM
)

# Custom target to ensure WASM module is built
add_custom_target(wasm_module ALL DEPENDS ${WASM_OUTPUT_HEADER} ${WASM2C_GENERATED_C} ${WASM2C_GENERATED_H})

# Ensure wamrc is built before WASM module
add_dependencies(wasm_module wamrc_tool)

# Ensure WASM module is built before the plugin
add_dependencies(${PROJECT_NAME} wasm_module)

# Detect the Rust host target triple
execute_process(
    COMMAND rustc -vV
    OUTPUT_VARIABLE RUSTC_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "host: ([a-zA-Z0-9_\\-]+)" _ ${RUSTC_VERSION_OUTPUT})
set(RUST_HOST_TARGET ${CMAKE_MATCH_1})
message(STATUS "Detected Rust host target: ${RUST_HOST_TARGET}")

# Set the wasmi-daisy library path based on host target
set(WASMI_DAISY_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/include/wasmi-daisy/target/${RUST_HOST_TARGET}/release/libwasmi_daisy.a)

# Custom command to build wasmi-daisy library
add_custom_command(
    OUTPUT ${WASMI_DAISY_LIB_PATH}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/include/wasmi-daisy/build.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/wasmi-daisy
    COMMENT "Building wasmi-daisy WASM interpreter library for ${RUST_HOST_TARGET}"
)

# Custom target to ensure the library is built
add_custom_target(wasmi_daisy_lib ALL DEPENDS ${WASMI_DAISY_LIB_PATH})

# Create an imported library target for wasmi-daisy
add_library(wasmi_daisy STATIC IMPORTED GLOBAL)
set_target_properties(wasmi_daisy PROPERTIES
    IMPORTED_LOCATION ${WASMI_DAISY_LIB_PATH}
)
add_dependencies(wasmi_daisy wasmi_daisy_lib)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

# If your target needs extra binary assets, you can add them here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when your target is built.

# juce_add_binary_data(AudioPluginData SOURCES ...)

juce_add_binary_data(AudioPluginData SOURCES media/RawGTR.wav)

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_audio_utils` module. Inter-module
# dependencies are resolved automatically, so `juce_core`, `juce_events` and so on will also be
# linked automatically. If we'd generated a binary data target above, we would need to link to it
# here too. This is a standard CMake command.

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        AudioPluginData           # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Ensure wasmi-daisy library is built before linking
add_dependencies(${PROJECT_NAME} wasmi_daisy)

# Link wasmi-daisy to plugin targets
target_link_libraries(${PROJECT_NAME}_Standalone PRIVATE wasmi_daisy)
target_link_libraries(${PROJECT_NAME}_VST3 PRIVATE wasmi_daisy)
target_link_libraries(${PROJECT_NAME}_AU PRIVATE wasmi_daisy)

# Rust libraries may need system libraries on macOS
if(APPLE)
    target_link_libraries(${PROJECT_NAME}_Standalone PRIVATE "-framework Security" "-framework Foundation")
    target_link_libraries(${PROJECT_NAME}_VST3 PRIVATE "-framework Security" "-framework Foundation")
    target_link_libraries(${PROJECT_NAME}_AU PRIVATE "-framework Security" "-framework Foundation")
endif()

# WAMR AOT Integration
set(WAMR_AOT_LIB_PATH ${CMAKE_BINARY_DIR}/libwamr_aot.a)

# Custom command to build WAMR AOT library
add_custom_command(
    OUTPUT ${WAMR_AOT_LIB_PATH}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build-wamr-aot.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building WAMR AOT runtime library"
)

# Custom target to ensure the library is built
add_custom_target(wamr_aot_lib ALL DEPENDS ${WAMR_AOT_LIB_PATH})

# Create an imported library target for WAMR AOT
add_library(wamr_aot STATIC IMPORTED GLOBAL)
set_target_properties(wamr_aot PROPERTIES
    IMPORTED_LOCATION ${WAMR_AOT_LIB_PATH}
)
add_dependencies(wamr_aot wamr_aot_lib)

# Add WAMR AOT include directory
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_include_directories(${PROJECT_NAME} PRIVATE include/wamr/core/iwasm/include)

# Add WAMR AOT wrapper source
target_sources(${PROJECT_NAME} PRIVATE src/wamr_aot_wrapper.c)

# Link WAMR AOT to plugin targets
target_link_libraries(${PROJECT_NAME}_Standalone PRIVATE wamr_aot)
target_link_libraries(${PROJECT_NAME}_VST3 PRIVATE wamr_aot)
target_link_libraries(${PROJECT_NAME}_AU PRIVATE wamr_aot)

# WASM2C Integration

# Build wasm2c runtime library
set(WASM2C_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/src/wasm-rt-impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/src/wasm-rt-mem-impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/src/wasm-rt-exceptions-impl.c
)

add_library(wasm2c_runtime STATIC ${WASM2C_RUNTIME_SOURCES})
target_include_directories(wasm2c_runtime PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/example
)
# Add compiler definitions needed for wasm2c runtime
target_compile_definitions(wasm2c_runtime PRIVATE
    WASM_RT_MEMCHECK_SIGNAL_HANDLER=0
    WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX=0
)
# Include the compatibility header before all other headers
target_compile_options(wasm2c_runtime PRIVATE 
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/example/wasm-rt-compat.h
)
# Disable strict warnings for third-party code
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(wasm2c_runtime PRIVATE 
        -Wno-implicit-function-declaration
        -Wno-int-conversion
    )
endif()

# Add generated C file to the build
add_library(wasm2c_module STATIC ${WASM2C_GENERATED_C})
target_include_directories(wasm2c_module PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/include
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(wasm2c_module PUBLIC wasm2c_runtime)

# Add wasm2c wrapper source
target_sources(${PROJECT_NAME} PRIVATE src/wasm2c_wrapper.c)

# Add wasm2c include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm2c-runtime/include
    ${CMAKE_BINARY_DIR}
)

# Link wasm2c to plugin targets
target_link_libraries(${PROJECT_NAME}_Standalone PRIVATE wasm2c_runtime wasm2c_module)
target_link_libraries(${PROJECT_NAME}_VST3 PRIVATE wasm2c_runtime wasm2c_module)
target_link_libraries(${PROJECT_NAME}_AU PRIVATE wasm2c_runtime wasm2c_module)

# Ensure wasm2c module is built after wasm module
add_dependencies(wasm2c_module wasm_module)