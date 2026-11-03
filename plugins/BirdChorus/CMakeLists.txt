# Bump this per release of this plugin; it is independent of the other plugins.
project(BirdChorus VERSION 1.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Bird Chorus"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PedalLookAndFeel.cpp)
