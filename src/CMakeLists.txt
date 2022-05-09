# add plugin linked to houdini lib
function(add_plugin PLUGIN_NAME)
    # Add a library and its source files.
    add_library( ${PLUGIN_NAME} SHARED
                 ${ARGN}
    )

    # Link against the Houdini libraries, and add required include directories and
    # compile definitions.
    target_link_libraries( ${PLUGIN_NAME} Houdini )

    # Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
    target_include_directories( ${PLUGIN_NAME} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()


# add sub directory
add_subdirectory(SOP)