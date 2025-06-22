# YMulator-Synth 拡張機能実装ガイド

> **⚠️ 重要**: このガイドは現在のYMulator-Synthの実装を前提としています。  
> **実装レベル**: 基本的なFM合成、JUCE AudioProcessorフレームワーク、ymfmライブラリ統合済み  
> **必要な前提実装**: Phase 1基盤構築完了レベル（詳細は[Phase 1 Roadmap](ymulatorsynth-phase1-completion-roadmap.md)参照）

## 1. プロジェクト背景

### 1.1 YMulator-Synthとは
YMulator-Synthは、macOS用のAudio Unitプラグインで、ヤマハYM2151（OPM）チップをソフトウェアでエミュレートするFMシンセサイザーです。Aaron Giles氏のymfmライブラリを使用し、X68000やアーケードゲームで使用された本物のYM2151サウンドを再現します。

### 1.2 現在の機能
- 8ボイスポリフォニー
- 4オペレータFM合成（8アルゴリズム）
- VOPM互換のパラメータセット
- LFO（1基）によるビブラート/トレモロ
- プリセット管理システム

### 1.3 拡張の目的
YM2151の基本機能は忠実に実装されていますが、現代的な音楽制作では以下の機能が求められています：
- **ユニゾン機能**: 複数の音を重ねて厚みのあるサウンドを作る
- **ステレオ表現**: より豊かな空間表現
- **高度なモジュレーション**: 複数のLFOによる複雑な音色変化
- **動的な音色制御**: 時間軸での自動パラメータ変化

## 2. 技術仕様

### 2.1 YM2151の基本仕様
```
- 8チャンネル（8音同時発音）
- 4オペレータ/チャンネル
- パン: L/C/R の3段階（REG 0x20-0x27のbit7,6）
- 周波数設定: KC（Key Code）とKF（Key Fraction）
- オペレータパラメータ: TL, AR, D1R, D2R, RR, DT1, DT2, MUL等
```

### 2.2 ymfmライブラリの使用方法
```cpp
// YmfmWrapperクラスを介した使用
YmfmWrapper ymfmWrapper;
ymfmWrapper.initialize(YmfmWrapper::ChipType::OPM, 44100);
ymfmWrapper.writeRegister(register_address, value);  // レジスタ書き込み
ymfmWrapper.generateSamples(leftBuffer, rightBuffer, numSamples);  // 音声生成
```

### 2.3 現在のコード構造
```
src/
├── PluginProcessor.h/cpp    // メイン処理、JUCE AudioProcessor実装
├── PluginEditor.h/cpp       // JUCE AudioProcessorEditor実装
├── dsp/
│   ├── YmfmWrapper.h/cpp    // ymfmライブラリのラッパークラス
│   ├── YM2151Registers.h    // YM2151レジスタ定義と定数
│   └── EnvelopeGenerator.h/cpp // エンベロープ生成器
├── core/
│   └── VoiceManager.h/cpp   // ボイス管理とMIDI処理
├── ui/
│   ├── MainComponent.h/cpp  // メインUI実装
│   ├── OperatorPanel.h/cpp  // オペレータUI
│   ├── RotaryKnob.h/cpp     // カスタムノブコンポーネント
│   ├── AlgorithmDisplay.h/cpp // アルゴリズム表示
│   └── EnvelopeDisplay.h/cpp  // エンベロープ表示
└── utils/
    ├── ParameterIDs.h       // パラメータID定義
    ├── Debug.h              // デバッグマクロ
    ├── PresetManager.h/cpp  // プリセット管理
    └── VOPMParser.h/cpp     // .opmファイル解析
```

## 3. 実装する機能の詳細

### 3.1 機能一覧と優先順位

1. **グローバルパン機能**（優先度：高、難易度：低）
   - 最終出力の定位をL/C/R/Randomから選択
   - 既存のチャンネルパン設定を上書き

2. **ユニゾン機能**（優先度：高、難易度：中）
   - 1つのノートに対して複数のYM2151インスタンスを使用
   - 各インスタンスに微小なピッチずれ（デチューン）を適用
   - ステレオ配置で音の広がりを演出

3. **拡張LFO**（優先度：中、難易度：中）
   - ソフトウェアで実装する追加LFO（3基）
   - ピッチ、音量、音色（オペレータTL）への適用

4. **マクロコントロール**（優先度：低、難易度：高）
   - エンベロープ型の時間変化
   - ステップシーケンサー

### 3.2 実装アプローチ

#### ユニゾン実装の核心
```
通常: 1ノート → 1 YM2151チャンネル
ユニゾン: 1ノート → 複数のYM2151インスタンス（各8ch持つ）

利点：
- 8ボイスポリフォニーを維持
- 各ユニゾンボイスを独立して制御可能
- CPU負荷はインスタンス数に比例（予測可能）
```

## 4. 実装手順

### 4.1 Phase 1: グローバルパン実装（推定作業時間：2-3時間）

#### ステップ1: パラメータ定義の追加

**ファイル: src/utils/ParameterIDs.h**

既存のGlobalパラメータ名前空間に以下を追加：
```cpp
namespace Global {
    // 既存のパラメータ...
    constexpr const char* GlobalPan = "global_pan";  // 新規追加
}

// パン位置の定義（新規追加）
enum class GlobalPanPosition {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2,
    RANDOM = 3
};
```

#### ステップ2: AudioProcessorValueTreeStateへの追加

**ファイル: src/PluginProcessor.cpp**

`createParameterLayout()`関数内に追加：
```cpp
layout.add(std::make_unique<juce::AudioParameterChoice>(
    ParamID::Global::GlobalPan,
    "Global Pan",
    juce::StringArray{"Left", "Center", "Right", "Random"},
    1  // デフォルト: Center
));
```

#### ステップ3: パン適用ロジック

**ファイル: src/PluginProcessor.cpp**

新しいメソッドを追加：
```cpp
void YMulatorSynthAudioProcessor::applyGlobalPan(int channel) {
    CS_ASSERT_CHANNEL(channel);
    
    // 現在のレジスタ値を読み取り（他のビットを保持）
    uint8_t currentReg = ymfmWrapper.readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
    uint8_t otherBits = currentReg & YM2151Regs::PRESERVE_ALG_FB;  // パン以外のビット
    
    // グローバルパン設定を取得
    auto panChoice = static_cast<GlobalPanPosition>(
        static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::GlobalPan)));
    uint8_t panBits;
    
    switch(panChoice) {
        case GlobalPanPosition::LEFT:   
            panBits = YM2151Regs::PAN_LEFT_ONLY;
            break;
        case GlobalPanPosition::CENTER: 
            panBits = YM2151Regs::PAN_CENTER;
            break;
        case GlobalPanPosition::RIGHT:  
            panBits = YM2151Regs::PAN_RIGHT_ONLY;
            break;
        case GlobalPanPosition::RANDOM:
            // ノートオン時にランダム決定
            int r = juce::Random::getSystemRandom().nextInt(3);
            panBits = (r == 0) ? YM2151Regs::PAN_LEFT_ONLY : 
                     (r == 1) ? YM2151Regs::PAN_RIGHT_ONLY : YM2151Regs::PAN_CENTER;
            break;
    }
    
    // YM2151に書き込み
    ymfmWrapper.writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, otherBits | panBits);
}
```

#### ステップ4: UI実装

**ファイル: src/ui/MainComponent.h**

ヘッダー部のコンポーネント定義に追加：
```cpp
private:
    // 既存のコンポーネント...
    std::unique_ptr<juce::ComboBox> globalPanComboBox;
    std::unique_ptr<juce::Label> globalPanLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> globalPanAttachment;
```

**ファイル: src/ui/MainComponent.cpp**

コンストラクタ内でUIを構築：
```cpp
// ALGORITHMドロップダウンの隣に配置
globalPanComboBox = std::make_unique<juce::ComboBox>();
globalPanComboBox->addItemList({"L", "C", "R", "Random"}, 1);
globalPanComboBox->setSelectedId(2);  // Center
globalPanLabel = std::make_unique<juce::Label>("globalPanLabel", "PAN");
globalPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
    audioProcessor.getParameters(), ParamID::Global::GlobalPan, *globalPanComboBox
);
addAndMakeVisible(globalPanComboBox.get());
addAndMakeVisible(globalPanLabel.get());
```

### 4.2 Phase 2: ユニゾン基本実装（推定作業時間：8-10時間）

#### ステップ1: UnisonEngineクラスの作成

**新規ファイル: src/dsp/UnisonEngine.h**
```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "YmfmWrapper.h"
#include <vector>
#include <memory>

class UnisonEngine {
public:
    UnisonEngine();
    ~UnisonEngine();
    
    // 初期化
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void reset();
    
    // ユニゾン設定
    void setVoiceCount(int count);      // 1-4
    void setDetune(float cents);        // 0-50 cents
    void setStereoSpread(float percent); // 0-100%
    void setStereoMode(int mode);       // Off/Auto/Wide/Narrow
    
    // 音声処理
    void processBlock(juce::AudioBuffer<float>& buffer, 
                     juce::MidiBuffer& midiMessages);
    
    // レジスタアクセス（全インスタンスに適用）
    void writeRegister(uint8_t reg, uint8_t value);
    void writeChannelRegister(int channel, uint8_t reg, uint8_t value);
    
    // 状態取得
    int getActiveVoiceCount() const { return activeVoices; }
    bool isUnisonEnabled() const { return activeVoices > 1; }

private:
    struct VoiceInstance {
        std::unique_ptr<ymfm::ym2151> chip;
        std::unique_ptr<ymfm::ym2151::output_data> output;
        float detuneRatio = 1.0f;
        uint8_t panValue = 0xC0;
        
        VoiceInstance() {
            chip = std::make_unique<ymfm::ym2151>();
            output = std::make_unique<ymfm::ym2151::output_data>();
            chip->reset();
        }
    };
    
    std::vector<VoiceInstance> instances;
    int activeVoices = 1;
    float detuneAmount = 0.0f;
    float stereoSpread = 80.0f;
    int stereoMode = 1; // Auto
    
    double currentSampleRate = 44100.0;
    
    // 内部ヘルパー
    void updateInstanceCount();
    void updateDetuneRatios();
    void updatePanPositions();
    float calculateDetuneRatio(int voiceIndex, int totalVoices);
    uint8_t calculatePanValue(int voiceIndex, int totalVoices);
    
    // 周波数変換
    std::pair<uint8_t, uint8_t> noteToKCKF(int note, float detuneRatio);
};
```

**新規ファイル: src/dsp/UnisonEngine.cpp**
```cpp
#include "UnisonEngine.h"
#include <cmath>

UnisonEngine::UnisonEngine() {
    // 初期状態で1インスタンスを作成
    instances.resize(1);
}

void UnisonEngine::setVoiceCount(int count) {
    if (count < 1 || count > 4 || count == activeVoices) return;
    
    activeVoices = count;
    updateInstanceCount();
    updateDetuneRatios();
    updatePanPositions();
}

void UnisonEngine::updateInstanceCount() {
    int currentSize = instances.size();
    
    if (activeVoices > currentSize) {
        // インスタンスを追加
        for (int i = currentSize; i < activeVoices; ++i) {
            instances.emplace_back();
        }
    } else if (activeVoices < currentSize) {
        // インスタンスを削減
        instances.resize(activeVoices);
    }
}

float UnisonEngine::calculateDetuneRatio(int voiceIndex, int totalVoices) {
    if (totalVoices == 1) return 1.0f;
    
    // 均等に分散（例: 2声なら -1, +1、3声なら -1, 0, +1）
    float position = (float)voiceIndex / (totalVoices - 1) * 2.0f - 1.0f;
    float cents = position * detuneAmount;
    
    // セントから周波数比に変換
    return std::pow(2.0f, cents / 1200.0f);
}

void UnisonEngine::processBlock(AudioBuffer<float>& buffer, 
                               MidiBuffer& midiMessages) {
    buffer.clear();
    
    // 各インスタンスを処理
    for (auto& instance : instances) {
        // MIDIメッセージを各インスタンスに適用
        for (const auto metadata : midiMessages) {
            // MIDIメッセージの処理（既存のコードを参考に）
        }
        
        // オーディオ生成
        instance.chip->generate(instance.output.get());
        
        // バッファに加算（ミックス）
        // TODO: instance.outputの内容をbufferに加算
    }
    
    // 音量調整（インスタンス数で除算）
    if (activeVoices > 1) {
        buffer.applyGain(1.0f / std::sqrt((float)activeVoices));
    }
}
```

#### ステップ2: PluginProcessorの変更

**ファイル: src/PluginProcessor.h**

変更前：
```cpp
private:
    YmfmWrapper ymfmWrapper;
```

変更後：
```cpp
private:
    // YmfmWrapper ymfmWrapper;  // ユニゾン対応時に置き換え
    UnisonEngine unisonEngine;  // 追加
```

**ファイル: src/PluginProcessor.cpp**

すべての`ymfmWrapper.writeRegister()`呼び出しを`unisonEngine.writeRegister()`に置き換え：
```cpp
// 変更前
ymfmWrapper.writeRegister(address, data);

// 変更後
unisonEngine.writeRegister(address, data);
```

### 4.3 Phase 3: 拡張LFO実装（推定作業時間：6-8時間）

#### ステップ1: ExtendedLFOクラスの作成

**新規ファイル: src/dsp/ExtendedLFO.h**
```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ExtendedLFO {
public:
    enum Target { 
        TARGET_PITCH,      // ピッチ（FNUM）
        TARGET_AMPLITUDE,  // 音量（TL）
        TARGET_TIMBRE     // 音色（TL/DT1/MUL選択可能）
    };
    
    enum Waveform { 
        WAVE_SINE, 
        WAVE_TRIANGLE, 
        WAVE_SAW, 
        WAVE_SQUARE, 
        WAVE_RANDOM 
    };
    
    ExtendedLFO();
    
    // 設定
    void setEnabled(bool enabled);
    void setTarget(Target target);
    void setRate(float hz);          // 0.1 - 20 Hz
    void setAmount(float percent);   // 0 - 100%
    void setWaveform(Waveform wave);
    void setDelay(float ms);        // 0 - 5000ms
    void setFadeIn(float ms);       // 0 - 5000ms
    void setKeySync(bool sync);
    void setTargetOperators(uint8_t opMask); // bit 0-3 for OP1-4
    
    // 処理
    void trigger();  // ノートオン時に呼ぶ
    void reset();
    void updatePhase(double deltaTime);
    float getCurrentValue() const { return currentValue; }
    
    // 状態取得
    bool isEnabled() const { return enabled; }
    Target getTarget() const { return target; }
    bool shouldApplyToOperator(int op) const {
        return (targetOperators & (1 << op)) != 0;
    }

private:
    bool enabled = false;
    Target target = TARGET_PITCH;
    float rate = 1.0f;
    float amount = 0.0f;
    Waveform waveform = WAVE_SINE;
    float delay = 0.0f;
    float fadeIn = 0.0f;
    bool keySync = true;
    uint8_t targetOperators = 0x0F;  // 全オペレータ
    
    float phase = 0.0f;
    float currentValue = 0.0f;
    double timeSinceTrigger = 0.0;
    
    float generateWaveform(float phase) const;
    float calculateEnvelope(double time) const;
};
```

**新規ファイル: src/dsp/ExtendedLFO.cpp**
```cpp
#include "ExtendedLFO.h"
#include <cmath>

float ExtendedLFO::generateWaveform(float phase) const {
    switch (waveform) {
        case WAVE_SINE:
            return std::sin(phase * 2.0f * M_PI);
            
        case WAVE_TRIANGLE:
            return 1.0f - 4.0f * std::abs(phase - 0.5f);
            
        case WAVE_SAW:
            return 2.0f * phase - 1.0f;
            
        case WAVE_SQUARE:
            return phase < 0.5f ? 1.0f : -1.0f;
            
        case WAVE_RANDOM:
            // シンプルなS&H
            return (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            
        default:
            return 0.0f;
    }
}

void ExtendedLFO::updatePhase(double deltaTime) {
    if (!enabled) return;
    
    timeSinceTrigger += deltaTime;
    
    // ディレイチェック
    if (timeSinceTrigger < delay * 0.001) {
        currentValue = 0.0f;
        return;
    }
    
    // フェーズ更新
    phase += rate * deltaTime;
    while (phase >= 1.0f) phase -= 1.0f;
    
    // 波形生成
    float rawValue = generateWaveform(phase);
    
    // エンベロープ適用
    float envelope = calculateEnvelope(timeSinceTrigger);
    
    // 最終値
    currentValue = rawValue * envelope * (amount / 100.0f);
}
```

#### ステップ2: LFOの統合

**ファイル: src/PluginProcessor.h**

追加：
```cpp
private:
    // LFO
    static constexpr int NUM_EXTENDED_LFOS = 3;
    ExtendedLFO extendedLFOs[NUM_EXTENDED_LFOS];
    
    // LFO適用
    void applyLFOModulation();
    void applyPitchModulation(int lfoIndex, float value);
    void applyAmplitudeModulation(int lfoIndex, float value);
    void applyTimbreModulation(int lfoIndex, float value);
    
    // キャリア判定
    bool isCarrierOperator(int algorithm, int op) const;
```

### 4.4 UI実装ガイドライン

#### タブUIの実装

**ファイル: src/ui/MainComponent.h**

追加：
```cpp
private:
    std::unique_ptr<TabbedComponent> tabbedComponent;
    
    // タブコンテンツ
    Component* createMainTab();
    Component* createExtendedTab();
    Component* createMacroTab();
```

**ファイル: src/ui/MainComponent.cpp**

コンストラクタを変更：
```cpp
MainComponent::MainComponent(
    YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    // タブコンポーネント作成
    tabbedComponent = std::make_unique<TabbedComponent>(
        TabbedButtonBar::TabsAtTop);
    
    // 各タブを追加
    tabbedComponent->addTab("Main", 
        getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
        createMainTab(), true);
    tabbedComponent->addTab("Extended", 
        getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
        createExtendedTab(), true);
    tabbedComponent->addTab("Macro", 
        getLookAndFeel().findColour(ResizableWindow::backgroundColourId),
        createMacroTab(), true);
    
    addAndMakeVisible(tabbedComponent.get());
    setSize(900, 600);  // 幅を広げる
}
```

## 5. パラメータ保存とプリセット管理

### 5.1 拡張パラメータの保存

**ファイル: src/PluginProcessor.cpp**

`getStateInformation()`と`setStateInformation()`を拡張：
```cpp
void YMulatorSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // 既存のパラメータ保存
    auto state = parameters.copyState();
    
    // 拡張パラメータを追加
    auto extendedParams = state.getOrCreateChildWithName(
        "ExtendedParams", nullptr);
    
    // ユニゾン設定
    extendedParams.setProperty("unisonVoices", 
        unisonEngine.getActiveVoiceCount(), nullptr);
    extendedParams.setProperty("unisonDetune", 
        *parameters.getRawParameterValue("unisonDetune"), nullptr);
    
    // LFO設定
    for (int i = 0; i < NUM_EXTENDED_LFOS; i++) {
        auto lfoState = extendedParams.getOrCreateChildWithName(
            "LFO" + String(i), nullptr);
        // LFOパラメータを保存
    }
    
    // XMLとして保存
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    juce::copyXmlToBinary(*xml, destData);
}
```

## 6. デバッグとトラブルシューティング

### 6.1 デバッグマクロ

```cpp
// src/utils/Debug.hにあるCS_*マクロを使用
#include "utils/Debug.h"

// 使用例：
CS_DBG("[Unison] Voice count: " + juce::String(voiceCount));
CS_DBG("[LFO] Rate: " + juce::String(rate) + " Amount: " + juce::String(amount));
CS_DBG("[REG] " + juce::String::toHexString(reg) + 
       " = " + juce::String::toHexString(val));

// アサーション例：
CS_ASSERT_CHANNEL(channel);
CS_ASSERT_OPERATOR(operatorNum);
CS_ASSERT_PARAMETER_RANGE(value, minVal, maxVal);
```

### 6.2 よくある問題と解決策

#### 問題1: ユニゾンで音が歪む
```cpp
// 解決策: 音量調整
void UnisonEngine::processBlock(...) {
    // ...
    // sqrt()を使用して知覚的な音量を調整
    buffer.applyGain(1.0f / std::sqrt((float)activeVoices));
}
```

#### 問題2: LFOが効かない
```cpp
// チェックポイント:
// 1. LFOが有効か
if (!extendedLFOs[i].isEnabled()) return;

// 2. 更新頻度は適切か（processBlockで毎回呼ばれているか）
// 3. レジスタ書き込みが正しいか
CS_DBG("Register write: " + juce::String::toHexString(targetReg) + 
       " = " + juce::String::toHexString(modulatedValue));
```

#### 問題3: パンが期待通りに動作しない
```cpp
// YM2151のパンレジスタは他のビットも含む
uint8_t currentValue = ymfmWrapper.readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
uint8_t preservedBits = currentValue & YM2151Regs::PRESERVE_ALG_FB;
ymfmWrapper.writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, preservedBits | newPanBits);
```

### 6.3 パフォーマンス最適化

```cpp
// レジスタキャッシュの実装例
class RegisterCache {
    uint8_t cache[256];
    bool dirty[256];
    
public:
    void write(uint8_t reg, uint8_t value) {
        if (cache[reg] != value) {
            cache[reg] = value;
            dirty[reg] = true;
        }
    }
    
    void flush(ymfm::ym2151& chip) {
        for (int i = 0; i < 256; i++) {
            if (dirty[i]) {
                chip.write(i, cache[i]);
                dirty[i] = false;
            }
        }
    }
};
```

## 7. テスト手順

### 7.1 単体テスト例

```cpp
// UnisonEngineのテスト
void testUnisonDetune() {
    UnisonEngine engine;
    engine.setVoiceCount(2);
    engine.setDetune(10.0f);  // 10 cents
    
    // 440Hz (A4) の場合
    // Voice 1: 437.46Hz (-10 cents)
    // Voice 2: 442.55Hz (+10 cents)
    
    // ノートナンバー69 (A4) でテスト
    auto [kc1, kf1] = engine.calculateKCKF(69, 0.9942f);  // -10 cents
    auto [kc2, kf2] = engine.calculateKCKF(69, 1.0058f);  // +10 cents
    
    // 期待値と比較
    EXPECT_NE(kf1, kf2);  // 異なるKF値
}
```

### 7.2 統合テスト

1. **基本動作確認**
   - プラグインがDAWで正常にロードされるか
   - 音が出るか
   - パラメータが保存/復元されるか

2. **ユニゾンテスト**
   - 2/3/4ボイスで正常に動作するか
   - デチューンが聴感上正しいか
   - CPU負荷は許容範囲か

3. **LFOテスト**
   - 各波形が正しく生成されるか
   - レートとアマウントが正しく適用されるか
   - ディレイ/フェードインが動作するか

## 8. リリース前チェックリスト

- [ ] すべての新機能が動作する
- [ ] 既存機能が壊れていない
- [ ] CPU使用率が許容範囲内（+50%以内）
- [ ] メモリリークがない
- [ ] プリセットの保存/読み込みが正常
- [ ] 主要DAWでの動作確認（Logic Pro, Ableton Live, Cubase）
- [ ] ドキュメントの更新

## 9. 参考資料

### YM2151レジスタマップ（重要部分）
```
0x08: Key On
0x20-0x27: RL, FB, CON (Pan, Feedback, Algorithm)
0x28-0x2F: KC (Key Code)
0x30-0x37: KF (Key Fraction)
0x40-0x5F: DT1, MUL
0x60-0x7F: TL (Total Level)
0x80-0x9F: KS, AR (Attack Rate)
0xA0-0xBF: AMS-EN, D1R (Decay Rate 1)
0xC0-0xDF: DT2, D2R (Decay Rate 2)
0xE0-0xFF: D1L, RR (Release Rate)
```

### 周波数計算式
```cpp
// MIDIノートから周波数
float freq = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);

// 周波数からKC/KF（YM2151Registers.hの定数を使用）
int octave = 0;
float fnote = note;
while (fnote >= YM2151Regs::NOTES_PER_OCTAVE) {
    fnote -= YM2151Regs::NOTES_PER_OCTAVE;
    octave++;
}
uint8_t kc = (octave << YM2151Regs::SHIFT_OCTAVE) | static_cast<int>(fnote);
uint8_t kf = static_cast<int>((fnote - static_cast<int>(fnote)) * YM2151Regs::KF_SCALE_FACTOR);
```

## 10. UI ワイヤフレーム

### 10.1 現在のメイン画面構成
```
┌─────────────────────────────────────────────────────────────────┐
│  [●] YMulator: Electric Piano                              [+]  │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  AL [Algorithm 4 ▼]  FB (6)  Bank [Factory ▼]  Preset [...]   │
│                                                          [Save] │
│ ┌─ LFO ────────────────────────────────────────────────────┐   │
│ │  (0)      (0)      (0)      [Saw ▼]     [ ] Noise  (0)   │   │
│ │  Rate     AMD      PMD       Wave       Enable     Freq   │   │
│ └───────────────────────────────────────────────────────────┘   │
│                                                                 │
│ ┌─ [✓] Operator 1 ─────────────────────────────────[ ] AMS─┐   │
│ │  ┌──────────────┐  (30) (22) (5) (5) (0) (3) (2) (3)     │   │
│ │  │   ENVELOPE   │   TL   AR  D1R D1L D2R  RR MUL DT1     │   │
│ │  │   ╱───╲___  │  (0) (0)                                │   │
│ │  │  ╱       ╲_ │  DT2  KS                                │   │
│ │  └──────────────┘                                         │   │
│ └───────────────────────────────────────────────────────────┘   │
│                                                                 │
│ ┌─ [✓] Operator 2 ─────────────────────────────────[ ] AMS─┐   │
│ │  ┌──────────────┐  (0) (16) (8) (2) (8) (7) (2) (3)      │   │
│ │  │   ENVELOPE   │   TL   AR  D1R D1L D2R  RR MUL DT1     │   │
│ │  │   ╱───╲___  │  (0) (1)                                │   │
│ │  │  ╱       ╲_ │  DT2  KS                                │   │
│ │  └──────────────┘                                         │   │
│ └───────────────────────────────────────────────────────────┘   │
│                                                                 │
│ [Operator 3 と 4 も同様の構成]                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 10.2 拡張後のメイン画面（Mainタブ）
```
┌─────────────────────────────────────────────────────────────────┐
│  [●] YMulator: Electric Piano                              [+]  │
├─────────────────────────────────────────────────────────────────┤
│  [Main] [Extended] [Macro]                                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  AL [Algorithm 4 ▼]  FB (6)  PAN [C ▼]  Bank [Factory ▼]     │
│                                         Preset [Electric Piano ▼]│
│                                                          [Save] │
│ ┌─ LFO ────────────────────────────────────────────────────┐   │
│ │  (0)      (0)      (0)      [Saw ▼]     [ ] Noise  (0)   │   │
│ │  Rate     AMD      PMD       Wave       Enable     Freq   │   │
│ └───────────────────────────────────────────────────────────┘   │
│                                                                 │
│ [以下、Operator 1-4 は現在と同じ構成]                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

変更点：
1. タブバーを追加（Main/Extended/Macro）
2. PANドロップダウンを追加（L/C/R/Random）
3. レイアウトを少し調整
```

### 10.3 Extendedタブ
```
┌─────────────────────────────────────────────────────────────────┐
│  [●] YMulator: Electric Piano                              [+]  │
├─────────────────────────────────────────────────────────────────┤
│  [Main] [Extended] [Macro]                                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ ┌─── Unison ────────────────────────────────────────────────┐ │
│ │                                                            │ │
│ │  Voices [2 ▼]     (15)        (80)      [Auto ▼]         │ │
│ │         1/2/3/4   Detune     Spread    Stereo Mode       │ │
│ │                   0-50¢      0-100%    Off/Auto/Wide/Nar  │ │
│ │                                                            │ │
│ │  Phase [Random ▼]   (±0)                                  │ │
│ │        Free/Random  Fine Tune                             │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ┌─── Extended LFO 1 (Pitch) ───────────────────────────────┐ │
│ │                                                            │ │
│ │  (4.5)      (30)       [Sin ▼]     (0)      (0)    [✓]   │ │
│ │  Rate      Amount      Wave       Delay   Fade   K.Sync  │ │
│ │  0.1-20Hz  0-100%                  ms      ms            │ │
│ │                                                            │ │
│ │  Target: [All Operators]  (固定)                          │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ┌─── Extended LFO 2 (Amplitude) ───────────────────────────┐ │
│ │                                                            │ │
│ │  (2.0)      (20)       [Tri ▼]    (100)    (500)   [✓]   │ │
│ │  Rate      Amount      Wave       Delay    Fade   K.Sync  │ │
│ │                                                            │ │
│ │  Target: [Auto-Carrier ▼]                                 │ │
│ │          Manual: [1 ] [2 ] [3 ] [4 ✓]                    │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ┌─── Extended LFO 3 (Timbre) ──────────────────────────────┐ │
│ │                                                            │ │
│ │  Target [Op TL ▼]  (0.5)   (40)    [Random ▼]            │ │
│ │                    Rate   Amount    Wave                  │ │
│ │                                                            │ │
│ │  (0)      (0)     [ ]      Operators: [1 ✓][2 ✓][3 ][4 ] │ │
│ │  Delay   Fade   K.Sync                                    │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 10.4 Macroタブ
```
┌─────────────────────────────────────────────────────────────────┐
│  [●] YMulator: Electric Piano                              [+]  │
├─────────────────────────────────────────────────────────────────┤
│  [Main] [Extended] [Macro]                                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│ ┌─── Macro Envelope ───────────────────────────────────────┐ │
│ │                                                            │ │
│ │  ┌──────────────────────────────────────────────────┐    │ │
│ │  │                                                  │    │ │
│ │  │         ╱───────╲                               │    │ │
│ │  │        ╱         ╲_____________                 │    │ │
│ │  │       ╱                        ╲___             │    │ │
│ │  │  ────┴───────────┴──────────────┴────          │    │ │
│ │  │     Attack      Hold          Decay             │    │ │
│ │  └──────────────────────────────────────────────────┘    │ │
│ │                                                            │ │
│ │  Target [TL ▼]    Operators: [1 ✓] [2 ✓] [3 ] [4 ]      │ │
│ │         TL/DT1/MUL/FB                                     │ │
│ │                                                            │ │
│ │  (500)        (200)       (1000)       [ ] Loop          │ │
│ │  Attack       Hold        Decay                           │ │
│ │  0-10000ms   0-10000ms   0-10000ms                       │ │
│ │                                                            │ │
│ │  (0)          (80)        (40)                           │ │
│ │  Start        Peak        End                             │ │
│ │  0-127        0-127       0-127                           │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
│ ┌─── Step Sequencer ───────────────────────────────────────┐ │
│ │                                                            │ │
│ │  Steps: [16 ▼]   Rate: [1/16 ▼]   Sync: [✓]   Play: [▶] │ │
│ │          8/16/32        1/32-1/1                          │ │
│ │                                                            │ │
│ │  ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐                   │ │
│ │  │█│▓│▒│░│█│▓│▒│░│█│▓│▒│░│█│▓│▒│░│ ← クリックで編集  │ │
│ │  └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘                   │ │
│ │   1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6                      │ │
│ │                                                            │ │
│ │  Target [DT1 ▼]   Range: [-7] to [+7]   Smooth: [✓]     │ │
│ │         DT1/DT2/TL/MUL                                    │ │
│ │                                                            │ │
│ │  Operators: [1 ✓] [2 ✓] [3 ✓] [4 ✓]                    │ │
│ └────────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 10.5 UIコンポーネントの詳細

#### ノブ（円形コントロール）
```
   (15)     ← 現在値
  ╱───╲    ← ノブ本体（ドラッグで操作）
 │  ●  │   ← 中心のインジケータ
  ╲___╱    
  Detune   ← ラベル
```

#### スライダー（ステップシーケンサー用）
```
┌─┐
│█│ ← 最大値
│▓│ ← 75%
│▒│ ← 50%
│░│ ← 25%
│ │ ← 最小値
└─┘
```

#### トグルボタン
```
[✓] 有効状態
[ ] 無効状態
```

#### ドロップダウン
```
[Algorithm 4 ▼]  ← クリックで選択肢表示
```

### 10.6 カラースキーム（参考）
```
背景色: #2D2D30 (ダークグレー)
パネル背景: #383838 (少し明るいグレー)
テキスト: #FFFFFF (白)
ノブリング: #00FFFF (シアン)
値表示: #00FF00 (グリーン)
非アクティブ: #666666 (グレー)
ボタンハイライト: #4080FF (ブルー)
```

### 10.7 レイアウトガイドライン

1. **余白とスペーシング**
   - コンポーネント間: 8px
   - パネル内余白: 12px
   - セクション間: 16px

2. **コンポーネントサイズ**
   - ノブ: 40x40px
   - ボタン: 高さ24px
   - ドロップダウン: 高さ24px
   - チェックボックス: 16x16px

3. **フォント**
   - ラベル: 11px
   - 値表示: 12px bold
   - セクションタイトル: 13px bold

## 11. 最後に

この実装ガイドは、YMulator-Synthの拡張機能を段階的に実装するための完全な指針です。各フェーズは独立して実装・テスト可能なので、一つずつ確実に進めることをお勧めします。

UIワイヤフレームを参考に、既存のデザインと調和する形で新機能を実装してください。質問や不明な点があれば、コード内のコメントやデバッグ出力を活用して、動作を理解しながら進めてください。

Happy Coding! 🎵