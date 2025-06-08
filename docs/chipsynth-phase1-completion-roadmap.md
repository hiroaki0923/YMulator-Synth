# ChipSynth-AU Phase 1 完了ロードマップ

このドキュメントは、Phase 1（基盤構築）の残り30%を完了するための具体的な実装手順を定義します。

## 現在の状況（70%完了）

### ✅ 完了済み（70%）
- プロジェクト構造とビルドシステム
- ymfmライブラリ統合
- 基本的なAudio Unitインターフェース
- MIDI Note On/Off処理
- 単音音声出力

### ❌ 未完了（30%）
- **JUCEパラメータシステム** (高優先度)
- **Factory Preset実装** (中優先度)  
- **MIDI CC処理** (中優先度)
- **ポリフォニック対応** (低優先度)

## 実装タスク詳細

### Task 1: JUCEパラメータシステム実装 (最優先)

**推定工数**: 2-3日  
**優先度**: 高  
**依存関係**: なし

#### 1.1 AudioProcessorValueTreeState構築

**実装ファイル**: `src/PluginProcessor.cpp`, `src/PluginProcessor.h`

```cpp
// 追加実装内容
class ChipSynthAudioProcessor : public juce::AudioProcessor
{
private:
    AudioProcessorValueTreeState parameters;
    std::unordered_map<int, AudioParameterInt*> ccToParameterMap;
    
public:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setupCCMapping();
    void handleMidiCC(int ccNumber, int value);
};
```

**実装手順**:
1. `createParameterLayout()`でグローバル+オペレータパラメータ定義
2. constructor内で`parameters`初期化
3. VOPMex互換MIDI CCマッピング実装
4. `processBlock()`内でMIDI CC処理追加

**検証方法**:
- DAWでオートメーション確認
- MIDI CCコントローラーでリアルタイム制御
- パラメータ状態保存・復元テスト

#### 1.2 パラメータ→ymfm連携

**実装ファイル**: `src/dsp/YmfmWrapper.cpp`

```cpp
// 追加実装内容
void updateOperatorParameters(int channel, int operator, const OperatorParams& params);
void updateGlobalParameters(const GlobalParams& params);
```

**実装手順**:
1. パラメータ変更コールバック実装
2. ymfmレジスタ更新順序最適化
3. スレッドセーフなパラメータ同期

### Task 2: Factory Preset実装 (中優先度)

**推定工数**: 1-2日  
**優先度**: 中  
**依存関係**: Task 1完了後

#### 2.1 プリセット管理基盤

**実装ファイル**: `src/utils/PresetManager.cpp`, `src/utils/PresetManager.h`

```cpp
// 実装内容
class PresetManager
{
public:
    static std::vector<FMPreset> getFactoryPresets();
    static FMPreset loadPreset(int index);
    void applyPreset(const FMPreset& preset, AudioProcessorValueTreeState& parameters);
};
```

**実装手順**:
1. 8つのFactory Presetデータ定義
2. `getNumPrograms()`/`setCurrentProgram()`実装
3. プリセット切り替え時のパラメータ同期
4. `auval`検証エラー解消

#### 2.2 状態保存・復元

**実装ファイル**: `src/PluginProcessor.cpp`

```cpp
// 実装内容
void getStateInformation(MemoryBlock& destData) override;
void setStateInformation(const void* data, int sizeInBytes) override;
```

**実装手順**:
1. XMLベースの状態シリアライゼーション
2. プリセット番号保存・復元
3. パラメータ一括復元機能

### Task 3: MIDI CC処理完全実装 (中優先度)

**推定工数**: 1日  
**優先度**: 中  
**依存関係**: Task 1完了後

#### 3.1 VOPMex互換CCマッピング

**実装ファイル**: `src/PluginProcessor.cpp`

**CCマッピング完全版**:
```cpp
// CC 14: Algorithm (0-7)
// CC 15: Feedback (0-7)
// CC 16-19: OP1-4 Total Level (0-127)
// CC 20-23: OP1-4 Multiple (0-15)
// CC 24-27: OP1-4 Detune1 (0-7)
// CC 39-42: OP1-4 Key Scale (0-3)
// CC 43-46: OP1-4 Attack Rate (0-31)
// CC 47-50: OP1-4 Decay1 Rate (0-31)
// CC 51-54: OP1-4 Decay2 Rate (0-31)
// CC 55-58: OP1-4 Release Rate (0-15)
// CC 59-62: OP1-4 Decay1 Level (0-15)
```

#### 3.2 ベロシティ対応強化

**実装ファイル**: `src/dsp/YmfmWrapper.cpp`

```cpp
// ベロシティ→TL変換改善
uint8_t convertVelocityToTL(int velocity, int baseTL);
```

### Task 4: ポリフォニック対応 (低優先度)

**推定工数**: 1-2日  
**優先度**: 低  
**依存関係**: Task 1-3完了後

#### 4.1 Voice Management実装

**実装ファイル**: `src/core/VoiceManager.cpp`, `src/core/VoiceManager.h`

```cpp
// 実装内容
class VoiceManager
{
private:
    struct Voice
    {
        int channel;
        int noteNumber;
        bool active;
        float age;
    };
    
    std::array<Voice, 8> voices; // OPM 8チャンネル
    
public:
    int allocateVoice(int noteNumber);
    void releaseVoice(int noteNumber);
    void updateVoices(float deltaTime);
};
```

**実装手順**:
1. 8チャンネル音声割り当てアルゴリズム
2. 音声盗取（Voice Stealing）実装
3. チャンネル別MIDI処理分離

## 実装スケジュール

### 週1: Task 1 (パラメータシステム)
- **月曜**: AudioParameterTree実装
- **火曜**: MIDI CCマッピング実装
- **水曜**: ymfm連携とテスト
- **木曜**: バグ修正と最適化

### 週2: Task 2-3 (プリセット+MIDI CC)
- **月曜**: Factory Preset実装
- **火曜**: 状態保存・復元実装
- **水曜**: MIDI CC完全対応
- **木曜**: 統合テストとauval検証

### 週3: Task 4 (ポリフォニック、オプション)
- **月曜-火曜**: Voice Manager実装
- **水曜**: 統合テストと最適化
- **木曜**: ドキュメント更新とPhase 1完了

## 品質保証計画

### テスト項目
1. **機能テスト**
   - 全パラメータのMIDI CC制御
   - Factory Presetの正常動作
   - 状態保存・復元の完全性
   - ポリフォニック音声割り当て

2. **パフォーマンステスト**
   - CPU使用率 < 15% (4-core, Balanced mode)
   - パラメータ更新レイテンシー < 3ms
   - メモリリーク検出

3. **互換性テスト**
   - macOS 10.13+ 対応確認
   - 主要DAW (Logic Pro, Ableton Live, Pro Tools) 動作確認
   - auval完全通過

### 検証手順
```bash
# 1. ビルドテスト
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# 2. Audio Unit検証
auval -v aufx ChpS Vend

# 3. DAW統合テスト
# Logic Pro X, Ableton Live でロード・動作確認

# 4. パフォーマンス測定
# Activity Monitor でCPU使用率監視
```

## リスク管理

### 高リスク項目
1. **auval検証失敗**: Factory Preset実装不備
   - **対策**: 早期にauval実行、問題の早期発見

2. **パフォーマンス劣化**: パラメータ更新頻度過多
   - **対策**: プロファイリング実装、更新レート制限

3. **MIDI CC互換性問題**: VOPMexとの差異
   - **対策**: 実際のVOPMexと動作比較テスト

### 中リスク項目
1. **ポリフォニック実装複雑化**: 音声管理ロジック
   - **対策**: 段階的実装、シンプルなアルゴリズム採用

## 完了基準

### Phase 1 完了条件
- ✅ 全パラメータのリアルタイム制御
- ✅ 8つのFactory Preset正常動作
- ✅ MIDI CC (14, 15, 16-62) 完全対応
- ✅ auval検証エラーゼロ
- ✅ 主要DAWでの正常動作確認
- ✅ CPU使用率目標達成 (< 15%)
- ✅ 開発ドキュメント更新

### Phase 2 移行準備
- UI実装用のパラメータインターフェース完成
- VOPM形式プリセット読み込み基盤
- OPNA実装準備（次フェーズ）

この計画により、2-3週間でPhase 1を完了し、Phase 2のUI実装へスムーズに移行できます。