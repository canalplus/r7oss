/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/*
   @file     stm_evt_test_proc.c
   @brief

 */


#define _NO_VERSION_
#include <linux/module.h>  /* Module support */
#include <linux/version.h> /* Kernel version */
#include "infra_os_wrapper.h"
#include "infra_proc.h"
#include "infra_proc_interface.h"
#include "infra_debug.h"

#define INFRA_PROC_ROOT_DIR_NAME		"infrastructure"

static infra_proc_entry_t infra_core_proc_entry_p = NULL;

static int infra_read_this_proc(infra_proc_msg_interface_t *msg_p)
{
	infra_proc_control_param_t		*control_p;

	control_p = (infra_proc_control_param_t *)msg_p->data_p;

	if (control_p->handle != (uint32_t)control_p) {
		infra_error_print("%s@%d Invalid Arg\n",
				 __func__, __LINE__);
		return -EINVAL;
	}

	control_p->read_fp(msg_p);

	return INFRA_NO_ERROR;
}


static int infra_write_to_this_proc(infra_proc_msg_interface_t *msg_p)
{
	uint32_t				index = 0;
	uint32_t				len = 0;
	uint32_t				extra_len = 0;
	uint8_t				dummy_buf;
	uint8_t				*dummy_src;
	infra_proc_control_param_t		*control_p;

	control_p = (infra_proc_control_param_t *)msg_p->data_p;

	if (control_p->handle != (uint32_t)control_p) {
		infra_error_print("%s@%d Invalid Arg\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	len = (msg_p->msg_len > control_p->msg_max_buffer_size) ?
			control_p->msg_max_buffer_size : msg_p->msg_len;
	extra_len = msg_p->msg_len - len;

	if (copy_from_user(control_p->msg_buffer_p, msg_p->msg_p, len))
		return -EFAULT;

	control_p->write_fp(msg_p);

	/*read the remaining bytes to avoid suprious callbacks.*/
	for (index = 0; index < extra_len; index++) {
		dummy_src = (uint8_t *)(msg_p->msg_p+(len+index));
		if (copy_from_user((void *)(&dummy_buf), dummy_src, 1))
			return -EFAULT;
	}

	return INFRA_NO_ERROR;
}

int	infra_init_procfs_module()
{
	infra_core_proc_entry_p = proc_mkdir(INFRA_PROC_ROOT_DIR_NAME, NULL);

	if (infra_core_proc_entry_p == NULL){
		infra_error_print("%s@%d Infra Proc Creation Failed \n",__FUNCTION__, __LINE__);
		return (0);
	}
	infra_debug_trace("%s@%d Infra Proc Creation done \n",__FUNCTION__, __LINE__);

	return (1);
}


void	infra_term_procfs_module(void)
{
	remove_proc_entry(INFRA_PROC_ROOT_DIR_NAME,NULL);
}




void* infra_create_proc_entry(infra_proc_create_param_t *proc_create_param_p)
{
	void			*ret_p;

	ret_p = infra_init_this_proc(proc_create_param_p, infra_core_proc_entry_p);

	return ret_p;
}

int infra_remove_proc_entry(void* data_p)
{
	infra_error_code_t		err;

	err = infra_term_this_proc(data_p);
	infra_debug_trace("%s@%d \n", __FUNCTION__, __LINE__);

	return err;
}


int infra_proc_read_interface(struct file *file,
			char __user *buffer,
			size_t       len,
			loff_t      *offset)
{
	infra_proc_msg_interface_t		msg;

	msg.msg_len = 0;
	msg.msg_p = (uint8_t *)buffer;
	msg.data_p = file->private_data;

	infra_read_this_proc(&msg);
	infra_debug_trace("%s@%d \n", __FUNCTION__, __LINE__);
#if 0
	/* DON'T DO THAT - buffer overruns are bad */
	len = sprintf(page, "%s = %d\n", __FUNCTION__,__LINE__);
#endif
	return (msg.msg_len);
}

int infra_proc_write_interface(struct file *file,
			const	char __user *buffer,
			size_t       len,
			loff_t      *offset)
{

	infra_proc_msg_interface_t		msg;

	msg.msg_len = len;
	msg.msg_p = (uint8_t *)buffer;
	msg.data_p = file->private_data;

	infra_write_to_this_proc(&msg);

#if 0
	if(copy_from_user(evt_test_cmd, buffer, len))
		return -EFAULT;

	evt_test_cmd[len] = '\0';
#endif
	infra_debug_trace("%s@%d \n", __FUNCTION__, __LINE__);

	return msg.msg_len;
}
