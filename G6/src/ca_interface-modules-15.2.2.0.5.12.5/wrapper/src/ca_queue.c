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
 @File   ca_queue.c
 @brief

*/

#include "ca_queue.h"

#define CA_OS_MESSAGE_Q_KEY 0xabcdabcd

int ca_q_remove_node(ca_q_t **head_p, uint32_t key, ca_q_t **node_p)
{
	ca_q_t	*cur_p = NULL;
	ca_q_t	*prev_p = NULL;
	int	error = -EINVAL;

	if(*head_p == NULL)
		return -EINVAL;

	cur_p = *head_p;
	prev_p = *head_p;

	/*Head is the node , we are looking for*/
	if(key == cur_p->key){
		/*Update the head*/
		*node_p = cur_p;
		*head_p = cur_p->next_p;
		return 0;
	}
	while(cur_p != NULL){
		if(cur_p->key == key){
			*node_p = cur_p;
			prev_p->next_p = cur_p->next_p;
			error = 0;
			break;
		}
		prev_p = cur_p;
		cur_p = cur_p->next_p;
	}

	return error;
}


int ca_q_insert_node(ca_q_t **head_p,
		ca_q_t *node_p,
		uint32_t key, void *data_p)
{
	ca_q_t *cur_p = NULL;

	node_p->next_p =  NULL;

	/*This is the first element*/
	if(*head_p == NULL){
		*head_p = node_p;
		node_p->key = key;
		node_p->data_p = data_p;
		return 0;
	}

	cur_p = *head_p;

	while(cur_p->next_p != NULL)
		cur_p = cur_p->next_p;

	cur_p->next_p = node_p;
	node_p->key = key;
	node_p->data_p = data_p;

	return 0;
}

int ca_q_insert_at_head(ca_q_t **head_p,
		ca_q_t *node_p,
		uint32_t key,
		void *data_p)
{
	ca_q_t		*temp_p = NULL;

	node_p->next_p =  NULL;

	/*This is the first element*/
	if(*head_p == NULL){
		*head_p = node_p;
		node_p->key = key;
		node_p->data_p = data_p;
		return 0;
	}

	temp_p = *head_p;

	*head_p = node_p;
	node_p->next_p = temp_p;
	node_p->key = key;
	node_p->data_p = data_p;

	return 0;
}

int ca_q_search_node(ca_q_t *head_p, uint32_t key, ca_q_t **node_q_p)
{
	ca_q_t *cur_p = NULL;
	int error = -EINVAL;

	/*check if the Q is empty*/
	if(head_p == NULL)
		return error;


	cur_p = head_p;

	/*set the default location of the node to be searched o NULL*/
	*node_q_p = NULL;

	while(cur_p != NULL){
		if(cur_p->key == key){
			*node_q_p = cur_p;
			error = 0;
			break;
		}
		cur_p = cur_p->next_p;
	}

	return error;
}

int ca_os_message_q_initialize(ca_os_message_q_t **queue_p)
{
	int error_code;
	*queue_p = (ca_os_message_q_t *)ca_os_malloc(sizeof(ca_os_message_q_t));
	if(!*queue_p)
		return -ENOMEM;
	error_code=ca_os_sema_initialize(&(*queue_p)->sema,0);
	if(error_code)
		goto sema_error;
	if(!(*queue_p)->sema)
		printk("<%s>sema failed \n",__FUNCTION__);
	else
		printk(" every thing ok for sema \n");
	return 0;
sema_error:
	ca_os_free(*queue_p);
	return error_code;
}

int ca_os_message_q_terminate(ca_os_message_q_t *queue_p)
{
	int error_code;
	ca_os_sema_terminate(queue_p->sema);
	error_code=ca_os_free(queue_p);
	return error_code;
}

int ca_os_message_q_send(ca_os_message_q_t *queue_p,
		void * data_p,
		bool insert_at_front)
{
	int error_code = 0;
	ca_q_t * node_p = (ca_q_t *)ca_os_malloc(sizeof(ca_q_t));

	if(insert_at_front)
		error_code= ca_q_insert_at_head(&queue_p->head_p,
				node_p,
				CA_OS_MESSAGE_Q_KEY,
				data_p);
	else
		error_code=ca_q_insert_node(&queue_p->head_p,
				node_p,
				CA_OS_MESSAGE_Q_KEY,
				data_p);

	ca_os_sema_signal(queue_p->sema);
	return error_code;
}

void * ca_os_message_q_receive(ca_os_message_q_t *queue_p)
{
	ca_q_t * node_p;
	void * data_p;
	ca_os_sema_wait(queue_p->sema);
	if(ca_q_remove_node(&queue_p->head_p, CA_OS_MESSAGE_Q_KEY,&node_p))
		return NULL;

	data_p = node_p->data_p;
	ca_os_free(node_p);
	return data_p;
}
