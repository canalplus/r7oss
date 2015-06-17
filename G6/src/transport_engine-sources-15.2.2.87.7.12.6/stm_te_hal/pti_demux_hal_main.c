/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided "AS IS", WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>		/* Initialisation support */
#include <linux/module.h>	/* Module support */
#include <linux/kernel.h>	/* Kernel support */

#include "pti_driver.h"
#include "stm_te_version.h"
#include "config/pti_platform.h"

/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("");
MODULE_DESCRIPTION("Transport Engine HAL");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");
MODULE_VERSION(TE_VERSION);

/*** PROTOTYPES **************************************************************/

static int demux_hal_init_module(void);
static void demux_hal_cleanup_module(void);
int stptiHAL_debug_init(void);
void stptiHAL_debug_term(void);

/*** MODULE PARAMETERS *******************************************************/

/*** GLOBAL VARIABLES *********************************************************/

/*** EXPORTED SYMBOLS ********************************************************/

/*** LOCAL TYPES *********************************************************/

/*** LOCAL CONSTANTS ********************************************************/

/*** LOCAL VARIABLES *********************************************************/

/*** METHODS ****************************************************************/

#ifdef DVD_STPTI5_STUB
STIPRC_DISPATCH_DECLARE(STPTI_Dispatch);
#endif

/*=============================================================================

   stpti5_core_init_module

   Initialise the module (initialise the access routines (fops) and set the
   major number. Aquire any resources needed and setup internal structures.

  ===========================================================================*/
static int __init demux_hal_init_module(void)
{
	int err;

	/* Check if DT has registered tango devices */
	err = stptiDriver_dt_check();
	if (err){
		pr_warning("No DT entry found for TE devices\n");
		return err;
	}


	err = stptiHAL_debug_init();
	if (err)
		return err;

	err = stptiAPI_register_drivers();
	if (err)
		goto error_driver_reg;

	return 0;

error_driver_reg:
	stptiHAL_debug_term();

	return err;
}

/*=============================================================================

   stpti5_core_cleanup_module

   Realease any resources allocaed to this module before the module is
   unloaded.

  ===========================================================================*/
static void __exit demux_hal_cleanup_module(void)
{
	stptiAPI_unregister_drivers();
	stptiHAL_debug_term();
}

/*** MODULE LOADING ******************************************************/

/* Tell the module system these are our entry points. */

module_init(demux_hal_init_module);
module_exit(demux_hal_cleanup_module);
