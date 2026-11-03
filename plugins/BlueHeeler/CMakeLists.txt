# Bump this per release of this plugin; it is independent of the other plugins.
project(BlueHeeler VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Blue Heeler"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PedalLookAndFeel.cpp)
