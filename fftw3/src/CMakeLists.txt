set(LIB_NAME ossim_fftw3_plugin)

# Include ossim cmake stuff:
include(OssimVersion)
include(OssimCommonVariables)

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC" )

FILE(GLOB_RECURSE OSSIMPLUGIN_SRCS *.cpp)
FILE(GLOB_RECURSE OSSIMPLUGIN_HEADERS *.h)

####
# OSSIMPLUGINSMAKINGDLL controls dll linkage on windows.  
# Adding this sets OSSIM_PLUGINS_DLL #define TO "__declspec(dllexport)".
###
add_definitions("-DOSSIMPLUGINSMAKINGDLL")

# Adjust the install library path:
if(OSSIM_INSTALL_PLUGINS_WITH_VERSION)
   set(INSTALL_LIBRARY_DIR   "${INSTALL_LIBRARY_DIR}/ossim-${OSSIM_VERSION}/plugins")
else(OSSIM_INSTALL_PLUGINS_WITH_VERSION)
   set(INSTALL_LIBRARY_DIR   "${INSTALL_LIBRARY_DIR}/ossim/plugins")
endif(OSSIM_INSTALL_PLUGINS_WITH_VERSION)

if(WIN32)
 set(INSTALL_RUNTIME_DIR "${INSTALL_RUNTIME_DIR}/ossim/plugins")
endif()

OSSIM_LINK_LIBRARY(${LIB_NAME}
                   COMPONENT_NAME ossim TYPE "${OSSIM_PLUGIN_LINK_TYPE}"
		             LIBRARIES ${OSSIM_LIBRARY} ${FFTW3_LIBRARY} 
                   HEADERS "${OSSIMPLUGIN_HEADERS}"
		             SOURCE_FILES "${OSSIMPLUGIN_SRCS}"
                   INSTALL_LIB)


