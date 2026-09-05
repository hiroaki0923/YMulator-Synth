#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/PresetManager.h"
#include <cstdio>

// Register-level golden test: for every bundled preset, loading it and playing
// a note must leave the YM2151 registers exactly as the .opm data prescribes.
// This pins the whole path preset -> parameters -> ParameterManager -> wrapper
// -> registers, independent of how the emulator then renders it.
namespace {

constexpr int kNote = 60;                 // C4 -> key code octave 3, note code 14
constexpr uint8_t kExpectedKeyCode = (3 << YM2151Regs::SHIFT_OCTAVE) | 14;

uint8_t r(const YmfmWrapperInterface& w, int address) { return w.readCurrentRegister(address); }

} // namespace

class RegisterGoldenTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    
    void process(juce::MidiBuffer& midi) {
        juce::AudioBuffer<float> buffer(2, 512);
        processor.processBlock(buffer, midi);
        midi.clear();
    }
    
    void checkPreset(int bank, int presetInBank) {
        const auto* preset = processor.getPresetManager().getPresetInBank(bank, presetInBank);
        ASSERT_NE(preset, nullptr);
        SCOPED_TRACE("bank " + std::to_string(bank) + " preset " + std::to_string(presetInBank)
                     + " \"" + preset->name.toStdString() + "\"");
        
        processor.setCurrentPresetInBank(bank, presetInBank);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, kNote, static_cast<juce::uint8>(127)), 0);
        process(midi);
        
        const auto& w = processor.getYmfmWrapper();
        const uint8_t keyOn = r(w, YM2151Regs::REG_KEY_ON_OFF);
        const int ch = keyOn & 0x07;
        
        uint8_t slotMask = 0;
        for (int op = 0; op < 4; ++op)
            if (preset->operators[op].slotEnable) slotMask |= static_cast<uint8_t>(1 << op);
        EXPECT_EQ(keyOn, static_cast<uint8_t>(YM2151Regs::keyOnBitsForSlotMask(slotMask) | ch)) << "key-on slot mask";
        
        EXPECT_EQ(r(w, YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch),
                  static_cast<uint8_t>(YM2151Regs::PAN_CENTER | (preset->feedback << YM2151Regs::SHIFT_FEEDBACK) | preset->algorithm))
            << "pan/feedback/algorithm";
        EXPECT_EQ(r(w, YM2151Regs::REG_KEY_CODE_BASE + ch), kExpectedKeyCode) << "key code";
        EXPECT_EQ(r(w, YM2151Regs::REG_KEY_FRACTION_BASE + ch), 0) << "key fraction";
        
        EXPECT_EQ(r(w, YM2151Regs::REG_LFO_RATE), static_cast<uint8_t>(preset->lfo.rate)) << "LFO rate";
        EXPECT_EQ(r(w, YM2151Regs::REG_LFO_DEPTH), static_cast<uint8_t>(YM2151Regs::LFO_DEPTH_SELECT_PMD | preset->lfo.pmd)) << "PMD is the last depth written";
        EXPECT_EQ(r(w, YM2151Regs::REG_LFO_WAVEFORM) & YM2151Regs::MASK_LFO_WAVEFORM, preset->lfo.waveform) << "LFO waveform";
        EXPECT_EQ(r(w, YM2151Regs::REG_LFO_AMS_PMS_BASE + ch),
                  static_cast<uint8_t>((preset->channels[0].pms << YM2151Regs::SHIFT_LFO_PMS) | preset->channels[0].ams)) << "AMS/PMS";
        EXPECT_EQ(r(w, YM2151Regs::REG_NOISE_CONTROL),
                  static_cast<uint8_t>((preset->channels[0].noiseEnable ? YM2151Regs::MASK_NOISE_ENABLE : 0) | preset->lfo.noiseFreq)) << "noise";
        
        for (int op = 0; op < 4; ++op) {
            SCOPED_TRACE("operator " + std::to_string(op + 1));
            const auto& o = preset->operators[op];
            const int slot = YM2151Regs::OPERATOR_SLOT_OFFSET[op] + ch;
            const auto u8 = [](float v) { return static_cast<uint8_t>(v); };
            EXPECT_EQ(r(w, YM2151Regs::REG_DT1_MUL_BASE + slot), static_cast<uint8_t>((u8(o.detune1) << YM2151Regs::SHIFT_DETUNE1) | u8(o.multiple))) << "DT1/MUL";
            EXPECT_EQ(r(w, YM2151Regs::REG_TOTAL_LEVEL_BASE + slot), u8(o.totalLevel)) << "TL";
            EXPECT_EQ(r(w, YM2151Regs::REG_KS_AR_BASE + slot), static_cast<uint8_t>((u8(o.keyScale) << YM2151Regs::SHIFT_KEY_SCALE) | u8(o.attackRate))) << "KS/AR";
            EXPECT_EQ(r(w, YM2151Regs::REG_AMS_D1R_BASE + slot), static_cast<uint8_t>(((o.amsEnable ? 1 : 0) << YM2151Regs::SHIFT_AMS_ENABLE) | u8(o.decay1Rate))) << "AMS-EN/D1R";
            EXPECT_EQ(r(w, YM2151Regs::REG_DT2_D2R_BASE + slot), static_cast<uint8_t>((u8(o.detune2) << YM2151Regs::SHIFT_DETUNE2) | u8(o.decay2Rate))) << "DT2/D2R";
            EXPECT_EQ(r(w, YM2151Regs::REG_D1L_RR_BASE + slot), static_cast<uint8_t>((u8(o.sustainLevel) << YM2151Regs::SHIFT_SUSTAIN_LEVEL) | u8(o.releaseRate))) << "D1L/RR";
        }
        
        midi.addEvent(juce::MidiMessage::noteOff(1, kNote), 0);
        process(midi);
        EXPECT_EQ(r(w, YM2151Regs::REG_KEY_ON_OFF), static_cast<uint8_t>(ch)) << "key off";
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(RegisterGoldenTest, EveryBundledPresetProducesExpectedRegisters)
{
    const auto& presets = processor.getPresetManager();
    const int numBanks = static_cast<int>(presets.getBanks().size());
    int checked = 0;
    for (int bank = 0; bank < numBanks; ++bank) {
        const int count = presets.getPresetsForBank(bank).size();
        std::printf("bank %d \"%s\": %d presets\n", bank, presets.getBanks()[static_cast<size_t>(bank)].name.c_str(), count);
        for (int i = 0; i < count; ++i) { checkPreset(bank, i); ++checked; }
    }
    // 8 factory presets plus the 64-voice bundled collection are always present
    EXPECT_GE(checked, 72) << "bundled presets missing from the test build";
}
