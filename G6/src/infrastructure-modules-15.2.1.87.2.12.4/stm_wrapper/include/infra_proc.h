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

#ifndef INFRA_PROC_H_
#define INFRA_PROC_H_

#include "infra_proc_interface.h"

typedef struct infra_proc_control_param_t{
	uint32_t				max_dir_level;
	infra_proc_dir_name_t		dir_name[INFRA_MAX_PROC_DIR_ID];
	infra_proc_entry_t			proc_entry_p[INFRA_MAX_PROC_DIR_ID];
	/*pointer to the mesage received from the user will be copied*/
	uint8_t				*msg_buffer_p;
	uint32_t				msg_max_buffer_size;
	infra_proc_read_fp			read_fp;
	infra_proc_cmd_fp			write_fp;
	uint32_t				handle;
}infra_proc_control_param_t;

int	infra_term_this_proc(void* );
void 	*infra_init_this_proc(infra_proc_create_param_t*, infra_proc_entry_t);

#endif
