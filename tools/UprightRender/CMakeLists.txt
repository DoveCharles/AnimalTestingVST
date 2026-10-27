# Offline checks for Upright: renders notes and a demo to WAV and measures tuning, inharmonicity,
# decay and CPU. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(UprightRender PRODUCT_NAME "UprightRender")

set(upright_source "${CMAKE_SOURCE_DIR}/plugins/Upright/Source")

target_sources(UprightRender PRIVATE
    Main.cpp
    "${upright_source}/PianoModel.cpp"
    "${upright_source}/HousePiano.cpp"
    "${upright_source}/RhodesPiano.cpp"
    "${upright_source}/PluginProcessor.cpp"
    "${upright_source}/PluginEditor.cpp")

target_include_directories(UprightRender PRIVATE "${upright_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(UprightRender PRIVATE
    JucePlugin_Name="AT Upright"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(UprightRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
