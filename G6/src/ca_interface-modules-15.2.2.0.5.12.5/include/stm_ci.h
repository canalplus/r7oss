#ifndef _STM_CI_H
#define _STM_CI_H

#include "linux/types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct stm_ci_s *stm_ci_h;

typedef enum{
	STM_CI_CAM_NONE,
	STM_CI_CAM_DVBCI,
	STM_CI_CAM_DVBCI_PLUS
}stm_ci_cam_type_t;

typedef enum{
	STM_CI_RESET_SOFT,
	STM_CI_RESET_LINK
}stm_ci_reset_type_t;

typedef enum{
	STM_CI_STATUS_IIR,
	STM_CI_STATUS_FREE,
	STM_CI_STATUS_DATA_AVAILABLE
}stm_ci_status_t;

typedef enum{
	STM_CI_COMMAND_DA_INT_ENABLE
}stm_ci_command_t;

typedef enum{
	STM_CI_EVENT_CAM_INSERTED = 0x1,
	STM_CI_EVENT_CAM_REMOVED  = 0x2
}stm_ci_event_type_t;

typedef struct{
	unsigned char	 *msg_p;
	unsigned int	  length;
	unsigned int      response_length;
}stm_ci_msg_params_t;

typedef struct{
	unsigned int		operator_profile_resource:1;
	unsigned int		reserved:31;
}stm_ci_cam_features_t;

typedef struct{
	stm_ci_cam_type_t		cam_type;
	unsigned int			cis_size;
	unsigned int			negotiated_buf_size;
	stm_ci_cam_features_t	features;
	bool					interrupt_supported;
}stm_ci_capabilities_t;


int stm_ci_new(
		unsigned int	ci_dev_id,
		stm_ci_h	       *ci_p);

int stm_ci_delete(
		stm_ci_h  ci);

int stm_ci_write(
		stm_ci_h   ci,
		stm_ci_msg_params_t	*params_p);

int stm_ci_read(
		stm_ci_h    ci,
		stm_ci_msg_params_t	*params_p);

int stm_ci_read_cis(
		stm_ci_h        ci,
		unsigned int    buf_size,
		unsigned int   *actual_cis_size,
		unsigned char  *cis_buf_p );

int stm_ci_reset(
		stm_ci_h  ci,
		stm_ci_reset_type_t type);

int stm_ci_get_capabilities(
		stm_ci_h  	ci,
		stm_ci_capabilities_t *cap_p);

int stm_ci_get_slot_status(
		stm_ci_h  ci,
		bool *is_present);

int stm_ci_get_status(
		stm_ci_h	ci,
		stm_ci_status_t	status,
		uint8_t		*value_p);

int stm_ci_set_command(
		stm_ci_h  	 ci,
		stm_ci_command_t command,
		uint8_t		 value);

#ifdef __cplusplus
}
#endif

#endif /* _STM_CI_H*/
