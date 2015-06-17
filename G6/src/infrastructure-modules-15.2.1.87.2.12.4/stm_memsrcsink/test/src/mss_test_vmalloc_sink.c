
#include "mss_test.h"
#include "mss_test_producer.h"

stm_memsink_h sink_vmalloc = (stm_memsink_h) NULL;

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

static int vmalloc_sink(stm_memsink_h sink_vmalloc)
{
	uint8_t *sink_buf;
	uint32_t read_size = 0;

	int retval = 0;

	sink_buf = vmalloc(512);

	if (sink_buf == NULL)
		return -ENOMEM;

	memset(sink_buf, 0x00, 1024);

	retval = stm_memsink_pull_data(sink_vmalloc,
		      sink_buf,
		      512,
		      &read_size);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Nothing pulld %p %p retval=%d\n",
				      sink_vmalloc,
				      sink_buf,
				      retval);
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Interrupted pulled %p %p retval=%d\n",
				      sink_vmalloc,
				      sink_buf,
				      retval);
			goto end;
		}
	} else {
		mss_test_etrace(MSS_TEST_MODULE,
			      " <%s> No. of bytes pulled : %d\n",
			      __FUNCTION__,
			      read_size);

		retval = validate_buffer(sink_buf, read_size, PULL_BYTE);

		if (retval != 0) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Wrong bytes pulled\n");
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      " %d bytes (0x%x) pulled OK\n",
				      read_size,
				      sink_buf[0]);
		}
	}

	vfree(sink_buf);
end:
	return retval;
}

int memsink_vmalloc_test(void)
{
	int retval = 0;
	stm_object_h producer;

	retval = stm_memsink_new("memsink_vmalloc",
		      STM_IOMODE_BLOCKING_IO,
		      KERNEL_VMALLOC,
		      &sink_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d creation memsnk object memsink_vmalloc\n",
			      retval);
		goto end;
	}

	mss_test_producer_create_new(&producer);
	retval = mss_test_producer_attach(producer, sink_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d attaching sink to src\n",
			      retval);
		goto end;
	}

	vmalloc_sink(sink_vmalloc);

	retval = mss_test_producer_detach(producer, sink_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d detach sink from src\n",
			      retval);
		goto end;
	}

	mss_test_dtrace(MSS_TEST_MODULE,
		      "%s : Delete sink obj = %p\n",
		      __FUNCTION__,
		      sink_vmalloc);

	retval = stm_memsink_delete(sink_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d deleting memsnk object %p\n",
			      retval,
			      sink_vmalloc);
		goto end;
	}
	/* free producer handle also */
	mss_test_producer_delete(producer);
end:
	return retval;

}
