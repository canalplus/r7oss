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
 @File   registry_iterator.h
 @brief
*/

#ifndef _REGISTRY_ITERATOR_H
#define _REGISTRY_ITERATOR_H

#include "registry_internal.h"
#include "stm_registry.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct stm_registry_iterator_s {
	stm_object_h			object_h;
	struct registrynode_ctx *children_np;
	struct connection_node_s *connection_np;
	struct attribute_node_s *attribute_np;
	char tag[STM_REGISTRY_MAX_TAG_SIZE];
	struct registrynode					*registry_node_p;
} stm_registry_iterator_t;



int registry_internal_new_iterator(struct registrynode *Registrynode,
				   stm_registry_member_type_t types,
				   stm_registry_iterator_h *p_iter);
int registry_internal_delete_iterator(stm_registry_iterator_h iter);

int registry_internal_get_next_iterator(stm_registry_iterator_h iter,
					char *tag,
					stm_registry_member_type_t *p_child_type);

#ifdef __cplusplus
}
#endif

#endif
