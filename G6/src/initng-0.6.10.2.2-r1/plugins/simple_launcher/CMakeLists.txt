INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR} )

ADD_LIBRARY(simplelauncher MODULE initng_simple_launcher.c)
TARGET_LINK_LIBRARIES(simplelauncher initng)
INSTALL_TARGETS(${LIB_INSTALL_DIR}/initng simplelauncher)

