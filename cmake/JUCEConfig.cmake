# JUCE Configuration
# This file handles JUCE framework setup and configuration

include(FetchContent)

# Prevent JUCE from trying to install system-wide
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

# Download JUCE if not already present
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(JUCE)

# Global JUCE settings
set_property(GLOBAL PROPERTY JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)

# Function to configure common JUCE target properties
function(configure_juce_target target)
    target_compile_definitions(${target}
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${target} PUBLIC
            JUCE_DEBUG=1
            DEBUG=1
        )
    endif()
endfunction()