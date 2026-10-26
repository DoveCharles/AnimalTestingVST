# Bump this per release of this plugin; it is independent of the other plugins.
project(Tapeworm VERSION 0.1.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Tapeworm"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
