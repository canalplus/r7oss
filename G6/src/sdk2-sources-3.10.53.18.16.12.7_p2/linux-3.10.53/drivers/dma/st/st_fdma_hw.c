/*
 * STMicroelectronics FDMA dmaengine driver low-level functions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This code borrows heavily from drivers/stm/fdma.c!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "st_fdma.h"


void st_fdma_hw_enable(struct st_fdma_device *fdev)
{
	unsigned long irqflags = 0;
	int i;

	spin_lock_irqsave(&fdev->lock, irqflags);

	/*
	 * This sequence comes from the FDMA Fuctional Specification.
	 */

	/* Disable SLIM core STBus sync */
	writel(1, fdev->io_base + fdev->regs.sync_reg);
	/* Reset cpu pipeline keeping cpu pipeline clock disabled */
	writel(5, fdev->io_base + fdev->regs.clk_gate);
	/* Enable cpu pipeline clock */
	writel(0, fdev->io_base + fdev->regs.clk_gate);
	/* Enable cpu */
	writel(1, fdev->io_base + fdev->regs.en);

	/*
	 * Clear certain registers
	 */

	/* Clear the command mailbox */
	writel(0xffffffff, fdev->io_base + fdev->regs.int_clr);
	/* Clear the interrupt mailbox */
	writel(0xffffffff, fdev->io_base + fdev->regs.cmd_clr);
	/* Clear the command status registers */
	for (i = ST_FDMA_MIN_CHANNEL; i <= ST_FDMA_MAX_CHANNEL; ++i)
		writel(0, CMD_STAT_REG(&fdev->ch_list[i]));

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_disable(struct st_fdma_device *fdev)
{
	unsigned long irqflags = 0;

	/*
	 * This sequence comes from the FDMA Fuctional Specification.
	 */

	spin_lock_irqsave(&fdev->lock, irqflags);

	/* Disable the cpu pipeline clock */
	writel(1, fdev->io_base + fdev->regs.clk_gate);
	/* Disable the cpu */
	writel(0, fdev->io_base + fdev->regs.en);

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_get_revisions(struct st_fdma_device *fdev,
		 int *hw_major, int *hw_minor, int *fw_major, int *fw_minor)
{
	unsigned long irqflags = 0;
	int reg;

	spin_lock_irqsave(&fdev->lock, irqflags);

	/* Read the hardware version */
	if (hw_major)
		*hw_major = readl(fdev->io_base + fdev->regs.id);
	if (hw_minor)
		*hw_minor = readl(fdev->io_base + fdev->regs.ver);

	/* Read the firmware version */
	reg = readl(fdev->io_base + fdev->regs.rev_id);
	if (fw_major)
		*fw_major = (reg & REV_ID_MAJOR_MASK) >> REV_ID_MAJOR_SHIFT;
	if (fw_minor)
		*fw_minor = (reg & REV_ID_MINOR_MASK) >> REV_ID_MINOR_SHIFT;

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_channel_reset(struct st_fdma_chan *fchan)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdev->lock, irqflags);

	writel(0, CMD_STAT_REG(fchan));

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

int st_fdma_hw_channel_enable_all(struct st_fdma_device *fdev)
{
	unsigned long irqflags = 0;
	int result;

	spin_lock_irqsave(&fdev->lock, irqflags);

	writel(0xffffffff, fdev->io_base + fdev->regs.int_mask);
	writel(0xffffffff, fdev->io_base + fdev->regs.cmd_mask);
	writel(1, fdev->io_base + fdev->regs.en);
	result = readl(fdev->io_base + fdev->regs.en) & 1;

	spin_unlock_irqrestore(&fdev->lock, irqflags);

	return result;
}

int st_fdma_hw_channel_disable_all(struct st_fdma_device *fdev)
{
	unsigned long irqflags = 0;
	int result;

	spin_lock_irqsave(&fdev->lock, irqflags);

	writel(0, fdev->io_base + fdev->regs.int_mask);
	writel(0, fdev->io_base + fdev->regs.cmd_mask);
	writel(0, fdev->io_base + fdev->regs.en);
	result = readl(fdev->io_base + fdev->regs.en) & ~1;

	spin_unlock_irqrestore(&fdev->lock, irqflags);

	return result;
}

int st_fdma_hw_channel_set_dreq(struct st_fdma_chan *fchan,
		struct st_dma_dreq_config *config)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;
	u32 value;

	/* Configure hold off value */
	if (config->hold_off & ~REQ_CONTROL_HOLD_OFF_MASK) {
		dev_err(fdev->dev, "Invalid hold off!\n");
		return -EINVAL;
	}
	value = config->hold_off << REQ_CONTROL_HOLD_OFF_SHIFT;

	/* Configure opcode */
	switch (config->buswidth) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		value |= REQ_CONTROL_OPCODE_LD_ST1;
		break;
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		value |= REQ_CONTROL_OPCODE_LD_ST2;
		break;
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		value |= REQ_CONTROL_OPCODE_LD_ST4;
		break;
	case DMA_SLAVE_BUSWIDTH_8_BYTES:
		value |= REQ_CONTROL_OPCODE_LD_ST8;
		break;
	default:
		dev_err(fdev->dev, "Invalid bus width!\n");
		return -EINVAL;
	}

	/* Configure read or write */
	switch (config->direction) {
	case DMA_DEV_TO_MEM:
		value |= REQ_CONTROL_WNR_READ;
		break;
	case DMA_MEM_TO_DEV:
		value |= REQ_CONTROL_WNR_WRITE;
		break;
	default:
		dev_err(fdev->dev, "Invalid data direction!\n");
		return -EINVAL;
	}

	/* Configure data swap (only used by mchi) */
	if (config->data_swap)
		value |= REQ_CONTROL_DATA_SWAP_ENB;
	else
		value |= REQ_CONTROL_DATA_SWAP_DIS;

	/* Configure increment */
	if (config->increment)
		value |= REQ_CONTROL_INCREMENT_ON;
	else
		value |= REQ_CONTROL_INCREMENT_OFF;

	/* Configure initiator type */
	switch (config->initiator) {
	case 0:
		value |= REQ_CONTROL_INITIATOR_0;
		break;
	case 1:
		value |= REQ_CONTROL_INITIATOR_1;
		break;
	default:
		dev_err(fdev->dev, "Invalid initiator!\n");
		return -EINVAL;
	}

	/* Configure number of ops (0=1 transfer, 1=2 transfers...) */
	if (config->maxburst & ~REQ_CONTROL_NUM_OPS_MASK) {
		dev_err(fdev->dev, "Invalid number of ops!\n");
		return -EINVAL;
	}
	value |= (config->maxburst - 1) << REQ_CONTROL_NUM_OPS_SHIFT;

	spin_lock_irqsave(&fdev->lock, irqflags);

	writel(value, REQ_CONTROLn_REG(fchan->fdev, config->request_line));

	spin_unlock_irqrestore(&fdev->lock, irqflags);

	return 0;
}

void st_fdma_hw_channel_start(struct st_fdma_chan *fchan,
		struct st_fdma_desc *fdesc)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fdev->lock, irqflags);

	/* Set this to ensure a valid residue when channel not yet started */
	writel(fdesc->llu->nbytes, NODE_COUNT_REG(fchan));

	/* Write physical node address and start command to FDMA */
	writel(fdesc->dma_desc.phys | CMD_STAT_CMD_START, CMD_STAT_REG(fchan));
	writel(MBOX_CMD_START << (fchan->id * 2), MBOX_CMD_REG(fchan));

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_channel_pause(struct st_fdma_chan *fchan, int flush)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;
	u32 cmd;

	spin_lock_irqsave(&fdev->lock, irqflags);

	cmd = flush ? MBOX_CMD_FLUSH : MBOX_CMD_PAUSE;
	writel(cmd << (fchan->id * 2), MBOX_CMD_REG(fchan));

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_channel_resume(struct st_fdma_chan *fchan)
{
	struct st_fdma_device *fdev = fchan->fdev;
	unsigned long irqflags = 0;
	u32 value;

	spin_lock_irqsave(&fdev->lock, irqflags);

	value = readl(CMD_STAT_REG(fchan));
	value &= ~CMD_STAT_CMD_MASK;
	value |= CMD_STAT_CMD_RESTART;
	writel(value, CMD_STAT_REG(fchan));
	writel(MBOX_CMD_START << (fchan->id * 2), MBOX_CMD_REG(fchan));

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

void st_fdma_hw_channel_switch(struct st_fdma_chan *fchan,
		struct st_fdma_desc *fdesc, struct st_fdma_desc *tdesc,
		int ioc)
{
	struct st_fdma_device *fdev = fchan->fdev;
	struct st_fdma_desc *child;
	unsigned long irqflags = 0;
	unsigned long status;

	spin_lock_irqsave(&fdev->lock, irqflags);

	/* Set next pointer and ioc bit */
	fdesc->llu->next = tdesc->dma_desc.phys;
	fdesc->llu->control |= (ioc ? NODE_CONTROL_COMP_IRQ : 0);

	/* Walk current descriptor nodes setting next pointer and ioc bit */
	list_for_each_entry(child, &fdesc->llu_list, node) {
		child->llu->next = tdesc->dma_desc.phys;
		child->llu->control |= (ioc ? NODE_CONTROL_COMP_IRQ : 0);
	}

	/* Loop until processing the node to switch to */
	do {
		status = readl(CMD_STAT_REG(fchan)) & CMD_STAT_DATA_MASK;
	} while (status != tdesc->dma_desc.phys);

	spin_unlock_irqrestore(&fdev->lock, irqflags);
}

int st_fdma_hw_channel_status(struct st_fdma_chan *fchan)
{
	return readl(CMD_STAT_REG(fchan)) & CMD_STAT_STATUS_MASK;
}

int st_fdma_hw_channel_error(struct st_fdma_chan *fchan)
{
	return readl(CMD_STAT_REG(fchan)) & CMD_STAT_ERROR_MASK;
}
