# Offline checks for Blue Heeler: gain and harmonics by input level, renders audio through the
# processor and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(BlueHeelerRender PRODUCT_NAME "BlueHeelerRender")

set(blueheeler_source "${CMAKE_SOURCE_DIR}/plugins/BlueHeeler/Source")

target_sources(BlueHeelerRender PRIVATE
    Main.cpp
    "${blueheeler_source}/PluginProcessor.cpp"
    "${blueheeler_source}/PluginEditor.cpp"
    "${blueheeler_source}/PedalLookAndFeel.cpp")

target_include_directories(BlueHeelerRender PRIVATE "${blueheeler_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(BlueHeelerRender PRIVATE
    JucePlugin_Name="AT Blue Heeler"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(BlueHeelerRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
