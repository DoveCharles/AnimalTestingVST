# Offline checks for Chilltron: plays notes through the processor and reports levels, and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(ChilltronRender PRODUCT_NAME "ChilltronRender")

set(chilltron_source "${CMAKE_SOURCE_DIR}/plugins/Chilltron/Source")

target_sources(ChilltronRender PRIVATE
    Main.cpp
    "${chilltron_source}/TapeLibrary.cpp"
    "${chilltron_source}/TapeEngine.cpp"
    "${chilltron_source}/PluginProcessor.cpp"
    "${chilltron_source}/PluginEditor.cpp"
    "${chilltron_source}/ChilltronLookAndFeel.cpp")

target_include_directories(ChilltronRender PRIVATE "${chilltron_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(ChilltronRender PRIVATE
    JucePlugin_Name="AT Chilltron"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ChilltronRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
