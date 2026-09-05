#pragma once
#include <array>
#include <cstdint>

namespace ymulatorsynth {

/**
 * YM2151 algorithm table: which operators are carriers and which operator each
 * modulator feeds. Operators are 0-based in voice order (0 = M1, 1 = C1,
 * 2 = M2, 3 = C2), the same order ymfm uses for O1..O4. Derived from ymfm's
 * s_algorithm_ops; AlgorithmInfoTest checks it against those bit patterns.
 */
struct AlgorithmInfo
{
    struct Edge { uint8_t from; uint8_t to; };
    
    uint8_t carrierMask;            // bit n set: operator n reaches the output
    uint8_t edgeCount;
    std::array<Edge, 3> edges;      // modulator -> target, edgeCount valid entries
    const char* structure;          // compact notation, e.g. "(1+2)>3>4"
    const char* description;        // one line for the UI
    
    constexpr bool isCarrier(int op) const { return ((carrierMask >> op) & 1) != 0; }
    constexpr bool isModulator(int op) const { return !isCarrier(op); }
    constexpr uint8_t modulatorMask() const { return static_cast<uint8_t>(~carrierMask & 0x0F); }
    
    /** Operator this modulator feeds, or -1 for carriers. */
    constexpr int targetOf(int op) const
    {
        for (int i = 0; i < edgeCount; ++i)
            if (edges[static_cast<size_t>(i)].from == op) return edges[static_cast<size_t>(i)].to;
        return -1;
    }
    
    /** True when operator `from` modulates operator `to`. */
    constexpr bool modulates(int from, int to) const
    {
        for (int i = 0; i < edgeCount; ++i)
            if (edges[static_cast<size_t>(i)].from == from && edges[static_cast<size_t>(i)].to == to) return true;
        return false;
    }
};

inline constexpr std::array<AlgorithmInfo, 8> kAlgorithms = {{
    { 0x08, 3, {{ {0, 1}, {1, 2}, {2, 3} }}, "1>2>3>4",     "Serial 4-op chain. Brightest and sharpest: bass, lead, metallic" },
    { 0x08, 3, {{ {0, 2}, {1, 2}, {2, 3} }}, "(1+2)>3>4",   "Two modulators into a chain: brass, thick leads" },
    { 0x08, 3, {{ {0, 3}, {1, 2}, {2, 3} }}, "(1+(2>3))>4", "Core plus ornament into one carrier: clav, guitar" },
    { 0x08, 3, {{ {0, 1}, {1, 3}, {2, 3} }}, "((1>2)+3)>4", "2-op chain plus a modulator into one carrier: e.piano, harpsichord" },
    { 0x0A, 2, {{ {0, 1}, {2, 3}, {0, 0} }}, "(1>2)+(3>4)", "Two independent 2-op pairs. The all-rounder: e.piano, bell, guitar, brass" },
    { 0x0E, 3, {{ {0, 1}, {0, 2}, {0, 3} }}, "1>(2+3+4)",   "One modulator into three carriers: organ, strings, pad" },
    { 0x0E, 1, {{ {0, 1}, {0, 0}, {0, 0} }}, "(1>2)+3+4",   "One 2-op pair plus two sines: bass with sub, organ" },
    { 0x0F, 0, {{ {0, 0}, {0, 0}, {0, 0} }}, "1+2+3+4",     "Four carriers, additive: organ, flute, pure tones" },
}};

inline constexpr const AlgorithmInfo& algorithmInfo(int algorithm)
{
    return kAlgorithms[static_cast<size_t>(algorithm < 0 ? 0 : (algorithm > 7 ? 7 : algorithm))];
}

} // namespace ymulatorsynth
