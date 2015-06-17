
#ifndef __REG_TREE_H
#define __REG_TREE_H

#include <linux/rbtree.h>
#include <registry_internal.h>

int	 registry_rbtree_insert(struct rb_root *root,
				struct registrynode *reg_node);

struct registrynode *registry_rbtree_search(struct rb_root *root,
					    stm_object_h Object);

int registry_rbtree_remove_node(struct rb_root *root,
				stm_object_h Object);

#endif /* __REG_TREE_H */

