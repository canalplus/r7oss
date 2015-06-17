/*******************************************************************************
  This is the Downstream/Upstream callbacks

  Copyright(C) 2012 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#include <linux/slab.h>
#include <linux/io.h>
#include "regs.h"
#include "isve_hw.h"

/* ********************** Downstream functions ********************** */
static void dfwd_enable_int(void __iomem *ioaddr, int enable)
{
	unsigned int ctrl = readl_relaxed(ioaddr + DFWD_OUTQ_CTL);

	if (enable)
		ctrl &= ~DFWD_QUEUE_IRQ_MASK;
	else
		ctrl |= DFWD_QUEUE_IRQ_MASK;

	writel_relaxed(ctrl, ioaddr + DFWD_OUTQ_CTL);
}

static void dfwd_core_init(void __iomem *ioaddr, unsigned int remove_header)
{
	unsigned int ctrl = DFWD_OUTQ_CTL_SWAPOUT |
			    DFWD_OUTQ_CTL_FREE_FIFO_FULL |
			    DFWD_OUTQ_CTL_FIFO_THRESH;

	pr_info("%s: init Downstream module\n", __func__);

	/* Clear counters */
	writel_relaxed(0, ioaddr + DFWD_OVF_DROP_CNT);
	writel_relaxed(0, ioaddr + DFWD_PANIC_DROP_CNT);

	if (remove_header & DFWD_REMOVE_DOCSIS_HDR)
		ctrl |= DFWD_OUTQ_CTL_REMOVE_DOCSIS_HEADER;
	if (remove_header & DFWD_REMOVE_SPECIAL_HDR)
		ctrl |= DFWD_OUTQ_CTL_REMOVE_EXTRA_HEADER;

	writel_relaxed(ctrl, ioaddr + DFWD_OUTQ_CTL);

	dfwd_enable_int(ioaddr, 1);
}

static void dfwd_get_counters(void __iomem *ioaddr, struct isve_extra_stats *x)
{
	x->dfwd_ovf_drop_cnt = readl_relaxed(ioaddr + DFWD_OVF_DROP_CNT);
	x->dfwd_panic_drop_cnt = readl_relaxed(ioaddr + DFWD_PANIC_DROP_CNT);
}

static void dfwd_dump_regs(void __iomem *ioaddr)
{
	int i;
	pr_info("Forwarding Queue Registers (base addr = 0x%p)\n", ioaddr);

	for (i = 0; i < QUEUE_REGS; i++) {
		int offset = DFWD_OUTQ_CTL + (i * 4);
		pr_info("\tReg No. %d (offset 0x%x): 0x%08x\n", i,
			offset, readl_relaxed(ioaddr + offset));
	}
}

static int dfwd_fifo_empty_status(void __iomem *ioaddr)
{
	int ret = 0;
	u32 status = readl(ioaddr + DFWD_QINT_STAT);

	if ((status & 0x0F) == DFWD_INT_STAT_FREE_FIFO_EMPTY)
		ret = 1;

	return ret;
}

static int dfwd_interrupt(void __iomem *ioaddr, struct isve_extra_stats *x)
{
	int status;
	int ret = 0;

	status = readl(ioaddr + DFWD_QINT_STAT);

	if (status & DFWD_INT_STAT_DROP_PKT_INT) {
		x->dfwd_int_drop_pkt_int++;
		ret = out_packet_dropped;
	}
	if (status & DFWD_INT_STAT_FREE_FIFO)
		x->dfwd_int_free_fifo++;
	if (status & DFWD_INT_STAT_FREE_FIFO_FULL)
		x->dfwd_int_free_fifo_full++;
	if (status & DFWD_INT_STAT_FREE_FIFO_EMPTY)
		x->dfwd_int_free_fifo_empty++;

	if (status & DFWD_INT_STAT_FIFO)
		x->dfwd_int_fifo++;
	if (status & DFWD_INT_STAT_FIFO_FULL)
		x->dfwd_int_fifo_full++;
	if (likely(status & DFWD_INT_STAT_FIFO_NOT_EMPTY)) {
		x->dfwd_int_fifo_not_empty++;
		ret = out_packet;
	}

	writel(status, ioaddr + DFWD_QINT_STAT);

	return ret;
}

static void dfwd_init_rx_fifo(void __iomem *ioaddr, unsigned int buffer)
{
	writel_relaxed(buffer & DFWD_FREE_ADD_MASK, ioaddr + DFWD_FREE_ADD);
}

static int dfwd_get_rx_len(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DFWD_USED_LEN) & DFWD_USED_LEN_MASK;

	return value;
}

static u32 dfwd_get_rx_used_add(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DFWD_USED_ADD) & DFWD_USED_ADD_MASK;

	return value;
}

static struct dfwd_fifo dfwd_get_fifo_entries(void __iomem *ioaddr)
{
	struct dfwd_fifo p;
	u32 reg;

	reg = readl(ioaddr + DFWD_GET_ENTRY_CNT);

	p.used = ((reg & DFWD_FIFO_ENTRY_USEDFIFO_ENTRIES_MASK) >>
		  DFWD_FIFO_ENTRY_USEDFIFO_ENTRIES_LSB);

	p.free = ((reg & DFWD_FIFO_ENTRY_FREEFIFO_ENTRIES_MASK) >>
		  DFWD_FIFO_ENTRY_FREEFIFO_ENTRIES_LSB);
	return p;
}

/* ********************** Upstream functions ********************** */
static void upiim_enable_int(void __iomem *ioaddr, int enable)
{
	if (enable)
		writel_relaxed(UPIIM_QUEUE_IRQ_DEFAULT, ioaddr + UPIIM_INQ_CTL);
	else
		writel_relaxed(UPIIM_QUEUE_DISABLE, ioaddr + UPIIM_INQ_CTL);
}

static void upiim_core_init(void __iomem *ioaddr)
{
	pr_info("%s: init upstream module\n", __func__);

	upiim_enable_int(ioaddr, 1);
}

static void upiim_dump_regs(void __iomem *ioaddr)
{
	int i;

	pr_info("Upstream Queue Registers (base addr = 0x%p)\n", ioaddr);

	for (i = 0; i < QUEUE_REGS; i++) {
		int offset = UPIIM_INQ_CTL + (i * 4);
		pr_info("\tReg No. %d (offset 0x%x): 0x%08x\n", i,
			offset, readl_relaxed(ioaddr + offset));
	}
}

static int upiim_interrupt(void __iomem *ioaddr, struct isve_extra_stats *x)
{
	int status;
	int ret = no_packet;	/* By default there is no work */

	/* get queue interrupt reason */
	status = readl(ioaddr + UPIIM_INQ_INT_STAT);

	if (status & UPIIM_INQ_INT_STAT_FILL_FIFO)
		x->upiim_inq_int_fill_fifo++;
	if (status & UPIIM_INQ_INT_STAT_FILL_FULL_FIFO) {
		x->upiim_inq_int_fill_full_fifo++;
		ret = in_packet_dropped;	/* Stop feeding packets */
	}
	if (status & UPIIM_INQ_INT_STAT_FILL_EMPTY_FIFO)
		x->upiim_inq_int_fill_empty_fifo++;
	if (status & UPIIM_INQ_INT_STAT_FREE_FIFO) {
		x->upiim_inq_int_free_fifo++;
		ret = in_packet;
	}
	if (status & UPIIM_INQ_INT_STAT_FREE_FULL_FIFO) {
		x->upiim_inq_int_free_full_fifo++;
		ret = in_packet;
	}
	if (status & UPIIM_INQ_INT_STAT_FREE_EMPTY_FIFO) {
		x->upiim_inq_int_free_empty_fifo++;
		ret = in_packet;
	}

	/* Writing a one to the serviced interrupt bit will clear the
	 * interrupt.
	 */
	writel(status, ioaddr + UPIIM_INQ_INT_STAT);

	return ret;
}

static void upiim_fill_tx_add(void __iomem *ioaddr, int add, int len)
{
	writel(len & UPIIM_FILLED_LEN_MASK, ioaddr + UPIIM_FILLED_LEN);
	writel(add & UPIIM_FIFO_ADD_MASK, ioaddr + UPIIM_FILLED_ADD);

	/* Writing to the FIFO will not automatically update the status
	 * register. And the status register drives interrupt generation.
	 * We set all the filled FIFO status bits to make the hardware update
	 * the register, and possibly raise an interrupt.
	 */
	writel(UPIIM_INQ_INT_STAT_FILL_FIFO |
	       UPIIM_INQ_INT_STAT_FILL_FULL_FIFO |
	       UPIIM_INQ_INT_STAT_FILL_EMPTY_FIFO, ioaddr + UPIIM_INQ_INT_STAT);
}

static u32 upiim_freed_tx_add(void __iomem *ioaddr)
{
	u32 status;
	u32 value = 0;		/* FIFO empty */

	/* The address in the FIFO is only valid if the not-empty bit is set
	 * in the status register.
	 */
	status = readl(ioaddr + UPIIM_INQ_INT_STAT);
	if (status & UPIIM_INQ_INT_STAT_FREE_EMPTY_FIFO) {
		value = readl(ioaddr + UPIIM_FREED_ADD) & UPIIM_FIFO_ADD_MASK;

		/* Reading the FIFO will not automatically update the status
		 * register. And the status register drives interrupt
		 * generation.
		 * We set all the freed FIFO status bits to make the hardware
		 * update the register, and maybe lower the interrupt.
		 */
		writel(UPIIM_INQ_INT_STAT_FREE_EMPTY_FIFO |
		       UPIIM_INQ_INT_STAT_FREE_FULL_FIFO |
		       UPIIM_INQ_INT_STAT_FREE_FIFO,
		       ioaddr + UPIIM_INQ_INT_STAT);
	}

	return value;
}

static struct isve_hw_ops dfwd_ops = {
	.dfwd_init = dfwd_core_init,
	.dump_regs = dfwd_dump_regs,
	.isr = dfwd_interrupt,
	.dfwd_fifo_empty = dfwd_fifo_empty_status,
	.enable_irq = dfwd_enable_int,
	.get_stats = dfwd_get_counters,
	.init_rx_fifo = dfwd_init_rx_fifo,
	.get_rx_len = dfwd_get_rx_len,
	.get_rx_used_add = dfwd_get_rx_used_add,
	.get_rx_fifo_status = dfwd_get_fifo_entries,
};

static struct isve_hw_ops upiim_ops = {
	.upiim_init = upiim_core_init,
	.dump_regs = upiim_dump_regs,
	.isr = upiim_interrupt,
	.enable_irq = upiim_enable_int,
	.fill_tx_add = upiim_fill_tx_add,
	.freed_tx_add = upiim_freed_tx_add,
};

struct isve_hw_ops *isve_dfwd_core(void __iomem *ioaddr)
{
	return &dfwd_ops;
}

struct isve_hw_ops *isve_upiim_core(void __iomem *ioaddr)
{
	return &upiim_ops;
}
