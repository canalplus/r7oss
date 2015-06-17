/*
 * STMicroelectronics BDispII driver - linux debugfs support
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: André Draszik <andre.draszik@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "blitter_os.h"

#include "bdisp2/bdispII_debugfs.h"
#include "bdisp2/bdispII_aq.h"
#include "bdisp2/bdispII_nodes.h"
#include "bdisp2_os_linuxkernel.h"

#if !defined(KBUILD_SYSTEM_INFO)
#define KBUILD_SYSTEM_INFO "<unknown>"
#endif
#if !defined(KBUILD_USER)
#define KBUILD_USER "<unknown>"
#endif
#if !defined(KBUILD_SOURCE)
#define KBUILD_SOURCE "<unknown>"
#endif
#if !defined(KBUILD_VERSION)
#define KBUILD_VERSION "<unknown>"
#endif
#if !defined(KBUILD_DATE)
#define KBUILD_DATE "<unknown>"
#endif

#define STR_SIZE 512

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
struct debugfs_reg32 {
	char *name;
	unsigned long offset;
};
#endif

#if !defined(CONFIG_DEBUG_FS) \
	|| (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
static int
debugfs_print_regs32(struct seq_file *s, const struct debugfs_reg32 *regs,
			   int nregs, void __iomem *base, char *prefix)
{
	int i, ret = 0;

	for (i = 0; i < nregs; i++, regs++) {
		if (prefix)
			ret += seq_printf(s, "%s", prefix);
		ret += seq_printf(s, "%s = 0x%08x\n", regs->name,
				  readl(base + regs->offset));
	}
	return ret;
}
#endif

static int
stm_bdisp2_debugfs_aq_regs_show(struct seq_file *s,
				void            *v)
{
	const struct stm_bdisp2_aq * const aq = s->private;

	static const struct debugfs_reg32 regs[] = {
		{ .name = "AQ_CTL", .offset = BDISP_AQ_CTL},
		{ .name = "AQ_IP ", .offset = BDISP_AQ_IP},
		{ .name = "AQ_LNA", .offset = BDISP_AQ_LNA},
		{ .name = "AQ_STA", .offset = BDISP_AQ_STA},
	};

	debugfs_print_regs32(s,
			     regs, ARRAY_SIZE(regs),
			     ((void __iomem *) aq->stdrv.mmio_base + 0xa00
			      + (BDISP_AQ1_BASE
				 + (aq->stdev.queue_id * 0x10))),
			     NULL);

	return 0;
}

static int
stm_bdisp2_aq_regs_open(struct inode *inode,
			struct file  *file)
{
	return single_open(file, stm_bdisp2_debugfs_aq_regs_show,
			   inode->i_private);
}

static const struct file_operations stm_bdisp2_debugfs_aq_regs_fops = {
	.open    = stm_bdisp2_aq_regs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


static int
stm_bdisp2_debugfs_regs_show(struct seq_file *s,
			     void            *v)
{
	const struct stm_bdisp2_aq * const aq = s->private;
	char *str;

	static const struct debugfs_reg32 regs[] = {
		{ .name = "STA", .offset = BDISP_STA},
	};

	debugfs_print_regs32(s,
			     regs, ARRAY_SIZE(regs),
			     (void __iomem *) aq->stdrv.mmio_base + 0xa00,
			     NULL);

	str = bdisp2_sprint_node(
			(const struct _BltNodeGroup00 *) (aq->stdrv.mmio_base
							  + 0xc00),
			true);
	if (str) {
		seq_printf(s, "%s\n", str);
		stm_blitter_free_mem(str);
	}

	return 0;
}

static int
stm_bdisp2_debugfs_info_show(struct seq_file *s,
			     void            *v)
{
	char *buf;
	int sz = 0;

	buf = stm_blitter_allocate_mem(STR_SIZE);
	if (!buf)
		return 0;

	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "Build Source:  " KBUILD_SOURCE "\n");
	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "Build Version: " KBUILD_VERSION "\n");
	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "Build User:    " KBUILD_USER "\n");
	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "Build Date:    " KBUILD_DATE "\n");
	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "Build System:  " KBUILD_SYSTEM_INFO "\n");

	if (buf) {
		seq_printf(s, "%s\n", buf);
		stm_blitter_free_mem(buf);
	}

	return 0;
}

static int
stm_bdisp2_debugfs_stats_show(struct seq_file *s,
			     void            *v)
{
	struct stm_bdisp2_aq * const aq = s->private;
	char *buf;
	int sz = 0;

	struct stm_bdisp2_driver_data * const stdrv = &aq->stdrv;
	unsigned long long ops;

	buf = stm_blitter_allocate_mem(STR_SIZE);
	if (!buf)
		return 0;

	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "starts:                      %u\n"
		       "interrupts (total/node/lna): %u (%u/%u)\n"
		       "wait_idle:                   %u\n"
		       "wait_next:                   %u\n"
		       "idle:                        %u\n",
		       stdrv->bdisp_shared->num_starts,
		       stdrv->bdisp_shared->num_irqs,
		       stdrv->bdisp_shared->num_node_irqs,
		       stdrv->bdisp_shared->num_lna_irqs,
		       stdrv->bdisp_shared->num_wait_idle,
		       stdrv->bdisp_shared->num_wait_next,
		       stdrv->bdisp_shared->num_idle);

	ops = ((unsigned long long) stdrv->bdisp_shared->num_ops_hi) << 32;
	ops += stdrv->bdisp_shared->num_ops_lo;

	sz += snprintf(&buf[sz], STR_SIZE - sz,
		       "\n"
		       "ops:             %llu\n"
		       "ops per start:   %llu\n"
		       "ops per idle:    %llu\n"
		       "starts per idle: %llu\n",
		       ops,
		       (stdrv->bdisp_shared->num_starts
			? bdisp2_div64_u64(ops, stdrv->bdisp_shared->num_starts)
			: 0),
		       (stdrv->bdisp_shared->num_idle
			? bdisp2_div64_u64(ops, stdrv->bdisp_shared->num_idle)
			: 0),
		       (stdrv->bdisp_shared->num_idle
			? bdisp2_div64_u64(stdrv->bdisp_shared->num_starts,
					   stdrv->bdisp_shared->num_idle)
			: 0));

	if (buf) {
		seq_printf(s, "%s\n", buf);
		stm_blitter_free_mem(buf);
	}

	return 0;
}

static int
stm_bdisp2_debugfs_busy_show(struct seq_file *s,
			     void            *v)
{
	struct stm_bdisp2_aq * const aq = s->private;

	void *dev_data = aq;

	(void) dev_data;

	return 0;
}

static int
stm_bdisp2_regs_open(struct inode *inode,
		     struct file  *file)
{
	return single_open(file, stm_bdisp2_debugfs_regs_show,
			   inode->i_private);
}

static int
stm_bdisp2_info_open(struct inode *inode,
		     struct file  *file)
{
	return single_open(file, stm_bdisp2_debugfs_info_show,
			   inode->i_private);
}

static int
stm_bdisp2_stats_open(struct inode *inode,
		     struct file  *file)
{
	return single_open(file, stm_bdisp2_debugfs_stats_show,
			   inode->i_private);
}

static int
stm_bdisp2_busy_open(struct inode *inode,
		     struct file  *file)
{
	return single_open(file, stm_bdisp2_debugfs_busy_show,
			   inode->i_private);
}

static const struct file_operations stm_bdisp2_debugfs_regs_fops = {
	.open    = stm_bdisp2_regs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations stm_bdisp2_debugfs_info_fops = {
	.open    = stm_bdisp2_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations stm_bdisp2_debugfs_stats_fops = {
	.open    = stm_bdisp2_stats_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations stm_bdisp2_debugfs_busy_fops = {
	.open    = stm_bdisp2_busy_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void
stm_bdisp2_aq_add_debugfs(struct stm_bdisp2_aq *aq,
			  struct dentry        *root)
{
	char name[10];

	if (!root)
		return;

	/* create a directory for this AQ to put everything in */
	snprintf(name, sizeof(name), "AQ%u", aq->stdev.queue_id);
	root = debugfs_create_dir(name, root);
	if (IS_ERR(root))
		return;

	debugfs_create_u32("queue_id", S_IRUGO,
			   root,
			   &aq->stdev.queue_id);
	debugfs_create_u32("usable_nodes_size", S_IRUGO,
			   root,
			   &aq->stdev.usable_nodes_size);
	debugfs_create_u32("node_irq_delay", S_IRUGO | S_IWUSR,
			   root,
			   &aq->stdev.node_irq_delay);

	debugfs_create_bool("force_slow_path", S_IRUGO | S_IWUSR,
			    root,
			    &aq->stdev.force_slow_path);

	debugfs_create_bool("no_blend_optimisation", S_IRUGO | S_IWUSR,
			    root,
			    &aq->stdev.no_blend_optimisation);

	debugfs_create_file("registers", S_IRUGO,
			    root, aq, &stm_bdisp2_debugfs_aq_regs_fops);

	/* TODO: add more stuff here */
}

void
stm_bdisp2_add_debugfs(const struct device *sbcd_device,
		       struct dentry       *root)
{
	struct stm_bdisp2_aq * const aq = dev_get_drvdata(sbcd_device);

	debugfs_create_file("registers", S_IRUGO,
			    root, aq, &stm_bdisp2_debugfs_regs_fops);

	debugfs_create_file("info", S_IRUGO,
			    root, aq, &stm_bdisp2_debugfs_info_fops);

	debugfs_create_file("busy", S_IRUGO,
			    root, aq, &stm_bdisp2_debugfs_busy_fops);

	debugfs_create_file("stats", S_IRUGO,
			    root, aq, &stm_bdisp2_debugfs_stats_fops);

	stm_bdisp2_aq_add_debugfs(aq, root);
}
