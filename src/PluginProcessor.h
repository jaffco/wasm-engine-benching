#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "wamr_aot_wrapper.h"
#include "wasm2c_wrapper.h"

// Forward declarations for wasmi
extern "C" {
    typedef struct WasmiEngine WasmiEngine;
    typedef struct WasmiStore WasmiStore;
    typedef struct WasmiModule WasmiModule;
    typedef struct WasmiInstance WasmiInstance;
    typedef struct WasmiFunc WasmiFunc;
}

enum class EngineType
{
    WAMR = 0,
    Wasm2c,
    Wasmi,
    Bypass
};

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    EngineType getSelectedEngine() const { return selectedEngine; }
    void setSelectedEngine(EngineType engine) { selectedEngine = engine; }

private:
    //==============================================================================
    juce::AudioBuffer<float> sampleBuffer;
    int currentPosition = 0;

    // WAMR AOT engine components
    WamrAotEngine* wamrEngine = nullptr;
    
    // WASM2C engine components
    Wasm2cEngine* wasm2cEngine = nullptr;
    
    // Wasmi engine components
    WasmiEngine* wasmiEngine = nullptr;
    WasmiStore* wasmiStore = nullptr;
    WasmiModule* wasmiModule = nullptr;
    WasmiInstance* wasmiInstance = nullptr;
    WasmiFunc* wasmiFunc = nullptr;
    
    // Engine selection
    EngineType selectedEngine = EngineType::Bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
