# Offline checks for Pitch-Pot: renders audio through the processor, measures the pitch of every
# shift, chords, level ripple, delay and CPU, and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(PitchPotRender PRODUCT_NAME "PitchPotRender")

set(pitchpot_source "${CMAKE_SOURCE_DIR}/plugins/PitchPot/Source")

target_sources(PitchPotRender PRIVATE
    Main.cpp
    "${pitchpot_source}/PluginProcessor.cpp"
    "${pitchpot_source}/PluginEditor.cpp"
    "${pitchpot_source}/CourtPainting.cpp"
    "${pitchpot_source}/PedalLookAndFeel.cpp")

target_include_directories(PitchPotRender PRIVATE "${pitchpot_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(PitchPotRender PRIVATE
    JucePlugin_Name="AT Pitch-Pot"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(PitchPotRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
