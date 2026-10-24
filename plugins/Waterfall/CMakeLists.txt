# Bump this per release of this plugin; it is independent of the other plugins.
project(Waterfall VERSION 1.0.0)

at_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME "AT Waterfall"
    SOURCES
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SpectrogramVisual.cpp
        Source/WaterfallLookAndFeel.cpp)
