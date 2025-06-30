#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>

namespace YMulatorSynth {
namespace Test {

class MockAudioProcessorHost : public juce::AudioProcessorListener,
                                public juce::AudioProcessorParameter::Listener
{
public:
    MockAudioProcessorHost();
    ~MockAudioProcessorHost();

    // Initialize the processor with specific configuration
    void initializeProcessor(juce::AudioProcessor& processor,
                           double sampleRate = 44100.0,
                           int blockSize = 512,
                           int numChannels = 2);

    // Process audio blocks
    void processBlock(juce::AudioProcessor& processor, int numSamples);
    
    // Get the last processed audio buffer
    const juce::AudioBuffer<float>& getLastProcessedBuffer() const { return processedBuffer; }
    
    // MIDI event injection
    void sendMidiNoteOn(juce::AudioProcessor& processor, int channel, int noteNumber, int velocity);
    void sendMidiNoteOff(juce::AudioProcessor& processor, int channel, int noteNumber);
    void sendMidiCC(juce::AudioProcessor& processor, int channel, int ccNumber, int value);
    void sendMidiPitchBend(juce::AudioProcessor& processor, int channel, int pitchBendValue);
    void sendMidiEvents(juce::AudioProcessor& processor, const juce::MidiBuffer& midiBuffer);
    
    // Parameter automation
    void setParameterValue(juce::AudioProcessor& processor, const juce::String& parameterID, float value);
    float getParameterValue(juce::AudioProcessor& processor, const juce::String& parameterID);
    void automateParameter(juce::AudioProcessor& processor, const juce::String& parameterID, 
                          const std::vector<float>& values, int samplesPerValue);
    
    // User gesture simulation for custom preset testing
    void setParameterValueWithGesture(juce::AudioProcessor& processor, const juce::String& parameterID, float value);
    
    // Transport control
    void setTransportPlaying(bool playing);
    void setTransportPosition(double ppqPosition);
    void setBPM(double bpm);
    
    // State management
    juce::MemoryBlock saveProcessorState(juce::AudioProcessor& processor);
    void loadProcessorState(juce::AudioProcessor& processor, const juce::MemoryBlock& state);
    
    // Callbacks from AudioProcessorListener
    void audioProcessorParameterChanged(juce::AudioProcessor* processor,
                                       int parameterIndex, float newValue) override;
    void audioProcessorChanged(juce::AudioProcessor* processor, 
                              const ChangeDetails& details) override;
    
    // Callbacks from AudioProcessorParameter::Listener
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    
    // Test utilities
    void clearProcessedBuffer();
    bool hasNonSilentOutput(float threshold = 0.0001f) const;
    float getRMSLevel(int channel) const;
    float getPeakLevel(int channel) const;
    
private:
    juce::AudioBuffer<float> processedBuffer;
    juce::MidiBuffer midiBuffer;
    juce::Optional<juce::AudioPlayHead::PositionInfo> currentPositionInfo;
    
    double currentSampleRate;
    int currentBlockSize;
    bool isPlaying;
    
    // Parameter change tracking
    struct ParameterChange {
        int index;
        float value;
        int64_t timestamp;
    };
    std::vector<ParameterChange> parameterChanges;
    
    class MockPlayHead : public juce::AudioPlayHead {
    public:
        MockPlayHead(MockAudioProcessorHost& host) : hostRef(host) {}
        juce::Optional<PositionInfo> getPosition() const override {
            return hostRef.currentPositionInfo;
        }
    private:
        MockAudioProcessorHost& hostRef;
    };
    
    std::unique_ptr<MockPlayHead> mockPlayHead;
};

// Helper class for testing audio output
class AudioOutputVerifier {
public:
    AudioOutputVerifier(const juce::AudioBuffer<float>& buffer);
    
    bool verifyChannelCount(int expectedChannels) const;
    bool verifySampleCount(int expectedSamples) const;
    bool verifyNotSilent(float threshold = 0.0001f) const;
    bool verifySilent(float threshold = 0.0001f) const;
    bool verifyRMSRange(int channel, float minRMS, float maxRMS) const;
    bool verifyPeakRange(int channel, float minPeak, float maxPeak) const;
    bool verifyFrequencyContent(int channel, float targetFrequency, 
                               float tolerance = 5.0f, double sampleRate = 44100.0) const;
    
private:
    const juce::AudioBuffer<float>& bufferRef;
    
    float calculateRMS(int channel) const;
    float calculatePeak(int channel) const;
    float detectFundamentalFrequency(int channel, double sampleRate) const;
};

// Helper class for MIDI sequence testing
class MidiSequenceGenerator {
public:
    MidiSequenceGenerator();
    
    void addNoteOn(int timeInSamples, int channel, int noteNumber, int velocity);
    void addNoteOff(int timeInSamples, int channel, int noteNumber);
    void addCC(int timeInSamples, int channel, int ccNumber, int value);
    void addPitchBend(int timeInSamples, int channel, int pitchBendValue);
    void addChord(int timeInSamples, int channel, const std::vector<int>& noteNumbers, int velocity);
    void addArpeggio(int startTime, int channel, const std::vector<int>& noteNumbers, 
                     int noteDuration, int velocity);
    
    juce::MidiBuffer generateBuffer() const;
    void clear();
    
private:
    juce::MidiBuffer sequence;
};

} // namespace Test
} // namespace YMulatorSynth