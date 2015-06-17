#ifndef CONFIG_ARCH_STI
#include <linux/stm/gpio.h>
#include <linux/stm/pio.h>
#include <linux/stm/pad.h>
#endif

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "ca_os_wrapper.h"
#include "scr_utils.h"
#include "stm_event.h"

char state[7][50] = {"SCR_THREAD_STATE_IDLE",
			"SCR_THREAD_STATE_CARD_NOT_PRESENT",
			"SCR_THREAD_STATE_DEACTIVATE",
			"SCR_THREAD_STATE_CARD_PRESENT",
			"SCR_THREAD_STATE_PROCESS",
			"SCR_THREAD_STATE_DELETE",
			"SCR_THREAD_STATE_LOW_POWER",
			};

static void scr_task_process_reset(scr_ctrl_t *scr_p, scr_message_t *msg_p);
static void scr_task_retry_reset(scr_ctrl_t *scr_p, scr_message_t *msg_p);

void scr_thread(void * params) /* don't allocate any memory in the thread */
{
	scr_message_t *message;
	scr_ctrl_t *scr_handle_p;
	scr_thread_state_t last_message;
	stm_event_t event;
	int error = 0;
	scr_handle_p = (scr_ctrl_t *)params;

	while (1) {
		scr_debug_print("<%s> ::<%d> %p\n", __func__, __LINE__,
			scr_handle_p->message_q->sema);
		message = ca_os_message_q_receive(scr_handle_p->message_q);
		if (NULL == message)
			continue;
		scr_debug_print("<%s> ::<%d> current->>%d next->>%d state %s\n",
			__func__, __LINE__, scr_handle_p->state,
			message->next_state, state[message->next_state]);

		if (scr_check_state(scr_handle_p, message->next_state)) {
		/* check the next state validity with the current state*/
			scr_handle_p->last_status = -EINVAL;
			if (message->sema == NULL)
				/* this message send from non-blocking point*/
				ca_os_free(message);
			else
				ca_os_sema_signal(message->sema);
			continue;
		}
		scr_handle_p->state = message->next_state;
		last_message = message->next_state;

		switch (scr_handle_p->state) {

		case SCR_THREAD_STATE_CARD_NOT_PRESENT:
			/* Send remove event to user */
			event.object = (void*)scr_handle_p;
			event.event_id = STM_SCR_EVENT_CARD_REMOVED;
			error = stm_event_signal(&event);
			if (error)
				ca_error_print("stm_event_signal "\
				"failed(%d)\n", error);

			scr_internal_deactivate(message->scr_handle_p);
			scr_handle_p->state = SCR_THREAD_STATE_IDLE;
			scr_handle_p->reset_done = false;
			/* this message send from non-blocking point*/
			ca_os_free(message);
			message = NULL;
			/* No break*/
		case SCR_THREAD_STATE_IDLE :
			scr_card_present(scr_handle_p);
			/* This will execute only for the first time */
			if (SCR_THREAD_STATE_IDLE == last_message)
				ca_os_sema_signal(message->sema);
			if (scr_card_present(scr_handle_p)) {
				scr_handle_p->state = SCR_THREAD_STATE_CARD_PRESENT;
				last_message = SCR_THREAD_STATE_CARD_PRESENT;
			}
		break;

		case SCR_THREAD_STATE_DEACTIVATE:
			scr_internal_deactivate(message->scr_handle_p);
			scr_handle_p->state = SCR_THREAD_STATE_CARD_PRESENT;
			if (message->sema == NULL) {
				ca_os_free(message);
				message = NULL;
			} else
				ca_os_sema_signal(message->sema);
			/* No break */
		case SCR_THREAD_STATE_CARD_PRESENT:
			if (SCR_THREAD_STATE_CARD_PRESENT == last_message) {
				ca_os_sleep_milli_sec(10);
				scr_debug_print("<%s>::<%d> current state %d\n",
					__func__, __LINE__,
					scr_handle_p->state);

				event.object = (void*)scr_handle_p;
				event.event_id = STM_SCR_EVENT_CARD_INSERTED;
				error = stm_event_signal(&event);
				if (error)
					ca_error_print("stm_event_signal "\
					"failed(%d)\n", error);

				ca_os_free(message);
				message = NULL;
			}
		break;
		case SCR_THREAD_STATE_PROCESS:
			switch (message->type) {
			case SCR_PROCESS_CMD_TRANSFER: {
				stm_scr_transfer_params_t *trans_param_p =
				(stm_scr_transfer_params_t *)message->data_p;

				if (STM_SCR_TRANSFER_RAW_WRITE ==
					trans_param_p->transfer_mode)
					scr_handle_p->last_status =
					scr_internal_write(
						message->scr_handle_p,
						trans_param_p);
				else if (STM_SCR_TRANSFER_RAW_READ ==
					trans_param_p->transfer_mode)
					scr_handle_p->last_status =
					scr_internal_read(message->scr_handle_p,
					trans_param_p);
				else if (STM_SCR_TRANSFER_APDU ==
					trans_param_p->transfer_mode)
					scr_handle_p->last_status =
					scr_internal_transfer(
					message->scr_handle_p,
					trans_param_p);
				else
					scr_handle_p->last_status = -EINVAL;
				ca_os_sema_signal(message->sema);

			}
			break;

			case SCR_PROCESS_CMD_PPS:
				scr_handle_p->last_status =
					scr_internal_pps(message->scr_handle_p,
					(uint8_t *)message->data_p);
				if (message->scr_handle_p->last_status) {
					scr_handle_p->last_error =
							SCR_ERROR_PPS_FAILED;
					scr_send_message(scr_handle_p,
						SCR_THREAD_STATE_DEACTIVATE,
						SCR_PROCESS_CMD_INVALID,
						NULL,
						true);
				}
				ca_os_sema_signal(message->sema);

			break;

			case SCR_PROCESS_CMD_RESET:
				scr_handle_p->reset_done = false;
				scr_task_process_reset(scr_handle_p, message);
			break;

			case SCR_PROCESS_CMD_ABORT:
			/* uart abort to be called from the API context*/
				ca_os_sema_signal(message->sema);
			break;

			case SCR_PROCESS_CMD_IFSC :
				scr_handle_p->last_status =
					scr_internal_send_ifsc(
						message->scr_handle_p,
						*(uint8_t *)message->data_p);

				if (scr_handle_p->last_status) {
					scr_error_print(" IFSC failed sending "\
					"Deactivate message\n");
					scr_send_message(scr_handle_p,
						SCR_THREAD_STATE_DEACTIVATE,
						SCR_PROCESS_CMD_INVALID,
						NULL,
						true);
				}
				ca_os_sema_signal(message->sema);
			default:
			break;
			}
		break;

		case SCR_THREAD_STATE_DELETE:
			ca_os_sema_signal(message->sema);
			scr_error_print("SCR Thread exit\n");
			ca_os_task_exit();
		return;

		case SCR_THREAD_STATE_LOW_POWER:
		break;

		default :
		break;
			/*Error condition*/
		}
	}
}

void scr_task_retry_reset(scr_ctrl_t *scr_p, scr_message_t *msg_p)
{
	if (!scr_p->reset_retry)
		return;
	scr_error_print("<INFO> Reset failed, Changing class %d\n",
		scr_p->last_status);
	scr_internal_deactivate(scr_p);
	scr_internal_change_class(scr_p);
	scr_p->last_status = scr_internal_reset(scr_p,
				*(uint32_t *)msg_p->data_p);
}

void scr_task_process_reset(scr_ctrl_t *scr_p, scr_message_t *msg_p)
{
	scr_p->last_status =
		scr_internal_reset(scr_p, *(uint32_t *)msg_p->data_p);

	if (scr_p->last_status)
		scr_task_retry_reset(scr_p, msg_p);

	if (scr_p->last_status) {
		scr_error_print("Reset failed, sending message\n");
		scr_send_message(scr_p,
			SCR_THREAD_STATE_DEACTIVATE,
			SCR_PROCESS_CMD_INVALID,
			NULL,
			true);
	}
	else
	{
		scr_p->reset_done = true;
	}

#ifdef CONFIG_ARCH_STI
	scr_p->scr_caps.class_sel_gpio_value =
		scr_internal_read_class_sel_pad(scr_p);
	scr_debug_print("<%s> :: < %d> working class sel "\
		"pad value for this card %d\n", __func__, __LINE__,
		(uint8_t)scr_p->scr_caps.class_sel_gpio_value);
#else
	if (scr_p->class_sel_pad_state) {
		scr_p->scr_caps.class_sel_pad_value =
			scr_internal_read_class_sel_pad(scr_p);
		scr_debug_print("<%s> :: < %d> working class sel "\
			"pad value for this card %d\n",
			__func__, __LINE__,
		(uint8_t)scr_p->scr_caps.class_sel_pad_value);
	}
#endif
	ca_os_sema_signal(msg_p->sema);
}
