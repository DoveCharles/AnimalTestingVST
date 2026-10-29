# Bump this per release of this plugin; it is independent of the other plugins.
project(Notes VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Notes"
    WANTS_KEYBOARD_FOCUS
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
