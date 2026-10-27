# Bump this per release of this plugin; it is independent of the other plugins.
project(Chilltron VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Chilltron"
    IS_SYNTH
    SOURCES
        Source/TapeLibrary.cpp
        Source/TapeEngine.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ChilltronLookAndFeel.cpp)

# The tapes are read from disk at run time, not built in. When installing after build, copy them
# to where the plugin looks first.
set(CHILLTRON_TAPE_SOURCE "${CMAKE_SOURCE_DIR}/Redtron_SE/Redtron_SE"
    CACHE PATH "Folder holding the Redtron .sf2 tapes, copied into Application Support on build")

if(APPLE AND AT_COPY_PLUGIN_AFTER_BUILD AND EXISTS "${CHILLTRON_TAPE_SOURCE}")
    file(GLOB chilltron_tapes "${CHILLTRON_TAPE_SOURCE}/*.sf2")
    list(FILTER chilltron_tapes EXCLUDE REGEX "Default_Set\\.sf2$")
    set(chilltron_tape_dir "$ENV{HOME}/Library/Application Support/Animal Testing/Chilltron/Tapes")

    add_custom_target(ChilltronTapes
        COMMAND ${CMAKE_COMMAND} -E make_directory "${chilltron_tape_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${chilltron_tapes} "${chilltron_tape_dir}"
        COMMENT "Installing Chilltron tapes"
        VERBATIM)
    add_dependencies(${PROJECT_NAME} ChilltronTapes)
endif()
