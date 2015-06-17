
#include "mss_test.h"
#include "mss_test_consumer.h"

size_t consumer_size = 1024;

#define PUSH_BYTE	10

struct stm_consumer_class_s {
	stm_object_h myself;
};
static struct stm_consumer_class_s consumer_type = {0};

typedef enum stm_consumer_state_e {
	STM_CONSUMER_STATE_DETACHED,
	STM_CONSUMER_STATE_ATTACHED
} stm_consumer_state_t;

struct stm_consumer_s;

struct stm_consumer_s {
	stm_consumer_state_t	state;
	stm_memory_iomode_t	iomode;
	infra_os_mutex_t	consumer_mutex;
	stm_object_h		external_src_connection;
/*	uint32_t		data_count; */
/*	struct task_struct	*read_notify_thread; */
/*	struct stm_data_interface_pull_sink pull_intf; */
/*	int			notify_required; */
};

typedef struct stm_consumer_s *stm_consumer_h;

static int validate_buffer(char *buffer, int size, char fill)
{
	int errorcode = 0;
	int i;
	for (i = 0; i < size; i++) {
		if (buffer[i] != fill) {
			mss_test_etrace(MSS_TEST_CONSUMER,
				      "Error check :@0x%08x =0x%02hhx (expected 0x%02hhx)\n",
				      (int) &(buffer[i]), buffer[i], fill);
			errorcode = -1;
		}
	}
	return errorcode;
}

static int mss_test_push_connect(stm_object_h src_object,
				 stm_object_h consumer_object)
{
	struct stm_consumer_s *consumer_obj_p = consumer_object;
	stm_object_h cons_type;
	int retval;

	mss_test_dtrace(MSS_TEST_CONSUMER,
		      "consumer_object: %p, src_object: %p\n",
		      consumer_obj_p, src_object);
	retval = stm_registry_get_object_type(consumer_obj_p, &cons_type);
	if ((retval) || (cons_type != consumer_type.myself)) {
		/* Error: object expected is not a consumer object */
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "object (%p) is not a consumer object\n",
			      consumer_obj_p);
		return -ENXIO;
	}

	mss_test_dtrace(MSS_TEST_CONSUMER,
		      "attaching consumer obj (%p) with src object (%p)\n",
		      consumer_obj_p,
		      src_object);

	if (consumer_obj_p->external_src_connection) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      " test consumer obj(%p) already has an attachment\n",
			      consumer_obj_p);
		return -EBUSY;
	}

	infra_os_mutex_lock(consumer_obj_p->consumer_mutex);

	consumer_obj_p->external_src_connection = src_object;
	consumer_obj_p->state = STM_CONSUMER_STATE_ATTACHED;

	infra_os_mutex_unlock(consumer_obj_p->consumer_mutex);

	return retval;
}

static int mss_test_push_disconnect(stm_object_h src_object,
				    stm_object_h consumer_object)
{
	/* TODO : check the responsibility of this disconnect function .
	* This should be defined properly.*/

	struct stm_consumer_s *consumer_obj_p = consumer_object;
	int ret = 0;
	mss_test_dtrace(MSS_TEST_CONSUMER,
		      "consumer_object: %p, src_object: %p\n",
		      consumer_object,
		      src_object);

	if (src_object == NULL ||
		consumer_obj_p->external_src_connection != src_object) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "Invalid Source=%p [Attached Source=%p]\n",
			      src_object,
			      consumer_obj_p->external_src_connection);

		/* Has to be attached to the asked object */
		return -ENXIO;
	}

	infra_os_mutex_lock(consumer_obj_p->consumer_mutex);

	consumer_obj_p->external_src_connection = NULL;
	consumer_obj_p->state = STM_CONSUMER_STATE_DETACHED;

	infra_os_mutex_unlock(consumer_obj_p->consumer_mutex);

	return ret;
}

static int mss_test_push_data(stm_object_h consumer_object,
			      struct stm_data_block *block_list,
			      uint32_t block_count, uint32_t *data_blocks)
{

	uint32_t index;
	struct stm_data_block *this_block;
	uint32_t bytes_pushed = 0;
	int retval = 0;

	mss_test_dtrace(MSS_TEST_CONSUMER, "\n");

	mss_test_etrace(MSS_TEST_CONSUMER, "Data byte :\n");

	for (index = 0; (index < block_count); index++) {
		this_block = &block_list[index];
		retval = validate_buffer(this_block->data_addr,
			      this_block->len,
			      PUSH_BYTE);
		if (retval != 0) {
			mss_test_etrace(MSS_TEST_CONSUMER,
				      "Wrong bytes pulled\n");
		}
#if 0
		for (int i = 0; i < this_block->len; i++)
			pr_info("%d ", *((uint8_t *) this_block->data_addr));

#endif
		bytes_pushed += this_block->len;
	}

	mss_test_dtrace(MSS_TEST_CONSUMER, "bytes_pushed %d\n ", bytes_pushed);
	*data_blocks = index;

	return retval;

}

static stm_data_interface_push_sink_t mss_data_push_interface = {
	mss_test_push_connect,
	mss_test_push_disconnect,
	mss_test_push_data,
	KERNEL,
	STM_IOMODE_BLOCKING_IO,
	ALIGN_CACHE_LINE,
	PAGE_SIZE,
	0
};

int mss_test_consumer_init(void)
{
	uint32_t result;
	result =
		stm_registry_add_object(STM_REGISTRY_TYPES, "consumer_type",
		(stm_object_h) &consumer_type);

	if (result)
		pr_err(" %s Add object failure\n", __FUNCTION__);


	consumer_type.myself = (stm_object_h) &consumer_type;

	result = stm_registry_add_attribute(consumer_type.myself, /* parent */
		STM_DATA_INTERFACE_PUSH, /* tag */
		STM_REGISTRY_ADDRESS, /* data type tag */
		&mss_data_push_interface, /* value */
		sizeof(mss_data_push_interface));

	if (result)
		pr_err("stm_registry_add_attribute failed to register "
			      "consumer interface\n");

	return 0;
}

void mss_test_consumer_term(void)
{
	uint32_t retval;

	retval = stm_registry_remove_attribute(consumer_type.myself,
		      STM_DATA_INTERFACE_PUSH);
	/*if (retval)
		 TODO error handling */

	consumer_type.myself = NULL;
	retval = stm_registry_remove_object((stm_object_h) &consumer_type);
	/*if (retval)
		 error handling */

	return;
}

int mss_test_consumer_create_new(stm_object_h *consumer_ctx)
{
	struct stm_consumer_s *consumer_obj_p;
	uint32_t retval;

	consumer_obj_p = infra_os_malloc(sizeof(*consumer_obj_p));
	if (!consumer_obj_p)
		return -ENOMEM;


	*consumer_ctx = (stm_object_h) consumer_obj_p;

	mss_test_dtrace(MSS_TEST_CONSUMER,
		      "\n Add instance to registry : consumer_p : %p ,consumer_type.myself :%p\n",
		      *consumer_ctx,
		      consumer_type.myself);

	retval = stm_registry_add_instance(STM_REGISTRY_INSTANCES, /* parent */
		      consumer_type.myself, /* type */
		      NULL,
		      (stm_object_h) *consumer_ctx);

	if (retval) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "error in stm_registry_add_instance\n");
		return retval;
	}

	retval = infra_os_mutex_initialize(&(consumer_obj_p->consumer_mutex));
	if (retval != 0)
		return retval;

	consumer_obj_p->state = STM_CONSUMER_STATE_DETACHED;
	consumer_obj_p->external_src_connection = NULL;

	return retval;

}

int mss_test_consumer_delete(stm_object_h consumer_p)
{
	int retval = 0;
	struct stm_consumer_s *consumer_obj_p = consumer_p;

	mss_test_dtrace(MSS_TEST_CONSUMER, "%s\n", __FUNCTION__);

	if (consumer_obj_p == NULL) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "consumer_obj_p (%p) already deleted\n",
			      consumer_obj_p);
		return -ENXIO;
	}

	if (consumer_obj_p->external_src_connection) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "Cannot delete as consumer_obj_p (%p) is attached\n ",
			      consumer_obj_p);
		return -EBUSY;
	}

	retval = stm_registry_remove_object((stm_object_h) consumer_obj_p);
	if (retval) {
		mss_test_etrace(MSS_TEST_CONSUMER,
			      "error in stm_registry_remove_object consumer_obj_p(%p) is attached\n ",
			      consumer_obj_p);
		return retval;

	}
	consumer_obj_p->external_src_connection = NULL;
	infra_os_mutex_terminate(consumer_obj_p->consumer_mutex);
	infra_os_free(consumer_p);

	return retval;
}

