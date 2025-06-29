# Crash Prevention Lessons - YMulator-Synth

このドキュメントは、実際に発生したクラッシュから学んだ教訓をまとめ、同様の問題を防ぐためのガイドラインを提供します。

## 🚨 Critical Crash Case: Null Pointer Dereference in MIDI Processing

### 発生したクラッシュ

**日時**: 2025-06-29 18:52:07  
**ログファイル**: `AUHostingServiceXPC_arrow-2025-06-29-185207.ips`  
**症状**: GarageBandでプラグインロード時にクラッシュ  

**スタックトレース**:
```
Thread 8 (AUOOPRenderingServer-302551321): CRASHED
ymulatorsynth::MidiProcessor::handlePitchBend(int) + 176
ymulatorsynth::MidiProcessor::processMidiMessages(juce::MidiBuffer&) + 1136  
YMulatorSynthAudioProcessor::processBlock(...) + 700
```

**エラータイプ**: `EXC_BAD_ACCESS` / `SIGSEGV`  
**原因**: Null pointer (0x0) へのアクセス

### 根本原因分析

**問題のコード** (MidiProcessor.cpp:142):
```cpp
// 危険: パラメータが存在しない場合にnull pointer dereference
int pitchBendRange = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::PitchBendRange));
```

**根本原因**:
1. `PitchBendRange` パラメータが `ParameterManager::createParameterLayout()` で定義されていなかった
2. `getRawParameterValue()` が nullptr を返しているのに、直接デリファレンスしていた
3. パラメータの存在チェックが不十分だった

### 修正手順

#### 1. 緊急修正 (一時的対応)
```cpp
// SAFETY: Check if parameter exists to prevent null pointer crash
auto* pitchBendRangeParam = parameters.getRawParameterValue(ParamID::Global::PitchBendRange);
int pitchBendRange = pitchBendRangeParam ? static_cast<int>(*pitchBendRangeParam) : 2; // Default: 2 semitones
```

#### 2. 正しい修正 (根本解決)
**Step 1**: ParameterManagerにパラメータを追加
```cpp
// ParameterManager.cpp - createParameterLayout()内
// Pitch Bend Range (1-12 semitones)
layout.add(std::make_unique<juce::AudioParameterInt>(
    ParamID::Global::PitchBendRange, "Pitch Bend Range", 1, 12, 2));
```

**Step 2**: 元のコードに戻す
```cpp
// MidiProcessor.cpp - 安全にアクセス可能
int pitchBendRange = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::PitchBendRange));
```

#### 3. 不要な修正の削除
- Rate limiting の削除 (パッチ的対応だったため)
- atomicカウンターの削除
- 過度な防御コードの削除

### 検証

**ビルド確認**:
```bash
cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful"
# ✅ Build successful

auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED"  
# ✅ auval PASSED
```

**GarageBandテスト**: ✅ クラッシュなし

## 📚 予防ガイドライン

### 1. パラメータアクセスの安全パターン

#### ❌ 危険なパターン
```cpp
// 直接デリファレンス - クラッシュリスク
float value = *parameters.getRawParameterValue(paramId);
```

#### ✅ 安全なパターン
```cpp
// Pattern 1: null check (一時的な防御)
auto* param = parameters.getRawParameterValue(paramId);
float value = param ? *param : defaultValue;

// Pattern 2: パラメータ存在保証 (推奨)
// ParameterLayoutで必ず定義し、直接アクセス
float value = *parameters.getRawParameterValue(paramId);
```

### 2. パラメータレイアウト設計原則

#### 必須パラメータの定義
```cpp
// ParameterManager::createParameterLayout()で必ず定義
namespace ParamID::Global {
    constexpr const char* PitchBendRange = "pitch_bend_range";  // 定義
    constexpr const char* Algorithm = "algorithm";             // 定義済み
    constexpr const char* Feedback = "feedback";               // 定義済み
}

// レイアウトで必ず追加
layout.add(std::make_unique<juce::AudioParameterInt>(
    ParamID::Global::PitchBendRange, "Pitch Bend Range", 1, 12, 2));
```

#### パラメータ存在の検証
```cpp
// 開発時の検証用コード (Debug buildのみ)
#ifdef JUCE_DEBUG
void validateParameterLayout(juce::AudioProcessorValueTreeState& parameters) {
    std::vector<const char*> criticalParams = {
        ParamID::Global::Algorithm,
        ParamID::Global::Feedback,
        ParamID::Global::PitchBendRange,  // 今回追加
        ParamID::Global::GlobalPan
    };
    
    for (const char* paramId : criticalParams) {
        jassert(parameters.getRawParameterValue(paramId) != nullptr);
    }
}
#endif
```

### 3. MIDI処理での注意点

#### Pitch Bend処理の安全性
```cpp
void MidiProcessor::handlePitchBend(int pitchBendValue) {
    // 1. 入力値検証
    CS_ASSERT_PARAMETER_RANGE(pitchBendValue, 0, 16383);
    
    // 2. パラメータアクセス (存在が保証されている前提)
    int pitchBendRange = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::PitchBendRange));
    
    // 3. 計算と適用
    float pitchBendSemitones = ((pitchBendValue - 8192) / 8192.0f) * pitchBendRange;
    
    // 4. 全チャンネルへの適用
    for (int channel = 0; channel < 8; ++channel) {
        if (voiceManager.isVoiceActive(channel)) {
            ymfmWrapper.setPitchBend(channel, pitchBendSemitones);
        }
    }
}
```

### 4. 開発フローでの予防策

#### レビューチェックリスト
- [ ] 新しいパラメータIDを追加した場合、ParameterLayoutにも追加したか
- [ ] `getRawParameterValue()` の結果を直接デリファレンスしていないか
- [ ] MIDI処理で使用するパラメータが全て定義されているか
- [ ] テスト環境でauval検証が通るか

#### テスト戦略
```cpp
// 基本的なパラメータ存在テスト (既存テストに追加可能)
TEST(ParameterTest, CriticalParametersExist) {
    auto layout = ParameterManager::createParameterLayout();
    // PitchBendRangeなど重要パラメータの存在確認
    EXPECT_TRUE(/* parameter exists check */);
}
```

## 🎯 今後の対策

### 1. 静的解析の活用
- Clang Static Analyzer でnull pointer dereference検出
- CodeQL での潜在的脆弱性検出

### 2. 実行時検証の強化
```cpp
// リリースビルドでも有効な軽量チェック
#define CS_SAFE_PARAM_ACCESS(params, id, defaultVal) \
    ([&]() { \
        auto* p = params.getRawParameterValue(id); \
        return p ? *p : defaultVal; \
    })()
```

### 3. ドキュメント化
- 新しいパラメータ追加時の手順書
- クラッシュレポートの分析手順
- テストケースのカバレッジ強化

## 📊 Impact Summary

| 項目 | 修正前 | 修正後 |
|------|--------|--------|
| GarageBandクラッシュ | ❌ 発生 | ✅ 解消 |
| auval検証 | ❌ 失敗 | ✅ 成功 |
| パラメータ完全性 | ❌ 欠損あり | ✅ 完全 |
| コード品質 | ⚠️ 防御的 | ✅ 安全で簡潔 |

この経験から、**「パッチではなく根本修正」** の重要性と、**パラメータレイアウトの完全性チェック** の必要性が明確になりました。