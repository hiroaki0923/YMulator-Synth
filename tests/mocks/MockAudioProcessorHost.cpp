#include "MockAudioProcessorHost.h"
#include <cmath>
#include <numeric>

namespace YMulatorSynth {
namespace Test {

MockAudioProcessorHost::MockAudioProcessorHost()
    : currentSampleRate(44100.0)
    , currentBlockSize(512)
    , isPlaying(false)
{
    mockPlayHead = std::make_unique<MockPlayHead>(*this);
    
    // Initialize with default position info
    juce::AudioPlayHead::PositionInfo info;
    info.setTimeInSamples(0);
    info.setTimeInSeconds(0.0);
    info.setPpqPosition(0.0);
    info.setBpm(120.0);
    info.setTimeSignature(juce::AudioPlayHead::TimeSignature{4, 4});
    info.setIsPlaying(false);
    currentPositionInfo = info;
}

MockAudioProcessorHost::~MockAudioProcessorHost() = default;

void MockAudioProcessorHost::initializeProcessor(juce::AudioProcessor& processor,
                                               double sampleRate,
                                               int blockSize,
                                               int numChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    
    processor.setPlayHead(mockPlayHead.get());
    processor.prepareToPlay(sampleRate, blockSize);
    
    processedBuffer.setSize(numChannels, blockSize);
    processedBuffer.clear();
    
    // Add this host as listener for all parameters
    for (auto* param : processor.getParameters()) {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            param->addListener(this);
        }
    }
    
    processor.addListener(this);
}

void MockAudioProcessorHost::processBlock(juce::AudioProcessor& processor, int numSamples)
{
    jassert(numSamples <= processedBuffer.getNumSamples());
    
    // Update position info
    if (currentPositionInfo.hasValue() && isPlaying) {
        auto info = *currentPositionInfo;
        auto currentTimeInSamples = info.getTimeInSamples().orFallback(0);
        auto currentTimeInSeconds = info.getTimeInSeconds().orFallback(0.0);
        auto currentPpq = info.getPpqPosition().orFallback(0.0);
        auto bpm = info.getBpm().orFallback(120.0);
        
        info.setTimeInSamples(currentTimeInSamples + numSamples);
        info.setTimeInSeconds(currentTimeInSeconds + (numSamples / currentSampleRate));
        info.setPpqPosition(currentPpq + (numSamples / currentSampleRate / 60.0 * bpm));
        
        currentPositionInfo = info;
    }
    
    // Create audio buffer for processing
    juce::AudioBuffer<float> buffer(processedBuffer.getNumChannels(), numSamples);
    buffer.clear();
    
    // Process the block
    juce::MidiBuffer processBuffer;
    processBuffer.swapWith(midiBuffer);
    processor.processBlock(buffer, processBuffer);
    
    // Store the processed audio
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        processedBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }
    
    midiBuffer.clear();
}

void MockAudioProcessorHost::sendMidiNoteOn(juce::AudioProcessor& processor, 
                                           int channel, int noteNumber, int velocity)
{
    juce::MidiMessage msg = juce::MidiMessage::noteOn(channel, noteNumber, static_cast<uint8_t>(velocity));
    midiBuffer.addEvent(msg, 0);
}

void MockAudioProcessorHost::sendMidiNoteOff(juce::AudioProcessor& processor,
                                            int channel, int noteNumber)
{
    juce::MidiMessage msg = juce::MidiMessage::noteOff(channel, noteNumber);
    midiBuffer.addEvent(msg, 0);
}

void MockAudioProcessorHost::sendMidiCC(juce::AudioProcessor& processor,
                                       int channel, int ccNumber, int value)
{
    juce::MidiMessage msg = juce::MidiMessage::controllerEvent(channel, ccNumber, value);
    midiBuffer.addEvent(msg, 0);
}

void MockAudioProcessorHost::sendMidiPitchBend(juce::AudioProcessor& processor,
                                              int channel, int pitchBendValue)
{
    juce::MidiMessage msg = juce::MidiMessage::pitchWheel(channel, pitchBendValue);
    midiBuffer.addEvent(msg, 0);
}

void MockAudioProcessorHost::sendMidiEvents(juce::AudioProcessor& processor,
                                           const juce::MidiBuffer& midiEvents)
{
    midiBuffer.addEvents(midiEvents, 0, -1, 0);
}

void MockAudioProcessorHost::setParameterValue(juce::AudioProcessor& processor,
                                              const juce::String& parameterID,
                                              float value)
{
    for (auto* param : processor.getParameters()) {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            if (paramWithID->paramID == parameterID) {
                param->setValueNotifyingHost(value);
                return;
            }
        }
    }
}

void MockAudioProcessorHost::setParameterValueWithGesture(juce::AudioProcessor& processor,
                                                         const juce::String& parameterID,
                                                         float value)
{
    // Find parameter and its index
    auto& allParams = processor.getParameters();
    for (int i = 0; i < allParams.size(); ++i) {
        auto* param = allParams[i];
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            if (paramWithID->paramID == parameterID) {
                // Simulate user gesture by calling parameterGestureChanged directly
                if (auto* listener = dynamic_cast<juce::AudioProcessorParameter::Listener*>(&processor)) {
                    listener->parameterGestureChanged(i, true);  // Begin gesture
                    param->setValueNotifyingHost(value);
                    listener->parameterGestureChanged(i, false); // End gesture
                }
                return;
            }
        }
    }
}

float MockAudioProcessorHost::getParameterValue(juce::AudioProcessor& processor,
                                               const juce::String& parameterID)
{
    for (auto* param : processor.getParameters()) {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            if (paramWithID->paramID == parameterID) {
                return param->getValue();
            }
        }
    }
    return 0.0f;
}

void MockAudioProcessorHost::automateParameter(juce::AudioProcessor& processor,
                                              const juce::String& parameterID,
                                              const std::vector<float>& values,
                                              int samplesPerValue)
{
    for (size_t i = 0; i < values.size(); ++i) {
        // Set parameter value
        setParameterValue(processor, parameterID, values[i]);
        
        // Process audio to apply the parameter change
        processBlock(processor, samplesPerValue);
    }
}

void MockAudioProcessorHost::setTransportPlaying(bool playing)
{
    isPlaying = playing;
    if (currentPositionInfo.hasValue()) {
        auto info = *currentPositionInfo;
        info.setIsPlaying(playing);
        currentPositionInfo = info;
    }
}

void MockAudioProcessorHost::setTransportPosition(double ppqPosition)
{
    if (currentPositionInfo.hasValue()) {
        auto info = *currentPositionInfo;
        auto bpm = info.getBpm().orFallback(120.0);
        
        info.setPpqPosition(ppqPosition);
        info.setTimeInSeconds((ppqPosition / bpm) * 60.0);
        info.setTimeInSamples(static_cast<int64_t>((ppqPosition / bpm) * 60.0 * currentSampleRate));
        
        currentPositionInfo = info;
    }
}

void MockAudioProcessorHost::setBPM(double bpm)
{
    if (currentPositionInfo.hasValue()) {
        auto info = *currentPositionInfo;
        info.setBpm(bpm);
        currentPositionInfo = info;
    }
}

juce::MemoryBlock MockAudioProcessorHost::saveProcessorState(juce::AudioProcessor& processor)
{
    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);
    return stateData;
}

void MockAudioProcessorHost::loadProcessorState(juce::AudioProcessor& processor,
                                               const juce::MemoryBlock& state)
{
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}

void MockAudioProcessorHost::audioProcessorParameterChanged(juce::AudioProcessor* processor,
                                                           int parameterIndex, float newValue)
{
    int64_t timestamp = 0;
    if (currentPositionInfo.hasValue()) {
        timestamp = currentPositionInfo->getTimeInSamples().orFallback(0);
    }
    parameterChanges.push_back({parameterIndex, newValue, timestamp});
}

void MockAudioProcessorHost::audioProcessorChanged(juce::AudioProcessor* processor,
                                                  const ChangeDetails& details)
{
    // Handle processor changes if needed
}

void MockAudioProcessorHost::parameterValueChanged(int parameterIndex, float newValue)
{
    // Track parameter changes
}

void MockAudioProcessorHost::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    // Track parameter gestures if needed
}

void MockAudioProcessorHost::clearProcessedBuffer()
{
    processedBuffer.clear();
}

bool MockAudioProcessorHost::hasNonSilentOutput(float threshold) const
{
    for (int ch = 0; ch < processedBuffer.getNumChannels(); ++ch) {
        const float* samples = processedBuffer.getReadPointer(ch);
        for (int i = 0; i < processedBuffer.getNumSamples(); ++i) {
            if (std::abs(samples[i]) > threshold) {
                return true;
            }
        }
    }
    return false;
}

float MockAudioProcessorHost::getRMSLevel(int channel) const
{
    if (channel >= processedBuffer.getNumChannels()) return 0.0f;
    
    const float* samples = processedBuffer.getReadPointer(channel);
    float sum = 0.0f;
    
    for (int i = 0; i < processedBuffer.getNumSamples(); ++i) {
        sum += samples[i] * samples[i];
    }
    
    return std::sqrt(sum / processedBuffer.getNumSamples());
}

float MockAudioProcessorHost::getPeakLevel(int channel) const
{
    if (channel >= processedBuffer.getNumChannels()) return 0.0f;
    
    const float* samples = processedBuffer.getReadPointer(channel);
    float peak = 0.0f;
    
    for (int i = 0; i < processedBuffer.getNumSamples(); ++i) {
        peak = std::max(peak, std::abs(samples[i]));
    }
    
    return peak;
}

// AudioOutputVerifier implementation
AudioOutputVerifier::AudioOutputVerifier(const juce::AudioBuffer<float>& buffer)
    : bufferRef(buffer)
{
}

bool AudioOutputVerifier::verifyChannelCount(int expectedChannels) const
{
    return bufferRef.getNumChannels() == expectedChannels;
}

bool AudioOutputVerifier::verifySampleCount(int expectedSamples) const
{
    return bufferRef.getNumSamples() == expectedSamples;
}

bool AudioOutputVerifier::verifyNotSilent(float threshold) const
{
    for (int ch = 0; ch < bufferRef.getNumChannels(); ++ch) {
        const float* samples = bufferRef.getReadPointer(ch);
        for (int i = 0; i < bufferRef.getNumSamples(); ++i) {
            if (std::abs(samples[i]) > threshold) {
                return true;
            }
        }
    }
    return false;
}

bool AudioOutputVerifier::verifySilent(float threshold) const
{
    return !verifyNotSilent(threshold);
}

bool AudioOutputVerifier::verifyRMSRange(int channel, float minRMS, float maxRMS) const
{
    float rms = calculateRMS(channel);
    return rms >= minRMS && rms <= maxRMS;
}

bool AudioOutputVerifier::verifyPeakRange(int channel, float minPeak, float maxPeak) const
{
    float peak = calculatePeak(channel);
    return peak >= minPeak && peak <= maxPeak;
}

bool AudioOutputVerifier::verifyFrequencyContent(int channel, float targetFrequency,
                                                float tolerance, double sampleRate) const
{
    float detectedFreq = detectFundamentalFrequency(channel, sampleRate);
    return std::abs(detectedFreq - targetFrequency) <= tolerance;
}

float AudioOutputVerifier::calculateRMS(int channel) const
{
    if (channel >= bufferRef.getNumChannels()) return 0.0f;
    
    const float* samples = bufferRef.getReadPointer(channel);
    float sum = 0.0f;
    
    for (int i = 0; i < bufferRef.getNumSamples(); ++i) {
        sum += samples[i] * samples[i];
    }
    
    return std::sqrt(sum / bufferRef.getNumSamples());
}

float AudioOutputVerifier::calculatePeak(int channel) const
{
    if (channel >= bufferRef.getNumChannels()) return 0.0f;
    
    const float* samples = bufferRef.getReadPointer(channel);
    float peak = 0.0f;
    
    for (int i = 0; i < bufferRef.getNumSamples(); ++i) {
        peak = std::max(peak, std::abs(samples[i]));
    }
    
    return peak;
}

float AudioOutputVerifier::detectFundamentalFrequency(int channel, double sampleRate) const
{
    // Simple zero-crossing based frequency detection
    if (channel >= bufferRef.getNumChannels()) return 0.0f;
    
    const float* samples = bufferRef.getReadPointer(channel);
    int zeroCrossings = 0;
    
    for (int i = 1; i < bufferRef.getNumSamples(); ++i) {
        if ((samples[i-1] <= 0 && samples[i] > 0) ||
            (samples[i-1] >= 0 && samples[i] < 0)) {
            zeroCrossings++;
        }
    }
    
    double duration = bufferRef.getNumSamples() / sampleRate;
    return (zeroCrossings / 2.0) / duration;
}

// MidiSequenceGenerator implementation
MidiSequenceGenerator::MidiSequenceGenerator()
{
}

void MidiSequenceGenerator::addNoteOn(int timeInSamples, int channel,
                                     int noteNumber, int velocity)
{
    sequence.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, 
                                                static_cast<uint8_t>(velocity)),
                     timeInSamples);
}

void MidiSequenceGenerator::addNoteOff(int timeInSamples, int channel, int noteNumber)
{
    sequence.addEvent(juce::MidiMessage::noteOff(channel, noteNumber), timeInSamples);
}

void MidiSequenceGenerator::addCC(int timeInSamples, int channel, int ccNumber, int value)
{
    sequence.addEvent(juce::MidiMessage::controllerEvent(channel, ccNumber, value),
                     timeInSamples);
}

void MidiSequenceGenerator::addPitchBend(int timeInSamples, int channel, int pitchBendValue)
{
    sequence.addEvent(juce::MidiMessage::pitchWheel(channel, pitchBendValue),
                     timeInSamples);
}

void MidiSequenceGenerator::addChord(int timeInSamples, int channel,
                                    const std::vector<int>& noteNumbers, int velocity)
{
    for (int note : noteNumbers) {
        addNoteOn(timeInSamples, channel, note, velocity);
    }
}

void MidiSequenceGenerator::addArpeggio(int startTime, int channel,
                                       const std::vector<int>& noteNumbers,
                                       int noteDuration, int velocity)
{
    int currentTime = startTime;
    for (int note : noteNumbers) {
        addNoteOn(currentTime, channel, note, velocity);
        addNoteOff(currentTime + noteDuration, channel, note);
        currentTime += noteDuration;
    }
}

juce::MidiBuffer MidiSequenceGenerator::generateBuffer() const
{
    return sequence;
}

void MidiSequenceGenerator::clear()
{
    sequence.clear();
}

} // namespace Test
} // namespace YMulatorSynth