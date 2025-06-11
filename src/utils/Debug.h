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

// Additional debug utilities and assertions
#ifdef JUCE_DEBUG
    #define CS_ASSERT(condition) jassert(condition)
    #define CS_DBG_ONLY(code) code
    
    // Parameter validation assertions
    #define CS_ASSERT_CHANNEL(ch) jassert((ch) < 8)
    #define CS_ASSERT_OPERATOR(op) jassert((op) < 4)
    #define CS_ASSERT_PARAMETER_RANGE(val, min, max) jassert((val) >= (min) && (val) <= (max))
    #define CS_ASSERT_BUFFER_SIZE(size) jassert((size) > 0 && (size) <= 2048)
    #define CS_ASSERT_SAMPLE_RATE(rate) jassert((rate) >= 22050 && (rate) <= 192000)
    #define CS_ASSERT_PAN_RANGE(pan) jassert((pan) >= 0.0f && (pan) <= 1.0f)
    #define CS_ASSERT_VELOCITY(vel) jassert((vel) >= 0 && (vel) <= 127)
    #define CS_ASSERT_NOTE(note) jassert((note) >= 0 && (note) <= 127)
#else
    #define CS_ASSERT(condition) ((void)0)
    #define CS_DBG_ONLY(code) ((void)0)
    #define CS_ASSERT_CHANNEL(ch) ((void)0)
    #define CS_ASSERT_OPERATOR(op) ((void)0)
    #define CS_ASSERT_PARAMETER_RANGE(val, min, max) ((void)0)
    #define CS_ASSERT_BUFFER_SIZE(size) ((void)0)
    #define CS_ASSERT_SAMPLE_RATE(rate) ((void)0)
    #define CS_ASSERT_PAN_RANGE(pan) ((void)0)
    #define CS_ASSERT_VELOCITY(vel) ((void)0)
    #define CS_ASSERT_NOTE(note) ((void)0)
#endif