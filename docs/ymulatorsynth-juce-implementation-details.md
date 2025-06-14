# YMulator-Synth JUCE実装詳細

このドキュメントは、JUCEフレームワークを使用したYMulator-Synth実装の具体的な技術詳細を提供します。

## 1. JUCEパラメータシステム実装

### 1.1 AudioProcessorValueTreeStateの構築

```cpp
// PluginProcessor.h
class YMulatorSynthAudioProcessor : public juce::AudioProcessor
{
private:
    AudioProcessorValueTreeState parameters;
    
public:
    YMulatorSynthAudioProcessor();
    
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};

// PluginProcessor.cpp
AudioProcessorValueTreeState::ParameterLayout YMulatorSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    // グローバルパラメータ
    params.push_back(std::make_unique<AudioParameterInt>(
        "algorithm", "Algorithm", 0, 7, 0));
    params.push_back(std::make_unique<AudioParameterInt>(
        "feedback", "Feedback", 0, 7, 0));
    
    // オペレータパラメータ（4つのオペレータ）
    for (int op = 1; op <= 4; ++op)
    {
        String opId = "op" + String(op);
        
        // Total Level (TL)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_tl", "OP" + String(op) + " TL", 
            0, 127, 99));
        
        // Attack Rate (AR)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_ar", "OP" + String(op) + " AR", 
            0, 31, 31));
        
        // Decay Rate (D1R)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_d1r", "OP" + String(op) + " D1R", 
            0, 31, 0));
        
        // Sustain Rate (D2R)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_d2r", "OP" + String(op) + " D2R", 
            0, 31, 0));
        
        // Release Rate (RR)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_rr", "OP" + String(op) + " RR", 
            0, 15, 7));
        
        // Sustain Level (D1L)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_d1l", "OP" + String(op) + " D1L", 
            0, 15, 0));
        
        // Multiple (MUL)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_mul", "OP" + String(op) + " MUL", 
            0, 15, 1));
        
        // Detune 1 (DT1)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_dt1", "OP" + String(op) + " DT1", 
            0, 7, 3));
        
        // Key Scale (KS)
        params.push_back(std::make_unique<AudioParameterInt>(
            opId + "_ks", "OP" + String(op) + " KS", 
            0, 3, 0));
        
        // AM Enable (AMS-EN)
        params.push_back(std::make_unique<AudioParameterBool>(
            opId + "_ams_en", "OP" + String(op) + " AMS-EN", false));
    }
    
    return { params.begin(), params.end() };
}
```

### 1.2 MIDI CCマッピング実装

```cpp
// PluginProcessor.h
class YMulatorSynthAudioProcessor : public juce::AudioProcessor
{
private:
    std::unordered_map<int, AudioParameterInt*> ccToParameterMap;
    
    void setupCCMapping();
    void handleMidiCC(int ccNumber, int value);
};

// PluginProcessor.cpp
void YMulatorSynthAudioProcessor::setupCCMapping()
{
    // VOPMex互換MIDI CCマッピング
    
    // グローバルパラメータ
    ccToParameterMap[14] = dynamic_cast<AudioParameterInt*>(parameters.getParameter("algorithm"));
    ccToParameterMap[15] = dynamic_cast<AudioParameterInt*>(parameters.getParameter("feedback"));
    
    // オペレータパラメータ（4つのオペレータ）
    for (int op = 1; op <= 4; ++op)
    {
        String opId = "op" + String(op);
        int opIndex = op - 1;
        
        // Total Level (CC 16-19)
        ccToParameterMap[16 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_tl"));
        
        // Multiple (CC 20-23)
        ccToParameterMap[20 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_mul"));
        
        // Detune1 (CC 24-27)
        ccToParameterMap[24 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_dt1"));
        
        // Attack Rate (CC 43-46)
        ccToParameterMap[43 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_ar"));
        
        // Decay1 Rate (CC 47-50)
        ccToParameterMap[47 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_d1r"));
        
        // Sustain Rate (CC 51-54)
        ccToParameterMap[51 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_d2r"));
        
        // Release Rate (CC 55-58)
        ccToParameterMap[55 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_rr"));
        
        // Sustain Level (CC 59-62)
        ccToParameterMap[59 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_d1l"));
        
        // Key Scale (CC 39-42)
        ccToParameterMap[39 + opIndex] = 
            dynamic_cast<AudioParameterInt*>(parameters.getParameter(opId + "_ks"));
    }
}

void YMulatorSynthAudioProcessor::handleMidiCC(int ccNumber, int value)
{
    auto it = ccToParameterMap.find(ccNumber);
    if (it != ccToParameterMap.end() && it->second != nullptr)
    {
        // 正規化（0-127を0.0-1.0に変換）
        float normalizedValue = juce::jlimit(0.0f, 1.0f, value / 127.0f);
        
        // パラメータ更新（スレッドセーフ）
        it->second->setValueNotifyingHost(normalizedValue);
    }
}
```

### 1.3 processBlock内でのMIDI処理

```cpp
void YMulatorSynthAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    
    // MIDIメッセージ処理
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn())
        {
            ymfmWrapper.noteOn(message.getChannel() - 1, 
                              message.getNoteNumber(), 
                              message.getVelocity());
        }
        else if (message.isNoteOff())
        {
            ymfmWrapper.noteOff(message.getChannel() - 1, 
                               message.getNoteNumber());
        }
        else if (message.isController())
        {
            handleMidiCC(message.getControllerNumber(), 
                        message.getControllerValue());
        }
    }
    
    // パラメータ更新をymfmWrapperに適用
    updateYmfmParameters();
    
    // オーディオ生成
    ymfmWrapper.processBlock(buffer);
}
```

## 2. Audio Unit Factory Preset実装

### 2.1 基本構造

```cpp
// PluginProcessor.h
class YMulatorSynthAudioProcessor : public juce::AudioProcessor
{
private:
    int currentPreset = 0;
    static constexpr int NUM_FACTORY_PRESETS = 8;
    
public:
    int getNumPrograms() override { return NUM_FACTORY_PRESETS; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String& newName) override {}
};

// PluginProcessor.cpp
void YMulatorSynthAudioProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < NUM_FACTORY_PRESETS)
    {
        currentPreset = index;
        loadFactoryPreset(index);
    }
}

const String YMulatorSynthAudioProcessor::getProgramName(int index)
{
    const String presetNames[NUM_FACTORY_PRESETS] = {
        "Electric Piano",
        "Synth Bass", 
        "Brass Section",
        "String Pad",
        "Lead Synth",
        "Organ",
        "Bells",
        "Init"
    };
    
    if (index >= 0 && index < NUM_FACTORY_PRESETS)
        return presetNames[index];
    
    return {};
}
```

### 2.2 プリセットデータ構造

```cpp
// PresetManager.h
struct FMPreset
{
    String name;
    int algorithm;
    int feedback;
    
    struct OperatorData
    {
        int ar, d1r, d2r, rr, d1l, tl, ks, mul, dt1;
        bool amsEn;
    } operators[4];
    
    // LFO設定
    int lfoFreq, amd, pmd, waveform;
};

// PluginProcessor.cpp
void YMulatorSynthAudioProcessor::loadFactoryPreset(int index)
{
    FMPreset preset = getFactoryPreset(index);
    
    // パラメータに反映
    *parameters.getRawParameterValue("algorithm") = preset.algorithm;
    *parameters.getRawParameterValue("feedback") = preset.feedback;
    
    for (int op = 0; op < 4; ++op)
    {
        String opId = "op" + String(op + 1);
        const auto& opData = preset.operators[op];
        
        *parameters.getRawParameterValue(opId + "_ar") = opData.ar;
        *parameters.getRawParameterValue(opId + "_d1r") = opData.d1r;
        *parameters.getRawParameterValue(opId + "_d2r") = opData.d2r;
        *parameters.getRawParameterValue(opId + "_rr") = opData.rr;
        *parameters.getRawParameterValue(opId + "_d1l") = opData.d1l;
        *parameters.getRawParameterValue(opId + "_tl") = opData.tl;
        *parameters.getRawParameterValue(opId + "_ks") = opData.ks;
        *parameters.getRawParameterValue(opId + "_mul") = opData.mul;
        *parameters.getRawParameterValue(opId + "_dt1") = opData.dt1;
        *parameters.getRawParameterValue(opId + "_ams_en") = opData.amsEn ? 1.0f : 0.0f;
    }
    
    // UIに変更を通知
    parameters.state.sendPropertyChangeMessage("presetChanged");
}
```

## 3. 状態保存・復元

### 3.1 getStateInformation/setStateInformation実装

```cpp
void YMulatorSynthAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = parameters.copyState();
    
    // 現在のプリセット番号も保存
    state.setProperty("currentPreset", currentPreset, nullptr);
    
    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void YMulatorSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = ValueTree::fromXml(*xmlState);
            parameters.replaceState(newState);
            
            // プリセット番号も復元
            currentPreset = newState.getProperty("currentPreset", 0);
        }
    }
}
```

## 4. パフォーマンス最適化

### 4.1 パラメータ更新レート制限

```cpp
class YMulatorSynthAudioProcessor : public juce::AudioProcessor
{
private:
    std::atomic<int> parameterUpdateCounter{0};
    static constexpr int PARAMETER_UPDATE_RATE_DIVIDER = 8;
    
    void updateYmfmParameters();
};

void YMulatorSynthAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // パラメータ更新レート制限
    bool shouldUpdateParams = (++parameterUpdateCounter % PARAMETER_UPDATE_RATE_DIVIDER) == 0;
    
    if (shouldUpdateParams)
    {
        updateYmfmParameters();
    }
    
    // 通常の音声処理...
}
```

### 4.2 メモリアロケーション回避

```cpp
class YMulatorSynthAudioProcessor : public juce::AudioProcessor
{
private:
    // 事前に確保済みのスクラッチバッファ
    std::array<float, 4096> scratchBuffer;
    size_t scratchIndex = 0;
    
    // ロックフリーパラメータキュー
    struct ParameterChange
    {
        int parameterId;
        float value;
    };
    
    juce::AbstractFifo parameterQueue{1024};
    std::array<ParameterChange, 1024> parameterBuffer;
    
public:
    float* getScratchBuffer(size_t samples);
    void queueParameterChange(int id, float value);
};
```

この実装により、JUCEの機能を最大限活用したリアルタイム音声処理対応のFMシンセサイザーが構築できます。