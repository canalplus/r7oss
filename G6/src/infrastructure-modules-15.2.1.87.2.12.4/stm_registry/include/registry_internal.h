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
 @File   registry_internal.h
 @brief
*/

#ifndef _REGISTRY_INTERNAL_H
#define _REGISTRY_INTERNAL_H

#include <linux/string.h>
#include <linux/kobject.h>

#include "infra_os_wrapper.h"
#include "stm_registry.h"
#include <linux/rbtree.h>
#include "infra_debug.h"
#include "registry_debug.h"


#ifdef __cplusplus
extern "C" {
#endif


#define STM_REGISTRY_ROOT_KOBJECT     "stkpi"
#define REG_CONNECTION_SYMLINK_TAG_MAX_LEN     100
/* REG_SYSLINK_MAX_TAG_SIZE is set to size of (STM_REGISTRY_MAX_TAG_SIZE + 5)
 * to use 32 characters of the tag and is prefixed with "inst_" to add to sysfs.
*/
#define REG_SYSLINK_MAX_TAG_SIZE 37


/* for children list */
struct registrynode_ctx {
	struct registrynode *registry_p;
	struct registrynode_ctx			*node_next_np;
};

struct connection_info_s {
	stm_object_h connected_object;
	char tag_p[STM_REGISTRY_MAX_TAG_SIZE];
};

struct connection_node_s {
	struct connection_info_s connect_info;
	struct connection_node_s	    *connection_next_np;
};

struct attribute_info_s {
	void *buffer_p;
	int usize;
	char tag_p[STM_REGISTRY_MAX_TAG_SIZE];
	char datatypetag_p[STM_REGISTRY_MAX_TAG_SIZE];
};

struct attribute_node_s {
	struct attribute_info_s attribute_info;
#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	struct kobj_attribute	 reg_attr;
#endif
	struct registrynode *Registrynode;
	struct attribute_node_s *attribute_next_np;
};


struct object_info_s {
	const char *tag_p;
	stm_object_h *object_p;
};

struct objecttype_info_s {
	stm_object_h object_type_h;
	char objecttypetag_p[STM_REGISTRY_MAX_TAG_SIZE];
};

/*
  * building block of registry hierarchy . Each and every registry node is
  * represented by single registrynode.
  * As long as s_count reference is held , the regsitrynode itseld is accessible.
  *
  */
struct registrynode {
	struct registrynode				*Parent_p;
	struct rb_node					r_rbnode;
	stm_object_h				parent_h;
	stm_object_h				object_h;
	char					*tag_p;

	struct registrynode_ctx			*children_np;
	struct connection_node_s	*connection_np;
	struct attribute_node_s		 *attribute_np;
	struct objecttype_info_s	 object_type;

	infra_os_semaphore_t sem_lock;
	infra_os_semaphore_t				no_active_usr_sem_p;
#if (defined SDK2_REGISTRY_ENABLE_SYSFS_ATTRIBUTES)
	struct kobject				*kobj;
#endif
	uint32_t				connected_obj_count;
	uint32_t							usr_cnt;
	bool								deletion_started;
};

typedef enum {
	/* Flag bit relation*/
	REGISTRY_TAG = 1, /* 0000001 */
	REGISTRY_PARENT = 2, /* 0000010 */
	REGISTRY_OBJECT = 4, /* 0000100 */
	REGISTRY_CONNECTION = 8, /* 0001000 */
	REGISTRY_ATTRIBUTE = 16, /* 0010000 */
	REGISTRY_OBJECT_TYPE = 32, /* 0100000 */
}
Info_type_flag;

extern struct registrynode registry_root;


int	take_regnode_lock(struct registrynode *regnode);
int	release_regnode_lock(struct registrynode *regnode);


/*Registry internal API*/

int registry_internal_find_node(stm_object_h object, struct registrynode **Registrynode);

int registry_internal_alloc_node(struct registrynode **Registrynode);

int registry_internal_add_object(struct registrynode *Registrynode,
				 stm_object_h object,
				 stm_object_h parent);

int registry_internal_dealloc_node(struct registrynode *Registrynode);

int registry_internal_remove_object(struct registrynode *Registrynode, stm_object_h object);

int registry_internal_add_node_info(struct registrynode *Registrynode, Info_type_flag InfoFlag, void *Info_p);

int registry_internal_get_node_info(struct registrynode *Registrynode, Info_type_flag InfoFlag, void *Info_p);

int registry_internal_remove_node_info(struct registrynode *Registrynode, Info_type_flag InfoFlag, void *Info_p);

int registry_internal_clean_registry(void);


int registry_internal_fill_connection_node(struct connection_node_s **Connection_np,
					   struct connection_info_s *ConnectionInfo_p);

int registry_internal_fill_attribute_node(struct attribute_node_s **Attr_np,
					  struct attribute_info_s *Attrinfo_p,
					  struct registrynode *Registrynode);

int registry_internal_add_root(const char *tag,
			       stm_object_h object);
void registry_internal_delete_link(struct registrynode *Registrynode);

int registry_internal_create_link(struct registrynode *Registrynode_objtype,
				  struct registrynode *Registrynode);

void registry_internal_dump_registry(void);


/* to be used for deleting and adding the node from the registry link list (HeadRegistryNode_np)*/

int registry_internal_create_connection_symblink(struct registrynode *Registrynode,
						 struct connection_info_s *connectioninfo_p);
int registry_internal_del_connection_symblink(struct registrynode *Registrynode,
					      struct connection_info_s *connectioninfo_p);
void registry_intenal_string_create(char *Destination_string, char *Tag_string, stm_object_h Object_h);


int registry_internal_check_node_for_deletion(struct registrynode *Registry_p);

int registry_internal_entry_report(stm_object_h object,
				   bool do_removal,
				   struct registrynode **Registry_p);

int registry_internal_exit_report(struct registrynode *Registrynode);

int registry_internal_update_attribute(struct registrynode *Registrynode,
				    struct attribute_info_s *attribute_info);

#ifdef __cplusplus
}
#endif

#endif
