#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "wasmi_daisy.h"
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

    // WASM bytecode for add function
    static const uint8_t wasm_add[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01,
        0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01,
        0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09,
        0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a,
        0x0b
    };

    // Demonstrate WASM engine usage
    std::cout << "=== WASM Engine Demonstration ===" << std::endl;

    // Create engine
    WasmiEngine* engine = wasmi_engine_new();
    if (!engine) {
        std::cout << "Failed to create WASM engine!" << std::endl;
        juce::JUCEApplication::quit();
        return;
    }

    // Create store
    WasmiStore* store = wasmi_store_new(engine);
    if (!store) {
        std::cout << "Failed to create WASM store!" << std::endl;
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Load module
    WasmiModule* module = wasmi_module_new(engine, wasm_add, sizeof(wasm_add));
    if (!module) {
        std::cout << "Failed to load WASM module!" << std::endl;
        wasmi_store_delete(store);
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Instantiate module
    WasmiInstance* instance = wasmi_instance_new(store, module);
    if (!instance) {
        std::cout << "Failed to instantiate WASM module!" << std::endl;
        wasmi_module_delete(module);
        wasmi_store_delete(store);
        wasmi_engine_delete(engine);
        juce::JUCEApplication::quit();
        return;
    }

    // Get exported function
    const char* func_name = "add";
    WasmiFunc* add_func = wasmi_instance_get_func(
        store, 
        instance,
        reinterpret_cast<const uint8_t*>(func_name),
        3  // strlen("add")
    );

    if (add_func) {
        // Test values
        int32_t a = 42;
        int32_t b = 58;
        
        // Call WASM function
        int32_t wasm_result = wasmi_func_call_i32_i32_to_i32(store, add_func, a, b);
        
        // Native addition for comparison
        int32_t native_result = a + b;
        
        std::cout << "WASM add(" << a << ", " << b << ") = " << wasm_result << std::endl;
        std::cout << "Native add(" << a << ", " << b << ") = " << native_result << std::endl;
        
        if (wasm_result == native_result) {
            std::cout << "✅ Results match! WASM engine working correctly." << std::endl;
        } else {
            std::cout << "❌ Results don't match! WASM engine error." << std::endl;
        }
        
        wasmi_func_delete(add_func);
    } else {
        std::cout << "Failed to get WASM function!" << std::endl;
    }

    // Cleanup
    wasmi_instance_delete(instance);
    wasmi_module_delete(module);
    wasmi_store_delete(store);
    wasmi_engine_delete(engine);

    std::cout << "=== Demonstration Complete ===" << std::endl;
    
    // Quit the application
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
        for (int channel = 0; channel < bufferChannels; ++channel)
        {
            float* out = buffer.getWritePointer(channel);
            float sampleValue = sampleBuffer.getSample(channel % sampleChannels, currentPosition);
            out[sample] += sampleValue;
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
