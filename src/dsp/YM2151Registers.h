#pragma once

#include <cstdint>

// YM2151 (OPM) and YM2608 (OPNA) Register Definitions and Constants
// This header defines all register addresses, bitmasks, and constants
// used for programming the Yamaha FM synthesis chips.

namespace YM2151Regs {

// =============================================================================
// Register Address Constants
// =============================================================================

// Channel Control Registers (0x20-0x3F range)
constexpr uint8_t REG_ALGORITHM_FEEDBACK_BASE = 0x20;  // 0x20 + channel
constexpr uint8_t REG_KEY_CODE_BASE = 0x28;           // 0x28 + channel  
constexpr uint8_t REG_KEY_FRACTION_BASE = 0x30;       // 0x30 + channel

// Operator Parameter Registers (base_addr = operator * 8 + channel)
constexpr uint8_t REG_DT1_MUL_BASE = 0x40;           // 0x40 + base_addr
constexpr uint8_t REG_TOTAL_LEVEL_BASE = 0x60;       // 0x60 + base_addr
constexpr uint8_t REG_KS_AR_BASE = 0x80;             // 0x80 + base_addr
constexpr uint8_t REG_AMS_D1R_BASE = 0xA0;           // 0xA0 + base_addr
constexpr uint8_t REG_DT2_D2R_BASE = 0xC0;           // 0xC0 + base_addr
constexpr uint8_t REG_D1L_RR_BASE = 0xE0;            // 0xE0 + base_addr

// System Control Registers
constexpr uint8_t REG_KEY_ON_OFF = 0x08;             // Key on/off register

// =============================================================================
// YM2608 (OPNA) Specific Registers
// =============================================================================

constexpr uint8_t REG_OPNA_MODE = 0x29;              // OPNA mode register
constexpr uint8_t REG_OPNA_FNUM_LOW_BASE = 0xA0;     // 0xA0 + channel
constexpr uint8_t REG_OPNA_FNUM_HIGH_BASE = 0xA4;    // 0xA4 + channel
constexpr uint8_t REG_OPNA_TL_OP2_BASE = 0x44;       // 0x44 + channel
constexpr uint8_t REG_OPNA_KEY_ON_OFF = 0x28;        // OPNA key on/off

// =============================================================================
// Bitmask Constants
// =============================================================================

// Key On/Off Control
constexpr uint8_t KEY_ON_ALL_OPS = 0x78;             // All 4 operators on
constexpr uint8_t KEY_OFF_MASK = 0x00;               // Key off value
constexpr uint8_t OPNA_KEY_ON_ALL_OPS = 0xF0;        // OPNA all operators on

// Register Field Masks (for parameter extraction)
constexpr uint8_t MASK_ALGORITHM = 0x07;             // Algorithm field (bits 0-2)
constexpr uint8_t MASK_FEEDBACK = 0x07;              // Feedback field (bits 0-2)
constexpr uint8_t MASK_MULTIPLE = 0x0F;              // Multiple field (bits 0-3)
constexpr uint8_t MASK_DETUNE1 = 0x07;               // Detune1 field (bits 0-2)
constexpr uint8_t MASK_DETUNE2 = 0x03;               // Detune2 field (bits 0-1)
constexpr uint8_t MASK_KEY_SCALE = 0x03;             // Key scale field (bits 0-1)
constexpr uint8_t MASK_ATTACK_RATE = 0x1F;           // Attack rate field (bits 0-4)
constexpr uint8_t MASK_DECAY1_RATE = 0x1F;           // Decay1 rate field (bits 0-4)
constexpr uint8_t MASK_DECAY2_RATE = 0x1F;           // Decay2 rate field (bits 0-4)
constexpr uint8_t MASK_RELEASE_RATE = 0x0F;          // Release rate field (bits 0-3)
constexpr uint8_t MASK_SUSTAIN_LEVEL = 0x0F;         // Sustain level field (bits 0-3)
constexpr uint8_t MASK_KEY_FRACTION = 0x3F;          // Key fraction field (bits 0-5)
constexpr uint8_t MASK_KEY_CODE = 0x7F;              // Key code field (bits 0-6)
constexpr uint8_t MASK_OCTAVE = 0x07;                // Octave field (bits 0-2)

// Pan Control Masks (for register 0x20 + channel)
constexpr uint8_t MASK_LEFT_ENABLE = 0x80;           // Left output enable (bit 7)
constexpr uint8_t MASK_RIGHT_ENABLE = 0x40;          // Right output enable (bit 6)
constexpr uint8_t MASK_PAN_LR = 0xC0;                // Both L/R enable bits (bits 6-7)

// Preserve Field Masks (for read-modify-write operations)
constexpr uint8_t PRESERVE_ALG_FB_LR = 0xF8;         // Preserve L/R/FB, update ALG
constexpr uint8_t PRESERVE_ALG_LR = 0xC7;            // Preserve L/R/ALG, update FB
constexpr uint8_t PRESERVE_ALG_FB = 0x3F;            // Preserve ALG/FB, update L/R
constexpr uint8_t PRESERVE_KS = 0xC0;                // Preserve KS, update AR
constexpr uint8_t PRESERVE_AMS = 0x80;               // Preserve AMS-EN, update D1R
constexpr uint8_t PRESERVE_DT2 = 0xC0;               // Preserve DT2, update D2R
constexpr uint8_t PRESERVE_D1L = 0xF0;               // Preserve D1L, update RR
constexpr uint8_t PRESERVE_RR = 0x0F;                // Preserve RR, update D1L
constexpr uint8_t PRESERVE_MUL = 0x70;               // Preserve DT1, update MUL
constexpr uint8_t PRESERVE_DT1 = 0x0F;               // Preserve MUL, update DT1
constexpr uint8_t PRESERVE_D2R = 0x1F;               // Preserve D2R, update DT2
constexpr uint8_t PRESERVE_AR = 0x1F;                // Preserve AR, update KS

// =============================================================================
// Bit Shift Constants
// =============================================================================

constexpr uint8_t SHIFT_FEEDBACK = 3;                // Feedback field shift (bits 3-5)
constexpr uint8_t SHIFT_DETUNE1 = 4;                 // Detune1 field shift (bits 4-6)  
constexpr uint8_t SHIFT_DETUNE2 = 6;                 // Detune2 field shift (bits 6-7)
constexpr uint8_t SHIFT_KEY_SCALE = 6;               // Key scale field shift (bits 6-7)
constexpr uint8_t SHIFT_SUSTAIN_LEVEL = 4;           // Sustain level shift (bits 4-7)
constexpr uint8_t SHIFT_KEY_FRACTION = 2;            // Key fraction shift for register format
constexpr uint8_t SHIFT_KEY_CODE = 6;                // Key code shift in FNUM
constexpr uint8_t SHIFT_OCTAVE = 4;                  // Octave shift in key code
constexpr uint8_t SHIFT_OPNA_BLOCK = 3;              // OPNA block shift

// =============================================================================
// Audio Processing Constants
// =============================================================================

// Clock and Sample Rate Constants
constexpr uint32_t OPM_DEFAULT_CLOCK = 3579545;      // Default YM2151 clock frequency
constexpr uint32_t OPNA_INTERNAL_RATE = 55466;       // OPNA internal sample rate
constexpr uint32_t DEFAULT_OUTPUT_RATE = 44100;      // Default output sample rate
constexpr uint32_t OPM_INTERNAL_RATE = 62500;        // OPM internal rate (fallback)

// Audio Format Constants
constexpr float SAMPLE_SCALE_FACTOR = 32768.0f;      // Int16 to float conversion
constexpr float TEST_FREQUENCY = 440.0f;             // A4 test tone frequency
constexpr float TEST_AMPLITUDE = 0.3f;               // Test tone amplitude
constexpr float SEMITONE_RATIO = 2.0f;               // Frequency ratio per octave
constexpr float REFERENCE_FREQUENCY = 440.0f;        // A4 reference frequency

// =============================================================================
// Channel and Voice Constants
// =============================================================================

constexpr uint8_t MAX_OPM_CHANNELS = 8;              // YM2151 channels
constexpr uint8_t MAX_OPNA_FM_CHANNELS = 6;          // YM2608 FM channels
constexpr uint8_t MAX_OPERATORS_PER_VOICE = 4;       // FM operators per voice
constexpr uint8_t OPERATOR_ADDRESS_STEP = 8;         // Address step between operators

// =============================================================================
// MIDI and Note Constants
// =============================================================================

constexpr uint8_t MIDI_NOTE_C4 = 60;                 // Middle C
constexpr uint8_t MIDI_NOTE_A4 = 69;                 // A4 reference note
constexpr uint8_t MAX_VELOCITY = 127;                // Maximum MIDI velocity
constexpr uint8_t VELOCITY_TO_TL_OFFSET = 127;       // Velocity to TL conversion
constexpr uint8_t MAX_OCTAVE = 7;                    // Maximum octave for YM2151
constexpr uint8_t MIN_OCTAVE = 0;                    // Minimum octave
constexpr uint8_t NOTES_PER_OCTAVE = 12;             // Chromatic scale
constexpr uint8_t KF_SCALE_FACTOR = 64;              // KF fractional scaling

// =============================================================================
// Default Parameter Values
// =============================================================================

// Default voice parameters (for setupBasicPianoVoice)
constexpr uint8_t DEFAULT_ALGORITHM_FB_LR = 0xC7;    // Algorithm 7, L/R both, FB=0
constexpr uint8_t DEFAULT_DT1_MUL = 0x01;            // DT1=0, MUL=1
constexpr uint8_t DEFAULT_TOTAL_LEVEL = 32;          // Moderate volume
constexpr uint8_t DEFAULT_KS_AR = 0x1F;              // KS=0, AR=31 (fast attack)
constexpr uint8_t DEFAULT_AMS_D1R = 0x00;            // AMS-EN=0, D1R=0
constexpr uint8_t DEFAULT_DT2_D2R = 0x00;            // DT2=0, D2R=0  
constexpr uint8_t DEFAULT_D1L_RR = 0xF7;             // D1L=15, RR=7
constexpr uint8_t OPNA_MODE_VALUE = 0x9f;            // OPNA extended mode enable

// Pan Setting Values (for register 0x20 + channel bits 6-7)
constexpr uint8_t PAN_OFF = 0x00;                    // No output (L=0, R=0)
constexpr uint8_t PAN_RIGHT_ONLY = 0x40;             // Right only (L=0, R=1)
constexpr uint8_t PAN_LEFT_ONLY = 0x80;              // Left only (L=1, R=0)
constexpr uint8_t PAN_CENTER = 0xC0;                 // Center/Both (L=1, R=1)

// =============================================================================
// Debug and Testing Constants
// =============================================================================

constexpr int DEBUG_SAMPLE_COUNT = 10;               // Number of samples to debug
constexpr int DEBUG_COUNTER_INTERVAL = 1000;         // Debug output interval
constexpr int MAX_DEBUG_CALLS = 5;                   // Maximum debug calls

// =============================================================================
// Helper Functions for Address Calculation
// =============================================================================

// Calculate operator register address (operator 0-3, channel 0-7)
constexpr uint8_t getOperatorRegister(uint8_t baseReg, uint8_t operator_num, uint8_t channel) {
    return baseReg + (operator_num * OPERATOR_ADDRESS_STEP) + channel;
}

// Calculate channel register address
constexpr uint8_t getChannelRegister(uint8_t baseReg, uint8_t channel) {
    return baseReg + channel;
}

// Convert pan parameter (0.0=left, 0.5=center, 1.0=right) to YM2151 pan bits
constexpr uint8_t panValueToPanBits(float panValue) {
    if (panValue <= 0.25f) {
        return PAN_LEFT_ONLY;
    } else if (panValue >= 0.75f) {
        return PAN_RIGHT_ONLY;
    } else {
        return PAN_CENTER;
    }
}

} // namespace YM2151Regs