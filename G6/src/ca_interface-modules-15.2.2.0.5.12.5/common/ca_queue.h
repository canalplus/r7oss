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
 @File   ca_queue.h
 @brief



*/


#ifndef _CA_QUEUE_H_
#define _CA_QUEUE_H_

#include "ca_os_wrapper.h"

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct ca_q_s{
	/*Pointer to the actual node whose linked list is constructed*/
	void	*data_p;
	/*key to locate any node. helps in searching and deltion*/
	uint32_t key;
	struct ca_q_s *next_p; /*Pointer to the next Q element*/
}ca_q_t;

typedef struct{
	ca_os_semaphore_t sema;
	ca_q_t *head_p;
}ca_os_message_q_t;


#define CA_Q_GET_DATA(q_p, type)		(type*)(q_p->data_p)
#define CA_Q_SET_DATA(q_p, data_p)		q_p->node_data_p = (void*)data_p
#define CA_Q_GET_NEXT(q_p)			q_p->next_p

/*************************************************************************/
/*              Message queue functions                                  */
/*************************************************************************/

int ca_os_message_q_initialize(ca_os_message_q_t **queue);
int ca_os_message_q_terminate(ca_os_message_q_t *queue);
int ca_os_message_q_send(ca_os_message_q_t *queue,
		void * data_p,
		bool insert_at_front);
void * ca_os_message_q_receive(ca_os_message_q_t *queue);


int ca_q_remove_node(ca_q_t **head_p, uint32_t key, ca_q_t **node_p);
int ca_q_insert_node(ca_q_t **head_p,
		ca_q_t *node_p,
		uint32_t key,
		void *data_p);
int ca_q_search_node(ca_q_t *head_p, uint32_t key, ca_q_t **node_q_p);


#ifdef __cplusplus
}
#endif

#endif /*_CA_QUEUE_H_*/
