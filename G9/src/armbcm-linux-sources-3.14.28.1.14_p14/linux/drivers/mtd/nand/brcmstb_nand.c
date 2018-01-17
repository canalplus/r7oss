/*
 * Copyright (C) 2010 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>
#include <linux/of_mtd.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/log2.h>

#include <linux/brcmstb/brcmstb.h>

/*
 * SWLINUX-1818: this flag controls if WP stays on between erase/write
 * commands to mitigate flash corruption due to power glitches. Values:
 * 0: NAND_WP is not used or not available
 * 1: NAND_WP is set by default, cleared for erase/write operations
 * 2: NAND_WP is always cleared
 */
static int wp_on = 1;
module_param(wp_on, int, 0444);

/***********************************************************************
 * Definitions
 ***********************************************************************/

#define DBG(args...)		pr_debug(args)

#define DRV_NAME		"brcmstb_nand"
#define CONTROLLER_VER		(10 * CONFIG_BRCMNAND_MAJOR_VERS + \
		CONFIG_BRCMNAND_MINOR_VERS)

#define CMD_NULL		0x00
#define CMD_PAGE_READ		0x01
#define CMD_SPARE_AREA_READ	0x02
#define CMD_STATUS_READ		0x03
#define CMD_PROGRAM_PAGE	0x04
#define CMD_PROGRAM_SPARE_AREA	0x05
#define CMD_COPY_BACK		0x06
#define CMD_DEVICE_ID_READ	0x07
#define CMD_BLOCK_ERASE		0x08
#define CMD_FLASH_RESET		0x09
#define CMD_BLOCKS_LOCK		0x0a
#define CMD_BLOCKS_LOCK_DOWN	0x0b
#define CMD_BLOCKS_UNLOCK	0x0c
#define CMD_READ_BLOCKS_LOCK_STATUS	0x0d

#if CONTROLLER_VER >= 40
#define CMD_PARAMETER_READ	0x0e
#define CMD_PARAMETER_CHANGE_COL	0x0f
#define CMD_LOW_LEVEL_OP	0x10
#endif

struct brcm_nand_dma_desc {
	u32 next_desc;
	u32 next_desc_ext;
	u32 cmd_irq;
	u32 dram_addr;
	u32 dram_addr_ext;
	u32 tfr_len;
	u32 total_len;
	u32 flash_addr;
	u32 flash_addr_ext;
	u32 cs;
	u32 pad2[5];
	u32 status_valid;
} __packed;

/* Bitfields for brcm_nand_dma_desc::status_valid */
#define FLASH_DMA_ECC_ERROR	(1 << 8)
#define FLASH_DMA_CORR_ERROR	(1 << 9)

/* 512B flash cache in the NAND controller HW */
#define FC_SHIFT		9U
#define FC_BYTES		512U
#define FC_WORDS		(FC_BYTES >> 2)
#define FC(x)			(BCHP_NAND_FLASH_CACHEi_ARRAY_BASE + ((x) << 2))

#if CONTROLLER_VER >= 60
#define MAX_CONTROLLER_OOB	64
#define OFS_10_RD		BCHP_NAND_SPARE_AREA_READ_OFS_10
#define OFS_10_WR		BCHP_NAND_SPARE_AREA_WRITE_OFS_10
#elif CONTROLLER_VER >= 50
#define MAX_CONTROLLER_OOB	32
#define OFS_10_RD		BCHP_NAND_SPARE_AREA_READ_OFS_10
#define OFS_10_WR		BCHP_NAND_SPARE_AREA_WRITE_OFS_10
#else
#define MAX_CONTROLLER_OOB	16
#define OFS_10_RD		-1
#define OFS_10_WR		-1
#endif

#if CONTROLLER_VER >= 71
#define REG_SPACING		0x14UL
#else
#define REG_SPACING		0x10UL
#endif

#if CONTROLLER_VER >= 60

#define REG_ACC_CONTROL(cs) (BCHP_NAND_ACC_CONTROL_CS0 + ((cs) * REG_SPACING))

#define REG_CONFIG(cs) (BCHP_NAND_CONFIG_CS0 + ((cs) * REG_SPACING))
#define REG_CONFIG_EXT(cs) (BCHP_NAND_CONFIG_EXT_CS0 + ((cs) * REG_SPACING))

#define REG_TIMING_1(cs) (BCHP_NAND_TIMING_1_CS0 + ((cs) * REG_SPACING))
#define REG_TIMING_2(cs) (BCHP_NAND_TIMING_2_CS0 + ((cs) * REG_SPACING))

#define CORR_ERROR_COUNT (BDEV_RD(BCHP_NAND_CORR_ERROR_COUNT))

#define WR_CORR_THRESH(cs, val) do { \
	u32 contents = BDEV_RD(BCHP_NAND_CORR_STAT_THRESHOLD); \
	u32 shift = BCHP_NAND_CORR_STAT_THRESHOLD_CORR_STAT_THRESHOLD_CS1_SHIFT * (cs); \
	contents &= ~(BCHP_NAND_CORR_STAT_THRESHOLD_CORR_STAT_THRESHOLD_CS0_MASK \
			<< shift); \
	contents |= ((val) & BCHP_NAND_CORR_STAT_THRESHOLD_CORR_STAT_THRESHOLD_CS0_MASK) \
			<< shift; \
	BDEV_WR(BCHP_NAND_CORR_STAT_THRESHOLD, contents); \
	} while (0);

#else /* CONTROLLER_VER < 60 */

#define REG_ACC_CONTROL(cs) \
	((cs) == 0 ? BCHP_NAND_ACC_CONTROL : \
	 (BCHP_NAND_ACC_CONTROL_CS1 + (((cs) - 1) << 4)))

#define REG_CONFIG(cs) \
	((cs) == 0 ? BCHP_NAND_CONFIG : \
	 (BCHP_NAND_CONFIG_CS1 + (((cs) - 1) << 4)))

#define REG_TIMING_1(cs) \
	((cs) == 0 ? BCHP_NAND_TIMING_1 : \
	 (BCHP_NAND_TIMING_1_CS1 + (((cs) - 1) << 4)))
#define REG_TIMING_2(cs) \
	((cs) == 0 ? BCHP_NAND_TIMING_2 : \
	 (BCHP_NAND_TIMING_2_CS1 + (((cs) - 1) << 4)))

#define CORR_ERROR_COUNT (1)

#define WR_CORR_THRESH(cs, val) do { \
	BDEV_WR(BCHP_NAND_CORR_STAT_THRESHOLD, \
		((val) & BCHP_NAND_CORR_STAT_THRESHOLD_CORR_STAT_THRESHOLD_MASK) \
		    << BCHP_NAND_CORR_STAT_THRESHOLD_CORR_STAT_THRESHOLD_SHIFT); \
	} while (0);

#endif /* CONTROLLER_VER < 60 */

#define WR_CONFIG(cs, field, val) do { \
	u32 reg = REG_CONFIG(cs), contents = BDEV_RD(reg); \
	contents &= ~(BCHP_NAND_CONFIG_CS1_##field##_MASK); \
	contents |= (val) << BCHP_NAND_CONFIG_CS1_##field##_SHIFT; \
	BDEV_WR(reg, contents); \
	} while (0)

#define WR_CONFIG_EXT(cs, field, val) do { \
	u32 reg = REG_CONFIG_EXT(cs), contents = BDEV_RD(reg); \
	contents &= ~(BCHP_NAND_CONFIG_EXT_CS1_##field##_MASK); \
	contents |= (val) << BCHP_NAND_CONFIG_EXT_CS1_##field##_SHIFT; \
	BDEV_WR(reg, contents); \
	} while (0)

#define RD_CONFIG(cs, field) \
	((BDEV_RD(REG_CONFIG(cs)) & BCHP_NAND_CONFIG_CS1_##field##_MASK) \
	 >> BCHP_NAND_CONFIG_CS1_##field##_SHIFT)

#define RD_CONFIG_EXT(cs, field) \
	((BDEV_RD(REG_CONFIG_EXT(cs)) & \
	  BCHP_NAND_CONFIG_EXT_CS1_##field##_MASK) \
	 >> BCHP_NAND_CONFIG_EXT_CS1_##field##_SHIFT)

#define WR_ACC_CONTROL(cs, field, val) do { \
	u32 reg = REG_ACC_CONTROL(cs), contents = BDEV_RD(reg); \
	contents &= ~(BCHP_NAND_ACC_CONTROL_CS1_##field##_MASK); \
	contents |= (val) << BCHP_NAND_ACC_CONTROL_CS1_##field##_SHIFT; \
	BDEV_WR(reg, contents); \
	} while (0)

#define RD_ACC_CONTROL(cs, field) \
	((BDEV_RD(REG_ACC_CONTROL(cs)) & \
	BCHP_NAND_ACC_CONTROL_CS1_##field##_MASK) \
		>> BCHP_NAND_ACC_CONTROL_CS1_##field##_SHIFT)

#define HIF_ENABLED_IRQ(bit) \
	(!BDEV_RD_F(HIF_INTR2_CPU_MASK_STATUS, bit##_INTR))

struct brcmstb_nand_controller {
	struct nand_hw_control	controller;
	volatile void __iomem	*flash_dma_base;
	unsigned int		irq;
	unsigned int		dma_irq;
	bool			irq_cascaded;
	int			cmd_pending;
	bool			dma_pending;
	struct completion	done;
	struct completion	dma_done;

	/* List of NAND hosts (one for each chip-select) */
	struct list_head host_list;

	struct brcm_nand_dma_desc *dma_desc;
	dma_addr_t		dma_pa;

	/* in-memory cache of the FLASH_CACHE, used only for some commands */
	u32			flash_cache[FC_WORDS];

	u32			nand_cs_nand_select;
	u32			nand_cs_nand_xor;
	u32			corr_stat_threshold;
	u32			flash_dma_mode;
};

struct brcmstb_nand_cfg {
	u64			device_size;
	unsigned int		block_size;
	unsigned int		page_size;
	unsigned int		spare_area_size;
	unsigned int		device_width;
	unsigned int		col_adr_bytes;
	unsigned int		blk_adr_bytes;
	unsigned int		ful_adr_bytes;
	unsigned int		sector_size_1k;
	unsigned int		ecc_level;
	/* use for low-power standby/resume only */
	u32			acc_control;
	u32			config;
	u32			config_ext;
	u32			timing_1;
	u32			timing_2;
};

struct brcmstb_nand_host {
	struct list_head	node;
	struct device_node	*of_node;

	struct nand_chip	chip;
	struct mtd_info		mtd;
	struct platform_device	*pdev;
	int			cs;

	unsigned int		last_cmd;
	unsigned int		last_byte;
	u64			last_addr;
	struct brcmstb_nand_cfg	hwcfg;
	struct brcmstb_nand_controller *ctrl;
};

static struct nand_ecclayout brcmstb_nand_dummy_layout = {
	.eccbytes		= 16,
	.eccpos			= { 0, 1, 2, 3, 4, 5, 6, 7,
				    8, 9, 10, 11, 12, 13, 14, 15 },
};

/***********************************************************************
 * Flash DMA
 ***********************************************************************/

enum flash_dma_reg {
	FLASH_DMA_REVISION		= 0x00,
	FLASH_DMA_FIRST_DESC		= 0x04,
	FLASH_DMA_FIRST_DESC_EXT	= 0x08,
	FLASH_DMA_CTRL			= 0x0c,
	FLASH_DMA_MODE			= 0x10,
	FLASH_DMA_STATUS		= 0x14,
	FLASH_DMA_INTERRUPT_DESC	= 0x18,
	FLASH_DMA_INTERRUPT_DESC_EXT	= 0x1c,
	FLASH_DMA_ERROR_STATUS		= 0x20,
	FLASH_DMA_CURRENT_DESC		= 0x24,
	FLASH_DMA_CURRENT_DESC_EXT	= 0x28,
};

static inline bool has_flash_dma(struct brcmstb_nand_controller *ctrl)
{
	return ctrl->flash_dma_base;
}

static inline bool flash_dma_buf_ok(const void *buf)
{
	return buf && !is_vmalloc_addr(buf) &&
		likely(IS_ALIGNED((uintptr_t)buf, 4));
}

static inline void flash_dma_writel(struct brcmstb_nand_controller *ctrl, u8 offs,
		u32 val)
{
	__raw_writel(val, ctrl->flash_dma_base + offs);
}

static inline u32 flash_dma_readl(struct brcmstb_nand_controller *ctrl, u8 offs)
{
	return __raw_readl(ctrl->flash_dma_base + offs);
}

static inline bool flash_dma_irq_done(struct brcmstb_nand_controller *ctrl)
{
#ifdef BCHP_HIF_INTR2_CPU_STATUS_FLASH_DMA_DONE_INTR_MASK
	if (HIF_TEST_IRQ(FLASH_DMA_DONE)) {
		HIF_ACK_IRQ(FLASH_DMA_DONE);
		return true;
	}
#endif
	return false;
}

static inline void flash_dma_irq_enable(struct brcmstb_nand_controller *ctrl)
{
#ifdef BCHP_HIF_INTR2_CPU_STATUS_FLASH_DMA_DONE_INTR_MASK
	HIF_ACK_IRQ(FLASH_DMA_DONE);
	HIF_ENABLE_IRQ(FLASH_DMA_DONE);
#endif
}

static inline void flash_dma_irq_disable(struct brcmstb_nand_controller *ctrl)
{
#ifdef BCHP_HIF_INTR2_CPU_STATUS_FLASH_DMA_DONE_INTR_MASK
	HIF_DISABLE_IRQ(FLASH_DMA_DONE);
#endif
}

static inline bool flash_dma_irq_enabled(struct brcmstb_nand_controller *ctrl)
{
#ifdef BCHP_HIF_INTR2_CPU_STATUS_FLASH_DMA_DONE_INTR_MASK
	return HIF_ENABLED_IRQ(FLASH_DMA_DONE);
#else
	return false;
#endif
}

/*
 * Get the DMA physical address for the n'th DMA descriptor
 */
static inline dma_addr_t flash_dma_get_desc_pa(
		struct brcmstb_nand_controller *ctrl, int idx)
{
	return ctrl->dma_pa + (idx * sizeof(struct brcm_nand_dma_desc));
}

/* Low-level operation types: command, address, write, or read */
enum brcmstb_nand_llop_type {
	LL_OP_CMD,
	LL_OP_ADDR,
	LL_OP_WR,
	LL_OP_RD,
};

/***********************************************************************
 * Internal support functions
 ***********************************************************************/

static inline bool is_hamming_ecc(struct brcmstb_nand_cfg *cfg)
{
	return cfg->sector_size_1k == 0 && cfg->spare_area_size == 16 &&
		cfg->ecc_level == 15;
}

/*
 * Returns a nand_ecclayout strucutre for the given layout/configuration.
 * Returns NULL on failure.
 */
static struct nand_ecclayout *brcmstb_nand_create_layout(int ecc_level,
		struct brcmstb_nand_host *host)
{
	struct brcmstb_nand_cfg *cfg = &host->hwcfg;
	int i, j;
	struct nand_ecclayout *layout;
	int req;
	int sectors;
	int sas;
	int idx1, idx2;

	layout = devm_kzalloc(&host->pdev->dev, sizeof(*layout), GFP_KERNEL);
	if (!layout)
		return NULL;

	sectors = cfg->page_size / (512 << cfg->sector_size_1k);
	sas = cfg->spare_area_size << cfg->sector_size_1k;

	/* Hamming */
	if (is_hamming_ecc(cfg)) {
		for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
			/* First sector of each page may have BBI */
			if (i == 0) {
				layout->oobfree[idx2].offset = i * sas + 1;
				/* Small-page NAND use byte 6 for BBI */
				if (cfg->page_size == 512)
					layout->oobfree[idx2].offset--;
				layout->oobfree[idx2].length = 5;
			} else {
				layout->oobfree[idx2].offset = i * sas;
				layout->oobfree[idx2].length = 6;
			}
			idx2++;
			layout->eccpos[idx1++] = i * sas + 6;
			layout->eccpos[idx1++] = i * sas + 7;
			layout->eccpos[idx1++] = i * sas + 8;
			layout->oobfree[idx2].offset = i * sas + 9;
			layout->oobfree[idx2].length = 7;
			idx2++;
			/* Leave zero-terminated entry for OOBFREE */
			if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
				    idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
				break;
		}
		goto out;
	}

	/*
	 * CONTROLLER_VERSION:
	 *   < v5.0: ECC_REQ = ceil(BCH_T * 13/8)
	 *  >= v5.0: ECC_REQ = ceil(BCH_T * 14/8)  [see SWLINUX-2038]
	 * But we will just be conservative.
	 */
	req = (ecc_level * 14 + 7) / 8;
	if (req >= sas) {
		pr_info("%s: ECC too large for OOB, using dummy layout\n",
			__func__);
		memcpy(layout, &brcmstb_nand_dummy_layout, sizeof(*layout));
		return layout;
	}

	DBG("OOBLAYOUT: sas=%d  req=%d  sectors=%d\n", sas, req, sectors);

	layout->eccbytes = req * sectors;
	for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
		for (j = sas - req; j < sas && idx1 <
				MTD_MAX_ECCPOS_ENTRIES_LARGE; j++, idx1++)
			layout->eccpos[idx1] = i * sas + j;

		/* First sector of each page may have BBI */
		if (i == 0) {
			if (cfg->page_size == 512 && (sas - req >= 6)) {
				/* Small-page NAND use byte 6 for BBI */
				layout->oobfree[idx2].offset = 0;
				layout->oobfree[idx2].length = 5;
				idx2++;
				if (sas - req > 6) {
					layout->oobfree[idx2].offset = 6;
					layout->oobfree[idx2].length =
						sas - req - 6;
					idx2++;
				}
			} else if (sas > req + 1) {
				layout->oobfree[idx2].offset = i * sas + 1;
				layout->oobfree[idx2].length = sas - req - 1;
				idx2++;
			}
		} else if (sas > req) {
			layout->oobfree[idx2].offset = i * sas;
			layout->oobfree[idx2].length = sas - req;
			idx2++;
		}
		/* Leave zero-terminated entry for OOBFREE */
		if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
				idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
			break;
	}
out:
	/* Sum available OOB */
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES_LARGE; i++)
		layout->oobavail += layout->oobfree[i].length;
	return layout;
}

static struct nand_ecclayout *brcmstb_choose_ecc_layout(
		struct brcmstb_nand_host *host)
{
	struct nand_ecclayout *layout;
	struct brcmstb_nand_cfg *p = &host->hwcfg;
	unsigned int ecc_level = p->ecc_level;

	if (p->sector_size_1k)
		ecc_level <<= 1;

	layout = brcmstb_nand_create_layout(ecc_level, host);
	if (!layout) {
		dev_err(&host->pdev->dev,
				"no proper ecc_layout for this NAND cfg\n");
		return NULL;
	}

	return layout;
}

static void brcmstb_nand_wp(struct mtd_info *mtd, int wp)
{
#ifdef BCHP_NAND_CS_NAND_SELECT_NAND_WP_MASK
	if (wp_on == 1) {
		static int old_wp = -1;
		if (old_wp != wp) {
			DBG("%s: WP %s\n", __func__, wp ? "on" : "off");
			old_wp = wp;
		}
		BDEV_WR_F_RB(NAND_CS_NAND_SELECT, NAND_WP, wp);
	}
#endif
}

/* Helper functions for reading and writing OOB registers */
static inline unsigned char oob_reg_read(int offs)
{
	if (offs >= MAX_CONTROLLER_OOB)
		return 0x77;

	if (offs < 16)
		return BDEV_RD(BCHP_NAND_SPARE_AREA_READ_OFS_0 + (offs & ~0x03))
			>> (24 - ((offs & 0x03) << 3));

	offs -= 16;

	return BDEV_RD(OFS_10_RD + (offs & ~0x03))
		>> (24 - ((offs & 0x03) << 3));
}

static inline void oob_reg_write(int offs, unsigned long data)
{
	if (offs >= MAX_CONTROLLER_OOB)
		return;

	if (offs < 16) {
		BDEV_WR(BCHP_NAND_SPARE_AREA_WRITE_OFS_0 + (offs & ~0x03),
				data);
		return;
	}

	offs -= 16;

	BDEV_WR(OFS_10_WR + (offs & ~0x03), data);
}

/*
 * read_oob_from_regs - read data from OOB registers
 * @i: sub-page sector index
 * @oob: buffer to read to
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int read_oob_from_regs(int i, u8 *oob, int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - MAX_CONTROLLER_OOB);
	tbytes = min(tbytes, MAX_CONTROLLER_OOB);

	for (j = 0; j < tbytes; j++)
		oob[j] = oob_reg_read(j);
	return tbytes;
}

/*
 * write_oob_to_regs - write data to OOB registers
 * @i: sub-page sector index
 * @oob: buffer to write from
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int write_oob_to_regs(int i, const u8 *oob, int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - MAX_CONTROLLER_OOB);
	tbytes = min(tbytes, MAX_CONTROLLER_OOB);

	for (j = 0; j < tbytes; j += 4)
		oob_reg_write(j,
				(oob[j + 0] << 24) |
				(oob[j + 1] << 16) |
				(oob[j + 2] <<  8) |
				(oob[j + 3] <<  0));
	return tbytes;
}

static irqreturn_t brcmstb_nand_ctlrdy_irq(int irq, void *data)
{
	struct brcmstb_nand_controller *ctrl = data;

	/* Discard all NAND_CTLRDY interrupts during DMA */
	if (ctrl->dma_pending)
		return IRQ_HANDLED;

	complete(&ctrl->done);
	return IRQ_HANDLED;
}

static irqreturn_t brcmstb_nand_dma_irq(int irq, void *data)
{
	struct brcmstb_nand_controller *ctrl = data;

	complete(&ctrl->dma_done);

	return IRQ_HANDLED;
}

/*
 * High-level HIF interrupt handler; used only if we aren't using a
 * properly-cascaded L2 interrupt controller driver
 */
static irqreturn_t brcmstb_nand_irq(int irq, void *data)
{
	struct brcmstb_nand_controller *ctrl = data;

	if (has_flash_dma(ctrl) && flash_dma_irq_done(ctrl))
		return brcmstb_nand_dma_irq(irq, data);

	if (HIF_TEST_IRQ(NAND_CTLRDY)) {
		HIF_ACK_IRQ(NAND_CTLRDY);
		return brcmstb_nand_ctlrdy_irq(irq, data);
	}
	return IRQ_NONE;
}

static void brcmstb_nand_send_cmd(struct brcmstb_nand_host *host, int cmd)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;

	DBG("%s: native cmd %d addr_lo 0x%lx\n", __func__, cmd,
		BDEV_RD(BCHP_NAND_CMD_ADDRESS));
	BUG_ON(ctrl->cmd_pending != 0);
	ctrl->cmd_pending = cmd;
	mb();
	BDEV_WR(BCHP_NAND_CMD_START, cmd << BCHP_NAND_CMD_START_OPCODE_SHIFT);
}

/***********************************************************************
 * NAND MTD API: read/program/erase
 ***********************************************************************/

static void brcmstb_nand_cmd_ctrl(struct mtd_info *mtd, int dat,
	unsigned int ctrl)
{
	/* intentionally left blank */
}

static int brcmstb_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned long timeo = msecs_to_jiffies(100);

	DBG("%s: native cmd %d\n", __func__, ctrl->cmd_pending);
	if (ctrl->cmd_pending &&
			wait_for_completion_timeout(&ctrl->done, timeo) <= 0) {
		dev_err_ratelimited(&host->pdev->dev,
			"timeout waiting for command %u (%ld)\n",
			host->last_cmd, BDEV_RD(BCHP_NAND_CMD_START) >>
			BCHP_NAND_CMD_START_OPCODE_SHIFT);
		dev_err_ratelimited(&host->pdev->dev,
			"irq status %08lx, intfc status %08lx\n",
			BDEV_RD(BCHP_HIF_INTR2_CPU_STATUS),
			BDEV_RD(BCHP_NAND_INTFC_STATUS));
	}
	ctrl->cmd_pending = 0;
	return BDEV_RD_F(NAND_INTFC_STATUS, FLASH_STATUS);
}

static int brcmstb_nand_low_level_op(struct brcmstb_nand_host *host,
		enum brcmstb_nand_llop_type type, u32 data, bool last_op)
{
	struct mtd_info *mtd = &host->mtd;
	struct nand_chip *chip = &host->chip;
	u32 tmp;

	tmp = data & BCHP_NAND_LL_OP_DATA_MASK;
	switch (type) {
	case LL_OP_CMD:
		tmp |= BCHP_NAND_LL_OP_CLE_MASK;
		tmp |= BCHP_NAND_LL_OP_WE_MASK;
		break;
	case LL_OP_ADDR:
		tmp |= BCHP_NAND_LL_OP_ALE_MASK;
		tmp |= BCHP_NAND_LL_OP_WE_MASK;
		break;
	case LL_OP_WR:
		tmp |= BCHP_NAND_LL_OP_WE_MASK;
		break;
	case LL_OP_RD:
		tmp |= BCHP_NAND_LL_OP_RE_MASK;
		break;
	}
	if (last_op)
		tmp |= BCHP_NAND_LL_OP_RETURN_IDLE_MASK;

	DBG("%s: cmd 0x%lx\n", __func__, (unsigned long)tmp);

	BDEV_WR_RB(BCHP_NAND_LL_OP, tmp);

	brcmstb_nand_send_cmd(host, CMD_LOW_LEVEL_OP);
	return brcmstb_nand_waitfunc(mtd, chip);
}

static void brcmstb_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
	int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u64 addr = (u64)page_addr << chip->page_shift;
	int native_cmd = 0;

	if (command == NAND_CMD_READID || command == NAND_CMD_PARAM ||
			command == NAND_CMD_RNDOUT)
		addr = (u64)column;
	/* Avoid propagating a negative, don't-care address */
	else if (page_addr < 0)
		addr = 0;

	DBG("%s: cmd 0x%x addr 0x%llx\n", __func__, command,
		(unsigned long long)addr);
	host->last_cmd = command;
	host->last_byte = 0;
	host->last_addr = addr;

	switch (command) {
	case NAND_CMD_RESET:
		native_cmd = CMD_FLASH_RESET;
		break;
	case NAND_CMD_STATUS:
		native_cmd = CMD_STATUS_READ;
		break;
	case NAND_CMD_READID:
		native_cmd = CMD_DEVICE_ID_READ;
		break;
	case NAND_CMD_READOOB:
		native_cmd = CMD_SPARE_AREA_READ;
		break;
	case NAND_CMD_ERASE1:
		native_cmd = CMD_BLOCK_ERASE;
		brcmstb_nand_wp(mtd, 0);
		break;
#if CONTROLLER_VER >= 40
	case NAND_CMD_PARAM:
		native_cmd = CMD_PARAMETER_READ;
		break;
	case NAND_CMD_SET_FEATURES:
	case NAND_CMD_GET_FEATURES:
		brcmstb_nand_low_level_op(host, LL_OP_CMD, command, false);
		brcmstb_nand_low_level_op(host, LL_OP_ADDR, column, false);
		break;
	case NAND_CMD_RNDOUT:
		native_cmd = CMD_PARAMETER_CHANGE_COL;
		addr &= ~((u64)(FC_BYTES - 1));
		/*
		 * HW quirk: PARAMETER_CHANGE_COL requires SECTOR_SIZE_1K=0
		 * NB: hwcfg.sector_size_1k may not be initialized yet
		 */
		if (RD_ACC_CONTROL(host->cs, SECTOR_SIZE_1K)) {
			host->hwcfg.sector_size_1k =
				RD_ACC_CONTROL(host->cs, SECTOR_SIZE_1K);
			WR_ACC_CONTROL(host->cs, SECTOR_SIZE_1K, 0);
		}
		break;
#endif
	}

	if (!native_cmd)
		return;

	BDEV_WR_RB(BCHP_NAND_CMD_EXT_ADDRESS,
		(host->cs << 16) | ((addr >> 32) & 0xffff));
	BDEV_WR_RB(BCHP_NAND_CMD_ADDRESS, addr & 0xffffffff);

	brcmstb_nand_send_cmd(host, native_cmd);
	brcmstb_nand_waitfunc(mtd, chip);

#if CONTROLLER_VER >= 40
	if (native_cmd == CMD_PARAMETER_READ ||
			native_cmd == CMD_PARAMETER_CHANGE_COL) {
		int i;
		/*
		 * Must cache the FLASH_CACHE now, since changes in
		 * SECTOR_SIZE_1K may invalidate it
		 */
		for (i = 0; i < FC_WORDS; i++)
			ctrl->flash_cache[i] = le32_to_cpu(BDEV_RD(FC(i)));
		/* Cleanup from HW quirk: restore SECTOR_SIZE_1K */
		if (host->hwcfg.sector_size_1k)
			WR_ACC_CONTROL(host->cs, SECTOR_SIZE_1K,
					host->hwcfg.sector_size_1k);
	}
#endif

	/* Re-enable protection is necessary only after erase */
	if (command == NAND_CMD_ERASE1)
		brcmstb_nand_wp(mtd, 1);
}

static uint8_t brcmstb_nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	uint8_t ret = 0;
	int addr, offs;

	switch (host->last_cmd) {
	case NAND_CMD_READID:
		if (host->last_byte < 4)
			ret = BDEV_RD(BCHP_NAND_FLASH_DEVICE_ID) >>
				(24 - (host->last_byte << 3));
		else if (host->last_byte < 8)
			ret = BDEV_RD(BCHP_NAND_FLASH_DEVICE_ID_EXT) >>
				(56 - (host->last_byte << 3));
		break;

	case NAND_CMD_READOOB:
		ret = oob_reg_read(host->last_byte);
		break;

	case NAND_CMD_STATUS:
		ret = BDEV_RD(BCHP_NAND_INTFC_STATUS) & 0xff;
		if (wp_on) /* SWLINUX-1818: hide WP status from MTD */
			ret |= NAND_STATUS_WP;
		break;

#if CONTROLLER_VER >= 40
	case NAND_CMD_PARAM:
	case NAND_CMD_RNDOUT:
		addr = host->last_addr + host->last_byte;
		offs = addr & (FC_BYTES - 1);

		/* At FC_BYTES boundary, switch to next column */
		if (host->last_byte > 0 && offs == 0)
			chip->cmdfunc(mtd, NAND_CMD_RNDOUT, addr, -1);

		ret = ctrl->flash_cache[offs >> 2] >>
					(24 - ((offs & 0x03) << 3));
		break;
	case NAND_CMD_GET_FEATURES:
		if (host->last_byte >= ONFI_SUBFEATURE_PARAM_LEN) {
			ret = 0;
		} else {
			bool last = host->last_byte ==
				ONFI_SUBFEATURE_PARAM_LEN - 1;
			brcmstb_nand_low_level_op(host, LL_OP_RD, 0, last);
			ret = BDEV_RD(BCHP_NAND_LL_RDDATA) & 0xff;
		}
#endif
	}

	DBG("%s: byte = 0x%02x\n", __func__, ret);
	host->last_byte++;

	return ret;
}

static void brcmstb_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++, buf++)
		*buf = brcmstb_nand_read_byte(mtd);
}

static void brcmstb_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;

	switch (host->last_cmd) {
	case NAND_CMD_SET_FEATURES:
		for (i = 0; i < len; i++)
			brcmstb_nand_low_level_op(host, LL_OP_WR, buf[i], (i + 1) == len);
		break;
	default:
		BUG();
		break;
	}
}

/**
 * Construct a FLASH_DMA descriptor as part of a linked list. You must know the
 * following ahead of time:
 *  - Is this descriptor the beginning or end of a linked list?
 *  - What is the (DMA) address of the next descriptor in the linked list?
 */
static int brcmstb_nand_fill_dma_desc(struct brcmstb_nand_host *host,
		struct brcm_nand_dma_desc *desc, u64 addr, dma_addr_t buf,
		u32 len, u8 dma_cmd, bool begin, bool end, dma_addr_t next_desc)
{
	memset(desc, 0, sizeof(*desc));
	/* Descriptors are written in native byte order (wordwise) */
	desc->next_desc = next_desc & 0xffffffff;
	desc->next_desc_ext = ((u64)next_desc) >> 32;
	desc->cmd_irq = (dma_cmd << 24) |
		(end ? (0x03 << 8) : 0) | /* IRQ | STOP */
		(!!begin) | ((!!end) << 1); /* head, tail */
#ifdef CONFIG_CPU_BIG_ENDIAN
	desc->cmd_irq |= 0x01 << 12;
#endif
	desc->dram_addr = buf & 0xffffffff;
	desc->dram_addr_ext = (u64)buf >> 32;
	desc->tfr_len = len;
	desc->total_len = len;
	desc->flash_addr = addr & 0xffffffff;
	desc->flash_addr_ext = addr >> 32;
	desc->cs = host->cs;
	desc->status_valid = 0x01;
	return 0;
}

/**
 * Kick the FLASH_DMA engine, with a given DMA descriptor
 */
static void brcmstb_nand_dma_run(struct brcmstb_nand_host *host, dma_addr_t desc)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned long timeo = msecs_to_jiffies(100);

	flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC, desc & 0xffffffff);
	(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC);
	flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC_EXT, (u64)desc >> 32);
	(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC_EXT);

	/* Start FLASH_DMA engine */
	ctrl->dma_pending = true;
	mb();
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0x03); /* wake | run */

	if (wait_for_completion_timeout(&ctrl->dma_done, timeo) <= 0) {
		dev_err(&host->pdev->dev,
				"timeout waiting for DMA; status %#x, error status %#x\n",
				flash_dma_readl(ctrl, FLASH_DMA_STATUS),
				flash_dma_readl(ctrl, FLASH_DMA_ERROR_STATUS));
	}
	ctrl->dma_pending = false;
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0); /* force stop */
}

static int brcmstb_nand_dma_trans(struct brcmstb_nand_host *host, u64 addr,
	u32 *buf, u32 len, u8 dma_cmd)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	dma_addr_t buf_pa;
	int dir = dma_cmd == CMD_PAGE_READ ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	buf_pa = dma_map_single(&host->pdev->dev, buf, len, dir);

	brcmstb_nand_fill_dma_desc(host, ctrl->dma_desc, addr, buf_pa, len,
				   dma_cmd, true, true, 0);

	brcmstb_nand_dma_run(host, ctrl->dma_pa);

	dma_unmap_single(&host->pdev->dev, buf_pa, len, dir);

	if (ctrl->dma_desc->status_valid & FLASH_DMA_ECC_ERROR)
		return -EBADMSG;
	else if (ctrl->dma_desc->status_valid & FLASH_DMA_CORR_ERROR)
		return -EUCLEAN;

	return 0;
}

/*
 * Assumes proper CS is already set
 */
static int brcmstb_nand_read_by_pio(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, unsigned int trans,
	u32 *buf, u8 *oob, u64 *err_addr)
{
	struct brcmstb_nand_host *host = chip->priv;
	int i, j, ret = 0;

	/* Clear error addresses */
	BDEV_WR_RB(BCHP_NAND_ECC_UNC_ADDR, 0);
	BDEV_WR_RB(BCHP_NAND_ECC_CORR_ADDR, 0);

	BDEV_WR_RB(BCHP_NAND_CMD_EXT_ADDRESS,
			(host->cs << 16) | ((addr >> 32) & 0xffff));

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		BDEV_WR_RB(BCHP_NAND_CMD_ADDRESS, addr & 0xffffffff);
		/* SPARE_AREA_READ does not use ECC, so just use PAGE_READ */
		brcmstb_nand_send_cmd(host, CMD_PAGE_READ);
		brcmstb_nand_waitfunc(mtd, chip);

		if (likely(buf))
			for (j = 0; j < FC_WORDS; j++, buf++)
				*buf = le32_to_cpu(BDEV_RD(FC(j)));

		if (oob)
			oob += read_oob_from_regs(i, oob, mtd->oobsize / trans, host->hwcfg.sector_size_1k);

		if (!ret) {
			*err_addr = BDEV_RD(BCHP_NAND_ECC_UNC_ADDR) |
				((u64)(BDEV_RD(BCHP_NAND_ECC_UNC_EXT_ADDR) &
					0xffff) << 32);
			if (*err_addr)
				ret = -EBADMSG;
		}

		if (!ret) {
			*err_addr = BDEV_RD(BCHP_NAND_ECC_CORR_ADDR) |
				((u64)(BDEV_RD(BCHP_NAND_ECC_CORR_EXT_ADDR) &
					0xffff) << 32);
			if (*err_addr)
				ret = -EUCLEAN;
		}
	}

	return ret;
}

/*
 * Check a page to see if it is erased (w/ bitflips) after an uncorrectable ECC
 * error
 *
 * Because the HW ECC signals an ECC error if an erase paged has even a single
 * bitflip, we must check each ECC error to see if it is actually an erased
 * page with bitflips, not a truly corrupted page.
 *
 * On a real error, return a negative error code (-EBADMSG for ECC error), and
 * buf will contain raw data.
 * Otherwise, fill buf with 0xff and return the maximum number of
 * bitflips-per-ECC-sector to the caller.
 *
 */
static int brcmstb_nand_verify_erased_page(struct mtd_info *mtd,
		  struct nand_chip *chip, void *buf, u64 addr)
{
	int i, sas, oob_nbits, data_nbits;
	void *oob = chip->oob_poi;
	unsigned int max_bitflips = 0;
	int page = addr >> chip->page_shift;
	int ret;

	if (!buf) {
		buf = chip->buffers->databuf;
		/* Invalidate page cache */
		chip->pagebuf = -1;
	}

	sas = mtd->oobsize / chip->ecc.steps;
	oob_nbits = sas << 3;
	data_nbits = chip->ecc.size << 3;

	/* read without ecc for verification */
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
	ret = chip->ecc.read_page_raw(mtd, chip, buf, true, page);
	if (ret)
		return ret;

	for (i = 0; i < chip->ecc.steps; i++, oob += sas) {
		unsigned int bitflips = 0;

		bitflips += oob_nbits - bitmap_weight(oob, oob_nbits);
		bitflips += data_nbits - bitmap_weight(buf, data_nbits);

		buf += chip->ecc.size;
		addr += chip->ecc.size;

		/* Too many bitflips */
		if (bitflips > chip->ecc.strength)
			return -EBADMSG;

		max_bitflips = max(max_bitflips, bitflips);
	}

	return max_bitflips;
}

static int brcmstb_nand_read(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, unsigned int trans,
	u32 *buf, u8 *oob)
{
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u64 err_addr = 0;
	int err;
	bool retry = true;

	DBG("%s %llx -> %p\n", __func__, (unsigned long long)addr, buf);

try_dmaread:
#if CONTROLLER_VER >= 60
	BDEV_WR_RB(BCHP_NAND_UNCORR_ERROR_COUNT, 0);
#endif
	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		err = brcmstb_nand_dma_trans(host, addr, buf, trans * FC_BYTES,
					     CMD_PAGE_READ);
		if (err) {
			if (mtd_is_bitflip_or_eccerr(err))
				err_addr = addr;
			else
				return -EIO;
		}
	} else {
		if (oob)
			memset(oob, 0x99, mtd->oobsize);

		err = brcmstb_nand_read_by_pio(mtd, chip, addr, trans, buf,
					       oob, &err_addr);
	}

	if (mtd_is_eccerr(err)) {
		int ret = 0;
		/*
		 * CRNAND-32 : if we are doing a DMA read after a prior
		 * PIO read that reported uncorrectable error, the DMA
		 * engine captures this error in the first DMA read
		 * after this PIO read, cleared only on subsequent DMA
		 * read, so just retry once to clear a possible false
		 * error reported for current DMA read
		 */
		if (retry) {
			retry = false;
			goto try_dmaread;
		}
		ret = brcmstb_nand_verify_erased_page(mtd, chip, buf, addr);
		if (ret < 0) {
			dev_dbg(&host->pdev->dev,
					"uncorrectable error at 0x%llx\n",
					(unsigned long long)err_addr);
			mtd->ecc_stats.failed++;
			/* NAND layer expects zero on ECC errors */
			return 0;
		} else {
			if (buf)
				memset(buf, 0xff, FC_BYTES * trans);
			if (oob)
				memset(oob, 0xff, mtd->oobsize);

			dev_info(&host->pdev->dev,
					"corrected %d bitflips in blank page at 0x%llx\n",
					ret, (unsigned long long)addr);
			return ret;
		}
	}

	if (mtd_is_bitflip(err)) {
		unsigned int corrected = CORR_ERROR_COUNT;
		dev_dbg(&host->pdev->dev, "corrected error at 0x%llx\n",
			(unsigned long long)err_addr);
		mtd->ecc_stats.corrected += corrected;
		/* Always exceed the software-imposed threshold */
		return max(mtd->bitflip_threshold, corrected);
	}

	return 0;
}

static int brcmstb_nand_read_page(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;

	return brcmstb_nand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
}

static int brcmstb_nand_read_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;
	int ret;

	WR_ACC_CONTROL(host->cs, RD_ECC_EN, 0);
	WR_ACC_CONTROL(host->cs, ECC_LEVEL, 0);
	ret = brcmstb_nand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
	WR_ACC_CONTROL(host->cs, ECC_LEVEL, host->hwcfg.ecc_level);
	WR_ACC_CONTROL(host->cs, RD_ECC_EN, 1);
	return ret;
}

static int brcmstb_nand_read_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	return brcmstb_nand_read(mtd, chip, (u64)page << chip->page_shift,
			mtd->writesize >> FC_SHIFT,
			NULL, (u8 *)chip->oob_poi);
}

static int brcmstb_nand_read_oob_raw(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	struct brcmstb_nand_host *host = chip->priv;

	WR_ACC_CONTROL(host->cs, RD_ECC_EN, 0);
	WR_ACC_CONTROL(host->cs, ECC_LEVEL, 0);
	brcmstb_nand_read(mtd, chip, (u64)page << chip->page_shift,
		mtd->writesize >> FC_SHIFT,
		NULL, (u8 *)chip->oob_poi);
	WR_ACC_CONTROL(host->cs, ECC_LEVEL, host->hwcfg.ecc_level);
	WR_ACC_CONTROL(host->cs, RD_ECC_EN, 1);
	return 0;
}

static int brcmstb_nand_read_subpage(struct mtd_info *mtd,
	struct nand_chip *chip, uint32_t data_offs, uint32_t readlen,
	uint8_t *bufpoi)
{
	struct brcmstb_nand_host *host = chip->priv;

	return brcmstb_nand_read(mtd, chip, host->last_addr + data_offs,
			readlen >> FC_SHIFT, (u32 *)bufpoi, NULL);
}

static int brcmstb_nand_write(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, const u32 *buf, u8 *oob)
{
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned int i, j, trans = mtd->writesize >> FC_SHIFT;
	int status, ret = 0;

	DBG("%s %llx <- %p\n", __func__, (unsigned long long)addr, buf);

	if (unlikely((u32)buf & 0x03)) {
		dev_warn(&host->pdev->dev, "unaligned buffer: %p\n", buf);
		buf = (u32 *)((u32)buf & ~0x03);
	}

	brcmstb_nand_wp(mtd, 0);

	for (i = 0; i < MAX_CONTROLLER_OOB; i += 4)
		oob_reg_write(i, 0xffffffff);

	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		if (brcmstb_nand_dma_trans(host, addr, (u32 *)buf,
					mtd->writesize, CMD_PROGRAM_PAGE))
			ret = -EIO;
		goto out;
	}

	BDEV_WR_RB(BCHP_NAND_CMD_EXT_ADDRESS,
		(host->cs << 16) | ((addr >> 32) & 0xffff));

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		/* full address MUST be set before populating FC */
		BDEV_WR_RB(BCHP_NAND_CMD_ADDRESS, addr & 0xffffffff);

		if (buf)
			for (j = 0; j < FC_WORDS; j++, buf++)
				BDEV_WR(FC(j), cpu_to_le32(*buf));
		else if (oob)
			for (j = 0; j < FC_WORDS; j++)
				BDEV_WR(FC(j), 0xffffffff);

		if (oob) {
			oob += write_oob_to_regs(i, oob, mtd->oobsize / trans,
					host->hwcfg.sector_size_1k);
		}

		/* we cannot use SPARE_AREA_PROGRAM when PARTIAL_PAGE_EN=0 */
		brcmstb_nand_send_cmd(host, CMD_PROGRAM_PAGE);
		status = brcmstb_nand_waitfunc(mtd, chip);

		if (status & NAND_STATUS_FAIL) {
			dev_info(&host->pdev->dev, "program failed at %llx\n",
				(unsigned long long)addr);
			ret = -EIO;
			goto out;
		}
	}
out:
	brcmstb_nand_wp(mtd, 1);
	return ret;
}

static int brcmstb_nand_write_page(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;

	brcmstb_nand_write(mtd, chip, host->last_addr, (u32 *)buf, oob);
	return 0;
}

static int brcmstb_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;

	WR_ACC_CONTROL(host->cs, WR_ECC_EN, 0);
	brcmstb_nand_write(mtd, chip, host->last_addr, (u32 *)buf, oob);
	WR_ACC_CONTROL(host->cs, WR_ECC_EN, 1);
	return 0;
}

static int brcmstb_nand_write_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	return brcmstb_nand_write(mtd, chip, (u64)page << chip->page_shift, NULL,
		(u8 *)chip->oob_poi);
}

static int brcmstb_nand_write_oob_raw(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	int ret;

	WR_ACC_CONTROL(host->cs, WR_ECC_EN, 0);
	ret = brcmstb_nand_write(mtd, chip, (u64)page << chip->page_shift, NULL,
		(u8 *)chip->oob_poi);
	WR_ACC_CONTROL(host->cs, WR_ECC_EN, 1);

	return ret;
}

/***********************************************************************
 * Per-CS setup (1 NAND device)
 ***********************************************************************/

#if CONTROLLER_VER >= 71
/* use powers of two */
#elif CONTROLLER_VER >= 60
static const unsigned int block_sizes[] = { 8, 16, 128, 256, 512, 1024, 2048 };
static const unsigned int page_sizes[] = { 512, 2048, 4096, 8192 };
#elif CONTROLLER_VER >= 40
static const unsigned int block_sizes[] = { 16, 128, 8, 512, 256, 1024, 2048 };
static const unsigned int page_sizes[] = { 512, 2048, 4096, 8192 };
#else
static const unsigned int block_sizes[] = { 16, 128, 8, 512, 256 };
static const unsigned int page_sizes[] = { 512, 2048, 4096 };
#endif

static void brcmstb_nand_set_cfg(struct brcmstb_nand_host *host,
	struct brcmstb_nand_cfg *cfg)
{
#if CONTROLLER_VER >= 71
	if (cfg->block_size < 8192 || cfg->block_size > 8192*1024)
		dev_warn(&host->pdev->dev, "invalid block size %u\n",
			cfg->block_size);
	else
		WR_CONFIG_EXT(host->cs, BLOCK_SIZE,
			      ffs(cfg->block_size) - ffs(8192));

	if (cfg->page_size < 512 || cfg->page_size > 16384)
		dev_warn(&host->pdev->dev, "invalid page size %u\n",
			cfg->page_size);
	else
		WR_CONFIG_EXT(host->cs, PAGE_SIZE,
			      ffs(cfg->page_size) - ffs(512));
#else
	int i, found;

	for (i = 0, found = 0; i < ARRAY_SIZE(block_sizes); i++)
		if ((block_sizes[i] << 10) == cfg->block_size) {
			WR_CONFIG(host->cs, BLOCK_SIZE, i);
			found = 1;
		}
	if (!found)
		dev_warn(&host->pdev->dev, "invalid block size %u\n",
			cfg->block_size);

	for (i = 0, found = 0; i < ARRAY_SIZE(page_sizes); i++)
		if (page_sizes[i] == cfg->page_size) {
			WR_CONFIG(host->cs, PAGE_SIZE, i);
			found = 1;
		}
	if (!found)
		dev_warn(&host->pdev->dev, "invalid page size %u\n",
			cfg->page_size);
#endif

	if (fls64(cfg->device_size) < 23)
		dev_warn(&host->pdev->dev, "invalid device size 0x%llx\n",
			(unsigned long long)cfg->device_size);

	WR_CONFIG(host->cs, DEVICE_SIZE, fls64(cfg->device_size) - 23);
	WR_CONFIG(host->cs, DEVICE_WIDTH, cfg->device_width == 16 ? 1 : 0);
	WR_CONFIG(host->cs, COL_ADR_BYTES, cfg->col_adr_bytes);
	WR_CONFIG(host->cs, BLK_ADR_BYTES, cfg->blk_adr_bytes);
	WR_CONFIG(host->cs, FUL_ADR_BYTES, cfg->ful_adr_bytes);

	WR_ACC_CONTROL(host->cs, SPARE_AREA_SIZE, cfg->spare_area_size);
#if CONTROLLER_VER >= 50
	WR_ACC_CONTROL(host->cs, SECTOR_SIZE_1K, cfg->sector_size_1k);
#endif

	WR_ACC_CONTROL(host->cs, ECC_LEVEL, cfg->ecc_level);
	/* threshold = ceil(BCH-level * 0.75) */
	WR_CORR_THRESH(host->cs, ((cfg->ecc_level << cfg->sector_size_1k)
				* 3 + 2) / 4);
}

static void brcmstb_nand_get_cfg(struct brcmstb_nand_host *host,
	struct brcmstb_nand_cfg *cfg)
{
#if CONTROLLER_VER >= 71
	cfg->block_size = 8192 << RD_CONFIG_EXT(host->cs, BLOCK_SIZE);
	cfg->page_size = 512 << RD_CONFIG_EXT(host->cs, PAGE_SIZE);
#else
	cfg->block_size = RD_CONFIG(host->cs, BLOCK_SIZE);
	if (cfg->block_size < ARRAY_SIZE(block_sizes))
		cfg->block_size = block_sizes[cfg->block_size] << 10;
	else
		cfg->block_size = 128 << 10;

	cfg->page_size = RD_CONFIG(host->cs, PAGE_SIZE);
	if (cfg->page_size < ARRAY_SIZE(page_sizes))
		cfg->page_size = page_sizes[cfg->page_size];
	else
		cfg->page_size = 2048;
#endif
	cfg->device_size = (4ULL << 20) << RD_CONFIG(host->cs, DEVICE_SIZE);
	cfg->device_width = RD_CONFIG(host->cs, DEVICE_WIDTH) ? 16 : 8;
	cfg->col_adr_bytes = RD_CONFIG(host->cs, COL_ADR_BYTES);
	cfg->blk_adr_bytes = RD_CONFIG(host->cs, BLK_ADR_BYTES);
	cfg->ful_adr_bytes = RD_CONFIG(host->cs, FUL_ADR_BYTES);
	cfg->spare_area_size = RD_ACC_CONTROL(host->cs, SPARE_AREA_SIZE);
#if CONTROLLER_VER >= 50
	cfg->sector_size_1k = RD_ACC_CONTROL(host->cs, SECTOR_SIZE_1K);
#else
	cfg->sector_size_1k = 0;
#endif
	cfg->ecc_level = RD_ACC_CONTROL(host->cs, ECC_LEVEL);
}

static void brcmstb_nand_print_cfg(char *buf, struct brcmstb_nand_cfg *cfg)
{
	buf += sprintf(buf,
		"%lluMiB total, %uKiB blocks, %u%s pages, %uB OOB, %u-bit",
		(unsigned long long)cfg->device_size >> 20,
		cfg->block_size >> 10,
		cfg->page_size >= 1024 ? cfg->page_size >> 10 : cfg->page_size,
		cfg->page_size >= 1024 ? "KiB" : "B",
		cfg->spare_area_size, cfg->device_width);

	/* Account for Hamming ECC and for BCH 512B vs 1KiB sectors */
	if (is_hamming_ecc(cfg))
		sprintf(buf, ", Hamming ECC");
	else if (cfg->sector_size_1k)
		sprintf(buf, ", BCH-%u (1KiB sector)", cfg->ecc_level << 1);
	else
		sprintf(buf, ", BCH-%u\n", cfg->ecc_level);
}

/*
 * Return true if the two configurations are basically identical. Note that we
 * allow certain variations in spare area size.
 */
static bool brcmstb_nand_config_match(struct brcmstb_nand_cfg *orig,
		struct brcmstb_nand_cfg *new)
{
	/* Negative matches */
	if (orig->device_size != new->device_size)
		return false;
	if (orig->block_size != new->block_size)
		return false;
	if (orig->page_size != new->page_size)
		return false;
	if (orig->device_width != new->device_width)
		return false;
	if (orig->col_adr_bytes != new->col_adr_bytes)
		return false;
	/* blk_adr_bytes can be larger than expected, but not smaller */
	if (orig->blk_adr_bytes < new->blk_adr_bytes)
		return false;
	if (orig->ful_adr_bytes < new->ful_adr_bytes)
		return false;

	/* Positive matches */
	if (orig->spare_area_size == new->spare_area_size)
		return true;
	return orig->spare_area_size >= 27 &&
	       orig->spare_area_size <= new->spare_area_size;
}

/*
 * Minimum number of bytes to address a page. Calculated as:
 *     roundup(log2(size / page-size) / 8)
 *
 * NB: the following does not "round up" for non-power-of-2 'size'; but this is
 *     OK because many other things will break if 'size' is irregular...
 */
static inline int get_blk_adr_bytes(u64 size, u32 writesize)
{
	return ALIGN(ilog2(size) - ilog2(writesize), 8) >> 3;
}

static int brcmstb_nand_setup_dev(struct brcmstb_nand_host *host)
{
	struct mtd_info *mtd = &host->mtd;
	struct nand_chip *chip = &host->chip;
	struct brcmstb_nand_cfg orig_cfg, new_cfg;
	char msg[128];

	brcmstb_nand_get_cfg(host, &orig_cfg);
	host->hwcfg = orig_cfg;

	memset(&new_cfg, 0, sizeof(new_cfg));
	new_cfg.device_size = mtd->size;
	new_cfg.block_size = mtd->erasesize;
	new_cfg.page_size = mtd->writesize;
	new_cfg.spare_area_size = mtd->oobsize / (mtd->writesize >> FC_SHIFT);
	new_cfg.device_width = (chip->options & NAND_BUSWIDTH_16) ? 16 : 8;
	new_cfg.col_adr_bytes = 2;
	new_cfg.blk_adr_bytes = get_blk_adr_bytes(mtd->size, mtd->writesize);

	new_cfg.ful_adr_bytes = new_cfg.blk_adr_bytes;
	if (mtd->writesize > 512)
		new_cfg.ful_adr_bytes += new_cfg.col_adr_bytes;
	else
		new_cfg.ful_adr_bytes += 1;

	if (new_cfg.spare_area_size > MAX_CONTROLLER_OOB)
		new_cfg.spare_area_size = MAX_CONTROLLER_OOB;

	if (!brcmstb_nand_config_match(&orig_cfg, &new_cfg)) {
#if CONTROLLER_VER >= 50
		/* default to 1K sector size (if page is large enough) */
		new_cfg.sector_size_1k = (new_cfg.page_size >= 1024) ? 1 : 0;
#endif

		WR_ACC_CONTROL(host->cs, RD_ECC_EN, 1);
		WR_ACC_CONTROL(host->cs, WR_ECC_EN, 1);

		if (new_cfg.spare_area_size >= 36 && new_cfg.sector_size_1k)
			new_cfg.ecc_level = 20;
		else if (new_cfg.spare_area_size >= 22)
			new_cfg.ecc_level = 12;
		else if (chip->badblockpos == NAND_SMALL_BADBLOCK_POS)
			new_cfg.ecc_level = 5;
		else
			new_cfg.ecc_level = 8;

		brcmstb_nand_set_cfg(host, &new_cfg);
		host->hwcfg = new_cfg;

		if (BDEV_RD(BCHP_NAND_CS_NAND_SELECT) & (0x100 << host->cs)) {
			/* bootloader activated this CS */
			dev_warn(&host->pdev->dev, "overriding bootloader "
				"settings on CS%d\n", host->cs);
			brcmstb_nand_print_cfg(msg, &orig_cfg);
			dev_warn(&host->pdev->dev, "was: %s\n", msg);
			brcmstb_nand_print_cfg(msg, &new_cfg);
			dev_warn(&host->pdev->dev, "now: %s\n", msg);
		} else {
			/*
			 * nandcs= argument activated this CS; assume that
			 * nobody even tried to set the device configuration
			 */
			brcmstb_nand_print_cfg(msg, &new_cfg);
			dev_info(&host->pdev->dev, "detected %s\n", msg);
		}
	} else {
		/*
		 * Set oobsize to be consistent with controller's
		 * spare_area_size. This helps nandwrite testing.
		 */
		mtd->oobsize = orig_cfg.spare_area_size *
			       (mtd->writesize >> FC_SHIFT);

		brcmstb_nand_print_cfg(msg, &orig_cfg);
		dev_info(&host->pdev->dev, "%s\n", msg);
	}

#if CONTROLLER_VER < 70
	WR_ACC_CONTROL(host->cs, FAST_PGM_RDIN, 0);
#endif
	WR_ACC_CONTROL(host->cs, RD_ERASED_ECC_EN, 0);
	WR_ACC_CONTROL(host->cs, PARTIAL_PAGE_EN, 0);
	WR_ACC_CONTROL(host->cs, PAGE_HIT_EN, 1);
#if CONTROLLER_VER >= 60
	WR_ACC_CONTROL(host->cs, PREFETCH_EN, 0);
#endif
	mb();

	return 0;
}

static int brcmstb_nand_init_cs(struct brcmstb_nand_host *host)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	struct device_node *dn = host->of_node;
	struct platform_device *pdev = host->pdev;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int ret = 0;

	const char *part_probe_types[] = { "cmdlinepart", "ofpart", "RedBoot",
		NULL };
	struct mtd_part_parser_data ppdata = { .of_node = dn };

	ret = of_property_read_u32(dn, "reg", &host->cs);
	if (ret) {
		dev_err(&pdev->dev, "can't get chip-select\n");
		return -ENXIO;
	}

	DBG("%s: cs %d\n", __func__, host->cs);

	mtd = &host->mtd;
	chip = &host->chip;

	chip->priv = host;
	mtd->priv = chip;
	mtd->name = dev_name(&pdev->dev);
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;

	chip->IO_ADDR_R = (void *)0xdeadbeef;
	chip->IO_ADDR_W = (void *)0xdeadbeef;

	chip->cmd_ctrl = brcmstb_nand_cmd_ctrl;
	chip->cmdfunc = brcmstb_nand_cmdfunc;
	chip->waitfunc = brcmstb_nand_waitfunc;
	chip->read_byte = brcmstb_nand_read_byte;
	chip->read_buf = brcmstb_nand_read_buf;
	chip->write_buf = brcmstb_nand_write_buf;

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.read_page = brcmstb_nand_read_page;
	chip->ecc.read_subpage = brcmstb_nand_read_subpage;
	chip->ecc.write_page = brcmstb_nand_write_page;
	chip->ecc.read_page_raw = brcmstb_nand_read_page_raw;
	chip->ecc.write_page_raw = brcmstb_nand_write_page_raw;
	chip->ecc.write_oob_raw = brcmstb_nand_write_oob_raw;
	chip->ecc.read_oob_raw = brcmstb_nand_read_oob_raw;
	chip->ecc.read_oob = brcmstb_nand_read_oob;
	chip->ecc.write_oob = brcmstb_nand_write_oob;

	chip->controller = &ctrl->controller;

	if (nand_scan_ident(mtd, 1, NULL))
		return -ENXIO;

	chip->options |= NAND_NO_SUBPAGE_WRITE | NAND_SKIP_BBTSCAN;
	/*
	 * NAND_USE_BOUNCE_BUFFER option prevents us from getting
	 * passed kmapped buffer that we cannot DMA.
	 * When option is set nand_base passes preallocated poi
	 * buffer that is used as bounce buffer for DMA
	 */
	chip->options |= NAND_USE_BOUNCE_BUFFER;

	if (of_get_nand_on_flash_bbt(dn))
		chip->bbt_options |= NAND_BBT_USE_FLASH | NAND_BBT_NO_OOB;

	if (brcmstb_nand_setup_dev(host))
		return -ENXIO;

	/* nand_scan_tail() needs this to be set up */
	if (is_hamming_ecc(&host->hwcfg))
		chip->ecc.strength = 1;
	else
		chip->ecc.strength = host->hwcfg.ecc_level
				<< host->hwcfg.sector_size_1k;
	chip->ecc.size = host->hwcfg.sector_size_1k ? 1024 : 512;
	/* only use our internal HW threshold */
	mtd->bitflip_threshold = 1;

	chip->ecc.layout = brcmstb_choose_ecc_layout(host);
	if (!chip->ecc.layout)
		return -ENXIO;

	if (nand_scan_tail(mtd) || chip->scan_bbt(mtd))
		return -ENXIO;

	/* Update ecclayout info after nand_scan_tail() */
	mtd->oobavail = chip->ecc.layout->oobavail;
	mtd->ecclayout = chip->ecc.layout;

	return mtd_device_parse_register(mtd, part_probe_types, &ppdata, NULL, 0);
}

static int brcmstb_nand_suspend(struct device *dev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(dev);
	struct brcmstb_nand_host *host;

	dev_dbg(dev, "Save state for S3 suspend\n");

	list_for_each_entry(host, &ctrl->host_list, node) {
		host->hwcfg.acc_control = BDEV_RD(REG_ACC_CONTROL(host->cs));
		host->hwcfg.config = BDEV_RD(REG_CONFIG(host->cs));
#if CONTROLLER_VER >= 71
		host->hwcfg.config_ext = BDEV_RD(REG_CONFIG_EXT(host->cs));
#endif
		host->hwcfg.timing_1 = BDEV_RD(REG_TIMING_1(host->cs));
		host->hwcfg.timing_2 = BDEV_RD(REG_TIMING_2(host->cs));
	}

	ctrl->nand_cs_nand_select = BDEV_RD(BCHP_NAND_CS_NAND_SELECT);
	ctrl->nand_cs_nand_xor = BDEV_RD(BCHP_NAND_CS_NAND_XOR);
	ctrl->corr_stat_threshold = BDEV_RD(BCHP_NAND_CORR_STAT_THRESHOLD);

	if (!ctrl->irq_cascaded) {
		HIF_DISABLE_IRQ(NAND_CTLRDY);
		if (has_flash_dma(ctrl))
			flash_dma_irq_disable(ctrl);
	}

	if (has_flash_dma(ctrl))
		ctrl->flash_dma_mode = flash_dma_readl(ctrl, FLASH_DMA_MODE);

	return 0;
}

static int brcmstb_nand_resume(struct device *dev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(dev);
	struct brcmstb_nand_host *host;

	dev_dbg(dev, "Restore state after S3 suspend\n");

	if (has_flash_dma(ctrl)) {
		flash_dma_writel(ctrl, FLASH_DMA_MODE, ctrl->flash_dma_mode);
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);
	}

	BDEV_WR_RB(BCHP_NAND_CS_NAND_SELECT, ctrl->nand_cs_nand_select);
	BDEV_WR_RB(BCHP_NAND_CS_NAND_XOR, ctrl->nand_cs_nand_xor);
	BDEV_WR_RB(BCHP_NAND_CORR_STAT_THRESHOLD,
			ctrl->corr_stat_threshold);

	if (!ctrl->irq_cascaded) {
		HIF_ACK_IRQ(NAND_CTLRDY);
		HIF_ENABLE_IRQ(NAND_CTLRDY);
		if (has_flash_dma(ctrl))
			flash_dma_irq_enable(ctrl);
	}

	list_for_each_entry(host, &ctrl->host_list, node) {
		struct mtd_info *mtd = &host->mtd;
		struct nand_chip *chip = mtd->priv;

		BDEV_WR_RB(REG_ACC_CONTROL(host->cs), host->hwcfg.acc_control);
		BDEV_WR_RB(REG_CONFIG(host->cs), host->hwcfg.config);
#if CONTROLLER_VER >= 71
		BDEV_WR_RB(REG_CONFIG_EXT(host->cs), host->hwcfg.config_ext);
#endif
		BDEV_WR_RB(REG_TIMING_1(host->cs), host->hwcfg.timing_1);
		BDEV_WR_RB(REG_TIMING_2(host->cs), host->hwcfg.timing_2);

		/* Reset the chip, required by some chips after power-up */
		chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
	}

	return 0;
}

static const struct dev_pm_ops brcmstb_nand_pm_ops = {
	.suspend		= brcmstb_nand_suspend,
	.resume			= brcmstb_nand_resume,
};

/***********************************************************************
 * Platform driver setup (per controller)
 ***********************************************************************/

static int brcmstb_nand_check_irq_cascade(struct device *dev,
		struct brcmstb_nand_controller *ctrl)
{
	struct device_node *dn = dev->of_node, *int_parent;
	int ret, intlen;

	int_parent = of_parse_phandle(dn, "interrupt-parent", 0);
	if (!int_parent) {
		ctrl->irq_cascaded = false;
		return 0;
	}

	ret = of_property_read_u32(int_parent, "#interrupt-cells", &intlen);
	if (ret)
		return -EINVAL;
	if (intlen == 1) {
		dev_info(dev, "using HIF_INTR2 cascaded IRQ\n");
		ctrl->irq_cascaded = true;
	}

	return 0;
}

static int brcmstb_nand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node, *child;
	static struct brcmstb_nand_controller *ctrl;
	struct resource *res;
	int ret;

	/* We only support device-tree instantiation */
	if (!dn)
		return -ENODEV;

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	dev_set_drvdata(dev, ctrl);

	init_completion(&ctrl->done);
	init_completion(&ctrl->dma_done);
	spin_lock_init(&ctrl->controller.lock);
	init_waitqueue_head(&ctrl->controller.wq);
	INIT_LIST_HEAD(&ctrl->host_list);

	ret = brcmstb_nand_check_irq_cascade(dev, ctrl);
	if (ret)
		return ret;

	/*
	 * NAND
	 * FIXME: eventually, should get resource by name ("nand"), but the DT
	 * binding may be in flux for a while (8/16/13)
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get NAND register range\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start, resource_size(res),
				DRV_NAME)) {
		dev_err(dev, "can't request NAND memory region\n");
		return -ENODEV;
	}

	/* FLASH_DMA */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "flash-dma");
	if (res) {
		ctrl->flash_dma_base = devm_request_and_ioremap(dev, res);
		if (!ctrl->flash_dma_base)
			return -ENODEV;

		flash_dma_writel(ctrl, FLASH_DMA_MODE, 1); /* linked-list */
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);

		/* Allocate descriptor(s) */
		ctrl->dma_desc = dmam_alloc_coherent(dev,
						     sizeof(*ctrl->dma_desc),
						     &ctrl->dma_pa, GFP_KERNEL);
		if (!ctrl->dma_desc)
			return -ENOMEM;

		if (ctrl->irq_cascaded) {
			ctrl->dma_irq = platform_get_irq(pdev, 1);
			if ((int)ctrl->dma_irq < 0) {
				dev_err(dev, "missing FLASH_DMA IRQ\n");
				return -ENODEV;
			}

			ret = devm_request_irq(dev, ctrl->dma_irq,
					brcmstb_nand_dma_irq, 0, DRV_NAME,
					ctrl);
			if (ret < 0) {
				dev_err(dev, "can't allocate IRQ %d: error %d\n",
						ctrl->dma_irq, ret);
				return ret;
			}
		}

		dev_info(dev, "enabling FLASH_DMA\n");
	}

	BDEV_WR_F(NAND_CS_NAND_SELECT, AUTO_DEVICE_ID_CONFIG, 0);

	/* disable direct addressing + XOR for all NAND devices */
	BDEV_UNSET(BCHP_NAND_CS_NAND_SELECT, 0xff);
	BDEV_UNSET(BCHP_NAND_CS_NAND_XOR, 0xff);

#ifdef BCHP_NAND_CS_NAND_SELECT_NAND_WP_MASK
	if (wp_on == 2) /* SWLINUX-1818: Permanently remove write-protection */
		BDEV_WR_F_RB(NAND_CS_NAND_SELECT, NAND_WP, 0);
#else
	wp_on = 0;
#endif

	/* IRQ */
	ctrl->irq = platform_get_irq(pdev, 0);
	if ((int)ctrl->irq < 0) {
		dev_err(dev, "no IRQ defined\n");
		return -ENODEV;
	}

	if (!ctrl->irq_cascaded) {
		HIF_ACK_IRQ(NAND_CTLRDY);
		HIF_ENABLE_IRQ(NAND_CTLRDY);
		if (has_flash_dma(ctrl))
			flash_dma_irq_enable(ctrl);
	}

	if (ctrl->irq_cascaded) {
		ret = devm_request_irq(dev, ctrl->irq, brcmstb_nand_ctlrdy_irq,
				0, DRV_NAME, ctrl);
	} else {
		ret = devm_request_irq(dev, ctrl->irq, brcmstb_nand_irq,
				IRQF_SHARED, DRV_NAME, ctrl);
	}
	if (ret < 0) {
		dev_err(dev, "can't allocate IRQ %d: error %d\n",
			ctrl->irq, ret);
		goto out;
	}

	for_each_available_child_of_node(dn, child) {
		if (of_device_is_compatible(child, "brcm,nandcs")) {
			struct brcmstb_nand_host *host;

			host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
			if (!host) {
				ret = -ENOMEM;
				goto out;
			}
			host->pdev = pdev;
			host->ctrl = ctrl;
			host->of_node = child;

			ret = brcmstb_nand_init_cs(host);
			if (ret)
				continue; /* Try all chip-selects */

			list_add_tail(&host->node, &ctrl->host_list);
		}
	}

	/* No chip-selects could initialize properly */
	if (list_empty(&ctrl->host_list)) {
		ret = -ENODEV;
		goto out;
	}

	return 0;

out:
	if (!ctrl->irq_cascaded) {
		HIF_DISABLE_IRQ(NAND_CTLRDY);
		if (has_flash_dma(ctrl))
			flash_dma_irq_disable(ctrl);
	}
	return ret;
}

static int brcmstb_nand_remove(struct platform_device *pdev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(&pdev->dev);
	struct brcmstb_nand_host *host;

	list_for_each_entry(host, &ctrl->host_list, node)
		nand_release(&host->mtd);

	if (!ctrl->irq_cascaded) {
		HIF_DISABLE_IRQ(NAND_CTLRDY);
		if (has_flash_dma(ctrl))
			flash_dma_irq_disable(ctrl);
	}

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id brcmstb_nand_of_match[] = {
	{ .compatible = "brcm,brcmnand" },
	{},
};

static struct platform_driver brcmstb_nand_driver = {
	.probe			= brcmstb_nand_probe,
	.remove			= brcmstb_nand_remove,
	.driver = {
		.name		= "brcmnand",
		.owner		= THIS_MODULE,
		.pm		= &brcmstb_nand_pm_ops,
		.of_match_table = of_match_ptr(brcmstb_nand_of_match),
	}
};
module_platform_driver(brcmstb_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("NAND driver for STB chips");
