#include <linux/init.h>
#include <linux/module.h>

#include "memutil.h"
#include "infra_os_wrapper.h"
#include "stm_registry.h"
#include "stm_event.h"
#include "stm_memsrc.h"
#include "stm_data_interface.h"
#include "mss_debug.h"

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("Module for memory source");
MODULE_LICENSE("GPL");

#define src_key		0xbadabdc7

typedef enum stm_memsrc_state_e {
	STM_MEMSRC_STATE_DETACHED,
	STM_MEMSRC_STATE_CHECK_CONNECTION,
	STM_MEMSRC_STATE_READY,
	STM_MEMSRC_STATE_PROCESSING,
	STM_MEMSRC_STATE_REPORTING,
	STM_MEMSRC_STATE_EXIT
} stm_memsrc_state_t;

#define memsrc_update_state(memsrc, state)	{\
		infra_os_mutex_lock(memsrc->state_mutex);\
		memsrc->cur_state = state;\
		infra_os_mutex_unlock(memsrc->state_mutex);\
}

struct stm_memsrc_class_s {
	stm_object_h myself;
};

static struct stm_memsrc_class_s memsrc_type = {0};

static int handle_push_in_kernel_mode(struct stm_memsrc_s *src_p,
				      struct stm_data_block *block_list,
				      uint32_t *injected_size);
static int handle_push_in_kernel_vmalloc_mode(struct stm_memsrc_s *src_p,
					      struct stm_data_block *block_list,
					      uint32_t *injected_size);
static int handle_push_in_user_mode(struct stm_memsrc_s *src_p,
				    struct stm_data_block *block_list,
				    uint32_t *injected_size);
static int memsrc_check_state(struct stm_memsrc_s *src_p,
			      stm_memsrc_state_t desired_state);
static int stm_memsrc_notify(stm_object_h src, unsigned int event_id);

/*! Memory source object */
struct stm_memsrc_s {
	uint32_t key;
	stm_memory_iomode_t mode;
	stm_object_h attach;
	stm_data_interface_push_sink_t sink_interface;
	stm_data_interface_push_notify_t notify_interface;
	stm_data_mem_type_t address_space;
	stm_memsrc_state_t	cur_state;
	infra_os_mutex_t state_mutex;
};

stm_data_interface_push_notify_t mem_src_interface_push_notify = {
	.notify = stm_memsrc_notify,
};

static bool memsrc_obj_is_valid(struct stm_memsrc_s *memsrc_obj_p)
{
	bool	ret = true;
	if (memsrc_obj_p == NULL || memsrc_obj_p->key != src_key)
		ret = false;
	return ret;
}

int __init stm_memsrc_init(void)
{
	int		error = 0;

/* register the memory source object type */
	error = stm_registry_add_object(STM_REGISTRY_TYPES,
		      "memsource",
		      (stm_object_h) &memsrc_type);
	if (error)
		MSS_ERROR_MSG("stm_regstry_add_object failed(%d)\n", error);

	memsrc_type.myself = (stm_object_h) &memsrc_type;

	error = stm_registry_add_attribute(memsrc_type.myself, /*parent*/
		      STM_DATA_INTERFACE_PUSH_NOTIFY, /*tag*/
		      STM_REGISTRY_ADDRESS, /*data type tag*/
		      &mem_src_interface_push_notify, /*value*/
		      sizeof(mem_src_interface_push_notify)); /*value size*/
	if (error)
		MSS_ERROR_MSG("stm_regstry_add_attribute failed(%d)\n", error);

	return error;
}

void __exit stm_memsrc_term(void)
{
	int		error = 0;
	error = stm_registry_remove_object((stm_object_h) &memsrc_type);
	if (error)
		MSS_ERROR_MSG("stm_registry_remove_object failed(%d)\n",
			      error);

	error = stm_registry_remove_attribute(memsrc_type.myself,
		      STM_DATA_INTERFACE_PUSH_NOTIFY);
	if (error)
		MSS_ERROR_MSG("stm_registry_remove_attribute failed(%d)\n",
			      error);
}

/*!

*/
int
stm_memsrc_new(const char *name,
	       stm_memory_iomode_t iomode,
	       stm_data_mem_type_t address_space,
	       stm_memsrc_h *memsrc_ctx)
{
	struct stm_memsrc_s *memsrc_obj_p;
	int retval;

	if ((!memsrc_ctx) || (!name))
		return -EINVAL;

	if (address_space > KERNEL || address_space < USER)
		return -EINVAL;

	memsrc_obj_p = infra_os_malloc(sizeof(*memsrc_obj_p));
	if (!memsrc_obj_p)
		return -ENOMEM;

	/* infra_os_malloc already memset the buffer to 0 */
	retval = stm_registry_add_instance(STM_REGISTRY_INSTANCES, /*parent*/
		      memsrc_type.myself, /*type*/
		      name,
		      (stm_object_h) memsrc_obj_p);
	if (retval) {
		/* error in the registry */
		MSS_ERROR_MSG("stm_registry_add_instance failed(%d)\n",
			      retval);
		infra_os_free(memsrc_obj_p);
		*memsrc_ctx = NULL;
		return retval;
	}

	/* not usefull as long as infra_os_malloc set to 0 buffers */
	memsrc_obj_p->attach = (stm_object_h) NULL; /* Not yet attached */
	memset(&(memsrc_obj_p->sink_interface),
		      0,
		      sizeof(memsrc_obj_p->sink_interface));

	memset(&(memsrc_obj_p->notify_interface),
		      0, sizeof(memsrc_obj_p->notify_interface));
	retval = infra_os_mutex_initialize(&(memsrc_obj_p->state_mutex));
	if (retval != INFRA_NO_ERROR)
		return retval;
	memsrc_obj_p->cur_state = STM_MEMSRC_STATE_DETACHED;
	memsrc_obj_p->address_space = address_space;
	memsrc_obj_p->key = src_key;
	memsrc_obj_p->mode = iomode;
	*memsrc_ctx = memsrc_obj_p;

	return 0;
}
EXPORT_SYMBOL(stm_memsrc_new);

/*!

*/
int
stm_memsrc_delete(stm_memsrc_h memsrc_ctx)
{
	struct stm_memsrc_s *memsrc_obj_p = memsrc_ctx;
	int ret = 0;

	MSS_DEBUG_MSG(MEM_SRC, "(%p)\n", memsrc_ctx);
	if (!memsrc_obj_is_valid(memsrc_obj_p))
		return -ENXIO;

	ret = memsrc_check_state(memsrc_obj_p, STM_MEMSRC_STATE_EXIT);
	if (ret < 0)
		return ret;

	ret = stm_registry_remove_object(memsrc_ctx);
	if (ret)
		MSS_ERROR_MSG("stm_registry_remove_object(:%d)\n", ret);

	infra_os_mutex_terminate(memsrc_obj_p->state_mutex);
	memsrc_obj_p->state_mutex = NULL;
	memsrc_obj_p->key = 0x0;
	memsrc_obj_p->attach = NULL;
	infra_os_free(memsrc_obj_p);

	return 0;
}
EXPORT_SYMBOL(stm_memsrc_delete);

/*!

 */
int
stm_memsrc_attach(stm_memsrc_h memsrc_ctx,
		  stm_object_h consumer_ctx,
		  const char *consumer_interface_type)
{
	int retval = 0;
	stm_object_h consumer_type;
	uint32_t returned_size = 0;
	struct stm_memsrc_s *memsrc_obj_p = memsrc_ctx;

/* TODO check if consumer_ctx is a valid object to connect to */
	if (!memsrc_obj_is_valid(memsrc_obj_p))
		return -ENXIO;

	/*get the source object type*/
	retval = stm_registry_get_object_type(consumer_ctx, &consumer_type);
	if (retval) {
		MSS_ERROR_MSG("stm_registry_get_object_type(%p, &%p)",
			      consumer_ctx,
			      consumer_type);
		return -ENODEV;
	}

	retval = stm_registry_get_attribute(consumer_type,
		      consumer_interface_type,
		      STM_REGISTRY_ADDRESS,
		      sizeof(memsrc_obj_p->sink_interface),
		      &memsrc_obj_p->sink_interface,
		      &returned_size);

	if (retval) {
		if ((retval == -ENOMEM) ||
			(returned_size != sizeof(memsrc_obj_p->sink_interface))) {
			MSS_ERROR_MSG("error attribute not compatible\n");
			return -ECONNREFUSED;
		} else {
			MSS_ERROR_MSG("doesn't recognize consumer\n");
			return -ENODEV;
		}
	}

	retval = memsrc_check_state(memsrc_obj_p,
		      STM_MEMSRC_STATE_CHECK_CONNECTION);
	if (retval < 0)
		return retval;

	memsrc_obj_p->sink_interface.mode = memsrc_obj_p->mode;

	if (0 != stm_registry_add_attribute(consumer_ctx, /* parent */
		consumer_interface_type, /*tag*/
		STM_REGISTRY_ADDRESS, /* data type tag*/
		&memsrc_obj_p->sink_interface,
		sizeof(memsrc_obj_p->sink_interface)))
		MSS_DEBUG_MSG(MEM_SRC, "attribute already exists\n");

	/* call the sink interface's connect handler to connect the consumer */
	retval = memsrc_obj_p->sink_interface.connect(memsrc_ctx, consumer_ctx);
	if (retval) {
		MSS_ERROR_MSG("connect callback failed (%d)\n",
			      retval);
		memsrc_update_state(memsrc_obj_p,
			      STM_MEMSRC_STATE_DETACHED);
		return -ECONNREFUSED;
	}

	retval = stm_registry_add_connection(memsrc_ctx,
		      "attach",
		      consumer_ctx);

	if (!retval)
		memsrc_obj_p->attach = consumer_ctx;

	memsrc_update_state(memsrc_obj_p, STM_MEMSRC_STATE_READY);

	return retval;
}
EXPORT_SYMBOL(stm_memsrc_attach);

/*!

*/
int
stm_memsrc_detach(stm_memsrc_h memsrc_ctx)
/* a second parameter [stm_object_h consumer_ctx] may be usefull
* for an other than memeory sourcerc
* but as memsrc has only one attach, useless here */
{
	int retval = 0;
	struct stm_memsrc_s *memsrc_obj_p = memsrc_ctx;
	stm_object_h consumer_ctx;

	MSS_DEBUG_MSG(MEM_SRC, "(%p)\n", memsrc_ctx);
	if (!memsrc_obj_is_valid(memsrc_obj_p)) {
		MSS_ERROR_MSG("not expected memsrc_ctx\n");
		return -ENXIO;
	}

	if (memsrc_obj_p->attach == NULL) {
		MSS_ERROR_MSG("not attachd to any object. %p, %d",
			      memsrc_obj_p,
			      retval);
		return -ENXIO;
	}

	retval = memsrc_check_state(memsrc_obj_p,
		      STM_MEMSRC_STATE_CHECK_CONNECTION);
	if (retval < 0) {
		MSS_ERROR_MSG("Detach %p, %d\n",
			      memsrc_obj_p,
			      retval);
		return retval;
	}

	consumer_ctx = memsrc_obj_p->attach;
	memsrc_obj_p->attach = NULL;

	retval = stm_registry_remove_connection(memsrc_ctx, "attach");
	if (retval) {
		MSS_ERROR_MSG("stm_registry_remove_connection(%p)fail %d\n",
			      memsrc_obj_p,
			      retval);
	}
	retval = memsrc_obj_p->sink_interface.disconnect(
		      (stm_object_h) memsrc_obj_p,
		      consumer_ctx);
	if (retval) {
		MSS_ERROR_MSG("(%p)->sink_interface.disconnect(%p) fail\n",
			      memsrc_obj_p,
			      consumer_ctx);
	}
	retval = stm_registry_remove_attribute(consumer_ctx,
		STM_DATA_INTERFACE_PUSH);
	if (retval) {
		MSS_ERROR_MSG("(%p) remove_attribute fail\n", consumer_ctx);
	}

	/* reset get_attribute */
	memset(&(memsrc_obj_p->sink_interface), 0,
		      sizeof(memsrc_obj_p->sink_interface));

	memsrc_update_state(memsrc_obj_p, STM_MEMSRC_STATE_DETACHED);

	return retval;
}
EXPORT_SYMBOL(stm_memsrc_detach);

/*!

*/
int
stm_memsrc_set_control(stm_memsrc_h memsrc_ctx,
		       stm_memsrc_control_t selector,
		       uint32_t value)
{
	int retval;
	struct stm_memsrc_s *memsrc_obj_p = memsrc_ctx;

	if (!memsrc_obj_is_valid(memsrc_obj_p))
		return -ENXIO;

	switch (selector) {
	case STM_MEMSRC_FLUSH_SHARED_POOL:
		retval = -ENOSYS;
		break;
	case STM_MEMSRC_RETURN_ALL_EMPTY_BLOCKS:
		retval = -ENOSYS;
		break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}
EXPORT_SYMBOL(stm_memsrc_set_control);

int
stm_memsrc_set_compound_control(stm_memsrc_h memsrc_ctx,
				stm_memsrc_control_t selector,
				const void *value,
				uint32_t size)
{
	int retval = 0;
	return retval;
}
EXPORT_SYMBOL(stm_memsrc_set_compound_control);

/*!

*/
int
stm_memsrc_push_data(stm_memsrc_h memsrc_ctx,
		     void *data_addr,
		     uint32_t data_length,
		     uint32_t *injected_size)
{
	struct stm_data_block block_list;
	struct stm_memsrc_s *memsrc_obj_p = memsrc_ctx;
	int retval = 0;

	if (!memsrc_obj_is_valid(memsrc_obj_p)) {
		MSS_ERROR_MSG("Object non valid\n");
		return -ENXIO;
	}
	if ((!data_addr) || (!injected_size)) {
		MSS_ERROR_MSG("Data non valid\n");
		return -EINVAL;
	}

	MSS_DEBUG_MSG(MEM_SRC, "buf=%p len=%d inj=%d\n",
		      data_addr,
		      data_length,
		      *injected_size);

	retval = memsrc_check_state(memsrc_obj_p, STM_MEMSRC_STATE_PROCESSING);
	if (retval < 0)
		return retval;

	*injected_size = 0;
	block_list.data_addr = data_addr;
	block_list.len = data_length;

	if (memsrc_obj_p->sink_interface.push_data) {
		switch (memsrc_obj_p->address_space) {
		case USER:
			retval = handle_push_in_user_mode(memsrc_obj_p,
				      &block_list,
				      injected_size);
			break;

		case KERNEL_VMALLOC:
			retval = handle_push_in_kernel_vmalloc_mode(
				      memsrc_obj_p,
				      &block_list,
				      injected_size);
			break;

		case KERNEL:
			retval = handle_push_in_kernel_mode(memsrc_obj_p,
				      &block_list,
				      injected_size);
			break;

		case PHYSICAL:
			memsrc_update_state(memsrc_obj_p,
				      STM_MEMSRC_STATE_READY);
			return -EINVAL;
		}

	} else {
		MSS_ERROR_MSG("Not a good interface\n");
		retval = -ENXIO;
	}

	MSS_DEBUG_MSG(MEM_SRC, "buf=%p len=%d inj=%d\n",
		      data_addr,
		      data_length,
		      *injected_size);
	memsrc_update_state(memsrc_obj_p, STM_MEMSRC_STATE_READY);

	return retval;
}
EXPORT_SYMBOL(stm_memsrc_push_data);

static int handle_push_in_user_mode(struct stm_memsrc_s *memsrc_obj_p,
				    struct stm_data_block *block_list,
				    uint32_t *injected_size)
{
	int32_t			retval;
	uint32_t		processed = 0, mapped = 0;
	struct scatterlist	*input_sg_list;
	struct page		**input_pages;
	struct stm_data_block	*input_block_list;
	uint32_t		i = 0, temp = 0;

	retval = generate_block_list(block_list->data_addr,
		      block_list->len,
		      &input_sg_list,
		      &input_pages,
		      &input_block_list,
		      &mapped);
	if (0 > retval)
		return -ENOMEM;

	retval = memsrc_obj_p->sink_interface.push_data(memsrc_obj_p->attach,
		      input_block_list,
		      mapped,
		      &processed);

	temp = processed;
	while (temp--) {
		*injected_size += input_block_list[i].len;
		i++;
	}

	free_block_list(input_pages,
		      input_sg_list,
		      input_block_list,
		      mapped,
		      processed);

	return retval;
}

static int handle_push_in_kernel_vmalloc_mode(struct stm_memsrc_s *memsrc_obj_p,
					      struct stm_data_block *block_list,
					      uint32_t *injected_size)
{
	int32_t			retval;
	uint32_t		processed = 0, mapped = 0;
	struct scatterlist	*input_sg_list;
	struct page		**input_pages;
	struct stm_data_block	*input_block_list;
	uint32_t		i = 0, temp = 0;

	retval = generate_block_list_vmalloc(block_list->data_addr,
		      block_list->len,
		      &input_sg_list,
		      &input_pages,
		      &input_block_list,
		      &mapped);
	if (0 > retval)
		return -ENOMEM;

	retval = memsrc_obj_p->sink_interface.push_data(memsrc_obj_p->attach,
		      input_block_list,
		      mapped,
		      &processed);

	temp = processed;
	while (temp--) {
		*injected_size += input_block_list[i].len;
		i++;
	}

	free_block_list_vmalloc(input_pages,
		      input_sg_list,
		      input_block_list,
		      mapped,
		      processed);

	return retval;
}

static int handle_push_in_kernel_mode(struct stm_memsrc_s *src_p,
				      struct stm_data_block *block_list,
				      uint32_t *injected_size)
{
	int32_t		ret;
	uint32_t	processed = 0;

	ret = src_p->sink_interface.push_data(src_p->attach,
		      block_list,
		      1,
		      &processed);
	*injected_size = (processed == 1) ? block_list->len : 0;

	return ret;
}

static int memsrc_check_state(struct stm_memsrc_s *src_p,
			      stm_memsrc_state_t desired_state)
{
	int ret = 0;
	stm_memsrc_state_t	cur_state;

	infra_os_mutex_lock(src_p->state_mutex);
	cur_state = src_p->cur_state;

	switch (desired_state) {
	case STM_MEMSRC_STATE_REPORTING:
		if ((cur_state != STM_MEMSRC_STATE_READY) &&
			(cur_state != STM_MEMSRC_STATE_REPORTING)) {
			ret = -EPERM;
		}
		break;
	case STM_MEMSRC_STATE_PROCESSING:
		if (cur_state != STM_MEMSRC_STATE_READY) {
			if (cur_state == STM_MEMSRC_STATE_REPORTING)
				ret = -EBUSY;
			else
				ret = -EPERM;
		}
		break;
	case STM_MEMSRC_STATE_CHECK_CONNECTION:
		if ((cur_state != STM_MEMSRC_STATE_READY) &&
			(cur_state != STM_MEMSRC_STATE_DETACHED)) {
			if (cur_state == STM_MEMSRC_STATE_REPORTING)
				ret = -EBUSY;
			else
				ret = -EPERM;
		}
		break;
	case STM_MEMSRC_STATE_READY:
		if ((cur_state == STM_MEMSRC_STATE_EXIT) ||
			(cur_state == STM_MEMSRC_STATE_DETACHED))
			ret = -EPERM;

		break;
	case STM_MEMSRC_STATE_DETACHED:
		if ((cur_state != STM_MEMSRC_STATE_CHECK_CONNECTION))
			ret = -EPERM;

		break;
	case STM_MEMSRC_STATE_EXIT:
		if ((cur_state != STM_MEMSRC_STATE_DETACHED)) {
			if (cur_state == STM_MEMSRC_STATE_CHECK_CONNECTION)
				ret = -EBUSY;
			else
				ret = -EPERM;
		}
		break;
	}

	if (!ret)
		src_p->cur_state = desired_state;

	infra_os_mutex_unlock(src_p->state_mutex);

	return ret;
}

int stm_memsrc_notify(stm_object_h src, unsigned int event_id)
{
	struct stm_memsrc_s *memsrc_obj_p = src;
	int error = 0;
	stm_event_t src_event;

	MSS_DEBUG_MSG(MEM_SRC, "memsrc(%p)\n", src);
	if (!memsrc_obj_is_valid(memsrc_obj_p)) {
		MSS_ERROR_MSG("not expected memsrc_ctx %p\n", src);
		return -ENXIO;
	}

	/*TODO : Add check for correct state . Remove complex state
	* machine and use only two states ATTACHED and DETACHED */

	switch (event_id) {
	case STM_MEMSRC_EVENT_CONTINUE_INJECTION:
	case STM_MEMSRC_EVENT_BUFFER_UNDERFLOW:
		src_event.event_id = event_id;
		break;

	default:
		MSS_ERROR_MSG("Not expected event Id : %d\n", event_id);
		error = -ENXIO;
	}

	if (!error) {
		src_event.object = src;
		MSS_DEBUG_MSG(MEM_SRC, "signal %d for %p\n",
			      event_id, src_event.object);
		error = stm_event_signal(&src_event);
	}
	return error;
}

/* EOF */
