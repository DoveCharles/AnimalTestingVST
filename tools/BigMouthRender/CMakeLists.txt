# Offline checks for Big Mouth: feeds synthetic and recorded beatbox through the detector, scores the pads and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(BigMouthRender PRODUCT_NAME "BigMouthRender")

set(bigmouth_source "${CMAKE_SOURCE_DIR}/plugins/BigMouth/Source")

target_sources(BigMouthRender PRIVATE
    Main.cpp
    "${bigmouth_source}/PluginProcessor.cpp"
    "${bigmouth_source}/PluginEditor.cpp")

target_include_directories(BigMouthRender PRIVATE "${bigmouth_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(BigMouthRender PRIVATE
    JucePlugin_Name="AT Big Mouth"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=1
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(BigMouthRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
