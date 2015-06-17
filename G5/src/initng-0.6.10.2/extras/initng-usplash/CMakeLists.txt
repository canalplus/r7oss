INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/plugins/nge ${CMAKE_BINARY_DIR})

ADD_EXECUTABLE(initng-usplash initng-usplash.c)
TARGET_LINK_LIBRARIES(initng-usplash ngeclient)
INSTALL_TARGETS(${SBIN_INSTALL_DIR} initng-usplash)

ADD_EXECUTABLE(initng-usplash-static initng-usplash.c)
TARGET_LINK_LIBRARIES(initng-usplash-static ngeclient_static)
SET_TARGET_PROPERTIES(initng-usplash-static PROPERTIES LINK_FLAGS -static)

ADD_EXECUTABLE(initng-usplash-shutdown initng-usplash-shutdown.c)
TARGET_LINK_LIBRARIES(initng-usplash-shutdown ngeclient)
INSTALL_TARGETS(${SBIN_INSTALL_DIR} initng-usplash-shutdown)

INSTALL_FILES(${DATA_INSTALL_DIR}/doc/initng FILES README.usplash)
INSTALL_FILES(${SYSCONF_INSTALL_DIR}/initng/system FILES usplash.i)
