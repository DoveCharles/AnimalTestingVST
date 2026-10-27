# Bump this per release of this plugin; it is independent of the other plugins.
project(MouthBox VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Mouth Box"
    NEEDS_MIDI_OUTPUT
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
