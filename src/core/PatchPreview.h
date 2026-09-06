#pragma once

#include <vector>
#include "../dsp/YmfmWrapper.h"
#include "../utils/PresetManager.h"

namespace ymulatorsynth {

/**
 * Renders what the current patch sounds like on its own: one note on a
 * private chip, so the display does not depend on what is being played.
 * Message thread only.
 */
class PatchPreview
{
public:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kNote = 60;                 // C4
    
    PatchPreview();
    
    /** Mono render of `numSamples` samples of `preset` playing kNote at full velocity from the key-on. */
    std::vector<float> render(const Preset& preset, int numSamples);
    
    /** Samples per period of kNote at kSampleRate. */
    static double periodInSamples();
    
private:
    YmfmWrapper chip;
    std::vector<float> left, right;
};

} // namespace ymulatorsynth
