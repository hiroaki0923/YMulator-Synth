# YMulator-Synth Quick パネル設計仕様

**ステータス**: 実装済み（0.1.0〜0.1.2）。本書は設計の根拠と、それを実装しているクラスの対応を記す。
**関連**: [ADR-010](ymulatorsynth-adr.md#adr-010-quickdetail-2モード-ui-とマクロの相対写像方式)、[アーキテクチャ](ymulatorsynth-architecture.md)、[技術仕様書](ymulatorsynth-technical-spec.md)、[Motion 設計](ymulatorsynth-motion-design.md)

## 0. 目的と前提

生パラメータ（4 オペレータ × 11 + アルゴリズム/FB/LFO/ノイズ）を直接いじらなくても FM らしい音色を作れるように、既存パラメータの上に「意味のある抽象化」の層を被せる。UI は Quick（マクロ＋ジェネレータ）と Detail（生パラメータ）の 2 ビューで、どちらも同じ `AudioProcessorValueTreeState` を別の顔で見せる。ビューを切り替えても音は変わらない。

### 0.1 オペレータ番号と役割

オペレータは VOPM 順で番号付けする。ymfm のアルゴリズム定義の op1〜op4 と同じ順であり、レジスタスロットとの対応は `YM2151Regs::OPERATOR_SLOT_OFFSET` が担う（[YM2151 レジスタの事実](ym2151-register-facts.md) 参照）。

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

この表は `src/dsp/AlgorithmInfo.h` の `kAlgorithms`（constexpr）に 1 か所で定義し、`MacroMapper`、`PatchGenerator`、ToneStrip / QuickView の説明文が共有する。`AlgorithmInfoTest` が ymfm のビットパターンと照合する。図は `AlgorithmDisplay` が `resources/algorithms/algorithmN.svg` と、FB > 0 のとき差し替える `algorithmN_fb.svg`（`tools/gen_algorithm_svg.py` で生成、BinaryData として同梱）を描く。

## 1. 三層構成

```
Layer 3  Motion（時間変化の表現。ymulatorsynth-motion-design.md）
Layer 2  Quick: マクロ 6 個 ＋ ジェネレータ ＋ A/B・Undo   ← 本書
Layer 1  Raw: 既存 APVTS パラメータ                      ← Detail が直接編集
```

Layer 2 は Layer 1 への写像で、音源側（ymfm、ParameterManager の更新経路）には手を入れない。

## 2. マクロ仕様

### 2.1 共通原則

- マクロは **プリセット値からの相対オフセット** として働く。中央（0）はプリセットそのまま。表示は −50〜+50、内部値 m ∈ [−1, +1]。プリセットの個性を保ったまま方向だけ変えるのが目的で、絶対値への写像は採らない（[ADR-010](ymulatorsynth-adr.md#adr-010-quickdetail-2モード-ui-とマクロの相対写像方式) 参照）。
- マクロは 6 個: Brightness、Harmonics、Attack、Decay、Release、Spread。Harmonics だけ選択式（先頭に「Preset」を持つ）。
- Feedback はマクロではない。生パラメータ `feedback`（`ParamID::Global::Feedback`） そのもの（0〜7）で、Quick では ALGORITHM カードの中、Detail では ALG コンボの隣に置く。アルゴリズムと同じチャンネルレジスタ（0x20+ch）の一部であり、結線図と一緒に見せるほうが分かりやすいため。
- 対象パラメータの集合はアルゴリズムで決まる（0.2 の役割表）。
- 各マクロは対象パラメータの **アンカー値**（2.3）に対して式を適用し、結果を clamp して生パラメータに書く。

### 2.2 写像表

| マクロ | 対象 | 式 | 備考 |
|---|---|---|---|
| Brightness | モジュレータの TL | TL' = clamp(TL_a − round(m·40), 0, 127) | 40 ステップ = 約 30 dB。ALG 7（モジュレータなし）では FB' = clamp(FB_a + round(m·7), 0, 7) に代替 |
| Harmonics | モジュレータの MUL（Sub はキャリア、Octave は全 op） | 2.2.1 のテンプレート | 選択式 9 段。Preset = アンカー値に戻す |
| Attack | キャリアの AR | AR' = clamp(AR_a − round(m·12), 0, 31) | m > 0 で遅く（ADSR の「Attack time」と同じ向き） |
| Decay | キャリアの D1R, D2R | D1R' = clamp(D1R_a − round(m·10), 0, 31)、D2R' = clamp(D2R_a − round(m·6), 0, 31) | D1L は変えない |
| Release | キャリアの RR | RR' = clamp(RR_a − round(m·6), 0, 15) | |
| Spread | Op1〜3 の DT1 | \|dt\|' = clamp(\|dt_a\| + round(m·3), 0, 3)、符号はアンカーの符号。アンカーが 0 なら Op1: +, Op2: −, Op3: + | Op4 (C2) は音程の基準として固定。DT1 の符号化は 0〜3 = 0,+1,+2,+3、4〜7 = 0,−1,−2,−3 |

エンベロープ系 3 つ（Attack / Decay / Release）は **キャリアだけ** を動かす。音量エンベロープを操作するのが目的で、モジュレータのエンベロープは音色エンベロープなのでパッチのまま残す。かつて Decay はモジュレータも半分動かしていたが、伸ばした尾が本来より明るく残るので廃止した。

係数は `src/core/MacroMapper.cpp` 冒頭の定数（`kBrightnessTlSteps` など）。

#### 2.2.1 Harmonics テンプレート

モジュレータの MUL を、それが変調する先（0.2 の変調エッジの target）の MUL に対する比 r で決める。直列（ALG 0）は各段が次段に対する比で、Op4 側から順に決めるので更新後の target を使う。複数のモジュレータが同じキャリアに入る場合（ALG 1, 3）は同じ r を使う。

| 位置 | 名称 | r | 意図 |
|---|---|---|---|
| 0 | Preset | — | アンカー値に戻す |
| 1 | Saw | 1 | 1:1。ノコギリ波に近い倍音列 |
| 2 | Square | 2 | 2:1。矩形波に近い奇数倍音 |
| 3 | Pulse | 3 | 3:1。細いパルス |
| 4 | Bright | 4 | 4:1。明るく硬い |
| 5 | Bell | 3.5 | 非整数比。金属的な鐘 |
| 6 | Metal | 5.5 | 非整数比＋高次。金属音 |
| 7 | Sub | — | キャリアの MUL を半分（整数除算。MUL 1 → 0 = ×0.5）。モジュレータはそのまま |
| 8 | Octave | — | 全 op の MUL を 2 倍（MUL 0 は 1 に、上限 15） |

MUL' = clamp(round(MUL_target × r), 0, 15)。MUL_target が 0（×0.5）のときは 0.5 として計算する。ALG 7 にはエッジが無いので Sub / Octave 以外は何もしない。

### 2.3 アンカーと再基準化

- **アンカー** = マクロの基準となる生パラメータのスナップショット（4 op × TL, AR, D1R, D2R, RR, DT1, MUL ＋ FB。`ymulatorsynth::RawPatch`）。
- アンカーを取り直すタイミング（`MacroMapper::captureAnchor`）: プリセット読込（`StateManager`）、ジェネレータの生成（`PatchWorkspace::applyPatch`）、プリセット保存（`PluginProcessor`）。いずれもマクロは中央（Harmonics は Preset）に戻す。Undo / A/B の復元はスナップショットに入っていたアンカーをそのまま戻す（`setAnchor`）。
- **Detail で生パラメータを直接編集したとき**: そのパラメータ p について、マクロ値は変えずにアンカーを再基準化する。`anchor_p' = anchor_p + (raw_new − raw_before)`。clamp によってずれが出る場合は raw_new を優先する（次回マクロ操作時に再計算されるので、ずれは 1 ステップ以内）。
- **アルゴリズム変更時**: 生パラメータは動かさない（Detail でアルゴリズムを回したときに他のノブが跳ねないようにする）。新しい役割集合は次にマクロを動かしたときから効く。
- **ホストへの申告**: マクロ 6 パラメータは他パラメータを書き換えるため meta パラメータとして登録する（`withMeta(true)`。auval の「Parameter values are different since last set」検査の要件）。
- **マクロ操作時**: MacroMapper が対象 raw を `setValueNotifyingHost` で書く。この間は再基準化を抑止するガード（`applying`）を立てる。プリセット読込やスナップショット復元のような一括書き込みの間は `setSuspended(true)` で写像と再基準化の両方を止める。
- **永続化**: マクロ 6 値は APVTS パラメータ（ホストのオートメーション可、ID は `ParamID::Macro::*`）。アンカーは `parameters.state` の子ノード `macroAnchor`（`MacroMapper::anchorNodeType`）に保存し、`StateManager::getStateInformation` の既存経路でホストのプロジェクトに残す。復元時に `macroAnchor` が無ければ、現在の raw をアンカーにしてマクロを中央にする（旧バージョンとの互換）。
- **プリセット保存（.opm / ユーザーバンク）**: 生パラメータを保存する。マクロは保存しない。保存後はアンカーを取り直す。

### 2.4 Quick と Detail の同期

- 単一の APVTS を両ビューが参照する。Quick ノブ → MacroMapper → raw params → ParameterManager → ymfm。Detail ノブ → raw params → 再基準化。
- **ハイライト**: `MacroMapper::targetsOf(macro, algorithm)` が対象パラメータ ID の一覧を返す。Detail の TONE 行でマクロに触れている間（ドラッグ中とホバー）、`ToneStrip::onMacroFocus` → `MainComponent::setMacroFocus` が OperatorPanel と ToneStrip の対象ノブに輪を描く。`MainComponentTest.MacroFocusHighlightsExactlyItsTargets` が `targetsOf()` との一致を確認する。
- **編集状態**: raw がアンカーから外れているか、マクロが中央から外れていれば編集済み（`MacroMapper::isEdited`。既存の isCustomMode に反映）。

### 2.5 スレッド

すべてメッセージスレッドで完結する。オーディオスレッドは既存の `ParameterManager` の更新経路のみで、そこはパラメータごとに前回書いた値を覚え、変わったレジスタだけを書く（`lastOpValues` / `lastGlobalValues`、`RegisterUpdateTest`）。マクロ 1 回の操作で最大 12 パラメータが同時に動くため、この差分送信が前提になる。

## 3. ジェネレータ仕様

### 3.1 入力

- カテゴリ: Bass / Lead / Brass / E.Piano / Bell / Pad / SE / Any
- 方向スライダー 6 本（各 0〜1）: Dark↔Bright、Simple↔Complex、Soft↔Hard attack、Short↔Long、Still↔Moving、Harmonic↔Metallic
- 乱数シード（`juce::Random`。テスト用に固定可能）

`ymulatorsynth::GeneratorInput`（`src/core/PatchGenerator.h`）。UI での選択は `parameters.state` の子ノード `generator` に残り、エディタを開き直しても保たれる。

### 3.2 生成手順

1. カテゴリから ALG 候補集合と EG の形状レンジを引く（3.3）。
2. Complex で ALG を選ぶ（直列ほど Complex 側）。FB は Complex に比例（0〜7、カテゴリごとの下限あり）。
3. Bright でモジュレータ TL のレンジを決める（Dark: 60〜100、Bright: 10〜45）。キャリア TL は 0〜12。
4. Hard attack / Length で AR, D1R, D1L, RR を決める（キャリアとモジュレータで別レンジ。モジュレータの D1R を速くすると「アタックだけ明るい」定番の形になる）。
5. Moving で LFO（PMD/AMD/Rate）、AMS-EN、D2R を決める。
6. Metallic でモジュレータの MUL を非整数比（DT2 も使用）に寄せる。Harmonic 側では 2.2.1 の Saw/Square/Pulse から選ぶ。
7. DT1 は Op1〜3 に軽く散らす（0〜±2）。
8. 生成した raw を書き、アンカーを取り直し、マクロを中央にする（`PatchWorkspace::applyPatch`）。

### 3.3 カテゴリ表

値は `src/core/PatchGenerator.cpp` の `kRules`。試聴で調整する。

| カテゴリ | ALG 候補 | キャリア EG | モジュレータ EG | キャリア MUL | 備考 |
|---|---|---|---|---|---|
| Bass | 0, 1, 4, 6 | AR 25〜31, D1R 6〜14, D1L 2〜6, RR 6〜10 | D1R 10〜20 | 0〜1 | Sub 傾向 |
| Lead | 0, 2, 3, 4 | AR 20〜31, D1R 0〜6, D1L 0〜2, RR 4〜8 | D1R 4〜12 | 1〜2 | FB 4 以上 |
| Brass | 1, 3, 4 | AR 12〜20, D1R 4〜10, D1L 1〜3, RR 5〜8 | AR 12〜20, D1R 4〜12 | 1 | 立ち上がりを揃える |
| E.Piano | 3, 4, 5 | AR 28〜31, D1R 6〜12, D1L 4〜8, RR 6〜9 | D1R 10〜18 | 1〜2 | Bell との中間。PMD 控えめ |
| Bell | 4, 5, 7 | AR 31, D1R 2〜6, D1L 6〜10, RR 3〜6 | AR 28〜31, D1R 2〜8 | 1〜2 | Metallic 0.6 以上（非整数 MUL、DT2）。長い減衰 |
| Pad | 5, 6, 7 | AR 6〜12, D1R 0〜4, D1L 0〜1, RR 3〜6 | AR 6〜12, D1R 0〜6 | 1〜2 | LFO を薄く |
| SE | 0, 1, 2 | 任意 | 任意 | 0〜15 | FB 7、3 割でノイズ有効 |
| Any | 0〜7 | AR 4〜31, D1R 0〜20, D1L 0〜12, RR 2〜12 | D1R 0〜24 | 0〜4 | 制約ほぼなし |

### 3.4 Undo と A/B

`PatchWorkspace`（適用と復元）と `SnapshotStore`（純データ）で実装する。

- スナップショット `PatchSnapshot` = 音に関わる全パラメータの正規化値（ピッチベンド幅、Expressive MIDI、チャンネル pan を除く。Motion パラメータは含む）＋ アンカー ＋ 編集済みフラグ。
- Undo スタック: 深さ 16（`SnapshotStore::kMaxUndo`）。積むタイミングは、生成の直前と、TONE ノブ（6 マクロ ＋ Feedback）のジェスチャ開始時。
- A/B: 2 スロット。Generate は直前の音を A、生成結果を B に入れて B を選ぶ。A / B ボタンは現在の音を今のスロットに退避してから相手を復元する。復元はアンカーも戻す。

### 3.5 テスト

`tests/unit/PatchGeneratorTest.cpp`、`PatchWorkspaceTest.cpp`。

- 同じシードで同じ結果（決定性）。
- 生成結果は必ず可聴（少なくとも 1 つのキャリアで TL ≤ 40、AR ≥ 6）で、全値がレジスタ範囲内。
- ALG がカテゴリの候補集合に入る。
- Bright 1.0 の平均モジュレータ TL は Bright 0.0 より小さい（単調性）。Moving が高いと LFO が入る。
- 生成後にアンカーが取り直され、Undo は生成前の値に完全一致で戻り、A/B は前後を行き来し、Undo の深さは 16 で頭打ち。

## 4. コンポーネント構成

### 4.1 core / dsp

| ファイル | 責務 |
|---|---|
| `src/dsp/AlgorithmInfo.h` | 0.2 の役割表・結線・説明文。constexpr テーブル `kAlgorithms` と `algorithmInfo(alg)` |
| `src/core/MacroMapper.{h,cpp}` | 2.2 の写像（静的な `apply()`、純関数）＋ アンカー管理 ＋ `targetsOf()`。APVTS のリスナーとしてマクロ変更を raw に書き、raw の直接編集で再基準化する |
| `src/core/PatchGenerator.{h,cpp}` | 3 章。`generate(input, seed)` は純関数 |
| `src/core/PatchWorkspace.{h,cpp}` | 生成結果とスナップショットの適用・復元、Undo 点の採取、A/B、再アンカー |
| `src/core/SnapshotStore.{h,cpp}` | Undo スタック（16）と A/B スロットの入れ物 |
| `src/core/PatchPreview.{h,cpp}` | 現在のパッチを専用チップ（48 kHz）で C4 1 音だけ鳴らしてモノラルにする。OUTPUT カードの波形用 |
| `src/utils/ParameterIDs.h` | `ParamID::Macro`（`macro_brightness`, `macro_harmonics`, `macro_attack`, `macro_decay`, `macro_release`, `macro_spread`）。Feedback は既存 `Global::Feedback` |

### 4.2 ui

| コンポーネント | 内容 |
|---|---|
| `MainComponent` | エディタのルート。1000×640。ヘッダに YMULATOR ロゴ、Quick / Detail ボタン、`PresetUIManager`（バンクとプリセットのコンボ、Save）。パンの操作はヘッダに無い。選んだビューは `parameters.state` の `uiViewMode` に残す。アルゴリズム・FB・ノイズの変化を聞いてオペレータの役割タグを更新し、マクロのハイライトを配る |
| `QuickView` | 左列: RECIPE カード（`GeneratorPanel` = カテゴリチップと方向スライダー 6 本。右下に Undo / A / B / Generate）と、その下の TONE 行（Large ノブ 6 個。副題に対象を書く: Modulator level / Ratio template / Carrier AR / Carrier D1R・D2R / Carrier RR / DT1 spread）。右列: ALGORITHM カード（`AlgorithmDisplay` の図、結線の表記、説明文、‹ › でアルゴリズムを前後、Feedback ノブ）、MOTION カード（`MotionPanel`。[Motion 設計 §4](ymulatorsynth-motion-design.md#4-ui)）、OUTPUT カード（`OutputScope`。`PatchPreview` で 0.5 秒保持＋0.5 秒リリースを描画）。フッタに DETAIL ▸ リンク、音色の要約、ボイス数とサンプルレート |
| Detail（`MainComponent` 直下） | `ToneStrip`（Small ノブ 6 個 Bright / Harm / Atk / Dec / Rel / Sprd、ヒント文、`AlgorithmDisplay`、ALG コンボ、FB ノブ）、`OperatorPanel` × 4（役割タグ MODULATOR / CARRIER / NOISE、主ノブ Level / Ratio / Detune、`EnvelopeDisplay`、EG 5 ノブ AR / D1R / D1L / D2R / RR、KS / DT2 / AMS）、`MotionStrip`（LFO / ENVELOPE / SPACE / PLAY の 4 カード）、`LfoNoiseStrip`（ハードウェア LFO とノイズ、ステータス行） |
| 共有 | `RotaryKnob`（Large / Primary / Small / Tiny。ドラッグ、Shift で微調整、ホイール、ダブルクリックで生値を入力する `TextEditor`、ハイライト輪、表示用フォーマッタと副題）、`KnobBinding`（APVTS との接続）、`AlgorithmDisplay`（同梱 SVG。FB > 0 で強調版に切替）、`EnvelopeDisplay`、`UiTheme` / `YmLookAndFeel` |

表示の約束: Level は 100 = 最大（TL 0）、Ratio は ×0.5〜×15、Detune は ±3。副表示に TL / MUL / DT1 の生値。キャリアは青、モジュレータは桃、EG は緑、その他は青。

## 5. 実装順序（完了）

0. 前提: オペレータのスロット順修正、`ParameterManager` の差分送信、`AlgorithmInfo.h` の抽出と AlgorithmDisplay の置き換え（済）
1. `ParamID::Macro`、`MacroMapper` と単体テスト、アンカーの永続化（済、0.1.0）
2. Detail ビューの再構成（役割タグ、主従ノブ、TONE 行、ハイライト）（済、0.1.0）
3. Quick ビュー: TONE 行と ALGORITHM カード（済、0.1.0）
4. `PatchGenerator`、`GeneratorPanel`、`PatchWorkspace` / `SnapshotStore`（Undo / A/B）（済、0.1.0）
5. `OutputScope` と `PatchPreview`（済、0.1.0）
6. Feedback をマクロ行から ALGORITHM カード / ALG コンボの隣へ移動、ヘッダのパン操作を廃止（0.1.2）

## 6. テスト

- **MacroMapper**（`tests/unit/MacroMapperTest.cpp`）: 全 ALG で中央は恒等（`EXPECT_EQ`）、Brightness の単調性とモジュレータ限定、ALG 7 での FB 代替、`targetsOf()` が役割表と一致、Harmonics テンプレート（2 系統と直列）、エンベロープ 3 つがキャリアのみ、Spread の符号維持と Op4 固定、DT1 符号化の往復。プロセッサ経由: マクロ移動 → 中央で完全復元、直接編集の再基準化と往復、アルゴリズム変更は次のマクロ操作から、プリセット読込でマクロとアンカーがリセット、状態の往復でアンカーとマクロが一致、`macroAnchor` の無い旧状態からの復元。
- **AlgorithmInfo**（`AlgorithmInfoTest.cpp`）: エッジとキャリアが ymfm の符号化と一致、モジュレータは必ず 1 つの target を持つ。
- **ジェネレータ / ワークスペース / プレビュー**: 3.5 と `PatchPreviewTest.cpp`（決定性、C4 の周期、キーオフ後のリリース）。
- **UI**（`tests/ui/MainComponentTest.cpp`, `RotaryKnobTest.cpp`）: マクロのフォーカスが `targetsOf()` と同じノブを光らせる、ビュー切替と永続化、‹ › でアルゴリズムが進む、Generate → A/B で戻る、ダブルクリック入力が 1 ジェスチャで反映され範囲とステップに丸まる。
