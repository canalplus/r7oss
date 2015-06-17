/*******************************************************************************
  Copyright (C) 2012  STMicroelectronics Ltd

  This is the header file to describe the Downstream and Upstream Interfaces.

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
#ifndef __STMICRO_ISVE_HW__
#define __STMICRO_ISVE_HW__

#define DFWD_REMOVE_DOCSIS_HDR	0x1
#define DFWD_REMOVE_SPECIAL_HDR	0x2

enum interrupt_packet_status {
	no_packet = 0,
	in_packet = 0x1,
	in_packet_dropped = 0x2,
	out_packet = 0x4,
	out_packet_dropped = 0x8,
};

struct dfwd_fifo {
	u32 used;
	u32 free;
};

/* Extra stats contains specific HW fields. */
struct isve_extra_stats {
	/* Generic Counters */
	unsigned long tx_pkt_n;
	unsigned long rx_pkt_n;
	/* Downstream queue counters */
	unsigned long dfwd_ovf_drop_cnt;
	unsigned long dfwd_panic_drop_cnt;
	/* dowstream interrupt status */
	unsigned long dfwd_int_drop_pkt_int;
	unsigned long dfwd_int_free_fifo;
	unsigned long dfwd_int_free_fifo_full;
	unsigned long dfwd_int_free_fifo_empty;
	unsigned long dfwd_int_fifo;
	unsigned long dfwd_int_fifo_full;
	unsigned long dfwd_int_fifo_not_empty;
	/* upiim interrupt status */
	unsigned long upiim_inq_int_fill_fifo;
	unsigned long upiim_inq_int_fill_full_fifo;
	unsigned long upiim_inq_int_fill_empty_fifo;
	unsigned long upiim_inq_int_free_fifo;
	unsigned long upiim_inq_int_free_full_fifo;
	unsigned long upiim_inq_int_free_empty_fifo;
};

/* Dowstream and Upstream modules share the same callback because
 * almost suitable to cover both needed. Just some call could be
 * dedicated to a specific module.
 */
struct isve_hw_ops {
	/* UIIPM / DFWD init callbacks */
	void (*dfwd_init) (void __iomem *ioaddr, unsigned int remove_header);
	void (*upiim_init) (void __iomem *ioaddr);
	/* Check dfwd fifo empty status */
	int (*dfwd_fifo_empty) (void __iomem *ioaddr);

	/* Dump MAC registers */
	void (*dump_regs) (void __iomem *ioaddr);
	/* Enable/Disable interrupt */
	void (*enable_irq) (void __iomem *ioaddr, int enable);
	/* Interrupt hook */
	int (*isr) (void __iomem *ioaddr, struct isve_extra_stats *x);
	/* Get stats */
	void (*get_stats) (void __iomem *ioaddr, struct isve_extra_stats *x);
	/* Manage tx entries in the input FIFO */
	void (*fill_tx_add) (void __iomem *ioaddr, int add, int len);
	u32 (*freed_tx_add) (void __iomem *ioaddr);
	/* Manage RX resources */
	void (*init_rx_fifo) (void __iomem *ioaddr, unsigned int buffer);
	u32 (*get_rx_used_add) (void __iomem *ioaddr);
	int (*get_rx_len) (void __iomem *ioaddr);
	struct dfwd_fifo (*get_rx_fifo_status) (void __iomem *ioaddr);
};

struct isve_hw_ops *isve_upiim_core(void __iomem *ioaddr);
struct isve_hw_ops *isve_dfwd_core(void __iomem *ioaddr);
#endif
