#include "PatchGenerator.h"
#include "MacroMapper.h"
#include "../dsp/AlgorithmInfo.h"

namespace ymulatorsynth {

namespace {

struct Range { int lo, hi; };

struct CategoryRule {
    const char* name;
    std::vector<int> algorithms;
    Range carrierAr, carrierD1r, carrierD1l, carrierRr;
    Range modulatorD1r;
    Range modulatorAr;      // {-1,-1}: follow the carrier attack
    Range carrierMul;
    int feedbackMin;
    float metallicMin;
    float noiseChance;
    int maxPmd;
};

// Section 3.3 of the design; tuned by ear later
const std::array<CategoryRule, 8> kRules = {{
    { "Bass",    { 0, 1, 4, 6 },          { 25, 31 }, { 6, 14 }, { 2, 6 },  { 6, 10 }, { 10, 20 }, { -1, -1 }, { 0, 1 }, 0, 0.0f, 0.0f, 40 },
    { "Lead",    { 0, 2, 3, 4 },          { 20, 31 }, { 0, 6 },  { 0, 2 },  { 4, 8 },  { 4, 12 },  { -1, -1 }, { 1, 2 }, 4, 0.0f, 0.0f, 40 },
    { "Brass",   { 1, 3, 4 },             { 12, 20 }, { 4, 10 }, { 1, 3 },  { 5, 8 },  { 4, 12 },  { 12, 20 }, { 1, 1 }, 0, 0.0f, 0.0f, 40 },
    { "E.Piano", { 3, 4, 5 },             { 28, 31 }, { 6, 12 }, { 4, 8 },  { 6, 9 },  { 10, 18 }, { -1, -1 }, { 1, 2 }, 0, 0.0f, 0.0f, 24 },
    { "Bell",    { 4, 5, 7 },             { 31, 31 }, { 2, 6 },  { 6, 10 }, { 3, 6 },  { 2, 8 },   { 28, 31 }, { 1, 2 }, 0, 0.6f, 0.0f, 24 },
    { "Pad",     { 5, 6, 7 },             { 6, 12 },  { 0, 4 },  { 0, 1 },  { 3, 6 },  { 0, 6 },   { 6, 12 },  { 1, 2 }, 0, 0.0f, 0.0f, 12 },
    { "SE",      { 0, 1, 2 },             { 0, 31 },  { 0, 31 }, { 0, 15 }, { 0, 15 }, { 0, 31 },  { 0, 31 },  { 0, 15 }, 7, 0.4f, 0.3f, 127 },
    { "Any",     { 0, 1, 2, 3, 4, 5, 6, 7 }, { 4, 31 }, { 0, 20 }, { 0, 12 }, { 2, 12 }, { 0, 24 }, { -1, -1 }, { 0, 4 }, 0, 0.0f, 0.05f, 60 },
}};

// Simple to complex, used with the Complex direction
constexpr int kComplexityOrder[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };

const int kMetallicMultiples[] = { 3, 5, 7, 9, 11, 13 };
const int kDetuneChoices[] = { 0, 0, 1, -1, 2, -2 };

int lerpInt(int lo, int hi, float t) { return juce::roundToInt(lo + (hi - lo) * juce::jlimit(0.0f, 1.0f, t)); }

} // namespace

const std::vector<int>& PatchGenerator::algorithmCandidates(GeneratorCategory category)
{
    return kRules[static_cast<size_t>(category)].algorithms;
}

const char* PatchGenerator::categoryName(GeneratorCategory category)
{
    return kRules[static_cast<size_t>(category)].name;
}

GeneratedPatch PatchGenerator::generate(const GeneratorInput& input, juce::int64 seed)
{
    const auto& rule = kRules[static_cast<size_t>(input.category)];
    juce::Random rng(seed);
    auto uniform = [&](int lo, int hi) { return lo + rng.nextInt(hi - lo + 1); };
    auto uniformR = [&](Range r) { return uniform(r.lo, r.hi); };
    auto jitter = [&](int v, int amount, int lo, int hi) { return juce::jlimit(lo, hi, v + uniform(-amount, amount)); };
    auto pick = [&](const Range& r, float t, int j) { return jitter(lerpInt(r.lo, r.hi, t), j, r.lo, r.hi); };
    
    GeneratedPatch patch;
    
    // 1. Algorithm by complexity, feedback with it
    std::vector<int> ordered;
    for (int alg : kComplexityOrder)
        if (std::find(rule.algorithms.begin(), rule.algorithms.end(), alg) != rule.algorithms.end()) ordered.push_back(alg);
    int index = lerpInt(0, static_cast<int>(ordered.size()) - 1, input.complex);
    if (rng.nextFloat() < 0.3f) index = juce::jlimit(0, static_cast<int>(ordered.size()) - 1, index + (rng.nextBool() ? 1 : -1));
    patch.algorithm = ordered[static_cast<size_t>(index)];
    patch.feedback = jitter(lerpInt(0, 7, input.complex), 1, rule.feedbackMin, 7);
    const auto& info = algorithmInfo(patch.algorithm);
    
    // 2. Levels: modulators by brightness, carriers near full
    const Range modulatorTl { lerpInt(60, 10, input.bright), lerpInt(100, 45, input.bright) };
    bool firstCarrier = true;
    for (int op = 0; op < 4; ++op) {
        auto& o = patch.ops[static_cast<size_t>(op)];
        if (info.isCarrier(op)) {
            o.tl = firstCarrier ? uniform(0, 4) : uniform(0, 12);
            firstCarrier = false;
        } else {
            o.tl = uniformR(modulatorTl);
        }
    }
    
    // 3. Envelopes
    const int carrierAr = pick(rule.carrierAr, input.hardAttack, 2);
    const int carrierRr = pick({ rule.carrierRr.hi, rule.carrierRr.lo }, input.length, 1);
    for (int op = 0; op < 4; ++op) {
        auto& o = patch.ops[static_cast<size_t>(op)];
        if (info.isCarrier(op)) {
            o.ar = jitter(carrierAr, 1, rule.carrierAr.lo, rule.carrierAr.hi);
            o.d1r = pick({ rule.carrierD1r.hi, rule.carrierD1r.lo }, input.length, 2);
            o.d1l = uniformR(rule.carrierD1l);
            o.rr = jitter(carrierRr, 1, rule.carrierRr.lo, rule.carrierRr.hi);
            o.d2r = juce::jlimit(0, 31, lerpInt(0, 6, input.moving) + (input.length < 0.3f ? uniform(0, 3) : 0));
        } else {
            o.ar = rule.modulatorAr.lo >= 0 ? pick(rule.modulatorAr, input.hardAttack, 2)
                                            : juce::jlimit(0, 31, carrierAr + uniform(-3, 3));
            // A fast modulator decay gives the classic "bright only at the attack"
            o.d1r = pick(rule.modulatorD1r, input.hardAttack, 2);
            o.d1l = uniform(2, 8);
            o.rr = juce::jlimit(0, 15, carrierRr + uniform(-1, 1));
            o.d2r = uniform(0, 2);
        }
        o.ks = info.isCarrier(op) ? uniform(0, 2) : uniform(0, 1);
    }
    
    // 4. Motion through the hardware LFO
    if (input.moving > 0.15f) {
        patch.lfoRate = uniform(150, 220);
        patch.lfoPmd = juce::jmin(rule.maxPmd, lerpInt(0, 40, input.moving));
        patch.lfoAmd = lerpInt(0, 30, input.moving);
        patch.lfoWaveform = rng.nextBool() ? 2 : 0;
        patch.lfoPms = input.moving > 0.3f ? uniform(2, 4) : 1;
        patch.lfoAms = input.moving > 0.5f ? uniform(1, 2) : 0;
        for (int op = 0; op < 4; ++op)
            patch.ops[static_cast<size_t>(op)].amsEnable = info.isCarrier(op) && input.moving > 0.5f;
    }
    
    // 5. Ratios: carriers low, modulators harmonic or metallic against their target
    for (int op = 0; op < 4; ++op)
        if (info.isCarrier(op)) patch.ops[static_cast<size_t>(op)].mul = uniformR(rule.carrierMul);
    const float metallic = juce::jmax(input.metallic, rule.metallicMin);
    for (int op = 3; op >= 0; --op) {
        if (info.isCarrier(op)) continue;
        auto& o = patch.ops[static_cast<size_t>(op)];
        if (rng.nextFloat() < metallic) {
            o.mul = kMetallicMultiples[static_cast<size_t>(uniform(0, 5))];
            if (rng.nextBool()) o.dt2 = uniform(1, 3);
        } else if (input.category == GeneratorCategory::EPiano) {
            o.mul = uniform(4, 14);
        } else {
            const int target = info.targetOf(op);
            const int targetMul = target >= 0 ? patch.ops[static_cast<size_t>(target)].mul : 1;
            const float base = targetMul == 0 ? 0.5f : static_cast<float>(targetMul);
            o.mul = juce::jlimit(0, 15, juce::roundToInt(base * static_cast<float>(uniform(1, 3))));
        }
    }
    
    // Always audible: the first carrier keeps a usable attack whatever the ranges say
    for (int op = 0; op < 4; ++op)
        if (info.isCarrier(op)) { auto& o = patch.ops[static_cast<size_t>(op)]; o.ar = juce::jmax(o.ar, 6); o.tl = juce::jmin(o.tl, 40); break; }
    
    // 6. Light detune on operators 1-3
    for (int op = 0; op < 3; ++op)
        patch.ops[static_cast<size_t>(op)].dt1 = MacroMapper::encodeDetune1(kDetuneChoices[static_cast<size_t>(uniform(0, 5))]);
    
    // 7. Effects-only extras
    if (rng.nextFloat() < rule.noiseChance) {
        patch.noiseEnable = true;
        patch.noiseFrequency = uniform(0, 31);
    }
    return patch;
}

} // namespace ymulatorsynth
