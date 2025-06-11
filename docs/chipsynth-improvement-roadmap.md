# ChipSynth-AU 改善ロードマップ

このドキュメントは、improvement.md レポートの分析に基づいて作成された改善実施計画です。

## 概要

改善項目を優先度と実装タイミングに基づいて3つのフェーズに分類し、段階的に実装します。

## Phase A: 即座実施項目（Phase 2完了前）

### A.1 コードクリーンアップ 🔴 **高優先度**

#### A.1.1 未使用ファイルの削除
- [x] `src/core/FMCore.h` の削除
- [x] `src/core/FMCore.cpp` の削除
- [x] CMakeLists.txt からFMCore関連の記述を削除

#### A.1.2 冗長コードの削除
- [x] `src/dsp/YmfmWrapper.h` の冗長な `ChipSynthInterface` クラス定義（11-23行目）を削除
  - YmfmWrapper自体が ymfm_interface を継承しているため不要

### A.2 デバッグ出力の最適化 🔴 **高優先度**

#### A.2.1 条件付きコンパイルマクロの実装
- [x] プロジェクト共通のデバッグマクロ定義ヘッダーを作成（`src/utils/Debug.h`）
- [x] 全ソースファイルで `DBG()` と `std::cout` を条件付きマクロに置換
  - [x] YmfmWrapper.cpp (31箇所をCS_DBG/CS_LOGF に置換)
  - [x] PluginProcessor.cpp (37箇所をCS_DBGに置換)
  - [x] VoiceManager.cpp (8箇所をCS_DBGに置換)
  - [x] PresetManager.cpp (6箇所をCS_DBGに置換)
  - [x] VOPMParser.cpp (5箇所をCS_DBGに置換)
- [x] リリースビルドでデバッグ出力を完全に除外

**実装例:**
```cpp
// src/utils/Debug.h
#ifdef JUCE_DEBUG
    #define CS_DBG(msg) DBG(msg)
    #define CS_LOG(msg) std::cout << msg << std::endl
#else
    #define CS_DBG(msg) ((void)0)
    #define CS_LOG(msg) ((void)0)
#endif
```

## Phase B: Phase 2.3での実施項目

### B.1 コード可読性向上 🟡 **中優先度**

#### B.1.1 YmfmWrapper のマジックナンバー定数化
- [ ] YM2151レジスタアドレスの定数定義
- [ ] ビットマスクとシフト値の定数定義
- [ ] チップ固有の値の定数定義

**実装例:**
```cpp
namespace YM2151Regs {
    // Register addresses
    constexpr uint8_t REG_TEST = 0x01;
    constexpr uint8_t REG_KEY_ON = 0x08;
    constexpr uint8_t REG_NOISE = 0x0F;
    
    // Bit masks
    constexpr uint8_t MASK_ALGORITHM = 0x07;
    constexpr uint8_t MASK_FEEDBACK = 0x38;
    
    // Shifts
    constexpr uint8_t SHIFT_FEEDBACK = 3;
}
```

#### B.1.2 パラメータIDの型安全性向上
- [ ] パラメータID定義用の構造体/名前空間を作成
- [ ] 文字列リテラルを定数に置換
- [ ] パラメータ生成ヘルパー関数の実装

**実装例:**
```cpp
namespace ParamID {
    namespace Global {
        constexpr const char* Algorithm = "algorithm";
        constexpr const char* Feedback = "feedback";
    }
    
    namespace Op {
        inline std::string tl(int opNum) { 
            return "op" + std::to_string(opNum) + "_tl"; 
        }
    }
}
```

## Phase C: Phase 3での実施項目

### C.1 UIコードリファクタリング 🟢 **低優先度**

#### C.1.1 OperatorPanel の簡素化
- [ ] コントロール仕様を定義する構造体の作成
- [ ] データ駆動型のUI生成ロジック実装
- [ ] 個別のunique_ptrメンバ変数を削減

**実装例:**
```cpp
struct ControlSpec {
    std::string paramIdSuffix;
    std::string labelText;
    float minValue;
    float maxValue;
    float defaultValue;
};

const std::vector<ControlSpec> operatorControls = {
    {"tl", "Total Level", 0, 127, 0},
    {"ar", "Attack Rate", 0, 31, 0},
    // ...
};
```

#### C.1.2 MainComponent の最適化
- [ ] valueTreePropertyChanged の呼び出し条件を精査
- [ ] 不要なUI更新を削減

### C.2 コードスタイル統一 🟢 **低優先度**

- [ ] .clang-format ファイルの作成
- [ ] 全ソースコードに clang-format を適用
- [ ] エディタ設定の統一

## 実装スケジュール

| フェーズ | 期間 | 内容 |
|---------|------|------|
| Phase A | 即座（1-2時間） | クリーンアップとデバッグ最適化 |
| Phase B | Phase 2.3実施時 | コード可読性向上 |
| Phase C | Phase 3実施時 | UIリファクタリングとスタイル統一 |

## 期待される効果

1. **即座の効果（Phase A）**
   - ビルドサイズの削減
   - リリースビルドのパフォーマンス向上
   - コードベースの整理

2. **中期的効果（Phase B）**
   - コードの可読性向上
   - バグ発生リスクの低減
   - メンテナンス性の向上

3. **長期的効果（Phase C）**
   - UI開発の効率化
   - 一貫したコードスタイル
   - 新規開発者の参入障壁低減

## 注意事項

- 各フェーズは独立して実施可能
- 実装時は必ず動作確認とauval検証を実施
- Phase Aの項目は他の開発作業と並行して実施可能

---

## 実装進捗

### Phase A 完了状況

**完了日**: 2025-06-11  
**実装時間**: 約1時間

#### 実装内容
1. **コードクリーンアップ** ✅
   - 未使用のFMCore.h/cppファイルを削除
   - CMakeLists.txtから参照を削除
   - YmfmWrapper.hから冗長なChipSynthInterfaceクラスを削除

2. **デバッグ出力の最適化** ✅
   - src/utils/Debug.h を作成（条件付きコンパイルマクロ）
   - 87箇所のデバッグ出力を新マクロに置換
   - リリースビルドでのパフォーマンス向上を実現

#### 成果
- ビルドサイズの削減
- リリースビルドでのコンソール出力ノイズ削減
- コードベースの整理完了
- auval検証通過確認済み

### 次のステップ
Phase B（コード可読性向上）は、Phase 2.3の実施時に着手予定。

---

**最終更新**: 2025-06-11