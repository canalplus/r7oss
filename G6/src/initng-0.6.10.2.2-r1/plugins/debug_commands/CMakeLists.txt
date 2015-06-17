INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR} )

ADD_LIBRARY(debug_commands MODULE initng_debug_commands.c print_service.c)
TARGET_LINK_LIBRARIES(debug_commands initng)
INSTALL_TARGETS(${LIB_INSTALL_DIR}/initng debug_commands)

