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
   @file     infra_proc.c
   @brief

 */


#define _NO_VERSION_
#include <linux/module.h>  /* Module support */
#include <linux/version.h> /* Kernel version */
#include "infra_os_wrapper.h"
#include "infra_proc.h"
#include "infra_debug.h"


int infra_term_this_proc(void* data_p)
{

	infra_proc_control_param_t	*control_p;
	uint8_t				index = 0;

	control_p = (infra_proc_control_param_t *)data_p;

	if(control_p == NULL){
		return -EINVAL;
	}

	if(control_p->handle != (uint32_t)control_p){
		infra_error_print("%s@%d Invalid Arg\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	for(index = (control_p->max_dir_level-1);index>0;index--){
		remove_proc_entry(control_p->dir_name[index], control_p->proc_entry_p[index-1]);
	}

	pr_info("%s@%d \n", __FUNCTION__, __LINE__);

	if(control_p->msg_buffer_p != NULL){
		infra_os_free((void *) control_p->msg_buffer_p);
	}

	infra_os_free((void *) control_p);

	return INFRA_NO_ERROR;
}

static int infra_open(struct inode *inode, struct file *file)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	file->private_data = (void *) PDE_DATA(inode);
#else
	file->private_data = (void *) PDE(inode)->data;
#endif
	pr_info("%s (user data %p)\n", __func__, file->private_data);
	/*file->private_data = inode->i_private; */
	if (!file->private_data)
		return -EINVAL;
	return 0;
}

/*
 * close: do nothing!
 */
static int infra_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations infra_fops = {
	.owner		= THIS_MODULE,
	.open		= infra_open,
	.read		= infra_proc_read_interface,
	.write		= infra_proc_write_interface,
	.llseek		= default_llseek,
	.release	= infra_close,
};

void *infra_init_this_proc(infra_proc_create_param_t *proc_create_param_p, infra_proc_entry_t core_proc_entry_p)
{
	uint32_t				index = 0;
	infra_proc_control_param_t		*control_p;
	char		      *lname;
	struct proc_dir_entry *lpde;

	if(core_proc_entry_p == NULL){
		infra_error_print("%s@%d Invalid Arg\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	if(proc_create_param_p->msg_buffer_max_size == 0){
		infra_error_print("%s@%d Invalid Arg\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	if((proc_create_param_p->read_fp == NULL) || (proc_create_param_p->write_fp == NULL)){
		infra_error_print("%s@%d Invalid Arg\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	control_p = (infra_proc_control_param_t *)infra_os_malloc(sizeof(infra_proc_control_param_t));

	if(control_p == NULL){
		infra_error_print("%s@%d Alloc failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	control_p->msg_buffer_p = (uint8_t *)infra_os_malloc(proc_create_param_p->msg_buffer_max_size);

	if(control_p->msg_buffer_p == NULL){
		infra_os_free((void *) control_p);
		infra_error_print("%s@%d Alloc failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	control_p->handle = (uint32_t)control_p;
	control_p->max_dir_level = INFRA_MAX_PROC_DIR_ID;
	control_p->msg_max_buffer_size = proc_create_param_p->msg_buffer_max_size;
	control_p->read_fp = proc_create_param_p->read_fp;
	control_p->write_fp = proc_create_param_p->write_fp;
	control_p->proc_entry_p[INFRA_PROC_DIR_ROOT] = core_proc_entry_p;

	for(;index<control_p->max_dir_level; index++){
		strncpy(control_p->dir_name[index], proc_create_param_p->dir_name[index], (INFRA_MAX_PROC_DIR_NAME-1));
	}


	infra_debug_trace("%s@%d core_proc_entry_p>PROC_p:0x%x\n", __FUNCTION__, __LINE__, (uint32_t)control_p->proc_entry_p[INFRA_PROC_DIR_ROOT]);

	control_p->proc_entry_p[INFRA_PROC_DIR_PARENT] = proc_mkdir(control_p->dir_name[INFRA_PROC_DIR_PARENT], control_p->proc_entry_p[INFRA_PROC_DIR_ROOT]);
	infra_debug_trace("%s@%d DIR_PARENT:%s PROC_p:0x%x\n", __FUNCTION__, __LINE__,
				control_p->dir_name[INFRA_PROC_DIR_PARENT],
					(uint32_t)control_p->proc_entry_p[INFRA_PROC_DIR_PARENT] );

	lname = control_p->dir_name[INFRA_PROC_NAME_SELF];
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				control_p->proc_entry_p[INFRA_PROC_DIR_PARENT],
				&infra_fops,
				(void *)control_p);
	if (!lpde) {
		infra_os_free((void *)control_p);
		return NULL;
	}

	return (void *)control_p;

}


