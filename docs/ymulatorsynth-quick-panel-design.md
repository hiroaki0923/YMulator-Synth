# YMulator-Synth Quick パネル設計仕様

**ステータス**: ドラフト（2026-09-05）
**関連**: [ADR-010](ymulatorsynth-adr.md#adr-010-quickdetail-2モード-ui-とマクロの相対写像方式)、[技術仕様書](ymulatorsynth-technical-spec.md)、[JUCE 実装詳細](ymulatorsynth-juce-implementation-details.md)

## 0. 目的と前提

生パラメータ（4 オペレータ × 11 + アルゴリズム/FB/LFO/ノイズ）を直接いじらなくても FM らしい音色を作れるように、既存パラメータの上に「意味のある抽象化」の層を被せる。UI は Quick（マクロ＋ジェネレータ）と Detail（生パラメータ）の 2 モードで、どちらも同じ `AudioProcessorValueTreeState` を別の顔で見せる。モードを切り替えても音は変わらない。

### 0.1 オペレータ番号と役割

オペレータは VOPM 順で番号付けする。ymfm のアルゴリズム定義の op1〜op4 と同じ順であり、レジスタスロットとの対応は `YM2151Regs::OPERATOR_SLOT_OFFSET` が担う（修正コミット参照）。

| Op | 名称 | レジスタ | 役割 |
|---|---|---|---|
| 1 | M1 | +0 | ALG 7 以外ではモジュレータ |
| 2 | C1 | +16 | ALG 4〜7 でキャリア |
| 3 | M2 | +8 | ALG 5〜7 でキャリア |
| 4 | C2 | +24 | 常にキャリア |

### 0.2 アルゴリズム別の結線と役割（ymfm `s_algorithm_ops` より）

| ALG | 結線 | キャリア | モジュレータ | 変調エッジ (mod→target) | 一言 |
|---|---|---|---|---|---|
| 0 | 1→2→3→4 | 4 | 1, 2, 3 | 1→2, 2→3, 3→4 | 4 段直列。最も鋭い。ベース、リード、金属音 |
| 1 | (1+2)→3→4 | 4 | 1, 2, 3 | 1→3, 2→3, 3→4 | 2 入力の直列。ブラス、太いリード |
| 2 | (1+(2→3))→4 | 4 | 1, 2, 3 | 1→4, 2→3, 3→4 | 芯と飾りを分けた直列。クラビ、ギター |
| 3 | ((1→2)+3)→4 | 4 | 1, 2, 3 | 1→2, 2→4, 3→4 | 2 段＋1 段を 1 キャリアに。EP、ハープシコード |
| 4 | (1→2)+(3→4) | 2, 4 | 1, 3 | 1→2, 3→4 | 2 系統の 2op。最も汎用。EP、ベル、ギター、ブラス |
| 5 | 1→(2+3+4) | 2, 3, 4 | 1 | 1→2, 1→3, 1→4 | 1 モジュレータ 3 キャリア。オルガン、ストリングス、パッド |
| 6 | (1→2)+3+4 | 2, 3, 4 | 1 | 1→2 | 2op ＋サイン 2 本。ベース＋サブ、オルガン |
| 7 | 1+2+3+4 | 1, 2, 3, 4 | なし | なし | 加算合成。オルガン、フルート、純音 |

この表は `src/dsp/AlgorithmInfo.h` に 1 か所で定義し、MacroMapper、AlgorithmDisplay、PatchGenerator が共有する。現在の `AlgorithmDisplay.cpp` 内の結線表は ymfm と一致していないため、AlgorithmInfo からの再生成に置き換える。

## 1. 三層構成

```
Layer 3  Driver FX（演奏表現。本仕様の対象外、別途設計）
Layer 2  Quick: マクロ 7 個 ＋ ジェネレータ ＋ A/B・Undo   ← 本仕様
Layer 1  Raw: 既存 APVTS パラメータ                      ← Detail が直接編集
```

Layer 2 は Layer 1 への写像で、音源側（ymfm、ParameterManager の更新経路）には手を入れない。

## 2. マクロ仕様

### 2.1 共通原則

- マクロは **プリセット値からの相対オフセット** として働く。中央（0）はプリセットそのまま。表示は −50〜+50、内部値 m ∈ [−1, +1]。プリセットの個性を保ったまま方向だけ変えるのが目的で、絶対値への写像は採らない（[ADR-010](ymulatorsynth-adr.md) 参照）。
- 例外は 2 つ。Harmonics は選択式（先頭に「Preset」を持つ）で、Feedback は生パラメータそのもの（オフセットではなく FB 0〜7 を直接表示）。
- 対象パラメータの集合はアルゴリズムで決まる（0.2 の役割表）。
- 各マクロは対象パラメータの **アンカー値**（2.3）に対して式を適用し、結果を clamp して生パラメータに書く。

### 2.2 写像表

| マクロ | 対象 | 式 | 備考 |
|---|---|---|---|
| Brightness | モジュレータの TL | TL' = clamp(TL_a − round(m·40), 0, 127) | 40 ステップ = 約 30 dB。ALG 7（モジュレータなし）では FB' = clamp(FB_a + round(m·7), 0, 7) に代替 |
| Harmonics | モジュレータの MUL（Sub/Octave は全 op） | 2.2.1 のテンプレート | 選択式 9 段。Preset = アンカー値に戻す |
| Feedback | FB | FB そのもの | Detail の FB と同一パラメータ |
| Attack | 全 op の AR | AR' = clamp(AR_a − round(m·12), 0, 31) | m > 0 で遅く（ADSR の「Attack time」と同じ向き） |
| Decay | 全 op の D1R, D2R | キャリア: D1R' = clamp(D1R_a − round(m·10)), D2R' = clamp(D2R_a − round(m·6)) / モジュレータ: 変化量を半分 | D1L は変えない |
| Release | 全 op の RR | RR' = clamp(RR_a − round(m·6), 0, 15) | |
| Spread | Op1〜3 の DT1 | \|dt\|' = clamp(\|dt_a\| + round(m·3), 0, 3)、符号はアンカーの符号。アンカーが 0 なら Op1: +, Op2: −, Op3: + | Op4 (C2) は音程の基準として固定。DT1 の符号化は 0〜3 = 0,+1,+2,+3、4〜7 = 0,−1,−2,−3 |

#### 2.2.1 Harmonics テンプレート

モジュレータの MUL を、それが変調する先（0.2 の変調エッジの target）の MUL に対する比 r で決める。直列（ALG 0）は各段が次段に対する比。複数のモジュレータが同じキャリアに入る場合（ALG 1, 3）は同じ r を使う。

| 位置 | 名称 | r | 意図 |
|---|---|---|---|
| 0 | Preset | — | アンカー値に戻す |
| 1 | Saw | 1 | 1:1。ノコギリ波に近い倍音列 |
| 2 | Square | 2 | 2:1。矩形波に近い奇数倍音 |
| 3 | Pulse | 3 | 3:1。細いパルス |
| 4 | Bright | 4 | 4:1。明るく硬い |
| 5 | Bell | 3.5 | 非整数比。金属的な鐘 |
| 6 | Metal | 5.5 | 非整数比＋高次。金属音 |
| 7 | Sub | — | キャリアの MUL を半分（MUL 0 = ×0.5 を利用）。モジュレータはそのまま |
| 8 | Octave | — | 全 op の MUL を 2 倍（clamp 15） |

MUL' = clamp(round(MUL_target × r), 0, 15)。MUL_target が 0（×0.5）のときは 0.5 として計算する。ALG 7 では Sub / Octave 以外は無効。

### 2.3 アンカーと再基準化

- **アンカー** = マクロの基準となる生パラメータのスナップショット（4 op × TL, AR, D1R, D2R, RR, DT1, MUL ＋ FB）。
- アンカーを取り直すタイミング: プリセット読込、ジェネレータの生成、A/B の復元、プリセット保存。いずれもマクロは中央（Harmonics は Preset）にリセットする。
- **Detail で生パラメータを直接編集したとき**: そのパラメータ p について、マクロ値は変えずにアンカーを再基準化する。`anchor_p' = anchor_p + (raw_new − raw_before)`。clamp によってずれが出る場合は raw_new を優先する（次回マクロ操作時に再計算されるので、ずれは 1 ステップ以内）。
- **マクロ操作時**: MacroMapper が対象 raw を `setValueNotifyingHost` で書く。この間は再基準化を抑止するガードを立てる（ParameterManager の既存 `s_isProcessingParameterChange` と同型）。
- **永続化**: マクロ 7 値は APVTS パラメータ（DAW オートメーション可、ID は `ParamID::Macro::*`）。アンカーは `parameters.state` の子ノード `macroAnchor` に保存し、`StateManager::getStateInformation` の既存経路で DAW プロジェクトに残す。復元時に `macroAnchor` が無ければ、現在の raw をアンカーにしてマクロを中央にする（旧バージョンとの互換）。
- **プリセット保存（.opm / ユーザーバンク）**: 生パラメータを保存する。マクロは保存しない。保存後はアンカーを取り直す。

### 2.4 Quick と Detail の同期

- 単一の APVTS を両モードが参照する。Quick ノブ → MacroMapper → raw params → ParameterManager → ymfm。Detail ノブ → raw params → 再基準化。
- **ハイライト**: `MacroMapper::targetsOf(macro, algorithm)` が対象パラメータ ID の一覧を返す。Detail の TONE 行でマクロに触れている間（gesture 中とホバー）、対象ノブに琥珀の輪を描く。ジェネレータ実行直後は変化した全ノブを 1 秒間ハイライトする。
- **編集状態**: raw がアンカーから外れているか、マクロが中央から外れていれば EDITED を表示する（既存の isCustomMode を流用）。

### 2.5 スレッド

すべてメッセージスレッドで完結する。オーディオスレッドは既存の `updateYmfmParameters` 経路のみ。前提作業として、`updateYmfmParameters` の毎ブロック全送信を差分送信に変える（マクロ 1 回の操作で最大 12 パラメータが同時に動くため）。

## 3. ジェネレータ仕様

### 3.1 入力

- カテゴリ: Bass / Lead / Brass / E.Piano / Bell / Pad / SE / Any
- 方向スライダー 6 本（各 0〜1）: Bright↔Dark、Simple↔Complex、Soft↔Hard attack、Short↔Long、Still↔Moving、Harmonic↔Metallic
- 乱数シード（テスト用に固定可能）

### 3.2 生成手順

1. カテゴリから ALG 候補集合と EG の形状レンジを引く（3.3）。
2. Complex で ALG を選ぶ（直列ほど Complex 側）。FB は Complex に比例（0〜7）。
3. Bright でモジュレータ TL のレンジを決める（Dark: 60〜100、Bright: 10〜45）。キャリア TL は 0〜12。
4. Hard attack / Length で AR, D1R, D1L, RR を決める（キャリアとモジュレータで別レンジ。モジュレータの D1R を速くすると「アタックだけ明るい」定番の形になる）。
5. Moving で LFO（PMD/AMD/Rate）、AMS-EN、D2R を決める。
6. Metallic でモジュレータの MUL を非整数比（DT2 も使用）に寄せる。Harmonic 側では 2.2.1 の Saw/Square/Pulse から選ぶ。
7. DT1 は Op1〜3 に軽く散らす（0〜±2）。
8. 生成した raw を書き、アンカーを取り直し、マクロを中央にする。

### 3.3 カテゴリ表（初期案。試聴で調整する）

| カテゴリ | ALG 候補 | キャリア EG | モジュレータ EG | 備考 |
|---|---|---|---|---|
| Bass | 0, 1, 4, 6 | AR 25〜31, D1R 6〜14, D1L 2〜6, RR 6〜10 | D1R 10〜20 | Sub 傾向。MUL 0〜1 のキャリア |
| Lead | 0, 2, 3, 4 | AR 20〜31, D1R 0〜6, D1L 0〜2, RR 4〜8 | D1R 4〜12 | FB 高め |
| Brass | 1, 3, 4 | AR 12〜20, D1R 4〜10, D1L 1〜3, RR 5〜8 | AR 12〜20 | 立ち上がりを揃える |
| E.Piano | 3, 4, 5 | AR 28〜31, D1R 6〜12, D1L 4〜8, RR 6〜9 | D1R 10〜18, MUL 高め | Bell との中間 |
| Bell | 4, 5, 7 | AR 31, D1R 2〜6, D1L 6〜10, RR 3〜6 | 非整数 MUL, DT2 | 長い減衰 |
| Pad | 5, 6, 7 | AR 6〜12, D1R 0〜4, D1L 0〜1, RR 3〜6 | AR 6〜12 | LFO を薄く |
| SE | 0, 1, 2 | 任意 | 任意 | FB 7、ノイズ有効も可 |
| Any | 0〜7 | 全レンジ | 全レンジ | 制約なし |

### 3.4 Undo と A/B

- Undo スタック: 生成前とマクロ操作前の（raw ＋ マクロ ＋ アンカー）を積む。深さ 16。
- A/B: 2 スロット。A に現在を退避、B と切り替え。切り替え時はアンカーも復元する。

### 3.5 テスト

- 同じシードで同じ結果（決定性）。
- 生成結果は必ず可聴（少なくとも 1 つのキャリアで TL ≤ 40、AR ≥ 6）。
- ALG がカテゴリの候補集合に入る。
- Bright 1.0 の平均モジュレータ TL は Bright 0.0 より小さい（単調性）。

## 4. コンポーネント構成

### 4.1 core / dsp

| ファイル | 責務 |
|---|---|
| `src/dsp/AlgorithmInfo.h` | 0.2 の役割表・結線・説明文。constexpr テーブル |
| `src/core/MacroMapper.{h,cpp}` | 2.2 の写像（純関数）＋ アンカー管理 ＋ `targetsOf()`。APVTS へは `MacroMapperInterface` 越しに書く |
| `src/core/PatchGenerator.{h,cpp}` | 3 章。乱数はシード可能な `juce::Random` |
| `src/core/SnapshotStore.{h,cpp}` | Undo スタックと A/B スロット |
| `src/utils/ParameterIDs.h` | `namespace Macro { Brightness = "macro_brightness", Harmonics = "macro_harmonics", Attack = "macro_attack", Decay = "macro_decay", Release = "macro_release", Spread = "macro_spread" }`。Feedback は既存 `Global::Feedback` |

### 4.2 ui

| コンポーネント | 内容 |
|---|---|
| `MainComponent` | ヘッダ（モード切替、バンク/プリセット、EDITED、Save、Pan）、Quick / Detail の切替、ステータス行。1000×640 |
| `QuickView` | `TonePanel`（マクロ 7 ノブ）、`AlgorithmCard`（図＋説明＋前後）、`GeneratorPanel`（カテゴリ、6 スライダー、New sound、Undo）、`ComparePanel`（A/B）、`OutputScope` |
| `DetailView` | `ToneStrip`（マクロの小ノブ＋アルゴリズム）、`OperatorRow` × 4（役割タグ、Level/Ratio/Detune の主ノブ、EnvelopeDisplay、EG 5 ノブ、KS/DT2/AMS）、`LfoNoiseStrip` |
| 共有 | `RotaryKnob`（サイズ 3 種、ハイライト輪、人間向け表示＋生値の副表示）、`RoleTag`、`AlgorithmDisplay`（AlgorithmInfo から描画、役割色）、`EnvelopeDisplay`（既存） |

表示の約束: Level は 100 = 最大（TL 0）、Ratio は ×0.5〜×15、Detune は ±3。副表示に TL / MUL / DT1 の生値。キャリアは青、モジュレータは桃、EG は緑、その他は青。

## 5. 実装順序

0. 前提: オペレータのスロット順修正（済）、`updateYmfmParameters` の差分化、`AlgorithmInfo.h` の抽出と AlgorithmDisplay の置き換え
1. `ParamID::Macro`、`MacroMapper` と単体テスト、アンカーの永続化
2. Detail ビューの再構成（役割タグ、主従ノブ、TONE 行、ハイライト）
3. Quick ビュー: `TonePanel` と `AlgorithmCard`
4. `PatchGenerator`、`GeneratorPanel`、`SnapshotStore`（Undo / A/B）
5. `OutputScope`

各段でビルド、全テスト、auval を通してからコミットする。

## 6. テスト方針

- **MacroMapper**: 中央で無変化（`EXPECT_EQ`）、m の単調性、clamp、ALG 別の対象集合（0.2 の表と一致）、再基準化の往復（raw 編集 → マクロ操作 → 元の raw に戻る、`EXPECT_EQ`）。
- **状態**: アンカーとマクロの save / restore は完全一致。`macroAnchor` が無い旧状態からの復元。
- **音響**: ymfm 経由で Brightness +50 が零交差数を増やし、−50 が減らすことを検証（OperatorSlotOrderTest と同じ手法）。
- **UI**: `targetsOf()` の一覧と Detail のハイライト対象が一致。
