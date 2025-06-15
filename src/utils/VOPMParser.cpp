#include "VOPMParser.h"
#include "Debug.h"

namespace ymulatorsynth {

std::vector<VOPMVoice> VOPMParser::parseFile(const juce::File& file)
{
    if (!file.exists())
    {
        return {};
    }
    
    juce::String content = file.loadFileAsString();
    if (content.isEmpty())
    {
        return {};
    }
    
    return parseContent(content);
}

std::vector<VOPMVoice> VOPMParser::parseContent(const juce::String& content)
{
    std::vector<VOPMVoice> voices;
    juce::StringArray lines;
    lines.addTokens(content, "\n\r", "");
    
    VOPMVoice currentVoice;
    int operatorIndex = 0;
    bool inVoice = false;
    
    for (const auto& line : lines)
    {
        auto trimmedLine = line.trim();
        
        // Skip comment lines and empty lines
        if (isCommentLine(trimmedLine) || trimmedLine.isEmpty())
            continue;
        
        if (trimmedLine.startsWith("@:"))
        {
            // Save previous voice if complete
            if (inVoice && operatorIndex == 4)
            {
                auto validation = validate(currentVoice);
                if (validation.isValid)
                {
                    voices.push_back(currentVoice);
                }
            }
            
            // Start new voice
            currentVoice = VOPMVoice(); // Reset to defaults
            parseVoiceHeader(trimmedLine, currentVoice);
            operatorIndex = 0;
            inVoice = true;
        }
        else if (inVoice && trimmedLine.startsWith("LFO:"))
        {
            parseLFO(trimmedLine, currentVoice.lfo);
        }
        else if (inVoice && trimmedLine.startsWith("CH:"))
        {
            parseChannel(trimmedLine, currentVoice.channel);
        }
        else if (inVoice && trimmedLine.contains(":") && operatorIndex < 4)
        {
            // Parse operator line (M1:, C1:, M2:, C2:)
            parseOperator(trimmedLine, currentVoice.operators[operatorIndex]);
            operatorIndex++;
        }
    }
    
    // Save last voice if complete
    if (inVoice && operatorIndex == 4)
    {
        auto validation = validate(currentVoice);
        if (validation.isValid)
        {
            voices.push_back(currentVoice);
        }
        else
        {
        }
    }
    else if (inVoice)
    {
    }
    
    return voices;
}

ValidationResult VOPMParser::validate(const VOPMVoice& voice)
{
    ValidationResult result;
    
    // Voice number check
    if (!isValidRange(voice.number, 0, 127))
    {
        result.isValid = false;
        result.errors.add("Voice number out of range (0-127): " + juce::String(voice.number));
    }
    
    // LFO parameter checks (optional, warn only)
    if (!isValidRange(voice.lfo.frequency, 0, 255))
    {
        result.warnings.add("LFO frequency out of range (0-255): " + juce::String(voice.lfo.frequency));
    }
    
    if (!isValidRange(voice.lfo.amd, 0, 127))
    {
        result.warnings.add("LFO AMD out of range (0-127): " + juce::String(voice.lfo.amd));
    }
    
    if (!isValidRange(voice.lfo.pmd, 0, 127))
    {
        result.warnings.add("LFO PMD out of range (0-127): " + juce::String(voice.lfo.pmd));
    }
    
    if (!isValidRange(voice.lfo.waveform, 0, 3))
    {
        result.warnings.add("LFO waveform out of range (0-3): " + juce::String(voice.lfo.waveform));
    }
    
    if (!isValidRange(voice.lfo.noiseFreq, 0, 31))
    {
        result.warnings.add("LFO noise frequency out of range (0-31): " + juce::String(voice.lfo.noiseFreq));
    }
    
    // Channel parameter checks (warnings only for more tolerance)
    if (!isValidRange(voice.channel.pan, 0, 3))
    {
        result.warnings.add("Channel pan out of range (0-3): " + juce::String(voice.channel.pan));
    }
    
    if (!isValidRange(voice.channel.feedback, 0, 7))
    {
        result.warnings.add("Channel feedback out of range (0-7): " + juce::String(voice.channel.feedback));
    }
    
    if (!isValidRange(voice.channel.algorithm, 0, 7))
    {
        result.warnings.add("Channel algorithm out of range (0-7): " + juce::String(voice.channel.algorithm));
    }
    
    if (!isValidRange(voice.channel.ams, 0, 3))
    {
        result.warnings.add("Channel AMS out of range (0-3): " + juce::String(voice.channel.ams));
    }
    
    if (!isValidRange(voice.channel.pms, 0, 7))
    {
        result.warnings.add("Channel PMS out of range (0-7): " + juce::String(voice.channel.pms));
    }
    
    if (!isValidRange(voice.channel.noiseEnable, 0, 1))
    {
        result.warnings.add("Channel noise enable out of range (0-1): " + juce::String(voice.channel.noiseEnable));
    }
    
    if (!isValidRange(voice.channel.slotMask, 0, 15))
    {
        result.warnings.add("Channel slot mask out of range (0-15): " + juce::String(voice.channel.slotMask));
    }
    
    // Operator parameter checks
    for (int i = 0; i < 4; ++i)
    {
        const auto& op = voice.operators[i];
        juce::String opName = "OP" + juce::String(i + 1);
        
        if (!isValidRange(op.attackRate, 0, 31))
        {
            result.warnings.add(opName + " AR out of range (0-31): " + juce::String(op.attackRate));
        }
        
        if (!isValidRange(op.decay1Rate, 0, 31))
        {
            result.warnings.add(opName + " D1R out of range (0-31): " + juce::String(op.decay1Rate));
        }
        
        if (!isValidRange(op.decay2Rate, 0, 31))
        {
            result.warnings.add(opName + " D2R out of range (0-31): " + juce::String(op.decay2Rate));
        }
        
        if (!isValidRange(op.releaseRate, 0, 15))
        {
            result.warnings.add(opName + " RR out of range (0-15): " + juce::String(op.releaseRate));
        }
        
        if (!isValidRange(op.decay1Level, 0, 15))
        {
            result.warnings.add(opName + " D1L out of range (0-15): " + juce::String(op.decay1Level));
        }
        
        if (!isValidRange(op.totalLevel, 0, 127))
        {
            result.warnings.add(opName + " TL out of range (0-127): " + juce::String(op.totalLevel));
        }
        
        if (!isValidRange(op.keyScale, 0, 3))
        {
            result.warnings.add(opName + " KS out of range (0-3): " + juce::String(op.keyScale));
        }
        
        if (!isValidRange(op.multiple, 0, 15))
        {
            result.warnings.add(opName + " MUL out of range (0-15): " + juce::String(op.multiple));
        }
        
        if (!isValidRange(op.detune1, 0, 7))
        {
            result.warnings.add(opName + " DT1 out of range (0-7): " + juce::String(op.detune1));
        }
        
        if (!isValidRange(op.detune2, 0, 3))
        {
            result.warnings.add(opName + " DT2 out of range (0-3): " + juce::String(op.detune2));
        }
        
        if (!isValidRange(op.amsEnable, 0, 1))
        {
            result.warnings.add(opName + " AMS-EN out of range (0-1): " + juce::String(op.amsEnable));
        }
    }
    
    return result;
}

juce::String VOPMParser::voiceToString(const VOPMVoice& voice)
{
    juce::String result;
    
    // Voice header
    result << "@:" << voice.number << " " << voice.name << "\n";
    
    // LFO line
    result << "LFO: " 
           << voice.lfo.frequency << " "
           << voice.lfo.amd << " "
           << voice.lfo.pmd << " "
           << voice.lfo.waveform << " "
           << voice.lfo.noiseFreq << "\n";
    
    // Channel line
    result << "CH: "
           << convertInternalPanToOpm(voice.channel.pan) << " "
           << voice.channel.feedback << " "
           << voice.channel.algorithm << " "
           << voice.channel.ams << " "
           << voice.channel.pms << " "
           << convertInternalSlotToOpm(voice.channel.slotMask) << " "
           << voice.channel.noiseEnable << "\n";
    
    // Operator lines
    juce::StringArray opLabels = {"M1", "C1", "M2", "C2"};
    for (int i = 0; i < 4; ++i)
    {
        const auto& op = voice.operators[i];
        result << opLabels[i] << ": "
               << op.attackRate << " "
               << op.decay1Rate << " "
               << op.decay2Rate << " "
               << op.releaseRate << " "
               << op.decay1Level << " "
               << op.totalLevel << " "
               << op.keyScale << " "
               << op.multiple << " "
               << op.detune1 << " "
               << op.detune2 << " "
               << convertInternalAmeToOpm(op.amsEnable) << "\n";
    }
    
    return result;
}

// OPM format conversion functions
int VOPMParser::convertOpmPanToInternal(int opmPan)
{
    // OPM PAN values: 0, 64, 128, 192 (<<6 bit shift)
    // Convert to internal 0-3 range: 0=off, 1=right, 2=left, 3=center
    switch (opmPan)
    {
        case 0:   return 0; // Off
        case 64:  return 1; // Right
        case 128: return 2; // Left  
        case 192: return 3; // Center
        default:  return 3; // Default to center for invalid values
    }
}

int VOPMParser::convertOpmAmeToInternal(int opmAme)
{
    // OPM AME values: 0 or 128 (<<7 bit shift)
    // Convert to internal 0-1 range
    return (opmAme >= 128) ? 1 : 0;
}

int VOPMParser::convertInternalPanToOpm(int internalPan)
{
    // Internal 0-3 range to OPM PAN values: 0, 64, 128, 192
    switch (internalPan)
    {
        case 0: return 0;   // Off
        case 1: return 64;  // Right
        case 2: return 128; // Left
        case 3: return 192; // Center
        default: return 192; // Default to center
    }
}

int VOPMParser::convertInternalAmeToOpm(int internalAme)
{
    // Internal 0-1 range to OPM AME values: 0 or 128
    return (internalAme > 0) ? 128 : 0;
}

int VOPMParser::convertOpmSlotToInternal(int opmSlot)
{
    // VOPMex OpMsk format investigation
    // 120 = 01111000 should represent slot pattern
    // Let's try different interpretations
    
    // Debug current conversion
    int original = (opmSlot >> 3) & 0x0F;
    
    // Try bit reversal approach - VOPM might use upper bits
    int reversed = 0;
    if (opmSlot & 0x08) reversed |= 0x01;  // bit 3 -> bit 0
    if (opmSlot & 0x10) reversed |= 0x02;  // bit 4 -> bit 1  
    if (opmSlot & 0x20) reversed |= 0x04;  // bit 5 -> bit 2
    if (opmSlot & 0x40) reversed |= 0x08;  // bit 6 -> bit 3
    
    // VOPMex standard: 120 means all slots enabled  
    // Convert to our internal format (15 = all slots enabled)
    if (opmSlot == 120) {
        return 15;  // All slots on
    }
    
    // For other values, use original conversion
    return original;
}

int VOPMParser::convertInternalSlotToOpm(int internalSlot)
{
    // Internal 0-15 range to VOPMex OpMsk format: 3-bit left shift
    return (internalSlot & 0x0F) << 3;
}

// Private helper methods

void VOPMParser::parseVoiceHeader(const juce::String& line, VOPMVoice& voice)
{
    // Parse "@:123 Voice Name" format
    auto afterColon = line.substring(2); // Remove "@:"
    auto firstSpace = afterColon.indexOfChar(' ');
    
    if (firstSpace >= 0)
    {
        voice.number = afterColon.substring(0, firstSpace).getIntValue();
        voice.name = afterColon.substring(firstSpace + 1).trim();
    }
    else
    {
        voice.number = afterColon.getIntValue();
        voice.name = "Untitled";
    }
}

void VOPMParser::parseLFO(const juce::String& line, VOPMVoice::LFO& lfo)
{
    auto tokens = tokenizeLine(line);
    
    if (tokens.size() >= 5)
    {
        lfo.frequency = tokens[0].getIntValue();
        lfo.amd = tokens[1].getIntValue();
        lfo.pmd = tokens[2].getIntValue();
        lfo.waveform = tokens[3].getIntValue();
        lfo.noiseFreq = tokens[4].getIntValue();
    }
}

void VOPMParser::parseChannel(const juce::String& line, VOPMVoice::Channel& channel)
{
    auto tokens = tokenizeLine(line);
    
    if (tokens.size() < 7) {
        return;
    }
    
    // Convert OPM PAN format (0, 64, 128, 192) to internal (0-3)
    channel.pan = convertOpmPanToInternal(tokens[0].getIntValue());
    channel.feedback = tokens[1].getIntValue();
    channel.algorithm = tokens[2].getIntValue();
    channel.ams = tokens[3].getIntValue();
    channel.pms = tokens[4].getIntValue();
    // Convert VOPMex OpMsk format to internal (3-bit right shift)
    channel.slotMask = convertOpmSlotToInternal(tokens[5].getIntValue());
    channel.noiseEnable = tokens[6].getIntValue();
}

void VOPMParser::parseOperator(const juce::String& line, VOPMVoice::Operator& op)
{
    auto tokens = tokenizeLine(line);
    
    if (tokens.size() >= 11)
    {
        op.attackRate = tokens[0].getIntValue();
        op.decay1Rate = tokens[1].getIntValue();
        op.decay2Rate = tokens[2].getIntValue();
        op.releaseRate = tokens[3].getIntValue();
        op.decay1Level = tokens[4].getIntValue();
        op.totalLevel = tokens[5].getIntValue();
        op.keyScale = tokens[6].getIntValue();
        op.multiple = tokens[7].getIntValue();
        op.detune1 = tokens[8].getIntValue();
        op.detune2 = tokens[9].getIntValue();
        // Convert OPM AME format (0 or 128) to internal (0-1)
        op.amsEnable = convertOpmAmeToInternal(tokens[10].getIntValue());
    }
}

juce::StringArray VOPMParser::tokenizeLine(const juce::String& line)
{
    // Get content after the colon
    auto content = line.fromFirstOccurrenceOf(":", false, false).trim();
    
    // Normalize whitespace by replacing multiple spaces/tabs with single spaces
    juce::String normalized = content;
    normalized = normalized.replace("\t", " ");  // Replace tabs with spaces
    
    // Replace multiple consecutive spaces with single space
    while (normalized.contains("  ")) {
        normalized = normalized.replace("  ", " ");
    }
    normalized = normalized.trim();
    
    // Split by single space
    juce::StringArray tokens;
    tokens.addTokens(normalized, " ", "");
    
    return tokens;
}

bool VOPMParser::isCommentLine(const juce::String& line)
{
    auto trimmed = line.trim();
    return trimmed.startsWith("//") || 
           trimmed.startsWith(";") || 
           trimmed.startsWith("#");
}

bool VOPMParser::isValidRange(int value, int min, int max)
{
    return value >= min && value <= max;
}

} // namespace ymulatorsynth
