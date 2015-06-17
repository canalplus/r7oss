
#include "mss_test.h"
#include "mss_test_consumer.h"

stm_memsrc_h src_vmalloc = (stm_memsrc_h) NULL;
#define VMALLOC_SRC_SIZE 512

static int vmalloc_src_data(stm_memsrc_h src_vmalloc)
{
	uint8_t *src_buf;
	uint32_t write_size = 0;

	int retval = 0;

	src_buf = vmalloc(VMALLOC_SRC_SIZE);

	if (src_buf == NULL)
		return -ENOMEM;

	memset(src_buf, PUSH_BYTE, VMALLOC_SRC_SIZE);

	retval = stm_memsrc_push_data(src_vmalloc,
		      src_buf,
		      VMALLOC_SRC_SIZE,
		      &write_size);
	if (retval < 0) {
		if (retval == -EAGAIN) {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Nothing pushed %p %p retval=%d\n",
				      src_vmalloc,
				      src_buf,
				      retval);
		} else {
			mss_test_etrace(MSS_TEST_MODULE,
				      "Interrupted pushed %p %p retval=%d\n",
				      src_vmalloc,
				      src_buf,
				      retval);
			goto end;
		}
	} else {
		mss_test_etrace(MSS_TEST_MODULE,
			      " <%s> No. of bytes pushed : %d\n",
			      __FUNCTION__,
			      write_size);

	}

	vfree(src_buf);
end:
	return retval;
}

int memsrc_vmalloc_test(void)
{
	int retval = 0;
	stm_object_h consumer;

	retval = stm_memsrc_new("memsrc_vmalloc",
		      STM_IOMODE_BLOCKING_IO,
		      KERNEL_VMALLOC,
		      &src_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d creation memsnk object memsrc_vmalloc\n",
			      retval);
		goto end;
	}

	mss_test_consumer_create_new(&consumer);
	retval = stm_memsrc_attach(src_vmalloc,
		      consumer, STM_DATA_INTERFACE_PUSH);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d attaching memsrc to consumer\n",
			      retval);
		goto end;
	}

	vmalloc_src_data(src_vmalloc);

	retval = stm_memsrc_detach(src_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d detaching src\n", retval);
		goto end;
	}

	retval = stm_memsrc_delete(src_vmalloc);
	if (retval) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Error %d creation memsrc object %p\n",
			      retval,
			      src_vmalloc);
		goto end;
	}

	/* free consumer handle also */
	mss_test_consumer_delete(consumer);
end:
	return retval;

}
