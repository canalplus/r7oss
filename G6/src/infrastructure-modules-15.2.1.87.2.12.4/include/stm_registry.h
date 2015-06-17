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
/*
 @File   stm_registry.h
 @brief
*/

#include "stm_common.h"

#ifndef _STM_REGISTRY_H
#define _STM_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#define STM_REGISTRY_ROOT     ((stm_object_h)0)

extern stm_object_h stm_registry_types;
#define STM_REGISTRY_TYPES  stm_registry_types

extern stm_object_h stm_registry_instances;
#define STM_REGISTRY_INSTANCES	stm_registry_instances

#define STM_REGISTRY_MAX_TAG_SIZE   32


#define STM_REGISTRY_INT32     "int32"
#define STM_REGISTRY_UINT32    "uint32"
#define STM_REGISTRY_ADDRESS   "address"

typedef enum stm_registry_member_type_e {
	STM_REGISTRY_MEMBER_TYPE_NONE = 0,
	STM_REGISTRY_MEMBER_TYPE_OBJECT = 0x1,
	STM_REGISTRY_MEMBER_TYPE_ATTRIBUTE = 0x2,
	STM_REGISTRY_MEMBER_TYPE_CONNECTION = 0x4,
	STM_REGISTRY_MEMBER_TYPE_ALL = 0x7,
} stm_registry_member_type_t;

typedef struct {
	int (*print_handler)(
			     stm_object_h object,
			     char *buf,
			     size_t size,
			     char *user_buf,
			     size_t user_size);
	int (*store_handler)(
			     stm_object_h object,
			     char *buf,
			     size_t size,
			     const char *user_buf,
			     size_t user_size);
} stm_registry_type_def_t;

typedef struct stm_registry_iterator_s *stm_registry_iterator_h;


int __must_check stm_registry_add_object(stm_object_h parent,
					 const char *tag,
					 stm_object_h object);

int __must_check stm_registry_add_instance(stm_object_h parent,
					   stm_object_h type_object,
					   const char *tag,
					   stm_object_h object);
int __must_check stm_registry_remove_object(stm_object_h object);

int __must_check stm_registry_get_object_tag(stm_object_h object,
					     const char *tag);

int __must_check stm_registry_get_object(stm_object_h parent,
					 const char *tag,
					 stm_object_h *p_object);


int __must_check stm_registry_get_object_parent(stm_object_h object,
						stm_object_h *p_parent);

int __must_check stm_registry_get_object_type(stm_object_h object,
					      stm_object_h *p_type_object);


int __must_check stm_registry_add_attribute(stm_object_h object,
					    const char *tag,
					    const char *type_tag,
					    void *buffer,
					    int size);

int __must_check stm_registry_get_attribute(stm_object_h object,
					    const char *tag,
					    char *type_tag,
					    int buffer_size,
					    void *buffer,
					    int *p_actual_size);


int __must_check stm_registry_remove_attribute(stm_object_h object,
					       const char *tag);


int __must_check stm_registry_add_connection(stm_object_h object,
					     const char *tag,
					     stm_object_h connected_object);

int __must_check stm_registry_get_connection(stm_object_h object,
					     const char *tag,
					     stm_object_h *p_connected_object);

int __must_check stm_registry_remove_connection(stm_object_h object,
						const char *tag);

int __must_check stm_registry_add_data_type(const char *tag,
					    stm_registry_type_def_t *def);



int __must_check stm_registry_get_data_type(const char *tag,
					    stm_registry_type_def_t *def);


int __must_check stm_registry_remove_data_type(const char *tag);


int __must_check stm_registry_new_iterator(stm_object_h object,
					   stm_registry_member_type_t types,
					   stm_registry_iterator_h *p_iter);


int __must_check stm_registry_delete_iterator(stm_registry_iterator_h iter);


int __must_check stm_registry_iterator_get_next(stm_registry_iterator_h iter,
						char *tag,
						stm_registry_member_type_t *p_child_type);

void stm_registry_dumptheregistry(void);

int __must_check stm_registry_update_attribute(stm_object_h object,
					    const char *tag,
					    const char *type_tag,
					    void *buffer,
					    int size);

#ifdef __cplusplus
}
#endif

#endif /*_STM_REGISTRY_H*/
