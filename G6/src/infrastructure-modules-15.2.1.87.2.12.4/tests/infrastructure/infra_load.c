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
   @file     infra_test_main.c
   @brief

 */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */

#include "infra_os_wrapper.h"
#include "registry_test.h"
#include "cec_test.h"
#include "evt_test.h"
#include "mss_test.h"
/*
#include "stm_event.h"
#include "stm_registry.h"
#include "stm_memsrc.h"
#include "stm_memsink.h"
*/
#include "infra_platform.h"

/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("Bharat Jauhari");
MODULE_DESCRIPTION("TEST SUIT FOR EVENT MANAGER TESTING");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");


/*** PROTOTYPES **************************************************************/

static int  init_infra_load_module(void);
static void term_infra_load_module(void);



/*=============================================================================

   stm_event_main_init_modules

   Initialise the module (initialise the access routines (fops) and set the
   major number. Aquire any resources needed and setup internal structures.

  ===========================================================================*/


static int __init init_infra_load_module(void)
{
	pr_info("Load module infra_loader [?]\t\tby %s (pid %i)\n",
			current->comm, current->pid);

	registry_test_modprobe_func();
	evt_test_modprobe_func();
	mss_test_modprobe_func();
/*	cec_test_modprobe_func(); */
	return (0);  /* If we get here then we have succeeded */
}


/*=============================================================================

   stm_event_main_term_module

   Realease any resources allocaed to this module before the module is
   unloaded.

  ===========================================================================*/
static void __exit term_infra_load_module(void)
{
	pr_info("Unload module infra_loader by %s (pid %i)\n", current->comm,
			current->pid);
}


/*** MODULE LOADING ******************************************************/

/* Tell the module system these are our entry points. */

module_init(init_infra_load_module);
module_exit(term_infra_load_module);
