# ChipSynth AU 技術仕様書

## 1. 詳細設計

### 1.1 ymfm統合

#### 1.1.1 サポートチップ
```cpp
enum class ChipType {
    YM2151_OPM,    // X68000, アーケード基板
    YM2608_OPNA,   // PC-88VA, PC-98
    AY38910_SSG    // SSG部分
};
```

#### 1.1.2 ymfmインターフェース
```cpp
class YmfmInterface : public ymfm::ymfm_interface {
public:
    // タイマー管理
    virtual uint32_t get_timer_resolution() override;
    
    // 外部メモリアクセス（ADPCM用）
    virtual uint8_t external_read(ymfm::access_class type, uint32_t address) override;
    virtual void external_write(ymfm::access_class type, uint32_t address, uint8_t data) override;
    
private:
    std::vector<uint8_t> adpcm_rom;
};
```

### 1.2 音色パラメータ構造

#### 1.2.1 FM音色パラメータ
```cpp
struct FMVoiceParameters {
    // Algorithm & Feedback
    uint8_t algorithm;      // 0-7
    uint8_t feedback;       // 0-7
    
    // Per-operator parameters (x4)
    struct Operator {
        // Envelope Generator
        uint8_t attack_rate;    // 0-31
        uint8_t decay1_rate;    // 0-31
        uint8_t decay2_rate;    // 0-31
        uint8_t release_rate;   // 0-15
        uint8_t decay1_level;   // 0-15
        
        // Frequency
        uint8_t detune;         // 0-7
        uint8_t multiple;       // 0-15
        
        // Output
        uint8_t total_level;    // 0-127
        uint8_t key_scale;      // 0-3
        
        // LFO
        uint8_t ams_enable;     // 0-1
        uint8_t pms_depth;      // 0-7
    } operators[4];
    
    // Global parameters
    uint8_t lfo_frequency;      // 0-7
    uint8_t lfo_waveform;       // 0-3
    uint8_t panning;            // 0-3 (L, R, LR)
    
    // Noise generator (YM2151 only)
    uint8_t noise_enable;       // 0-1 (noise enable)
    uint8_t noise_frequency;    // 0-31 (noise frequency)
};
```

#### 1.2.2 SSGパラメータ
```cpp
struct SSGVoiceParameters {
    uint8_t tone_enable[3];     // トーン出力ON/OFF
    uint8_t noise_enable[3];    // ノイズ出力ON/OFF
    uint8_t envelope_shape;     // エンベロープ形状
    uint16_t envelope_period;   // エンベロープ周期
    uint8_t noise_period;       // ノイズ周期
};
```

### 1.3 プリセット管理

#### 1.3.1 プリセットフォーマット
```cpp
struct PresetVoice {
    std::string name;
    std::string category;
    std::string author;
    ChipType chip_type;
    
    union {
        FMVoiceParameters fm_params;
        SSGVoiceParameters ssg_params;
    };
    
    std::vector<uint8_t> custom_data;  // 拡張用
};
```

#### 1.3.2 プリセットソース
- **VOPM形式** (.opm): テキスト形式のOPM音色定義（プライマリサポート）
- **その他の音色フォーマット**: 将来的な拡張として検討
- **カスタムJSON形式**: プラグイン独自の拡張形式

### 1.4 ADPCM管理

#### 1.4.1 WAVファイル読み込み
```cpp
class ADPCMManager {
public:
    // WAVファイルをADPCM形式に変換
    bool loadWAVFile(const std::string& path);
    
    // ADPCMデータをチップに転送
    void uploadToChip(YmfmInterface* interface);
    
private:
    // WAV→ADPCM変換
    std::vector<uint8_t> convertToADPCM(const std::vector<int16_t>& pcm_data);
    
    // サンプリングレート変換
    std::vector<int16_t> resample(const std::vector<int16_t>& input, 
                                   int src_rate, int dst_rate);
};
```

### 1.5 MIDI実装仕様

#### 1.5.1 MIDI CCマッピング（VOPMex互換）
本プラグインは、VOPMexのexモードと互換性のあるCCマッピングを採用する。これにより、既存のVOPMユーザーがスムーズに移行できる。

```cpp
// MIDI CC定義（VOPMex準拠）
enum class VopmCC : uint8_t {
    // LFO関連
    LFO_FREQ_MSB = 1,      // CC 1: LFO周波数（上位）
    LFO_PMD = 2,           // CC 2: ピッチ変調深度
    LFO_AMD = 3,           // CC 3: 振幅変調深度
    LFO_WAVEFORM = 12,     // CC 12: LFO波形（0-3）
    LFO_FREQ_LSB = 33,     // CC 33: LFO周波数（下位）
    
    // アルゴリズム・フィードバック
    ALGORITHM = 14,        // CC 14: アルゴリズム（0-7）
    FEEDBACK = 15,         // CC 15: フィードバック（0-7）
    
    // オペレータ1-4 パラメータ（+0,+1,+2,+3でOP1-4）
    TL_OP1 = 16,          // CC 16-19: Total Level
    MUL_OP1 = 20,         // CC 20-23: Multiple
    DT1_OP1 = 24,         // CC 24-27: Detune1
    DT2_OP1 = 28,         // CC 28-31: Detune2
    KS_OP1 = 39,          // CC 39-42: Key Scale
    AR_OP1 = 43,          // CC 43-46: Attack Rate
    D1R_OP1 = 47,         // CC 47-50: Decay1 Rate
    D2R_OP1 = 51,         // CC 51-54: Decay2 Rate
    D1L_OP1 = 55,         // CC 55-58: Decay1 Level
    RR_OP1 = 59,          // CC 59-62: Release Rate
    AME_OP1 = 70,         // CC 70-73: AM Enable
    
    // LFO感度
    PMS = 75,             // CC 75: ピッチ変調感度（0-7）
    AMS = 76,             // CC 76: 振幅変調感度（0-3）
    
    // ノイズ
    NOISE_ENABLE = 80,    // CC 80: ノイズ有効（0-1）
    NOISE_FREQ = 82,      // CC 82: ノイズ周波数（0-31）
    
    // その他
    PITCH_BEND_RANGE = 81, // CC 81: ピッチベンド幅
    VELOCITY_SENS_OP1 = 87,// CC 87-90: ベロシティ感度
    OP_MASK = 93,         // CC 93: オペレータマスク
    
    // S98録音
    S98_LOOP_MARK = 118,  // CC 118: ループポイント設定
    S98_RECORD = 119,     // CC 119: 録音開始/停止
};
```

#### 1.5.2 パラメータ変換仕様
```cpp
class CCParameterConverter {
public:
    // VOPMexと同様に、一部パラメータは逆方向の値を採用
    static uint8_t convertTL(uint8_t ccValue) {
        // CC値0-127 → TL 127-0（逆方向）
        return 127 - ccValue;
    }
    
    static uint8_t convertEnvelope(uint8_t ccValue, uint8_t maxValue) {
        // エンベロープも逆方向（アナログシンセ風）
        return maxValue - (ccValue * maxValue / 127);
    }
    
    static uint8_t convertDirect(uint8_t ccValue, uint8_t maxValue) {
        // 直接マッピング
        return ccValue * maxValue / 127;
    }
};
```

#### 1.5.3 MIDI処理実装
```cpp
class ChipSynthAudioProcessor : public juce::AudioProcessor {
public:
    void handleMidiCC(int channel, int ccNumber, int value) {
        // オペレータパラメータの処理
        for (int op = 0; op < 4; ++op) {
            if (ccNumber == VopmCC::TL_BASE + op) {
                setOperatorTL(op, value);
            } else if (ccNumber == VopmCC::AR_BASE + op) {
                setOperatorAR(op, value);
            }
            // ... 他のパラメータも同様
        }
        
        // グローバルパラメータの処理
        switch (ccNumber) {
            case VopmCC::ALGORITHM:
                setAlgorithm(value);
                break;
            case VopmCC::FEEDBACK:
                setFeedback(value);
                break;
            // ... その他のパラメータ
        }
    }
};
```

#### 1.5.4 NRPN実装
```cpp
// VOPMex互換モード切り替え用NRPN
struct NRPNCommands {
    static constexpr uint8_t MODE_SWITCH_LSB = 0;
    static constexpr uint8_t MODE_SWITCH_MSB = 127;
    
    // 個別チャンネル設定
    static constexpr uint8_t CHANNEL_MODE_LSB = 0;
    static constexpr uint8_t CHANNEL_MODE_MSB = 127;
    
    // パラメータ入力モード切り替え
    static constexpr uint8_t PARAM_MODE_LSB = 127;
    static constexpr uint8_t PARAM_MODE_MSB = 126;
};
```

### 1.6 YM2151ノイズジェネレータ実装

#### 1.6.1 ハードウェア制約
```cpp
namespace YM2151NoiseConstraints {
    // YM2151 ハードウェア制約
    constexpr uint8_t NOISE_CHANNEL = 7;           // ノイズはチャンネル7でのみ動作
    constexpr uint8_t NOISE_OPERATOR = 3;          // ノイズはオペレータ4（インデックス3）でのみ生成
    constexpr uint8_t REG_NOISE_CONTROL = 0x0F;    // ノイズ制御レジスタ
    
    // ノイズ制御マスク
    constexpr uint8_t MASK_NOISE_ENABLE = 0x80;    // ビット7: ノイズ有効
    constexpr uint8_t MASK_NOISE_FREQUENCY = 0x1F; // ビット0-4: ノイズ周波数
    
    // 周波数範囲
    constexpr uint8_t NOISE_FREQUENCY_MIN = 0;     // 最高周波数（最も速い）
    constexpr uint8_t NOISE_FREQUENCY_MAX = 31;    // 最低周波数（最も遅い）
    constexpr uint8_t NOISE_FREQUENCY_DEFAULT = 16; // デフォルト周波数
}
```

#### 1.6.2 ノイズジェネレータ実装
```cpp
class YmfmWrapper {
public:
    // ノイズ制御API
    void setNoiseEnable(bool enable);
    void setNoiseFrequency(uint8_t frequency);  // 0-31
    void setNoiseParameters(bool enable, uint8_t frequency);
    
    bool getNoiseEnable() const;
    uint8_t getNoiseFrequency() const;
    
    // テスト用メソッド
    void testNoiseChannel();
    
private:
    void configureNoiseChannel();
};

// 実装例
void YmfmWrapper::setNoiseParameters(bool enable, uint8_t frequency) {
    CS_ASSERT_PARAMETER_RANGE(frequency, YM2151Regs::NOISE_FREQUENCY_MIN, YM2151Regs::NOISE_FREQUENCY_MAX);
    
    if (chipType != ChipType::OPM) {
        CS_DBG("Warning: Noise is only supported on OPM (YM2151) chip");
        return;
    }
    
    // レジスタ0x0Fに書き込み：ビット7=有効、ビット0-4=周波数
    uint8_t noiseValue = (enable ? YM2151Regs::MASK_NOISE_ENABLE : 0) | 
                         (frequency & YM2151Regs::MASK_NOISE_FREQUENCY);
    
    writeRegister(YM2151Regs::REG_NOISE_CONTROL, noiseValue);
}
```

#### 1.6.3 リズム音色プリセット対応
```cpp
// リズム系プリセットの設定例
struct RhythmPresetConfig {
    const char* name;
    uint8_t noiseFrequency;
    bool noiseEnable;
    uint8_t operatorConfig[4][10]; // TL, AR, D1R, D2R, RR, D1L, KS, MUL, DT1, DT2
};

const RhythmPresetConfig rhythmPresets[] = {
    {"Kick Drum",    8, true, {{31,31,0,15,0,0,2,0,0,0}, {31,31,20,15,0,50,3,1,0,0}, {0,0,0,0,0,127,0,1,0,0}, {0,0,0,0,0,127,0,1,0,0}}},
    {"Snare Drum",   20, true, {{31,31,0,15,0,0,3,15,0,0}, {31,31,31,15,0,40,3,1,0,0}, {0,0,0,0,0,127,0,1,0,0}, {0,0,0,0,0,127,0,1,0,0}}},
    {"Hi-Hat",       25, true, {{31,31,0,15,0,0,3,15,0,0}, {31,31,31,15,0,35,3,15,0,0}, {0,0,0,0,0,127,0,1,0,0}, {0,0,0,0,0,127,0,1,0,0}}},
    {"Crash Cymbal", 15, true, {{31,31,31,10,0,0,3,15,0,0}, {31,20,20,5,15,20,3,15,0,0}, {0,0,0,0,0,127,0,1,0,0}, {0,0,0,0,0,127,0,1,0,0}}}
};
```

#### 1.6.4 アルゴリズムとノイズの関係
```cpp
// YM2151ノイズはオペレータ4の正弦波出力をノイズに置き換える
// アルゴリズムに関係なく、オペレータ4が最終出力に寄与する場合にノイズが聞こえる

// オペレータ4が出力に寄与するアルゴリズム：
// - アルゴリズム 0, 1, 2, 3, 4, 5, 6, 7 (全アルゴリズム)
// オペレータ4が出力に寄与しないアルゴリズム：
// - なし（YM2151では全アルゴリズムでオペレータ4が出力される）

// ノイズを確実に聞かせるための設定例（テスト用）
void setupNoiseChannelForTesting() {
    const uint8_t noiseChannel = 7;  // 必須：チャンネル7
    
    // アルゴリズム7を使用（全オペレータ並列で最も分かりやすい）
    // 注意：他のアルゴリズムでもノイズは動作する
    uint8_t algorithmValue = 0x07;
    writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + noiseChannel, 
                  algorithmValue | YM2151Regs::PAN_CENTER);
    
    // オペレータ1-3を無音に設定（ノイズを際立たせるため）
    for (int op = 0; op < 3; op++) {
        int baseAddr = op * 8 + noiseChannel;
        writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + baseAddr, 127); // 最大減衰
    }
    
    // オペレータ4（ノイズ用）を設定
    int op4BaseAddr = 3 * 8 + noiseChannel;
    writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + op4BaseAddr, 32); // 適度な音量
    // その他のオペレータ4パラメータ設定...
}

// 既存の音色にノイズを追加する場合の例
void addNoiseToExistingVoice(uint8_t algorithm) {
    const uint8_t noiseChannel = 7;  // 必須：チャンネル7
    
    // 既存のアルゴリズムをそのまま使用可能
    writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + noiseChannel, 
                  algorithm | YM2151Regs::PAN_CENTER);
    
    // オペレータ4の設定により、正弦波＋ノイズのミックスが可能
    // オペレータ1-3は既存の音色設定を維持
}
```

### 1.6 ポリフォニック実装仕様

#### 1.6.1 VoiceManager設計

```cpp
class VoiceManager {
public:
    static constexpr int MAX_VOICES = 8;  // YM2151の物理チャンネル数
    
    // ボイス割り当て戦略
    enum class StealingPolicy {
        OLDEST,     // 最も古い音を停止（デフォルト）
        QUIETEST,   // 最も小さい音を停止
        LOWEST      // 最も低い音を停止
    };
    
    // チャンネル状態管理
    struct Voice {
        bool active = false;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint64_t timestamp = 0;  // ボイス年齢追跡用
    };
    
    int allocateVoice(uint8_t note, uint8_t velocity);
    void releaseVoice(uint8_t note);
    int getChannelForNote(uint8_t note) const;
};
```

#### 1.6.2 パラメータ同期の実装

**問題**: UIパラメータ変更時、全チャンネルへの反映が必要
**解決**: パラメータ更新ループで全8チャンネルを更新

```cpp
void updateYmfmParameters() {
    // 全チャンネルのパラメータを同期
    for (int channel = 0; channel < 8; ++channel) {
        // グローバルパラメータ
        ymfmWrapper.setAlgorithm(channel, algorithm);
        ymfmWrapper.setFeedback(channel, feedback);
        
        // オペレータパラメータ
        for (int op = 0; op < 4; ++op) {
            ymfmWrapper.setOperatorParameters(channel, op, 
                tl, ar, d1r, d2r, rr, d1l, ks, mul, dt1);
        }
    }
}
```

#### 1.6.3 MIDI処理フロー

```
MIDI Note On → VoiceManager::allocateVoice() → チャンネル決定
    ↓
ymfmWrapper.noteOn(channel, note, velocity)
    ↓
レジスタ書き込み（KC, KF, Key On）

MIDI Note Off → VoiceManager::getChannelForNote() → チャンネル特定
    ↓
ymfmWrapper.noteOff(channel, note)
    ↓
レジスタ書き込み（Key Off） → VoiceManager::releaseVoice()
```

### 1.7 S98録音機能

#### 1.6.1 S98フォーマット仕様
```cpp
struct S98Header {
    char magic[3];          // "S98"
    char version;           // '3'
    uint32_t timer_info;    // タイマー情報
    uint32_t timer_info2;   // タイマー情報2
    uint32_t compressing;   // 圧縮フラグ
    uint32_t tag_offset;    // タグオフセット
    uint32_t dump_offset;   // データオフセット
    uint32_t loop_offset;   // ループオフセット
    uint32_t device_count;  // デバイス数
};
```

#### 1.6.2 録音エンジン
```cpp
class S98Recorder {
public:
    void startRecording();
    void stopRecording();
    
    // レジスタ書き込みを記録
    void logRegisterWrite(uint8_t chip_id, uint16_t address, uint8_t data);
    
    // 同期待機を記録
    void logSync(uint32_t samples);
    
    // S98ファイルとして出力
    bool exportToFile(const std::string& path);
    
private:
    std::vector<uint8_t> command_buffer;
    bool is_recording;
    uint64_t sample_counter;
};
```

## 2. UI設計

### 2.1 JUCE実装概要

#### 2.1.1 基本構成
```cpp
class ChipSynthPluginEditor : public juce::AudioProcessorEditor {
public:
    ChipSynthPluginEditor(ChipSynthAudioProcessor& processor);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    // UIコンポーネント
    PresetBrowserComponent presetBrowser;
    VoiceEditorComponent voiceEditor;
    ADPCMManagerComponent adpcmManager;
    S98RecorderComponent s98Recorder;
    
    // カスタムLook&Feel
    VopmLookAndFeel lookAndFeel;
};
```

#### 2.1.2 カスタムLook&Feel
```cpp
class VopmLookAndFeel : public juce::LookAndFeel_V4 {
public:
    // VOPMライクな外観を再現
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
    
    // 3Dエフェクト付きボタン
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;
};
```

### 2.2 主要コンポーネント設計

#### 2.2.1 オペレータエディタ
```cpp
class OperatorComponent : public juce::Component,
                          public juce::Slider::Listener {
public:
    OperatorComponent(int operatorNumber);
    
private:
    // エンベロープパラメータ
    juce::Slider arSlider{"AR"};   // Attack Rate
    juce::Slider d1rSlider{"D1R"}; // Decay1 Rate
    juce::Slider d2rSlider{"D2R"}; // Decay2 Rate
    juce::Slider rrSlider{"RR"};   // Release Rate
    juce::Slider d1lSlider{"D1L"}; // Decay1 Level
    juce::Slider tlSlider{"TL"};   // Total Level
    
    // 周波数パラメータ
    juce::Slider mulSlider{"MUL"}; // Multiple
    juce::Slider dt1Slider{"DT1"}; // Detune1
    juce::Slider dt2Slider{"DT2"}; // Detune2
    juce::Slider ksSlider{"KS"};   // Key Scale
    
    // モジュレーション
    juce::ToggleButton ameButton{"AM"};
    
    // カスタム描画エリア
    EnvelopeDisplay envelopeDisplay;
};
```

#### 2.2.2 アルゴリズム表示
```cpp
class AlgorithmDisplay : public juce::Component {
public:
    void setAlgorithm(int algorithm);
    void paint(juce::Graphics& g) override;
    
private:
    void drawOperatorConnection(juce::Graphics& g, int algorithm);
    int currentAlgorithm = 0;
};
```

#### 2.2.3 エンベロープ表示
```cpp
class EnvelopeDisplay : public juce::Component,
                        public juce::Timer {
public:
    void updateEnvelope(const EnvelopeParameters& params);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    
    // ドラッグによる編集機能
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
private:
    juce::Path envelopePath;
    std::vector<juce::Point<float>> controlPoints;
};
```

### 2.3 MIDI CCマッピング実装

#### 2.3.1 VOPMex互換CCマッピング
```cpp
// VOPMex準拠のCCマッピング定義
namespace VopmCC {
    // オペレータパラメータ (OP1-4)
    constexpr int TL_BASE = 16;      // CC 16-19: Total Level
    constexpr int AR_BASE = 43;      // CC 43-46: Attack Rate
    constexpr int D1R_BASE = 47;     // CC 47-50: Decay1 Rate
    constexpr int D1L_BASE = 55;     // CC 55-58: Decay1 Level
    constexpr int D2R_BASE = 51;     // CC 51-54: Decay2 Rate
    constexpr int RR_BASE = 59;      // CC 59-62: Release Rate
    constexpr int KS_BASE = 39;      // CC 39-42: Key Scale
    constexpr int MUL_BASE = 20;     // CC 20-23: Multiple
    constexpr int DT1_BASE = 24;     // CC 24-27: Detune1
    constexpr int DT2_BASE = 28;     // CC 28-31: Detune2
    constexpr int AME_BASE = 70;     // CC 70-73: AM Enable
    
    // グローバルパラメータ
    constexpr int ALGORITHM = 14;    // CC 14: Connection (Algorithm)
    constexpr int FEEDBACK = 15;     // CC 15: Feedback Level
    constexpr int LFO_WAVE = 12;     // CC 12: LFO Waveform
    constexpr int LFO_FREQ_MSB = 1;  // CC 1: LFO Frequency MSB
    constexpr int LFO_FREQ_LSB = 33; // CC 33: LFO Frequency LSB
    constexpr int PMS = 75;          // CC 75: Pitch Mod Sensitivity
    constexpr int AMS = 76;          // CC 76: Amplitude Mod Sensitivity
    constexpr int PMD = 2;           // CC 2: Pitch Mod Depth
    constexpr int AMD = 3;           // CC 3: Amplitude Mod Depth
    
    // ノイズ
    constexpr int NOISE_ENABLE = 80; // CC 80: Noise Enable
    constexpr int NOISE_FREQ = 82;   // CC 82: Noise Frequency
}
```

### 2.4 メインウィンドウレイアウト（JUCE実装）

#### 2.4.1 レイアウト構成
```cpp
void ChipSynthPluginEditor::resized() {
    auto bounds = getLocalBounds();
    
    // 左側パネル（プリセット・ADPCM）
    auto leftPanel = bounds.removeFromLeft(200);
    presetBrowser.setBounds(leftPanel.removeFromTop(300));
    adpcmManager.setBounds(leftPanel);
    
    // 右側メインエリア
    auto mainArea = bounds;
    
    // 音色エディタ
    auto editorArea = mainArea.removeFromTop(400);
    voiceEditor.setBounds(editorArea);
    
    // S98レコーダー
    s98Recorder.setBounds(mainArea);
}
```

#### 2.4.2 プリセットブラウザ
```cpp
class PresetBrowserComponent : public juce::Component {
private:
    juce::TreeView presetTree;
    
    // カテゴリ別表示
    // - Factory/VOPM
    // - Factory/Other Formats（将来拡張）
    // - User Presets
};
```

### 2.5 キーボードショートカット実装
```cpp
class ChipSynthPluginEditor : public juce::KeyListener {
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override {
        if (key.isKeyCode(juce::KeyPress::spaceKey)) {
            // プレビュー音の再生/停止
            togglePreview();
            return true;
        }
        
        if (key.getModifiers().isCommandDown()) {
            switch (key.getKeyCode()) {
                case 'S': savePreset(); return true;
                case 'L': loadPreset(); return true;
                case 'R': toggleS98Recording(); return true;
            }
        }
        return false;
    }
};
```

## 3. テスト計画

### 3.1 単体テスト
- ymfmラッパークラス
- 音色パラメータ変換
- S98エンコーダ

### 3.2 統合テスト
- DAWとの互換性（Logic Pro, Ableton Live, etc.）
- CPU使用率とレイテンシ
- プリセット互換性

### 3.3 ユーザビリティテスト
- 音色エディタの操作性
- プリセットブラウジング
- 録音ワークフロー