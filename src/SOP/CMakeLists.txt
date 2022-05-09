set(PLUGINS
    SOP_Flatten
    SOP_PolyClip
    SOP_Star
    )


# Code generation for the embedded DS file in SOP_Star.C.
houdini_generate_proto_headers( FILES SOP_Star.C )
houdini_generate_proto_headers( FILES SOP_PolyClip.C )

# add plugins    
foreach(PLUGIN_NAME ${PLUGINS})
    add_plugin(${PLUGIN_NAME} ${PLUGIN_NAME}.c ${PLUGIN_NAME}.h)
endforeach()

# Sets several common target properties, such as the library's output directory.
#houdini_configure_target( SOP_Flatten  )

# install files
install(TARGETS 
${PLUGINS} 
DESTINATION dso)