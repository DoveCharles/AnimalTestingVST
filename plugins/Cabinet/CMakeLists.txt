# Bump this per release of this plugin; it is independent of the other plugins.
project(Cabinet VERSION 1.0.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Cabinet"
    SOURCES
        Source/CabModel.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/CabLookAndFeel.cpp)
