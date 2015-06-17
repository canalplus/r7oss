/*
 * STMicroelectronics FDMA dmaengine driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ST_FDMA_REGS_H__
#define __ST_FDMA_REGS_H__


/*
 * FDMA register offsets structure
 */

struct st_fdma_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
	unsigned long slim_pc;
	unsigned long rev_id;
	unsigned long mchi_rx_nb_cur;
	unsigned long mchi_rx_nb_all;
	unsigned long cmd_statn;
	unsigned long ptrn;
	unsigned long ctrln;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
	unsigned long node_size;
	unsigned long req_ctln;
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};


#define LEGACY_NODE_DATA_SIZE		0x40


/*
 * FDMA revision id register
 */

#define REV_ID_MAJOR_MASK		0x00ff0000
#define REV_ID_MAJOR_SHIFT		16
#define REV_ID_MINOR_MASK		0x0000ff00
#define REV_ID_MINOR_SHIFT		8


/*
 * FDMA command and status register
 */

#define CMD_STAT_OFFSET			0x04

#define CMD_STAT_CMD_MASK		0x0000001f
#define CMD_STAT_CMD_RESTART		0x00000000
#define CMD_STAT_CMD_START		0x00000001

#define CMD_STAT_STATUS_MASK		0x00000003
#define CMD_STAT_STATUS_IDLE		0x00000000
#define CMD_STAT_STATUS_RUNNING		0x00000002
#define CMD_STAT_STATUS_PAUSED		0x00000003

#define CMD_STAT_ERROR_MASK		0x0000001c
#define CMD_STAT_ERROR_INTR		0x00000000
#define CMD_STAT_ERROR_NAND		0x00000004
#define CMD_STAT_ERROR_MCHI		0x00000008

#define CMD_STAT_DATA_MASK		0xffffffe0

#define CMD_STAT_REG(_fchan) \
	((_fchan)->fdev->io_base + (_fchan)->fdev->regs.cmd_statn + \
		((_fchan)->id * CMD_STAT_OFFSET))


/*
 * FDMA request control register
 */

#define REQ_CONTROL_OFFSET		0x04

#define REQ_CONTROL_HOLD_OFF_MASK	0x00000007
#define REQ_CONTROL_HOLD_OFF_SHIFT	0

#define REQ_CONTROL_OPCODE_MASK		0x000000f0
#define REQ_CONTROL_OPCODE_LD_ST1	0x00000000
#define REQ_CONTROL_OPCODE_LD_ST2	0x00000010
#define REQ_CONTROL_OPCODE_LD_ST4	0x00000020
#define REQ_CONTROL_OPCODE_LD_ST8	0x00000030
#define REQ_CONTROL_OPCODE_LD_ST16	0x00000040
#define REQ_CONTROL_OPCODE_LD_ST32	0x00000050
#define REQ_CONTROL_OPCODE_LD_ST64	0x00000060

#define REQ_CONTROL_WNR_MASK		0x00004000
#define REQ_CONTROL_WNR_READ		0x00000000
#define REQ_CONTROL_WNR_WRITE		0x00004000

#define REQ_CONTROL_DATA_SWAP_MASK	0x00020000
#define REQ_CONTROL_DATA_SWAP_DIS	0x00000000
#define REQ_CONTROL_DATA_SWAP_ENB	0x00020000

#define REQ_CONTROL_INCREMENT_MASK	0x00200000
#define REQ_CONTROL_INCREMENT_OFF	0x00000000
#define REQ_CONTROL_INCREMENT_ON	0x00200000

#define REQ_CONTROL_INITIATOR_MASK	0x00c00000
#define REQ_CONTROL_INITIATOR_0		0x00000000
#define REQ_CONTROL_INITIATOR_1		0x00400000

#define REQ_CONTROL_NUM_OPS_MASK	0x000000ff
#define REQ_CONTROL_NUM_OPS_SHIFT	24

#define REQ_CONTROL_REG(_fchan) \
	((_fchan)->fdev->io_base + (_fchan)->fdev->regs.req_ctln + \
	((_fchan)->dreq->request_line * REQ_CONTROL_OFFSET))

#define REQ_CONTROLn_REG(_fdev, n) \
	((_fdev)->io_base + (_fdev)->regs.req_ctln + (n * REQ_CONTROL_OFFSET))


/*
 * FDMA node parameter registers
 */

#define NODE_PTR_REG(_fchan) \
	((_fchan)->fdev->io_base + \
	(((_fchan)->id * (_fchan)->fdev->regs.node_size) + \
	(_fchan)->fdev->regs.ptrn))

#define NODE_CTRL_REG(_fchan) \
	((_fchan)->fdev->io_base + \
	(((_fchan)->id * (_fchan)->fdev->regs.node_size) + \
	(_fchan)->fdev->regs.ctrln))

#define NODE_COUNT_REG(_fchan) \
	((_fchan)->fdev->io_base + \
	(((_fchan)->id * (_fchan)->fdev->regs.node_size) + \
	(_fchan)->fdev->regs.cntn))

#define NODE_SADDR_REG(_fchan) \
	((_fchan)->fdev->io_base + \
	(((_fchan)->id * (_fchan)->fdev->regs.node_size) + \
	(_fchan)->fdev->regs.saddrn))

#define NODE_DADDR_REG(_fchan) \
	((_fchan)->fdev->io_base + \
	(((_fchan)->id * (_fchan)->fdev->regs.node_size) + \
	(_fchan)->fdev->regs.daddrn))

/*
 * FDMA TELSS node parameter registers
 */
#define NODE_TELSS_NODE_PARAM_REG(_fchan) \
	(NODE_PTR_REG(_fchan) + 0x14)

#define NODE_TELSS_HANDSET_PARAMn_REG(_fchan, n) \
	(NODE_PTR_REG(_fchan) + 0x18 + (n * 4))

/*
 * FDMA MCHI node parameter registers
 */
#define NODE_MCHI_LENGTH_REG(_fchan) \
	(NODE_PTR_REG(_fchan) + 0x14)

#define NODE_MCHI_RX_FIFO_THR_ADDR_REG(_fchan) \
	(NODE_PTR_REG(_fchan) + 0x18)

#define NODE_MCHI_DSTRIDE_REG(_fchan) \
	(NODE_PTR_REG(_fchan) + 0x1c)

/*
 * FDMA mailbox command register
 */
#define MBOX_CMD_MASK			0x00000003
#define MBOX_CMD_START			0x00000001
#define MBOX_CMD_PAUSE			0x00000002
#define MBOX_CMD_FLUSH			0x00000003

#define MBOX_CMD_REG(_fchan) \
	((_fchan)->fdev->io_base + (_fchan)->fdev->regs.cmd_set)

#endif /* __ST_FDMA_REGS_H__ */
