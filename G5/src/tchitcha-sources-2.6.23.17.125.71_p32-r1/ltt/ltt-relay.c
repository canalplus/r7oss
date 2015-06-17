/*
 * (C) Copyright 2005-2006 - Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * Contains the kernel code for the Linux Trace Toolkit.
 *
 * Author:
 *	Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * Inspired from LTT :
 *	Karim Yaghmour (karim@opersys.com)
 *	Tom Zanussi (zanussi@us.ibm.com)
 *	Bob Wisniewski (bob@watson.ibm.com)
 * And from K42 :
 *  Bob Wisniewski (bob@watson.ibm.com)
 *
 * Changelog:
 *  19/10/05, Complete lockless mechanism. (Mathieu Desnoyers)
 *	27/05/05, Modular redesign and rewrite. (Mathieu Desnoyers)

 * Comments :
 * num_active_traces protects the functors. Changing the pointer is an atomic
 * operation, but the functions can only be called when in tracing. It is then
 * safe to unload a module in which sits a functor when no tracing is active.
 *
 * filter_control functor is protected by incrementing its module refcount.
 */

#include <linux/time.h>
#include <linux/ltt-tracer.h>
#include <linux/relay.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/debugfs.h>
#include <linux/stat.h>
#include <linux/cpu.h>
#include <asm/atomic.h>
#include <asm/local.h>

static struct dentry *ltt_root_dentry;
static struct file_operations ltt_file_operations;

/*
 * A switch is done during tracing or as a final flush after tracing (so it
 * won't write in the new sub-buffer).
 */
enum force_switch_mode { FORCE_ACTIVE, FORCE_FLUSH };

static int ltt_relay_create_buffer(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *ltt_chan,
		struct rchan_buf *buf,
		unsigned int cpu,
		unsigned n_subbufs);

static void ltt_relay_destroy_buffer(struct ltt_channel_struct *ltt_chan,
		unsigned int cpu);

static void ltt_force_switch(struct rchan_buf *buf,
		enum force_switch_mode mode);

/*
 * Trace callbacks
 */
static void ltt_buffer_begin_callback(struct rchan_buf *buf,
			u64 tsc, unsigned int subbuf_idx)
{
	struct ltt_channel_struct *channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_block_start_header *header =
		(struct ltt_block_start_header *)
			(buf->start + (subbuf_idx*buf->chan->subbuf_size));

	header->begin.cycle_count = tsc;
	header->begin.freq = ltt_frequency();
	header->lost_size = 0xFFFFFFFF; /* for debugging */
	header->buf_size = buf->chan->subbuf_size;
	ltt_write_trace_header(channel->trace, &header->trace);
}

/*
 * offset is assumed to never be 0 here : never deliver a completely empty
 * subbuffer. The lost size is between 0 and subbuf_size-1.
 */
static void ltt_buffer_end_callback(struct rchan_buf *buf,
		u64 tsc, unsigned int offset, unsigned int subbuf_idx)
{
	struct ltt_block_start_header *header =
		(struct ltt_block_start_header *)
			(buf->start + (subbuf_idx*buf->chan->subbuf_size));

	header->lost_size = SUBBUF_OFFSET((buf->chan->subbuf_size - offset),
				buf->chan);
	header->end.cycle_count = tsc;
	header->end.freq = ltt_frequency();
}

static int ltt_subbuf_start_callback(struct rchan_buf *buf,
		void *subbuf,
		void *prev_subbuf,
		size_t prev_padding)
{
	return 0;
}



static void ltt_deliver(struct rchan_buf *buf, unsigned subbuf_idx,
		void *subbuf)
{
	struct ltt_channel_struct *channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_channel_buf_struct *ltt_buf = &channel->buf[buf->cpu];

	atomic_set(&ltt_buf->wakeup_readers, 1);
}

static void ltt_buf_mapped_callback(struct rchan_buf *buf,
		struct file *filp)
{
}

static void ltt_buf_unmapped_callback(struct rchan_buf *buf,
		struct file *filp)
{
}

static struct dentry *ltt_create_buf_file_callback(const char *filename,
		struct dentry *parent, int mode,
		struct rchan_buf *buf, int *is_global)
{
	struct ltt_channel_struct *ltt_chan;
	int err;
	struct dentry *dentry;

	ltt_chan = buf->chan->private_data;
	err = ltt_relay_create_buffer(ltt_chan->trace, ltt_chan,
					buf, buf->cpu,
					buf->chan->n_subbufs);
	if (err)
		return ERR_PTR(err);

	dentry = debugfs_create_file(filename, mode, parent, buf,
			&ltt_file_operations);
	if (!dentry)
		goto error;
	return dentry;
error:
	ltt_relay_destroy_buffer(ltt_chan, buf->cpu);
	return NULL;
}

static int ltt_remove_buf_file_callback(struct dentry *dentry)
{
	struct rchan_buf *buf = dentry->d_inode->i_private;
	struct ltt_channel_struct *ltt_chan = buf->chan->private_data;

	debugfs_remove(dentry);
	ltt_relay_destroy_buffer(ltt_chan, buf->cpu);

	return 0;
}

/*
 * This function should not be called from NMI interrupt context
 */
static void ltt_buf_unfull(struct rchan_buf *buf,
		unsigned subbuf_idx,
		void *subbuf)
{
	struct ltt_channel_struct *ltt_channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];
	if (waitqueue_active(&ltt_buf->write_wait))
		schedule_work(&ltt_buf->wake_writers);
}

/**
 *	poll file op for ltt files
 *	@filp: the file
 *	@wait: poll table
 *
 *	Poll implementation.
 */
static unsigned int ltt_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct inode *inode = filp->f_dentry->d_inode;
	struct rchan_buf *buf = inode->i_private;
	struct ltt_channel_struct *ltt_channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];

	if (filp->f_mode & FMODE_READ) {
		poll_wait(filp, &buf->read_wait, wait);

		if (atomic_long_read(&ltt_buf->active_readers) != 0) {
			return 0;
		} else {
			if (SUBBUF_TRUNC(
				local_read(&ltt_buf->offset), buf->chan)
					- SUBBUF_TRUNC(
				atomic_long_read(&ltt_buf->consumed), buf->chan)
					== 0) {
				if (buf->finalized)
					return POLLHUP;
				else
					return 0;
			} else {
				struct rchan *rchan =
					ltt_channel->trans_channel_data;
				if (SUBBUF_TRUNC(local_read(&ltt_buf->offset),
							buf->chan)
						- SUBBUF_TRUNC(
				atomic_long_read(&ltt_buf->consumed), buf->chan)
						>= rchan->alloc_size)
					return POLLPRI | POLLRDBAND;
				else
					return POLLIN | POLLRDNORM;
			}
		}
	}
	return mask;
}


/**
 *	ioctl control on the debugfs file
 *
 *	@inode: the inode
 *	@filp: the file
 *	@cmd: the command
 *	@arg: command arg
 *
 *	This ioctl implements three commands necessary for a minimal
 *	producer/consumer implementation :
 *	RELAY_GET_SUBBUF
 *		Get the next sub buffer that can be read. It never blocks.
 *	RELAY_PUT_SUBBUF
 *		Release the currently read sub-buffer. Parameter is the last
 *		put subbuffer (returned by GET_SUBBUF).
 *	RELAY_GET_N_BUBBUFS
 *		returns the number of sub buffers in the per cpu channel.
 *	RELAY_GET_SUBBUF_SIZE
 *		returns the size of the sub buffers.
 */
static int ltt_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct rchan_buf *buf = inode->i_private;
	struct ltt_channel_struct *ltt_channel =
		(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];
	u32 __user *argp = (u32 __user *)arg;

	switch (cmd) {
	case RELAY_GET_SUBBUF:
	{
		long consumed_old, consumed_idx;
		atomic_long_inc(&ltt_buf->active_readers);
		consumed_old = atomic_long_read(&ltt_buf->consumed);
		consumed_idx = SUBBUF_INDEX(consumed_old, buf->chan);
		if (SUBBUF_OFFSET(
			local_read(&ltt_buf->commit_count[consumed_idx]),
				buf->chan) != 0) {
			atomic_long_dec(&ltt_buf->active_readers);
			return -EAGAIN;
		}
		if ((SUBBUF_TRUNC(local_read(&ltt_buf->offset), buf->chan)
				- SUBBUF_TRUNC(consumed_old, buf->chan)) == 0) {
			atomic_long_dec(&ltt_buf->active_readers);
			return -EAGAIN;
		}
		/* must make sure we read the counter before reading the data
		 * in the buffer. */
		smp_rmb();
		return put_user((u32)consumed_old, argp);
		break;
	}
	case RELAY_PUT_SUBBUF:
	{
		u32 uconsumed_old;
		int ret;
		long consumed_new, consumed_old;

		ret = get_user(uconsumed_old, argp);
		if (ret)
			return ret; /* will return -EFAULT */

		consumed_old = atomic_long_read(&ltt_buf->consumed);
		consumed_old = consumed_old & (~0xFFFFFFFFL);
		consumed_old = consumed_old | uconsumed_old;
		consumed_new = SUBBUF_ALIGN(consumed_old, buf->chan);

		spin_lock(&ltt_buf->full_lock);
		if (atomic_long_cmpxchg(
			&ltt_buf->consumed, consumed_old, consumed_new)
				!= consumed_old) {
			/* We have been pushed by the writer : the last
			 * buffer read _is_ corrupted! It can also
			 * happen if this is a buffer we never got. */
			atomic_long_dec(&ltt_buf->active_readers);
			spin_unlock(&ltt_buf->full_lock);
			return -EIO;
		} else {
			/* tell the client that buffer is now unfull */
			int index;
			void *data;
			index = SUBBUF_INDEX(consumed_old, buf->chan);
			data = buf->start +
				BUFFER_OFFSET(consumed_old, buf->chan);
			ltt_buf_unfull(buf, index, data);
			atomic_long_dec(&ltt_buf->active_readers);
			spin_unlock(&ltt_buf->full_lock);
		}
		break;
	}
	case RELAY_GET_N_SUBBUFS:
		return put_user((u32)buf->chan->n_subbufs, argp);
		break;
	case RELAY_GET_SUBBUF_SIZE:
		return put_user((u32)buf->chan->subbuf_size, argp);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long ltt_compat_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	lock_kernel();
	ret = ltt_ioctl(file->f_dentry->d_inode, file, cmd, arg);
	unlock_kernel();

	return ret;
}
#endif

static void ltt_relay_print_subbuffer_errors(
		struct ltt_channel_struct *ltt_chan,
		long cons_off, unsigned int i)
{
	struct rchan *rchan = ltt_chan->trans_channel_data;
	long cons_idx;

	printk(KERN_WARNING
		"LTT : unread channel %s offset is %ld "
		"and cons_off : %ld (cpu %u)\n",
		ltt_chan->channel_name,
		local_read(&ltt_chan->buf[i].offset), cons_off, i);
	/* Check each sub-buffer for non zero commit count */
	cons_idx = SUBBUF_INDEX(cons_off, rchan);
	if (SUBBUF_OFFSET(local_read(&ltt_chan->buf[i].commit_count[cons_idx]),
				rchan))
		printk(KERN_ALERT
			"LTT : %s : subbuffer %lu has non zero "
			"commit count.\n",
			ltt_chan->channel_name, cons_idx);
	printk(KERN_ALERT "LTT : %s : commit count : %lu, subbuf size %zd\n",
			ltt_chan->channel_name,
			local_read(&ltt_chan->buf[i].commit_count[cons_idx]),
			rchan->subbuf_size);
}

static void ltt_relay_print_errors(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *ltt_chan, int cpu)
{
	struct rchan *rchan = ltt_chan->trans_channel_data;
	long cons_off;

	for (cons_off = atomic_long_read(&ltt_chan->buf[cpu].consumed);
			(SUBBUF_TRUNC(local_read(&ltt_chan->buf[cpu].offset),
				rchan) - cons_off) > 0;
			cons_off = SUBBUF_ALIGN(cons_off, rchan))
		ltt_relay_print_subbuffer_errors(ltt_chan, cons_off, cpu);
}

static void ltt_relay_print_buffer_errors(struct ltt_channel_struct *ltt_chan,
		unsigned int cpu)
{
	struct ltt_trace_struct *trace = ltt_chan->trace;

	if (local_read(&ltt_chan->buf[cpu].events_lost))
		printk(KERN_ALERT
			"LTT : %s : %ld events lost "
			"in %s channel (cpu %u).\n",
			ltt_chan->channel_name,
			local_read(&ltt_chan->buf[cpu].events_lost),
			ltt_chan->channel_name, cpu);
	if (local_read(&ltt_chan->buf[cpu].corrupted_subbuffers))
		printk(KERN_ALERT
			"LTT : %s : %ld corrupted subbuffers "
			"in %s channel (cpu %u).\n",
			ltt_chan->channel_name,
			local_read(
				&ltt_chan->buf[cpu].corrupted_subbuffers),
			ltt_chan->channel_name, cpu);

	ltt_relay_print_errors(trace, ltt_chan, cpu);
}

static void ltt_relay_remove_dirs(struct ltt_trace_struct *trace)
{
	debugfs_remove(trace->dentry.control_root);
	debugfs_remove(trace->dentry.trace_root);
}

static void ltt_relay_release_channel(struct kref *kref)
{
	struct ltt_channel_struct *ltt_chan = container_of(kref,
			struct ltt_channel_struct, kref);
	kfree(ltt_chan);
}

/*
 * Create ltt buffer.
 */
static int ltt_relay_create_buffer(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *ltt_chan, struct rchan_buf *buf,
		unsigned int cpu, unsigned n_subbufs)
{
	unsigned int j;

	ltt_chan->buf[cpu].commit_count =
		kmalloc(sizeof(local_t) * n_subbufs, GFP_KERNEL);
	if (!ltt_chan->buf[cpu].commit_count)
		return -ENOMEM;
	kref_get(&trace->kref);
	kref_get(&trace->ltt_transport_kref);
	kref_get(&ltt_chan->kref);
	local_set(&ltt_chan->buf[cpu].offset,
		ltt_subbuf_header_len());
	atomic_long_set(&ltt_chan->buf[cpu].consumed, 0);
	atomic_long_set(&ltt_chan->buf[cpu].active_readers, 0);
	for (j = 0; j < n_subbufs; j++)
		local_set(&ltt_chan->buf[cpu].commit_count[j], 0);
	init_waitqueue_head(&ltt_chan->buf[cpu].write_wait);
	atomic_set(&ltt_chan->buf[cpu].wakeup_readers, 0);
	INIT_WORK(&ltt_chan->buf[cpu].wake_writers, ltt_wakeup_writers);
	spin_lock_init(&ltt_chan->buf[cpu].full_lock);

	ltt_buffer_begin_callback(buf, trace->start_tsc, 0);
	/* atomic_add made on local variable on data that belongs to
	 * various CPUs : ok because tracing not started (for this cpu). */
	local_add(ltt_subbuf_header_len(),
		&ltt_chan->buf[cpu].commit_count[0]);

	local_set(&ltt_chan->buf[cpu].events_lost, 0);
	local_set(&ltt_chan->buf[cpu].corrupted_subbuffers, 0);

	return 0;
}

static void ltt_relay_destroy_buffer(struct ltt_channel_struct *ltt_chan,
		unsigned int cpu)
{
	struct ltt_trace_struct *trace = ltt_chan->trace;

	kref_put(&ltt_chan->trace->ltt_transport_kref,
		ltt_release_transport);
	ltt_relay_print_buffer_errors(ltt_chan, cpu);
	kfree(ltt_chan->buf[cpu].commit_count);
	ltt_chan->buf[cpu].commit_count = NULL;
	kref_put(&ltt_chan->kref, ltt_relay_release_channel);
	kref_put(&trace->kref, ltt_release_trace);
}

/*
 * Create channel.
 */
static int ltt_relay_create_channel(char *trace_name,
		struct ltt_trace_struct *trace, struct dentry *dir,
		char *channel_name, struct ltt_channel_struct **ltt_chan,
		unsigned int subbuf_size, unsigned int n_subbufs,
		int overwrite)
{
	char *tmpname;
	unsigned int tmpname_len;
	int err = 0;

	tmpname = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!tmpname)
		return EPERM;
	if (overwrite) {
		strncpy(tmpname, LTT_FLIGHT_PREFIX, PATH_MAX-1);
		strncat(tmpname, channel_name,
			PATH_MAX-1-sizeof(LTT_FLIGHT_PREFIX));
	} else {
		strncpy(tmpname, channel_name, PATH_MAX-1);
	}
	strncat(tmpname, "_", PATH_MAX-1-strlen(tmpname));

	*ltt_chan = kzalloc(sizeof(struct ltt_channel_struct), GFP_KERNEL);
	if (!(*ltt_chan))
		goto ltt_chan_alloc_error;
	kref_init(&(*ltt_chan)->kref);

	(*ltt_chan)->trace = trace;
	(*ltt_chan)->buffer_begin = ltt_buffer_begin_callback;
	(*ltt_chan)->buffer_end = ltt_buffer_end_callback;
	(*ltt_chan)->overwrite = overwrite;
	if (strcmp(channel_name, LTT_COMPACT_CHANNEL) == 0)
		(*ltt_chan)->compact = 1;
	else
		(*ltt_chan)->compact = 0;
	(*ltt_chan)->trans_channel_data = relay_open(tmpname,
			dir,
			subbuf_size,
			n_subbufs,
			&trace->callbacks,
			*ltt_chan);
	tmpname_len = strlen(tmpname);
	if (tmpname_len > 0) {
		/* Remove final _ for pretty printing */
		tmpname[tmpname_len-1] = '\0';
	}
	if ((*ltt_chan)->trans_channel_data == NULL) {
		printk(KERN_ERR "LTT : Can't open %s channel for trace %s\n",
				tmpname, trace_name);
		goto relay_open_error;
	}

	strncpy((*ltt_chan)->channel_name, tmpname, PATH_MAX-1);

	err = 0;
	goto end;

relay_open_error:
	kfree(*ltt_chan);
	*ltt_chan = NULL;
ltt_chan_alloc_error:
	err = EPERM;
end:
	kfree(tmpname);
	return err;
}

static int ltt_relay_create_dirs(struct ltt_trace_struct *new_trace)
{
	new_trace->dentry.trace_root = debugfs_create_dir(new_trace->trace_name,
			ltt_root_dentry);
	if (new_trace->dentry.trace_root == NULL) {
		printk(KERN_ERR "LTT : Trace directory name %s already taken\n",
				new_trace->trace_name);
		return EEXIST;
	}

	new_trace->dentry.control_root = debugfs_create_dir(LTT_CONTROL_ROOT,
			new_trace->dentry.trace_root);
	if (new_trace->dentry.control_root == NULL) {
		printk(KERN_ERR "LTT : Trace control subdirectory name "\
				"%s/%s already taken\n",
				new_trace->trace_name, LTT_CONTROL_ROOT);
		debugfs_remove(new_trace->dentry.trace_root);
		return EEXIST;
	}

	new_trace->callbacks.subbuf_start = ltt_subbuf_start_callback;
	new_trace->callbacks.buf_mapped = ltt_buf_mapped_callback;
	new_trace->callbacks.buf_unmapped = ltt_buf_unmapped_callback;
	new_trace->callbacks.create_buf_file = ltt_create_buf_file_callback;
	new_trace->callbacks.remove_buf_file = ltt_remove_buf_file_callback;

	return 0;
}

/*
 * LTTng channel flush function.
 *
 * Must be called when no tracing is active in the channel, because of
 * accesses across CPUs.
 */
static void ltt_relay_buffer_flush(struct rchan_buf *buf)
{
	buf->finalized = 1;
	ltt_force_switch(buf, FORCE_FLUSH);
}

static void ltt_relay_async_wakeup_chan(struct ltt_channel_struct *ltt_channel)
{
	unsigned int i;
	struct rchan *rchan = ltt_channel->trans_channel_data;

	for_each_possible_cpu(i) {
		if (atomic_read(&ltt_channel->buf[i].wakeup_readers) == 1) {
			atomic_set(&ltt_channel->buf[i].wakeup_readers, 0);
			wake_up_interruptible(&rchan->buf[i]->read_wait);
		}
	}
}

/*
 * Wake writers :
 *
 * This must be done after the trace is removed from the RCU list so that there
 * are no stalled writers.
 */
static void ltt_relay_wake_writers(struct ltt_channel_buf_struct *ltt_buf)
{

	if (waitqueue_active(&ltt_buf->write_wait))
		schedule_work(&ltt_buf->wake_writers);
}

static void ltt_relay_finish_buffer(struct ltt_channel_struct *ltt_channel,
		unsigned int cpu)
{
	struct rchan *rchan = ltt_channel->trans_channel_data;
	struct ltt_channel_buf_struct *ltt_buf;

	if (rchan->buf[cpu]) {
		ltt_buf = &ltt_channel->buf[cpu];;
		ltt_relay_buffer_flush(rchan->buf[cpu]);
		ltt_relay_wake_writers(ltt_buf);
	}
}


static void ltt_relay_finish_channel(struct ltt_channel_struct *ltt_channel)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		ltt_relay_finish_buffer(ltt_channel, i);
	}
}

static void ltt_relay_remove_channel(struct ltt_channel_struct *channel)
{
	struct rchan *rchan = channel->trans_channel_data;

	relay_close(rchan);
	kref_put(&channel->kref, ltt_relay_release_channel);
}

struct ltt_reserve_switch_offsets {
	long begin, end, old;
	long begin_switch, end_switch_current, end_switch_old;
	long commit_count, reserve_commit_diff;
	size_t before_hdr_pad, size;
};

/*
 * Returns :
 * 0 if ok
 * !0 if execution must be aborted.
 */
static inline int ltt_relay_try_reserve(
		struct ltt_channel_struct *ltt_channel,
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets, size_t data_size,
		u64 *tsc)
{
	offsets->begin = local_read(&ltt_buf->offset);
	offsets->old = offsets->begin;
	offsets->begin_switch = 0;
	offsets->end_switch_current = 0;
	offsets->end_switch_old = 0;

	if (SUBBUF_OFFSET(offsets->begin, buf->chan) == 0) {
		offsets->begin_switch = 1;		/* For offsets->begin */
	} else {
		offsets->size = ltt_get_header_size(ltt_channel,
				buf->start + offsets->begin, data_size,
				&offsets->before_hdr_pad) + data_size;
		if ((SUBBUF_OFFSET(offsets->begin, buf->chan) + offsets->size)
				> buf->chan->subbuf_size) {
			offsets->end_switch_old = 1;	/* For offsets->old */
			offsets->begin_switch = 1;	/* For offsets->begin */
		}
	}
	if (offsets->begin_switch) {
		if (offsets->end_switch_old)
			offsets->begin = SUBBUF_ALIGN(offsets->begin,
						buf->chan);
		offsets->begin = offsets->begin + ltt_subbuf_header_len();
		/* Test new buffer integrity */
		offsets->reserve_commit_diff =
			SUBBUF_OFFSET(buf->chan->subbuf_size
			- local_read(&ltt_buf->commit_count[
				SUBBUF_INDEX(offsets->begin, buf->chan)]),
					buf->chan);
		if (offsets->reserve_commit_diff == 0) {
			/* Next buffer not corrupted. */
			if (!ltt_channel->overwrite &&
				(SUBBUF_TRUNC(offsets->begin, buf->chan)
				- SUBBUF_TRUNC(
					atomic_long_read(&ltt_buf->consumed),
					buf->chan))
				>= rchan->alloc_size) {
				/*
				 * We do not overwrite non consumed buffers
				 * and we are full : event is lost.
				 */
				local_inc(&ltt_buf->events_lost);
				return -1;
			} else {
				/*
				 * next buffer not corrupted, we are either in
				 * overwrite mode or the buffer is not full.
				 * It's safe to write in this new subbuffer.
				 */
			}
		} else {
			/*
			 * Next subbuffer corrupted. Force pushing reader even
			 * in normal mode. It's safe to write in this new
			 * subbuffer.
			 */
		}
		offsets->size = ltt_get_header_size(ltt_channel,
			buf->start + offsets->begin, data_size,
			&offsets->before_hdr_pad) + data_size;
		if ((SUBBUF_OFFSET(offsets->begin, buf->chan) + offsets->size)
				> buf->chan->subbuf_size) {
			/*
			 * Event too big for subbuffers, report error, don't
			 * complete the sub-buffer switch.
			 */
			local_inc(&ltt_buf->events_lost);
			return -1;
		} else {
			/*
			 * We just made a successful buffer switch and the event
			 * fits in the new subbuffer. Let's write.
			 */
		}
	} else {
		/*
		 * Event fits in the current buffer and we are not on a switch
		 * boundary. It's safe to write.
		 */
	}
	offsets->end = offsets->begin + offsets->size;

	if ((SUBBUF_OFFSET(offsets->end, buf->chan)) == 0) {
		/*
		 * The offset_end will fall at the very beginning of the next
		 * subbuffer.
		 */
		offsets->end_switch_current = 1;	/* For offsets->begin */
	}
#ifdef CONFIG_LTT_HEARTBEAT
	if (offsets->begin_switch || offsets->end_switch_old
			|| offsets->end_switch_current)
		*tsc = ltt_get_timestamp64();
	else
		*tsc = ltt_get_timestamp32();
#else
	*tsc = ltt_get_timestamp64();
#endif
	return 0;
}

/*
 * Returns :
 * 0 if ok
 * !0 if execution must be aborted.
 */
static inline int ltt_relay_try_switch(
		enum force_switch_mode mode,
		struct ltt_channel_struct *ltt_channel,
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets,
		u64 *tsc)
{
	offsets->begin = local_read(&ltt_buf->offset);
	offsets->old = offsets->begin;
	offsets->end_switch_old = 0;

	if (SUBBUF_OFFSET(offsets->begin, buf->chan) != 0) {
		offsets->begin = SUBBUF_ALIGN(offsets->begin, buf->chan);
		offsets->end_switch_old = 1;
	} else {
		/* we do not have to switch : buffer is empty */
		return -1;
	}
	if (mode == FORCE_ACTIVE)
		offsets->begin += ltt_subbuf_header_len();
	/*
	 * Always begin_switch in FORCE_ACTIVE mode.
	 * Test new buffer integrity
	 */
	offsets->reserve_commit_diff = SUBBUF_OFFSET(buf->chan->subbuf_size
		- local_read(
		&ltt_buf->commit_count[SUBBUF_INDEX(offsets->begin,
					buf->chan)]), buf->chan);
	if (offsets->reserve_commit_diff == 0) {
		/* Next buffer not corrupted. */
		if (mode == FORCE_ACTIVE && !ltt_channel->overwrite &&
		(offsets->begin - atomic_long_read(&ltt_buf->consumed))
			>= rchan->alloc_size) {
			/*
			 * We do not overwrite non consumed buffers and we are
			 * full : ignore switch while tracing is active.
			 */
			return -1;
		}
	} else {
		/*
		 * Next subbuffer corrupted. Force pushing reader even in normal
		 * mode
		 */
	}
	offsets->end = offsets->begin;
	*tsc = ltt_get_timestamp64();
	return 0;
}

static inline void ltt_reserve_push_reader(
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets)
{
	long consumed_old, consumed_new;

	do {
		consumed_old = atomic_long_read(&ltt_buf->consumed);
		/*
		 * If buffer is in overwrite mode, push the reader consumed
		 * count if the write position has reached it and we are not
		 * at the first iteration (don't push the reader farther than
		 * the writer). This operation can be done concurrently by many
		 * writers in the same buffer, the writer being at the farthest
		 * write position sub-buffer index in the buffer being the one
		 * which will win this loop.
		 * If the buffer is not in overwrite mode, pushing the reader
		 * only happens if a sub-buffer is corrupted.
		 */
		if ((SUBBUF_TRUNC(offsets->end-1, buf->chan)
			- SUBBUF_TRUNC(consumed_old, buf->chan))
					>= rchan->alloc_size)
			consumed_new = SUBBUF_ALIGN(consumed_old, buf->chan);
		else {
			consumed_new = consumed_old;
			break;
		}
	} while (atomic_long_cmpxchg(&ltt_buf->consumed, consumed_old,
			consumed_new) != consumed_old);

	if (consumed_old != consumed_new) {
		/*
		 * Reader pushed : we are the winner of the push, we can
		 * therefore reequilibrate reserve and commit. Atomic increment
		 * of the commit count permits other writers to play around
		 * with this variable before us. We keep track of
		 * corrupted_subbuffers even in overwrite mode :
		 * we never want to write over a non completely committed
		 * sub-buffer : possible causes : the buffer size is too low
		 * compared to the unordered data input, or there is a writer
		 * that died between the reserve and the commit.
		 */
		if (offsets->reserve_commit_diff) {
			/*
			 * We have to alter the sub-buffer commit count : a
			 * sub-buffer is corrupted. We do not deliver it.
			 */
			local_add(
				offsets->reserve_commit_diff,
				&ltt_buf->commit_count[
				SUBBUF_INDEX(offsets->begin, buf->chan)]);
			local_inc(&ltt_buf->corrupted_subbuffers);
		}
	}
}


/*
 * ltt_reserve_switch_old_subbuf: switch old subbuffer
 *
 * Concurrency safe because we are the last and only thread to alter this
 * sub-buffer. As long as it is not delivered and read, no other thread can
 * alter the offset, alter the reserve_count or call the
 * client_buffer_end_callback on this sub-buffer.
 *
 * The only remaining threads could be the ones with pending commits. They will
 * have to do the deliver themselves.  Not concurrency safe in overwrite mode.
 * We detect corrupted subbuffers with commit and reserve counts. We keep a
 * corrupted sub-buffers count and push the readers across these sub-buffers.
 *
 * Not concurrency safe if a writer is stalled in a subbuffer and another writer
 * switches in, finding out it's corrupted.  The result will be than the old
 * (uncommited) subbuffer will be declared corrupted, and that the new subbuffer
 * will be declared corrupted too because of the commit count adjustment.
 *
 * Note : offset_old should never be 0 here.
 */
static inline void ltt_reserve_switch_old_subbuf(
		struct ltt_channel_struct *ltt_channel,
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets, u64 *tsc)
{
		ltt_channel->buffer_end(buf, *tsc, offsets->old,
			SUBBUF_INDEX((offsets->old-1), buf->chan));
		/* Must write buffer end before incrementing commit count */
		smp_wmb();
		offsets->commit_count =
			local_add_return(buf->chan->subbuf_size
				- (SUBBUF_OFFSET(offsets->old-1, buf->chan)+1),
				&ltt_buf->commit_count[SUBBUF_INDEX(
						offsets->old-1, buf->chan)]);
		if (SUBBUF_OFFSET(offsets->commit_count, buf->chan) == 0)
			ltt_deliver(buf, SUBBUF_INDEX((offsets->old-1),
						buf->chan), NULL);
}

/*
 * ltt_reserve_switch_new_subbuf: Populate new subbuffer.
 *
 * This code can be executed unordered : writers may already have written to the
 * sub-buffer before this code gets executed, caution.  The commit makes sure
 * that this code is executed before the deliver of this sub-buffer.
 */
static inline void ltt_reserve_switch_new_subbuf(
		struct ltt_channel_struct *ltt_channel,
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets, u64 *tsc)
{
	ltt_channel->buffer_begin(buf, *tsc, SUBBUF_INDEX(offsets->begin,
				buf->chan));
	/* Must write buffer end before incrementing commit count */
	smp_wmb();
	offsets->commit_count = local_add_return(ltt_subbuf_header_len(),
			&ltt_buf->commit_count[
				SUBBUF_INDEX(offsets->begin, buf->chan)]);
	/* Check if the written buffer has to be delivered */
	if (SUBBUF_OFFSET(offsets->commit_count, buf->chan) == 0)
		ltt_deliver(buf, SUBBUF_INDEX(offsets->begin, buf->chan), NULL);
}


/*
 * ltt_reserve_end_switch_current: finish switching current subbuffer
 *
 * Concurrency safe because we are the last and only thread to alter this
 * sub-buffer. As long as it is not delivered and read, no other thread can
 * alter the offset, alter the reserve_count or call the
 * client_buffer_end_callback on this sub-buffer.
 *
 * The only remaining threads could be the ones with pending commits. They will
 * have to do the deliver themselves.  Not concurrency safe in overwrite mode.
 * We detect corrupted subbuffers with commit and reserve counts. We keep a
 * corrupted sub-buffers count and push the readers across these sub-buffers.
 *
 * Not concurrency safe if a writer is stalled in a subbuffer and another writer
 * switches in, finding out it's corrupted.  The result will be than the old
 * (uncommited) subbuffer will be declared corrupted, and that the new subbuffer
 * will be declared corrupted too because of the commit count adjustment.
 */
static inline void ltt_reserve_end_switch_current(
		struct ltt_channel_struct *ltt_channel,
		struct ltt_channel_buf_struct *ltt_buf, struct rchan *rchan,
		struct rchan_buf *buf,
		struct ltt_reserve_switch_offsets *offsets, u64 *tsc)
{
	ltt_channel->buffer_end(buf, *tsc, offsets->end,
		SUBBUF_INDEX((offsets->end-1), buf->chan));
	/* Must write buffer begin before incrementing commit count */
	smp_wmb();
	offsets->commit_count =
		local_add_return(buf->chan->subbuf_size
			- (SUBBUF_OFFSET(offsets->end-1, buf->chan)+1),
			&ltt_buf->commit_count[SUBBUF_INDEX(
					offsets->end-1, buf->chan)]);
	if (SUBBUF_OFFSET(offsets->commit_count, buf->chan) == 0)
		ltt_deliver(buf, SUBBUF_INDEX((offsets->end-1),
			buf->chan), NULL);
}

/**
 * ltt_relay_reserve_slot: Atomic slot reservation in a LTTng buffer.
 * @trace : the trace structure to log to.
 * @ltt_channel : channel structure
 * @transport_data : data structure specific to ltt relay
 * @data_size : size of the variable length data to log.
 * @slot_size : pointer to total size of the slot (out)
 * @tsc : pointer to the tsc at the slot reservation (out)
 * @cpu : cpuid
 *
 * Return : NULL if not enough space, else returns the pointer
 * 		to the beginning of the reserved slot, aligned for the
 * 		event header.
 * It will take care of sub-buffer switching.
 */
static void *ltt_relay_reserve_slot(struct ltt_trace_struct *trace,
		struct ltt_channel_struct *ltt_channel, void **transport_data,
		size_t data_size, size_t *slot_size, u64 *tsc, int cpu)
{
	struct rchan *rchan = ltt_channel->trans_channel_data;
	struct rchan_buf *buf = *transport_data =
		rchan->buf[cpu];
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];

	struct ltt_reserve_switch_offsets offsets;

	offsets.reserve_commit_diff = 0;
	offsets.size = 0;

	/*
	 * Perform retryable operations.
	 */
	if (ltt_nesting[smp_processor_id()] > 4) {
		local_inc(&ltt_buf->events_lost);
		return NULL;
	}
	do {

		if (ltt_relay_try_reserve(ltt_channel, ltt_buf,
				rchan, buf, &offsets, data_size, tsc))
			return NULL;
	} while (local_cmpxchg(&ltt_buf->offset, offsets.old,
			offsets.end) != offsets.old);

	/*
	 * Push the reader if necessary
	 */
	ltt_reserve_push_reader(ltt_buf, rchan, buf, &offsets);

	/*
	 * Switch old subbuffer if needed.
	 */
	if (offsets.end_switch_old)
		ltt_reserve_switch_old_subbuf(ltt_channel, ltt_buf, rchan, buf,
			&offsets, tsc);

	/*
	 * Populate new subbuffer.
	 */
	if (offsets.begin_switch)
		ltt_reserve_switch_new_subbuf(ltt_channel, ltt_buf, rchan,
			buf, &offsets, tsc);

	if (offsets.end_switch_current)
		ltt_reserve_end_switch_current(ltt_channel, ltt_buf, rchan,
			buf, &offsets, tsc);

	*slot_size = offsets.size;

	return buf->start + BUFFER_OFFSET(offsets.begin, buf->chan)
		+ offsets.before_hdr_pad;
}

/*
 * Force a sub-buffer switch for a per-cpu buffer. This operation is
 * completely reentrant : can be called while tracing is active with
 * absolutely no lock held.
 *
 * Note, however, that as a local_cmpxchg is used for some atomic
 * operations, this function must be called from the CPU which owns the buffer
 * for a ACTIVE flush.
 */
static void ltt_force_switch(struct rchan_buf *buf, enum force_switch_mode mode)
{
	struct ltt_channel_struct *ltt_channel =
			(struct ltt_channel_struct *)buf->chan->private_data;
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];
	struct rchan *rchan = ltt_channel->trans_channel_data;
	struct ltt_reserve_switch_offsets offsets;
	u64 tsc;

	offsets.reserve_commit_diff = 0;
	offsets.size = 0;

	/*
	 * Perform retryable operations.
	 */
	do {
		if (ltt_relay_try_switch(mode, ltt_channel, ltt_buf,
				rchan, buf, &offsets, &tsc))
			return;
	} while (local_cmpxchg(&ltt_buf->offset, offsets.old,
			offsets.end) != offsets.old);

	/*
	 * Push the reader if necessary
	 */
	if (mode == FORCE_ACTIVE)
		ltt_reserve_push_reader(ltt_buf, rchan, buf, &offsets);

	/*
	 * Switch old subbuffer if needed.
	 */
	if (offsets.end_switch_old)
		ltt_reserve_switch_old_subbuf(ltt_channel, ltt_buf, rchan, buf,
			&offsets, &tsc);

	/*
	 * Populate new subbuffer.
	 */
	if (mode == FORCE_ACTIVE)
		if (offsets.begin_switch)
			ltt_reserve_switch_new_subbuf(ltt_channel,
				ltt_buf, rchan, buf, &offsets, &tsc);
}

/*
 * Atomic unordered slot commit. Increments the commit count in the
 * specified sub-buffer, and delivers it if necessary.
 *
 * Parameters:
 *
 * @ltt_channel : channel structure
 * @transport_data: transport-specific data
 * @reserved : address following the event header.
 * @slot_size : size of the reserved slot.
 */
static void ltt_relay_commit_slot(struct ltt_channel_struct *ltt_channel,
		void **transport_data, void *reserved, size_t slot_size)
{
	struct rchan_buf *buf = *transport_data;
	struct ltt_channel_buf_struct *ltt_buf = &ltt_channel->buf[buf->cpu];
	unsigned int offset_end = reserved - buf->start;
	long commit_count;

	/* Must write slot data before incrementing commit count */
	smp_wmb();
	commit_count = local_add_return(slot_size,
		&ltt_buf->commit_count[SUBBUF_INDEX(offset_end-1, buf->chan)]);
	/* Check if all commits have been done */
	if (SUBBUF_OFFSET(commit_count, buf->chan) == 0)
		ltt_deliver(buf, SUBBUF_INDEX(offset_end-1, buf->chan), NULL);
	/*
	 * Update lost_size for each commit. It's needed only for extracting
	 * ltt buffers from vmcore, after crash.
	 */
	ltt_write_commit_counter(buf, reserved, slot_size);
}

/*
 * This is called with preemption disabled when user space has requested
 * blocking mode.  If one of the active traces has free space below a
 * specific threshold value, we reenable preemption and block.
 */
static int ltt_relay_user_blocking(struct ltt_trace_struct *trace,
		unsigned int index, size_t data_size, struct user_dbg_data *dbg)
{
	struct rchan *rchan;
	struct ltt_channel_buf_struct *ltt_buf;
	struct ltt_channel_struct *channel;
	struct rchan_buf *relay_buf;
	int cpu;
	DECLARE_WAITQUEUE(wait, current);

	channel = ltt_get_channel_from_index(trace, index);
	rchan = channel->trans_channel_data;
	cpu = smp_processor_id();
	relay_buf = rchan->buf[cpu];
	ltt_buf = &channel->buf[cpu];
	/*
	 * Check if data is too big for the channel : do not
	 * block for it.
	 */
	if (LTT_RESERVE_CRITICAL + data_size > relay_buf->chan->subbuf_size)
		return 0;

	/*
	 * If free space too low, we block. We restart from the
	 * beginning after we resume (cpu id may have changed
	 * while preemption is active).
	 */
	spin_lock(&ltt_buf->full_lock);
	if (!channel->overwrite &&
			(dbg->avail_size = (dbg->write = local_read(
				&channel->buf[relay_buf->cpu].offset))
			+ LTT_RESERVE_CRITICAL + data_size
			 - SUBBUF_TRUNC((dbg->read = atomic_long_read(
			&channel->buf[relay_buf->cpu].consumed)),
			relay_buf->chan)) >= rchan->alloc_size) {
		__set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&ltt_buf->write_wait, &wait);
		spin_unlock(&ltt_buf->full_lock);
		preempt_enable();
		schedule();
		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&ltt_buf->write_wait, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS;
		preempt_disable();
		return 1;
	}
	spin_unlock(&ltt_buf->full_lock);
	return 0;
}

static void ltt_relay_print_user_errors(struct ltt_trace_struct *trace,
		unsigned int index, size_t data_size, struct user_dbg_data *dbg,
		int cpu)
{
	struct rchan *rchan;
	struct ltt_channel_buf_struct *ltt_buf;
	struct ltt_channel_struct *channel;
	struct rchan_buf *relay_buf;

	channel = ltt_get_channel_from_index(trace, index);
	rchan = channel->trans_channel_data;
	relay_buf = rchan->buf[cpu];
	ltt_buf = &channel->buf[cpu];
	printk(KERN_ERR "Error in LTT usertrace : "
	"buffer full : event lost in blocking "
	"mode. Increase LTT_RESERVE_CRITICAL.\n");
	printk(KERN_ERR "LTT nesting level is %u.\n",
		ltt_nesting[cpu]);
	printk(KERN_ERR "LTT avail size %lu.\n",
		dbg->avail_size);
	printk(KERN_ERR "avai write : %lu, read : %lu\n",
			dbg->write, dbg->read);
	printk(KERN_ERR "LTT cur size %lu.\n",
		(dbg->write = local_read(
		&channel->buf[relay_buf->cpu].offset))
	+ LTT_RESERVE_CRITICAL + data_size
	 - SUBBUF_TRUNC((dbg->read = atomic_long_read(
	&channel->buf[relay_buf->cpu].consumed)),
				relay_buf->chan));
	printk(KERN_ERR "cur write : %lu, read : %lu\n",
			dbg->write, dbg->read);
}

static struct ltt_transport ltt_relay_transport = {
	.name = "relay",
	.owner = THIS_MODULE,
	.ops = {
		.create_dirs = ltt_relay_create_dirs,
		.remove_dirs = ltt_relay_remove_dirs,
		.create_channel = ltt_relay_create_channel,
		.finish_channel = ltt_relay_finish_channel,
		.remove_channel = ltt_relay_remove_channel,
		.wakeup_channel = ltt_relay_async_wakeup_chan,
		.commit_slot = ltt_relay_commit_slot,
		.reserve_slot = ltt_relay_reserve_slot,
		.user_blocking = ltt_relay_user_blocking,
		.user_errors = ltt_relay_print_user_errors,
	},
};

static int __init ltt_relay_init(void)
{
	printk(KERN_INFO "LTT : ltt-relay init\n");
	ltt_root_dentry = debugfs_create_dir(LTT_RELAY_ROOT, NULL);
	if (ltt_root_dentry == NULL)
		return -EEXIST;

	ltt_file_operations = relay_file_operations;
	ltt_file_operations.owner = THIS_MODULE;
	ltt_file_operations.poll = ltt_poll;
	ltt_file_operations.ioctl = ltt_ioctl;
#ifdef CONFIG_COMPAT
	ltt_file_operations.compat_ioctl = ltt_compat_ioctl;
#endif

	ltt_transport_register(&ltt_relay_transport);

	return 0;
}

static void __exit ltt_relay_exit(void)
{
	printk(KERN_INFO "LTT : ltt-relay exit\n");

	ltt_transport_unregister(&ltt_relay_transport);

	debugfs_remove(ltt_root_dentry);
}

module_init(ltt_relay_init);
module_exit(ltt_relay_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("Linux Trace Toolkit Next Generation Tracer");
