/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
IrplayerAudioProcessorEditor::IrplayerAudioProcessorEditor(IrplayerAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    addAndMakeVisible(dryWetSlider);
    dryWetSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    dryWetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);

    addAndMakeVisible(dryWetLabel);
    dryWetLabel.setJustificationType(juce::Justification::centred);

    dryWetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "dry_wet", dryWetSlider);

    setSize(300, 200);
}

IrplayerAudioProcessorEditor::~IrplayerAudioProcessorEditor()
{
}

//==============================================================================
void IrplayerAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void IrplayerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto padding = 10;

    dryWetLabel.setBounds(area.removeFromTop(20).reduced(padding));
    dryWetSlider.setBounds(area.reduced(padding));
}
