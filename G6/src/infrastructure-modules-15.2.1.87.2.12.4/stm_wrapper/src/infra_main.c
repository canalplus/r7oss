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
   @file     infra_main.c
   @brief

 */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */

#include "infra_queue.h"
#include "infra_os_wrapper.h"
#include "infra_proc_interface.h"

/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("Bharat Jauhari");
MODULE_DESCRIPTION("OS WRAPPER FOR INFRA MODULE");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");


/*** PROTOTYPES **************************************************************/

static int  infra_main_entry_module(void);
static void infra_main_exit_module(void);

/*** MODULE PARAMETERS *******************************************************/

/*** GLOBAL VARIABLES *********************************************************/


/*** EXPORTED SYMBOLS ********************************************************/

/*** LOCAL TYPES *********************************************************/


/*** METHODS ****************************************************************/

/*=============================================================================

   infra_main_entry_module

   Initialise the module (initialise the access routines (fops) and set the
   major number. Aquire any resources needed and setup internal structures.

  ===========================================================================*/
static int __init infra_main_entry_module(void)
{
	pr_info("Load module infra_main [?]\t\tby %s (pid %i)\n", current->comm,
			current->pid);
	infra_init_procfs_module();
	return (0);  /* If we get here then we have succeeded */
}


/*=============================================================================

   infra_main_exit_module

   Realease any resources allocaed to this module before the module is
   unloaded.

  ===========================================================================*/
static void __exit infra_main_exit_module(void)
{
	infra_term_procfs_module();
	pr_info("Unload module infra_os_wrapper by %s (pid %i)\n",
			current->comm, current->pid);
}

/*** MODULE LOADING ******************************************************/

/* Tell the module system these are our entry points. */

module_init(infra_main_entry_module);
module_exit(infra_main_exit_module);
