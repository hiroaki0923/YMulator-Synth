#pragma once

// Mock BinaryData for CI testing where JUCE binary resources are not available
namespace BinaryData {
    // Empty OPM preset collection for testing
    extern const char* ymulatorsynthpresetcollection_opm;
    extern const int ymulatorsynthpresetcollection_opmSize;
}