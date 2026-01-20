#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "wamr_aot_wrapper.h"
#include "wasm2c_wrapper.h"
#include "add_aot.h"  // Generated AOT bytecode header
#include "add_wasm.h"  // Generated WASM bytecode header
#include <iostream>
#include <chrono>

// Wasmi C API
extern "C" {
    #include "wasmi_daisy.h"
    
    // Memory allocation functions required by wasmi-daisy
    void* jaffx_sdram_malloc(size_t size) {
        return malloc(size);
    }
    
    void jaffx_sdram_free(void* ptr) {
        free(ptr);
    }
}

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    // Cleanup WAMR
    if (wamrEngine) wamr_aot_engine_delete(wamrEngine);
    
    // Cleanup wasm2c
    if (wasm2cEngine) wasm2c_engine_delete(wasm2cEngine);
    
    // Cleanup wasmi
    if (wasmiFunc) wasmi_func_delete(wasmiFunc);
    if (wasmiInstance) wasmi_instance_delete(wasmiInstance);
    if (wasmiModule) wasmi_module_delete(wasmiModule);
    if (wasmiStore) wasmi_store_delete(wasmiStore);
    if (wasmiEngine) wasmi_engine_delete(wasmiEngine);
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║        WASM Engine Benchmarking Suite                       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "Sample Rate: " << sampleRate << " Hz" << std::endl;
    std::cout << "Samples Per Block: " << samplesPerBlock << std::endl;
    std::cout << std::endl;
    
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    // Load the embedded WAV file
    auto* wavData = BinaryData::RawGTR_wav;
    auto wavSize = BinaryData::RawGTR_wavSize;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(new juce::MemoryInputStream(wavData, wavSize, false), true));

    if (reader != nullptr)
    {
        sampleBuffer.setSize(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&sampleBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        std::cout << "✓ Audio sample loaded: " << reader->numChannels 
                  << " channels, " << reader->lengthInSamples << " samples" << std::endl;
    }
    else
    {
        std::cout << "✗ ERROR: Failed to load audio sample!" << std::endl;
    }

    // Get bytecode for all engines
    const uint8_t* aot_bytes = add_aot;
    size_t aot_size = add_aot_len;
    const uint8_t* wasm_bytes = add_wasm;
    size_t wasm_size = add_wasm_len;
    
    std::cout << "\nModule sizes:" << std::endl;
    std::cout << "  WASM: " << wasm_size << " bytes" << std::endl;
    std::cout << "  AOT:  " << aot_size << " bytes" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // BENCHMARK 1: WAMR AOT Engine
    // ========================================================================
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Engine 1: WAMR AOT (Ahead-of-Time Compilation)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    auto wamr_start = std::chrono::high_resolution_clock::now();
    
    wamrEngine = wamr_aot_engine_new();
    if (!wamrEngine) {
        std::cout << "✗ Failed to create WAMR engine" << std::endl;
    } else if (!wamr_aot_engine_load_module(wamrEngine, aot_bytes, aot_size)) {
        std::cout << "✗ Failed to load WAMR module" << std::endl;
        wamr_aot_engine_delete(wamrEngine);
        wamrEngine = nullptr;
    } else {
        auto wamr_load = std::chrono::high_resolution_clock::now();
        auto wamr_load_time = std::chrono::duration_cast<std::chrono::microseconds>(wamr_load - wamr_start).count();
        
        // Test execution
        float wamr_result = wamr_aot_engine_get_sample(wamrEngine);
        auto wamr_end = std::chrono::high_resolution_clock::now();
        auto wamr_exec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(wamr_end - wamr_load).count();
        
        std::cout << "  ✓ Load time: " << wamr_load_time << " μs" << std::endl;
        std::cout << "  ✓ First execution: " << wamr_exec_time << " ns" << std::endl;
        std::cout << "  ✓ Result: " << wamr_result << std::endl;
        
        // Benchmark multiple calls
        const int iterations = 10000;
        auto bench_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            wamr_aot_engine_get_sample(wamrEngine);
        }
        auto bench_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(bench_end - bench_start).count();
        std::cout << "  ✓ " << iterations << " calls: " << total_time << " μs (" 
                  << (total_time * 1000.0 / iterations) << " ns/call)" << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // BENCHMARK 2: wasm2c Engine
    // ========================================================================
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Engine 2: wasm2c (WASM to C Transpilation)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    auto wasm2c_start = std::chrono::high_resolution_clock::now();
    
    wasm2cEngine = wasm2c_engine_new();
    if (!wasm2cEngine) {
        std::cout << "✗ Failed to create wasm2c engine" << std::endl;
    } else {
        auto wasm2c_load = std::chrono::high_resolution_clock::now();
        auto wasm2c_load_time = std::chrono::duration_cast<std::chrono::microseconds>(wasm2c_load - wasm2c_start).count();
        
        // Test execution
        float wasm2c_result = wasm2c_engine_get_sample(wasm2cEngine);
        auto wasm2c_end = std::chrono::high_resolution_clock::now();
        auto wasm2c_exec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(wasm2c_end - wasm2c_load).count();
        
        std::cout << "  ✓ Load time: " << wasm2c_load_time << " μs" << std::endl;
        std::cout << "  ✓ First execution: " << wasm2c_exec_time << " ns" << std::endl;
        std::cout << "  ✓ Result: " << wasm2c_result << std::endl;
        
        // Benchmark multiple calls
        const int iterations = 10000;
        auto bench_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            wasm2c_engine_get_sample(wasm2cEngine);
        }
        auto bench_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(bench_end - bench_start).count();
        std::cout << "  ✓ " << iterations << " calls: " << total_time << " μs (" 
                  << (total_time * 1000.0 / iterations) << " ns/call)" << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // BENCHMARK 3: Wasmi Engine
    // ========================================================================
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Engine 3: Wasmi (Stack-based Interpreter)" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    auto wasmi_start = std::chrono::high_resolution_clock::now();
    
    wasmiEngine = wasmi_engine_new();
    if (!wasmiEngine) {
        std::cout << "✗ Failed to create Wasmi engine" << std::endl;
    } else {
        wasmiStore = wasmi_store_new(wasmiEngine);
        if (!wasmiStore) {
            std::cout << "✗ Failed to create Wasmi store" << std::endl;
        } else {
            wasmiModule = wasmi_module_new(wasmiEngine, wasm_bytes, wasm_size);
            if (!wasmiModule) {
                std::cout << "✗ Failed to load Wasmi module" << std::endl;
            } else {
                wasmiInstance = wasmi_instance_new(wasmiStore, wasmiModule);
                if (!wasmiInstance) {
                    std::cout << "✗ Failed to instantiate Wasmi module" << std::endl;
                } else {
                    const char* func_name = "get_sample";
                    wasmiFunc = wasmi_instance_get_func(wasmiStore, wasmiInstance, 
                                                       (const uint8_t*)func_name, strlen(func_name));
                    if (!wasmiFunc) {
                        std::cout << "✗ Failed to get function from Wasmi" << std::endl;
                    } else {
                        auto wasmi_load = std::chrono::high_resolution_clock::now();
                        auto wasmi_load_time = std::chrono::duration_cast<std::chrono::microseconds>(wasmi_load - wasmi_start).count();
                        
                        // Test execution
                        float dummy = 0.0f;
                        float wasmi_result = wasmi_func_call_f32_to_f32(wasmiStore, wasmiFunc, dummy);
                        
                        auto wasmi_end = std::chrono::high_resolution_clock::now();
                        auto wasmi_exec_time = std::chrono::duration_cast<std::chrono::nanoseconds>(wasmi_end - wasmi_load).count();
                        
                        std::cout << "  ✓ Load time: " << wasmi_load_time << " μs" << std::endl;
                        std::cout << "  ✓ First execution: " << wasmi_exec_time << " ns" << std::endl;
                        std::cout << "  ✓ Result: " << wasmi_result << std::endl;
                        
                        // Benchmark multiple calls
                        const int iterations = 10000;
                        auto bench_start = std::chrono::high_resolution_clock::now();
                        for (int i = 0; i < iterations; i++) {
                            wasmi_func_call_f32_to_f32(wasmiStore, wasmiFunc, dummy);
                        }
                        auto bench_end = std::chrono::high_resolution_clock::now();
                        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(bench_end - bench_start).count();
                        std::cout << "  ✓ " << iterations << " calls: " << total_time << " μs (" 
                                  << (total_time * 1000.0 / iterations) << " ns/call)" << std::endl;
                    }
                }
            }
        }
    }
    std::cout << std::endl;
    
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  All engines initialized and benchmarked                     ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    juce::JUCEApplication::quit();
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    static int callCount = 0;
    static bool firstCall = true;
    
    if (firstCall) {
        std::cout << "\n=== First processBlock call ===" << std::endl;
        std::cout << "Buffer size: " << buffer.getNumSamples() << " samples" << std::endl;
        std::cout << "Num channels: " << buffer.getNumChannels() << std::endl;
        std::cout << "wamrEngine pointer: " << (void*)wamrEngine << std::endl;
        std::cout << "Sample buffer size: " << sampleBuffer.getNumSamples() << std::endl;
        firstCall = false;
    }
    
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // ..do something to the data...
    }

    // Add looping sample playback
    int numSamples = buffer.getNumSamples();
    int bufferChannels = buffer.getNumChannels();
    int sampleChannels = sampleBuffer.getNumChannels();

    static int sampleDebugCount = 0;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get sine sample from WAMR AOT
        float sine_sample = 0.0f;
        if (wamrEngine) {
            sine_sample = wamr_aot_engine_get_sample(wamrEngine);
            
            // Debug first few samples
            if (sampleDebugCount < 5) {
                std::cout << "Sample " << sampleDebugCount << ": sine_sample = " << sine_sample << std::endl;
                sampleDebugCount++;
            }
        } else if (sampleDebugCount < 1) {
            std::cout << "WARNING: wamrEngine is null in processBlock!" << std::endl;
            sampleDebugCount++;
        }

        for (int channel = 0; channel < bufferChannels; ++channel)
        {
            float* out = buffer.getWritePointer(channel);
            float sampleValue = sampleBuffer.getSample(channel % sampleChannels, currentPosition) * 0.f;
            float finalSample = sampleValue + sine_sample * 0.2f;
            
            // Debug first few output samples
            if (sampleDebugCount <= 5 && sample == 0 && channel == 0) {
                std::cout << "  Output sample = " << finalSample << " (sine * 0.2 = " << (sine_sample * 0.2f) << ")" << std::endl;
            }
            
            out[sample] += finalSample;  // Add sine with low volume
        }
        currentPosition = (currentPosition + 1) % sampleBuffer.getNumSamples();
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
