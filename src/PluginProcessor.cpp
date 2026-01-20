#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "wasmi_daisy.h"
#include "add_wasm.h"  // Generated WASM bytecode header
#include <iostream>

// Memory management functions for wasmi-daisy (using standard malloc/free)
extern "C" {
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
    if (get_sample_func) wasmi_func_delete(get_sample_func);
    if (instance) wasmi_instance_delete(instance);
    if (store) wasmi_store_delete(store);
    if (engine) wasmi_engine_delete(engine);
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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
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
    }

    // Use WASM bytecode from generated header (built at compile time)
    const uint8_t* wasm_add = add_wasm;
    size_t wasm_add_size = add_wasm_len;

    // Demonstrate WASM engine usage
    std::cout << "=== WASM Engine Demonstration ===" << std::endl;
    std::cout << "Using WASM module built from wasm-module/add.cpp" << std::endl;
    std::cout << "Module size: " << wasm_add_size << " bytes" << std::endl;

    // Create engine
    engine = wasmi_engine_new();
    if (!engine) {
        std::cout << "Failed to create WASM engine!" << std::endl;
        juce::JUCEApplication::quit();
        return;
    }

    // Create store
    store = wasmi_store_new(engine);
    if (!store) {
        std::cout << "Failed to create WASM store!" << std::endl;
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Load module
    WasmiModule* module = wasmi_module_new(engine, wasm_add, wasm_add_size);
    if (!module) {
        std::cout << "Failed to load WASM module!" << std::endl;
        wasmi_store_delete(store);
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Instantiate module
    instance = wasmi_instance_new(store, module);
    if (!instance) {
        std::cout << "Failed to instantiate WASM module!" << std::endl;
        wasmi_module_delete(module);
        wasmi_store_delete(store);
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Get exported function
    const char* func_name = "get_sample";
    get_sample_func = wasmi_instance_get_func(
        store, 
        instance,
        reinterpret_cast<const uint8_t*>(func_name),
        10  // strlen("get_sample")
    );

    if (get_sample_func) {
        // Test the function
        float wasm_result = wasmi_func_call_f32_to_f32(store, get_sample_func, 0.0f); // dummy arg, since no args
        
        std::cout << "WASM get_sample() = " << wasm_result << std::endl;
        std::cout << "✅ WASM sine oscillator loaded successfully." << std::endl;
    } else {
        std::cout << "Failed to get WASM function!" << std::endl;
        wasmi_instance_delete(instance);
        wasmi_module_delete(module);
        wasmi_store_delete(store);
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Cleanup module (instance keeps it alive)
    wasmi_module_delete(module);

    std::cout << "=== WASM Loaded for Audio Processing ===" << std::endl;
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

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get sine sample from WASM
        float sine_sample = 0.0f;
        if (get_sample_func) {
            sine_sample = wasmi_func_call_f32_to_f32(store, get_sample_func, 0.0f);
        }

        for (int channel = 0; channel < bufferChannels; ++channel)
        {
            float* out = buffer.getWritePointer(channel);
            float sampleValue = sampleBuffer.getSample(channel % sampleChannels, currentPosition) * 0.f;
            out[sample] += sampleValue + sine_sample * 0.2f;  // Add sine with low volume
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
