# YMulator-Synth リファクタリング戦略

**作成日**: 2025-06-28  
**状態**: 計画中  
**目的**: 大きすぎるクラスの分割とテスタビリティの向上

## 概要

YMulator-Synthは既に優秀なアーキテクチャ（依存性注入、インターフェース設計）を持っているが、いくつかの大きなクラスがSingle Responsibility Principleに違反している。本リファクタリングにより、テスタビリティと保守性を大幅に向上させる。

## 現状分析

### 🔴 Critical Issues (高優先度)

#### 1. PluginProcessor.cpp - 1,514行のモノリス
**問題**:
- MIDI処理、パラメータ管理、プリセット管理、音声処理オーケストレーション、DAW統合を1つのクラスで処理
- 48メソッド、テスト困難
- 責任範囲が広すぎる

#### 2. MainComponent.cpp - 1,104行のUI オーケストレーター  
**問題**:
- Global controls、LFO panels、Preset管理、File操作、Parameter attachmentを1つのクラスで処理
- UIロジックとビジネスロジックが混在
- 個別コンポーネントのテストが困難

### 🟡 Medium Priority Issues

#### 3. YmfmWrapper.cpp - 978行のハードウェア抽象化
**現状**: 良いインターフェース設計だが、実装で複数の責任を持つ
- ハードウェアレジスタ管理
- MIDI note変換
- パラメータバリデーション

#### 4. PresetManager.cpp - 879行のデータ管理
**現状**: 構造化されているが、複数のストレージ関心事を処理
- Factory preset管理
- User preset永続化
- OPMファイル形式処理

### ✅ Well-Designed Components (変更不要)

- **OperatorPanel.cpp (242行)**: データ駆動アプローチ、優秀設計
- **VoiceManager.cpp (226行)**: 完璧なインターフェース実装
- **PluginEditor.cpp (23行)**: 理想的な委譲パターン

## リファクタリング計画

### Phase 1: PluginProcessor分割 (最優先)

#### Step 1: MidiProcessor 抽出

```cpp
class MidiProcessor {
public:
    MidiProcessor(VoiceManagerInterface& voiceManager, 
                 YmfmWrapperInterface& ymfmWrapper);
                 
    void processMidiMessages(juce::MidiBuffer& midiMessages);
    void processMidiNoteOn(const juce::MidiMessage& message);
    void processMidiNoteOff(const juce::MidiMessage& message);
    void handleMidiCC(int ccNumber, int value);
    void handlePitchBend(int pitchBendValue);
    
private:
    VoiceManagerInterface& voiceManager;
    YmfmWrapperInterface& ymfmWrapper;
    std::unordered_map<int, juce::RangedAudioParameter*> ccToParameterMap;
};
```

**抽出対象メソッド**:
- `processMidiMessages()`
- `processMidiNoteOn()` / `processMidiNoteOff()`
- `handleMidiControlChange()`
- `handlePitchBend()`
- `setupCCMapping()`

**テストケース例**:
```cpp
TEST(MidiProcessorTest, CCToParameterMapping) {
    MockYmfmWrapper mockYmfm;
    MockVoiceManager mockVoices;
    MidiProcessor processor(mockYmfm, mockVoices);
    
    processor.handleMidiCC(14, 64); // Algorithm CC
    EXPECT_CALL(mockYmfm, setAlgorithm(4));
}
```

#### Step 2: ParameterManager 抽出

```cpp
class ParameterManager {
public:
    ParameterManager(YmfmWrapperInterface& ymfmWrapper,
                    juce::AudioProcessorValueTreeState& parameters);
                    
    void updateYmfmParameters();
    void setupCCMapping();
    void handleParameterValueChanged(int parameterIndex, float newValue);
    void handleParameterGestureChanged(int parameterIndex, bool gestureIsStarting);
    
private:
    YmfmWrapperInterface& ymfmWrapper;
    juce::AudioProcessorValueTreeState& parameters;
    std::atomic<int> parameterUpdateCounter{0};
};
```

**抽出対象メソッド**:
- `updateYmfmParameters()`
- `parameterValueChanged()` / `parameterGestureChanged()`
- `setupCCMapping()`
- Parameter validation logic

#### Step 3: StateManager 抽出

```cpp
class StateManager {
public:
    StateManager(PresetManagerInterface& presetManager,
                ParameterManager& parameterManager);
                
    void loadPreset(int index);
    void loadPreset(const ymulatorsynth::Preset* preset);
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    
private:
    PresetManagerInterface& presetManager;
    ParameterManager& parameterManager;
};
```

**抽出対象メソッド**:
- `getStateInformation()` / `setStateInformation()`
- `loadPreset()` variations
- `getCurrentProgram()` / `setCurrentProgram()`

#### Step 4: PanProcessor 抽出

```cpp
class PanProcessor {
public:
    PanProcessor(YmfmWrapperInterface& ymfmWrapper);
    
    void applyGlobalPan(int channel);
    void applyGlobalPanToAllChannels();
    void setChannelRandomPan(int channel);
    
private:
    YmfmWrapperInterface& ymfmWrapper;
    uint8_t channelRandomPanBits[8];
};
```

**抽出対象メソッド**:
- `applyGlobalPan()`
- `applyGlobalPanToAllChannels()`
- `setChannelRandomPan()`

### Phase 2: MainComponent分割

#### Step 1: PresetUIManager 抽出

```cpp
class PresetUIManager : public juce::Component {
public:
    PresetUIManager(YMulatorSynthAudioProcessor& audioProcessor);
    
    void setupPresetSelector();
    void updateBankComboBox();
    void updatePresetComboBox(); 
    void onBankChanged();
    void onPresetChanged();
    void loadOpmFileDialog();
    void savePresetDialog();
    
private:
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::ComboBox> bankComboBox;
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::TextButton> loadButton;
    std::unique_ptr<juce::TextButton> saveButton;
};
```

#### Step 2: GlobalControlsPanel 抽出

```cpp
class GlobalControlsPanel : public juce::Component {
public:
    GlobalControlsPanel(juce::AudioProcessorValueTreeState& parameters);
    
    void setupGlobalControls();
    void setupLfoControls();
    void resized() override;
    void paint(juce::Graphics& g) override;
    
private:
    juce::AudioProcessorValueTreeState& parameters;
    std::unique_ptr<juce::ComboBox> algorithmComboBox;
    std::unique_ptr<RotaryKnob> feedbackKnob;
    std::unique_ptr<RotaryKnob> globalPanKnob;
    // LFO controls
};
```

#### Step 3: 簡略化されたMainComponent

```cpp
class MainComponent : public juce::Component {
public:
    MainComponent(YMulatorSynthAudioProcessor& audioProcessor);
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
private:
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<GlobalControlsPanel> globalControls;
    std::unique_ptr<PresetUIManager> presetManager;
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    std::unique_ptr<AlgorithmDisplay> algorithmDisplay;
};
```

### Phase 3: Enhanced Abstraction (オプション)

#### AudioProcessingInterface 追加

```cpp
class AudioProcessingInterface {
public:
    virtual ~AudioProcessingInterface() = default;
    virtual void processAudioBlock(juce::AudioBuffer<float>& buffer) = 0;
    virtual void generateAudioSamples(juce::AudioBuffer<float>& buffer) = 0;
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
};
```

#### YmfmWrapper更なる分割 (必要に応じて)

```cpp
class RegisterManager {
public:
    void writeRegister(int address, uint8_t data);
    uint8_t readCurrentRegister(int address) const;
    void updateRegisterCache(uint8_t address, uint8_t value);
    
private:
    uint8_t currentRegisters[256];
};

class NoteConverter {
public:
    uint16_t noteToFnum(uint8_t note);
    uint16_t noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones);
};

class ParameterConverter {
public:
    uint8_t convertOperatorParameter(OperatorParameter param, uint8_t value);
    uint8_t convertChannelParameter(ChannelParameter param, uint8_t value);
    void validateParameterRange(uint8_t value, uint8_t min, uint8_t max);
};
```

## 実装スケジュール

### Week 1-2: Critical Refactoring
- [ ] MidiProcessor 抽出
- [ ] ParameterManager 抽出  
- [ ] 包括的テスト作成
- [ ] Audio Unit validation確認

### Week 3-4: UI Refactoring
- [ ] PresetUIManager 抽出
- [ ] GlobalControlsPanel 抽出
- [ ] UI固有のユニットテスト追加
- [ ] DAW統合検証

### Week 5: Enhanced Abstraction
- [ ] AudioProcessingInterface追加
- [ ] 必要に応じてYmfmWrapper関心事リファクタリング
- [ ] 包括的統合テスト
- [ ] パフォーマンス回帰テスト

## テスト戦略

### ビルド検証 (各リファクタリングステップ後必須)

```bash
# ビルド検証
cd /Users/hiroaki.kimura/projects/ChipSynth-AU/build && cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful" || echo "Build failed"

# Audio Unit検証
auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"

# 全テスト実行
ctest --output-on-failure --quiet
```

### ユニットテストカバレッジ目標

- **MidiProcessor**: 95%+ (CC mapping, note handling)
- **ParameterManager**: 100% (parameter updates, validation)
- **StateManager**: 100% (preset loading/saving)
- **PresetUIManager**: 90%+ (UI state management)

### Mock要件

- 全ての抽出クラスはインターフェースを受け入れる（具象実装ではない）
- 既存パターン使用: コンストラクタ経由の依存性注入
- 既存インターフェース契約維持 (YmfmWrapperInterface等)

## リスク軽減策

1. **段階的実装**: 1つずつクラスを抽出、毎回テスト実行
2. **既存テスト活用**: 155個の既存テストが回帰検出
3. **インターフェース保持**: 既存の依存性注入アーキテクチャを維持
4. **Audio Unit検証**: 各ステップ後にDAW互換性確認

## 期待される効果

### テスタビリティの向上
- 各コンポーネントを分離してユニットテスト可能
- Mock使用で外部依存関係を排除
- テスト実行時間の短縮

### 保守性の向上  
- 明確な境界により将来の変更が安全
- 小さなクラスは理解・修正が容易
- 責任範囲の明確化

### 拡張性の向上
- インターフェースベース設計で機能追加が容易
- 新しいMIDI処理ロジックやUI要素の追加が簡単
- プラグイン形式の拡張（VST3等）への準備

## 成功指標

- [ ] PluginProcessor.cpp: 1,514行 → 400-500行
- [ ] MainComponent.cpp: 1,104行 → 200-300行  
- [ ] テストカバレッジ: 現在の100% → 各新クラス90%+
- [ ] ビルド時間: 維持または改善
- [ ] Audio Unit validation: 100% pass継続
- [ ] 全155テスト: pass継続

## 次のステップ

1. **MidiProcessor抽出から開始** (最低リスク、高効果)
2. **小さなコミット単位** で継続的統合
3. **各抽出後にレビュー** とテスト実行
4. **ドキュメント更新** (ADR、設計文書)

---

**注意**: このリファクタリングは既存の優秀なアーキテクチャを活用し、テスタビリティと保守性を向上させることが目的です。既存の機能や性能を損なわないよう、慎重かつ段階的に実行します。