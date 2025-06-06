# ChipSynth AU アーキテクチャ決定記録

## 目次
- [ADR-001: UIフレームワークの選定](#adr-001-uiフレームワークの選定)
- [ADR-002: FM音源エミュレーションライブラリの選定](#adr-002-fm音源エミュレーションライブラリの選定)
- [ADR-003: 録音フォーマットの選定](#adr-003-録音フォーマットの選定)
- [ADR-004: Audio Unitバージョンの選定](#adr-004-audio-unitバージョンの選定)
- [ADR-005: MIDI実装方式の選定](#adr-005-midi実装方式の選定)
- [ADR-006: プリセット音色フォーマットの選定](#adr-006-プリセット音色フォーマットの選定)
- [ADR-007: MIDIチャンネルとチップ割り当て方式の選定](#adr-007-midiチャンネルとチップ割り当て方式の選定)
- [ADR-008: レイテンシーとCPU使用率のトレードオフ設計](#adr-008-レイテンシーとcpu使用率のトレードオフ設計)
- [ADR-009: スレッディングモデルとリアルタイム処理](#adr-009-スレッディングモデルとリアルタイム処理)

---

## ADR-001: UIフレームワークの選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
Audio Unitプラグインのユーザーインターフェース実装において、以下の3つの選択肢を検討した：
1. JUCE Framework
2. ネイティブ実装（AppKit/Cocoa）
3. SwiftUI

本プラグインは以下の要件を満たす必要がある：
- VOPMライクな複雑なシンセサイザーUI
- リアルタイムの波形表示とパラメータ更新
- 将来的なクロスプラットフォーム展開の可能性
- 効率的な開発とメンテナンス

### 決定
**JUCE Frameworkを採用する**

### 理由

#### 採用理由
1. **オーディオプラグイン専用の成熟したフレームワーク**
   - ノブ、スライダー、エンベロープ表示など必要なUIコンポーネントが標準装備
   - 多数の商用プラグイン（Serum、Vital等）での実績

2. **クロスプラットフォーム対応**
   - 同一コードベースでAU/VST3/AAX対応可能
   - 将来的なWindows版展開が容易

3. **開発効率**
   - C++で統一された開発環境
   - ymfmとのシームレスな統合
   - 豊富なドキュメントとコミュニティサポート

4. **パフォーマンス**
   - リアルタイム処理に最適化済み
   - 効率的なグラフィックス描画

#### 他選択肢を不採用とした理由

**ネイティブ実装（AppKit）**
- ✗ カスタムUIコンポーネントの開発工数が膨大
- ✗ macOS限定でクロスプラットフォーム展開が困難
- ✗ Objective-C++とC++の混在による複雑性

**SwiftUI**
- ✗ Audio UnitとC++コードとの統合が困難
- ✗ リアルタイム描画のパフォーマンス懸念
- ✗ カスタム描画コンポーネントの制限
- ✗ macOS 10.15以降のみサポート

### 影響

#### ポジティブな影響
- 開発期間の短縮（推定30-40%削減）
- 保守性の向上
- 将来的な機能拡張の容易さ
- プロフェッショナルなUIの実現

#### ネガティブな影響
- ライセンスコスト（商用利用時：$800/年〜）
- JUCEフレームワークの学習が必要
- バイナリサイズの増加（約2-3MB）
- macOSネイティブとは異なるルック&フィール

### 実装方針

1. **ライセンス戦略**
   - 開発初期：Personal/Educationライセンス（無料）
   - 商用リリース時：適切なライセンスを購入
   - オープンソース化の場合：GPLv3での公開を検討

2. **UIコンポーネント設計**
   ```cpp
   class ChipSynthPluginEditor : public juce::AudioProcessorEditor {
       // JUCEコンポーネントを活用した実装
       juce::Slider operatorSliders[4][10];  // 各オペレータのパラメータ
       juce::ComboBox algorithmSelector;      // アルゴリズム選択
       CustomEnvelopeDisplay envelopeView;    // カスタム波形表示
   };
   ```

3. **カスタムルック&フィール**
   - VOPMライクなデザインを再現するカスタムLookAndFeelクラスを実装
   - 必要に応じてネイティブ風の外観にカスタマイズ

### 参考資料
- [JUCE Documentation](https://docs.juce.com)
- [JUCE Licensing](https://juce.com/get-juce/)
- 類似プロジェクト: OPNplug, Dexed

---

## ADR-002: FM音源エミュレーションライブラリの選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
FM音源エミュレーションのコアライブラリとして、以下の選択肢を検討した：
1. ymfm（Aaron Giles作）
2. Nuked-OPM（Nuke.YKT作）
3. MAME FM音源コア
4. 独自実装

### 決定
**ymfmライブラリを採用する**

### 理由

#### 採用理由
1. **包括的なチップサポート**
   - OPM、OPNA、OPN、OPL系を統一的なAPIで実装
   - 将来的な拡張（OPN2、OPL3等）が容易

2. **実装品質**
   - 実チップの解析に基づく高精度エミュレーション
   - モダンなC++実装で保守性が高い
   - アクティブなメンテナンス

3. **ライセンス**
   - BSD-3-Clauseライセンスで商用利用可能
   - GPLの制約を受けない

4. **パフォーマンス**
   - 最適化された実装で低レイテンシ
   - リアルタイム処理に適している

#### 他選択肢を不採用とした理由
- **Nuked-OPM**: 単一チップのみ対応、拡張性に欠ける
- **MAME FM音源**: ライセンスが複雑、組み込みが困難
- **独自実装**: 開発工数が膨大、品質保証が困難

---

## ADR-003: 録音フォーマットの選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
チップチューンの録音フォーマットとして、以下を検討した：
1. S98フォーマット
2. VGM（Video Game Music）フォーマット
3. 独自フォーマット

### 決定
**S98フォーマットを採用する**

### 理由

#### 採用理由
1. **日本のレトロPC文化との親和性**
   - PC-98、X68000コミュニティで広く使用
   - VOPMユーザーに馴染みがある

2. **シンプルな仕様**
   - レジスタダンプ形式で実装が容易
   - ファイルサイズが比較的小さい

3. **既存エコシステム**
   - 多数の再生プレイヤーが存在
   - コンバータツールが充実

#### VGMを不採用とした理由
- より複雑な仕様で実装工数が増加
- 主に海外コミュニティで使用され、対象ユーザーと乖離
- ファイルサイズが大きくなりがち

### 補足
将来的にVGMエクスポート機能を追加することは可能。Phase 4以降の拡張機能として検討。

---

## ADR-004: Audio Unitバージョンの選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
macOS向けプラグインフォーマットとして、以下を検討した：
1. Audio Unit v3（AUv3）
2. Audio Unit v2
3. 両方のサポート

### 決定
**Audio Unit v3を採用し、v2互換レイヤーを提供する**

### 理由

#### 採用理由
1. **将来性**
   - Appleが推奨する最新規格
   - iOS/iPadOSとの互換性

2. **セキュリティ**
   - サンドボックス化による安定性向上
   - App Storeでの配布が可能

3. **JUCE対応**
   - JUCEがAUv3を完全サポート
   - 自動的にv2互換性も提供

#### 実装方針
- JUCEのAudioProcessorを使用
- v3ネイティブ機能を活用しつつ、v2ホストでも動作
- iOS版への展開を視野に入れた設計

---

## ADR-005: MIDI実装方式の選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
MIDIパラメータ制御の実装方式として検討した：
1. 独自のCCマッピング
2. VOPMex互換CCマッピング
3. 標準的なシンセサイザーCCマッピング

### 決定
**VOPMex互換CCマッピングを採用する**

### 理由

#### 採用理由
1. **既存ユーザーベース**
   - VOPMユーザーの移行が容易
   - 既存のMIDIデータ資産を活用可能

2. **実績のある設計**
   - 実使用で検証されたマッピング
   - FM音源特有のパラメータに最適化

3. **ドキュメント充実**
   - VOPMexの詳細な仕様書が存在
   - ユーザーの学習コストが低い

#### 拡張性
- NRPNによるモード切り替えを実装
- 将来的に代替マッピングモードを追加可能

---

## ADR-006: プリセット音色フォーマットの選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
プリセット音色の保存・読み込みフォーマットとして検討した：
1. VOPM .opmフォーマット（テキスト形式）
2. val-soundバイナリフォーマット
3. 独自JSONフォーマット
4. 独自バイナリフォーマット

### 決定
**複数フォーマットのサポートを実装する**
- 読み込み: VOPM .opm形式（プライマリ）、将来的に他フォーマット対応を検討
- 保存: VOPM .opm形式をプライマリ、独自JSON形式を拡張用

### 理由

#### 採用理由
1. **既存資産の活用**
   - 大量のVOPM音色ファイルが存在
   - 将来的な他形式音色ライブラリとの互換性も視野に

2. **可読性と互換性のバランス**
   - .opm形式: 人間が読めるテキスト形式
   - エディタでの直接編集が可能
   - 他のVOPM互換ツールとの相互運用性

3. **拡張性の確保**
   - JSONフォーマットで新機能に対応
   - ベロシティカーブ、エフェクト設定等の拡張パラメータ
   - 将来的な音色フォーマットの追加が容易

#### 実装詳細
```cpp
// .opmフォーマット例
//@:0 Instrument Name
//LFO: 0 0 0 0 0
//CH: 64 7 2 0 0 120 0
//M1: 31 15 0 0 15 0 0 3 0 0 0
//C1: 31 15 0 0 15 0 0 3 0 0 0
//M2: 31 15 0 0 15 0 0 3 0 0 0
//C2: 31 15 0 0 15 0 0 3 0 0 0
```

#### 将来の拡張
- 他の音色フォーマットのインポート機能
- SysExダンプのインポート機能
- クラウドベースの音色共有機能
- 機械学習による音色分類・タグ付け

---

## ADR-007: MIDIチャンネルとチップ割り当て方式の選定

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
複数のFM音源チップ（OPM/OPNA）を搭載する本プラグインにおいて、MIDIチャンネルと物理チップの割り当て方式を検討した。主に以下の3つの方式を比較検討した：

1. **Traditional方式**: VOPMと同様の自動ボイスアロケーション
2. **Per-Chip方式**: 1 MIDIチャンネル = 1チップの固定割り当て
3. **Hybrid方式**: 両方式の混在

### 決定
**Traditional方式（自動ボイスアロケーション）を採用する**

### 理由

#### 採用理由
1. **一般的なDAWワークフローとの親和性**
   - MIDIチャンネル = 楽器パートという標準的な使い方が可能
   - 1チャンネルで和音演奏が自然に行える
   - 既存のMIDIデータをそのまま利用可能

2. **ユーザビリティの優先**
   - 「幅広い人に楽しんでもらう」という製品目標に合致
   - 初心者でも直感的に理解できる動作
   - 特殊な知識なしに音楽制作が可能

3. **VOPMとの互換性**
   - 既存VOPMユーザーがシームレスに移行可能
   - 操作感の一貫性を維持

4. **柔軟な音色管理**
   - 同一音色を複数チャンネルで使用する際の制約がない
   - プログラムチェンジによる音色切り替えが標準的

#### 他方式を不採用とした理由

**Per-Chip方式**
- MIDIの使い方が特殊で学習コストが高い
- チャンネルごとの発音数制限により和音演奏が困難
- 既存MIDIデータの移植に大幅な編集が必要

**Hybrid方式**
- 動作の予測が困難で混乱を招く
- チャンネルによって挙動が異なり一貫性に欠ける
- 実装・保守の複雑性が増大

### 実装方針

1. **インテリジェントなボイスアロケーション**
   ```cpp
   // 音色特性に基づく最適なチップ選択
   if (voice.requiresNoise()) {
       // OPNA優先（SSGノイズ機能）
   } else if (voice.requiresHighPolyphony()) {
       // OPM優先（8音ポリフォニー）
   }
   ```

2. **音色ごとの推奨チップ設定**
   - 各音色に「推奨チップ」メタデータを持たせる
   - S98録音時の再現性を向上

3. **S98出力の最適化**
   - 録音時はチップ割り当ての一貫性を保持
   - ヘッダーコメントに使用チップ情報を記録

### 補足
将来的に上級者向けオプションとして、特定のMIDIチャンネルを特定のチップに固定する機能を追加することは可能。ただし、メインのユーザー体験を損なわないよう、高度な設定として実装する。

---

## ADR-008: レイテンシーとCPU使用率のトレードオフ設計

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
ソフトウェア音源として、リアルタイム演奏時の応答性（低レイテンシー）と、CPU使用率のバランスを検討した。FM音源は比較的軽量な処理であるが、複数チップのエミュレーションと高品質な音声処理を両立する必要がある。

一般的なソフトウェア音源のレイテンシー目標：
- 超低レイテンシー: < 3ms（リアルタイム演奏重視）
- 低レイテンシー: 3-10ms（一般的なシンセ）
- 中レイテンシー: 10-20ms（複雑な音源）

リアルタイム演奏における体感：
- < 5ms: ほぼ遅延を感じない
- 5-10ms: 敏感な人は感じるが演奏可能
- 10-20ms: 明確に遅延を感じる
- > 20ms: 演奏が困難

### 決定
**3段階のレイテンシーモードを実装し、用途に応じて切り替え可能にする**

1. **Ultra Lowモード**: 64サンプル（約1.5ms @44.1kHz）
2. **Balancedモード**: 128サンプル（約3ms @44.1kHz）- デフォルト
3. **Relaxedモード**: 256サンプル（約6ms @44.1kHz）

### 理由

#### 複数モード採用の理由
1. **用途の多様性に対応**
   - リアルタイム演奏: 低レイテンシー必須
   - ミックス作業: CPU効率を優先
   - モバイル環境: バッテリー消費を考慮

2. **FM音源の特性を活用**
   - 処理自体が軽量で低レイテンシー実現可能
   - エフェクト処理がないためバッファサイズを小さくできる

3. **ユーザー環境の多様性**
   - 高性能なスタジオ環境から一般的なラップトップまで対応
   - DAWのバッファ設定との協調

### 実装詳細

```cpp
class LatencyConfiguration {
public:
    enum class Mode {
        UltraLow,    // 64 samples - リアルタイム演奏向け
        Balanced,    // 128 samples - 標準設定
        Relaxed      // 256 samples - CPU節約
    };
    
    struct Settings {
        int bufferSize;
        float cpuTarget;    // 目標CPU使用率
        bool autoSwitch;    // 自動切り替え有効
    };
    
    // サンプルレートに応じた実際のレイテンシー計算
    float getLatencyMs(int sampleRate) const {
        return (float)currentBufferSize / sampleRate * 1000.0f;
    }
};
```

#### インテリジェント自動切り替え
```cpp
// リアルタイムMIDI入力検出時の動的切り替え
void adaptToRealtimeInput() {
    if (hasRealtimeMidiInput && currentMode != Mode::UltraLow) {
        temporarilySwitchToLowLatency();
    }
}
```

#### DAWへの遅延通知
```cpp
int getLatencySamples() const override {
    // Plugin Delay Compensation (PDC) のための正確な値を返す
    return currentBufferSize;
}
```

### UI実装
- 設定画面でモード選択を提供
- 現在のレイテンシーをms単位で表示
- CPU使用率のリアルタイム表示

### パフォーマンス目標
| モード | バッファサイズ | レイテンシー | CPU使用率（目安） |
|--------|---------------|-------------|-----------------|
| Ultra Low | 64 | 約1.5ms | 15-25% |
| Balanced | 128 | 約3ms | 10-15% |
| Relaxed | 256 | 約6ms | 5-10% |

*CPU使用率は4コア環境での目安

### リスクと対策
- **Ultra Lowモードでのクリック/ポップノイズ**
  → 十分なCPUヘッドルームの確保と警告表示
- **自動切り替えによる音の途切れ**
  → クロスフェード処理の実装

### 将来の拡張
- AIによるCPU負荷予測と先行的なモード切り替え
- DAWとの協調によるバッファサイズ最適化
- チップごとの個別レイテンシー設定

---

## ADR-009: スレッディングモデルとリアルタイム処理

**ステータス**: 承認済み  
**日付**: 2025-01-06  
**決定者**: 開発チーム  

### コンテキスト
Audio Unitプラグインでは、CoreAudioのリアルタイムオーディオ処理とJUCEフレームワーク、UI更新、バックグラウンドタスクなど、複数のスレッドが協調動作する必要がある。特にオーディオスレッドはリアルタイム制約があり、不適切な実装は音切れやクリックノイズの原因となる。

関係するスレッド：
1. **Audio Thread** (CoreAudio/高優先度リアルタイム)
2. **Main Thread** (UI更新、ユーザー入力)
3. **Background Threads** (ファイルI/O、重い処理)

### 決定
**ロックフリー通信を基本とした3層スレッドアーキテクチャを採用する**

### 理由

#### 採用理由
1. **リアルタイム制約の遵守**
   - オーディオスレッドでのブロッキング回避
   - 予測可能なレイテンシーの実現

2. **スムーズなUI更新**
   - オーディオ処理を妨げないUI更新
   - 応答性の高いユーザー体験

3. **JUCEとの親和性**
   - JUCEの推奨パターンに準拠
   - 既存のユーティリティクラスを活用

### 実装設計

#### 1. スレッド間通信アーキテクチャ
```cpp
class ThreadingArchitecture {
    // オーディオスレッド専用データ（他スレッドからアクセス禁止）
    struct AudioThreadData {
        std::unique_ptr<ymfm::ym2151> opm;
        std::unique_ptr<ymfm::ym2608> opna;
        VoiceAllocator voiceAllocator;
        float* workBuffer;  // 事前確保
    };
    
    // ロックフリー通信チャネル
    struct Communication {
        // パラメータ更新（UI→Audio）
        moodycamel::ReaderWriterQueue<ParameterUpdate> paramQueue;
        
        // メーター情報（Audio→UI）
        std::atomic<float> peakLevel[2];
        std::atomic<float> rmsLevel[2];
        
        // S98録音データ（Audio→Background）
        juce::AbstractFifo s98Fifo{65536};
        std::vector<S98Command> s98Buffer;
    };
};
```

#### 2. パラメータ同期戦略
```cpp
class ParameterSynchronization {
    // ダブルバッファリング方式
    struct ParameterSet {
        std::array<float, 1024> values;
        std::atomic<bool> dirty{false};
    };
    
    ParameterSet paramSetA, paramSetB;
    std::atomic<ParameterSet*> activeSet{&paramSetA};
    
    // UIスレッドから
    void updateParameter(int id, float value) {
        auto* inactiveSet = (activeSet.load() == &paramSetA) ? &paramSetB : &paramSetA;
        inactiveSet->values[id] = value;
        inactiveSet->dirty = true;
    }
    
    // オーディオスレッドから（processBlockの開始時）
    void syncParameters() {
        auto* inactiveSet = (activeSet.load() == &paramSetA) ? &paramSetB : &paramSetA;
        if (inactiveSet->dirty.exchange(false)) {
            activeSet.store(inactiveSet);
        }
    }
};
```

#### 3. UI更新タイミング
```cpp
class UIUpdateStrategy : private juce::Timer {
    void startTimerHz(int fps) {
        // 30-60 FPSでUI更新
        juce::Timer::startTimerHz(fps);
    }
    
    void timerCallback() override {
        // アトミック変数から値を取得
        updateLevelMeters();
        
        // エンベロープ表示更新
        if (envelopeDataReady.exchange(false)) {
            envelopeDisplay.repaint();
        }
    }
};
```

#### 4. S98録音の非同期処理
```cpp
class S98AsyncRecording {
    std::thread writerThread;
    std::atomic<bool> isRecording{false};
    
    // オーディオスレッドから（ロックフリー）
    void logCommand(uint8_t chip, uint8_t addr, uint8_t data) {
        S98Command cmd{getCurrentTimestamp(), chip, addr, data};
        
        auto scope = s98Fifo.write(1);
        if (scope.blockSize1 > 0) {
            s98Buffer[scope.startIndex1] = cmd;
        }
    }
    
    // バックグラウンドスレッド
    void writerThreadLoop() {
        while (isRecording.load()) {
            auto scope = s98Fifo.read(s98Fifo.getNumReady());
            
            // ファイルへの書き込み（ブロッキングOK）
            for (int i = 0; i < scope.blockSize1; ++i) {
                writeToFile(s98Buffer[scope.startIndex1 + i]);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};
```

### オーディオスレッドの禁止事項

以下の操作はオーディオスレッドで絶対に行わない：
- メモリの動的確保（malloc, new）
- Mutexやセマフォによるロック
- ファイルI/O操作
- Objective-Cメッセージ送信（重いもの）
- 例外のスロー
- 仮想関数の過度な使用

### パフォーマンス目標

- オーディオスレッドのCPU使用率: 最大40%（1コア）
- パラメータ更新レイテンシー: < 1ms
- UI更新頻度: 30-60 FPS
- S98録音オーバーヘッド: < 5% CPU

### リスクと対策

1. **優先度逆転**
   - 対策: リアルタイムスレッドからの非RTスレッドへの依存を排除

2. **キューオーバーフロー**
   - 対策: 適切なキューサイズとオーバーフロー時の graceful degradation

3. **メモリ順序の問題**
   - 対策: 適切なmemory orderingの指定（memory_order_acquire/release）

### テスト戦略

- Thread Sanitizerによるデータ競合検出
- DTraceによるリアルタイム違反の検出
- 高負荷時のグリッチ測定