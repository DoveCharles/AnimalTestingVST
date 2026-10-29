# Offline checks for Notes: types, copies and pastes into the editor, then saves and reloads the
# state into a fresh instance and checks nothing was lost. Saves a picture of the editor.
# Built only with -DAT_BUILD_TOOLS=ON.
juce_add_console_app(NotesCheck PRODUCT_NAME "NotesCheck")

set(notes_source "${CMAKE_SOURCE_DIR}/plugins/Notes/Source")

target_sources(NotesCheck PRIVATE
    Main.cpp
    "${notes_source}/PluginProcessor.cpp"
    "${notes_source}/PluginEditor.cpp")

target_include_directories(NotesCheck PRIVATE "${notes_source}")

# The plugin sources expect the macros juce_add_plugin normally defines.
target_compile_definitions(NotesCheck PRIVATE
    JucePlugin_Name="AT Notes"
    JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
    JucePlugin_VersionString="0.1.0"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(NotesCheck PRIVATE
    at::at_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
