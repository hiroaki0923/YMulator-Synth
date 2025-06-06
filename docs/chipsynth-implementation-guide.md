# ChipSynth AU 実装ガイド

## 1. 実装ガイド

### 1.1 クイックスタートガイド

#### 1.1.1 ymfm基本実装
```cpp
#include "ymfm_opm.h"
#include "ymfm_opna.h"

// 最小限のOPM実装例
class BasicOPMSynth {
private:
    ymfm::ym2151 opm_chip;
    ymfm::ym2151::output_data opm_output;
    uint32_t sample_rate = 44100;
    
public:
    void initialize() {
        // チップをリセット
        opm_chip.reset();
        
        // 簡単なピアノ音色を設定（チャンネル0）
        setupPianoVoice();
        
        // テスト音を鳴らす
        playNote(60, 100); // Middle C, velocity 100
    }
    
    void setupPianoVoice() {
        // Algorithm 4, Feedback 5
        writeOPMRegister(0x20, 0x04 | (5 << 3)); 
        
        // Operator 1 (Modulator)
        writeOPMRegister(0x40, 0x23); // DT1=2, MUL=3
        writeOPMRegister(0x60, 0x7F); // TL=127 (min volume)
        writeOPMRegister(0x80, 0x1F); // KS=0, AR=31
        writeOPMRegister(0xA0, 0x00); // AMS=0, D1R=0
        writeOPMRegister(0xC0, 0x0F); // DT2=0, D2R=15
        writeOPMRegister(0xE0, 0x0F); // D1L=0, RR=15
        
        // Operator 2 (Carrier) 
        writeOPMRegister(0x48, 0x21); // DT1=2, MUL=1
        writeOPMRegister(0x68, 0x33); // TL=51 (output level)
        writeOPMRegister(0x88, 0x1F); // KS=0, AR=31
        writeOPMRegister(0xA8, 0x00); // AMS=0, D1R=0
        writeOPMRegister(0xC8, 0x00); // DT2=0, D2R=0
        writeOPMRegister(0xE8, 0x0F); // D1L=0, RR=15
        
        // 他のオペレータも同様に設定...
    }
    
    void writeOPMRegister(uint8_t address, uint8_t data) {
        opm_chip.write_address(address);
        opm_chip.write_data(data);
    }
    
    void playNote(uint8_t note, uint8_t velocity) {
        // ノート番号からOPM用の周波数データを計算
        uint16_t fnum = noteToFnum(note);
        uint8_t octave = note / 12 - 1;
        
        // KC (Key Code) レジスタに書き込み
        writeOPMRegister(0x28, (octave << 4) | ((fnum >> 8) & 0x07));
        
        // KF (Key Fraction) レジスタに書き込み
        writeOPMRegister(0x30, (fnum & 0xF8));
        
        // ベロシティをTLに反映
        uint8_t tl = 127 - velocity;
        writeOPMRegister(0x68, tl); // Operator 2 (Carrier) TL
        
        // Key On
        writeOPMRegister(0x08, 0x78); // CH0, All operators on
    }
    
    void generateAudio(float* outputBuffer, int numSamples) {
        for (int i = 0; i < numSamples; i++) {
            // ymfmのクロックを進める（4MHz想定）
            opm_chip.generate(&opm_output, 1);
            
            // ステレオ出力を正規化
            outputBuffer[i * 2]     = opm_output.data[0] / 32768.0f;
            outputBuffer[i * 2 + 1] = opm_output.data[1] / 32768.0f;
        }
    }
    
private:
    uint16_t noteToFnum(uint8_t note) {
        // MIDI note to OPM frequency number conversion
        static const uint16_t fnum_table[12] = {
            0x269, 0x28E, 0x2B5, 0x2DE, 0x30A, 0x338,
            0x369, 0x39D, 0x3D4, 0x40E, 0x44C, 0x48E
        };
        return fnum_table[note % 12];
    }
};
```

#### 1.1.2 JUCEプラグインへの統合
```cpp
class ChipSynthAudioProcessor : public juce::AudioProcessor {
private:
    BasicOPMSynth opmSynth;
    juce::MidiKeyboardState keyboardState;
    
public:
    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        opmSynth.initialize();
    }
    
    void processBlock(juce::AudioBuffer<float>& buffer, 
                      juce::MidiBuffer& midiMessages) override {
        // MIDIメッセージの処理
        for (const auto metadata : midiMessages) {
            auto message = metadata.getMessage();
            
            if (message.isNoteOn()) {
                opmSynth.playNote(message.getNoteNumber(), 
                                 message.getVelocity());
            } else if (message.isNoteOff()) {
                // Note off処理
                opmSynth.writeOPMRegister(0x08, 0x00); // Key off
            }
        }
        
        // オーディオ生成
        opmSynth.generateAudio(buffer.getWritePointer(0), 
                              buffer.getNumSamples());
    }
};
```

### 1.2 レジスタマップリファレンス

#### 1.2.1 YM2151 (OPM) レジスタマップ

##### グローバルレジスタ
| アドレス | 機能 | ビット構成 | 説明 |
|---------|------|-----------|------|
| 0x01 | Test | - | テストモード（通常0） |
| 0x08 | Key On/Off | `CCCC-MMMM` | C=チャンネル(0-7), M=オペレータマスク |
| 0x0F | Noise | `E---NNNN` | E=有効, N=周波数(0-31) |
| 0x14 | Timer/IRQ | - | タイマー制御 |
| 0x18 | LFO Freq | `FFFFFFFF` | LFO周波数(0-255) |
| 0x19 | LFO PMD/AMD | `P-AAAAAA` | P=PMD(0-127), A=AMD(0-127) |
| 0x1B | LFO Wave | `------WW` | 波形(0=鋸,1=矩形,2=三角,3=ノイズ) |

##### チャンネル別レジスタ (CH = 0-7)
| アドレス | 機能 | ビット構成 | 説明 |
|---------|------|-----------|------|
| 0x20+CH | RL/FB/CON | `RL-FFFCCC` | R=右,L=左,F=FB(0-7),C=CON(0-7) |
| 0x28+CH | KC | `-OOOKKKK` | O=オクターブ(0-7),K=キーコード |
| 0x30+CH | KF | `FFFFFF--` | キーフラクション(0-63) |
| 0x38+CH | PMS/AMS | `-PPPP-AA` | P=PMS(0-7),A=AMS(0-3) |

##### オペレータ別レジスタ (OP = 0-3, CH = 0-7)
| アドレス計算 | 機能 | ビット構成 | 説明 |
|-------------|------|-----------|------|
| 0x40+OP*8+CH | DT1/MUL | `-DDDMMMM` | D=DT1(0-7),M=MUL(0-15) |
| 0x60+OP*8+CH | TL | `-TTTTTTT` | Total Level(0-127) |
| 0x80+OP*8+CH | KS/AR | `KK-RRRRR` | K=KS(0-3),R=AR(0-31) |
| 0xA0+OP*8+CH | AMS-EN/D1R | `A--RRRRR` | A=AMS有効,R=D1R(0-31) |
| 0xC0+OP*8+CH | DT2/D2R | `DD-RRRRR` | D=DT2(0-3),R=D2R(0-31) |
| 0xE0+OP*8+CH | D1L/RR | `LLLL-RRRR` | L=D1L(0-15),R=RR(0-15) |

#### 1.2.2 YM2608 (OPNA) レジスタマップ

##### FMパート（ポート0）
| アドレス | 機能 | 説明 |
|---------|------|------|
| 0x22 | LFO | LFO周波数制御 |
| 0x27 | Timer | タイマー制御 |
| 0x28 | Key On/Off | FM Key On/Off |
| 0x2D-2F | Prescaler | プリスケーラ設定 |

##### SSGパート（ポート0）
| アドレス | 機能 | 説明 |
|---------|------|------|
| 0x00-0x01 | Ch A Freq | チャンネルA周波数 |
| 0x02-0x03 | Ch B Freq | チャンネルB周波数 |
| 0x04-0x05 | Ch C Freq | チャンネルC周波数 |
| 0x06 | Noise Freq | ノイズ周波数 |
| 0x07 | Enable | トーン/ノイズ有効 |
| 0x08-0x0A | Volume | 各チャンネル音量 |
| 0x0B-0x0D | Envelope | エンベロープ設定 |

#### 1.2.3 実装例：音色パラメータの設定
```cpp
// VOPMパラメータをOPMレジスタに変換
void setOperatorParams(uint8_t ch, uint8_t op, const OperatorParams& params) {
    uint8_t base = op * 8 + ch;
    
    // DT1/MUL
    writeOPMRegister(0x40 + base, (params.dt1 << 4) | params.mul);
    
    // TL (Total Level)
    writeOPMRegister(0x60 + base, params.tl);
    
    // KS/AR
    writeOPMRegister(0x80 + base, (params.ks << 6) | params.ar);
    
    // AMS-EN/D1R  
    writeOPMRegister(0xA0 + base, (params.ams_en << 7) | params.d1r);
    
    // DT2/D2R
    writeOPMRegister(0xC0 + base, (params.dt2 << 6) | params.d2r);
    
    // D1L/RR
    writeOPMRegister(0xE0 + base, (params.d1l << 4) | params.rr);
}
```

### 1.3 トラブルシューティング

#### よくある問題と解決方法
1. **音が出ない**
   - Key Onレジスタ（0x08）の確認
   - TL値が127（最小音量）になっていないか確認
   - アルゴリズムとオペレータ接続の確認

2. **音程が狂う**
   - KC/KF値の計算確認
   - クロックレートの設定確認（OPM: 3.58MHz）

3. **音が歪む**
   - 出力のクリッピング確認
   - フィードバック値が大きすぎないか確認

### 1.4 開発環境セットアップ（VSCode + CMake）

#### 1.4.1 必要なツールとバージョン
```bash
# macOS開発環境
- Xcode Command Line Tools (最新版)
- CMake 3.22以降
- VSCode
- Python 3.8以降（JUCEビルドスクリプト用）

# VSCode拡張機能
- C/C++ (Microsoft)
- CMake Tools (Microsoft)
- CMake Language Support
- C++ TestMate（テスト実行用）
- CodeLLDB（デバッグ用）
```

#### 1.4.2 プロジェクト構造
```
ChipSynth-AU/
├── CMakeLists.txt          # ルートCMake設定
├── CLAUDE.md               # Claude Code開発ガイド
├── .gitignore             # Git除外設定
├── .vscode/               # VSCode設定
│   ├── settings.json      # エディタ設定
│   ├── tasks.json         # ビルドタスク
│   └── extensions.json    # 推奨拡張機能
├── cmake/                 # CMakeモジュール
│   ├── CMakeLists.txt
│   ├── JUCEConfig.cmake   # JUCE設定
│   └── CompilerWarnings.cmake
├── src/                   # ソースコード
│   ├── PluginProcessor.h/.cpp  # メインプロセッサ
│   ├── PluginEditor.h/.cpp     # UI編集画面
│   ├── core/              # コア機能
│   │   ├── FMCore.h/.cpp
│   │   └── VoiceManager.h/.cpp
│   ├── dsp/               # デジタル信号処理
│   │   ├── YmfmWrapper.h/.cpp
│   │   └── EnvelopeGenerator.h/.cpp
│   ├── ui/                # ユーザーインターフェース
│   │   ├── MainComponent.h/.cpp
│   │   └── OperatorPanel.h/.cpp
│   └── utils/             # ユーティリティ
│       └── PresetManager.h/.cpp
├── third_party/           # 外部ライブラリ
│   ├── CMakeLists.txt
│   └── ymfm/              # ymfmサブモジュール
├── tests/                 # テストコード
│   ├── CMakeLists.txt
│   └── test_main.cpp
├── docs/                  # ドキュメント
│   ├── chipsynth-adr.md
│   ├── chipsynth-design-main.md
│   ├── chipsynth-implementation-guide.md
│   └── chipsynth-technical-spec.md
├── include/               # ヘッダーファイル（予約）
├── resources/             # リソースファイル（予約）
└── build/                 # ビルド出力（.gitignoreで除外）
    └── _deps/             # CMake依存関係（JUCE等）
        └── juce-src/      # JUCEソースコード（自動ダウンロード）
```

**重要な変更点:**
- **JUCE**: CMakeのFetchContentで自動ダウンロード（`build/_deps/juce-src/`）
- **ymfm**: Gitサブモジュールとして管理（`third_party/ymfm/`）
- **CMake構造**: モジュール化された設定ファイル
- **ソースファイル**: 機能別ディレクトリ構造
- **VSCode設定**: CMake Tools対応

#### 1.4.3 CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.22)
project(ChipSynthAU VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCEの追加
add_subdirectory(external/JUCE)

# ymfmの追加
add_library(ymfm STATIC
    external/ymfm/src/ymfm_opm.cpp
    external/ymfm/src/ymfm_opna.cpp
    external/ymfm/src/ymfm_ssg.cpp
)
target_include_directories(ymfm PUBLIC external/ymfm/src)

# Audio Unitプラグイン定義
juce_add_plugin(ChipSynthAU
    COMPANY_NAME "hiroaki0923"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Hiro
    PLUGIN_CODE Chip
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "ChipSynth AU"
)

# ソースファイルの追加
target_sources(ChipSynthAU PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ymfm/YmfmWrapper.cpp
    src/dsp/VoiceAllocator.cpp
)

# 依存関係の設定
target_link_libraries(ChipSynthAU
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        ymfm
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# テストの設定（後述）
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

#### 1.4.4 VSCode設定

**.vscode/settings.json**
```json
{
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.generator": "Xcode",
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    },
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "{ BasedOnStyle: LLVM, IndentWidth: 4 }"
}
```

**.vscode/launch.json**
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug AU in Logic Pro",
            "type": "lldb",
            "request": "launch",
            "program": "/Applications/Logic Pro X.app/Contents/MacOS/Logic Pro X",
            "args": [],
            "cwd": "${workspaceFolder}",
            "preLaunchTask": "Build AU"
        },
        {
            "name": "Debug Standalone",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/ChipSynthAU_artefacts/Debug/Standalone/ChipSynth AU.app/Contents/MacOS/ChipSynth AU",
            "args": [],
            "cwd": "${workspaceFolder}",
            "preLaunchTask": "Build Standalone"
        }
    ]
}
```

### 1.5 テスト戦略とテストコード

#### 1.5.1 テストフレームワーク
```cmake
# tests/CMakeLists.txt
include(FetchContent)

# Google Testの取得
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# JUCEテストランナー
juce_add_console_app(ChipSynthTests)

target_sources(ChipSynthTests PRIVATE
    unit/YmfmWrapperTest.cpp
    unit/VoiceAllocatorTest.cpp
    unit/ParameterConversionTest.cpp
    integration/MidiProcessingTest.cpp
    integration/S98RecorderTest.cpp
    performance/LatencyTest.cpp
)

target_link_libraries(ChipSynthTests
    PRIVATE
        ChipSynthAU
        gtest_main
)
```

#### 1.5.2 単体テストの例

**tests/unit/YmfmWrapperTest.cpp**
```cpp
#include <gtest/gtest.h>
#include "ymfm/YmfmWrapper.h"

class YmfmWrapperTest : public ::testing::Test {
protected:
    YmfmWrapper wrapper;
    std::vector<float> outputBuffer;
    
    void SetUp() override {
        wrapper.initialize(44100);
        outputBuffer.resize(512);
    }
};

TEST_F(YmfmWrapperTest, InitializationTest) {
    // 初期化後は無音であることを確認
    wrapper.generateSamples(outputBuffer.data(), outputBuffer.size() / 2);
    
    for (float sample : outputBuffer) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST_F(YmfmWrapperTest, NoteOnGeneratesSound) {
    // ノートオンで音が生成されることを確認
    wrapper.noteOn(60, 100);
    wrapper.generateSamples(outputBuffer.data(), outputBuffer.size() / 2);
    
    // 少なくとも一つのサンプルが非ゼロ
    bool hasSound = false;
    for (float sample : outputBuffer) {
        if (std::abs(sample) > 0.0001f) {
            hasSound = true;
            break;
        }
    }
    EXPECT_TRUE(hasSound);
}

TEST_F(YmfmWrapperTest, RegisterWriteTest) {
    // レジスタ書き込みが正しく反映されるか
    wrapper.writeRegister(0x20, 0x07); // Algorithm 7
    
    // 音色が変わることを確認（実装依存）
    // ここでは内部状態の確認や音の違いをテスト
}
```

**tests/unit/ParameterConversionTest.cpp**
```cpp
#include <gtest/gtest.h>
#include "dsp/ParameterConverter.h"

TEST(ParameterConversionTest, VelocityToTLConversion) {
    // ベロシティ127 → TL 0（最大音量）
    EXPECT_EQ(ParameterConverter::velocityToTL(127), 0);
    
    // ベロシティ0 → TL 127（無音）
    EXPECT_EQ(ParameterConverter::velocityToTL(0), 127);
    
    // 中間値のリニアリティ
    EXPECT_EQ(ParameterConverter::velocityToTL(64), 63);
}

TEST(ParameterConversionTest, CCToParameterConversion) {
    // CC値（0-127）からパラメータ値への変換
    // AR: 0-31の範囲
    EXPECT_EQ(ParameterConverter::ccToParameter(0, 31), 0);
    EXPECT_EQ(ParameterConverter::ccToParameter(127, 31), 31);
    EXPECT_EQ(ParameterConverter::ccToParameter(64, 31), 16);
}

TEST(ParameterConversionTest, FrequencyConversion) {
    // MIDI note to OPM frequency
    auto [kc, kf] = ParameterConverter::noteToOPMFreq(69); // A4
    EXPECT_EQ(kc & 0x70, 0x40); // Octave 4
    // KF値は実装に依存するが、A4の周波数に対応すること
}
```

#### 1.5.3 統合テストの例

**tests/integration/MidiProcessingTest.cpp**
```cpp
class MidiProcessingTest : public ::testing::Test {
protected:
    std::unique_ptr<ChipSynthAudioProcessor> processor;
    juce::MidiBuffer midiBuffer;
    juce::AudioBuffer<float> audioBuffer;
    
    void SetUp() override {
        processor = std::make_unique<ChipSynthAudioProcessor>();
        processor->prepareToPlay(44100, 512);
        audioBuffer.setSize(2, 512);
    }
    
    void addNoteOn(int sample, int note, int velocity) {
        midiBuffer.addEvent(
            juce::MidiMessage::noteOn(1, note, (uint8)velocity),
            sample
        );
    }
};

TEST_F(MidiProcessingTest, PolyphonicPlayback) {
    // 和音の再生テスト
    addNoteOn(0, 60, 100);   // C
    addNoteOn(0, 64, 100);   // E
    addNoteOn(0, 67, 100);   // G
    
    processor->processBlock(audioBuffer, midiBuffer);
    
    // 3音が同時に鳴っていることを確認
    // (実装依存: スペクトラム解析などで確認)
}

TEST_F(MidiProcessingTest, VoiceStealingBehavior) {
    // 8音を超えた場合のボイススティーリング
    for (int i = 0; i < 10; i++) {
        addNoteOn(i * 10, 60 + i, 100);
    }
    
    processor->processBlock(audioBuffer, midiBuffer);
    
    // 最初の2音が切られ、最後の8音が鳴っていることを確認
}
```

#### 1.5.4 パフォーマンステスト

**tests/performance/LatencyTest.cpp**
```cpp
TEST(PerformanceTest, ProcessingLatency) {
    ChipSynthAudioProcessor processor;
    processor.prepareToPlay(44100, 64); // Ultra-low latency
    
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    
    // 処理時間の測定
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        processor.processBlock(buffer, midi);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 1000ブロックの平均処理時間
    double avgMicroseconds = duration.count() / 1000.0;
    double bufferDurationMs = (64.0 / 44100.0) * 1000.0;
    double cpuUsage = (avgMicroseconds / 1000.0) / bufferDurationMs * 100.0;
    
    // CPU使用率が目標値以下であることを確認
    EXPECT_LT(cpuUsage, 25.0); // 25%以下
}

TEST(PerformanceTest, MemoryUsage) {
    // メモリ使用量の測定
    // プラグインインスタンスのメモリフットプリント確認
}
```

#### 1.5.5 テスト実行コマンド
```bash
# VSCodeのターミナルで実行
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Debug
ctest --output-on-failure

# 特定のテストのみ実行
./tests/ChipSynthTests --gtest_filter="YmfmWrapperTest.*"

# パフォーマンステストは別途
./tests/ChipSynthTests --gtest_filter="PerformanceTest.*" --gtest_repeat=10
```

### 1.6 Audio Unit検証ガイド（auval）

#### 1.6.1 基本的な検証コマンド
```bash
# Audio Unitが正しくインストールされているか確認
auval -l | grep ChipSynth

# 完全な検証を実行
auval -strict -v aufx Chip Hiro

# クイック検証（開発中の確認用）
auval -v aufx Chip Hiro
```

#### 1.6.2 よくある検証エラーと対処法

##### 1. ERROR: Factory preset 0 is not valid
```
原因: プリセットの初期化が不完全
対処: 
- AUAudioUnitのfactoryPresetsを正しく実装
- 最低1つのデフォルトプリセットを提供
```

##### 2. ERROR: Audio Unit did not cleanup properly
```
原因: リソースの解放漏れ
対処:
- deallocateRenderResourcesで全リソースを解放
- タイマーやスレッドの確実な停止
```

##### 3. ERROR: Reported latency is inconsistent
```
原因: レイテンシー値が動的に変化
対処:
- latencySamplesを一定値で返す
- モード変更時はホストに通知
```

##### 4. WARNING: Render at Maximum Hardware I/O Buffer Size
```
原因: 大きなバッファサイズでの動作未確認
対処:
- maximumFramesToRenderを適切に設定
- 4096サンプル以上でもテスト
```

#### 1.6.3 検証に合格するための実装チェックリスト
```objc
// 必須実装項目
□ allocateRenderResources / deallocateRenderResources
□ internalRenderBlock
□ factoryPresets（最低1つ）
□ parameterTree（全パラメータを含む）
□ latencySamples（一貫した値）
□ tailTime（リリース時間を考慮）
□ shouldBypassEffect対応
□ 全サンプルレート対応（44.1k, 48k, 88.2k, 96k）
□ 全バッファサイズ対応（64〜4096）
```

### 1.7 .opmファイルフォーマット仕様

#### 1.7.1 基本構造
```
//MiOPMdrv sound bank Paramer Ver2002.04.22
//LFO: LFRQ AMD PMD WF NFRQ
//CH: PAN FL CON AMS PMS SLOT NE
//[OPname]: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN

@:0 Instrument 0
LFO: 0 0 0 0 0
CH: 64 7 2 0 0 120 0
M1: 31 5 0 0 0 20 0 1 0 0 0
C1: 31 10 7 8 0 0 0 1 3 0 0
M2: 31 5 0 0 0 18 0 1 0 0 0
C2: 31 18 10 8 1 0 0 1 3 0 0
```

#### 1.7.2 パラメータ詳細

##### ヘッダー行
```
@:プログラム番号 音色名
```

##### LFOパラメータ
| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| LFRQ | 0-255 | LFO周波数 |
| AMD | 0-127 | 振幅変調深度 |
| PMD | 0-127 | ピッチ変調深度 |
| WF | 0-3 | 波形（0:鋸,1:矩形,2:三角,3:ノイズ） |
| NFRQ | 0-31 | ノイズ周波数 |

##### チャンネルパラメータ
| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| PAN | 0-127 | パン（64=中央） |
| FL | 0-7 | フィードバックレベル |
| CON | 0-7 | アルゴリズム（接続） |
| AMS | 0-3 | AMS（振幅変調感度） |
| PMS | 0-7 | PMS（ピッチ変調感度） |
| SLOT | 0-120 | オペレータマスク |
| NE | 0-1 | ノイズ有効 |

##### オペレータパラメータ（M1,C1,M2,C2）
| パラメータ | 範囲 | 説明 |
|-----------|------|------|
| AR | 0-31 | アタックレート |
| D1R | 0-31 | ディケイ1レート |
| D2R | 0-31 | ディケイ2レート |
| RR | 0-15 | リリースレート |
| D1L | 0-15 | ディケイ1レベル |
| TL | 0-127 | トータルレベル |
| KS | 0-3 | キースケール |
| MUL | 0-15 | 周波数倍率 |
| DT1 | 0-7 | デチューン1 |
| DT2 | 0-3 | デチューン2 |
| AMS-EN | 0-1 | AMS有効 |

#### 1.7.3 パーサー実装例
```cpp
class OpmParser {
public:
    struct OpmVoice {
        std::string name;
        uint8_t lfo[5];
        uint8_t ch[7];
        uint8_t op[4][11];
    };
    
    std::vector<OpmVoice> parseFile(const std::string& content) {
        std::vector<OpmVoice> voices;
        std::istringstream stream(content);
        std::string line;
        OpmVoice currentVoice;
        
        while (std::getline(stream, line)) {
            // コメント行をスキップ
            if (line.empty() || line[0] == '/' && line[1] == '/') {
                continue;
            }
            
            // 音色定義の開始
            if (line[0] == '@') {
                parseVoiceHeader(line, currentVoice);
            }
            // LFOパラメータ
            else if (line.substr(0, 4) == "LFO:") {
                parseLFO(line, currentVoice);
            }
            // チャンネルパラメータ
            else if (line.substr(0, 3) == "CH:") {
                parseChannel(line, currentVoice);
            }
            // オペレータパラメータ
            else if (line.size() > 3 && line[2] == ':') {
                parseOperator(line, currentVoice);
                
                // C2まで読んだら音色を追加
                if (line.substr(0, 2) == "C2") {
                    voices.push_back(currentVoice);
                }
            }
        }
        
        return voices;
    }
    
private:
    void parseOperator(const std::string& line, OpmVoice& voice) {
        // "M1: 31 5 0 0 0 20 0 1 0 0 0" のような行をパース
        std::string opName = line.substr(0, 2);
        int opIndex = getOperatorIndex(opName);
        
        std::istringstream iss(line.substr(4));
        for (int i = 0; i < 11; i++) {
            iss >> voice.op[opIndex][i];
        }
    }
    
    int getOperatorIndex(const std::string& name) {
        if (name == "M1") return 0;
        if (name == "C1") return 1;
        if (name == "M2") return 2;
        if (name == "C2") return 3;
        return -1;
    }
};
```

### 1.8 よくあるビルドエラーと対処法

#### 1.8.1 JUCE関連のエラー

##### 1. "Unknown type name 'AudioProcessor'"
```
原因: JUCEモジュールのインクルード漏れ
対処:
#include <JuceHeader.h>
または
#include <juce_audio_processors/juce_audio_processors.h>
```

##### 2. "No member named 'startTimerHz' in 'Timer'"
```
原因: 古いJUCEバージョン
対処:
- JUCE 6.0以降を使用
- または startTimer(1000/30) を使用（30fps の場合）
```

##### 3. Projucerプロジェクトとの競合
```
原因: CMakeとProjucerの設定が混在
対処:
- どちらか一方に統一（CMake推奨）
- .gitignoreに*.jucerを追加
```

#### 1.8.2 ymfm統合時のエラー

##### 1. "ymfm.h not found"
```
原因: インクルードパスの設定ミス
対処: CMakeLists.txtで
target_include_directories(ymfm PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ymfm/src
)
```

##### 2. リンカエラー "Undefined symbols for ymfm::..."
```
原因: ymfmのソースファイルがビルドされていない
対処:
- ymfm_opm.cpp等が add_library に含まれているか確認
- C++17以降を指定 set(CMAKE_CXX_STANDARD 17)
```

##### 3. "multiple definition of ymfm::xxx"
```
原因: ヘッダーに実装が含まれている
対処:
- ymfmはヘッダーオンリーではない
- .cppファイルを正しくリンク
```

#### 1.8.3 Audio Unit特有のエラー

##### 1. "Could not find factory preset 0"
```
原因: factoryPresetsが空
対処:
- (AUAudioUnitFactory)initWithComponentDescription内で設定
- 最低1つのプリセットを用意
```

##### 2. プラグインがDAWに表示されない
```
原因: インストール場所が間違っている
対処:
- ~/Library/Audio/Plug-Ins/Components/ に配置
- codesign --force --sign - ChipSynthAU.component
- 再起動またはkillall -9 AudioComponentRegistrar
```

##### 3. "host crashed" エラー
```
原因: リアルタイムスレッドでの不正な処理
対処:
- malloc/freeを使わない
- Objective-Cのallocを避ける
- std::mutexの代わりにjuce::SpinLockを使用
```

#### 1.8.4 デバッグのヒント
```bash
# Audio Unitのログを確認
log show --predicate 'subsystem == "com.apple.audio.AudioToolbox"' --last 5m

# クラッシュログの確認
~/Library/Logs/DiagnosticReports/

# シンボリックリンクで開発を効率化
ln -s /path/to/build/ChipSynthAU.component ~/Library/Audio/Plug-Ins/Components/
```

### 1.9 基本音色サンプルデータ

#### 1.9.1 エレクトリックピアノ（DX7風）
```cpp
// レジスタ直接書き込み形式
void setupElectricPiano(uint8_t channel) {
    // Algorithm 5, Feedback 7
    writeReg(0x20 + channel, 0x05 | (7 << 3));
    
    // Operator 1 (Modulator 1)
    writeReg(0x40 + channel, 0x12);  // DT1=1, MUL=2
    writeReg(0x60 + channel, 0x4A);  // TL=74
    writeReg(0x80 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xA0 + channel, 0x05);  // AMS=0, D1R=5
    writeReg(0xC0 + channel, 0x05);  // DT2=0, D2R=5
    writeReg(0xE0 + channel, 0x25);  // D1L=2, RR=5
    
    // Operator 2 (Carrier)
    writeReg(0x48 + channel, 0x71);  // DT1=7, MUL=1
    writeReg(0x68 + channel, 0x1A);  // TL=26
    writeReg(0x88 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xA8 + channel, 0x0D);  // AMS=0, D1R=13
    writeReg(0xC8 + channel, 0x06);  // DT2=0, D2R=6
    writeReg(0xE8 + channel, 0x26);  // D1L=2, RR=6
    
    // Operator 3 (Modulator 2)
    writeReg(0x50 + channel, 0x51);  // DT1=5, MUL=1
    writeReg(0x70 + channel, 0x48);  // TL=72
    writeReg(0x90 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xB0 + channel, 0x03);  // AMS=0, D1R=3
    writeReg(0xD0 + channel, 0x05);  // DT2=0, D2R=5
    writeReg(0xF0 + channel, 0x47);  // D1L=4, RR=7
    
    // Operator 4 (Carrier 2)
    writeReg(0x58 + channel, 0x11);  // DT1=1, MUL=1
    writeReg(0x78 + channel, 0x16);  // TL=22
    writeReg(0x98 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xB8 + channel, 0x09);  // AMS=0, D1R=9
    writeReg(0xD8 + channel, 0x05);  // DT2=0, D2R=5
    writeReg(0xF8 + channel, 0x36);  // D1L=3, RR=6
}
```

**.opm形式**
```
@:0 Electric Piano
LFO: 0 0 0 0 0
CH: 64 7 5 0 0 120 0
M1: 31 5 5 5 2 74 0 2 1 0 0
C1: 31 13 6 6 2 26 0 1 7 0 0
M2: 31 3 5 7 4 72 0 1 5 0 0
C2: 31 9 5 6 3 22 0 1 1 0 0
```

#### 1.9.2 シンセベース（ソリッドベース）
```cpp
void setupSynthBass(uint8_t channel) {
    // Algorithm 7, Feedback 6
    writeReg(0x20 + channel, 0x07 | (6 << 3));
    
    // Operator 1 (Modulator)
    writeReg(0x40 + channel, 0x01);  // DT1=0, MUL=1
    writeReg(0x60 + channel, 0x32);  // TL=50
    writeReg(0x80 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xA0 + channel, 0x00);  // AMS=0, D1R=0
    writeReg(0xC0 + channel, 0x00);  // DT2=0, D2R=0
    writeReg(0xE0 + channel, 0x0F);  // D1L=0, RR=15
    
    // Operator 2 (Carrier)
    writeReg(0x48 + channel, 0x01);  // DT1=0, MUL=1
    writeReg(0x68 + channel, 0x18);  // TL=24
    writeReg(0x88 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xA8 + channel, 0x0F);  // AMS=0, D1R=15
    writeReg(0xC8 + channel, 0x00);  // DT2=0, D2R=0
    writeReg(0xE8 + channel, 0x0F);  // D1L=0, RR=15
    
    // Operators 3,4 (Carriers)
    writeReg(0x50 + channel, 0x00);  // DT1=0, MUL=0
    writeReg(0x70 + channel, 0x15);  // TL=21
    writeReg(0x90 + channel, 0x5F);  // KS=1, AR=31
    writeReg(0xB0 + channel, 0x10);  // AMS=0, D1R=16
    writeReg(0xD0 + channel, 0x00);  // DT2=0, D2R=0
    writeReg(0xF0 + channel, 0x07);  // D1L=0, RR=7
    
    // Op4 similar settings...
}
```

**.opm形式**
```
@:1 Synth Bass
LFO: 0 0 0 0 0
CH: 64 6 7 0 0 120 0
M1: 31 0 0 15 0 50 0 1 0 0 0
C1: 31 15 0 15 0 24 0 1 0 0 0
M2: 31 16 0 7 0 21 1 0 0 0 0
C2: 31 16 0 7 0 21 1 0 0 0 0
```

#### 1.9.3 ブラスセクション
```cpp
void setupBrass(uint8_t channel) {
    // Algorithm 4, Feedback 6
    writeReg(0x20 + channel, 0x04 | (6 << 3));
    
    // Modulator settings for bright attack
    writeReg(0x40 + channel, 0x21);  // DT1=2, MUL=1
    writeReg(0x60 + channel, 0x4B);  // TL=75
    writeReg(0x80 + channel, 0x1F);  // KS=0, AR=31
    writeReg(0xA0 + channel, 0x08);  // AMS=0, D1R=8
    writeReg(0xC0 + channel, 0x03);  // DT2=0, D2R=3
    writeReg(0xE0 + channel, 0x48);  // D1L=4, RR=8
    // ... continue for all operators
}
```

**.opm形式**
```
@:2 Brass Section
LFO: 0 0 0 0 0
CH: 64 6 4 0 0 120 0
M1: 31 8 3 8 4 75 0 1 2 0 0
C1: 31 10 2 9 2 23 1 1 0 0 0
M2: 31 9 4 10 3 65 0 3 1 0 0
C2: 31 12 3 8 1 20 1 1 0 0 0
```

#### 1.9.4 ストリングス（アナログ風）
**.opm形式**
```
@:3 Analog Strings
LFO: 22 0 35 0 0
CH: 64 5 1 1 3 120 0
M1: 20 12 0 12 0 40 0 2 3 0 0
C1: 25 8 0 10 0 18 0 1 0 0 0
M2: 22 10 0 12 0 35 0 1 3 0 0
C2: 24 6 0 8 0 20 0 1 0 0 0
```

#### 1.9.5 リードシンセ（ソロ向け）
**.opm形式**
```
@:4 Synth Lead
LFO: 0 0 0 0 0
CH: 64 7 0 0 0 120 0
M1: 31 5 7 15 9 18 0 5 0 0 0
C1: 31 7 5 10 5 12 0 1 0 0 0
M2: 31 5 7 15 9 18 0 4 0 0 0
C2: 31 7 5 10 5 12 0 2 0 0 0
```

#### 1.9.6 オルガン（ハモンド風）
```cpp
void setupOrgan(uint8_t channel) {
    // Algorithm 7 (全オペレータがキャリア)
    writeReg(0x20 + channel, 0x07 | (0 << 3));
    
    // 各オペレータで倍音を生成
    // Op1: 基音
    writeReg(0x40 + channel, 0x00);  // DT1=0, MUL=0 (0.5x)
    writeReg(0x60 + channel, 0x18);  // TL=24
    
    // Op2: 2倍音
    writeReg(0x48 + channel, 0x01);  // DT1=0, MUL=1
    writeReg(0x68 + channel, 0x1C);  // TL=28
    
    // Op3: 3倍音
    writeReg(0x50 + channel, 0x02);  // DT1=0, MUL=2
    writeReg(0x70 + channel, 0x22);  // TL=34
    
    // Op4: 4倍音
    writeReg(0x58 + channel, 0x03);  // DT1=0, MUL=3
    writeReg(0x78 + channel, 0x26);  // TL=38
    
    // エンベロープは全てオルガン風（瞬時立ち上がり）
    for (int op = 0; op < 4; op++) {
        uint8_t base = op * 8 + channel;
        writeReg(0x80 + base, 0x1F);  // AR=31
        writeReg(0xA0 + base, 0x00);  // D1R=0
        writeReg(0xC0 + base, 0x00);  // D2R=0
        writeReg(0xE0 + base, 0x0A);  // D1L=0, RR=10
    }
}
```

#### 1.9.7 音色パラメータのガイドライン

##### アルゴリズム選択の指針
```
Algorithm 0: 4オペレータ直列（金属的な音）
Algorithm 1: 1→2→3＋1→4（ベル、マリンバ）
Algorithm 2: 1→2＋1→3→4（木管楽器）
Algorithm 3: 1→2→3、1→4（複雑なFM）
Algorithm 4: 1→2→3＋4（エレピ、ブラス）
Algorithm 5: 1→2、1→3、1→4（豊かな倍音）
Algorithm 6: 1→2＋3→4（パッド、ストリングス）
Algorithm 7: 1＋2＋3＋4（オルガン、加算合成）
```

##### TL（Total Level）設定の目安
```
キャリア: 10-30（音量）
モジュレータ: 30-80（変調の深さ）
- 値が小さいほど影響大
- 127で完全に無音/無効果
```

##### エンベロープ設定の特徴
```
ピアノ系: AR=31, D1R=5-10, D2R=3-8, RR=5-10
ベース系: AR=31, D1R=10-15, D2R=0, RR=10-15
ブラス系: AR=28-31, D1R=8-12, D2R=2-5, RR=8-12
ストリングス: AR=20-25, D1R=8-12, D2R=0, RR=8-12
オルガン: AR=31, D1R=0, D2R=0, RR=8-10
```

## 開発フロー

1. **環境構築**
   ```bash
   git clone https://github.com/hiroaki0923/ChipSynth-AU.git
   cd ChipSynth-AU
   git submodule update --init  # JUCE, ymfm
   ```

2. **VSCodeで開発**
   - CMake Toolsが自動的にプロジェクトを認識
   - IntelliSenseが効いた快適な開発

3. **テスト駆動開発**
   - テストを書いてから実装
   - `Cmd+Shift+B`でビルド
   - Test Explorerでテスト実行

4. **デバッグ**
   - Logic ProやStandaloneでの実機デバッグ
   - ブレークポイントとステップ実行