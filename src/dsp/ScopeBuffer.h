#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace ymulatorsynth {

/**
 * Single-writer (audio thread) / single-reader (message thread) ring of the
 * most recent output samples for the waveform display. Writes never block;
 * a read may straddle a write, which only costs a stale frame.
 */
class ScopeBuffer
{
public:
    static constexpr size_t kSize = 8192;   // power of two, about 170 ms at 48 kHz
    
    void push(const float* left, const float* right, int numSamples) noexcept
    {
        size_t w = writeIndex.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i) {
            samples[w & (kSize - 1)] = 0.5f * (left[i] + right[i]);
            ++w;
        }
        writeIndex.store(w, std::memory_order_release);
    }
    
    /** Copies the newest `count` samples (oldest first) into `out`; returns how many were valid. */
    size_t readLatest(float* out, size_t count) const noexcept
    {
        const size_t w = writeIndex.load(std::memory_order_acquire);
        if (count > kSize) count = kSize;
        const size_t available = w < count ? w : count;
        const size_t start = w - available;
        for (size_t i = 0; i < available; ++i) out[i] = samples[(start + i) & (kSize - 1)];
        for (size_t i = available; i < count; ++i) out[i] = 0.0f;
        return available;
    }
    
    size_t totalWritten() const noexcept { return writeIndex.load(std::memory_order_acquire); }
    
private:
    std::array<float, kSize> samples {};
    std::atomic<size_t> writeIndex { 0 };
};

} // namespace ymulatorsynth
