#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace ymulatorsynth {

enum class GeneratorCategory { Bass, Lead, Brass, EPiano, Bell, Pad, SE, Any, Count };

/** What the user asked for: a category and six directions, each 0..1. */
struct GeneratorInput
{
    GeneratorCategory category = GeneratorCategory::Any;
    float bright = 0.5f;       // 0 dark .. 1 bright
    float complex = 0.5f;      // 0 simple .. 1 complex
    float hardAttack = 0.5f;   // 0 soft .. 1 hard
    float length = 0.5f;       // 0 short .. 1 long
    float moving = 0.2f;       // 0 still .. 1 moving
    float metallic = 0.2f;     // 0 harmonic .. 1 metallic
};

/** A complete raw patch, operators in voice order (M1, C1, M2, C2). */
struct GeneratedPatch
{
    struct Operator { int tl = 0, ar = 31, d1r = 0, d1l = 0, d2r = 0, rr = 7, ks = 0, mul = 1, dt1 = 0, dt2 = 0; bool amsEnable = false; };
    int algorithm = 0, feedback = 0;
    std::array<Operator, 4> ops;
    int lfoRate = 0, lfoAmd = 0, lfoPmd = 0, lfoWaveform = 0, lfoAms = 0, lfoPms = 0;
    bool noiseEnable = false;
    int noiseFrequency = 0;
};

/**
 * Guided random patches. The category chooses the algorithms and envelope
 * ranges; the directions pick where inside those ranges the values land.
 * Same input and seed always give the same patch.
 * Spec: docs/ymulatorsynth-quick-panel-design.md section 3.
 */
class PatchGenerator
{
public:
    static GeneratedPatch generate(const GeneratorInput& input, juce::int64 seed);
    static const std::vector<int>& algorithmCandidates(GeneratorCategory category);
    static const char* categoryName(GeneratorCategory category);
};

} // namespace ymulatorsynth
