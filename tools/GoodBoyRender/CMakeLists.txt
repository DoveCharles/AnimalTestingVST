# Offline checks for Good Boy: renders audio through the processor, measures loudness and
# peaks per mode and Amount, and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(GoodBoyRender PRODUCT_NAME "GoodBoyRender")

set(goodboy_source "${CMAKE_SOURCE_DIR}/plugins/GoodBoy/Source")

target_sources(GoodBoyRender PRIVATE
    Main.cpp
    "${goodboy_source}/PluginProcessor.cpp"
    "${goodboy_source}/PluginEditor.cpp")

target_include_directories(GoodBoyRender PRIVATE "${goodboy_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(GoodBoyRender PRIVATE
    JucePlugin_Name="AT Good Boy"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(GoodBoyRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
