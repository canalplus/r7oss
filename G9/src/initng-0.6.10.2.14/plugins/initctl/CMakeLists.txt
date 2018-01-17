INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR} )

ADD_LIBRARY(initctl MODULE initng_initctl.c)
TARGET_LINK_LIBRARIES(initctl initng)
INSTALL_TARGETS(${LIB_INSTALL_DIR}/initng initctl)

install (FILES initreq.h DESTINATION /usr/include/initng)
