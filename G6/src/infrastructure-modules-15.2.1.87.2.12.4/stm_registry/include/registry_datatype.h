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
 @File   registry_datatype.h
 @brief
*/

#ifndef _REGISTRY_DATATYPE_H
#define _REGISTRY_DATATYPE_H

#include "stm_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct registry_datatype_info_s {
	char tag[STM_REGISTRY_MAX_TAG_SIZE];
	stm_registry_type_def_t def;
} registry_datatype_info_t;

typedef struct registry_datatype_s {
	registry_datatype_info_t datatype_info;
	struct registry_datatype_s *datatype_next_np;
} registry_datatype_t;


int registry_internal_add_default_data_type(void);

int registry_internal_remove_default_data_type(void);

int registry_internal_add_data_type(const char *tag,
				    stm_registry_type_def_t *def);

int registry_internal_remove_data_type(const char *tag);

int registry_internal_get_data_type(const char *tag,
				    stm_registry_type_def_t *def);
#ifdef __cplusplus
}
#endif

#endif
