#pragma once

// Conditional debug output macros for ChipSynth-AU
// These macros compile to actual logging only in debug builds

#ifdef JUCE_DEBUG
    #define CS_DBG(msg) DBG(msg)
    #define CS_LOG(msg) std::cout << msg << std::endl
    #define CS_LOGF(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#else
    #define CS_DBG(msg) ((void)0)
    #define CS_LOG(msg) ((void)0)
    #define CS_LOGF(fmt, ...) ((void)0)
#endif

// Additional debug utilities
#ifdef JUCE_DEBUG
    #define CS_ASSERT(condition) jassert(condition)
    #define CS_DBG_ONLY(code) code
#else
    #define CS_ASSERT(condition) ((void)0)
    #define CS_DBG_ONLY(code) ((void)0)
#endif