cmake_minimum_required( VERSION 3.6 )

project( HoudiniHDKTools )

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )

# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
find_package( Houdini REQUIRED )

# add sub directory
add_subdirectory(src)

# install help and Icons
install(DIRECTORY Icons DESTINATION config)
install(DIRECTORY help DESTINATION .)

configure_file(config/HoudiniHDKTools.json.in HoudiniHDKTools.json)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/HoudiniHDKTools.json" DESTINATION .)