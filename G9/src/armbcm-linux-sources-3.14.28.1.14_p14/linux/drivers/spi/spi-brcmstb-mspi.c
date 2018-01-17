/*
 * Copyright (C) 2009-2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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

#define DBG(...)			pr_debug(__VA_ARGS__)

/* These defaults are used if not provided in device tree or board file */
#define NUM_CHIPSELECT		4
#define MSPI_BASE_FREQ		27000000UL

#define SPBR_MIN		8U
#define SPBR_MAX		255U

/*
 * The registers controlling these parameters are shared across all chip
 * selects and so must be updated when they differ across CS, not just when
 * a change is requested.
 */
struct bcmspi_parms {
	u32			speed_hz;
	u8			mode;
	u8			bits_per_word;
};

static struct bcmspi_parms bcmspi_default_parms_cs0 = {
	.mode			= SPI_MODE_3,
	.bits_per_word		= 8,
};

struct position {
	struct spi_transfer	*trans;
	int			byte;
};

#define NUM_TXRAM		32
#define NUM_RXRAM		32
#define NUM_CDRAM		16

#define CDRAM_OUTP	(1 << 8)   /* data direction for half-duplex */
#define CDRAM_CONT	(1 << 7)   /* continue driving SSb after transfer */
#define CDRAM_BITSE	(1 << 6)   /* bits per transfer (0 = 8 bits,
			              1 = use SPCR0_MSB) */
#define CDRAM_DT	(1 << 5)   /* Delay after transfer (0 = 32 clks,
			              1 = use SPCR1_LSB */
#define CDRAM_DSCK	(1 << 4)   /* PCS to SCK delay (0 = 1/2 SCK, 1 =
				    SPCR1_MSB determines */
#define CDRAM_PCS	0xf      /* Peripheral chip selects, active low */

#define BRCM_MSPI_REG_NONE  U16_MAX

enum brcm_mspi_reg {
	BRCM_MSPI_SPCR0_LSB = 0,
	BRCM_MSPI_SPCR0_MSB,
	BRCM_MSPI_SPCR1_LSB,
	BRCM_MSPI_SPCR1_MSB,
	BRCM_MSPI_NEWQP,
	BRCM_MSPI_ENDQP,
	BRCM_MSPI_SPCR2,
	BRCM_MSPI_MSPI_STATUS,
	BRCM_MSPI_CPTQP,
	BRCM_MSPI_TXRAM,
	BRCM_MSPI_RXRAM,
	BRCM_MSPI_CDRAM,
	BRCM_MSPI_WRITE_LOCK,  /* only when paired with BSPI */
	/* below added in MSPI v1.5 */
	BRCM_MSPI_SPCR3,
	BRCM_MSPI_REVISION,
};

/* Older MSPI versions that don't have a revision reg */
static const u16 brcm_mspi_regs_legacy[] = {
	[BRCM_MSPI_SPCR0_LSB]		= 0x000,
	[BRCM_MSPI_SPCR0_MSB]		= 0x004,
	[BRCM_MSPI_SPCR1_LSB]		= 0x008,
	[BRCM_MSPI_SPCR1_MSB]		= 0x00c,
	[BRCM_MSPI_NEWQP]		= 0x010,
	[BRCM_MSPI_ENDQP]		= 0x014,
	[BRCM_MSPI_SPCR2]		= 0x018,
	[BRCM_MSPI_MSPI_STATUS]		= 0x020,
	[BRCM_MSPI_CPTQP]		= 0x024,
	[BRCM_MSPI_SPCR3]		= BRCM_MSPI_REG_NONE,
	[BRCM_MSPI_REVISION]		= BRCM_MSPI_REG_NONE,
	[BRCM_MSPI_TXRAM]		= 0x040,
	[BRCM_MSPI_RXRAM]		= 0x0c0,
	[BRCM_MSPI_CDRAM]		= 0x140,
	[BRCM_MSPI_WRITE_LOCK]		= 0x180,
};

/* MSPI v1.5 */
static const u16 brcm_mspi_regs_v1_5[] = {
	[BRCM_MSPI_SPCR0_LSB]		= 0x000,
	[BRCM_MSPI_SPCR0_MSB]		= 0x004,
	[BRCM_MSPI_SPCR1_LSB]		= 0x008,
	[BRCM_MSPI_SPCR1_MSB]		= 0x00c,
	[BRCM_MSPI_NEWQP]		= 0x010,
	[BRCM_MSPI_ENDQP]		= 0x014,
	[BRCM_MSPI_SPCR2]		= 0x018,
	[BRCM_MSPI_MSPI_STATUS]		= 0x020,
	[BRCM_MSPI_CPTQP]		= 0x024,
	[BRCM_MSPI_SPCR3]		= 0x028,
	[BRCM_MSPI_REVISION]		= 0x02c,
	[BRCM_MSPI_TXRAM]		= 0x040,
	[BRCM_MSPI_RXRAM]		= 0x0c0,
	[BRCM_MSPI_CDRAM]		= 0x140,
	[BRCM_MSPI_WRITE_LOCK]		= 0x180,
};

/* BSPI registers */
#define BSPI_MAST_N_BOOT_CTRL			0x08

#define STATE_IDLE		0
#define STATE_RUNNING		1
#define STATE_SHUTDOWN		2

struct bcmspi_priv {
	struct platform_device	*pdev;
	struct spi_master	*master;
	struct clk		*clk;
	u8			revision;
	const u16		*reg_offsets;
	unsigned long		base_freq;
	struct bcmspi_parms	last_parms;
	struct position		pos;
	int			next_udelay;
	int			cs_change;
	unsigned long		max_speed_hz;
	void __iomem		*mspi_base;
	void __iomem		*bspi;

	struct completion	done;
};

static inline bool has_bspi(struct bcmspi_priv *priv)
{
	return priv->bspi;
}

static inline u32 brcm_mspi_readl(void __iomem *addr)
{
	/*
	 * MIPS endianness is configured by boot strap, which also reverses all
	 * bus endianness (i.e., big-endian CPU + big endian bus ==> native
	 * endian I/O).
	 *
	 * Other architectures (e.g., ARM) either do not support big endian, or
	 * else leave I/O in little endian mode.
	 */
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		return __raw_readl(addr);
	else
		return readl_relaxed(addr);
}

static inline void brcm_mspi_writel(u32 val, void __iomem *addr)
{
	/* See brcm_mspi_readl() comments */
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		__raw_writel(val, addr);
	else
		writel_relaxed(val, addr);
}

static inline u32 brcm_mspi_readl_offset(struct bcmspi_priv *priv, u32 offset)
{
	return brcm_mspi_readl(priv->mspi_base + offset);
}

static inline void brcm_mspi_writel_offset(struct bcmspi_priv *priv, u32 offset,
				u32 val)
{
	return brcm_mspi_writel(val, priv->mspi_base + offset);
}

static inline u32 brcm_mspi_readreg(struct bcmspi_priv *priv,
		enum brcm_mspi_reg reg)
{
	u16 offset = priv->reg_offsets[reg];

	if (offset == BRCM_MSPI_REG_NONE)
		return 0;
	else
		return brcm_mspi_readl_offset(priv, offset);
}

static inline void brcm_mspi_writereg(struct bcmspi_priv *priv,
		enum brcm_mspi_reg reg, u32 val)
{
	u16 offset = priv->reg_offsets[reg];

	if (offset != BRCM_MSPI_REG_NONE)
		brcm_mspi_writel_offset(priv, offset, val);
}

/* True if parms changed, false if not */
static bool bcmspi_parms_did_change(const struct bcmspi_parms * const cur,
		const struct bcmspi_parms * const prev)
{
	return (cur->speed_hz != prev->speed_hz) ||
		(cur->mode != prev->mode) ||
		(cur->bits_per_word != prev->bits_per_word);
}

static void bcmspi_hw_set_parms(struct bcmspi_priv *priv,
	const struct bcmspi_parms *xp)
{
	u32 val;

	if (!bcmspi_parms_did_change(xp, &priv->last_parms))
		return;

	if (xp->speed_hz) {
		unsigned int spbr = priv->base_freq / (2 * xp->speed_hz);

		val = max(min(spbr, SPBR_MAX), SPBR_MIN);
	} else {
		/* go as fast as possible */
		val = SPBR_MIN;
	}
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR0_LSB, val);

	val =
#ifdef CONFIG_BRCM_MSPI_LEGACY
		0x80 |	/* MSTR bit */
#endif
		((xp->bits_per_word ? xp->bits_per_word : 8) << 2) |
		(xp->mode & 3);
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR0_MSB, val);
	priv->last_parms = *xp;
}

static void bcmspi_update_parms(struct bcmspi_priv *priv,
	struct spi_device *spidev, struct spi_transfer *trans)
{
	struct bcmspi_parms xp;

	/* transfer can override spidev-level speed */
	xp.speed_hz = min(trans->speed_hz ? trans->speed_hz :
		(spidev->max_speed_hz ? spidev->max_speed_hz :
		 priv->max_speed_hz),
		priv->max_speed_hz);
	xp.mode = spidev->mode;
	xp.bits_per_word = trans->bits_per_word ? trans->bits_per_word :
		(spidev->bits_per_word ? spidev->bits_per_word : 8);

	bcmspi_hw_set_parms(priv, &xp);
}


static int bcmspi_setup(struct spi_device *spi)
{
	struct bcmspi_parms *xp;
	struct bcmspi_priv *priv = spi_master_get_devdata(spi->master);

	if (spi->bits_per_word > 16)
		return -EINVAL;

	xp = spi_get_ctldata(spi);
	if (!xp) {
		xp = kzalloc(sizeof(*xp), GFP_KERNEL);
		if (!xp)
			return -ENOMEM;
		spi_set_ctldata(spi, xp);
	}
	if (spi->max_speed_hz < priv->max_speed_hz)
		xp->speed_hz = spi->max_speed_hz;
	else
		xp->speed_hz = 0;

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


/**
 * find_next_byte() - advance byte and check what flags should be set
 * @priv: pointer to bcmspi private struct
 * @p: pointer to current position pointer
 * @flags: flags to take into account at the end of a spi_transfer
 *
 * Advances to the next byte, incrementing the byte in the position pointer p.
 * If any flags are passed in and we're at the end of a transfer, those are
 * applied to the return value if applicable to the spi_transfer.
 *
 * Return: flags describing the break condition or FNB_BREAK_NONE.
 */
static int find_next_byte(struct bcmspi_priv *priv, struct position *p,
			  int flags)
{
	int ret = FNB_BREAK_NONE;

	if (p->trans->bits_per_word <= 8)
		p->byte++;
	else
		p->byte += 2;

	if (p->byte >= p->trans->len) {
		/* we're at the end of the spi_transfer */

		/* in TX mode, need to pause for a delay or CS change */
		if (p->trans->delay_usecs && (flags & FNB_BREAK_DELAY))
			ret |= FNB_BREAK_DELAY;
		if (p->trans->cs_change && (flags & FNB_BREAK_CS_CHANGE))
			ret |= FNB_BREAK_CS_CHANGE;
		if (ret)
			goto done;

		DBG("find_next_byte: advance msg exit\n");
		if (spi_transfer_is_last(priv->master, p->trans))
			ret = FNB_BREAK_EOM;
		else
			ret = FNB_BREAK_NO_BYTES;
		/* FIXME: we probably need to check for NULL values in other
		 * parts of the code */
		p->trans = NULL;
	}

done:
	DBG("find_next_byte: trans %p len %d byte %d ret %x\n",
		p->trans, p->trans ? p->trans->len : 0, p->byte, ret);
	return ret;
}

static inline u8 read_rxram_slot_u8(struct bcmspi_priv *priv, int slot)
{
	u16 slot_offset = priv->reg_offsets[BRCM_MSPI_RXRAM] +
			 (slot << 3) + 0x4;

	/* mask out reserved bits */
	return brcm_mspi_readl_offset(priv, slot_offset) & 0xff;
}

static inline u16 read_rxram_slot_u16(struct bcmspi_priv *priv, int slot)
{
	u16 reg_offset = priv->reg_offsets[BRCM_MSPI_RXRAM];
	u16 lsb_offset = reg_offset + (slot << 3) + 0x4;
	u16 msb_offset = reg_offset + (slot << 3);

	return (brcm_mspi_readl_offset(priv, lsb_offset) & 0xff) |
		((brcm_mspi_readl_offset(priv, msb_offset) & 0xff) << 8);
}

static void read_from_hw(struct bcmspi_priv *priv, int slots)
{
	struct position p;
	int slot;

	if (slots > NUM_CDRAM) {
		/* should never happen */
		dev_err(&priv->pdev->dev, "%s: too many slots!\n", __func__);
		return;
	}

	p = priv->pos;

	for (slot = 0; slot < slots; slot++) {
		if (p.trans->bits_per_word <= 8) {
			u8 *buf = p.trans->rx_buf;

			if (buf)
				buf[p.byte] = read_rxram_slot_u8(priv, slot);
			DBG("RD %02x\n", buf ? buf[p.byte] : 0xff);
		} else {
			u16 *buf = p.trans->rx_buf;

			if (buf)
				buf[p.byte / 2] = read_rxram_slot_u16(priv, slot);
			DBG("RD %04x\n", buf ? buf[p.byte] : 0xffff);
		}

		find_next_byte(priv, &p, FNB_BREAK_NONE);
	}

	priv->pos = p;
}

static inline void write_txram_slot_u8(struct bcmspi_priv *priv, int slot,
		u8 val)
{
	u16 reg_offset = priv->reg_offsets[BRCM_MSPI_TXRAM] +
			 (slot << 3);

	/* mask out reserved bits */
	brcm_mspi_writel_offset(priv, reg_offset, val);
}

static inline void write_txram_slot_u16(struct bcmspi_priv *priv, int slot,
		u16 val)
{
	u16 reg_offset = priv->reg_offsets[BRCM_MSPI_TXRAM];
	u16 msb_offset = reg_offset + (slot << 3);
	u16 lsb_offset = reg_offset + (slot << 3) + 0x4;

	brcm_mspi_writel_offset(priv, msb_offset, (val >> 8));
	brcm_mspi_writel_offset(priv, lsb_offset, val & 0xff);
}

static inline u32 read_cdram_slot(struct bcmspi_priv *priv, int slot)
{
	return brcm_mspi_readl_offset(priv,
			priv->reg_offsets[BRCM_MSPI_CDRAM] + (slot << 2));
}

static inline void write_cdram_slot(struct bcmspi_priv *priv, int slot, u32 val)
{
	u16 reg_offset = priv->reg_offsets[BRCM_MSPI_CDRAM] + (slot << 2);

	brcm_mspi_writel_offset(priv, reg_offset, val);
}

/* Return number of slots written */
static int write_to_hw(struct bcmspi_priv *priv, struct spi_device *spidev)
{
	struct position p;
	int slot = 0, fnb = 0;

	p = priv->pos;

	bcmspi_update_parms(priv, spidev, p.trans);

	/*
	 * TODO: would be nice to have some sanity checking for testing only
	 * _just_ in case some parameters change out from under us
	 */

	/* Run until end of transfer or reached the max data */
	while (!fnb && slot < NUM_CDRAM) {
		if (p.trans->bits_per_word <= 8) {
			const u8 *buf = p.trans->tx_buf;
			u8 val = buf ? buf[p.byte] : 0xff;

			write_txram_slot_u8(priv, slot, val);
			DBG("WR %02x\n", val);
		} else {
			const u16 *buf = p.trans->tx_buf;
			u16 val = buf ? buf[p.byte / 2] : 0xffff;

			write_txram_slot_u16(priv, slot, val);
			DBG("WR %04x\n", val);
		}
		write_cdram_slot(priv, slot,
			(CDRAM_CONT |
			 (~(1 << spidev->chip_select) & CDRAM_PCS)) |
			((p.trans->bits_per_word <= 8) ? 0 : CDRAM_BITSE));

		/* NOTE: This can update p.trans */
		fnb = find_next_byte(priv, &p, FNB_BREAK_TX);
		slot++;
	}
	if (!slot) {
		dev_err(&priv->pdev->dev, "%s: no data to send?", __func__);
		goto done;
	}

	/* in TX mode, need to pause for a delay or CS change */
	if (fnb & FNB_BREAK_CS_CHANGE)
		priv->cs_change = 1;
	if (fnb & FNB_BREAK_DELAY)
		priv->next_udelay = p.trans->delay_usecs;

	DBG("submitting %d slots\n", slot);
	brcm_mspi_writereg(priv, BRCM_MSPI_NEWQP, 0);
	brcm_mspi_writereg(priv, BRCM_MSPI_ENDQP, slot - 1);

	/* deassert CS on the final byte */
	if (fnb & FNB_BREAK_DESELECT)
		write_cdram_slot(priv, slot - 1,
				read_cdram_slot(priv, slot - 1) & ~CDRAM_CONT);

	if (has_bspi(priv))
		brcm_mspi_writereg(priv, BRCM_MSPI_WRITE_LOCK, 1);

	/* Must flush previous writes before starting MSPI operation */
	mb();
	/* Set cont | spe | spifie */
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR2, 0xe0);

done:
	return slot;
}

static void hw_stop(struct bcmspi_priv *priv)
{
	if (has_bspi(priv))
		brcm_mspi_writereg(priv, BRCM_MSPI_WRITE_LOCK, 0);
}

static int bcmspi_transfer_one(struct spi_master *master,
		struct spi_device *spidev, struct spi_transfer *trans)
{
	struct bcmspi_priv *priv = spi_master_get_devdata(master);
	int slots;

	priv->pos.trans = trans;
	priv->pos.byte = 0;

	while (priv->pos.byte < trans->len) {
		reinit_completion(&priv->done);

		slots = write_to_hw(priv, spidev);

		if (!wait_for_completion_timeout(&priv->done, HZ)) {
			dev_err(&priv->pdev->dev, "timeout waiting for MSPI\n");
			return -EIO;
		}

		if (priv->next_udelay) {
			udelay(priv->next_udelay);
			priv->next_udelay = 0;
		}

		read_from_hw(priv, slots);
		if (priv->cs_change) {
			udelay(10);
			priv->cs_change = 0;
		}
	}

	if (spi_transfer_is_last(master, trans))
		hw_stop(priv);

	/* 0 indicates transfer complete */
	return 0;
}

static void bcmspi_cleanup(struct spi_device *spi)
{
	struct bcmspi_parms *xp = spi_get_ctldata(spi);

	kfree(xp);
}

static irqreturn_t bcmspi_interrupt(int irq, void *dev_id)
{
	struct bcmspi_priv *priv = dev_id;
	u32 status = brcm_mspi_readreg(priv, BRCM_MSPI_MSPI_STATUS);

	if (status & 1) {
		/* clear interrupt */
		brcm_mspi_writereg(priv, BRCM_MSPI_MSPI_STATUS, status & ~1);
		complete(&priv->done);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/* requires priv->max_speed_hz to be initialized */
static void bcmspi_hw_init(struct bcmspi_priv *priv)
{
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR1_LSB, 0);
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR1_MSB, 0);
	brcm_mspi_writereg(priv, BRCM_MSPI_NEWQP, 0);
	brcm_mspi_writereg(priv, BRCM_MSPI_ENDQP, 0);
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR2, 0x20);  /* spifie */
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR3, 0);

	bcmspi_default_parms_cs0.speed_hz = priv->max_speed_hz;
	bcmspi_hw_set_parms(priv, &bcmspi_default_parms_cs0);
}

static void bcmspi_hw_uninit(struct bcmspi_priv *priv)
{
	/* disable irq and enable bits */
	brcm_mspi_writereg(priv, BRCM_MSPI_SPCR2, 0);
}

static int bcmspi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bcmspi_priv *priv;
	struct spi_master *master;
	struct resource *res;
	int ret;
	u32 temp;
	int irq;

	if (!dev->of_node)
		return -ENODEV;

	master = spi_alloc_master(dev, sizeof(struct bcmspi_priv));
	if (!master) {
		dev_err(&pdev->dev, "error allocating spi_master\n");
		return -ENOMEM;
	}

	priv = spi_master_get_devdata(master);

	priv->pdev = pdev;
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
	master->transfer_one = bcmspi_transfer_one;
	master->cleanup = bcmspi_cleanup;
	master->dev.of_node = dev->of_node;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't get resource 0\n");
		ret = -EIO;
		goto err2;
	}
	/* MSPI register range */
	priv->mspi_base = devm_ioremap(&pdev->dev, res->start,
			res->end - res->start);
	if (!priv->mspi_base) {
		dev_err(&pdev->dev, "can't ioremap\n");
		ret = -EIO;
		goto err2;
	}

	/* Optional: BSPI */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "bspi");
	if (res) {
		priv->bspi = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(priv->bspi)) {
			ret = PTR_ERR(priv->bspi);
			goto err2;
		}
		dev_info(&pdev->dev, "has BSPI\n");
	}

	/* Start in MSPI mode */
	if (has_bspi(priv))
		__raw_writel(1, priv->bspi + BSPI_MAST_N_BOOT_CTRL);

	/* IRQ */
	irq = platform_get_irq_byname(pdev, "mspi_done");
	if (irq < 0) {
		dev_err(&pdev->dev, "no mspi_done IRQ defined\n");
		ret = -ENODEV;
		goto err2;
	}

	ret = devm_request_irq(&pdev->dev, irq, bcmspi_interrupt, 0,
			"spi_brcmstb", priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to allocate IRQ\n");
		goto err2;
	}

	/* Clock */
	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_warn(&pdev->dev, "unable to get clock\n");
		/* Ignore, if we don't have any clocks to get */
		priv->clk = NULL;
		priv->base_freq = MSPI_BASE_FREQ;
	} else {
		ret = clk_prepare_enable(priv->clk);
		if (ret) {
			dev_err(&pdev->dev, "failed to prepare clock\n");
			goto err2;
		}
		priv->base_freq = clk_get_rate(priv->clk);
	}
	priv->max_speed_hz = priv->base_freq / (SPBR_MIN * 2);

#ifdef CONFIG_BRCM_MSPI_LEGACY
	/*
	 * Legacy MSPI does not have a revision register, so we need this
	 * ifdef---there's no way to tell what version you have at runtime.
	 */
	priv->reg_offsets = brcm_mspi_regs_legacy;
	priv->revision = 0;
#else
	/* Ideally we should read the revision first and determine what
	 * registers are present, but right now at least there's only one
	 * supported revision */
	priv->reg_offsets = brcm_mspi_regs_v1_5;
	priv->revision = brcm_mspi_readreg(priv, BRCM_MSPI_REVISION);
#endif

	bcmspi_hw_init(priv);
	init_completion(&priv->done);
	platform_set_drvdata(pdev, priv);

	ret = devm_spi_register_master(&pdev->dev, master);
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

	bcmspi_hw_uninit(priv);
	clk_disable_unprepare(priv->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bcmspi_suspend(struct device *dev)
{
	struct bcmspi_priv *priv = dev_get_drvdata(dev);

	clk_disable(priv->clk);
	return 0;
};

static int bcmspi_resume(struct device *dev)
{
	struct bcmspi_priv *priv = dev_get_drvdata(dev);

	bcmspi_hw_init(priv);

	return clk_enable(priv->clk);
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(bcmspi_pm_ops, bcmspi_suspend, bcmspi_resume);

static const struct of_device_id spi_brcmstb_of_match[] = {
#ifndef CONFIG_SPI_BRCMSTB
	/*
	 * With this ifdef, this driver (which does not support BSPI) will not
	 * be used for MSPI+BSPI configurations unless the driver that supports
	 * both is disabled.
	 */
	{ .compatible = "brcm,spi-brcmstb" },
#endif
	{ .compatible = "brcm,spi-brcmstb-mspi" },
	{},
};

static struct platform_driver spi_brcmstb_driver = {
	.driver = {
		.name = "spi-brcmstb-mspi",
		.bus = &platform_bus_type,
		.pm		= &bcmspi_pm_ops,
		.of_match_table = spi_brcmstb_of_match,
	},
	.probe = bcmspi_probe,
	.remove = bcmspi_remove,
};

module_platform_driver(spi_brcmstb_driver);

MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Broadcom MSPI SPI driver");
MODULE_LICENSE("GPL");
