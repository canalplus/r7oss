/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided “AS IS”, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   registry_sysfs.h
 @brief
*/

#ifndef _REGISTRY_SYSFS_H
#define _REGISTRY_SYSFS_H

#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
#include <linux/kobject.h>

#ifdef __cplusplus
extern "C" {
#endif


int registry_sysfs_add_attribute(struct attribute_node_s *attr_np,
				 struct kobject *kobj,
				 char *name);

int registry_sysfs_add_symblink(struct kobject *kobj,
				struct kobject *target,
				char *name);

int registry_sysfs_add_kobject(char *kobject_name,
			       struct kobject *parent_kobject,
			       struct kobject **registry_kobject);

void registry_sysfs_remove_attribute(struct kobject *kobj,
				     char *name);

void registry_sysfs_remove_symblink(struct kobject *kobj,
				    char *name);

void registry_sysfs_remove_kobject(struct kobject **reg_kobject);

int registry_sysfs_move_kobject(struct kobject *kobj,
				struct kobject *new_parent);

#ifdef __cplusplus
}
#endif

#endif
#endif
