# Offline checks for Smooth Operator: renders audio through the processor, measures how much it
# cuts a resonance (and how little it touches everything else) and saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(SmoothOperatorRender PRODUCT_NAME "SmoothOperatorRender")

set(smoothoperator_source "${CMAKE_SOURCE_DIR}/plugins/SmoothOperator/Source")

target_sources(SmoothOperatorRender PRIVATE
    Main.cpp
    "${smoothoperator_source}/PluginProcessor.cpp"
    "${smoothoperator_source}/PluginEditor.cpp")

target_include_directories(SmoothOperatorRender PRIVATE "${smoothoperator_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(SmoothOperatorRender PRIVATE
    JucePlugin_Name="AT Smooth Operator"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(SmoothOperatorRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
