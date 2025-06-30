#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "YmfmWrapper.h"
#include "YmfmWrapperInterface.h"
#include "../utils/Debug.h"
#include <vector>
#include <memory>
#include <functional>

// Convenience aliases for parameter enums
using OperatorParameter = YmfmWrapperInterface::OperatorParameter;
using ChannelParameter = YmfmWrapperInterface::ChannelParameter;

/**
 * UnisonEngine - Multi-Instance YM2151 Engine for Unison Effect
 * 
 * This class manages multiple YmfmWrapper instances to create unison effect.
 * Each instance represents a complete YM2151 chip with 8 channels.
 * 
 * Key Features:
 * - 1-4 voice unison (controlled by voice count)
 * - Detune per voice for thickness 
 * - Stereo spread control
 * - Independent YM2151 instances for true parallel processing
 * 
 * Design Philosophy:
 * - Each unison voice = complete YM2151 instance
 * - Maintains 8-voice polyphony within each instance
 * - CPU cost scales linearly with voice count (predictable)
 */
class UnisonEngine {
public:
    UnisonEngine();
    ~UnisonEngine();
    
    // Initialization
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void reset();
    
    // Unison settings
    void setVoiceCount(int count);              // 1-4 voices
    void setDetune(float cents);                // 0-50 cents detune amount
    void setStereoSpread(float percent);        // 0-100% stereo spread
    void setStereoMode(int mode);               // 0=Off, 1=Auto, 2=Wide, 3=Narrow
    
    // Audio processing
    void processBlock(juce::AudioBuffer<float>& buffer, 
                     juce::MidiBuffer& midiMessages);
    
    // Register access (applies to all instances)
    void writeRegister(uint8_t reg, uint8_t value);
    void writeChannelRegister(int channel, uint8_t reg, uint8_t value);
    
    // Note management (delegates to all instances with detune)
    void noteOn(int channel, int note, float velocity);
    void noteOff(int channel, int note);
    void allNotesOff();
    
    // Parameter delegation
    void setOperatorParameter(uint8_t channel, uint8_t operator_num, 
                            OperatorParameter param, uint8_t value);
    void setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value);
    void setAlgorithm(uint8_t channel, uint8_t algorithm);
    void setFeedback(uint8_t channel, uint8_t feedback);
    void setChannelPan(uint8_t channel, float panValue);
    void setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform);
    void setNoiseParameters(bool enable, uint8_t frequency);
    
    // State access
    int getActiveVoiceCount() const { return activeVoices; }
    bool isUnisonEnabled() const { return activeVoices > 1; }
    float getCurrentDetune() const { return detuneAmount; }
    float getCurrentStereoSpread() const { return stereoSpread; }
    
    // Performance monitoring
    double getCpuUsage() const { return cpuUsage; }

private:
    /**
     * VoiceInstance - Single YM2151 instance with its properties
     */
    struct VoiceInstance {
        std::unique_ptr<YmfmWrapper> wrapper;
        float detuneRatio = 1.0f;      // Frequency multiplier for detune
        float panPosition = 0.5f;      // Stereo position (0=left, 1=right)
        float gainMultiplier = 1.0f;   // Volume adjustment
        bool isActive = true;
        
        VoiceInstance();
        ~VoiceInstance() = default;
        
        // Prevent copying
        VoiceInstance(const VoiceInstance&) = delete;
        VoiceInstance& operator=(const VoiceInstance&) = delete;
        
        // Allow moving
        VoiceInstance(VoiceInstance&&) = default;
        VoiceInstance& operator=(VoiceInstance&&) = default;
    };
    
    // Instance management
    std::vector<VoiceInstance> instances;
    int activeVoices = 1;
    
    // Unison parameters
    float detuneAmount = 0.0f;         // Detune amount in cents
    float stereoSpread = 80.0f;        // Stereo spread percentage
    int stereoMode = 1;                // Stereo mode (Auto)
    
    // Audio setup
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    // Performance monitoring
    mutable double cpuUsage = 0.0;
    juce::Time lastProcessTime;
    
    // Internal methods
    void updateInstanceCount();
    void updateDetuneRatios();
    void updateStereoPositions();
    void updateGainMultipliers();
    
    // Calculation helpers
    float calculateDetuneRatio(int voiceIndex, int totalVoices) const;
    float calculateStereoPosition(int voiceIndex, int totalVoices) const;
    float calculateGainMultiplier(int voiceIndex, int totalVoices) const;
    
    // Note frequency helpers
    std::pair<uint8_t, uint8_t> applyDetuneToNote(int note, float detuneRatio) const;
    float noteToFrequency(int note) const;
    float frequencyToNote(float frequency) const;
    
    // Processing helpers
    void processInstanceAudio(VoiceInstance& instance, 
                            juce::AudioBuffer<float>& tempBuffer,
                            int numSamples);
    void mixInstanceToOutput(const VoiceInstance& instance,
                           const juce::AudioBuffer<float>& instanceBuffer,
                           juce::AudioBuffer<float>& outputBuffer,
                           int numSamples);
    
    // Thread safety
    juce::CriticalSection parameterLock;
    
    // Voice configuration
    void configureBasicVoice(YmfmWrapper* wrapper);
    
    // Debug and logging
    void logInstanceState(const VoiceInstance& instance, int index) const;
    void logUnisonState() const;
};