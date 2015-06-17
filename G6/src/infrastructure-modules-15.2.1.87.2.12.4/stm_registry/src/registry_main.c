/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   registry_main.c
 @brief
*/

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */

#include "registry_internal.h"
#include "registry_datatype.h"
#include "stm_registry.h"
#include "registry_sysfs.h"
#include "registry_debugfs.h"
#include "registry_rbtree.h"

/* Module infos */

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("Registry for STKPI");
MODULE_LICENSE("GPL");

struct rb_root registry_tree = RB_ROOT;
struct registrynode *registry_p;


registry_datatype_t *HeadDataType_np;
registry_datatype_t *HeadDataType_removable_np;

infra_os_semaphore_t g_registry_sem_lock;
infra_os_semaphore_t g_registry_data_sem_lock;

stm_object_h stm_registry_types = (void *)0xCDCDCDCD; /* Dummy values for roots*/
stm_object_h stm_registry_instances = (void *)0xABABABAB;

static int __init registry_init_module(void)
{
	int retval;

	REGISTRY_DEBUG_MSG("registry_init_module\n");
	infra_os_sema_initialize(&g_registry_sem_lock, 1);
	infra_os_sema_initialize(&g_registry_data_sem_lock, 1);

	registry_p = (struct registrynode *) infra_os_malloc(sizeof(struct registrynode));

	if (registry_p == NULL)
		return -ENOMEM;


	registry_p->tag_p = (char *) infra_os_malloc(STM_REGISTRY_MAX_TAG_SIZE);

	/*same name as the root object at /linux/kernel*/
	strlcpy(registry_p->tag_p, STM_REGISTRY_ROOT_KOBJECT, STM_REGISTRY_MAX_TAG_SIZE);

	registry_p->object_h = STM_REGISTRY_ROOT;
	registry_p->attribute_np = NULL;
	registry_p->children_np = NULL;
	registry_p->connection_np = NULL;
	registry_p->object_type.object_type_h = NULL;
	registry_p->Parent_p = NULL;
	registry_p->parent_h = NULL;
	infra_os_sema_initialize(&registry_p->sem_lock, 1);

	registry_rbtree_insert(&registry_tree, registry_p);

	/* Add the root kobject under /sys/kernel */

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	if (registry_sysfs_add_kobject(STM_REGISTRY_ROOT_KOBJECT, kernel_kobj,
		&registry_p->kobj) != 0) {
		return -1;
	}
#endif
	if (registry_internal_add_root("instance", stm_registry_instances) != 0)
		return -1;

	if (registry_internal_add_root("type", stm_registry_types) != 0)
		return -1;



	/* create the head node for the data type link list*/
	HeadDataType_np = (registry_datatype_t *) infra_os_malloc(sizeof(registry_datatype_t));
	if (!HeadDataType_np) {
		infra_os_free(registry_p);
		return -ENODEV;
	}

	HeadDataType_np->datatype_next_np = NULL;

	/* Add default data type for registry*/
	retval = registry_internal_add_default_data_type();
	if (retval != 0)
		return -1;


#if (defined SDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES)
	/* create debugfs entry */
	registry_create_debugfs();
#endif
	return 0;
}




/* we atill need to clean up the exit module */
/* TO DO after the initial testing*/
static void __exit registry_cleanup_module(void)
{
	int retval;


	REGISTRY_DEBUG_MSG("cleanup module\n");

	retval = registry_internal_clean_registry();
	if (retval != 0)
		pr_err("registry_internal_clean_registry failed(%d)\n", retval);

	infra_os_sema_terminate(registry_p->sem_lock);
	registry_rbtree_remove_node(&registry_tree, registry_p->object_h);

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	/* Remove the root kobject under /sys/kernel */
	registry_sysfs_remove_kobject(&registry_p->kobj); /* this funcion return void*/
#endif

	infra_os_free(registry_p->tag_p);
	infra_os_free(registry_p);

	/* Remove default data type for registry*/
	registry_internal_remove_default_data_type();
	infra_os_free(HeadDataType_np);
	HeadDataType_np = NULL;
	HeadDataType_removable_np = NULL;

#if (defined SDK2_REGISTRY_ENABLE_DEBUGFS_ATTRIBUTES)
	registry_remove_debugfs();
#endif
	infra_os_sema_terminate(g_registry_sem_lock);
	infra_os_sema_terminate(g_registry_data_sem_lock);

}

module_init(registry_init_module);
module_exit(registry_cleanup_module);
