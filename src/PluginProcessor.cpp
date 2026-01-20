#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "wamr_aot_wrapper.h"
#include "add_aot.h"  // Generated AOT bytecode header
#include <iostream>

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
    if (wamrEngine) wamr_aot_engine_delete(wamrEngine);
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
    std::cout << "\n=== prepareToPlay called ===" << std::endl;
    std::cout << "Sample Rate: " << sampleRate << " Hz" << std::endl;
    std::cout << "Samples Per Block: " << samplesPerBlock << std::endl;
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    // Load the embedded WAV file
    auto* wavData = BinaryData::RawGTR_wav;
    auto wavSize = BinaryData::RawGTR_wavSize;
    std::cout << "WAV Data pointer: " << (void*)wavData << ", size: " << wavSize << std::endl;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(new juce::MemoryInputStream(wavData, wavSize, false), true));

    if (reader != nullptr)
    {
        std::cout << "WAV Reader created: channels=" << reader->numChannels 
                  << ", samples=" << reader->lengthInSamples << std::endl;
        sampleBuffer.setSize(reader->numChannels, (int)reader->lengthInSamples);
        reader->read(&sampleBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        std::cout << "Sample buffer loaded successfully" << std::endl;
    }
    else
    {
        std::cout << "ERROR: Failed to create WAV reader!" << std::endl;
    }

    // Use AOT bytecode from generated header (built at compile time)
    const uint8_t* aot_add = add_aot;
    size_t aot_add_size = add_aot_len;

    // Demonstrate WAMR AOT engine usage
    std::cout << "=== WAMR AOT Engine Demonstration ===" << std::endl;
    std::cout << "Using AOT module built from wasm-module/add.cpp" << std::endl;
    std::cout << "Module size: " << aot_add_size << " bytes" << std::endl;

    // Create WAMR AOT engine
    wamrEngine = wamr_aot_engine_new();
    if (!wamrEngine) {
        std::cout << "Failed to create WAMR AOT engine!" << std::endl;
        juce::JUCEApplication::quit();
        return;
    }

    // Load AOT module
    if (!wamr_aot_engine_load_module(wamrEngine, aot_add, aot_add_size)) {
        std::cout << "Failed to load AOT module!" << std::endl;
        wamr_aot_engine_delete(wamrEngine);
        wamrEngine = nullptr;
        juce::JUCEApplication::quit();
        return;
    }

    // Test the function
    float aot_result = wamr_aot_engine_get_sample(wamrEngine);
    std::cout << "AOT get_sample() = " << aot_result << std::endl;
    std::cout << "✅ WAMR AOT sine oscillator loaded successfully." << std::endl;

    std::cout << "=== WAMR AOT Loaded for Audio Processing ===" << std::endl;
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
