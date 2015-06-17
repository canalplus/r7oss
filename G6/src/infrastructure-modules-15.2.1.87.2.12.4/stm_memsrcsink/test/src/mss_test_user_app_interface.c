
#include "mss_test_user_app_interface.h"

static int32_t mss_src_open(struct inode *inode, struct file *file);
static int32_t mss_src_close(struct inode *inode, struct file *file);
#if 0
static ssize_t mss_src_read(struct file *file_p, /* see include/linux/fs.h */
			    char *buffer, /* buffer to fill with data */
			    size_t length, /* length of the buffer */
			    loff_t *offset);
#endif
static ssize_t mss_src_write(struct file *file_p, /* see include/linux/fs.h */
			     const char *buffer, /* buffer to fill with data */
			     size_t length, /* length of the buffer	*/
			     loff_t *offset);
/* static long mss_src_ioctl(struct file *file, unsigned int command, unsigned long data); */

static int32_t mss_sink_open(struct inode *inode, struct file *file);
static int32_t mss_sink_close(struct inode *inode, struct file *file);

static ssize_t mss_sink_read(struct file *file_p, /* see include/linux/fs.h */
			     char *buffer, /* buffer to fill with data */
			     size_t length, /* length of the buffer */
			     loff_t *offset);
#if 0
static ssize_t mss_src_write(struct file *file_p, /* see include/linux/fs.h */
			     const char *buffer, /* buffer to fill with data */
			     size_t length, /* length of the buffer */
			     loff_t *offset);
static long mss_src_ioctl(struct file *file, unsigned int command, unsigned long data);
#endif

static long mss_sink_ioctl(struct file *file, unsigned int command, unsigned long data);

static struct file_operations fops_src = {
	/* .read = mss_stm_read, */
	.write = mss_src_write,
	.open = mss_src_open,
	.release = mss_src_close,
	/* .unlocked_ioctl = mss_src_ioctl */
};

static struct file_operations fops_sink = {
	.read = mss_sink_read,
	/* .write = mss_stm_write, */
	.open = mss_sink_open,
	.release = mss_sink_close,
	.unlocked_ioctl = mss_sink_ioctl
};

static uint32_t major_src;
static uint32_t major_sink;
static	stm_memsrc_h memsrc_hdl;
static	stm_memsink_h memsink_hdl;
stm_object_h producer;

int32_t mss_register_char_driver(void)
{
	major_src = register_chrdev(0, MEMSRC_NAME, &fops_src);
	if (major_src < 0) {
		mss_error_print("Registering char device failed\n");
		return major_src;
	}
	pr_info("%s register_chrdev successful with major no. %d\n",
		      __FUNCTION__,
		      major_src);

	major_sink = register_chrdev(0, MEMSINK_NAME, &fops_sink);
	if (major_sink < 0) {
		mss_error_print("Registering char device failed\n");
		return major_sink;
	}

	pr_info("%s register_chrdev successful with major no. %d\n",
		      __FUNCTION__,
		      major_sink);
	return 0;
}

void mss_unregister_char_driver(void)
{
	unregister_chrdev(major_src, MEMSRC_NAME);
	unregister_chrdev(major_sink, MEMSINK_NAME);
}

static int32_t mss_src_open(struct inode *inode, struct file *file)
{
	uint32_t error_code;

	int32_t num = MINOR(inode->i_rdev);

	error_code = stm_memsrc_new("mss_test_src0",
		      STM_IOMODE_BLOCKING_IO, USER, &memsrc_hdl);
	if (error_code < 0)
		mss_error_print("MSS_src open failed with %d\n", num);
	else
		file->private_data = memsrc_hdl;
	return error_code;
}

static int32_t mss_src_close(struct inode *inode, struct file *file)
{
	uint32_t error_code;
	int32_t num = MINOR(inode->i_rdev);
	mss_debug_print(" <%s> :: <%d> Minor No. %d\n", __FUNCTION__,
		      __LINE__,
		      num);
	error_code = stm_memsrc_delete(((stm_memsrc_h) file->private_data));
	if (error_code < 0)
		mss_error_print("mss_src close failed	with %d\n", num);
	return error_code;
}

static int32_t mss_sink_open(struct inode *inode, struct file *file)
{
	uint32_t error_code;

	int32_t num = MINOR(inode->i_rdev);

	mss_test_producer_create_new(&producer);
	error_code = stm_memsink_new("mss_test_sink0",
		      STM_IOMODE_BLOCKING_IO,
		      USER,
		      &memsink_hdl);
	if (error_code < 0)
		mss_error_print("MSS_sink open failed	with %d\n", num);
	else
		file->private_data = memsink_hdl;

	return error_code;
}

static int32_t mss_sink_close(struct inode *inode, struct file *file)
{
	uint32_t error_code;
	int32_t num = MINOR(inode->i_rdev);
	mss_debug_print(" <%s> :: <%d> Minor No. %d\n",
		      __FUNCTION__,
		      __LINE__,
		      num);

	error_code = stm_memsink_delete(((stm_memsink_h) file->private_data));
	if (error_code < 0)
		mss_error_print("mss_sink close failed with %d\n", num);
	return error_code;
}

static ssize_t mss_sink_read(struct file *file_p, /* see include/linux/fs.h*/
			     char *buffer, /* buffer to fill with data */
			     size_t length, /* length of the buffer	*/
			     loff_t *offset)
{
	uint32_t count;
	uint32_t error_code = 0;

	int32_t num = MINOR(file_p->f_dentry->d_inode->i_rdev);
	/*	pr_info("%s::%d\n", __FUNCTION__, __LINE__); */

	error_code = stm_memsink_pull_data(((stm_memsink_h) file_p->private_data),
		      buffer, length, &count);
	if (error_code < 0) {
		mss_error_print("mss_sink read failed	with %d\n", num);
		return -1;
	}

	return	count;
}

static ssize_t mss_src_write(struct file *file_p, /* see include/linux/fs.h*/
			     const char *buffer, /* buffer of data */
			     size_t length, /* length of the buffer */
			     loff_t *offset)
{
	return	0;
}

static long mss_sink_ioctl(struct file *file, unsigned int command,
			   unsigned long data)
{
	uint32_t retval = 0;
	int32_t num = MINOR(file->f_dentry->d_inode->i_rdev);

	pr_info("%s::%d\n", __FUNCTION__, __LINE__);
	mss_debug_print(" <%s> :: <%d> command %x for %d\n",
		      __FUNCTION__,
		      __LINE__,
		      command,
		      num);
	switch (command) {
	case MSS_SINK_CONNECT:
	{
		pr_info("%s::%d\n", __FUNCTION__, __LINE__);
		retval	= mss_test_producer_attach(producer,
			      (stm_memsink_h) file->private_data);
		if (retval)
			pr_err("Error %d attaching sink to src\n", retval);


		break;
	}
	case MSS_SINK_DISCONNECT:
	{
		retval	= mss_test_producer_detach(producer,
			      (stm_memsink_h) file->private_data);
		if (retval)
			pr_err("Error %d detach sink from src\n", retval);


		break;
	}

	}
	return retval;
}

