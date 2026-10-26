# Offline checks for Sky Chorus: renders audio through the processor, measures the wet path and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(SkyChorusRender PRODUCT_NAME "SkyChorusRender")

set(skychorus_source "${CMAKE_SOURCE_DIR}/plugins/SkyChorus/Source")

target_sources(SkyChorusRender PRIVATE
    Main.cpp
    "${skychorus_source}/PluginProcessor.cpp"
    "${skychorus_source}/PluginEditor.cpp"
    "${skychorus_source}/PedalLookAndFeel.cpp")

target_include_directories(SkyChorusRender PRIVATE "${skychorus_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(SkyChorusRender PRIVATE
    JucePlugin_Name="AT Sky Chorus"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="1.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(SkyChorusRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
