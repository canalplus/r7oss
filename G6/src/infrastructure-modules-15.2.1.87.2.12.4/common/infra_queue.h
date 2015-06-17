/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
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
 @File   infra_os_wrapper.h
 @brief



*/


#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_

#include "infra_os_wrapper.h"

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct infra_q_s{
	void			*data_p; /*Pointer to the actual node whose linked list is constructed*/
	uint32_t			key; /*key to locate any node. helps in searching and deltion*/
	struct infra_q_s		*next_p; /*Pointer to the next Q element*/
}infra_q_t;

typedef struct{
	infra_os_semaphore_t sema;
	infra_q_t *head_p;
}infra_os_message_q_t;


#define INFRA_Q_GET_DATA(q_p, type)		(type*)(q_p->data_p)
#define INFRA_Q_SET_DATA(q_p, data_p)		q_p->node_data_p = (void*)data_p
#define INFRA_Q_GET_NEXT(q_p)			q_p->next_p

/*************************************************************************/
/*              Message queue functions                                  */
/*************************************************************************/

infra_error_code_t infra_os_message_q_initialize(infra_os_message_q_t **queue);
infra_error_code_t infra_os_message_q_terminate(infra_os_message_q_t *queue);
infra_error_code_t infra_os_message_q_send(infra_os_message_q_t *queue, void * data_p,bool insert_at_front);
void * infra_os_message_q_receive(infra_os_message_q_t *queue);


infra_error_code_t infra_q_remove_node(infra_q_t **head_p, uint32_t key, infra_q_t **node_p);
infra_error_code_t infra_q_insert_node(infra_q_t **head_p, infra_q_t *node_p, uint32_t key, void *data_p);
infra_error_code_t infra_q_search_node(infra_q_t *head_p, uint32_t key, infra_q_t **node_q_p);


#ifdef __cplusplus
}
#endif

#endif /*_EVENT_QUEUE_H_*/
