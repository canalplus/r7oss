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
 @File   registry_sysfs.c
 @brief
*/
#include <linux/slab.h>

#include "registry_internal.h"
#include "registry_sysfs.h"
#include "registry_datatype.h"
#include "registry_debug.h"

#define to_regattribute_obj(x) container_of(x, struct attribute_node_s, reg_attr)

#define SYSFS_MODE 0644

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#define REG_SYSFS_ATTR_INIT(attr)	sysfs_attr_init(attr)
#else
#define REG_SYSFS_ATTR_INIT(attr)	do {} while (0)
#endif


static int reg_sysfs_get_kobj_handler(struct kobj_attribute *attr,
				      struct attribute_node_s *attr_np,
				      stm_registry_type_def_t *def_p);



static int reg_sysfs_get_kobj_handler(struct kobj_attribute *attr,
				      struct attribute_node_s *attr_np,
				      stm_registry_type_def_t *def_p)
{
	int err = 0;

	/*Check if the its a valid tag*/
	if (strcmp(attr_np->attribute_info.tag_p, attr->attr.name) != 0)
		return -ENODEV;

	/* get the store handler for data type link list*/
	err = registry_internal_get_data_type(
		      attr_np->attribute_info.datatypetag_p,
		      def_p);
	return err;
}


ssize_t kobject_attr_show(struct kobject *kobj,
			  struct kobj_attribute *attr,
			  char *buf)
{
	int err = 0;
	struct attribute_node_s *attr_np;
	stm_registry_type_def_t def;

	/* To get the attribute node of registry for the attribute*/
	attr_np = to_regattribute_obj(attr);
	err = reg_sysfs_get_kobj_handler(attr, attr_np, &def);

	REGISTRY_DEBUG_MSG("\nattri.usize : %d, attribute_info.buffer_p : %d\n",
		      attr_np->attribute_info.usize,
		      *((int32_t *) attr_np->attribute_info.buffer_p));

	if (err == 0 && def.print_handler != NULL) {
		return def.print_handler(attr_np->Registrynode->object_h,
			      attr_np->attribute_info.buffer_p,
			      attr_np->attribute_info.usize,
			      buf,
			      PAGE_SIZE);
	}

	return err;
}

/*
 * Just like the default show function above, but this one is for when the
 * sysfs "store" is requested (when a value is written to a file.)
 */
ssize_t kobject_attr_store(struct	kobject *kobj,
			   struct	kobj_attribute *attr,
			   const char	*buf,
			   size_t	len)
{
	int err = 0;
	struct attribute_node_s	*attr_np;
	stm_registry_type_def_t	def;

	/* To get the attribute node of registry for the attribute*/
	attr_np = to_regattribute_obj(attr);
	err = reg_sysfs_get_kobj_handler(attr, attr_np, &def);

	REGISTRY_DEBUG_MSG("\nBuffer Len : %d, attribute_info.usize:%d\n",
		      len,
		      attr_np->attribute_info.usize);

	/*If no error then we have a handler stored with registry */
	if (err == 0 && def.store_handler != NULL) {
		return def.store_handler(attr_np->Registrynode->object_h,
			      attr_np->attribute_info.buffer_p,
			      attr_np->attribute_info.usize,
			      buf,
			      len);
	}

	return err;
}

int registry_sysfs_add_kobject(char *Kobject_name,
			       struct kobject *Parent_kobject,
			       struct kobject **Registry_kobject)
{
	struct kobject *registry_kobj;

	REGISTRY_DEBUG_MSG("\n");
	registry_kobj = kobject_create_and_add(Kobject_name, Parent_kobject);

	if (!registry_kobj)
		return -ENOMEM;

	*Registry_kobject = registry_kobj;
	return 0;
}

void registry_sysfs_remove_kobject(struct kobject **Reg_kobject)
{
	struct kobject *kobj;
	REGISTRY_DEBUG_MSG("\n");
	kobj = *Reg_kobject;

	kobject_put(kobj);
	*Reg_kobject = kobj;
}

int registry_sysfs_add_symblink(struct kobject *Kobj,
				struct kobject *Target,
				char *Name)
{
	REGISTRY_DEBUG_MSG("\n");
	return sysfs_create_link(Kobj, Target, Name); /*when OK return 0*/
}
void registry_sysfs_remove_symblink(struct kobject *Kobj,
				    char *Name)
{
	REGISTRY_DEBUG_MSG("\n");
	sysfs_remove_link(Kobj, Name);
}

int registry_sysfs_add_attribute(struct attribute_node_s *Attr_np,
				 struct kobject *Kobj,
				 char *Name)
{
	REGISTRY_DEBUG_MSG("\n");
	Attr_np->reg_attr.attr.name = Name;
	Attr_np->reg_attr.attr.mode = SYSFS_MODE;
	Attr_np->reg_attr.show = kobject_attr_show;
	Attr_np->reg_attr.store = kobject_attr_store;
	REG_SYSFS_ATTR_INIT(&(Attr_np->reg_attr.attr));
	return sysfs_create_file(Kobj, &(Attr_np->reg_attr.attr)); /*when OK return 0*/
}

void registry_sysfs_remove_attribute(struct kobject *Kobj, char *Name)
{
	struct attribute attr;

	REGISTRY_DEBUG_MSG("\n");
	attr.name = Name;
	attr.mode = SYSFS_MODE;
	sysfs_remove_file(Kobj, &attr);
}

int registry_sysfs_move_kobject(struct kobject *Kobj,
				struct kobject *New_parent)
{
	REGISTRY_DEBUG_MSG("\n");
	/*Basically, move not supported so do nothing*/
	return 0;
}
