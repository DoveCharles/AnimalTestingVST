# Offline checks for Voice Box: renders audio through the processor, measures pitch accuracy and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(VoiceBoxRender PRODUCT_NAME "VoiceBoxRender")

set(voicebox_source "${CMAKE_SOURCE_DIR}/plugins/VoiceBox/Source")

target_sources(VoiceBoxRender PRIVATE
    Main.cpp
    "${voicebox_source}/PluginProcessor.cpp"
    "${voicebox_source}/PluginEditor.cpp")

target_include_directories(VoiceBoxRender PRIVATE "${voicebox_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(VoiceBoxRender PRIVATE
    JucePlugin_Name="AT Voice Box"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(VoiceBoxRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
