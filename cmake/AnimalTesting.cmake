# Series-wide settings shared by every plugin. Change these in one place only.
set(AT_MANUFACTURER_NAME    "Animal Testing")
set(AT_MANUFACTURER_CODE    "AnTs")   # 4 chars, at least one upper-case letter
set(AT_BUNDLE_ID_PREFIX     "com.animaltesting")
set(AT_COMPANY_WEBSITE      "")

set(AT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AT_FORMATS AU)
endif()

# Registers a plugin code and adds plugins/<name>. Called from the top-level CMakeLists.
function(at_plugin name)
    cmake_parse_arguments(ARG "" "CODE" "" ${ARGN})

    string(LENGTH "${ARG_CODE}" code_length)
    if(NOT code_length EQUAL 4)
        message(FATAL_ERROR "Plugin ${name}: CODE '${ARG_CODE}' must be exactly 4 characters")
    endif()

    string(REGEX MATCHALL "[A-Z]" upper "${ARG_CODE}")
    list(LENGTH upper upper_count)
    if(NOT upper_count EQUAL 1)
        message(FATAL_ERROR "Plugin ${name}: CODE '${ARG_CODE}' must contain exactly one upper-case letter")
    endif()

    if(ARG_CODE STREQUAL AT_MANUFACTURER_CODE)
        message(FATAL_ERROR "Plugin ${name}: CODE '${ARG_CODE}' collides with the manufacturer code")
    endif()

    get_property(used_codes GLOBAL PROPERTY AT_PLUGIN_CODES)
    get_property(used_names GLOBAL PROPERTY AT_PLUGIN_NAMES)
    if(ARG_CODE IN_LIST used_codes)
        message(FATAL_ERROR "Plugin ${name}: CODE '${ARG_CODE}' is already used by another plugin")
    endif()
    if(name IN_LIST used_names)
        message(FATAL_ERROR "Plugin ${name} is registered twice")
    endif()

    set_property(GLOBAL APPEND PROPERTY AT_PLUGIN_CODES "${ARG_CODE}")
    set_property(GLOBAL APPEND PROPERTY AT_PLUGIN_NAMES "${name}")

    set(AT_PLUGIN_CODE "${ARG_CODE}")
    add_subdirectory(plugins/${name})
endfunction()

# Creates the plugin target. Called from plugins/<name>/CMakeLists.txt after project(<name> VERSION x.y.z).
#   at_add_plugin(<target> PRODUCT_NAME "..." SOURCES ... [IS_SYNTH])
function(at_add_plugin target)
    cmake_parse_arguments(ARG "IS_SYNTH" "PRODUCT_NAME" "SOURCES" ${ARGN})

    if(NOT DEFINED AT_PLUGIN_CODE)
        message(FATAL_ERROR "${target}: register it with at_plugin() in the top-level CMakeLists.txt")
    endif()
    if(NOT PROJECT_VERSION)
        message(FATAL_ERROR "${target}: call project(${target} VERSION x.y.z) before at_add_plugin()")
    endif()
    if(NOT ARG_PRODUCT_NAME)
        set(ARG_PRODUCT_NAME "${target}")
    endif()

    if(ARG_IS_SYNTH)
        set(synth_args IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE
            VST3_CATEGORIES Instrument Synth AU_MAIN_TYPE kAudioUnitType_MusicDevice)
    else()
        set(synth_args IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE
            VST3_CATEGORIES Fx AU_MAIN_TYPE kAudioUnitType_Effect)
    endif()

    juce_add_plugin(${target}
        PRODUCT_NAME             "${ARG_PRODUCT_NAME}"
        VERSION                  "${PROJECT_VERSION}"
        COMPANY_NAME             "${AT_MANUFACTURER_NAME}"
        COMPANY_WEBSITE          "${AT_COMPANY_WEBSITE}"
        BUNDLE_ID                "${AT_BUNDLE_ID_PREFIX}.${target}"
        PLUGIN_MANUFACTURER_CODE "${AT_MANUFACTURER_CODE}"
        PLUGIN_CODE              "${AT_PLUGIN_CODE}"
        FORMATS                  ${AT_FORMATS}
        COPY_PLUGIN_AFTER_BUILD  ${AT_COPY_PLUGIN_AFTER_BUILD}
        NEEDS_MIDI_OUTPUT        FALSE
        IS_MIDI_EFFECT           FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        ${synth_args})

    target_sources(${target} PRIVATE ${ARG_SOURCES})

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            at::at_shared
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # `cmake --install <build> --component plugins` collects every plugin into <prefix>/{VST3,AU,Standalone} (used by CI packaging).
    install(DIRECTORY "$<TARGET_GENEX_EVAL:${target}_VST3,$<TARGET_PROPERTY:${target}_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>" DESTINATION VST3 COMPONENT plugins)
    if(TARGET ${target}_AU)
        install(DIRECTORY "$<TARGET_GENEX_EVAL:${target}_AU,$<TARGET_PROPERTY:${target}_AU,JUCE_PLUGIN_ARTEFACT_FILE>>" DESTINATION AU COMPONENT plugins)
    endif()
    if(APPLE)
        install(DIRECTORY "$<TARGET_BUNDLE_DIR:${target}_Standalone>" DESTINATION Standalone COMPONENT plugins)
    else()
        install(PROGRAMS "$<TARGET_FILE:${target}_Standalone>" DESTINATION Standalone COMPONENT plugins)
    endif()
endfunction()
