# Bump this per release of this plugin; it is independent of the other plugins.
project(VoiceBox VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Voice Box"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
