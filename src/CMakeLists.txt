# Main plugin target
juce_add_plugin(ChipSynthAU
    # Basic information
    COMPANY_NAME "Hiroaki"
    PLUGIN_MANUFACTURER_CODE Vend
    PLUGIN_CODE ChpS
    PRODUCT_NAME "ChipSynth AU"
    
    # Plugin formats
    FORMATS AU AUv3
    
    # Plugin characteristics
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    
    # AU specific
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    
    # Other settings
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_NAME "ChipSynth AU"
    
    # Bundle identifier
    BUNDLE_ID "jp.hiroaki.chipsynth"
)

# Source files
target_sources(ChipSynthAU
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        core/FMCore.cpp
        core/VoiceManager.cpp
        dsp/YmfmWrapper.cpp
        dsp/EnvelopeGenerator.cpp
        ui/MainComponent.cpp
        ui/OperatorPanel.cpp
        utils/PresetManager.cpp
)

# Include directories
target_include_directories(ChipSynthAU
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src
)

# Link libraries
target_link_libraries(ChipSynthAU
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(ChipSynthAU
    PUBLIC
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Configure JUCE plugin
configure_juce_target(ChipSynthAU)

# Set compiler warnings
set_project_warnings(ChipSynthAU)