
find_package(PythonLibs REQUIRED)

include_directories( ${PYTHON_INCLUDE_DIRS} )

add_library(mpt1327modem MODULE module.c channel.c )

target_link_libraries( mpt1327modem
                       mskmodem
                       ${PYTHON_LIBRARIES} 
                       ${GLIB2_LIBRARIES} )

