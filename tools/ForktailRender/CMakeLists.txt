# Offline checks for Forktail: renders audio through the processor, measures the pitch of every
# shift, chords, level ripple, delay and CPU, and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(ForktailRender PRODUCT_NAME "ForktailRender")

set(forktail_source "${CMAKE_SOURCE_DIR}/plugins/Forktail/Source")

target_sources(ForktailRender PRIVATE
    Main.cpp
    "${forktail_source}/PluginProcessor.cpp"
    "${forktail_source}/PluginEditor.cpp"
    "${forktail_source}/PedalLookAndFeel.cpp")

target_include_directories(ForktailRender PRIVATE "${forktail_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(ForktailRender PRIVATE
    JucePlugin_Name="AT Forktail"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ForktailRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
