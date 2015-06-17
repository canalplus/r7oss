/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/

/*
   @file     event_debugfs.c
   @brief

 */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/debugfs.h>

#include "event_signaler.h"
#include "event_subscriber.h"
#include "event_debugfs.h"

static struct dentry *stm_event_dir;
static struct dentry *stm_event_signaler_entry;
static struct dentry *stm_event_subscriber_entry;

static ssize_t signaler_dump_debugfs(struct file *f, char __user *buf, size_t count, loff_t *ppos);
static ssize_t subscriber_dump_debugfs(struct file *f, char __user *buf, size_t count, loff_t *ppos);
static int io_open(struct inode *inode, struct file *file);

static const struct file_operations signaler_fops = {
	.owner = THIS_MODULE,
	.read = signaler_dump_debugfs,
	.open = io_open
};

static const struct file_operations subscriber_fops = {
	.owner = THIS_MODULE,
	.read = subscriber_dump_debugfs,
	.open = io_open
};

static ssize_t signaler_dump_debugfs(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	return dump_signaler(f, buf, count, ppos);
}


static int io_open(struct inode *inode, struct file *file)
{

	struct io_descriptor *io_desc;

	if (inode->i_private) {
		file->private_data = inode->i_private;
		io_desc = file->private_data;
		io_desc->size = 0;
		io_desc->size_allocated = 0;
	} else
		return -EFAULT;

	return 0;
}
static ssize_t subscriber_dump_debugfs(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	return dump_subscriber(f, buf, count, ppos);
}

struct io_descriptor io_val = {
	/*.size =*/0,
	/*.size_allocated =*/0,
	/*.data =*/NULL
};
int event_create_debugfs(void)
{
	stm_event_dir = debugfs_create_dir("stm_event", NULL);
	if (!stm_event_dir) {
		pr_err(" <%s> : <%d> Failed to create stm event directory folder!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	stm_event_signaler_entry = debugfs_create_file("dump_signalers", 0644, stm_event_dir, &io_val, &signaler_fops);
	if (!stm_event_signaler_entry) {
		pr_err("<%s> : <%d> Failed to create dentry for dump_signaler\n", __FUNCTION__, __LINE__);
		return -1;
	}

	stm_event_subscriber_entry = debugfs_create_file("dump_subscribers", 0644, stm_event_dir, &io_val, &subscriber_fops);
	if (!stm_event_subscriber_entry) {
		pr_err("<%s> : <%d> Failed to create dentry for dump_subscriber\n", __FUNCTION__, __LINE__);
		return -1;
	}
	return 0;
}

void event_remove_debugfs(void)
{
	debugfs_remove(stm_event_signaler_entry);
	debugfs_remove(stm_event_subscriber_entry);
	debugfs_remove(stm_event_dir);
}
