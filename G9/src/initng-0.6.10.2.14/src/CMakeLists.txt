INCLUDE_DIRECTORIES( ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

SET(initng_lib_srcs
	initng_active_db.c
	initng_active_state.c
	initng_common.c
	initng_control_command.c
	initng_depend.c
	initng_error.c
	initng_execute.c
	initng_env_variable.c
	initng_fd.c
	initng_fork.c
	initng_handler.c
	initng_interrupt.c
	initng_kill_handler.c
	initng_load_module.c
	initng_main.c
	initng_open_read_close.c
	initng_plugin_callers.c
	initng_process_db.c
	initng_service_data_types.c
	initng_static_event_types.c
	initng_service_types.c
	initng_signal.c
	initng_static_data_id.c
	initng_static_states.c
	initng_static_service_types.c
	initng_string_tools.c
	initng_struct_data.c
	initng_toolbox.c
	initng_event_types.c
	initng_event_hook.c
	initng_event.c
)

IF(SERVICE_CACHE)
	SET(initng_lib_srcs ${initng_lib_srcs}
		initng_service_cache.c
	)
ENDIF(SERVICE_CACHE)

SET(initng_bin_srcs
	main.c
	initng_global.c
)

ADD_LIBRARY(initng SHARED ${initng_lib_srcs})
TARGET_LINK_LIBRARIES(initng dl rt ${COREDUMPER_LIBS} ${SELINUX_LIBS})
SET_TARGET_PROPERTIES(initng PROPERTIES VERSION 0.0.0 SOVERSION 0)
INSTALL_TARGETS(${LIB_INSTALL_DIR} initng)

ADD_EXECUTABLE(initng_bin ${initng_bin_srcs})
SET_TARGET_PROPERTIES(initng_bin PROPERTIES COMPILE_FLAGS "-fPIE -fstack-protector-all")
SET_TARGET_PROPERTIES(initng_bin PROPERTIES LINK_FLAGS "-pie")
TARGET_LINK_LIBRARIES(initng_bin initng)
SET_TARGET_PROPERTIES(initng_bin PROPERTIES OUTPUT_NAME initng)
INSTALL_TARGETS(${SBIN_INSTALL_DIR} initng_bin)

########### install files ###############

SET(initng_headers
	initng_global.h
	initng.h
	initng_module.h
	initng_plugin.h
	initng_system_states.h
	initng_list.h
	initng_active_db.h
	initng_active_state.h
	initng_common.h
	initng_control_command.h
	initng_depend.h
	initng_error.h
	initng_execute.h
	initng_env_variable.h
	initng_fd.h
	initng_fork.h
	initng_handler.h
	initng_interrupt.h
	initng_is.h
	initng_kill_handler.h
	initng_load_module.h
	initng_main.h
	initng_open_read_close.h
	initng_plugin_callers.h
	initng_process_db.h
	initng_service_data_types.h
	initng_static_event_types.h
	initng_service_types.h
	initng_signal.h
	initng_static_data_id.h
	initng_static_states.h
	initng_static_service_types.h
	initng_string_tools.h
	initng_struct_data.h
	initng_toolbox.h
	initng_msg.h
	initng_event_types.h
	initng_event_hook.h
	initng_event.h
)

IF(SERVICE_CACHE)
	SET(initng_headers ${initng_headers}
		initng_service_cache.h
	)
ENDIF(SERVICE_CACHE)

INSTALL_FILES(${INCLUDE_INSTALL_DIR}/initng FILES ${initng_headers})
