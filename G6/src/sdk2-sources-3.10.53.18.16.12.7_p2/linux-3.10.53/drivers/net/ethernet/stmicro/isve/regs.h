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
#ifndef __STMICRO_ISVE_REGS__
#define __STMICRO_ISVE_REGS__
#define QUEUE_REGS	9

/***************************************************************************
 *				Forwarding Registers
 ***************************************************************************/
#define DFWD_OUTQ_CTL		0x0	/* Out queue control */
#define DFWD_QINT_STAT		0x4	/* Out queue interrupt status */
#define DFWD_FREE_ADD		0x8	/* Out queue free addr FIFO */
#define DFWD_USED_LEN		0xC	/* Out queue used length FIFO */
#define DFWD_USED_ADD		0x10	/* Out queue used addr FIFO */
#define DFWD_GET_ENTRY_CNT	0x14	/* To returns the number of entries in
					 * the FIFOs.
					 */
#define DFWD_OVF_DROP_CNT	0x18
#define DFWD_PANIC_DROP_CNT	0x1C

/* Queue Registers */
#define DFWD_OUTQ_CTL_SWAPOUT		(1<<23)
#define DFWD_OUTQ_CTL_NO_FIFO_POINTER	(1<<22)
#define DFWD_OUTQ_CTL_REMOVE_EXTRA_HEADER	(1<<21)
#define DFWD_OUTQ_CTL_REMOVE_DOCSIS_HEADER	(1<<20)
#define DFWD_OUTQ_CTL_DROP_PKT_INT_MASK	(1<<16)
#define DFWD_OUTQ_CTL_FREE_FIFO		(1<<15)	/* Free fifo almost empty */
#define DFWD_OUTQ_CTL_FREE_FIFO_FULL	(1<<14)
#define DFWD_OUTQ_CTL_FREE_FIFO_EMPTY	(1<<13)	/* Free fifo empty */
#define DFWD_OUTQ_CTL_FREE_FIFO_THRESH_MASK	0x1f
#define DFWD_OUTQ_CTL_FREE_FIFO_T_SHIFT	8
#define DFWD_OUTQ_CTL_FIFO		(1<<7)
#define DFWD_OUTQ_CTL_FIFO_FULL		(1<<6)
#define DFWD_OUTQ_CTL_FIFO_NOT_EMPTY	(1<<5)
#define DFWD_OUTQ_CTL_FIFO_THRESH_MASK	0x1f
#define DFWD_OUTQ_CTL_FIFO_THRESH	0x1D

/* 0: enabled, 1: disabled */
#define DFWD_QUEUE_IRQ_MASK	(DFWD_OUTQ_CTL_DROP_PKT_INT_MASK | \
				 DFWD_OUTQ_CTL_FREE_FIFO | \
				 DFWD_OUTQ_CTL_FREE_FIFO_EMPTY | \
				 DFWD_OUTQ_CTL_FIFO | \
				 DFWD_OUTQ_CTL_FIFO_FULL | \
				 DFWD_OUTQ_CTL_FIFO_NOT_EMPTY)

#define DFWD_INT_STAT_DROP_PKT_INT	(1<<6)
#define DFWD_INT_STAT_FREE_FIFO		(1<<5)
#define DFWD_INT_STAT_FREE_FIFO_FULL	(1<<4)
#define DFWD_INT_STAT_FREE_FIFO_EMPTY	(1<<3)
#define DFWD_INT_STAT_FIFO		(1<<2)
#define DFWD_INT_STAT_FIFO_FULL		(1<<1)
#define DFWD_INT_STAT_FIFO_NOT_EMPTY	(1<<0)

#define DFWD_FREE_ADD_MASK		0xffffffe0
#define DFWD_FREE_ADD_SHIFT		5

#define DFWD_USED_LEN_MASK		0xfff

#define DFWD_USED_ADD_MASK		0xffffffe0
#define DFWD_USED_ADD_SHIFT		5

/* Get free and used FIFO entries:
 *	[21:16]: Used entries
 *	[5:0]: Free entries
 */
#define	DFWD_FIFO_ENTRY_USEDFIFO_ENTRIES_MASK		0x003f0000
#define	DFWD_FIFO_ENTRY_USEDFIFO_ENTRIES_LSB		16
#define	DFWD_FIFO_ENTRY_FREEFIFO_ENTRIES_MASK		0x0000003f
#define	DFWD_FIFO_ENTRY_FREEFIFO_ENTRIES_LSB		0

/***************************************************************************
 *				UPIIM Registers
 ****************************************************************************/
#define UPIIM_INQ_CTL		0x0	/* Input queue control */
#define UPIIM_INQ_INT_STAT	0x4	/* Input queue interrupt status */
#define UPIIM_INQ_DEFAULT_FLAGS1	0x8	/* Input queue flags 1. */
#define UPIIM_INQ_DEFAULT_FLAGS2	0xC	/* Input queue flags 2. */
#define UPIIM_INQ_FLAGS1	0x10	/* Inqueue flags 1. */
#define UPIIM_INQ_FLAGS2	0x14	/* Input queue flags 2. */
#define UPIIM_FILLED_LEN	0x18	/* Input queue filled address FIFO */
#define UPIIM_FILLED_ADD	0x1C	/* Input queue filled length FIFO */
#define UPIIM_FREED_ADD		0x20	/* Input queue freed address FIFO */
#define UPIIM_INQ_FIFO_ENTRY	0x24

/* Input queue control
 * For the FIFO bits a 1 will mask the interrupt, and 0 will allow it.
 */
#define UPIIM_INQ_CTL_SWAPOUT			(1<<23)
#define UPIIM_INQ_CTL_NO_FIFO_POINTER		(1<<22)
#define UPIIM_INQ_CTL_NO_FIFO_POINTER		(1<<22)
#define UPIIM_INQ_CTL_FILL_FIFO			(1<<15)
#define UPIIM_INQ_CTL_FILL_FIFO_FULL		(1<<14)
#define UPIIM_INQ_CTL_FILL_FIFO_EMPTY		(1<<13)
#define UPIIM_INQ_CTL_FILL_EMPTY_THRES_MASK	0x1f
#define UPIIM_INQ_CTL_FILL_EMPTY_THRES_SHIFT	8
#define UPIIM_INQ_CTL_FREED_FIFO		(1<<7)
#define UPIIM_INQ_CTL_FREED_FIFO_FULL		(1<<6)
#define UPIIM_INQ_CTL_FREED_FIFO_EMPTY		(1<<5)
#define UPIIM_INQ_CTL_FREED_EMPTY_THRES_MASK	0x1f
#define UPIIM_INQ_CTL_FREED_EMPTY_THRES_SHIFT	0

#define	UPIIM_QUEUE_DISABLE	0xffffffff
/* For the filled FIFO only enable the full interrupt.
 * For the freed FIFO only enable the almost full interrupt, with a threshold
 * of 16 packets. If the freed FIFO is completely full the UPIIM will stop
 * processing the filled FIFO.
 */
#define UPIIM_QUEUE_IRQ_DEFAULT	(UPIIM_INQ_CTL_SWAPOUT | \
		UPIIM_INQ_CTL_FILL_FIFO | UPIIM_INQ_CTL_FILL_FIFO_EMPTY | \
		UPIIM_INQ_CTL_FREED_FIFO_FULL | \
		UPIIM_INQ_CTL_FREED_FIFO_EMPTY | \
		(0x1D << UPIIM_INQ_CTL_FILL_EMPTY_THRES_SHIFT) | \
		(0x10 << UPIIM_INQ_CTL_FREED_EMPTY_THRES_SHIFT))

/* Input queue stat */
#define UPIIM_INQ_INT_STAT_FILL_FIFO		(1<<5)
#define UPIIM_INQ_INT_STAT_FILL_FULL_FIFO	(1<<4)
#define UPIIM_INQ_INT_STAT_FILL_EMPTY_FIFO	(1<<3)
#define UPIIM_INQ_INT_STAT_FREE_FIFO		(1<<2)
#define UPIIM_INQ_INT_STAT_FREE_FULL_FIFO	(1<<1)
#define UPIIM_INQ_INT_STAT_FREE_EMPTY_FIFO	(1<<0)

/* Input queue filled length FIFO */
#define UPIIM_FILLED_LEN_MASK		0x7ff

/* Input queue filled/freed address FIFO */
#define UPIIM_FIFO_ADD_MASK		0xffffffe0
#define UPIIM_FIFO_ADD_SHIFT		5

#define UPIIM_INQ_DEFAULT_FLAGS1_INGRESS_SHIFT	8
#define UPIIM_INQ_DEFAULT_FLAGS1_EGRESS		(1<<16)
#define UPIIM_INQ_FLAGS1_DEFAULT (UPIIM_INQ_DEFAULT_FLAGS1_EGRESS |	\
				  7 << UPIIM_INQ_DEFAULT_FLAGS1_INGRESS_SHIFT)
#endif
