#include <linux/fs.h>
#include <linux/string.h>
#include "stm_ci.h"
#include "ci_internal.h"

#ifndef	CI_MAGIC_NO
#define	CI_MAGIC_NO 'l'
#endif

enum {
	IOCTL_RESET = 0x1,
	IOCTL_GET_SLOT_STATUS,
	IOCTL_SET_COMMAND,
	IOCTL_GET_CAPABILITIES,
	IOCTL_GET_STATUS,
	IOCTL_GET_CIS
};

#define	CI_RESET			_IOWR(CI_MAGIC_NO, \
						IOCTL_RESET, uint32_t)
#define	CI_GET_SLOT_STATUS		_IOWR(CI_MAGIC_NO, \
						IOCTL_GET_SLOT_STATUS, uint32_t)
#define	CI_SET_COMMAND			_IOWR(CI_MAGIC_NO, \
						IOCTL_SET_COMMAND, uint32_t)
#define	CI_GET_CAPABILITIES		_IOWR(CI_MAGIC_NO, \
						IOCTL_GET_CAPABILITIES, \
						uint32_t)
#define	CI_GET_STATUS			_IOWR(CI_MAGIC_NO, \
						IOCTL_GET_STATUS, uint32_t)
#define	CI_GET_CIS			_IOWR(CI_MAGIC_NO, \
						IOCTL_GET_CIS, uint32_t)

#define	CI_DEVICE_NAME "stm_ci"
typedef	struct {
	stm_ci_command_t command;
	uint8_t	value;
} cam_command_t;

typedef	struct {
	stm_ci_status_t	status;
	uint8_t	value;
} cam_status_t;

typedef	struct {
	uint32_t reset_type;
} reset_command_t;

typedef	struct {
	int error;
	stm_ci_capabilities_t capability;
} ci_ioctl_getcapability_t;

static int32_t ci_stm_open(struct inode	*inode, struct file *file);
static int32_t ci_stm_close(struct inode *inode, struct	file *file);
static ssize_t ci_stm_read(struct file *file_p, /* see include/linux/fs.h */
			char *buffer, /*buffer to fill with data */
			size_t length, /* length of the buffer*/
			loff_t *offset);
static ssize_t ci_stm_write(struct file	*file_p, /* see include/linux/fs.h */
			const char *buffer, /* buffer to fill with data */
			size_t length, /*length of the buffer*/
			loff_t *offset);
static long ci_stm_ioctl(struct	file *file, unsigned int command,
		unsigned long	data);

static struct file_operations fops = {
	.read =	ci_stm_read,
	.write = ci_stm_write,
	.open =	ci_stm_open,
	.release = ci_stm_close,
	.unlocked_ioctl	= ci_stm_ioctl
};

static int32_t major;

int32_t	ci_register_char_driver(void)
{
	major =	register_chrdev(0, CI_DEVICE_NAME, &fops);
	if (major < 0) {
		ci_error_trace(CI_API_RUNTIME,
			"Registering char device failed\n");
		return major;
	} else {
		ci_debug_trace(CI_API_RUNTIME,
			"inside	register char driver %d\n",
				major);
	}
	return 0;
}
void ci_unregister_char_driver(void)
{
	unregister_chrdev(major, CI_DEVICE_NAME);
}

static int32_t ci_stm_open(struct inode	*inode, struct file *file)
{
	int32_t	error_code;
	stm_ci_h ci_p;

	int32_t	num = MINOR(inode->i_rdev);

	error_code = stm_ci_new(num, &ci_p);
	if (error_code < 0)
		ci_error_trace(CI_API_RUNTIME, "ci open	failed with %d\n", num);
	else
		file->private_data = ci_p;

	return error_code;
}

static int32_t ci_stm_close(struct inode *inode, struct	file *file)
{
	int32_t	error_code;
	int32_t	num = MINOR(inode->i_rdev);
	ci_debug_trace(CI_API_RUNTIME,
			"Minor No. %d\n", num);

	ci_debug_trace(CI_API_RUNTIME,
			"stm_ci_delete called\n");

	error_code = stm_ci_delete(((stm_ci_h)file->private_data));
	if (error_code < 0)
		ci_error_trace(CI_API_RUNTIME,
				"ci close failed with %d\n",
				num);

	return error_code;
}

static ssize_t ci_stm_read(struct file *file_p, /* see include/linux/fs.h */
			char *buffer, /*buffer to fill with data */
			size_t length, /*length of the buffer*/
			loff_t *offset)
{
	int32_t	error_code;
	stm_ci_msg_params_t	params;

	int32_t	num = MINOR(file_p->f_dentry->d_inode->i_rdev);

	ci_debug_trace(CI_API_RUNTIME, "Minor No. %d\n", num);

	params.msg_p = (uint8_t	*)ca_os_malloc(length * sizeof(char));
	params.response_length = 0;
	params.length =	length;	/* set to maximum size possible	*/
	error_code = stm_ci_read(((stm_ci_h)file_p->private_data),
							&params);
	if (error_code < 0) {
		ci_error_trace(CI_API_RUNTIME,
				"CI read failed	with %d\n",
				num);
		return -1;
	}
	error_code = copy_to_user((int8_t *)buffer,
				(int8_t	*)params.msg_p,
				params.response_length);
	if (error_code < 0) {
		ci_error_trace(CI_API_RUNTIME,
			"copy to user failed %d\n", error_code);
		return -1;
	}

	ca_os_free(params.msg_p);
	return	params.response_length;
}

static ssize_t ci_stm_write(struct file	*file_p, /* see include/linux/fs.h*/
		const char *buffer, /* buffer of data */
		size_t length, /*length of the buffer */
		loff_t *offset)
{
	int32_t	error_code;
	int8_t *buffer_p;
	stm_ci_msg_params_t	params;
	int32_t	num = MINOR(file_p->f_dentry->d_inode->i_rdev);

	ci_debug_trace(CI_API_RUNTIME, "Minor No. %d\n", num);

	buffer_p = (uint8_t *)ca_os_malloc(length * sizeof(char));

	params.msg_p = buffer_p;
	params.length =	length;
	params.response_length = 0;
	error_code = copy_from_user(buffer_p, buffer, length);
	if (error_code < 0) {
		ci_error_trace(CI_API_RUNTIME,
				"copy from user failed %d\n",
				error_code);
		return -1;
	}
	error_code = stm_ci_write(((stm_ci_h)file_p->private_data), &params);
	if (error_code < 0) {
		ci_error_trace(CI_API_RUNTIME,
			"CI write failed with %d\n",
			num);
		return -1;
	}
	ca_os_free(buffer_p);
	return	params.response_length;
}

static long ci_stm_ioctl(struct	file *file, unsigned int command,
		unsigned long	data)
{
	int32_t	error_code = 0, error = 0;

	switch (command) {
	case CI_SET_COMMAND:{
		cam_command_t ctrl_command;
		error =	copy_from_user((char *)&ctrl_command,
				(char *) data, sizeof(cam_command_t));
		if (error < 0) {
			ci_error_trace(CI_API_RUNTIME,
			"copy_from_user for ci_stm_ioctl for command %d\n",
					command);
			return -1;
		}
		ci_debug_trace(CI_API_RUNTIME,
			" command %d , set command %x, value %d\n",
				command,
				ctrl_command.command,
				ctrl_command.value);
		error_code = stm_ci_set_command(
			((stm_ci_h)file->private_data), ctrl_command.command,
				ctrl_command.value);
		if (error_code)
			ci_error_trace(CI_API_RUNTIME,
				" CI Ioctl command failed for command %d\n",
					command);
	}
	break;
	case CI_GET_STATUS:{
		static uint8_t value;
		cam_status_t cam_status;
		error =	copy_from_user((char *)&cam_status, (char *) data,
				sizeof(cam_status_t));
		if (error < 0) {
			ci_error_trace(CI_API_RUNTIME,
			"copy_from_user	for ci_stm_ioctl for command %d\n",
				command);
				return -1;
		}
		ci_debug_trace(CI_API_RUNTIME,
			"command %d, ctrl_flag %x\n",
				command,
				cam_status.status);
		error_code = stm_ci_get_status(((stm_ci_h)file->private_data),
				cam_status.status, &value);
		if (error_code)
			ci_error_trace(CI_API_RUNTIME,
				"CI Ioctl command failed for command %d\n",
				command);
		else {
			ci_debug_trace(CI_API_RUNTIME,
				"value returned %d\n",
				value);

			cam_status.value = value;
			error =	copy_to_user((char *) data,
				(char *) &cam_status, sizeof(cam_status_t));
			if (error < 0)
				ci_error_trace(CI_API_RUNTIME,
				"copy to user failed for get status %d\n",
					error);
			}
		}
	break;

	case CI_RESET:{
		reset_command_t	reset_command;
		error =	copy_from_user((char *)&reset_command,
				(char *)data, sizeof(reset_command_t));
		if (error < 0) {
			ci_error_trace(CI_API_RUNTIME,
			"copy_from_user for ci_stm_ioctl for command %d\n",
				command);
			return -1;
		}
		ci_debug_trace(CI_API_RUNTIME,
				"reset type %d\n",
				reset_command.reset_type);
		error = stm_ci_reset(((stm_ci_h)file->private_data),
					reset_command.reset_type);
		if (error)
			ci_error_trace(CI_API_RUNTIME,
				"CI Ioctl command failed with error %d\n",
					error);
		else {
			ci_debug_trace(CI_API_RUNTIME,
				"CI Ioctl command passed with error %d\n",
					error);
			}
		}
		break;

		case CI_GET_CAPABILITIES:{
			ci_ioctl_getcapability_t userparams;
			ci_debug_trace(CI_API_RUNTIME,
					"Get capabilities\n");
			userparams.error = stm_ci_get_capabilities(
					((stm_ci_h)file->private_data),
					&userparams.capability);
			if (userparams.error)
				ci_error_trace(CI_API_RUNTIME,
				"CI Ioctl command failed wth error %d\n",
						userparams.error);
			else {
				error =	copy_to_user(
				(ci_ioctl_getcapability_t *)data,
				&userparams, sizeof(ci_ioctl_getcapability_t));
				if (error < 0)
					ci_error_trace(CI_API_RUNTIME,
				"copy to user failed for get capabilities \
					%d\n", error);
			}
		}
		break;

		case CI_GET_SLOT_STATUS:{
			bool is_present;
			error_code = stm_ci_get_slot_status(
				((stm_ci_h)file->private_data), &is_present);
			if (error_code)
				ci_error_trace(CI_API_RUNTIME,
				"CI Ioctl command failed for command %d\n",
						(command));
			else {
				ci_debug_trace(CI_API_RUNTIME,
						"CI CAM present status %d\n",
						is_present);
				error =	copy_to_user((char *) data,
						(char *) &is_present,
						sizeof(bool));
				if (error < 0)
					ci_error_trace(CI_API_RUNTIME,
					" copy to user failed for get\
					CARD_STATUS %d\n",
				error);
			}
		}
		break;
	}
	return error_code;
}
