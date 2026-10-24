# Offline renderer for the Waterfall editor: plays an audio file through the processor and saves PNG
# snapshots. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(WaterfallPreview PRODUCT_NAME "WaterfallPreview")

set(waterfall_source "${CMAKE_SOURCE_DIR}/plugins/Waterfall/Source")

target_sources(WaterfallPreview PRIVATE
    Main.cpp
    "${waterfall_source}/PluginProcessor.cpp"
    "${waterfall_source}/PluginEditor.cpp"
    "${waterfall_source}/SpectrogramVisual.cpp"
    "${waterfall_source}/WaterfallLookAndFeel.cpp")

target_include_directories(WaterfallPreview PRIVATE "${waterfall_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(WaterfallPreview PRIVATE
    JucePlugin_Name="AT Waterfall"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(WaterfallPreview PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
