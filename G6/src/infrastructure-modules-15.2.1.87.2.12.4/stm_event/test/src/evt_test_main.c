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
   @file     evt_test_main.c
   @brief

 */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */

#include "evt_test_subscriber.h"
#include "evt_test_signaler.h"
#include "infra_test_interface.h"
#include "infra_proc_interface.h"
/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("Bharat Jauhari");
MODULE_DESCRIPTION("TEST SUIT FOR EVENT MANAGER TESTING");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");


/*** PROTOTYPES **************************************************************/

static int evt_test_main_init_module(void);
static void evt_test_main_term_module(void);


void evt_test_modprobe_func(void)
{
	pr_info("This is INFRA's EVT TEST Module\n");
}
EXPORT_SYMBOL(evt_test_modprobe_func);


/*=============================================================================

   stm_event_main_init_modules

   Initialise the module (initialise the access routines (fops) and set the
   major number. Aquire any resources needed and setup internal structures.

  ===========================================================================*/


static int __init evt_test_main_init_module(void)
{
	/* infra_proc_create_param_t		proc_create_param; */

	pr_info("Load module evt_test_main [?]\t\tby %s (pid %i)\n",
			current->comm, current->pid);

	evt_test_signaler_entry();
	evt_test_subscribe_entry();

	return 0; /* If we get here then we have succeeded */
}


/*=============================================================================

   stm_event_main_term_module

   Realease any resources allocaed to this module before the module is
   unloaded.

  ===========================================================================*/
static void __exit evt_test_main_term_module(void)
{
	evt_test_signaler_exit();
	evt_test_subscribe_entry();

	pr_info("Unload module stm_event_main by %s (pid %i)\n",
			current->comm, current->pid);
}


/*** MODULE LOADING ******************************************************/

/* Tell the module system these are our entry points. */

module_init(evt_test_main_init_module);
module_exit(evt_test_main_term_module);
