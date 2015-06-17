
#include "mss_test.h"
#include "mss_test_producer.h"
#include "mss_test_consumer.h"

struct pp_descriptor {
	stm_object_h obj;
	struct io_descriptor *pull;
	struct io_descriptor *push;
	stm_object_h producer;
	stm_object_h consumer;
	int state;
	struct task_struct *pull_thread;
};

#define NB_INTERFACE 4
#define MSS_TEST_MAX_STR_LEN 40

enum api {
	NEW = 0,
	ATTACH,
	INIT_RESET,
	DETACH,
	DELETE,
	DETACH_DEL
};

char cmd_new[] = "new";
char cmd_attach[] = "attach";
char cmd_detach[] = "detach";
char cmd_delete[] = "delete";
char cmd_get_ctrl[] = "get_ctrl";
char cmd_set_ctrl[] = "set_ctrl";
char cmd_set_iomode[] = "set_iomode";

char cmd_vmalloc_push[] = "vmalloc_push";
char cmd_vmalloc_pull[] = "vmalloc_pull";

int parse_process_sink_api(const char *api);
int parse_process_src_api(const char *api);
int parse_process_kernel_ops(const char *cmd);

struct pp_descriptor memsrc_interface[NB_INTERFACE] = {{0} };
struct pp_descriptor memsink_interface[NB_INTERFACE] = {{0} };
struct dentry *d_sink_pull[NB_INTERFACE] = {NULL};
struct dentry *d_src_push[NB_INTERFACE] = {NULL};
struct dentry *d_init[NB_INTERFACE] = {NULL};

char *iomode_name[] = {
	[STM_IOMODE_BLOCKING_IO] = "WAIT",
	[STM_IOMODE_NON_BLOCKING_IO] = "NO_WAIT",
/*[STM_IOMODE_STREAMING_IO] = "STREAMING" TODO*/
};

int get_remaining_data(struct pp_descriptor *pp_desc, char *buf);
/* #define TEST_API_SIZE 360 */
static char SinkTestApi[10];
static char SrcTestApi[10];
static char kernel_func[MSS_TEST_MAX_STR_LEN-1];

static int validate_buffer(char *buffer, int size, char fill)
{
	int errorcode = 0;
	int i;
	for (i = 0; i < size; i++) {
		if (buffer[i] != fill) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error check :@0x%08x =0x%02hhx (expected 0x%02hhx)\n",
				      (int) &(buffer[i]), buffer[i], fill);
			errorcode = -1;
		}
	}
	return errorcode;
}

int addbytes(char __user *buf, size_t count, char *fmt, ...)
{
	va_list args;
	char *str;
	int nbbytes;

	str = kmalloc(count, GFP_KERNEL);
	if (!str)
		return -ENOMEM;


	va_start(args, fmt);
	nbbytes = vsnprintf(str, count, fmt, args);
	va_end(args);

	/* not useful nbbytes +=1 ; */ /* count the trailling 0, we add bytes */

	if (copy_to_user(buf, str, nbbytes))
		nbbytes = -EFAULT;

	kfree(str);
	return nbbytes;
}

static ssize_t push_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct pp_descriptor *pp_desc = file->private_data;
	int nbpush = 0;
	int retval = 0;

	memset(buf, PUSH_BYTE, pp_desc->push->size);
	retval = stm_memsrc_push_data(pp_desc->obj,
		      buf,
		      pp_desc->push->size,
		      &nbpush);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Nothing pushed %p %p %d current=%p retval=%d\n",
				      pp_desc->obj,
				      pp_desc->push->data,
				      pp_desc->push->size,
				      current,
				      retval);
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Interupted pushed %p %p %d current=%p retval=%d\n",
				      pp_desc->obj,
				      pp_desc->push->data,
				      pp_desc->push->size,
				      current,
				      retval);
			goto end;
		}

	} else {
		pr_info(" End push %p, bytes pushed = %d\n",
			      pp_desc->obj,
			      nbpush);
#if 0
		if (nbpull < pp_desc->pull->size) {
			mss_test_dtrace(MSS_TEST_MODULE,
				      "Wait for signal for remaining bytes\n");
			/* wait for signal */
		}
#endif
	}

end:
	return retval;
}

static ssize_t pull_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct pp_descriptor *pp_desc = file->private_data;
	int nbpull = 0;
	int retval;

	retval = stm_memsink_pull_data(pp_desc->obj,
		      buf,
		      pp_desc->pull->size,
		      &nbpull);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Nothing pulld %p %p %d current=%p retval=%d\n",
				      pp_desc->obj,
				      pp_desc->pull->data,
				      pp_desc->pull->size_allocated,
				      current,
				      retval);
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Interrupted pulled %p %p %d current=%p retval=%d\n",
				      pp_desc->obj,
				      pp_desc->pull->data,
				      pp_desc->pull->size_allocated,
				      current,
				      retval);
			goto end;
		}
	} else {
		mss_test_etrace(MSS_TEST_MODULE,
			      " <%s> No. of bytes pulled : %d\n",
			      __FUNCTION__,
			      nbpull);

		retval = validate_buffer(buf, nbpull, PULL_BYTE);

		if (retval != 0) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Wrong bytes pulled\n");
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      " %d bytes (0x%x) pulled OK\n",
				      nbpull,
				      buf[0]);
		}
	}

	if (nbpull < pp_desc->pull->size) {	/* for non blocking call */
		mss_test_dtrace(MSS_TEST_MODULE,
			      "Wait for signal for remaining bytes\n");
		/* wait for event */
		retval = get_remaining_data(pp_desc, buf);

	}

end:
	return retval;

}

/* Kthread function for reading from a memsink into a file */
int get_remaining_data(struct pp_descriptor *pp_desc, char *buf)
{
	int err = 0;
	unsigned int nb_evt = 0;
	uint32_t available_data = 0;
	uint32_t read_size = 0;

	stm_event_subscription_h evt_sub = NULL;
	stm_event_info_t evt_info;
	stm_event_subscription_entry_t memsink_data_evt = {
		.object = pp_desc->obj,
		.event_mask = STM_MEMSINK_EVENT_DATA_AVAILABLE,
		.cookie = NULL,
	};

	err = stm_event_subscription_create(&memsink_data_evt, 1,
		      &evt_sub);
	mss_test_etrace(MSS_TEST_MODULE,
		      "Subscribed to data event on memsink %p (err=%d)\n",
		      pp_desc->obj,
		      err);

	/* Event mode: Wait for event */

	err = stm_event_wait(evt_sub, 500, 1, &nb_evt,
		      &evt_info);
	if (!nb_evt || err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Event timeout for memsink %p err=%d\n",
			      pp_desc->obj,
			      err);
	}

	mss_test_etrace(MSS_TEST_MODULE,
		      "Received data event from memsink %p\n",
		      pp_desc->obj);

	err = stm_memsink_test_for_data(pp_desc->obj,
		      &available_data);
	if (err != 0) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in stm_memsink_test_for_data\n");
		/* return err; */
	}

	while (available_data) {
		err = stm_memsink_pull_data(pp_desc->obj, buf,
			      available_data,
			      &read_size);
		if (err == -EBUSY) {
			err = 0;
			continue;
		}
		if (err) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "stm_memsink_pull_data, memsnk%p ret%d",
				      pp_desc->obj, err);
			break;
		}
		mss_test_etrace(MSS_TEST_MODULE,
			      "read %d bytes of %d available\n",
			      read_size,
			      available_data);

		available_data -= read_size;
	}

	if (evt_sub) {
		err = stm_event_subscription_delete(evt_sub);
		if (err) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "error in deleting evt subscription\n");
		}
	}

	return err;
}

static int pp_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t init_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	int ret = 0;
	int i = (int) file->private_data;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "init_read %d\n", i);
	mss_test_dtrace(MSS_TEST_MODULE,
		      " %s memsnk = %p , memsrc = %p\n ",
		      __FUNCTION__,
		      memsink_interface[i].obj,
		      memsrc_interface[i].obj);

	if ((memsrc_interface[i].obj) && (memsink_interface[i].obj)) {
		ret =	delete_srcsinkobjects_i(i);
		if (ret)
			return ret;
	}

	ret = create_srcsinkobjects_i(i, NULL);
	return ret;
}

static ssize_t sink_api_test_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	int error = 0;
	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s\n",
		      __FUNCTION__);

	/* reset old API */
	memset((void *) SinkTestApi, 0, 10);
	if (copy_from_user(SinkTestApi, buf, count))
		return -EFAULT;
	error = parse_process_sink_api(SinkTestApi);

	return count;

}

static ssize_t src_api_test_write(struct file *file,
				  const char __user *buf,
				  size_t count,
				  loff_t *ppos)
{
	int error;
	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s\n",
		      __FUNCTION__);

	memset((void *) SrcTestApi, 0, 10);
	if (copy_from_user(SrcTestApi, buf, count))
		return -EFAULT;
	error = parse_process_src_api(SrcTestApi);
	return count;
}

static ssize_t kernel_ops_write(struct file *file,
				const char __user *buf,
				size_t count,
				loff_t *ppos)
{
	int error;
	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s\n",
		      __FUNCTION__);

	memset((void *) kernel_func, 0, 50);
	if (copy_from_user(kernel_func, buf, count))
		return -EFAULT;
	error = parse_process_kernel_ops(kernel_func);
	return count;
}

const struct file_operations push_ops = {
	.read = push_read,
	.open = pp_open
};

const struct file_operations pull_ops = {
	.read = pull_read,
	.open = pp_open
};

const struct file_operations init_ops = {
	.read = init_read,
	.open = pp_open
};

const struct file_operations src_api_ops = {
	.write = src_api_test_write,
	.open = pp_open
};

const struct file_operations sink_api_ops = {
	.write = sink_api_test_write,
	.open = pp_open
};

const struct file_operations kernel_ops = {
	.write = kernel_ops_write,
	.open = pp_open
};

int create_producer_sink_objects(stm_object_h *producer_p, stm_memsink_h *snk_p,
				 stm_memory_iomode_t snk_mode, char *snk_name,
				 int api)
{
	int retval = 0;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s\n",
		      __FUNCTION__);
	if (api == NEW || api == INIT_RESET) {
		retval = stm_memsink_new(snk_name, snk_mode, USER, snk_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d creation memsnk object %s)\n",
				      retval,
				      snk_name);
			goto end;
		}
	}

	if (api == ATTACH || api == INIT_RESET) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Attach producer_p=%p, snk_obj=%p (%s)\n",
			      *producer_p,
			      *snk_p,
			      snk_name);

		mss_test_producer_create_new(producer_p);
		retval = mss_test_producer_attach(*producer_p, *snk_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d attaching sink to src\n",
				      retval);
		}
		goto end;
	}

end:
	return retval;
}

int create_consumer_src_objects(stm_object_h *consumer_p,
				stm_memsrc_h *src_p,
				stm_memory_iomode_t src_mode, char *src_name,
				int api)
{
	int retval = 0;

	if (api == NEW || api == INIT_RESET) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "%s create new src object\n",
			      __FUNCTION__);
		retval = stm_memsrc_new(src_name, src_mode, USER, src_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Err %d creation memsrc object %s\n",
				      retval,
				      src_name);
			return retval;
		}

	}

	if (api == ATTACH || api == INIT_RESET) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "Attach src_obj=%p (%s), consumer_obj=%p\n",
			      *src_p,
			      src_name,
			      *consumer_p);

		mss_test_consumer_create_new(consumer_p);
		retval = stm_memsrc_attach(*src_p,
			      *consumer_p,
			      STM_DATA_INTERFACE_PUSH);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d attaching memsrc to consumer\n",
				      retval);
			return retval;
		}
	}

	return retval;
}

int delete_producer_sink_objects(stm_object_h producer_p,
				 stm_memsink_h snk_p,
				 int api)
{
	int retval = 0;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s\n",
		      __FUNCTION__);

	if (api == DETACH || api == DETACH_DEL) {
		retval = mss_test_producer_detach(producer_p, snk_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d detach sink from src\n",
				      retval);
			return retval;
		}
	}

	if (api == DELETE || api == DETACH_DEL) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "%s : Delete sink obj = %p\n",
			      __FUNCTION__,
			      snk_p);
		retval = stm_memsink_delete(snk_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d deleting memsnk object %p\n",
				      retval,
				      snk_p);
			return retval;
			goto end;
		}
		/* free producer handle also */
		mss_test_producer_delete(producer_p);
	}

end:
	return retval;

}

int delete_consumer_src_objects(stm_object_h consumer_p,
				stm_memsrc_h src_p,
				int api)
{
	int retval = 0;

	if (api == DETACH || api == DETACH_DEL) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "%s : Detach src_obj=%p\n",
			      __FUNCTION__,
			      src_p);
		retval = stm_memsrc_detach(src_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d detaching src\n", retval);
			goto end;
		}

	}

	if (api == DELETE || api == DETACH_DEL) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "%s : Delete src obj = %p\n",
			      __FUNCTION__,
			      src_p);

		retval = stm_memsrc_delete(src_p);
		if (retval) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Error %d creation memsrc object %p\n",
				      retval,
				      src_p);
			goto end;
		}
		/* free consumer handle also */
		mss_test_consumer_delete(consumer_p);
	}

end:
	return retval;

}

int delete_srcsinkobjects_i(int i)
{
	int retval;

	retval = delete_producer_sink_objects(
		      (stm_object_h) memsink_interface[i].producer,
		      (stm_memsink_h) memsink_interface[i].obj,
		      DETACH_DEL);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in delete_producer_sink_objects\n");
		return retval;
	}

	retval = delete_consumer_src_objects(
		      (stm_object_h) memsrc_interface[i].consumer,
		      (stm_memsrc_h) memsrc_interface[i].obj,
		      DETACH_DEL);
	return retval;

}

int create_srcsinkobjects_i(int i, struct dentry *d_memsrcsink)
{
	int retval;
	stm_memory_iomode_t push_mode;
	stm_memory_iomode_t pull_mode;
	char pull_name[32];
	char push_name[32];

	sprintf(pull_name, "sink%d", i);
	sprintf(push_name, "src%d", i);

	push_mode = (i & 1) ? STM_IOMODE_BLOCKING_IO : STM_IOMODE_NON_BLOCKING_IO;
	pull_mode = (i & 2) ? STM_IOMODE_NON_BLOCKING_IO : STM_IOMODE_BLOCKING_IO;

	retval = create_producer_sink_objects(
		      (stm_object_h *) &(memsink_interface[i].producer),
		      (stm_memsink_h *) &(memsink_interface[i].obj),
		      pull_mode,
		      pull_name,
		      INIT_RESET);

	if (!d_sink_pull[i]) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "Create %s mode %s\n",
			      pull_name,
			      iomode_name[pull_mode]);

		d_sink_pull[i] = debugfs_create_file(pull_name,
			      0644,
			      d_memsrcsink,
			      &memsink_interface[i],
			      &pull_ops);
	}

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s memsnk obj =%p\n",
		      __FUNCTION__,
		      memsink_interface[i].obj);

	retval = create_consumer_src_objects(
		      (stm_object_h *) &(memsrc_interface[i].consumer),
		      (stm_memsrc_h *) &(memsrc_interface[i].obj),
		      push_mode,
		      push_name,
		      INIT_RESET);

	if (!d_src_push[i]) {
		mss_test_dtrace(MSS_TEST_MODULE,
			      "Create %s mode %s\n",
			      push_name,
			      iomode_name[push_mode]);

		d_src_push[i] = debugfs_create_file(push_name,
			      0644,
			      d_memsrcsink,
			      &memsrc_interface[i],
			      &push_ops);
	}

	memsink_interface[i].state = 1;
	memsrc_interface[i].state = 1;
	return retval;
}

int mss_test_create_srcsink(struct dentry *d_memsrcsink)
{
	int i;
	char init_name[32];
	int err = 0;
	struct dentry *d_data;

	d_data = debugfs_create_dir("data", d_memsrcsink);

	for (i = 0; i < NB_INTERFACE; i++) {
		create_srcsinkobjects_i(i, d_data);

		memsrc_interface[i].push = &push;
		memsrc_interface[i].pull = NULL;

		memsink_interface[i].push = NULL;
		memsink_interface[i].pull = &pull;
		sprintf(init_name, "init%d", i);
		mss_test_dtrace(MSS_TEST_MODULE, "Create %s\n", init_name);
		/* Not as clean as possible, intead of i, shoudl pas &pull[i]
		* and &push[i] to much rework rigth now */
		d_init[i] = debugfs_create_file(init_name,
			      0644,
			      d_data,
			      (void *) i,
			      &init_ops);
	}

	debugfs_create_file("sink_api", 0777, d_data, (void *) i, &sink_api_ops);
	debugfs_create_file("src_api", 0777, d_data, (void *) i, &src_api_ops);
	debugfs_create_file("kernel_memtype", 0777, d_data, (void *) i, &kernel_ops);

	return err;
}

int mss_test_delete_srcsink(void)
{
	int i;
	int err = 0;

	/* cleanup created object */
	for (i = 0; i < NB_INTERFACE; i++) {
		err = delete_srcsinkobjects_i(i);
		/* debugfs file will be recusivly deleted */
		if (err)
			goto end;
	}

end:
	return err;
}

int test_iomode_sink_objects(stm_object_h snk_p)
{
	int err = 0;
	size_t actual_size;
	stm_data_interface_pull_sink_t pull_interface;
	struct stm_memsink_s *memsink_obj_p = snk_p;
	stm_memory_iomode_t mode;

	mss_test_dtrace(MSS_TEST_MODULE,
			     "testing iomode functionality for object (%p)\n", snk_p);

	err = stm_registry_get_attribute(memsink_obj_p,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS,
		      sizeof(stm_data_interface_pull_sink_t),
		      &pull_interface,
		      &actual_size);

	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in setting iomode\n");
		goto end;
	}

	mode = pull_interface.mode;

	/* Testing for STM_IOMODE_BLOCKING_IO */
	err = stm_memsink_set_iomode(memsink_obj_p, STM_IOMODE_BLOCKING_IO);

	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in setting iomode\n");
		goto end;
	}

	err = stm_registry_get_attribute(memsink_obj_p,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS,
		      sizeof(stm_data_interface_pull_sink_t),
		      &pull_interface,
		      &actual_size);

	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in getting registry attribute\n");
		goto end;
	}

	if (pull_interface.mode != STM_IOMODE_BLOCKING_IO) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "iomode not correctly retrieved\n");
		err = -1;
		goto end;
	}

	/* Testing for STM_IOMODE_NON_BLOCKING_IO */
	err = stm_memsink_set_iomode(memsink_obj_p, STM_IOMODE_NON_BLOCKING_IO);

	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in setting iomode\n");
		goto end;

	}

	err = stm_registry_get_attribute(memsink_obj_p,
		      STM_DATA_INTERFACE_PULL,
		      STM_REGISTRY_ADDRESS,
		      sizeof(stm_data_interface_pull_sink_t),
		      &pull_interface,
		      &actual_size);

	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in getting registry attribute\n");
		goto end;
	}

	if (pull_interface.mode != STM_IOMODE_NON_BLOCKING_IO) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "iomode not correctly retrieved\n");
		err = -1;
		goto end;
	}

	/* Restore the original mode */
	err = stm_memsink_set_iomode(memsink_obj_p, mode);
	if (err) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "error in setting iomode\n");
		goto end;
	}

	mss_test_dtrace(MSS_TEST_MODULE,
			     "succesfully tested iomode functionality\n\n");

end:
	return err;
}

int parse_process_sink_api(const char *api)
{
	int err = 0;
	int ctrl = 0;
	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s API to be tested(lenght : %d) :%s\n",
		      __FUNCTION__,
		      strlen(api),
		      api);

	if (!(strlen(api)))
		/* The command is empty */
		return -1;

	if (!strncmp(api, "new", strlen(cmd_new))) {

		err = create_producer_sink_objects(
			      (stm_object_h *) &(memsink_interface[0].producer),
			      (stm_memsink_h *) &(memsink_interface[0].obj),
			      STM_IOMODE_BLOCKING_IO,
			      "sink0",
			      NEW);
		goto end;
	}

	if (!strncmp(api, "attach", strlen(cmd_attach))) {

		err = create_producer_sink_objects(
			      (stm_object_h *) &(memsink_interface[0].producer),
			      (stm_memsink_h *) &(memsink_interface[0].obj),
			      STM_IOMODE_BLOCKING_IO,
			      "sink0",
			      ATTACH);
		goto end;
	}

	if (!strncmp(api, "get_ctrl", strlen(cmd_get_ctrl))) {
		err = stm_memsink_get_compound_control(
			      (stm_memsink_h) memsink_interface[0].obj,
			      STM_MEMSINK_OPAQUE_CTRL,
			      (void *) ctrl,
			      sizeof(int));
		goto end;
	}

	if (!strncmp(api, "set_ctrl", strlen(cmd_set_ctrl))) {
		err = stm_memsink_set_compound_control(
			      (stm_memsink_h) memsink_interface[0].obj,
			      STM_MEMSINK_OPAQUE_CTRL,
			      (void *) ctrl,
			      sizeof(int));
		goto end;
	}

	if (!strncmp(api, "detach", strlen(cmd_detach)))
		if (!strncmp(api, "detach", strlen(cmd_detach))) {

			err = delete_producer_sink_objects(
				      (stm_object_h) memsink_interface[0].producer,
				      (stm_memsink_h) memsink_interface[0].obj,
				      DETACH);
			goto end;
		}

	if (!strncmp(api, "delete", strlen(cmd_delete))) {

		err = delete_producer_sink_objects(
			      (stm_object_h) memsink_interface[0].producer,
			      (stm_memsink_h) memsink_interface[0].obj,
			      DELETE);
		goto end;
	}

	if (!strncmp(api, "set_iomode", strlen(cmd_set_iomode))) {

		err = test_iomode_sink_objects(
			      (stm_memsink_h) memsink_interface[0].obj
			      );
		goto end;
	}

	mss_test_dtrace(MSS_TEST_MODULE, "%s bad command\n", __FUNCTION__);
end:
	/* reset api str */

	return err;

}

int parse_process_src_api(const char *api)
{
	int err = 0;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s API to be tested : %s\n",
		      __FUNCTION__, api);
	if (!(strlen(api)))
		/* The command is empty */
		return -1;

	if (!strncmp(api, "new", strlen(cmd_new))) {
		err = create_consumer_src_objects(
			      (stm_object_h *) &(memsrc_interface[0].consumer),
			      (stm_memsrc_h *) &(memsrc_interface[0].obj),
			      STM_IOMODE_BLOCKING_IO,
			      "src0",
			      NEW);
		goto end;
	}

	if (!strncmp(api, "attach", strlen(cmd_attach))) {

		err = create_consumer_src_objects(
			      (stm_object_h *) &(memsrc_interface[0].consumer),
			      (stm_memsrc_h *) &(memsrc_interface[0].obj),
			      STM_IOMODE_BLOCKING_IO,
			      "src0",
			      ATTACH);
		goto end;
	}

	if (!strncmp(api, "detach", strlen(cmd_detach))) {

		err = delete_consumer_src_objects(
			      (stm_object_h) memsrc_interface[0].consumer,
			      (stm_memsrc_h) memsrc_interface[0].obj,
			      DETACH);
		goto end;
	}

	if (!strncmp(api, "delete", strlen(cmd_delete))) {

		err = delete_consumer_src_objects(
			      (stm_object_h) memsrc_interface[0].consumer,
			      (stm_memsrc_h) memsrc_interface[0].obj,
			      DELETE);
		goto end;
	}

	mss_test_etrace(MSS_TEST_PRODUCER, "%s bad command\n", __FUNCTION__);
end:
	/* reset api str */


	return err;

}

int parse_process_kernel_ops(const char *cmd)
{
	int err = 0;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s Kernel op to be tested : %s\n",
		      __FUNCTION__, cmd);

	if (!(strlen(cmd)))
		/* The command is empty */
		return -1;

	if (!strncmp(cmd, "vmalloc_push", strlen(cmd_vmalloc_push)))
		memsrc_vmalloc_test();


	if (!strncmp(cmd, "vmalloc_pull", strlen(cmd_vmalloc_pull)))
		memsink_vmalloc_test();


	return err;	/* TODO : add proper error handling */
}

/*** MODULE LOADING ******************************************************/

void mss_test_modprobe_func(void)
{
	pr_info("This is INFRA's MSS TEST Module\n");
}
EXPORT_SYMBOL(mss_test_modprobe_func);

/* EOF */
