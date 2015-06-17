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
 @File   event_signaler.c
 @brief



*/

#include "event_signaler.h"
#include "event_subscriber.h"
#include "event_utils.h"
#include "event_debug.h"
#include "event_debugfs.h"

static evt_mng_signaler_control_t	*evt_signaler_control_p;

static evt_mng_signaler_param_h evtmng_get_signaler_from_object(
								stm_object_h object);
static infra_error_code_t evtmng_alloc_new_signaler(
						    evt_mng_signaler_param_t **signaler_param_p,
						    stm_event_subscription_entry_t *subscription_entry,
						    void *subscriber_p);

#define __get_sig(sig) atomic_inc(&sig->use_cnt)

#define __put_sig(sig) \
do { \
	atomic_dec(&sig->use_cnt); \
	wake_up_interruptible(&sig->delete_waitq); \
} while (0)


static int __delete_signaler(evt_mng_signaler_param_t *signaler_p)
{
	int err = 0;
	evt_mng_signaler_control_t *sig_head = evt_signaler_control_p;
	evt_mng_signaler_q_t		*signaler_q_p = NULL;
	u_long flags = 0;

	infra_os_write_lock_irqsave(&evt_signaler_control_p->lock_sig_q_param_p, flags);
	if (signaler_p->subcriber_head_p == NULL) {
		err = infra_q_remove_node(&(sig_head->head),
		      (uint32_t) (signaler_p->object),
		      &signaler_q_p);
		if (err == 0)
			sig_head->num_signaler--;
	} else
		err = -EBUSY;
	infra_os_write_unlock_irqrestore(&evt_signaler_control_p->lock_sig_q_param_p, flags);

	if (err != 0) {
		evt_debug_trace(EVENT_SIGNALER, "sig(%p) deletion failed(%d)\n",
				signaler_p, err);
		return err;
	}

	/*check if this signaler is actively used
	 * If yes, wait until its ref count is 0
	 * */
	if (atomic_read(&signaler_p->use_cnt) != 0) {
		wait_event_interruptible(signaler_p->delete_waitq,
			(atomic_read(&signaler_p->use_cnt) == 0));
	}

	infra_os_free((void *) signaler_p);
	return err;
}

static infra_error_code_t evtmng_alloc_new_signaler(
						    evt_mng_signaler_param_t **signaler_p,
						    stm_event_subscription_entry_t *subscription_entry,
						    void *subscriber_p)
{

	evt_mng_signaler_param_t		*signaler_cur_p;
	u_long flags = 0;
	infra_error_code_t error = INFRA_NO_ERROR;
	evt_mng_subscribe_q_t *subscriber_q_p;
	stm_object_h object = subscription_entry->object;

	signaler_cur_p = (evt_mng_signaler_param_t *) infra_os_malloc(sizeof(*signaler_cur_p));

	if (NULL == signaler_cur_p) {
		*signaler_p = NULL;
		return -ENOMEM;
	}

	subscriber_q_p = (evt_mng_subscribe_q_t *) infra_os_malloc(
			      sizeof(*subscriber_q_p));

	if (NULL == subscriber_q_p) {
		infra_os_free((void *) signaler_cur_p);
		*signaler_p = NULL;
		return -ENOMEM;
	}

	infra_q_insert_node(&(signaler_cur_p->subcriber_head_p),
		      subscriber_q_p,
		      (uint32_t) subscriber_p,
		      (void *) subscriber_p);

	infra_os_rwlock_init(&(signaler_cur_p->lock_signaler_node));

	/*Intialize the object node*/
	signaler_cur_p->object = object;
	signaler_cur_p->signaler_handle = (uint32_t) signaler_cur_p;

	signaler_cur_p->evt_mask = subscription_entry->event_mask;

	init_waitqueue_head(&signaler_cur_p->delete_waitq);
	atomic_set(&signaler_cur_p->use_cnt, 0);

	infra_os_write_lock_irqsave(&evt_signaler_control_p->lock_sig_q_param_p, flags);

	/*The key for signaler linked list is its object value*/
	infra_q_insert_node(&(evt_signaler_control_p->head),
		      &(signaler_cur_p->signaler_q_p),
		      (uint32_t) object,
		      (void *) signaler_cur_p);
	evt_signaler_control_p->num_signaler++;

	infra_os_write_unlock_irqrestore(&evt_signaler_control_p->lock_sig_q_param_p, flags);

	evt_debug_trace(EVENT_SIGNALER,
		      " Obj:0x%x Sig_p:0x%x\n",
		      (uint32_t) object,
		      (uint32_t) signaler_cur_p);

	*signaler_p = signaler_cur_p;

	return error;
}

void evt_mng_set_signaler_control_param(evt_mng_signaler_control_t *control_p)
{
	evt_signaler_control_p = control_p;
}

evt_mng_signaler_control_t *evt_mng_get_signaler_control_param(void)
{
	return evt_signaler_control_p;
}


stm_object_h evt_mng_get_signaler_id(uint32_t signaler_handle)
{
	return ((evt_mng_signaler_param_t *) signaler_handle)->object;
}


/* This function must be called by taking global signaler control lock.
 */
static evt_mng_signaler_param_h evtmng_get_signaler_from_object(stm_object_h object)
{
	evt_mng_signaler_q_t		*signaler_q_p = NULL;
	evt_mng_signaler_param_t	*signaler_p = NULL;
	infra_error_code_t		error = INFRA_NO_ERROR;


	error = infra_q_search_node(
		      evt_signaler_control_p->head,
		      (uint32_t) object,
		      &signaler_q_p);

	if (error == INFRA_NO_ERROR) {
		/*we found a valid signaler*/
		signaler_p = INFRA_Q_GET_DATA(signaler_q_p, evt_mng_signaler_param_t);
		__get_sig(signaler_p);
	}

	evt_debug_trace(EVENT_SIGNALER,
		      " Err:%d Obj:0x%x Sig_q_p:0x%x Sig_p:0x%x\n",
		      error,
		      (uint32_t) object,
		      (uint32_t) signaler_q_p,
		      (uint32_t) signaler_p);

	return signaler_p;
}


infra_error_code_t evt_mng_signaler_deattach_subscriber(
							uint32_t signaler_handle,
							void *subscriber_p)
{
	evt_mng_subscribe_q_t		*subscriber_q_p = NULL;
	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_mng_signaler_param_t	*signaler_p;
	u_long flags = 0;

	/*Since this function is internal to event manger, we trust that the handle is valid*/
	signaler_p = (evt_mng_signaler_param_t *) signaler_handle;

	/*remove this subscriber from the subscriber list*/

	infra_os_write_lock_irqsave(&signaler_p->lock_signaler_node,
		      flags);
	error = infra_q_remove_node(&(signaler_p->subcriber_head_p),
		      (uint32_t) subscriber_p,
		      &(subscriber_q_p));
	infra_os_write_unlock_irqrestore(&signaler_p->lock_signaler_node,
		      flags);


	if (subscriber_q_p != NULL) {
		/*de-alloc the subscriber Q memory*/
		infra_os_free((void *) subscriber_q_p);
	} else {
		evt_error_trace(EVENT_SIGNALER, "Inavlid Subscriber\n");
	}

	evt_debug_trace(EVENT_SIGNALER,
		      " Error(%d) Obj:0x%x Sig:0x%x\n",
		      error,
		      (uint32_t) signaler_p->object,
		      (uint32_t) signaler_p);

	/*try to delete this signaler if this was
	 * the last subscriber attached.*/
	__delete_signaler(signaler_p);

	return error;
}

infra_error_code_t evt_mng_signaler_attach_subscriber(
						      stm_event_subscription_entry_t *subscription_entry,
						      uint32_t *signaler_handle_p,
						      void *subscriber_p)

{
	evt_mng_subscribe_q_t		*subscriber_q_p;
	evt_mng_signaler_param_t	*signaler_p;
	infra_error_code_t		error = INFRA_NO_ERROR;
	u_long flags = 0;

	infra_os_read_lock(&evt_signaler_control_p->lock_sig_q_param_p);
	/*First look, if this object is already present in the Q*/
	signaler_p = evtmng_get_signaler_from_object(subscription_entry->object);

	/*Since signaler was not found, create a new*/
	if (NULL == signaler_p) {
		infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);
		error = evtmng_alloc_new_signaler(
			      &(signaler_p),
			      subscription_entry,
			      subscriber_p);

	if (error != INFRA_NO_ERROR) {
			evt_error_trace(EVENT_SIGNALER, " Err: %d\n", error);
		return error;
	}
	} else {

		evt_debug_trace(EVENT_SIGNALER,
		      " Obj:0x%x Sig_p:0x%x EvtMask:0x%x\n",
		      (uint32_t) signaler_p->object,
		      (uint32_t) signaler_p,
		      signaler_p->evt_mask);

		infra_os_read_lock(&signaler_p->lock_signaler_node);
		error = infra_q_search_node(signaler_p->subcriber_head_p,
			      (uint32_t) (subscriber_p),
			      &subscriber_q_p);
		infra_os_read_unlock(&signaler_p->lock_signaler_node);

		/*is this subcriber already present in our linked list? Error means NO, so alloc a new node*/
		if (error != INFRA_NO_ERROR) {
			/*reset the error*/
			error = INFRA_NO_ERROR;
			subscriber_q_p = (evt_mng_subscribe_q_t *) infra_os_malloc(
				      sizeof(*subscriber_q_p));

			if (NULL == subscriber_q_p) {
				infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);
				__put_sig(signaler_p);
				return -ENOMEM;
			}

			infra_os_write_lock_irqsave(&signaler_p->lock_signaler_node,
				      flags);
			infra_q_insert_node(&(signaler_p->subcriber_head_p),
				      subscriber_q_p,
				      (uint32_t) subscriber_p,
				      (void *) subscriber_p);
			infra_os_write_unlock_irqrestore(&signaler_p->lock_signaler_node,
				      flags);
		}

		infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);

		infra_os_write_lock_irqsave(&signaler_p->lock_signaler_node,
					      flags);
		signaler_p->evt_mask |= subscription_entry->event_mask;
		infra_os_write_unlock_irqrestore(&signaler_p->lock_signaler_node,
					      flags);

		__put_sig(signaler_p);
	}

	*signaler_handle_p = (uint32_t) signaler_p;

	return error;
}


infra_error_code_t evt_mng_signaler_update_subscriber_info(
							   stm_event_subscription_entry_t *subscription_entry,
							   uint32_t signaler_handle)
{
	evt_mng_signaler_param_t	*signaler_p;
	infra_error_code_t		error = INFRA_NO_ERROR;
	u_long flags = 0;

	/*First look, if this object is present in the Q*/
	signaler_p = (evt_mng_signaler_param_t *) signaler_handle;

	/*Did we locate the correct signaler?*/
	if (signaler_p->signaler_handle == signaler_handle) {
		/*Update the event mask*/
		evt_debug_trace(EVENT_SIGNALER, " Error(%d)\n", error);
		infra_os_write_lock_irqsave(&signaler_p->lock_signaler_node,
			      flags);
		signaler_p->evt_mask |= subscription_entry->event_mask;
		infra_os_write_unlock_irqrestore(&signaler_p->lock_signaler_node,
			      flags);
	} else {
		error = -EINVAL;
	}

	return error;
}

infra_error_code_t evt_mng_handle_evt(stm_event_t *event)
{
	infra_error_code_t		error = INFRA_NO_ERROR;
	evt_mng_signaler_param_h	my_signaler_handle = NULL;
	stm_event_info_t		*evt_info_p;
	evt_mng_subscribe_q_t		*subcriber_cur_q_p;
	stm_event_subscription_h	subcriber_cur;
	uint32_t			number_of_events;
	int8_t				index;
	u_long			flags = 0;

	infra_os_read_lock(&evt_signaler_control_p->lock_sig_q_param_p);
	/*Get the required object from the object linked list*/
	my_signaler_handle = evtmng_get_signaler_from_object(event->object);
	infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);

	/*Validate that the object handle, if its a valid signaler*/
	if (my_signaler_handle == NULL) {
		evt_debug_trace(EVENT_SIGNALER, " SIGNALER NOT SUBSCRIBED>>>\n");
		return INFRA_NO_ERROR;
	}

	/*First validate that the event has been request else ignore this event*/
	infra_os_read_lock(&my_signaler_handle->lock_signaler_node);
	if ((event->event_id & my_signaler_handle->evt_mask) == 0) {
		evt_debug_trace(EVENT_SIGNALER, " EVT NOT REQUESTED>>>\n");
		__put_sig(my_signaler_handle);
		infra_os_read_unlock(&my_signaler_handle->lock_signaler_node);
		return INFRA_NO_ERROR;
	}
	infra_os_read_unlock(&my_signaler_handle->lock_signaler_node);

	/*Use EVT ID for index.*/
	index = evt_mng_get_bit_pos(event->event_id);
	if (index < 0) {
		__put_sig(my_signaler_handle);
		return -EINVAL;
	}

	evt_info_p = &my_signaler_handle->evt_mng_evt_info_t[index];

	infra_os_write_lock_irqsave(&my_signaler_handle->lock_signaler_node,
		      flags);

	/*We are here, so event has been requested.
	 * Copy the event related info into its place holder.*/
	infra_os_copy(&(evt_info_p->event), event, sizeof(*event));
	/*Copy other imporatnt data*/
	evt_info_p->timestamp = infra_os_get_time_in_milli_sec();

	my_signaler_handle->evt_occured |= event->event_id;

	infra_os_write_unlock_irqrestore(&my_signaler_handle->lock_signaler_node,
		      flags);


	/*presently lets hardcode the number of events to 1.
	 * will need modification during asyncronous callback*/
	number_of_events = 1;

	/*Now start informing the subscriber which have
	 * regsistered there Wait Q or Handlers*/
	infra_os_read_lock(&my_signaler_handle->lock_signaler_node);
	subcriber_cur_q_p = my_signaler_handle->subcriber_head_p;

	while (subcriber_cur_q_p != NULL) {
		subcriber_cur = INFRA_Q_GET_DATA(subcriber_cur_q_p,
			      stm_event_subscription_t);
		/*check subscriber validity*/
		error = evt_mng_report_entry(subcriber_cur, false);
		if (error != INFRA_NO_ERROR) {
			evt_error_trace(EVENT_SUBSCRIBER,
				      "Subscriber(%p) does not exist\n",
				      subcriber_cur);
			subcriber_cur_q_p = INFRA_Q_GET_NEXT(subcriber_cur_q_p);
			continue;
		}
		error = evt_mng_signal_subsciber(subcriber_cur,
			      evt_info_p,
			      number_of_events);
		evt_mng_report_exit(subcriber_cur);
		evt_debug_trace(EVENT_SIGNALER,
			      " INFORM SUB:0x%x >>>\n",
			      (uint32_t) subcriber_cur);
		subcriber_cur_q_p = INFRA_Q_GET_NEXT(subcriber_cur_q_p);
	}

	infra_os_read_unlock(&my_signaler_handle->lock_signaler_node);
	__put_sig(my_signaler_handle);

	return error;
}



bool evt_mng_signaler_lookup_events(
				    uint32_t signaler_handle,
				    evt_mng_check_event_param_t *check_event_param_p)
{
	evt_mng_signaler_param_t	*signaler_p;
	stm_event_info_t		*cur_event_p;
	stm_event_info_t		*src_event_p;
	uint32_t			evt_bit_pos = 0;
	uint32_t			evt_occured_mask = 0;
	bool				evt_occured = false;
	uint32_t			evt_status = 0x0;
	u_long flags = 0;


	signaler_p = (evt_mng_signaler_param_t *) signaler_handle;

	evt_debug_trace(EVENT_SIGNALER,
		      " evt_occured:0x%x  requested_evt_mask:0x%x\n",
		      signaler_p->evt_occured,
		      check_event_param_p->requested_evt_mask);

	infra_os_read_lock(&signaler_p->lock_signaler_node);
	if ((signaler_p->evt_occured &
		check_event_param_p->requested_evt_mask) == 0) {
		infra_os_read_unlock(&signaler_p->lock_signaler_node);
		return false;
	}

	evt_status = (check_event_param_p->evt_occurence_mask &
		signaler_p->evt_occured &
		check_event_param_p->requested_evt_mask);
	if (check_event_param_p->do_evt_retrieval == 0 && (evt_status != 0)) {
		infra_os_read_unlock(&signaler_p->lock_signaler_node);
		return true;
	}

	evt_occured_mask = signaler_p->evt_occured;
	infra_os_read_unlock(&signaler_p->lock_signaler_node);

	cur_event_p = (stm_event_info_t *) check_event_param_p->evt_info_mem_p;


	while ((evt_bit_pos < EVT_MNG_EVT_ARR_SIZE) && (evt_occured_mask > 0)) {
		if (((evt_occured_mask & 0x1) != 0) &&
			(check_event_param_p->evt_occurence_mask &
			(1 << evt_bit_pos))) {
			/*We have atleast one evt to be copied*/
			infra_os_write_lock_irqsave(
				      &signaler_p->lock_signaler_node,
				      flags);
			src_event_p = &(signaler_p->evt_mng_evt_info_t[evt_bit_pos]);
			infra_os_copy(cur_event_p,
				      src_event_p,
				      sizeof(*cur_event_p));
			cur_event_p->events_missed =
				(check_event_param_p->evt_occurence_info[evt_bit_pos] > 1) ?
				true : false;
			/*Also copy the cookie*/
			cur_event_p->cookie = check_event_param_p->cookie;
			/*Since event has been reported, so reset the occurence*/
			check_event_param_p->evt_occurence_info[evt_bit_pos] = 0;
			check_event_param_p->evt_occurence_mask &= ~(1 << evt_bit_pos);
			cur_event_p++;
			infra_os_write_unlock_irqrestore(
				      &signaler_p->lock_signaler_node,
				      flags);

			evt_debug_trace(EVENT_SIGNALER,
				      " Evt:0x%x Obj:0x%x\n",
				      src_event_p->event.event_id,
				      (uint32_t) src_event_p->event.object);
			evt_occured = true;
			check_event_param_p->num_evt_occured++;
		}
		evt_occured_mask = (evt_occured_mask >> 1);
		evt_bit_pos++;
		/*we have exceeded the max space to copy the events, so break now*/
		if (check_event_param_p->num_evt_occured >=
			check_event_param_p->max_evt_space)
			break;
	}

	/*Update the event data pointer before leaving*/
	check_event_param_p->evt_info_mem_p = (uint8_t *) cur_event_p;

	return evt_occured;
}

#if (defined SDK2_EVENT_ENABLE_DEBUGFS_STATISTICS)
/* This function dumps the all signalers and their subscribers for debug
 * purpose.
 */
ssize_t dump_signaler(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	evt_mng_signaler_param_t *signaler_cur_p;
	evt_mng_signaler_q_t *signaler_cur_q_p = NULL;
	evt_mng_subscribe_q_t *subscribe_cur_q_p;
	stm_event_subscription_h subscribe_cur_p;
	int32_t ii, jj;
	struct io_descriptor *io_desc;
	ssize_t nbread, size_copy;

	io_desc = f->private_data;

	if (*ppos >= io_desc->size_allocated && io_desc->size_allocated)
		goto done;

	if (*ppos == 0) {
		infra_os_read_lock(&evt_signaler_control_p->lock_sig_q_param_p);

		nbread = snprintf(NULL, 0, "Dumping Signaler List: Total signalers %d\n",
		      evt_signaler_control_p->num_signaler);

		ii = 1;
		/*Traverse the signaler list and
		* print each reference for the this signaler*/
		signaler_cur_q_p = evt_signaler_control_p->head;
		/* first find the size */
		while (signaler_cur_q_p != NULL) {
			signaler_cur_p = INFRA_Q_GET_DATA(signaler_cur_q_p,
				      evt_mng_signaler_param_t);

			infra_os_read_lock(&signaler_cur_p->lock_signaler_node);
			nbread += snprintf(NULL, 0, "signaler %d->0x%x object 0x%x evt_mask 0x%x\n",
			      ii++, (uint32_t) signaler_cur_p,
			      (uint32_t)signaler_cur_p->object,
			      signaler_cur_p->evt_mask);

			subscribe_cur_q_p = signaler_cur_p->subcriber_head_p;
			jj = 1;
			while (subscribe_cur_q_p) {
				subscribe_cur_p = INFRA_Q_GET_DATA(subscribe_cur_q_p,
					      stm_event_subscription_t);
				nbread += snprintf(NULL, 0, "\t\tsubscriber: %d->0x%x\n",
					      jj++, (uint32_t) subscribe_cur_p);
				subscribe_cur_q_p = INFRA_Q_GET_NEXT(subscribe_cur_q_p);
			}
			infra_os_read_unlock(&signaler_cur_p->lock_signaler_node);
			signaler_cur_q_p = INFRA_Q_GET_NEXT((&(signaler_cur_p->signaler_q_p)));
		}

		io_desc->data = infra_os_malloc(nbread);
		if (io_desc->data == NULL) {
			infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);
			return -ENOMEM;
		}

		io_desc->size_allocated = nbread;
		io_desc->size = nbread;

		ii = 1;
		signaler_cur_q_p = evt_signaler_control_p->head;
		nbread = sprintf(&((char *)io_desc->data)[0], "Dumping Signaler List: Total signalers %d\n",
			      evt_signaler_control_p->num_signaler);

		/* Actual copy of the list */
		while (signaler_cur_q_p != NULL) {
			signaler_cur_p = INFRA_Q_GET_DATA(signaler_cur_q_p,
				      evt_mng_signaler_param_t);

			infra_os_read_lock(&signaler_cur_p->lock_signaler_node);
			nbread += sprintf(&((char *)io_desc->data)[nbread], "signaler %d->0x%x object 0x%x evt_mask 0x%x\n",
			      ii++, (uint32_t) signaler_cur_p,
			      (uint32_t)signaler_cur_p->object,
			      signaler_cur_p->evt_mask);

			subscribe_cur_q_p = signaler_cur_p->subcriber_head_p;
			jj = 1;
			while (subscribe_cur_q_p) {
				subscribe_cur_p = INFRA_Q_GET_DATA(subscribe_cur_q_p,
					      stm_event_subscription_t);
				nbread += sprintf(&((char *)io_desc->data)[nbread], "\t\tsubscriber: %d->0x%x\n",
					      jj++, (uint32_t) subscribe_cur_p);
				subscribe_cur_q_p = INFRA_Q_GET_NEXT(subscribe_cur_q_p);
			}
			infra_os_read_unlock(&signaler_cur_p->lock_signaler_node);
			signaler_cur_q_p = INFRA_Q_GET_NEXT((&(signaler_cur_p->signaler_q_p)));
		}

		infra_os_read_unlock(&evt_signaler_control_p->lock_sig_q_param_p);
	}

	size_copy = (count < io_desc->size) ? count : io_desc->size;

	if (copy_to_user(buf, io_desc->data + *ppos, size_copy))
		goto done;

	*ppos += size_copy;
	io_desc->size -= size_copy;
	return size_copy;

done:
	if (io_desc->data)
		infra_os_free(io_desc->data);
	io_desc->data = NULL;
	io_desc->size = 0;
	io_desc->size_allocated = 0;
	return 0;
}
#endif
