#*****************************************************************************
# Copyright 2022 NVIDIA Corporation. All rights reserved.
#*****************************************************************************

function(FIND_MATERIALX)

    set(MATERIALX_REPOSITORY "NOT-SPECIFIED" CACHE PATH "Checked out MaterialX git repository.")
    set(MATERIALX_BUILD "NOT-SPECIFIED" CACHE PATH "CMake build folder with MaterialX libraries compiled from the git repository.")
    
    if(NOT MDL_ENABLE_MATERIALX)
        message(WARNING "Support of MaterialX in Examples is disabled. Enable the option 'MDL_ENABLE_MATERIALX' and resolve the required dependencies to re-enable them.")
        return()
    endif()
    
    # check if the dependencies are available, if not disable
    #-----------------------------------------------------------------------------------------------
    foreach(_TO_CHECK ${MATERIALX_REPOSITORY}/source ${MATERIALX_BUILD}/source)
        if(NOT EXISTS ${_TO_CHECK})
            message(FATAL_ERROR "The dependency \"MaterialX\" could not be resolved and the support is disabled now. Please specify 'MATERIALX_REPOSITORY' as well as 'MATERIALX_BUILD' or disable 'MDL_ENABLE_MATERIALX'")
            return()
        endif()
    endforeach()

    # collect information required for the build
    #-----------------------------------------------------------------------------------------------

    # set include dir
    set(_MX_INCLUDE ${MATERIALX_REPOSITORY}/source)
    
    # set libs
    set(_MX_LIBS
        ${MATERIALX_BUILD}/source/MaterialXCore/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}MaterialXCore${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${MATERIALX_BUILD}/source/MaterialXFormat/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}MaterialXFormat${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${MATERIALX_BUILD}/source/MaterialXGenShader/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}MaterialXGenShader${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${MATERIALX_BUILD}/source/MaterialXGenMdl/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}MaterialXGenMdl${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
    
    # store path that are later used in the add_materialx.cmake
    set(MDL_DEPENDENCY_MATERIALX_INCLUDE ${_MX_INCLUDE} CACHE INTERNAL "materialx headers" FORCE)
    set(MDL_DEPENDENCY_MATERIALX_LIBS ${_MX_LIBS} CACHE INTERNAL "materialx libs" FORCE)
  
    if(MDL_LOG_DEPENDENCIES)
        message(STATUS "[INFO] MDL_DEPENDENCY_MATERIALX_INCLUDE:   ${MDL_DEPENDENCY_MATERIALX_INCLUDE}")
        message(STATUS "[INFO] MDL_DEPENDENCY_MATERIALX_LIBS:      ${MDL_DEPENDENCY_MATERIALX_LIBS}")
    endif()

endfunction()
