#pragma once

#include <string>

// Parameter ID definitions for ChipSynth-AU
// This header provides type-safe parameter ID management to prevent
// string literal errors and improve maintainability.

namespace ParamID {

// =============================================================================
// Global Parameters
// =============================================================================

namespace Global {
    // Main synthesis parameters
    constexpr const char* Algorithm = "algorithm";
    constexpr const char* Feedback = "feedback";
    
    // Preset management
    constexpr const char* PresetIndex = "presetIndex";
    constexpr const char* PresetIndexChanged = "presetIndexChanged";
    constexpr const char* IsCustomMode = "isCustomMode";
    
    // Global settings
    constexpr const char* PitchBendRange = "pitch_bend_range";
    constexpr const char* MasterPan = "master_pan";
} // namespace Global

// =============================================================================
// Channel-Specific Parameters
// =============================================================================

namespace Channel {
    // Parameter name suffix for channels
    constexpr const char* Pan = "_pan";
    
    // Helper function to generate channel parameter IDs (channels 0-7)
    inline std::string pan(int channelNum) {
        return "ch" + std::to_string(channelNum) + Pan;
    }
    
    // Convenience function to get all parameter IDs for a channel
    struct ChannelParams {
        std::string pan;
        
        ChannelParams(int channelNum) 
            : pan(Channel::pan(channelNum))
        {}
    };
    
} // namespace Channel

// =============================================================================
// Operator-Specific Parameters
// =============================================================================

namespace Op {
    // Parameter name suffixes for operators
    constexpr const char* TotalLevel = "_tl";
    constexpr const char* AttackRate = "_ar";
    constexpr const char* Decay1Rate = "_d1r";
    constexpr const char* Decay2Rate = "_d2r";
    constexpr const char* ReleaseRate = "_rr";
    constexpr const char* SustainLevel = "_d1l";
    constexpr const char* Multiple = "_mul";
    constexpr const char* Detune1 = "_dt1";
    constexpr const char* Detune2 = "_dt2";
    constexpr const char* KeyScale = "_ks";
    constexpr const char* AmsEnable = "_ams_en";
    
    // Helper functions to generate operator parameter IDs
    inline std::string tl(int opNum) { 
        return "op" + std::to_string(opNum) + TotalLevel; 
    }
    
    inline std::string ar(int opNum) { 
        return "op" + std::to_string(opNum) + AttackRate; 
    }
    
    inline std::string d1r(int opNum) { 
        return "op" + std::to_string(opNum) + Decay1Rate; 
    }
    
    inline std::string d2r(int opNum) { 
        return "op" + std::to_string(opNum) + Decay2Rate; 
    }
    
    inline std::string rr(int opNum) { 
        return "op" + std::to_string(opNum) + ReleaseRate; 
    }
    
    inline std::string d1l(int opNum) { 
        return "op" + std::to_string(opNum) + SustainLevel; 
    }
    
    inline std::string mul(int opNum) { 
        return "op" + std::to_string(opNum) + Multiple; 
    }
    
    inline std::string dt1(int opNum) { 
        return "op" + std::to_string(opNum) + Detune1; 
    }
    
    inline std::string dt2(int opNum) { 
        return "op" + std::to_string(opNum) + Detune2; 
    }
    
    inline std::string ks(int opNum) { 
        return "op" + std::to_string(opNum) + KeyScale; 
    }
    
    inline std::string ams_en(int opNum) { 
        return "op" + std::to_string(opNum) + AmsEnable; 
    }
    
    // Convenience function to get all parameter IDs for an operator
    struct OperatorParams {
        std::string tl, ar, d1r, d2r, rr, d1l, mul, dt1, dt2, ks, ams_en;
        
        OperatorParams(int opNum) 
            : tl(Op::tl(opNum))
            , ar(Op::ar(opNum))
            , d1r(Op::d1r(opNum))
            , d2r(Op::d2r(opNum))
            , rr(Op::rr(opNum))
            , d1l(Op::d1l(opNum))
            , mul(Op::mul(opNum))
            , dt1(Op::dt1(opNum))
            , dt2(Op::dt2(opNum))
            , ks(Op::ks(opNum))
            , ams_en(Op::ams_en(opNum))
        {}
    };
    
} // namespace Op

// =============================================================================
// MIDI CC Mapping Constants
// =============================================================================

namespace MIDI_CC {
    // VOPMex compatible MIDI CC mapping
    constexpr int Algorithm = 14;
    constexpr int Feedback = 15;
    
    // Channel Pan (CC 32-39) - ChipSynth extension
    constexpr int Ch0_Pan = 32;
    constexpr int Ch1_Pan = 33;
    constexpr int Ch2_Pan = 34;
    constexpr int Ch3_Pan = 35;
    constexpr int Ch4_Pan = 36;
    constexpr int Ch5_Pan = 37;
    constexpr int Ch6_Pan = 38;
    constexpr int Ch7_Pan = 39;
    
    // Operator Total Level (CC 16-19)
    constexpr int Op1_TL = 16;
    constexpr int Op2_TL = 17;
    constexpr int Op3_TL = 18;
    constexpr int Op4_TL = 19;
    
    // Operator Multiple (CC 20-23)
    constexpr int Op1_MUL = 20;
    constexpr int Op2_MUL = 21;
    constexpr int Op3_MUL = 22;
    constexpr int Op4_MUL = 23;
    
    // Operator Detune1 (CC 24-27)
    constexpr int Op1_DT1 = 24;
    constexpr int Op2_DT1 = 25;
    constexpr int Op3_DT1 = 26;
    constexpr int Op4_DT1 = 27;
    
    // Operator Detune2 (CC 28-31) - ChipSynth extension
    constexpr int Op1_DT2 = 28;
    constexpr int Op2_DT2 = 29;
    constexpr int Op3_DT2 = 30;
    constexpr int Op4_DT2 = 31;
    
    // Operator Key Scale (CC 39-42)
    constexpr int Op1_KS = 39;
    constexpr int Op2_KS = 40;
    constexpr int Op3_KS = 41;
    constexpr int Op4_KS = 42;
    
    // Operator Attack Rate (CC 43-46)
    constexpr int Op1_AR = 43;
    constexpr int Op2_AR = 44;
    constexpr int Op3_AR = 45;
    constexpr int Op4_AR = 46;
    
    // Operator Decay1 Rate (CC 47-50)
    constexpr int Op1_D1R = 47;
    constexpr int Op2_D1R = 48;
    constexpr int Op3_D1R = 49;
    constexpr int Op4_D1R = 50;
    
    // Operator Decay2 Rate (CC 51-54)
    constexpr int Op1_D2R = 51;
    constexpr int Op2_D2R = 52;
    constexpr int Op3_D2R = 53;
    constexpr int Op4_D2R = 54;
    
    // Operator Release Rate (CC 55-58)
    constexpr int Op1_RR = 55;
    constexpr int Op2_RR = 56;
    constexpr int Op3_RR = 57;
    constexpr int Op4_RR = 58;
    
    // Operator Decay1 Level (CC 59-62)
    constexpr int Op1_D1L = 59;
    constexpr int Op2_D1L = 60;
    constexpr int Op3_D1L = 61;
    constexpr int Op4_D1L = 62;
    
    // Helper function to get CC number for operator parameter
    inline int getOpCC(int opNum, const char* paramType) {
        if (std::string(paramType) == Op::TotalLevel) {
            return Op1_TL + opNum - 1;
        } else if (std::string(paramType) == Op::Multiple) {
            return Op1_MUL + opNum - 1;
        } else if (std::string(paramType) == Op::Detune1) {
            return Op1_DT1 + opNum - 1;
        } else if (std::string(paramType) == Op::Detune2) {
            return Op1_DT2 + opNum - 1;
        } else if (std::string(paramType) == Op::KeyScale) {
            return Op1_KS + opNum - 1;
        } else if (std::string(paramType) == Op::AttackRate) {
            return Op1_AR + opNum - 1;
        } else if (std::string(paramType) == Op::Decay1Rate) {
            return Op1_D1R + opNum - 1;
        } else if (std::string(paramType) == Op::Decay2Rate) {
            return Op1_D2R + opNum - 1;
        } else if (std::string(paramType) == Op::ReleaseRate) {
            return Op1_RR + opNum - 1;
        } else if (std::string(paramType) == Op::SustainLevel) {
            return Op1_D1L + opNum - 1;
        }
        return -1; // Invalid parameter type
    }
} // namespace MIDI_CC

// =============================================================================
// Parameter Validation
// =============================================================================

namespace Validation {
    // Check if a parameter ID is valid
    inline bool isValidParameterID(const std::string& paramID) {
        // Global parameters
        if (paramID == Global::Algorithm || 
            paramID == Global::Feedback ||
            paramID == Global::PresetIndex ||
            paramID == Global::PresetIndexChanged ||
            paramID == Global::IsCustomMode ||
            paramID == Global::PitchBendRange ||
            paramID == Global::MasterPan) {
            return true;
        }
        
        // Check channel parameters (ch0-ch7)
        for (int ch = 0; ch < 8; ++ch) {
            if (paramID == Channel::pan(ch)) {
                return true;
            }
        }
        
        // Check operator parameters (op1-op4)
        for (int op = 1; op <= 4; ++op) {
            if (paramID == Op::tl(op) || paramID == Op::ar(op) ||
                paramID == Op::d1r(op) || paramID == Op::d2r(op) ||
                paramID == Op::rr(op) || paramID == Op::d1l(op) ||
                paramID == Op::mul(op) || paramID == Op::dt1(op) ||
                paramID == Op::dt2(op) || paramID == Op::ks(op) ||
                paramID == Op::ams_en(op)) {
                return true;
            }
        }
        
        return false;
    }
    
    // Extract operator number from parameter ID (1-4, or 0 if not operator param)
    inline int getOperatorNumber(const std::string& paramID) {
        if (paramID.length() >= 3 && paramID.substr(0, 2) == "op") {
            char opChar = paramID[2];
            if (opChar >= '1' && opChar <= '4') {
                return opChar - '0';
            }
        }
        return 0; // Not an operator parameter
    }
    
    // Check if parameter ID is for a specific operator
    inline bool isOperatorParameter(const std::string& paramID) {
        return getOperatorNumber(paramID) > 0;
    }
    
    // Check if parameter ID is a global parameter
    inline bool isGlobalParameter(const std::string& paramID) {
        return !isOperatorParameter(paramID) && isValidParameterID(paramID);
    }
} // namespace Validation

} // namespace ParamID