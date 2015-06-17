/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   stm_event.h
 @brief



*/


#ifndef _STM_EVENT_H
#define _STM_EVENT_H

#include "stm_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STM_EVT_DATA_USAGE			0
#define STM_EVT_NO_ERROR			0



typedef struct stm_event_subscription_s *stm_event_subscription_h;

typedef enum stm_event_subscription_op_e {
	STM_EVENT_SUBSCRIPTION_OP_ADD,
	STM_EVENT_SUBSCRIPTION_OP_REMOVE,
	STM_EVENT_SUBSCRIPTION_OP_UPDATE
} stm_event_subscription_op_t;

typedef struct stm_event_s {
	stm_object_h	object;
	unsigned int	event_id;
} stm_event_t;

typedef struct stm_event_info_s {
	stm_event_t	event;
	stm_time_t	timestamp;
	bool		events_missed;
	void		*cookie;
} stm_event_info_t;


typedef struct stm_event_subscription_entry_s {
	stm_object_h	object;
	unsigned int	event_mask;
	void		*cookie;
} stm_event_subscription_entry_t;

typedef void (*stm_event_handler_t)(
	unsigned int				number_of_events,
	stm_event_info_t			*events);

int __must_check stm_event_subscription_create(
					       stm_event_subscription_entry_t *subscription_entries,
					       unsigned int number_of_entries,
					       stm_event_subscription_h *subscription);

int __must_check stm_event_subscription_delete(stm_event_subscription_h subscription);

int __must_check stm_event_subscription_modify(
					       stm_event_subscription_h subscription,
					       stm_event_subscription_entry_t *subscription_entry,
					       stm_event_subscription_op_t operation);

int __must_check stm_event_wait(
				stm_event_subscription_h subscription,
				int		timeout,
				unsigned int	max_number_of_events,
				unsigned int	*number_of_events_p,
				stm_event_info_t *events);

int __must_check stm_event_set_handler(
				       stm_event_subscription_h subscription,
				       stm_event_handler_t event_handler);

int __must_check stm_event_get_handler(
				       stm_event_subscription_h subscription,
				       stm_event_handler_t *event_handler_p);

int __must_check stm_event_set_wait_queue(
					  stm_event_subscription_h subscription,
					  stm_event_wait_queue_t *queue,
					  bool	interruptible);


int __must_check stm_event_get_wait_queue(
					  stm_event_subscription_h subscription,
					  stm_event_wait_queue_t **queue_p,
					  bool	*interruptible_p);

int __must_check stm_event_signal(stm_event_t		*event);




#ifdef __cplusplus
}
#endif

#endif /*_STM_EVENT_H*/
