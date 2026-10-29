# Bump this per release of this plugin; it is independent of the other plugins.
project(Magpie VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Magpie"
    IS_SYNTH
    SOURCES
        Source/SoundFont.cpp
        Source/Engine.cpp
        Source/FontLoader.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
