/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided ?AS IS?, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   event_receiver.h
 @brief



*/


#ifndef _EVENT_TYPES_H_
#define _EVENT_TYPES_H_


#define EVT_MNG_EVT_ARR_SIZE		32
#define EVT_MNG_EVT_DATA_SIZE		4 /*In word size*/
#define EVT_INAVLID_HANDLE		0xFFFFFFFF

typedef struct infra_q_s		evt_mng_subscribe_q_t;
typedef struct infra_q_s		evt_mng_signaler_info_q_t;

typedef struct evt_mng_signaler_param_s	*evt_mng_signaler_param_h;
typedef struct infra_q_s		evt_mng_signaler_q_t;

typedef struct evt_mng_check_event_param_s {
	uint8_t		*evt_info_mem_p;
	/*Set 1, if events are to be retreived.
	Set 0, for event lookup*/
	uint8_t		do_evt_retrieval;
	uint32_t	evt_occurence_mask;
	uint32_t	evt_occurence_info[EVT_MNG_EVT_ARR_SIZE];
	uint32_t	max_evt_space;
	uint32_t	num_evt_occured;
	uint32_t	requested_evt_mask;
	int32_t		timeout;
	void		*cookie;
} evt_mng_check_event_param_t;
#endif
