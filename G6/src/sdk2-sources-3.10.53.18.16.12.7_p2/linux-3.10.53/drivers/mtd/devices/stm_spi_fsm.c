/*
 * stm_spi_fsm.c	Support for STM SPI Serial Flash Controller
 *
 * Author: Angus Clark <angus.clark@st.com>
 *
 * Copyright (C) 2010-2011 STMicroelectronics Limited
 *
 * JEDEC probe based on drivers/mtd/devices/m25p80.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/sort.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>

#include <asm/div64.h>

#include "stm_spi_fsm.h"

#define NAME		"stm-spi-fsm"

#define FLASH_PROBE_FREQ	10		/* Probe freq. (MHz) */
#define FLASH_PAGESIZE		256

/* Maximum operation times (in ms) */
#define FLASH_MAX_CHIP_ERASE_MS	500000		/* Chip Erase time */
#define FLASH_MAX_SEC_ERASE_MS	30000		/* Sector Erase time */
#define FLASH_MAX_PAGE_WRITE_MS 100		/* Write Page time */
#define FLASH_MAX_STA_WRITE_MS	4000		/* Write status reg time */
#define FSM_MAX_WAIT_SEQ_MS	1000		/* FSM execution time */

/*
 * Flags to tweak operation of default read/write/erase/lock routines
 */
#define CFG_READ_TOGGLE32BITADDR		0x00000001
#define CFG_WRITE_TOGGLE32BITADDR		0x00000002
#define CFG_LOCK_TOGGLE32BITADDR		0x00000004
#define CFG_ERASESEC_TOGGLE32BITADDR		0x00000008
#define CFG_S25FL_CHECK_ERROR_FLAGS		0x00000010
#define CFG_N25Q_CHECK_ERROR_FLAGS		0x00000020
#define CFG_WRSR_FORCE_16BITS			0x00000040
#define CFG_RD_WR_LOCK_REG			0x00000080
#define CFG_MX25_LOCK_TOGGLE_QE_BIT		0x00000100

/* Block Protect Bits (BPx) */
#define BPX_MAX_BITS		4			/* Max BPx bits */
#define BPX_MAX_N_BNDS		(1 << BPX_MAX_BITS)	/* Max BPx boundaries */

struct stm_spifsm_caps {
	/* Board/SoC/IP capabilities */
	unsigned dual_mode:1;		/* DUAL mode */
	unsigned quad_mode:1;		/* QUAD mode */

	/* Board capabilities */
	unsigned reset_signal:1;	/* SoC reset routed to device reset */
	unsigned reset_por:1;		/* SoC reset forces device POR (e.g.
					 *   on-board logic/controller) */
	unsigned boot_from_spi:1;	/* Boot device is SPI */

	/* IP capabilities */
	unsigned addr_32bit:1;		/* 32bit addressing supported */
	unsigned no_poll_mode_change:1;	/* Polling MODE_CHANGE broken */
	unsigned no_clk_div_4:1;	/* Bug prevents ClK_DIV=4 */
	unsigned no_sw_reset:1;		/* S/W reset not possible */
	unsigned dummy_on_write:1;	/* Bug requires "dummy" sequence on
					 * WRITE */
	unsigned no_read_repeat:1;	/* READ repeat sequence broken */
	unsigned no_write_repeat:1;	/* WRITE repeat sequence broken */
	enum {
		spifsm_no_read_status = 1,	/* READ_STA broken */
		spifsm_read_status_clkdiv4,	/* READ_STA only at CLK_DIV=4 */
	} read_status_bug;
};

struct stm_plat_spifsm_data {
	char			*name;
	struct mtd_partition	*parts;
	unsigned int		nr_parts;
	unsigned int		max_freq;
	struct stm_spifsm_caps	capabilities;
};

/*
 * SPI FSM Controller data
 */
struct stm_spi_fsm {
	struct mtd_info		mtd;
	struct device		*dev;
	struct resource		*region;
	struct stm_spifsm_caps	capabilities;
	uint32_t		configuration;
	uint32_t		fifo_dir_delay;

	/* needed to be able to resume properly */
	bool			reset;
	unsigned int		max_freq;
	int			(*resume)(struct stm_spi_fsm *);

	/* Block locking support */
	uint8_t			lock_mask;
	uint8_t			lock_val[2];

	/* 4KiB Parameter Sector boundaries */
	uint64_t		p4k_bot_end;
	uint64_t		p4k_top_start;

	/* 'BPx' Block Protection scheme */
	int			bpx_tbprot;
	int			bpx_n_bnds;
	int			bpx_n_bits;
	uint8_t			bpx_sr_masks[BPX_MAX_BITS];
	uint64_t		bpx_bnds[BPX_MAX_N_BNDS];

	void __iomem		*base;
	struct clk		*clk;
	struct mutex		lock;
	unsigned		partitioned;
	uint8_t	page_buf[FLASH_PAGESIZE]__aligned(4);
};

/*
 * SPI FLASH
 */

/* Commands */
#define FLASH_CMD_WREN		0x06
#define FLASH_CMD_WRDI		0x04
#define FLASH_CMD_RDID		0x9f
#define FLASH_CMD_RDSR		0x05
#define FLASH_CMD_RDSR2		0x35
#define FLASH_CMD_RDSR3		0x15
#define FLASH_CMD_WRSR		0x01
#define FLASH_CMD_WRSR3		0x11
#define FLASH_CMD_SE_4K		0x20
#define FLASH_CMD_SE_32K	0x52
#define FLASH_CMD_SE		0xd8
#define FLASH_CMD_CHIPERASE	0xc7

#define FLASH_CMD_READ		0x03	/* READ */
#define FLASH_CMD_READ_FAST	0x0b	/* FAST READ */
#define FLASH_CMD_READ_1_1_2	0x3b	/* DUAL OUTPUT READ */
#define FLASH_CMD_READ_1_2_2	0xbb	/* DUAL I/O READ */
#define FLASH_CMD_READ_1_1_4	0x6b	/* QUAD OUTPUT READ */
#define FLASH_CMD_READ_1_4_4	0xeb	/* QUAD I/O READ */

#define FLASH_CMD_WRITE		0x02	/* PAGE PROGRAM */
#define FLASH_CMD_WRITE_1_1_2	0xa2	/* DUAL INPUT PROGRAM */
#define FLASH_CMD_WRITE_1_2_2	0xd2	/* DUAL INPUT EXT PROGRAM */
#define FLASH_CMD_WRITE_1_1_4	0x32	/* QUAD INPUT PROGRAM */
#define FLASH_CMD_WRITE_1_4_4	0x12	/* QUAD INPUT EXT PROGRAM */

#define FLASH_CMD_EN4B_ADDR	0xb7	/* Enter 4-byte address mode */
#define FLASH_CMD_EX4B_ADDR	0xe9	/* Exit 4-byte address mode */

/* READ commands with 32-bit addressing (N25Q256 and S25FLxxxS) */
#define FLASH_CMD_READ4		0x13
#define FLASH_CMD_READ4_FAST	0x0c
#define FLASH_CMD_READ4_1_1_2	0x3c
#define FLASH_CMD_READ4_1_2_2	0xbc
#define FLASH_CMD_READ4_1_1_4	0x6c
#define FLASH_CMD_READ4_1_4_4	0xec

/* N25Q Commands */
/*	- READ/CLEAR Flags Status register */
#define N25Q_CMD_RFSR		0x70
#define N25Q_CMD_CLFSR		0x50
#define N25Q_CMD_WRVCR		0x81
#define N25Q_CMD_RDVCR		0x85
#define N25Q_CMD_RDVECR		0x65
#define N25Q_CMD_RDNVCR		0xb5
#define N25Q_CMD_WRNVCR		0xb1
#define N25Q_CMD_RDLOCK		0xe8
#define N25Q_CMD_WRLOCK		0xe5


/* N25Q Flags Status Register: Error Flags */
#define N25Q_FLAGS_ERR_ERASE	(1 << 5)
#define N25Q_FLAGS_ERR_PROG	(1 << 4)
#define N25Q_FLAGS_ERR_VPP	(1 << 3)
#define N25Q_FLAGS_ERR_PROT	(1 << 1)
#define N25Q_FLAGS_ERROR	(N25Q_FLAGS_ERR_ERASE	| \
				 N25Q_FLAGS_ERR_PROG	| \
				 N25Q_FLAGS_ERR_VPP	| \
				 N25Q_FLAGS_ERR_PROT)

/* W25Q commands */
#define W25Q_CMD_RDLOCK		0x3d
#define W25Q_CMD_LOCK		0x36
#define W25Q_CMD_UNLOCK		0x39

/* MX25 Commands */
/*	- Read Security Register (home of '4BYTE' status bit!) */
#define MX25_CMD_WRITE_1_4_4	0x38
#define MX25_CMD_RDCR		0x15
#define MX25_CMD_RDSCUR		0x2b
#define MX25_CMD_RDSFDP		0x5a
#define MX25_CMD_SBLK		0x36
#define MX25_CMD_SBULK		0x39
#define MX25_CMD_RDBLOCK	0x3c
#define MX25_CMD_RDDPB		0xe0
#define MX25_CMD_WRDPB		0xe1

/* S25FLxxxS commands */
/*	- WRITE/ERASE 32-bit address commands */
#define S25FL_CMD_WRITE4	0x12	/* Note, opcode shared with clashes with
					 * 'FLASH_CMD_WRITE_1_4_4', as found on
					 * N25Qxxx devices! */
#define S25FL_CMD_WRITE4_1_1_4	0x34
#define S25FL_CMD_SE4		0xdc
/*	- Clear status register flags */
#define S25FL_CMD_CLSR		0x30
/*	- Read/Write 'DYB' lock bit */
#define S25FL_CMD_DYBWR		0xe1
#define S25FL_CMD_DYBRD		0xe0

/* Status register */
#define FLASH_STATUS_BUSY	0x01
#define FLASH_STATUS_WEL	0x02
#define FLASH_STATUS_BP0	0x04
#define FLASH_STATUS_BP1	0x08
#define FLASH_STATUS_BP2	0x10
#define FLASH_STATUS_BP3	0x20
#define FLASH_STATUS_SRWP0	0x80
/*	- S25FL Error Flags */
#define S25FL_STATUS_E_ERR	0x20
#define S25FL_STATUS_P_ERR	0x40
/*	- Timeout status */
#define FLASH_STATUS_TIMEOUT	0xff

/* Maximum READID length */
#define MAX_READID_LEN		6

/* Capabilities */
#define FLASH_CAPS_SINGLE	0x0000ffff
#define FLASH_CAPS_READ_WRITE	0x00000001
#define FLASH_CAPS_READ_FAST	0x00000002
#define FLASH_CAPS_SE_4K	0x00000004
#define FLASH_CAPS_SE_32K	0x00000008
#define FLASH_CAPS_CE		0x00000010
#define FLASH_CAPS_32BITADDR	0x00000020
#define FLASH_CAPS_RESET	0x00000040
/* Device supports some form of volatile, individual block locking scheme
 * (e.g. Spansion's "DYB" scheme, or Micron's "Sector Lock Register").
 */
#define FLASH_CAPS_BLK_LOCKING	0x00000080
/* 'BPx' Block Protection scheme */
#define FLASH_CAPS_BPX_LOCKING	0x00000100

#define FLASH_CAPS_DUAL		0x00ff0000
#define FLASH_CAPS_READ_1_1_2	0x00010000
#define FLASH_CAPS_READ_1_2_2	0x00020000
#define FLASH_CAPS_READ_2_2_2	0x00040000
#define FLASH_CAPS_WRITE_1_1_2	0x00100000
#define FLASH_CAPS_WRITE_1_2_2	0x00200000
#define FLASH_CAPS_WRITE_2_2_2	0x00400000

#define FLASH_CAPS_QUAD		0xff000000
#define FLASH_CAPS_READ_1_1_4	0x01000000
#define FLASH_CAPS_READ_1_4_4	0x02000000
#define FLASH_CAPS_READ_4_4_4	0x04000000
#define FLASH_CAPS_WRITE_1_1_4	0x10000000
#define FLASH_CAPS_WRITE_1_4_4	0x20000000
#define FLASH_CAPS_WRITE_4_4_4	0x40000000

#define FSM_BLOCK_UNLOCKED	0
#define FSM_BLOCK_LOCKED	1

/*
 * FSM template sequences (Note, various fields are modified on-the-fly)
 */
struct fsm_seq {
	uint32_t data_size;
	uint32_t addr1;
	uint32_t addr2;
	uint32_t addr_cfg;
	uint32_t seq_opc[5];
	uint32_t mode;
	uint32_t dummy;
	uint32_t status;
	uint8_t  seq[16];
	uint32_t seq_cfg;
} __attribute__((__packed__, aligned(4)));
#define FSM_SEQ_SIZE			sizeof(struct fsm_seq)

static struct fsm_seq fsm_seq_dummy = {
	.data_size = TRANSFER_SIZE(0),
	.seq = {
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

#define MAX_READID_LEN_ALIGNED	((MAX_READID_LEN + 0x3) & ~0x3)
static struct fsm_seq fsm_seq_read_jedec = {
	.data_size = TRANSFER_SIZE(MAX_READID_LEN_ALIGNED),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDID)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq fsm_seq_read_status_fifo = {
	.data_size = TRANSFER_SIZE(4),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDSR)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq fsm_seq_write_status = {
	.seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),
	.seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_WRSR)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_STA_WR1,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq fsm_seq_erase_sector = {
	/* 'addr_cfg' configured during initialisation */
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_SE)),
	},
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_ADD1,
		FSM_INST_ADD2,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq fsm_seq_erase_chip = {
	.seq_opc = {
		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_WREN) | SEQ_OPC_CSDEASSERT),

		(SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
		 SEQ_OPC_OPCODE(FLASH_CMD_CHIPERASE) | SEQ_OPC_CSDEASSERT),
	},
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_CMD2,
		FSM_INST_WAIT,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_ERASE |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

static struct fsm_seq fsm_seq_read_rdsfdp = {
	.data_size = TRANSFER_SIZE(4),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(MX25_CMD_RDSFDP)),
	.addr_cfg = (ADR_CFG_CYCLES_ADD1(8) |
		     ADR_CFG_PADS_1_ADD1 |
		     ADR_CFG_CYCLES_ADD2(16) |
		     ADR_CFG_PADS_1_ADD2),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_ADD1,
		FSM_INST_ADD2,
		FSM_INST_DUMMY,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

/* Configure 'addr_cfg' according to addressing mode */
static int configure_erasesec_seq(struct fsm_seq *seq, int use_32bit_addr)
{
	int addr1_cycles = use_32bit_addr ? 16 : 8;

	seq->addr_cfg = (ADR_CFG_CYCLES_ADD1(addr1_cycles) |
			 ADR_CFG_PADS_1_ADD1 |
			 ADR_CFG_CYCLES_ADD2(16) |
			 ADR_CFG_PADS_1_ADD2 |
			 ADR_CFG_CSDEASSERT_ADD2);
	return 0;
}

/* The following FSM sequences are configured during initialisation according to
 * the platform and device capabilities.
 */
static struct fsm_seq fsm_seq_read;
static struct fsm_seq fsm_seq_write;
static struct fsm_seq fsm_seq_en32bitaddr;
static struct fsm_seq fsm_seq_rd_lock_reg;
static struct fsm_seq fsm_seq_wr_lock_reg;
static struct fsm_seq fsm_seq_lock;
static struct fsm_seq fsm_seq_unlock;

/*
 * Debug code for examining FSM sequences
 */
#ifdef CONFIG_STM_SPI_FSM_DEBUG
char *flash_cmd_strs[256] = {
	[FLASH_CMD_WREN]	= "WREN",
	[FLASH_CMD_WRDI]	= "WRDI",
	[FLASH_CMD_RDID]	= "RDID",
	[FLASH_CMD_RDSR]	= "RDSR",
	[FLASH_CMD_RDSR2]	= "RDSR2",
	[FLASH_CMD_RDSR3]	= "RDSR3",
	[FLASH_CMD_WRSR]	= "WRSR",
	[FLASH_CMD_WRSR3]	= "WRSR3",
	[FLASH_CMD_SE]		= "SE",
	[FLASH_CMD_SE_4K]	= "SE_4K",
	[FLASH_CMD_SE_32K]	= "SE_32K",
	[FLASH_CMD_CHIPERASE]	= "CHIPERASE",
	[FLASH_CMD_READ]	= "READ",
	[FLASH_CMD_READ_FAST]	= "READ_FAST",
	[FLASH_CMD_READ_1_1_2]	= "READ_1_1_2",
	[FLASH_CMD_READ_1_2_2]	= "READ_1_2_2",
	[FLASH_CMD_READ_1_1_4]	= "READ_1_1_4",
	[FLASH_CMD_READ_1_4_4]	= "READ_1_4_4",
	[FLASH_CMD_WRITE]	= "WRITE",
	[FLASH_CMD_WRITE_1_1_2]	= "WRITE_1_1_2",
	[FLASH_CMD_WRITE_1_2_2]	= "WRITE_1_2_2",
	[FLASH_CMD_WRITE_1_1_4]	= "WRITE_1_1_4",
	[FLASH_CMD_WRITE_1_4_4] = "WRITE_1_4_4", /* Note, opcode shared with
						  * 'S25FL_CMD_WRITE4' */
	[FLASH_CMD_EN4B_ADDR]	= "EN4B_ADDR",
	[FLASH_CMD_EX4B_ADDR]	= "EX4B_ADDR",
	[FLASH_CMD_READ4]	= "READ4",
	[FLASH_CMD_READ4_FAST]	= "READ4_FAST",
	[FLASH_CMD_READ4_1_1_2]	= "READ4_1_1_2",
	[FLASH_CMD_READ4_1_2_2]	= "READ4_1_2_2",
	[FLASH_CMD_READ4_1_1_4]	= "READ4_1_1_4",
	[FLASH_CMD_READ4_1_4_4]	= "READ4_1_4_4",
};

char *fsm_inst_strs[256] = {
	[FSM_INST_CMD1]		= "CMD1",
	[FSM_INST_CMD2]		= "CMD2",
	[FSM_INST_CMD3]		= "CMD3",
	[FSM_INST_CMD4]		= "CMD4",
	[FSM_INST_CMD5]		= "CMD5",
	[FSM_INST_ADD1]		= "ADD1",
	[FSM_INST_ADD2]		= "ADD2",
	[FSM_INST_DATA_WRITE]	= "D_WR",
	[FSM_INST_DATA_READ]	= "D_RD",
	[FSM_INST_STA_RD1]	= "S_RW1",
	[FSM_INST_STA_RD2]	= "S_R2",
	[FSM_INST_STA_WR1_2]	= "S_W12",
	[FSM_INST_MODE]		= "MODE",
	[FSM_INST_DUMMY]	= "DUMMY",
	[FSM_INST_WAIT]		= "WAIT",
	[FSM_INST_STOP]		= "STOP",
};

static void fsm_dump_seq(char *tag, struct fsm_seq *seq)
{
	int i, j, k;
	uint8_t cmd;
	uint32_t *seq_reg;
	char *str;
	char cmd_str[64], *c;

	printk(KERN_INFO "%s:\n", tag ? tag : "FSM Sequence");
	printk(KERN_INFO "\t-------------------------------------------------"
	       "--------------\n");
	printk(KERN_INFO "\tTRANS_SIZE : 0x%08x  [ %d cycles/%d bytes ]\n",
	       seq->data_size, seq->data_size, seq->data_size/8);
	printk(KERN_INFO "\tADD1       : 0x%08x\n", seq->addr1);
	printk(KERN_INFO "\tADD2       : 0x%08x\n", seq->addr2);
	printk(KERN_INFO "\tADD_CFG    : 0x%08x  [ ADD1 %d(x%d)%s%s ]\n",
	       seq->addr_cfg, seq->addr_cfg & 0x1f,
	       ((seq->addr_cfg >> 6) & 0x3) + 1,
	       ((seq->addr_cfg >> 8) & 0x1) ? " CSDEASSERT" : "",
	       ((seq->addr_cfg >> 9) & 0x1) ? " 32BIT" : " 24BIT");
	printk(KERN_INFO "\t                         [ ADD2 %d(x%d)%s%s ]\n",
	       (seq->addr_cfg >> 16) & 0x1f,
	       ((seq->addr_cfg >> 22) & 0x3) + 1,
	       ((seq->addr_cfg >> 24) & 0x1) ? " CSDEASSERT" : "",
	       ((seq->addr_cfg >> 25) & 0x1) ? " 32BIT" : " 24BIT");

	for (i = 0; i < 5; i++) {
		if (seq->seq_opc[i] == 0x00000000) {
			printk(KERN_INFO "\tSEQ_OPC%d   : 0x%08x\n",
			       i + 1, seq->seq_opc[i]);
		} else {
			cmd = (uint8_t)(seq->seq_opc[i] & 0xff);
			str = flash_cmd_strs[cmd];
			if (!str)
				str = "UNKNOWN";
			printk(KERN_INFO "\tSEQ_OPC%d   : 0x%08x  "
			       "[ 0x%02x/%-12s %d(x%d)%11s ]\n",
			       i + 1, seq->seq_opc[i], cmd, str,
			       (seq->seq_opc[i] >> 8) & 0x3f,
			       ((seq->seq_opc[i] >> 14) & 0x3) + 1,
			       ((seq->seq_opc[i] >> 16) & 0x1) ?
			       " CSDEASSERT" : "");
		}
	}

	printk(KERN_INFO "\tMODE       : 0x%08x  [ 0x%02x %d(x%d)%s ]\n",
	       seq->mode,
	       seq->mode & 0xff,
	       (seq->mode >> 16) & 0x3f,
	       ((seq->mode >> 22) & 0x3) + 1,
	       ((seq->mode >> 24) & 0x1) ? " CSDEASSERT" : "");

	printk(KERN_INFO "\tDUMMY      : 0x%08x  [ 0x%02x %d(x%d)%s ]\n",
	       seq->dummy,
	       seq->dummy & 0xff,
	       (seq->dummy >> 16) & 0x3f,
	       ((seq->dummy >> 22) & 0x3) + 1,
	       ((seq->dummy >> 24) & 0x1) ? " CSDEASSERT" : "");

	if ((seq->status >> 21) & 0x1)
		printk(KERN_INFO "\tFLASH_STA  : 0x%08x  [ READ x%d%s ]\n",
		       seq->status, ((seq->status >> 16) & 0x3) + 1,
		       ((seq->status >> 20) & 0x1) ? " CSDEASSERT" : "");
	else
		printk(KERN_INFO "\tFLASH_STA  : 0x%08x  "
		       "[ WRITE {0x%02x, 0x%02x} x%d%s ]\n",
		       seq->status, seq->status & 0xff,
		       (seq->status >> 8) & 0xff,
		       ((seq->status >> 16) & 0x3) + 1,
		       ((seq->status >> 20) & 0x1) ? " CSDEASSERT" : "");

	seq_reg = (uint32_t *)seq->seq;
	for (i = 0, k = 0; i < 4; i++) {
		c = cmd_str;
		*c = '\0';
		for (j = 0; j < 4; j++, k++) {
			cmd = (uint8_t)(seq->seq[k] & 0xff);
			str = fsm_inst_strs[cmd];
			if (j != 0)
				c += sprintf(c, "%4s", str ? " -> " : "");
			c += sprintf(c, "%5s", str ? str : "");
		}
		printk(KERN_INFO "\tFAST_SEQ%d  : 0x%08x  [ %s ]\n",
		       i, seq_reg[i], cmd_str);
	}

	printk(KERN_INFO "\tSEQ_CONFIG : 0x%08x  [ START_SEQ   = %d ]\n",
	       seq->seq_cfg, seq->seq_cfg & 0x1);
	printk(KERN_INFO "\t                         [ SW_RESET    = %d ]\n",
	       (seq->seq_cfg >> 5) & 0x1);
	printk(KERN_INFO "\t                         [ CSDEASSERT  = %d ]\n",
	       (seq->seq_cfg >> 6) & 0x1);
	printk(KERN_INFO "\t                         [ RD_NOT_WR   = %d ]\n",
	       (seq->seq_cfg >> 7) & 0x1);
	printk(KERN_INFO "\t                         [ ERASE_BIT   = %d ]\n",
	       (seq->seq_cfg >> 8) & 0x1);
	printk(KERN_INFO "\t                         [ DATA_PADS   = %d ]\n",
	       ((seq->seq_cfg >> 16) & 0x3) + 1);
	printk(KERN_INFO "\t                         [ DREQ_HALF   = %d ]\n",
	       (seq->seq_cfg >> 18) & 0x1);
	printk(KERN_INFO "\t-------------------------------------------------"
	       "--------------\n");
}
#endif /* CONFIG_STM_SPI_FSM_DEBUG */

/*
 * SPI Flash Device Table
 */
struct flash_info {
	char		*name;

	/* READID data, as returned by 'FLASH_CMD_RDID' (0x9f). */
	u8		readid[MAX_READID_LEN];
	int		readid_len;

	/* The size listed here is what works with FLASH_CMD_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned	sector_size;
	u16		n_sectors;

	/* FLASH device capabilities.  Note, in contrast to the other
	 * capabilities, 'FLASH_CAPS_32BITADDR' is set at probe time, based on
	 * the size of the device found.
	 */
	u32		capabilities;

	/* Maximum operating frequency.  Note, where FAST_READ is supported,
	 * freq_max specifies the FAST_READ frequency, not the READ frequency.
	 */
	u32		max_freq;

	int		(*config)(struct stm_spi_fsm *, struct flash_info *);
	int		(*resume)(struct stm_spi_fsm *);
};

/* Device with standard 3-byte JEDEC ID */
#define JEDEC_INFO(_name, _jedec_id, _sector_size, _n_sectors,		\
		   _capabilities, _max_freq, _config, _resume)		\
	{								\
		.name = (_name),					\
		.readid[0] = ((_jedec_id) >> 16 & 0xff),		\
		.readid[1] = ((_jedec_id) >>  8 & 0xff),		\
		.readid[2] = ((_jedec_id) >>  0 & 0xff),		\
		.readid_len = 3,					\
		.sector_size = (_sector_size),				\
		.n_sectors = (_n_sectors),				\
		.capabilities = (_capabilities),			\
		.max_freq = (_max_freq),				\
		.config = (_config),					\
		.resume = (_resume)					\
	}

/* Device with arbitrary-length READID */
#define RDID(...) __VA_ARGS__	/* Dummy macro to protect array argument. */
#define RDID_INFO(_name, _readid, _readid_len, _sector_size,		\
		  _n_sectors, _capabilities, _max_freq, _config,	\
		  _resume)						\
	{								\
		.name = (_name),					\
		.readid = _readid,					\
		.readid_len = _readid_len,				\
		.capabilities = (_capabilities),			\
		.sector_size = (_sector_size),				\
		.n_sectors = (_n_sectors),				\
		.capabilities = (_capabilities),			\
		.max_freq = (_max_freq),				\
		.config = (_config),					\
		.resume = (_resume)					\
	}


static int w25q_config(struct stm_spi_fsm *fsm, struct flash_info *info);
static int n25q_config(struct stm_spi_fsm *fsm, struct flash_info *info);
static int mx25_config(struct stm_spi_fsm *fsm, struct flash_info *info);
static int s25fl_config(struct stm_spi_fsm *fsm, struct flash_info *info);

static int n25q_resume(struct stm_spi_fsm *fsm);
static int mx25_resume(struct stm_spi_fsm *fsm);

static struct flash_info flash_types[] = {

	/* ST Microelectronics/Numonyx --
	 * (newer production versions may have feature updates (eg faster
	 * operating frequency) */
#define M25P_CAPS (FLASH_CAPS_READ_WRITE | FLASH_CAPS_READ_FAST)
	JEDEC_INFO("m25p40",  0x202013,  64 * 1024,   8, M25P_CAPS, 25, NULL,
		   NULL),
	JEDEC_INFO("m25p80",  0x202014,  64 * 1024,  16, M25P_CAPS, 25, NULL,
		   NULL),
	JEDEC_INFO("m25p16",  0x202015,  64 * 1024,  32, M25P_CAPS, 25, NULL,
		   NULL),
	JEDEC_INFO("m25p32",  0x202016,  64 * 1024,  64, M25P_CAPS, 50, NULL,
		   NULL),
	JEDEC_INFO("m25p64",  0x202017,  64 * 1024, 128, M25P_CAPS, 50, NULL,
		   NULL),
	JEDEC_INFO("m25p128", 0x202018, 256 * 1024,  64, M25P_CAPS, 50, NULL,
		   NULL),

#define M25PX_CAPS (FLASH_CAPS_READ_WRITE	| \
		    FLASH_CAPS_READ_FAST	| \
		    FLASH_CAPS_READ_1_1_2	| \
		    FLASH_CAPS_WRITE_1_1_2)
	JEDEC_INFO("m25px32", 0x207116,  64 * 1024,  64, M25PX_CAPS, 75, NULL,
		   NULL),
	JEDEC_INFO("m25px64", 0x207117,  64 * 1024, 128, M25PX_CAPS, 75, NULL,
		   NULL),

	/* Macronix MX25xxx
	 *     - Support for 'FLASH_CAPS_WRITE_1_4_4' is omitted for devices
	 *       where operating frequency must be reduced.
	 */
#define MX25_CAPS (FLASH_CAPS_READ_WRITE	| \
		   FLASH_CAPS_READ_FAST		| \
		   FLASH_CAPS_READ_1_1_2	| \
		   FLASH_CAPS_READ_1_2_2	| \
		   FLASH_CAPS_READ_1_1_4	| \
		   FLASH_CAPS_READ_1_4_4	| \
		   FLASH_CAPS_SE_4K		| \
		   FLASH_CAPS_SE_32K)
	JEDEC_INFO("mx25l3255e",  0xc29e16, 64 * 1024, 64,
		   (MX25_CAPS | FLASH_CAPS_WRITE_1_4_4), 86,
		   mx25_config, mx25_resume),
	JEDEC_INFO("mx25l12835f", 0xc22018, 64 * 1024, 256,
		   (MX25_CAPS | FLASH_CAPS_RESET), 70,
		   mx25_config, mx25_resume),
	JEDEC_INFO("mx25l25635f", 0xc22019, 64 * 1024, 512,
		   (MX25_CAPS | FLASH_CAPS_RESET), 70,
		   mx25_config, mx25_resume),
	JEDEC_INFO("mx25l25655f", 0xc22619, 64 * 1024, 512,
		   (MX25_CAPS | FLASH_CAPS_RESET), 70,
		   mx25_config, mx25_resume),

	/* Micron N25Qxxx */
#define N25Q_CAPS (FLASH_CAPS_READ_WRITE	| \
		   FLASH_CAPS_READ_FAST		| \
		   FLASH_CAPS_READ_1_1_2	| \
		   FLASH_CAPS_READ_1_2_2	| \
		   FLASH_CAPS_READ_1_1_4	| \
		   FLASH_CAPS_READ_1_4_4	| \
		   FLASH_CAPS_WRITE_1_1_2	| \
		   FLASH_CAPS_WRITE_1_2_2	| \
		   FLASH_CAPS_WRITE_1_1_4	| \
		   FLASH_CAPS_WRITE_1_4_4	| \
		   FLASH_CAPS_BLK_LOCKING)
	JEDEC_INFO("n25q128", 0x20ba18, 64 * 1024,  256,
		   N25Q_CAPS, 108, n25q_config, n25q_resume),

	/* Micron N25Q256/N25Q512/N25Q00A (32-bit ADDR devices)
	 *
	 * Versions are available with or without a dedicated RESET# pin
	 * (e.g. N25Q512A83GSF40G vs. N25Q512A13GSF40G).  To complicate matters,
	 * the versions that include a RESET# pin (Feature Set = 8) require a
	 * different opcode for the FLASH_CMD_WRITE_1_4_4 command.
	 * Unfortunately it is not possible to determine easily at run-time
	 * which version is being used.  We therefore remove support for
	 * FLASH_CAPS_WRITE_1_4_4 (falling back to FLASH_CAPS_WRITE_1_1_4), and
	 * defer overall support for RESET# to the board-level platform/Device
	 * Tree property "reset-signal".
	 */
#define N25Q_32BITADDR_CAPS	((N25Q_CAPS		| \
				  FLASH_CAPS_RESET)	& \
				 ~FLASH_CAPS_WRITE_1_4_4)
	JEDEC_INFO("n25q256", 0x20ba19, 64 * 1024,   512,
		   N25Q_32BITADDR_CAPS, 108, n25q_config, n25q_resume),
	RDID_INFO("n25q512", RDID({0x20, 0xba, 0x20, 0x10, 0x00}), 5,
		  64 * 1024,  1024, N25Q_32BITADDR_CAPS, 108,
		  n25q_config, n25q_resume),
	RDID_INFO("n25q00a", RDID({0x20, 0xba, 0x21, 0x10, 0x00}), 5,
		  64 * 1024,  2048, N25Q_32BITADDR_CAPS, 108,
		  n25q_config, n25q_resume),

	/* Spansion S25FLxxxP
	 *     - 256KiB and 64KiB sector variants (identified by ext. JEDEC)
	 *     - S25FL128Px does not support DUAL or QUAD I/O and has
	 *       non-standard BPx locking scheme.
	 */
#define S25FLXXXP_CAPS (FLASH_CAPS_READ_WRITE	| \
			FLASH_CAPS_READ_1_1_2	| \
			FLASH_CAPS_READ_1_2_2	| \
			FLASH_CAPS_READ_1_1_4	| \
			FLASH_CAPS_READ_1_4_4	| \
			FLASH_CAPS_WRITE_1_1_4	| \
			FLASH_CAPS_READ_FAST	| \
			FLASH_CAPS_BPX_LOCKING)
	RDID_INFO("s25fl032p", RDID({0x01, 0x02, 0x15, 0x4d, 0x00}), 5,
		  64 * 1024,  64, S25FLXXXP_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl064p", RDID({0x01, 0x02, 0x16, 0x4d, 0x00}), 5,
		  64 * 1024,  128, S25FLXXXP_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl128p1", RDID({0x01, 0x20, 0x18, 0x03, 0x00}), 5,
		  256 * 1024, 64,
		 (FLASH_CAPS_READ_WRITE | FLASH_CAPS_READ_FAST), 104,
		 NULL, NULL),
	RDID_INFO("s25fl128p0", RDID({0x01, 0x20, 0x18, 0x03, 0x01}), 5,
		  64 * 1024, 256,
		  (FLASH_CAPS_READ_WRITE | FLASH_CAPS_READ_FAST), 104,
		  NULL, NULL),
	RDID_INFO("s25fl129p0", RDID({0x01, 0x20, 0x18, 0x4d, 0x00}), 5,
		  256 * 1024,  64, S25FLXXXP_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl129p1", RDID({0x01, 0x20, 0x18, 0x4d, 0x01}), 5,
		  64 * 1024, 256, S25FLXXXP_CAPS, 80,
		  s25fl_config, NULL),

	/* Spansion S25FLxxxS
	 *     - 256KiB and 64KiB sector variants (identified by ext. JEDEC)
	 *     - RESET# signal supported by die but not bristled out on all
	 *       package types.  The package type is a function of board design,
	 *       so this information is captured in the board's capabilities.
	 *     - Supports 'DYB' sector protection. Depending on variant, sectors
	 *       may default to locked state on power-on.
	 *     - S25FL127Sx handled as S25FL128Sx
	 */
#define S25FLXXXS_CAPS (S25FLXXXP_CAPS		| \
			FLASH_CAPS_RESET	| \
			FLASH_CAPS_BLK_LOCKING	| \
			FLASH_CAPS_BPX_LOCKING)
	RDID_INFO("s25fl128s0", RDID({0x01, 0x20, 0x18, 0x4d, 0x00, 0x80}), 6,
		  256 * 1024, 64, S25FLXXXS_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl128s1", RDID({0x01, 0x20, 0x18, 0x4d, 0x01, 0x80}), 6,
		  64 * 1024, 256, S25FLXXXS_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl256s0", RDID({0x01, 0x02, 0x19, 0x4d, 0x00, 0x80}), 6,
		  256 * 1024, 128, S25FLXXXS_CAPS, 80,
		  s25fl_config, NULL),
	RDID_INFO("s25fl256s1", RDID({0x01, 0x02, 0x19, 0x4d, 0x01, 0x80}), 6,
		  64 * 1024, 512, S25FLXXXS_CAPS, 80,
		  s25fl_config, NULL),

	/* Spansion S25FL1xxK */
#define S25FL1XXK_CAPS (FLASH_CAPS_READ_WRITE	| \
			FLASH_CAPS_READ_1_1_2	| \
			FLASH_CAPS_READ_1_2_2	| \
			FLASH_CAPS_READ_1_1_4	| \
			FLASH_CAPS_READ_1_4_4	| \
			FLASH_CAPS_READ_FAST)
	JEDEC_INFO("s25fl116k", 0x014015, 64 * 1024,  32, S25FL1XXK_CAPS,
		   108, s25fl_config, NULL),
	JEDEC_INFO("s25fl132k", 0x014016, 64 * 1024,  64, S25FL1XXK_CAPS,
		   108, s25fl_config, NULL),
	JEDEC_INFO("s25fl164k", 0x014017, 64 * 1024, 128, S25FL1XXK_CAPS,
		   108, s25fl_config, NULL),

	/* Winbond -- w25x "blocks" are 64K, "sectors" are 4KiB */
#define W25X_CAPS (FLASH_CAPS_READ_WRITE	| \
		   FLASH_CAPS_READ_FAST		| \
		   FLASH_CAPS_READ_1_1_2	| \
		   FLASH_CAPS_WRITE_1_1_2)
	JEDEC_INFO("w25x40", 0xef3013, 64 * 1024,   8, W25X_CAPS, 75,
		   NULL, NULL),
	JEDEC_INFO("w25x80", 0xef3014, 64 * 1024,  16, W25X_CAPS, 75,
		   NULL, NULL),
	JEDEC_INFO("w25x16", 0xef3015, 64 * 1024,  32, W25X_CAPS, 75,
		   NULL, NULL),
	JEDEC_INFO("w25x32", 0xef3016, 64 * 1024,  64, W25X_CAPS, 75,
		   NULL, NULL),
	JEDEC_INFO("w25x64", 0xef3017, 64 * 1024, 128, W25X_CAPS, 75,
		   NULL, NULL),

	/* Winbond -- w25q "blocks" are 64K, "sectors" are 4KiB */
#define W25Q_CAPS (FLASH_CAPS_READ_WRITE	| \
		   FLASH_CAPS_READ_FAST		| \
		   FLASH_CAPS_READ_1_1_2	| \
		   FLASH_CAPS_READ_1_2_2	| \
		   FLASH_CAPS_READ_1_1_4	| \
		   FLASH_CAPS_READ_1_4_4	| \
		   FLASH_CAPS_WRITE_1_1_4       | \
		   FLASH_CAPS_BLK_LOCKING       | \
		   FLASH_CAPS_BPX_LOCKING)
	JEDEC_INFO("w25q80", 0xef4014, 64 * 1024,  16,
		   W25Q_CAPS, 80, w25q_config, NULL),
	JEDEC_INFO("w25q16", 0xef4015, 64 * 1024,  32,
		   W25Q_CAPS, 80, w25q_config, NULL),
	JEDEC_INFO("w25q32", 0xef4016, 64 * 1024,  64,
		   W25Q_CAPS, 80, w25q_config, NULL),
	JEDEC_INFO("w25q64", 0xef4017, 64 * 1024, 128,
		   W25Q_CAPS, 80, w25q_config, NULL),

	{ },
};

/*
 * SoC reset on 'boot-from-spi' systems
 *
 * Certain modes of operation cause the Flash device to enter a particular state
 * for a period of time (e.g. 'Erase Sector', 'Quad Enable', and 'Enter 32-bit
 * Addr' commands).  On boot-from-spi systems, it is important to consider what
 * happens if a warm reset occurs during this period.  The SPIBoot controller
 * assumes that Flash device is in its default reset state, 24-bit address mode,
 * and ready to accept commands.  This can be achieved using some form of
 * on-board logic/controller to force a device POR in response to a SoC-level
 * reset or by making use of the device reset signal if available (limited
 * number of devices only).
 *
 * Failure to take such precautions can cause problems following a warm reset.
 * For some operations (e.g. ERASE), there is little that can be done.  For
 * other modes of operation (e.g. 32-bit addressing), options are often
 * available that can help minimise the window in which a reset could cause a
 * problem.
 *
 */
static int can_handle_soc_reset(struct stm_spi_fsm *fsm)
{
	/* Reset signal is available on the board and supported by the device */
	if (fsm->capabilities.reset_signal && fsm->reset)
		return 1;

	/* Board-level logic forces a power-on-reset */
	if (fsm->capabilities.reset_por)
		return 1;

	/* Reset is not properly handled and may result in failure to reboot. */
	return 0;
}

/* Parameters to configure a READ or WRITE FSM sequence */
struct seq_rw_config {
	uint32_t	capabilities;	/* capabilities to support config */
	uint8_t		cmd;		/* FLASH command */
	int		write;		/* Write Sequence */
	uint8_t		addr_pads;	/* No. of addr pads (MODE & DUMMY) */
	uint8_t		data_pads;	/* No. of data pads */
	uint8_t		mode_data;	/* MODE data */
	uint8_t		mode_cycles;	/* No. of MODE cycles */
	uint8_t		dummy_cycles;	/* No. of DUMMY cycles */
};

/* Default READ configurations, in order of preference */
static struct seq_rw_config default_read_configs[] = {
	{FLASH_CAPS_READ_1_4_4, FLASH_CMD_READ_1_4_4,	0, 4, 4, 0x00, 2, 4},
	{FLASH_CAPS_READ_1_1_4, FLASH_CMD_READ_1_1_4,	0, 1, 4, 0x00, 4, 0},
	{FLASH_CAPS_READ_1_2_2, FLASH_CMD_READ_1_2_2,	0, 2, 2, 0x00, 4, 0},
	{FLASH_CAPS_READ_1_1_2, FLASH_CMD_READ_1_1_2,	0, 1, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_FAST,	FLASH_CMD_READ_FAST,	0, 1, 1, 0x00, 0, 8},
	{FLASH_CAPS_READ_WRITE, FLASH_CMD_READ,	        0, 1, 1, 0x00, 0, 0},

	/* terminating entry */
	{0x00,			 0,			0, 0, 0, 0x00, 0, 0},
};

/* Default WRITE configurations, in order of preference */
static struct seq_rw_config default_write_configs[] = {
	{FLASH_CAPS_WRITE_1_4_4, FLASH_CMD_WRITE_1_4_4, 1, 4, 4, 0x00, 0, 0},
	{FLASH_CAPS_WRITE_1_1_4, FLASH_CMD_WRITE_1_1_4, 1, 1, 4, 0x00, 0, 0},
	{FLASH_CAPS_WRITE_1_2_2, FLASH_CMD_WRITE_1_2_2, 1, 2, 2, 0x00, 0, 0},
	{FLASH_CAPS_WRITE_1_1_2, FLASH_CMD_WRITE_1_1_2, 1, 1, 2, 0x00, 0, 0},
	{FLASH_CAPS_READ_WRITE,  FLASH_CMD_WRITE,       1, 1, 1, 0x00, 0, 0},

	/* terminating entry */
	{0x00,			  0,			 0, 0, 0, 0x00, 0, 0},
};

/* Search for preferred configuration based on available capabilities */
static struct seq_rw_config
*search_seq_rw_configs(struct seq_rw_config configs[], uint32_t capabilities)
{
	struct seq_rw_config *config;

	for (config = configs; config->cmd != 0; config++)
		if ((config->capabilities & capabilities) ==
		    config->capabilities)
			return config;

	return NULL;
}

/* Configure a READ/WRITE sequence according to configuration parameters */
static int configure_rw_seq(struct fsm_seq *seq, struct seq_rw_config *cfg,
			    int use_32bit_addr)
{
	int i;
	int addr1_cycles, addr2_cycles;

	memset(seq, 0x00, FSM_SEQ_SIZE);

	i = 0;
	/* Add READ/WRITE OPC  */
	seq->seq_opc[i++] = (SEQ_OPC_PADS_1 |
			     SEQ_OPC_CYCLES(8) |
			     SEQ_OPC_OPCODE(cfg->cmd));

	/* Add WREN OPC for a WRITE sequence */
	if (cfg->write)
		seq->seq_opc[i++] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
				     SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
				     SEQ_OPC_CSDEASSERT);

	/* Address configuration (24 or 32-bit addresses) */
	addr1_cycles = use_32bit_addr ? 16 : 8;
	addr1_cycles /= cfg->addr_pads;
	addr2_cycles = 16 / cfg->addr_pads;
	seq->addr_cfg = ((addr1_cycles & 0x3f) << 0 |	/* ADD1 cycles */
			 (cfg->addr_pads - 1) << 6 |	/* ADD1 pads */
			 (addr2_cycles & 0x3f) << 16 |	/* ADD2 cycles */
			 ((cfg->addr_pads - 1) << 22));	/* ADD2 pads */

	/* Data/Sequence configuration */
	seq->seq_cfg = ((cfg->data_pads - 1) << 16 |
			SEQ_CFG_STARTSEQ |
			SEQ_CFG_CSDEASSERT);
	if (!cfg->write)
		seq->seq_cfg |= SEQ_CFG_READNOTWRITE;

	/* Mode configuration (no. of pads taken from addr cfg) */
	seq->mode = ((cfg->mode_data & 0xff) << 0 |	/* data */
		     (cfg->mode_cycles & 0x3f) << 16 |	/* cycles */
		     (cfg->addr_pads - 1) << 22);	/* pads */

	/* Dummy configuration (no. of pads taken from addr cfg) */
	seq->dummy = ((cfg->dummy_cycles & 0x3f) << 16 |	/* cycles */
		      (cfg->addr_pads - 1) << 22);		/* pads */


	/* Instruction sequence */
	i = 0;
	if (cfg->write)
		seq->seq[i++] = FSM_INST_CMD2;

	seq->seq[i++] = FSM_INST_CMD1;

	seq->seq[i++] = FSM_INST_ADD1;
	seq->seq[i++] = FSM_INST_ADD2;

	if (cfg->mode_cycles)
		seq->seq[i++] = FSM_INST_MODE;

	if (cfg->dummy_cycles)
		seq->seq[i++] = FSM_INST_DUMMY;

	seq->seq[i++] = cfg->write ? FSM_INST_DATA_WRITE : FSM_INST_DATA_READ;
	seq->seq[i++] = FSM_INST_STOP;

	return 0;
}


/* Configure preferred READ/WRITE sequence according to available
 * capabilities
 */
static int fsm_search_configure_rw_seq(struct stm_spi_fsm *fsm,
				       struct fsm_seq *seq,
				       struct seq_rw_config *configs,
				       uint32_t capabilities)
{
	struct seq_rw_config *config;

	config = search_seq_rw_configs(configs, capabilities);
	if (!config) {
		dev_err(fsm->dev, "failed to find suitable config\n");
		return 1;
	}

	if (configure_rw_seq(seq, config,
			     capabilities & FLASH_CAPS_32BITADDR) != 0) {
		dev_err(fsm->dev, "failed to configure READ/WRITE sequence\n");
		return 1;
	}

	return 0;
}

/* [DEFAULT] Configure READ/WRITE/ERASE sequences */
static int fsm_config_rwe_seqs_default(struct stm_spi_fsm *fsm,
				       struct flash_info *info)
{
	/* Configure 'READ' sequence */
	if (fsm_search_configure_rw_seq(fsm, &fsm_seq_read,
					default_read_configs,
					info->capabilities) != 0) {
		dev_err(fsm->dev, "failed to configure READ sequence according to capabilities [0x%08x]\n",
			info->capabilities);
		return 1;
	}

	/* Configure 'WRITE' sequence */
	if (fsm_search_configure_rw_seq(fsm, &fsm_seq_write,
					default_write_configs,
					info->capabilities) != 0) {
		dev_err(fsm->dev, "failed to configure WRITE sequence according to capabilities [0x%08x]\n",
			info->capabilities);
		return 1;
	}

	/* Configure 'ERASE_SECTOR' sequence */
	configure_erasesec_seq(&fsm_seq_erase_sector,
			       info->capabilities & FLASH_CAPS_32BITADDR);

	return 0;
}

/* Configure Block Lock Reg Read sequence */
static int configure_block_rd_lock_seq(struct fsm_seq *rd_lock_seq,
				     uint8_t rd_lock_opcode,
				     int use_32bit_addr)
{
	int addr1_cycles = use_32bit_addr ? 16 : 8;

	/* Read Block Lock status */
	*rd_lock_seq = (struct fsm_seq) {
		.data_size = TRANSFER_SIZE(4),
		.seq_opc[0] = (SEQ_OPC_PADS_1 |
			       SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(rd_lock_opcode)),
		.addr_cfg = (ADR_CFG_CYCLES_ADD1(addr1_cycles) |
			     ADR_CFG_PADS_1_ADD1 |
			     ADR_CFG_CYCLES_ADD2(16) |
			     ADR_CFG_PADS_1_ADD2),
		.seq = {
			FSM_INST_CMD1,
			FSM_INST_ADD1,
			FSM_INST_ADD2,
			FSM_INST_DATA_READ,
			FSM_INST_STOP,
		},
		.seq_cfg = (SEQ_CFG_PADS_1 |
			    SEQ_CFG_READNOTWRITE |
			    SEQ_CFG_CSDEASSERT |
			    SEQ_CFG_STARTSEQ),
	};

	return 0;
}

/* Configure Block Lock Reg Write sequence */
static int configure_block_wr_lock_seq(struct fsm_seq *wr_lock_seq,
				     uint8_t wr_lock_opcode,
				     int use_32bit_addr)
{
	int addr1_cycles = use_32bit_addr ? 16 : 8;

	/* Write Block Lock status */
	*wr_lock_seq = (struct fsm_seq) {
		.seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
			       SEQ_OPC_CSDEASSERT),
		.seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(wr_lock_opcode)),
		.addr_cfg = (ADR_CFG_CYCLES_ADD1(addr1_cycles) |
			     ADR_CFG_PADS_1_ADD1 |
			     ADR_CFG_CYCLES_ADD2(16) |
			     ADR_CFG_PADS_1_ADD2),
		.seq = {
			FSM_INST_CMD1,
			FSM_INST_CMD2,
			FSM_INST_ADD1,
			FSM_INST_ADD2,
			FSM_INST_STA_WR1,
			FSM_INST_STOP,
		},
		.seq_cfg = (SEQ_CFG_PADS_1 |
			    SEQ_CFG_READNOTWRITE |
			    SEQ_CFG_CSDEASSERT |
			    SEQ_CFG_STARTSEQ),
	};

	return 0;
}

/* Configure Block Lock/Unlock sequence */
static int configure_block_lock_seq(struct fsm_seq *lock_seq,
				    uint8_t lock_opcode,
				    int use_32bit_addr)
{
	int addr1_cycles = use_32bit_addr ? 16 : 8;

	/* Lock/Unlock */
	*lock_seq = (struct fsm_seq) {
		.seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
			       SEQ_OPC_CSDEASSERT),
		.seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(lock_opcode)),
		.addr_cfg = (ADR_CFG_CYCLES_ADD1(addr1_cycles) |
			     ADR_CFG_PADS_1_ADD1 |
			     ADR_CFG_CYCLES_ADD2(16) |
			     ADR_CFG_PADS_1_ADD2 |
			     ADR_CFG_CSDEASSERT_ADD2),
		.seq = {
			FSM_INST_CMD1,
			FSM_INST_CMD2,
			FSM_INST_ADD1,
			FSM_INST_ADD2,
			FSM_INST_STOP,
		},
		.seq_cfg = (SEQ_CFG_PADS_1 |
			    SEQ_CFG_READNOTWRITE |
			    SEQ_CFG_CSDEASSERT |
			    SEQ_CFG_STARTSEQ),
	};

	return 0;
}

/*
 * Configure the "BPx" Block Protection Boundaries
 *
 * The BPx Block Protection scheme allows an area of Flash to be protected from
 * Write and Erase operations.  The protected area is determined by the BPx bits
 * which define an address boundary, and the TBPROT bit which selects whether
 * protection is active below or above the boundary.  The permissible boundary
 * addresses are determined by the size of the device, the state of TBPROT, and
 * the number of boundaries supported by the device.
 *
 * Example: BP[2-0], 8 boundaries, TBPROT = 1:
 *                   __________________________________________________________
 *          Device: | |  |   |     |        |             |                    |
 *                  |_|__|___|_____|________|_____________|____________________|
 *      Protection
 *     from bottom: |--....->
 *                NONE                                                       ALL
 *                  ^ ^  ^   ^     ^        ^             ^                    ^
 * BP[2-0]:         | |  |   |     |        |             |                    |
 *   0 0 0 ( 0/64) -  |  |   |     |        |             |                    |
 *   0 0 1 ( 1/64) ---   |   |     |        |             |                    |
 *   0 1 0 ( 2/64) ------    |     |        |             |                    |
 *   0 1 1 ( 4/64) ----------      |        |             |                    |
 *   1 0 0 ( 8/64) ----------------         |             |                    |
 *   1 0 1 (16/64) -------------------------              |                    |
 *   1 1 0 (32/64) ---------------------------------------                     |
 *   1 1 1 (64/64) ------------------------------------------------------------
 *
 */
static void configure_bpx_boundaries(struct stm_spi_fsm *fsm, int tbprot,
				     uint64_t size, int n_bnds)
{
	int i;
	uint64_t bpx_size = size;

	BUG_ON(n_bnds > BPX_MAX_N_BNDS);

	fsm->bpx_tbprot = tbprot;
	fsm->bpx_n_bnds = n_bnds;

	fsm->bpx_bnds[0] = tbprot ? 0 : size;
	do_div(bpx_size, (1 << (n_bnds - 2)));

	for (i = 1; i < n_bnds; i++) {
		fsm->bpx_bnds[i] = tbprot ? bpx_size : size - bpx_size;
		bpx_size *= 2;
	}
}

/* Print the protected region for a particular value of PBx */
static void dump_bpx_region(struct stm_spi_fsm *fsm, uint8_t bpx)
{
	char bpx_str[BPX_MAX_BITS*2+1];
	uint8_t mask = 1 << (fsm->bpx_n_bits - 1);
	char *p;
	int i;

	p = bpx_str;
	for (i = 0; i < fsm->bpx_n_bits; i++) {
		*p++ = ' ';
		*p++ = (bpx & mask) ? '1' : '0';
		mask >>= 1;
	}
	*p = '\0';

	pr_info("\t%s: 0x%08llx -> 0x%08llx\n", bpx_str,
		fsm->bpx_tbprot ? 0UL : fsm->bpx_bnds[bpx],
		fsm->bpx_tbprot ? fsm->bpx_bnds[bpx] : fsm->mtd.size);
}

#ifdef CONFIG_STM_SPI_FSM_DEBUG
/* Print the lockable regions supported by the BPx scheme */
static void dump_bpx_regions(struct stm_spi_fsm *fsm)
{
	int i;

	dev_info(fsm->dev, "BP[%d-0] Lockable Regions (TBPROT = %d):\n",
		 fsm->bpx_n_bits - 1, fsm->bpx_tbprot);

	for (i = 0; i < fsm->bpx_n_bnds; i++)
		dump_bpx_region(fsm, i);
}
#endif

/* Extract BPx from the value of the status register */
static uint8_t status_to_bpx(struct stm_spi_fsm *fsm, uint8_t status)
{
	uint8_t bpx = 0;
	uint8_t mask = 1;
	int i;

	for (i = 0; i < fsm->bpx_n_bits; i++) {
		if (status & fsm->bpx_sr_masks[i])
			bpx |= mask;
		mask <<= 1;
	}

	return bpx;
}

static int fsm_read_rdsfdp(struct stm_spi_fsm *fsm, uint32_t offs,
			   uint8_t dummy_cycles, uint8_t *data, int bytes);
static int fsm_read_status(struct stm_spi_fsm *fsm, uint8_t cmd,
			   uint8_t *data, int bytes);
static int fsm_write_status(struct stm_spi_fsm *fsm, uint8_t cmd,
			    uint16_t status, int bytes, int wait_busy);
static int fsm_enter_32bitaddr(struct stm_spi_fsm *fsm, int enter);
static uint8_t fsm_wait_busy(struct stm_spi_fsm *fsm, unsigned int max_time_ms);
static int fsm_write_fifo(struct stm_spi_fsm *fsm,
			  const uint32_t *buf, const uint32_t size);
static int fsm_read_fifo(struct stm_spi_fsm *fsm,
			 uint32_t *buf, const uint32_t size);
static inline void fsm_load_seq(struct stm_spi_fsm *fsm,
				const struct fsm_seq *const seq);
static int fsm_wait_seq(struct stm_spi_fsm *fsm);

/*
 * [W25Qxxx] Configuration
 */
#define W25Q_STATUS1_TB			(0x1 << 5)
#define W25Q_STATUS1_SEC		(0x1 << 6)
#define W25Q_STATUS2_QE			(0x1 << 1)
#define W25Q_STATUS2_CMP		(0x1 << 6)
#define W25Q_STATUS3_WPS		(0x1 << 2)

#define W25Q16_DEVICE_ID		0x15
#define W25Q80_DEVICE_ID		0x14

static int w25q_config(struct stm_spi_fsm *fsm, struct flash_info *info)
{
	uint32_t data_pads;
	uint8_t sr1, sr2, sr3;
	uint16_t sr_wr;
	int update_sr;
	int wps, n_bnds, i;
	uint64_t size = info->sector_size * info->n_sectors;

	if (fsm_config_rwe_seqs_default(fsm, info) != 0)
		return 1;

	/* WRSR must always cover STATUS register 2 to prevent loss of QUAD bit
	 * and CMP bit state
	 */
	fsm->configuration |= CFG_WRSR_FORCE_16BITS;

	/* Try to enable individual block locking scheme */
	if (info->capabilities & FLASH_CAPS_BLK_LOCKING) {
		/* Get 'WPS' bit */
		fsm_read_status(fsm, FLASH_CMD_RDSR3, &sr3, 1);
		wps = (sr3 & W25Q_STATUS3_WPS) ? 1 : 0;

		/* Check if wps is not an arbitrary data */
		if (wps) {
			/* Set 'WPS' bit to 0 */
			sr3 &= ~W25Q_STATUS3_WPS;
			fsm_write_status(fsm, FLASH_CMD_WRSR3, sr3, 1, 1);

			/* Get 'WPS' bit */
			fsm_read_status(fsm, FLASH_CMD_RDSR3, &sr3, 1);
			wps = (sr3 & W25Q_STATUS3_WPS) ? 1 : 0;

			/* if 'WPS'= 1 => Individual block locking scheme
			*  not supported
			*/
			if (wps)
				info->capabilities &= ~FLASH_CAPS_BLK_LOCKING;
		}

		/* Enable individual block locking scheme */
		if (!wps) {
			/* Set 'WPS' bit to 1 */
			sr3 |= W25Q_STATUS3_WPS;
			fsm_write_status(fsm, FLASH_CMD_WRSR3, sr3, 1, 1);

			/* Get 'WPS' bit */
			fsm_read_status(fsm, FLASH_CMD_RDSR3, &sr3, 1);
			wps = (sr3 & W25Q_STATUS3_WPS) ? 1 : 0;

			/* if 'WPS'=0 => Individual block locking scheme
			*  not supported
			*/
			if (!wps)
				info->capabilities &= ~FLASH_CAPS_BLK_LOCKING;
		}
	}

	/* Configure block locking support */
	if (info->capabilities & FLASH_CAPS_BLK_LOCKING) {
		configure_block_rd_lock_seq(&fsm_seq_rd_lock_reg,
					    W25Q_CMD_RDLOCK, 0);
		configure_block_lock_seq(&fsm_seq_lock,
					 W25Q_CMD_LOCK, 0);
		configure_block_lock_seq(&fsm_seq_unlock,
					 W25Q_CMD_UNLOCK, 0);

		/* Handle 4KiB parameter sectors at the top and the bottom */
		fsm->p4k_bot_end = 16 * 0x1000;
		fsm->p4k_top_start = size - (16 * 0x1000);

		fsm->lock_mask = 0x1;
		fsm->lock_val[FSM_BLOCK_UNLOCKED] = 0x0;
		fsm->lock_val[FSM_BLOCK_LOCKED] = 0x1;
	} else if (info->capabilities & FLASH_CAPS_BPX_LOCKING) {
		int tbprot;
		int secprot;
		int cmpprot;

		/* Get 'TB' and 'SEC' bits */
		fsm_read_status(fsm, FLASH_CMD_RDSR, &sr1, 1);
		tbprot = (sr1 & W25Q_STATUS1_TB) ? 1 : 0;
		secprot = (sr1 & W25Q_STATUS2_CMP) ? 1 : 0;

		/* Get 'CMP' bit */
		fsm_read_status(fsm, FLASH_CMD_RDSR2, &sr2, 1);
		cmpprot = (sr2 & W25Q_STATUS2_CMP) ? 1 : 0;

		if (cmpprot || secprot) {
			/* This scheme is not supported */
			dev_warn(fsm->dev,
				"Lock/unlock scheme not supported."
				"Only schemes whith CMP=0 and SEC=0 "
				"is supported.\n");

			/* disable BPx locking support */
			info->capabilities &= ~FLASH_CAPS_BPX_LOCKING;
		} else {
			/* Configure BPx bits */
			fsm->bpx_n_bits = 3;
			fsm->bpx_sr_masks[0] = FLASH_STATUS_BP0;
			fsm->bpx_sr_masks[1] = FLASH_STATUS_BP1;
			fsm->bpx_sr_masks[2] = FLASH_STATUS_BP2;

			/* Configure BPx boundaries: 8 levels from top/bottom */
			if (info->readid[2] == W25Q16_DEVICE_ID)
				n_bnds = 7;
			else if (info->readid[2] == W25Q80_DEVICE_ID)
				n_bnds = 6;
			else
				n_bnds = 8;

			configure_bpx_boundaries(fsm, tbprot, size, n_bnds);

			for (i = n_bnds; i < 8; i++)
				fsm->bpx_bnds[i] = fsm->bpx_bnds[n_bnds - 1];
		}
	}

	/* Check status of 'QE' bit, update if required. */
	data_pads = ((fsm_seq_read.seq_cfg >> 16) & 0x3) + 1;
	fsm_read_status(fsm, FLASH_CMD_RDSR2, &sr2, 1);
	update_sr = 0;
	if (data_pads == 4) {
		if (!(sr2 & W25Q_STATUS2_QE)) {
			/* Set 'QE' */
			sr2 |= W25Q_STATUS2_QE;
			update_sr = 1;
		}
	} else {
		if (sr2 &  W25Q_STATUS2_QE) {
			/* Clear 'QE' */
			sr2 &= ~W25Q_STATUS2_QE;
			update_sr = 1;
		}
	}
	if (update_sr) {
		/* Write status register */
		fsm_read_status(fsm, FLASH_CMD_RDSR, &sr1, 1);
		sr_wr = ((uint16_t)sr2 << 8) | sr1;
		fsm_write_status(fsm, FLASH_CMD_WRSR, sr_wr, 2, 1);
	}

#ifdef CONFIG_STM_SPI_FSM_DEBUG
	/* Debug strings for W25Qxxx specific commands */
	flash_cmd_strs[W25Q_CMD_RDLOCK]	= "RDLOCK";
	flash_cmd_strs[W25Q_CMD_LOCK] = "LOCK";
	flash_cmd_strs[W25Q_CMD_UNLOCK] = "UNLOCK";
#endif

	return 0;
}

/*
 *[S25FLxxx] Configuration
 */
#define S25FL_CONFIG_QE			(0x1 << 1)
#define S25FL_CONFIG_TBPROT		(0x1 << 5)
#define S25FL1XXK_DEVICE_TYPE		0x40

/*
 * S25FLxxxS devices provide three ways of supporting 32-bit addressing: Bank
 * Register, Extended Address Modes, and a 32-bit address command set.  The
 * 32-bit address command set is used here, since it avoids any problems with
 * entering a state that is incompatible with the SPIBoot Controller.
 */
static struct seq_rw_config s25fl_read4_configs[] = {
	{FLASH_CAPS_READ_1_4_4,  FLASH_CMD_READ4_1_4_4,  0, 4, 4, 0x00, 2, 4},
	{FLASH_CAPS_READ_1_1_4,  FLASH_CMD_READ4_1_1_4,  0, 1, 4, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_2_2,  FLASH_CMD_READ4_1_2_2,  0, 2, 2, 0x00, 4, 0},
	{FLASH_CAPS_READ_1_1_2,  FLASH_CMD_READ4_1_1_2,  0, 1, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_FAST,   FLASH_CMD_READ4_FAST,   0, 1, 1, 0x00, 0, 8},
	{FLASH_CAPS_READ_WRITE,  FLASH_CMD_READ4,        0, 1, 1, 0x00, 0, 0},
	{0x00,                   0,                      0, 0, 0, 0x00, 0, 0},
};

static struct seq_rw_config s25fl_write4_configs[] = {
	{FLASH_CAPS_WRITE_1_1_4, S25FL_CMD_WRITE4_1_1_4, 1, 1, 4, 0x00, 0, 0},
	{FLASH_CAPS_READ_WRITE,  S25FL_CMD_WRITE4,       1, 1, 1, 0x00, 0, 0},
	{0x00,                   0,                      0, 0, 0, 0x00, 0, 0},
};

static int s25fl_configure_erasesec_seq_32(struct fsm_seq *seq)
{
	seq->seq_opc[1] = (SEQ_OPC_PADS_1 |
			   SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(S25FL_CMD_SE4));

	seq->addr_cfg = (ADR_CFG_CYCLES_ADD1(16) |
			 ADR_CFG_PADS_1_ADD1 |
			 ADR_CFG_CYCLES_ADD2(16) |
			 ADR_CFG_PADS_1_ADD2 |
			 ADR_CFG_CSDEASSERT_ADD2);
	return 0;
}

static int s25fl_clear_status_reg(struct stm_spi_fsm *fsm)
{
	struct fsm_seq seq = {
		.seq_opc[0] = (SEQ_OPC_PADS_1 |
			       SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(S25FL_CMD_CLSR) |
			       SEQ_OPC_CSDEASSERT),
		.seq_opc[1] = (SEQ_OPC_PADS_1 |
			       SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(FLASH_CMD_WRDI) |
			       SEQ_OPC_CSDEASSERT),
		.seq = {
			FSM_INST_CMD1,
			FSM_INST_CMD2,
			FSM_INST_WAIT,
			FSM_INST_STOP,
		},
		.seq_cfg = (SEQ_CFG_PADS_1 |
			    SEQ_CFG_ERASE |
			    SEQ_CFG_READNOTWRITE |
			    SEQ_CFG_CSDEASSERT |
			    SEQ_CFG_STARTSEQ),
	};

	fsm_load_seq(fsm, &seq);

	fsm_wait_seq(fsm);

	return 0;
}

static int s25fl_config(struct stm_spi_fsm *fsm, struct flash_info *info)
{
	uint32_t data_pads;
	uint8_t sr1, cr1;
	uint16_t sta_wr;
	int update_sr;
	uint64_t size = info->sector_size * info->n_sectors;

	if (info->capabilities & FLASH_CAPS_32BITADDR) {
		/*
		 * Configure Read/Write/Erase sequences according to S25FLxxx
		 * 32-bit address command set
		 */
		if (fsm_search_configure_rw_seq(fsm, &fsm_seq_read,
						s25fl_read4_configs,
						info->capabilities) != 0)
			return 1;

		if (fsm_search_configure_rw_seq(fsm, &fsm_seq_write,
						s25fl_write4_configs,
						info->capabilities) != 0)
			return 1;
		if (s25fl_configure_erasesec_seq_32(&fsm_seq_erase_sector) != 0)
			return 1;

	} else {
		/* Use default configurations for 24-bit addressing */
		if (fsm_config_rwe_seqs_default(fsm, info) != 0)
			return 1;
	}

	/* WRSR must always cover CONFIG register to prevent loss of QUAD bit
	 * state
	 */
	fsm->configuration |= CFG_WRSR_FORCE_16BITS;

	/*
	 * S25FLxxx devices support Program and Error error flags, with the
	 * exception of the S25FL1xxK family.
	 */
	if (info->readid[1] != S25FL1XXK_DEVICE_TYPE)
		fsm->configuration |= CFG_S25FL_CHECK_ERROR_FLAGS;

	/* Configure block locking support */
	if (info->capabilities & FLASH_CAPS_BLK_LOCKING) {
		configure_block_rd_lock_seq(&fsm_seq_rd_lock_reg,
					    S25FL_CMD_DYBRD, 1);
		configure_block_wr_lock_seq(&fsm_seq_wr_lock_reg,
					    S25FL_CMD_DYBWR, 1);

		/* S25FLxxx are using a read register and write register command
		 * to lock/unlock blocks/sectors
		 */
		fsm->configuration |= CFG_RD_WR_LOCK_REG;

		/* Handle 4KiB parameter sectors: catch-all approach to
		 * accommodate all variants (e.g. top vs. bottom and 16 vs. 32
		 * parameter sectors).
		 */
		fsm->p4k_bot_end = 32 * 0x1000;
		fsm->p4k_top_start = size - (32 * 0x1000);

		fsm->lock_mask = 0xff;
		fsm->lock_val[FSM_BLOCK_UNLOCKED] = 0xff;
		fsm->lock_val[FSM_BLOCK_LOCKED] = 0x00;
	} else if (info->capabilities & FLASH_CAPS_BPX_LOCKING) {
		int tbprot;

		/* Get 'TBPROT' bit */
		fsm_read_status(fsm, FLASH_CMD_RDSR2, &cr1, 1);
		tbprot = (cr1 & S25FL_CONFIG_TBPROT) ? 1 : 0;

		/* Configure BPx bits */
		fsm->bpx_n_bits = 3;
		fsm->bpx_sr_masks[0] = FLASH_STATUS_BP0;
		fsm->bpx_sr_masks[1] = FLASH_STATUS_BP1;
		fsm->bpx_sr_masks[2] = FLASH_STATUS_BP2;

		/* Configure BPx boundaries: 8 levels from top/bottom */
		configure_bpx_boundaries(fsm, tbprot, size, 8);
	}

	/* Check status of 'QE' bit, update if required. */
	data_pads = ((fsm_seq_read.seq_cfg >> 16) & 0x3) + 1;
	fsm_read_status(fsm, FLASH_CMD_RDSR2, &cr1, 1);
	update_sr = 0;
	if (data_pads == 4) {
		if (!(cr1 & S25FL_CONFIG_QE)) {
			/* Set 'QE' */
			cr1 |= S25FL_CONFIG_QE;
			update_sr = 1;
		}
	} else {
		if (cr1 & S25FL_CONFIG_QE) {
			/* Clear 'QE' */
			cr1 &= ~S25FL_CONFIG_QE;
			update_sr = 1;
		}
	}
	if (update_sr) {
		fsm_read_status(fsm, FLASH_CMD_RDSR, &sr1, 1);
		sta_wr = ((uint16_t)cr1  << 8) | sr1;
		fsm_write_status(fsm, FLASH_CMD_WRSR, sta_wr, 2, 1);
	}

#ifdef CONFIG_STM_SPI_FSM_DEBUG
	/* Debug strings for S25FLxxx specific commands */
	flash_cmd_strs[S25FL_CMD_WRITE4]	= "WRITE4";
	flash_cmd_strs[S25FL_CMD_WRITE4_1_1_4]	= "WRITE4_1_1_4";
	flash_cmd_strs[S25FL_CMD_SE4]		= "SE4";
	flash_cmd_strs[S25FL_CMD_CLSR]		= "CLSR";
	flash_cmd_strs[S25FL_CMD_DYBWR]		= "DYBWR";
	flash_cmd_strs[S25FL_CMD_DYBRD]		= "DYBRD";
#endif
	return 0;
}

/* [MX25xxx] Configure READ/Write sequences */
#define MX25_STATUS_QE			(0x1 << 6)
#define MX25_SECURITY_WPSEL		(0x1 << 7)
#define MX25_CONFIG_TB			(0x1 << 3)

#define MX25L32_DEVICE_ID		0x16
#define MX25L128_DEVICE_ID		0x18

/* mx25 WRITE configurations, in order of preference */
static struct seq_rw_config mx25_write_configs[] = {
	{FLASH_CAPS_WRITE_1_4_4, MX25_CMD_WRITE_1_4_4,  1, 4, 4, 0x00, 0, 0},
	{FLASH_CAPS_READ_WRITE,  FLASH_CMD_WRITE,       1, 1, 1, 0x00, 0, 0},

	/* terminating entry */
	{0x00,			  0,			 0, 0, 0, 0x00, 0, 0},
};

static int mx25_configure_en32bitaddr_seq(struct fsm_seq *seq)
{
	seq->seq_opc[0] = (SEQ_OPC_PADS_1 |
			   SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_EN4B_ADDR) |
			   SEQ_OPC_CSDEASSERT);

	seq->seq[0] = FSM_INST_CMD1;
	seq->seq[1] = FSM_INST_WAIT;
	seq->seq[2] = FSM_INST_STOP;

	seq->seq_cfg = (SEQ_CFG_PADS_1 |
			SEQ_CFG_ERASE |
			SEQ_CFG_READNOTWRITE |
			SEQ_CFG_CSDEASSERT |
			SEQ_CFG_STARTSEQ);

	return 0;
}

static int mx25_config(struct stm_spi_fsm *fsm, struct flash_info *info)
{
	uint8_t	rdsfdp[2];
	uint32_t data_pads;
	uint32_t capabilities = info->capabilities;
	uint8_t sta, lock_opcode, cr;
	int tb, n_bnds, i;
	uint64_t size = info->sector_size * info->n_sectors;

	/* Configure 'READ' sequence */
	if (fsm_search_configure_rw_seq(fsm, &fsm_seq_read,
					default_read_configs,
					capabilities) != 0) {
		dev_err(fsm->dev, "failed to configure READ sequence according to capabilities [0x%08x]\n",
			capabilities);
		return 1;
	}

	/* Configure 'WRITE' sequence */
	if (fsm_search_configure_rw_seq(fsm, &fsm_seq_write,
					mx25_write_configs,
					capabilities) != 0) {
		dev_err(fsm->dev, "failed to configure WRITE sequence according to capabilities [0x%08x]\n",
			capabilities);
		return 1;
	}

	/* Configure 'ERASE_SECTOR' sequence */
	configure_erasesec_seq(&fsm_seq_erase_sector,
			       capabilities & FLASH_CAPS_32BITADDR);

	/*
	 * Configure 32-bit Address Support
	 */
	if (info->capabilities & FLASH_CAPS_32BITADDR) {
		/* Configure 'enter_32bitaddr' FSM sequence */
		mx25_configure_en32bitaddr_seq(&fsm_seq_en32bitaddr);

		if (!fsm->capabilities.boot_from_spi ||
		    can_handle_soc_reset(fsm))
			/* If we can handle SoC resets, we enable 32-bit address
			 * mode pervasively */
			fsm_enter_32bitaddr(fsm, 1);
		else
			/* Else, enable/disable 32-bit addressing before/after
			 * each operation */
			fsm->configuration = (CFG_READ_TOGGLE32BITADDR |
					      CFG_WRITE_TOGGLE32BITADDR |
					      CFG_ERASESEC_TOGGLE32BITADDR);
	}

	/*
	 * Check WPSEL
	 * WPSEL = 0 => Block Lock protection mode
	 * WPSEL = 1 => Individual block lock protection mode
	 */
	fsm_read_status(fsm, MX25_CMD_RDSCUR, &sta, 1);
	if (sta & MX25_SECURITY_WPSEL) {
		/* Individual block lock protection mode detected */

		/*
		 * Read opcode used to lock
		 * Read SFDP mode at @0x68, 8 dummy cycles required
		 * bits 2-9 => opcode used
		 */
		fsm_read_rdsfdp(fsm, 0x68, 8, rdsfdp, 2);
		lock_opcode = ((rdsfdp[1] & 0x03) << 6) |
			      ((rdsfdp[0] & 0xfc) >> 2);

		if (lock_opcode == MX25_CMD_SBLK) {
			info->capabilities |= FLASH_CAPS_BLK_LOCKING;

			configure_block_rd_lock_seq(&fsm_seq_rd_lock_reg,
						    MX25_CMD_RDBLOCK, 0);
			configure_block_lock_seq(&fsm_seq_lock,
						 MX25_CMD_SBLK, 0);
			configure_block_lock_seq(&fsm_seq_unlock,
						 MX25_CMD_SBULK, 0);

			/*
			 * Handle 4KiB parameter sectors
			 * at the top and the bottom
			 */
			fsm->p4k_bot_end = 16 * 0x1000;
			fsm->p4k_top_start = size - (16 * 0x1000);

			fsm->lock_mask = 0xff;
			fsm->lock_val[FSM_BLOCK_UNLOCKED] = 0x00;
			fsm->lock_val[FSM_BLOCK_LOCKED] = 0xff;
		} else if (lock_opcode == MX25_CMD_WRDPB) {
			info->capabilities |= FLASH_CAPS_BLK_LOCKING;

			configure_block_rd_lock_seq(&fsm_seq_rd_lock_reg,
						    MX25_CMD_RDDPB, 1);
			configure_block_wr_lock_seq(&fsm_seq_wr_lock_reg,
						    MX25_CMD_WRDPB, 1);

			fsm->configuration |= CFG_RD_WR_LOCK_REG;

			/*
			 *Handle 4KiB parameter sectors
			 * at the top and the bottom
			 */
			fsm->p4k_bot_end = 16 * 0x1000;
			fsm->p4k_top_start = size - (16 * 0x1000);

			fsm->lock_mask = 0xff;
			fsm->lock_val[FSM_BLOCK_UNLOCKED] = 0x00;
			fsm->lock_val[FSM_BLOCK_LOCKED] = 0xff;
		} else
			/* Lock opcode is not supported */
			dev_warn(fsm->dev,
				 "Lock/unlock command %02x not supported.\n",
				 lock_opcode);
	} else {
		/* BP lock lock protection mode detected */
		info->capabilities |= FLASH_CAPS_BPX_LOCKING;

		/* Get 'tb' bit */
		fsm_read_status(fsm, MX25_CMD_RDCR, &cr, 1);
		tb = (cr & MX25_CONFIG_TB) ? 1 : 0;

		/* Configure BPx bits */
		fsm->bpx_n_bits = 4;
		fsm->bpx_sr_masks[0] = FLASH_STATUS_BP0;
		fsm->bpx_sr_masks[1] = FLASH_STATUS_BP1;
		fsm->bpx_sr_masks[2] = FLASH_STATUS_BP2;
		fsm->bpx_sr_masks[3] = FLASH_STATUS_BP3;

		/* Configure BPx boundaries: 16 levels from top/bottom */
		if (info->readid[2] == MX25L32_DEVICE_ID)
			n_bnds = 8;
		else if (info->readid[2] == MX25L128_DEVICE_ID)
			n_bnds = 10;
		else
			n_bnds = 11;

		configure_bpx_boundaries(fsm, tb, size, n_bnds);

		for (i = n_bnds; i < 16; i++)
			fsm->bpx_bnds[i] = fsm->bpx_bnds[n_bnds - 1];
	}

	/* Check status of 'QE' bit, update if required. */
	data_pads = ((fsm_seq_read.seq_cfg >> 16) & 0x3) + 1;
	fsm_read_status(fsm, FLASH_CMD_RDSR, &sta, 1);
	if (data_pads == 4) {
		if (!(sta & MX25_STATUS_QE)) {
			/* Set 'QE' */
			sta |= MX25_STATUS_QE;
			fsm_write_status(fsm, FLASH_CMD_WRSR, sta, 1, 1);
		}

		/* impossible to lock/unlock mx25l3255e if QE bit is set */
		fsm->configuration |= CFG_MX25_LOCK_TOGGLE_QE_BIT;
	} else {
		if (sta & MX25_STATUS_QE) {
			/* Clear 'QE' */
			sta &= ~MX25_STATUS_QE;
			fsm_write_status(fsm, FLASH_CMD_WRSR, sta, 1, 1);
		}
	}

#ifdef CONFIG_STM_SPI_FSM_DEBUG
	/* Debug strings for MX25xxx specific commands */
	flash_cmd_strs[MX25_CMD_RDSCUR]	= "RDSCUR";
#endif

	return 0;
}

static int mx25_resume(struct stm_spi_fsm *fsm)
{
	/*
	 * Configure 32-bit Address Support
	 */
	if (fsm->mtd.size > 0x1000000) {
		if (!fsm->capabilities.boot_from_spi ||
		    can_handle_soc_reset(fsm))
			/* If we can handle SoC resets, we enable 32-bit address
			 * mode pervasively */
			fsm_enter_32bitaddr(fsm, 1);
	}

	return 0;
}

/*
 * [N25Qxxx] Configuration
 */
#define N25Q_VCR_DUMMY_CYCLES(x)	(((x) & 0xf) << 4)
#define N25Q_VCR_XIP_DISABLED		((uint8_t)0x1 << 3)
#define N25Q_VCR_WRAP_CONT		0x3

/* N25Q 3-byte Address READ configurations
 *	- 'FAST' variants configured for 8 dummy cycles.
 *
 * Note, the number of dummy cycles used for 'FAST' READ operations is
 * configurable and would normally be tuned according to the READ command and
 * operating frequency.  However, this applies universally to all 'FAST' READ
 * commands, including those used by the SPIBoot controller, and remains in
 * force until the device is power-cycled.  Since the SPIBoot controller is
 * hard-wired to use 8 dummy cycles, we must configure the device to also use 8
 * cycles.
 */
static struct seq_rw_config n25q_read3_configs[] = {
	{FLASH_CAPS_READ_1_4_4, FLASH_CMD_READ_1_4_4,	0, 4, 4, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_1_4, FLASH_CMD_READ_1_1_4,	0, 1, 4, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_2_2, FLASH_CMD_READ_1_2_2,	0, 2, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_1_2, FLASH_CMD_READ_1_1_2,	0, 1, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_FAST,	FLASH_CMD_READ_FAST,	0, 1, 1, 0x00, 0, 8},
	{FLASH_CAPS_READ_WRITE, FLASH_CMD_READ,	        0, 1, 1, 0x00, 0, 0},
	{0x00,			 0,			0, 0, 0, 0x00, 0, 0},
};

/* N25Q 4-byte Address READ configurations
 *	- use special 4-byte address READ commands (reduces overheads, and
 *        reduces risk of hitting watchdog resets issues).
 *	- 'FAST' variants configured for 8 dummy cycles (see note above.)
 */
static struct seq_rw_config n25q_read4_configs[] = {
	{FLASH_CAPS_READ_1_4_4, FLASH_CMD_READ4_1_4_4,	0, 4, 4, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_1_4, FLASH_CMD_READ4_1_1_4,	0, 1, 4, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_2_2, FLASH_CMD_READ4_1_2_2,	0, 2, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_1_1_2, FLASH_CMD_READ4_1_1_2,	0, 1, 2, 0x00, 0, 8},
	{FLASH_CAPS_READ_FAST,	FLASH_CMD_READ4_FAST,	0, 1, 1, 0x00, 0, 8},
	{FLASH_CAPS_READ_WRITE, FLASH_CMD_READ4,	0, 1, 1, 0x00, 0, 0},
	{0x00,			 0,			0, 0, 0, 0x00, 0, 0},
};

static int n25q_configure_en32bitaddr_seq(struct fsm_seq *seq)
{
	seq->seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_EN4B_ADDR));
	seq->seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_WREN) |
			   SEQ_OPC_CSDEASSERT);

	seq->seq[0] = FSM_INST_CMD2;
	seq->seq[1] = FSM_INST_CMD1;
	seq->seq[2] = FSM_INST_WAIT;
	seq->seq[3] = FSM_INST_STOP;

	seq->seq_cfg = (SEQ_CFG_PADS_1 |
			SEQ_CFG_ERASE |
			SEQ_CFG_READNOTWRITE |
			SEQ_CFG_CSDEASSERT |
			SEQ_CFG_STARTSEQ);

	return 0;
}

static int n25q_clear_flags(struct stm_spi_fsm *fsm)
{
	struct fsm_seq seq = {
		.seq_opc[0] = (SEQ_OPC_PADS_1 |
			       SEQ_OPC_CYCLES(8) |
			       SEQ_OPC_OPCODE(N25Q_CMD_CLFSR) |
			       SEQ_OPC_CSDEASSERT),
		.seq = {
			FSM_INST_CMD1,
			FSM_INST_STOP,
		},
		.seq_cfg = (SEQ_CFG_PADS_1 |
			    SEQ_CFG_READNOTWRITE |
			    SEQ_CFG_CSDEASSERT |
			    SEQ_CFG_STARTSEQ),
	};

	fsm_load_seq(fsm, &seq);

	fsm_wait_seq(fsm);

	return 0;
}

static int n25q_config(struct stm_spi_fsm *fsm, struct flash_info *info)
{
	uint8_t vcr, sta;
	int ret = 0;

	/*
	 * Configure 'READ' sequence
	 */
	if (info->capabilities & FLASH_CAPS_32BITADDR)
		/* 32-bit addressing supported by N25Q 'READ4' commands */
		ret = fsm_search_configure_rw_seq(fsm, &fsm_seq_read,
						  n25q_read4_configs,
						  info->capabilities);
	else
		/* 24-bit addressing with 8 dummy cycles */
		ret = fsm_search_configure_rw_seq(fsm, &fsm_seq_read,
						  n25q_read3_configs,
						  info->capabilities);

	if (ret != 0) {
		dev_err(fsm->dev, "failed to configure READ sequence according to capabilities [0x%08x]\n",
			info->capabilities);
		return 1;
	}


	/*
	 * Configure 'WRITE' sequence (use default configs)
	 */
	ret = fsm_search_configure_rw_seq(fsm, &fsm_seq_write,
					  default_write_configs,
					  info->capabilities);
	if (ret != 0) {
		dev_err(fsm->dev, "failed to configure WRITE sequence according to capabilities [0x%08x]\n",
			info->capabilities);
		return 1;
	}

	/*
	 * Configure 'ERASE_SECTOR' sequence
	 */
	configure_erasesec_seq(&fsm_seq_erase_sector,
			       info->capabilities & FLASH_CAPS_32BITADDR);

	/*
	 * Configure block locking
	 */
	if (info->capabilities & FLASH_CAPS_BLK_LOCKING) {
		int use_32bit_addr = info->capabilities & FLASH_CAPS_32BITADDR;
		configure_block_rd_lock_seq(&fsm_seq_rd_lock_reg,
					    N25Q_CMD_RDLOCK, use_32bit_addr);
		configure_block_wr_lock_seq(&fsm_seq_wr_lock_reg,
					    N25Q_CMD_WRLOCK, use_32bit_addr);

		/* N25Qxxx are using a read register and write register command
		 * to lock/unlock blocks.
		 */
		fsm->configuration |= CFG_RD_WR_LOCK_REG;

		fsm->p4k_bot_end = 0;
		fsm->p4k_top_start = info->sector_size * info->n_sectors;

		fsm->lock_mask = 0x1;
		fsm->lock_val[FSM_BLOCK_UNLOCKED] = 0x0;
		fsm->lock_val[FSM_BLOCK_LOCKED] = 0x1;
	}


	/*
	 * Configure 32-bit address support
	 */
	if (info->capabilities & FLASH_CAPS_32BITADDR) {
		/* Configure 'enter_32bitaddr' FSM sequence */
		n25q_configure_en32bitaddr_seq(&fsm_seq_en32bitaddr);

		if (!fsm->capabilities.boot_from_spi ||
		    can_handle_soc_reset(fsm)) {
			/* If we can handle SoC resets, we enable 32-bit address
			 * mode pervasively */
			fsm_enter_32bitaddr(fsm, 1);
		} else {
			/* Else, enable/disable for WRITE and ERASE operations
			 * (READ uses special commands) */
			fsm->configuration |= (CFG_WRITE_TOGGLE32BITADDR |
					      CFG_ERASESEC_TOGGLE32BITADDR |
					      CFG_LOCK_TOGGLE32BITADDR);
		}
	}

	/*
	 * Check/Clear Error Flags
	 */
	fsm->configuration |= CFG_N25Q_CHECK_ERROR_FLAGS;
	fsm_read_status(fsm, N25Q_CMD_RFSR, &sta, 1);
	if (sta & N25Q_FLAGS_ERROR)
		n25q_clear_flags(fsm);

	/*
	 * Configure device to use 8 dummy cycles
	 */
	vcr = (N25Q_VCR_DUMMY_CYCLES(8) | N25Q_VCR_XIP_DISABLED |
	       N25Q_VCR_WRAP_CONT);
	fsm_write_status(fsm, N25Q_CMD_WRVCR, vcr, 1, 0);

#ifdef CONFIG_STM_SPI_FSM_DEBUG
	/* Debug strings for N25Qxxx specific commands */
	flash_cmd_strs[N25Q_CMD_RFSR]	= "RFSR";
	flash_cmd_strs[N25Q_CMD_CLFSR]	= "CLRFSR";
	flash_cmd_strs[N25Q_CMD_RDVCR]	= "RDVCR";
	flash_cmd_strs[N25Q_CMD_RDVECR]	= "RDVECR";
	flash_cmd_strs[N25Q_CMD_WRVCR]	= "WRVCR";
	flash_cmd_strs[N25Q_CMD_RDNVCR]	= "RDNVCR";
	flash_cmd_strs[N25Q_CMD_WRNVCR]	= "WRNVCR";
	flash_cmd_strs[N25Q_CMD_RDLOCK] = "RDLOCK";
	flash_cmd_strs[N25Q_CMD_WRLOCK] = "WRLOCK";
#endif

	return ret;
}

static int n25q_resume(struct stm_spi_fsm *fsm)
{
	uint8_t vcr, sta;

	/*
	 * Configure 32-bit address support
	 */
	if (fsm->mtd.size > 0x1000000) {
		if (!fsm->capabilities.boot_from_spi ||
		    can_handle_soc_reset(fsm))
			/* If we can handle SoC resets, we enable 32-bit address
			 * mode pervasively */
			fsm_enter_32bitaddr(fsm, 1);
	}

	/*
	 * Check/Clear Error Flags
	 */
	fsm_read_status(fsm, N25Q_CMD_RFSR, &sta, 1);
	if (sta & N25Q_FLAGS_ERROR)
		n25q_clear_flags(fsm);

	/*
	 * Configure device to use 8 dummy cycles
	 */
	vcr = (N25Q_VCR_DUMMY_CYCLES(8) | N25Q_VCR_XIP_DISABLED |
	       N25Q_VCR_WRAP_CONT);
	fsm_write_status(fsm, N25Q_CMD_WRVCR, vcr, 1, 0);

	return 0;
}

/* TCH_FIX_BEGIN : PF 24/10/2014  export read/write  status register functions for Flash OTP */
static struct stm_spi_fsm *m25p_flash;

uint8_t m25p_read_sr(void)
{
    uint8_t ret = -1;
    if (m25p_flash) {
        fsm_read_status(m25p_flash, FLASH_CMD_RDSR, &ret, 1);
    }
    return ret;
}
EXPORT_SYMBOL(m25p_read_sr);

int m25p_write_sr(u8 val)
{
    int ret = -1;
    if (m25p_flash) {
	ret = fsm_write_status(m25p_flash, FLASH_CMD_WRSR, val, 1, 1);
    }
    return ret;
}
EXPORT_SYMBOL(m25p_write_sr);
/* TCH_FIX_BEGIN : PF 24/10/2014  */

/*
 * FSM interface
 */
static inline int fsm_is_idle(struct stm_spi_fsm *fsm)
{
	return readl(fsm->base + SPI_FAST_SEQ_STA) & 0x10;
}

static inline uint32_t fsm_fifo_available(struct stm_spi_fsm *fsm)
{
	return (readl(fsm->base + SPI_FAST_SEQ_STA) >> 5) & 0x7f;
}

static inline void fsm_load_seq(struct stm_spi_fsm *fsm,
				const struct fsm_seq *const seq)
{
	int words = FSM_SEQ_SIZE/sizeof(uint32_t);
	const uint32_t *src = (const uint32_t *)seq;
	void __iomem *dst = fsm->base + SPI_FAST_SEQ_TRANSFER_SIZE;

	BUG_ON(!fsm_is_idle(fsm));

	while (words--) {
		writel(*src, dst);
		src++;
		dst += 4;
	}
}

static int fsm_wait_seq(struct stm_spi_fsm *fsm)
{
	unsigned long deadline = jiffies +
		msecs_to_jiffies(FSM_MAX_WAIT_SEQ_MS);
	int timeout = 0;

	while (!timeout) {
		if (time_after_eq(jiffies, deadline))
			timeout = 1;

		if (fsm_is_idle(fsm))
			return 0;

		cond_resched();
	}

	dev_err(fsm->dev, "timeout on sequence completion\n");

	return 1;
}

/* Dummy sequence to read one byte of data from flash into the FIFO */
static const struct fsm_seq fsm_seq_load_fifo_byte = {
	.data_size = TRANSFER_SIZE(1),
	.seq_opc[0] = (SEQ_OPC_PADS_1 |
		       SEQ_OPC_CYCLES(8) |
		       SEQ_OPC_OPCODE(FLASH_CMD_RDID)),
	.seq = {
		FSM_INST_CMD1,
		FSM_INST_DATA_READ,
		FSM_INST_STOP,
	},
	.seq_cfg = (SEQ_CFG_PADS_1 |
		    SEQ_CFG_READNOTWRITE |
		    SEQ_CFG_CSDEASSERT |
		    SEQ_CFG_STARTSEQ),
};

/*
 * Clear the data FIFO
 *
 * Typically, this is only required during driver initialisation, where no
 * assumptions can be made regarding the state of the FIFO.
 *
 * The process of clearing the FIFO is complicated by fact that while it is
 * possible for the FIFO to contain an arbitrary number of bytes [1], the
 * SPI_FAST_SEQ_STA register only reports the number of complete 32-bit words
 * present.  Furthermore, data can only be drained from the FIFO by reading
 * complete 32-bit words.
 *
 * With this in mind, a two stage process is used to the clear the FIFO:
 *
 *     1. Read any complete 32-bit words from the FIFO, as reported by the
 *        SPI_FAST_SEQ_STA register.
 *
 *     2. Mop up any remaining bytes.  At this point, it is not known if there
 *        are 0, 1, 2, or 3 bytes in the FIFO.  To handle all cases, a dummy FSM
 *        sequence is used to load one byte at a time, until a complete 32-bit
 *        word is formed; at most, 4 bytes will need to be loaded.
 *
 * [1] It is theoretically possible for the FIFO to contain an arbitrary number
 *     of bits.  However, since there are no known use-cases that leave
 *     incomplete bytes in the FIFO, only words and bytes are considered here.
 */
static int fsm_clear_fifo(struct stm_spi_fsm *fsm)
{
	const struct fsm_seq *seq = &fsm_seq_load_fifo_byte;
	uint32_t words;
	int i;

	/* 1. Clear any 32-bit words */
	words = fsm_fifo_available(fsm);
	if (words) {
		for (i = 0; i < words; i++)
			readl(fsm->base + SPI_FAST_SEQ_DATA_REG);
		dev_dbg(fsm->dev, "cleared %d words from FIFO\n", words);
	}

	/* 2. Clear any remaining bytes
	 *    - Load the FIFO, one byte at a time, until a complete 32-bit word
	 *      is available.
	 */
	for (i = 0, words = 0; i < 4 && !words; i++) {
		fsm_load_seq(fsm, seq);
		fsm_wait_seq(fsm);
		words = fsm_fifo_available(fsm);
	}

	/*    - A single word must be available now */
	if (words != 1) {
		dev_err(fsm->dev, "failed to clear bytes from the data FIFO\n");
		return 1;
	}

	/*    - Read the 32-bit word */
	readl(fsm->base + SPI_FAST_SEQ_DATA_REG);

	dev_dbg(fsm->dev, "cleared %d byte(s) from the data FIFO\n", 4 - i);

	return 0;
}

static int fsm_read_fifo(struct stm_spi_fsm *fsm,
			 uint32_t *buf, const uint32_t size)
{
	uint32_t avail;
	uint32_t remaining = size >> 2;
	uint32_t words;

	dev_dbg(fsm->dev, "reading %d bytes from FIFO\n", size);

	BUG_ON((((uint32_t)buf) & 0x3) || (size & 0x3));

	while (remaining) {
		while (!(avail = fsm_fifo_available(fsm)))
			;
		words = min(avail, remaining);
		remaining -= words;

		readsl(fsm->base + SPI_FAST_SEQ_DATA_REG, buf, words);
		buf += words;

	};

	return size;
}

static int fsm_write_fifo(struct stm_spi_fsm *fsm,
			  const uint32_t *buf, const uint32_t size)
{
	uint32_t words = size >> 2;

	dev_dbg(fsm->dev, "writing %d bytes to FIFO\n", size);

	BUG_ON((((uint32_t)buf) & 0x3) || (size & 0x3));

	writesl(fsm->base + SPI_FAST_SEQ_DATA_REG, buf, words);

	return size;
}

/*
 * Serial Flash operations
 */
static uint8_t fsm_wait_busy(struct stm_spi_fsm *fsm, unsigned int max_time_ms)
{
	struct fsm_seq *seq = &fsm_seq_read_status_fifo;
	unsigned long deadline;
	uint32_t status;
	int timeout = 0;

	/* Use RDRS1 */
	seq->seq_opc[0] = (SEQ_OPC_PADS_1 |
			   SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(FLASH_CMD_RDSR));

	/* Load read_status sequence */
	fsm_load_seq(fsm, seq);

	/*
	 * Repeat until busy bit is deasserted, or timeout, or error (S25FLxxxS)
	 */
	deadline = jiffies + msecs_to_jiffies(max_time_ms);
	while (!timeout) {
		cond_resched();

		if (time_after_eq(jiffies, deadline))
			timeout = 1;

		fsm_wait_seq(fsm);

		fsm_read_fifo(fsm, &status, 4);

		if ((status & FLASH_STATUS_BUSY) == 0)
			return 0;

		if ((fsm->configuration & CFG_S25FL_CHECK_ERROR_FLAGS) &&
		    ((status & S25FL_STATUS_P_ERR) ||
		     (status & S25FL_STATUS_E_ERR)))
			return (uint8_t)(status & 0xff);

		if (!timeout)
			/* Restart */
			writel(seq->seq_cfg, fsm->base + SPI_FAST_SEQ_CFG);
	}

	dev_err(fsm->dev, "timeout on wait_busy\n");

	return FLASH_STATUS_TIMEOUT;
}

static int fsm_read_jedec(struct stm_spi_fsm *fsm, uint8_t *const jedec)
{
	const struct fsm_seq *seq = &fsm_seq_read_jedec;
	uint32_t tmp[MAX_READID_LEN_ALIGNED/4];

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, tmp, MAX_READID_LEN_ALIGNED);

	memcpy(jedec, tmp, MAX_READID_LEN);

	fsm_wait_seq(fsm);

	return 0;
}

static int fsm_read_rdsfdp(struct stm_spi_fsm *fsm, uint32_t offs,
			   uint8_t dummy_cycles, uint8_t *data, int bytes)
{
	struct fsm_seq *seq = &fsm_seq_read_rdsfdp;
	uint32_t tmp;
	uint8_t *t = (uint8_t *)&tmp;
	int i;

	dev_dbg(fsm->dev, "read 'rdsfdp' register, %d byte(s)\n", bytes);

	seq->addr1 = (offs >> 16) & 0xffff;
	seq->addr2 = offs & 0xffff;

	seq->dummy = (dummy_cycles & 0x3f) << 16;

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, &tmp, 4);

	for (i = 0; i < bytes; i++)
		data[i] = t[i];

	fsm_wait_seq(fsm);

	return 0;
}

static int fsm_read_status(struct stm_spi_fsm *fsm, uint8_t cmd,
			   uint8_t *data, int bytes)
{
	struct fsm_seq *seq = &fsm_seq_read_status_fifo;
	uint32_t tmp;
	uint8_t *t = (uint8_t *)&tmp;
	int i;

	dev_dbg(fsm->dev, "read 'status' register [0x%02x], %d byte(s)\n",
		cmd, bytes);

	BUG_ON(bytes != 1 && bytes != 2);

	seq->seq_opc[0] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(cmd)),

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, &tmp, 4);

	for (i = 0; i < bytes; i++)
		data[i] = t[i];

	fsm_wait_seq(fsm);

	return 0;
}

static int fsm_write_status(struct stm_spi_fsm *fsm, uint8_t cmd,
			    uint16_t data, int bytes, int wait_busy)
{
	struct fsm_seq *seq = &fsm_seq_write_status;

	dev_dbg(fsm->dev, "write 'status' register [0x%02x], %d byte(s), 0x%04x, %s wait-busy\n",
		cmd, bytes, data, wait_busy ? "with" : "no");

	BUG_ON(bytes != 1 && bytes != 2);

	/*
	 * S25FLxxxX: auto-clearing QUAD bit issue
	 *
	 * A Write Status operation is achieved by a 1-byte WRSR command.
	 * However, this seems to have the effect of clearing the QUAD bit in
	 * the Config register (other bits in the Config register are not
	 * affected).  As a workaround, we first read the Config register,
	 * combine with the new Status data, and then promote to a 2-byte WRSR
	 * command.
	 *
	 * W25QxxxX: auto-clearing QUAD bit and CMP bit issue
	 *
	 * Same workaround as S25FLxxxX
	 */
	if (cmd == FLASH_CMD_WRSR &&
	    bytes == 1 &&
	    (fsm->configuration & CFG_WRSR_FORCE_16BITS)) {
		uint8_t cr;

		fsm_read_status(fsm, FLASH_CMD_RDSR2, &cr, 1);

		data = (data & 0xff) | ((uint16_t)cr << 8);
		bytes = 2;
	}

	seq->seq_opc[1] = (SEQ_OPC_PADS_1 | SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(cmd));

	seq->status = (uint32_t)data | STA_PADS_1 | STA_CSDEASSERT;
	seq->seq[2] = (bytes == 1) ? FSM_INST_STA_WR1 : FSM_INST_STA_WR1_2;

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	if (wait_busy)
		fsm_wait_busy(fsm, FLASH_MAX_STA_WRITE_MS);

	return 0;
}

static int fsm_enter_32bitaddr(struct stm_spi_fsm *fsm, int enter)
{
	struct fsm_seq *seq = &fsm_seq_en32bitaddr;
	uint32_t cmd = enter ? FLASH_CMD_EN4B_ADDR : FLASH_CMD_EX4B_ADDR;

	seq->seq_opc[0] = (SEQ_OPC_PADS_1 |
			   SEQ_OPC_CYCLES(8) |
			   SEQ_OPC_OPCODE(cmd) |
			   SEQ_OPC_CSDEASSERT);

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	return 0;
}

static void fsm_enter_quad_mode(struct stm_spi_fsm *fsm, int enter)
{
	uint8_t sta;

	fsm_read_status(fsm, FLASH_CMD_RDSR, &sta, 1);
	sta = (enter ? sta | MX25_STATUS_QE : sta & ~MX25_STATUS_QE);
	fsm_write_status(fsm, FLASH_CMD_WRSR, sta, 1, 1);
}

static uint8_t fsm_read_lock_reg(struct stm_spi_fsm *fsm, uint32_t offs)
{
	struct fsm_seq *seq = &fsm_seq_rd_lock_reg;
	uint32_t tmp;

	seq->addr1 = (offs >> 16) & 0xffff;
	seq->addr2 = offs & 0xffff;

	fsm_load_seq(fsm, seq);

	fsm_read_fifo(fsm, &tmp, 4);

	fsm_wait_seq(fsm);

	return (uint8_t)(tmp & 0xff);
}

static int fsm_write_lock_reg(struct stm_spi_fsm *fsm, uint32_t offs,
			      uint8_t val)
{
	struct fsm_seq *seq = &fsm_seq_wr_lock_reg;

	seq->addr1 = (offs >> 16) & 0xffff;
	seq->addr2 = offs & 0xffff;
	seq->status = val | STA_PADS_1 | STA_CSDEASSERT;

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	fsm_wait_busy(fsm, FLASH_MAX_STA_WRITE_MS);

	return 0;
}

static int fsm_lock(struct stm_spi_fsm *fsm, uint32_t offs,
		    int lock)
{
	struct fsm_seq *seq = lock ? &fsm_seq_lock : &fsm_seq_unlock;

	seq->addr1 = (offs >> 16) & 0xffff;
	seq->addr2 = offs & 0xffff;

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	fsm_wait_busy(fsm, FLASH_MAX_STA_WRITE_MS);

	return 0;
}

static int fsm_erase_sector(struct stm_spi_fsm *fsm, const uint32_t offset)
{
	struct fsm_seq *seq = &fsm_seq_erase_sector;
	uint8_t sta;
	int ret = 0;

	dev_dbg(fsm->dev, "erasing sector at 0x%08x\n", offset);

	/* Enter 32-bit address mode, if required */
	if (fsm->configuration & CFG_ERASESEC_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 1);

	seq->addr1 = (offset >> 16) & 0xffff;
	seq->addr2 = offset & 0xffff;

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	/* Wait for completion */
	sta = fsm_wait_busy(fsm, FLASH_MAX_SEC_ERASE_MS);
	if (sta != 0) {
		ret = 1;
		if (fsm->configuration & CFG_S25FL_CHECK_ERROR_FLAGS)
			s25fl_clear_status_reg(fsm);
	}

	/* N25Q: Check/Clear Error Flags */
	if (fsm->configuration & CFG_N25Q_CHECK_ERROR_FLAGS) {
		fsm_read_status(fsm, N25Q_CMD_RFSR, &sta, 1);
		if (sta & N25Q_FLAGS_ERROR) {
			ret = 1;
			n25q_clear_flags(fsm);
		}
	}


	/* Exit 32-bit address mode, if required */
	if (fsm->configuration & CFG_ERASESEC_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 0);

	return ret;
}

static int fsm_erase_chip(struct stm_spi_fsm *fsm)
{
	const struct fsm_seq *seq = &fsm_seq_erase_chip;

	dev_dbg(fsm->dev, "erasing chip\n");

	fsm_load_seq(fsm, seq);

	fsm_wait_seq(fsm);

	fsm_wait_busy(fsm, FLASH_MAX_CHIP_ERASE_MS);

	return 0;
}

static int fsm_read(struct stm_spi_fsm *fsm, uint8_t *const buf,
		    const uint32_t size, const uint32_t offset)
{
	struct fsm_seq *seq = &fsm_seq_read;
	uint32_t data_pads;
	uint32_t read_mask;
	uint8_t *page_buf = fsm->page_buf;
	uint32_t size_ub;
	uint32_t size_lb;
	uint32_t size_mop;
	uint32_t tmp[4];
	uint8_t *p;

	dev_dbg(fsm->dev, "reading %d bytes from 0x%08x\n", size, offset);

	/* Enter 32-bit address mode, if required */
	if (fsm->configuration & CFG_READ_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 1);

	/* Must read in multiples of 32 cycles (i.e. (32*pads)/8 bytes) */
	data_pads = ((seq->seq_cfg >> 16) & 0x3) + 1;
	read_mask = (data_pads << 2) - 1;

	/* Handle non-aligned buf */
	p = ((uint32_t)buf & 0x3) ? page_buf : buf;

	/* Handle non-aligned size */
	size_ub = (size + read_mask) & ~read_mask;
	size_lb = size & ~read_mask;
	size_mop = size & read_mask;

	seq->data_size = TRANSFER_SIZE(size_ub);
	seq->addr1 = (offset >> 16) & 0xffff;
	seq->addr2 = offset & 0xffff;

	fsm_load_seq(fsm, seq);

	if (size_lb)
		fsm_read_fifo(fsm, (uint32_t *)p, size_lb);

	if (size_mop) {
		fsm_read_fifo(fsm, tmp, read_mask + 1);
		memcpy(p + size_lb, &tmp, size_mop);
	}

	/* Handle non-aligned buf */
	if ((uint32_t)buf & 0x3)
		memcpy(buf, page_buf, size);

	/* Wait for sequence to finish */
	fsm_wait_seq(fsm);

	/* Exit 32-bit address mode, if required */
	if (fsm->configuration & CFG_READ_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 0);

	return 0;
}

static int fsm_write(struct stm_spi_fsm *fsm, const uint8_t *const buf,
		     const uint32_t size, const uint32_t offset)
{
	struct fsm_seq *seq = &fsm_seq_write;
	uint32_t data_pads;
	uint32_t write_mask;
	uint8_t *page_buf = fsm->page_buf;
	uint32_t size_ub;
	uint32_t size_lb;
	uint32_t size_mop;
	uint32_t tmp[4];
	uint8_t *t = (uint8_t *)&tmp;
	int i;
	const uint8_t *p;
	uint8_t sta;
	int ret = 0;

	dev_dbg(fsm->dev, "writing %d bytes to 0x%08x\n", size, offset);

	/* Enter 32-bit address mode, if required */
	if (fsm->configuration & CFG_WRITE_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 1);

	/* Must write in multiples of 32 cycles (i.e. (32*pads)/8 bytes) */
	data_pads = ((seq->seq_cfg >> 16) & 0x3) + 1;
	write_mask = (data_pads << 2) - 1;

	/* Handle non-aligned buf */
	if ((uint32_t)buf & 0x3) {
		memcpy(page_buf, buf, size);
		p = page_buf;
	} else {
		p = buf;
	}

	/* Handle non-aligned size */
	size_ub = (size + write_mask) & ~write_mask;
	size_lb = size & ~write_mask;
	size_mop = size & write_mask;

	seq->data_size = TRANSFER_SIZE(size_ub);
	seq->addr1 = (offset >> 16) & 0xffff;
	seq->addr2 = offset & 0xffff;

	if (fsm->capabilities.dummy_on_write) {
		fsm_load_seq(fsm, &fsm_seq_dummy);
		readl(fsm->base + SPI_FAST_SEQ_CFG);
	}

	/* Need to set FIFO to write mode, before writing data to FIFO (see
	 * GNBvb79594)
	 */
	writel(0x00040000, fsm->base + SPI_FAST_SEQ_CFG);

	/*
	 * Before writing data to the FIFO, apply a small delay to allow a
	 * potential change of FIFO direction to complete.
	 */
	if (fsm->fifo_dir_delay == 0)
		readl(fsm->base + SPI_FAST_SEQ_CFG);
	else
		udelay(fsm->fifo_dir_delay);


	/* Write data to FIFO, before starting sequence (see GNBvd79593) */
	if (size_lb) {
		fsm_write_fifo(fsm, (uint32_t *)p, size_lb);
		p += size_lb;
	}

	/* Handle non-aligned size */
	if (size_mop) {
		memset(t, 0xff, write_mask + 1);	/* fill with 0xff's */
		for (i = 0; i < size_mop; i++)
			t[i] = *p++;

		fsm_write_fifo(fsm, tmp, write_mask + 1);
	}

	/* Start sequence */
	fsm_load_seq(fsm, seq);

	/* Wait for sequence to finish */
	fsm_wait_seq(fsm);

	/* Wait for completion */
	sta = fsm_wait_busy(fsm, FLASH_MAX_PAGE_WRITE_MS);
	if (sta != 0) {
		ret = 1;
		if (fsm->configuration & CFG_S25FL_CHECK_ERROR_FLAGS)
			s25fl_clear_status_reg(fsm);
	}

	/* N25Q: Check/Clear Error Flags */
	if (fsm->configuration & CFG_N25Q_CHECK_ERROR_FLAGS) {
		fsm_read_status(fsm, N25Q_CMD_RFSR, &sta, 1);
		if (sta & N25Q_FLAGS_ERROR) {
			ret = 1;
			n25q_clear_flags(fsm);
		}
	}

	/* Exit 32-bit address mode, if required */
	if (fsm->configuration & CFG_WRITE_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 0);

	return ret;
}

static int fsm_xxlock_oneblock(struct stm_spi_fsm *fsm, loff_t offs, int lock)
{
	uint8_t msk;
	uint8_t val;
	uint8_t reg;

	msk = fsm->lock_mask;
	val = lock ? fsm->lock_val[FSM_BLOCK_LOCKED] :
		fsm->lock_val[FSM_BLOCK_UNLOCKED];

	reg = fsm_read_lock_reg(fsm, offs);
	if ((reg & msk) != val) {
		if (fsm->configuration & CFG_RD_WR_LOCK_REG) {
			reg = (reg & ~msk) | (val & msk);
			fsm_write_lock_reg(fsm, offs, reg);
		} else
			fsm_lock(fsm, offs, lock);
		reg = fsm_read_lock_reg(fsm, offs);
		if ((reg & msk) != val) {
			dev_err(fsm->dev, "Failed to %s sector at 0x%012llx\n",
				lock ? "lock" : "unlock", offs);
			return 1;
		}
	}

	return 0;
}

static int fsm_xxlock_blocks(struct stm_spi_fsm *fsm, loff_t offs, uint64_t len,
			     int lock)
{
	struct mtd_info *mtd = &fsm->mtd;
	uint64_t offs_end = offs + len;
	uint64_t p4k_bot_end = fsm->p4k_bot_end;
	uint64_t p4k_top_start = fsm->p4k_top_start;

	if (fsm->configuration & CFG_LOCK_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 1);

	if (fsm->configuration & CFG_MX25_LOCK_TOGGLE_QE_BIT)
		fsm_enter_quad_mode(fsm, 0);

	while (offs < offs_end) {
		fsm_xxlock_oneblock(fsm, offs, lock);

		/* Parameter sectors have 4KiB locking granularity */
		if (offs < p4k_bot_end || offs >= p4k_top_start)
			offs += 0x1000;
		else
			offs += mtd->erasesize;
	}

	if (fsm->configuration & CFG_MX25_LOCK_TOGGLE_QE_BIT)
		fsm_enter_quad_mode(fsm, 1);

	if (fsm->configuration & CFG_LOCK_TOGGLE32BITADDR)
		fsm_enter_32bitaddr(fsm, 0);

	return 0;
}

static void fsm_dump_bpx_state(struct stm_spi_fsm *fsm)
{
	uint8_t sr;
	uint8_t bpx;

	dev_info(fsm->dev, "BP[%d-0] Locked Region:\n", fsm->bpx_n_bits - 1);

	fsm_read_status(fsm, FLASH_CMD_RDSR, &sr, 1);
	bpx = status_to_bpx(fsm, sr);

	dump_bpx_region(fsm, bpx);
}

/*
 * BPx Block Protection scheme: Lock/Unlock
 *
 * Lock/Unlock requests involve moving the BPx boundary up or down:
 *
 *	Lock (TBPROT = 1): Move boundary down
 *    Unlock (TBPROT = 0)        <-------|
 *                   ____________________|____________________
 *          Device: |                    |                    |
 *                  |____________________|____________________|
 *                                       |
 *    Unlock (TBPROT = 1)                |------>
 *      Lock (TBPROT = 0): Move boundary up
 *
 * Returns an error if the new boundary is not supported by the BPx scheme, or
 * if the request is not contiguous with the existing BPx state.
 */
static int fsm_bpx_xxlock(struct stm_spi_fsm *fsm, loff_t offs, uint64_t len,
			  int lock)
{
	const char *const opstr = lock ? "lock" : "unlock";
	uint64_t old_boundary, new_boundary;
	int i;

	uint8_t sr, bpx;

	/* Get current BPx state */
	fsm_read_status(fsm, FLASH_CMD_RDSR, &sr, 1);
	bpx = status_to_bpx(fsm, sr);
	old_boundary = fsm->bpx_bnds[bpx];

	if ((lock && fsm->bpx_tbprot) ||
	    (!lock && !fsm->bpx_tbprot)) {
		/*
		 * Lock/TBPROT=1 and Unlock/TBPROT=0: Move lock boundary up
		 */

		/* Set new boundary according to request */
		new_boundary = offs + len;

		/* Boundary already higher than new request */
		if (new_boundary <= old_boundary)
			goto out1;

		/* Request is non-contiguous with current boundary */
		if (offs > old_boundary)
			goto err1;
	} else {
		/*
		 * Unlock/TBPROT=1 and Lock/TBPROT=0: Move lock boundary down
		 */

		/* Set new boundary according to request */
		new_boundary = offs;

		/* Boundary already lower than new request */
		if (new_boundary >= old_boundary)
			goto out1;

		/* Request is non-contiguous with current boundary */
		if (offs + len < old_boundary)
			goto err1;
	}

	/* Find a valid PBx boundary */
	for (i = 0; i < fsm->bpx_n_bnds; i++) {
		if (new_boundary == fsm->bpx_bnds[i]) {
			bpx = i;
			break;
		}
	}

	/* Request does not match a BPx boundary */
	if (i == fsm->bpx_n_bnds)
		goto err2;

	/* Update Status register with new BPx */
	for (i = 0; i < fsm->bpx_n_bits; i++) {
		if (bpx & (1 << i))
			sr |= fsm->bpx_sr_masks[i];
		else
			sr &= ~fsm->bpx_sr_masks[i];
	}
	fsm_write_status(fsm, FLASH_CMD_WRSR, sr, 1, 1);

	return 0;

 err2:
	dev_err(fsm->dev, "%s 0x%08llx -> 0x%08llx: request not compatible with BPx boundaries\n",
		opstr, offs, offs + len);
	return 1;
 err1:
	dev_err(fsm->dev, "%s 0x%08llx -> 0x%08llx: request not compatible BPx state\n",
		opstr, offs, offs + len);
	return -EINVAL;
 out1:
	dev_info(fsm->dev, "%s 0x%08llx -> 0x%08llx: already within %sed region\n",
		 opstr, offs, offs + len, opstr);
	return 0;
}

/*
 * FSM Configuration
 */
static int fsm_set_mode(struct stm_spi_fsm *fsm, uint32_t mode)
{
	/* Wait for controller to accept mode change */
	if (!fsm->capabilities.no_poll_mode_change) {
		while (!(readl(fsm->base + SPI_STA_MODE_CHANGE) & 0x1))
			;
	}
	writel(mode, fsm->base + SPI_MODESELECT);

	return 0;
}

static int fsm_set_freq(struct stm_spi_fsm *fsm, uint32_t freq)
{
	uint32_t emi_freq;
	uint32_t clk_div;

	if (!fsm->clk) {
		dev_warn(fsm->dev,
			"No EMI clock available. Using default 100MHz.\n");
		emi_freq = 100000000UL;
	} else
		emi_freq = clk_get_rate(fsm->clk);

	freq *= 1000000;

	/* Calculate clk_div: multiple of 2, round up, 2 -> 128. Note, clk_div =
	 * 4 is not supported on some SoCs, use 6 instead */
	clk_div = 2 * DIV_ROUND_UP(emi_freq, 2 * freq);
	if (clk_div < 2)
		clk_div = 2;
	else if (clk_div == 4 && fsm->capabilities.no_clk_div_4)
		clk_div = 6;
	else if (clk_div > 128)
		clk_div = 128;

	/*
	 * Determine a suitable delay for the IP to complete a change of
	 * direction of the FIFO.  The required delay is related to the clock
	 * divider used.  The following heuristics are based on empirical tests,
	 * using a 100MHz EMI clock.
	 */
	if (clk_div <= 4)
		fsm->fifo_dir_delay = 0;
	else if (clk_div <= 10)
		fsm->fifo_dir_delay = 1;
	else
		fsm->fifo_dir_delay = DIV_ROUND_UP(clk_div, 10);

	dev_dbg(fsm->dev, "emi_clk = %uHZ, spi_freq = %uHZ, clock_div = %u\n",
		emi_freq, freq, clk_div);

	/* Set SPI_CLOCKDIV */
	writel(clk_div, fsm->base + SPI_CLOCKDIV);

	return 0;
}

static int fsm_init(struct stm_spi_fsm *fsm)
{
	/* Perform a soft reset of the FSM controller */
	if (!fsm->capabilities.no_sw_reset) {
		writel(SEQ_CFG_SWRESET, fsm->base + SPI_FAST_SEQ_CFG);
		udelay(1);
		writel(0, fsm->base + SPI_FAST_SEQ_CFG);
	}

	/* Set clock to 'safe' frequency for device probe */
	fsm_set_freq(fsm, FLASH_PROBE_FREQ);

	/* Switch to FSM */
	fsm_set_mode(fsm, SPI_MODESELECT_FSM);

	/* Set timing parameters (use reset values for now (awaiting information
	 * from Validation Team)
	 */
	writel(SPI_CFG_DEVICE_ST |
	       SPI_CFG_MIN_CS_HIGH(0x0AA) |
	       SPI_CFG_CS_SETUPHOLD(0xa0) |
	       SPI_CFG_DATA_HOLD(0x00), fsm->base + SPI_CONFIGDATA);
	writel(0x00000001, fsm->base + SPI_STATUS_WR_TIME_REG);

	/*
	 * Set the FSM 'WAIT' delay to the minimum workable value.  Note, for
	 * our purposes, the WAIT instruction is used purely to achieve
	 * "sequence validity" rather than actually implement a delay.
	 */
	writel(0x000001, fsm->base + SPI_PROGRAM_ERASE_TIME);

	/* Clear the data FIFO */
	fsm_clear_fifo(fsm);

	return 0;
}

static void fsm_exit(struct stm_spi_fsm *fsm)
{

}


/*
 * MTD interface
 */

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
static int fsm_mtd_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t *retlen, u_char *buf)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	uint32_t bytes;

	dev_dbg(fsm->dev, "%s %s 0x%08x, len %zd\n",  __func__,
		"from", (u32)from, len);

	/* Byte count starts at zero. */
	if (retlen)
		*retlen = 0;

	if (!len)
		return 0;

	if (from + len > mtd->size)
		return -EINVAL;

	mutex_lock(&fsm->lock);

	while (len > 0) {
		bytes = min(len, (size_t)FLASH_PAGESIZE);

		fsm_read(fsm, buf, bytes, from);

		buf += bytes;
		from += bytes;
		len -= bytes;

		if (retlen)
			*retlen += bytes;
	}

	mutex_unlock(&fsm->lock);

	return 0;
}

/*
 * Write an address range to the flash chip.  Data must be written in
 * FLASH_PAGESIZE chunks.  The address range may be any size provided
 * it is within the physical boundaries.
 */
static int fsm_mtd_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	struct stm_spi_fsm *fsm = mtd->priv;

	u32 page_offs;
	u32 bytes;
	uint8_t *b = (uint8_t *)buf;
	int ret = 0;

	dev_dbg(fsm->dev, "%s %s 0x%08x, len %zd\n", __func__,
		"to", (u32)to, len);

	if (retlen)
		*retlen = 0;

	if (!len)
		return 0;

	if (to + len > mtd->size)
		return -EINVAL;

	/* offset within page */
	page_offs = to % FLASH_PAGESIZE;

	mutex_lock(&fsm->lock);

	while (len) {

		/* write up to page boundary */
		bytes = min(FLASH_PAGESIZE - page_offs, len);

		if (fsm_write(fsm, b, bytes, to) != 0) {
			ret = -EIO;
			goto out1;
		}

		b += bytes;
		len -= bytes;
		to += bytes;

		/* We are now page-aligned */
		page_offs = 0;

		if (retlen)
			*retlen += bytes;

	}

 out1:
	mutex_unlock(&fsm->lock);

	return ret;
}

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int fsm_mtd_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	u32 addr, len;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", (long long)instr->addr, (long long)instr->len);

	if (instr->addr + instr->len > mtd->size)
		return -EINVAL;

	if (instr->len & (mtd->erasesize - 1))
		return -EINVAL;

	addr = instr->addr;
	len = instr->len;

	mutex_lock(&fsm->lock);

	/* whole-chip erase? */
	if (len == mtd->size) {
		if (fsm_erase_chip(fsm)) {
			instr->state = MTD_ERASE_FAILED;
			mutex_unlock(&fsm->lock);
			return -EIO;
		}
	} else {
		while (len) {
			if (fsm_erase_sector(fsm, addr)) {
				instr->state = MTD_ERASE_FAILED;
				mutex_unlock(&fsm->lock);
				return -EIO;
			}

			addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
	}

	mutex_unlock(&fsm->lock);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

/*
 * Individual Block Locking Support
 */
static int fsm_mtd_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	uint32_t block_mask = mtd->erasesize - 1;
	int ret = 0;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", ofs, len);

	if (ofs & block_mask || len & block_mask)
		return -EINVAL;

	mutex_lock(&fsm->lock);

	ret = fsm_xxlock_blocks(fsm, ofs, len, 1);

	mutex_unlock(&fsm->lock);

	return ret;
}

static int fsm_mtd_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	uint32_t block_mask = mtd->erasesize - 1;
	int ret = 0;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", ofs, len);

	if (ofs & block_mask || len & block_mask)
		return -EINVAL;

	mutex_lock(&fsm->lock);

	ret = fsm_xxlock_blocks(fsm, ofs, len, 0);

	mutex_unlock(&fsm->lock);

	return ret;
}

/*
 * PBx Block Protection Support
 */
static int fsm_mtd_bpx_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	int ret = 0;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", ofs, len);

	mutex_lock(&fsm->lock);

	ret = fsm_bpx_xxlock(fsm, ofs, len, 1);

	fsm_dump_bpx_state(fsm);

	mutex_unlock(&fsm->lock);

	return ret;

}

static int fsm_mtd_bpx_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len)
{
	struct stm_spi_fsm *fsm = mtd->priv;
	int ret = 0;

	dev_dbg(fsm->dev, "%s %s 0x%llx, len %lld\n", __func__,
		"at", ofs, len);

	mutex_lock(&fsm->lock);

	ret = fsm_bpx_xxlock(fsm, ofs, len, 0);

	fsm_dump_bpx_state(fsm);

	mutex_unlock(&fsm->lock);

	return ret;
}

static int cmp_flash_info_readid_len(const void *a, const void *b)
{
	return ((struct flash_info *)b)->readid_len -
		((struct flash_info *)a)->readid_len;
}

static struct flash_info *fsm_jedec_probe(struct stm_spi_fsm *fsm)
{
	uint8_t	readid[MAX_READID_LEN];
	char readid_str[MAX_READID_LEN * 3 + 1];
	struct flash_info *info;

	if (fsm_read_jedec(fsm, readid) != 0) {
		dev_info(fsm->dev, "error reading JEDEC ID\n");
		return NULL;
	}

	hex_dump_to_buffer(readid, MAX_READID_LEN, 16, 1,
			   readid_str, sizeof(readid_str), 0);

	dev_dbg(fsm->dev, "READID = %s\n", readid_str);

	/* The 'readid' may match multiple entries in the table.  To ensure we
	 * retrieve the most specific match, the table is sorted in order of
	 * 'readid_len'.
	 */
	sort(flash_types, ARRAY_SIZE(flash_types) - 1,
	     sizeof(struct flash_info), cmp_flash_info_readid_len, NULL);

	for (info = flash_types; info->name; info++) {
		if (memcmp(info->readid, readid, info->readid_len) == 0)
			return info;
	}

	dev_err(fsm->dev, "Unrecognized READID [%s]\n", readid_str);

	return NULL;
}

#ifdef CONFIG_OF
static void *stm_fsm_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_spifsm_data *data;
	struct device_node *tp;
	struct regmap *regmap;
	uint32_t boot_device_reg;
	uint32_t boot_device_spi;
	uint32_t boot_device_msk;
	uint32_t boot_device;     /* Value we read from *boot_device_reg */
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return data;

	of_property_read_string(np, "flash-name",
				(const char **)&data->name);
	of_property_read_u32(np, "max-freq", &data->max_freq);

	/* Assume boot-from-spi, unless we can prove otherwise. */
	data->capabilities.boot_from_spi = 1;

	regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		goto boot_device_fail;
	}

	/* Where in the syscon the boot device information lives */
	ret = of_property_read_u32(np, "st,boot-device-reg", &boot_device_reg);
	if (ret)
		goto boot_device_fail;

	/* Boot device value when booted from SPI NOR */
	ret = of_property_read_u32(np, "st,boot-device-spi", &boot_device_spi);
	if (ret)
		goto boot_device_fail;

	/* Mask to apply on boot_device_reg */
	ret = of_property_read_u32(np, "st,boot-device-msk", &boot_device_msk);
	if (ret)
		goto boot_device_fail;

	ret = regmap_read(regmap, boot_device_reg, &boot_device);
	if (ret)
		goto boot_device_fail;

	boot_device &= boot_device_msk;
	if (boot_device != boot_device_spi)
		data->capabilities.boot_from_spi = 0;

boot_device_fail:
	if (ret)
		dev_warn(&pdev->dev, "failed to fetch boot device, assuming boot from SPI\n");

	tp = of_parse_phandle(np, "caps-handle", 0);
	if (!tp) {
		dev_warn(&pdev->dev, "No CAPs node\n");
		return data;
	}

	data->capabilities.dual_mode = of_property_read_bool(tp, "dual-mode");

	data->capabilities.quad_mode =	of_property_read_bool(tp, "quad-mode");

	data->capabilities.reset_signal = of_property_read_bool(tp,
								"reset-signal");
	data->capabilities.reset_por = of_property_read_bool(tp, "reset-por");

	data->capabilities.addr_32bit = of_property_read_bool(tp, "addr-32bit");
	data->capabilities.no_poll_mode_change =
			of_property_read_bool(tp, "no-poll-mode-change");
	data->capabilities.no_clk_div_4 =
			of_property_read_bool(tp, "no-clk-div-4");
	data->capabilities.no_sw_reset =
			of_property_read_bool(tp, "no-sw-reset");
	data->capabilities.dummy_on_write =
			of_property_read_bool(tp, "dummy-on-write");
	data->capabilities.no_read_repeat =
			of_property_read_bool(tp, "no-read-repeat");
	data->capabilities.no_write_repeat =
			of_property_read_bool(tp, "no-write-repeat");
	if (of_property_read_bool(tp, "no-read-status"))
		data->capabilities.read_status_bug = spifsm_no_read_status;
	if (of_property_read_bool(tp, "read-status-clkdiv4"))
		data->capabilities.read_status_bug =
					spifsm_read_status_clkdiv4;

	return data;
}
#else
static void *stm_fsm_dt_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

/*
 * STM SPI FSM driver setup
 */
static int stm_spi_fsm_probe(struct platform_device *pdev)
{
	struct stm_plat_spifsm_data *data;
	struct stm_spi_fsm *fsm;
	struct resource *resource;
	int ret = 0;
	struct flash_info *info;
	uint64_t size;
	struct mtd_part_parser_data ppdata;

	/* Allocate memory for the driver structure (and zero it) */
	fsm = kzalloc(sizeof(struct stm_spi_fsm), GFP_KERNEL);
	if (!fsm) {
		dev_err(&pdev->dev, "failed to allocate fsm controller data\n");
		return -ENOMEM;
	}

        /* TCH_FIX_BEGIN : PF 24/10/2014  */
        m25p_flash = fsm;
        /* TCH_FIX_END : PF 24/10/2014  */

	fsm->dev = &pdev->dev;

	if (pdev->dev.of_node)
		pdev->dev.platform_data = stm_fsm_dt_get_pdata(pdev);

	data = pdev->dev.platform_data;
	if (!data) {
		ret = -ENOMEM;
		goto out1;
	}

	/* Platform capabilities (Board/SoC/IP) */
	fsm->capabilities = data->capabilities;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev, "failed to find IORESOURCE_MEM\n");
		ret = -ENODEV;
		goto out1;
	}

	fsm->region = request_mem_region(resource->start,
					 resource_size(resource), pdev->name);
	if (!fsm->region) {
		dev_err(&pdev->dev, "failed to reserve memory region "
			"[0x%08x-0x%08x]\n", resource->start, resource->end);
		ret = -EBUSY;
		goto out1;
	}

	fsm->base = ioremap_nocache(resource->start, resource_size(resource));

	if (!fsm->base) {
		dev_err(&pdev->dev, "failed to ioremap [0x%08x]\n",
			resource->start);
		ret = -EINVAL;
		goto out2;
	}

	fsm->clk = clk_get(&pdev->dev, "emi_clk");
	if (IS_ERR(fsm->clk)) {
		dev_warn(fsm->dev, "Failed to find EMI clock.\n");
		fsm->clk = NULL;
	} else if (clk_prepare_enable(fsm->clk)) {
		dev_warn(fsm->dev, "Failed to enable EMI clock.\n");
		clk_put(fsm->clk);
		fsm->clk = NULL;
	}

	mutex_init(&fsm->lock);

	/* Initialise FSM */

	if (fsm_init(fsm) != 0) {
		dev_err(&pdev->dev, "failed to initialise SPI FSM "
			"Controller\n");
		ret = -EINVAL;
		goto out3;
	}

	/* Detect SPI FLASH device */
	info = fsm_jedec_probe(fsm);
	if (!info) {
		ret = -ENODEV;
		goto out4;
	}

	/* Issue warning if device found does not match that specified in the
	 * platform data.
	 */
	if (data->name && strcmp(data->name, info->name) != 0)
		dev_info(&pdev->dev, "Expecting '%s', found '%s'\n",
			 data->name, info->name);

	/* Mask-out capabilities not supported by the platform */
	if (fsm->capabilities.quad_mode == 0)
		info->capabilities &= ~FLASH_CAPS_QUAD;
	if (fsm->capabilities.dual_mode == 0)
		info->capabilities &= ~FLASH_CAPS_DUAL;

	/* Determine 32-bit addressing requirement */
	size = info->sector_size * info->n_sectors;
	if (size > 0x1000000)
		info->capabilities |= FLASH_CAPS_32BITADDR;

	/* Configure READ/WRITE/ERASE sequences according to platform and device
	 * capabilities.
	 */
	if (info->config)
		ret = info->config(fsm, info);
	else
		ret = fsm_config_rwe_seqs_default(fsm, info);

	if (ret != 0) {
		ret = -EINVAL;
		goto out4;
	}

	fsm->resume = info->resume;
	fsm->reset = (info->capabilities & FLASH_CAPS_RESET ? true : false);

	if (fsm->capabilities.boot_from_spi &&
	    !can_handle_soc_reset(fsm))
		dev_info(&pdev->dev, "No provision for SPI reset on boot-from-spi system\n");

#ifdef CONFIG_STM_SPI_FSM_DEBUG
	fsm_dump_seq("FSM READ SEQ", &fsm_seq_read);
	fsm_dump_seq("FSM WRITE_SEQ", &fsm_seq_write);
	fsm_dump_seq("FSM ERASE_SECT_SEQ", &fsm_seq_erase_sector);
	fsm_dump_seq("FSM EN32BITADDR_SEQ", &fsm_seq_en32bitaddr);
#endif

	platform_set_drvdata(pdev, fsm);

	/* Set operating frequency, from table or overridden by platform data */
	if (data->max_freq) {
		fsm_set_freq(fsm, data->max_freq);
		fsm->max_freq = data->max_freq;
	} else if (info->max_freq) {
		fsm_set_freq(fsm, info->max_freq);
		fsm->max_freq = info->max_freq;
	}

	/* Set up MTD parameters */
	fsm->mtd.priv = fsm;
	if (data && data->name)
		fsm->mtd.name = data->name;
	else
		fsm->mtd.name = NAME;

	fsm->mtd.dev.parent = &pdev->dev;
	fsm->mtd.type = MTD_NORFLASH;
	fsm->mtd.writesize = 4;
	fsm->mtd.writebufsize = fsm->mtd.writesize;
	fsm->mtd.flags = MTD_CAP_NORFLASH;
	fsm->mtd.size = size;
	fsm->mtd.erasesize = info->sector_size;

	fsm->mtd._read = fsm_mtd_read;
	fsm->mtd._write = fsm_mtd_write;
	fsm->mtd._erase = fsm_mtd_erase;

	/* Block Locking Support: */
	if (info->capabilities & FLASH_CAPS_BLK_LOCKING) {
		/* - Individual Block Locking scheme */
		fsm->mtd._lock = fsm_mtd_lock;
		fsm->mtd._unlock = fsm_mtd_unlock;

		dev_info(fsm->dev, "Individual block locking scheme enabled\n");
	} else if (info->capabilities & FLASH_CAPS_BPX_LOCKING) {
		/* - PBx Block Protection Scheme */
		fsm->mtd._lock = fsm_mtd_bpx_lock;
		fsm->mtd._unlock = fsm_mtd_bpx_unlock;

		dev_info(fsm->dev, "BPx block locking scheme enabled\n");
#ifdef CONFIG_STM_SPI_FSM_DEBUG
		dump_bpx_regions(fsm);
#endif
		fsm_dump_bpx_state(fsm);
	}

	dev_info(&pdev->dev, "found device: %s, size = %llx (%lldMiB) "
		 "erasesize = 0x%08x (%uKiB)\n",
		 info->name,
		 (long long)fsm->mtd.size, (long long)(fsm->mtd.size >> 20),
		 fsm->mtd.erasesize, (fsm->mtd.erasesize >> 10));

	if (pdev->dev.of_node)
		ppdata.of_node = of_parse_phandle(pdev->dev.of_node,
							"partitions", 0);
	ret = mtd_device_parse_register(&fsm->mtd, NULL, &ppdata,
					data ? data->parts : NULL,
					data ? data->nr_parts : 0);
	if (!ret)
		return 0;

 out4:
	fsm_exit(fsm);
	platform_set_drvdata(pdev, NULL);
 out3:
	if (fsm->clk) {
		clk_disable_unprepare(fsm->clk);
		clk_put(fsm->clk);
	}
	iounmap(fsm->base);
 out2:
	release_resource(fsm->region);
 out1:
	kfree(fsm);

	return ret;
}

static int stm_spi_fsm_remove(struct platform_device *pdev)
{
	struct stm_spi_fsm *fsm = platform_get_drvdata(pdev);
	int err;

	err = mtd_device_unregister(&fsm->mtd);
	if (err)
		return err;

	fsm_exit(fsm);
	if (fsm->clk) {
		clk_disable_unprepare(fsm->clk);
		clk_put(fsm->clk);
	}
	iounmap(fsm->base);
	release_resource(fsm->region);
	platform_set_drvdata(pdev, NULL);

	kfree(fsm);

	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id spi_fsm_match[] = {
	{
		.compatible = "st,spi-fsm",
	},
	{},
};

MODULE_DEVICE_TABLE(of, spi_fsm_match);
#endif

#ifdef CONFIG_PM
static int stm_spi_fsm_suspend(struct device *dev)
{
	struct stm_spi_fsm *fsm = dev_get_drvdata(dev);

	if (fsm->clk)
		clk_disable_unprepare(fsm->clk);
	return 0;
}

static int stm_spi_fsm_resume(struct device *dev)
{
	struct stm_spi_fsm *fsm = dev_get_drvdata(dev);

	if (fsm->clk)
		clk_prepare_enable(fsm->clk);

	fsm_init(fsm);

	if (fsm->resume)
		fsm->resume(fsm);

	if (fsm->max_freq)
		fsm_set_freq(fsm, fsm->max_freq);

	return 0;
}

SIMPLE_DEV_PM_OPS(stm_spi_fsm_pm_ops, stm_spi_fsm_suspend, stm_spi_fsm_resume);
#define STM_SPI_PM_OPS	(&stm_spi_fsm_pm_ops)
#else
#define STM_SPI_PM_OPS	NULL
#endif

static struct platform_driver stm_spi_fsm_driver = {
	.probe		= stm_spi_fsm_probe,
	.remove		= stm_spi_fsm_remove,
	.driver		= {
		.name	= NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(spi_fsm_match),
		.pm	= STM_SPI_PM_OPS,
	},
};

static int __init stm_spi_fsm_init(void)
{
	return platform_driver_register(&stm_spi_fsm_driver);
}

static void __exit stm_spi_fsm_exit(void)
{
	platform_driver_unregister(&stm_spi_fsm_driver);
}

module_init(stm_spi_fsm_init);
module_exit(stm_spi_fsm_exit);

MODULE_AUTHOR("Angus Clark <Angus.Clark@st.com>");
MODULE_DESCRIPTION("STM SPI FSM driver");
MODULE_LICENSE("GPL");
