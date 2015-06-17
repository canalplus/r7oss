/*
 *  Multi-Target Trace solution
 *
 *  RELAYFS SUPPORT DRIVER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */
#include <linux/module.h>
#include <linux/prctl.h>
#include <linux/debugfs.h>
#include <linux/futex.h>
#include <linux/version.h>
#include <net/sock.h>
#include <asm/sections.h>
#include <linux/relay.h>

#include <linux/mtt/mtt.h>
#include <linux/crc-ccitt.h>

/* relay data */
static struct rchan *my_chan;
static int buf_full;		   /* has relay_buf_full() occurred ? */
static unsigned int buf_full_cnt;  /* relay_buf_full() occurence counter */
static unsigned int frame_cnt;	   /* How many frames are produced */
static unsigned int byte_cnt;	   /* How many bytes are produced */
static int logging;		   /* 1 if trace is enabled, 0 otherwise */
static size_t subbuf_size = 512*1024;
static size_t n_subbufs = 16;
#define RELAY_MAXSUBBUFSIZE 16777216
#define RELAY_MAXSUBBUFS 256

/* channel-management control files */
static struct dentry *enabled_control;
static struct dentry *create_control;
static struct dentry *subbuf_size_control;
static struct dentry *n_subbufs_control;
static struct dentry *bufferfull_control;
static struct dentry *framewrite_control;
static struct dentry *bytewrite_control;

/* produced/consumed control files */
static struct dentry *produced_control[NR_CPUS];
static struct dentry *consumed_control[NR_CPUS];

/* control file fileop declarations */
static const struct file_operations enabled_fops;
static const struct file_operations create_fops;
static const struct file_operations subbuf_size_fops;
static const struct file_operations n_subbufs_fops;
static const struct file_operations produced_fops;
static const struct file_operations consumed_fops;

/* forward declarations */

static int debugfs_ok;	/*set to 1 if debugfs created. */
static int create_controls(struct dentry *debugfs, void *drv_config);
static void destroy_channel(void);
static void remove_controls(void);

static mtt_return_t
mtt_drv_relay_config(struct mtt_sys_kconfig *cfg);

static struct mtt_output_driver relay_output_driver = {
	.write_func = mtt_drv_relay_write,
	.config_func = mtt_drv_relay_config,
	.debugfs = NULL,
	.guid = MTT_DRV_GUID_DFS,
	.private = NULL,
	.last_error = MTT_ERR_OTHER,
	.devid = 0
};

/**
 * remove_channel_controls - removes produced/consumed control files
 */
static void remove_channel_controls_cpu(int i)
{
	printk(KERN_DEBUG "remove_channel_controls_cpu(%d)\n", i);

	if (produced_control[i]) {
		printk(KERN_DEBUG "\tproduced_control(%d)\n", i);
		debugfs_remove(produced_control[i]);
		produced_control[i] = NULL;
	}

	if (consumed_control[i]) {
		printk(KERN_DEBUG "\tconsumed_control(%d)\n", i);
		debugfs_remove(consumed_control[i]);
		consumed_control[i] = NULL;
	}
}

static void remove_channel_controls(void)
{
	int i;

	printk(KERN_DEBUG "remove_channel_controls\n");

	for_each_possible_cpu(i)
	    remove_channel_controls_cpu(i);
}

/**
 * create_channel_controls - creates produced/consumed control files
 * Returns 1 on success, 0 otherwise.
 */
static int
create_channel_controls(struct dentry *parent,
			const char *base_filename, struct rchan_buf *buf,
			int cpu)
{
	char *tmpname = kmalloc(NAME_MAX + 1, GFP_KERNEL);

	if (!tmpname)
		return 0;

	printk(KERN_DEBUG \
	"create_channel_controls %s.produced/.consumed\n", base_filename);

	sprintf(tmpname, "%s.produced", base_filename);
	produced_control[cpu] =
	    debugfs_create_file(tmpname, S_IRUGO, parent, buf, &produced_fops);
	if (!produced_control[cpu]) {
		printk(KERN_WARNING "Couldn't create relay control file %s\n",
		       tmpname);
		goto _cleanup;
	}

	sprintf(tmpname, "%s.consumed", base_filename);
	consumed_control[cpu] = debugfs_create_file(tmpname, S_IWUSR | S_IRUGO,
						    parent, buf,
						    &consumed_fops);
	if (!consumed_control[cpu]) {
		printk(KERN_WARNING "Couldn't create relay control file %s.\n",
		       tmpname);
		goto _cleanup;
	}

	kfree(tmpname);
	return 1;

_cleanup:
	kfree(tmpname);
	remove_channel_controls();

	return 0;
}

/**
 * subbuf_start() relay callback.
 */
static int
subbuf_start_handler(struct rchan_buf *buf, void *subbuf,
		     void *prev_subbuf, unsigned int prev_padding)
{
	if (prev_subbuf)
		*((unsigned *)prev_subbuf) = prev_padding;

	/**
	 * 'no-overwrite' mode:
	 * If the current buffer is full, i.e. all sub-buffers
	 * remain unconsumed, the callback returns 0 to indicate
	 * that the buffer switch should not occur yet, i.e. until
	 * the consumer has had a chance to read the current set of
	 * ready sub-buffers.
	 */
	if (relay_buf_full(buf)) {
		if (buf_full == 0) {
			buf_full_cnt++;
			buf_full = 1;
		}
		return 0;
	} else {
		buf_full = 0;
	}

	subbuf_start_reserve(buf, sizeof(unsigned int));

	/* After flushing, logging falls to 0, and this channel
	 * is halted. */
	return logging;
}

/**
 * file_create() callback.  Creates relay file in debugfs.
 */
static struct dentry *create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      umode_t mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;
	unsigned long cpu;

	if (kstrtoul(filename + 5, 10, &cpu)) {
		printk(KERN_WARNING
		       "kptrace (create_buf_file_handler): "
		       "invalid CPU number\n");
		return NULL;
	}

	printk(KERN_DEBUG "create_buf_file_handler %s\n", filename);

	buf_file = debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);

	if (!create_channel_controls(parent, filename, buf, cpu)) {
		printk(KERN_WARNING
		       "kptrace: unable to create relayfs channel\n");
		return NULL;
	}

	return buf_file;
}

/**
 * file_remove() default callback.  Removes relay file in debugfs.
 */
static int remove_buf_file_handler(struct dentry *dentry)
{
	unsigned long cpu;

	if (kstrtoul(dentry->d_name.name + 5, 10, &cpu))
		return -EINVAL;

	printk(KERN_DEBUG "remove_buf_file_handler %s, cpu=%ld\n",
	       dentry->d_name.name, cpu);

	remove_channel_controls_cpu(cpu);
	debugfs_remove(dentry);

	return 0;
}

/**
 * relay callbacks
 */
static struct rchan_callbacks relay_callbacks = {
	.subbuf_start = subbuf_start_handler,
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

/**
 * create_channel - creates channel /debug/kptrace/trace0
 * Creates channel along with associated produced/consumed control files
 * Returns channel on success, NULL otherwise
 */
static struct rchan *create_channel(unsigned subbuf_size, unsigned n_subbufs)
{
	struct rchan *tmpchan;

	mtt_printk(KERN_DEBUG "create_channel(%d,%d)\n", subbuf_size,
		   n_subbufs);

	tmpchan = relay_open("trace", relay_output_driver.debugfs,
			     subbuf_size, n_subbufs, &relay_callbacks, NULL);

	if (!tmpchan) {
		printk(KERN_WARNING "relay app channel creation failed\n");
		return NULL;
	}

	logging = 0;

	return tmpchan;
}

/**
 * destroy_channel - destroys channel /debug/kptrace/trace0
 * Destroys channel along with associated produced/consumed control files
 */
static DEFINE_SPINLOCK(tmpbuf_lock);

static void destroy_channel(void)
{
	struct rchan *tmpchan = NULL;

	spin_lock(&tmpbuf_lock);
	if (my_chan) {
		tmpchan = my_chan;
		my_chan = NULL;
	}
	spin_unlock(&tmpbuf_lock);

	if (tmpchan)
		relay_close(tmpchan);
}

/**
 * remove_controls - removes channel management control files
 */
static void remove_controls(void)
{
	if (enabled_control)
		debugfs_remove(enabled_control);

	if (subbuf_size_control)
		debugfs_remove(subbuf_size_control);

	if (n_subbufs_control)
		debugfs_remove(n_subbufs_control);

	if (create_control)
		debugfs_remove(create_control);

	if (bufferfull_control)
		debugfs_remove(bufferfull_control);

	if (framewrite_control)
		debugfs_remove(framewrite_control);

	if (bytewrite_control)
		debugfs_remove(bytewrite_control);

	framewrite_control = bytewrite_control =
	    bufferfull_control = create_control =
	    n_subbufs_control = subbuf_size_control =
	    enabled_control = NULL;

	debugfs_ok = 0;
}

/**
 * create_controls - creates channel management control files
 * Returns 1 on success, 0 otherwise.
 */
static int create_controls(struct dentry *debugfs, void *c)
{
	enabled_control = debugfs_create_file("enabled", 0, debugfs,
					      NULL, &enabled_fops);

	if (!enabled_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'enabled'.\n");
		goto fail;
	}

	subbuf_size_control = debugfs_create_file("subbuf_size", 0, debugfs,
						NULL, &subbuf_size_fops);
	if (!subbuf_size_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'subbuf_size'.\n");
		goto fail;
	}

	n_subbufs_control = debugfs_create_file("n_subbufs", 0, debugfs, NULL,
						&n_subbufs_fops);
	if (!n_subbufs_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'n_subbufs'.\n");
		goto fail;
	}

	create_control =
	    debugfs_create_file("create", 0, debugfs, NULL, &create_fops);
	if (!create_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'create'.\n");
		goto fail;
	}

	bufferfull_control =
	    debugfs_create_u32("bufferfull", S_IRUGO, debugfs, &buf_full_cnt);
	if (!bufferfull_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'bufferfull'.\n");
		goto fail;
	}

	framewrite_control =
	    debugfs_create_u32("sentframes", S_IRUGO, debugfs, &frame_cnt);
	if (!framewrite_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'sentframes'.\n");
		goto fail;
	}

	bytewrite_control =
		debugfs_create_u32("sentbytes", S_IRUGO, debugfs, &byte_cnt);
	if (!bytewrite_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'sentbytes'.\n");
		goto fail;
	}

	return 1;
fail:
	remove_controls();

	return 0;
}

/*
 * control files for relay channel management
 */
static ssize_t
enabled_read(struct file *filp, char __user *buffer,
	     size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", logging);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
enabled_write(struct file *filp, const char __user *buffer,
	      size_t count, loff_t *ppos)
{
	char buf[16];
	long enabled;

	printk(KERN_DEBUG "enabled_write\n");

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (kstrtol(buf, 10, &enabled))
		return -EINVAL;

	if (enabled && my_chan)
		logging = 1;
	else if (!enabled) {
		logging = 0;
		if (my_chan)
			relay_flush(my_chan);
	}

	return count;
}

/*
 * 'enabled' file operations - boolean r/w
 *  toggles logging to the relay channel
 */
static const struct file_operations enabled_fops = {
	.read = enabled_read,
	.write = enabled_write,
};

static ssize_t
create_read(struct file *filp, char __user *buffer,
	    size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", (unsigned int)my_chan);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
create_write(struct file *filp, const char __user *buffer,
	     size_t count, loff_t *ppos)
{
	/* Do nothing: let's remove this control later
	 * to not generate an error with the current
	 * usermode package.
	 */
	return 0;
}

/*
 * 'create' file operations - boolean r/w
 *  (DEPRECATED) creates/destroys the relay channel
 */
static const struct file_operations create_fops = {
	.read = create_read,
	.write = create_write,
};

static ssize_t
subbuf_size_read(struct file *filp, char __user *buffer,
		 size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", subbuf_size);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
subbuf_size_write(struct file *filp, const char __user *buffer,
		  size_t count, loff_t *ppos)
{
	char buf[16];
	long tmp;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if ((kstrtol(buf, 10, &tmp) == -EINVAL)
			|| (tmp < 1)
			|| (tmp > RELAY_MAXSUBBUFSIZE))
		return -EINVAL;

	subbuf_size = (size_t) tmp;

	return count;
}

/**
 * 'subbuf_size' file operations - r/w
 *  gets/sets the subbuffer size to use in channel creation
 */
static const struct file_operations subbuf_size_fops = {
	.read = subbuf_size_read,
	.write = subbuf_size_write,
};

static ssize_t
n_subbufs_read(struct file *filp, char __user *buffer,
	       size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", n_subbufs);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
n_subbufs_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[16];
	long tmp;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if ((kstrtol(buf, 10, &tmp) == -EINVAL)
			|| (tmp < 1)
			|| (tmp > RELAY_MAXSUBBUFS))
		return -EINVAL;

	n_subbufs = (size_t) tmp;

	return count;
}

/**
 * 'n_subbufs' file operations - r/w
 *  gets/sets the number of subbuffers to use in channel creation
 */
static const struct file_operations n_subbufs_fops = {
	.read = n_subbufs_read,
	.write = n_subbufs_write,
};

/**
 * control files for relay produced/consumed sub-buffer counts
 */
static int produced_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t
produced_read(struct file *filp, char __user *buffer,
	      size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_produced,
				       sizeof(buf->subbufs_produced));
}

/**
 * 'produced' file operations - r, binary
 *  There is a .produced file associated with each relay file.
 *  Reading a .produced file returns the number of sub-buffers so far
 *  produced for the associated relay buffer.
 */
static const struct file_operations produced_fops = {
	.open = produced_open,
	.read = produced_read
};

static int consumed_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t
consumed_read(struct file *filp, char __user *buffer,
	      size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_consumed,
				       sizeof(buf->subbufs_consumed));
}

static ssize_t
consumed_write(struct file *filp, const char __user *buffer,
	       size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;
	size_t consumed;

	if (copy_from_user(&consumed, buffer, sizeof(consumed)))
		return -EFAULT;

	relay_subbufs_consumed(buf->chan, buf->cpu, consumed);

	return count;
}

/**
 * 'consumed' file operations - r/w, binary
 *  There is a .consumed file associated with each relay file.
 *  Writing to a .consumed file adds the value written to the
 *  subbuffers-consumed count of the associated relay buffer.
 *  Reading a .consumed file returns the number of sub-buffers so far
 *  consumed for the associated relay buffer.
 */
static const struct file_operations
consumed_fops = {
	.open = consumed_open,
	.read = consumed_read,
	.write = consumed_write,
};

/**
 * initialize the relay channel and the debugfs tree
 */
int __init mtt_relay_init(void)
{
	mtt_printk(KERN_DEBUG "mtt_relay_init\n");
	mtt_register_output_driver(&relay_output_driver);
	debugfs_ok = 0;

	return 0;
}

/**
 * free all the tracepoints and sets, remove the sysdev and
 * destroy the relay channel.
 */
void __exit mtt_relay_cleanup(void)
{
	mtt_printk(KERN_DEBUG "mtt_relay_cleanup\n");

	destroy_channel();
	remove_controls();
}

/**
 * initialize the relay channel and the debugfs tree
 */
static mtt_return_t
mtt_drv_relay_config(struct mtt_sys_kconfig *cfg)
{
	struct mtt_dfs_drv_cfg *c;
	relay_output_driver.last_error =  MTT_ERR_FERROR;

	mtt_printk(KERN_DEBUG "mtt_drv_relay_config\n");

	c = (struct mtt_dfs_drv_cfg *)(&(cfg->media_cfg));
	BUG_ON(!relay_output_driver.debugfs);

	if (!debugfs_ok)
		debugfs_ok =
		  create_controls(relay_output_driver.debugfs, c);

	if (!debugfs_ok) {
		printk(KERN_ERR "mtt_relay:" \
			"Couldn't create debugfs files\n");
		return MTT_ERR_FERROR;
	}

	/* Re-create channels */
	destroy_channel();

	my_chan = create_channel(c->buf_size, c->n_subbufs);
	if (!my_chan)
		return MTT_ERR_FERROR;

	subbuf_size = c->buf_size;
	n_subbufs = c->n_subbufs;
	buf_full = 0;
	buf_full_cnt = 0;
	frame_cnt = 0;
	byte_cnt = 0;

	return relay_output_driver.last_error = 0;
}

static inline void __relay_write_nolock(struct rchan *chan,
					const void *data, size_t length)
{
	struct rchan_buf *buf;

	buf = chan->buf[raw_smp_processor_id()];
	if (unlikely((buf->offset + length) > buf->chan->subbuf_size))
		length = relay_switch_subbuf(buf, length);

	memcpy(buf->data + buf->offset, data, length);
	buf->offset += length;
}

void mtt_drv_relay_write(mtt_packet_t *p, int lock)
{
#ifdef _MTT_TESTCRC_
	u32 crc = 0;
	/* Add a 16bits CCITT CRC for validation */
	p->u.preamble->command_param |= MTT_PARAM_CRC;

	crc = (u32)crc_ccitt(0xffff, p->u.buf, p->length);
	*(uint32_t *)(p->u.buf + p->length) = crc;
	p->length += 4;
#endif
	MTT_BUG_ON(my_chan == NULL);

	/* try the already locked version. */
	if (likely(lock == APILOCK))
		__relay_write_nolock(my_chan, p->u.buf, p->length);
	else
		relay_write(my_chan, p->u.buf, p->length);

	frame_cnt++;
	byte_cnt += p->length;
}
