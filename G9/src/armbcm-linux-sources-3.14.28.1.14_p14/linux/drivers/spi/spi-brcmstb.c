/*
 * Copyright (C) 2009 Broadcom Corporation
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/clk.h>

#include <linux/brcmstb/brcmstb.h>

#if 0
#define DBG printk
#else
#define DBG(...) /* */
#endif

#if CONFIG_BRCM_BSPI_MAJOR_VERS >= 4
#define BSPI_HAS_4BYTE_ADDRESS
#define BSPI_HAS_FLEX_MODE
#endif

#define BSPI_LR_TIMEOUT			(1000)

#define BSPI_LR_INTERRUPTS_DATA \
	(BCHP_HIF_SPI_INTR2_CPU_SET_SPI_LR_SESSION_DONE_MASK	| \
	 BCHP_HIF_SPI_INTR2_CPU_SET_SPI_LR_FULLNESS_REACHED_MASK)

#define BSPI_LR_INTERRUPTS_ERROR \
	(BCHP_HIF_SPI_INTR2_CPU_SET_SPI_LR_OVERREAD_MASK	| \
	 BCHP_HIF_SPI_INTR2_CPU_SET_SPI_LR_IMPATIENT_MASK	| \
	 BCHP_HIF_SPI_INTR2_CPU_SET_SPI_LR_SESSION_ABORTED_MASK)

#define BSPI_LR_INTERRUPTS_ALL \
	(BSPI_LR_INTERRUPTS_ERROR | \
	 BSPI_LR_INTERRUPTS_DATA)

#define NUM_CHIPSELECT		4
#define MSPI_BASE_FREQ		27000000UL
#define SPBR_MIN		8U
#define SPBR_MAX		255U
#define MAX_SPEED_HZ		(MSPI_BASE_FREQ / (SPBR_MIN * 2))
#define BSPI_BASE_ADDR		KSEG1ADDR(0x1f000000)

#define OPCODE_RDID		0x9f
#define OPCODE_WREN		0x06
#define OPCODE_WRR		0x01
#define OPCODE_RCR		0x35
#define OPCODE_READ		0x03
#define OPCODE_RDSR		0x05
#define OPCODE_WRSR		0x01
#define OPCODE_FAST_READ	0x0B
#define OPCODE_DOR		0x3B
#define OPCODE_QOR		0x6B
#define OPCODE_FAST_READ_4B	0x0C
#define OPCODE_DOR_4B		0x3C
#define OPCODE_QOR_4B		0x6C

#define OPCODE_DIOR		0xBB
#define OPCODE_QIOR		0xEB
#define OPCODE_DIOR_4B		0xBC
#define OPCODE_QIOR_4B		0xEC

#define OPCODE_PP		0x02
#define OPCODE_PP_4B		0x12
#define OPCODE_EN4B		0xB7
#define OPCODE_EX4B		0xE9
#define OPCODE_BRWR		0x17

#define	BSPI_WIDTH_1BIT		1
#define BSPI_WIDTH_2BIT		2
#define BSPI_WIDTH_4BIT		4

#define	BSPI_ADDRLEN_3BYTES	3
#define	BSPI_ADDRLEN_4BYTES	4
#define BSPI_BR_EXTADD		BIT(7)
#define BSPI_FLASH_TYPE_SPANSION	0
#define BSPI_FLASH_TYPE_MACRONIX	1
#define BSPI_FLASH_TYPE_NUMONYX		2
#define BSPI_FLASH_TYPE_SST		3
#define BSPI_FLASH_TYPE_UNKNOWN		-1

static int nobspi;
module_param(nobspi, int, 0444);

static int bspi_width  = BSPI_WIDTH_1BIT;
module_param(bspi_width, int, 0444);

static int bspi_addrlen  = BSPI_ADDRLEN_3BYTES;
module_param(bspi_addrlen, int, 0444);

static int bspi_hp;
module_param(bspi_hp, int, 0444);

static int bspi_flash = BSPI_FLASH_TYPE_UNKNOWN;
module_param(bspi_flash, int, 0444);

struct bcmspi_parms {
	u32			speed_hz;
	u8			chip_select;
	u8			mode;
	u8			bits_per_word;
};

static const struct bcmspi_parms bcmspi_default_parms_cs0 = {
	.speed_hz		= MAX_SPEED_HZ,
	.chip_select		= 0,
	.mode			= SPI_MODE_3,
	.bits_per_word		= 8,
};

struct position {
	struct spi_message	*msg;
	struct spi_transfer	*trans;
	int			byte;
};

#define NUM_TXRAM		32
#define NUM_RXRAM		32
#define NUM_CDRAM		16

struct bcm_mspi_hw {
	u32			spcr0_lsb;		/* 0x000 */
	u32			spcr0_msb;		/* 0x004 */
	u32			spcr1_lsb;		/* 0x008 */
	u32			spcr1_msb;		/* 0x00c */
	u32			newqp;			/* 0x010 */
	u32			endqp;			/* 0x014 */
	u32			spcr2;			/* 0x018 */
	u32			reserved0;		/* 0x01c */
	u32			mspi_status;		/* 0x020 */
	u32			cptqp;			/* 0x024 */

#ifdef CONFIG_BRCM_MSPI_LEGACY
	u32			reserved1[6];		/* 0x028 */
#else
	/* MSPI v1.5 has a revision register */
	u32			spcr3;			/* 0x028 */
	u32			revision;		/* 0x02c */
	u32			reserved1[4];		/* 0x030 */
#endif

	u32			txram[NUM_TXRAM];	/* 0x040 */
	u32			rxram[NUM_RXRAM];	/* 0x0c0 */
	u32			cdram[NUM_CDRAM];	/* 0x140 */
};

struct bcm_bspi_hw {
	u32			revision_id;		/* 0x000 */
	u32			scratch;		/* 0x004 */
	u32			mast_n_boot_ctrl;	/* 0x008 */
	u32			busy_status;		/* 0x00c */
	u32			intr_status;		/* 0x010 */
	u32			b0_status;		/* 0x014 */
	u32			b0_ctrl;		/* 0x018 */
	u32			b1_status;		/* 0x01c */
	u32			b1_ctrl;		/* 0x020 */
	u32			strap_override_ctrl;	/* 0x024 */
#if CONFIG_BRCM_BSPI_MAJOR_VERS >= 4
	u32			flex_mode_enable;	/* 0x028 */
	u32			bits_per_cycle;		/* 0x02C */
	u32			bits_per_phase;		/* 0x030 */
	u32			cmd_and_mode_byte;	/* 0x034 */
	u32			flash_upper_addr_byte;	/* 0x038 */
	u32			xor_value;		/* 0x03C */
	u32			xor_enable;		/* 0x040 */
	u32			pio_mode_enable;	/* 0x044 */
	u32			pio_iodir;		/* 0x048 */
	u32			pio_data;		/* 0x04C */
#endif
};

struct bcm_bspi_raf {
	u32			start_address;		/* 0x00 */
	u32			num_words;		/* 0x04 */
	u32			ctrl;			/* 0x08 */
	u32			fullness;		/* 0x0C */
	u32			watermark;		/* 0x10 */
	u32			status;			/* 0x14 */
	u32			read_data;		/* 0x18 */
	u32			word_cnt;		/* 0x1C */
	u32			curr_addr;		/* 0x20 */
};

struct bcm_flex_mode {
	int			width;
	int			addrlen;
	int			hp;
};

#define STATE_IDLE		0
#define STATE_RUNNING		1
#define STATE_SHUTDOWN		2
#define STATE_SUSPEND		3

struct bcmspi_priv {
	struct platform_device	*pdev;
	struct spi_master	*master;
	struct clk		*clk;
	spinlock_t		lock;
	struct bcmspi_parms	last_parms;
	struct position		pos;
	struct list_head	msg_queue;
	int			state;
	int			outstanding_bytes;
	int			next_udelay;
	int			cs_change;
	unsigned int		max_speed_hz;
	volatile struct bcm_mspi_hw *mspi_hw;
	int			irq;
	struct tasklet_struct	tasklet;
	int			curr_cs;

	/* BSPI */
	volatile struct bcm_bspi_hw *bspi_hw;
	int			bspi_enabled;
	/* all chip selects controlled by BSPI */
	int			bspi_chip_select;

	/* LR */
	volatile struct bcm_bspi_raf *bspi_hw_raf;
	struct spi_transfer	*cur_xfer;
	u32			cur_xfer_idx;
	u32			cur_xfer_len;
	u32			xfer_status;
	struct spi_message	*cur_msg;
	u32			actual_length;

	/* current flex mode settings */
	struct bcm_flex_mode	flex_mode;

	/* S3 warm boot context save */
	u32			s3_intr2_mask;
	u32			s3_strap_override_ctrl;
};

static void bcmspi_enable_interrupt(u32 mask)
{
	BDEV_SET_RB(BCHP_HIF_SPI_INTR2_CPU_MASK_CLEAR, mask);
}

static void bcmspi_disable_interrupt(u32 mask)
{
	BDEV_SET_RB(BCHP_HIF_SPI_INTR2_CPU_MASK_SET, mask);
}

static void bcmspi_clear_interrupt(u32 mask)
{
	BDEV_SET_RB(BCHP_HIF_SPI_INTR2_CPU_CLEAR, mask);
}

static u32 bcmspi_read_interrupt(void)
{
	return BDEV_RD(BCHP_HIF_SPI_INTR2_CPU_STATUS);
}

static int bcmspi_busy_poll(struct bcmspi_priv *priv)
{
	int i;

	/* this should normally finish within 10us */
	for (i = 0; i < 1000; i++) {
		if ((priv->bspi_hw->busy_status & 1) == 0)
			return 0;
		udelay(1);
	}
	dev_warn(&priv->pdev->dev, "timeout waiting for !busy_status\n");
	return -EIO;
}

static void bcmspi_flush_prefetch_buffers(struct bcmspi_priv *priv)
{
	/* SWLINUX-2407: avoid flushing if B1 prefetch is still active */
	bcmspi_busy_poll(priv);

	/* Force rising edge for the b0/b1 'flush' field */
	priv->bspi_hw->b0_ctrl = 1;
	priv->bspi_hw->b1_ctrl = 1;
	priv->bspi_hw->b0_ctrl = 0;
	priv->bspi_hw->b1_ctrl = 0;
}

static int bcmspi_lr_is_fifo_empty(struct bcmspi_priv *priv)
{
	return priv->bspi_hw_raf->status & BCHP_BSPI_RAF_STATUS_FIFO_EMPTY_MASK;
}

/* BSPI v3 LR is LE only, convert data to host endianness */
#if CONFIG_BRCM_BSPI_MAJOR_VERS >= 4
#define BSPI_DATA(a) (a)
#else
#define BSPI_DATA(a) le32_to_cpu(a)
#endif

static inline u32 bcmspi_lr_read_fifo(struct bcmspi_priv *priv)
{
	return BSPI_DATA(priv->bspi_hw_raf->read_data);
}

static inline void bcmspi_lr_start(struct bcmspi_priv *priv)
{
	priv->bspi_hw_raf->ctrl = BCHP_BSPI_RAF_CTRL_START_MASK;
}

static inline void bcmspi_lr_clear(struct bcmspi_priv *priv)
{
	priv->bspi_hw_raf->ctrl = BCHP_BSPI_RAF_CTRL_CLEAR_MASK;
	bcmspi_flush_prefetch_buffers(priv);
}

static inline int bcmspi_is_4_byte_mode(struct bcmspi_priv *priv)
{
	return priv->flex_mode.addrlen == BSPI_ADDRLEN_4BYTES;
}

static int bcmspi_set_override_ctrl(struct bcmspi_priv *priv,
	int width, int addrlen, int hp)
{
	u32 strap_override = priv->bspi_hw->strap_override_ctrl;

	switch (width) {
	case BSPI_WIDTH_4BIT:
		strap_override &= ~0x2; /* clear dual mode */
		strap_override |= 0x9; /* set quad mode + override */
		break;
	case BSPI_WIDTH_2BIT:
		strap_override &= ~0x8; /* clear quad mode */
		strap_override |= 0x3; /* set dual mode + override */
		break;
	case BSPI_WIDTH_1BIT:
		strap_override &= ~0xA; /* clear quad/dual mode */
		strap_override |= 1; /* set override */
		break;
	default:
		break;
	}

	if (addrlen == BSPI_ADDRLEN_4BYTES) {
		strap_override |= 0x5; /* set 4byte + override */
	} else {
		strap_override &= ~0x4; /* clear 4 byte */
		strap_override |= 0x1; /* set override */
	}

	priv->bspi_hw->strap_override_ctrl = strap_override;

	return 0;
}

#ifdef BSPI_HAS_FLEX_MODE
static int bcmbspi_flash_type(struct bcmspi_priv *priv);

static int bcmspi_set_flex_mode(struct bcmspi_priv *priv,
	int width, int addrlen, int hp)
{
	int bpc = 0, bpp = 0, command;
	int flex_mode = 1, error = 0;

	if (priv->bspi_hw->strap_override_ctrl & 0x1 ||
		priv->s3_strap_override_ctrl & 0x1) {
		flex_mode = 0;
		command = OPCODE_FAST_READ;
		error = bcmspi_set_override_ctrl(priv, width, addrlen, hp);
		goto override;
	}

	switch (width) {
	case BSPI_WIDTH_1BIT:
		bpp = 8; /* dummy cycles */
		if (addrlen == BSPI_ADDRLEN_3BYTES) {
			/* default mode, does not need flex_cmd */
			flex_mode = 0;
			command = OPCODE_FAST_READ;
		} else {
			bpp |= BCHP_BSPI_BITS_PER_PHASE_addr_bpp_select_MASK;
			command = OPCODE_FAST_READ_4B;
		}
		break;
	case BSPI_WIDTH_2BIT:
		bpc = 0x00000001; /* only data is 2-bit */
		if (hp) {
			bpc |= 0x00010100; /* address and mode are 2-bit too */
			bpp |= BCHP_BSPI_BITS_PER_PHASE_mode_bpp_MASK;
			command = OPCODE_DIOR;
			if (addrlen == BSPI_ADDRLEN_4BYTES) {
				bpp |=
				 BCHP_BSPI_BITS_PER_PHASE_addr_bpp_select_MASK;
				command = OPCODE_DIOR_4B;
			}
		} else {
			bpp = 8; /* dummy cycles */
			command = OPCODE_DOR;
			if (addrlen == BSPI_ADDRLEN_4BYTES) {
				bpp |=
				 BCHP_BSPI_BITS_PER_PHASE_addr_bpp_select_MASK;
				command = OPCODE_DOR_4B;
			}
		}
		break;
	case BSPI_WIDTH_4BIT:
		bpc = 0x00000002; /* only data is 4-bit */
		if (hp) {
			bpc |= 0x00020200; /* address and mode are 4-bit too */
			bpp = 4; /* dummy cycles */
			bpp |= BCHP_BSPI_BITS_PER_PHASE_mode_bpp_MASK;
			command = OPCODE_QIOR;
			if (addrlen == BSPI_ADDRLEN_4BYTES) {
				bpp |=
				 BCHP_BSPI_BITS_PER_PHASE_addr_bpp_select_MASK;
				command = OPCODE_QIOR_4B;
			}
		} else {
			bpp = 8; /* dummy cycles */
			command = OPCODE_QOR;
			if (addrlen == BSPI_ADDRLEN_4BYTES) {
				bpp |=
				 BCHP_BSPI_BITS_PER_PHASE_addr_bpp_select_MASK;
				command = OPCODE_QOR_4B;
			}
		}
		break;
	default:
		error = 1;
		break;
	}

override:
	if (!error) {
		priv->bspi_hw->flex_mode_enable = 0;
		priv->bspi_hw->bits_per_cycle = bpc;
		priv->bspi_hw->bits_per_phase = bpp;
		priv->bspi_hw->cmd_and_mode_byte = command;
		priv->bspi_hw->flex_mode_enable = flex_mode ?
			BCHP_BSPI_FLEX_MODE_ENABLE_bspi_flex_mode_enable_MASK
			: 0;
		DBG("%s: width=%d addrlen=%d hp=%d\n",
			__func__, width, addrlen, hp);
		DBG("%s: fme=%08x bpc=%08x bpp=%08x cmd=%08x\n", __func__,
			priv->bspi_hw->flex_mode_enable,
			priv->bspi_hw->bits_per_cycle,
			priv->bspi_hw->bits_per_phase,
			priv->bspi_hw->cmd_and_mode_byte);
	}

	return error;
}
#else
static int bcmspi_set_flex_mode(struct bcmspi_priv *priv,
	int width, int addrlen, int hp)
{
	return bcmspi_set_override_ctrl(priv, width, addrlen, hp);
}
#endif

static void bcmspi_set_mode(struct bcmspi_priv *priv,
	int width, int addrlen, int hp)
{
	int error = 0;

	if (width == -1)
		width = priv->flex_mode.width;
	if (addrlen == -1)
		addrlen = priv->flex_mode.addrlen;
	if (hp == -1)
		hp = priv->flex_mode.hp;

	error = bcmspi_set_flex_mode(priv, width, addrlen, hp);

	if (!error) {
		priv->flex_mode.width = width;
		priv->flex_mode.addrlen = addrlen;
		priv->flex_mode.hp = hp;
		dev_info(&priv->pdev->dev,
			"%d-lane output, %d-byte address%s\n",
			priv->flex_mode.width,
			priv->flex_mode.addrlen,
			priv->flex_mode.hp ? ", high-performance mode" : "");
	} else
		dev_warn(&priv->pdev->dev,
			"INVALID COMBINATION: width=%d addrlen=%d hp=%d\n",
			width, addrlen, hp);
}

static void bcmspi_set_chip_select(struct bcmspi_priv *priv, int cs)
{
	if (priv->curr_cs != cs) {
		DBG("Switching CS%1d => CS%1d\n",
			priv->curr_cs, cs);
		BDEV_WR_RB(BCHP_EBI_CS_SPI_SELECT,
			(BDEV_RD(BCHP_EBI_CS_SPI_SELECT) & ~0xff) |
			(1 << cs));
		udelay(10);
	}
	priv->curr_cs = cs;

}

static inline int is_bspi_chip_select(struct bcmspi_priv *priv, u8 cs)
{
	return priv->bspi_chip_select & (1 << cs);
}

static void bcmspi_disable_bspi(struct bcmspi_priv *priv)
{
	if (!priv->bspi_hw || !priv->bspi_enabled)
		return;

	priv->bspi_enabled = 0;
	if ((priv->bspi_hw->mast_n_boot_ctrl & 1) == 1)
		return;

	DBG("disabling bspi\n");
	bcmspi_busy_poll(priv);
	priv->bspi_hw->mast_n_boot_ctrl = 1;
	udelay(1);
}

static void bcmspi_enable_bspi(struct bcmspi_priv *priv)
{
	if (!priv->bspi_hw || priv->bspi_enabled)
		return;
	if ((priv->bspi_hw->mast_n_boot_ctrl & 1) == 0) {
		priv->bspi_enabled = 1;
		return;
	}

	DBG("enabling bspi\n");
	bcmspi_flush_prefetch_buffers(priv);
	udelay(1);
	priv->bspi_hw->mast_n_boot_ctrl = 0;
	priv->bspi_enabled = 1;
}

static void bcmspi_hw_set_parms(struct bcmspi_priv *priv,
	const struct bcmspi_parms *xp)
{
	if (xp->speed_hz) {
		unsigned int spbr = MSPI_BASE_FREQ / (2 * xp->speed_hz);

		priv->mspi_hw->spcr0_lsb = max(min(spbr, SPBR_MAX), SPBR_MIN);
	} else {
		priv->mspi_hw->spcr0_lsb = SPBR_MIN;
	}
	priv->mspi_hw->spcr0_msb =
#ifdef CONFIG_MSPI_LEGACY
		0x80 |	/* MSTR bit */
#endif
		((xp->bits_per_word ? xp->bits_per_word : 8) << 2) |
		(xp->mode & 3);
	priv->last_parms = *xp;
}

#define PARMS_NO_OVERRIDE		0
#define PARMS_OVERRIDE			1

static int bcmspi_update_parms(struct bcmspi_priv *priv,
	struct spi_device *spidev, struct spi_transfer *trans, int override)
{
	struct bcmspi_parms xp;

	xp.speed_hz = min(trans->speed_hz ? trans->speed_hz :
		(spidev->max_speed_hz ? spidev->max_speed_hz : MAX_SPEED_HZ),
		MAX_SPEED_HZ);
	xp.chip_select = spidev->chip_select;
	xp.mode = spidev->mode;
	xp.bits_per_word = trans->bits_per_word ? trans->bits_per_word :
		(spidev->bits_per_word ? spidev->bits_per_word : 8);

	if ((override == PARMS_OVERRIDE) ||
		((xp.speed_hz == priv->last_parms.speed_hz) &&
		 (xp.chip_select == priv->last_parms.chip_select) &&
		 (xp.mode == priv->last_parms.mode) &&
		 (xp.bits_per_word == priv->last_parms.bits_per_word))) {
		bcmspi_hw_set_parms(priv, &xp);
		return 0;
	}
	/* no override, and parms do not match */
	return 1;
}


static int bcmspi_setup(struct spi_device *spi)
{
	struct bcmspi_parms *xp;
	struct bcmspi_priv *priv = spi_master_get_devdata(spi->master);

	DBG("%s\n", __func__);

	if (spi->bits_per_word > 16)
		return -EINVAL;

	xp = spi_get_ctldata(spi);
	if (!xp) {
		xp = kzalloc(sizeof(struct bcmspi_parms), GFP_KERNEL);
		if (!xp)
			return -ENOMEM;
		spi_set_ctldata(spi, xp);
	}
	if (spi->max_speed_hz < priv->max_speed_hz)
		xp->speed_hz = spi->max_speed_hz;
	else
		xp->speed_hz = 0;

	xp->chip_select = spi->chip_select;
	xp->mode = spi->mode;
	xp->bits_per_word = spi->bits_per_word ? spi->bits_per_word : 8;

	return 0;
}

/* stop at end of transfer, no other reason */
#define FNB_BREAK_NONE			0
/* stop at end of spi_message */
#define FNB_BREAK_EOM			1
/* stop at end of spi_transfer if delay */
#define FNB_BREAK_DELAY			2
/* stop at end of spi_transfer if cs_change */
#define FNB_BREAK_CS_CHANGE		4
/* stop if we run out of bytes */
#define FNB_BREAK_NO_BYTES		8

/* events that make us stop filling TX slots */
#define FNB_BREAK_TX			(FNB_BREAK_EOM | FNB_BREAK_DELAY | \
					 FNB_BREAK_CS_CHANGE)

/* events that make us deassert CS */
#define FNB_BREAK_DESELECT		(FNB_BREAK_EOM | FNB_BREAK_CS_CHANGE)


static int find_next_byte(struct bcmspi_priv *priv, struct position *p,
	struct list_head *completed, int flags)
{
	int ret = FNB_BREAK_NONE;

	p->byte++;

	while (p->byte >= p->trans->len) {
		/* we're at the end of the spi_transfer */

		/* in TX mode, need to pause for a delay or CS change */
		if (p->trans->delay_usecs && (flags & FNB_BREAK_DELAY))
			ret |= FNB_BREAK_DELAY;
		if (p->trans->cs_change && (flags & FNB_BREAK_CS_CHANGE))
			ret |= FNB_BREAK_CS_CHANGE;
		if (ret)
			return ret;

		/* advance to next spi_message? */
		if (list_is_last(&p->trans->transfer_list,
				&p->msg->transfers)) {
			struct spi_message *next_msg = NULL;

			/* TX breaks at the end of each message as well */
			if (!completed || (flags & FNB_BREAK_EOM)) {
				DBG("find_next_byte: advance msg exit\n");
				return FNB_BREAK_EOM;
			}
			if (!list_is_last(&p->msg->queue, &priv->msg_queue)) {
				next_msg = list_entry(p->msg->queue.next,
					struct spi_message, queue);
			}
			/* delete from run queue, add to completion queue */
			list_del(&p->msg->queue);
			list_add_tail(&p->msg->queue, completed);

			p->msg = next_msg;
			p->byte = 0;
			if (p->msg == NULL) {
				p->trans = NULL;
				ret = FNB_BREAK_NO_BYTES;
				break;
			}

			/*
			 * move on to the first spi_transfer of the new
			 * spi_message
			 */
			p->trans = list_entry(p->msg->transfers.next,
				struct spi_transfer, transfer_list);
		} else {
			/* or just advance to the next spi_transfer */
			p->trans = list_entry(p->trans->transfer_list.next,
				struct spi_transfer, transfer_list);
			p->byte = 0;
		}
	}
	DBG("find_next_byte: msg %p trans %p len %d byte %d ret %x\n",
		p->msg, p->trans, p->trans ? p->trans->len : 0, p->byte, ret);
	return ret;
}

static void read_from_hw(struct bcmspi_priv *priv, struct list_head *completed)
{
	struct position p;
	int slot = 0, n = priv->outstanding_bytes;

	DBG("%s\n", __func__);

	p = priv->pos;

	while (n > 0) {
		BUG_ON(p.msg == NULL);

		if (p.trans->bits_per_word <= 8) {
			u8 *buf = p.trans->rx_buf;

			if (buf) {
				buf[p.byte] =
					priv->mspi_hw->rxram[(slot << 1) + 1]
						& 0xff;
			}
			DBG("RD %02x\n", buf ? buf[p.byte] : 0xff);
		} else {
			u16 *buf = p.trans->rx_buf;

			if (buf) {
				buf[p.byte] =
					((priv->mspi_hw->rxram[(slot << 1) + 1]
						& 0xff) << 0) |
					((priv->mspi_hw->rxram[(slot << 1) + 0]
						& 0xff) << 8);
			}
		}
		slot++;
		n--;
		p.msg->actual_length++;

		find_next_byte(priv, &p, completed, FNB_BREAK_NONE);
	}

	priv->pos = p;
	priv->outstanding_bytes = 0;
}

static void write_to_hw(struct bcmspi_priv *priv)
{
	struct position p;
	int slot = 0, fnb = 0;
	struct spi_message *msg = NULL;

	DBG("%s\n", __func__);

	bcmspi_disable_bspi(priv);

	p = priv->pos;

	while (1) {
		if (p.msg == NULL)
			break;
		if (!msg) {
			msg = p.msg;
			bcmspi_update_parms(priv, msg->spi, p.trans,
				PARMS_OVERRIDE);
		} else {
			/* break if the speed, bits, etc. changed */
			if (bcmspi_update_parms(priv, msg->spi, p.trans,
				PARMS_NO_OVERRIDE)) {
				DBG("parms don't match, breaking\n");
				break;
			}
		}
		if (p.trans->bits_per_word <= 8) {
			const u8 *buf = p.trans->tx_buf;

			priv->mspi_hw->txram[slot << 1] = buf ?
				(buf[p.byte] & 0xff) : 0xff;
			DBG("WR %02x\n", buf ? buf[p.byte] : 0xff);
		} else {
			const u16 *buf = p.trans->tx_buf;

			priv->mspi_hw->txram[(slot << 1) + 0] = buf ?
				(buf[p.byte] >> 8) : 0xff;
			priv->mspi_hw->txram[(slot << 1) + 1] = buf ?
				(buf[p.byte] & 0xff) : 0xff;
		}
		priv->mspi_hw->cdram[slot] =
			((p.trans->bits_per_word <= 8) ? 0x8e : 0xce);
		slot++;

		fnb = find_next_byte(priv, &p, NULL, FNB_BREAK_TX);

		if (fnb & FNB_BREAK_CS_CHANGE)
			priv->cs_change = 1;
		if (fnb & FNB_BREAK_DELAY)
			priv->next_udelay = p.trans->delay_usecs;
		if (fnb || (slot == NUM_CDRAM))
			break;
	}
	if (slot) {
		DBG("submitting %d slots\n", slot);
		priv->mspi_hw->newqp = 0;
		priv->mspi_hw->endqp = slot - 1;

		/* deassert CS on the final byte */
		if (fnb & FNB_BREAK_DESELECT)
			priv->mspi_hw->cdram[slot - 1] &= ~0x80;

		/* tell HIF_MSPI which CS to use */
		bcmspi_set_chip_select(priv, msg->spi->chip_select);

		BDEV_WR_F_RB(HIF_MSPI_WRITE_LOCK, WriteLock, 1);
		priv->mspi_hw->spcr2 = 0xe0;	/* cont | spe | spifie */

		priv->state = STATE_RUNNING;
		priv->outstanding_bytes = slot;
	} else {
		BDEV_WR_F_RB(HIF_MSPI_WRITE_LOCK, WriteLock, 0);
		priv->state = STATE_IDLE;
	}
}

#define DWORD_ALIGNED(a) (!(((unsigned long)(a)) & 3))
#define ADDR_TO_4MBYTE_SEGMENT(addr)	(((u32)(addr)) >> 22)

static int bcmspi_emulate_flash_read(struct bcmspi_priv *priv,
	struct spi_message *msg)
{
	struct spi_transfer *trans;
	u32 addr, len, len_in_dwords;
	u8 *buf;
	unsigned long flags;
	int idx;

#ifndef BSPI_HAS_4BYTE_ADDRESS
	/* 4-byte address mode is not supported through BSPI */
	if (bcmspi_is_4_byte_mode(priv))
		return -1;
#endif

	/* acquire lock when the MSPI is idle */
	while (1) {
		spin_lock_irqsave(&priv->lock, flags);
		if (priv->state == STATE_IDLE)
			break;
		spin_unlock_irqrestore(&priv->lock, flags);
		if (priv->state == STATE_SHUTDOWN)
			return -EIO;
		udelay(1);
	}
	bcmspi_set_chip_select(priv, msg->spi->chip_select);

	/* first transfer - OPCODE_READ + {3,4}-byte address */
	trans = list_entry(msg->transfers.next, struct spi_transfer,
		transfer_list);
	buf = (void *)trans->tx_buf;

	idx = 1;

#ifdef BSPI_HAS_4BYTE_ADDRESS
	if (bcmspi_is_4_byte_mode(priv))
		priv->bspi_hw->flash_upper_addr_byte = buf[idx++] << 24;
	else
		priv->bspi_hw->flash_upper_addr_byte = 0;
#endif

	addr = (buf[idx] << 16) | (buf[idx+1] << 8) | buf[idx+2];
	addr |= priv->bspi_hw->flash_upper_addr_byte;

	/* second transfer - read result into buffer */
	trans = list_entry(msg->transfers.next->next, struct spi_transfer,
		transfer_list);

	buf = (void *)trans->rx_buf;
	len = trans->len;

#if CONFIG_BRCM_BSPI_MAJOR_VERS < 4
	/*
	 * The address coming into this function is a raw flash offset.  But
	 * for BSPI <= V3, we need to convert it to a remapped BSPI address.
	 * If it crosses a 4MB boundary, just revert back to using MSPI.
	 */
	addr = (addr + 0xc00000) & 0xffffff;

	if (ADDR_TO_4MBYTE_SEGMENT(addr) ^
	    ADDR_TO_4MBYTE_SEGMENT(addr + len - 1)) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return -1;
	}
#endif

	/* non-aligned and very short transfers are handled by MSPI */
	if (unlikely(!DWORD_ALIGNED(addr) ||
		     !DWORD_ALIGNED(buf) ||
		     len < sizeof(u32) ||
		     !priv->bspi_hw_raf)) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return -1;
	}

	bcmspi_enable_bspi(priv);

	len_in_dwords = (len + 3) >> 2;

	/* initialize software parameters */
	priv->xfer_status = 0;
	priv->cur_xfer = trans;
	priv->cur_xfer_idx = 0;
	priv->cur_xfer_len = len;
	priv->cur_msg = msg;
	priv->actual_length = idx + 4 + trans->len;

	/* setup hardware */
	/* address must be 4-byte aligned */
	priv->bspi_hw_raf->start_address = addr;
	priv->bspi_hw_raf->num_words = len_in_dwords;
	priv->bspi_hw_raf->watermark = 0;

	DBG("READ: %08x %08x (%08x)\n", addr, len_in_dwords, trans->len);

	bcmspi_clear_interrupt(0xffffffff);
	bcmspi_enable_interrupt(BSPI_LR_INTERRUPTS_ALL);
	bcmspi_lr_start(priv);

	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static int bcmspi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct bcmspi_priv *priv = spi_master_get_devdata(spi->master);
	unsigned long flags;

	if (is_bspi_chip_select(priv, msg->spi->chip_select)) {
		struct spi_transfer *trans;

		trans = list_entry(msg->transfers.next,
			struct spi_transfer, transfer_list);
		if (trans && trans->len && trans->tx_buf) {
			u8 command = ((u8 *)trans->tx_buf)[0];
			switch (command) {
			case OPCODE_FAST_READ_4B:
				if (!bcmspi_is_4_byte_mode(priv))
					bcmspi_set_mode(priv, -1,
						 BSPI_ADDRLEN_4BYTES, -1);
				/* fall through */
			case OPCODE_FAST_READ:
				if (bcmspi_emulate_flash_read(priv, msg) == 0)
					return 0;
				break;
			case OPCODE_QIOR_4B:
			case OPCODE_QOR_4B:
			case OPCODE_QOR:
				if (bcmspi_emulate_flash_read(priv, msg) == 0)
					return 0;
				break;
			case OPCODE_EN4B:
				DBG("ENABLE 4-BYTE MODE\n");
				bcmspi_set_mode(priv, -1,
					BSPI_ADDRLEN_4BYTES, -1);
				break;
			case OPCODE_EX4B:
				DBG("DISABLE 4-BYTE MODE\n");
				bcmspi_set_mode(priv, -1,
					BSPI_ADDRLEN_3BYTES, -1);
				break;
			case OPCODE_BRWR:
				{
					u8 enable = ((u8 *)trans->tx_buf)[1];
					DBG("%s 4-BYTE MODE\n",
					    enable ? "ENABLE" : "DISABLE");
					bcmspi_set_mode(priv, -1,
						enable ? BSPI_ADDRLEN_4BYTES :
						BSPI_ADDRLEN_3BYTES, -1);
				}
				break;
			case OPCODE_PP:
				DBG("Program page 2-3Byte addressing command:\n");
				break;
			case OPCODE_PP_4B:
				DBG("Program page 4Byte addressing command:\n");
				break;

			default:
				break;
			}
		}
	}

	spin_lock_irqsave(&priv->lock, flags);

	if (priv->state == STATE_SHUTDOWN) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return -EIO;
	}

	msg->actual_length = 0;

	list_add_tail(&msg->queue, &priv->msg_queue);

	if (priv->state == STATE_IDLE) {
		BUG_ON(priv->pos.msg != NULL);
		priv->pos.msg = msg;
		priv->pos.trans = list_entry(msg->transfers.next,
			struct spi_transfer, transfer_list);
		priv->pos.byte = 0;

		write_to_hw(priv);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static void bcmspi_cleanup(struct spi_device *spi)
{
	struct bcmspi_parms *xp = spi_get_ctldata(spi);

	DBG("%s\n", __func__);

	kfree(xp);
}

static irqreturn_t bcmspi_interrupt(int irq, void *dev_id)
{
	struct bcmspi_priv *priv = dev_id;

	if (priv->bspi_enabled && priv->cur_xfer) {
		int done = 0;
		u32 status = bcmspi_read_interrupt();
		u32 *buf = (u32 *)priv->cur_xfer->rx_buf;
		if (status & BSPI_LR_INTERRUPTS_DATA) {
			while (!bcmspi_lr_is_fifo_empty(priv)) {
				u32 data = bcmspi_lr_read_fifo(priv);
				if (likely(priv->cur_xfer_len >= 4)) {
					buf[priv->cur_xfer_idx++] = data;
					priv->cur_xfer_len -= 4;
				} else {
					/*
					 * Read out remaining bytes, make sure
					 * we do not cross the buffer boundary
					 */
					u8 *cbuf =
						(u8 *)&buf[priv->cur_xfer_idx];
					data = cpu_to_le32(data);
					while (priv->cur_xfer_len) {
						*cbuf++ = (u8)data;
						data >>= 8;
						priv->cur_xfer_len--;
					}
				}
			}
		}
		if (status & BSPI_LR_INTERRUPTS_ERROR) {
			dev_err(&priv->pdev->dev, "ERROR %02x\n", status);
			priv->xfer_status = -EIO;
		} else if (!priv->cur_xfer_len)
			done = 1;
		if (done) {
			priv->cur_xfer = NULL;
			bcmspi_disable_interrupt(BSPI_LR_INTERRUPTS_ALL);

			if (priv->xfer_status) {
				bcmspi_lr_clear(priv);
			} else {
				bcmspi_flush_prefetch_buffers(priv);

				if (priv->cur_msg) {
					priv->cur_msg->actual_length =
						priv->actual_length;
					priv->cur_msg->complete(
						priv->cur_msg->context);
					priv->cur_msg->status = 0;
				}
			}
			priv->cur_msg = NULL;
		}
		bcmspi_clear_interrupt(status);
		return IRQ_HANDLED;
	}

	if (priv->mspi_hw->mspi_status & 1) {
		/* clear interrupt */
		priv->mspi_hw->mspi_status &= ~1;
		bcmspi_clear_interrupt(
			BCHP_HIF_SPI_INTR2_CPU_SET_MSPI_DONE_MASK);

		tasklet_schedule(&priv->tasklet);
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

static void bcmspi_tasklet(unsigned long param)
{
	struct bcmspi_priv *priv = (void *)param;
	struct list_head completed;
	struct spi_message *msg;
	unsigned long flags;

	INIT_LIST_HEAD(&completed);
	spin_lock_irqsave(&priv->lock, flags);

	if (priv->next_udelay) {
		udelay(priv->next_udelay);
		priv->next_udelay = 0;
	}

	msg = priv->pos.msg;

	read_from_hw(priv, &completed);
	if (priv->cs_change) {
		udelay(10);
		priv->cs_change = 0;
	}

	write_to_hw(priv);

	while (!list_empty(&completed)) {
		msg = list_first_entry(&completed, struct spi_message, queue);
		list_del(&msg->queue);
		msg->status = 0;
		if (msg->complete)
			msg->complete(msg->context);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void bcmspi_complete(void *arg)
{
	complete(arg);
}

static int bcmspi_simple_transaction(struct bcmspi_priv *priv,
	const void *tx_buf, int tx_len, void *rx_buf, int rx_len)
{
	struct bcmspi_parms *xp = &priv->last_parms;
	DECLARE_COMPLETION_ONSTACK(fini);
	struct spi_message m;
	struct spi_transfer t_tx, t_rx;
	struct spi_device spi;
	int ret;

	memset(&spi, 0, sizeof(spi));
	spi.max_speed_hz = xp->speed_hz;
	spi.chip_select = xp->chip_select;
	spi.mode = xp->mode;
	spi.bits_per_word = xp->bits_per_word;
	spi.master = priv->master;

	spi_message_init(&m);
	m.complete = bcmspi_complete;
	m.context = &fini;
	m.spi = &spi;

	memset(&t_tx, 0, sizeof(t_tx));
	memset(&t_rx, 0, sizeof(t_rx));
	t_tx.tx_buf = tx_buf;
	t_tx.len = tx_len;
	t_rx.rx_buf = rx_buf;
	t_rx.len = rx_len;

	if (tx_len)
		spi_message_add_tail(&t_tx, &m);
	if (rx_len)
		spi_message_add_tail(&t_rx, &m);

	ret = bcmspi_transfer(&spi, &m);
	if (!ret)
		wait_for_completion(&fini);
	return ret;
}

static void bcmspi_hw_init(struct bcmspi_priv *priv)
{
	priv->mspi_hw->spcr1_lsb = 0;
	priv->mspi_hw->spcr1_msb = 0;
	priv->mspi_hw->newqp = 0;
	priv->mspi_hw->endqp = 0;
	priv->mspi_hw->spcr2 = 0x20;	/* spifie */
#ifndef CONFIG_BRCM_MSPI_LEGACY
	priv->mspi_hw->spcr3 = 0;
#endif

	bcmspi_hw_set_parms(priv, &bcmspi_default_parms_cs0);

	if (!priv->bspi_hw)
		return;

	priv->bspi_enabled = 1;
	bcmspi_disable_bspi(priv);

#if CONFIG_BRCM_BSPI_MAJOR_VERS >= 4
	/* Force a sane 1:1 mapping of BSPI address -> flash offset */
	priv->bspi_hw->xor_enable = 1;
	priv->bspi_hw->xor_value = 0;
#endif

	priv->bspi_hw->b0_ctrl = 0;
	priv->bspi_hw->b1_ctrl = 0;
}

static void bcmspi_hw_uninit(struct bcmspi_priv *priv)
{
	priv->mspi_hw->spcr2 = 0x0;	/* disable irq and enable bits */
	bcmspi_enable_bspi(priv);
}

static int bcmbspi_flash_type(struct bcmspi_priv *priv)
{
	char tx_buf[4];
	unsigned char jedec_id[5] = {0};

	/* command line override */
	if (bspi_flash != BSPI_FLASH_TYPE_UNKNOWN)
		return bspi_flash;

	/* Read ID */
	tx_buf[0] = OPCODE_RDID;
	bcmspi_simple_transaction(priv, tx_buf, 1, &jedec_id, 5);

	switch (jedec_id[0]) {
	case 0x01: /* Spansion */
	case 0xef:
		bspi_flash = BSPI_FLASH_TYPE_SPANSION;
		break;
	case 0xc2: /* Macronix */
		bspi_flash = BSPI_FLASH_TYPE_MACRONIX;
		break;
	case 0xbf: /* SST */
		bspi_flash = BSPI_FLASH_TYPE_SST;
		break;
	case 0x89: /* Numonyx */
		bspi_flash = BSPI_FLASH_TYPE_NUMONYX;
		break;
	default:
		bspi_flash = BSPI_FLASH_TYPE_UNKNOWN;
		break;
	}
	return bspi_flash;
}

static int bcmspi_set_quad_mode(struct bcmspi_priv *priv, int _enable)
{
	char tx_buf[4];
	unsigned char cfg_reg, sts_reg;

	switch (bcmbspi_flash_type(priv)) {
	case BSPI_FLASH_TYPE_SPANSION:
		/* RCR */
		tx_buf[0] = OPCODE_RCR;
		bcmspi_simple_transaction(priv, tx_buf, 1, &cfg_reg, 1);
		if (_enable)
			cfg_reg |= 0x2;
		else
			cfg_reg &= ~0x2;
		/* WREN */
		tx_buf[0] = OPCODE_WREN;
		bcmspi_simple_transaction(priv, tx_buf, 1, NULL, 0);
		/* WRR */
		tx_buf[0] = OPCODE_WRR;
		tx_buf[1] = 0; /* status register */
		tx_buf[2] = cfg_reg; /* configuration register */
		bcmspi_simple_transaction(priv, tx_buf, 3, NULL, 0);
		/* wait till ready */
		do {
			tx_buf[0] = OPCODE_RDSR;
			bcmspi_simple_transaction(priv, tx_buf, 1, &sts_reg,
						  1);
			udelay(1);
		} while (sts_reg & 1);
		break;
	case BSPI_FLASH_TYPE_MACRONIX:
		/* RDSR */
		tx_buf[0] = OPCODE_RDSR;
		bcmspi_simple_transaction(priv, tx_buf, 1, &cfg_reg, 1);
		if (_enable)
			cfg_reg |= 0x40;
		else
			cfg_reg &= ~0x40;
		/* WREN */
		tx_buf[0] = OPCODE_WREN;
		bcmspi_simple_transaction(priv, tx_buf, 1, NULL, 0);
		/* WRSR */
		tx_buf[0] = OPCODE_WRSR;
		tx_buf[1] = cfg_reg; /* status register */
		bcmspi_simple_transaction(priv, tx_buf, 2, NULL, 0);
		/* wait till ready */
		do {
			tx_buf[0] = OPCODE_RDSR;
			bcmspi_simple_transaction(priv, tx_buf, 1, &sts_reg,
						  1);
			udelay(1);
		} while (sts_reg & 1);
		/* RDSR */
		tx_buf[0] = OPCODE_RDSR;
		bcmspi_simple_transaction(priv, tx_buf, 1, &cfg_reg, 1);
		break;
	case BSPI_FLASH_TYPE_SST:
	case BSPI_FLASH_TYPE_NUMONYX:
		/* TODO - send Quad mode control command */
		break;
	default:
		if (_enable)
			dev_err(&priv->pdev->dev,
				"Unrecognized flash type, no QUAD MODE support");
		return _enable ? -1 : 0;
	}

	return 0;
}

static int bcmspi_enable_disable_4byte_addressing(struct bcmspi_priv *priv,
						  int _enable)
{
	char tx_buf[4];
	int ret = 0;

	switch (bcmbspi_flash_type(priv)) {
	case BSPI_FLASH_TYPE_SPANSION:
		tx_buf[0] = OPCODE_BRWR;
		if (_enable)
			tx_buf[1] = BSPI_BR_EXTADD;
		else
			tx_buf[1] = 0;
		ret = bcmspi_simple_transaction(priv, tx_buf, 2, NULL, 0);
		break;
	case BSPI_FLASH_TYPE_SST:
	case BSPI_FLASH_TYPE_NUMONYX:
	case BSPI_FLASH_TYPE_MACRONIX:
		/* enable write by sending WREN */
		tx_buf[0] = OPCODE_WREN;
		ret = bcmspi_simple_transaction(priv, tx_buf, 1, NULL, 0);
		if (ret != 0)
			return ret;

		if (_enable)
			tx_buf[0] = OPCODE_EN4B;
		else
			tx_buf[0] = OPCODE_EX4B;
		ret = bcmspi_simple_transaction(priv, tx_buf, 1, NULL, 0);
		break;
	default:
		if (_enable)
			dev_err(&priv->pdev->dev,
				"4-byte addressing not supported\n");
		return _enable ? -1 : 0;
	}

	if (ret != 0)
		dev_err(&priv->pdev->dev,
			"Unable to enter/exit 4-byte address mode\n");

	return ret;
}

/* Configure the BSPI chip-select */
static int bcmspi_bspi_config_cs(struct bcmspi_priv *priv)
{
	struct platform_device *pdev = priv->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node, *childnode;
	struct spi_master *master = priv->master;

	if (!priv->bspi_hw) {
		priv->bspi_chip_select = 0;
		return 0;
	}

	priv->bspi_chip_select = 0;
	for_each_child_of_node(dn, childnode) {
		if (of_find_property(childnode, "use-bspi", NULL)) {
			const u32 *regp;
			int size;

			/* "reg" field holds chip-select number */
			regp = of_get_property(childnode, "reg", &size);
			if (!regp || size != sizeof(*regp))
				return -EINVAL;
			if (regp[0] < master->num_chipselect)
				priv->bspi_chip_select |= (1 << regp[0]);
		}
	}
	return 0;
}

static int bcmspi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bcmspi_priv *priv;
	struct spi_master *master;
	struct resource *res;
	int ret;
	u32 temp;

	DBG("bcmspi_probe\n");

	if (!dev->of_node)
		return -ENODEV;

	BDEV_WR_RB(BCHP_BSPI_MAST_N_BOOT_CTRL, 1);
	bcmspi_disable_interrupt(0xffffffff);
	bcmspi_clear_interrupt(0xffffffff);
	bcmspi_enable_interrupt(BCHP_HIF_SPI_INTR2_CPU_SET_MSPI_DONE_MASK);

	master = spi_alloc_master(dev, sizeof(struct bcmspi_priv));
	if (!master) {
		dev_err(&pdev->dev, "error allocating spi_master\n");
		return -ENOMEM;
	}

	priv = spi_master_get_devdata(master);

	priv->pdev = pdev;
	priv->state = STATE_IDLE;
	priv->pos.msg = NULL;
	priv->master = master;

	master->bus_num = pdev->id;
	if (!of_property_read_u32(dev->of_node, "num-cs", &temp)) {
		master->num_chipselect = temp;
	} else {
		/* Not provided by device-tree; use default */
		master->num_chipselect = NUM_CHIPSELECT;
	}
	master->mode_bits = SPI_MODE_3;

	master->setup = bcmspi_setup;
	master->transfer = bcmspi_transfer;
	master->cleanup = bcmspi_cleanup;
	master->dev.of_node = dev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't get resource 0\n");
		ret = -EIO;
		goto err2;
	}
	/* MSPI register range */
	priv->mspi_hw = (volatile void *)devm_ioremap(&pdev->dev,
		res->start, res->end - res->start);
	if (!priv->mspi_hw) {
		dev_err(&pdev->dev, "can't ioremap\n");
		ret = -EIO;
		goto err2;
	}

	/* IRQ */
	priv->irq = platform_get_irq(pdev, 0);
	if (priv->irq < 0) {
		dev_err(&pdev->dev, "no IRQ defined\n");
		ret = -ENODEV;
		goto err2;
	}

	ret = devm_request_irq(&pdev->dev, priv->irq, bcmspi_interrupt, 0,
			"spi_brcmstb", priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to allocate IRQ\n");
		goto err2;
	}

	/* BSPI register range (not supported on all platforms) */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res && !nobspi) {
		priv->bspi_hw = (volatile void *)devm_ioremap(&pdev->dev,
			res->start, res->end - res->start);
		if (!priv->bspi_hw) {
			dev_err(&pdev->dev, "can't ioremap BSPI range\n");
			ret = -EIO;
			goto err2;
		}
	} else
		priv->bspi_hw = NULL;

	/* BSPI_RAF register range */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res && !nobspi) {
		priv->bspi_hw_raf = (volatile void *)devm_ioremap(&pdev->dev,
			res->start, res->end - res->start);
		if (!priv->bspi_hw_raf) {
			dev_err(&pdev->dev, "can't ioremap BSPI_RAF range\n");
			ret = -EIO;
			goto err2;
		}
	} else
		priv->bspi_hw_raf = NULL;

	/* Clock */
	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "unable to get clock\n");
		/* Ignore, if we don't have any clocks to get */
		priv->clk = NULL;
	}
	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to prepare clock\n");
		goto err2;
	}

	bcmspi_hw_init(priv);
	priv->curr_cs = -1;

	ret = bcmspi_bspi_config_cs(priv);
	if (ret) {
		dev_err(&pdev->dev, "error detecting chip-select\n");
		goto err1;
	}

#ifdef BCHP_SUN_TOP_CTRL_STRAP_VALUE_0_strap_nand_flash_boot_MASK
	if (BDEV_RD(BCHP_SUN_TOP_CTRL_STRAP_VALUE_0) &
		BCHP_SUN_TOP_CTRL_STRAP_VALUE_0_strap_nand_flash_boot_MASK) {
		/* BSPI is disabled if the board is strapped for NAND */
		priv->bspi_chip_select = 0;
	}
#endif /* BCHP_SUN_TOP_CTRL_STRAP_VALUE_0_strap_nand_flash_boot_MASK */


	INIT_LIST_HEAD(&priv->msg_queue);
	spin_lock_init(&priv->lock);

	platform_set_drvdata(pdev, priv);

	tasklet_init(&priv->tasklet, bcmspi_tasklet, (unsigned long)priv);

	/* default values - undefined */
	priv->flex_mode.width =
	priv->flex_mode.addrlen =
	priv->flex_mode.hp = -1;

	if (priv->bspi_chip_select) {
		if ((BDEV_RD(BCHP_BSPI_STRAP_OVERRIDE_CTRL) &
		     BCHP_BSPI_STRAP_OVERRIDE_CTRL_data_quad_MASK)) {
			if (bcmspi_set_quad_mode(priv, BSPI_WIDTH_4BIT))
				bspi_width = BSPI_WIDTH_1BIT;
			else
				bspi_width = BSPI_WIDTH_4BIT;
		}
		bcmspi_set_mode(priv, bspi_width, bspi_addrlen, bspi_hp);
	}

	if (bcmspi_is_4_byte_mode(priv))
	    bcmspi_enable_disable_4byte_addressing(priv, BSPI_ADDRLEN_4BYTES);

	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't register master\n");
		goto err1;
	}

	return 0;

err1:
	bcmspi_hw_uninit(priv);
	clk_disable_unprepare(priv->clk);
err2:
	spi_master_put(master);
	return ret;
}

static int bcmspi_remove(struct platform_device *pdev)
{
	struct bcmspi_priv *priv = platform_get_drvdata(pdev);
	unsigned long flags;

	/* acquire lock when the MSPI is idle */
	while (1) {
		spin_lock_irqsave(&priv->lock, flags);
		if (priv->state == STATE_IDLE)
			break;
		spin_unlock_irqrestore(&priv->lock, flags);
		udelay(100);
	}
	priv->state = STATE_SHUTDOWN;
	spin_unlock_irqrestore(&priv->lock, flags);

	tasklet_kill(&priv->tasklet);
	platform_set_drvdata(pdev, NULL);
	bcmspi_hw_uninit(priv);
	clk_disable_unprepare(priv->clk);
	spi_unregister_master(priv->master);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bcmspi_suspend(struct device *dev)
{
	struct bcmspi_priv *priv = dev_get_drvdata(dev);
	unsigned long flags;

	/* acquire lock when the MSPI is idle */
	while (1) {
		spin_lock_irqsave(&priv->lock, flags);
		if (priv->state == STATE_IDLE) {
			BDEV_WR_F_RB(HIF_MSPI_WRITE_LOCK, WriteLock, 0);
			priv->state = STATE_SUSPEND;
			break;
		}
		spin_unlock_irqrestore(&priv->lock, flags);
		if (priv->state == STATE_SHUTDOWN)
			return -EIO;
		udelay(1);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	priv->s3_intr2_mask = BDEV_RD(BCHP_HIF_SPI_INTR2_CPU_MASK_STATUS);
	priv->s3_strap_override_ctrl = BDEV_RD(BCHP_BSPI_STRAP_OVERRIDE_CTRL);

	clk_disable(priv->clk);
	bcmspi_hw_uninit(priv);
	return 0;
};

static int bcmspi_resume(struct device *dev)
{
	struct bcmspi_priv *priv = dev_get_drvdata(dev);
	int curr_cs = priv->curr_cs;
	int width = -1, addrlen = -1;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	priv->state = STATE_IDLE;
	spin_unlock_irqrestore(&priv->lock, flags);

	ret = clk_enable(priv->clk);
	if (ret != 0)
		return ret;

	bcmspi_hw_init(priv);
	priv->curr_cs = -1;
	bcmspi_set_chip_select(priv, curr_cs);
	BDEV_WR_RB(BCHP_HIF_SPI_INTR2_CPU_MASK_CLEAR, ~priv->s3_intr2_mask);

	if (bcmspi_is_4_byte_mode(priv) ||
	    (priv->s3_strap_override_ctrl &
	     BCHP_BSPI_STRAP_OVERRIDE_CTRL_addr_4byte_n_3byte_MASK))
		addrlen = BSPI_ADDRLEN_4BYTES;

	if (bspi_width == BSPI_WIDTH_4BIT ||
	    (priv->s3_strap_override_ctrl &
	     BCHP_BSPI_STRAP_OVERRIDE_CTRL_data_quad_MASK)) {
		width = BSPI_WIDTH_4BIT;

		if (bcmspi_set_quad_mode(priv, width))
			bspi_width = BSPI_WIDTH_1BIT;
	}

	bcmspi_set_mode(priv, width, addrlen, -1);
	if (bcmspi_is_4_byte_mode(priv))
	    bcmspi_enable_disable_4byte_addressing(priv, BSPI_ADDRLEN_4BYTES);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(bcmspi_pm_ops, bcmspi_suspend, bcmspi_resume);

static const struct of_device_id spi_brcmstb_of_match[] = {
	{ .compatible = "brcm,spi-brcmstb" },
	{},
};

static struct platform_driver spi_brcmstb_driver = {
	.driver = {
		.name = "spi_brcmstb",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.pm		= &bcmspi_pm_ops,
		.of_match_table = spi_brcmstb_of_match,
	},
	.probe = bcmspi_probe,
	.remove = bcmspi_remove,
};

module_platform_driver(spi_brcmstb_driver);

MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("MSPI/HIF SPI driver");
MODULE_LICENSE("GPL");
