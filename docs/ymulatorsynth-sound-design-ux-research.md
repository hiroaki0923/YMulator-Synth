# YMulator-Synth 音色作成 UX 調査 (2026-09)

**目的**: 「パラメータを直接いじらなくても FM らしい音色を簡単に作れる」UI を設計するため、近年の FM 音源（ハードウェア・ソフトウェア）が採っている抽象化手法を整理する。
**結論**: 各社のアプローチは 5 パターンに分類できる。YMulator-Synth には「Quick パネル（マクロ＋言い換え）」「ガイド付きランダマイザ」「ドライバ FX」の 3 層構成を推奨する。

関連文書: [拡張機能実装ガイド](ymulatorsynth-extension-guide.md)、[開発ステータス](ymulatorsynth-development-status.md)

## 1. 調査対象

| 製品 | 種別 | オペレータ数 | 特徴的な UI |
|---|---|---|---|
| Plogue chipsynth OPS7 | ソフト (DX7 系エミュレーション) | 6 | Dynamic layout、色分け、Randomize、A/B レイヤー、ピッチ EG、アルペジエータ、コーラス |
| Plogue chipsynth MD | ソフト (YM2612 エミュレーション) | 4 | ビット精度エミュ + PSG + PCM、6 倍ポリ |
| Inphonik RYM2612 | ソフト (YM2612 エミュレーション) | 4 | 全コントロールを 1 画面に配置、16 音ポリ、PCM チャンネル |
| Korg opsix | ハード | 6 | Operator Mixer（Ratio ノブ + Level フェーダー）、キャリア赤 / モジュレータ青、Randomize（対象選択可）、オシロ・スペアナ |
| Elektron Digitone II | ハード | 4 | 比率を 0.25 刻みに量子化、オペレータのペア化、HARM（加算合成で倍音付加） |
| NI FM8 | ソフト | 6 | Easy ページ（Harmonic / Detune / Brightness マクロ + Timbre EG）、Morph Square |
| Yamaha reface DX | ハード | 4 | 4 本のタッチスライダーで Algorithm / Frequency / Level / Feedback |
| Yamaha Montage / MODX (FM-X) | ハード | 8 | Smart Morph（自己組織化マップで最大 8 音色を学習し 32×32 グリッドで補間） |
| Synthmata | Web エディタ (Volca FM / DX7) | 6 | 10 本の「意味スライダー」によるガイド付きランダム生成 |
| Ableton Operator | ソフト | 4 | 減算合成の語彙（フィルタ等）と併存させ、プリセットルーティングのみ |
| Arturia DX7 V | ソフト | 6 | 色分けオペレータ、アルゴリズム巡回、エンベロープ編集の改善 |

## 2. 抽象化の 5 パターン

### 2.1 キャリア / モジュレータを見た目で区別し、必要な時だけ深掘りさせる

- OPS7 は通常表示を各オペレータ Level / Coarse / Detune の 3 ノブに絞り、クリックしたオペレータだけ詳細エディタに展開する（dynamic layout）。モジュレータは桃色、キャリアは青。
- OPS7 のアルゴリズム一覧は図ではなく言葉で説明する。例: 「2 層・やや明るい・素直: FM ベース、リード、ブラス、ストリングス、ピアノ」「1 層・非常に明るく歪む: エレキギター、ノイズ系」。
- opsix の Operator Mixer は 6 オペレータそれぞれに Ratio ノブと Level フェーダーを 1 本ずつ。「FM を初めて理解可能かつ操作可能にした」と評価された。

**YMulator-Synth への示唆**: AlgorithmDisplay をキャリア / モジュレータで色分けし、8 アルゴリズムに言葉の説明を付ける。OperatorPanel は「基本 3 ノブ + 展開」に再構成できる。

### 2.2 パラメータを音楽的な言葉に言い換える・量子化する

- Digitone II は周波数比を 0.25 刻みに量子化し、2 オペレータを 1 エンコーダで束ねる。「使えない比率」を最初から出させない。HARM は加算合成で倍音を足し、ウェーブテーブル的に補間する。
- OPS7 マニュアルの「アナログシンセ換算表」:

| FM パラメータ | モジュレータに適用した場合 | キャリアに適用した場合 |
|---|---|---|
| 周波数比 (Coarse) | 波形選択 (1 = ノコギリ、2 = 矩形、5 以上 = リング変調) | バンドパスフィルタ |
| Level | フィルタカットオフ | 音量 |
| Feedback | フィルタレゾナンス | ノコギリ波 |
| Level 最大 + Feedback 最大 | ノイズ | ノコギリ波 |
| Key Scaling | フィルタキーボードトラック | VCA キーボードトラック |
| R1 / R3 / L3 / R4 | フィルタ EG の A / D / S / R | VCA EG の A / D / S / R |
| Detune | PWM | - |

- 比率 1:1 = ノコギリ波、2:1 = 矩形波、3:1 = パルス波、2:3 / 3:2 = 帯域制限された波形、という「比率 → 波形」の対応も図示している。
- Montage FM-X は Spectral Form（Sine / All / Odd / Res）+ Skirt + Resonance という、減算合成に近い語彙でオペレータ波形を指定する。

**YMulator-Synth への示唆**: 既存パラメータへの写像だけで実現できる最も安価な抽象化。TL を「Brightness / Cutoff」、FB を「Resonance」、MUL の組み合わせを「Saw / Square / Pulse / Bell」と表示するだけで理解が変わる。

### 2.3 マクロ層を上に被せる

- FM8 Easy ページ: Harmonic（オペレータ比率をまとめて変更）、Detune、Brightness（変調量をまとめて増減）、Timbre エンベロープ、LFO。「モジュレーションマトリクス数十クリック分を 1 ノブで」。
- reface DX: 4 本のタッチスライダーで「FM プログラミングの頭痛を取り除いた」と評される。
- Digitone: モジュレータレベルを A / B の 2 マクロと X / Y エンベロープに束ねる。

**YMulator-Synth への示唆**: 6〜7 個のマクロノブで十分。候補は Brightness（モジュレータ TL 群）、Harmonics（MUL の音楽的セット）、Attack / Decay / Release（キャリア・モジュレータの EG を連動）、Feedback、Spread（DT1 の散らし）、Velocity Sens。

### 2.4 ガイド付きランダム化と機械学習モーフ

- Synthmata: 完全ランダムでは 9 割が使えない音になるとして、10 本のスライダーで方向付けする。
  - Timbre: Atonality / Complexity / Brightness
  - Envelope: Hardness / Hit / Twang / Longness
  - Movement: Wobble / Wubble / Velocity
- opsix: Randomize の対象を「全体 / オペレータ / アルゴリズム / シーケンス」から選べる。OPS7 にもレイヤー単位の Randomize がある。
- Montage Smart Morph: 最大 8 音色を自己組織化マップで学習し、32×32 グリッドを指でなぞって中間音色を得る。Super Knob に 2 点間の移動を割り当てられる。
- FM8 Morph Square: 4 隅に音色を置いて 2 次元補間。

**YMulator-Synth への示唆**: Synthmata 型スライダー（4〜6 本）+ カテゴリ（Bass / Lead / Bell / EP / Brass / Pad）+ Undo。乱数は既存パラメータ範囲内で完結するので DSP 変更不要。モーフは A/B 2 音色の線形補間から始められる。

### 2.5 チップ外の「演奏表現」を足す

- OPS7: ピッチエンベロープ (R1-R4 / L1-L4)、ディレイ付き LFO、ポルタメント / グリッサンド、モノレガート、アルペジエータ、Dimension D 系コーラス、ステレオディレイ、リバーブ、A/B レイヤーのデチューン、ステップシーケンサによる任意パラメータ変調。
- RYM2612: 16 音ポリ、PCM チャンネル、オーディオ入力。
- チップ再現系プラグインは「正確な再現 + 現代的な利便性」の組み合わせで差別化している。

**YMulator-Synth への示唆**: X68000 / アーケードのドライバ (MXDRV, PMD 等) が行っていた表現、すなわちディレイ付きソフトウェアビブラート、ピッチエンベロープ、ポルタメント、ノート単位デチューン、ユニゾンをプラグイン側で再現する。これが競合との差別化軸になる。未マージの feature/unison-engine-implementation はこの層に合流させる。

## 3. 推奨アーキテクチャ: 3 層構成

```
┌──────────────────────────────────────────────┐
│ Layer 3: Driver FX (演奏表現)                  │  ← 2.5
│   delay vibrato / pitch EG / portamento / unison│
├──────────────────────────────────────────────┤
│ Layer 2: Quick panel (マクロ + 言い換え)        │  ← 2.2 / 2.3
│   Brightness / Harmonics / A-D-R / FB / Spread │
│   + guided randomizer (Layer 2 の上で動く)     │  ← 2.4
├──────────────────────────────────────────────┤
│ Layer 1: Raw parameters (既存 UI, 色分け強化)   │  ← 2.1
│   4 operator × 11 params, algorithm, LFO, noise│
└──────────────────────────────────────────────┘
```

- Layer 2 は Layer 1 への写像のみで、状態を持たない（プリセット互換性を壊さない）。
- ランダマイザは Layer 2 の語彙で乱数を振り、Layer 1 に書き込む。
- Layer 3 は新規パラメータを持ち、.opm には保存できないため独自拡張（プリセット JSON か ValueTree）が必要。[技術仕様書](ymulatorsynth-technical-spec.md) 1.3 のカスタム形式を利用する。

## 4. 実装順序の提案

| 順 | 項目 | 主な変更箇所 | 効果 / コスト |
|---|---|---|---|
| 1 | アルゴリズム表示の色分け + 言葉の説明 | ui/AlgorithmDisplay | 小 / 小 |
| 2 | Quick パネル（マクロ 6〜7 ノブ） | 新規 ui/QuickPanel, core/MacroMapper | 大 / 中 |
| 3 | ガイド付きランダマイザ + Undo | 新規 core/PatchGenerator | 大 / 中 |
| 4 | ドライバ FX（ビブラート・ピッチ EG・ポルタメント） | dsp/ 新規, core/VoiceManager | 大 / 大 |
| 5 | ユニゾンブランチの合流 | feature/unison-engine-implementation | 中 / 中 |

前提として、processBlock 毎の全パラメータ再送信を差分化しておくこと（マクロで多数のパラメータが同時に動くため）。

## 5. 参考資料

- Plogue chipsynth OPS7 manual v1.007: https://s3.amazonaws.com/chipsynth/OPS7_manual.pdf
- Plogue chipsynth OPS7: https://www.plogue.com/products/chipsynth-ops7.html
- Plogue chipsynth MD: https://www.plogue.com/products/chipsynth-md.html
- Korg opsix: https://www.korg.com/us/products/synthesizers/opsix/
- Korg opsix review (MusicRadar): https://www.musicradar.com/reviews/korg-opsix
- Elektron Digitone II review (Sound On Sound): https://www.soundonsound.com/reviews/elektron-digitone-ii
- Elektron Digitone Harmonics: https://support.elektron.se/support/solutions/articles/43000566560-harmonics
- NI FM8 feature details: https://www.native-instruments.com/en/products/komplete/synths/fm8/feature-details/
- FM8: An Introduction (ModeAudio): https://modeaudio.com/magazine/fm8-an-introduction
- Yamaha reface DX review (Synthtopia): https://www.synthtopia.com/content/2015/09/22/yamaha-reface-dx-synthesizer-review-user-friendly-fm-programming-in-a-hardware-keyboard/
- Yamaha Smart Morph (YamahaSynth): https://yamahasynth.com/learn/montage-series-synthesizers/mastering-montage-smart-morph/
- Synthmata Volca FM editor: https://synthmata.com/volca-fm/
- Inphonik RYM2612: https://www.inphonik.com/products/rym2612-iconic-fm-synthesizer/
- DX7 vs DX7 V vs Dexed (Synthtopia): https://www.synthtopia.com/content/2017/12/22/hardware-vs-software-yamaha-dx7-vs-arturia-dx7-v-vs-dexed-vst/
