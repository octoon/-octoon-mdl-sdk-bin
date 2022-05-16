#*****************************************************************************
# Copyright 2022 NVIDIA Corporation. All rights reserved.
#*****************************************************************************

# -------------------------------------------------------------------------------------------------
# script expects the following variables:
    # - TARGET_ADD_TOOL_DEPENDENCY_TARGET
    # - TARGET_ADD_TOOL_DEPENDENCY_TOOL
# -------------------------------------------------------------------------------------------------

# use a default fallback
find_program(clang_PATH clang)
if(NOT clang_PATH)
    MESSAGE(FATAL_ERROR "The tool dependency \"${TARGET_ADD_TOOL_DEPENDENCY_TOOL}\" for target \"${TARGET_ADD_TOOL_DEPENDENCY_TARGET}\" could not be resolved.")
endif()

# call --version
get_filename_component(clang_PATH_ABS ${clang_PATH} REALPATH)
set(clang_PATH ${clang_PATH_ABS} CACHE FILEPATH "Path of the Clang 7.0 binary." FORCE)
execute_process(COMMAND "${clang_PATH}" "--version" 
    OUTPUT_VARIABLE 
        _CLANG_VERSION_STRING 
    ERROR_VARIABLE 
        _CLANG_VERSION_STRING
    )

# check version
if(NOT _CLANG_VERSION_STRING)
    message(STATUS "clang_PATH: ${clang_PATH}")
    message(FATAL_ERROR "Clang version could not be determined.")
else()
    # parse version number
    STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _CLANG_VERSION_STRING ${_CLANG_VERSION_STRING})
    if(${_CLANG_VERSION_STRING} VERSION_GREATER_EQUAL "7.1.0" OR ${_CLANG_VERSION_STRING} VERSION_LESS "7.0.0")
        message(FATAL_ERROR "Clang 7.0 is required but Clang ${_CLANG_VERSION_STRING} was found instead. Please set the CMake option 'clang_PATH' that needs to point to a clang 7.0.x compiler.")
    endif()
endif()