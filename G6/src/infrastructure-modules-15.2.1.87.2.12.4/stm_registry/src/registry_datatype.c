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
 @File   registry_datatype.c
 @brief
*/

#include <linux/string.h>
#include <linux/kallsyms.h>

#include "registry_datatype.h"
#include "infra_os_wrapper.h"
#include "registry_internal.h"
extern registry_datatype_t *HeadDataType_np;
extern registry_datatype_t *HeadDataType_removable_np;

extern infra_os_semaphore_t g_registry_data_sem_lock;

/*Attribute will contain the function pointer only*/
int registry_print_int32(stm_object_h object,
			 char *buf,
			 size_t size,
			 char *user_buf,
			 size_t user_size)
{
	/*"%d"*/
	int count = 0, last_write = 0, total_write = 0;
	char temp_buffer[20]; /* Max convetion for integer can be only 10*/
	while (count * 4 < size) {
		last_write = sprintf(temp_buffer, "%d\n", *((int *) buf + count));
		if (total_write + last_write < user_size) {
			memcpy(user_buf + total_write, temp_buffer, last_write);
			total_write += last_write;
		} else
			break;
		count++;
	}
	return total_write;
}
int registry_print_uint32(stm_object_h object,
			  char *buf,
			  size_t size,
			  char *user_buf,
			  size_t user_size)
{
	/*"%u"*/
	int count = 0, last_write = 0, total_write = 0;
	char temp_buffer[20]; /* Max convetion for unsigned integer can be only 10*/
	while (count * 4 < size) {
		last_write = sprintf(temp_buffer, "%u\n", *((int *) buf + count));
		if (total_write + last_write < user_size) {
			memcpy(user_buf + total_write, temp_buffer, last_write);
			total_write += last_write;
		} else
			break;
		count++;
	}
	return last_write;
}
int registry_print_address(stm_object_h object,
			   char *buf,
			   size_t size,
			   char *user_buf,
			   size_t user_size)
{
	int count = 0, last_write = 0, total_write = 0;
	char temp_buffer[KSYM_SYMBOL_LEN+2];
	while (count * 4 < size) {
		last_write = sprint_symbol(temp_buffer,
			      *((unsigned long *) ((int *) buf + count)));
		last_write += sprintf(temp_buffer + last_write, "%s", "\n");
		if (total_write + last_write < user_size) {
			memcpy(user_buf + total_write, temp_buffer, last_write);
			total_write += last_write;
		} else
			break;
		count = count + 1;
	}
	return total_write;
}



/*Attribute will contain the function pointer only*/
int registry_store_int32(stm_object_h object,
			 char *buf,
			 size_t size,
			 const char *user_buf,
			 size_t user_size)
{

	/*"%d"*/

	REGISTRY_DEBUG_MSG("\n");

	sscanf(user_buf, "%d", (int32_t *) buf);

	return user_size;
}

int registry_store_uint32(stm_object_h object,
			  char *buf,
			  size_t size,
			  const char *user_buf,
			  size_t user_size)
{
	/*"%u"*/

	REGISTRY_DEBUG_MSG("\n");
	sscanf(user_buf, "%u", (int32_t *) buf);

	return user_size;
}


int registry_store_address(stm_object_h object,
			   char *buf,
			   size_t size,
			   const char *user_buf,
			   size_t user_size)
{
/*TODO : add store address support*/
	return 0;
}

int registry_internal_add_data_type(const char *Tag, stm_registry_type_def_t *Def)
{
	registry_datatype_t *datatype_trvs_np;
	registry_datatype_t *datatype_prev_np = NULL;
	registry_datatype_t *datatype_np = NULL;

	infra_os_sema_wait(g_registry_data_sem_lock);
	datatype_trvs_np = HeadDataType_np;
	while (datatype_trvs_np) {
		if (!strcmp(datatype_trvs_np->datatype_info.tag, Tag)) {
			REGISTRY_ERROR_MSG("<%s>:<%d> Data type exist\n", __FUNCTION__, __LINE__);
			infra_os_sema_signal(g_registry_data_sem_lock);
			return -EEXIST;
		}
		datatype_prev_np = datatype_trvs_np;
		datatype_trvs_np = datatype_trvs_np->datatype_next_np;
	}
	datatype_np = (registry_datatype_t *) infra_os_malloc(sizeof(registry_datatype_t));
	if (!datatype_np) {
		REGISTRY_ERROR_MSG("<%s>:<%d> Memory allocation failed\n", __FUNCTION__, __LINE__);
		infra_os_sema_signal(g_registry_data_sem_lock);
		return -ENOMEM;
	}
	datatype_np->datatype_info.def.print_handler = Def->print_handler;
	datatype_np->datatype_info.def.store_handler = Def->store_handler;
	strlcpy(datatype_np->datatype_info.tag, Tag, STM_REGISTRY_MAX_TAG_SIZE);
	datatype_np->datatype_next_np = NULL;

	datatype_prev_np->datatype_next_np = datatype_np;

	if (datatype_trvs_np == HeadDataType_removable_np)
		HeadDataType_removable_np = datatype_np;

	infra_os_sema_signal(g_registry_data_sem_lock);
	return 0;
}


int registry_internal_remove_data_type(const char *Tag)
{
	registry_datatype_t *datatype_np;
	registry_datatype_t *datatype_prev_np = NULL;
	/* Protection for default data type */

	infra_os_sema_wait(g_registry_data_sem_lock);
	datatype_np = HeadDataType_np->datatype_next_np->datatype_next_np->datatype_next_np;
	datatype_prev_np = HeadDataType_np->datatype_next_np->datatype_next_np;
	while (datatype_np) {
		if (!strcmp(datatype_np->datatype_info.tag, Tag)) {
			datatype_prev_np->datatype_next_np = datatype_np->datatype_next_np;
			infra_os_free(datatype_np);
			break;
		}
		datatype_prev_np = datatype_np;
		datatype_np = datatype_np->datatype_next_np;
	}

	if (!datatype_np) {
		REGISTRY_ERROR_MSG("<%s>:<%d> Data type Does not exist\n", __FUNCTION__, __LINE__);
		infra_os_sema_signal(g_registry_data_sem_lock);
		return -ENODEV;
	}
	infra_os_sema_signal(g_registry_data_sem_lock);
	return 0;
}

int registry_internal_get_data_type(const char *Tag, stm_registry_type_def_t *Def)
{
	registry_datatype_t *datatype_np;

	infra_os_sema_wait(g_registry_data_sem_lock);
	datatype_np = HeadDataType_np;

	while (datatype_np) {
		if (!strcmp(datatype_np->datatype_info.tag, Tag)) {
			Def->print_handler = datatype_np->datatype_info.def.print_handler;
			Def->store_handler = datatype_np->datatype_info.def.store_handler;
			break;
		}
		datatype_np = datatype_np->datatype_next_np;
	}
	if (!datatype_np) {
		REGISTRY_ERROR_MSG("<%s>:<%d> Data type Does not exist\n", __FUNCTION__, __LINE__);
		infra_os_sema_signal(g_registry_data_sem_lock);
		return -ENODEV;
	}
	infra_os_sema_signal(g_registry_data_sem_lock);
	return 0;
}


int registry_internal_add_default_data_type(void)
{
	registry_datatype_t *datatype_np;
	registry_datatype_t *datatype_prev_np;

	infra_os_sema_wait(g_registry_data_sem_lock);
	datatype_np = HeadDataType_np;
	datatype_np->datatype_info.def.print_handler = registry_print_int32;
	datatype_np->datatype_info.def.store_handler = registry_store_int32;
	strlcpy(datatype_np->datatype_info.tag, STM_REGISTRY_INT32, (STM_REGISTRY_MAX_TAG_SIZE - 1));
	datatype_prev_np = datatype_np;

	datatype_np = (registry_datatype_t *) infra_os_malloc(sizeof(registry_datatype_t));
	if (!datatype_np) {
		infra_os_sema_signal(g_registry_data_sem_lock);
		return -ENOMEM;
	}

	datatype_np->datatype_next_np = NULL;
	datatype_prev_np->datatype_next_np = datatype_np;
	datatype_np->datatype_info.def.print_handler = registry_print_uint32;
	datatype_np->datatype_info.def.store_handler = registry_store_uint32;
	strlcpy(datatype_np->datatype_info.tag, STM_REGISTRY_UINT32, (STM_REGISTRY_MAX_TAG_SIZE - 1));
	datatype_prev_np = datatype_np;

	datatype_np = (registry_datatype_t *) infra_os_malloc(sizeof(registry_datatype_t));
	if (!datatype_np) {
		infra_os_sema_signal(g_registry_data_sem_lock);
		return -ENOMEM;
	}

	datatype_np->datatype_next_np = NULL;
	datatype_prev_np->datatype_next_np = datatype_np;
	datatype_np->datatype_info.def.print_handler = registry_print_address;
	datatype_np->datatype_info.def.store_handler = registry_store_address;
	strlcpy(datatype_np->datatype_info.tag, STM_REGISTRY_ADDRESS, (STM_REGISTRY_MAX_TAG_SIZE - 1));

	HeadDataType_removable_np = datatype_np->datatype_next_np;
	infra_os_sema_signal(g_registry_data_sem_lock);
	return 0;
}

int registry_internal_remove_default_data_type(void)
{
	registry_datatype_t *datatype_np;
	registry_datatype_t *datatype_next_np;

	infra_os_sema_wait(g_registry_data_sem_lock);
	datatype_np = HeadDataType_np->datatype_next_np;
	datatype_next_np = datatype_np->datatype_next_np;

	infra_os_free(datatype_np);

	datatype_np = datatype_next_np;
	datatype_next_np = datatype_np->datatype_next_np;

	infra_os_free(datatype_np);
	HeadDataType_removable_np = NULL;
	infra_os_sema_signal(g_registry_data_sem_lock);

	return 0;
}
