#include <linux/proc_fs.h>
#include <linux/module.h>

#include "stm_ci.h"
#include "ci_internal.h"
#include "ci_debugfs.h"

static int ci_free_timeout_read(struct file *file,
			char __user *buffer,
			size_t	len,
			loff_t	*offset)
{
	stm_ci_t *ci_p = file->private_data;
	char		_timeout[32];

	/* convert to ASCII before passing to user */
	sprintf(_timeout, "%d\n", ci_p->fr_timeout);
	return simple_read_from_buffer(buffer, len, offset, /* to */
				_timeout, strlen(_timeout));
}

static int ci_free_timeout_write(struct file *file,
			const char __user *buffer,
			size_t len,
			loff_t	*offset)
{
	stm_ci_t *ci_p = file->private_data;
	unsigned long fr_timeout = 0;
	int		ret;

	ret = kstrtoul_from_user(buffer, len, 10, &fr_timeout);
	if (ret)
		return ret;
	ci_p->fr_timeout = fr_timeout;
	return len;
}

static int ci_open(struct inode *inode, struct file *file)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	file->private_data = (void *) PDE_DATA(inode);
#else
	file->private_data = (void *) PDE(inode)->data;
#endif
	printk(KERN_ALERT "%s (user data %p)\n", __func__, file->private_data);
	/*file->private_data = inode->i_private; */
	if (!file->private_data)
		return -EINVAL;
	return 0;
}

/*
 * close: do nothing!
 */
static int ci_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations ci_free_timeout_fops = {
	.owner		= THIS_MODULE,
	.open		= ci_open,
	.read		= ci_free_timeout_read,
	.write		= ci_free_timeout_write,
	.llseek		= default_llseek,
	.release		= ci_close,
};

static int ci_da_timeout_read(struct file *file,
			char __user *buffer,
			size_t	len,
			loff_t	*offset)
{
	stm_ci_t *ci_p = file->private_data;
	char		_timeout[32];

	/* convert to ASCII before passing to user */
	sprintf(_timeout, "%d\n", ci_p->da_timeout);
	return simple_read_from_buffer(buffer, len, offset, /* to */
				_timeout, strlen(_timeout));
}

static int ci_da_timeout_write(struct file *file,
			const char __user *buffer,
			size_t len,
			loff_t	*offset)
{
	stm_ci_t *ci_p = file->private_data;
	unsigned long da_timeout = 0;
	int		ret;

	ret = kstrtoul_from_user(buffer, len, 10, &da_timeout);
	if (ret)
		return ret;
	ci_p->da_timeout = da_timeout;
	return len;
}

static const struct file_operations ci_da_timeout_fops = {
	.owner		= THIS_MODULE,
	.open		= ci_open,
	.read		= ci_da_timeout_read,
	.write		= ci_da_timeout_write,
	.llseek		= default_llseek,
	.release		= ci_close,
};

static int ci_ciplus_polling_read(struct file *file,
			char __user *buffer,
			size_t len,
			loff_t *offset)
{
	stm_ci_t *ci_p = file->private_data;
	char _polling[32];

	/* convert to ASCII before passing to user */
	sprintf(_polling, "%d\n", ci_p->ciplus_in_polling);
	return simple_read_from_buffer(buffer, len, offset,
					_polling, strlen(_polling));
}

static int ci_ciplus_polling_write(struct file *file,
				const char __user *buffer,
				size_t len,
				loff_t	*offset)
{
	unsigned long _polling = 0;
	stm_ci_t *ci_p = file->private_data;
	int	ret;

	ret = kstrtoul_from_user(buffer, len, 10, &_polling);
	if (ret)
		return ret;

	ci_p->ciplus_in_polling = (uint32_t)_polling;
	return len;
}
static const struct file_operations ci_configure_ciplus_in_polling_ops = {
	.owner		= THIS_MODULE,
	.open		= ci_open,
	.read		= ci_ciplus_polling_read,
	.write		= ci_ciplus_polling_write,
	.llseek		= default_llseek,
	.release		= ci_close,
};

int ci_create_debugfs(uint32_t ci_num, stm_ci_t *ci_p)
{
	char ci_dir_temp[20];
	struct proc_dir_entry *lpde;
	char	*lname;

	/* create proc directory for Master card */
	sprintf(ci_dir_temp, "%s-%d", "stm_ci", ci_num);
	ci_p->ci_dir = proc_mkdir(ci_dir_temp, NULL);
	if (!ci_p->ci_dir) {
		ci_error_trace(CI_API_RUNTIME,
			"Failed to create stm_ci directory folder!\n");
		return -1;
	}

	ci_debug_trace(CI_API_RUNTIME, "%s (user data %p)\n", __func__, ci_p);

	lname = "ci_free_timeout";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				ci_p->ci_dir, &ci_free_timeout_fops,
				(void *) ci_p);
	if (!lpde)
		goto _proc_create_failure;

	lname = "ci_da_timeout";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
				ci_p->ci_dir, &ci_da_timeout_fops,
				(void *) ci_p);
	if (!lpde)
		goto _proc_create_failure;

	lname = "ci_configure_ciplus_in_polling";
	lpde = proc_create_data(lname, (S_IRUGO | S_IWUSR),
			ci_p->ci_dir, &ci_configure_ciplus_in_polling_ops,
				(void *) ci_p);
	if (!lpde)
		goto _proc_create_failure;

	return 0;

_proc_create_failure:
	ci_error_trace(CI_API_RUNTIME,
		"failed to create proc entry for %s\n", lname);
	return -1;
}

void ci_delete_debugfs(uint32_t ci_num, stm_ci_t *ci_p)
{
	uint8_t ci_dir_temp[20];

	sprintf(ci_dir_temp, "%s-%d", "stm_ci", ci_num);
	remove_proc_entry("ci_free_timeout", ci_p->ci_dir);
	remove_proc_entry("ci_da_timeout", ci_p->ci_dir);
	remove_proc_entry("ci_configure_ciplus_in_polling", ci_p->ci_dir);
	remove_proc_entry(ci_dir_temp, NULL);
}

