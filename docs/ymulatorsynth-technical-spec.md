# YMulator-Synth 技術仕様書

対象: YMulator-Synth v0.1.2。MIDI 実装、パラメーター一覧、ボイス割り当て、ノイズ、オーディオ出力の仕様をまとめる。数値はすべて `src/utils/ParameterIDs.h`、`src/core/ParameterManager.cpp`（`createParameterLayout`）、`src/core/MidiProcessor.cpp` から取っている。疑わしい点はソースを正とする。

関連文書:

- 構成要素とスレッド: [docs/ymulatorsynth-architecture.md](ymulatorsynth-architecture.md)
- YM2151 のレジスタ事実: [docs/ym2151-register-facts.md](ym2151-register-facts.md)
- .opm 形式: [docs/ymulatorsynth-vopm-format-spec.md](ymulatorsynth-vopm-format-spec.md)
- Quick パネルとマクロ: [docs/ymulatorsynth-quick-panel-design.md](ymulatorsynth-quick-panel-design.md)
- Motion: [docs/ymulatorsynth-motion-design.md](ymulatorsynth-motion-design.md)
- ymfm の使い方: [docs/ymulatorsynth-ymfm-integration-guide.md](ymulatorsynth-ymfm-integration-guide.md)

## 1. 概要

| 項目 | 内容 |
|------|------|
| 音源 | YM2151 (OPM) のみ。エミュレーションは ymfm |
| 同時発音数 | 8（YM2151 の 8 チャンネルをそのまま使う） |
| 形式 | AU / AUv3 / VST3 / Standalone、macOS / Windows / Linux |
| AU 識別子 | `aumu YMul Hrki` |
| チップ動作レート | 3,579,545 Hz / 64 = 55,930 Hz。`YmfmWrapper` がホストのサンプルレートへ 4 点キュービック補間でリサンプルする（§6） |
| 音色 | 1 音色を全 8 チャンネルに書き込む（マルチティンバーではない） |
| プリセット | .opm（VOPM / VOPMex 形式）にレジスタ値のみ保存。マクロと Motion はプラグイン状態に保存（§3.7、ADR-010） |

`YmfmWrapperInterface::ChipType` には `OPNA` も列挙されているが使われていない。YM2608、SSG、ADPCM、S98 録音、レイテンシーモードは実装していない。

## 2. MIDI 実装仕様

### 2.1 受信するメッセージ

`MidiProcessor::processMidiMessages` が扱うのは次のメッセージ。MIDI チャンネル番号は見ない（全チャンネルを同じに扱う）。

| メッセージ | 処理 |
|-----------|------|
| Note On | `VoiceManager` でチャンネルを割り当て、`YmfmWrapper::noteOn`。Mono / アルペジオ時は §2.8 |
| Note Off | 該当チャンネルに `noteOff`、ボイス解放 |
| Control Change | §2.2〜2.5 |
| Channel Pressure | Expressive MIDI が ON のときだけ Brightness マクロへ（§2.5） |
| Pitch Bend | §2.6 |

Velocity 0 の Note On は JUCE が Note Off として扱う。プログラムチェンジ、ポリフォニックアフタータッチ、All Notes Off は処理しない。

### 2.2 CC 一覧

VOPMex 互換の番号を使う。「CC 値の扱い」の列は §2.3 を参照。

#### グローバル

| CC | 対象パラメーター | 範囲 | CC 値の扱い |
|----|----------------|------|------------|
| 14 | `algorithm` (CON) | 0-7 | レジスタ値 |
| 15 | `feedback` (FL) | 0-7 | レジスタ値 |
| 1 | `lfo_rate` (LFRQ) 上位 7 ビット | 0-255 | `(CC1 << 1) \| CC33 のビット` |
| 33 | `lfo_rate` 最下位ビット | — | CC 値 ≥ 64 で 1。CC 1 を受けたときに合成される |
| 2 | `lfo_pmd` | 0-127 | レジスタ値 |
| 3 | `lfo_amd` | 0-127 | レジスタ値 |
| 12 | `lfo_waveform` (WF) | 0-3 | レジスタ値 |
| 75 | `lfo_pms` (PMS) | 0-7 | レジスタ値 |
| 76 | `lfo_ams` (AMS) | 0-3 | レジスタ値 |
| 80 | `noise_enable` (NE) | 0/1 | レジスタ値 |
| 82 | `noise_frequency` (NFRQ) | 0-31 | レジスタ値 |
| 81 | `noise_frequency`（0.0.6 以前の番号、互換のため残す） | 0-31 | レジスタ値 |

#### オペレーター（Op1〜Op4 が連番。Op1 = M1、Op2 = C1、Op3 = M2、Op4 = C2）

| CC (Op1/Op2/Op3/Op4) | パラメーター | 範囲 | ナチュラルモードで反転 |
|----------------------|------------|------|------------------|
| 16/17/18/19 | `opN_tl` | 0-127 | する |
| 20/21/22/23 | `opN_mul` | 0-15 | しない |
| 24/25/26/27 | `opN_dt1` | 0-7 | しない |
| 28/29/30/31 | `opN_dt2` | 0-3 | しない |
| 39/40/41/42 | `opN_ks` | 0-3 | しない |
| 43/44/45/46 | `opN_ar` | 0-31 | する |
| 47/48/49/50 | `opN_d1r` | 0-31 | する |
| 51/52/53/54 | `opN_d2r` | 0-31 | する |
| 55/56/57/58 | `opN_d1l` | 0-15 | する |
| 59/60/61/62 | `opN_rr` | 0-15 | する |
| 70/71/72/73 | `opN_ams_en` (AMS-EN) | 0/1 | しない |

`opN_slot_en` に対応する CC はない。

#### Quick マクロ（CC 値 = 範囲上の位置、64 ≈ 中央）

| CC | パラメーター | 範囲 |
|----|------------|------|
| 102 | `macro_brightness` | -50..+50 |
| 103 | `macro_harmonics` | 選択肢 9 個（Preset, Saw, Square, Pulse, Bright, Bell, Metal, Sub, Octave） |
| 104 | `macro_attack` | -50..+50 |
| 105 | `macro_decay` | -50..+50 |
| 106 | `macro_release` | -50..+50 |
| 107 | `macro_spread` | -50..+50 |

#### Motion（CC 値 = 範囲上の位置）

| CC | パラメーター | 範囲 |
|----|------------|------|
| 110 | `motion_wide` | 0-100 |
| 111 | `motion_vib_depth` | 0-100 |
| 112 | `motion_timbre_depth` | 0-40 |
| 113 | `motion_echo_level` | 0-100 |
| 114 | `motion_sweep_amount` | -40..+40 |
| 115 | `motion_level_attack` (Swell) | 0-3000 ms |
| 116 | `motion_porta_time` | 0-1000 ms |
| 117 | `motion_pitch_env` | -2400..+2400 cent |
| 118 | `motion_vel_bright` | 0-100 |

#### アルペジオ（CC 値 = 範囲上の位置）

| CC | パラメーター | 範囲 |
|----|------------|------|
| 108 | `motion_arp_chord` | 選択肢 12 個（None, Major, Minor, 7th, m7, Maj7, Sus4, Sus2, Dim, Aug, 5th, Octave） |
| 109 | `motion_arp_octaves` | 1-4 |
| 119 | `motion_arp_gate` | 10-100 % |

#### 制御用

| CC | 役割 |
|----|------|
| 99 / 98 / 6 | NRPN（§2.4） |
| 121 | Reset All Controllers: レジスタ値モードを解除し、NRPN 状態を消し、6 つのマクロを既定値（中央）へ戻す。ノートは止めない |
| 1 | Expressive MIDI が ON のときは `motion_vib_depth`（§2.5）。OFF のときは上記 LFRQ MSB |

### 2.3 CC 値の解釈

`MidiProcessor::applyCcToParameter` の規則。

**ナチュラルモード（既定、VOPMex の既定と同じ）**: 7 ビットの CC 値をパラメーターの段数へスケールし、エンベロープ系は反転する。

```
bits  = 最大値のビット数（7 → 3, 15 → 4, 31 → 5, 127 → 7, 1 → 1）
value = cc >> max(0, 7 - bits)
反転するパラメーター（TL/AR/D1R/D2R/D1L/RR）は value = max - value
```

例: CC 14 = 127 → ALG 7、CC 43 = 0 → AR 31、CC 16 = 0 → TL 127（無音）、CC 80 ≥ 64 → NE = 1。

**レジスタ値モード**（NRPN で切り替え）: `value = cc & max`。反転もスケールもしない。

**LFRQ（CC 1）**: モードに関係なく `(cc << 1) | lsb` を書く。`lsb` は直前に受けた CC 33 の値 ≥ 64 なら 1。

**位置指定 CC（102-119）**: `cc / 127` を正規化値としてそのままパラメーターへ渡す。`macro_*` の -50..+50 では CC 64 が 0（アンカーのまま）に丸まる。選択肢型（`macro_harmonics`、`motion_arp_chord`）はリストの上の位置になる。

すべての CC は `setValueNotifyingHost` でパラメーターを書き換えるので、ホストのオートメーションや UI と同じ経路で音色に反映される。レジスタ系 CC を動かすとプリセットは Custom になる。

### 2.4 NRPN

VOPMex と同じ手順でモードを切り替える。

| 送る順 | 意味 |
|--------|------|
| CC 99 = 126, CC 98 = 127, CC 6 = 127 | レジスタ値モード（VOPMex では全チャンネル） |
| CC 99 = 126, CC 98 = 0, CC 6 = 127 | レジスタ値モード（VOPMex では当該チャンネル） |
| 上のどちらかの後に CC 6 = 0 | ナチュラルモードに戻す |
| CC 121 | ナチュラルモードに戻す |

本プラグインは MIDI チャンネルを区別しないので、両方の形式が同じくプラグイン全体に効く。他の NRPN 番号は無視する。

### 2.5 Expressive MIDI

`midi_expressive`（既定 OFF）を ON にすると次の 2 つが変わる。

| 入力 | 対象 |
|------|------|
| CC 1（モジュレーションホイール） | `motion_vib_depth` を `cc / 127` の位置に設定。LFRQ MSB としては扱わない |
| チャンネルプレッシャー | `macro_brightness` を `0.5 + pressure / 254` の正規化値に設定（0 で中央、127 で最大） |

### 2.6 ピッチベンド

- 14 ビット値（0-16383、中央 8192）を `pitch_bend_range`（1-12 半音、既定 2）で半音へ換算する。
- 発音中の全チャンネルに `YmfmWrapper::setPitchBend` で適用する。KC / KF は「基準ノート + ベンド + Motion のピッチオフセット」から書き直される（`writePitch`）。
- ベンド量はチャンネル状態として保持され、次のノートオンにもそのまま乗る。
- ピッチベンド幅を変える CC はない。パラメーターまたは UI で設定する。

### 2.7 ベロシティ

`YmfmWrapper::applyVelocityToChannel`（`YM2151Regs::VELOCITY_TL_RANGE = 32`）。

- `quiet = 1 - velocity / 127`。キャリアの TL に `round(quiet * 32)` ステップを加える（velocity 127 でプリセットどおり、velocity 1 で 32 ステップ ≈ 24 dB 減衰）。
- モジュレーターは既定では変えない（音色はベロシティで変わらない）。`motion_vel_bright`（0-100）を上げると `round(quiet * vel_bright/100 * 32 * 1.25)` ステップ（最大 40）をモジュレーターにも加え、弱く弾くほど暗くなる。
- TL は「パラメーター値 + 減衰」として書かれるので、発音中にパラメーターを書き換えてもベロシティは残る。

### 2.8 Mono / レガート、アルペジオ、ラッチ

`motion_mono` または `motion_arp_mode ≠ Off` のとき、押されているノートは `HeldNotes`（最大 16 音、新しいものが末尾）にまとめられ、1 チャンネルだけを使う。

**Mono / レガート**
- 最初のノートで通常どおりチャンネルを割り当てる。
- 押している間に次のノートが来たら同じチャンネルを `retuneChannel` で移調する（キーオンし直さない）。`motion_porta_time > 0` ならグライドする。
- ノートオフで残っているノートがあれば直前に押したノートへ戻り、最後のノートオフでチャンネルを解放する。

**アルペジオ**
- 押されているノートは `HeldNotes` に入り、`MotionEngine` が `motion_arp_div` の刻みで順に鳴らす（Up / Down / Up Down / Random / As Played、`motion_arp_octaves` で上のオクターブを追加、`motion_arp_chord` で 1 音からコードを作る）。
- `motion_arp_retrig` OFF ではピッチだけ切り替える（チップ流）。ON では各ステップでキーオンし直し、`motion_arp_gate` の割合だけ押さえる。
- `motion_arp_accent` / `motion_arp_accent_depth` はアクセントのないステップのキャリア TL を下げる。
- ノートの追加・削除のたびに `HeldNotes::version` が進み、パターンは先頭からやり直す。

**ラッチ（`motion_arp_latch`）**
- ON のとき、ノートオフしてもノートは `HeldNotes` に残り、アルペジオは鳴り続ける。
- 全部の鍵を離した後に新しいノートを押すと、その時点でラッチ中のコードを捨てて新しいコードに置き換える（チャンネルは鳴ったまま）。
- ラッチを OFF にした時点で鍵が押されていなければ `releaseLatchedNotes` がチャンネルを解放する。

Mono でもアルペジオでもない状態に戻ったときは `HeldNotes` を空にして通常のポリ割り当てに戻る。

## 3. パラメーター一覧

`ParameterManager::createParameterLayout` が作る APVTS パラメーター。表の「.opm」は .opm ファイルに保存されるもの（§3.7）。

### 3.1 Operator（N = 1..4）

| ID | 名前 | 型 | 範囲 | 既定 | .opm |
|----|------|----|------|------|------|
| `opN_tl` | OpN TL | int | 0-127 | 0 | TL |
| `opN_ar` | OpN AR | int | 0-31 | 31 | AR |
| `opN_d1r` | OpN D1R | int | 0-31 | 0 | D1R |
| `opN_d1l` | OpN D1L | int | 0-15 | 15 | D1L |
| `opN_d2r` | OpN D2R | int | 0-31 | 0 | D2R |
| `opN_rr` | OpN RR | int | 0-15 | 7 | RR |
| `opN_ks` | OpN KS | int | 0-3 | 0 | KS |
| `opN_mul` | OpN MUL | int | 0-15 | 1 | MUL |
| `opN_dt1` | OpN DT1 | int | 0-7 | 3 | DT1 |
| `opN_dt2` | OpN DT2 | int | 0-3 | 0 | DT2 |
| `opN_ams_en` | OpN AMS Enable | bool | — | OFF | AMS-EN |
| `opN_slot_en` | OpN Slot | bool | — | ON | SLOT（4 つをまとめて 1 バイト） |

`d1l` の既定 15 は D1L レジスタとして「サステイン最小」だが、`d1r = 0` なので減衰せず、実質はサステイン音になる。

### 3.2 Global

| ID | 名前 | 型 | 範囲 | 既定 | .opm |
|----|------|----|------|------|------|
| `algorithm` | Algorithm | int | 0-7 | 0 | CON |
| `feedback` | Feedback | int | 0-7 | 0 | FL |
| `pitch_bend_range` | Pitch Bend Range | int | 1-12 | 2 | — |
| `midi_expressive` | Expressive MIDI | bool | — | OFF | — |

### 3.3 LFO / Noise

| ID | 名前 | 型 | 範囲 | 既定 | .opm |
|----|------|----|------|------|------|
| `lfo_rate` | LFO Rate | int | 0-255 | 0 | LFRQ |
| `lfo_pmd` | LFO PMD | int | 0-127 | 0 | PMD |
| `lfo_amd` | LFO AMD | int | 0-127 | 0 | AMD |
| `lfo_waveform` | LFO Waveform | choice | Sawtooth, Square, Triangle, Noise | Sawtooth | WF |
| `lfo_ams` | LFO AMS | int | 0-3 | 0 | AMS |
| `lfo_pms` | LFO PMS | int | 0-7 | 0 | PMS |
| `noise_enable` | Noise Enable | bool | — | OFF | NE |
| `noise_frequency` | Noise Frequency | int | 0-31 | 0 | NFRQ |

AMS / PMS はチャンネルレジスタ（0x38+ch）だが、1 音色を全チャンネルに書くので 1 組しか持たない。

### 3.4 Macro（Quick パネル）

すべて meta パラメーター（動かすと §3.1〜3.3 の生パラメーターを書き換える）。写像は `src/core/MacroMapper.h` と [Quick パネル設計](ymulatorsynth-quick-panel-design.md) を参照。

| ID | 名前 | 型 | 範囲 | 既定 |
|----|------|----|------|------|
| `macro_brightness` | Brightness | float | -50..+50、刻み 1 | 0 |
| `macro_harmonics` | Harmonics | choice | Preset, Saw, Square, Pulse, Bright, Bell, Metal, Sub, Octave | Preset |
| `macro_attack` | Attack | float | -50..+50、刻み 1 | 0 |
| `macro_decay` | Decay | float | -50..+50、刻み 1 | 0 |
| `macro_release` | Release | float | -50..+50、刻み 1 | 0 |
| `macro_spread` | Spread | float | -50..+50、刻み 1 | 0 |

Feedback ノブは `feedback` そのもの（マクロではない）。

### 3.5 Motion

すべてプラグイン状態のみ（.opm には入らない）。動作は [Motion 設計](ymulatorsynth-motion-design.md)。`divisions` は共通の音符リスト: 1/1, 1/2, 1/4, 1/8, 1/16, 1/2T, 1/4T, 1/8T, 1/32, 1/64, 1/16T（この順、index 0-10）。

| ID | 名前 | 型 | 範囲 | 既定 |
|----|------|----|------|------|
| `motion_vib_depth` | Vibrato Depth | float | 0-100（→ 0-50 cent） | 0 |
| `motion_vib_rate` | Vibrato Rate | float | 0.5-12 Hz、刻み 0.1 | 5 |
| `motion_vib_delay` | Vibrato Delay | float | 0-2000 ms、刻み 10 | 0 |
| `motion_vib_rise` | Vibrato Rise | float | 0-2000 ms、刻み 10 | 300 |
| `motion_vib_wave` | Vibrato Wave | choice | Sine, Triangle, Saw, Square, Random | Sine |
| `motion_vib_div` | Vibrato Sync Rate | choice | divisions | 1/8 |
| `motion_wide` | Wide | float | 0-100（→ 0-25 cent） | 0 |
| `motion_wide_pan` | Wide Pan | choice | L / R, Center | L / R |
| `motion_timbre_depth` | Timbre LFO Depth | float | 0-40 TL ステップ | 0 |
| `motion_timbre_rate` | Timbre LFO Rate | float | 0.1-12 Hz、刻み 0.1 | 1 |
| `motion_timbre_wave` | Timbre LFO Wave | choice | Sine, Triangle, Saw, Square, Random | Triangle |
| `motion_timbre_div` | Timbre LFO Sync Rate | choice | divisions | 1/1 |
| `motion_trem_depth` | Tremolo Depth | float | 0-24 TL ステップ | 0 |
| `motion_trem_rate` | Tremolo Rate | float | 0.5-12 Hz、刻み 0.1 | 5 |
| `motion_trem_div` | Tremolo Sync Rate | choice | divisions | 1/8 |
| `motion_lfo_oneshot` | LFO One Shot | bool | — | OFF |
| `motion_sync` | Motion Sync | bool | — | OFF |
| `motion_pan_mode` | Pan | choice | Off, Alternate, Step, Left, Right, Random | Off |
| `motion_pan_rate` | Pan Step | choice | divisions | 1/4 |
| `motion_pitch_env` | Pitch Env | float | -2400..+2400 cent | 0 |
| `motion_pitch_time` | Pitch Env Time | float | 0-500 ms | 60 |
| `motion_pitch_env2` | Pitch Env 2 | float | -2400..+2400 cent | 0 |
| `motion_pitch_time2` | Pitch Env Time 2 | float | 0-1000 ms | 0 |
| `motion_echo_level` | Echo Level | float | 0-100 | 0 |
| `motion_echo_time` | Echo Time | float | 10-500 ms | 120 |
| `motion_echo_div` | Echo Sync Rate | choice | divisions | 1/16 |
| `motion_sweep_amount` | Sweep Amount | float | -40..+40 TL ステップ | 0 |
| `motion_sweep_time` | Sweep Time | float | 50-4000 ms、刻み 10 | 1500 |
| `motion_level_attack` | Level EG Attack | float | 0-3000 ms、刻み 10 | 0 |
| `motion_level_decay` | Level EG Decay | float | 0-3000 ms、刻み 10 | 0 |
| `motion_level_sustain` | Level EG Sustain | float | 0-40 TL ステップ | 0 |
| `motion_mono` | Mono / Legato | bool | — | OFF |
| `motion_porta_time` | Portamento Time | float | 0-1000 ms、刻み 5 | 0 |
| `motion_vel_bright` | Velocity Brightness | float | 0-100 | 0 |
| `motion_arp_mode` | Arpeggio | choice | Off, Up, Down, Up Down, Random, As Played | Off |
| `motion_arp_div` | Arpeggio Step | choice | divisions | 1/64 |
| `motion_arp_octaves` | Arpeggio Octaves | int | 1-4 | 1 |
| `motion_arp_retrig` | Arpeggio Retrigger | bool | — | OFF |
| `motion_arp_gate` | Arpeggio Gate | float | 10-100 %、刻み 5 | 70 |
| `motion_arp_latch` | Arpeggio Latch | bool | — | OFF |
| `motion_arp_chord` | Arpeggio Chord | choice | None, Major, Minor, 7th, m7, Maj7, Sus4, Sus2, Dim, Aug, 5th, Octave | None |
| `motion_arp_accent` | Arpeggio Accent | choice | Off, Beat, 2 steps, 3 steps, 4 steps | Off |
| `motion_arp_accent_depth` | Arpeggio Accent Depth | float | 0-24 TL ステップ | 6 |

`motion_sync` ON のとき、`*_rate` / `*_time` の代わりに `*_div` がホストテンポ上の音符長として使われる（Pan Step と Arpeggio Step は常に音符長）。

### 3.6 プラグイン状態の付加情報

`StateManager::getStateInformation` は APVTS の全パラメーターに加えて次を保存する。

| キー | 内容 |
|------|------|
| `currentPreset` | プログラム番号 |
| `isCustomPreset` / `customPresetName` | 編集済み（Custom）かどうかと表示名 |
| `currentBankIndex` / `currentPresetInBank` | バンクとバンク内プリセット位置（ValueTree プロパティ） |
| `macroAnchor`（子ノード） | マクロの基準になる生パラメーター値（`MacroMapper::writeAnchorTo`） |
| `uiViewMode` | `"quick"` / `"detail"` |
| `generator`（子ノード） | ジェネレーターカードの設定（`GeneratorPanel`） |

`ParameterIDs.h` の `presetIndex`、`presetIndexChanged`、`isCustomMode`、および `Channel::pan/ams/pms` は定義だけが残っており、パラメーターにもプロパティにも使われていない。

### 3.7 .opm に入るものと入らないもの

| 保存先 | 内容 |
|--------|------|
| .opm（レジスタ値） | §3.1 のオペレーター 12 項目 × 4、`algorithm`、`feedback`、`lfo_*`（LFRQ/AMD/PMD/WF/AMS/PMS）、`noise_enable`、`noise_frequency`。PAN は書き出し時に常にセンター（3）、読み込み時は使わない |
| プラグイン状態のみ | `pitch_bend_range`、`midi_expressive`、`macro_*`、`motion_*`、§3.6 の付加情報 |

読み込み時は .opm のレジスタ値をパラメーターへ入れ、マクロを中央へ戻してアンカーを取り直す。詳細は [VOPM 形式仕様](ymulatorsynth-vopm-format-spec.md) と ADR-010。

## 4. ボイス割り当て

`VoiceManager`（`src/core/VoiceManager.cpp`）。8 チャンネル、1 チャンネル 1 ノート。

1. 同じノート番号が鳴っていればそのチャンネルを再利用する（リトリガー）。
2. `noise_enable` が ON のプリセットはチャンネル 7 だけを使う。空いていなければチャンネル 7 の音を奪う。
3. それ以外は 7 → 0 の順に空きチャンネルを探す（チャンネル 7 は空いていれば普通に使う）。
4. 全部使用中なら `StealingPolicy` に従って奪う。既定は `OLDEST`（最も古いキーオン）。`QUIETEST` / `LOWEST` はインターフェースにあるが、パラメーターや UI からは選べない。

ノートオフはノート番号からチャンネルを引き、`YmfmWrapper::noteOff` の後にボイスを解放する。Mono / アルペジオ時の扱いは §2.8。

## 5. ノイズジェネレーター

YM2151 のハードウェア制約（`YM2151Regs`）:

| 項目 | 値 |
|------|----|
| レジスタ | 0x0F: bit 7 = NE、bit 0-4 = NFRQ |
| 鳴るチャンネル | 7 のみ（`NOISE_CHANNEL`） |
| 鳴るオペレーター | Op4 = C2 のみ（`NOISE_OPERATOR = 3`）。C2 の正弦波出力がノイズに置き換わる |
| NFRQ | 0 が最も高い周波数、31 が最も低い |

C2 はどのアルゴリズムでもキャリアなので、ノイズは全アルゴリズムで出力に届く。Op1〜Op3 を TL 127 にすればノイズだけになり、残せばノイズと FM 音の混合になる。§4 のとおり、ノイズ ON のプリセットはチャンネル 7 に固定されるので、実質モノフォニックになる。

API: `YmfmWrapper::setNoiseEnable / setNoiseFrequency / setNoiseParameters / getNoiseEnable / getNoiseFrequency`。

## 6. オーディオ出力

`YmfmWrapper`（`src/dsp/YmfmWrapper.h/.cpp`）。

- ymfm の出力 `ymfm::ym2151::output_data` はインターリーブではない。`data[0]` = 左、`data[1]` = 右。`1 / 32768`（`SAMPLE_SCALE_FACTOR`）で float に落とす。
- チップは 55,930 Hz で動く（`ym2151::sample_rate(3579545)`）。`generateSamples` は 4 サンプルの履歴に対する Catmull-Rom 補間でホストレートへリサンプルする。`resampleStep` = ネイティブレート / ホストレート、位相が 1 を超えるたびに `renderNativeSample` で 1 サンプル進める。
- `generateSamples` は最初に出力バッファをゼロクリアする。未初期化のときはクリアだけして返る。
- **シャドウチップ**: 2 台目の `ym2151` がすべてのレジスタ書き込みをミラーする。Wide か Echo が ON のときだけ `renderNativeSample` でメインと合成される。
  - Wide: シャドウ側を最大 ±25 cent ずらす。`motion_wide_pan` = L / R では各チップが片側を受け持ち、Center では両方を -3 dB（0.7071）で混ぜる。
  - Echo: キーオン、ピッチ、レベルの書き込みをキュー（最大 8192 件）に積み、`motion_echo_time` 後にシャドウへ流す。キャリアは減衰させる。L / R ではノートは自分のパンを保ち、エコーは左右交互。
- Motion はレジスタ書き込みだけで実現している。出力の後段処理はない。

## 7. UI

UI の仕様はこの文書には置かない。

- Quick / Detail の 2 ビューとマクロ、ジェネレーター: [docs/ymulatorsynth-quick-panel-design.md](ymulatorsynth-quick-panel-design.md)
- Motion カードと各効果: [docs/ymulatorsynth-motion-design.md](ymulatorsynth-motion-design.md)
- コンポーネント一覧とスレッド: [docs/ymulatorsynth-architecture.md](ymulatorsynth-architecture.md)
