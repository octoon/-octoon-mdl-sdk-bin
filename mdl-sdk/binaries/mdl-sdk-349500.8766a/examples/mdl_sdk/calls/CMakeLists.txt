#*****************************************************************************
# Copyright 2022 NVIDIA Corporation. All rights reserved.
#*****************************************************************************

# name of the target and the resulting example
set(PROJECT_NAME examples-mdl_sdk-calls)

# collect sources
set(PROJECT_SOURCES
    "example_calls.cpp"
    )

# create target from template
create_from_base_preset(
    TARGET ${PROJECT_NAME}
    TYPE EXECUTABLE
    NAMESPACE mdl_sdk
    OUTPUT_NAME "calls"
    SOURCES ${PROJECT_SOURCES}
    EXAMPLE
)

# add dependencies
target_add_dependencies(TARGET ${PROJECT_NAME}
    DEPENDS
        mdl::mdl_sdk
        mdl_sdk::shared
    )
    
# creates a user settings file to setup the debugger (visual studio only, otherwise this is a no-op)
target_create_vs_user_settings(TARGET ${PROJECT_NAME})

# -------------------------------------------------------------------------------------------------
# Create installation rules to copy the build directory
# -------------------------------------------------------------------------------------------------
add_target_install(
    TARGET ${PROJECT_NAME}
    DESTINATION "examples/mdl_sdk/calls"
    )

# -------------------------------------------------------------------------------------------------
# Add tests if available
# -------------------------------------------------------------------------------------------------
add_tests()
