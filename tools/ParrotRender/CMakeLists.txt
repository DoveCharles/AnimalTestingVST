# Offline checks for Parrot: prints how lyrics are read, sings a melody to a WAV (with a
# spectrogram), and saves a picture of the editor. Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(ParrotRender PRODUCT_NAME "ParrotRender")

set(parrot_source "${CMAKE_SOURCE_DIR}/plugins/Parrot/Source")

target_sources(ParrotRender PRIVATE
    Main.cpp
    "${parrot_source}/Lyrics.cpp"
    "${parrot_source}/FormantVoice.cpp"
    "${parrot_source}/PluginProcessor.cpp"
    "${parrot_source}/PluginEditor.cpp")

target_include_directories(ParrotRender PRIVATE "${parrot_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(ParrotRender PRIVATE
    JucePlugin_Name="AT Parrot"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=1
    JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ParrotRender PRIVATE
    at::at_shared
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
