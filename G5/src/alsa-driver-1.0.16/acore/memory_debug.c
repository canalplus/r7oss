#define __NO_VERSION__
/*
 *  Memory allocation helpers.
 *
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "adriver.h"
#include <linux/init.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/info.h>


/*
 *  memory allocation helpers and debug routines
 */

struct snd_alloc_track {
	unsigned long magic;
	void *caller;
	size_t size;
	struct list_head list;
	long data[0];
};

#define snd_alloc_track_entry(obj) (struct snd_alloc_track *)((char*)obj - (unsigned long)((struct snd_alloc_track *)0)->data)

static long snd_alloc_kmalloc;
static LIST_HEAD(snd_alloc_kmalloc_list);
static DEFINE_SPINLOCK(snd_alloc_kmalloc_lock);
#define KMALLOC_MAGIC 0x87654321

void snd_memory_done(void)
{
	struct list_head *head;
	struct snd_alloc_track *t;

	if (snd_alloc_kmalloc > 0)
		printk(KERN_ERR "snd: Not freed snd_alloc_kmalloc = %li\n", snd_alloc_kmalloc);
	list_for_each_prev(head, &snd_alloc_kmalloc_list) {
		t = list_entry(head, struct snd_alloc_track, list);
		if (t->magic != KMALLOC_MAGIC) {
			printk(KERN_ERR "snd: Corrupted kmalloc\n");
			break;
		}
		printk(KERN_ERR "snd: kmalloc(%ld) from %p not freed\n", (long) t->size, t->caller);
	}
}

static void *__snd_kmalloc(size_t size, gfp_t gfp_flags, void *caller)
{
	unsigned long flags;
	struct snd_alloc_track *t;
	void *ptr;
	
	ptr = snd_wrapper_kmalloc(size + sizeof(struct snd_alloc_track), gfp_flags);
	if (ptr != NULL) {
		t = (struct snd_alloc_track *)ptr;
		t->magic = KMALLOC_MAGIC;
		t->caller = caller;
		spin_lock_irqsave(&snd_alloc_kmalloc_lock, flags);
		list_add_tail(&t->list, &snd_alloc_kmalloc_list);
		spin_unlock_irqrestore(&snd_alloc_kmalloc_lock, flags);
		t->size = size;
		snd_alloc_kmalloc += size;
		ptr = t->data;
	}
	return ptr;
}

#define _snd_kmalloc(size, flags) __snd_kmalloc((size), (flags), __builtin_return_address(0));
void *snd_hidden_kmalloc(size_t size, gfp_t gfp_flags)
{
	return _snd_kmalloc(size, gfp_flags);
}

void *snd_hidden_kzalloc(size_t size, gfp_t gfp_flags)
{
	void *ret = _snd_kmalloc(size, gfp_flags);
	if (ret)
		memset(ret, 0, size);
	return ret;
}

void *snd_hidden_kcalloc(size_t n, size_t size, gfp_t gfp_flags)
{
	void *ret = NULL;
	if (n != 0 && size > INT_MAX / n)
		return ret;
	return snd_hidden_kzalloc(n * size, gfp_flags);
}

void snd_hidden_kfree(const void *obj)
{
	unsigned long flags;
	struct snd_alloc_track *t;
	if (obj == NULL)
		return;
	t = snd_alloc_track_entry(obj);
	if (t->magic != KMALLOC_MAGIC) {
		printk(KERN_WARNING "snd: bad kfree (called from %p)\n", __builtin_return_address(0));
		return;
	}
	spin_lock_irqsave(&snd_alloc_kmalloc_lock, flags);
	list_del(&t->list);
	spin_unlock_irqrestore(&snd_alloc_kmalloc_lock, flags);
	t->magic = 0;
	snd_alloc_kmalloc -= t->size;
	obj = t;
	snd_wrapper_kfree(obj);
}

char *snd_hidden_kstrdup(const char *s, gfp_t gfp_flags)
{
	int len;
	char *buf;

	if (!s) return NULL;

	len = strlen(s) + 1;
	buf = _snd_kmalloc(len, gfp_flags);
	if (buf)
		memcpy(buf, s, len);
	return buf;
}

#ifdef CONFIG_PROC_FS
static struct snd_info_entry *snd_memory_info_entry;

static void snd_memory_info_read(struct snd_info_entry *entry, struct snd_info_buffer *buffer)
{
	snd_iprintf(buffer, "kmalloc: %li bytes\n", snd_alloc_kmalloc);
}

#define PROC_FILE_NAME	"driver/snd-memory-debug"

int __init snd_memory_info_init(void)
{
	struct snd_info_entry *entry;

	entry = snd_info_create_module_entry(THIS_MODULE, "meminfo", NULL);
	if (entry) {
		entry->c.text.read = snd_memory_info_read;
		if (snd_info_register(entry) < 0) {
			snd_info_free_entry(entry);
			entry = NULL;
		}
	}
	snd_memory_info_entry = entry;
	return 0;
}

void __exit snd_memory_info_done(void)
{
	snd_info_free_entry(snd_memory_info_entry);
}
#endif
