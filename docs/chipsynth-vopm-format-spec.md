# ChipSynth-AU VOPM形式仕様

このドキュメントは、ChipSynth-AUでサポートするVOPM (.opm)ファイル形式の詳細仕様を定義します。

## 1. VOPM形式概要

VOPMファイルはYM2151 (OPM)用のボイス情報を含むプレーンテキスト形式です。VOPM/VOPMexとの互換性を保ちながら、ChipSynth-AUで必要な機能をサポートします。

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
- `PAN` - パンポット (0-3: 無音、右、左、センター)
- `FL` - フィードバック (0-7)
- `CON` - アルゴリズム (0-7)
- `AMS` - AM感度 (0-3)
- `PMS` - PM感度 (0-7)
- `SLOT` - スロットマスク (0-15、ビットマスク)
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
namespace chipsynth {

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

} // namespace chipsynth
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
            channel.pan = values[0].getIntValue();
            channel.feedback = values[1].getIntValue();
            channel.algorithm = values[2].getIntValue();
            channel.ams = values[3].getIntValue();
            channel.pms = values[4].getIntValue();
            channel.slotMask = values[5].getIntValue();
            channel.noiseEnable = values[6].getIntValue();
        }
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
namespace chipsynth {

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

} // namespace chipsynth
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

この仕様により、VOPMex互換のプリセット管理とファイルI/O機能が実装できます。