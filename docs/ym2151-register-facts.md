# YM2151 レジスタの事実集

このプラグインが依存する YM2151（OPM）の事実を、出典つきで一か所にまとめる。**出典の無い事実はここに書かない。** 出典は次の 3 つのどれか。

- **マニュアル**: Yamaha「YM2151 Application Manual」（英語版、31 ページ）。ページ番号と図番号で示す。
- **ymfm**: `third_party/ymfm/src/ymfm_opm.h` / `.cpp` の実装。
- **テスト**: `tests/` の中で、その事実が破られると落ちるテスト。

コード側の定数は `src/dsp/YM2151Registers.h` にある。文書とコードが食い違ったら、マニュアルと ymfm を見て両方を直す。

一年前（v0.0.6）はこの表の半分が間違っていて、音程が 3 半音ずれ、オペレーターが入れ替わり、ビブラートが鳴らなかった。経緯は `CHANGELOG.md` の 0.0.7〜0.1.0 を参照。

## 1. クロックと出力レート

| 事実 | 値 | 出典 | コード | テスト |
|---|---|---|---|---|
| マスタークロック φM | 3.579545 MHz | マニュアル p.7（KC の説明）、p.11（NFRQ の式） | `OPM_DEFAULT_CLOCK = 3579545` | `PitchAccuracyTest.WrapperA4IsConcertPitchAtCommonSampleRates` |
| 出力サンプルレート | φM / 64 = 55,930 Hz | ymfm `ym2151::sample_rate()` | `YmfmWrapper.cpp` の `opmChip->sample_rate(...)` | 同上 |
| ホストレートへの変換 | チップはネイティブレートで動かし、ラッパーが 4 点補間でリサンプルする | 本プロジェクトの設計（マニュアルの範囲外） | `YmfmWrapper::generateSamples`、`resampleStep` | `PitchAccuracyTest.ProcessorA4IsConcertPitchAtCommonSampleRates`（44.1 / 48 / 96 kHz） |
| 出力の並び | `data[0]` = 左、`data[1]` = 右（インターリーブではない） | ymfm `ym2151::output_data` | `renderNativeSample` | `WideTest.OffKeepsBothChipsIdenticalAndTheOutputCentred` |

## 2. アドレスマップ（書き込み専用）

マニュアル Fig. 2.3 a) / b)（p.17）。レジスタは書き込み専用で読み戻せない。ラッパーは書いた値をキャッシュし、`readCurrentRegister()` で返す。

| アドレス | 内容 | ビット | コード |
|---|---|---|---|
| 0x01 | TEST（D1 = LFO リセット） | | |
| 0x08 | KON: キーオン | D6〜D3 スロット、D2〜D0 チャンネル（§4） | `REG_KEY_ON_OFF` |
| 0x0F | NE / NFRQ | D7 ノイズ有効、D4〜D0 周波数 | `REG_NOISE_CONTROL` |
| 0x10, 0x11 | CLKA1 / CLKA2 | タイマー A（未使用） | |
| 0x12 | CLKB | タイマー B（未使用） | |
| 0x14 | CSM / F RESET / IRQ EN / LOAD | 未使用 | |
| 0x18 | LFRQ | 8 ビット（Fig. 2.16 の表） | `REG_LFO_RATE` |
| 0x19 | PMD / AMD | **D7 = 1 で PMD、0 で AMD**。D6〜D0 が深さ | `REG_LFO_DEPTH`、`LFO_DEPTH_SELECT_PMD = 0x80` |
| 0x1B | CT / W | D7〜D6 CT 出力、D1〜D0 LFO 波形 | `REG_LFO_WAVEFORM` |
| 0x20〜0x27 | RL / FB / CON（チャンネル別） | D7 右、D6 左、D5〜D3 FB、D2〜D0 CON | `REG_ALGORITHM_FEEDBACK_BASE` |
| 0x28〜0x2F | KC（キーコード） | D6〜D4 オクターブ、D3〜D0 ノート | `REG_KEY_CODE_BASE` |
| 0x30〜0x37 | KF（キーフラクション） | D7〜D2 | `REG_KEY_FRACTION_BASE` |
| 0x38〜0x3F | PMS / AMS | D6〜D4 PMS、D1〜D0 AMS | `REG_LFO_AMS_PMS_BASE` |
| 0x40〜0x5F | DT1 / MUL（スロット別） | D6〜D4 DT1、D3〜D0 MUL | `REG_DT1_MUL_BASE` |
| 0x60〜0x7F | TL | D6〜D0 | `REG_TOTAL_LEVEL_BASE` |
| 0x80〜0x9F | KS / AR | D7〜D6 KS、D4〜D0 AR | `REG_KS_AR_BASE` |
| 0xA0〜0xBF | AMS-EN / D1R | D7 AMS 有効、D4〜D0 D1R | `REG_AMS_D1R_BASE` |
| 0xC0〜0xDF | DT2 / D2R | D7〜D6 DT2、D4〜D0 D2R | `REG_DT2_D2R_BASE` |
| 0xE0〜0xFF | D1L / RR | D7〜D4 D1L、D3〜D0 RR | `REG_D1L_RR_BASE` |

**0x1A に PMD は無い。** 空きアドレス。v0.0.6 はここに書いていたのでビブラートが鳴らなかった（0.0.8 で修正、`LfoWiringTest.PitchModulationReachesTheChip`）。

## 3. スロットのアドレス順序

マニュアル Fig. 2.2 Slot Designations（p.17）。スロット別レジスタ（0x40 以降）のアドレスは `base + offset + channel` で、offset は次のとおり。

| オペレーター | アドレス offset | VOPM の表記順 |
|---|---|---|
| M1（Modulator 1） | +0 | Op1 |
| M2（Modulator 2） | +8 | Op3 |
| C1（Carrier 1） | +16 | Op2 |
| C2（Carrier 2） | +24 | Op4 |

.opm と UI のオペレーター順は **M1, C1, M2, C2**（Op1〜Op4）なので、Op 番号からアドレスへは `OPERATOR_SLOT_OFFSET = {0, 16, 8, 24}` で引く。`op * 8` と書くと C1 と M2 が入れ替わり、全プリセットの音色が変わる（v0.0.6 の状態。0.0.7 で修正）。

- テスト: `OperatorSlotOrderTest.M1ModulatesC1InAlgorithm4`、`OperatorSlotOrderTest.RegisterHelperUsesVoiceOrderSlots`、`RegisterGoldenTest.EveryBundledPresetProducesExpectedRegisters`。

## 4. キーオン（0x08）のビット順序

マニュアル p.6「The SN bits D3, D4, D5, and D6 correspond to M1, C1, M2 and C2.」

| ビット | スロット |
|---|---|
| D3 | M1 |
| D4 | C1 |
| D5 | M2 |
| D6 | C2 |

つまりキーオンのビット順は**チェーン順（M1, C1, M2, C2）**で、§3 のアドレス順（M1, M2, C1, C2）とは違う。ymfm の `operator_map` が `(0, 16, 8, 24)` なのはこのため。VOPM の Op 順と同じなので、Op n のビットは D(3+n−1)。コードは `keyOnBitsForSlotMask()` と `OPERATOR_HW_SLOT = {0, 1, 2, 3}`。

0.0.8 でアドレス順（M1, M2, C1, C2）と誤って実装し、SLOT マスク付きのプリセット（ドラム）が無音になった。0.1.0 で修正。

- テスト: `SlotEnableTest.KeyOnBitsFollowTheChainOrder`、`SlotEnableTest.OnlyTheEnabledOperatorsSound`（音声で検証）、`SlotEnableTest.PresetSlotMaskMapsKeyOnBitsToOperators`。

## 5. キーコード（KC）とキーフラクション（KF）

マニュアル Fig. 2.4（p.18）。

- D6〜D4: オクターブ 0〜7（C 327 Hz 〜 C 4186 Hz の範囲）。
- D3〜D0: ノート。**0 = C#** から始まり、C はそのオクターブの最後（14）。

| ノート | C# | D | D# | E | F | F# | G | G# | A | A# | B | C |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 値 | 0 | 1 | 2 | 4 | 5 | 6 | 8 | 9 | 10 | 12 | 13 | 14 |

3, 7, 11, 15 は欠番。コードは `KEY_CODE_NOTE_TABLE = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14}` で、C から数えた添字で引くときは C が「1 つ下のオクターブの 14」になる点に注意する。v0.0.6 は表を C 基点で読み、全音が 1 半音高かった。

- KF（0x30）: D7〜D2 の 6 ビットで半音を 64 分割（1.5625 セント刻み）。
- 検算例（マニュアル p.7）: φM = 3.579545 MHz、KC = OCT 4 / NOTE 10（0x4A）、KF = 0、MUL = 1、DT1 = 0、DT2 = 0、PMS = 0 で **440.0 Hz**。
- テスト: `PitchAccuracyTest.WrapperA4IsConcertPitchAtCommonSampleRates`、`PitchAccuracyTest.WrapperOctavesTrackMidiNotes`、`PitchAccuracyTest.WrapperPitchBendUsesKeyFraction`。

## 6. パン（0x20 の D7 / D6）

マニュアル 2.1.8 ACC「LR: LEFT CHANNEL ENABLE / RIGHT CHANNEL ENABLE」（p.15）。D7 が右、D6 が左。

| 値 | 意味 | コード |
|---|---|---|
| 0x40 | 左のみ | `PAN_LEFT_ONLY` |
| 0x80 | 右のみ | `PAN_RIGHT_ONLY` |
| 0xC0 | 両方（中央） | `PAN_CENTER` |
| 0x00 | **無音** | |

リセット直後の 0x20 は 0 なので、L/R ビットを書くまでチャンネルは無音。FB / CON を書くときは L/R を保存する（`PRESERVE_ALG_FB_LR` など）。0.1.2 まで、Wide の L/R 配置と Echo を同時に使うと主チップのビットが 0 のままになり、原音が消えていた（`EchoTest.WideAndEchoFromTheFirstTickStillPlayTheNoteItself`）。

## 7. エンベロープ

マニュアル 2.1.4 EG（p.9〜10）、Fig. 2.11〜2.14（p.20〜21）。

| 事実 | 内容 | コード / テスト |
|---|---|---|
| 段階 | AR で 0 dB まで上がり、D1R で D1L まで下がり、D2R でキーオフまで下がり続け、RR で 96 dB へ | `EnvelopeDisplay::computeShape` |
| サステインの位置 | 減衰量は **TL + 4 × D1L**（D1L の 1 段 = 3 dB、TL の 1 段 = 0.75 dB）。D1L = 15 は「D7〜D4 すべて 1 なら 48 dB を足す」ので無音まで下がる | `EnvelopeDisplayTest.SustainSitsBelowThePeakByFourTimesD1L`、`D1LFifteenGoesAllTheWayDown` |
| レートのスケール | RATE = 2 × R + Rks。RR は 2 × RR + 1。RATE が 4 増えると時間は半分 | `EnvelopeDisplay::timeForRate`（2^((31−R)/4)） |
| キースケーリング | KS 0〜3 と KC の上位 5 ビットで Rks を足す（Fig. 2.12 の表） | ymfm |
| TL の重み | D6〜D0 = 48, 24, 12, 6, 3, 1.5, 0.75 dB。最小分解能 0.75 dB | `VelocityTest.LowerVelocityAttenuatesCarriersOnlyAndSurvivesBlocks` |

## 8. LFO とノイズ

- LFRQ（0x18）と周波数の対応は Fig. 2.16 の表（p.22）。上限は約 50 Hz、下限は 0.01 Hz 未満（p.11 本文）。
- 波形（0x1B の D1〜D0）: 0 のこぎり、1 矩形、2 三角、3 ノイズ（Fig. 2.16 の図、p.12）。**MOTION のビブラート波形（sine / triangle / saw / square / random）はプラグイン側のソフトウェア LFO で、これとは別物。**
- PMS（0x38 の D6〜D4）: 0, ±5, ±10, ±20, ±50, ±100, ±400, ±700 セント（Fig. 2.8、p.19）。AMS（D1〜D0）: 0, 23.9, 47.8, 95.6 dB（Fig. 2.15、p.21）。
- AMS-EN（0xA0 の D7）が 0 のスロットには振幅変調がかからない。
- ノイズ: NE = 1 で **スロット 32（チャンネル 7 の C2）** がノイズ源になる（Fig. 2.2 の注記、p.17）。周波数は NFRQ から p.11 の式で決まり、約 3.5 kHz 〜 111.9 kHz。だからノイズを使う音色はチャンネル 7 に置く（`VoiceManager::allocateVoiceWithNoisePriority`）。
- テスト: `LfoWiringTest.PitchModulationReachesTheChip`、`LfoWiringTest.AmplitudeModulationNeedsDepthSensitivityAndOperatorEnable`。

## 9. アルゴリズム（CON）とフィードバック

- CON 0〜7 の結線は Fig. 2.9（p.19）。コードは `src/dsp/AlgorithmInfo.h`（キャリア / モジュレーターの判定、MacroMapper と PatchGenerator が共有）。
- FL（フィードバック）0〜7 = OFF, π/16, π/8, π/4, π/2, π, 2π, 4π（Fig. 2.10）。M1 だけが自己フィードバックを持つ。
- テスト: `AlgorithmInfoTest`、`OperatorSlotOrderTest.M1ModulatesC1InAlgorithm4`、`M2ModulatesC2InAlgorithm4`。

## 10. DT1 / DT2 / MUL

- MUL（Fig. 2.5）: 0 は ×0.5、1〜15 は ×1〜×15。Init プリセットの MUL は 1（v0.1.0 まで 0 で 1 オクターブ低かった）。
- DT1（Fig. 2.6、p.18）: D6〜D4。マニュアルの表は 0〜3 の量（キーコードでスケール、数セント〜10 セント）。4〜7 は符号が負で、4 は 0 と同じ（ymfm、VOPM の表記もこれに従う）。
- DT2（Fig. 2.7）: 0, +600, +781, +950 セント（×1, ×1.41, ×1.57, ×1.73）。効果音向け。

## 11. この文書の使い方

- レジスタ定数を追加・変更するときは、この表の該当行と出典を更新し、固定するテストを書く。
- 新しい事実を書くときは、マニュアルの図番号か ymfm の該当箇所を必ず添える。記憶で書かない。
- 一般的な FM 音源（OPN 系）の知識をそのまま当てはめない。OPM で違う点: スロットのアドレス順とキーオン順が別、ノートコードが C# 始まりで欠番あり、PMD/AMD が 1 レジスタ共用、ノイズはスロット 32 固定。
