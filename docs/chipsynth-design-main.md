# ChipSynth AU 設計書

**プロジェクト名**: ChipSynth AU  
**リポジトリ**: https://github.com/hiroaki0923/ChipSynth-AU  
**ライセンス**: MIT License  

## 関連ドキュメント
- [技術仕様書](TECHNICAL_SPEC.md) - 詳細設計、UI設計、テスト計画
- [実装ガイド](IMPLEMENTATION_GUIDE.md) - 開発環境構築、コーディングガイド
- [アーキテクチャ決定記録](ADR.md) - 技術的決定事項の記録

## 1. 概要

### 1.1 プロジェクト概要
ChipSynth AUは、ymfmライブラリを使用してヤマハFM音源（OPM/OPNA）およびSSGをエミュレートするAudio Unitプラグインを開発する。チップチューン制作者向けに、直感的なUIと豊富なプリセット音色、S98形式での録音機能を提供する。

### 1.2 主要機能
- FM音源（YM2151/OPM、YM2608/OPNA）のエミュレーション
- SSG（AY-3-8910互換）音源のエミュレーション
- ADPCMサンプル再生機能
- VOPMライクな音色エディタUI
- プリセット音色管理システム
- S98フォーマットでの録音・出力機能

## 2. システムアーキテクチャ

### 2.1 レイヤー構成
```
┌─────────────────────────────────────────┐
│         Audio Unit Host (DAW)           │
├─────────────────────────────────────────┤
│      Audio Unit Plugin Interface        │
├─────────────────────────────────────────┤
│         Plugin Core Controller          │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐  │
│  │ Voice Editor│  │ Preset Manager  │  │
│  └─────────────┘  └─────────────────┘  │
├─────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────────┐  │
│  │ ymfm Engine │  │ S98 Recorder    │  │
│  └─────────────┘  └─────────────────┘  │
└─────────────────────────────────────────┘
```

### 2.2 コンポーネント構成

#### 2.2.1 Audio Unit Framework
- **AUAudioUnit**: メインのAudioUnitクラス
- **AUParameter**: パラメータ管理
- **AUParameterTree**: パラメータツリー構造

#### 2.2.2 ymfm Integration Layer
- **YmfmWrapper**: ymfmライブラリのC++ラッパー
- **ChipEmulator**: チップ別エミュレーション管理
- **SampleRateConverter**: サンプリングレート変換

#### 2.2.3 Voice Management
- **VoiceParameter**: 音色パラメータ定義
- **VoiceBank**: 音色バンク管理
- **PresetLoader**: プリセット読み込み

#### 2.2.4 UI Components
- **MainViewController**: メインUI管理
- **VoiceEditorView**: 音色エディタビュー
- **EnvelopeView**: エンベロープ表示・編集
- **OperatorView**: オペレータパラメータ編集

## 3. 実装計画

### 3.1 開発環境
- **言語**: C++ (Core, UI)
- **フレームワーク**: Audio Unit v3, Core Audio, JUCE
- **UI**: JUCE Framework
- **ビルドシステム**: Projucer, CMake

### 3.2 外部ライブラリ
- **ymfm**: FM音源エミュレーション
- **JUCE**: UIフレームワーク、オーディオ処理
- **libsndfile**: WAVファイル読み込み（JUCEの機能で代替可能）
- **json**: プリセット管理用（JUCEのJSON機能で代替可能）

### 3.3 フェーズ別実装

#### Phase 1: 基本機能（2ヶ月）
- Audio Unitフレームワーク実装
- ymfm統合
- 基本的な音声出力
- MIDI CCマッピング（VOPMex互換）

#### Phase 2: 音色エディタ（1.5ヶ月）
- JUCEベースのVOPMライクUI実装
- パラメータ編集機能
- リアルタイムプレビュー
- エンベロープ表示と編集

#### Phase 3: プリセット管理（1ヶ月）
- VOPMフォーマット読み込み
- 他音色フォーマット対応の基盤構築
- カスタムプリセット保存
- プリセットブラウザUI

#### Phase 4: 高度な機能（1.5ヶ月）
- ADPCM管理機能
- S98録音機能
- 最適化とバグ修正

## 4. 今後の拡張計画

### 4.1 追加チップ対応
- OPN2 (YM2612) - メガドライブ
- OPL3 (YM262) - Sound Blaster
- OPLL (YM2413) - MSX

### 4.2 追加機能
- MIDIラーニング
- オートメーション記録
- VGMファイルインポート
- マルチティンバー対応

### 4.3 プラットフォーム拡張
- VST3版の開発（JUCEで容易に対応可能）
- iOS版（AUv3）
- スタンドアロンアプリ（JUCEで自動生成可能）