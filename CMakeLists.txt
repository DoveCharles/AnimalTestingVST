cmake_minimum_required(VERSION 3.22)

# Must be set before project(). CI overrides CMAKE_OSX_ARCHITECTURES for universal builds.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AnimalTesting VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AT_COPY_PLUGIN_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)
option(AT_BUILD_TOOLS "Build developer tools such as WaterfallPreview" OFF)

add_subdirectory(libs/JUCE)

include(cmake/AnimalTesting.cmake)

# Shared JUCE module: LookAndFeel, DSP, parameter/preset helpers.
juce_add_module(shared/at_shared ALIAS_NAMESPACE at)

# ------------------------------------------------------------------------------
# Plugin registry: the single source of truth for plugin codes.
# Each line registers a unique 4-char code and builds plugins/<Name>.
# Codes need exactly one upper-case letter (AU rule); duplicates fail at configure time.
# ------------------------------------------------------------------------------
at_plugin(Saturator CODE Satr)
at_plugin(Waterfall CODE Wtfl)
at_plugin(GreenRooskie CODE Grsk)
at_plugin(Shifty CODE Shft)
at_plugin(SkyChorus CODE Skyc)
at_plugin(Cabinet CODE Cabn)
at_plugin(SevenRooms CODE Svnr)
at_plugin(Upright CODE Uprt)
at_plugin(Chilltron CODE Chtn)
at_plugin(VoiceBox CODE Vbox)
at_plugin(SmoothOperator CODE Smop)
at_plugin(Tapeworm CODE Tpwm)
at_plugin(GoodBoy CODE Gdby)
at_plugin(BigMouth CODE Bmth)
at_plugin(Forktail CODE Fktl)

if(AT_BUILD_TOOLS)
    add_subdirectory(tools/WaterfallPreview)
    add_subdirectory(tools/CabinetRender)
    add_subdirectory(tools/UprightRender)
    add_subdirectory(tools/SkyChorusRender)
    add_subdirectory(tools/ChilltronRender)
    add_subdirectory(tools/VoiceBoxRender)
    add_subdirectory(tools/SmoothOperatorRender)
    add_subdirectory(tools/TapeCapture)
    add_subdirectory(tools/TapewormRender)
    add_subdirectory(tools/GoodBoyRender)
    add_subdirectory(tools/BigMouthRender)
    add_subdirectory(tools/ForktailRender)
endif()
