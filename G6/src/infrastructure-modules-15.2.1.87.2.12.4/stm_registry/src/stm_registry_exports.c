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
 *    @file     stm_registry_exports.c
 *       @brief    Implementation for exporting Registry Export's functions
 *
 *        */


/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/version.h> /* Kernel version */

#include "stm_registry.h"


#define EXPORT_SYMTAB

EXPORT_SYMBOL(stm_registry_add_object);
EXPORT_SYMBOL(stm_registry_add_instance);
EXPORT_SYMBOL(stm_registry_remove_object);
EXPORT_SYMBOL(stm_registry_get_object_tag);
EXPORT_SYMBOL(stm_registry_get_object);
EXPORT_SYMBOL(stm_registry_get_object_parent);
EXPORT_SYMBOL(stm_registry_get_object_type);
EXPORT_SYMBOL(stm_registry_add_attribute);
EXPORT_SYMBOL(stm_registry_update_attribute);
EXPORT_SYMBOL(stm_registry_get_attribute);
EXPORT_SYMBOL(stm_registry_remove_attribute);
EXPORT_SYMBOL(stm_registry_add_connection);
EXPORT_SYMBOL(stm_registry_get_connection);
EXPORT_SYMBOL(stm_registry_remove_connection);
EXPORT_SYMBOL(stm_registry_add_data_type);
EXPORT_SYMBOL(stm_registry_get_data_type);
EXPORT_SYMBOL(stm_registry_remove_data_type);
EXPORT_SYMBOL(stm_registry_new_iterator);
EXPORT_SYMBOL(stm_registry_delete_iterator);
EXPORT_SYMBOL(stm_registry_iterator_get_next);
EXPORT_SYMBOL(stm_registry_dumptheregistry);
EXPORT_SYMBOL(stm_registry_types);
EXPORT_SYMBOL(stm_registry_instances);

