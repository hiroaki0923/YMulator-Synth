# YMulator Synth

クラシックなYM2151 (OPM) チップの本格的なサウンドを、直感的な4オペレーター・インターフェースでDAWに提供する、モダンなFMシンセシス Audio Unit プラグインです。

![YMulator Synth Screenshot](docs/images/screenshot.png)

## 機能

### 🎹 本格的なYM2151 (OPM) エミュレーション
- **8ボイス・ポリフォニックFMシンセシス** Aaron Giles のymfmライブラリを使用
- **高精度エミュレーション** X68000やアーケードシステムのYM2151チップ
- **全8種類のFMアルゴリズム** 完全なオペレーター制御
- **完全なパラメーターセット**: DT1、DT2、Key Scale、Feedbackなど

### 🎚️ プロフェッショナルなインターフェース
- **4オペレーターFMシンセシス制御** 直感的なレイアウト
- **グローバルパラメーター**: アルゴリズム選択とフィードバック制御
- **グローバルパン制御**: LEFT/CENTER/RIGHT/RANDOMパンニングモード
- **オペレーター毎の制御**: TL、AR、D1R、D2R、RR、D1L、KS、MUL、DT1、DT2
- **SLOT有効/無効**: タイトルバーのチェックボックスによる個別オペレーターON/OFF制御
- **プリセット名の保持**: グローバルパン変更時もプリセット名を維持
- **リアルタイムパラメーター更新** 3ms未満のレイテンシー

### 🎵 プロフェッショナル機能
- **8つのファクトリープリセット**: Electric Piano、Bass、Brass、Strings、Lead、Organ、Bells、Init
- **64のOPMプリセット**: クラシックなFMサウンドのバンドルコレクション（⚠️ *現在サウンドデザイン調整中*）
- **完全なプリセット管理**: Bank/Preset デュアルComboBoxシステム、OPMファイルインポート
- **OPMファイルサポート**: VOPMなど互換アプリケーションからエクスポートされた.opmプリセットファイルの読み込み
- **DAWプロジェクト永続化**: Bank/Presetの選択がDAWプロジェクト保存/読み込み時も維持
- **完全なMIDI CCサポート**: VOPMex互換CCマッピング (14-62)
- **ポリフォニック・ボイス・アロケーション** 自動ボイススティーリング
- **強化されたプリセット** DT2、Key Scale、Feedbackを活用したリッチな音色
- **LFOサポート**: AMD/PMDモジュレーション、4波形（Saw/Square/Triangle/Noise）
- **YM2151ノイズジェネレーター**: チャンネル7のハードウェア精密ノイズ合成
- **ピッチベンド**: 設定可能な範囲でのリアルタイムピッチモジュレーション

### 🔧 モダンなワークフロー
- **Audio Unit v2/v3 互換** (Music Effect type)
- **強化されたDAW互換性** GarageBandでの安定性向上
- **64ビットネイティブ処理** IntelおよびApple Silicon
- **完全なDAWオートメーションサポート**
- **リアルタイムパフォーマンス最適化** オーディオバッファ改善
- **クロスプラットフォームサポート**: Windows (VST3)、macOS (AU/VST3)、Linux (VST3)

## 動作環境

### 🪟 Windows
- Windows 10以降（64ビット）
- VST3対応DAW（Ableton Live、FL Studio、Reaperなど）

### 🍎 macOS
- macOS 10.13以降
- Audio Unit対応DAW（Logic Pro、Ableton Live、GarageBandなど）
- 64ビット IntelまたはApple Siliconプロセッサー

### 🐧 Linux
- モダンなLinuxディストリビューション（Ubuntu 18.04+、Fedora 30+など）
- VST3対応DAW（Reaper、Ardour、Bitwig Studioなど）
- 64ビット x86_64プロセッサー

## インストール

### リリース版のダウンロード
[Releases](https://github.com/hiroaki0923/YMulator-Synth/releases) ページから最新リリースをダウンロードし、プラットフォーム固有の手順に従ってください：

#### 🪟 Windows
1. `YMulator-Synth-Windows-VST3.zip` をダウンロード
2. アーカイブを展開
3. `YMulator-Synth.vst3` を `C:\Program Files\Common Files\VST3\` にコピー
4. DAWを再起動

#### 🍎 macOS
1. 適切なパッケージをダウンロード：
   - `YMulator-Synth-macOS-AU.zip` (Audio Unit)
   - `YMulator-Synth-macOS-VST3.zip` (VST3)
   - `YMulator-Synth-macOS-Standalone.zip` (スタンドアロンアプリ)
2. アーカイブを展開
3. 適切なディレクトリにプラグインをコピー：
   - **Audio Unit**: `/Library/Audio/Plug-Ins/Components/`
   - **VST3**: `/Library/Audio/Plug-Ins/VST3/`
   - **スタンドアロン**: `.app` を `Applications` にインストール
4. DAWを再起動

#### 🐧 Linux
1. 適切なパッケージをダウンロード：
   - `YMulator-Synth-Linux-VST3.tar.gz` (VST3)
   - `YMulator-Synth-Linux-Standalone.tar.gz` (スタンドアロン)
2. アーカイブを展開： `tar -xzf YMulator-Synth-Linux-VST3.tar.gz`
3. VST3ディレクトリにプラグインをコピー：
   - **システム全体**: `/usr/lib/vst3/` (sudoが必要)
   - **ユーザー専用**: `~/.vst3/`
4. DAWを再起動

### ソースからのビルド
下記の[ビルド](#ビルド)セクションを参照してください。

## クイックスタート

1. **プラグインを読み込み** DAWのインストゥルメント・トラック（Music Effectカテゴリ）
2. **プリセットを選択** 8つの内蔵ファクトリープリセットから選択
3. **演奏** MIDIキーボードまたはDAWのピアノロールを使用
4. **パラメーター調整** 直感的な4オペレーター・インターフェースを使用
5. **実験** DT2、Key Scale、Feedbackでユニークなサウンドを作成

## ビルド

### 前提条件

#### 🪟 Windows
```bash
# Chocolatey経由でCMakeをインストール
choco install cmake
# または https://cmake.org/download/ からダウンロード

# Gitをインストール
choco install git

# Visual Studio 2019/2022をC++ワークロードでインストール
```

#### 🍎 macOS
```bash
# Xcode Command Line Toolsをインストール
xcode-select --install

# CMakeとGitをインストール
brew install cmake git

# オプション: テスト用にGoogle Testをインストール
brew install googletest
```

#### 🐧 Linux (Ubuntu/Debian)
```bash
# 依存関係をインストール
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    git \
    libgtest-dev \
    libasound2-dev \
    libjack-jackd2-dev \
    libfreetype6-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxrender-dev \
    libglu1-mesa-dev
```

### クローンとビルド

#### クイックスタート（全プラットフォーム）
```bash
# サブモジュールと一緒にリポジトリをクローン
git clone --recursive https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth

# 初回セットアップ
./scripts/build.sh setup

# プロジェクトをビルド
./scripts/build.sh build

# プラグインをインストール（macOSのみ）
./scripts/build.sh install
```

#### プラットフォーム固有のビルド
```bash
# 開発用デバッグビルド
./scripts/build.sh debug

# 配布用リリースビルド
./scripts/build.sh release

# クリーンして再ビルド
./scripts/build.sh rebuild

# ビルドディレクトリのみクリーン
./scripts/build.sh clean
```

#### 手動ビルド（スクリプトが動作しない場合）
```bash
# サブモジュールと一緒にリポジトリをクローン
git clone --recursive https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth

# ビルドディレクトリを作成して移動
mkdir build && cd build

# 設定（いずれかを選択）
cmake .. -DCMAKE_BUILD_TYPE=Release                    # Linux/macOS
cmake .. -G "Visual Studio 17 2022" -A x64            # Windows

# ビルド
cmake --build . --config Release --parallel

# インストール（macOSのみ - Audio Unitディレクトリにコピー）
cmake --install .
```

### VSCodeでの開発
1. VSCodeでプロジェクトフォルダーを開く
2. 推奨拡張機能のインストールを求められたらインストール（CMake Tools、C/C++）
3. CMake Tools拡張機能を使用してビルドとデバッグ
4. 詳細な開発環境セットアップは `CLAUDE.md` を参照

## 技術仕様

### オーディオ処理
- サンプルレート: 44.1、48、88.2、96 kHz対応
- バッファサイズ: 64-4096サンプル
- 内部処理: 32ビットfloat
- ポリフォニー: 自動ボイススティーリング付き8ボイス
- レイテンシー: パラメーター応答時間 < 3ms

### MIDI実装（VOPMex互換）
| CC# | パラメーター | 範囲 | 説明 |
|-----|-------------|------|-----|
| 14 | Algorithm | 0-7 | FMアルゴリズム選択 |
| 15 | Feedback | 0-7 | オペレーター1フィードバックレベル |
| 16-19 | TL OP1-4 | 0-127 | オペレーター毎のトータルレベル |
| 20-23 | MUL OP1-4 | 0-15 | オペレーター毎のマルチプル |
| 24-27 | DT1 OP1-4 | 0-7 | オペレーター毎のデチューン1 |
| 28-31 | DT2 OP1-4 | 0-3 | オペレーター毎のデチューン2 |
| 39-42 | KS OP1-4 | 0-3 | オペレーター毎のキースケール |
| 43-46 | AR OP1-4 | 0-31 | オペレーター毎のアタックレート |
| 47-50 | D1R OP1-4 | 0-31 | オペレーター毎のディケイ1レート |
| 51-54 | D2R OP1-4 | 0-31 | オペレーター毎のディケイ2レート |
| 55-58 | RR OP1-4 | 0-15 | オペレーター毎のリリースレート |
| 59-62 | D1L OP1-4 | 0-15 | オペレーター毎のサスティンレベル |

### ファクトリープリセット
| # | 名前 | アルゴリズム | 特徴 |
|---|------|-------------|------|
| 0 | Electric Piano | 5 | DT2コーラス、リッチハーモニクス |
| 1 | Synth Bass | 7 | アグレッシブなKey Scale、パンチ |
| 2 | Brass Section | 4 | アンサンブルスプレッド、DT2バリエーション |
| 3 | String Pad | 1 | ウォームフィードバック、サブトルDT2 |
| 4 | Lead Synth | 7 | シャープアタック、コンプレックスデチューン |
| 5 | Organ | 7 | ハーモニックシリーズ、オーガニックキャラクター |
| 6 | Bells | 1 | 非調和関係 |
| 7 | Init | 7 | 基本サイン波テンプレート |

## 貢献

貢献を歓迎します！ガイドラインについては [CONTRIBUTING.md](CONTRIBUTING.md) を参照してください。

### 開発セットアップ
1. リポジトリをフォーク
2. フィーチャーブランチを作成（`git checkout -b feature/amazing-feature`）
3. 変更をコミット（`git commit -m 'Add amazing feature'`）
4. ブランチにプッシュ（`git push origin feature/amazing-feature`）
5. プルリクエストを開く

### テスト

#### クイックテスト
```bash
# 全テストを実行
./scripts/test.sh

# 特定のテストカテゴリを実行
./scripts/test.sh --unit           # ユニットテストのみ
./scripts/test.sh --integration    # インテグレーションテストのみ
./scripts/test.sh --regression     # リグレッションテストのみ

# Audio Unit検証（macOSのみ）
./scripts/test.sh --auval

# ビルドとテストを一つのコマンドで
./scripts/test.sh --build
```

#### 高度なテスト
```bash
# 利用可能なテストを一覧表示
./scripts/test.sh --list

# 特定のパターンにマッチするテストを実行
./scripts/test.sh --filter "ParameterManager"
./scripts/test.sh --filter "Pan"

# CI/自動テスト用クワイエットモード
./scripts/test.sh --quiet

# デバッグ用詳細モード
./scripts/test.sh --verbose

# Google Testに追加引数を渡す
./scripts/test.sh --gtest-args "--gtest_repeat=5"
```

#### 手動テスト（スクリプトが動作しない場合）
```bash
# ユニットテストを実行
cd build
ctest --output-on-failure

# 特定のテスト実行ファイルを実行
./bin/YMulatorSynthAU_Tests

# Audio Unit検証（macOSのみ）
auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"

# Audio Unit検証（デバッグ用詳細表示）
auval -v aumu YMul Hrki
```

## 現在の開発状況

このプロジェクトは以下の状況で積極的に開発されています：
- **Phase 1 (基盤)**: ✅ 100% 完了
- **Phase 2 (コアオーディオ)**: ✅ 100% 完了（OPMフォーカス）
- **Phase 3 (UI強化)**: ✅ 95% 完了（プリセット管理システム完了）
- **Phase 3+ (品質向上)**: ✅ 100% 完了（グローバルパンとDAW互換性）
- **全体進捗**: 100% 完了

### バージョン 0.0.6 機能（2025-06-23リリース）
- **グローバルパンシステム**: LEFT/CENTER/RIGHT/RANDOMパンニングモード、プリセット名保持
- **強化されたDAW互換性**: GarageBandでの安定性向上とオーディオバッファ最適化
- **オーディオ処理改善**: 重複音声問題と再生遅延の修正
- **パフォーマンス最適化**: CPU負荷削減とリアルタイム処理改善

### バージョン 0.0.5 機能（2025-06-16リリース）
- **クロスプラットフォームリリース**: Windows、macOS、Linuxサポート
- **複数プラグイン形式**: VST3、AU、AUv3、スタンドアロンアプリケーション
- **強化された配布**: 全プラットフォーム用包括的インストーラーパッケージ

### バージョン 0.0.3 機能
- **SLOT制御**: タイトルバーのチェックボックスによる個別オペレーター有効/無効制御
- **OPMファイル互換性**: 既存の.opmプリセットファイルとの完全SLOT マスク互換性
- **強化されたUI**: SLOT制御を備えた改良されたオペレーターパネルレイアウト
- **後方互換性**: 既存のプリセットはすべて完全に機能

詳細な進捗追跡については [docs/ymulatorsynth-development-status.md](docs/ymulatorsynth-development-status.md) を参照してください。

### 既知の問題
- **プリセット品質**: バンドルされた64のOPMプリセットは本格的な楽器音のためのサウンドデザイン調整が必要
- **ベル楽器**: マリンバ、ビブラフォンなど類似の打楽器音のパラメーター最適化が必要

### ロードマップ
- **Phase 3 完了**: 強化されたUI機能、追加のプリセット管理機能
- **Phase 4 (将来)**: YM2608 (OPNA) サポート、S98エクスポート、高度な編集機能

## 変更履歴

### バージョン 0.0.6 (2025-06-23)
**品質向上リリース: グローバルパンとDAW互換性**

**🎵 新機能:**
- **グローバルパンシステム**: 強化されたステレオ制御のためのLEFT/CENTER/RIGHT/RANDOMパンニングモード
- **プリセット名保持**: グローバルパン変更時も「Custom」モードに切り替わらない
- **強化されたDAW互換性**: GarageBandインテグレーションと安定性の向上
- **オーディオバッファ最適化**: 重複音声と再生遅延問題の修正
- **パフォーマンス向上**: CPU負荷削減でリアルタイム処理を最適化

**🔧 技術改善:**
- **正しいymfm出力処理**: オーディオバッファ解釈の修正（data[0]=left、data[1]=right）
- **バッファ管理**: オーディオアーティファクト防止のための適切なバッファクリア実装
- **パラメーター例外処理**: グローバルパンパラメーターはカスタムプリセットモード切り替えをバイパス
- **YM2151レジスター制御**: ビットレベル精度での正確なパンニングレジスター操作
- **リソース管理**: 安定動作のためのAudio Unitリソースクリーンアップ強化

**🐛 バグ修正:**
- 再生中の重複/重複ノートの修正
- 再生停止後の1-2秒のオーディオ遅延解決
- 各種DAWでのサンプルレート同期問題の修正
- 残留バッファデータからのオーディオアーティファクト除去

### バージョン 0.0.5 (2025-06-16)
**クロスプラットフォームリリース**

**🎵 新機能:**
- **マルチプラットフォームサポート**: Windows、macOS、Linuxバイナリ
- **複数プラグイン形式**: VST3、AU、AUv3、スタンドアロンバージョン
- **強化された配布**: 全プラットフォーム用包括的インストーラーパッケージ

### バージョン 0.0.4 (2025-06-15)
**メジャーリリース: 完全プリセット管理システム**

**🎵 新機能:**
- **Bank/Presetデュアル ComboBoxシステム**: Factoryとインポートバンクでのプリセット階層組織
- **OPMファイルインポート**: VOPMと互換アプリケーションからエクスポートされた.opmプリセットファイルの完全サポート
- **DAWプロジェクト永続化**: DAW再起動後のBank/Preset選択の自動復元
- **強化されたユーザー体験**: Fileメニュー削除と最適化されたレイアウトによる合理化UI

**🔧 技術改善:**
- **OPMパーサー堅牢性**: 信頼性のあるSLOTマスクとノイズ有効解析のための空白正規化修正
- **パフォーマンス最適化**: 包括的エラー報告を維持しながらデバッグ出力削減
- **状態管理**: シームレスなDAWプロジェクト保存/読み込みのためのValueTreeState統合
- **メモリ管理**: 重複バンク防止と効率的なユーザーデータ永続化

**🐛 バグ修正:**
- 複数のスペース/タブによる不正なパラメーター解析を引き起こすOPMパーサー処理の修正
- Fileメニュー削除後のUIレイアウトスペーシング問題解決
- プリセット適用前にユーザーデータ読み込みを行うDAWプロジェクト復元順序の修正

### バージョン 0.0.3 (2025-06-12)
**UI強化リリース**

**🎵 新機能:**
- **SLOT制御システム**: タイトルバーのチェックボックスによる個別オペレーター有効/無効
- **OPMファイル互換性**: 既存の.opmプリセットファイルとの完全SLOT マスク互換性
- **視覚的フィードバック**: 有効/無効オペレーターの明確な表示

**🔧 技術改善:**
- **後方互換性**: 既存のプリセットはすべて完全に機能
- **UI統合**: パラメーターシステムとのシームレスなSLOT制御統合

### バージョン 0.0.2 (2025-06-11)
**コアオーディオ強化リリース**

**🎵 新機能:**
- **YM2151ノイズジェネレーター**: チャンネル7でのハードウェア精密ノイズ合成
- **LFO完全実装**: AMS/PMSモジュレーション付き4波形
- **強化されたエンベロープシステム**: ベロシティセンシティビティとバッチ最適化

**🔧 技術改善:**
- **ハードウェア制約**: 完全なYM2151ハードウェア制限準拠
- **パフォーマンス最適化**: 効率的なエンベロープ処理
- **MIDI CC拡張**: ノイズとLFOパラメーター用追加コントローラー

### バージョン 0.0.1 (2025-06-08)
**初回リリース**

**🎵 コア機能:**
- **YM2151 (OPM) エミュレーション**: 8ボイス・ポリフォニックFMシンセシス
- **プロフェッショナルインターフェース**: 全パラメーター付き直感的4オペレーターレイアウト
- **8つのファクトリープリセット**: プロ品質のスタートサウンド
- **MIDI統合**: 完全なNote On/Off、CC、ピッチベンドサポート
- **Audio Unit互換性**: ネイティブmacOSプラグイン統合

## ライセンス

このプロジェクトはGPL v3ライセンスの下でライセンスされています - 詳細は [LICENSE](LICENSE) ファイルを参照してください。

### サードパーティーライブラリ
- [JUCE](https://juce.com/) - GPL v3 / Commercial
- [ymfm](https://github.com/aaronsgiles/ymfm) - BSD 3-Clause
- Sam の [VOPM](http://www.geocities.jp/sam_kb/VOPM/) とのOPMファイル形式互換性

## 謝辞

- 素晴らしいymfmエミュレーションライブラリを提供したAaron Giles
- オリジナルのVOPMとOPMファイル形式を作成したSam
- これらのサウンドを維持し続けるチップチューンコミュニティ
- このプラグインの形成に協力してくれた貢献者とテスター

## サポート

- **ドキュメント**: [Wiki](https://github.com/hiroaki0923/YMulator-Synth/wiki)
- **バグレポート**: [Issues](https://github.com/hiroaki0923/YMulator-Synth/issues)
- **ディスカッション**: [Discussions](https://github.com/hiroaki0923/YMulator-Synth/discussions)