# YMulator-Synth VOPM形式仕様

このドキュメントは、YMulator-SynthでサポートするVOPM (.opm)ファイル形式の詳細仕様を定義します。

## 1. VOPM形式概要

VOPMファイルはYM2151 (OPM)用のボイス情報を含むプレーンテキスト形式です。YMulator-Synthは**VOPMex標準フォーマット**との完全互換性を実現しており、VOPMexで作成されたプリセットファイルをそのまま使用できます。

### 1.1 VOPMex互換性

YMulator-SynthはVOPMex Ver2002.04.22フォーマットに完全対応しています：
- **ファイル形式**: MiOPMdrv sound bank Parameter形式
- **拡張子**: .opm
- **エンコーディング**: UTF-8またはShift_JIS対応
- **相互運用性**: VOPMex、その他VOPMツールとの完全な相互運用性

## 2. ファイル構造

### 2.1 基本構造

```
// コメント行（//で開始）
@:[Num] [Name]
LFO: LFRQ AMD PMD WF NFRQ
CH: PAN FL CON AMS PMS SLOT NE
[OP1]: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN
[OP2]: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN
[OP3]: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN
[OP4]: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN
```

### 2.2 パラメータ詳細

#### ボイスヘッダー
- `@:[Num]` - ボイス番号 (0-127)
- `[Name]` - ボイス名 (最大32文字)

#### LFOパラメータ
- `LFRQ` - LFO周波数 (0-255)
- `AMD` - AM深度 (0-127)
- `PMD` - PM深度 (0-127)
- `WF` - 波形選択 (0-3: ノコギリ波、矩形波、三角波、ノイズ)
- `NFRQ` - ノイズ周波数 (0-31、WF=3時のみ有効)

#### チャンネルパラメータ

⚠️ **重要**: YMulator-SynthはVOPMex拡張フォーマットを使用します。標準VOPM仕様と異なる値範囲があります。

- `PAN` - パンポット 
  - **ファイル形式**: 0, 64, 128, 192 (VOPMex拡張形式)
  - **内部変換**: 0=無音, 64=右, 128=左, 192=センター
  - **標準VOPM**: 0-3 (0=無音, 1=右, 2=左, 3=センター)
- `FL` - フィードバック (0-7)
- `CON` - アルゴリズム (0-7)
- `AMS` - AM感度 (0-3)
- `PMS` - PM感度 (0-7)
- `SLOT` - オペレータマスク（VOPMex OpMsk形式）
  - **変換ロジック**: `OpMsk = (内部値 0-15) << 3`
  - **主要な値**:
    - `120` (01111000): 全4オペレータ有効 (15 << 3)
    - `96` (01100000): M2,C2のみ (12 << 3) 
    - `24` (00011000): M1,C1のみ (3 << 3)
    - `0` (00000000): 全オペレータ無効 (0 << 3)
  - **ビット配置**: 
    - Bit 6 (0x40): C2オペレータ
    - Bit 5 (0x20): M2オペレータ  
    - Bit 4 (0x10): C1オペレータ
    - Bit 3 (0x08): M1オペレータ
  - **内部変換**: `内部値 = OpMsk >> 3`
  - **ハードウェア**: YM2151 Key Onレジスタ(0x08)に直接送信
- `NE` - ノイズイネーブル (0-1)

#### オペレータパラメータ
- `AR` - アタック・レート (0-31)
- `D1R` - 1次ディケイ・レート (0-31)
- `D2R` - 2次ディケイ・レート (0-31)
- `RR` - リリース・レート (0-15)
- `D1L` - 1次ディケイ・レベル (0-15)
- `TL` - トータル・レベル (0-127)
- `KS` - キー・スケール (0-3)
- `MUL` - マルチプル (0-15)
- `DT1` - デチューン1 (0-7)
- `DT2` - デチューン2 (0-3)
- `AMS-EN` - AM有効フラグ (0-1)

## 3. パース実装

### 3.1 データ構造定義

```cpp
// VOPMParser.h
namespace ymulatorsynth {

struct VOPMVoice
{
    int number;
    juce::String name;
    
    // LFO parameters
    struct LFO
    {
        int frequency;    // LFRQ (0-255)
        int amd;         // AMD (0-127)
        int pmd;         // PMD (0-127)
        int waveform;    // WF (0-3)
        int noiseFreq;   // NFRQ (0-31)
    } lfo;
    
    // Channel parameters
    struct Channel
    {
        int pan;         // PAN (0-3)
        int feedback;    // FL (0-7)
        int algorithm;   // CON (0-7)
        int ams;         // AMS (0-3)
        int pms;         // PMS (0-7)
        int slotMask;    // SLOT (0-15)
        int noiseEnable; // NE (0-1)
    } channel;
    
    // Operator parameters (4 operators)
    struct Operator
    {
        int attackRate;     // AR (0-31)
        int decay1Rate;     // D1R (0-31)
        int decay2Rate;     // D2R (0-31)
        int releaseRate;    // RR (0-15)
        int decay1Level;    // D1L (0-15)
        int totalLevel;     // TL (0-127)
        int keyScale;       // KS (0-3)
        int multiple;       // MUL (0-15)
        int detune1;        // DT1 (0-7)
        int detune2;        // DT2 (0-3)
        int amsEnable;      // AMS-EN (0-1)
    } operators[4];
};

} // namespace ymulatorsynth
```

### 3.2 パーサー実装

```cpp
// VOPMParser.cpp
class VOPMParser
{
public:
    static std::vector<VOPMVoice> parseFile(const juce::File& file)
    {
        juce::String content = file.loadFileAsString();
        return parseContent(content);
    }
    
    static std::vector<VOPMVoice> parseContent(const juce::String& content)
    {
        std::vector<VOPMVoice> voices;
        juce::StringArray lines;
        lines.addTokens(content, "\n", "");
        
        VOPMVoice currentVoice;
        int operatorIndex = 0;
        bool inVoice = false;
        
        for (const auto& line : lines)
        {
            auto trimmedLine = line.trim();
            
            // コメント行をスキップ
            if (trimmedLine.startsWith("//") || trimmedLine.isEmpty())
                continue;
            
            if (trimmedLine.startsWith("@:"))
            {
                if (inVoice && operatorIndex == 4)
                {
                    voices.push_back(currentVoice);
                }
                
                parseVoiceHeader(trimmedLine, currentVoice);
                operatorIndex = 0;
                inVoice = true;
            }
            else if (trimmedLine.startsWith("LFO:"))
            {
                parseLFO(trimmedLine, currentVoice.lfo);
            }
            else if (trimmedLine.startsWith("CH:"))
            {
                parseChannel(trimmedLine, currentVoice.channel);
            }
            else if (trimmedLine.contains(":") && operatorIndex < 4)
            {
                parseOperator(trimmedLine, currentVoice.operators[operatorIndex]);
                operatorIndex++;
            }
        }
        
        // 最後のボイスを追加
        if (inVoice && operatorIndex == 4)
        {
            voices.push_back(currentVoice);
        }
        
        return voices;
    }

private:
    static void parseVoiceHeader(const juce::String& line, VOPMVoice& voice)
    {
        // "@:123 Voice Name" 形式をパース
        auto tokens = juce::StringArray::fromTokens(line.substring(2), " ", "");
        
        if (tokens.size() >= 1)
        {
            voice.number = tokens[0].getIntValue();
            
            if (tokens.size() >= 2)
            {
                voice.name = line.fromFirstOccurrenceOf(" ", false, false).trim();
            }
        }
    }
    
    static void parseLFO(const juce::String& line, VOPMVoice::LFO& lfo)
    {
        // "LFO: LFRQ AMD PMD WF NFRQ" 形式をパース
        auto values = juce::StringArray::fromTokens(
            line.fromFirstOccurrenceOf(":", false, false), " ", "");
        
        if (values.size() >= 5)
        {
            lfo.frequency = values[0].getIntValue();
            lfo.amd = values[1].getIntValue();
            lfo.pmd = values[2].getIntValue();
            lfo.waveform = values[3].getIntValue();
            lfo.noiseFreq = values[4].getIntValue();
        }
    }
    
    static void parseChannel(const juce::String& line, VOPMVoice::Channel& channel)
    {
        // "CH: PAN FL CON AMS PMS SLOT NE" 形式をパース
        auto values = juce::StringArray::fromTokens(
            line.fromFirstOccurrenceOf(":", false, false), " ", "");
        
        if (values.size() >= 7)
        {
            // VOPMex拡張フォーマットから内部形式に変換
            channel.pan = convertOpmPanToInternal(values[0].getIntValue());
            channel.feedback = values[1].getIntValue();
            channel.algorithm = values[2].getIntValue();
            channel.ams = values[3].getIntValue();
            channel.pms = values[4].getIntValue();
            // VOPMex SLOT値を内部形式に変換
            int slotValue = values[5].getIntValue();
            channel.slotMask = convertOpmSlotToInternal(slotValue);
            channel.noiseEnable = values[6].getIntValue();
        }
    }
    
    // VOPMex PAN値変換関数
    static int convertOmpPanToInternal(int opmPan)
    {
        // VOPMex PAN値: 0, 64, 128, 192を内部形式0-3に変換
        switch (opmPan)
        {
            case 0:   return 0; // Off
            case 64:  return 1; // Right
            case 128: return 2; // Left  
            case 192: return 3; // Center
            default:  return 3; // Default to center for invalid values
        }
    }
    
    static int convertInternalPanToOpm(int internalPan)
    {
        // 内部形式0-3をVOPMex PAN値に変換
        switch (internalPan)
        {
            case 0: return 0;   // Off
            case 1: return 64;  // Right
            case 2: return 128; // Left
            case 3: return 192; // Center
            default: return 192; // Default to center
        }
    }
    
    // VOPMex OpMsk変換関数（実際のVOPMex実装に基づく）
    static int convertOpmSlotToInternal(int opmSlot)
    {
        // VOPMex OpMsk値を内部形式に変換
        // 実装: value = (pVo->OpMsk >> 3) / 15.0 から逆算
        return (opmSlot >> 3) & 0x0F;  // 3ビット右シフトして下位4ビット取得
    }
    
    static int convertInternalSlotToOpm(int internalSlot)
    {
        // 内部形式をVOPMex OpMsk値に変換
        // 実装: pVo->OpMsk = (unsigned char)data << 3
        return (internalSlot & 0x0F) << 3;  // 下位4ビットを3ビット左シフト
    }
    
    // OpMsk値の妥当性チェック
    static bool isValidOpMask(int opMsk)
    {
        // 有効なビットパターンかチェック（Bit 3-6のみ使用）
        return (opMsk & 0x87) == 0 && (opMsk >= 0) && (opMsk <= 120);
    }
    
    static void parseOperator(const juce::String& line, VOPMVoice::Operator& op)
    {
        // "OP1: AR D1R D2R RR D1L TL KS MUL DT1 DT2 AMS-EN" 形式をパース
        auto values = juce::StringArray::fromTokens(
            line.fromFirstOccurrenceOf(":", false, false), " ", "");
        
        if (values.size() >= 11)
        {
            op.attackRate = values[0].getIntValue();
            op.decay1Rate = values[1].getIntValue();
            op.decay2Rate = values[2].getIntValue();
            op.releaseRate = values[3].getIntValue();
            op.decay1Level = values[4].getIntValue();
            op.totalLevel = values[5].getIntValue();
            op.keyScale = values[6].getIntValue();
            op.multiple = values[7].getIntValue();
            op.detune1 = values[8].getIntValue();
            op.detune2 = values[9].getIntValue();
            op.amsEnable = values[10].getIntValue();
        }
    }
};
```

## 4. Factory Presetデータ

### 4.1 基本プリセット定義

```cpp
// FactoryPresets.cpp
namespace ymulatorsynth {

const VOPMVoice FACTORY_PRESETS[] = {
    // Electric Piano
    {
        0, "Electric Piano",
        {3, 0, 0, 0, 0}, // LFO
        {3, 7, 5, 0, 0, 15, 0}, // Channel
        {
            {31, 14, 0, 7, 0, 32, 0, 1, 3, 0, 0}, // OP1
            {31, 5, 0, 7, 0, 0, 0, 1, 3, 0, 0},   // OP2
            {31, 7, 0, 7, 0, 0, 0, 1, 3, 0, 0},   // OP3
            {31, 9, 0, 7, 0, 0, 0, 1, 3, 0, 0}    // OP4
        }
    },
    
    // Synth Bass
    {
        1, "Synth Bass",
        {0, 0, 0, 0, 0}, // LFO
        {3, 6, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 8, 0, 7, 0, 16, 1, 1, 3, 0, 0}, // OP1
            {31, 8, 0, 7, 0, 0, 1, 1, 3, 0, 0},  // OP2
            {31, 8, 0, 7, 0, 0, 1, 1, 3, 0, 0},  // OP3
            {31, 8, 0, 7, 0, 0, 1, 1, 3, 0, 0}   // OP4
        }
    },
    
    // Brass Section
    {
        2, "Brass Section",
        {0, 0, 0, 0, 0}, // LFO
        {3, 6, 4, 0, 0, 15, 0}, // Channel
        {
            {31, 14, 6, 7, 1, 45, 1, 1, 3, 0, 0}, // OP1
            {31, 14, 3, 7, 1, 29, 1, 1, 3, 0, 0}, // OP2
            {31, 11, 11, 7, 1, 18, 1, 1, 3, 0, 0}, // OP3
            {31, 14, 6, 7, 1, 3, 1, 1, 3, 0, 0}   // OP4
        }
    },
    
    // String Pad
    {
        3, "String Pad",
        {35, 0, 0, 0, 0}, // LFO
        {3, 0, 1, 0, 0, 15, 0}, // Channel
        {
            {15, 7, 7, 1, 1, 24, 0, 1, 3, 0, 0}, // OP1
            {15, 4, 4, 1, 1, 16, 0, 1, 3, 0, 0}, // OP2
            {15, 7, 7, 1, 1, 0, 0, 1, 3, 0, 0},  // OP3
            {15, 4, 4, 1, 1, 0, 0, 1, 3, 0, 0}   // OP4
        }
    },
    
    // Lead Synth
    {
        4, "Lead Synth",
        {0, 0, 0, 0, 0}, // LFO
        {3, 4, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 6, 2, 7, 0, 18, 2, 1, 3, 0, 0}, // OP1
            {31, 6, 2, 7, 0, 0, 2, 1, 3, 0, 0},  // OP2
            {31, 6, 2, 7, 0, 0, 2, 1, 3, 0, 0},  // OP3
            {31, 6, 2, 7, 0, 0, 2, 1, 3, 0, 0}   // OP4
        }
    },
    
    // Organ
    {
        5, "Organ",
        {0, 0, 0, 0, 0}, // LFO
        {3, 0, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 7, 0, 20, 0, 2, 3, 0, 0}, // OP1
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0},  // OP2
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0},  // OP3
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0}   // OP4
        }
    },
    
    // Bells
    {
        6, "Bells",
        {0, 0, 0, 0, 0}, // LFO
        {3, 0, 1, 0, 0, 15, 0}, // Channel
        {
            {31, 18, 0, 4, 3, 26, 1, 14, 3, 0, 0}, // OP1
            {31, 18, 0, 4, 3, 22, 1, 1, 3, 0, 0},  // OP2
            {31, 18, 0, 4, 3, 0, 1, 1, 3, 0, 0},   // OP3
            {31, 18, 0, 4, 3, 0, 1, 1, 3, 0, 0}    // OP4
        }
    },
    
    // Init (基本音色)
    {
        7, "Init",
        {0, 0, 0, 0, 0}, // LFO
        {3, 0, 7, 0, 0, 15, 0}, // Channel
        {
            {31, 0, 0, 7, 0, 32, 0, 1, 3, 0, 0}, // OP1
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0},  // OP2
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0},  // OP3
            {31, 0, 0, 7, 0, 0, 0, 1, 3, 0, 0}   // OP4
        }
    }
};

} // namespace ymulatorsynth
```

## 5. エラーハンドリング

### 5.1 検証機能

```cpp
class VOPMValidator
{
public:
    struct ValidationResult
    {
        bool isValid = true;
        juce::StringArray errors;
        juce::StringArray warnings;
    };
    
    static ValidationResult validate(const VOPMVoice& voice)
    {
        ValidationResult result;
        
        // ボイス番号チェック
        if (voice.number < 0 || voice.number > 127)
        {
            result.isValid = false;
            result.errors.add("Voice number out of range (0-127): " + juce::String(voice.number));
        }
        
        // アルゴリズムチェック
        if (voice.channel.algorithm < 0 || voice.channel.algorithm > 7)
        {
            result.isValid = false;
            result.errors.add("Algorithm out of range (0-7): " + juce::String(voice.channel.algorithm));
        }
        
        // オペレータパラメータチェック
        for (int i = 0; i < 4; ++i)
        {
            const auto& op = voice.operators[i];
            
            if (op.totalLevel < 0 || op.totalLevel > 127)
            {
                result.isValid = false;
                result.errors.add("OP" + juce::String(i+1) + " TL out of range (0-127): " + juce::String(op.totalLevel));
            }
            
            if (op.attackRate < 0 || op.attackRate > 31)
            {
                result.isValid = false;
                result.errors.add("OP" + juce::String(i+1) + " AR out of range (0-31): " + juce::String(op.attackRate));
            }
            
            // その他のパラメータも同様にチェック...
        }
        
        return result;
    }
};
```

## 6. VOPMex互換性実装ノート

### 6.1 ファイル形式の重要な違い

今回の分析により判明した、標準VOPMとVOPMexの重要な違い：

#### 6.1.1 PAN値の拡張
- **標準VOPM**: 0-3 (2ビット値)
- **VOPMex**: 0, 64, 128, 192 (6ビット左シフト値)
- **YMulator-Synth**: VOPMex形式をファイルで使用、内部で標準形式に変換

#### 6.1.2 OpMsk（オペレータマスク）の変換
- **標準VOPM**: 0-15 (4ビット値)
- **VOPMex**: OpMsk = (内部値) << 3
  - `120` (01111000): 全オペレータ有効 (15 << 3)
  - `96` (01100000): M2,C2のみ (12 << 3)
  - `24` (00011000): M1,C1のみ (3 << 3)
  - `0` (00000000): 全オペレータ無効 (0 << 3)
- **ハードウェア**: YM2151 Key Onレジスタに直接マッピング
- **YMulator-Synth**: ファイルではOpMsk形式、内部では4ビット値として処理

#### 6.1.3 変換処理のタイミング
```cpp
// ファイル読み込み時の変換
int filePan = 64;           // VOPMexファイル値
int internalPan = 1;        // 内部処理値 (右チャンネル)

// ファイル保存時の変換
int internalPan = 1;        // 内部処理値
int filePan = 64;           // VOPMexファイル値
```

### 6.2 互換性テスト

YMulator-Synthプリセットファイルの互換性は以下の方法で確認済み：

1. **VOPMexからのエクスポート**
   - bell.opm, gm.opm, piano.opmでフォーマット確認
   - PAN=64, SLOT=120の標準的な使用を確認

2. **相互変換テスト**
   - YMulator-Synth → VOPMex → YMulator-Synth の往復変換
   - パラメータ値の整合性確認
   - 音色の一致確認

3. **プリセットコレクション**
   - 64種類の実用的プリセットをVOPMex標準形式で提供
   - 全ての有効プリセットでPAN=64, SLOT=120を使用
   - SLOT=254は無効プリセット用として認識（使用しない）
   - アルゴリズム/フィードバック逆転問題を修正済み

### 6.3 実装のベストプラクティス

#### 6.3.1 ファイルI/O
```cpp
// ✅ 正しい実装
void saveToFile(const VOPMVoice& voice, juce::File& file)
{
    // 内部形式からVOPMex形式に変換して保存
    int opmPan = convertInternalPanToOpm(voice.channel.pan);
    int opmSlot = (voice.channel.slotMask == 15) ? 120 : voice.channel.slotMask;
    
    result << "CH: " << opmPan << " " << voice.channel.feedback << " " 
           << voice.channel.algorithm << " " << voice.channel.ams << " "
           << voice.channel.pms << " " << opmSlot << " " 
           << voice.channel.noiseEnable << "\n";
}
```

#### 6.3.2 バリデーション
```cpp
// ✅ VOPMex拡張値の考慮
bool isValidOpmPan(int value)
{
    return (value == 0 || value == 64 || value == 128 || value == 192);
}

bool isValidOpmSlot(int value)
{
    return ((value >= 0 && value <= 15) || value == 120 || value == 254);
}
```

この仕様により、VOPMex互換のプリセット管理とファイルI/O機能が実装でき、既存のVOPMエコシステムとの完全な相互運用性が保証されます。