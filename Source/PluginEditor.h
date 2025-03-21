/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 */
class IrplayerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    IrplayerAudioProcessorEditor(IrplayerAudioProcessor &);
    ~IrplayerAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    juce::Slider dryWetSlider;
    juce::Label dryWetLabel{"Dry/Wet Mix"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAttachment;

    IrplayerAudioProcessor &audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IrplayerAudioProcessorEditor)
};
