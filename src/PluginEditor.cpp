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
    
    // Setup Bypass button (first and default)
    bypassButton.setButtonText ("Bypass");
    bypassButton.setClickingTogglesState (true);
    bypassButton.setRadioGroupId (1001);
    bypassButton.setToggleState (true, juce::dontSendNotification);
    bypassButton.addListener (this);
    addAndMakeVisible (bypassButton);
    
    // Setup Wasmi button
    wasmiButton.setButtonText ("Wasmi interpreter");
    wasmiButton.setClickingTogglesState (true);
    wasmiButton.setRadioGroupId (1001);
    wasmiButton.addListener (this);
    addAndMakeVisible (wasmiButton);
    
    // Setup WAMR button
    wamrButton.setButtonText ("Wamr AOT engine");
    wamrButton.setClickingTogglesState (true);
    wamrButton.setRadioGroupId (1001);
    wamrButton.addListener (this);
    addAndMakeVisible (wamrButton);
    
    // Setup wasm2c button
    wasm2cButton.setButtonText ("wasm2c runtime");
    wasm2cButton.setClickingTogglesState (true);
    wasm2cButton.setRadioGroupId (1001);
    wasm2cButton.addListener (this);
    addAndMakeVisible (wasm2cButton);
    
    setSize (400, 350);
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
    bypassButton.setBounds (area.removeFromTop (buttonHeight));
    area.removeFromTop (10); // spacing
    wasmiButton.setBounds (area.removeFromTop (buttonHeight));
    area.removeFromTop (10); // spacing
    wamrButton.setBounds (area.removeFromTop (buttonHeight));
    area.removeFromTop (10); // spacing
    wasm2cButton.setBounds (area.removeFromTop (buttonHeight));
}

void AudioPluginAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &bypassButton)
    {
        processorRef.setSelectedEngine (EngineType::Bypass);
    }
    else if (button == &wasmiButton)
    {
        processorRef.setSelectedEngine (EngineType::Wasmi);
    }
    else if (button == &wamrButton)
    {
        processorRef.setSelectedEngine (EngineType::WAMR);
    }
    else if (button == &wasm2cButton)
    {
        processorRef.setSelectedEngine (EngineType::Wasm2c);
    }
}
