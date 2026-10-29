# Offline checks for Magpie: plays notes through the processor and reports levels, and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(MagpieRender PRODUCT_NAME "MagpieRender")

set(magpie_source "${CMAKE_SOURCE_DIR}/plugins/Magpie/Source")

target_sources(MagpieRender PRIVATE
    Main.cpp
    "${magpie_source}/SoundFont.cpp"
    "${magpie_source}/Engine.cpp"
    "${magpie_source}/FontLoader.cpp"
    "${magpie_source}/PluginProcessor.cpp"
    "${magpie_source}/PluginEditor.cpp")

target_include_directories(MagpieRender PRIVATE "${magpie_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(MagpieRender PRIVATE
    JucePlugin_Name="AT Magpie"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(MagpieRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
