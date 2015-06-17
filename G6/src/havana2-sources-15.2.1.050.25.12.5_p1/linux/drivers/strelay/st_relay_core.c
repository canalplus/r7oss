/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ratelimit.h>
#include <linux/printk.h>

#include "st_relay_core.h"

MODULE_DESCRIPTION("strelay core functions");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(st_relay_core_register);
EXPORT_SYMBOL(st_relay_core_unregister);
EXPORT_SYMBOL(st_relay_core_write_headerandbuffer);

#define SUBBUF_SIZE_DEF  768*1024
#define SUBBUF_SIZE_HIGH 768*4096
#define SUBBUF_COUNT_DEF  10
#define SUBBUF_COUNT_HIGH 40

unsigned int subbuf_size = 0;
module_param(subbuf_size, uint, S_IRUGO);
MODULE_PARM_DESC(subbuf_size, "subbuffer size; must be multiple of 768");

unsigned int subbuf_count = 0;
module_param(subbuf_count, uint, S_IRUGO);
MODULE_PARM_DESC(subbuf_count, "subbuffer count");

/* memory profiles */
int memory_profile = 1;
module_param(memory_profile, int, S_IRUGO);
MODULE_PARM_DESC(memory_profile,
                 "<0 no allocations (unactivation!); 0=manual setting; 1=default (768*1024,10); >1 high (768*4096,40)");

int st_relay_refcount = 0;
struct dentry *st_relay_dir = NULL;
struct rchan *st_relay_chan = NULL;
DEFINE_SPINLOCK(st_relay_slock);
DEFINE_MUTEX(st_relay_mlock);

#ifndef CONFIG_RELAY
#warn "file unexpectedly compiled with no relay support"
#endif
#ifndef CONFIG_DEBUG_FS
#warn "file unexpectedly compiled with no debugfs support"
#endif

/* file_create() callback.  Creates relay file in debugfs  */
static struct dentry *st_create_buf_file_handler(const char *filename,
                                                 struct dentry *parent,
                                                 umode_t mode,
                                                 struct rchan_buf *buf,
                                                 int *is_global)
{
	*is_global = 1;
	return debugfs_create_file(filename, mode, parent, buf, &relay_file_operations);
}

/* file_remove() callback.  Removes relay file in debugfs */
static int st_remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);
	return 0;
}

/* relayfs callbacks */
struct rchan_callbacks st_relay_callbacks = {
	.create_buf_file = st_create_buf_file_handler,
	.remove_buf_file = st_remove_buf_file_handler,
};

//cloned this from kernel's _relay_write to add length written return value
//and avoid changing it in the kernel tree itself.
//called under spin_lock_irqsave
static inline size_t st_relay_write(struct rchan *chan,
                                    const void *data,
                                    size_t length,
                                    const char *name)
{
	struct rchan_buf *buf;
	size_t rlength = length;

	buf = chan->buf[smp_processor_id()];
	if (unlikely(buf->offset + length > chan->subbuf_size)) {
		rlength = relay_switch_subbuf(buf, length); // either returns 0 or length
		if (rlength == 0) {
			pr_err_ratelimited("Error: %s relay_switch_subbuf nok for %s (buffers full)\n", __func__, name);
			goto write_out;
		}
	}

	memcpy(buf->data + buf->offset, data, rlength);
	buf->offset += rlength;

write_out:
	return rlength;
}

size_t st_relay_core_write_headerandbuffer(const struct st_relay_entry_header_s *header, size_t headerlength,
                                           const void *buf, size_t buflength)
{
	unsigned long flags;
	int res = 0;
	const char *name = "n.a.";

	spin_lock_irqsave(&st_relay_slock, flags);

	if (st_relay_chan == NULL) {
		goto core_write_out;
	}

	if (headerlength != 0) {
		BUG_ON(header == NULL);
		// write provided header
		name = &header->name[0];
		res = st_relay_write(st_relay_chan, header, headerlength, name);
		if (res == 0) {
			// if header was provided and failed to write: don't write buffer
			goto core_write_out;
		}
	}

	if (buflength != 0) {
		BUG_ON(buf == NULL);
		// write provided buffer
		res = st_relay_write(st_relay_chan, buf, buflength, name);
	}

core_write_out:
	spin_unlock_irqrestore(&st_relay_slock, flags);

	// return 0 in case of failure
	return res;
}

struct st_relay_entry_s *st_relay_core_register(int nb_entries, struct st_relay_typename_s *entries_tn_init)
{
	struct st_relay_entry_s *rentries;
	int n;
	void *err = NULL;

	pr_debug("%s %d-%p\n", __func__, nb_entries, entries_tn_init);

	if (entries_tn_init == NULL || nb_entries == 0) {
		return ERR_PTR(-EINVAL);
	}
	if (subbuf_size == 0 || subbuf_count == 0) {
		return ERR_PTR(-ENODEV);
	}

	// can be called by several core users => refcount management
	mutex_lock(&st_relay_mlock);

	if (st_relay_dir == NULL) {
		st_relay_dir = debugfs_create_dir("st_relay", NULL);
		if (IS_ERR(st_relay_dir)) {
			err = st_relay_dir;
			st_relay_dir = NULL;
			// fallthrough next if
		}
		if (st_relay_dir == NULL) {
			pr_err("Error: %s Couldn't create debugfs st_relay directory\n", __func__);
			mutex_unlock(&st_relay_mlock);
			return err;
		}
	}

	if (st_relay_chan == NULL) {
		st_relay_chan = relay_open("data", st_relay_dir, subbuf_size, subbuf_count, &st_relay_callbacks, 0);
		if (st_relay_chan == NULL) {
			pr_err("Error: %s Couldn't create relay channel data\n", __func__);
			debugfs_remove(st_relay_dir);
			st_relay_dir = NULL;
			mutex_unlock(&st_relay_mlock);
			return NULL;
		}
	}

	st_relay_refcount++;

	mutex_unlock(&st_relay_mlock);

	// clear and init entries table
	// and set up debugfs activable entries
	rentries = kzalloc(sizeof(struct st_relay_entry_s) * nb_entries, GFP_KERNEL);
	if (rentries == NULL) {
		pr_err("Error: %s failed alloc\n", __func__);
		st_relay_core_unregister(0, NULL);
		return NULL;
	}
	for (n = 0; n < nb_entries; n++) {
		rentries[n].type = entries_tn_init[n].type;  // type not used by core itself: for user needs
		rentries[n].name = entries_tn_init[n].name;
		// rentries[n].active already cleared
		rentries[n].dentry = debugfs_create_u32(rentries[n].name, 0666, st_relay_dir, &rentries[n].active);
		if (rentries[n].dentry == NULL) {
			pr_err("Error: %s failed debugfs create for %s\n", __func__, rentries[n].name);
			// keep going though (tbc)
		}
	}

	// dont keep internal reference: trust callers to call unregister method prior to module unload..
	// else todo keep ref list
	return rentries;
}

void st_relay_core_unregister(int nb_entries, struct st_relay_entry_s *entries)
{
	int n;
	unsigned long flags;

	pr_debug("%s\n", __func__);
	if (entries && nb_entries) {
		for (n = 0; n < nb_entries; n++) {
			if (entries[n].dentry) {
				debugfs_remove(entries[n].dentry);
			}
		}

		kfree(entries);
	}

	// can be called by several core users => refcount management
	mutex_lock(&st_relay_mlock);

	st_relay_refcount--;

	if (st_relay_refcount > 0) {
		pr_debug("%s refcount:%d\n", __func__, st_relay_refcount);
		mutex_unlock(&st_relay_mlock);
		return;
	}

	st_relay_refcount = 0;

	spin_lock_irqsave(&st_relay_slock, flags);
	if (st_relay_chan) {
		relay_close(st_relay_chan);
		st_relay_chan = NULL;
	}
	spin_unlock_irqrestore(&st_relay_slock, flags);

	if (st_relay_dir) {
		debugfs_remove(st_relay_dir);
		st_relay_dir = NULL;
	}

	mutex_unlock(&st_relay_mlock);
}

int st_relay_core_open(void)
{
	if (memory_profile < 0) {
		subbuf_size  = 0;
		subbuf_count = 0;
	} else if (memory_profile == 1) {
		subbuf_size  = SUBBUF_SIZE_DEF;
		subbuf_count = SUBBUF_COUNT_DEF;
	} else if (memory_profile > 1) {
		subbuf_size  = SUBBUF_SIZE_HIGH;
		subbuf_count = SUBBUF_COUNT_HIGH;
	}
	pr_info("%s done ok %dx%d\n", __func__, subbuf_size, subbuf_count);
	return 0;
}
void st_relay_core_close(void)
{
	pr_info("%s done\n", __func__);
}

module_init(st_relay_core_open);
module_exit(st_relay_core_close);
