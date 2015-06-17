#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include "infra_os_wrapper.h"
#include "mss_test_debugfs.h"
#include "mss_test.h"

struct dentry *d_memsrcsink;

struct dentry *d_push_size;
struct dentry *d_pull_size;

struct dentry *d_push_fill;
struct dentry *d_pull_fill;

struct dentry *d_push;
struct dentry *d_pull;

struct dentry *d_consumer_size;
struct dentry *d_producer_size;

/*size_t consumer_size=1024;*/
/*size_t producer_size=1024;*/

struct io_descriptor pull = {
	/*.size =*/1024,
	/*.size_allocated =*/0,
	/*.data =*/NULL
};

struct io_descriptor push = {
	/*.size =*/1024,
	/*.size_allocated =*/0,
	/*.data =*/NULL
};

static ssize_t io_read(struct file *file, char __user *buf,
		       size_t count, loff_t *ppos)
{
	struct io_descriptor *io_desc = file->private_data;
	ssize_t nbread;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "read buf=%p count=%d, ppos=%lld io_desc=%p\n",
		      buf, count, *ppos, io_desc);
	if (io_desc->size_allocated <= *ppos)
		return 0;


	nbread = min(count, (size_t) (io_desc->size_allocated - *ppos));
	mss_test_dtrace(MSS_TEST_MODULE,
		      "from io_desc->data+*ppos=%p to buf=%p\n",
		      io_desc->data + *ppos, buf);
	if (copy_to_user(buf, io_desc->data + *ppos, nbread))
		return -EFAULT;

	*ppos += nbread;
	return nbread;
}

static ssize_t io_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	struct io_descriptor *io_desc = file->private_data;
	ssize_t nbwrite;

	mss_test_dtrace(MSS_TEST_MODULE,
		      "write buf=%p count=%d, ppos=%lld io_desc=%p\n", buf,
		      count,
		      *ppos,
		      io_desc);
	if (io_desc->size_allocated <= *ppos) {
		mss_test_etrace(MSS_TEST_MODULE,
			      "Already full, forget other byte\n");
		*ppos += count;
		return count;
	}

	nbwrite = min(count, (size_t) (io_desc->size_allocated - *ppos));
	mss_test_dtrace(MSS_TEST_MODULE,
		      "to io_desc->data+*ppos=%p from buf=%p\n",
		      io_desc->data + *ppos,
		      buf);
	if (copy_from_user(io_desc->data + *ppos, buf, nbwrite))
		return -EFAULT;

	*ppos += nbwrite;
	return nbwrite;
}

static ssize_t io_fill(struct file *file, const char __user *buf,
		       size_t count, loff_t *ppos)
{
	struct io_descriptor *io_desc = file->private_data;
	char *str;
	size_t len;
	int i;

	str = kmalloc(count, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	if (copy_from_user(str, buf, count)) {
		kfree(str);
		return -EFAULT;
	}

	len = strnlen(str, count);
	if (0 == len) {
		kfree(str);
		return -EFAULT;
	}
	for (i = 0; (i + 1) * len <= io_desc->size_allocated; i++)
		memcpy(io_desc->data + (i * len), str, len);

	memcpy(io_desc->data + (i * len), str, (i + 1) * len - io_desc->size_allocated);
	((char *) io_desc->data)[io_desc->size_allocated-1] = 0;

	kfree(str);
	return count;
}

static int io_open(struct inode *inode, struct file *file)
{

	struct io_descriptor *io_desc;
	if (inode->i_private) {
		file->private_data = inode->i_private;
		io_desc = file->private_data;
	} else
		return -EFAULT;

	if (io_desc->size != io_desc->size_allocated) {
		if (io_desc->data)
			vfree(io_desc->data);

		io_desc->data = vmalloc(io_desc->size);
		if (io_desc->data != NULL)
			io_desc->size_allocated = io_desc->size;

		mss_test_dtrace(MSS_TEST_MODULE,
			      ".data = %p\n", io_desc->data);
		mss_test_dtrace(MSS_TEST_MODULE,
			      ".size = %d\n", io_desc->size);
		mss_test_dtrace(MSS_TEST_MODULE,
			      ".size_allocated= %d\n", io_desc->size_allocated);
	}

	return 0;

}

const struct file_operations io_ops = {
	.write = io_write, /* Not useful for out (but reset)*/
	.read = io_read, /* not useful for in (but check)*/
	.open = io_open /* start the source */
};

const struct file_operations fill_ops = {
	.write = io_fill,
	.open = io_open /* start the source */
};

struct dentry *mss_test_create_dbgfs(void)
{
	size_t i;
	/* CREATE DBUGEFS INTERFACE */
	d_memsrcsink = debugfs_create_dir("memsrcsink_test", NULL);
	if (IS_ERR(d_memsrcsink)) {
		mss_test_etrace(MSS_TEST_MODULE, "error\n");
		return NULL;
	}

	d_pull_size = debugfs_create_size_t("pull_count",
		      0644,
		      d_memsrcsink,
		      &pull.size);

	d_push_size = debugfs_create_size_t("push_count",
		      0644,
		      d_memsrcsink,
		      &push.size);

	d_producer_size = debugfs_create_size_t("producer_count",
		      0644,
		      d_memsrcsink,
		      &producer_size);

	d_consumer_size = debugfs_create_size_t("consumer_count",
		      0644,
		      d_memsrcsink,
		      &consumer_size);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "Create push io_desc=%p\n", &push);

	d_push = debugfs_create_file("push_ops",
		      0644,
		      d_memsrcsink,
		      &push,
		      &io_ops);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "Create pull io_desc=%p\n", &pull);

	d_pull = debugfs_create_file("pull_ops",
		      0644,
		      d_memsrcsink,
		      &pull,
		      &io_ops);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "Create fill_out io_desc=%p\n", &push);

	d_push = debugfs_create_file("push_fill",
		      0222,
		      d_memsrcsink,
		      &push,
		      &fill_ops);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "Create fill_in io_desc=%p\n", &pull);

	d_pull = debugfs_create_file("push_fill",
		      0222,
		      d_memsrcsink,
		      &pull,
		      &fill_ops);

	pull.data = vmalloc(pull.size);
	if (pull.data != NULL)
		pull.size_allocated = pull.size;


	for (i = 0; i < pull.size_allocated; i++)
		((char *) pull.data)[i] = 'i';


	mss_test_dtrace(MSS_TEST_MODULE,
		      "push.data = %p\n",
		      push.data);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "push.size = %d\n",
		      push.size);

	mss_test_dtrace(MSS_TEST_MODULE,
		      "push.size_allocated= %d\n", push.size_allocated);

	pull.data = vmalloc(pull.size);
	if (pull.data != NULL)
		pull.size_allocated = pull.size;


	for (i = 0; i < pull.size_allocated; i++)
		((char *) pull.data)[i] = 'o';

	mss_test_dtrace(MSS_TEST_MODULE, "pull.data = %p\n", pull.data);
	mss_test_dtrace(MSS_TEST_MODULE, "pull.size = %d\n", pull.size);
	mss_test_dtrace(MSS_TEST_MODULE, "pull.size_allocated= %d\n",
		      pull.size_allocated);

	return d_memsrcsink;
}

void mss_test_delete_dbgfs(void)
{
	/*clean up*/
	debugfs_remove_recursive(d_memsrcsink);
}
