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
 @File   registry_iterator.c
 @brief
*/

#include "registry_iterator.h"


int registry_internal_new_iterator(struct registrynode *Registrynode,
				   stm_registry_member_type_t Types,
				   stm_registry_iterator_h *Iter_p)
{
	stm_registry_iterator_h Iterator_p = (stm_registry_iterator_h) infra_os_malloc(sizeof(stm_registry_iterator_t));

	if (Iterator_p == NULL)
		return -ENOMEM;

	Iterator_p->object_h = Registrynode->object_h;

	if (Types & STM_REGISTRY_MEMBER_TYPE_OBJECT)
		Iterator_p->children_np = Registrynode->children_np;
	else
		Iterator_p->children_np = NULL;


	if (Types & STM_REGISTRY_MEMBER_TYPE_ATTRIBUTE)
		Iterator_p->attribute_np = Registrynode->attribute_np;
	else
		Iterator_p->attribute_np = NULL;


	if (Types & STM_REGISTRY_MEMBER_TYPE_CONNECTION)
		Iterator_p->connection_np = Registrynode->connection_np;
	else
		Iterator_p->connection_np = NULL;

	Iterator_p->registry_node_p = Registrynode;
	*Iter_p = Iterator_p;

	return 0;

}
int registry_internal_delete_iterator(stm_registry_iterator_h Iter)
{
	if (Iter == NULL)
		return -EINVAL;
	infra_os_free(Iter);
	Iter = NULL;
	return 0;
}

int registry_internal_get_next_iterator(stm_registry_iterator_h Iter,
					char *Tag,
					stm_registry_member_type_t *Child_type)
{
	int		err = 0;
	bool		tagfound = false;

	if (Iter->children_np) {
		strlcpy(Iter->tag,
			      Iter->children_np->registry_p->tag_p,
			      STM_REGISTRY_MAX_TAG_SIZE);

		*Child_type = STM_REGISTRY_MEMBER_TYPE_OBJECT;
		Iter->children_np = Iter->children_np->node_next_np;
		tagfound = true;
		goto found;
	}

	if (Iter->attribute_np) {
		strlcpy(Iter->tag,
			      Iter->attribute_np->attribute_info.tag_p,
			      (STM_REGISTRY_MAX_TAG_SIZE));

		Iter->attribute_np = Iter->attribute_np->attribute_next_np;

		*Child_type = STM_REGISTRY_MEMBER_TYPE_ATTRIBUTE;
		tagfound = true;
		goto found;
	}

	if (Iter->connection_np) {
		strlcpy(Iter->tag,
			      Iter->connection_np->connect_info.tag_p,
			      STM_REGISTRY_MAX_TAG_SIZE);

		Iter->connection_np = Iter->connection_np->connection_next_np;

		*Child_type = STM_REGISTRY_MEMBER_TYPE_CONNECTION;
		tagfound = true;
		goto found;
	}

found:
	if (tagfound)
		strlcpy(Tag, Iter->tag, STM_REGISTRY_MAX_TAG_SIZE);
	else
		err = -ENODEV;

	return err;
}
