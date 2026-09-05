# YMulator-Synth Motion（Layer 3）設計仕様

Quick パネル設計（[ymulatorsynth-quick-panel-design.md](ymulatorsynth-quick-panel-design.md)）の三層構成のうち、Layer 3「Driver FX」を本書で Motion と呼び、設計する。目的は、YM2151 の実機で音楽ドライバが担っていた**時間変化の表現**（唸り、揺れ、左右の動き）を、CC を手で操作せずにプリセットの一部として選べるようにすること。

## 0. 前提と方針

- **チップの外で音を加工しない。** Motion はレジスタ書き込みだけで実現する。ymfm の出力に対する後段処理（連続パン、コーラス、ディレイ）は採らない。実機で鳴らせない音は作らない。
- **Layer 1（生パラメータ）を汚さない。** Motion はプリセットの値を基準に「揺らす」だけで、APVTS の raw 値を書き換えない。マクロ（Layer 2）のアンカーとも干渉しない。
- **すべてオーディオスレッドで完結する。** Motion は制御レート（既定 64 サンプル、約 0.7 kHz）で走り、`YmfmWrapper::writeRegister` を直接呼ぶ。UI とのやり取りは APVTS パラメータのみ。
- **BPM 同期はホストのプレイヘッドから。** `AudioPlayHead::getPosition()` の BPM と拍位置を使い、停止中や情報が無いときは自由レートにフォールバックする。

## 1. 表現の棚卸し

実機時代のドライバが使っていた技法と、YM2151 で可能な実装。

| 表現 | 実機での手法 | 本設計での実装 | 備考 |
|---|---|---|---|
| 唸り（うねり） | 2 チャンネルを僅かにデチューンして重ねる。多くは片方を L、片方を R | **Wide**: ボイスごとに 2 チャンネル使用。KF（キー分数）で ±数セントずらし、パンを L / R に振る | 同時発音数は 8 → 4。これがユニゾンの本命 |
| 音色の揺れ（ワウ） | モジュレータの TL をソフト LFO で揺らす | **Timbre LFO**: モジュレータ TL へ三角波オフセット | ハードウェア LFO の AMS は全オペレータ共通で使いづらい |
| 遅延ビブラート | 発音後 N tick から徐々に深くなるビブラート | **Vibrato**: 遅延、立ち上がり、深さ、レート。KF へ書く | ハードウェア LFO の PMD は 8 チャンネル共通なので、ボイスごとの遅延ができない |
| トレモロ | TL 全体を揺らす | **Tremolo**: キャリア TL へ正弦オフセット | |
| 左右の動き | 発音ごとに L / R を切替、または拍ごとに L→C→R | **Pan Motion**: Alternate（発音ごと）、Step（拍同期で L→C→R→L…）、Wide（Wide と併用時は固定） | パンは 3 値のみ。連続移動は作らない |
| ピッチのアタック | 発音時に数十セント下（上）から滑り込む | **Pitch Env**: 初期オフセットと収束時間。KF へ書く | ポルタメントとは別 |

ハードウェア LFO（LFRQ / PMD / AMD / 波形）は既存のまま Detail の LFO 行で扱う。Motion のソフト LFO は「ボイスごと」「遅延あり」「BPM 同期」が必要な用途を受け持つ。

## 2. パラメータ

すべて APVTS パラメータ。プリセット（.opm）には保存できないため、DAW の状態には残るが .opm 書き出しでは失われる。ユーザーバンク（内部形式）には保存する。

| ID | 内容 | 範囲 |
|---|---|---|
| `motion_wide` | Wide の深さ（0 = OFF） | 0〜100（セント換算 0〜±25） |
| `motion_wide_pan` | Wide 時の 2 チャンネルの配置 | L/R, C/C |
| `motion_vib_depth` | ビブラート深さ | 0〜100（0〜±50 セント） |
| `motion_vib_rate` | ビブラート速さ | 0.5〜12 Hz、または音価 |
| `motion_vib_delay` | 発音からビブラート開始までの時間 | 0〜2000 ms |
| `motion_vib_rise` | 開始から最大深さまでの時間 | 0〜2000 ms |
| `motion_timbre_depth` | 音色 LFO の深さ（モジュレータ TL の振れ幅） | 0〜40 |
| `motion_timbre_rate` | 音色 LFO の速さ | 0.1〜12 Hz、または音価 |
| `motion_trem_depth` | トレモロ深さ（キャリア TL の振れ幅） | 0〜24 |
| `motion_trem_rate` | トレモロ速さ | 0.5〜12 Hz、または音価 |
| `motion_pan_mode` | パンの動き | Off / Alternate / Step |
| `motion_pan_rate` | Step の間隔 | 音価（1/4, 1/8, 1/16, 三連） |
| `motion_pitch_env` | 発音時のピッチオフセット | −100〜+100 セント |
| `motion_pitch_time` | 収束時間 | 0〜500 ms |
| `motion_sync` | レート系を BPM 同期にする | Off / On |

レートは `motion_sync` が On のとき音価（1/1〜1/32、付点・三連）として解釈し、Off のとき Hz として解釈する。同一パラメータで両方を表すため、UI 側が表示を切り替える。

## 3. 実装

### 3.1 MotionEngine（core）

```
class MotionEngine {
    void prepare(double sampleRate);
    void noteOn(int channel, ...);        // 位相と遅延タイマーをリセット
    void noteOff(int channel);
    void process(int numSamples, const juce::AudioPlayHead::PositionInfo*);  // 制御レートで tick
private:
    void tick();                          // 64 サンプルごと
    YmfmWrapperInterface& ymfm;
    per-channel state: phase, delayRemaining, riseProgress, baseKf, basePanBits
};
```

- `process()` は `processBlock` から呼ばれ、64 サンプル境界ごとに `tick()` を実行する。
- `tick()` はチャンネルごとに（1）ビブラート＋ピッチエンベロープの合成オフセットを KF に、（2）音色 LFO をモジュレータ TL に、（3）トレモロをキャリア TL に書く。値が前回と同じなら書かない（ステップ 0 の差分方針と同じ）。
- TL のオフセットは `ParameterManager` が書いた基準値に足す。基準値は `YmfmWrapper::readCurrentRegister` ではなく、ParameterManager が最後に書いた値を MotionEngine に渡す（Motion が書いた値を基準に取り込まないため）。
- KF は 6 ビット（1/64 半音 ≈ 1.56 セント）。±50 セントは ±32 ステップ。KF が 63 を超える／0 を下回るときは KC を繰り上げ／繰り下げる。

### 3.2 Wide（ユニゾン）

- `motion_wide > 0` のとき、VoiceManager は 1 ノートに 2 チャンネル（n と n+4）を割り当てる。同時発音は 4。
- チャンネル n は KF −d、n+4 は KF +d（d はセント→KF 換算）。パンは `motion_wide_pan` により L / R または両方 C。
- 既存のグローバルパン（LEFT / CENTER / RIGHT / RANDOM）は Wide が OFF のときだけ効く。
- **保留中のユニゾンブランチ（複数 ymfm インスタンス方式）は採用しない。** 1 チップ内の 2 チャンネルで実現でき、CPU も状態管理も軽い。ブランチは参照用に残し、マージしない。

### 3.3 Pan Motion

- Alternate: 発音ごとに L → R → L…。RANDOM パンと同じ経路（`PanProcessor`）でパンビットを書く。
- Step: BPM 同期で拍ごとに L → C → R → C → L…。発音中のチャンネルすべてに書く。
- Wide が ON のときは Pan Motion を無効にする（2 チャンネルの配置が優先）。

### 3.4 BPM 同期

- `processBlock` で `getPlayHead()->getPosition()` を取り、BPM と `ppqPosition` を MotionEngine に渡す。
- 音価を秒に換算して位相を進める。再生中は `ppqPosition` から位相を再計算し、ドリフトさせない。停止中は自由レートで進める。
- トランスポートが小節頭をまたいだとき、Step パンとソフト LFO の位相を 0 に揃える。

### 3.5 スレッドと差分

- MotionEngine はオーディオスレッド専用。パラメータは `RangedAudioParameter*` を通して読む（ParameterManager と同じ）。
- 1 tick あたりの最大書き込みは 8 チャンネル × (KF 1 ＋ TL 4 ＋ KC 1) = 48 レジスタ。差分により通常は数個。

## 4. UI（Quick の MOTION 枠）

Quick ビューの右下に MOTION カードを置く。内容は 3 段。

1. **プリセット行**: Off / Wide / Vibrato / Growl / Step Pan / Tremolo Pad などの定型を 1 クリックで選ぶ（各定型は 2 章のパラメータの組）。
2. **主ノブ 4 つ**: Wide、Vibrato、Timbre、Pan（Pan はセレクタ）。
3. **Sync トグルとレート**: Sync ON のとき音価セレクタ、OFF のとき Hz ノブ。

Detail には MOTION 行を追加しない。Detail の LFO 行はハードウェア LFO のままとし、ソフト LFO とは別物として扱う（両方使うと重なることを UI の注記で示す）。

## 5. テスト

- **KF 書き込み**: ビブラート深さ 50 セント、レート 1 Hz、遅延 0 で、1 秒間の KF の最大・最小が ±32 ステップ以内に収まり、位相が正しい向きで進むこと。
- **遅延**: 遅延 500 ms のとき、発音後 500 ms までは KF が基準値のまま。
- **Wide**: 2 チャンネルがそれぞれ −d / +d の KF を持ち、パンビットが L / R であること。同時発音 4 で 5 音目がボイススチールされること。
- **BPM 同期**: BPM 120、1/4 で、Step パンが 0.5 秒ごとに切り替わること（プレイヘッドのモックで検証）。
- **不変条件**: Motion を OFF にすると、書き込まれたレジスタが ParameterManager の基準値に戻り、`RegisterGoldenTest` が通ること。
- **差分**: Motion ON で音が鳴っていないとき、tick ごとの書き込みが 0 であること。

## 6. 実装順序

1. `MotionEngine` の骨組みと Vibrato（遅延・立ち上がり・深さ・レート、自由レート）
2. Wide（VoiceManager の 2 チャンネル割り当て、KF オフセット、L/R）
3. Timbre LFO と Tremolo
4. Pan Motion（Alternate、Step）と BPM 同期
5. Pitch Env
6. Quick の MOTION カードと定型

Quick ビュー（Quick パネル設計のステップ 3）は MOTION カードの領域を空けた状態で先に実装し、上記 1〜5 が済んでからカードを埋める。
