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
   @file     infra_exports.c
   @brief    Implementation for exporting Event Manager's functions

 */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/version.h> /* Kernel version */

#include "infra_os_wrapper.h"
#include "infra_queue.h"
#include "infra_proc_interface.h"

#define EXPORT_SYMTAB

EXPORT_SYMBOL(infra_os_malloc_d);
EXPORT_SYMBOL(infra_os_free_d);
EXPORT_SYMBOL(infra_os_copy);

EXPORT_SYMBOL(infra_os_invalidate_cache_range);
EXPORT_SYMBOL(infra_os_flush_cache_range);
EXPORT_SYMBOL(infra_os_purge_cache_range);
EXPORT_SYMBOL(infra_os_peripheral_address);

EXPORT_SYMBOL(infra_os_sema_initialize);
EXPORT_SYMBOL(infra_os_sema_terminate);
EXPORT_SYMBOL(infra_os_sema_wait);
EXPORT_SYMBOL(infra_os_sema_wait_timeout);
EXPORT_SYMBOL(infra_os_sema_signal);

EXPORT_SYMBOL(infra_os_hrt_sema_initialize);
EXPORT_SYMBOL(infra_os_hrt_sema_terminate);
EXPORT_SYMBOL(infra_os_hrt_sema_wait);
EXPORT_SYMBOL(infra_os_hrt_sema_wait_timeout);
EXPORT_SYMBOL(infra_os_hrt_sema_signal);

EXPORT_SYMBOL(infra_os_mutex_initialize);
EXPORT_SYMBOL(infra_os_mutex_terminate);
EXPORT_SYMBOL(infra_os_mutex_lock);
EXPORT_SYMBOL(infra_os_mutex_unlock);

EXPORT_SYMBOL(infra_os_wait_queue_wakeup);
EXPORT_SYMBOL(infra_os_wait_queue_initialize);
EXPORT_SYMBOL(infra_os_wait_queue_reinitialize);
EXPORT_SYMBOL(infra_os_wait_queue_deinitialize);

EXPORT_SYMBOL(infra_os_thread_create);
EXPORT_SYMBOL(infra_os_thread_terminate);
EXPORT_SYMBOL(infra_os_wait_thread);
EXPORT_SYMBOL(infra_os_task_exit);
EXPORT_SYMBOL(infra_os_should_task_exit);

EXPORT_SYMBOL(infra_os_get_time_in_sec);
EXPORT_SYMBOL(infra_os_get_time_in_milli_sec);
EXPORT_SYMBOL(infra_os_sleep_milli_sec);
EXPORT_SYMBOL(infra_os_sleep_usec);
EXPORT_SYMBOL(infra_os_time_now);

EXPORT_SYMBOL(infra_q_remove_node);
EXPORT_SYMBOL(infra_q_insert_node);
EXPORT_SYMBOL(infra_q_search_node);
EXPORT_SYMBOL(infra_os_message_q_initialize);
EXPORT_SYMBOL(infra_os_message_q_terminate);
EXPORT_SYMBOL(infra_os_message_q_send);
EXPORT_SYMBOL(infra_os_message_q_receive);

EXPORT_SYMBOL(infra_create_proc_entry);
EXPORT_SYMBOL(infra_remove_proc_entry);
