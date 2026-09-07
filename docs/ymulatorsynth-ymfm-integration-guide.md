# YMulator-Synth ymfm 統合ガイド

このドキュメントは、YMulator-Synth が ymfm ライブラリをどう使っているか（`src/dsp/YmfmWrapper.cpp`）と、YM2151 (OPM) で音を作るときの考え方をまとめたものです。対象チップは YM2151 (OPM) のみです。

レジスタの仕様そのもの（マニュアルの図番号と、それを固定しているテスト）は `docs/ym2151-register-facts.md` にあります。本書のレジスタに関する記述は `src/dsp/YM2151Registers.h` の定数と一致させてあり、食い違いがあればソースを正としてください。

## 目次

1. [概要](#1-概要)
2. [ymfm の基本](#2-ymfm-の基本)
3. [YmfmWrapper でのレジスタ操作](#3-ymfmwrapper-でのレジスタ操作)
4. [サウンドデザイン](#4-サウンドデザイン)
5. [トラブルシューティング](#5-トラブルシューティング)
6. [参考資料](#6-参考資料)

## 1. 概要

ymfm は Aaron Giles によるヤマハ FM 音源のエミュレーションライブラリです。YMulator-Synth は `third_party/ymfm` のサブモジュールをそのままビルドに含め、`ymfm::ym2151` を 2 個（メインチップと、Wide / Echo 用のシャドウチップ）生成して使います。

プラグイン側の窓口は `YmfmWrapper`（`src/dsp/YmfmWrapper.h`）です。ほかのコンポーネント（`ParameterManager`、`MotionEngine`、`VoiceManager`）は ymfm を直接触らず、この wrapper のメソッドを通してレジスタを書きます。

## 2. ymfm の基本

### 2.1 インターフェース

ymfm のチップは `ymfm::ymfm_interface` の実装を要求します。タイマーや割り込みはプラグインでは使わないため、YmfmWrapper は空実装を持つだけです（`YmfmWrapper` は `ymfm::ymfm_interface` を直接継承しています）。

```cpp
class YmfmWrapper : public YmfmWrapperInterface, public ymfm::ymfm_interface {
    // ymfm_sync_mode_write / ymfm_set_timer / ymfm_update_irq などは空実装
    std::unique_ptr<ymfm::ym2151> opmChip;
    std::unique_ptr<ymfm::ym2151> shadowChip;
};
```

チップは `reset()` 後、全チャンネルの L/R 出力ビットが 0 のため無音です。`initializeOPM()` は全 8 チャンネルの `0x20+ch` に `PAN_CENTER` (0xC0) を書いてから使い始めます。

### 2.2 レジスタ書き込み

ymfm への書き込みはアドレスとデータの 2 段階です。

```cpp
chip.write_address(address);
chip.write_data(data);
```

YM2151 のレジスタは書き込み専用で、書いた値をチップから読み戻す手段はありません。読み戻しが必要な場面（フィールドの一部だけ変えるとき）は wrapper のキャッシュを使います（3.1 参照）。

### 2.3 サンプル生成と出力フォーマット

```cpp
ymfm::ym2151::output_data output;
chip.generate(&output, 1);          // ネイティブレートで 1 サンプル
int16_t left  = output.data[0];
int16_t right = output.data[1];
```

`output_data::data` はインターリーブではなく、`data[0]` が左、`data[1]` が右です。1 回の `generate` で 1 サンプル分の L/R が得られます。`data[i * 2]` のようにインターリーブとして読むのは誤りです。

### 2.4 ネイティブレートとリサンプリング

YM2151 はクロック 3.579545 MHz を 64 分周した約 55,930 Hz で動作します（`opmChip->sample_rate(OPM_DEFAULT_CLOCK)`）。ymfm はこのレートでしかサンプルを出さないため、`YmfmWrapper::generateSamples` がホストのサンプルレートへリサンプリングします。

- `resampleStep = ネイティブレート / 出力レート`（`resetResampler`）
- 出力 1 サンプルごとに位相を進め、1.0 を超えた分だけ `renderNativeSample()` でネイティブサンプルを取り出す
- 直近 4 サンプルの履歴から Catmull-Rom（3 次）補間で出力値を作る
- `renderNativeSample` がメインチップとシャドウチップを混ぜる（Wide の中央配置では両者を -3 dB ずつ）

ホストのサンプルレートに合わせてチップのクロックを変える方式は取っていません。

## 3. YmfmWrapper でのレジスタ操作

### 3.1 レジスタキャッシュ

`YmfmWrapper::writeRegister` は、ymfm へ書く前に `currentRegisters[256]` に値を保存します。フィールドの一部だけを変えるときは `readCurrentRegister(address)` でキャッシュを読み、`YM2151Regs::PRESERVE_*` / `MASK_*_PRESERVE` でほかのビットを残して書き戻します。

```cpp
uint8_t current = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel,
              (current & YM2151Regs::PRESERVE_ALG_FB_LR) | (algorithm & YM2151Regs::MASK_ALGORITHM));
```

`reset()` はチップと一緒にキャッシュも消します。シャドウチップには `shadowRegisters` という別のキャッシュがあります。

### 3.2 オペレータのレジスタオフセット

オペレータ別レジスタ（0x40 DT1/MUL、0x60 TL、0x80 KS/AR、0xA0 AMS-EN/D1R、0xC0 DT2/D2R、0xE0 D1L/RR）のアドレスは次の形です。

```
address = base + slotOffset + channel
```

チップのアドレス順は M1 = +0、M2 = +8、C1 = +16、C2 = +24 です（マニュアル Fig. 2.2）。一方、VOPM とこのプラグインのオペレータ順 Op1..Op4 は M1, C1, M2, C2 なので、オフセット表は次のようになります。

```cpp
// YM2151Registers.h
constexpr uint8_t OPERATOR_SLOT_OFFSET[4] = {0, 16, 8, 24};   // Op1=M1, Op2=C1, Op3=M2, Op4=C2
constexpr uint8_t getOperatorRegister(uint8_t baseReg, uint8_t op, uint8_t channel) {
    return baseReg + OPERATOR_SLOT_OFFSET[op] + channel;
}
```

`base + op * 8 + channel` と書くと Op2 (C1) と Op3 (M2) が入れ替わり、アルゴリズム 4 以降でモジュレータとキャリアの役割がずれます。`OperatorSlotOrderTest` がこの対応を固定しています。

### 3.3 キーオンと SLOT マスク

キーオンレジスタ 0x08 は、ビット 0-2 がチャンネル、ビット 3-6 がオペレータの有効ビットです。ビットの並びはアドレス順ではなく、アルゴリズムの連鎖順 M1, C1, M2, C2 です（ビット 3 = M1、4 = C1、5 = M2、6 = C2。マニュアル p.6）。

wrapper はチャンネルごとに VOPM の SLOT 値に相当する `slotMask`（ビット n = Op(n+1) を鳴らす）を持ち、`keyOnBitsForSlotMask` で 0x08 のビット配置に変換します。

```cpp
writeRegister(YM2151Regs::REG_KEY_ON_OFF,
              YM2151Regs::keyOnBitsForSlotMask(channelStates[channel].slotMask) | channel);
// キーオフはビット 3-6 をすべて 0 にして書く
writeRegister(YM2151Regs::REG_KEY_ON_OFF, YM2151Regs::KEY_OFF_MASK | channel);
```

Op1..Op4 の順と 0x08 のビット順が同じなので、`slotMask` のビット n はそのままビット 3+n に入ります。`SlotEnableTest` がこれを検証しています。

### 3.4 音程（KC / KF）

キーコード 0x28+ch はビット 6-4 がオクターブ (0-7)、ビット 3-0 がノートコードです。ノートコードは C# から始まり、C はひとつ下のオクターブのコード 14 です（マニュアル Fig. 2.4）。

```cpp
// YM2151Registers.h: C# を 0 とした半音数で引く
constexpr uint8_t KEY_CODE_NOTE_TABLE[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};
//                                            C# D  D# E  F  F# G  G# A   A#  B   C
constexpr uint8_t MIDI_NOTE_CSHARP4 = 61;    // キーコードのオクターブ 4 の最初の音
constexpr uint8_t KEY_CODE_OCTAVE_4 = 4;
```

`YmfmWrapper::noteToFnumWithPitchBend` は MIDI ノート番号から 61 (C#4) を引いた半音数でオクターブと表の添字を決めます。A4 (MIDI 69) は 61 + 8 なのでオクターブ 4、コード 10、KC = 0x4A です。マニュアル p.7 の例（KC = 0x4A、KF = 0、MUL = 1 で 440.0 Hz）と一致し、`PitchAccuracyTest` がこれを確認します。

C を 0 とした表（`{0, 1, 2, 4, ...}` を C から引く）は誤りです。0.0.7 まではこの C 基準の表を使っていたため、全音が半音ずれていました。コード 3, 7, 11, 15 はチップが使いません。

キーフラクション 0x30+ch は上位 6 ビット（ビット 7-2）が有効で、半音を 64 等分した 1.5625 セント刻みです。ピッチベンドと Wide のデチューンはこの KF で表現されます。

### 3.5 パン（L / R 出力ビット）

`0x20+ch` の上位 2 ビットが出力の有効ビットです。

| ビット | 定数 | 意味 |
|--------|------|------|
| 6 (0x40) | `PAN_LEFT_ONLY` / `MASK_LEFT_ENABLE` | 左出力を有効にする |
| 7 (0x80) | `PAN_RIGHT_ONLY` / `MASK_RIGHT_ENABLE` | 右出力を有効にする |
| 6+7 (0xC0) | `PAN_CENTER` | 両方 |
| 0 (0x00) | `PAN_OFF` | 無音 |

両ビットが 0 だとそのチャンネルは鳴りません。リセット直後はこの状態なので、初期化で必ず `PAN_CENTER` を書きます。アルゴリズムやフィードバックを書き換えるときは `PRESERVE_ALG_FB_LR` などでこの 2 ビットを保存します。Wide の L/R 配置時は `panForChip` がメインチップを左、シャドウチップを右に振り分け、レジスタキャッシュ側は中央のままです。

### 3.6 LFO とノイズ

| レジスタ | 内容 |
|----------|------|
| 0x18 (`REG_LFO_RATE`) | LFRQ 0-255 |
| 0x19 (`REG_LFO_DEPTH`) | AMD と PMD の共用。ビット 7 = 1 で PMD、0 で AMD、ビット 6-0 が深さ (0-127) |
| 0x1B (`REG_LFO_WAVEFORM`) | ビット 1-0 が波形（0 ノコギリ、1 矩形、2 三角、3 ノイズ）。ビット 7-6 は CT 出力 |
| 0x38+ch (`REG_LFO_AMS_PMS_BASE`) | ビット 6-4 が PMS (0-7)、ビット 1-0 が AMS (0-3) |
| 0xA0+slot (`REG_AMS_D1R_BASE`) | ビット 7 がオペレータごとの AMS 有効 (AMS-EN) |
| 0x0F (`REG_NOISE_CONTROL`) | ビット 7 がノイズ有効、ビット 4-0 が周波数。ノイズはチャンネル 7 の C2 のみ |
| 0x01 (TEST) | LFO リセット。wrapper は使用しない |

AMD と PMD は同じアドレスなので、両方を設定するには 2 回書きます（`setLfoParameters`）。

```cpp
writeRegister(YM2151Regs::REG_LFO_DEPTH, amd & YM2151Regs::MASK_LFO_DEPTH);
writeRegister(YM2151Regs::REG_LFO_DEPTH, YM2151Regs::LFO_DEPTH_SELECT_PMD | (pmd & YM2151Regs::MASK_LFO_DEPTH));
```

AMS / PMS はチャンネルレジスタ 0x20 ではなく 0x38+ch にあります。`LfoWiringTest` が配線を確認します。

### 3.7 パラメータ変更のタイミング

パラメータの変更はキーオフや待ち時間なしにそのまま書きます。`ParameterManager::updateYmfmParameters` は前回値と比べて変わったものだけを wrapper に渡し、wrapper はキャッシュから読み・変えたビットだけ書き換えて即座に ymfm へ送ります。ymfm は次のサンプルから新しい値で計算するので、発音中の TL やレート変更もそのまま反映されます（`RegisterUpdateTest`）。「パラメータ変更の前にキーオフして数 ms 待つ」といった手順は不要です。

音程（0x28 / 0x30）だけは `writePitch` がメインチップとシャドウチップに別々の値を書きます（Wide のデチューン）。それ以外の書き込みはメインチップとシャドウチップの両方に届き、Echo 有効時はキーオン・音程・レベルの書き込みがシャドウチップに遅れて届きます。

## 4. サウンドデザイン

ここでの Op1..Op4 は M1, C1, M2, C2 の順です。UI の OperatorPanel と `.opm` の行順と同じです。

### 4.1 アルゴリズム

`src/dsp/AlgorithmInfo.h` の `kAlgorithms` から。`>` は変調、`+` は加算です。

| ALG | 構成 | キャリア | 向く音色 |
|-----|------|----------|----------|
| 0 | 1>2>3>4 | 4 | 最も鋭く明るい。ベース、リード、金属音 |
| 1 | (1+2)>3>4 | 4 | ブラス、厚いリード |
| 2 | (1+(2>3))>4 | 4 | クラビ、ギター |
| 3 | ((1>2)+3)>4 | 4 | エレピ、ハープシコード |
| 4 | (1>2)+(3>4) | 2, 4 | 独立した 2 組の 2 オペレータ。エレピ、ベル、ギター、ブラス |
| 5 | 1>(2+3+4) | 2, 3, 4 | 1 つのモジュレータで 3 キャリアを変調。オルガン、ストリングス、パッド |
| 6 | (1>2)+3+4 | 2, 3, 4 | 2 オペレータのペアにサイン波 2 本。サブ付きベース、オルガン |
| 7 | 1+2+3+4 | 1, 2, 3, 4 | 加算合成。オルガン、フルート、純音 |

フィードバック (FB 0-7) は Op1 (M1) の自己変調で、ノコギリ波に近づけたいときに上げます。アルゴリズム 7 のようにモジュレータのないアルゴリズムでは、明るさの調整はフィードバックで行います（Brightness マクロもそう動きます）。

### 4.2 TL と変調の深さ

FM の倍音の量はモジュレータの TL で決まります。TL は減衰量で、0 が最大出力、127 が無音です。

| 役割 | 目安 |
|------|------|
| キャリア | 0-30（音量） |
| モジュレータ、浅い変調 | 50-60（倍音少なめ） |
| モジュレータ、中程度 | 30-40 |
| モジュレータ、深い変調 | 0-20（倍音豊富、歪みやすい） |

MIDI ベロシティはキャリアの TL だけを最大 32 ステップ（約 24 dB）減衰させます（`VELOCITY_TL_RANGE`）。モジュレータは動かさないので、弱く弾いても倍音構成は変わりません。明るさをベロシティで変えたいときは Motion の Velocity→Brightness を使います。

### 4.3 エンベロープ

各オペレータの AR (0-31)、D1R (0-31)、D2R (0-31)、D1L (0-15)、RR (0-15) と、チップの挙動で押さえておく点です。

- サステインレベルは TL + 4 × D1L の減衰量になる。D1L = 15 はそのオペレータの無音を意味する
- レートは 4 段階上がるごとに速さが 2 倍になる。RR は内部で 2 × RR + 1 として扱われるため、AR / D1R の半分の分解能
- D2R = 0 ならサステインは減衰しない
- KS (0-3) を上げると高音域ほどレートが速くなる

用途別の出発点:

| 音色 | AR | D1R | D2R | D1L | RR |
|------|----|-----|-----|-----|----|
| ピアノ、エレピ | 31 | 5-10 | 3-8 | 2-4 | 5-10 |
| ベース | 31 | 10-15 | 0 | 0-2 | 10-15 |
| ブラス | 28-31 | 8-12 | 2-5 | 2-4 | 8-12 |
| ストリングス、パッド | 20-25 | 8-12 | 0 | 0 | 8-12 |
| オルガン | 31 | 0 | 0 | 0 | 8-10 |

モジュレータのエンベロープは時間とともに倍音が減る様子（ピアノの減衰、ブラスのアタック）を作ります。キャリアのエンベロープは音量の輪郭です。Attack / Decay / Release マクロはキャリアだけを動かします。

### 4.4 MUL と DT1 / DT2

- MUL (0-15) はオペレータの周波数倍率。0 は 0.5 倍。キャリアとモジュレータが整数比なら調和的、非整数比（DT2 や MUL の組み合わせ）なら金属的
- DT1 (0-7) は数セントの微小デチューン。1-2 でコーラス感、3-4 で厚み。ビット 6 が符号で、4-7 は負方向。Spread マクロは Op1-3 の DT1 を動かす
- DT2 (0-3) は大きなデチューン（0、+600、+781、+950 セント相当）。ベルや効果音向け

### 4.5 LFO の使いどころ

- ビブラート: PMD を上げ、チャンネルの PMS で感度を決める。PMS 4-5 で一般的な深さ
- トレモロ: AMD と AMS に加え、揺らしたいオペレータの AMS-EN を立てる。キャリアに掛ければ音量、モジュレータに掛ければ音色が揺れる
- LFO はチップ全体で 1 つ。チャンネルごとに変えられるのは PMS / AMS だけ
- 速度や深さを時間で変えたい場合は Motion の Vibrato / Timbre LFO / Tremolo が制御レート (64 サンプル) でこれらのレジスタを書き換える

### 4.6 実例

具体的な音色は `src/utils/PresetManager.cpp` の `FACTORY_VOICES`（Electric Piano、Synth Bass、Brass Section、String Pad、Lead Synth、Organ、Bells、Init）と、同梱バンク `resources/presets/ymulator-synth-preset-collection.opm` を参照してください。`.opm` の各行の意味は `docs/ymulatorsynth-vopm-format-spec.md` にあります。

## 5. トラブルシューティング

### 5.1 音が出ない

- `0x20+ch` の L/R ビットが両方 0 になっていないか（リセット直後の状態）
- キャリアの TL が 127 に近くないか。モジュレータの TL を下げても音量は上がらない
- 0x08 のスロットビットが 0 のままキーオンしていないか（SLOT マスクを確認）
- キャリアの D1L が 15 だと減衰後に無音になる
- チップが `initialize` 済みか。`generateSamples` は未初期化なら無音バッファを返す

### 5.2 音程がずれる

- ノートコードの表が C# 基準になっているか（3.4）。半音ずれるなら C 基準の表を使っている
- クロックが 3.579545 MHz か。ネイティブレートを別の値で計算すると全体がずれる
- KF はビット 7-2 に置く（`SHIFT_KEY_FRACTION` = 2）

### 5.3 クリックやノイズ

- AR = 31 で立ち上がりが硬すぎるなら 20-28 に下げる
- RR が速すぎると（14-15）リリースがクリックになる
- ボイススティール時の切り替えは避けられない。VoiceManager は最も古いノートを奪う

### 5.4 想定と違う音色になる

- `readCurrentRegister(0x20 + ch) & MASK_ALGORITHM` でアルゴリズムを確認
- オペレータのオフセットが `OPERATOR_SLOT_OFFSET` 経由か。`op * 8` なら Op2 と Op3 が入れ替わっている
- `.opm` の行順は M1, C1, M2, C2。M1, M2, C1, C2 と読むと同じ入れ替わりが起きる
- モジュレータの TL が低すぎると倍音が飽和して別の音になる

## 6. 参考資料

- [ymfm](https://github.com/aaronsgiles/ymfm)
- YM2151 Application Manual (Yamaha)。図番号付きの整理は `docs/ym2151-register-facts.md`
- `src/dsp/YM2151Registers.h`、`src/dsp/YmfmWrapper.cpp`、`src/dsp/AlgorithmInfo.h`
- テスト: `tests/unit/RegisterGoldenTest.cpp`、`OperatorSlotOrderTest.cpp`、`SlotEnableTest.cpp`、`LfoWiringTest.cpp`、`PitchAccuracyTest.cpp`、`RegisterUpdateTest.cpp`
