# Measures a third-party tape plugin from the outside: loads the installed VST3, plays test signals
# through it at chosen parameter settings and prints what comes out (gain curve and harmonics,
# frequency response, wow/flutter, noise, stereo). The numbers are the targets for our own model.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(TapeCapture PRODUCT_NAME "TapeCapture")

target_sources(TapeCapture PRIVATE Main.cpp)

target_compile_definitions(TapeCapture PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(TapeCapture PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
