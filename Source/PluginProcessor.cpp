/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::ParameterID dryWetID{"dry_wet", 1};
}

//==============================================================================
IrplayerAudioProcessor::IrplayerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
#endif
      parameters(*this, nullptr, juce::Identifier("Parameters"),
                 {std::make_unique<juce::AudioParameterFloat>(
                     dryWetID,
                     "Dry/Wet",
                     juce::NormalisableRange<float>(0.0f, 1.0f),
                     1.0f,
                     juce::AudioParameterFloatAttributes()
                         .withLabel("%"))})
{
    parameters.addParameterListener(dryWetID.getParamID(), this);
    setLatencySamples(1024); // Set latency compensation for the FFT size
}

IrplayerAudioProcessor::~IrplayerAudioProcessor()
{
    parameters.removeParameterListener(dryWetID.getParamID(), this);
}

void IrplayerAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == dryWetID.getParamID())
        dryWetMix = newValue;
}

//==============================================================================
const juce::String IrplayerAudioProcessor::getName() const
{
    return "IR Player";
}

bool IrplayerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool IrplayerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool IrplayerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double IrplayerAudioProcessor::getTailLengthSeconds() const
{
    return 2.0; // Return a reasonable tail length for the IR
}

int IrplayerAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int IrplayerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IrplayerAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String IrplayerAudioProcessor::getProgramName(int index)
{
    return {};
}

void IrplayerAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

//==============================================================================
void IrplayerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;

    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumInputChannels();
    convolution.reset();

    if (!irLoaded)
    {
        const char *irData = BinaryData::fbcimpulse_wav;
        const int irSize = BinaryData::fbcimpulse_wavSize;

        DBG("Loading IR from binary resource, size: " + juce::String(irSize) + " bytes");

        if (irData != nullptr && irSize > 0)
        {
            convolution.loadImpulseResponse(
                static_cast<const void *>(irData),
                static_cast<size_t>(irSize),
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::yes,
                static_cast<size_t>(0),
                juce::dsp::Convolution::Normalise::yes);
            irLoaded = true;
            DBG("Successfully loaded IR from binary resource");
        }
        else
        {
            DBG("Failed to load IR from binary resource");
        }
    }

    convolution.prepare(spec);

    // Pre-allocate the dry buffer to avoid reallocations during processing
    dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);

    juce::String debugMessage = juce::String("Prepared convolution with sample rate: ") +
                                juce::String(sampleRate) +
                                juce::String(", block size: ") +
                                juce::String(samplesPerBlock) +
                                juce::String(", channels: ") +
                                juce::String(spec.numChannels);
    DBG(debugMessage);
}

void IrplayerAudioProcessor::releaseResources()
{
    convolution.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool IrplayerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void IrplayerAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Quick return for fully dry signal
    if (dryWetMix < 0.01f)
        return;

    // Process through convolution for fully wet signal
    if (dryWetMix > 0.99f)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution.process(context);
        return;
    }

    // For mix values in between, we need a copy of the dry signal to mix with the wet signal
    dryBuffer.makeCopyOf(buffer, true);
    // Process wet signal through convolution
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution.process(context);
    }

    // Mix dry and wet signals
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *dryData = dryBuffer.getReadPointer(channel);
        auto *wetData = buffer.getWritePointer(channel);

        juce::FloatVectorOperations::multiply(wetData, dryWetMix, buffer.getNumSamples());
        juce::FloatVectorOperations::addWithMultiply(wetData, dryData, (1.0f - dryWetMix), buffer.getNumSamples());
    }
}

//==============================================================================
bool IrplayerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *IrplayerAudioProcessor::createEditor()
{
    return new IrplayerAudioProcessorEditor(*this);
}

//==============================================================================
void IrplayerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void IrplayerAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new IrplayerAudioProcessor();
}
