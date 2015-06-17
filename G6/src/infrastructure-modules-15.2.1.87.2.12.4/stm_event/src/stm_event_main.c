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
   @file     stm_event_main.c
   @brief

 */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/debugfs.h>

#include "event_signaler.h"
#include "event_subscriber.h"
#include "evt_signal_async.h"
#include "event_debugfs.h"

/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("Bharat Jauhari");
MODULE_DESCRIPTION("MANAGES ALL THE EVENT FLOW");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");


/*** PROTOTYPES **************************************************************/

static int stm_event_main_init_module(void);
static void stm_event_main_term_module(void);


/**************************************************************************
   stm_event_main_init_modules

   Initialise the module (initialise the access routines (fops) and set the
   major number. Aquire any resources needed and setup internal structures.
***************************************************************************/
static int __init stm_event_main_init_module(void)
{
	evt_mng_subscriber_control_t		*subscriber_control_p;
	evt_mng_signaler_control_t		*signaler_control_p;

	pr_info("Load module stm_event_main [?]\t\tby %s (pid %i)\n", current->comm, current->pid);

	subscriber_control_p = (evt_mng_subscriber_control_t *) infra_os_malloc(sizeof(*subscriber_control_p));
	if (NULL == subscriber_control_p) {
		pr_err("Allocation for Subscriber param failed\n");
		return -1;
	}

	signaler_control_p = (evt_mng_signaler_control_t *) infra_os_malloc(sizeof(*signaler_control_p));
	if (NULL == signaler_control_p) {
		pr_err("Allocation for Signaler param failed\n");
		infra_os_free((void *) subscriber_control_p);
		return -1;
	}

	rwlock_init(&(subscriber_control_p->lock_subscriber_q));
	infra_os_rwlock_init(&(signaler_control_p->lock_sig_q_param_p));


	evt_async_signaler_init();
	evt_mng_set_subscriber_control_param(subscriber_control_p);
	evt_mng_set_signaler_control_param(signaler_control_p);

#if (defined SDK2_EVENT_ENABLE_DEBUGFS_STATISTICS)
	event_create_debugfs();
#endif

	return 0; /* If we get here then we have succeeded */
}


/**************************************************************************
   stm_event_main_term_module

   Realease any resources allocaed to this module before the module is
   unloaded.
***************************************************************************/
static void __exit stm_event_main_term_module(void)
{
	evt_mng_subscriber_control_t		*subscriber_control_p;
	evt_mng_signaler_control_t		*signaler_control_p;

	subscriber_control_p = evt_mng_get_subscriber_control_param();
	signaler_control_p = evt_mng_get_signaler_control_param();

	subscriber_control_p->subscriber_term_fp((void *) subscriber_control_p);
	evt_async_signaler_term();

	infra_os_free((void *) subscriber_control_p);
	infra_os_free((void *) signaler_control_p);

#if (defined SDK2_EVENT_ENABLE_DEBUGFS_STATISTICS)
	event_remove_debugfs();
#endif

	pr_info("Unload module stm_event_main by %s (pid %i)\n", current->comm, current->pid);
}

/*** MODULE LOADING ******************************************************/

/* Tell the module system these are our entry points. */

module_init(stm_event_main_init_module);
module_exit(stm_event_main_term_module);
