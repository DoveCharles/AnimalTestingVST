# Offline checks for Bird Chorus: renders audio through the processor, measures the wet path and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(BirdChorusRender PRODUCT_NAME "BirdChorusRender")

set(birdchorus_source "${CMAKE_SOURCE_DIR}/plugins/BirdChorus/Source")

target_sources(BirdChorusRender PRIVATE
    Main.cpp
    "${birdchorus_source}/PluginProcessor.cpp"
    "${birdchorus_source}/PluginEditor.cpp"
    "${birdchorus_source}/PedalLookAndFeel.cpp")

target_include_directories(BirdChorusRender PRIVATE "${birdchorus_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(BirdChorusRender PRIVATE
    JucePlugin_Name="AT Bird Chorus"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="1.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(BirdChorusRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
