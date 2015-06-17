/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_dbgfs.c
   @brief  DebugFS wrapper

 */

#include "linuxcommon.h"

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/list.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 33)
static inline long __must_check IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}
#endif

#include "../pti_debug.h"
#include "../pti_tshal_api.h"
#include "../tango/pti_pdevice.h"
#include "../stfe/pti_tsinput.h"
#include "../stfe/pti_stfe.h"

#include "pti_debug_print.h"
#include "pti_debug_print_lite.h"

/* Generic debug object */
struct debug_object {
	struct list_head entry;
	FullHandle_t 	 handle;
	struct dentry   *de;
	struct dentry	*parent;
};

static LIST_HEAD(dbg_objs);
static struct dentry *demux_hal_debugfs_root = NULL;
static struct dentry *demux_hal_debugfs_tsInput = NULL;

/*
 * Open and Show wrappers
 */

#define DEBUG_ENTRY_OPS(name, ...)					    \
static int name##_show(struct seq_file *seq, void *v)			    \
{									    \
	return stptiHAL_Print_##name (					    \
		seq, (debug_print)seq_printf, seq->private, ##__VA_ARGS__); \
}									    \
									    \
static int name##_open(struct inode *inode, struct file *file)		    \
{									    \
	return single_open(file, name##_show, inode->i_private);	    \
}

DEBUG_ENTRY_OPS(pDevice);
DEBUG_ENTRY_OPS(pDeviceSharedMemory);
DEBUG_ENTRY_OPS(pDeviceTrigger, 0);
DEBUG_ENTRY_OPS(TPCycleCount);
DEBUG_ENTRY_OPS(UtilisationTP);
DEBUG_ENTRY_OPS(vDeviceInfo);
DEBUG_ENTRY_OPS(PIDTable);
DEBUG_ENTRY_OPS(SlotInfo);
DEBUG_ENTRY_OPS(FullSlotInfo);
DEBUG_ENTRY_OPS(BufferInfo);
DEBUG_ENTRY_OPS(BufferOverflowInfo);
DEBUG_ENTRY_OPS(IndexerInfo);
DEBUG_ENTRY_OPS(CAMData);
DEBUG_ENTRY_OPS(FullObjectTree);
DEBUG_ENTRY_OPS(PrintBuffer);
DEBUG_ENTRY_OPS(TSInput);

DEBUG_ENTRY_OPS(FullObjectTreeLite);

struct debugfs_entry {
	char *name;
	struct file_operations fops;
};

#define DEBUG_ENTRY(n) {			\
	.name = #n,				\
	.fops = {				\
		.owner = THIS_MODULE,		\
		.release = single_release,	\
		.read = seq_read,		\
		.llseek = seq_lseek,		\
		.open = n##_open,		\
	}					\
}

/*
 * Array of device wide debug entries
 */
static const struct debugfs_entry device_entries[] = {
	DEBUG_ENTRY(PrintBuffer),
};

static const struct debugfs_entry TSInput_entry = DEBUG_ENTRY(TSInput);

/*
 * Array of pDevice debug FS entries
 */
static const struct debugfs_entry pDevice_entries[] = {
	DEBUG_ENTRY(pDevice),
	DEBUG_ENTRY(pDeviceTrigger),
	DEBUG_ENTRY(pDeviceSharedMemory),
	DEBUG_ENTRY(TPCycleCount),
	DEBUG_ENTRY(UtilisationTP),
	DEBUG_ENTRY(vDeviceInfo),
	DEBUG_ENTRY(PIDTable),
	DEBUG_ENTRY(SlotInfo),
	DEBUG_ENTRY(FullSlotInfo),
	DEBUG_ENTRY(BufferInfo),
	DEBUG_ENTRY(BufferOverflowInfo),
	DEBUG_ENTRY(IndexerInfo),
	DEBUG_ENTRY(CAMData),
	DEBUG_ENTRY(FullObjectTree),
};

/*
 * Array of debug FS entries for Tango lite HAL
 */
static const struct debugfs_entry pDevice_lite_entries[] = {
	DEBUG_ENTRY(FullObjectTreeLite)
};

static void create_pDevice_entry(FullHandle_t handle, const struct debugfs_entry *array_p, unsigned int array_size)
{
	char name[16];
	struct dentry *de;
	struct debug_object *dbg;
	stptiHAL_pDevice_t *pDevice_p = stptiHAL_GetObjectpDevice_p(handle);
	int i;

	if (IS_ERR_OR_NULL(demux_hal_debugfs_root)) {
		pr_debug("no root");
		return;
	}

	dbg = kzalloc(sizeof(*dbg), GFP_KERNEL);
	if (!dbg) {
		pr_err("Unable to allocate 0x%08x\n", handle.word);
		return;
	}

	dbg->parent = demux_hal_debugfs_root;
	dbg->handle = handle;

	snprintf(name, sizeof(name), "pDevice.%d", handle.member.pDevice);
	dbg->de = debugfs_create_dir(name, dbg->parent);

	if (IS_ERR_OR_NULL(dbg->de)) {
		pr_err("couldn't create debugfs dir for pDevice %u : %ld\n",
			handle.member.pDevice, PTR_ERR(dbg->de));
		goto error;
	}

	for (i = 0; i < array_size; i++) {
		de = debugfs_create_file(array_p[i].name, S_IWUSR,
			dbg->de, pDevice_p, &array_p[i].fops);

		if (IS_ERR_OR_NULL(de)) {
			pr_warning("couldn't create %s file %u : %ld\n",
				pDevice_entries[i].name,
				handle.member.pDevice,
				PTR_ERR(de));
		}
	}

	list_add_tail(&dbg->entry, &dbg_objs);

	return;

error:
	if (!IS_ERR_OR_NULL(dbg->de))
		debugfs_remove_recursive(dbg->de);
	if (dbg)
		kfree(dbg);
	return;
}

void stptiHAL_debug_register_STFE(int numIBs, int numSWTSs, int numMIBs)
{
	int i;
	struct dentry *de;

	pr_debug("registering %d IBs %d SWTSs %d MIBs\n", numIBs, numSWTSs, numMIBs);

	if (IS_ERR_OR_NULL(demux_hal_debugfs_root)) {
		pr_debug("no root");
		return;
	}

	demux_hal_debugfs_tsInput = debugfs_create_dir("TSInput",
					demux_hal_debugfs_root);

	if (IS_ERR_OR_NULL(demux_hal_debugfs_tsInput)) {
		pr_warning("couldn't create tsinput directory\n");
		return;
	}

	for (i = 0; i < numIBs; i++) {
		char name[32];
		snprintf(name, sizeof(name), "IB.%d", i);
		de = debugfs_create_file(name, S_IWUSR, demux_hal_debugfs_tsInput, (void *)i, &TSInput_entry.fops);

		if (IS_ERR_OR_NULL(de)) {
			pr_warning("couldn't create %s : %ld\n",
				name,
				PTR_ERR(de));
		}
	}

	for (i = 0; i < numSWTSs; i++) {
		char name[32];
		snprintf(name, sizeof(name), "SWTS.%d", i);
		de = debugfs_create_file(
					name, S_IWUSR,
					demux_hal_debugfs_tsInput,
					(void *)(0x1000 | i),
					&TSInput_entry.fops);

		if (IS_ERR_OR_NULL(de)) {
			pr_warning("couldn't create %s : %ld\n",
				name,
				PTR_ERR(de));
		}
	}
	for (i = 0; i < numMIBs; i++) {
		char name[32];
		snprintf(name, sizeof(name), "MIB.%d", i);
		de = debugfs_create_file(
					name, S_IWUSR,
					demux_hal_debugfs_tsInput,
					(void *)(0x2000 | i),
					&TSInput_entry.fops);

		if (IS_ERR_OR_NULL(de)) {
			pr_warning("couldn't create %s : %ld\n",
				name,
				PTR_ERR(de));
		}
	}
}

void stptiHAL_debug_unregister_STFE(void)
{
	pr_debug("");
	if (!IS_ERR_OR_NULL(demux_hal_debugfs_tsInput)) {
		debugfs_remove_recursive(demux_hal_debugfs_tsInput);
	}
}

int stptiHAL_debug_init(void)
{
	int i;
	struct dentry *de;

	demux_hal_debugfs_root = debugfs_create_dir("stm_te_hal", NULL);
	if (IS_ERR_OR_NULL(demux_hal_debugfs_root)) {
		pr_warning("couldn't create hal debugfs root\n");
		return 0;
	}

	/* Create device wide debug here */
	for (i = 0; i < ARRAY_SIZE(device_entries); i++) {
		de = debugfs_create_file(
					device_entries[i].name, S_IWUSR,
					demux_hal_debugfs_root, NULL,
					&device_entries[i].fops);

		if (IS_ERR_OR_NULL(de)) {
			pr_warning("couldn't create %s : %ld\n",
				device_entries[i].name,
				PTR_ERR(de));
		}
	}

	return 0;
}

void stptiHAL_debug_term(void)
{
	if (!IS_ERR_OR_NULL(demux_hal_debugfs_root))
		debugfs_remove_recursive(demux_hal_debugfs_root);
}

void stptiHAL_debug_add_object(FullHandle_t handle)
{
	pr_debug("adding 0x%08x\n", handle.word);
	switch (handle.member.ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		create_pDevice_entry(handle,
				pDevice_entries,
				ARRAY_SIZE(pDevice_entries));
		break;
	default:
		break;
	}
}

void stptiHAL_debug_add_object_lite(FullHandle_t handle)
{
	pr_debug("adding 0x%08x\n", handle.word);
	switch (handle.member.ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		create_pDevice_entry(handle,
				pDevice_lite_entries,
				ARRAY_SIZE(pDevice_lite_entries));
		break;
	default:
		break;
	}
}

void stptiHAL_debug_rem_object(FullHandle_t handle)
{
	/* Search for handle in debug objects and delete */
	struct debug_object *dbg;
	pr_debug("removing 0x%08x\n", handle.word);
	list_for_each_entry(dbg, &dbg_objs, entry) {
		if (stptiOBJMAN_IsHandlesEqual(dbg->handle, handle)) {
			debugfs_remove_recursive(dbg->de);
			list_del(&dbg->entry);
			kfree(dbg);
			break;
		}
	}
}
