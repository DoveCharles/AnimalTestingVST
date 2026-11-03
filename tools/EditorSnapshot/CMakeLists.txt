# Saves a picture of the editor for plugins that have no render tool of their own: one console app
# per plugin, <Name>Snapshot, built from that plugin's sources. Used for the website's screenshots.
# Built only with -DAT_BUILD_TOOLS=ON.
function(at_editor_snapshot name product version)
    cmake_parse_arguments(ARG "IS_SYNTH" "" "SOURCES" ${ARGN})
    set(target ${name}Snapshot)
    set(source_dir "${CMAKE_SOURCE_DIR}/plugins/${name}/Source")

    juce_add_console_app(${target} PRODUCT_NAME "${target}")

    list(TRANSFORM ARG_SOURCES PREPEND "${source_dir}/")
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp" ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE "${source_dir}")

    if(ARG_IS_SYNTH)
        set(is_synth 1)
    else()
        set(is_synth 0)
    endif()

    # The plugin sources expect the macros juce_add_plugin normally defines.
    target_compile_definitions(${target} PRIVATE
        JucePlugin_Name="${product}"
        JucePlugin_Manufacturer="${AT_MANUFACTURER_NAME}"
        JucePlugin_VersionString="${version}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_IsSynth=${is_synth}
        JucePlugin_WantsMidiInput=${is_synth}
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(${target} PRIVATE
        at::at_shared
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endfunction()

at_editor_snapshot(Saturator "AT Saturator" 1.0.0
    SOURCES PluginProcessor.cpp PluginEditor.cpp)

at_editor_snapshot(WoollyBear "AT Woolly Bear" 1.0.0
    SOURCES PluginProcessor.cpp PluginEditor.cpp PedalLookAndFeel.cpp)

at_editor_snapshot(Upright "AT Upright" 0.1.0 IS_SYNTH
    SOURCES PianoModel.cpp HousePiano.cpp RhodesPiano.cpp Vibraphone.cpp PluginProcessor.cpp PluginEditor.cpp)
