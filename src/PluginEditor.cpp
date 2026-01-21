#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Title label
    titleLabel.setText ("WASM Engine Selection", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);
    
    // Setup WAMR button
    wamrButton.setButtonText ("WAMR AOT");
    wamrButton.setClickingTogglesState (true);
    wamrButton.setRadioGroupId (1001);
    wamrButton.setToggleState (true, juce::dontSendNotification);
    wamrButton.addListener (this);
    addAndMakeVisible (wamrButton);
    
    // Setup wasm2c button
    wasm2cButton.setButtonText ("wasm2c");
    wasm2cButton.setClickingTogglesState (true);
    wasm2cButton.setRadioGroupId (1001);
    wasm2cButton.addListener (this);
    addAndMakeVisible (wasm2cButton);
    
    // Setup Wasmi button
    wasmiButton.setButtonText ("Wasmi");
    wasmiButton.setClickingTogglesState (true);
    wasmiButton.setRadioGroupId (1001);
    wasmiButton.addListener (this);
    addAndMakeVisible (wasmiButton);
    
    setSize (400, 300);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    
    titleLabel.setBounds (area.removeFromTop (40));
    area.removeFromTop (20); // spacing
    
    auto buttonHeight = 50;
    wamrButton.setBounds (area.removeFromTop (buttonHeight));
    area.removeFromTop (10); // spacing
    wasm2cButton.setBounds (area.removeFromTop (buttonHeight));
    area.removeFromTop (10); // spacing
    wasmiButton.setBounds (area.removeFromTop (buttonHeight));
}

void AudioPluginAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &wamrButton)
    {
        processorRef.setSelectedEngine (EngineType::WAMR);
    }
    else if (button == &wasm2cButton)
    {
        processorRef.setSelectedEngine (EngineType::Wasm2c);
    }
    else if (button == &wasmiButton)
    {
        processorRef.setSelectedEngine (EngineType::Wasmi);
    }
}
