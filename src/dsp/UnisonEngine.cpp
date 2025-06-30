#include "UnisonEngine.h"
#include "YM2151Registers.h"
#include <cmath>
#include <algorithm>

UnisonEngine::VoiceInstance::VoiceInstance() {
    wrapper = std::make_unique<YmfmWrapper>();
    CS_DBG("UnisonEngine::VoiceInstance created");
}

UnisonEngine::UnisonEngine() {
    CS_DBG("=== UnisonEngine Constructor ===");
    
    // Create initial single instance
    instances.resize(1);
    
    CS_DBG("UnisonEngine: Initial single instance created");
    CS_DBG("=== UnisonEngine Constructor Complete ===");
}

UnisonEngine::~UnisonEngine() {
    CS_DBG("=== UnisonEngine Destructor ===");
    
    instances.clear();
    
    CS_DBG("=== UnisonEngine Destructor Complete ===");
}

void UnisonEngine::prepareToPlay(double sampleRate, int samplesPerBlock) {
    CS_DBG("=== UnisonEngine::prepareToPlay ===");
    CS_DBG("Sample rate: " + juce::String(sampleRate));
    CS_DBG("Block size: " + juce::String(samplesPerBlock));
    
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    
    // Initialize all instances
    for (auto& instance : instances) {
        if (instance.wrapper) {
            instance.wrapper->initialize(YmfmWrapperInterface::ChipType::OPM, 
                                       static_cast<uint32_t>(sampleRate));
            
            // Configure basic FM voice for audio generation
            configureBasicVoice(instance.wrapper.get());
        }
    }
    
    CS_DBG("UnisonEngine: " + juce::String(instances.size()) + " instances prepared");
    CS_DBG("=== UnisonEngine::prepareToPlay Complete ===");
}

void UnisonEngine::reset() {
    CS_DBG("UnisonEngine::reset");
    
    juce::ScopedLock lock(parameterLock);
    
    // Reset all instances
    for (auto& instance : instances) {
        if (instance.wrapper) {
            instance.wrapper->reset();
        }
    }
    
    cpuUsage = 0.0;
}

void UnisonEngine::setVoiceCount(int count) {
    CS_DBG("UnisonEngine::setVoiceCount - count=" + juce::String(count));
    
    if (count < 1 || count > 4 || count == activeVoices) {
        CS_DBG("UnisonEngine::setVoiceCount - invalid or same count, ignoring");
        return;
    }
    
    juce::ScopedLock lock(parameterLock);
    
    activeVoices = count;
    updateInstanceCount();
    updateDetuneRatios();
    updateStereoPositions();
    updateGainMultipliers();
    
    CS_DBG("UnisonEngine: Voice count updated to " + juce::String(activeVoices));
    logUnisonState();
}

void UnisonEngine::setDetune(float cents) {
    CS_DBG("UnisonEngine::setDetune - cents=" + juce::String(cents, 2));
    
    cents = juce::jlimit(0.0f, 50.0f, cents);
    
    if (std::abs(cents - detuneAmount) < 0.01f) {
        return; // No significant change
    }
    
    juce::ScopedLock lock(parameterLock);
    
    detuneAmount = cents;
    updateDetuneRatios();
    
    CS_DBG("UnisonEngine: Detune amount updated to " + juce::String(detuneAmount, 2) + " cents");
}

void UnisonEngine::setStereoSpread(float percent) {
    CS_DBG("UnisonEngine::setStereoSpread - percent=" + juce::String(percent, 1));
    
    percent = juce::jlimit(0.0f, 100.0f, percent);
    
    if (std::abs(percent - stereoSpread) < 0.1f) {
        return; // No significant change
    }
    
    juce::ScopedLock lock(parameterLock);
    
    stereoSpread = percent;
    updateStereoPositions();
    
    CS_DBG("UnisonEngine: Stereo spread updated to " + juce::String(stereoSpread, 1) + "%");
}

void UnisonEngine::setStereoMode(int mode) {
    CS_DBG("UnisonEngine::setStereoMode - mode=" + juce::String(mode));
    
    mode = juce::jlimit(0, 3, mode);
    
    if (mode == stereoMode) {
        return;
    }
    
    juce::ScopedLock lock(parameterLock);
    
    stereoMode = mode;
    updateStereoPositions();
    
    juce::String modeNames[] = {"Off", "Auto", "Wide", "Narrow"};
    CS_DBG("UnisonEngine: Stereo mode updated to " + modeNames[stereoMode]);
}

void UnisonEngine::updateInstanceCount() {
    int currentSize = static_cast<int>(instances.size());
    
    CS_DBG("UnisonEngine::updateInstanceCount - current:" + juce::String(currentSize) + 
           " target:" + juce::String(activeVoices));
    
    if (activeVoices > currentSize) {
        // Add instances
        for (int i = currentSize; i < activeVoices; ++i) {
            instances.emplace_back();
            
            // Initialize the new instance if we have sample rate
            if (currentSampleRate > 0) {
                instances.back().wrapper->initialize(YmfmWrapperInterface::ChipType::OPM, 
                                                   static_cast<uint32_t>(currentSampleRate));
                
                // Configure basic FM voice for audio generation
                configureBasicVoice(instances.back().wrapper.get());
            }
            
            CS_DBG("UnisonEngine: Added instance " + juce::String(i));
        }
    } else if (activeVoices < currentSize) {
        // Remove instances
        instances.resize(activeVoices);
        CS_DBG("UnisonEngine: Removed " + juce::String(currentSize - activeVoices) + " instances");
    }
    
    CS_DBG("UnisonEngine: Instance count now " + juce::String(instances.size()));
}

void UnisonEngine::updateDetuneRatios() {
    CS_DBG("UnisonEngine::updateDetuneRatios");
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        instances[i].detuneRatio = calculateDetuneRatio(static_cast<int>(i), activeVoices);
        
        CS_DBG("Instance " + juce::String(static_cast<int>(i)) + " detune ratio: " + 
               juce::String(instances[i].detuneRatio, 6));
    }
}

void UnisonEngine::updateStereoPositions() {
    CS_DBG("UnisonEngine::updateStereoPositions");
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        instances[i].panPosition = calculateStereoPosition(static_cast<int>(i), activeVoices);
        
        CS_DBG("Instance " + juce::String(static_cast<int>(i)) + " pan position: " + 
               juce::String(instances[i].panPosition, 3));
    }
}

void UnisonEngine::updateGainMultipliers() {
    CS_DBG("UnisonEngine::updateGainMultipliers");
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        instances[i].gainMultiplier = calculateGainMultiplier(static_cast<int>(i), activeVoices);
        
        CS_DBG("Instance " + juce::String(static_cast<int>(i)) + " gain multiplier: " + 
               juce::String(instances[i].gainMultiplier, 3));
    }
}

float UnisonEngine::calculateDetuneRatio(int voiceIndex, int totalVoices) const {
    if (totalVoices <= 1 || detuneAmount <= 0.0f) {
        return 1.0f;
    }
    
    // Spread voices evenly around center
    // For 2 voices: -1, +1 (positions)
    // For 3 voices: -1, 0, +1
    // For 4 voices: -1.5, -0.5, +0.5, +1.5
    
    float position;
    if (totalVoices == 1) {
        position = 0.0f;
    } else {
        // Map voice index to symmetric positions around 0
        position = (2.0f * voiceIndex / (totalVoices - 1)) - 1.0f;
    }
    
    // Apply detune amount
    float centsOffset = position * detuneAmount;
    
    // Convert cents to frequency ratio
    // Formula: ratio = 2^(cents/1200)
    float ratio = std::pow(2.0f, centsOffset / 1200.0f);
    
    return ratio;
}

float UnisonEngine::calculateStereoPosition(int voiceIndex, int totalVoices) const {
    if (stereoMode == 0 || totalVoices <= 1) {
        return 0.5f; // Center
    }
    
    float basePosition;
    if (totalVoices == 1) {
        basePosition = 0.5f; // Center
    } else {
        // Map voice index to stereo field (0=left, 1=right)
        basePosition = static_cast<float>(voiceIndex) / (totalVoices - 1);
    }
    
    // Apply stereo spread
    float spreadAmount = stereoSpread / 100.0f;
    
    // Adjust based on stereo mode
    switch (stereoMode) {
        case 1: // Auto
            spreadAmount *= 1.0f;
            break;
        case 2: // Wide  
            spreadAmount *= 1.2f;
            break;
        case 3: // Narrow
            spreadAmount *= 0.6f;
            break;
        default:
            spreadAmount = 0.0f;
            break;
    }
    
    // Apply spread around center
    float centeredPosition = (basePosition - 0.5f) * spreadAmount + 0.5f;
    
    // Clamp to valid range
    return juce::jlimit(0.0f, 1.0f, centeredPosition);
}

float UnisonEngine::calculateGainMultiplier(int voiceIndex, int totalVoices) const {
    juce::ignoreUnused(voiceIndex);
    
    if (totalVoices <= 1) {
        return 1.0f;
    }
    
    // Use square root scaling to maintain perceived loudness
    // This prevents the unison from becoming too loud
    return 1.0f / std::sqrt(static_cast<float>(totalVoices));
}

void UnisonEngine::processBlock(juce::AudioBuffer<float>& buffer, 
                               juce::MidiBuffer& midiMessages) {
    // Performance timing start
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    // Clear output buffer
    buffer.clear();
    
    // First, process MIDI messages for all instances
    for (const auto metadata : midiMessages) {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            // Extract MIDI data
            int channel = message.getChannel() - 1;  // MIDI channels are 1-based
            int note = message.getNoteNumber();
            float velocity = message.getFloatVelocity();
            
            // Send to all instances (unison)
            noteOn(channel, note, velocity);
            
        } else if (message.isNoteOff()) {
            int channel = message.getChannel() - 1;
            int note = message.getNoteNumber();
            
            // Send to all instances
            noteOff(channel, note);
            
        } else if (message.isAllNotesOff()) {
            allNotesOff();
        }
        // Other MIDI messages can be handled here
    }
    
    // Process each active instance
    juce::AudioBuffer<float> tempBuffer(numChannels, numSamples);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        auto& instance = instances[i];
        if (!instance.wrapper) continue;
        
        // Clear temp buffer for this instance
        tempBuffer.clear();
        
        // Generate audio for this instance
        processInstanceAudio(instance, tempBuffer, numSamples);
        
        // Mix to output with stereo positioning and gain
        mixInstanceToOutput(instance, tempBuffer, buffer, numSamples);
    }
    
    // Performance timing end
    auto endTime = juce::Time::getMillisecondCounterHiRes();
    double processingTime = endTime - startTime;
    double blockDuration = (numSamples * 1000.0) / currentSampleRate;
    cpuUsage = processingTime / blockDuration;
}

void UnisonEngine::processInstanceAudio(VoiceInstance& instance,
                                      juce::AudioBuffer<float>& tempBuffer,
                                      int numSamples) {
    if (!instance.wrapper) return;
    
    // Generate samples from YmfmWrapper
    float* leftBuffer = tempBuffer.getWritePointer(0);
    float* rightBuffer = tempBuffer.getNumChannels() > 1 ? tempBuffer.getWritePointer(1) : nullptr;
    
    instance.wrapper->generateSamples(leftBuffer, rightBuffer, numSamples);
}

void UnisonEngine::mixInstanceToOutput(const VoiceInstance& instance,
                                     const juce::AudioBuffer<float>& instanceBuffer,
                                     juce::AudioBuffer<float>& outputBuffer,
                                     int numSamples) {
    if (outputBuffer.getNumChannels() < 2) {
        // Mono output
        outputBuffer.addFrom(0, 0, instanceBuffer, 0, 0, numSamples, instance.gainMultiplier);
        return;
    }
    
    // Stereo output with panning
    // Use constant power panning for better results
    float panAngle = instance.panPosition * juce::MathConstants<float>::halfPi;
    float leftGain = std::cos(panAngle) * instance.gainMultiplier;
    float rightGain = std::sin(panAngle) * instance.gainMultiplier;
    
    // Process based on source channel configuration
    if (instanceBuffer.getNumChannels() >= 2) {
        // Stereo source - apply panning to both channels
        outputBuffer.addFrom(0, 0, instanceBuffer, 0, 0, numSamples, leftGain);
        outputBuffer.addFrom(1, 0, instanceBuffer, 1, 0, numSamples, rightGain);
    } else {
        // Mono source - duplicate to both channels with panning
        outputBuffer.addFrom(0, 0, instanceBuffer, 0, 0, numSamples, leftGain);
        outputBuffer.addFrom(1, 0, instanceBuffer, 0, 0, numSamples, rightGain);
    }
}

// Register delegation methods
void UnisonEngine::writeRegister(uint8_t reg, uint8_t value) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->writeRegister(reg, value);
        }
    }
}

void UnisonEngine::writeChannelRegister(int channel, uint8_t reg, uint8_t value) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->writeRegister(reg + channel, value);
        }
    }
}

// Note management delegation
void UnisonEngine::noteOn(int channel, int note, float velocity) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            // For unison, we send the same note to all instances
            // Detune is applied by modifying the frequency after note on
            instances[i].wrapper->noteOn(static_cast<uint8_t>(channel), 
                                       static_cast<uint8_t>(note), 
                                       static_cast<uint8_t>(velocity * 127.0f));
            
            // Apply detune if this is a unison voice (more than 1 voice)
            if (activeVoices > 1 && detuneAmount > 0.0f) {
                // Calculate the detuned frequency for this voice
                float detuneRatio = instances[i].detuneRatio;
                
                // For YM2151, we need to adjust the KC (Key Code) and KF (Key Fraction)
                // This is a simplified approach - full implementation would calculate
                // proper KC/KF values based on the detuned frequency
                
                // For now, we'll use pitch bend to approximate detune
                // Convert detune ratio to semitones: semitones = 12 * log2(ratio)
                float semitones = 12.0f * std::log2(detuneRatio);
                instances[i].wrapper->setPitchBend(static_cast<uint8_t>(channel), semitones);
            }
        }
    }
}

void UnisonEngine::noteOff(int channel, int note) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->noteOff(static_cast<uint8_t>(channel), 
                                        static_cast<uint8_t>(note));
        }
    }
}

void UnisonEngine::allNotesOff() {
    juce::ScopedLock lock(parameterLock);
    
    // Implement allNotesOff by sending note off for all channels and notes
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            // Send all notes off for all channels (0-7)
            for (uint8_t ch = 0; ch < 8; ++ch) {
                for (uint8_t note = 0; note < 128; ++note) {
                    instances[i].wrapper->noteOff(ch, note);
                }
            }
        }
    }
}

// Parameter delegation methods
void UnisonEngine::setOperatorParameter(uint8_t channel, uint8_t operator_num, 
                                      OperatorParameter param, uint8_t value) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setOperatorParameter(channel, operator_num, param, value);
        }
    }
}

void UnisonEngine::setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setChannelParameter(channel, param, value);
        }
    }
}

void UnisonEngine::setAlgorithm(uint8_t channel, uint8_t algorithm) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setAlgorithm(channel, algorithm);
        }
    }
}

void UnisonEngine::setFeedback(uint8_t channel, uint8_t feedback) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setFeedback(channel, feedback);
        }
    }
}

void UnisonEngine::setChannelPan(uint8_t channel, float panValue) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setChannelPan(channel, panValue);
        }
    }
}

void UnisonEngine::setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setLfoParameters(rate, amd, pmd, waveform);
        }
    }
}

void UnisonEngine::setNoiseParameters(bool enable, uint8_t frequency) {
    juce::ScopedLock lock(parameterLock);
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        if (instances[i].wrapper) {
            instances[i].wrapper->setNoiseParameters(enable, frequency);
        }
    }
}

// Voice configuration
void UnisonEngine::configureBasicVoice(YmfmWrapper* wrapper) {
    CS_DBG("UnisonEngine: Configuring basic FM voice");
    
    for (uint8_t channel = 0; channel < 8; ++channel) {
        // Set algorithm 4 (good for basic FM sounds)
        wrapper->setAlgorithm(channel, 4);
        wrapper->setFeedback(channel, 2);
        
        // Configure operators for basic FM sound
        // Operator 0 (carrier) - main output
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::TotalLevel, 0);        // Max output
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::AttackRate, 31);       // Fast attack
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::Decay1Rate, 10);       // Medium decay
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::SustainLevel, 8);      // Some sustain
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::ReleaseRate, 5);       // Medium release
        wrapper->setOperatorParameter(channel, 0, OperatorParameter::Multiple, 1);         // 1x frequency
        
        // Operator 1 (modulator)
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::TotalLevel, 32);       // Moderate modulator level
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::AttackRate, 31);       // Fast attack
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::Decay1Rate, 10);       // Medium decay
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::SustainLevel, 8);      // Some sustain
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::ReleaseRate, 5);       // Medium release
        wrapper->setOperatorParameter(channel, 1, OperatorParameter::Multiple, 1);         // 1x frequency
        
        // Operator 2 (for algorithm 4)
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::TotalLevel, 64);       // Lower modulator level
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::AttackRate, 31);       // Fast attack
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::Decay1Rate, 10);       // Medium decay
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::SustainLevel, 8);      // Some sustain
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::ReleaseRate, 5);       // Medium release
        wrapper->setOperatorParameter(channel, 2, OperatorParameter::Multiple, 2);         // 2x frequency
        
        // Operator 3 (for algorithm 4)
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::TotalLevel, 64);       // Lower modulator level
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::AttackRate, 31);       // Fast attack
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::Decay1Rate, 10);       // Medium decay
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::SustainLevel, 8);      // Some sustain
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::ReleaseRate, 5);       // Medium release
        wrapper->setOperatorParameter(channel, 3, OperatorParameter::Multiple, 1);         // 1x frequency
    }
    
    CS_DBG("UnisonEngine: Basic FM voice configuration complete");
}

// Debug and logging
void UnisonEngine::logInstanceState(const VoiceInstance& instance, int index) const {
    CS_DBG("Instance " + juce::String(index) + ":");
    CS_DBG("  Detune ratio: " + juce::String(instance.detuneRatio, 6));
    CS_DBG("  Pan position: " + juce::String(instance.panPosition, 3));
    CS_DBG("  Gain multiplier: " + juce::String(instance.gainMultiplier, 3));
    CS_DBG("  Active: " + juce::String(instance.isActive ? "true" : "false"));
    
    // Use parameters to avoid unused warnings
    juce::ignoreUnused(instance, index);
}

void UnisonEngine::logUnisonState() const {
    CS_DBG("=== UnisonEngine State ===");
    CS_DBG("Active voices: " + juce::String(activeVoices));
    CS_DBG("Detune amount: " + juce::String(detuneAmount, 2) + " cents");
    CS_DBG("Stereo spread: " + juce::String(stereoSpread, 1) + "%");
    CS_DBG("Stereo mode: " + juce::String(stereoMode));
    CS_DBG("CPU usage: " + juce::String(cpuUsage * 100.0, 1) + "%");
    
    for (size_t i = 0; i < static_cast<size_t>(activeVoices); ++i) {
        logInstanceState(instances[i], static_cast<int>(i));
    }
    
    CS_DBG("=== End UnisonEngine State ===");
}