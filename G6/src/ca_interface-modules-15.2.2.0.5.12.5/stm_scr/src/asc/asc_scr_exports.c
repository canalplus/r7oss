

/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/version.h> /* Kernel version */

#include "stm_asc_scr.h"

#define EXPORT_SYMTAB

EXPORT_SYMBOL(stm_asc_scr_new);
EXPORT_SYMBOL(stm_asc_scr_delete);
EXPORT_SYMBOL(stm_asc_scr_read);
EXPORT_SYMBOL(stm_asc_scr_write);
EXPORT_SYMBOL(stm_asc_scr_abort);
EXPORT_SYMBOL(stm_asc_scr_flush);
EXPORT_SYMBOL(stm_asc_scr_set_control);
EXPORT_SYMBOL(stm_asc_scr_get_control);
EXPORT_SYMBOL(stm_asc_scr_set_compound_control);
EXPORT_SYMBOL(stm_asc_scr_get_compound_control);
