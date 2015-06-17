#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <asm/page.h>
#include <linux/spinlock.h>

#include "memutil.h"
#include "infra_os_wrapper.h"
#include "stm_registry.h"
#include "stm_event.h"
#include "stm_memsink.h"
#include "mss_debug.h"

MODULE_AUTHOR("Nidhi Chadha");
MODULE_DESCRIPTION("Module for memory sink");
MODULE_LICENSE("GPL");

typedef enum stm_memsink_state_e {
	STM_MEMSINK_STATE_DETACHED,
	STM_MEMSINK_STATE_ATTACHED
} stm_memsink_state_t;

static int stm_memsink_attach_pull(stm_object_h src_object,
				   stm_object_h sink_object,
				   struct stm_data_interface_pull_src *pull_src);

static int stm_memsink_detach(stm_object_h source,
			      stm_object_h sink);

static int stm_memsink_notify(stm_object_h memsink, unsigned int event_id);
static stm_memsink_state_t memsink_get_state(struct stm_memsink_s *memsink_p);

static int memsink_set_state(struct stm_memsink_s *memsink_p,
			     stm_memsink_state_t desired_state);

static int sink_handle_pull_in_user_mode(struct stm_memsink_s *memsink_p,
					 struct stm_data_block *block_list,
					 uint32_t *pulled_size);
static int sink_handle_pull_kernel_vmalloc(struct stm_memsink_s *sink_p,
					   struct stm_data_block *block_list,
					   uint32_t *pulled_size);

/*! Memory sink object
\par Stability
Unstable
\par Description
Each new instance of Memory Sink creates a new working context to control data
extraction
*/
struct stm_memsink_s {
	stm_memory_iomode_t mode;
	stm_data_mem_type_t address_space;
	stm_memsink_control_t policy;
	stm_object_h attach;
	stm_data_interface_pull_sink_t pull_interface;
	stm_data_interface_pull_src_t pull_peer;
	stm_memsink_state_t	cur_state;
	infra_os_mutex_t sink_mutex;
};

static stm_data_interface_pull_sink_t mem_sink_interface_pull = {
	.connect = stm_memsink_attach_pull,
	.disconnect = stm_memsink_detach,
	.notify = stm_memsink_notify,
};

struct stm_memsink_class_s {
	stm_object_h myself;
};

static struct stm_memsink_class_s memsink_type = {0};

static bool memsink_obj_is_valid(struct stm_memsink_s *memsink_obj_p)
{
	int retval = 0;
	stm_object_h sink_type;

	MSS_DEBUG_MSG(MEM_SINK, "Entry\n");

	if (memsink_obj_p == NULL) {
		pr_err("%s::%d Sink Object cannot be NULL\n",
			      __func__,
			      __LINE__);
		return false;
	}

	retval = stm_registry_get_object_type((stm_object_h) memsink_obj_p,
		      &sink_type);
	if ((retval) || (sink_type != memsink_type.myself)) {
		/* Error: object expected is not a sink object */
		MSS_ERROR_MSG("invalid object type %p\n", memsink_obj_p);
		return	false;
	}

	return true;
}

static int sink_handle_pull_in_user_mode(struct stm_memsink_s *sink_p,
					 struct stm_data_block *block_list,
					 uint32_t *pulled_size)
{
	int32_t				retval = 0;
	uint32_t			filled_blocks = 0, mapped = 0;
	struct scatterlist	*input_sg_list;
	struct page		**input_pages;
	struct stm_data_block	*input_block_list;
	int pulled = 0;

	retval = generate_block_list(block_list->data_addr,
		      block_list->len,
		      &input_sg_list,
		      &input_pages,
		      &input_block_list,
		      &mapped);

	if (0 > retval)
		return -ENOMEM;

	pulled = sink_p->pull_peer.pull_data(sink_p->attach,
		      input_block_list,
		      mapped,
		      &filled_blocks);

	free_block_list(input_pages,
		      input_sg_list,
		      input_block_list,
		      mapped,
		      filled_blocks);
	if (pulled >= 0) {
		*pulled_size = pulled;
		retval = 0;
	} else {
		retval = pulled;
	}

	return retval;
}

static int sink_handle_pull_kernel_vmalloc(struct stm_memsink_s *sink_p,
					   struct stm_data_block *block_list,
					   uint32_t *pulled_size)
{
	int32_t			retval = 0;
	uint32_t		filled_blocks = 0, mapped = 0;
	struct scatterlist	*input_sg_list;
	struct page		**input_pages;
	struct stm_data_block	*input_block_list;
	int pulled = 0;

	retval = generate_block_list_vmalloc(block_list->data_addr,
		      block_list->len,
		      &input_sg_list,
		      &input_pages,
		      &input_block_list,
		      &mapped);

	if (0 > retval)
		return -ENOMEM;

	pulled = sink_p->pull_peer.pull_data(sink_p->attach,
		      input_block_list,
		      mapped,
		      &filled_blocks);

	free_block_list_vmalloc(input_pages,
		      input_sg_list,
		      input_block_list,
		      mapped,
		      filled_blocks);

	if (pulled >= 0) {
		*pulled_size = pulled;
		retval = 0;
	} else {
		retval = pulled;
	}

	return retval;

}
int __init stm_memsink_init(void)
{
	int error = 0;

	MSS_DEBUG_MSG(MEM_SINK, "Entry\n");

	/*register the memory sink object type */

	error = stm_registry_add_object(STM_REGISTRY_TYPES,
		      "memsink",
		      (stm_object_h) &memsink_type);
	if (error) {
		MSS_ERROR_MSG("stm_registry_add_object failed(%d)\n", error);
		return error;
	}

	memsink_type.myself = (stm_object_h) &memsink_type;

	/*add the sink interface to the MEMORY-SINK object type */
	error = stm_registry_add_attribute(memsink_type.myself,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS, /* data type tag*/
		      &mem_sink_interface_pull,
		      sizeof(mem_sink_interface_pull));
	if (error) {
		MSS_ERROR_MSG("stm_registry_add_attribute failed(%d)\n", error);
		error = stm_registry_remove_object((stm_object_h) &memsink_type);
		return error;
	}

	return error;
}

void __exit stm_memsink_term(void)
{
	int err;

	err = stm_registry_remove_attribute(memsink_type.myself,
		      STM_DATA_INTERFACE_PULL);
	if (err) {
		MSS_ERROR_MSG("stm_registry_remove_attribute fail(%d)\n", err);
		/* TODO error handling */
	}

	memsink_type.myself = NULL;
	err = stm_registry_remove_object((stm_object_h) &memsink_type);
	if (err)
		MSS_ERROR_MSG("stm_registry_remove_attribute fail(%d)\n", err);
}

/*
*/
int
stm_memsink_new(const char *name,
		stm_memory_iomode_t iomode,
		stm_data_mem_type_t address_space,
		stm_memsink_h *memsink_ctx)
{
	int retval = INFRA_NO_ERROR;
	struct stm_memsink_s *memsink_obj_p;

	MSS_DEBUG_MSG(MEM_SINK, "\n");
	if ((!memsink_ctx) || (!name)) {
		MSS_ERROR_MSG("wrong arguments memsink_ctx (%p)\n",
			      memsink_ctx);
		return -EINVAL;
	}

	if (address_space > KERNEL || address_space < USER)
		return -EINVAL;

	/* Add error check for iomode */

	memsink_obj_p = infra_os_malloc(sizeof(*memsink_obj_p));
	if (!memsink_obj_p) {
		MSS_ERROR_MSG("error in memory allocation (%p)\n",
			      memsink_obj_p);
		return -ENOMEM;
	}

	/* infra_os_malloc already memset the buffer to 0 */

	retval = stm_registry_add_instance(STM_REGISTRY_INSTANCES,
		      memsink_type.myself,
		      name,
		      (stm_object_h) memsink_obj_p);

	if (retval) {
		MSS_ERROR_MSG("stm_registry_add_instance failed(%d)\n", retval);

		infra_os_free(memsink_obj_p);
		return retval;
	}

/* not usefull as long as infra_os_malloc set to 0 buffers */
	memsink_obj_p->attach = (stm_object_h) NULL; /* Not yet attached */
	memsink_obj_p->mode = iomode;

	memset(&(memsink_obj_p->pull_interface), 0,
		      sizeof(memsink_obj_p->pull_interface));
	memset(&(memsink_obj_p->pull_peer), 0,
		      sizeof(memsink_obj_p->pull_peer));
	retval = infra_os_mutex_initialize(&(memsink_obj_p->sink_mutex));
	if (retval != INFRA_NO_ERROR) {
		MSS_ERROR_MSG("error in initializing\n");
		if (stm_registry_remove_object(memsink_obj_p))
			MSS_ERROR_MSG("err :removing obj not successful\n");
		infra_os_free(memsink_obj_p);
		return retval;
	}
	memsink_obj_p->cur_state = STM_MEMSINK_STATE_DETACHED;
	memsink_obj_p->address_space = address_space;

	*memsink_ctx = memsink_obj_p;
	memsink_obj_p->pull_interface.connect = stm_memsink_attach_pull,
		memsink_obj_p->pull_interface.disconnect = stm_memsink_detach;
	memsink_obj_p->pull_interface.notify = stm_memsink_notify;

	memsink_obj_p->pull_interface.mode = iomode;
	retval = stm_registry_add_attribute(memsink_obj_p, /* parent */
		      STM_DATA_INTERFACE_PULL, /*tag*/
		      STM_REGISTRY_ADDRESS, /* data type tag*/
		      &memsink_obj_p->pull_interface,
		      sizeof(memsink_obj_p->pull_interface));
	if (retval)
		MSS_ERROR_MSG("error in stm_registry_add_attribute\n");

	return retval;
}
EXPORT_SYMBOL(stm_memsink_new);

/*
*/
int
stm_memsink_delete(stm_memsink_h memsink_ctx)
{
	struct stm_memsink_s *memsink_obj_p = memsink_ctx;
	int ret = 0;

	MSS_DEBUG_MSG(MEM_SINK, "(%p)\n", memsink_ctx);

	if (!memsink_obj_is_valid(memsink_obj_p))
		return -ENXIO;

	infra_os_mutex_lock(memsink_obj_p->sink_mutex);
	if (memsink_get_state(memsink_obj_p) == STM_MEMSINK_STATE_ATTACHED) {
		MSS_ERROR_MSG("Cant delete as memsink obj(%p) is attached\n ",
			      memsink_obj_p);
		return -EBUSY;
	}

	/* reset get_attribute */
	memset(&(memsink_obj_p->pull_interface), 0,
		      sizeof(memsink_obj_p->pull_interface));

	ret = stm_registry_remove_object(memsink_ctx);
	if (ret)
		MSS_ERROR_MSG("stm_registry_remove_object failed(%d)\n",
			      ret);

	infra_os_mutex_unlock(memsink_obj_p->sink_mutex);
	infra_os_mutex_terminate(memsink_obj_p->sink_mutex);
	memsink_obj_p->sink_mutex = NULL;
	memsink_obj_p->attach = NULL;
	infra_os_free(memsink_obj_p);

	return ret;
}
EXPORT_SYMBOL(stm_memsink_delete);

/*
*/
int
stm_memsink_set_control(stm_memsink_h memsink_ctx,
			stm_memsink_control_t selector,
			uint32_t value)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(stm_memsink_set_control);

int	stm_memsink_set_compound_control(stm_memsink_h	memsink_ctx,
					 stm_memsink_control_t selector,
					 const void *value,
					 uint32_t size)
{
	struct stm_memsink_s *memsink_obj_p = memsink_ctx;
	int ret = 0;

	MSS_DEBUG_MSG(MEM_SINK, "(%p)\n", memsink_ctx);
	if (!memsink_obj_is_valid(memsink_obj_p))
		return -ENXIO;

	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_ATTACHED) {
		MSS_ERROR_MSG("memsink obj(%p) not attached\n ",
			      memsink_obj_p);
		return -EBUSY;
	}

	if (NULL == value) {
		ret = -EINVAL;
		goto error;
	}

	switch (selector) {

	case STM_MEMSINK_USE_SHARED_DATA_POOL:
		MSS_ERROR_MSG("control not suported\n");
		ret = -ENOSYS;
		goto error;

	case STM_MEMSINK_OPAQUE_CTRL:
		break;

	default:
		ret = -EINVAL;
		goto error;
	}

	infra_os_mutex_lock(memsink_ctx->sink_mutex);

	if (memsink_ctx->pull_peer.set_compound_control) {
		ret = memsink_ctx->pull_peer.set_compound_control(
			      memsink_ctx->attach,
			      selector,
			      (void *) value,
			      size);
	} else
		ret = -ENXIO;

	infra_os_mutex_unlock(memsink_ctx->sink_mutex);

error:
	return ret;

}
EXPORT_SYMBOL(stm_memsink_set_compound_control);

int	stm_memsink_get_compound_control(stm_memsink_h	memsink_ctx,
					 stm_memsink_control_t selector,
					 void *value,
					 uint32_t size)
{

	struct stm_memsink_s *memsink_obj_p = memsink_ctx;
	int ret = 0;

	MSS_DEBUG_MSG(MEM_SINK, "(%p)\n", memsink_ctx);
	if (!memsink_obj_is_valid(memsink_obj_p))
		return -ENXIO;

	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_ATTACHED) {
		MSS_ERROR_MSG("memsink obj(%p) not attached\n ",
			      memsink_obj_p);
		return -EBUSY;
	}

	if (NULL == value) {
		ret = -EINVAL;
		goto error;
	}

	switch (selector) {

	case STM_MEMSINK_USE_SHARED_DATA_POOL:
		MSS_ERROR_MSG("control not suported\n");
		ret = -ENOSYS;
		goto error;

	case STM_MEMSINK_OPAQUE_CTRL:
		break;

	default:
		ret = -EINVAL;
		goto error;
	}

	infra_os_mutex_lock(memsink_ctx->sink_mutex);

	if (memsink_ctx->pull_peer.get_compound_control) {
		ret = memsink_ctx->pull_peer.get_compound_control(
			      memsink_ctx->attach,
			      selector,
			      (void *) value,
			      size);
	} else
		ret = -ENXIO;

	infra_os_mutex_unlock(memsink_ctx->sink_mutex);

error:
	return ret;

}
EXPORT_SYMBOL(stm_memsink_get_compound_control);

/*
*/
int
stm_memsink_pull_data(stm_memsink_h memsink_ctx,
		      void *data_addr,
		      uint32_t data_length,
		      uint32_t *extracted_size)
{
	struct stm_memsink_s *memsink_obj_p = memsink_ctx;
	uint32_t filled_blocks;
	int32_t read = 0;
	int32_t retval = 0;
	struct stm_data_block block_list;

	if (!memsink_obj_is_valid(memsink_obj_p)) {
		retval = -ENXIO;
		goto error;
	}

	infra_os_mutex_lock(memsink_ctx->sink_mutex);
	if ((!data_addr) || (!extracted_size)) {
		retval = -EINVAL;
		goto error;
	}

	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_ATTACHED) {
		MSS_ERROR_MSG("memsink object %p detached\n", memsink_ctx);
		retval = -ECONNRESET;
		goto error;
	}

	/* now this is done by attached src(producer) object */
/*
	if (memsink_obj_p->mode & STM_IOMODE_NON_BLOCKING_IO) {
		*extracted_size = 0;
		if (memsink_ctx->pull_peer.test_for_data) {
			retval = memsink_ctx->pull_peer.test_for_data(
					memsink_obj_p->attach, extracted_size);
		} else {
			retval = -ENXIO;
		}

		retval = (*extracted_size == 0) ? -EWOULDBLOCK : retval;
		if (retval != 0)
			return retval;

	}
*/

	/* If the address_space was user then use the user pages
	* otherwise use the address that was passed in as kernel */
	*extracted_size = 0;
	block_list.data_addr = data_addr;
	block_list.len = data_length;

	if (memsink_obj_p->pull_peer.pull_data) {

		switch (memsink_obj_p->address_space) {
		case USER:
			retval = sink_handle_pull_in_user_mode(memsink_obj_p,
				      &block_list, extracted_size);
			break;

		case KERNEL:
		{
			read = memsink_obj_p->pull_peer.pull_data(
				      memsink_obj_p->attach,
				      &block_list, 1, &filled_blocks);
			if (read < 0)
				retval = read;
			else
				*extracted_size = read;

		}
			break;

		case KERNEL_VMALLOC:
			retval = sink_handle_pull_kernel_vmalloc(
				      memsink_obj_p,
				      &block_list,
				      extracted_size);
			break;
		case PHYSICAL:
			retval = -EINVAL;
			break;
		}
	}

	infra_os_mutex_unlock(memsink_ctx->sink_mutex);
	return retval;

error:
	infra_os_mutex_unlock(memsink_ctx->sink_mutex);
	return retval;

}
EXPORT_SYMBOL(stm_memsink_pull_data);

int
stm_memsink_test_for_data(stm_memsink_h memsink_ctx, uint32_t *data)
{
	int32_t ret = 0;

	if (!memsink_obj_is_valid(memsink_ctx))
		return -ENXIO;

	infra_os_mutex_lock(memsink_ctx->sink_mutex);
	/*check attached state*/
	if (memsink_get_state(memsink_ctx) == STM_MEMSINK_STATE_ATTACHED) {
		*data = 0;
		if (memsink_ctx->pull_peer.test_for_data) {
			ret = memsink_ctx->pull_peer.test_for_data(
				      memsink_ctx->attach, data);
		} else {
			ret = -ENXIO;
		}
	} else {
		MSS_ERROR_MSG("error : memsink not attached\n");
		infra_os_mutex_unlock(memsink_ctx->sink_mutex);
		return -EPERM;
	}

	infra_os_mutex_unlock(memsink_ctx->sink_mutex);
	return ret;

}
EXPORT_SYMBOL(stm_memsink_test_for_data);

/*! \internal
\brief
this func is call when some object is attched to it
*/
static int
stm_memsink_attach_pull(stm_object_h source,
			stm_object_h memsink,
			struct stm_data_interface_pull_src *pull_src)
{
	struct stm_memsink_s *memsink_obj_p = memsink;
	int retval = 0;

	MSS_DEBUG_MSG(MEM_SINK,
		      "source %p to be attached to mem sink object %p\n",
		      source,
		      memsink);

	infra_os_mutex_lock(memsink_obj_p->sink_mutex);
	if (source == NULL) {
		MSS_ERROR_MSG("source (%p) to be attached is NULL\n", source);
		retval = -ENODEV;
		goto ret;
	}

	if (!memsink_obj_is_valid(memsink_obj_p)) {
		MSS_ERROR_MSG("invalid memsink obj %p\n", memsink_obj_p);
		retval = -ENXIO;
		goto ret;
	}

	if (memsink_obj_p->attach) {
		MSS_ERROR_MSG("already attached\n");
		retval = -EBUSY;
		goto ret;
	}

	/* no need to check again , already checked above */
	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_ATTACHED) {
		memcpy(&memsink_obj_p->pull_peer, pull_src, sizeof(*pull_src));
		memsink_obj_p->attach = source;
		memsink_set_state(memsink_obj_p, STM_MEMSINK_STATE_ATTACHED);
	} else {
		retval = -EBUSY;
		goto ret;
	}

ret:
	infra_os_mutex_unlock(memsink_obj_p->sink_mutex);
	return retval;
}

static int
stm_memsink_detach(stm_object_h source,
		   stm_object_h memsink)
{
	struct stm_memsink_s *memsink_obj_p = memsink;
	int retval = 0;

	MSS_DEBUG_MSG(MEM_SINK,
		      "source %p to be detached from memsink %p\n",
		      source,
		      memsink);

	infra_os_mutex_lock(memsink_obj_p->sink_mutex);

	if (!memsink_obj_is_valid(memsink_obj_p)) {
		MSS_ERROR_MSG("not expected memsink_ctx\n");
		retval = -ENXIO;
		goto ret;
	}

	if (source == NULL || memsink_obj_p->attach != source) {
		MSS_ERROR_MSG("Invalid Source=%p [Attached Source=%p]\n",
			      source, memsink_obj_p->attach);
		/* Has to be attached to the asked object */
		retval = -ENXIO;
		goto ret;
	}

	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_DETACHED) {
		memsink_obj_p->attach = NULL;
		/* reset pull_peer */
		memset(&(memsink_obj_p->pull_peer), 0,
			      sizeof(memsink_obj_p->pull_peer));
		memsink_set_state(memsink_obj_p, STM_MEMSINK_STATE_DETACHED);
	} else {
		MSS_ERROR_MSG(" memsink %p not attached to any object\n",
			      memsink_obj_p);
		retval = -EPERM;
		goto ret;
	}

ret:
	infra_os_mutex_unlock(memsink_obj_p->sink_mutex);
	return retval;

}

static int
stm_memsink_notify(stm_object_h memsink, unsigned int event_id)
{

	struct stm_memsink_s *memsink_obj_p = memsink;
	int ret = 0;
	stm_event_t memsink_event;

	MSS_DEBUG_MSG(MEM_SINK, "(%p)\n", memsink);

	/* checking the validity of memsink obj(incase it is already deleted)*/

	if (!memsink_obj_is_valid(memsink_obj_p)) {
		MSS_ERROR_MSG("not expected memsink_ctx\n");
		return -ENXIO;
	}

	if (memsink_get_state(memsink_obj_p) != STM_MEMSINK_STATE_ATTACHED) {
		MSS_ERROR_MSG(" memsink %p not attached to any object\n",
			      memsink_obj_p);
		return -EPERM;
	}

	switch (event_id) {
	case STM_MEMSINK_EVENT_DATA_AVAILABLE:
	case STM_MEMSINK_EVENT_BUFFER_OVERFLOW:
		memsink_event.event_id = event_id;
		break;

	default:
		MSS_ERROR_MSG("Not expected event Id\n");
		ret = -ENXIO;
	}

	if (!ret) {
		memsink_event.object = memsink;
		MSS_DEBUG_MSG(MEM_SINK, "signal %d for %p\n", event_id,
			      memsink_event.object);
		ret = stm_event_signal(&memsink_event);
	}

	return ret;
}

static int memsink_set_state(struct stm_memsink_s *memsink_p,
			     stm_memsink_state_t desired_state)
{
	int ret = 0;

	MSS_DEBUG_MSG(MEM_SINK, "set state of memsink_p (%p) to %d\n",
		      memsink_p, desired_state);
	memsink_p->cur_state = desired_state;

	return ret;
}

static stm_memsink_state_t memsink_get_state(struct stm_memsink_s *memsink_p)
{
	MSS_DEBUG_MSG(MEM_SINK, "get state of memsink_p (%p)\n", memsink_p);
	return memsink_p->cur_state;
}

int stm_memsink_set_iomode(stm_memsink_h memsink_ctx,
				     stm_memory_iomode_t iomode)
{
	int retval = 0;
	struct stm_memsink_s *memsink_obj_p = memsink_ctx;

	MSS_DEBUG_MSG(MEM_SINK, "set iomode %d of memsink_obj_p (%p)\n",
		      iomode, memsink_obj_p);

	if (!memsink_obj_is_valid(memsink_obj_p))
		return -ENXIO;

	infra_os_mutex_lock(memsink_obj_p->sink_mutex);
	if (memsink_obj_p->pull_interface.mode == iomode)
		goto exit;

	memsink_obj_p->mode = iomode;
	memsink_obj_p->pull_interface.mode = iomode;
	retval = stm_registry_update_attribute(memsink_obj_p, /* parent */
		      STM_DATA_INTERFACE_PULL, /*tag*/
		      STM_REGISTRY_ADDRESS, /* data type tag*/
		      &memsink_obj_p->pull_interface,
		      sizeof(memsink_obj_p->pull_interface));
	if (retval) {
		MSS_ERROR_MSG("error in stm_registry_update_attribute %d\n", retval);
		retval = -EINVAL;
		goto exit;
	}

exit:
	infra_os_mutex_unlock(memsink_obj_p->sink_mutex);
	return retval;
}
EXPORT_SYMBOL(stm_memsink_set_iomode);

/*EOF*/
