/*
 * STMicroelectronics FDMA dmaengine driver debug functions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>

#include <linux/platform_data/dma-st-fdma.h>
#include "st_fdma.h"

/*
 * Data
 */

static struct dentry *st_fdma_debugfs_root;
static char *st_fdma_debugfs_fw_state[] = {
	"INIT", "LOADING", "LOADED", "ERROR"};
static char *st_fdma_debugfs_fchan_state[] = {
	"IDLE", "RUNNING", "STOPPING", "PAUSING", "PAUSED", "ERROR"};
static char *st_fdma_debugfs_direction[] = {
	"MEM_TO_MEM", "MEM_TO_DEV", "DEV_TO_MEM", "DEV_TO_DEV"};

/*
 * Macro
 */

#define SEQ_PRINTF(m, s, a) \
	seq_printf(m, "%-30s (0x%p) = 0x%08x\n", s, a, readl(a));


/*
 * Debugfs register file functions
 */

static int st_fdma_debugfs_regs_show(struct seq_file *m, void *v)
{
	struct st_fdma_device *fdev = m->private;
	int i;

	seq_printf(m, "--- %s (0x%p) (FW: %s) ---\n", dev_name(fdev->dev),
		   fdev->io_base, st_fdma_debugfs_fw_state[fdev->fw_state]);

	SEQ_PRINTF(m, "ID", fdev->io_base + fdev->regs.id);
	SEQ_PRINTF(m, "VER", fdev->io_base + fdev->regs.ver);
	SEQ_PRINTF(m, "EN", fdev->io_base + fdev->regs.en);
	SEQ_PRINTF(m, "CLK_GATE", fdev->io_base + fdev->regs.clk_gate);
	SEQ_PRINTF(m, "SLIM_PC", fdev->io_base + fdev->regs.slim_pc);
	SEQ_PRINTF(m, "STBUS_SYNC", fdev->io_base + fdev->regs.sync_reg);
	SEQ_PRINTF(m, "REV_ID", fdev->io_base + fdev->regs.rev_id);

	if (fdev->regs.mchi_rx_nb_cur)
		SEQ_PRINTF(m, "MCHI_RX_NB_CUR",
			fdev->io_base + fdev->regs.mchi_rx_nb_cur);

	if (fdev->regs.mchi_rx_nb_all)
		SEQ_PRINTF(m, "MCHI_RX_NB_ALL",
			fdev->io_base + fdev->regs.mchi_rx_nb_all);

	for (i = 1; i < 31; ++i) {
		char buffer[80];
		sprintf(buffer, "REQ_CONTROL[%d]", i);
		SEQ_PRINTF(m, buffer, REQ_CONTROLn_REG(fdev, i));
	}

	SEQ_PRINTF(m, "CMD_STA", fdev->io_base + fdev->regs.cmd_sta);
	SEQ_PRINTF(m, "CMD_SET", fdev->io_base + fdev->regs.cmd_set);
	SEQ_PRINTF(m, "CMD_CLR", fdev->io_base + fdev->regs.cmd_clr);
	SEQ_PRINTF(m, "CMD_MASK", fdev->io_base + fdev->regs.cmd_mask);

	SEQ_PRINTF(m, "INT_STA", fdev->io_base + fdev->regs.int_sta);
	SEQ_PRINTF(m, "INT_SET", fdev->io_base + fdev->regs.int_set);
	SEQ_PRINTF(m, "INT_CLR", fdev->io_base + fdev->regs.int_clr);
	SEQ_PRINTF(m, "INT_MASK", fdev->io_base + fdev->regs.int_mask);

	return 0;
}

static int st_fdma_debugfs_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, st_fdma_debugfs_regs_show, inode->i_private);
}

static const struct file_operations st_fdma_debugfs_regs_fops = {
	.open		= st_fdma_debugfs_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 * Debugfs register dmem functions
 */

static int st_fdma_debugfs_dmem_show(struct seq_file *m, void *v)
{
	struct st_fdma_device *fdev = m->private;
	struct st_plat_fdma_ram *dmem = &fdev->hw->dmem;
	int i, j;

	seq_printf(m, "--- %s (0x%p) DMEM ---\n", dev_name(fdev->dev),
			fdev->io_base);

	for (i = 0; i < dmem->size; ) {
		seq_printf(m, "%p: ", fdev->io_base + dmem->offset + i);

		for (j = 0; (j < 16) && (i < dmem->size); ++j, ++i)
			seq_printf(m, "%02x ",
				readb(fdev->io_base + dmem->offset + i));

		seq_printf(m, "\n");
	}

	seq_printf(m, "\n");

	return 0;
}

static int st_fdma_debugfs_dmem_open(struct inode *inode, struct file *file)
{
	return single_open(file, st_fdma_debugfs_dmem_show, inode->i_private);
}

static const struct file_operations st_fdma_debugfs_dmem_fops = {
	.open		= st_fdma_debugfs_dmem_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 * Debugfs channel file functions
 */

static int st_fdma_debugfs_chan_show(struct seq_file *m, void *v)
{
	struct st_fdma_chan *fchan = m->private;
	char buffer[80];
	int i;

	seq_printf(m, "--- %s (0x%p) channel %d (%s) ---\n",
			dev_name(fchan->fdev->dev), fchan->fdev->io_base,
			fchan->id, st_fdma_debugfs_fchan_state[fchan->state]);

	SEQ_PRINTF(m, "CMD_STAT", CMD_STAT_REG(fchan));
	SEQ_PRINTF(m, "PTR", NODE_PTR_REG(fchan));
	SEQ_PRINTF(m, "CTRL", NODE_CTRL_REG(fchan));
	SEQ_PRINTF(m, "COUNT", NODE_COUNT_REG(fchan));
	SEQ_PRINTF(m, "SADDR", NODE_SADDR_REG(fchan));
	SEQ_PRINTF(m, "DADDR", NODE_DADDR_REG(fchan));

	switch (fchan->type) {
	case ST_DMA_TYPE_TELSS:
		SEQ_PRINTF(m, "NPARAM", NODE_TELSS_NODE_PARAM_REG(fchan));
		for (i = 0; i < ST_FDMA_LLU_TELSS_HANDSETS; ++i) {
			sprintf(buffer, "HPARAM[%d]", i);
			SEQ_PRINTF(m, buffer,
				NODE_TELSS_HANDSET_PARAMn_REG(fchan, i));
		}
		break;

	case ST_DMA_TYPE_MCHI:
		SEQ_PRINTF(m, "MCHI_LENGTH", NODE_MCHI_LENGTH_REG(fchan));
		SEQ_PRINTF(m, "MCHI_RX_FIFO_THR_ADDR",
				NODE_MCHI_RX_FIFO_THR_ADDR_REG(fchan));
		SEQ_PRINTF(m, "MCHI_DSTRIDE", NODE_MCHI_DSTRIDE_REG(fchan));
		break;

	default:
		/* No additional registers to print */
		break;
	}

	if (fchan->dreq) {
		sprintf(buffer, "REQ_CONTROL[%d]", fchan->dreq->request_line);
		SEQ_PRINTF(m, buffer,
			REQ_CONTROLn_REG(fchan->fdev,
				fchan->dreq->request_line));
		sprintf(buffer, "TRANSFER DIRECTION");
		seq_printf(m, "%-30s              = %s\n", buffer,
			st_fdma_debugfs_direction[fchan->dreq->direction]);
	}

	seq_printf(m, "\n");

	return 0;
}

static int st_fdma_debugfs_chan_open(struct inode *inode, struct file *file)
{
	return single_open(file, st_fdma_debugfs_chan_show, inode->i_private);
}

static const struct file_operations st_fdma_debugfs_chan_fops = {
	.open		= st_fdma_debugfs_chan_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 * Device register/unregister and initialise/shutdown
 */

void st_fdma_debugfs_register(struct st_fdma_device *fdev)
{
	int c;
	char name[16];

	/* Create a debugfs directory for this device  */
	fdev->debug_dir = debugfs_create_dir(dev_name(&fdev->pdev->dev),
					     st_fdma_debugfs_root);
	if (!fdev->debug_dir)
		goto error_create_dir;

	/* Create entry for fdma registers */
	fdev->debug_regs = debugfs_create_file("registers", S_IRUGO,
					fdev->debug_dir, fdev,
					&st_fdma_debugfs_regs_fops);
	if (!fdev->debug_regs)
		goto error_regs_file;

	/* Create entry for fdma dmem */
	fdev->debug_dmem = debugfs_create_file("dmem", S_IRUGO,
					fdev->debug_dir, fdev,
					&st_fdma_debugfs_dmem_fops);
	if (!fdev->debug_dmem)
		goto error_dmem_file;

	/* Create a debugfs entry for each channel */
	for (c = ST_FDMA_MIN_CHANNEL; c <= ST_FDMA_MAX_CHANNEL; c++) {
		snprintf(name, sizeof(name), "channel%d", c);

		fdev->debug_chans[c] = debugfs_create_file(name, S_IRUGO,
				fdev->debug_dir, &fdev->ch_list[c],
				&st_fdma_debugfs_chan_fops);
		if (fdev->debug_chans[c] == NULL)
			goto error_chan_file;
	}

	return;

error_chan_file:
	/* Remove the channel file entries */
	for (c = ST_FDMA_MIN_CHANNEL; c <= ST_FDMA_MAX_CHANNEL; c++) {
		if (fdev->debug_chans[c]) {
			debugfs_remove(fdev->debug_chans[c]);
			fdev->debug_chans[c] = NULL;
		}
	}
	debugfs_remove(fdev->debug_dmem);
	fdev->debug_dmem = NULL;
error_dmem_file:
	/* Remove the registers file entry */
	debugfs_remove(fdev->debug_regs);
	fdev->debug_regs = NULL;
error_regs_file:
	/* Remove the directory entry */
	debugfs_remove(fdev->debug_dir);
	fdev->debug_dir = NULL;
error_create_dir:
	return;
}

void st_fdma_debugfs_unregister(struct st_fdma_device *fdev)
{
	int c;

	/* Remove the debugfs entry for each channel */
	for (c = ST_FDMA_MIN_CHANNEL; c <= ST_FDMA_MAX_CHANNEL; c++)
		debugfs_remove(fdev->debug_chans[c]);

	debugfs_remove(fdev->debug_dmem);
	debugfs_remove(fdev->debug_regs);
	debugfs_remove(fdev->debug_dir);
}


void st_fdma_debugfs_init(void)
{
	/* Create the root directory in debugfs */
	st_fdma_debugfs_root = debugfs_create_dir("fdma", NULL);
}

void st_fdma_debugfs_exit(void)
{
	debugfs_remove(st_fdma_debugfs_root);
}
