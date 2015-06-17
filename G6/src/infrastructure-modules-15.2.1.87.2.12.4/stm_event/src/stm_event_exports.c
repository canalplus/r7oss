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
   @file     stm_event_exports.c
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

#include "stm_event.h"


#define EXPORT_SYMTAB

EXPORT_SYMBOL(stm_event_subscription_create);
EXPORT_SYMBOL(stm_event_subscription_delete);
EXPORT_SYMBOL(stm_event_subscription_modify);
EXPORT_SYMBOL(stm_event_wait);
EXPORT_SYMBOL(stm_event_set_handler);
EXPORT_SYMBOL(stm_event_get_handler);
EXPORT_SYMBOL(stm_event_set_wait_queue);
EXPORT_SYMBOL(stm_event_get_wait_queue);
EXPORT_SYMBOL(stm_event_signal);
