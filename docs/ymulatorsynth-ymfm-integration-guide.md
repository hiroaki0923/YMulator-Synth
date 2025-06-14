# YMulator Synth ymfm Integration Guide

このドキュメントは、YMulator-Synthプロジェクトにおけるymfmライブラリの統合方法と、YM2151 (OPM) およびYM2608 (OPNA) のプログラミングパターンについて説明します。

## 目次

1. [概要](#概要)
2. [ymfmライブラリの基本](#ymfmライブラリの基本)
3. [OPMプログラミングパターン](#opmプログラミングパターン)
4. [実装ガイドライン](#実装ガイドライン)
5. [サウンドデザインテクニック](#サウンドデザインテクニック)
6. [トラブルシューティング](#トラブルシューティング)

## 概要

ymfmは、Aaron Gilesによって開発された高精度なヤマハFM音源エミュレーションライブラリです。YMulator Synthでは、このライブラリを使用してYM2151 (OPM) とYM2608 (OPNA) のエミュレーションを実現しています。

### 主な特徴

- サイクル精度のエミュレーション
- 低レイテンシー
- クリーンなC++ API
- 複数チップの同時エミュレーション対応

## ymfmライブラリの基本

### 1. 初期化とセットアップ

```cpp
// ymfm_interfaceの実装
class opm_interface : public ymfm::ymfm_interface {
public:
    opm_interface() : m_chip(*this) {
        m_chip.reset();
    }
    
    // 必須のインターフェースメソッド
    virtual void ymfm_sync_mode_write(uint8_t data) override {}
    virtual void ymfm_sync_check_interrupts() override {}
    virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override {}
    virtual void ymfm_set_busy_end(uint32_t clocks) override {}
    virtual void ymfm_update_irq(bool asserted) override {}
    
    ymfm::ym2151 m_chip;
};
```

### 2. レジスタ書き込み

ymfmでは、`write_address()`と`write_data()`の2段階でレジスタに書き込みます：

```cpp
void writeRegister(uint8_t address, uint8_t data) {
    chip.write_address(address);
    chip.write_data(data);
}
```

### 3. サンプル生成

```cpp
ymfm::ym2151::output_data output;
chip.generate(&output, 1);  // 1サンプル生成
int16_t left = output.data[0];
int16_t right = output.data[1];
```

## OPMプログラミングパターン

### 1. 基本的な音色設定（サイン波）

最もシンプルな音色設定です。Algorithm 7（全オペレータ並列）を使用：

```cpp
void setupSineWaveTimbre(ymfm::ym2151& chip, int channel) {
    // Algorithm 7, L/R両出力, FB=0
    writeRegister(0x20 + channel, 0xC7);
    
    // 全4オペレータを同じ設定に
    for (int op = 0; op < 4; op++) {
        int base_addr = op * 8 + channel;
        
        writeRegister(0x40 + base_addr, 0x01);  // DT1=0, MUL=1
        writeRegister(0x60 + base_addr, 32);    // TL=32 (適度な音量)
        writeRegister(0x80 + base_addr, 0x1F);  // KS=0, AR=31
        writeRegister(0xA0 + base_addr, 0x00);  // AMS-EN=0, D1R=0
        writeRegister(0xC0 + base_addr, 0x00);  // DT2=0, D2R=0
        writeRegister(0xE0 + base_addr, 0xF7);  // D1L=15, RR=7
    }
}
```

### 2. ピアノ音色

Algorithm 4（2つの並列FMペア）を使用した、より複雑な音色：

```cpp
void setupPianoTimbre(ymfm::ym2151& chip, int channel) {
    // Algorithm 4, FB=3
    writeRegister(0x20 + channel, 0xC4 | (3 << 3));
    
    // オペレータごとに異なる設定
    // OP1 (モジュレータ): multiplier=2
    writeRegister(0x40 + channel, 0x02);
    writeRegister(0x60 + channel, 40);
    // ... 他のパラメータ
    
    // OP2 (キャリア): multiplier=1, 速い減衰
    writeRegister(0x48 + channel, 0x01);
    writeRegister(0x68 + channel, 0);
    // ... パーカッシブなエンベロープ設定
}
```

### 3. 周波数設定とノートオン

```cpp
void noteOn(uint8_t channel, uint8_t midiNote, uint8_t velocity) {
    // MIDI note → 周波数 → KC/KF変換
    float freq = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    
    // KC (Key Code) とKF (Key Fraction) の計算
    float fnote = 12.0f * log2f(freq / 440.0f) + 69.0f;
    int noteInt = (int)round(fnote);
    int octave = (noteInt / 12) - 1;
    int noteInOctave = noteInt % 12;
    
    // YM2151のノートコード
    const uint8_t noteCode[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 14};
    uint8_t kc = ((octave & 0x07) << 4) | noteCode[noteInOctave];
    uint8_t kf = 0;  // ファインチューニングなし
    
    // レジスタに書き込み
    writeRegister(0x28 + channel, kc);
    writeRegister(0x30 + channel, kf << 2);
    
    // キーオン（全オペレータ有効）
    writeRegister(0x08, 0x78 | channel);
}
```

### 4. LFO効果（ビブラート・トレモロ）

```cpp
void setupLFO(ymfm::ym2151& chip) {
    // LFO周波数とリセット
    writeRegister(0x19, 0x80);  // LFOリセット
    writeRegister(0x19, 0x00);  // LFO有効化
    
    // LFO設定: 波形=三角波, 周波数=中速
    writeRegister(0x18, 0x02);
    
    // PMD (Pitch Modulation Depth) = 64
    writeRegister(0x19, 0x40);
    
    // チャンネルごとにPMS (Pitch Modulation Sensitivity) を設定
    uint8_t con_reg = readRegister(0x20 + channel);
    con_reg = (con_reg & 0xF8) | 0x04;  // PMS = 4
    writeRegister(0x20 + channel, con_reg);
}
```

### 5. ステレオとパンニング

```cpp
void setPanning(ymfm::ym2151& chip, int channel, bool left, bool right) {
    uint8_t con_reg = readRegister(0x20 + channel);
    con_reg &= 0x3F;  // L/Rビットをクリア
    if (left) con_reg |= 0x80;
    if (right) con_reg |= 0x40;
    writeRegister(0x20 + channel, con_reg);
}
```

## 実装ガイドライン

### 1. レジスタアドレス計算

OPMのレジスタアドレスは以下のパターンに従います：

- **オペレータ固有レジスタ**: `base_register + (operator * 8) + channel`
- **チャンネル固有レジスタ**: `base_register + channel`

```cpp
// 例: TL (Total Level) レジスタ (0x60-0x7F)
int op = 2;        // オペレータ3
int channel = 0;   // チャンネル1
uint8_t address = 0x60 + (op * 8) + channel;  // = 0x70
```

### 2. アンチアーティファクトテクニック

音の切り替え時のノイズを防ぐ技術：

```cpp
// 1. パラメータ変更前にキーオフ
writeRegister(0x08, 0x00 | channel);

// 2. エンベロープのリセット待ち
// 数ミリ秒待機

// 3. パラメータ変更
setupNewTimbre(chip, channel);

// 4. 新しいノートをキーオン
noteOn(channel, newNote, velocity);
```

### 3. 複数チャンネルの管理

```cpp
class VoiceManager {
    struct Voice {
        uint8_t channel;
        uint8_t note;
        bool active;
    };
    
    Voice voices[8];  // YM2151は8チャンネル
    
    int allocateVoice(uint8_t note) {
        // 空いているチャンネルを探す
        for (int i = 0; i < 8; i++) {
            if (!voices[i].active) {
                voices[i].note = note;
                voices[i].active = true;
                return i;
            }
        }
        // ボイススティーリングの実装
        return stealOldestVoice();
    }
};
```

## サウンドデザインテクニック

### 1. アルゴリズムの選択

| Algorithm | 構成 | 適した音色 |
|-----------|------|-----------|
| 0 | 4オペレータ直列 | ベル、金属音 |
| 1 | 3直列+1並列 | エレピ、ブラス |
| 4 | 2つのFMペア | ピアノ、ストリングス |
| 7 | 4オペレータ並列 | オルガン、パッド |

### 2. エンベロープデザイン

```cpp
// パーカッシブ（打楽器的）
AR = 31;  // 高速アタック
D1R = 12; // 中速減衰
D1L = 2;  // 低サステインレベル
RR = 7;   // 中速リリース

// サステイン（持続音）
AR = 20;  // 中速アタック
D1R = 0;  // 減衰なし
D1L = 0;  // 最大サステイン
RR = 4;   // 遅いリリース

// フェードイン
AR = 10;  // 遅いアタック
D1R = 0;  // 減衰なし
D1L = 0;  // 最大サステイン
RR = 10;  // 遅いリリース
```

### 3. デチューン効果

```cpp
// 微妙なコーラス効果
DT1 = 1;  // わずかなデチューン

// 厚みのある音
DT1 = 2;  // 中程度のデチューン

// 金属的な音
DT1 = 3-4; // 強いデチューン
```

### 4. モジュレーションインデックス

FM合成の深さは、モジュレータのTL（Total Level）で制御：

```cpp
// 浅いFM（倍音少ない）
modulator_TL = 50-60;

// 中程度のFM（バランスの良い倍音）
modulator_TL = 30-40;

// 深いFM（倍音豊富）
modulator_TL = 0-20;
```

## トラブルシューティング

### 1. 音が出ない場合

```cpp
// チェックリスト：
// 1. チップがリセットされているか
chip.reset();

// 2. 音色が設定されているか
setupTimbre(chip, channel);

// 3. 周波数が設定されているか
writeRegister(0x28 + channel, kc);
writeRegister(0x30 + channel, kf << 2);

// 4. キーオンされているか
writeRegister(0x08, 0x78 | channel);  // 全オペレータON

// 5. TL（音量）が適切か
// TL=127は無音、TL=0は最大音量
```

### 2. ノイズやクリックが発生する場合

```cpp
// 1. アタックレートを遅くする
AR = 20;  // 31から20に下げる

// 2. キー切り替え時にギャップを入れる
noteOff(channel);
// 5-10ms待機
noteOn(channel, newNote);

// 3. 音量フェードを実装
for (int vol = current_vol; vol >= 0; vol--) {
    setVolume(channel, vol);
    // 短い待機
}
```

### 3. 想定と異なる音色になる場合

```cpp
// 1. アルゴリズムを確認
uint8_t con = readRegister(0x20 + channel);
uint8_t algorithm = con & 0x07;

// 2. オペレータ接続を確認
// モジュレータのTLが高すぎないか
// キャリアのTLが適切か

// 3. マルチプライヤーを確認
// 整数倍以外は非調和倍音を生成
```

## 参考資料

- [ymfm GitHub リポジトリ](https://github.com/aaronsgiles/ymfm)
- YM2151 Application Manual (Yamaha)
- FM音源プログラミングガイド（各種書籍）

## 更新履歴

- 2025-06-08: 初版作成（ymfm OPMサンプルコードの分析に基づく）