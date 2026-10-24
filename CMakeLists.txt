cmake_minimum_required(VERSION 3.22)

# Must be set before project(). CI overrides CMAKE_OSX_ARCHITECTURES for universal builds.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AnimalTesting VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AT_COPY_PLUGIN_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)

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
