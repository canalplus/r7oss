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
   @file     stttx_core_main.c
   @brief

 */

#ifndef INFRA_PROC_INTERFACE_H
#define INFRA_PROC_INTERFACE_H

#include <linux/proc_fs.h>
#include <linux/errno.h>   /* Defines standard error codes */


#define INFRA_MAX_PROC_DIR_NAME		20

typedef struct proc_dir_entry*		infra_proc_entry_t;
typedef char infra_proc_dir_name_t[INFRA_MAX_PROC_DIR_NAME];

typedef enum infra_proc_dir_name_id_s{
	INFRA_PROC_DIR_ROOT,
	INFRA_PROC_DIR_PARENT,
	INFRA_PROC_NAME_SELF,
	INFRA_MAX_PROC_DIR_ID
}infra_proc_dir_name_id_t;


typedef struct infra_proc_msg_interface_s{
	void		*data_p;
	/*pointer to the mesage received or to be sent to the user*/
	uint8_t		*msg_p;
	/*length of the message*/
	uint32_t		msg_len;
}infra_proc_msg_interface_t;

typedef int	(*infra_proc_read_fp)(infra_proc_msg_interface_t *);
typedef void	(*infra_proc_cmd_fp)(infra_proc_msg_interface_t *);

typedef struct infra_proc_create_param_s{
	/*name of the directories for the procfs tree*/
	infra_proc_dir_name_t		dir_name[INFRA_MAX_PROC_DIR_ID];
	/*Size of the data buffer expected from the user*/
	uint32_t				msg_buffer_max_size;
	/*Function pointer of the read function. Will be called, when the user will read teh proc*/
	infra_proc_read_fp			read_fp;
	/*Function pointer of the write function. Will be called, when the user will write to the proc*/
	infra_proc_cmd_fp			write_fp;
}infra_proc_create_param_t;


int	infra_init_procfs_module(void);
void	infra_term_procfs_module(void);


void*	infra_create_proc_entry(infra_proc_create_param_t *);
int	infra_remove_proc_entry(void *);

int infra_proc_write_interface(struct file *file,
			const char __user *buffer,
			size_t       len,
			loff_t      *offset);

int infra_proc_read_interface(struct file *file,
			char __user *buffer,
			size_t       len,
			loff_t      *offset);




#endif /*INFRA_PROC_INTERFACE_H*/
