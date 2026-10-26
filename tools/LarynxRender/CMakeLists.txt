# Offline checks for Larynx: renders audio through the processor, measures pitch accuracy and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(LarynxRender PRODUCT_NAME "LarynxRender")

set(larynx_source "${CMAKE_SOURCE_DIR}/plugins/Larynx/Source")

target_sources(LarynxRender PRIVATE
    Main.cpp
    "${larynx_source}/PluginProcessor.cpp"
    "${larynx_source}/PluginEditor.cpp")

target_include_directories(LarynxRender PRIVATE "${larynx_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(LarynxRender PRIVATE
    JucePlugin_Name="AT Larynx"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(LarynxRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
