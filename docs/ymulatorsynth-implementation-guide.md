# YMulator-Synth 実装ガイド

開発環境の準備、ビルド、テスト、Audio Unit の検証とトラブルシューティングをまとめたものです。記述は `CMakeLists.txt`、`cmake/JUCEConfig.cmake`、`scripts/build.sh`、`scripts/test.sh`、`tests/CMakeLists.txt`、`.github/workflows/*.yml` に合わせてあり、食い違いがあればそれらを正としてください。

関連ドキュメント:

- コンポーネントとスレッドの構成: `docs/ymulatorsynth-architecture.md`
- YM2151 のレジスタ仕様: `docs/ym2151-register-facts.md`
- ymfm の使い方と音作り: `docs/ymulatorsynth-ymfm-integration-guide.md`
- MIDI CC / NRPN、パラメータ変換: `docs/ymulatorsynth-technical-spec.md`
- `.opm` ファイル形式: `docs/ymulatorsynth-vopm-format-spec.md`
- 設計判断: `docs/ymulatorsynth-adr.md`

## 目次

1. [開発環境セットアップ](#1-開発環境セットアップ)
2. [テスト戦略](#2-テスト戦略)
3. [トラブルシューティング](#3-トラブルシューティング)
4. [ymfm 出力の扱い](#4-ymfm-出力の扱い)

## 1. 開発環境セットアップ

### 1.1 必要なもの

| 項目 | 内容 |
|------|------|
| CMake | 3.22 以上 |
| コンパイラ | C++17 対応。macOS は Xcode Command Line Tools、Windows は Visual Studio（CMake が自動検出）、Linux は build-essential |
| Git | ymfm をサブモジュールで取得する |
| Google Test | テストを組むときに必要。macOS は `brew install googletest`、Linux は `libgtest-dev`。見つからなければテストはビルドされない |
| JUCE | 9.0.1。`cmake/JUCEConfig.cmake` の FetchContent が configure 時に取得するため、初回はネットワークが必要（`build/_deps/juce-src` に置かれる） |
| Python 3 | `tools/gen_algorithm_svg.py` を使うときのみ |

Linux で JUCE をビルドするには X11 / ALSA / FreeType 系の開発パッケージが要ります。パッケージ名の一覧は `.github/workflows/pr-tests.yml` の `linux-tests` ジョブにあります。

### 1.2 取得とビルド

```bash
git clone https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth
./scripts/build.sh setup      # git submodule update --init --recursive
./scripts/build.sh build      # build/ に Release 構成で configure + build
```

`scripts/build.sh` のコマンド:

| コマンド | 内容 |
|----------|------|
| `setup` | サブモジュールの取得 |
| `configure` | `build/` に Release で configure |
| `build` | ビルド（`build/` がなければ configure から） |
| `debug` / `release` | 構成を指定して configure + build |
| `rebuild` | `build/` を消して Release で作り直す |
| `clean` | `build/` を消す |
| `test` | `ctest --output-on-failure` |
| `auval` | `auval -v aumu YMul Hrki` |
| `install` | `cmake --install .`（既定のインストール先は `build/install`） |

オプションは `-q` (出力抑制)、`-j N` (並列数)。`scripts/build-dev.sh` と `scripts/build-release.sh` は旧スクリプトで、`build.sh debug` / `build.sh release` に置き換わっています。

手動で行う場合:

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

ビルドディレクトリは `build/` のほか `cmake-build-*/` と `out/` が `.gitignore` に入っているので、IDE ごとに別のディレクトリを使えます。`.vscode/` には CMake Tools 用の設定（ビルドディレクトリ `build/`、Configure / Build / Test のタスク）が入っています。

### 1.3 CMake オプション

| オプション | 既定 | 内容 |
|------------|------|------|
| `YMULATOR_COPY_PLUGIN` | ON | ビルド後にプラグインをユーザーのプラグインフォルダへコピーする（JUCE の `COPY_PLUGIN_AFTER_BUILD`）。ホストがプラグインを読み込んでいる間にビルドすると上書きされるので、その場合は `-DYMULATOR_COPY_PLUGIN=OFF` で configure する |
| `BUILD_TESTS` | ON | `tests/` をビルドする（Google Test が見つかる場合） |
| `CMAKE_BUILD_TYPE` | (なし) | `Debug` で `JUCE_DEBUG=1` が定義され、`CS_DBG` などのデバッグ出力とアサートが有効になる |

### 1.4 成果物

`juce_add_plugin` のフォーマットは AU / AUv3 / VST3 / Standalone です。成果物は `build/src/YMulator-Synth_artefacts/<構成>/` 以下の `AU/`、`VST3/`、`Standalone/` に置かれます。Audio Unit の識別子は `aumu YMul Hrki`（PLUGIN_CODE `YMul`、MANUFACTURER_CODE `Hrki`）、バンドル ID は `jp.hiroaki.ymulatorsynth` です。

テストバイナリと開発ツールは `build/bin/` に置かれます。

### 1.5 CI

- `.github/workflows/pr-tests.yml`: `main` / `develop` への pull request で実行。macOS ジョブは `build.sh debug` の後に統合バイナリ `YMulatorSynthAU_Tests` を実行し、auval は失敗してもワークフローを止めない。Linux ジョブは RelWithDebInfo でテストターゲットだけを 4 並列でビルドし（無制限の並列ビルドはランナーのメモリを使い切る）、`xvfb-run` と `JUCE_FONT_PATH=/usr/share/fonts` の下で 7 つの分割バイナリを順に実行する
- `.github/workflows/build-cross-platform.yml`: `v*` タグの push で Windows (VST3)、macOS (AU / VST3 / Standalone)、Linux (VST3 / Standalone) をビルドし、GitHub Release を作る。macOS と Linux はリリースビルドの後にテストも実行する

## 2. テスト戦略

### 2.1 テストバイナリ

`tests/CMakeLists.txt` は 9 つの gtest バイナリを定義します。すべて同じ `test_main.cpp` と、プラグイン本体のソース一式（`COMMON_SOURCES`）を含みます。

| バイナリ | 内容 | ソース |
|----------|------|--------|
| `YMulatorSynthAU_BasicTests` | プラグインの基本動作、パラメータの読み書き、複数インスタンス、MIDI CC 対応表、アルゴリズム表 | `unit/PluginBasicTest`, `SimpleParameterTest`, `ParameterDebugTest`, `MultiInstanceTest`, `MidiCcMappingTest`, `AlgorithmInfoTest` |
| `YMulatorSynthAU_PresetTests` | プリセット管理、`.opm` パーサ、状態の保存と復元 | `unit/PresetManagerTest`, `VOPMParserTest`, `StateManagerTest` |
| `YMulatorSynthAU_ParameterTests` | パラメータ管理、パラメータと状態の連携、マクロ、ジェネレータ、ワークスペース (Undo / A/B) | `unit/ParameterManagerTest`, `ParameterStateIntegrationTest`, `tests/unit/MacroMapperTest.cpp`（`MacroMapperPureTest` / `MacroMapperProcessorTest`）, `PatchGeneratorTest`, `PatchWorkspaceTest` |
| `YMulatorSynthAU_PanTests` | パンモーション、Wide、Echo | `unit/PanMotionTest`, `WideTest`, `EchoTest` |
| `YMulatorSynthAU_IntegrationTests` | プロセッサ全体、ボイス管理、YmfmWrapper、コンポーネント間の統合 | `unit/PluginProcessorComprehensiveTest`, `VoiceManagerTest`, `YmfmWrapperTest`, `integration/ComprehensiveIntegrationTest` |
| `YMulatorSynthAU_UITests` | エディタ、エンベロープ表示、ノブ | `ui/MainComponentTest`, `EnvelopeDisplayTest`, `RotaryKnobTest` |
| `YMulatorSynthAU_QualityTests` | 音声品質と、チップ仕様を固定するテスト群（レジスタのゴールデン値、オペレータ順、スロット有効ビット、LFO 配線、音程精度、ベロシティ、パラメータの到達、レジスタ更新、プレビュー、Motion、モノ / アルペジオ） | `AudioQualityTest`, `unit/PitchAccuracyTest`, `OperatorSlotOrderTest`, `RegisterGoldenTest`, `RegisterUpdateTest`, `SlotEnableTest`, `LfoWiringTest`, `ParameterReachTest`, `VelocityTest`, `PatchPreviewTest`, `MotionEngineTest`, `MonoArpTest` |
| `YMulatorSynthAU_PerformanceTests` | 処理時間の回帰 | `performance/PerformanceRegressionTest` |
| `YMulatorSynthAU_Tests` | 上記すべてを 1 つにまとめた統合バイナリ（`ctest` と CI の macOS ジョブが使う） | 全ソース |

`tests/mocks/MockAudioProcessorHost` がホストの代わりにプロセッサを準備・駆動するので、テストは DAW なしで動きます。

### 2.2 実行方法

```bash
# scripts/test.sh: 分割バイナリを並列に実行（既定）
./scripts/test.sh
./scripts/test.sh --split-seq            # 分割バイナリを順に
./scripts/test.sh --unified              # 統合バイナリを ctest 経由で
./scripts/test.sh -f "ParameterManager"  # 名前でフィルタ（統合バイナリ）
./scripts/test.sh --gtest-only           # gtest の行だけ表示
./scripts/test.sh --build                # 先にビルド
./scripts/test.sh --auval                # auval のみ

# ctest: 統合バイナリの全テストが登録されている
cd build
ctest --output-on-failure
ctest -R "PitchAccuracyTest" --output-on-failure
ctest -N                                 # 一覧

# バイナリを直接実行（最も速い）
./bin/YMulatorSynthAU_QualityTests --gtest_filter="RegisterGoldenTest.*"
./bin/YMulatorSynthAU_PanTests --gtest_brief=1
./bin/YMulatorSynthAU_Tests --gtest_list_tests
```

全テストを統合バイナリで通すと数分かかるので、開発中は該当する分割バイナリと `--gtest_filter` を使ってください。Debug ビルドでは `CS_DBG` の出力が標準エラーに出るため、CI と同じく `2> /dev/null` を付けると結果だけが読めます。

Linux ではエディタのテストに X ディスプレイが必要です。`xvfb-run -a ./bin/YMulatorSynthAU_UITests` のように実行し、フォントが見つからない場合は `JUCE_FONT_PATH` を設定します。

### 2.3 テストを書くときの規則

- 期待値はデータシート（YM2151 Application Manual）と参照実装（ymfm、VOPM の挙動）から取ります。今の実装の出力をそのまま期待値にしてはいけません。`RegisterGoldenTest`、`OperatorSlotOrderTest`、`SlotEnableTest`、`LfoWiringTest`、`PitchAccuracyTest` はマニュアルの図と数値を根拠にしており、`docs/ym2151-register-facts.md` にその対応があります
- テストが失敗したら、まず実装と仕様を調べます。テストを実装に合わせて書き換えるのは、実装が仕様どおりだと確認できたあとだけです
- 許容誤差 (`EXPECT_NEAR`) は技術的な理由（レジスタの量子化、浮動小数点の計算）がある場合に限り、理由をコメントに書きます。状態の保存と復元や整数パラメータの往復は `EXPECT_FLOAT_EQ` / `EXPECT_EQ` で厳密に比べます
- オペレータは Op1..Op4 の 1 始まりで、順序は M1, C1, M2, C2 です（`ParamID::Op::tl(1)` など）。Op0 は存在しません
- 新しいテストは内容に合う分割バイナリと、統合バイナリ `YMulatorSynthAU_Tests` の両方（`tests/CMakeLists.txt`）に追加します

### 2.4 テスト実行環境 (`tests/test_main.cpp`)

- `--gtest_list_tests` 付きで起動されたとき（CMake の `gtest_discover_tests` がビルド時に呼ぶ）は JUCE の初期化を飛ばして一覧だけ返す
- それ以外は `juce::ScopedJuceInitialiser_GUI` でメッセージスレッドを用意する（エディタやタイマーを使うテストのため。Linux では必須）
- `ScopedTestUserData` が一時ディレクトリを作り、`PresetManager::setUserDataDirectoryOverride` でユーザーデータの置き場をそこに向ける。バンクの取り込みやユーザープリセットの保存を行うテストが、開発者自身のプリセットに触れることはない。ディレクトリはプロセス終了時に削除される

### 2.5 開発ツール

テストと同じ構成でビルドされる補助ツールです（Google Test が見つかる場合のみ）。使い方の詳細は `tools/README.md` にあります。

- `tools/ui_snapshot.cpp` (`YMulatorSynthAU_UISnapshot`): エディタをオフスクリーンで描画して PNG に書き出す。`--preset` / `--bank` / `--view quick|detail` / `--note N` / `--focus-macro N` / `--dump` など。UI の変更をホストなしで確認する
- `tools/song_render.cpp` (`YMulatorSynthAU_SongRender`): 標準 MIDI ファイルをトラックごとにプロセッサへ通して WAV に書く。`--program`、`--opm` + `--voice`、`--param`、`--motion`、`--bpm` でトラックの設定を指定する。環境変数 `SONG_RENDER_DEBUG=1` を付けると、MIDI を含むブロックごとに発音チャンネルのレジスタを出力する
- `tools/gen_algorithm_svg.py`: `resources/algorithms/` のアルゴリズム図を `src/dsp/AlgorithmInfo.h` から生成する。変更後は再ビルドが必要

```bash
cd build
cmake --build . --target YMulatorSynthAU_UISnapshot YMulatorSynthAU_SongRender
./bin/YMulatorSynthAU_UISnapshot --out ui.png --view detail --preset 2
SONG_RENDER_DEBUG=1 ./bin/YMulatorSynthAU_SongRender --midi song.mid --out song.wav --program 1=10
```

## 3. トラブルシューティング

### 3.1 Audio Unit の検証 (auval)

```bash
auval -a | grep -i ymulator        # 登録されているか
auval -v aumu YMul Hrki            # 検証（詳細出力）
auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"
./scripts/test.sh --auval          # 同じことをスクリプトで
```

auval は macOS のみです。リリース前には必ず通します。

### 3.2 Audio Unit がホストに現れない、古い版が読まれる

- `YMULATOR_COPY_PLUGIN=ON` でビルドしていれば `~/Library/Audio/Plug-Ins/Components/` にコピーされている。OFF にしている場合は成果物を手で置く
- 登録キャッシュを更新する: `killall -9 AudioComponentRegistrar`。それでも古い版が読まれるならホストを再起動する
- ホストがプラグインを開いたままビルドすると、コピーの途中で読み込まれることがある。ホストを閉じてからビルドするか `-DYMULATOR_COPY_PLUGIN=OFF` を使う

### 3.3 ログとデバッグ出力

```bash
# AudioToolbox のログ（登録や読み込みの失敗を調べる）
log show --predicate 'subsystem == "com.apple.audio.AudioToolbox"' --last 5m

# クラッシュログ
ls ~/Library/Logs/DiagnosticReports/
```

プラグイン側のデバッグ出力は `src/utils/Debug.h` の `CS_*` マクロで、Debug ビルド (`JUCE_DEBUG`) でのみ有効です。

- `CS_DBG` は JUCE の `DBG`（標準エラー）
- `CS_FILE_DBG` はデスクトップ（なければ一時ディレクトリ）の `ymulator_debug.txt` に追記する。ホスト内で `prepareToPlay` やサンプルレートの受け渡しを調べるときに使う
- `CS_ASSERT_CHANNEL` などのアサートも Debug ビルドでのみ働く

生の `DBG` や `std::cout` は使わず、必ずこれらのマクロを使います（Release では消えます）。

### 3.4 ビルドエラー

| 症状 | 原因と対処 |
|------|------------|
| `cmake: command not found` | `brew install cmake`（macOS）。3.22 未満なら更新 |
| `No rule to make target 'Makefile'` | `build/` 以外で `cmake --build .` を実行している。`cd build` するか `./scripts/build.sh build` を使う |
| configure 中に JUCE の取得で止まる | FetchContent がネットワークを必要とする。オフラインでは初回 configure ができない。`build/_deps` を消すと再取得になる |
| `Google Test not found. Tests will not be built.` | `brew install googletest` / `apt-get install libgtest-dev` の後に configure し直す |
| `ymfm_opm.h` が見つからない | サブモジュール未取得。`./scripts/build.sh setup` |
| Linux で X11 / freetype のヘッダがない | `pr-tests.yml` の `linux-tests` にある `apt-get install` の一覧を入れる |
| Linux の CI ビルドが途中で落ちる | 並列数が多すぎてメモリ不足。`--parallel 4` 程度に抑え、必要なターゲットだけ組む |
| Windows でジェネレータの指定に失敗する | `cmake .. -A x64` のみ指定し、Visual Studio の版は CMake に任せる |
| ビルド後にホストで音が変わらない | 3.2 を参照。コピー先とキャッシュを確認 |

`./scripts/build.sh clean && ./scripts/build.sh rebuild` で大抵の中途半端な状態は解消します。

### 3.5 実行時の注意

- オーディオスレッド（`processBlock`）ではメモリ確保、ファイル I/O、ロックを行わない。MIDI 処理、Motion の tick、レジスタ書き込み、レンダリングはすべてこのスレッドで完結する
- パラメータ変更はキーオフや待ち時間なしにそのままレジスタへ書ける。変更分だけが即座に送られる（`docs/ymulatorsynth-ymfm-integration-guide.md` 3.7）
- 音が出ない、音程がずれる、音色が想定と違う場合のチェックリストは同ガイドの 5 章にある

## 4. ymfm 出力の扱い

ymfm の `ymfm::ym2151::output_data` はインターリーブではありません。`data[0]` が左、`data[1]` が右で、`generate(&output, 1)` を 1 回呼ぶごとに 1 サンプル分の L/R が得られます。`data[i * 2]` / `data[i * 2 + 1]` のように読むのは誤りで、右チャンネルに存在しないデータを読むことになります。

チップは 3.579545 MHz / 64 = 約 55,930 Hz でしか動かないため、`YmfmWrapper::generateSamples` がこのネイティブレートからホストのサンプルレートへ 3 次補間でリサンプリングします。`float` への変換は `1 / YM2151Regs::SAMPLE_SCALE_FACTOR` (1 / 32768) です。詳細は `docs/ymulatorsynth-ymfm-integration-guide.md` の 2.3 と 2.4 を参照してください。
