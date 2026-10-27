# Offline checks for Mouth Box: feeds synthetic and recorded beatbox through the detector, scores the pads and
# saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(MouthBoxRender PRODUCT_NAME "MouthBoxRender")

set(mouthbox_source "${CMAKE_SOURCE_DIR}/plugins/MouthBox/Source")

target_sources(MouthBoxRender PRIVATE
    Main.cpp
    "${mouthbox_source}/PluginProcessor.cpp"
    "${mouthbox_source}/PluginEditor.cpp")

target_include_directories(MouthBoxRender PRIVATE "${mouthbox_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(MouthBoxRender PRIVATE
    JucePlugin_Name="AT Mouth Box"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=1
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(MouthBoxRender PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
