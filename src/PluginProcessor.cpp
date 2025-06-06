#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthAudioProcessor::ChipSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

ChipSynthAudioProcessor::~ChipSynthAudioProcessor()
{
}

const juce::String ChipSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChipSynthAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ChipSynthAudioProcessor::producesMidi() const
{
    return false;
}

bool ChipSynthAudioProcessor::isMidiEffect() const
{
    return false;
}

double ChipSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChipSynthAudioProcessor::getNumPrograms()
{
    return 1;
}

int ChipSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChipSynthAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ChipSynthAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ChipSynthAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ChipSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void ChipSynthAudioProcessor::releaseResources()
{
}

bool ChipSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ChipSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    
    buffer.clear();
}

bool ChipSynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ChipSynthAudioProcessor::createEditor()
{
    return new ChipSynthAudioProcessorEditor(*this);
}

void ChipSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void ChipSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipSynthAudioProcessor();
}