/*
 *  ------------------------------------------------------------------------
 *  stm_afm_nand.c STMicroelectronics Advanced Flex Mode NAND Flash driver
 *                 for SoC's with the Hamming NAND Controller.
 *  ------------------------------------------------------------------------
 *
 *  Copyright (c) 2010 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  ------------------------------------------------------------------------
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *  ------------------------------------------------------------------------
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/stm_nand.h>
#include <linux/mtd/partitions.h>
#include <linux/err.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>

#include "stm_nand_ecc.h"
#include "stm_nand_regs.h"
#include "stm_nand_bbt.h"
#include "stm_nand_dt.h"

#define NAME	"stm-nand-afm"


#include <linux/mtd/partitions.h>

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
/*
 * Support for STM boot-mode ECC: The STM NAND Boot Controller uses a different
 * ECC scheme to the AFM controller.  In order to support boot-mode ECC data, we
 * maintian two sets of ECC parameters and switch depending on which partition
 * we are about to read from.
 */
struct ecc_params {
	/* nand_chip params */
	struct nand_ecc_ctrl	ecc_ctrl;
	int			subpagesize;

	/* mtd_info params */
	u_int32_t		subpage_sft;
};

static void afm_select_eccparams(struct mtd_info *mtd, loff_t offs);
static int afm_write_page_boot(struct mtd_info *mtd,
				struct nand_chip *chip,
				const uint8_t *buf,
				int oob_required);
static int afm_read_page_boot(struct mtd_info *mtd,
			      struct nand_chip *chip,
			      uint8_t *buf,
			      int oob_required,
			      int page);

/* Module parameter for specifying name of boot partition */
static char *nbootpart;
module_param(nbootpart, charp, 0000);
MODULE_PARM_DESC(nbootpart, "MTD name of NAND boot-mode ECC partition");

#else
#define afm_select_eccparams(x, y) do {} while (0);
#endif /* CONFIG_STM_NAND_AFM_BOOTMODESUPPORT */


/* External functions */

/* AFM 32-byte program block */
struct afm_prog {
	uint8_t instr[16];
	uint32_t addr_reg;
	uint32_t extra_reg;
	uint8_t cmds[4];
	uint32_t seq_cfg;
} __attribute__((__packed__));

/* NAND device connected to STM NAND Controller */
struct stm_nand_afm_device {
	struct nand_chip	chip;
	struct mtd_info		mtd;

	int			csn;

	uint32_t		ctl_timing;
	uint32_t		wen_timing;
	uint32_t		ren_timing;

	uint32_t		afm_gen_cfg;

	struct device		*dev;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	unsigned long		boot_start;
	unsigned long		boot_end;
	struct ecc_params	ecc_boot;
	struct ecc_params	ecc_afm;
#endif
	int			nr_parts;
	struct mtd_partition	*parts;

};

/* STM NAND AFM Controller  */
struct stm_nand_afm_controller {
	void __iomem		*base;
	void __iomem		*fifo_cached;
	unsigned long		fifo_phys;

	struct clk		*clk;
	/* resources */
	struct device		*dev;
	unsigned int		map_base;
	unsigned int		map_size;
	unsigned int		irq;

	spinlock_t		lock; /* save/restore IRQ flags */

	int			current_csn;

	/* access control */
	struct nand_hw_control	hwcontrol;

	/* irq completions */
	struct completion	rbn_completed;
	struct completion	seq_completed;

	/* emulate MTD nand_base.c implementation */
	uint32_t		status;
	uint32_t		col;
	uint32_t		page;

	/* devices connected to controller */
	struct stm_nand_afm_device *devices[0];
};

#define afm_writereg(val, reg)	iowrite32(val, afm->base + (reg))
#define afm_readreg(reg)	ioread32(afm->base + (reg))

static struct stm_nand_afm_controller *mtd_to_afm(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_hw_control *controller = chip->controller;
	struct stm_nand_afm_controller *afm;

	afm = container_of(controller, struct stm_nand_afm_controller,
			    hwcontrol);
	return afm;
}


/*
 * Define some template AFM programs
 */
#define AFM_INSTR(cmd, op)	((cmd) | ((op) << 4))

/* AFM Prog: Block Erase */
struct afm_prog afm_prog_erase = {
	.cmds = {
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE2,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_ADDR,	0),
		AFM_INSTR(AFM_ADDR,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Block Erase, extra address cycle */
struct afm_prog afm_prog_erase_ext = {
	.cmds = {
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE1,
		NAND_CMD_ERASE2,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_ADDR,	0),
		AFM_INSTR(AFM_ADDR,	1),
		AFM_INSTR(AFM_ADDR,	2),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
		},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read OOB [SmallPage] */
struct afm_prog afm_prog_read_oob_sp = {
	.cmds = {
		NAND_CMD_READOOB
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read OOB [LargePage] */
struct afm_prog afm_prog_read_oob_lp = {
	.cmds = {
		NAND_CMD_READ0,
		NAND_CMD_READSTART
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read Raw Page and OOB Data [SmallPage]
 *
 * Note, "SPARE3" would normally be used to read the OOB data.  However, SPARE3
 * has been observed to cause problems with EMI Arbitration resulting in system
 * deadlock.  To avoid such problems, we use the DATA0 command here.  This will
 * have the effect of over-reading 48 bytes from the end of OOB area but should
 * be benign on current NAND devices.
 */
struct afm_prog afm_prog_read_raw_sp = {
	.cmds = {
		NAND_CMD_READ0
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Read Raw Page and OOB Data [LargePage] */
struct afm_prog afm_prog_read_raw_lp = {
	.cmds = {
		NAND_CMD_READ0,
		NAND_CMD_READSTART
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO,
};

/* AFM Prog: Write Raw Page [SmallPage]
 *           OOB written with FLEX mode.
 */
struct afm_prog afm_prog_write_raw_sp_a = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_READ0,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CMD,	3),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Raw Page and OOB Data [LargePage] */
struct afm_prog afm_prog_write_raw_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Page and OOB Data, with ECC support [SmallPage]
 *
 * Note, the AFM command, "SPARE3", would normally be used to write the OOB
 * data, with ECC inserted on the fly by the NAND controller.  However, the
 * SPARE3 command has been observed to cause EMI Arbitration issues resulting in
 * a bus stall and system deadlock.  To avoid this problem, we switch to FLEX
 * mode to write the OOB data, extracting and inserting the ECC bytes from the
 * NAND ECC checkcode regiter (see afm_write_page_ecc_sp())
 */
struct afm_prog afm_prog_write_ecc_sp_a = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_READ0,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	1),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write Page and OOB Data, with ECC support [LargePage] */
struct afm_prog afm_prog_write_ecc_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS,
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	2),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};

/* AFM Prog: Write OOB Data [LargePage] */
struct afm_prog afm_prog_write_oob_lp = {
	.cmds = {
		NAND_CMD_SEQIN,
		NAND_CMD_PAGEPROG,
		NAND_CMD_STATUS
	},
	.instr = {
		AFM_INSTR(AFM_CMD,	0),
		AFM_INSTR(AFM_DATA,	0),
		AFM_INSTR(AFM_CMD,	1),
		AFM_INSTR(AFM_CMD,	2),
		AFM_INSTR(AFM_CHECK,	0),
		AFM_INSTR(AFM_STOP,	0)
	},
	.seq_cfg = AFM_SEQ_CFG_GO | AFM_SEQ_CFG_DIR_WRITE,
};


/*
 * STMicroeclectronics H/W ECC layouts
 *
 * AFM4 ECC:      512-byte data records, 3 bytes H/W ECC, 1 byte S/W ECC, 3
 *                bytes marker ({'A', 'F', 'M'})
 * Boot-mode ECC: 128-byte data records, 3 bytes ECC + 1 byte marker ('B')
 *
 */
static struct nand_ecclayout afm_oob_16 = {
	.eccbytes = 7,
	.eccpos = {
		/* { HW_ECC0, HW_ECC1, HW_ECC2, 'A', 'F', 'M', SW_ECC } */
		0, 1, 2, 3, 4, 5, 6},
	.oobfree = {
		{.offset = 7,
		 . length = 9},
	}
};

static struct nand_ecclayout afm_oob_64 = {
	.eccbytes = 28,
	.eccpos = {
		/* { HW_ECC0, HW_ECC1, HW_ECC2, 'A', 'F', 'M', SW_ECC } */
		0,   1,  2,  3,  4,  5,  6,	/* Record 0 */
		16, 17, 18, 19, 20, 21, 22,	/* Record 1 */
		32, 33, 34, 35, 36, 37, 38,	/* Record 2 */
		48, 49, 50, 51, 52, 53, 54,	/* Record 3 */
	},
	.oobfree = {
		{.offset = 7,
		 .length = 9
		},
		{.offset = 23,
		 .length = 9
		},
		{.offset = 39,
		 .length = 9
		},
		{.offset = 55,
		 .length = 9
		},
	}
};

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
static struct nand_ecclayout boot_oob_16 = {
	.eccbytes = 16,
	.eccpos = {
		0, 1, 2, 3,	/* Record 0: ECC0, ECC1, ECC2, 'B' */
		4, 5, 6, 7,	/* Record 1: ECC0, ECC1, ECC2, 'B' */
		8, 9, 10, 11,	/*                  ...            */
		12, 13, 14, 15
	},
	.oobfree = {{0, 0} },	/* No free space in OOB */
};

static struct nand_ecclayout boot_oob_64 = {
	.eccbytes = 64,
	.eccpos = {
		0, 1, 2, 3,	/* Record 0: ECC0, ECC1, ECC2, B */
		4, 5, 6, 7,	/* Record 1: ECC0, ECC1, ECC2, B */
		8,  9, 10, 11,	/*                 ...           */
		12, 13, 14, 15,
		16, 17, 18, 19,
		20, 21, 22, 23,
		24, 25, 26, 27,
		28, 29, 30, 31,
		32, 33, 34, 35,
		36, 37, 38, 39,
		40, 41, 42, 43,
		44, 45, 46, 47,
		48, 49, 50, 51,
		52, 53, 54, 55,
		56, 57, 58, 59,
		60, 61, 62, 63
	},
	.oobfree = {{0, 0} },	/* No free OOB bytes */
};
#endif


static void afm_enable_interrupts(struct stm_nand_afm_controller *afm,
				  uint32_t irqs)
{
	uint32_t reg;

	reg = afm_readreg(NANDHAM_INT_EN);
	reg |= irqs;
	afm_writereg(reg, NANDHAM_INT_EN);

}

static void afm_disable_interrupts(struct stm_nand_afm_controller *afm,
				   uint32_t irqs)
{
	uint32_t reg;

	reg = afm_readreg(NANDHAM_INT_EN);
	reg &= ~irqs;
	afm_writereg(reg, NANDHAM_INT_EN);
}

static irqreturn_t afm_irq_handler(int irq, void *dev)
{
	struct stm_nand_afm_controller *afm =
		(struct stm_nand_afm_controller *)dev;
	unsigned int irq_status;

	irq_status = afm_readreg(NANDHAM_INT_STA);

	/* RBn */
	if (irq_status & NAND_INT_RBN) {
		afm_writereg(NAND_INT_CLR_RBN, NANDHAM_INT_CLR);
		complete(&afm->rbn_completed);
	}

	/* ECC_fix */
	if (irq_status & NANDHAM_INT_ECC_FIX_REQ)
		afm_writereg(NANDHAM_INT_CLR_ECC_FIX_REQ, NANDHAM_INT_CLR);

	/* Seq_req */
	if (irq_status & NANDHAM_INT_SEQ_DREQ) {
		afm_writereg(NANDHAM_INT_CLR_SEQ_DREQ, NANDHAM_INT_CLR);
		complete(&afm->seq_completed);
	}

	return IRQ_HANDLED;
}

/*
 * AFM Initialisation
 */

/* Derive AFM_GEN_CFG data according to device probed */
static uint32_t afm_gen_config(struct stm_nand_afm_controller *afm,
			       uint32_t busw, uint32_t page_size,
			       uint32_t chip_size)
{
	uint32_t reg;

	reg = 0x00;
	/* Bus width */
	if ((busw & NAND_BUSWIDTH_16) == 0)
		reg |= AFM_GEN_CFG_DATA_8_NOT_16;

	/* Page size, Address cycles */
	if (page_size > 512) {
		reg |= AFM_GEN_CFG_LARGE_PAGE;
		if (chip_size > (128 << 20))
			reg |= AFM_GEN_CFG_EXTRA_ADD_CYCLE;
	} else if (chip_size > (32 << 20)) {
		reg |= AFM_GEN_CFG_EXTRA_ADD_CYCLE;
	}

	return reg;
}

/* Derive timing register values from 'stm_nand_timing_data' data.
 *
 * [DEPRECATED in favour of afm_calc_timing_registers() based on 'struct
 * nand_timing_spec' data.]
 */
static void afm_calc_timing_registers_legacy(struct stm_nand_timing_data *tm,
					      struct clk *clk,
					      uint32_t *ctl_timing,
					      uint32_t *wen_timing,
					      uint32_t *ren_timing)
{
	uint32_t n;
	uint32_t reg;

	uint32_t emi_t_ns;

	/* Timings set in terms of EMI clock... */
	if (!clk) {
		pr_warning(NAME
			": No EMI clock available. Using default 100MHz.\n");
		emi_t_ns = 10;
	} else
		emi_t_ns = 1000000000UL / clk_get_rate(clk);

	/* CONTROL_TIMING */
	n = (tm->sig_setup + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->sig_hold + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	n = (tm->CE_deassert + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 16;

	n = (tm->WE_to_RBn + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 24;

	*ctl_timing = reg;

	/* WEN_TIMING */
	n = (tm->wr_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->wr_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	*wen_timing = reg;


	/* REN_TIMING */
	n = (tm->rd_on + emi_t_ns - 1)/emi_t_ns;
	reg = (n & 0xff) << 0;

	n = (tm->rd_off + emi_t_ns - 1)/emi_t_ns;
	reg |= (n & 0xff) << 8;

	*ren_timing = reg;
}

/* Derive timing register values from 'nand_timing_spec' data */
static void afm_calc_timing_registers(struct nand_timing_spec *spec,
				      struct clk *clk,
				      int relax,
				      uint32_t *ctl_timing,
				      uint32_t *wen_timing,
				      uint32_t *ren_timing)
{
	int tCLK;

	int tMAX_HOLD;
	int n_ctl_setup;
	int n_ctl_hold;
	int n_ctl_wb;

	int tMAX_WEN_OFF;
	int n_wen_on;
	int n_wen_off;

	int tMAX_REN_OFF;
	int n_ren_on;
	int n_ren_off;

	/* Get EMI clock (default 100MHz) */
	if (!clk) {
		pr_warning(NAME
			": No EMI clock available. Using default 100MHz.\n");
		tCLK = 10;
	} else
		tCLK = 1000000000 / clk_get_rate(clk);

	/*
	 * CTL_TIMING
	 */

	/*	- SETUP */
	n_ctl_setup = (spec->tCLS - spec->tWP + tCLK - 1)/tCLK;
	if (n_ctl_setup < 1)
		n_ctl_setup = 1;
	n_ctl_setup += relax;

	/*	- HOLD */
	tMAX_HOLD = spec->tCLH;
	if (spec->tCH > tMAX_HOLD)
		tMAX_HOLD = spec->tCH;
	if (spec->tALH > tMAX_HOLD)
		tMAX_HOLD = spec->tALH;
	if (spec->tDH > tMAX_HOLD)
		tMAX_HOLD = spec->tDH;
	n_ctl_hold = (tMAX_HOLD + tCLK - 1)/tCLK + relax;

	/*	- CE_deassert_hold = 0 */

	/*	- WE_high_to_RBn_low */
	n_ctl_wb = (spec->tWB + tCLK - 1)/tCLK;

	*ctl_timing = ((n_ctl_setup & 0xff) |
		       (n_ctl_hold & 0xff) << 8 |
		       (n_ctl_wb & 0xff) << 24);

	/*
	 * WEN_TIMING
	 */

	/*	- ON */
	n_wen_on = (spec->tWH + tCLK - 1)/tCLK - 2;
	if (n_wen_on < 1)
		n_wen_on = 1;
	n_wen_on += relax;

	/*	- OFF */
	tMAX_WEN_OFF = spec->tWC - spec->tWH;
	if (spec->tWP > tMAX_WEN_OFF)
		tMAX_WEN_OFF = spec->tWP;
	n_wen_off = (tMAX_WEN_OFF + tCLK - 1)/tCLK + relax;

	*wen_timing = ((n_wen_on & 0xff) |
		       (n_wen_off & 0xff) << 8);


	/*
	 * REN_TIMING
	 */

	/*	- ON */
	if (spec->tREH == 3 * tCLK) {
		n_ren_on = 2;
	} else {
		n_ren_on = (spec->tREH + tCLK - 1)/tCLK - 1;
		if (n_ren_on < 1)
			n_ren_on = 1;
	}
	n_ren_on += relax;

	/*	- OFF */
	tMAX_REN_OFF = spec->tRC - spec->tREH;
	if (spec->tRP > tMAX_REN_OFF)
		tMAX_REN_OFF = spec->tRP;
	if (spec->tREA > tMAX_REN_OFF)
		tMAX_REN_OFF = spec->tREA;
	n_ren_off = (tMAX_REN_OFF + tCLK - 1)/tCLK + 1 + relax;

	*ren_timing = ((n_ren_on & 0xff) |
		       (n_ren_off & 0xff) << 8);
}

static void afm_init_controller(struct stm_nand_afm_controller *afm)
{
	/* Stop AFM Controller, in case it's still running! */
	afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
	memset(afm->base + NANDHAM_AFM_SEQ_REG_1, 0, 32);

	/* Reset AFM Controller */
	afm_writereg((0x1 << 3), NANDHAM_FLEXMODE_CFG);
	udelay(1);
	afm_writereg(0x00, NANDHAM_FLEXMODE_CFG);

	/* Disable boot_not_flex */
	afm_writereg(0x00000000, NANDHAM_BOOTBANK_CFG);

	/* Set Controller to AFM */
	afm_writereg(0x00000002, NANDHAM_FLEXMODE_CFG);

	/* Enable Interrupts: individual interrupts enabled when needed! */
	afm_writereg(0x0000007C, NANDHAM_INT_CLR);
	afm_writereg(NAND_EDGE_CFG_RBN_RISING, NANDHAM_INT_EDGE_CFG);
	afm_writereg(NAND_INT_ENABLE, NANDHAM_INT_EN);

	/* Configure FLEX Data register for 1-byte Read/Write operation */
	afm_writereg(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAREAD_CONFIG);
	afm_writereg(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);
}

/* Initialise the AFM NAND controller */
static struct stm_nand_afm_controller *
afm_init_resources(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_afm_controller *afm;
	struct resource *resource;
	int err = 0;

	afm = kzalloc(sizeof(struct stm_nand_afm_controller) +
		      (sizeof(struct stm_nand_afm_device *) * pdata->nr_banks),
		       GFP_KERNEL);
	if (!afm) {
		dev_err(&pdev->dev, "failed to allocate afm controller data\n");
		err = -ENOMEM;
		goto err0;
	}

	afm->dev = &pdev->dev;

	/* Request IO Memory */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"nand_mem");
	if (!resource) {
		dev_err(&pdev->dev, "failed to find IOMEM resource\n");
		err = -ENOENT;
		goto err1;
	}
	afm->map_base = resource->start;
	afm->map_size = resource->end - resource->start + 1;

	if (!request_mem_region(afm->map_base, afm->map_size, pdev->name)) {
		dev_err(&pdev->dev, "request memory region failed [0x%08x]\n",
			afm->map_base);
		err = -EBUSY;
		goto err1;
	}

	/* Map base address */
	afm->base = ioremap_nocache(afm->map_base, afm->map_size);
	if (!afm->base) {
		dev_err(&pdev->dev, "base ioremap failed [0x%08x]\n",
			afm->map_base);
		err = -ENXIO;
		goto err2;
	}

	afm->clk = clk_get(&pdev->dev, "emi_clk");

	if (!afm->clk || IS_ERR(afm->clk)) {
		printk(KERN_WARNING NAME ": Failed to find EMI clock.\n");
		afm->clk = NULL;
	} else if (clk_prepare_enable(afm->clk)) {
		printk(KERN_WARNING NAME ": Failed to enable EMI clock.\n");
		clk_put(afm->clk);
		afm->clk = NULL;
	}

#ifdef CONFIG_STM_NAND_AFM_CACHED
	/* Setup cached mapping to the AFM data FIFO */
	afm->fifo_phys = (afm->map_base + NANDHAM_AFM_DATA_FIFO);
#ifndef CONFIG_32BIT
	/* 29-bit uses 'Area 7' address.  [Should this be done in ioremap?] */
	afm->fifo_phys &= 0x1fffffff;
#endif
	afm->fifo_cached = ioremap_cache(afm->fifo_phys, 512);
	if (!afm->fifo_cached) {
		dev_err(&pdev->dev, "fifo ioremap failed [0x%08x]\n",
			afm->map_base + NANDHAM_AFM_DATA_FIFO);
		err = -ENXIO;
		goto err3;
	}

	spin_lock_init(&afm->lock);
#endif

	/* Setup IRQ handler */
	afm->irq = platform_get_irq_byname(pdev, "nand_irq");
	if (afm->irq < 0) {
		dev_err(&pdev->dev, "failed to find IRQ resource\n");
		err = afm->irq;
		goto err4;
	}
	err = request_irq(afm->irq, afm_irq_handler, IRQF_DISABLED,
			  dev_name(&pdev->dev), afm);
	if (err) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto err4;
	}

	/* Initialise 'controller' structure */
	spin_lock_init(&afm->hwcontrol.lock);
	init_waitqueue_head(&afm->hwcontrol.wq);

	init_completion(&afm->rbn_completed);
	init_completion(&afm->seq_completed);

	afm->current_csn = -1;

	afm_init_controller(afm);

	platform_set_drvdata(pdev, afm);

	/* Success :-) */
	return afm;


	/* Error path */
 err4:
#ifdef CONFIG_STM_NAND_AFM_CACHED
	iounmap(afm->fifo_cached);
 err3:
#endif
	iounmap(afm->base);
 err2:
	release_mem_region(afm->map_base, afm->map_size);
 err1:
	kfree(afm);
 err0:
	return ERR_PTR(err);

}

static void afm_exit_controller(struct platform_device *pdev)
{
	struct stm_nand_afm_controller *afm = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	free_irq(afm->irq, afm);
#ifdef CONFIG_STM_NAND_AFM_CACHED
	iounmap(afm->fifo_cached);
#endif
	iounmap(afm->base);
	release_mem_region(afm->map_base, afm->map_size);
	if (afm->clk)
		clk_disable_unprepare(afm->clk);
	kfree(afm);
}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
static void afm_setup_eccparams(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	/* Take a copy of ECC AFM params, as set up during nand_scan() */
	data->ecc_afm.ecc_ctrl = chip->ecc;
	data->ecc_afm.subpagesize = chip->subpagesize;
	data->ecc_afm.subpage_sft = mtd->subpage_sft;

	/* Set ECC BOOT params */
	data->ecc_boot.ecc_ctrl = data->ecc_afm.ecc_ctrl;
	data->ecc_boot.ecc_ctrl.write_page = afm_write_page_boot;
	data->ecc_boot.ecc_ctrl.read_page = afm_read_page_boot;
	data->ecc_boot.ecc_ctrl.size = 128;
	data->ecc_boot.ecc_ctrl.bytes = 4;

	if (mtd->oobsize == 16)
		data->ecc_boot.ecc_ctrl.layout = &boot_oob_16;
	else
		data->ecc_boot.ecc_ctrl.layout = &boot_oob_64;

	data->ecc_boot.ecc_ctrl.layout->oobavail = 0;
	data->ecc_boot.ecc_ctrl.steps = mtd->writesize /
		data->ecc_boot.ecc_ctrl.size;

	if (data->ecc_boot.ecc_ctrl.steps * data->ecc_boot.ecc_ctrl.size !=
	    mtd->writesize) {
		dev_err(data->dev, "invalid ECC parameters\n");
		BUG();
	}
	data->ecc_boot.ecc_ctrl.total = data->ecc_boot.ecc_ctrl.steps *
		data->ecc_boot.ecc_ctrl.bytes;

	/* Disable subpage writes */
	data->ecc_boot.subpage_sft = 0;
	data->ecc_boot.subpagesize = mtd->writesize;
}

/* Switch ECC parameters */
static void afm_set_eccparams(struct mtd_info *mtd, struct ecc_params *params)
{
	struct nand_chip *chip = mtd->priv;

	chip->ecc = params->ecc_ctrl;
	chip->subpagesize = params->subpagesize;

	mtd->oobavail = params->ecc_ctrl.layout->oobavail;
	mtd->subpage_sft = params->subpage_sft;
}

static void afm_select_eccparams(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	if (offs >= data->boot_start &&
	    offs < data->boot_end) {
		if (chip->ecc.layout != data->ecc_boot.ecc_ctrl.layout) {
			dev_dbg(data->dev, "switching to boot-mode ECC\n");
			afm_set_eccparams(mtd, &data->ecc_boot);
		}
	} else {
		if (chip->ecc.layout != data->ecc_afm.ecc_ctrl.layout) {
			dev_dbg(data->dev, "switching to AFM4 ECC\n");
			afm_set_eccparams(mtd, &data->ecc_afm);
		}
	}
}
#endif



/*
 * Internal helper-functions for MTD Interface callbacks
 */

static int afm_do_read_oob(struct mtd_info *mtd, loff_t from,
			   struct mtd_oob_ops *ops)
{
	int page, realpage, chipnr;
	struct nand_chip *chip = mtd->priv;
	int readlen = ops->ooblen;
	int len;
	uint8_t *buf = ops->oobbuf;

	pr_debug("afm_do_read_oob: from = 0x%08Lx, len = %i\n",
	      (unsigned long long)from, readlen);


	if (ops->mode == MTD_OPS_AUTO_OOB)
		len = chip->ecc.layout->oobavail;
	else
		len = mtd->oobsize;

	if (unlikely(ops->ooboffs >= len)) {
		pr_debug("nand_read_oob: "
			"Attempt to start read outside oob\n");
		return -EINVAL;
	}

	/* Do not allow reads past end of device */
	if (unlikely(from >= mtd->size ||
		     ops->ooboffs + readlen >
		     ((mtd->size >> chip->page_shift) -
		      (from >> chip->page_shift)) * len)) {
		pr_debug("nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Shift to get page */
	realpage = (int)(from >> chip->page_shift);
	page = realpage & chip->pagemask;

	while (1) {
#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
		/* Need to check/Update ECC scheme if using PBL boot-boundary */
		afm_select_eccparams(mtd, page << chip->page_shift);
		if (ops->mode == MTD_OPS_AUTO_OOB)
			len = chip->ecc.layout->oobavail;
#endif
		chip->ecc.read_oob(mtd, chip, page);

		len = min(len, readlen);
		buf = nand_transfer_oob(chip, buf, ops, len);

		if (!(chip->options & NAND_NEED_READRDY)) {
			/*
			 * Apply delay or wait for ready/busy pin. Do this
			 * before the AUTOINCR check, so no problems arise if a
			 * chip which does auto increment is marked as
			 * NOAUTOINCR by the board driver.
			 */
			if (!chip->dev_ready)
				udelay(chip->chip_delay);
			else
				nand_wait_ready(mtd);
		}

		readlen -= len;
		if (!readlen)
			break;

		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}
	}

	ops->oobretlen = ops->ooblen;
	return 0;
}

static int afm_do_read_ops(struct mtd_info *mtd, loff_t from,
			   struct mtd_oob_ops *ops)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int chipnr, page, realpage, col, bytes, aligned;
	struct nand_chip *chip = mtd->priv;
	struct mtd_ecc_stats stats;
	int blkcheck = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;
	int sndcmd = 1;
	int ret = 0;
	uint32_t readlen = ops->len;
	uint32_t oobreadlen = ops->ooblen;
	uint8_t *bufpoi, *oob, *buf;
	uint32_t max_oobsize = ops->mode == MTD_OPS_AUTO_OOB ?
		mtd->oobavail : mtd->oobsize;

	pr_debug("afm_do_read_ops: from = 0x%08Lx, len = %i\n",
	      (unsigned long long)from, readlen);

	stats = mtd->ecc_stats;

	chipnr = (int)(from >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	realpage = (int)(from >> chip->page_shift);	/* actual page no.  */
	page = realpage & chip->pagemask;		/* within-chip page */

	col = (int)(from & (mtd->writesize - 1));	/* col within page  */

	buf = ops->datbuf;			/* pointer to data buf */
	oob = ops->oobbuf;			/* pointer to oob buf	*/

	while (1) {
#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
		/* Need to check/Update ECC scheme if using PBL boot-boundary */
		afm_select_eccparams(mtd, page << chip->page_shift);
#endif
		/* #bytes in next transfer */
		bytes = min(mtd->writesize - col, readlen);
		/* tranfer aligned to page? */
		aligned = (bytes == mtd->writesize);

		/* Add test for for alignment */
		if ((uint32_t)buf & 0x3)
			aligned = 0;

		/* Is the current page in the buffer ? */
		if (realpage != chip->pagebuf || oob) {
			bufpoi = aligned ? buf : chip->buffers->databuf;

			afm->page = page;
			afm->col = 0;
			/* Now read the page into the buffer, without ECC if
			 * MTD_OPS_RAW */
			if (unlikely(ops->mode == MTD_OPS_RAW))
				ret = chip->ecc.read_page_raw(mtd, chip,
							      bufpoi, 1, page);
			else
				ret = chip->ecc.read_page(mtd, chip,
							  bufpoi, 1, page);
			if (ret < 0)
				break;

			/* Transfer not aligned data */
			if (!aligned) {
				chip->pagebuf = (ops->mode == MTD_OPS_RAW) ?
					-1 : realpage;
				memcpy(buf, chip->buffers->databuf + col,
				       bytes);
			}

			buf += bytes;

			/* Done page data, now look at OOB... */
			if (unlikely(oob)) {

				int toread = min(oobreadlen, max_oobsize);

				if (toread) {
					oob = nand_transfer_oob(chip,
								oob, ops,
								toread);
					oobreadlen -= toread;
				}
			}
		} else {
			memcpy(buf, chip->buffers->databuf + col, bytes);
			buf += bytes;
		}

		readlen -= bytes;

		if (!readlen)
			break;

		/* For subsequent reads align to page boundary. */
		col = 0;
		/* Increment page address */
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}

		/* Check, if the chip supports auto page increment
		 * or if we have hit a block boundary.
		 */
		if (!NAND_CANAUTOINCR(chip) || !(page & blkcheck))
			sndcmd = 1;
	}

	ops->retlen = ops->len - (size_t) readlen;
	if (oob)
		ops->oobretlen = ops->ooblen - oobreadlen;

	if (ret)
		return ret;

	if (mtd->ecc_stats.failed - stats.failed)
		return -EBADMSG;

	return  mtd->ecc_stats.corrected - stats.corrected ? -EUCLEAN : 0;

}

static int afm_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			uint32_t offset, int data_len, const uint8_t *buf,
			int oob_required, int page, int cached, int raw)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	afm->page = page;
	afm->col = 0;

	if (unlikely(raw))
		chip->ecc.write_page_raw(mtd, chip, buf, oob_required);
	else
		chip->ecc.write_page(mtd, chip, buf, oob_required);

	/* Check status information */
	if (afm->status & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}

#define NOTALIGNED(x)	((x & (chip->subpagesize - 1)) != 0)
static int afm_do_write_ops(struct mtd_info *mtd, loff_t to,
			     struct mtd_oob_ops *ops)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	int chipnr, realpage, page, blockmask, column;
	struct nand_chip *chip = mtd->priv;
	uint32_t writelen = ops->len;

	uint32_t oobwritelen = ops->ooblen;
	uint32_t oobmaxlen = ops->mode == MTD_OPS_AUTO_OOB ?
				mtd->oobavail : mtd->oobsize;

	uint8_t *oob = ops->oobbuf;
	uint8_t *buf = ops->datbuf;
	int ret, subpage;

	int oob_required = oob ? 1 : 0;

	ops->retlen = 0;
	if (!writelen)
		return 0;

	/* reject writes, which are not page aligned */
	if (NOTALIGNED(to) || NOTALIGNED(ops->len)) {
		dev_err(afm->dev, "attempt to write not page aligned data\n");
		return -EINVAL;
	}

	column = to & (mtd->writesize - 1);
	subpage = column || (writelen & (mtd->writesize - 1));

	if (subpage && oob)
		return -EINVAL;

	chipnr = (int)(to >> chip->chip_shift);
	chip->select_chip(mtd, chipnr);

	/* Check, if it is write protected */
	if (nand_check_wp(mtd))
		return -EIO;

	realpage = (int)(to >> chip->page_shift);
	page = realpage & chip->pagemask;
	blockmask = (1 << (chip->phys_erase_shift - chip->page_shift)) - 1;

	/* Invalidate the page cache, when we write to the cached page */
	if (to <= (chip->pagebuf << chip->page_shift) &&
	    (chip->pagebuf << chip->page_shift) < (to + ops->len))
		chip->pagebuf = -1;

	while (1) {
		int bytes = mtd->writesize;
		int cached = writelen > bytes && page != blockmask;
		uint8_t *wbuf = buf;

#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
		/* Need to check/Update ECC scheme if using PBL boot-boundary */
		afm_select_eccparams(mtd, page << chip->page_shift);
#endif
		/* Partial page write ? */
		/* TODO: change alignment constraints for DMA transfer! */
		if (unlikely(column || writelen < (mtd->writesize - 1) ||
			     ((uint32_t)wbuf & 31))) {
			cached = 0;
			bytes = min_t(int, bytes - column, (int) writelen);
			chip->pagebuf = -1;
			memset(chip->buffers->databuf, 0xff, mtd->writesize);
			memcpy(&chip->buffers->databuf[column], buf, bytes);
			wbuf = chip->buffers->databuf;
		}

		if (unlikely(oob)) {
			size_t len = min(oobwritelen, oobmaxlen);
			oob = nand_fill_oob(mtd, oob, len, ops);
			oobwritelen -= len;
		} else {
			/* We still need to erase leftover OOB data */
			memset(chip->oob_poi, 0xff, mtd->oobsize);
		}

		ret = chip->write_page(mtd, chip, column, bytes, wbuf,
					oob_required, page, cached,
					(ops->mode == MTD_OPS_RAW));

		if (ret)
			break;

		writelen -= bytes;
		if (!writelen)
			break;

		column = 0;
		buf += bytes;
		realpage++;

		page = realpage & chip->pagemask;
		/* Check, if we cross a chip boundary */
		if (!page) {
			chipnr++;
			chip->select_chip(mtd, -1);
			chip->select_chip(mtd, chipnr);
		}
	}

	ops->retlen = ops->len - writelen;
	if (unlikely(oob))
		ops->oobretlen = ops->ooblen;
	return ret;
}


/*
 * MTD Interface callbacks (replacing equivalent functions in nand_base.c)
 */


/* MTD Interface - erase block(s) */
static int afm_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	return nand_erase_nand(mtd, instr, 0);
}

/* MTD Interface - Check if block at offset is bad */
static int afm_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;

	/* We should always have a memory-resident BBT */
	BUG_ON(!chip->bbt);

	return nand_isbad_bbt(mtd, offs, 0);
}

/* MTD Interface - Mark block at the given offset as bad */
static int afm_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct nand_chip *chip = mtd->priv;
	uint8_t buf[16];
	int block, ret;

	/* Is is already marked bad? */
	ret = afm_block_isbad(mtd, offs);
	if (ret)
		return (ret > 0) ? 0 : ret;

	/* Update memory-resident BBT */
	BUG_ON(!chip->bbt);
	block = (int)(offs >> chip->bbt_erase_shift);
	chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

	/* Update non-volatile marker... */
	if (chip->bbt_options & NAND_BBT_USE_FLASH) {
		/* Update flash-resident BBT */
		ret = nand_update_bbt(mtd, offs);
	} else {
		/*
		 * Write the bad-block marker to OOB.  We also need to wipe the
		 * first 'AFM' marker (just in case it survived the preceding
		 * failed ERASE) to prevent subsequent on-boot scans from
		 * recognising an AFM block, instead of a marked-bad block.
		 */
		struct mtd_oob_ops ops;
		memset(buf, 0, 16);
		nand_get_device(mtd, FL_WRITING);
		offs += mtd->oobsize;
		ops.mode = MTD_OPS_PLACE_OOB;
		ops.len = 16;
		ops.ooblen = 16;
		ops.datbuf = NULL;
		ops.oobbuf = buf;
		ops.ooboffs = chip->badblockpos & ~0x01;

		ret = nand_do_write_oob(mtd, offs, &ops);
		nand_release_device(mtd);
	}
	if (ret == 0)
		mtd->ecc_stats.badblocks++;

	return ret;
}

/* MTD Interface - Read chunk of page data */
static int afm_read(struct mtd_info *mtd, loff_t from, size_t len,
		    size_t *retlen, uint8_t *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	nand_get_device(mtd, FL_READING);
	afm_select_eccparams(mtd, from);

	ops.len = len;
	ops.datbuf = buf;
	ops.oobbuf = NULL;

	ret = afm_do_read_ops(mtd, from, &ops);

	*retlen = ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/* MTD Interface - read page data and/or OOB */
static int afm_read_oob(struct mtd_info *mtd, loff_t from,
			struct mtd_oob_ops *ops)
{
	int ret = -ENOTSUPP;

	/* Do not allow reads past end of device */
	if (ops->datbuf && (from + ops->len) > mtd->size) {
		pr_debug("nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(mtd, FL_READING);
	afm_select_eccparams(mtd, from);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = afm_do_read_oob(mtd, from, ops);
	else
		ret = afm_do_read_ops(mtd, from, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

/* MTD Interface - write page data */
static int afm_write(struct mtd_info *mtd, loff_t to, size_t len,
			  size_t *retlen, const uint8_t *buf)
{
	int ret;
	struct mtd_oob_ops ops;

	nand_get_device(mtd, FL_WRITING);
	afm_select_eccparams(mtd, to);

	ops.len = len;
	ops.datbuf = (uint8_t *)buf;
	ops.oobbuf = NULL;
	ops.mode = 0;

	ret = afm_do_write_ops(mtd, to, &ops);

	*retlen = ops.retlen;

	nand_release_device(mtd);

	return ret;
}

/* MTD Interface - write page data and/or OOB */
static int afm_write_oob(struct mtd_info *mtd, loff_t to,
			  struct mtd_oob_ops *ops)
{
	int ret = -ENOTSUPP;

	/* Do not allow writes past end of device */
	if (ops->datbuf && (to + ops->len) > mtd->size) {
		pr_debug("nand_read_oob: "
		      "Attempt read beyond end of device\n");
		return -EINVAL;
	}

	nand_get_device(mtd, FL_WRITING);
	afm_select_eccparams(mtd, to);

	switch (ops->mode) {
	case MTD_OPS_PLACE_OOB:
	case MTD_OPS_AUTO_OOB:
	case MTD_OPS_RAW:
		break;

	default:
		goto out;
	}

	if (!ops->datbuf)
		ret = nand_do_write_oob(mtd, to, ops);
	else
		ret = afm_do_write_ops(mtd, to, ops);

 out:
	nand_release_device(mtd);
	return ret;
}

/*
 * AFM data transfer routines
 */

#ifdef CONFIG_STM_NAND_AFM_CACHED
static void afm_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	unsigned long irq_flags;

	int lenaligned = len & ~(L1_CACHE_BYTES-1);
	int lenleft = len & (L1_CACHE_BYTES-1);

	while (lenaligned > 0) {
		spin_lock_irqsave(&(afm->lock), irq_flags);
		invalidate_ioremap_region(afm->fifo_phys, afm->fifo_cached,
					  0, L1_CACHE_BYTES);
		stm_nand_memcpy_fromio(buf, afm->fifo_cached, L1_CACHE_BYTES);
		spin_unlock_irqrestore(&(afm->lock), irq_flags);

		buf += L1_CACHE_BYTES;
		lenaligned -= L1_CACHE_BYTES;
	}

#ifdef CONFIG_STM_L2_CACHE
	/* If L2 cache is enabled, we must ensure the cacheline is evicted prior
	 * to non-cached accesses..
	 */
	invalidate_ioremap_region(afm->fifo_phys, afm->fifo_cached,
				  0, L1_CACHE_BYTES);
#endif
	/* Mop up any remaining bytes */
	while (lenleft > 0) {
		*buf++ = readb(afm->base + NANDHAM_AFM_DATA_FIFO);
		lenleft--;
	}
}
#else
/* Default read buffer command */
static void afm_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	readsl(afm->base + NANDHAM_AFM_DATA_FIFO, buf, len/4);
}
#endif

/* Default write buffer command */
static void afm_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	writesl(afm->base + NANDHAM_AFM_DATA_FIFO, buf, len/4);
}


/*
 * AFM low-level chip operations
 */

/* AFM: Block Erase */
static void afm_erase_cmd(struct mtd_info *mtd, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	struct afm_prog	*prog;
	struct nand_chip *chip = mtd->priv;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Initialise Seq interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/* Select AFM program */
	if ((mtd->writesize == 512 && chip->chipsize > (32 << 20)) ||
	    (mtd->writesize == 2048 && chip->chipsize > (128 << 20)))
		/* For 'large' chips, we need an extra address cycle */
		prog = &afm_prog_erase_ext;
	else
		prog = &afm_prog_erase;

	/* Set page address */
	prog->extra_reg	= page;

	/* Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Wait for sequence to finish */
	ret = wait_for_completion_timeout(&afm->seq_completed, 2*HZ);
	if (!ret) {
		dev_warn(afm->dev, "%s: Seq timeout, force exit!\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return;
	}

	/* Get status */
	reg = afm_readreg(NANDHAM_AFM_SEQ_STA);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable interrupts */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
}

/* AFM: Read Page and OOB Data with ECC */
static int afm_read_page_ecc(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int oob_required, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int i, j;
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;

	uint8_t *p = buf;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *e1, *e2, t;

	/* Read raw page+oob data */
	chip->ecc.read_page_raw(mtd, chip, buf, 1, page);

	/* Recover ECC from OOB: H/W ECC + S/W LP16,17 from device */
	e1 = ecc_code;
	for (i = 0; i < chip->ecc.total; i++)
		e1[i] = chip->oob_poi[eccpos[i]];

	/* Generate linux-style 'ecc_code' and 'ecc_calc' */
	e1 = ecc_code;
	e2 = ecc_calc;
	p = buf;
	for (i = 0; i < eccsteps; i++) {
		uint32_t ecc_afm;
		uint8_t lp1617;
		uint32_t chkcode_offs;

		/* Get H/W ECC */
		chkcode_offs = (eccsteps == 1) ? (3 << 2) : (i << 2);
		ecc_afm = afm_readreg(NANDHAM_AFM_ECC_REG_0 + chkcode_offs);

		/* Extract ecc_code and ecc_calc */
		for (j = 0; j < 3; j++) {
			e2[j] = (unsigned char)(ecc_afm & 0xff);
			ecc_afm >>= 8;
		}

		/* Swap e[0] and e[1] */
		t = e1[0]; e1[0] = e1[1]; e1[1] = t;
		t = e2[0]; e2[0] = e2[1]; e2[1] = t;

		/* Generate S/W LP1617 ecc_calc */
		lp1617 = stm_afm_lp1617(p);

		/* Copy S/W LP1617 bits to standard location */
		e1[2] = (e1[2] & 0xfc) | (e1[6] & 0x3);
		e2[2] = (e2[2] & 0xfc) | (lp1617 & 0x3);

		/* Move on to next ECC block */
		e1 += eccbytes;
		e2 += eccbytes;
		p += eccsize;
	}

	/* Check for empty page before attempting ECC fixes */
	if (stmnand_test_empty_page(ecc_code, ecc_calc, eccsteps, eccbytes,
				    buf, NULL, mtd->writesize, 0, 1))
		return 0;

	/* Detect/Correct ECC errors */
	p = buf;
	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = nand_correct_data(mtd, p, &ecc_code[i], &ecc_calc[i]);

		if (stat == -1) {
			mtd->ecc_stats.failed++;
			printk(KERN_CONT "step %d, page %d]\n",
			       i / eccbytes, page);
		} else if (stat == 1) {
			printk(KERN_CONT "step %d, page = %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.corrected++;
		}
	}

	return  0;
}

/* AFM: Read Raw Page and OOB Data */
static int afm_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int oob_required, int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	struct afm_prog	*prog;
	uint32_t reg;
	int ret;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Select AFM program */
	prog = (mtd->writesize == 512) ?
		&afm_prog_read_raw_sp :
		&afm_prog_read_raw_lp;

	/* Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, NAND_INT_RBN);

	if (mtd->writesize == 512) {
		/* SmallPage: Reset ECC counters */
		reg = afm_readreg(NANDHAM_FLEXMODE_CFG);
		reg |= (1 << 6);
		afm_writereg(reg, NANDHAM_FLEXMODE_CFG);
	}

	/* Set page address */
	prog->addr_reg = page << 8;

	/* Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Wait for data to become available */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: RBn timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm_disable_interrupts(afm, NAND_INT_RBN);
		return 1;
	}
	/* Read page data and OOB (SmallPage: +48 bytes dummy data) */
	afm_read_buf(mtd, buf, mtd->writesize);
	afm_read_buf(mtd, chip->oob_poi, 64);

	/* Disable RBn interrupts */
	afm_disable_interrupts(afm, NAND_INT_RBN);

	return 0;
}

/* AFM: Read OOB data */
static int afm_read_oob_chip(struct mtd_info *mtd, struct nand_chip *chip,
		int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog	*prog;
	uint32_t reg;
	int ret;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Select AFM program */
	prog = (mtd->writesize == 512) ?
		&afm_prog_read_oob_sp :
		&afm_prog_read_oob_lp;

	/* Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, NAND_INT_RBN);

	if (mtd->writesize == 512) {
		/* SmallPage: Reset ECC counters and set page address */
		reg = afm_readreg(NANDHAM_FLEXMODE_CFG);
		reg |= (1 << 6);
		afm_writereg(reg, NANDHAM_FLEXMODE_CFG);

		prog->addr_reg	= page << 8;
	} else {
		/* LargePage: Set page address */
		prog->addr_reg	= (page << 8) | (2048 >> 8);
	}

	/* Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Wait for data to become available */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: RBn timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm_disable_interrupts(afm, NAND_INT_RBN);
		return 1;
	}

	/* Read OOB data to chip->oob_poi buffer */
	afm_read_buf(mtd, chip->oob_poi, 64);

	/* Disable RBn Interrupts */
	afm_disable_interrupts(afm, NAND_INT_RBN);

	return 0;
}

/* AFM: Write Page and OOB Data, with ECC support [SmallPage] */
static int afm_write_page_ecc_sp(struct mtd_info *mtd, struct nand_chip *chip,
			const uint8_t *buf, int oob_required)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_ecc_sp_a;
	uint32_t ecc_afm;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* 1. Use AFM to write page data to chip's page buffer */

	/*    Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/*    Initialise Seq interrupt */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/*    Reset ECC counters */
	reg = afm_readreg(NANDHAM_FLEXMODE_CFG);
	reg |= (1 << 6);
	afm_writereg(reg, NANDHAM_FLEXMODE_CFG);

	/*    Set page address */
	prog->addr_reg	= afm->page << 8;

	/*    Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/*    Write page data */
	afm_write_buf(mtd, buf, mtd->writesize);

	/*    Wait for the sequence to terminate */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: Seq timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return -ETIMEDOUT;
	}

	/*    Disable Seq interrupt */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/* 2. Use FLEX Mode to write OOB data */

	/*    Collect AFM ECC from Controller and populate OOB  */
	ecc_afm = afm_readreg(NANDHAM_AFM_ECC_REG_3);
	chip->oob_poi[0] = ecc_afm & 0xff; ecc_afm >>= 8;
	chip->oob_poi[1] = ecc_afm & 0xff; ecc_afm >>= 8;
	chip->oob_poi[2] = ecc_afm & 0xff;
	chip->oob_poi[3] = 'A';
	chip->oob_poi[4] = 'F';
	chip->oob_poi[5] = 'M';
	chip->oob_poi[6] = stm_afm_lp1617(buf);

	/*    Enable FLEX Mode */
	afm_writereg(CFG_ENABLE_FLEX, NANDHAM_FLEXMODE_CFG);

	/*    Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, NAND_INT_RBN);

	/*    Write OOB data */
	afm_writereg(FLEX_DATA_CFG_BEATS_4 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + NANDHAM_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);

	/*    Issue page program command */
	afm_writereg(FLEX_CMD(NAND_CMD_PAGEPROG), NANDHAM_FLEX_CMD);

	/*    Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "%s: RBn timeout, force exit\n", __func__);

	/*    Get status */
	afm_writereg(FLEX_CMD(NAND_CMD_STATUS), NANDHAM_FLEX_CMD);
	afm->status = afm_readreg(NANDHAM_FLEX_DATA);

	/*    Disable RBn Interrupt */
	afm_disable_interrupts(afm, NAND_INT_RBN);

	return 0;
}


/* AFM: Write Page and OOB Data, with ECC support [LargePage]  */
static int afm_write_page_ecc_lp(struct mtd_info *mtd,
				  struct nand_chip *chip,
				  const uint8_t *buf, int oob_required)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	const uint8_t *p;

	struct afm_prog *prog = &afm_prog_write_ecc_lp;

	uint32_t reg;
	int ret;
	int i;

	afm->status = 0;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Calc S/W ECC LP1617, insert into OOB area with 'AFM' signature */
	p = buf;
	for (i = 0; i < eccsteps; i++) {
		chip->oob_poi[eccpos[3+i*eccbytes]] = 'A';
		chip->oob_poi[eccpos[4+i*eccbytes]] = 'F';
		chip->oob_poi[eccpos[5+i*eccbytes]] = 'M';
		chip->oob_poi[eccpos[6+i*eccbytes]] = stm_afm_lp1617(p);
		p += eccsize;
	}

	/* Initialise Seq interrupt */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/* Reset ECC counters */
	reg = afm_readreg(NANDHAM_FLEXMODE_CFG);
	reg |= (1 << 6);
	afm_writereg(reg, NANDHAM_FLEXMODE_CFG);

	/* Set page address */
	prog->addr_reg	= afm->page << 8;

	/* Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Write page and oob data */
	afm_write_buf(mtd, buf, mtd->writesize);
	afm_write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: Seq timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return -ETIMEDOUT;
	}

	/* Get status */
	reg = afm_readreg(NANDHAM_AFM_SEQ_STA);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable Seq interrupt */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	return 0;
}

/* AFM: Write Raw Page and OOB Data [LargePage] */
static int afm_write_page_raw_lp(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf, int oob_required)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_raw_lp;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Initialise Seq Interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/* Set page address */
	prog->addr_reg	= afm->page << 8;

	/* Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Write page and OOB data */
	afm_write_buf(mtd, buf, 2048);
	afm_write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: Seq timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return -ETIMEDOUT;
	}

	/* Get status */
	reg = afm_readreg(NANDHAM_AFM_SEQ_STA);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable Seq Interrupts */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	return 0;
}

/* AFM: Write Raw Page and OOB Data [SmallPage] */
static int afm_write_page_raw_sp(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf, int oob_required)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_raw_sp_a;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* 1. Use AFM to write page data to chip's page buffer */

	/*    Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/*    Initialise Seq interrupt */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	/*    Set page address */
	prog->addr_reg	= afm->page << 8;

	/*    Copy program to controller, and start sequence */
	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/*    Write page data */
	afm_write_buf(mtd, buf, mtd->writesize);

	/*    Wait for the sequence to terminate */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ/2);
	if (!ret) {
		dev_err(afm->dev, "%s: Seq timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return -ETIMEDOUT;
	}

	/*    Disable Seq interrupt */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	reg = afm_readreg(NANDHAM_AFM_SEQ_STA);
	if ((reg & 0x60) != 0) {
		dev_err(afm->dev, "error programming page data\n");
		afm->status = NAND_STATUS_FAIL;
		return NAND_STATUS_FAIL;
	}

	/* 2. Use FLEX Mode to write OOB data */

	/*    Enable FLEX Mode */
	afm_writereg(CFG_ENABLE_FLEX, NANDHAM_FLEXMODE_CFG);

	/*    Initialise RBn interrupt */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, NAND_INT_RBN);

	/*    Send OOB pointer operation */
	afm_writereg(FLEX_CMD(NAND_CMD_READOOB),  NANDHAM_FLEX_CMD);

	/*    Send SEQIN command */
	afm_writereg(FLEX_CMD(NAND_CMD_SEQIN), NANDHAM_FLEX_CMD);

	/*    Send page address */
	reg = afm->page << 8 | FLEX_ADDR_ADD8_VALID |
		FLEX_ADDR_CSN;
	if (chip->chipsize > (32 << 20))
		reg |= FLEX_ADDR_BEATS_4;
	else
		reg |= FLEX_ADDR_BEATS_3;
	afm_writereg(reg, NANDHAM_FLEX_ADD);

	/*    Send OOB data */
	afm_writereg(FLEX_DATA_CFG_BEATS_4 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + NANDHAM_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);

	/*    Send page program command */
	afm_writereg(FLEX_CMD(NAND_CMD_PAGEPROG),  NANDHAM_FLEX_CMD);

	/* Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "%s: RBn timeout!\n", __func__);

	/*     Get status */
	afm_writereg(FLEX_CMD(NAND_CMD_STATUS), NANDHAM_FLEX_CMD);
	afm->status = afm_readreg(NANDHAM_FLEX_DATA);

	afm_disable_interrupts(afm, NAND_INT_RBN);

	return 0;
}

/* AFM: Write OOB Data [LargePage] */
static int afm_write_oob_chip_lp(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct afm_prog *prog = &afm_prog_write_oob_lp;
	uint32_t reg;
	int ret;

	afm->status = 0;

	/* Enable AFM */
	afm_writereg(CFG_ENABLE_AFM, NANDHAM_FLEXMODE_CFG);

	/* Enable sequence interrupts */
	INIT_COMPLETION(afm->seq_completed);
	afm_enable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	prog->addr_reg	= (page << 8) | (2048 >> 8);

	stm_nand_memcpy_toio(afm->base + NANDHAM_AFM_SEQ_REG_1, prog, 32);

	/* Write OOB */
	afm_write_buf(mtd, chip->oob_poi, 64);

	/* Wait for sequence to complete */
	ret = wait_for_completion_timeout(&afm->seq_completed, HZ);
	if (!ret) {
		dev_err(afm->dev, "%s: Seq timeout, force exit\n", __func__);
		afm_writereg(0x00000000, NANDHAM_AFM_SEQ_CFG);
		afm->status = NAND_STATUS_FAIL;
		afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);
		return 1;
	}

	/* Get status */
	reg = afm_readreg(NANDHAM_AFM_SEQ_STA);
	afm->status =  NAND_STATUS_READY | ((reg & 0x60) >> 5);

	/* Disable sequence interrupts */
	afm_disable_interrupts(afm, NANDHAM_INT_SEQ_DREQ);

	return afm->status & NAND_STATUS_FAIL ? -EIO : 0;
}

/* AFM: Write OOB Data [SmallPage] */
static int afm_write_oob_chip_sp(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int ret;
	uint32_t status;
	uint32_t reg;

	afm->status = 0;

	/* Enable FLEX Mode */
	afm_writereg(CFG_ENABLE_FLEX, NANDHAM_FLEXMODE_CFG);

	/* Initialise interrupts */
	INIT_COMPLETION(afm->rbn_completed);
	afm_enable_interrupts(afm, NAND_INT_RBN);

	/* Pointer Operation */
	afm_writereg(FLEX_CMD(NAND_CMD_READOOB), NANDHAM_FLEX_CMD);

	/* Send SEQIN command */
	afm_writereg(FLEX_CMD(NAND_CMD_SEQIN), NANDHAM_FLEX_CMD);

	/* Send Col/Page Addr */
	reg = (page << 8 |
	       FLEX_ADDR_RBN |
	       FLEX_ADDR_ADD8_VALID |
	       FLEX_ADDR_CSN);

	if (chip->chipsize > (32 << 20))
		/* Need extra address cycle for 'large' devices */
		reg |= FLEX_ADDR_BEATS_4;
	else
		reg |= FLEX_ADDR_BEATS_3;
	afm_writereg(reg, NANDHAM_FLEX_ADD);

	/* Write OOB data */
	afm_writereg(FLEX_DATA_CFG_BEATS_4 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);
	writesl(afm->base + NANDHAM_FLEX_DATA, chip->oob_poi, 4);
	afm_writereg(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		     NANDHAM_FLEX_DATAWRITE_CONFIG);

	/* Issue Page Program command */
	afm_writereg(FLEX_CMD(NAND_CMD_PAGEPROG), NANDHAM_FLEX_CMD);

	/* Wait for page program operation to complete */
	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "%s: RBn timeout\n", __func__);

	/* Get status */
	afm_writereg(FLEX_CMD(NAND_CMD_STATUS), NANDHAM_FLEX_CMD);
	status = afm_readreg(NANDHAM_FLEX_DATA);

	afm->status = 0xff & status;

	/* Disable RBn interrupts */
	afm_disable_interrupts(afm, NAND_INT_RBN);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
/* For boot-mode, we use software ECC with AFM raw read/write commands */
static int boot_calc_ecc(struct mtd_info *mtd, const unsigned char *buf,
			 unsigned char *ecc)
{
	stm_ecc_gen(buf, ecc, ECC_128);
	ecc[3] = 'B';

	return 0;
}

static int boot_correct_ecc(struct mtd_info *mtd, unsigned char *buf,
			    unsigned char *read_ecc, unsigned char *calc_ecc)
{
	int status;

	status = stm_ecc_correct(buf, read_ecc, calc_ecc, ECC_128);

	/* convert to MTD-compatible status */
	if (status == E_NO_CHK)
		return 0;
	if (status == E_D1_CHK || status == E_C1_CHK)
		return 1;

	return -1;
}

static int afm_read_page_boot(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int oob_required, int page)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	chip->ecc.read_page_raw(mtd, chip, buf, 1, page);

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		boot_calc_ecc(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	eccsteps = chip->ecc.steps;
	p = buf;

	/* Check for empty page before attempting ECC fixes */
	if (stmnand_test_empty_page(ecc_code, ecc_calc, eccsteps, eccbytes,
				    buf, NULL, mtd->writesize, 0, 1))
		return 0;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = boot_correct_ecc(mtd, p, &ecc_code[i], &ecc_calc[i]);
		if (stat == -1) {
			printk(KERN_CONT "step %d, page %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.failed++;
		} else if (stat == 1) {
			printk(KERN_CONT "step %d, page = %d]\n",
			       i / eccbytes, page);
			mtd->ecc_stats.corrected += stat;
		}
	}
	return 0;
}

static int afm_write_page_boot(struct mtd_info *mtd,
				struct nand_chip *chip,
				const uint8_t *buf,
				int oob_required)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	/* Software ecc calculation */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
		boot_calc_ecc(mtd, p, &ecc_calc[i]);

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	chip->ecc.write_page_raw(mtd, chip, buf, 1);

	return 0;
}
#endif /* CONFIG_STM_NAND_AFM_BOOTMODESUPPORT */


/* AFM : Select Chip
 * For now we only support 1 chip per 'stm_nand_afm_device' so chipnr will be 0
 * for select, -1 for deselect.
 */
static void afm_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	/* Deselect, do nothing */
	if (chipnr == -1) {
		return;
	} else if (chipnr == 0) {
		/* If same chip as last time, no need to change anything */
		if (data->csn == afm->current_csn)
			return;

		/* Set CSn on AFM controller */
		afm->current_csn = data->csn;
		afm_writereg(0x1 << data->csn, NANDHAM_FLEX_MUXCTRL);

		/* Update AFM_GEN_CFG */
		dev_dbg(afm->dev, "updating generic configuration [0x%08x]\n",
			data->afm_gen_cfg);
		afm_writereg(data->afm_gen_cfg, NANDHAM_AFM_GEN_CFG);

		/* Configure timing registers */
		if (data->ctl_timing) {
			dev_dbg(afm->dev, "updating timing configuration [0x%08x, 0x%08x, 0x%08x]\n",
				data->ctl_timing,
				data->wen_timing,
				data->ren_timing);
			afm_writereg(data->ctl_timing, NANDHAM_CTL_TIMING);
			afm_writereg(data->wen_timing, NANDHAM_WEN_TIMING);
			afm_writereg(data->ren_timing, NANDHAM_REN_TIMING);
		}
	} else {
		dev_err(afm->dev, "attempt to select chipnr = %d\n", chipnr);
	}

	return;
}

/*
 * Partial implementation of 'chip->cmdfunc' interface, based on Hamming-FLEX
 * operation.
 *
 * Allows us to make use of nand_base.c functions where possible
 * (e.g. nand_scan_ident() and friends).
 */
static int flex_wait_rbn(struct stm_nand_afm_controller *afm)
{
	int ret;

	ret = wait_for_completion_timeout(&afm->rbn_completed, HZ/2);
	if (!ret)
		dev_err(afm->dev, "%s: FLEX RBn timeout\n", __func__);

	return ret;
}

static void flex_cmd(struct stm_nand_afm_controller *afm, uint8_t cmd)
{
	uint32_t val;

	val = (FLEX_CMD_CSN | FLEX_CMD_BEATS_1 | cmd);

	afm_writereg(val, NANDHAM_FLEX_CMD);
}

static void flex_addr(struct stm_nand_afm_controller *afm,
		      uint32_t addr, int cycles)
{
	addr &= 0x00ffffff;

	BUG_ON(cycles < 1);
	BUG_ON(cycles > 3);

	addr |= (FLEX_ADDR_CSN | FLEX_ADDR_ADD8_VALID);
	addr |= (cycles & 0x3) << 28;

	afm_writereg(addr, NANDHAM_FLEX_ADD);
}

static uint8_t flex_read_byte(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	return (uint8_t)(afm_readreg(NANDHAM_FLEX_DATA) & 0xff);
}

static void flex_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int aligned;

	/* Read bytes until buf is 4-byte aligned */
	while (len && ((unsigned int)buf & 0x3)) {
		*buf++ = (uint8_t)(readl(afm->base + NANDHAM_FLEX_DATA)
				   & 0xff);
		len--;
	};

	/* Use 'BEATS_4'/readsl */
	if (len > 8) {
		aligned = len & ~0x3;
		writel(FLEX_DATA_CFG_BEATS_4 | FLEX_DATA_CFG_CSN,
		       afm->base + NANDHAM_FLEX_DATAREAD_CONFIG);

		readsl(afm->base + NANDHAM_FLEX_DATA, buf, aligned >> 2);

		buf += aligned;
		len -= aligned;

		writel(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		       afm->base + NANDHAM_FLEX_DATAREAD_CONFIG);
	}

	/* Mop up remaining bytes */
	while (len > 0) {
		*buf++ = (uint8_t)(readl(afm->base + NANDHAM_FLEX_DATA)
				   & 0xff);
		len--;
	}
}

static void flex_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int aligned;

	/* Write bytes until buf is 4-byte aligned */
	while (len && ((unsigned int)buf & 0x3)) {
		writel(*buf++, afm->base + NANDHAM_FLEX_DATA);
		len--;
	};

	/* USE 'BEATS_4/writesl */
	if (len > 8) {
		aligned = len & ~0x3;
		writel(FLEX_DATA_CFG_BEATS_4 | FLEX_DATA_CFG_CSN,
		       afm->base + NANDHAM_FLEX_DATAWRITE_CONFIG);
		writesl(afm->base + NANDHAM_FLEX_DATA, buf, aligned >> 2);
		buf += aligned;
		len -= aligned;
		writel(FLEX_DATA_CFG_BEATS_1 | FLEX_DATA_CFG_CSN,
		       afm->base + NANDHAM_FLEX_DATAWRITE_CONFIG);
	}

	/* Mop up remaining bytes */
	while (len > 0) {
		writel(*buf++, afm->base + NANDHAM_FLEX_DATA);
		len--;
	}
}

/* Wait for device to become ready, and return the status register */
static int afm_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int status;

	if (afm_readreg(NANDHAM_FLEXMODE_CFG) & CFG_ENABLE_AFM) {
		/* All AFM-based routines employ their own wait function,
		 * so all that remains is to return the saved status */
		status = afm->status;
	} else {
		/* If we were executing a FLEX-based routine, wait for RBn and
		 * read the status register. */
		flex_wait_rbn(afm);
		flex_cmd(afm, NAND_CMD_STATUS);
		status = (int)(afm_readreg(NANDHAM_FLEX_DATA) & 0xff);
	}

	return status;
}

static void flex_command(struct mtd_info *mtd, unsigned int command,
			 int column, int page)
{
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);

	/* Enable FLEX Mode */
	afm_writereg(CFG_ENABLE_FLEX, NANDHAM_FLEXMODE_CFG);

	switch (command) {
	case NAND_CMD_RESET:
	case NAND_CMD_PARAM:
		/* Prime RBn wait */
		INIT_COMPLETION(afm->rbn_completed);
		afm_enable_interrupts(afm, NAND_INT_RBN);
		break;
	case NAND_CMD_READID:
	case NAND_CMD_STATUS:
		break;
	default:
		/* Catch unexpected commands */
		BUG();
	}

	/*
	 * Command Cycle
	 */
	flex_cmd(afm, command);

	/*
	 * Address Cycles
	 */
	if (column != -1)
		flex_addr(afm, column,
			  (command == NAND_CMD_READID) ? 1 : 2);
	if (page != -1)
		flex_addr(afm, page, (chip->chipsize > (128 << 20)) ? 3 : 2);

	/*
	 * Wait for RBn, if required.
	 */
	if (command == NAND_CMD_RESET ||
	    command == NAND_CMD_PARAM) {
		flex_wait_rbn(afm);
		afm_disable_interrupts(afm, NAND_INT_RBN);
	}
}

static u16 afm_read_word_BUG(struct mtd_info *mtd)
{
	printk(KERN_ERR NAME ": %s Unsupported callback", __func__);
	BUG();

	return 0xff;
}

static int afm_block_bad_BUG(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	printk(KERN_ERR NAME ": %s Unsupported callback", __func__);
	BUG();

	return 1;
}

static int afm_default_block_markbad_BUG(struct mtd_info *mtd, loff_t ofs)
{
	printk(KERN_ERR NAME ": %s Unsupported callback", __func__);
	BUG();

	return 1;
}

/*
 * AFM scan/probe NAND routines
 */

/* Set AFM generic call-backs (not chip-specific) */
static void afm_set_defaults(struct nand_chip *chip)
{
	chip->chip_delay = 0;
	chip->cmdfunc = flex_command;
	chip->waitfunc = afm_wait;
	chip->select_chip = afm_select_chip;
	chip->read_byte = flex_read_byte;
	chip->write_buf = flex_write_buf;
	chip->read_buf = flex_read_buf;

	/* Unsupported callbacks */
	chip->read_word = afm_read_word_BUG;
	chip->block_bad = afm_block_bad_BUG;
	chip->block_markbad = afm_default_block_markbad_BUG;

	chip->scan_bbt = stmnand_scan_bbt;
}

/* Configure AFM according to device parameters */
static int afm_scan_tail(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	int i;
	struct nand_chip *chip = mtd->priv;

	int ret;
	if (!(chip->options & NAND_OWN_BUFFERS)) {
		chip->buffers = kmalloc(sizeof(*chip->buffers), GFP_KERNEL);
	}
	if (!chip->buffers) {
		return -ENOMEM;
	}

	/* Set the internal oob buffer location, just after the page data */
	chip->oob_poi = chip->buffers->databuf + mtd->writesize;

	if (mtd->writesize == 512 && mtd->oobsize == 16) {
		chip->ecc.layout = &afm_oob_16;
	} else if (mtd->writesize == 2048 && mtd->oobsize == 64) {
		chip->ecc.layout = &afm_oob_64;
	} else {
		dev_err(afm->dev, "Unsupported chip type "
			"[pagesize = %d, oobsize = %d]\n",
			mtd->writesize, mtd->oobsize);
		return 1;
	}

	/* Use our own 'erase_cmd', not the one set in nand_get_flash_type() */
	chip->erase_cmd = afm_erase_cmd;

	/* Set ECC parameters and call-backs */
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = 512;
	chip->ecc.bytes = 7;
	chip->write_page = afm_write_page;
	chip->ecc.read_page = afm_read_page_ecc;
	chip->ecc.read_page_raw = afm_read_page_raw;
	chip->ecc.read_oob = afm_read_oob_chip;
	if (mtd->writesize == 512) {
		chip->ecc.write_page = afm_write_page_ecc_sp;
		chip->ecc.write_page_raw = afm_write_page_raw_sp;
		chip->ecc.write_oob = afm_write_oob_chip_sp;
	} else {
		chip->ecc.write_page = afm_write_page_ecc_lp;
		chip->ecc.write_page_raw = afm_write_page_raw_lp;
		chip->ecc.write_oob = afm_write_oob_chip_lp;
	}
	chip->ecc.read_oob_raw = chip->ecc.read_oob;
	chip->ecc.write_oob_raw = chip->ecc.write_oob;
	chip->ecc.steps = mtd->writesize / chip->ecc.size;
	if (chip->ecc.steps * chip->ecc.size != mtd->writesize) {
		dev_err(afm->dev, "Invalid ecc parameters\n");
		return 1;
	}
	chip->ecc.total = chip->ecc.steps * chip->ecc.bytes;
	chip->ecc.calculate = NULL;	/* Hard-coded in call-backs */
	chip->ecc.correct = NULL;

	/* Count the number of OOB bytes available for client data */
	chip->ecc.layout->oobavail = 0;
	for (i = 0; chip->ecc.layout->oobfree[i].length
		     && i < MTD_MAX_OOBFREE_ENTRIES; i++)
		chip->ecc.layout->oobavail +=
			chip->ecc.layout->oobfree[i].length;
	mtd->oobavail = chip->ecc.layout->oobavail;

	/* Disable subpage writes for now */
	mtd->subpage_sft = 0;
	chip->subpagesize = mtd->writesize >> mtd->subpage_sft;

	/* Initialize state */
	chip->state = FL_READY;

	/* De-select the device */
	chip->select_chip(mtd, -1);

	/* Invalidate the pagebuffer reference */
	chip->pagebuf = -1;

	/* Fill in remaining MTD driver data */
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = afm_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = afm_read;
	mtd->_write = afm_write;
	mtd->_read_oob = afm_read_oob;
	mtd->_write_oob = afm_write_oob;
	mtd->_sync = nand_sync;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_suspend = nand_suspend;
	mtd->_resume = nand_resume;
	mtd->_block_isbad = afm_block_isbad;
	mtd->_block_markbad = afm_block_markbad;
	mtd->writebufsize = mtd->writesize;

	/* Propagate ecc.layout to mtd_info */
	mtd->ecclayout = chip->ecc.layout;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	/* Setup ECC params, for AFM and BOOT operation */
	afm_setup_eccparams(mtd);
#endif

	/* Check, if we should skip the bad block table scan */
	if (chip->options & NAND_SKIP_BBTSCAN)
		return 0;

	/* Build bad block table */
	ret = chip->scan_bbt(mtd);

	return ret;
}

#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY

#define PBL_BOOT_BOUNDARY_POINTER	0x0034
#define PBL_MARKER1			0x09b1b007
#define PBL_MARKER2			(~(PBL_MARKER1))

/* The NAND_BLOCK_ZERO_REMAP_REG is not implemented on current versions of the
 * NAND controller.  We must therefore scan for the logical block 0 pattern.
 */
static int check_block_zero_pattern(uint8_t *buf)
{
	uint8_t i;
	uint8_t *b;

	for (i = 0, b = buf + 128; i < 64; i++, b++)
		if (*b != i)
			return 1;

	return 0;
}

static int find_block_zero(struct mtd_info *mtd)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;
	uint8_t *buf = chip->buffers->databuf;

	int block, block_end;
	uint32_t offs;
	uint32_t page;
	int pages_per_block;

	/* Select boot-mode ECC */
	afm_select_eccparams(mtd, data->boot_start);

	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);

	block_end = min(512UL, (data->boot_end >> chip->phys_erase_shift));

	chip->pagebuf = -1;

	for (block = 0; block < block_end; block++) {
		offs = block << chip->phys_erase_shift;

		/* Skip if block is bad */
		if (nand_isbad_bbt(mtd, offs, 0))
			continue;

		page = block * pages_per_block;
		afm->page = page;

		afm_read_page_boot(mtd, chip, buf, 1, page);

		if (check_block_zero_pattern(buf) == 0)
			return block;
	}

	return -1;
}

static int find_pbl_marker(uint8_t *buf, int len, int *col)
{
	uint32_t *s = (uint32_t *)buf;
	int i;

	/* The PBL offset must also be present in the buffer for success, '0',
	 * to be returned. */
	for (i = 0; i < (len / 4) - 2; i++) {
		if ((s[i] == PBL_MARKER1) && (s[i + 1] == PBL_MARKER2)) {
			*col = i * 4;
			return 0;
		}
	}

	return 1;
}

/* Find the offset of the PBL.  Note, the PBL is not always located at the start
 * of block zero, so we must search for the PBL Markers and PBL offset. */
static int find_pbl_offset(struct mtd_info *mtd, int block_zero,
			   uint32_t *offset)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	struct stm_nand_afm_device *data = chip->priv;

	int buf_len;
	uint8_t *buf;

	int pages_per_block;
	uint32_t delta;
	int found = 0;
	int page;
	int page_end;
	int col;
	int marker_size = 2 * sizeof(uint32_t);

	pages_per_block = 1 << (chip->phys_erase_shift - chip->page_shift);
	page_end = data->boot_end >> chip->page_shift;

	/* Use extended buffer to handle the case where the PBL Markers and the
	 * Offset straddle a page boundary. */
	buf_len = mtd->writesize + marker_size;
	buf = kmalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		dev_err(afm->dev, "failed to allocate PBL search buffer\n");
		return 1;
	}

	/* Initialise start of buffer - see memcpy() below... */
	memset(buf, 0x00, marker_size);

	/* Start at block zero */
	for (page = block_zero * pages_per_block;
	     page < page_end; page++) {

		afm_read_page_boot(mtd, chip, buf + marker_size, 1, page);
		if (find_pbl_marker(buf, buf_len, &col) == 0) {
			found = 1;
			delta = *((uint32_t *)(buf + col + marker_size));
			*offset =  (page << chip->page_shift) + col - delta;
			break;
		}

		/* Copy end of buffer to start, just in case the markers and
		 * offset straddle a page boundary. */
		memcpy(buf, buf + (buf_len - marker_size), marker_size);
	}

	kfree(buf);

	/* Did we find the PBL? */
	return found ? 0 : 1;
}

static int pbl_boot_boundary(struct mtd_info *mtd, uint32_t *boundary)
{
	struct stm_nand_afm_controller *afm = mtd_to_afm(mtd);
	struct nand_chip *chip = mtd->priv;
	uint8_t *buf = chip->buffers->databuf;

	int block_zero;
	uint32_t pbl_offs = 0x00000000;
	int pbl_page;

	/* Find logical block zero */
	block_zero = find_block_zero(mtd);
	if (block_zero < 0) {
		dev_err(afm->dev, "failed to find logical block zero\n");
		return 1;
	}

	dev_dbg(afm->dev, "block zero: 0x%08x\n",
		 block_zero << chip->phys_erase_shift);

	/* Find PBL offset */
	if (find_pbl_offset(mtd, block_zero, &pbl_offs) != 0) {
		dev_err(afm->dev, "failed to find PBL marker\n");
		return 1;
	}

	dev_dbg(afm->dev, "PBL offset: 0x%08x\n", pbl_offs);

	if (pbl_offs & (mtd->writesize - 1)) {
		dev_err(afm->dev, "PBL offset is not page-aligned\n");
		return 1;
	}

	/* Extract boot-boundary from PBL image */
	pbl_page = pbl_offs >> chip->page_shift;
	afm_read_page_boot(mtd, chip, buf, 1, pbl_page);
	*boundary = *((uint32_t *)(buf + PBL_BOOT_BOUNDARY_POINTER));
	*boundary += block_zero << chip->phys_erase_shift;	/* physical */

	dev_info(afm->dev, "PBL boot-boundary: 0x%08x\n", *boundary);

	return 0;
}
#endif


static struct stm_nand_afm_device *
afm_init_bank(struct stm_nand_afm_controller *afm, int bank_nr,
		struct stm_nand_bank_data *bank, struct device *dev)
{
	struct stm_nand_afm_device *data;
	int err;
	struct mtd_part_parser_data ppdata;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	struct mtd_info *slave;
	uint64_t slave_offset;
	char *boot_part_name;
#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
	uint32_t boundary;
#endif
#endif
	if (dev->of_node)
		ppdata.of_node = stm_of_get_partitions_node(
						dev->of_node, bank_nr);

	/* Allocate memory for the driver structure (and zero it) */
	data = kzalloc(sizeof(struct stm_nand_afm_device), GFP_KERNEL);
	if (!data) {
		dev_err(afm->dev, "failed to allocate device structure\n");
		err = -ENOMEM;
		goto err1;
	}

	data->chip.priv = data;
	data->mtd.priv = &data->chip;
	data->mtd.owner = THIS_MODULE;
	data->mtd.dev.parent = dev;
	data->dev = afm->dev;

	/* Use hwcontrol structure to manage access to AFM Controller */
	data->chip.controller = &afm->hwcontrol;
	data->chip.state = FL_READY;

	/* Assign more sensible name (default is string from nand_ids.c!) */
	data->mtd.name = dev_name(dev);
	data->csn = bank->csn;

	data->chip.options = bank->options;
	data->chip.bbt_options = bank->bbt_options;
	data->chip.options |= NAND_NO_AUTOINCR;

	afm_set_defaults(&data->chip);

	/* Scan to find existence of device */
	if (nand_scan_ident(&data->mtd, 1, NULL) != 0) {
		err = -ENODEV;
		goto err2;
	}

	/* Calculate AFM_GEN_CFG for device found */
	data->afm_gen_cfg = afm_gen_config(afm,
				data->chip.options & NAND_BUSWIDTH_16,
				data->mtd.writesize,
				data->chip.chipsize);

	/*
	 * Configure timing registers
	 */
	if (bank->timing_spec) {
		dev_info(afm->dev, "Using platform timing data\n");
		afm_calc_timing_registers(bank->timing_spec,
					  afm->clk,
					  bank->timing_relax,
					  &data->ctl_timing,
					  &data->wen_timing,
					  &data->ren_timing);
		data->chip.chip_delay = bank->timing_spec->tR;
	} else if (bank->timing_data) {
		dev_info(afm->dev, "Using legacy platform timing data\n");
		afm_calc_timing_registers_legacy(bank->timing_data,
						 afm->clk,
						 &data->ctl_timing,
						 &data->wen_timing,
						 &data->ren_timing);
		data->chip.chip_delay = bank->timing_data->chip_delay;
	} else if (data->chip.onfi_version) {
		struct nand_onfi_params *onfi = &data->chip.onfi_params;
		int mode;

		mode = fls(le16_to_cpu(onfi->async_timing_mode)) - 1;
		/* Modes 4 and 5 (EDO) are not supported on our H/W */
		if (mode > 3)
			mode = 3;

		dev_info(afm->dev, "Using ONFI Timing Mode %d\n", mode);
		afm_calc_timing_registers(&nand_onfi_timing_specs[mode],
					  afm->clk,
					  bank->timing_relax,
					  &data->ctl_timing,
					  &data->wen_timing,
					  &data->ren_timing);
		data->chip.chip_delay = le16_to_cpu(data->chip.onfi_params.t_r);
	} else {
		dev_warn(afm->dev, "No timing data available\n");
	}

	/* Ensure 'complete' chip-specific configuration on next select_chip()
	 * activation */
	afm->current_csn = -1;

	/* Complete the scan */
	if (afm_scan_tail(&data->mtd) != 0) {
		err = -ENXIO;
		goto err2;
	}

	/* If all blocks are marked bad, mount as "recovery" partition */
	if (stmnand_blocks_all_bad(&data->mtd)) {
		dev_err(afm->dev, "initiating NAND Recovery Mode\n");
		data->mtd.name = "NAND RECOVERY MODE";
		err =  mtd_device_register(&data->mtd, NULL, 0);
		if (err)
			goto err2;

		return data;
	}

	err = mtd_device_parse_register(&data->mtd,
			NULL, &ppdata,
			bank->partitions, bank->nr_partitions);
	if (err)
		goto err3;

#ifdef CONFIG_STM_NAND_AFM_BOOTMODESUPPORT
	/* Set name of boot partition */
	boot_part_name = nbootpart ? nbootpart :
		CONFIG_STM_NAND_AFM_BOOTPARTITION;

	/* Update boot-mode slave partition */
	slave = get_mtd_partition_slave(&data->mtd, boot_part_name,
					&slave_offset);
	if (slave) {
		data->boot_start = slave_offset;
		data->boot_end = slave_offset + slave->size;

#ifdef CONFIG_STM_NAND_AFM_PBLBOOTBOUNDARY
		/* Update 'boot_end' with value in PBL image */
		if (pbl_boot_boundary(&data->mtd,
				      &boundary) != 0) {
			dev_err(afm->dev, "failed to get boot-mode "
				"boundary from PBL\n");
		} else {
			if (boundary < data->boot_start ||
			    boundary > data->boot_end) {
				dev_err(afm->dev, "PBL boot-mode "
					"boundary lies outside "
					"boot partition\n");
			} else {
				data->boot_end = boundary;
			}
		}
#endif
		slave->oobavail = data->ecc_boot.ecc_ctrl.layout->oobavail;
		slave->subpage_sft = data->ecc_boot.subpage_sft;
		slave->ecclayout = data->ecc_boot.ecc_ctrl.layout;

		dev_info(afm->dev, "boot-mode ECC: 0x%08x->0x%08x\n",
			 (unsigned int)data->boot_start,
			 (unsigned int)data->boot_end);
	} else {
		dev_info(afm->dev, "failed to find boot partition [%s]\n",
			 boot_part_name);
	}
#endif

	/* Success! */
	return data;


 err3:
	nand_release(&data->mtd);
 err2:
	kfree(data);
 err1:
	return ERR_PTR(err);
}

static void *stm_afm_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_nand_flex_data *data;

	if (!np)
		return ERR_PTR(-ENODEV);

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	data->flex_rbn_connected = of_property_read_bool(np,
					"st,rbn-flex-connected");
	data->nr_banks = stm_of_get_nand_banks(&pdev->dev, np, &data->banks);

	if (of_property_read_u32(np, "st,flex-select-reg",
				&data->flex_select_reg))
		return ERR_PTR(-ENODEV);

	if (of_property_read_u32(np, "st,flex-select-msk",
				&data->flex_select_msk))
		return ERR_PTR(-ENODEV);

	pdev->dev.platform_data = data;
	return data;
}

/*
 * stm-nand-afm device probe
 */
static int stm_afm_probe(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata;
	struct stm_nand_bank_data *bank;
	struct stm_nand_afm_controller *afm;
	struct stm_nand_afm_device *data;
	int n;
	int err = 0;

	pdata = stm_afm_dt_get_pdata(pdev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	stm_nandi_select(STM_NANDI_HAMMING, pdata->flex_select_reg,
				 pdata->flex_select_msk);

	afm = afm_init_resources(pdev);
	if (IS_ERR(afm)) {
		dev_err(&pdev->dev, "failed to initialise NAND Controller.\n");
		err = PTR_ERR(afm);
		return err;
	}

	bank = pdata->banks;
	for (n = 0; n < pdata->nr_banks; n++) {
		data = afm_init_bank(afm, n, bank, &pdev->dev);

		if (IS_ERR(data)) {
			err = PTR_ERR(data);
			goto err1;
		}

		afm->devices[n] = data;
		bank++;
	}

	return 0;

 err1:
	while (--n > 0) {
		data = afm->devices[n];
		nand_release(&data->mtd);
		kfree(data);
	}

	afm_exit_controller(pdev);

	return err;
}


static int stm_afm_remove(struct platform_device *pdev)
{
	struct stm_plat_nand_flex_data *pdata = pdev->dev.platform_data;
	struct stm_nand_afm_controller *afm = platform_get_drvdata(pdev);
	int n;

	for (n = 0; n < pdata->nr_banks; n++) {
		struct stm_nand_afm_device *data = afm->devices[n];
		nand_release(&data->mtd);
		kfree(data);
	}

	afm_exit_controller(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int stm_afm_nand_resume(struct device *dev)
{
	struct stm_nand_afm_controller *afm = dev_get_drvdata(dev);

	afm->current_csn = -1;
	afm_init_controller(afm);

	return 0;
}

SIMPLE_DEV_PM_OPS(stm_afm_nand_pm_ops, NULL, stm_afm_nand_resume);
#define STM_AFM_NAND_PM_OPS	(&stm_afm_nand_pm_ops)
#else
#define STM_AFM_NAND_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id nand_afm_match[] = {
	{
		.compatible = "st,nand-afm",
	},
	{},
};

MODULE_DEVICE_TABLE(of, nand_afm_match);
#endif

static struct platform_driver stm_afm_nand_driver = {
	.probe		= stm_afm_probe,
	.remove		= stm_afm_remove,
	.driver		= {
		.name	= NAME,
		.owner	= THIS_MODULE,
		.pm	= STM_AFM_NAND_PM_OPS,
		.of_match_table = of_match_ptr(nand_afm_match),
	},
};

static int __init stm_afm_nand_init(void)
{
	return platform_driver_register(&stm_afm_nand_driver);
}

static void __exit stm_afm_nand_exit(void)
{
	platform_driver_unregister(&stm_afm_nand_driver);
}

module_init(stm_afm_nand_init);
module_exit(stm_afm_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angus Clark");
MODULE_DESCRIPTION("STM AFM-based NAND Flash driver");
