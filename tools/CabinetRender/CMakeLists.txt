# Offline tool for Cabinet: renders audio through the processor, writes the IRs it builds, and
# compares a built-in cab's response with a reference IR file. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(CabinetRender PRODUCT_NAME "CabinetRender")

set(cabinet_source "${CMAKE_SOURCE_DIR}/plugins/Cabinet/Source")

target_sources(CabinetRender PRIVATE
    Main.cpp
    "${cabinet_source}/CabModel.cpp"
    "${cabinet_source}/PluginProcessor.cpp"
    "${cabinet_source}/PluginEditor.cpp"
    "${cabinet_source}/CabLookAndFeel.cpp")

target_include_directories(CabinetRender PRIVATE "${cabinet_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(CabinetRender PRIVATE
    JucePlugin_Name="AT Cabinet"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="1.0.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(CabinetRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
