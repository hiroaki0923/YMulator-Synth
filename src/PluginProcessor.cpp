#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <os/log.h>

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
    juce::ignoreUnused(samplesPerBlock);
    
    // Debug output to system log
    os_log_t logger = os_log_create("com.vendor.chipsynth", "audio");
    os_log(logger, "ChipSynth: prepareToPlay called - sampleRate: %f, samplesPerBlock: %d", sampleRate, samplesPerBlock);
    std::cout << "ChipSynth: prepareToPlay called - sampleRate: " << sampleRate << ", samplesPerBlock: " << samplesPerBlock << std::endl;
    DBG("ChipSynth: prepareToPlay called - sampleRate: " + juce::String(sampleRate) + ", samplesPerBlock: " + juce::String(samplesPerBlock));
    
    // Initialize ymfm wrapper with OPM for now
    ymfmWrapper.initialize(YmfmWrapper::ChipType::OPM, static_cast<uint32_t>(sampleRate));
    
    os_log(logger, "ChipSynth: ymfm initialization complete");
    DBG("ChipSynth: ymfm initialization complete");
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
    juce::ScopedNoDenormals noDenormals;
    
    static int processBlockCallCounter = 0;
    static bool hasLoggedFirstCall = false;
    
    processBlockCallCounter++;
    
    if (!hasLoggedFirstCall) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "audio");
        os_log(logger, "ChipSynth: processBlock FIRST CALL - channels: %d, samples: %d", 
               buffer.getNumChannels(), buffer.getNumSamples());
        hasLoggedFirstCall = true;
    }
    
    // Clear output buffer
    buffer.clear();
    
    // Debug MIDI events
    if (!midiMessages.isEmpty()) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "midi");
        os_log(logger, "ChipSynth: Received %d MIDI events", midiMessages.getNumEvents());
        std::cout << "ChipSynth: Received " << midiMessages.getNumEvents() << " MIDI events" << std::endl;
        DBG("ChipSynth: Received " + juce::String(midiMessages.getNumEvents()) + " MIDI events");
    }
    
    // Process MIDI events (simple note on/off for now)
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            os_log_t logger = os_log_create("com.vendor.chipsynth", "midi");
            os_log(logger, "ChipSynth: Note ON - Note: %d, Velocity: %d", message.getNoteNumber(), message.getVelocity());
            DBG("ChipSynth: Note ON - Note: " + juce::String(message.getNoteNumber()) + 
                ", Velocity: " + juce::String(message.getVelocity()));
            ymfmWrapper.noteOn(0, message.getNoteNumber(), message.getVelocity());
        } else if (message.isNoteOff()) {
            os_log_t logger = os_log_create("com.vendor.chipsynth", "midi");
            os_log(logger, "ChipSynth: Note OFF - Note: %d", message.getNoteNumber());
            DBG("ChipSynth: Note OFF - Note: " + juce::String(message.getNoteNumber()));
            ymfmWrapper.noteOff(0, message.getNoteNumber());
        }
    }
    
    // Generate audio
    const int numSamples = buffer.getNumSamples();
    static int audioCallCounter = 0;
    audioCallCounter++;
    
    if (numSamples > 0) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "audio");
        if (audioCallCounter % 1000 == 0) {  // Log every 1000 calls to avoid spam
            os_log(logger, "ChipSynth: processBlock audio generation - call #%d, numSamples: %d", audioCallCounter, numSamples);
        }
        
        ymfmWrapper.generateSamples(buffer.getWritePointer(0), numSamples);
        
        // Apply moderate gain to prevent clipping
        buffer.applyGain(0, 0, numSamples, 2.0f);
        
        // Copy left channel to right channel for stereo
        if (buffer.getNumChannels() > 1) {
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
        }
        
        // Check for non-zero audio (debug)
        static int debugCounter = 0;
        if (++debugCounter % 44100 == 0) { // Every ~1 second at 44.1kHz
            bool hasAudio = false;
            for (int i = 0; i < numSamples; i++) {
                if (std::abs(buffer.getSample(0, i)) > 0.0001f) {
                    hasAudio = true;
                    break;
                }
            }
            DBG("ChipSynth: Audio check - " + juce::String(hasAudio ? "HAS AUDIO" : "SILENT"));
        }
    }
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