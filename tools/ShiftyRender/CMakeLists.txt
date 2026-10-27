# Offline check for Shifty: saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(ShiftyRender PRODUCT_NAME "ShiftyRender")

set(shifty_source "${CMAKE_SOURCE_DIR}/plugins/Shifty/Source")

target_sources(ShiftyRender PRIVATE
    Main.cpp
    "${shifty_source}/PluginProcessor.cpp"
    "${shifty_source}/PluginEditor.cpp"
    "${shifty_source}/PedalLookAndFeel.cpp")

target_include_directories(ShiftyRender PRIVATE "${shifty_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(ShiftyRender PRIVATE
    JucePlugin_Name="AT Shifty"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ShiftyRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
