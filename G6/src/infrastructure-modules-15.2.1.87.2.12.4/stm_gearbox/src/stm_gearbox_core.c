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
 @File   stm_gearbox_core.c
 @brief
*/
#include "stm_registry.h"
#include "stm_gearbox_debug.h"
#include "stm_gearbox.h"

static infra_os_mutex_t rw_mutex = NULL;
static infra_os_mutex_t tuning_mutex = NULL;

char *type_to_tag[] = { "display_device", "playback", "audio", "video" };
typedef enum {
	DISPLAY_TYPE,
	PLAYBACK_TYPE,
	PLAYBACK_AUDIO_TYPE, // Child of playback object
	PLAYBACK_VIDEO_TYPE // Child of playback object
} OBJECT_TYPE;

int get_object_handle(OBJECT_TYPE type, stm_object_h *handle);

int stm_gearbox_register_read(stm_gearbox_reg_t* param)
{
	void* virtual_addr;

	if (param->size < 1 || param->size > 4){
		gearbox_dtrace("Invalid Size:%d\n",param->size);
		return -EINVAL;
	}
	infra_os_mutex_lock(rw_mutex);

	if (check_mem_region(param->address, 4)) {
		gearbox_etrace("Memory in use");
		infra_os_mutex_unlock(rw_mutex);
		return -EBUSY;
	}

	if (request_mem_region(param->address, 4, "stm_gearbox_dev") == NULL) {
		gearbox_etrace("request_mem_region failed");
		infra_os_mutex_unlock(rw_mutex);
		return -EFAULT;
	}
	virtual_addr = ioremap(param->address, 4);
	switch (param->size) {
		case 1:
			param->value = (uint32_t)ioread8(virtual_addr);
			break;
		case 2:
			param->value = (uint32_t)ioread16(virtual_addr);
			break;
		case 3:
			param->value = (uint32_t)ioread32(virtual_addr) & 0x00FFFFFF;
			break;
		case 4:
			param->value = (uint32_t)ioread32(virtual_addr);
			break;
	}
	iounmap(virtual_addr);
	release_mem_region(param->address, 4);
	infra_os_mutex_unlock(rw_mutex);

	return 0;
}
EXPORT_SYMBOL(stm_gearbox_register_read);

int stm_gearbox_register_write(stm_gearbox_reg_t *param)
{
	void* virtual_addr;
	if (param->size < 1 || param->size > 4){
		gearbox_dtrace("Invalid Size:%d\n",param->size);
		return -EINVAL;
	}

	infra_os_mutex_lock(rw_mutex);

	if (check_mem_region(param->address, 4)) {
		gearbox_etrace("Memory in use");
		infra_os_mutex_unlock(rw_mutex);
		return -EBUSY;
	}

	if (request_mem_region(param->address,	4, "stm_gearbox_dev") == NULL) {
		gearbox_etrace("request_mem_region failed");
		infra_os_mutex_unlock(rw_mutex);
		return -EBUSY;
	}
	virtual_addr = ioremap(param->address, 4);
	switch (param->size) {
		case 1:
			iowrite8((uint8_t)param->value, virtual_addr);
			break;
		case 2:
			iowrite16((uint16_t)param->value, virtual_addr);
			break;
		case 3:
			iowrite32(param->value |
				((uint32_t)ioread32(virtual_addr) & 0xFF000000),
        virtual_addr);
			break;
		case 4:
			iowrite32(param->value, virtual_addr);
			break;
	}

	iounmap(virtual_addr);
	release_mem_region(param->address, 4);
	infra_os_mutex_unlock(rw_mutex);

  return 0;
}
EXPORT_SYMBOL(stm_gearbox_register_write);

int stm_gearbox_handle_tuning(stm_gearbox_param_t *param)
{
	gearbox_dtrace("STM_GEARBOX_SET_TUNING_PARAMS:size:%d\n", param->size);
	/*TBD*/
  return 0;
}
EXPORT_SYMBOL(stm_gearbox_handle_tuning);

int get_object_handle(OBJECT_TYPE type, stm_object_h *handle)
{
	char *find_tag = NULL;
	char *child_tag = NULL;
	char tag[30];
	int result;
	stm_registry_iterator_h iter;
	stm_registry_member_type_t child_type;
	bool object_found = false;
	switch (type) {
		case DISPLAY_TYPE:
			find_tag = type_to_tag[type];
			break;

		case PLAYBACK_TYPE:
			find_tag = type_to_tag[type];
			break;

		case PLAYBACK_AUDIO_TYPE:
			find_tag = type_to_tag[PLAYBACK_TYPE];
			child_tag = type_to_tag[type];
			break;

		case PLAYBACK_VIDEO_TYPE:
			find_tag = type_to_tag[PLAYBACK_TYPE];
			child_tag = type_to_tag[type];
			break;

		default:
			gearbox_dtrace("Type not supported:%d", type);

	}
	gearbox_dtrace("get_handle_from_registry:type:%d", type);

	CHECK_CALL_RETURN(stm_registry_new_iterator(STM_REGISTRY_INSTANCES,
                                              STM_REGISTRY_MEMBER_TYPE_OBJECT,
                                              &iter));

	do {
		CHECK_CALL_BREAK(stm_registry_iterator_get_next(iter, tag, &child_type));
		gearbox_dtrace("iterator data tag %s of type %d\n", tag, child_type);
		if (strncmp(tag, find_tag, strlen(find_tag)) == 0
				|| strstr(tag, find_tag) != NULL) {
			CHECK_CALL_BREAK(stm_registry_get_object(STM_REGISTRY_INSTANCES, tag, handle));
			if (child_tag == NULL) {
				object_found = true;
				break;
			}
			else { // Find the child
				find_tag = child_tag;
				child_tag = NULL;
				CHECK_CALL_RETURN(stm_registry_delete_iterator(iter));
				CHECK_CALL_RETURN(stm_registry_new_iterator(*handle,
							STM_REGISTRY_MEMBER_TYPE_OBJECT,
							&iter));
			}
		}
	} while (1);

	CHECK_CALL_RETURN(stm_registry_delete_iterator(iter));

	return 0;
}

static void __exit stm_gearbox_exit_module(void)
{
	infra_os_mutex_terminate(rw_mutex);
	rw_mutex = NULL;
	infra_os_mutex_terminate(tuning_mutex);
	tuning_mutex = NULL;
}

static int __init stm_gearbox_init_module(void)
{
	int retval = infra_os_mutex_initialize(&rw_mutex);
	if (retval != INFRA_NO_ERROR)
		return retval;

	retval = infra_os_mutex_initialize(&tuning_mutex);
	if (retval != INFRA_NO_ERROR){
		infra_os_mutex_terminate(rw_mutex);
		rw_mutex = NULL;
		return retval;
	}

	return 0;
}

module_init(stm_gearbox_init_module);
module_exit(stm_gearbox_exit_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sonika");
MODULE_DESCRIPTION("STM_PROBE driver");
