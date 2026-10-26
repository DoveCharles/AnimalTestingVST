# Bump this per release of this plugin; it is independent of the other plugins.
project(Upright VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Upright"
    IS_SYNTH
    SOURCES
        Source/PianoModel.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
