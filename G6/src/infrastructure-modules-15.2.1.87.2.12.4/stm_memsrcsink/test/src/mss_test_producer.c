
#include "mss_test.h"
#include "mss_test_producer.h"

#define PULL_BYTE 20

size_t producer_size = 512;

typedef enum stm_producer_state_e {
	STM_PRODUCER_STATE_DETACHED,
	STM_PRODUCER_STATE_ATTACHED
} stm_producer_state_t;

struct stm_producer_s;

struct stm_producer_s {
	int x;	/* TODO : producer related attribute /function */
	stm_producer_state_t	state;
	stm_memory_iomode_t	iomode;
	infra_os_mutex_t	producer_mutex;
	stm_object_h		external_sink_connection;
	uint32_t		data_count;
	struct task_struct	*read_notify_thread;
	struct stm_data_interface_pull_sink pull_intf;
	int			notify_required;
};

typedef struct stm_producer_s *stm_producer_h;

void get_datacount(stm_object_h producer_p, uint32_t *data);
int producer_read_notify_task(stm_producer_h	producer_p);

static int producer_fill_block(struct stm_data_block *block_list,
			       uint32_t block_count,
			       uint32_t *filled_blocks,
			       int producer_count);

int mss_producer_wait_for_data(stm_object_h producer_p, uint32_t *size);
static int mss_test_pull_data(stm_object_h producer_object,
			      struct stm_data_block *block_list,
			      uint32_t block_count,
			      uint32_t *filled_blocks)
{
	uint32_t index;
	struct stm_data_block *this_block;

	int error = 0;
	uint32_t producer_count = 0;
	uint32_t requested_count = 0;
	uint32_t data_remaining = 0;

	struct stm_producer_s *producer_obj_p = producer_object;
	*filled_blocks = 0;

	/* check for the attached state . If not attached return error . */
	if (producer_obj_p->state != STM_PRODUCER_STATE_ATTACHED) {
		mss_test_etrace(MSS_TEST_PRODUCER, "not attached\n");
		return -EPERM;	/* check the error type */
	}

	/* if producer_size >= Requested , then copy_data and return(requested)
	* if producer_size < Requested , then check iomode
	* if not blocking : copy_data and return producer_size
	* if blocking : wait for signal (wait until data ready or detached)
	*/


	for (index = 0; (index < block_count); index++) {
		this_block = &block_list[index];
		requested_count += this_block->len;
	}

	get_datacount(producer_obj_p, &producer_count);	/* extract producer_size */

	data_remaining = producer_count;

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "requested_count is %d and the producer_count is %d\n",
		      requested_count, producer_count);

	if (producer_obj_p->iomode == STM_IOMODE_NON_BLOCKING_IO) {
		if (producer_count >= requested_count) {
			/* copy data */
			producer_fill_block(block_list,
				      block_count,
				      filled_blocks,
				      requested_count);

			mss_test_dtrace(MSS_TEST_PRODUCER,
				      "Return %d number of bytes\n",
				      requested_count);
			return requested_count;
		} else {	/* producer_count < requested */
			/* copy data */
			producer_fill_block(block_list,
				      block_count,
				      filled_blocks,
				      producer_count);

			mss_test_dtrace(MSS_TEST_PRODUCER,
				      "Return %d number of bytes\n",
				      producer_count);

			producer_obj_p->notify_required = 1;
			return producer_count;
		}
	}

	if (producer_obj_p->iomode == STM_IOMODE_BLOCKING_IO) {
		if (producer_count >= requested_count) {
			/* copy data */
			producer_fill_block(block_list,
				      block_count,
				      filled_blocks,
				      requested_count);

			mss_test_dtrace(MSS_TEST_PRODUCER,
				      "Return %d number of bytes\n",
				      requested_count);

			return requested_count;
		} else {	/* producer_count < requested */
			/* wait for data
			* will be called in practical cases.
			* Here data is readily available.so commenting it.
			* mss_producer_wait_for_data();
			*/
			producer_fill_block(block_list,
				      block_count,
				      filled_blocks,
				      requested_count);

			mss_test_dtrace(MSS_TEST_PRODUCER,
				      "Return %d number of bytes\n",
				      requested_count);
			return requested_count;
		}
	}

	return error;
}

static int producer_fill_block(struct stm_data_block *block_list,
			       uint32_t block_count,
			       uint32_t *filled_blocks,
			       int producer_count)
{

	int error = 0;

	int data_remaining = producer_count;
	while (!error && block_count > 0 && data_remaining > 0) {

		/* if(block_list->len > min_t(uint32_t,block_list->len,data_remaining))
		*	block_list->len = min_t(uint32_t,block_list->len,data_remaining);
		*/

		memset((uint8_t *) block_list->data_addr, PULL_BYTE, block_list->len);
		data_remaining -= block_list->len;
		(*filled_blocks)++;
		block_count--;
		block_list++;
	}
	return 0;
}

static int mss_test_for_data(stm_object_h src_object, uint32_t *size)
{
	uint32_t result = 0;
	*size = producer_size;
	return result;
}

static int mss_set_compound_control(stm_object_h src_object,
				    stm_memsink_control_t selector,
				    const void *value,
				    uint32_t	size)
{
	/* TODO */
	return 0;
}

static int mss_get_compound_control(stm_object_h src_object,
				    stm_memsink_control_t selector,
				    void *value,
				    uint32_t	size)
{
	/* TODO */
	return 0;
}

static struct stm_data_interface_pull_src mss_test_pull_interface = {
	mss_test_pull_data,
	mss_test_for_data,
	mss_set_compound_control,
	mss_get_compound_control
};

uint32_t mss_test_producer_attach(stm_object_h producer_p,
				  stm_memsink_h snk_p)
{
	uint32_t err = 0;
	size_t actual_size;
	stm_object_h target_type;

	struct stm_producer_s *producer_obj_p = producer_p;

	/* check for NULL producer_p and snk_p */

	/* check if producer is already attached to any target */
	if (producer_obj_p->external_sink_connection) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "test prod obj(%p) already has an attachment\n",
			      producer_obj_p);
		return -EBUSY;
	}

	err = stm_registry_get_object_type(snk_p, &target_type);
	if (0 != err) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "unable to get type of sink object type %p\n",
			      snk_p);
	}
	err = stm_registry_get_attribute(target_type,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS,
		      sizeof(stm_data_interface_pull_sink_t),
		      &producer_obj_p->pull_intf,
		      &actual_size);

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "Attach src_obj=%p , snk_obj=%p\n",
		      producer_obj_p,
		      snk_p);
	if (err) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "error in getting registry attribute\n");
		return err;
	}

	err = producer_obj_p->pull_intf.connect(producer_p,
		      snk_p,
		      &mss_test_pull_interface);
	if (err) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "Fail to connect producer to sink %p --> %p\n",
			      producer_obj_p,
			      snk_p);
		return err;
	}

	producer_obj_p->external_sink_connection = snk_p;

	/* Change according to new design . Producer should be aware of
	* iomode now */

	producer_obj_p->iomode = producer_obj_p->pull_intf.mode;

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "sink interface iomode is %d\n",
		      producer_obj_p->iomode);

	infra_os_mutex_lock(producer_obj_p->producer_mutex);

	producer_obj_p->state = STM_PRODUCER_STATE_ATTACHED;

	infra_os_mutex_unlock(producer_obj_p->producer_mutex);

	if (producer_obj_p->iomode == STM_IOMODE_NON_BLOCKING_IO) {
		mss_test_dtrace(MSS_TEST_PRODUCER,
			      "create kthread for read and notify task\n");
		producer_obj_p->read_notify_thread =
			kthread_run((int (*)(void *))producer_read_notify_task,
			      producer_obj_p,
			      "MssTestProducer");
		if (producer_obj_p->read_notify_thread < 0) {
			mss_test_etrace(MSS_TEST_PRODUCER,
				      "error in creating notify thread\n");
		}

		producer_obj_p->notify_required = 0;

	}

	return err;
}

uint32_t mss_test_producer_detach(stm_object_h producer_p,
				  stm_memsink_h snk_p)
{
	uint32_t error_code = 0;
	struct stm_data_interface_pull_sink pull_interface;
	size_t actual_size;
	stm_object_h target_type;

	struct stm_producer_s	*producer_obj_p = producer_p;

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "producer_p (%p) , sink_p (%p)\n",
		      producer_p,
		      snk_p);

	/* check if producer is already attached to any target */
	if (producer_obj_p->external_sink_connection != snk_p) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      " snk_p(%p) not attached to producer obj(%p)\n",
			      snk_p,
			      producer_obj_p);

		mss_test_etrace(MSS_TEST_PRODUCER,
			      " external_sink_connection(%p) is attchd to producer (%p)\n",
			      producer_obj_p->external_sink_connection,
			      producer_obj_p);
		return -EINVAL;
	}

	error_code = stm_registry_get_object_type(snk_p, &target_type);

	if (error_code) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      " error in stm_registry_get_object_type\n");
		return -EINVAL;
	}

	error_code = stm_registry_get_attribute(target_type,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS,
		      sizeof(pull_interface),
		      &pull_interface,
		      &actual_size);

	if (error_code) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      " error in stm_registry_get_attribute\n");
		return -EINVAL;
	}

	infra_os_mutex_lock(producer_obj_p->producer_mutex);
	producer_obj_p->state = STM_PRODUCER_STATE_DETACHED;
	infra_os_mutex_unlock(producer_obj_p->producer_mutex);

	/* signal waiting threads */

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "call memsink disconnect .producer_p(%p), sink_p(%p)\n",
		      producer_p,
		      snk_p);

	error_code = pull_interface.disconnect(producer_p, snk_p);

	if (!error_code)
		producer_obj_p->external_sink_connection = NULL;


	/* Stop read notify task */
	if (producer_obj_p->read_notify_thread)
		kthread_stop(producer_obj_p->read_notify_thread);

	return error_code;
}

int mss_test_producer_create_new(stm_object_h *producer_ctx)
{

	struct stm_producer_s *producer_obj_p;
	int retval;
	producer_obj_p = infra_os_malloc(sizeof(*producer_obj_p));
	if (!producer_obj_p)
		return -ENOMEM;


	retval = infra_os_mutex_initialize(&(producer_obj_p->producer_mutex));
	if (retval != 0)
		return retval;

	*producer_ctx = (stm_object_h) producer_obj_p;
	producer_obj_p->state = STM_PRODUCER_STATE_DETACHED;
	producer_obj_p->external_sink_connection = NULL;
	return retval;
}

int mss_test_producer_delete(stm_object_h producer_p)
{

	struct stm_producer_s *producer_obj_p = producer_p;

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "Delete producer object (%p)\n",
		      producer_obj_p);

	if (producer_obj_p == NULL) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "producer_obj_p(%p) already deleted\n",
			      producer_obj_p);
		return -ENXIO;
	}

	if (producer_obj_p->external_sink_connection) {
		mss_test_etrace(MSS_TEST_PRODUCER,
			      "Can't delete : producer_obj_p(%p) is attached\n",
			      producer_obj_p);
		return -EBUSY;
	}

	producer_obj_p->external_sink_connection = NULL;
	infra_os_mutex_terminate(producer_obj_p->producer_mutex);
	infra_os_free(producer_p);
	return 0;
}

void get_datacount(stm_object_h producer_p, uint32_t *data)
{
	*data = producer_size;
	return;

}

int mss_producer_wait_for_data(stm_object_h producer_p,
			       uint32_t *size)
{
/*	int err = 0; */

	struct stm_producer_s *producer_obj_p = producer_p;
	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "producer obj %p\n",
		      producer_obj_p);
	do {
		/* check if handle is still valid (detached) */
		if (producer_obj_p->state == STM_PRODUCER_STATE_DETACHED) {
			mss_test_etrace(MSS_TEST_PRODUCER,
				      "Src (producer %p)detached from memsink\n",
				      producer_p);
			return -1;
		}
		/* test_for_data
		* if no data
		* wait for some time (delay)
		*/

		mss_test_for_data(producer_p, size);
		if (0 == *size) {
			uint32_t interrupted;
			set_current_state(TASK_INTERRUPTIBLE);
			interrupted = schedule_timeout(msecs_to_jiffies(10));
			if (interrupted)
				return -EINTR;
		}
	} while (0 == *size);

/*	return err; */
	return 0;
}

int producer_read_notify_task(stm_producer_h producer_obj_p)
{
	/* if count >0 , notify */
	/* ig count < 0 , wait_for_signal */
	int err = 0;

	mss_test_dtrace(MSS_TEST_PRODUCER,
		      "%p\n",
		      producer_obj_p);

	while (!kthread_should_stop()) {
		msleep(40);
		/* adding sleep to emulate time required to get data ready . may not be required in practical scnarios. */
		infra_os_mutex_lock(producer_obj_p->producer_mutex);
		if (producer_obj_p->notify_required == 1) {
			if (producer_obj_p->state != STM_PRODUCER_STATE_ATTACHED) {
				mss_test_dtrace(MSS_TEST_PRODUCER,
					      "%p not in attached state.Terminate notify thread\n",
					      producer_obj_p);

				/* terminate thread */
				kthread_stop(producer_obj_p->read_notify_thread);
				return -1;	/* which error to be returned? */
				infra_os_mutex_unlock(producer_obj_p->producer_mutex);
			} else {
				/* Notify pull sink that section is available
				* (if pull sink notify function exists)
				*/
				if (producer_obj_p->pull_intf.notify) {
					err = producer_obj_p->pull_intf.notify(
						      producer_obj_p->external_sink_connection,
						      STM_MEMSINK_EVENT_DATA_AVAILABLE);
					if (err)
						mss_test_etrace(MSS_TEST_PRODUCER, "Notify sink %p failed (%d)\n",
							      producer_obj_p->external_sink_connection,
							      err);
				}
				producer_obj_p->notify_required = 0;

			}
		}

		infra_os_mutex_unlock(producer_obj_p->producer_mutex);
	}

	return err;
}
