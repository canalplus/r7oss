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
 *  @File   registry_tree.c
 *   @brief
 *   */

#include "registry_rbtree.h"
#include "registry_debug.h"

int registry_rbtree_insert(struct rb_root *root, struct registrynode *reg_node)

{
	struct rb_node **new_node = &root->rb_node;
	struct rb_node *parent_node = NULL;
	struct registrynode *this = NULL;

	REGISTRY_DEBUG_MSG("root (%x) , registrynode (%x)\n", (uint32_t) root, (uint32_t) reg_node);
	while (*new_node) {
		parent_node = *new_node;
		/*#define rb_entry(ptr, type, member) container_of(ptr, type, member)*/
		this = rb_entry(*new_node, struct registrynode, r_rbnode);

		if (reg_node->object_h < this->object_h)
			new_node = &(*new_node)->rb_left;
		else if (reg_node->object_h >= this->object_h)
			new_node = &(*new_node)->rb_right;
		else
			return false;
	}

	rb_link_node(&reg_node->r_rbnode, parent_node, new_node);
	rb_insert_color(&reg_node->r_rbnode, root);
	return true;
}



struct registrynode *registry_rbtree_search(struct rb_root *root, stm_object_h Object)
{


	struct rb_node *node = root->rb_node;
	struct registrynode *registry_entry;

	REGISTRY_DEBUG_MSG("root : %x , Object : %x", (uint32_t) root, (uint32_t) Object);
	while (node) {
		registry_entry = rb_entry(node, struct registrynode, r_rbnode);

		if (Object < registry_entry->object_h)
			node = node->rb_left;
		else if (Object > registry_entry->object_h)
			node = node->rb_right;
		else
			return registry_entry;
	}

	/* object not found in rbtree ; return NULL */
	return NULL;
}


int registry_rbtree_remove_node(struct rb_root *root, stm_object_h Object)
{
	struct registrynode *registry_entry;
	int err = 0;

	REGISTRY_DEBUG_MSG("Object is %x\n", (uint32_t) Object);

	registry_entry = registry_rbtree_search(root, Object);

	if (registry_entry) {

		REGISTRY_DEBUG_MSG("Erasing rb node of registry %x , Object %x\n", (uint32_t) registry_entry, (uint32_t) Object);
		rb_erase(&registry_entry->r_rbnode, root);
	} else {
		err = -ENODEV;
		return err;
	}

	return 0;
}



