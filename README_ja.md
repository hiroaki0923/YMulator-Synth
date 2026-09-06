# YMulator Synth

*[English version](README.md)*

YMulator Synth は YM2151（OPM）の FM シンセサイザープラグインです。X68000 やアーケード基板で鳴っていたあのチップを ymfm で忠実に再現し、レジスタをいじるのではなく「音を作る」ためのインターフェースを載せました。macOS では Audio Unit / VST3 / スタンドアロン、Windows と Linux では VST3 として動きます。

![YMulator Synth Quick 画面](docs/images/screenshot.png)

*Quick 画面: 7 つの TONE マクロで音を整え、レシピから新しい音を作り、Motion で動きを足し、結果を目で確かめる。*

![YMulator Synth Detail 画面](docs/images/screenshot-detail.png)

*Detail 画面: 4 オペレーターの全レジスタ。マクロが動かすノブには印が付く。*

## できること

- **本物の OPM**: Aaron Giles の ymfm による 8 音の YM2151 エミュレーション。8 アルゴリズム、DT1 / DT2 / KS / フィードバック、4 波形のハードウェア LFO、チャンネル 8 のノイズジェネレーター。チップ本来の 55.9 kHz 出力をホストのレートにリサンプリングするので音程は正確です。
- **1 つの音に 2 つの見方**: **Quick** 画面は音楽の言葉で、**Detail** 画面はレジスタで。同じパラメータを見ているだけなので、切り替えても音は変わりません。
- **TONE マクロ**: Brightness / Harmonics / Attack / Decay / Release / Spread / Feedback が、読み込んだプリセットを基準にオペレーターを相対的に動かします。プリセットの性格を保ったまま押し広げられます。Detail でレジスタを直接編集すると、マクロの基準がそこに移ります。
- **RECIPE**: カテゴリ（Bass / Lead / Brass / E.Piano / Bell / Pad / SE / Any）と 6 本の方向スライダーを決めて Generate。Undo と A/B で前の音と聴き比べられます。
- **Motion**: 当時の定番の仕掛けを内蔵。Wide（少しずらした 2 台目のチップを反対側に。8 音のまま）、Echo（遅らせた小さめのコピーを左右交互に）、ディレイとライズ付きビブラート、音色 LFO、トレモロ、パンの動き、ドラム用の 2 段ピッチエンベロープ、明るさのスイープ、レベルエンベロープ、ポルタメント / レガート、チップアルペジオ、ベロシティ→明るさ。レートはホストのテンポに音価で同期できます。
- **OUTPUT**: いまの音を専用チップで鳴らし、鳴り始めからリリースまでの波形と包絡線を再生表示します。無音の音色は理由を表示します。
- **プリセット**: ファクトリー 8 音色と 64 音色のコレクション、VOPM 形式 `.opm` バンクの読み込み、自分の音色の保存。バンクとプリセットの選択は DAW のプロジェクトと一緒に保存されます。
- **MIDI**: 全レジスタの VOPMex 互換 CC、マクロと Motion の量の CC、モジュレーションホイールとアフタータッチのための Expressive モード。

## 動作環境

| プラットフォーム | 形式 | 必要なもの |
|---|---|---|
| macOS 10.13 以降、Intel / Apple Silicon | Audio Unit、VST3、スタンドアロン | AU または VST3 ホスト（Logic Pro、GarageBand、Ableton Live、Reaper など） |
| Windows 10 以降、64 bit | VST3 | VST3 ホスト（Ableton Live、FL Studio、Reaper など） |
| Linux、x86_64 | VST3、スタンドアロン | VST3 ホスト（Reaper、Ardour、Bitwig Studio など） |

## インストール

[Releases](https://github.com/hiroaki0923/YMulator-Synth/releases) からプラットフォームのパッケージをダウンロードして展開し、プラグインを所定のフォルダにコピーしてから DAW を再スキャンまたは再起動してください。

| パッケージ | コピー先 |
|---|---|
| `YMulator-Synth-macOS-AU.zip` | `~/Library/Audio/Plug-Ins/Components/`（または `/Library/Audio/Plug-Ins/Components/`） |
| `YMulator-Synth-macOS-VST3.zip` | `~/Library/Audio/Plug-Ins/VST3/`（または `/Library/Audio/Plug-Ins/VST3/`） |
| `YMulator-Synth-macOS-Standalone.zip` | `/Applications/` |
| `YMulator-Synth-Windows-VST3.zip` | `C:\Program Files\Common Files\VST3\` |
| `YMulator-Synth-Linux-VST3.tar.gz` | `~/.vst3/`（または `/usr/lib/vst3/`） |
| `YMulator-Synth-Linux-Standalone.tar.gz` | 任意の場所。バイナリを直接実行 |

macOS で Audio Unit がすぐに出てこないときは `killall -9 AudioComponentRegistrar` を実行するか、ログアウトして入り直してください。ソースからのビルドは [ビルド](#ビルド) を参照。

## はじめかた

1. インストゥルメントトラックに YMulator Synth を挿します。**ステレオ**トラックにしてください。Wide、Echo、パンの動きは左右を使います。
2. ヘッダーでバンクとプリセットを選びます。最初のバンクがファクトリー、次がコレクションです。
3. 弾きます。ベロシティはキャリアに効くので、弱い音でも音色は変わりません。
4. **TONE** のノブで音を整えます。ノブは上下ドラッグ。Shift を押しながらで 10 倍細かく、ホイールでも動きます。コントロールに触れるとツールチップが出ます。
5. **MOTION** カードのチップを押すと動きが加わり、下のノブで量を決めます。複数同時に ON にできます。
6. RECIPE カードの **Generate** で、選んだ方向の新しい音が出ます。**Undo** で前の音に戻り、**A / B** で切り替えて聴き比べられます。
7. レジスタを触りたくなったら **Detail** を押します。

### Quick 画面

- **RECIPE**: カテゴリと 6 本のスライダー（暗い / 明るい、単純 / 複雑、柔らかい / 硬いアタック、短い / 長い、静か / 動く、倍音的 / 金属的）で欲しい音を指定し、Generate で作ります。現在の音を置き換え、TONE のノブは中央に戻ります。プリセット欄は「Generated」になります。
- **TONE**: いま読み込まれている音（プリセットでも生成した音でも）に効く 7 つのマクロ。Brightness はモジュレーターのレベル、Harmonics は比率のテンプレート（Preset / Saw / Square / Pulse / Bright / Bell / Metal / Sub / Octave）、Feedback はオペレーター 1 のフィードバック、Attack / Decay / Release は全オペレーターのエンベロープ、Spread は DT1 のばらつき。プリセットの読み込みや生成で中央に戻ります。
- **ALGORITHM**: 8 つの接続を図で表示。説明付きで、フィードバックが有効なときはループを強調します。
- **MOTION**: 機能ごとのチップ（Wide / Vib / Growl / Echo / Sweep / Swell / Glide / Arp / Kick / Trem / Pan）。押すと妥当な値で ON になり、ノブで量を決めます。**Sync** を ON にするとレートがホストのテンポに追従し、Vib rate のノブが音価ボックスに変わります。ほかの音価は Detail 画面にあります。
- **OUTPUT**: いまの音色を目で再生。包絡線の上を再生位置が進み、その位置の波形を表示するので、アタック・ディケイ・リリースの違いが見えます。

### Detail 画面

- 上段に TONE 行が残ります。TONE のノブに触れると、それが動かすオペレーターのノブに琥珀色の輪が付きます。
- オペレーターの各行には役割（MOD / CARRIER、ノイズ有効時は NOISE）、主なノブ 3 つ（Level / Ratio / Detune。人間向けの単位で表示し、レジスタ値を添える）、エンベロープの図、エンベロープの 5 ノブ、KS / DT2 / AMS があります。スイッチでオペレーターを ON / OFF（.opm の SLOT マスク）。
- MOTION 行には Motion の全パラメータ: ビブラートのディレイ・ライズ・波形、Wide の量と配置（L / R か Center）、音色 LFO とトレモロの深さとレート、パンのモードとステップ、Echo のレベルと時間、Sweep、2 段ピッチエンベロープ、レガートとポルタメント時間、ベロシティ→明るさ、アルペジオのモードとステップ、レベルエンベロープ、LFO の波形とワンショット。Sync ON のときは各レートノブが音価ボックスに変わります。
- 最下段はハードウェア LFO（レート、AMD、PMD、波形）、ノイズジェネレーター（ON / OFF と周波数）、Expressive MIDI のスイッチです。

### プリセットと .opm ファイル

- **Factory**（8）: Electric Piano、Synth Bass、Brass Section、String Pad、Lead Synth、Organ、Bells、Init。
- **Collection**（64）: 定番の FM 音色。
- **読み込み**: Bank の「Import OPM File...」で VOPM 形式の `.opm` バンクを読み込みます。読み込んだバンクは `~/Library/YMulator-Synth/banks/`（macOS）に複製され、以後すべてのインスタンスに現れます。
- **保存**: 編集後に **Save** を押すと User バンクに新しいプリセットとして保存されます。保存される値はチップが鳴らしている値そのものです。
- レジスタを変えるとプリセット欄に EDITED タグが付き、Generate の後は「Generated」と表示されます。

## MIDI

ピッチベンド（範囲 1〜12 半音、既定 2）、キャリアに効くベロシティ、Expressive モードでのチャンネルプレッシャーと CC 1、そして下のコントローラーに対応します。ノートは 8 チャンネルに動的に割り当てられ、足りなくなると一番古い音が奪われます。

### コントローラー（VOPMex 互換）

| CC# | パラメーター | 範囲 | 説明 |
|-----|------------|------|------|
| 14 | アルゴリズム | 0-7 | FM アルゴリズム選択 |
| 15 | フィードバック | 0-7 | オペレーター 1 のフィードバック |
| 16-19 | TL OP1-4 | 0-127 | オペレーターごとのトータルレベル |
| 20-23 | MUL OP1-4 | 0-15 | オペレーターごとの倍率 |
| 24-27 | DT1 OP1-4 | 0-7 | オペレーターごとのデチューン 1 |
| 28-31 | DT2 OP1-4 | 0-3 | オペレーターごとのデチューン 2 |
| 39-42 | KS OP1-4 | 0-3 | オペレーターごとのキースケール |
| 43-46 | AR OP1-4 | 0-31 | アタックレート |
| 47-50 | D1R OP1-4 | 0-31 | ディケイ 1 レート |
| 51-54 | D2R OP1-4 | 0-31 | ディケイ 2 レート |
| 55-58 | D1L OP1-4 | 0-15 | サステインレベル |
| 59-62 | RR OP1-4 | 0-15 | リリースレート |
| 70-73 | AME OP1-4 | 0-1 | AMS 有効 |
| 1 | LFO 周波数 | 0-127 | 8 ビット LFRQ の上位 7 ビット。最下位ビットは CC 33 |
| 2 | LFO PMD | 0-127 | ピッチ変調深度 |
| 3 | LFO AMD | 0-127 | 振幅変調深度 |
| 12 | LFO 波形 | 0-3 | Saw / Square / Triangle / Noise |
| 75 | LFO PMS | 0-7 | ピッチ変調感度（全チャンネル共通） |
| 76 | LFO AMS | 0-3 | 振幅変調感度（全チャンネル共通） |
| 80 | ノイズ有効 | 0 / 1-127 | チャンネル 8 のノイズ ON/OFF |
| 82 | ノイズ周波数 | 0-31 | NFRQ（81 も受け付け） |
| 102-107 | Quick マクロ | 位置 | Brightness / Harmonics / Attack / Decay / Release / Spread（64 が中央） |
| 110-118 | Motion の量 | 位置 | Wide / Vibrato / Timbre / Echo / Sweep / Swell / Porta / Pitch / Velocity brightness |
| 121 | Reset All Controllers | - | ナチュラルモードに戻し、マクロを中央へ |

マクロの CC は、レジスタ CC で決めた値を基準にした相対的な動きです。レジスタ CC を送るとマクロの基準がそこに移り、マクロ CC はその周りを動くので、両方を併用できます。Motion の CC はレジスタのパラメーターには触れません。Detail 画面の最下段にある **Expressive MIDI** をオンにすると、CC 1 がビブラートの深さ、アフタータッチが Brightness になります。

既定（VOPMex の「ナチュラル」モード）では CC の 0-127 をパラメーターの範囲に拡縮し、TL / AR / D1R / D1L / D2R / RR はレジスタと逆向き（CC 127 が最大音量 / 最速）になります。アナログシンセと同じ感覚です。NRPN 126/127 にデータ 127（CC 99=126、CC 98=127、CC 6=127）を送るとレジスタ値入力モードになり、CC 値がそのままレジスタ値になります。データ 0 でナチュラルモードに戻ります。

## ビルド

### 必要なもの

- CMake 3.22 以降と Git。
- **macOS**: Xcode Command Line Tools（`xcode-select --install`）。テストを動かすなら `brew install googletest`。
- **Windows**: Visual Studio 2022 以降と C++ ワークロード。
- **Linux**（Ubuntu / Debian）: `cmake build-essential git libgtest-dev libasound2-dev libjack-jackd2-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev`。

JUCE は CMake が取得します。ymfm はサブモジュールです。

### ビルド手順

```bash
git clone --recursive https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth
./scripts/build.sh setup     # 初回のみ
./scripts/build.sh build     # ほかに debug, release, rebuild, clean
```

macOS ではビルド時に AU と VST3 が `~/Library/Audio/Plug-Ins/` にコピーされ、DAW から見えるようになります。スクリプトを使わない場合:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release        # Windows: cmake .. -A x64
cmake --build . --config Release --parallel
```

### テスト

```bash
./scripts/test.sh              # 全テスト。分割バイナリを並列実行
./scripts/test.sh --filter Motion
./scripts/test.sh --auval      # Audio Unit の検証（macOS）
```

`build/` から直接なら `ctest --output-on-failure`、`bin/YMulatorSynthAU_*Tests` の各バイナリ、`auval -v aumu YMul Hrki`。テストはチップで音をレンダリングして確かめるので、パラメーターを露出させたまま配線し忘れたり、違うオペレーターを動かしたりするとテストが落ちます。

`bin/YMulatorSynthAU_UISnapshot` はホストなしでエディターを PNG に描画し（[tools/README.md](tools/README.md)）、`bin/YMulatorSynthAU_SongRender` は MIDI ファイルをプラグインで WAV に描画します。

## ドキュメント

- [CHANGELOG_ja.md](CHANGELOG_ja.md)（日本語）/ [CHANGELOG.md](CHANGELOG.md)（英語）
- [Quick 画面の設計](docs/ymulatorsynth-quick-panel-design.md)、[Motion の設計](docs/ymulatorsynth-motion-design.md)、[アーキテクチャ決定記録](docs/ymulatorsynth-adr.md)、[技術仕様](docs/ymulatorsynth-technical-spec.md)、[.opm 形式](docs/ymulatorsynth-vopm-format-spec.md)
- [開発状況](docs/ymulatorsynth-development-status.md)

## コントリビューション

Issue と Pull Request を歓迎します。フォークしてブランチを切り、テストを通したまま（`./scripts/test.sh`）`main` に向けて Pull Request を出してください。コードベースの決まりごとは `CLAUDE.md` にあります。

## ライセンス

GPL v3。[LICENSE](LICENSE) を参照してください。

- [JUCE](https://juce.com/) - GPL v3 / 商用
- [ymfm](https://github.com/aaronsgiles/ymfm) - BSD 3-Clause
- `.opm` ファイルは Sam 氏の VOPM と互換

## 謝辞

- ymfm を作った Aaron Giles 氏
- VOPM と .opm 形式を作った Sam 氏
- この音を今も鳴らし続けているチップチューンのコミュニティ

## サポート

- 不具合報告: [Issues](https://github.com/hiroaki0923/YMulator-Synth/issues)
- 質問: [Discussions](https://github.com/hiroaki0923/YMulator-Synth/discussions)
