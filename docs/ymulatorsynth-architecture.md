# YMulator-Synth アーキテクチャ

対象: v0.1.2 (2026-09)。この文書はソースの現状を写したものであり、ソースと食い違う場合はソースが正しい。

## 1. 目的と範囲

YMulator-Synth は YM2151 (OPM) を ymfm でエミュレートする 8 声の FM シンセサイザープラグインである。対応チップは OPM のみで、YM2608/OPNA・SSG・ADPCM は実装していない (`YmfmWrapperInterface::ChipType::OPNA` は列挙子として残っているが使われない)。

- 形式: AU / AUv3 / VST3 / Standalone (`src/CMakeLists.txt` の `juce_add_plugin`)。AU 識別子は `aumu YMul Hrki`
- プラットフォーム: macOS / Windows / Linux
- 言語と基盤: C++17、JUCE 9.0.1 (FetchContent)、CMake 3.22+
- エディタサイズ: 1000×640 (`PluginEditor.cpp`)

この文書はコンポーネント構成・処理の流れ・スレッド・UI 構成・ビルドとテストを扱う。個々の仕様は次の文書に分けてある。

| 内容 | 文書 |
|---|---|
| MIDI CC / NRPN、パラメータ変換、ノイズ、ボイス | `ymulatorsynth-technical-spec.md` |
| YM2151 レジスタの事実と固定テスト | `ym2151-register-facts.md` |
| Quick ビューとマクロ、ジェネレータ | `ymulatorsynth-quick-panel-design.md` |
| モーション (ビブラート、Wide、エコー、アルペジオなど) | `ymulatorsynth-motion-design.md` |
| .opm 形式 | `ymulatorsynth-vopm-format-spec.md` |
| ymfm の使い方 | `ymulatorsynth-ymfm-integration-guide.md` |
| セットアップ、ビルド、テスト戦略 | `ymulatorsynth-implementation-guide.md` |
| 設計判断 | `ymulatorsynth-adr.md` |

## 2. 全体像

```
ホスト (DAW)
  │ MIDI / オーディオ / パラメータ / 状態 / トランスポート
  ▼
YMulatorSynthAudioProcessor (src/PluginProcessor.*)   APVTS "YMulatorSynth"
  ├─ MidiProcessor ──── VoiceManager (8ch 割当て)     ┐
  ├─ ParameterManager (レイアウト生成、差分書き込み)  │ 音声スレッド
  ├─ MotionEngine (64 サンプルごとの tick)             │
  │                                                    ▼
  │                                     YmfmWrapper (src/dsp)
  │                                       ├─ ymfm::ym2151 main chip
  │                                       ├─ ymfm::ym2151 shadow chip (Wide / Echo)
  │                                       └─ Catmull-Rom リサンプラ (55,930 Hz → ホストレート)
  ├─ StateManager (get/setState、プログラム切替)
  ├─ MacroMapper (マクロ → 生レジスタ、アンカー)
  ├─ PatchWorkspace ── SnapshotStore (Undo 16 段、A/B)
  │        └─ PatchGenerator (決定的乱数パッチ)
  └─ PresetManager (src/utils) ── VOPMParser (.opm)

PluginEditor ── MainComponent ─┬─ ヘッダ: Quick/Detail ボタン、PresetUIManager
                               ├─ QuickView: GeneratorPanel, マクロ 6 個, AlgorithmDisplay+FB,
                               │             MotionPanel, OutputScope (PatchPreview が描画元)
                               └─ Detail: ToneStrip, OperatorPanel×4 (EnvelopeDisplay),
                                          MotionStrip (+ArpSettingsPanel), LfoNoiseStrip
```

依存はインターフェース経由で注入する。`YmfmWrapperInterface`、`VoiceManagerInterface`、`MidiProcessorInterface`、`PresetManagerInterface` は `PluginProcessor` の第 2 コンストラクタ (テスト用) が受け取る。`ParameterManager`、`StateManager`、`MacroMapper`、`PatchWorkspace`、`MotionEngine` は具象クラスをそのまま持つ。

## 3. コンポーネント一覧

### src/ (トップ)

| クラス | 責務 | 主な公開メソッド | テスト |
|---|---|---|---|
| `YMulatorSynthAudioProcessor` | 各コンポーネントの所有と配線、`processBlock`、プログラム/状態 API の委譲、.opm 読み書きとユーザーバンク保存 | `processBlock`, `prepareToPlay`, `setCurrentProgram`, `loadOpmFile`, `saveCurrentPresetAsOpm`, `saveCurrentPresetToUserBank`, `setCurrentPresetInBank`, `extractCurrentPreset` | PluginBasicTest, PluginProcessorComprehensiveTest, MultiInstanceTest, ComprehensiveIntegrationTest, AudioQualityTest |
| `YMulatorSynthAudioProcessorEditor` | `MainComponent` を包むだけ | `resized` | (MainComponentTest 経由) |

`PluginProcessor.h` には移行前の名残 (`ccToParameterMap`, `currentPitchBend`, `processMidiMessages` などの deprecated メソッド、`needsPresetReapply`) が残っているが、いずれも実際の経路では使われない。

### src/core

| クラス | 責務 | 主な公開メソッド | テスト |
|---|---|---|---|
| `MidiProcessor` (`MidiProcessorInterface`) | ノート on/off、CC → パラメータ (VOPMex 互換 + Quick/Motion 位置指定)、NRPN 126/127 でレジスタ値モード、ピッチベンド、モノ/アルペジオの `HeldNotes` | `processMidiMessages`, `handleMidiCC`, `handlePitchBend`, `setupCCMapping`, `releaseLatchedNotes`, `getHeldNotes` | MidiCcMappingTest, MonoArpTest, VelocityTest, PitchAccuracyTest |
| `HeldNotes` | モノ/アルペジオ中に押されているノートと発音チャンネル (構造体) | `add`, `remove`, `last`, `clear` | MonoArpTest |
| `VoiceManager` (`VoiceManagerInterface`) | 8 チャンネルの割当て。空きは 7→0 の順、満杯なら OLDEST を奪う。ノイズ音色は常に ch7 | `allocateVoiceWithNoisePriority`, `releaseVoice`, `getChannelForNote`, `setNoteForChannel` | VoiceManagerTest |
| `ParameterManager` | APVTS レイアウト生成 (`createParameterLayout`)、ブロックごとの差分書き込み、プリセットのツリー読み込みとチップ直書き、カスタムモード判定 | `createParameterLayout`, `updateYmfmParameters`, `invalidateRegisterCache`, `loadPresetParameters`, `applyPresetToYmfm`, `extractCurrentParameterValues`, `setCustomMode` | ParameterManagerTest, SimpleParameterTest, ParameterDebugTest, RegisterUpdateTest, ParameterReachTest, SlotEnableTest, LfoWiringTest |
| `StateManager` | `getStateInformation`/`setStateInformation`、JUCE プログラム API (末尾は常に "Custom")、プリセット読み込み手順、bank/preset プロパティの同期 | `getStateInformation`, `setStateInformation`, `setCurrentProgram`, `getProgramName`, `loadPreset`, `setMacroMapper` | StateManagerTest, ParameterStateIntegrationTest |
| `MacroMapper` | 6 マクロをアンカーからのオフセットとして生レジスタへ写像。APVTS リスナーとして macro_* の変化で書き込み、生値の直接編集でアンカーを再基準化。アンカーの状態保存 | `apply` (純関数), `targetsOf`, `captureAnchor`, `setSuspended`, `writeAnchorTo`, `restoreFromState`, `isEdited` | MacroMapperTest |
| `PatchGenerator` | カテゴリと 6 方向から決定的にパッチを生成 (静的) | `generate`, `algorithmCandidates`, `categoryName` | PatchGeneratorTest |
| `SnapshotStore` | Undo スタック (16 段) と A/B スロット。純データ | `pushUndo`, `popUndo`, `setSlot`, `slot` | PatchWorkspaceTest |
| `PatchWorkspace` | 生成結果やスナップショットをツリーへ適用、Undo 点の記録 (生成前と TONE ノブのジェスチャ開始時)、A/B 切替、適用後の再アンカー。メッセージスレッド専用 | `generate`, `applyPatch`, `undo`, `selectSlot`, `capture`, `restore` | PatchWorkspaceTest |
| `PatchPreview` | 私有の `YmfmWrapper` で C4 を 1 音レンダリング (OUTPUT スコープ用)。メッセージスレッド専用 | `render(preset, hold, total)`, `periodInSamples` | PatchPreviewTest |
| `MotionEngine` | 64 サンプルごとにビブラート、Wide、ティンバー LFO、トレモロ、パン、ピッチ EG、スイープ、レベル EG、ポルタメント、エコー、アルペジオをレジスタ書き込みとして実行。ビートクロックはホスト再生中は ppq に追従、停止中は自走 | `bindParameters`, `prepare`, `setTransport`, `tick`, `beatsForDivision`, `currentOffset` | MotionEngineTest, PanMotionTest, WideTest, EchoTest, MonoArpTest |
| `AudioProcessingInterface` / `AudioProcessor` | **使われていない。** `src/CMakeLists.txt` でコンパイルされるが、他のどのファイルからも include されない | – | なし |

### src/dsp

| クラス | 責務 | 主な公開メソッド | テスト |
|---|---|---|---|
| `YmfmWrapper` (`YmfmWrapperInterface`, `ymfm::ymfm_interface`) | main chip と shadow chip の所有、レジスタキャッシュ、KC/KF 計算 (ベンド + モーションオフセット)、ベロシティとモーションを含む TL の合成書き込み、スロットマスク、Wide のデチューンとパン、Echo の遅延書き込みキュー、リサンプラ | `initialize`, `generateSamples`, `noteOn`, `noteOff`, `retuneChannel`, `setOperatorParameter`, `setAlgorithm`, `setFeedback`, `setPitchBend`, `setChannelPitchOffset`, `setChannelLevelMotion`, `setWide`, `setEcho`, `setChannelSlotMask`, `writeRegister`, `readCurrentRegister`, `readShadowRegister` | YmfmWrapperTest, RegisterGoldenTest, OperatorSlotOrderTest, SlotEnableTest, PitchAccuracyTest, VelocityTest, WideTest, EchoTest |
| `YM2151Registers.h` | レジスタアドレス、マスク、`OPERATOR_SLOT_OFFSET`、`keyOnBitsForSlotMask`、`KEY_CODE_NOTE_TABLE` などの定数 | – | RegisterGoldenTest, OperatorSlotOrderTest |
| `AlgorithmInfo.h` | 8 アルゴリズムのキャリア/モジュレータと結線 (`kAlgorithms`)。UI の役割表示、マクロ、ジェネレータが参照 | `algorithmInfo`, `isCarrier`, `targetOf` | AlgorithmInfoTest |
| `EnvelopeGenerator` | **空のプレースホルダ。** 本体はなく、どこからも include されない | – | なし |
| `RegisterManager`, `NoteConverter`, `ParameterConverter` | **使われていない。** プラグインにコンパイルされるが include 元がなく、同等の処理は `YmfmWrapper` 内にある | – | なし |

### src/ui

| クラス | 責務 | 主な公開メソッド | テスト |
|---|---|---|---|
| `MainComponent` | エディタのルート。ヘッダ、Quick/Detail の切替と `uiViewMode` の保存、algorithm/feedback/noise_enable の変化で役割の再計算 (AsyncUpdater)、マクロフォーカスの中継 | `setViewMode`, `setMacroFocus`, `refreshRoles`, `highlightedParameterIds` | MainComponentTest |
| `PresetUIManager` | バンク/プリセットコンボ、Save ボタン、edited タグ、.opm 読み込み/保存ダイアログ。ValueTree のプリセット関連プロパティを監視し、カスタムモードは 5 Hz でポーリング | `updateBankComboBox`, `updatePresetComboBox`, `syncCustomMode` | MainComponentTest |
| `QuickView` | Quick ビュー: GENERATE / TONE / ALGORITHM / MOTION / OUTPUT カード。4 Hz でアルゴリズム表示・要約・プレビューを更新 | `refresh`, `getDisplayedAlgorithm`, `onShowDetail` | MainComponentTest |
| `GeneratorPanel` | カテゴリチップと 6 方向スライダー。設定は状態ツリーの `generator` ノードに保存 | `currentInput` | MainComponentTest |
| `MotionPanel` | MOTION カード本体: 機能チップ (on/off の既定値セット)、主要量のノブ、パン/アルペジオのコンボ、Sync | `features`, `toggleFeature`, `isFeatureOn`, `allOff`, `refresh` | MainComponentTest |
| `OutputScope` | レンダリング済み波形の再生表示 (3 周期 + エンベロープ帯、タイマーでプレイヘッド) | `setWaveform`, `setSilenceHint`, `setPlayhead` | PatchPreviewTest, MainComponentTest |
| `ToneStrip` | Detail の TONE 行: マクロ 6 個、ALG コンボ、`AlgorithmDisplay`、Feedback ノブ。触っているマクロを `onMacroFocus` で通知 | `setAlgorithm`, `setFeedback`, `setHighlightedParameters` | MainComponentTest |
| `OperatorPanel` | オペレータ 1 行: 役割タグ (MODULATOR / CARRIER / NOISE)、`ControlSpec` 駆動のノブ群、`EnvelopeDisplay`、SLOT/AMS トグル、ハイライト | `setRole`, `setHighlightedParameters` | MainComponentTest |
| `EnvelopeDisplay` | TL 単位でのエンベロープ描画 (`computeShape` は純関数) | `setYM2151Parameters`, `computeShape`, `timeForRate` | EnvelopeDisplayTest |
| `AlgorithmDisplay` | `resources/algorithms/*.svg` (バイナリデータ) の表示、フィードバック > 0 で `_fb` 版 | `setAlgorithm`, `setFeedbackLevel` | MainComponentTest |
| `MotionStrip` | Detail のモーション行: LFO / ENVELOPE / SPACE / PLAY の 4 カード。Sync 中はレート系ノブが分割コンボに置き換わる。"…" で `ArpSettingsPanel` をコールアウト表示 | `preferredHeight` | MainComponentTest |
| `ArpSettingsPanel` | コード表、オクターブ、リトリガー、ゲート、ラッチ、アクセント | – | MainComponentTest |
| `LfoNoiseStrip` | フッター: ハード LFO (rate/AMD/PMD/波形)、ノイズ、Expressive MIDI トグル、ステータス行 | `setStatusText` | MainComponentTest |
| `RotaryKnob` | 4 サイズのロータリー。ドラッグ、Shift で微調整、ホイール、ダブルクリックで生値入力、表示フォーマッタ、TL 用の反転アーク、ハイライトリング | `setValue`, `setRange`, `setValueFormatter`, `setInverted`, `setHighlighted`, `beginTextEntry`, `applyTypedValue` | RotaryKnobTest |
| `KnobBinding` | 非表示 `juce::Slider` + `SliderAttachment` で `RotaryKnob` をパラメータに結合し、ジェスチャを `beginChangeGesture`/`endChangeGesture` に流す | `attach` | RotaryKnobTest, MainComponentTest |
| `UiTheme.h`, `YmLookAndFeel` | 配色 (キャリア青、モジュレータ紫、エンベロープ緑、フォーカス琥珀) とフォント、ダークな LookAndFeel | – | – |

### src/utils

| クラス | 責務 | 主な公開メソッド | テスト |
|---|---|---|---|
| `PresetManager` (`PresetManagerInterface`) | ファクトリ音色 (`FACTORY_VOICES`)、同梱 Collection バンク、.opm 取り込み (ユーザーデータ下 `banks/` へコピー、`imported-banks.xml` に記録)、ユーザーバンク (`user-presets.xml`)。ユーザーデータは `userApplicationDataDirectory/YMulator-Synth`。テスト向けに `setUserDataDirectoryOverride` | `initialize`, `loadOPMFile`, `savePresetAsOPM`, `addUserPreset`, `getBanks`, `getGlobalPresetIndex`, `getUserDataDirectory` | PresetManagerTest |
| `VOPMParser` | .opm のパースと出力、値の範囲検証 | `parseFile`, `parseContent`, `validate`, `voiceToString` | VOPMParserTest |
| `ParameterIDs.h` | `ParamID::Global / Motion / Macro / Op / MIDI_CC / Validation`。`ParamID::Channel` (チャンネル別 pan/ams/pms) は定義だけで使用箇所がない | `Op::tl(n)` など | MidiCcMappingTest |
| `Debug.h` | `CS_DBG`, `CS_FILE_DBG`, `CS_ASSERT_*`。`JUCE_DEBUG` でのみ有効 | – | – |

## 4. 音声処理の流れ

### processBlock (`PluginProcessor.cpp`)

1. 出力バッファをクリアする。
2. `MidiProcessor::processMidiMessages`: ノート on/off、CC、チャンネルプレッシャ (Expressive 時は Brightness)、ピッチベンド。
3. `getPlayHead()` から BPM / ppq / 再生中を取り、`MotionEngine::setTransport` に渡す (ブロックに 1 回)。
4. `ParameterManager::updateYmfmParameters`: グローバル (ALG/FB、LFO、AMS/PMS、ノイズ) とオペレータ 12 種 × 4 の現在値を前回書き込み値と比べ、変わったものだけを 8 チャンネル分書き込む。
5. `generateAudioSamples`: 64 サンプル (`MotionEngine::kChunk`) ごとに `MotionEngine::tick` → `YmfmWrapper::generateSamples`。

`generateSamples` はリサンプラのループで、必要な回数だけ `renderNativeSample` を呼ぶ。そこでは main chip を 1 サンプル進め、Wide または Echo が有効なら `flushEcho` の後に shadow chip も進めてミックスする (Wide Centre かつ Echo なしのときは両チップとも −3 dB)。4 点の履歴から Catmull-Rom 補間でホストレートの出力を作る。チップは 3.579545 MHz / 64 = 55,930 Hz で動く。

### ノートオン

1. モノまたはアルペジオが有効なら `HeldNotes` に加える。既に発音中のチャンネルがあれば、モノでは `retuneChannel` (レガート)、アルペジオではリストに加わるだけで戻る。
2. それ以外は `VoiceManager::allocateVoiceWithNoisePriority`。`noise_enable` が有効な音色は ch7 のみ (使用中なら奪う)、それ以外は 7→0 の空きを探し、満杯なら OLDEST を奪う。
3. `YmfmWrapper::noteOn`: Echo が L/R 配置なら shadow 側のパンを左右交互に決め、`writePitch` (KC/KF、ベンドとモーションオフセット込み)、`applyVelocityToChannel` (キャリアを最大 32 TL ステップ減衰、`velocityBrightness` に応じてモジュレータも)、キーオン `0x08` に `keyOnBitsForSlotMask(slotMask) | channel`。

ノートオフはモノ/アルペジオでは最後のキーが離れたときだけチャンネルを解放し、それ以外は `getChannelForNote` で見つけたチャンネルに `noteOff` と `releaseVoice` を行う。

### プリセット読み込み

`setCurrentProgram` → `StateManager::loadPresetInternal`:

1. 現在のツリーを `lastSavedState` に控える。
2. `MacroMapper::setSuspended(true)` で写像と再基準化を止める。
3. `ParameterManager::loadPresetParameters` でツリーに書き、`applyPresetToYmfm` でチップにも直接書いて `invalidateRegisterCache`。
4. `MacroMapper::captureAnchor` (アンカー = 今の生値、マクロは中央へ)。
5. `setCustomMode(false)`、`currentBankIndex` / `currentPresetInBank` プロパティを更新。

生成 (`PatchWorkspace::generate`) も同じ順序で `applyPatch` → `captureAnchor` を行い、こちらはカスタムモード ("Generated") にする。

### パラメータ変更

- UI: `RotaryKnob` → `KnobBinding` の非表示スライダー → `SliderAttachment` → パラメータ。ジェスチャ中の変更を `ParameterManager::parameterValueChanged` が見てカスタムモードに入る (ジェスチャなしの変更、たとえばホストオートメーションではカスタムにならない)。
- チップへの反映は次の `processBlock` の `updateYmfmParameters` で行う。
- `MacroMapper` は APVTS リスナーとして `macro_*` の変化で生値を書き、生値の直接編集でアンカーを再基準化する。アルゴリズム変更だけでは再写像しない。
- `MainComponent` は algorithm / feedback / noise_enable を監視し、非同期に役割表示を更新する。

### 状態の保存と復元

`StateManager::getStateInformation` は APVTS の `copyState` に次を加えて XML にする。

| 内容 | 書き手 |
|---|---|
| 全パラメータ値 | APVTS |
| `macroAnchor` 子ノード (tl/ar/d1r/d2r/rr/dt1/mul × 4, feedback) | `MacroMapper::writeAnchorTo` |
| `currentPreset`, `isCustomPreset`, `customPresetName` | `StateManager` |
| `currentBankIndex`, `currentPresetInBank` | `StateManager` / `PluginProcessor::setCurrentPresetInBank` |
| `uiViewMode` ("quick" / "detail") | `MainComponent` |
| `generator` 子ノード (category と 6 方向) | `GeneratorPanel` |

復元は `MacroMapper` を止めて `replaceState` し、`restoreFromState` (ノードがなければ現在値を取り込む)、`currentPreset` とカスタムモードを戻す。

## 5. スレッドと制約

- 音声スレッドで動くもの: MIDI 処理、ボイス割当て、`MotionEngine::tick`、レジスタ書き込み、ymfm のレンダリングとリサンプル。ここではメモリ確保もファイル I/O も行わない。`CS_DBG` / `CS_FILE_DBG` はリリースビルドでは空になる (デバッグビルドの `CS_FILE_DBG` はファイルに書くので、音声スレッドの計測には使わない)。
- `MotionEngine` は `bindParameters` で取得した `RangedAudioParameter*` から値を読み、ツリーには触らない。Echo キューは固定長配列 (`PendingWrite` × 8192) で確保済み。
- MIDI CC は音声スレッド上で `setValueNotifyingHost` を呼ぶ。このとき `ParameterManager`、`MacroMapper`、`PatchWorkspace` のリスナーが同じスレッドで同期的に呼ばれる。
- UI → 音声: すべて APVTS のアタッチメント (`SliderAttachment` / `ComboBoxAttachment` / `ButtonAttachment`) 経由。
- 音声/プロセッサ → UI: `MainComponent` は `AsyncUpdater`、`PresetUIManager` は ValueTree のプロパティ (`presetListUpdated` など) をリスナーで受け、メッセージスレッド外なら `callAsync` で処理する。カスタムモードはプロパティを持たないので 5 Hz のタイマーで読む。`QuickView` は 4 Hz で表示を更新し、プレビューは音色パラメータの署名が変わったときだけ再レンダリングする。
- `PatchPreview` はメッセージスレッドで自前の `YmfmWrapper` を使い、再生中のチップには触れない。
- トランスポート: `processBlock` の先頭で `getPlayHead()->getPosition()` を読み、`MotionEngine::setTransport(bpm, ppq, playing, known)` に渡す。再生中はブロック内の位置を ppq から計算し、停止中は最後の BPM で自走する。
- `MotionEngine` の `onLatchOff` は `MidiProcessor::releaseLatchedNotes` に結ばれ、ラッチ解除時にラッチされた和音を離す。

## 6. UI の構成

`PluginEditor` は `MainComponent` を貼るだけである。`MainComponent` は最初のメンバとして `YmLookAndFeel` を持ち、ヘッダ (Quick / Detail ボタン、`PresetUIManager` のバンク・プリセットコンボと Save、edited タグ) の下に、選ばれたビューを表示する。ヘッダにパン制御はない。ビューの選択は状態ツリーの `uiViewMode` に保存され、次にエディタを開いたときに復元される。

Quick ビュー (`QuickView`):

- GENERATE カード: `GeneratorPanel` (カテゴリ Bass/Lead/Brass/E.Piano/Bell/Pad/SE/Any と 6 方向スライダー) と New sound / Undo / A / B ボタン。処理は `PatchWorkspace`。
- TONE: マクロ 6 個 (Brightness, Harmonics, Attack, Decay, Release, Spread)。
- ALGORITHM カード: `AlgorithmDisplay`、前後ボタン、説明文、Feedback ノブ (これはマクロではなく `feedback` レジスタそのもの)。
- MOTION カード: `MotionPanel` (機能チップ、量のノブ、パン/アルペジオのコンボ、Sync)。
- OUTPUT カード: `OutputScope`。`PatchPreview` が 0.5 秒保持 + 0.5 秒リリースを描画元として与える。

Detail ビュー:

- `ToneStrip`: マクロ 6 個 + ALG コンボ + `AlgorithmDisplay` + Feedback ノブ。マクロに触れると `onMacroFocus` → `MainComponent::setMacroFocus` → `MacroMapper::targetsOf` で対象の生ノブに琥珀色のリングが付く。
- `OperatorPanel` × 4: 役割タグ (アルゴリズムから MODULATOR / CARRIER、noise_enable 時の Op4 は NOISE)、`ControlSpec` の表から生成したノブ、`EnvelopeDisplay`、SLOT / AMS トグル。
- `MotionStrip`: LFO / ENVELOPE / SPACE / PLAY の 4 カード。Sync が入るとレートノブの位置に分割 (1/1 … 1/16T) のコンボが出る。"…" ボタンで `ArpSettingsPanel` をコールアウト表示。
- `LfoNoiseStrip`: ハード LFO、ノイズ、Expressive MIDI トグル、ステータス行。

ノブの結合は `KnobBinding::attach` に集約されている。`RotaryKnob` は Large / Primary / Small / Tiny の 4 サイズで、ドラッグ、Shift で微調整、ホイール、ダブルクリックで生値を直接入力できる。表示用フォーマッタとサブラベルで人向けの値 (level 0–100 など) を出しつつ、内部の範囲はレジスタ値のままである。配色は `UiTheme.h` にまとまり、`YmLookAndFeel` がコンボ・ボタン・トグル・スライダーの描画を上書きする。

## 7. ビルド・テスト・ツール

### ターゲットとオプション

| ターゲット | 内容 |
|---|---|
| `YMulator-Synth` | `juce_add_plugin` (AU, AUv3, VST3, Standalone)。ymfm のソースを直接コンパイル |
| `YMulator-Synth_Resources` | `juce_add_binary_data`: 同梱 Collection の .opm と `resources/algorithms/` の SVG 16 枚 |
| `YMulatorSynthAU_*Tests` | gtest バイナリ (下表)。`BUILD_TESTS` かつ GTest が見つかったときのみ |
| `YMulatorSynthAU_UISnapshot`, `YMulatorSynthAU_SongRender` | 開発ツール (`tools/`)。テストと同じ条件で構成される |

オプション: `BUILD_TESTS` (ON)、`YMULATOR_COPY_PLUGIN` (ON、ビルド後にユーザーのプラグインフォルダへコピー)、`BUILD_STANDALONE` (OFF だが参照されておらず、Standalone は常にビルドされる)。JUCE は `cmake/JUCEConfig.cmake` の FetchContent (9.0.1) で取得し、Debug 構成では `JUCE_DEBUG=1` が付く。

### テストバイナリ (`tests/CMakeLists.txt`)

| バイナリ | 内容 |
|---|---|
| `BasicTests` | PluginBasic, SimpleParameter, ParameterDebug, MultiInstance, MidiCcMapping, AlgorithmInfo |
| `PresetTests` | PresetManager, VOPMParser, StateManager |
| `ParameterTests` | ParameterManager, ParameterStateIntegration, MacroMapper, PatchGenerator, PatchWorkspace |
| `PanTests` | PanMotion, Wide, Echo |
| `IntegrationTests` | PluginProcessorComprehensive, VoiceManager, YmfmWrapper, ComprehensiveIntegration |
| `UITests` | MainComponent, EnvelopeDisplay, RotaryKnob |
| `QualityTests` | AudioQuality, PitchAccuracy, OperatorSlotOrder, RegisterGolden, RegisterUpdate, SlotEnable, LfoWiring, ParameterReach, Velocity, PatchPreview, MotionEngine, MonoArp |
| `PerformanceTests` | PerformanceRegression |
| `Tests` | 上記すべてを 1 本にまとめたもの |

`tests/test_main.cpp` は `--gtest_list_tests` のときは JUCE を初期化せず、通常実行では `ScopedJuceInitialiser_GUI` を立て、`PresetManager::setUserDataDirectoryOverride` で一時ディレクトリにユーザーデータを隔離する。`tests/mocks/MockAudioProcessorHost` がホストの代わりになる。

ビルドされないファイル: `tests/unit/MidiProcessorTest.cpp` (CMake でコメントアウト)、`tests/mocks/MockBinaryData.*`、`tests/standalone/` (存在しない `src/dsp/UnisonEngine.cpp` を参照しており、どこからも `add_subdirectory` されない)。

### CI (`.github/workflows/`)

- `pr-tests.yml`: `main` / `develop` への PR で `src/`, `tests/`, `third_party/`, `scripts/`, `CMakeLists.txt` が変わったとき。macOS で `./scripts/build.sh setup` → `debug` → 統合バイナリ `YMulatorSynthAU_Tests` を実行、`auval` は失敗しても続行。Linux では RelWithDebInfo でテストターゲットだけを 4 並列で組み、`xvfb-run` で 7 本の分割バイナリを回す。
- `build-cross-platform.yml`: `v*` タグまたは手動起動。Windows (VST3)、macOS (AU / VST3 / Standalone + 分割テスト 6 本)、Linux (VST3 / Standalone + xvfb でテスト) を組み、`create-release` が成果物をまとめて GitHub Release を作る。

### ツール (`tools/`)

- `ui_snapshot.cpp` (`YMulatorSynthAU_UISnapshot`): エディタをオフスクリーンで PNG に描く。`./bin/YMulatorSynthAU_UISnapshot --out ui.png --preset 2 --view detail`。`--dump`, `--focus-macro N`, `--note N`, `--param id=value`, `--arp-panel`, `--state-file` なども使える。
- `song_render.cpp` (`YMulatorSynthAU_SongRender`): 標準 MIDI ファイルをトラックごとに 1 インスタンスで鳴らして WAV に書く。`./bin/YMulatorSynthAU_SongRender --midi song.mid --out song.wav --program 1=10 --motion 2=2`。`SONG_RENDER_DEBUG=1` で MIDI のあったブロックごとにレジスタを出力する。
- `gen_algorithm_svg.py`: `resources/algorithms/algorithm0..7.svg` と `_fb` 版を生成する。`python3 tools/gen_algorithm_svg.py` をリポジトリルートで実行し、`AlgorithmInfo.h` を変えたら再生成する。

## 8. 設計文書へのリンク

- `ymulatorsynth-adr.md`: ADR-001 (JUCE)、ADR-002 (ymfm)、ADR-004 (AU の版)、ADR-005 (MIDI)、ADR-006 (.opm)、ADR-007 (チャンネル割当て)、ADR-009 (スレッドモデル)、ADR-010 (Quick/Detail とマクロの相対写像)、ADR-011 (0.1.2 の変更: パン設定の統合、キャリア限定マクロ、JUCE 9、Wide + Echo)。ADR-003 (S98 録音) と ADR-008 (レイテンシーモード) は未実装の判断として残っている
- `ymulatorsynth-quick-panel-design.md`: マクロ写像、ジェネレータ、Undo / A/B、OUTPUT スコープ
- `ymulatorsynth-motion-design.md`: `MotionEngine` の各機能と Wide / Echo の shadow chip
- `ymulatorsynth-technical-spec.md`: CC / NRPN 表、パラメータ ID と範囲、ノイズ、ボイス管理
- `ym2151-register-facts.md`: レジスタの事実と、それを固定するテスト
- `ymulatorsynth-vopm-format-spec.md`: .opm の読み書き
- `ymulatorsynth-implementation-guide.md`: セットアップ、ビルド、テストの回し方、トラブルシューティング
