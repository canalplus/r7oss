INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR} )

ADD_LIBRARY(daemon_clean MODULE initng_daemon_clean.c)
TARGET_LINK_LIBRARIES(daemon_clean initng)
INSTALL_TARGETS(${LIB_INSTALL_DIR}/initng daemon_clean)

