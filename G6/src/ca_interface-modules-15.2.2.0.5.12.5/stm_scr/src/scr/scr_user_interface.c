#include <linux/fs.h>
#include <linux/string.h>
#include <linux/poll.h>

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_user_interface.h"

static int32_t scr_stm_open(struct inode *inode, struct file *file);
static int32_t scr_stm_close(struct inode *inode, struct file *file);
static ssize_t scr_stm_read(struct file *file_p, /* see include/linux/fs.h */
			char *buffer, /* buffer to fill with data */
			size_t length, /* length of the buffer */
			loff_t *offset);
static ssize_t scr_stm_write(struct file *file_p, /* see include/linux/fs.h*/
			const char *buffer, /* buffer to fill with data*/
			size_t length, /* length of the buffer */
			loff_t *offset);
static long scr_stm_ioctl(struct file *file,
		unsigned int command,
		unsigned long data);
static unsigned int scr_stm_poll(struct file *filp, poll_table *pwait);
static int scr_event_setup(struct file *file, stm_scr_h scr_h);

static struct file_operations fops = {
	.read = scr_stm_read,
	.write = scr_stm_write,
	.open = scr_stm_open,
	.release = scr_stm_close,
	.unlocked_ioctl = scr_stm_ioctl,
	.poll = scr_stm_poll
};

struct file_operations *scr_get_fops(void)
{
	return &fops;
}

static int32_t scr_stm_open(struct inode *inode, struct file *file)
{
	int32_t error_code;
	int8_t *filename;
	int8_t buffer[100];
	stm_scr_h scr;
	stm_scr_device_type_t device_type;
	int32_t num = MINOR(inode->i_rdev);

	filename = d_path(&file->f_path, buffer, sizeof(buffer));

	device_type = SCR_DEVICE_TYPE(filename);
	scr_debug_print("<%s> :: <%d> Minor No. %d for %s for device type %d\n",
				__func__, __LINE__, num, filename, device_type);
	if (device_type == STM_SCR_DEVICE_CA)
		num = num - STM_SCR_MAX;

	if (num >= STM_SCR_MAX)
		scr_error_print("Open exceed the max scr no.\n");

	error_code = stm_scr_new(num, device_type, &scr);
	if (error_code < 0) {
		scr_error_print("Scr open failed with %d\n", num);
		goto error;
	}
	else
		file->private_data = scr;
	scr->event_occurred = 0;
	init_waitqueue_head(&scr->queue);
	error_code = scr_event_setup(file, scr);
	if (error_code < 0) {
		scr_error_print("Scr open event setup failed with %d\n", num);
		error_code = stm_scr_delete(scr);
		if (error_code < 0)
			scr_error_print("Scr delete failed\n");
	}
error:
	return error_code;
}

static int32_t scr_stm_close(struct inode *inode, struct file *file)
{
	int32_t error_code;
	int32_t num = MINOR(inode->i_rdev);
	stm_scr_h scr_h = ((stm_scr_h)file->private_data);
	scr_debug_print("<%s> :: <%d> Minor No. %d\n", __func__, __LINE__, num);

	error_code =  stm_event_subscription_delete(scr_h->subscription);
	if (error_code) {
		scr_debug_print(" <%s > :: <%d > stm_event_subscription_delete"\
						"failed !!!!! \n",
						__func__,
						__LINE__);
		goto error;
	} else
		scr_h->subscription = NULL;
        scr_h->event_occurred = POLLERR;
        wake_up_interruptible(&scr_h->queue);

	error_code = stm_scr_delete(scr_h);
	if (error_code < 0)
		scr_error_print("Scr delete failed with %d\n", num);
error:
	return error_code;
}


static ssize_t scr_stm_read(struct file *file_p, /* see include/linux/fs.h */
			char *buffer, /* buffer to fill with data */
			size_t length, /* length of the buffer */
			loff_t *offset)
{
	int32_t error_code;
	stm_scr_transfer_params_t transfer_params;

	int32_t num = MINOR(file_p->f_dentry->d_inode->i_rdev);

	scr_debug_print("<%s> :: <%d> Minor No. %d\n", __func__, __LINE__, num);

	transfer_params.response_p =
		(uint8_t *)ca_os_malloc(length * sizeof(char));
	if (NULL == transfer_params.response_p) {
		scr_error_print("memory allocation faile while scr read\n");
		return -1;
	}
	transfer_params.response_buffer_size = length;
	transfer_params.transfer_mode = STM_SCR_TRANSFER_RAW_READ;
	transfer_params.timeout_ms = 10000; /* Infinite wait */
	error_code = stm_scr_transfer(((stm_scr_h)file_p->private_data),
				&transfer_params);

	if (error_code < 0) {
		scr_error_print("Scr read failed with %d\n", num);
		return -1;
	}

	error_code = copy_to_user((int8_t *)buffer,
				(int8_t *)transfer_params.response_p,
				transfer_params.actual_response_size);
	if (error_code < 0) {
		scr_error_print("copy to user failed %d\n", error_code);
		return -1;
	}

	ca_os_free(transfer_params.response_p);
	return transfer_params.actual_response_size;
}

static ssize_t scr_stm_write(struct file *file_p, /* see include/linux/fs.h */
			const char *buffer, /* buffer of data */
			size_t length, /* length of the buffer */
			loff_t *offset)
{
	int32_t error_code;
	int8_t *buffer_p;
	stm_scr_transfer_params_t transfer_params;
	int32_t num = MINOR(file_p->f_dentry->d_inode->i_rdev);

	scr_debug_print("<%s> :: <%d> Minor No. %d\n", __func__, __LINE__, num);
	buffer_p = (uint8_t *)ca_os_malloc(length * sizeof(char));
	if (NULL == buffer_p) {
		scr_error_print("buffer allocation faile for scr_stm_write\n");
		return -1;
	}
	transfer_params.buffer_p = buffer_p;
	transfer_params.buffer_size = length;
	transfer_params.transfer_mode = STM_SCR_TRANSFER_RAW_WRITE;
	transfer_params.timeout_ms = 10000; /* Infinite wait */
	error_code = copy_from_user(buffer_p, buffer, length);
	if (error_code < 0) {
		scr_error_print("copy_from_user for scr_stm_write with %d\n",
				num);
		ca_os_free(buffer_p);
		return -1;
	}

	error_code = stm_scr_transfer(((stm_scr_h)file_p->private_data),
				&transfer_params);
	if (error_code < 0) {
		scr_error_print("Scr write failed with %d\n", num);
		ca_os_free(buffer_p);
		return -1;
	}
	ca_os_free(buffer_p);
	return transfer_params.buffer_size;
}

static int scr_copy_capabilities(char *data, stm_scr_capabilities_t
				capabilities)
{
	int error = 0;
	stm_scr_capabilities_t *cap = (stm_scr_capabilities_t *)data;
	error = copy_to_user((char *)cap->history,
				(char *)capabilities.history,
				capabilities.size_of_history_bytes);
	if(error < 0) {
		scr_error_print("copy_from_user for "\
			"scr_stm_ioctl failed for history bytes\n");
	}
	error = copy_to_user((char *)cap->pts_bytes,
			(char *)capabilities.pts_bytes,
			capabilities.pps_size);
	if(error < 0) {
		scr_error_print("copy_from_user for "\
			"scr_stm_ioctl failed for PTS bytes\n");
	}
	error = copy_to_user((char *)&cap->max_baud_rate,
				(char *)&capabilities.max_baud_rate,
				sizeof(stm_scr_capabilities_t)-8);
	if(error < 0) {
		scr_error_print("copy_from_user for "\
			"scr_stm_ioctl failed for capabilities\n");
	}
	return error;
}

static long scr_stm_ioctl(struct file *file,
		unsigned int command, unsigned long data)
{
	int32_t error_code = 0, error;
	int32_t num = MINOR(file->f_dentry->d_inode->i_rdev);

	scr_debug_print("<%s> :: <%d> command %x for %d\n",
		__func__, __LINE__, command, num);

	switch (command) {
	case STM_SCR_SET_CTRL: {
		struct stm_ctrl_command_s ctrl_command;
		error = copy_from_user((char *)&ctrl_command,
			(char *)data, sizeof(struct stm_ctrl_command_s));
		if (error_code < 0) {
			scr_error_print("copy_from_user for "\
				"scr_stm_ioctl failed for command %d\n",
				command);

			return -1;
		}

		scr_debug_print("command %d, ctrl_flag %x, data %d\n",
		command, ctrl_command.ctrl_flag, ctrl_command.value);

		error_code = stm_scr_set_control(
			((stm_scr_h)file->private_data),
			ctrl_command.ctrl_flag,
			ctrl_command.value);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed for "\
				"command %d\n", command);
			return -1;
		}
	}
	break;

	case STM_SCR_GET_CTRL: {
		static uint32_t value;
		struct stm_ctrl_command_s ctrl_command;
		error = copy_from_user((char *)&ctrl_command,
			(char *) data, sizeof(struct stm_ctrl_command_s));
		if (error_code < 0) {
			scr_error_print("copy_from_user for "\
			"scr_stm_ioctl for command %d\n", command);
			return -1;
		}

		scr_debug_print("command %d, ctrl_flag %x\n",
			command, ctrl_command.ctrl_flag);
		error_code = stm_scr_get_control(
			(stm_scr_h)file->private_data,
			ctrl_command.ctrl_flag, &value);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed "\
				"for command %d\n", command);
			return -1;
		} else {
			scr_debug_print("value returned %d\n", value);
			ctrl_command.value = value;
			error = copy_to_user((char *) data,
				(char *) &ctrl_command,
				sizeof(struct stm_ctrl_command_s));
			if (error < 0) {
				scr_error_print(" copy to user failed "\
					"for set ctrl %d\n", error);
				return -1;
			}
		}
	}
	break;

	case STM_SCR_RESET: {
		struct stm_reset_command_s reset_command ;
		error = copy_from_user((char *)&reset_command,
			(char *)data, sizeof(struct stm_reset_command_s));
		if (error_code < 0) {
			scr_error_print("copy_from_user for"\
			"scr_stm_ioctl for command %d\n", command);
			return -1;
		}

		error_code = stm_scr_reset((stm_scr_h)file->private_data,
					reset_command.reset_type,
					reset_command.atr,
					&reset_command.atr_length);
		if (error_code) {
			scr_error_print("SCR Ioctl command failed for command"\
				"%d\n", command);
			return -1;
		} else {
			scr_debug_print(" SCR reset passed length %d\n",
					reset_command.atr_length);
			error = copy_to_user((char *)data,
				(char *)&reset_command,
				sizeof(struct stm_reset_command_s));
			if (error < 0) {
				scr_error_print(" copy to user failed "\
					"for reset %d\n", error);
				return -1;
			}
		}
	}
	break;

	case STM_SCR_GET_CAPABILITIES: {
		stm_scr_capabilities_t capabilities;
		scr_debug_print("Get capabilities\n");
		error_code =
			stm_scr_get_capabilities((stm_scr_h)file->private_data,
				&capabilities);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed for command"\
				"%d\n", command);
			return -1;
		} else {
			error_code =
				scr_copy_capabilities((char *)data,
					capabilities);
			if (error_code < 0) {
				scr_error_print("copy of capabilities"
					" failed\n");
				return -1;
			}
		}
	}
	break;

	case STM_SCR_DEACTIVATE:
		error_code = stm_scr_deactivate((stm_scr_h)file->private_data);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed "\
				"for command %d\n", command);
			return -1;
		}
	break;

	case STM_SCR_ABORT:
		error_code = stm_scr_abort((stm_scr_h)file->private_data);
		if (error_code) {
			scr_error_print("SCR Ioctl command failed for"\
				"command %d\n", command);
			return -1;
		}
	break;

	case STM_SCR_GET_CARD_STATUS: {
		bool is_present;
		unsigned int ispresent=0;
		error_code = stm_scr_get_card_status(
			(stm_scr_h)file->private_data, &is_present);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed for"\
				" command %d\n", command);
			return -1;
		} else {
			scr_debug_print("SCR card present status %d\n",
				is_present);
			ispresent = (is_present ==true)? 1 : 0;
			error = copy_to_user((char *)data,
				(char *)&ispresent, sizeof(unsigned int));
			if (error < 0) {
				scr_error_print("copy to user failed "\
				"for get CARD_STATUS %d\n", error);
				return -1;
			}
		}
	}
	break;

	case STM_SCR_PROTOCOL_NEGOTIATION: {
		struct stm_pps_command_s pps;
		error = copy_from_user((char *)&pps,
			(char *)data, sizeof(struct stm_pps_command_s));
		if (error_code < 0) {
			scr_error_print("copy_from_user for"\
				"scr_stm_ioctl for command %d\n", command);
			return -1;
		}
		error_code = stm_scr_protocol_negotiation(
			(stm_scr_h)file->private_data,
				pps.pps_data, pps.pps_response);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed for"\
				"command %d\n", command);
			return -1;
		} else {
			scr_debug_print("SCR PPS negotiation\n");
			error = copy_to_user((char *)data,
			(char *)&pps, sizeof(struct stm_pps_command_s));
			if (error < 0) {
				scr_error_print("copy to user failed "\
				"for PPS negotiation %d\n", error);
				return -1;
			}
		}
	}
	break;
	case STM_SCR_TRANSFER: {
		stm_scr_transfer_params_t transfer_params;
		struct stm_transfer_command_s transfer;
		error = copy_from_user((char *)&transfer,
				(char *)data,
				sizeof(struct stm_transfer_command_s));
		if (error_code < 0) {
			scr_error_print("copy_from_user for"\
				"scr_stm_ioctl for command %d\n", command);
			return -1;
		}
		transfer_params.buffer_p =
				(uint8_t *)ca_os_malloc(
				transfer.write_size);

		if (transfer_params.buffer_p == NULL) {
			scr_error_print("Error nomem:failed in scr transfer\n");
			return -1;
		}

		transfer_params.response_p =
				(uint8_t *)ca_os_malloc(
				transfer.read_size);
		if (transfer_params.response_p == NULL) {
			scr_error_print("Error nomom:failed in scr transfer\n");
			ca_os_free((void *)transfer_params.buffer_p);
			return -1;
		}
		transfer_params.buffer_size = transfer.write_size;
		transfer_params.response_buffer_size = transfer.read_size;
		transfer_params.transfer_mode = STM_SCR_TRANSFER_APDU;
		transfer_params.timeout_ms = transfer.timeout_ms;
		error = copy_from_user((char *)transfer_params.buffer_p,
				(char *)transfer.write_buffer,
				transfer.write_size);
		if (error_code < 0) {
			scr_error_print("copy_from_user for data"\
				"for transfer failed\n");
			ca_os_free((void *)transfer_params.buffer_p);
			ca_os_free(transfer_params.response_p);
			return -1;
		}
		error_code = stm_scr_transfer(
			(stm_scr_h)file->private_data,
				&transfer_params);
		if (error_code) {
			scr_error_print(" SCR Ioctl command failed for"\
				"command %d\n", command);
			ca_os_free((void *)transfer_params.buffer_p);
			ca_os_free(transfer_params.response_p);
			return -1;
		}
		else {
			scr_debug_print("SCR Transfer\n");
			error = copy_to_user(transfer.read_buffer,
				transfer_params.response_p,
				transfer_params.actual_response_size);
			if (error < 0) {
				scr_error_print("copy to user failed "\
				"for data for transfer %d\n", error);
				ca_os_free((void *)transfer_params.buffer_p);
				ca_os_free(transfer_params.response_p);
				return -1;
			}
			transfer.actual_read_size =
				transfer_params.actual_response_size;
			error = copy_to_user((char *)data,
				(char *)&transfer,
				sizeof(struct stm_transfer_command_s));
			if (error < 0) {
				scr_error_print("copy to user failed "\
				"for transfer %d\n", error);
				ca_os_free((void *)transfer_params.buffer_p);
				ca_os_free(transfer_params.response_p);
				return -1;
			}
		}

		ca_os_free((void *)transfer_params.buffer_p);
		ca_os_free(transfer_params.response_p);
	}
	break;
	case STM_SCR_GET_LAST_ERROR: {
		unsigned int last_error = 0;
		error_code = stm_scr_get_last_error(
				(stm_scr_h)file->private_data, &last_error);
		if (error_code) {
			scr_error_print("SCR Ioctl command failed for"\
						" command %d\n", command);
			return -1;
		} else {
			scr_debug_print("SCR last error %d\n",
							last_error);

			error = copy_to_user((char *)data,
				 (char *)&last_error, sizeof(unsigned int));
			if (error < 0) {
				scr_error_print("copy to user failed "\
				     "for get CARD_LAST_ERROR %d\n", error);
				return -1;
			}
		}
	}
	break;

	}
	return error_code;
}

static void scr_event_handler(uint32_t number_of_events,
					stm_event_info_t *events)
{
	struct file *file_p = (struct file *)(events->cookie);
	stm_scr_h scr_h = (stm_scr_h)file_p->private_data;

	if (events->event.event_id == STM_SCR_EVENT_CARD_INSERTED)
		scr_debug_print("%s Card inserted\n", __func__);

	else if (events->event.event_id == STM_SCR_EVENT_CARD_REMOVED)
		scr_debug_print("%s Card removed\n", __func__);

	scr_h->event_occurred = true;
	wake_up_interruptible(&scr_h->queue);
}

static int scr_event_setup(struct file *file, stm_scr_h scr_h)
{
	stm_event_subscription_entry_t subscription_entries;
	ca_error_code_t error = CA_NO_ERROR;
	uint32_t number_of_entries = 1;
	subscription_entries.cookie = (void *)file;
	subscription_entries.event_mask =
		STM_SCR_EVENT_CARD_INSERTED | STM_SCR_EVENT_CARD_REMOVED;
	subscription_entries.object = (void *)scr_h;

	error = stm_event_subscription_create(&subscription_entries,
						number_of_entries,
						&scr_h->subscription);
	if (error) {
		scr_debug_print("<%s > :: <%d > stm_event_subscription_create"\
					"for %d entires failed !!!!!\n",
					__func__,
					__LINE__,
					number_of_entries);
		goto err;
	}
	error = stm_event_set_handler(scr_h->subscription, scr_event_handler);
	if (error) {
		scr_debug_print("<%s > :: <%d > stm_event_set_handler(0x%x)"\
					"failed for Handler(0x%x) !!!!!\n",
					__func__,
					__LINE__,
					(uint32_t)scr_h->subscription,
					(uint32_t)scr_event_handler);
	}
err:
	return error;
}

static unsigned int scr_stm_poll(struct file *file_p, poll_table *pwait)
{
	int32_t mask = 0;
	stm_scr_h scr_h = file_p->private_data;
	int32_t num = MINOR(file_p->f_dentry->d_inode->i_rdev);

	scr_debug_print("<%s> :: <%d> Minor No. %d\n",
			 __func__, __LINE__, num);
	poll_wait(file_p, &scr_h->queue, pwait);
	mask = scr_h->event_occurred;
	scr_h->event_occurred = 0;
	return mask;
}
