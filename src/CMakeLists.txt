# Main plugin target
juce_add_plugin(YMulator-Synth
    # Basic information
    COMPANY_NAME "Hiroaki"
    PLUGIN_MANUFACTURER_CODE Hrki
    PLUGIN_CODE YMul
    PRODUCT_NAME "YMulator-Synth"
    PLUGIN_DESCRIPTION "FM Synthesis Audio Unit"
    
    # Plugin formats
    FORMATS AU AUv3 VST3 Standalone
    
    # Plugin characteristics
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    
    # AU specific
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AU_SANDBOX_SAFE TRUE
    
    # Categories and tags for GarageBand library
    PLUGIN_AU_EXPORT_PREFIX YMulatorSynth
    
    # Other settings
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_NAME "YMulator-Synth"
    
    # Install only AU to user directory
    AU_COPY_PLUGIN_AFTER_BUILD TRUE
    
    # Bundle identifier
    BUNDLE_ID "jp.hiroaki.ymulatorsynth"
)

# Source files
target_sources(YMulator-Synth
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        core/VoiceManager.cpp
        core/MidiProcessor.cpp
        dsp/YmfmWrapper.cpp
        dsp/EnvelopeGenerator.cpp
        ui/MainComponent.cpp
        ui/OperatorPanel.cpp
        ui/RotaryKnob.cpp
        ui/EnvelopeDisplay.cpp
        ui/AlgorithmDisplay.cpp
        utils/PresetManager.cpp
        utils/VOPMParser.cpp
        # ymfm library sources
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opn.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_ssg.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_adpcm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_misc.cpp
)

# Include directories
target_include_directories(YMulator-Synth
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src
)

# Link libraries
target_link_libraries(YMulator-Synth
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
target_compile_definitions(YMulator-Synth
    PUBLIC
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Add resources
juce_add_binary_data(YMulator-Synth_Resources
    SOURCES
        ${CMAKE_SOURCE_DIR}/resources/presets/ymulator-synth-preset-collection.opm
)

# Link resources to the plugin
target_link_libraries(YMulator-Synth
    PRIVATE
        YMulator-Synth_Resources
)

# Configure JUCE plugin
configure_juce_target(YMulator-Synth)

# Set compiler warnings
set_project_warnings(YMulator-Synth)

