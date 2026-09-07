# YMulator-Synth Motion（Layer 3）設計仕様

**ステータス**: 実装済み（0.1.0〜0.1.2）。本書は設計の根拠と、それを実装しているクラスの対応を記す。
**関連**: [Quick パネル設計](ymulatorsynth-quick-panel-design.md)、[アーキテクチャ](ymulatorsynth-architecture.md)、[技術仕様書](ymulatorsynth-technical-spec.md)（MIDI CC）、[YM2151 レジスタの事実](ym2151-register-facts.md)

Quick パネル設計の三層構成のうち、Layer 3 を Motion と呼ぶ。目的は、YM2151 の実機で音楽ドライバが担っていた**時間変化の表現**（唸り、揺れ、左右の動き、分散和音、エコー）を、CC を手で操作せずにプリセットの一部として選べるようにすること。

## 0. 前提と方針

- **チップの外で音を加工しない。** Motion はレジスタ書き込みだけで実現する。ymfm の出力に対する後段処理（連続パン、コーラス、ディレイ）は採らない。実機で鳴らせない音は作らない。Wide と Echo は 2 つ目のチップインスタンス（シャドウチップ）で実現し、実機 2 枚と同じ制約に従う。
- **Layer 1（生パラメータ）を汚さない。** Motion はプリセットの値を基準に「揺らす」だけで、APVTS の raw 値を書き換えない。マクロ（Layer 2）のアンカーとも干渉しない。
- **すべてオーディオスレッドで完結する。** `MotionEngine` は制御レート（64 サンプル、`MotionEngine::kChunk`）で走り、`YmfmWrapperInterface` を直接呼ぶ。パラメータは `bindParameters()` で取ったハンドル（`RangedAudioParameter*`）を読むだけで、パラメータツリーには触れない。
- **BPM 同期はホストのプレイヘッドから。** `processBlock` が `AudioPlayHead` の BPM と拍位置を `setTransport()` で渡す。停止中や情報が無いときは最後のテンポで自走する。

## 1. 表現の棚卸し

実機時代のドライバが使っていた技法と、YM2151 で可能な実装。

| 表現 | 実機での手法 | 本設計での実装 | 備考 |
|---|---|---|---|
| 唸り（うねり） | 2 チャンネルを僅かにデチューンして重ねる。多くは片方を L、片方を R | **Wide**: シャドウチップに同じ音を ±25 セントまでずらして鳴らし、L / R か中央に置く | 同時発音数 8 を維持 |
| 音色の揺れ（ワウ） | モジュレータの TL をソフト LFO で揺らす | **Timbre LFO**: モジュレータ TL へのオフセット（正負両方向） | ハードウェア LFO の AMS は全オペレータ共通で使いづらい |
| 遅延ビブラート | 発音後 N tick から徐々に深くなるビブラート | **Vibrato**: 遅延、立ち上がり、深さ、レート。KC/KF へ書く | ハードウェア LFO の PMD は 8 チャンネル共通なので、ボイスごとの遅延ができない |
| トレモロ | TL 全体を揺らす | **Tremolo**: キャリア TL を減衰方向だけに揺らす | |
| 左右の動き | 発音ごとに L / R を切替、または拍ごとに L→C→R | **Pan**: Off / Alternate / Step / Left / Right / Random の 1 選択肢。Random は直前と違う側に置く | パンは 3 値のみ。連続移動は作らない |
| ピッチのアタック | 発音時に数十セント下（上）から滑り込む | **Pitch Env**: 2 段（開始点 → 第 2 点 → 本来の音程）、各 ±2400 セント。KC/KF へ書く | ドラムの沈み込み、SE のうねり |
| フィルタ開閉のようなパッド | モジュレータの TL を発音後ゆっくり下げて明るくしていく | **Sweep**: 発音時のモジュレータ TL オフセット（±40）と収束時間。二乗カーブ | |
| ソフトエンベロープ | TL を時間で動かす | **Level EG**: キャリア TL のアタック／ディケイ／サステイン減衰 | ハードウェア EG と加算 |
| LFO 波形・ワンショット | 三角以外の波形、1 周で止める | **Wave / 1shot**: ビブラートと音色 LFO に正弦・三角・ノコギリ・矩形・ランダム | |
| レガート・ポルタメント | 1 チャンネルで音程だけ変える、前の音から滑る | **Mono / Porta**: `HeldNotes` と `retuneChannel`。ポリでも直前の音からグライド | |
| 分散和音 | 1 チャンネルで押さえた音を高速に切り替える | **Arp**: Up / Down / Up Down / Random / As Played、音価は 1/64 まで、オクターブ、リトリガーとゲート、ラッチ、コードテーブル、アクセント | |
| 疑似リバーブ／エコー | 同じ音色をもう 1 チャンネルで遅らせて小さく鳴らす | **Echo**: シャドウチップへの書き込みを遅延キューに通し、キャリア TL を減衰。原音は自分のパンのまま、エコーは音ごとに左右交互 | 繰り返しは 1 回。Wide と併用可 |
| 強弱で音色も変える | ベロシティでモジュレータも下げる | **Vel → Bright**: 弱い音ほどモジュレータの TL も上げる | 既定のベロシティはキャリアだけ |

ハードウェア LFO（LFRQ / PMD / AMD / 波形）は Detail の `LfoNoiseStrip` で扱う。Motion のソフト LFO は「ボイスごと」「遅延あり」「BPM 同期」が必要な用途を受け持つ。両方使えば重なる。

## 2. パラメータ

すべて APVTS パラメータで、ID は `ParamID::Motion`（`src/utils/ParameterIDs.h`）、範囲と既定値は `ParameterManager::createParameterLayout`（`src/core/ParameterManager.cpp`）。ホストのプロジェクト（プラグイン状態）には残るが、`.opm` にもユーザーバンクにも保存されない（`Preset` 構造体はレジスタ値だけを持つ。[ADR-010](ymulatorsynth-adr.md#adr-010-quickdetail-2モード-ui-とマクロの相対写像方式)）。Undo / A/B のスナップショットには含まれる。

レートは Hz のパラメータと音価のパラメータ（`*_div`）を **別々に** 持つ。`motion_sync` が On のとき MotionEngine は音価を読み、UI はレートノブの位置に音価ボックスを出す。音価の選択肢 `divisions` は共通で、順に 1/1, 1/2, 1/4, 1/8, 1/16, 1/2T, 1/4T, 1/8T, 1/32, 1/64, 1/16T（インデックス 0〜10。拍数は `MotionEngine::beatsForDivision`）。

### 2.1 LFO

| ID | 内容 | 範囲 / 選択肢 | 既定 |
|---|---|---|---|
| `motion_vib_depth` | ビブラート深さ（100 = ±50 セント） | 0〜100 | 0 |
| `motion_vib_rate` | ビブラート速さ | 0.5〜12 Hz（0.1 刻み） | 5.0 |
| `motion_vib_div` | 同期時のビブラート音価 | divisions | 1/8 |
| `motion_vib_delay` | 発音からビブラート開始までの時間 | 0〜2000 ms（10 刻み） | 0 |
| `motion_vib_rise` | 開始から最大深さまでの時間 | 0〜2000 ms（10 刻み） | 300 |
| `motion_vib_wave` | ビブラート波形 | Sine / Triangle / Saw / Square / Random | Sine |
| `motion_timbre_depth` | 音色 LFO の深さ（モジュレータ TL の振れ幅、ステップ） | 0〜40 | 0 |
| `motion_timbre_rate` | 音色 LFO の速さ | 0.1〜12 Hz（0.1 刻み） | 1.0 |
| `motion_timbre_div` | 同期時の音色 LFO 音価 | divisions | 1/1 |
| `motion_timbre_wave` | 音色 LFO 波形 | Sine / Triangle / Saw / Square / Random | Triangle |
| `motion_trem_depth` | トレモロ深さ（キャリア TL の減衰幅、ステップ） | 0〜24 | 0 |
| `motion_trem_rate` | トレモロ速さ | 0.5〜12 Hz（0.1 刻み） | 5.0 |
| `motion_trem_div` | 同期時のトレモロ音価 | divisions | 1/8 |
| `motion_lfo_oneshot` | ビブラートと音色 LFO を 1 周で止める | Off / On | Off |
| `motion_sync` | レート系をホストテンポに同期する | Off / On | Off |

### 2.2 エンベロープ

| ID | 内容 | 範囲 / 選択肢 | 既定 |
|---|---|---|---|
| `motion_pitch_env` | 発音時のピッチオフセット | −2400〜+2400 セント（対称スキュー） | 0 |
| `motion_pitch_time` | 第 2 点に達するまでの時間 | 0〜500 ms | 60 |
| `motion_pitch_env2` | 第 2 点のピッチオフセット | −2400〜+2400 セント | 0 |
| `motion_pitch_time2` | 第 2 点から本来の音程までの時間 | 0〜1000 ms | 0 |
| `motion_sweep_amount` | 発音時のモジュレータ TL オフセット（− で明るく、+ で暗く始まる） | −40〜+40 ステップ | 0 |
| `motion_sweep_time` | パッチ本来の明るさに収束するまでの時間 | 50〜4000 ms（10 刻み） | 1500 |
| `motion_level_attack` | キャリアが −40 ステップから立ち上がる時間 | 0〜3000 ms（10 刻み） | 0 |
| `motion_level_decay` | そこからサステイン減衰量まで下がる時間 | 0〜3000 ms（10 刻み） | 0 |
| `motion_level_sustain` | ディケイ後に保つ減衰量 | 0〜40 ステップ | 0 |

### 2.3 空間

| ID | 内容 | 範囲 / 選択肢 | 既定 |
|---|---|---|---|
| `motion_wide` | Wide の深さ（100 = 2 チップ間 ±25 セント、0 = Off） | 0〜100 | 0 |
| `motion_wide_pan` | Wide 時の 2 チップの配置 | L / R, Center | L / R |
| `motion_echo_level` | エコーの大きさ（0 = Off、100 = 原音と同じ） | 0〜100 | 0 |
| `motion_echo_time` | エコーの遅れ | 10〜500 ms | 120 |
| `motion_echo_div` | 同期時のエコー音価 | divisions | 1/16 |
| `motion_pan_mode` | パンの配置と動き | Off / Alternate / Step / Left / Right / Random | Off |
| `motion_pan_rate` | Step の間隔 | divisions（Detail の UI は先頭 8 つ、1/8T まで） | 1/4 |

### 2.4 演奏

| ID | 内容 | 範囲 / 選択肢 | 既定 |
|---|---|---|---|
| `motion_mono` | モノ／レガート: 押さえた音は 1 チャンネルを音程変更で共有 | Off / On | Off |
| `motion_porta_time` | 直前の音からのグライド時間（0 = Off） | 0〜1000 ms（5 刻み） | 0 |
| `motion_vel_bright` | ベロシティでモジュレータも暗くする量 | 0〜100 | 0 |
| `motion_arp_mode` | アルペジオの順序 | Off / Up / Down / Up Down / Random / As Played | Off |
| `motion_arp_div` | 1 ステップの音価 | divisions | 1/64 |
| `motion_arp_octaves` | 押さえた音を上のオクターブにも重ねる数 | 1〜4 | 1 |
| `motion_arp_retrig` | ステップごとにキーオンし直す（Off はチップ流、音程だけ変える） | Off / On | Off |
| `motion_arp_gate` | リトリガー時にステップのうち鳴らす割合 | 10〜100 %（5 刻み） | 70 |
| `motion_arp_latch` | 鍵盤を離しても次の和音まで鳴らし続ける | Off / On | Off |
| `motion_arp_chord` | 単音に適用するコードテーブル | None / Major / Minor / 7th / m7 / Maj7 / Sus4 / Sus2 / Dim / Aug / 5th / Octave | None |
| `motion_arp_accent` | 音量を保つステップ | Off / Beat / 2 steps / 3 steps / 4 steps | Off |
| `motion_arp_accent_depth` | アクセント以外のステップが下がる量 | 0〜24 ステップ | 6 |

MIDI CC からは主な量だけを動かせる（CC 値をパラメータ範囲上の位置として写す）: 110 Wide、111 ビブラート深さ、112 音色 LFO 深さ、113 エコー、114 スイープ、115 Level EG アタック、116 ポルタメント、117 ピッチエンベロープ、118 Vel → Bright、108 コード、109 オクターブ、119 ゲート。詳細は[技術仕様書](ymulatorsynth-technical-spec.md)。

## 3. 実装

### 3.1 MotionEngine（core）

`src/core/MotionEngine.{h,cpp}`。`PluginProcessor::processBlock` が MIDI を処理したあと、64 サンプルごとに `tick(numSamples)` を呼び、続けて `YmfmWrapper::generateSamples` を呼ぶ。

- 書き込み口は 6 つ。`setChannelPitchOffset(ch, 半音)`（ピッチベンドと合算して `writePitch` が KC/KF を書く。KF は 6 ビット = 1/64 半音で、範囲を超えれば KC を繰り上げ／繰り下げ）、`setChannelLevelMotion(ch, キャリア分, モジュレータ分)`（`writeTotalLevel` が基準 TL ＋ ベロシティ減衰 ＋ Motion 分を clamp して書く）、`setChannelPan(ch)`、`setWide()`、`setEcho()`、`setVelocityBrightness()`。いずれも値が前回と同じなら書かない。
- TL の基準値は `ParameterManager` が `YmfmWrapper` に渡した `baseTotalLevel` で、Motion が書いた値を基準に取り込むことはない。
- チャンネルごとの状態（`Channel`）: 発音からの時間、各 LFO の位相と周期数、書いた音程オフセット・TL 分・パン、グライドの始点、ランダム波形のシード。`VoiceManager` のボイス状態と `YmfmWrapper::getNoteOnCount` で「新しい発音」「音程変更（レガート）」を検出し、新しい発音で時間と位相とパンのキャッシュを捨てる。
- 発音からの時間は、ビブラート・ピッチエンベロープ・スイープ・Level EG のどれかが有効なときだけ進める。
- 1 tick の合成順: Wide とエコーとベロシティ明るさの設定 → 拍位置の更新 → アルペジオ → チャンネルごとに、パン、音程オフセット（グライド ＋ ピッチエンベロープ ＋ ビブラート）、TL オフセット（スイープ／音色 LFO をモジュレータへ、Level EG／トレモロ／アクセントをキャリアへ）。
- 同じ対象を動かす機能が重なったときの現状: モジュレータ側は、音色 LFO が有効ならその値がスイープのオフセットを **置き換える**。キャリア側は、トレモロが有効ならその値が Level EG を置き換え、アルペジオのアクセントはその上に加算される。

### 3.2 Wide

`src/dsp/YmfmWrapper.cpp`。`WideTest`。

- `YmfmWrapper` が 2 つ目の `ymfm::ym2151`（シャドウチップ）とそのレジスタキャッシュを持つ。主チップへの全レジスタ書き込みをシャドウにも複製する（`writeShadow`）。音程レジスタ（KC/KF）だけは `writePitch` がチップごとに書く。
- Wide が ON のとき違うのは 2 点。音程は主 = 音程 − d、シャドウ = 音程 + d（d = `motion_wide` / 100 × 25 セント）。パン（レジスタ 0x20 の上位 2 ビット）は L / R 配置なら主 = L、シャドウ = R に差し替え（`panForChip`）、Center 配置なら両方ともレジスタの値のまま。
- 両チップの出力は `renderNativeSample` でミックスする。Center 配置（エコー無し）では二重になった分を 1/√2 で抑える。L / R では等倍。Wide も Echo も OFF ならシャドウチップはレンダリングしない（`shadowActive()`）。
- ペアリングは `YmfmWrapper` の内側に閉じるので、VoiceManager・ParameterManager・MacroMapper は Wide を知らない。同時発音数は 8 のまま（9 音目でボイススチール）。
- CPU: ymfm のレンダリングが 2 倍になるが、Wide / Echo が OFF なら増えない。

### 3.3 Pan

`MotionEngine::tick`、`PanMotionTest`。

- 配置と動きを 1 つの選択肢にまとめる。MotionEngine が毎 tick、チャンネルごとの目標パン（L / C / R）を決め、前回書いた値と違うときだけ `setChannelPan` でレジスタ 0x20 の上位 2 ビットを書く。発音のたびにキャッシュを捨てるので、新しいボイスには必ず書き直す。
- **Off**: 全チャンネル中央。**Left / Right**: 全チャンネル（休止中も）を片側へ。モードを変えると鳴っている音も動く。
- **Alternate**: 発音ごとに L → R → L…（全チャンネル共通のトグル）。鳴っている音は動かない。
- **Step**: 拍同期で L → C → R → C → L… を `motion_pan_rate` の間隔で。全チャンネルに書く。
- **Random**: 発音ごとに直前のランダム値と違う 2 つから選ぶ（xorshift、決定的）。同じ側が続かない。
- **Wide が ON で配置が L / R のとき**: 2 チップの配置が優先で、MotionEngine はモードに関係なく中央を書く。レジスタは中央を保ち、チップに届く実際のビットは `panForChip` が差し替える。こうしておくと Echo の原音はレジスタどおり中央から鳴り、エコーだけが左右に振れる（0.1.2 で修正）。配置が Center のときは両チップに同じパンが書かれ、Pan Motion が効く。
- 旧ヘッダの Global Pan（Left / Right / Random）は 0.1.2 でこのモードに統合した。

### 3.4 Echo

`YmfmWrapper`（`echoQueue`）、`MotionEngine::tick`、`EchoTest`。

- Echo が ON のとき、シャドウチップへの複製のうち「音を作る」レジスタ（キーオン/オフ 0x08、KC/KF、TL 32 本、0x20+ch のパン/ALG/FB）は `writeShadowNow` せず、`nativeSampleCount + 遅延` を期限にした遅延キュー（8192 エントリのリング、満杯なら捨てる）に積む。`renderNativeSample` が毎サンプル、期限の来たものをシャドウに流す（`flushEcho`）。それ以外のレジスタ（EG、MUL、LFO など）は即時に複製する。
- 遅延は `motion_echo_time`、同期時は `motion_echo_div` の音価を BPM から秒にしたもの。
- TL はキューに積むときにキャリアだけ減衰させる（`echoAttenuated`）: 減衰ステップ = round((100 − level) × 0.32)、level 100 で 0、level 1 で約 32 ステップ。レベルを変えたときは、鳴っている分のキャリア TL を即座に書き直す。
- パン: L / R 配置では原音（主チップ）は自分のパンのまま、エコー（シャドウ）はキーオンごとに右・左と交互に取る（`echoSide`）。原音を片側に寄せると定位が偏るため。Center 配置ではエコーも原音と同じ位置。Echo ON のときシャドウのミックスは等倍（エコーはすでに小さい）。
- Wide と併用できる。Wide のデチューンと Echo の遅延がどちらもシャドウチップに乗る。
- 繰り返しは 1 回。2 回以上はチップを増やす必要がある。
- Echo を OFF にしたときはキューを全部流し、シャドウのキャリア TL を主チップの値に戻す。

### 3.5 Vibrato / Timbre LFO / Tremolo

`MotionEngineTest`。

- **Vibrato**: 深さは 100 で ±50 セント。発音から `delay` までは 0、そこから `rise` かけて直線的に最大深さへ。位相は発音でリセットし、自由レートでは `rate` Hz で進め、同期時は拍位置から `beat / 音価` の小数部を位相にする（ドリフトしない）。波形 `waveform()`: Sine、Triangle（低い側から始まる）、Saw（上昇）、Square、Random（1 周期に 1 つの値を保持。チャンネルと発音回数からシードを取るので決定的）。
- **Timbre LFO**: モジュレータ TL へ ±深さ（ステップ）のオフセット。両方向に動くので Triangle 既定で「開いて閉じる」形になる。波形はビブラートと同じ 5 種。
- **Tremolo**: キャリア TL へ 0〜深さの減衰。`0.5 − 0.5·cos` で大きい側から始まるので、発音直後に音が引っ込まない。
- **1shot**: 自由レートのビブラートと音色 LFO は最初の 1 周期の終わりで位相を止める（Saw なら上がり切って保持、Sine なら 0 に戻って止まる）。同期時は無効（位相が拍から決まる）。
- 3 つの LFO の同期音価は別々（`motion_vib_div` / `motion_timbre_div` / `motion_trem_div`）。

### 3.6 Pitch Env

2 段: キーオン時 `pitch_env` から `pitch_time` かけて `pitch_env2` へ直線、続けて `pitch_time2` かけて 0（本来の音程）へ直線。`pitch_time2` が 0 なら第 2 点から即座に本来の音程へ。両方 0 なら何もしない。ビブラート、グライドと加算して 1 つの音程オフセットにする。Quick の Kick チップは +1200 → −500 → 0 の設定で沈み込みを作る。

### 3.7 Sweep

キーオン時にモジュレータ TL へ `sweep_amount` ステップのオフセット（+ で暗く、− で明るく始まる）を置き、`sweep_time` かけて 0 に収束する。カーブは残り時間の二乗（`remaining²`）で、フィルタエンベロープのように最初ゆっくり、最後に速く閉じる（開く）。

### 3.8 Level EG

キャリア TL に対する加算エンベロープ。アタック中は +40 ステップ（約 30 dB 下）から 0 へ直線で下りる（音が膨らむ）。その後 `level_decay` かけて 0 から `level_sustain` へ直線で上がり（音が引く）、以降 `level_sustain` を保つ。有効条件はアタック > 0 またはサステイン > 0（ディケイ単独では動かない）。ハードウェア EG と加算になるので、パッチ側の AR は速いままにしておくと形が読みやすい。

### 3.9 Mono / Legato / Portamento

`src/core/MidiProcessor.cpp`（`HeldNotes`）、`MonoArpTest`。

- **Mono**（`motion_mono`、またはアルペジオが ON のとき）: 押さえた音を `HeldNotes`（最大 16、新しい順）に積み、1 チャンネル（`held.channel`）で鳴らす。2 音目以降はそのチャンネルを `retuneChannel` で音程変更する（キーオンし直さない = レガート）。離鍵は、最後の 1 音を離したときだけキーオフ。鳴っている音を離してまだ他の音が残っていれば、残りのうち最新の音へ音程変更する。
- **Portamento**（`motion_porta_time` > 0）: 新しい発音は、直前に鳴った音（どのチャンネルでも）との音程差から始めて `porta_time` かけて直線で本来の音程へ。ポリでも効く。レガートの音程変更では、残っていたグライド分に新しい音程差を足して滑り続ける。アルペジオのステップはグライドしない。
- Mono は APVTS パラメータなので Quick の Glide チップ（Mono ＋ Porta 120 ms）でまとめて入る。

### 3.10 Vel → Bright

`YmfmWrapper::applyVelocityToChannel`。既定ではベロシティはキャリアの TL だけを最大 32 ステップ（`VELOCITY_TL_RANGE`）減衰させ、音色は変えない。`motion_vel_bright` を上げると、モジュレータの TL も `(1 − vel/127) × amount × 32 × 1.25` ステップ（最大 40）上がり、弱い音ほど暗くなる。キーオン時に決まり、発音中は変わらない。

### 3.11 Arpeggiator

`MotionEngine::runArpeggio`、`MidiProcessor`、`MonoArpTest`（`ArpeggiatorTest`、`ArpeggiatorTransportTest`）。

- 動作条件: `motion_arp_mode` が Off 以外で、`HeldNotes` に 1 音以上あり、そのチャンネルが決まっていること。MidiProcessor は Mono と同じ経路で押さえた音を `HeldNotes` に積む（アルペジオ中は音程変更しない）。
- **音の集合**: 押さえた音が 1 つでコードテーブルが None 以外なら、その音をルートにテーブル（半音: Major 0,4,7 / Minor 0,3,7 / 7th 0,4,7,10 / m7 0,3,7,10 / Maj7 0,4,7,11 / Sus4 0,5,7 / Sus2 0,2,7 / Dim 0,3,6 / Aug 0,4,8 / 5th 0,7 / Octave 0,12）を展開する。2 音以上ならそのまま。`octaves` で上のオクターブに複製（127 まで）。As Played 以外は昇順に並べる。結果が 2 音未満なら何もしない。
- **順序**: Up / As Played は順送り、Down は逆順、Up Down は 2n − 2 周期で折り返しの音を繰り返さない、Random は xorshift で直前と同じ音を選ばない。
- **ステップ**: 拍位置 ÷ 音価の整数部。`HeldNotes::version` が変わった（和音が変わった）とき、または拍位置が戻ったときにパターンを先頭から始める。開始ステップはステップ境界に量子化する: 和音がステップの後半で届いたら次のステップを起点にする（ホストは小節頭の音を数 ms 早く渡すため）。
- **音程変更 vs リトリガー**: 既定（Retrig Off）は各ステップで `retuneChannel` だけ（チップ流、EG は走り続ける）。Retrig On では新しい和音の最初のステップは MIDI のキーオンに任せ、以降のステップでキーオフ → キーオン。**Gate**（リトリガー時のみ）はステップの `gate` % が過ぎたところでキーオフし、次のステップに独自のアタックを与える。
- **Latch**: MidiProcessor が物理的に押されている鍵の数 `keysDown` を数える。ラッチ ON の離鍵は `HeldNotes` から消さず、チャンネルも鳴らし続ける。全部離した状態で次の音が来たら、和音を空にしてその音から新しい和音を始める。ラッチのパラメータを OFF にした瞬間、鍵が押されていなければ `onLatchOff` → `releaseLatchedNotes` がキーオフして和音を空にする。
- **Accent**: Beat なら拍頭のステップ（1 拍あたりのステップ数で割り切れるもの）、2/3/4 steps なら起点から n ステップごとが「アクセント」。それ以外のステップでは鳴っているチャンネルのキャリア TL を `accent_depth` ステップ上げる（Level EG / トレモロと加算）。
- 既定の音価は 1/64（120 BPM で約 31 ms）で、チップ流の高速アルペジオになる。

### 3.12 BPM 同期

- `processBlock` で `getPlayHead()->getPosition()` を取り、BPM、`ppqPosition`、再生中か、情報の有無を `setTransport()` に渡す。
- 再生中は tick ごとに `ppqAtBlockStart + ブロック内の経過サンプル` から拍位置を再計算し、ドリフトさせない。停止中や情報が無いときは最後の BPM（初期 120）で自走する。
- 同期した LFO の位相、Step パン、エコー時間、アルペジオのステップはすべてこの拍位置から決まる。トランスポートの位置に対して常に同じ位相になるので、小節頭の位相合わせは別途しない。

### 3.13 スレッドと差分

- MotionEngine はオーディオスレッド専用。パラメータは `bindParameters()` で取ったハンドルを `getValue()` で読む（ParameterManager と同じ）。
- 1 tick あたりの最大書き込みは 8 チャンネル × (KC/KF 2 ＋ TL 4 ＋ パン 1)。値が変わったときだけ書くので、通常は数個、音が鳴っていなければ 0。
- Wide / Echo / Vel → Bright の設定はチップ側にキャッシュがあり、変化したときだけ `YmfmWrapper` に渡す。

## 4. UI

### 4.1 Quick の MOTION カード（`MotionPanel`）

`src/ui/MotionPanel.{h,cpp}`。Quick ビュー右列、ALGORITHM カードと OUTPUT カードの間。

1. **機能チップ**（2 行、最後に Off）: 各チップは 1 つの「主パラメータ」が 0 以外なら点灯し、クリックで `toggleFeature(name)` が `onValues` / `offValues` をまとめて書く。定義は `MotionPanel::features()`。

| チップ | 主パラメータ | ON で書く値 | OFF で書く値 |
|---|---|---|---|
| Wide | `motion_wide` | Wide 50、配置 L / R | Wide 0 |
| Vib | `motion_vib_depth` | 深さ 35、5.5 Hz、遅延 200 ms、立ち上がり 350 ms、Sine | 深さ 0 |
| Growl | `motion_timbre_depth` | 深さ 18、0.7 Hz、Triangle | 深さ 0 |
| Echo | `motion_echo_level` | レベル 50、180 ms、音価 1/8 | レベル 0 |
| Sweep | `motion_sweep_amount` | +30、1500 ms | 0 |
| Swell | `motion_level_attack` | アタック 800 ms、ディケイ 0、サステイン 0 | アタック 0、サステイン 0 |
| Glide | `motion_porta_time` | Mono On、120 ms | 0 ms、Mono Off |
| Arp | `motion_arp_mode` | Up、1/64、Mono On | Off |
| Kick | `motion_pitch_env` | +1200 セント / 18 ms → −500 セント / 140 ms | 両方 0 |
| Trem | `motion_trem_depth` | 深さ 16、4 Hz | 深さ 0 |
| Pan | `motion_pan_mode` | Step、1/8、Sync On | Off |
| Off | — | `allOff()`: 全チップの OFF 値に加えて Sync、Vel → Bright、Mono、1shot を Off | |

2. **ノブ 10 個**（Small、2 行）: Wide、Vib、Vib rate、Timbre、Echo ／ Sweep、Swell、Porta、Pitch、Bright（= `motion_vel_bright`）。Sync ON のときは Vib rate ノブの場所に `motion_vib_div` の音価ボックスが出る。
3. **下段**: Pan モードボックス（Pan off / Alternate / Step / Left / Right / Random）、Arp モードボックス（Arp off / Up / Down / Up Down / Random / As played）、Sync トグル。

細かい設定（ディレイ、波形、エコー時間、Level EG の残り、アルペジオの詳細）は Detail に送る。

### 4.2 Detail の MOTION 行（`MotionStrip`）

`src/ui/MotionStrip.{h,cpp}`。OperatorPanel の下、`LfoNoiseStrip` の上。左端に MOTION ラベルと Sync トグル、右に 4 枚のカード。各カードは 2 段で、グループごとの見出し（小文字）をそのコントロールの真上に描く（上段はカード名の右、下段は段の間）。

| カード | 上段 | 下段 |
|---|---|---|
| LFO | **vibrato**: Depth / Rate / Delay / Rise、波形ボックス（Sin / Tri / Saw / Sqr / Rnd）、1shot トグル | **timbre**: Timbre / Rate / 波形 ・ **tremolo**: Trem / Rate |
| ENVELOPE | **pitch**: Env / Time / Env2 / Time2 | **sweep**: Sweep / Time ・ **level**: Atk / Dec / Sus |
| SPACE | **wide**: Wide、配置ボックス（L / R, Center） ・ **echo**: Echo / Time | **pan**: モード（Off / Alt / Step / Left / Right / Rnd）、Step の音価 |
| PLAY | **glide**: Legato トグル、Porta ・ **velocity**: Vel | **arpeggio**: モード（Off / Up / Down / UpDn / Rnd / Order）、音価、「…」ボタン |

Sync ON のとき、Rate ノブ（vibrato / timbre / tremolo）と Echo の Time ノブはそれぞれの `*_div` 音価ボックスに置き換わる。

### 4.3 アルペジオ設定（`ArpSettingsPanel`）

「…」ボタンから `juce::CallOutBox` で開く（344×150）。Chord と Accent のボックス、Oct / Gate / Depth のノブ、Retrig と Latch のトグル。すべて 2.4 のパラメータへの直接接続。

## 5. テストの観点

- **不変条件**: Motion をすべて OFF にすると、書き込まれたレジスタは ParameterManager の基準値に戻り、`RegisterGoldenTest` が通る。Motion ON で音が鳴っていないとき、tick ごとの書き込みは 0。
- **KF 書き込み**: 深さ 50 セント・遅延 0 で音程オフセットが ±0.5 半音に収まり、正しい向きで進む。遅延中は基準値のまま。
- **Wide**: 主チップとシャドウチップのレジスタが音程とパン以外で一致し、音程が −d / +d、パンビットが L / R。同時発音 8。OFF で出力が主チップ単独と一致。
- **BPM 同期**: BPM 120、1/4 で Step パンが 0.5 秒ごとに切り替わる（プレイヘッドのモック）。
- **アルペジオ**: 小節頭の直前に届いた和音がルートから始まり、Retrig で全ステップがキーオンされる。

## 6. 実装状況

すべて実装済み（0.1.0: Vibrato / Wide / Timbre LFO / Tremolo / Pan / Sync / Pitch Env / Echo / Sweep / Level EG / Mono / Porta / Vel → Bright / Arp / 波形と 1shot、0.1.1: Detail の 4 カード構成、0.1.2: パンモードの統合、Wide ＋ Echo の原音修正、見出しの配置）。

| 機能 | テスト |
|---|---|
| Vibrato（深さ、遅延・立ち上がり、発音ごとの遅延、波形、1shot、Random） | `tests/unit/MotionEngineTest.cpp`: NoVibratoLeavesPitchRegistersAlone, VibratoSwingsAroundTheNote, DelayAndRiseShapeTheOnset, EachNoteStartsItsOwnDelay, SawOneShotVibratoRampsUpAndHolds, RandomVibratoHoldsOneValuePerCycle |
| Timbre LFO / Tremolo | MotionEngineTest: TimbreLfoMovesModulatorsOnly, TremoloAttenuatesCarriersOnTopOfVelocity |
| Pitch Env（1 段、2 段、ビブラートとの加算） | MotionEngineTest: PitchEnvelopeSlidesOntoTheNote, PitchEnvelopeAndVibratoAddUp, TwoStagePitchEnvelopeForDrums |
| Sweep / Level EG | MotionEngineTest: SweepOpensTheModulatorsOverTime, LevelEnvelopeSwellsThenFallsToSustain |
| Wide | `tests/unit/WideTest.cpp`: OffKeepsBothChipsIdenticalAndTheOutputCentred, LeftRightSplitsTheChipsAndDetunesThem, CentreModeKeepsPanAndMixesBothChips, TurningWideOffRestoresThePansAndPitch, PolyphonyStaysAtEight |
| Echo | `tests/unit/EchoTest.cpp`: ShadowKeysOnAfterTheDelayWithQuieterCarriers, EchoIsAudibleOnTheOtherSideAndStopsWhenOff, EchoesTakeSidesInTurnWhileTheNoteKeepsItsPan, SyncedEchoFollowsTheTempoDivision, WideAndEchoFromTheFirstTickStillPlayTheNoteItself |
| Pan / Sync | `tests/unit/PanMotionTest.cpp`: AlternateSendsSuccessiveNotesLeftAndRight, StepFollowsTheHostBeat, OffPutsTheVoiceBackInTheCentre, LeftAndRightPlaceEveryVoice, RandomLandsEachNoteSomewhereElse, PresetChangeKeepsThePlacement, WideLeftRightWinsOverPanMotion, SyncedVibratoRunsAtTheTempo |
| Mono / Legato / Portamento | `tests/unit/MonoArpTest.cpp`: LegatoRetunesInsteadOfRetriggering, PortamentoGlidesFromThePreviousNote |
| Arpeggiator | MonoArpTest: ArpeggioCyclesHeldNotesOnOneChannel, UpDownLeavesOutTheTurningNotes, ANewChordRestartsThePattern, RetriggerKeysEveryStepAndTheGateLetsGo, OctavesExtendTheChord, AChordTableArpeggiatesASingleNote, LatchKeepsTheChordUntilTheNextOne, AccentSitsTheOffBeatStepsBack, AsPlayedFollowsTheOrderOfTheKeys, RandomStaysInsideTheChord, AChordJustBeforeTheBarStartsOnTheRootAndRetriggersEveryStep |
| Vel → Bright | `tests/unit/VelocityTest.cpp`: VelocityBrightnessDarkensTheModulators（既定でキャリアだけが減衰することの確認と併せて） |
| Layer 2 との境界 | `tests/unit/MacroMapperTest.cpp` はマクロが raw の 7 種（TL, AR, D1R, D2R, RR, DT1, MUL）と FB だけを動かすことを固定する。Motion が raw を書かないことは、上の各テストがレジスタ値で確認する |
| UI | `tests/ui/MainComponentTest.cpp`: MotionChipsToggleFeatures, EditorOpensWithEveryMotionSwitchAlreadyOn |
