# Creates plugins/<NAME> from templates/PluginTemplate and registers it in the top-level CMakeLists.txt.
#
#   cmake -DNAME=Delay -DCODE=Dlay -P scripts/NewPlugin.cmake
#
# NAME: CMake target / folder name (letters and digits, starts with a letter).
# CODE: unique 4-char plugin code with exactly one upper-case letter (validated again at configure time).

cmake_minimum_required(VERSION 3.22)

get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT NAME MATCHES "^[A-Za-z][A-Za-z0-9]*$")
    message(FATAL_ERROR "Pass -DNAME=<PluginName> (letters/digits, starting with a letter)")
endif()
if(NOT CODE MATCHES "^[A-Za-z0-9][A-Za-z0-9][A-Za-z0-9][A-Za-z0-9]$")
    message(FATAL_ERROR "Pass -DCODE=<4 chars>, e.g. -DCODE=Dlay")
endif()

set(dest "${root}/plugins/${NAME}")
if(EXISTS "${dest}")
    message(FATAL_ERROR "${dest} already exists")
endif()

file(READ "${root}/CMakeLists.txt" top)
if(top MATCHES "CODE ${CODE}[ \t\r\n)]")
    message(FATAL_ERROR "Plugin code ${CODE} is already registered in CMakeLists.txt")
endif()

file(COPY "${root}/templates/PluginTemplate/" DESTINATION "${dest}")

file(GLOB_RECURSE files "${dest}/*")
foreach(f IN LISTS files)
    file(READ "${f}" content)
    string(REPLACE "PluginTemplate" "${NAME}" content "${content}")
    file(WRITE "${f}" "${content}")
endforeach()

file(APPEND "${root}/CMakeLists.txt" "at_plugin(${NAME} CODE ${CODE})\n")

message(STATUS "Created plugins/${NAME} (code ${CODE}) and registered it in CMakeLists.txt")
