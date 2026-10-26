# Offline checks for Tapeworm: renders audio through the processor and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(TapewormRender PRODUCT_NAME "TapewormRender")

set(tapeworm_source "${CMAKE_SOURCE_DIR}/plugins/Tapeworm/Source")

target_sources(TapewormRender PRIVATE
    Main.cpp
    "${tapeworm_source}/PluginProcessor.cpp"
    "${tapeworm_source}/PluginEditor.cpp"
    "${tapeworm_source}/DeckLookAndFeel.cpp")

target_include_directories(TapewormRender PRIVATE "${tapeworm_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(TapewormRender PRIVATE
    JucePlugin_Name="AT Tapeworm"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(TapewormRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
