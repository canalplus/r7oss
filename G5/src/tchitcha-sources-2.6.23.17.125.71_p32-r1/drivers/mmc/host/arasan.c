/*
 * Arasan MMC/SD/SDIO driver
 *
 *  This is the driver for the Arasan MMC/SD/SDIO host controller
 *  integrated in the STMicroelectronics platforms
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro at st.com>
 * Copyright (C) 2010 STMicroelectronics Ltd
 *
 * Notes: only PIO and SDMA modes are supported. ADMA never tested.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/irq.h>
#include <linux/mmc/host.h>
#include <linux/highmem.h>

#include <asm/unaligned.h>

#include "arasan.h"

/* To enable more debug information. */
#undef ARASAN_DEBUG
/*#define ARASAN_DEBUG*/
#ifdef ARASAN_DEBUG
#define DBG(fmt, args...)  pr_info(fmt, ## args)
#else
#define DBG(fmt, args...)  do { } while (0)
#endif

static int maxfreq = ARASAN_CLOCKRATE_MAX;
module_param(maxfreq, int, S_IRUGO);
MODULE_PARM_DESC(maxfreq, "Maximum card clock frequency (default 50MHz)");

static unsigned int led;
module_param(led, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(led, "Enable|Disable LED");

static unsigned int pio;
module_param(pio, int, S_IRUGO);
MODULE_PARM_DESC(pio, "PIO mode (no DMA)");

struct arasan_cap {
	unsigned int timer_freq;
	unsigned int timer_unit;
	unsigned int base_clk_sd;
	unsigned int max_blk_len;
	unsigned int adma2;
	unsigned int high_speed;
	unsigned int sdma;
	unsigned int suspend;
	unsigned int voltage33;
	unsigned int voltage30;
	unsigned int voltage18;
	unsigned int int_mode;
	unsigned int spi;
	unsigned int spi_block;
};

struct arasan_host {
	void __iomem *base;
	struct mmc_request *mrq;
	unsigned int intr_en;
	u8 ctrl;
	unsigned int sg_frags;
	struct timer_list timer;
	struct mmc_host *mmc;
	struct device *dev;
	struct resource *res;
	int irq;
	struct arasan_cap cap;
	u8 vdd;
	unsigned int freq;
	unsigned int status;
	unsigned int use_pio;
	u16 pio_blksz;
	u32 pio_blocks;
	u32 *pio_blkbuf;
	spinlock_t lock; /* mmc lock */
	struct tasklet_struct card_tasklet;
};

static inline void arsan_sw_reset(struct arasan_host *host, unsigned int flag)
{
	/* After completing the reset, wait the HC clears these bits */
	if (likely(flag == reset_all)) {
		writeb(ARSAN_RESET_ALL, host->base + ARASAN_SW_RESET);
		do { } while ((readb(host->base + ARASAN_SW_RESET)) &
			 ARSAN_RESET_ALL);
	} else if (flag == reset_cmd_line) {
		writeb(ARSAN_RESET_CMD_LINE, host->base + ARASAN_SW_RESET);
		do { } while ((readb(host->base + ARASAN_SW_RESET)) &
			 ARSAN_RESET_CMD_LINE);

	} else if (flag == reset_dat_line) {
		writeb(ARSAN_RESET_DAT_LINE, host->base + ARASAN_SW_RESET);
		do { } while ((readb(host->base + ARASAN_SW_RESET)) &
			 ARSAN_RESET_DAT_LINE);
	}
}

static inline void arsan_hc_version(struct arasan_host *host)
{
	u16 version = readw(host->base + ARASAN_HOST_VERSION);

	pr_debug("Arasan MMC/SDIO:\n\tHC Vendor Version Number: %d\n",
		 (version >> 8));
	pr_debug("\tHC SPEC Version Number: %d\n", (version & 0x00ff));
}

static void arasan_capabilities(struct arasan_host *host)
{
	unsigned int cap = readl(host->base + ARASAN_CAPABILITIES);
	unsigned int max_blk_len;

	pr_debug("\tArasan capabilities: 0x%x\n", cap);

	host->cap.timer_freq = cap & 0x3f;
	host->cap.timer_unit = (cap >> 7) & 0x1;

	pr_debug("\tTimeout Clock Freq: %d %s\n", host->cap.timer_freq,
		 host->cap.timer_unit ? "MHz" : "KHz");

	host->cap.base_clk_sd = (cap >> 8) & 0x3f;
	pr_debug("\tBase Clock Freq for SD: %d MHz\n", host->cap.base_clk_sd);

	max_blk_len = (cap >> 16) & 0x3;
	switch (max_blk_len) {
	case 0:
		host->cap.max_blk_len = 512;
		break;
	case 1:
		host->cap.max_blk_len = 1024;
		break;
	case 2:
		host->cap.max_blk_len = 2048;
		break;
	case 3:
		host->cap.max_blk_len = 4096;
		break;
	default:
		break;
	}
	pr_debug("\tMax Block size: %d bytes\n", host->cap.max_blk_len);

	host->cap.adma2 = (cap >> 19) & 0x1;
	host->cap.high_speed = (cap >> 21) & 0x1;
	host->cap.sdma = (cap >> 22) & 0x1;

	pr_debug("\tadma2 %s, high speed %s, sdma %s\n",
		 host->cap.adma2 ? "Yes" : "Not",
		 host->cap.high_speed ? "Yes" : "Not",
		 host->cap.sdma ? "Yes" : "Not");

	host->cap.suspend = (cap >> 23) & 0x1;
	pr_debug("\tsuspend/resume %s suported\n",
		 host->cap.adma2 ? "is" : "Not");

	host->cap.voltage33 = (cap >> 24) & 0x1;
	host->cap.voltage30 = (cap >> 25) & 0x1;
	host->cap.voltage18 = (cap >> 26) & 0x1;
	host->cap.int_mode = (cap >> 27) & 0x1;
	host->cap.spi = (cap >> 29) & 0x1;
	host->cap.spi_block = (cap >> 30) & 0x1;

	if (host->cap.voltage33)
		pr_debug("\t3.3V voltage suported\n");
	if (host->cap.voltage30)
		pr_debug("\t3.0V voltage suported\n");
	if (host->cap.voltage18)
		pr_debug("\t1.8V voltage suported\n");

	if (host->cap.int_mode)
		pr_debug("\tInterrupt Mode supported\n");
	if (host->cap.spi)
		pr_debug("\tSPI Mode supported\n");
	if (host->cap.spi_block)
		pr_debug("\tSPI Block Mode supported\n");
}

static void arasan_ctrl_led(struct arasan_host *host, unsigned int flag)
{
	if (led) {
		u8 ctrl_reg = readb(host->base + ARASAN_HOST_CTRL);

		if (flag)
			ctrl_reg |= ARASAN_HOST_CTRL_LED;
		else
			ctrl_reg &= ~ARASAN_HOST_CTRL_LED;

		host->ctrl = ctrl_reg;
		writeb(host->ctrl, host->base + ARASAN_HOST_CTRL);
	}
}

static inline void arasan_set_interrupts(struct arasan_host *host)
{
	host->intr_en = ARASAN_IRQ_DEFAULT_MASK;
	writel(host->intr_en, host->base + ARASAN_NORMAL_INT_STATUS_EN);
	writel(host->intr_en, host->base + ARASAN_NORMAL_INT_SIGN_EN);
}

static inline void arasan_clear_interrupts(struct arasan_host *host)
{
	writel(0, host->base + ARASAN_NORMAL_INT_STATUS_EN);
	writel(0, host->base + ARASAN_ERR_INT_STATUS_EN);
	writel(0, host->base + ARASAN_NORMAL_INT_SIGN_EN);
}

static void arasan_power_set(struct arasan_host *host, unsigned int pwr, u8 vdd)
{
	u8 pwr_reg;

	pwr_reg = readb(host->base + ARASAN_PWR_CTRL);

	host->vdd = (1 << vdd);

	if (pwr) {
		pwr_reg &= 0xf1;

		if ((host->vdd & MMC_VDD_165_195) && host->cap.voltage18)
			pwr_reg |= ARASAN_PWR_BUS_VOLTAGE_18;
		else if ((host->vdd & MMC_VDD_29_30) && host->cap.voltage30)
			pwr_reg |= ARASAN_PWR_BUS_VOLTAGE_30;
		else if ((host->vdd & MMC_VDD_32_33) && host->cap.voltage33)
			pwr_reg |= ARASAN_PWR_BUS_VOLTAGE_33;

		pwr_reg |= ARASAN_PWR_CTRL_UP;
	} else
		pwr_reg &= ~ARASAN_PWR_CTRL_UP;

	DBG("%s: pwr_reg 0x%x, host->vdd = 0x%x\n", __func__, pwr_reg,
	    host->vdd);
	writeb(pwr_reg, host->base + ARASAN_PWR_CTRL);
}

static int arasan_test_card(struct arasan_host *host)
{
	unsigned int ret = 0;
	u32 present = readl(host->base + ARASAN_PRESENT_STATE);
	if (likely(!(present & ARASAN_PRESENT_STATE_CARD_PRESENT)))
		ret = -1;

#ifdef ARASAN_DEBUG
	if (present & ARASAN_PRESENT_STATE_CARD_STABLE)
		pr_info("\tcard stable...");
	if (!(present & ARASAN_PRESENT_STATE_WR_EN))
		pr_info("\tcard Write protected...");
	if (present & ARASAN_PRESENT_STATE_BUFFER_RD_EN)
		pr_info("\tPIO Read Enable...");
	if (present & ARASAN_PRESENT_STATE_BUFFER_WR_EN)
		pr_info("\tPIO Write Enable...");
	if (present & ARASAN_PRESENT_STATE_RD_ACTIVE)
		pr_info("\tRead Xfer data...");
	if (present & ARASAN_PRESENT_STATE_WR_ACTIVE)
		pr_info("\tWrite Xfer data...");
	if (present & ARASAN_PRESENT_STATE_DAT_ACTIVE)
		pr_info("\tDAT line active...");
#endif

	return ret;
}
static void arasan_set_clock(struct arasan_host *host, unsigned int freq)
{
	u16 clock = 0;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	if ((host->freq != freq) && (freq)) {
		u16 divisor;

		/* Ensure clock is off before making any changes */
		writew(clock, host->base + ARASAN_CLOCK_CTRL);

		/* core checks if this is a good freq < max_freq */
		host->freq = freq;

		DBG("%s:\n\tnew freq %d", __func__, host->freq);

		/* Work out divisor for specified clock frequency */
		for (divisor = 1; divisor <= 256; divisor *= 2)
			/* Find first divisor producing a frequency less
			 * than or equal to MHz */
			if ((maxfreq / divisor) <= freq)
				break;

		DBG("\tdivisor %d", divisor);
		/* Set the clock divisor and enable the internal clock */
		clock = divisor << (ARASAN_CLOCK_CTRL_SDCLK_SHIFT);
		clock &= ARASAN_CLOCK_CTRL_SDCLK_MASK;
		clock |= ARASAN_CLOCK_CTRL_ICLK_ENABLE;
		writew(clock, host->base + ARASAN_CLOCK_CTRL);

		/* Busy wait for the clock to become stable */
		do { } while (((readw(host->base + ARASAN_CLOCK_CTRL)) &
			  ARASAN_CLOCK_CTRL_ICLK_STABLE) == 0);

		/* Enable the SD clock */
		clock |= ARASAN_CLOCK_CTRL_SDCLK_ENABLE;
		writew(clock, host->base + ARASAN_CLOCK_CTRL);

		DBG("\tclk ctrl reg. [0x%x]\n",
		    (unsigned int)readw(host->base + ARASAN_CLOCK_CTRL));
	}

	spin_unlock_irqrestore(&host->lock, flags);
}

/* Read the response from the card */
static void arasan_get_resp(struct mmc_command *cmd, struct arasan_host *host)
{
	unsigned int i;
	unsigned int resp[4];

	for (i = 0; i < 4; i++)
		resp[i] = readl(host->base + ARASAN_RSP(i));

	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[3] = (resp[0] << 8);
		cmd->resp[2] = (resp[0] >> 24) | (resp[1] << 8);
		cmd->resp[1] = (resp[1] >> 24) | (resp[2] << 8);
		cmd->resp[0] = (resp[2] >> 24) | (resp[3] << 8);
	} else {
		cmd->resp[0] = resp[0];
		cmd->resp[1] = resp[1];
	}

	DBG("%s: resp length %s\n-(CMD%u):\n %08x %08x %08x %08x\n"
	    "-RAW reg:\n %08x %08x %08x %08x\n",
	    __func__, (cmd->flags & MMC_RSP_136) ? "136" : "48", cmd->opcode,
	    cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3],
	    resp[0], resp[1], resp[2], resp[3]);
}

static void arasan_read_block_pio(struct arasan_host *host)
{
	unsigned long flags;
	u16 blksz;

	DBG("\tPIO reading\n");

	local_irq_save(flags);

	for (blksz = host->pio_blksz; blksz > 0; blksz -= 4) {
		*host->pio_blkbuf =
		    readl(host->base + ARASAN_BUFF);
		host->pio_blkbuf++;
	}

	local_irq_restore(flags);
}

static void arasan_write_block_pio(struct arasan_host *host)
{
	unsigned long flags;
	u16 blksz;

	DBG("\tPIO writing\n");
	local_irq_save(flags);

	for (blksz = host->pio_blksz; blksz > 0; blksz -= 4) {
		writel(*host->pio_blkbuf,
		       host->base + ARASAN_BUFF);
		host->pio_blkbuf++;
	}

	local_irq_restore(flags);
}

static void arasan_data_pio(struct arasan_host *host)
{
	if (host->pio_blocks == 0)
		return;

	if (host->status == STATE_DATA_READ) {
		while (readl(host->base + ARASAN_PRESENT_STATE) &
			     ARASAN_PRESENT_STATE_BUFFER_RD_EN) {

			arasan_read_block_pio(host);

			host->pio_blocks--;
			if (host->pio_blocks == 0)
				break;
		}

	} else {
		while (readl(host->base + ARASAN_PRESENT_STATE) &
			     ARASAN_PRESENT_STATE_BUFFER_WR_EN) {

			arasan_write_block_pio(host);

			host->pio_blocks--;
			if (host->pio_blocks == 0)
				break;
		}
	}

	DBG("\tPIO transfer complete.\n");
}

static void arasan_start_cmd(struct arasan_host *host, struct mmc_command *cmd)
{
	u16 cmdreg = 0;

	/* Command Request */
	cmdreg = ARASAN_CMD_INDEX(cmd->opcode);
	DBG("%s: cmd type %04x,  CMD%d\n", __func__,
	    mmc_resp_type(cmd), cmd->opcode);

	if (cmd->flags & MMC_RSP_BUSY) {
		cmdreg |= ARASAN_CMD_RSP_48BUSY;
		DBG("\tResponse length 48 check Busy.\n");
	} else if (cmd->flags & MMC_RSP_136) {
		cmdreg |= ARASAN_CMD_RSP_136;
		DBG("\tResponse length 136\n");
	} else if (cmd->flags & MMC_RSP_PRESENT) {
		cmdreg |= ARASAN_CMD_RSP_48;
		DBG("\tResponse length 48\n");
	} else {
		cmdreg |= ARASAN_CMD_RSP_NONE;
		DBG("\tNo Response\n");
	}

	if (cmd->flags & MMC_RSP_CRC) {
		cmdreg |= ARASAN_CMD_CHECK_CMDCRC;
		DBG("\tCheck the CRC field in the response\n");
	}
	if (cmd->flags & MMC_RSP_OPCODE) {
		cmdreg |= ARASAN_CMD_INDX_CHECK;
		DBG("\tCheck the Index field in the response\n");
	}

	/* Wait until the CMD line is not in use */
	do { } while ((readl(host->base + ARASAN_PRESENT_STATE)) &
		 ARASAN_PRESENT_STATE_CMD_INHIBIT);

	/* Set the argument register */
	writel(cmd->arg, host->base + ARASAN_ARG);

	/* Data present and must be transferred */
	if (likely(host->mrq->data)) {
		cmdreg |= ARASAN_CMD_DATA_PRESENT;
		if (cmd->flags & MMC_RSP_BUSY)
			/* Wait for data inhibit */
			do { } while ((readl(host->base +
					ARASAN_PRESENT_STATE)) &
				 ARASAN_PRESENT_STATE_DAT_INHIBIT);
	}

	/* Write the Command */
	writew(cmdreg, host->base + ARASAN_CMD);

	DBG("\tcmd: 0x%x cmd reg: 0x%x - cmd->arg 0x%x, reg 0x%x\n",
	    cmdreg, readw(host->base + ARASAN_CMD), cmd->arg,
	    readl(host->base + ARASAN_ARG));
}

static int arasan_setup_data(struct arasan_host *host)
{
	u16 blksz;
	u16 xfer = 0;
	struct mmc_data *data = host->mrq->data;

	DBG("%s:\n\t%s mode, data dir: %s; Buff=0x%08x,"
	    "blocks=%d, blksz=%d\n", __func__, host->use_pio ? "PIO" : "DMA",
	    (data->flags & MMC_DATA_READ) ? "read" : "write",
	    (u32 *)(page_address(data->sg->page) + data->sg->offset),
	    data->blocks, data->blksz);

	/* Transfer Direction */
	if (data->flags & MMC_DATA_READ) {
		xfer |= ARASAN_XFER_DATA_DIR;
		host->status = STATE_DATA_READ;
	} else {
		xfer &= ~ARASAN_XFER_DATA_DIR;
		host->status = STATE_DATA_WRITE;
	}

	xfer |= ARASAN_XFER_BLK_COUNT_EN;

	if (data->blocks > 1)
		xfer |= ARASAN_XFER_MULTI_BLK | ARASAN_XFER_AUTOCMD12;

	/* Set the block size register */
	blksz = ARASAN_BLOCK_SIZE_SDMA_512KB;
	blksz |= (data->blksz & ARASAN_BLOCK_SIZE_TRANSFER);
	blksz |= (data->blksz & 0x1000) ? ARASAN_BLOCK_SIZE_SDMA_8KB : 0;

	writew(blksz, host->base + ARASAN_BLK_SIZE);

	/* Set the block count register */
	writew(data->blocks, host->base + ARASAN_BLK_COUNT);

	/* PIO mode is used when 'pio' var is set by the user or no
	 * sdma is available from HC caps. */
	if (unlikely(host->use_pio || (host->cap.sdma == 0))) {
		host->pio_blksz = data->blksz;
		host->pio_blocks = data->blocks;
		host->pio_blkbuf = (u32 *)(page_address(data->sg->page) +
					  data->sg->offset);
	} else {
		dma_addr_t phys_addr;

		/* Enable DMA */
		xfer |= ARASAN_XFER_DMA_EN;

		/* Scatter list init */
		host->sg_frags = dma_map_sg(mmc_dev(host->mmc), data->sg,
					    data->sg_len,
					    (host->status & STATE_DATA_READ) ?
					    DMA_FROM_DEVICE : DMA_TO_DEVICE);

		phys_addr = sg_dma_address(data->sg);

		/* SDMA Mode selected (default mode) */
		host->ctrl &= ~ARASAN_HOST_CTRL_ADMA2_64;

		writel((unsigned int)phys_addr,
		       host->base + ARASAN_SDMA_SYS_ADDR);
		writeb(host->ctrl, host->base + ARASAN_HOST_CTRL);

	}
	/* Set the data transfer mode register */
	writew(xfer, host->base + ARASAN_XFER_MODE);

	DBG("\tHC Reg [xfer 0x%x] [blksz 0x%x] [blkcount 0x%x] [CRTL 0x%x]\n",
	    readw(host->base + ARASAN_XFER_MODE),
	    readw(host->base + ARASAN_BLK_SIZE),
	    readw(host->base + ARASAN_BLK_COUNT),
	    readb(host->base + ARASAN_HOST_CTRL));

	return 0;
}

static void arasan_finish_data(struct arasan_host *host)
{
	struct mmc_data *data = host->mrq->data;

	DBG("\t%s\n", __func__);

	if (unlikely(host->pio_blkbuf)) {
		host->pio_blksz = 0;
		host->pio_blocks = 0;
		host->pio_blkbuf = NULL;
	} else {
		dma_unmap_sg(mmc_dev(host->mmc), data->sg,
			     host->sg_frags,
			     (host->status & STATE_DATA_READ) ?
			     DMA_FROM_DEVICE : DMA_TO_DEVICE);
	}

	data->bytes_xfered = data->blocks * data->blksz;
	host->status = STATE_CMD;
}

static int arasan_finish_cmd(unsigned int err_status, unsigned int status,
			     unsigned int opcode)
{
	int ret = 0;

	if (unlikely(err_status)) {
		if (err_status & ARASAN_CMD_TIMEOUT) {
			DBG("\tcmd_timeout...\n");
			ret = -ETIMEDOUT;
		}
		if (err_status & ARASAN_CMD_CRC_ERROR) {
			DBG("\tcmd_crc_error...\n");
			ret = -EILSEQ;
		}
		if (err_status & ARASAN_CMD_END_BIT_ERROR) {
			DBG("\tcmd_end_bit_errOr...\n");
			ret = -EILSEQ;
		}
		if (err_status & ARASAN_CMD_INDEX_ERROR) {
			DBG("\tcmd_index_error...\n");
			ret = -EILSEQ;
		}
	}
	if (likely(status & ARASAN_N_CMD_COMPLETE))
		DBG("\tCommand (CMD%u) Completed irq...\n", opcode);

	return ret;
}

/* Enable/Disable Normal and Error interrupts */
static void aranan_enable_disable_irq(struct mmc_host *mmc, int enable)
{
	unsigned long flags;
	struct arasan_host *host = mmc_priv(mmc);

	DBG("%s: %s CARD_IRQ\n", __func__, enable ? "enable" : "disable");

	spin_lock_irqsave(&host->lock, flags);
	if (enable)
		host->intr_en = ARASAN_IRQ_DEFAULT_MASK;
	else
		host->intr_en = 0;

	writel(host->intr_en, host->base + ARASAN_NORMAL_INT_STATUS_EN);
	spin_unlock_irqrestore(&host->lock, flags);
}

static void arasan_timeout_timer(unsigned long data)
{
	struct arasan_host *host = (struct arasan_host *)data;
	struct mmc_request *mrq;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);

	if ((host->mrq) && (host->status == STATE_CMD)) {
		mrq = host->mrq;

		pr_debug("%s: Timeout waiting for hardware interrupt.\n",
			 mmc_hostname(host->mmc));

		writel(0xffffffff, host->base + ARASAN_NORMAL_INT_STATUS);
		aranan_enable_disable_irq(host->mmc, 1);

		if (mrq->data) {
			arasan_finish_data(host);
			arsan_sw_reset(host, reset_dat_line);
			mrq->data->error = -ETIMEDOUT;
		}
		if (likely(mrq->cmd)) {
			mrq->cmd->error = -ETIMEDOUT;
			arsan_sw_reset(host, reset_cmd_line);
			arasan_get_resp(mrq->cmd, host);
		}
		arasan_ctrl_led(host, 0);
		host->mrq = NULL;
		mmc_request_done(host->mmc, mrq);
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

/* Process requests from the MMC layer */
static void arasan_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct arasan_host *host = mmc_priv(mmc);
	struct mmc_command *cmd = mrq->cmd;
	unsigned long flags;

	BUG_ON(host->mrq != NULL);

	spin_lock_irqsave(&host->lock, flags);

	DBG(">>> araran_request:\n");
	/* Check that there is a card in the slot */
	if (unlikely(arasan_test_card(host) < 0)) {
		DBG("%s: Error: No card present...\n", mmc_hostname(host->mmc));

		mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}

	host->mrq = mrq;

	host->status = STATE_CMD;
	if (likely(mrq->data))
		arasan_setup_data(host);

	/* Turn-on/off the LED when send/complete a cmd */
	arasan_ctrl_led(host, 1);

	arasan_start_cmd(host, cmd);

	mod_timer(&host->timer, jiffies + 5 * HZ);

	DBG("<<< araran_request done!\n");
	spin_unlock_irqrestore(&host->lock, flags);
}

static int arasan_get_ro(struct mmc_host *mmc)
{
	struct arasan_host *host = mmc_priv(mmc);

	u32 ro = readl(host->base + ARASAN_PRESENT_STATE);
	if (!(ro & ARASAN_PRESENT_STATE_WR_EN))
		return 1;

	return 0;
}

/* I/O bus settings (MMC clock/power ...) */
static void arasan_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct arasan_host *host = mmc_priv(mmc);
	u8 ctrl_reg = readb(host->base + ARASAN_HOST_CTRL);

	DBG("%s: pwr %d, clk %d, vdd %d, bus_width %d, timing %d\n",
	    __func__, ios->power_mode, ios->clock, ios->vdd, ios->bus_width,
	    ios->timing);

	/* Set the power supply mode */
	if (ios->power_mode == MMC_POWER_OFF)
		arasan_power_set(host, 0, ios->vdd);
	else
		arasan_power_set(host, 1, ios->vdd);

	/* Timing (high speed supported?) */
	if ((ios->timing == MMC_TIMING_MMC_HS ||
	     ios->timing == MMC_TIMING_SD_HS) && host->cap.high_speed)
		ctrl_reg |= ARASAN_HOST_CTRL_HIGH_SPEED;

	/* Clear the current bus width configuration */
	ctrl_reg &= ~ARASAN_HOST_CTRL_SD_MASK;

	/* Set SD bus bit mode */
	switch (ios->bus_width) {
	case MMC_BUS_WIDTH_4:
		ctrl_reg |= ARASAN_HOST_CTRL_SD;
		break;
	}

	/* Default to maximum timeout */
	writeb(0x0e, host->base + ARASAN_TIMEOUT_CTRL);

	/* Disable Card Interrupt in Host in case we change
	 * the Bus Width. */
	aranan_enable_disable_irq(host->mmc, 0);

	host->ctrl = ctrl_reg;
	writeb(host->ctrl, host->base + ARASAN_HOST_CTRL);

	aranan_enable_disable_irq(host->mmc, 1);

	/* Set clock */
	arasan_set_clock(host, ios->clock);
}

/* Tasklet for Card-detection */
static void arasan_tasklet_card(unsigned long data)
{
	unsigned long flags;
	struct arasan_host *host = (struct arasan_host *)data;

	spin_lock_irqsave(&host->lock, flags);

	if (likely((readl(host->base + ARASAN_PRESENT_STATE) &
		    ARASAN_PRESENT_STATE_CARD_PRESENT))) {
		if (host->mrq) {
			pr_err("%s: Card removed during transfer!\n",
			       mmc_hostname(host->mmc));
			/* Reset cmd and dat lines */
			arsan_sw_reset(host, reset_cmd_line);
			arsan_sw_reset(host, reset_dat_line);

			if (likely(host->mrq->cmd)) {
				host->mrq->cmd->error = -ENOMEDIUM;
				mmc_request_done(host->mmc, host->mrq);
			}
		}
	}

	spin_unlock_irqrestore(&host->lock, flags);

	if (likely(host->mmc))
		mmc_detect_change(host->mmc, msecs_to_jiffies(200));
}

static irqreturn_t arasan_irq(int irq, void *dev)
{
	struct arasan_host *host = dev;
	unsigned int status, err_status, handled = 0;
	struct mmc_command *cmd = NULL;
	struct mmc_data *data = NULL;

	spin_lock(&host->lock);

	/* Interrupt Status */
	status = readl(host->base + ARASAN_NORMAL_INT_STATUS);
	err_status = (status >> 16) & 0xffff;

	DBG("%s: Normal IRQ status  0x%x, Error status 0x%x\n",
	    __func__, status & 0xffff, err_status);

	if ((status & ARASAN_N_CARD_REMOVAL) ||
		    (status & ARASAN_N_CARD_INS))
			tasklet_schedule(&host->card_tasklet);

	if (unlikely(!host->mrq))
		goto out;

	cmd = host->mrq->cmd;
	data = host->mrq->data;

	cmd->error = 0;
	/* Check for any CMD interrupts */
	if (likely(status & ARASAN_INT_CMD_MASK)) {

		cmd->error = arasan_finish_cmd(err_status, status, cmd->opcode);
		if (cmd->error)
			arsan_sw_reset(host, reset_cmd_line);

		if ((host->status == STATE_CMD) || cmd->error) {
			arasan_get_resp(cmd, host);

			handled = 1;
		}
	}

	/* Check for any data interrupts */
	if (likely((status & ARASAN_INT_DATA_MASK)) && data) {
		data->error = 0;
		if (unlikely(err_status)) {
			if (err_status & ARASAN_DATA_TIMEOUT_ERROR) {
				DBG("\tdata_timeout_error...\n");
				data->error = -ETIMEDOUT;
			}
			if (err_status & ARASAN_DATA_CRC_ERROR) {
				DBG("\tdata_crc_error...\n");
				data->error = -EILSEQ;
			}
			if (err_status & ARASAN_DATA_END_ERROR) {
				DBG("\tdata_end_error...\n");
				data->error = -EILSEQ;
			}
			if (err_status & ARASAN_AUTO_CMD12_ERROR) {
				unsigned int err_cmd12 =
				    readw(host->base + ARASAN_CMD12_ERR_STATUS);

				DBG("\tc12err 0x%04x\n", err_cmd12);

				if (err_cmd12 & ARASAN_AUTOCMD12_ERR_NOTEXE)
					data->stop->error = -ENOEXEC;

				if ((err_cmd12 & ARASAN_AUTOCMD12_ERR_TIMEOUT)
				    && !(err_cmd12 & ARASAN_AUTOCMD12_ERR_CRC))
					/* Timeout Error */
					data->stop->error = -ETIMEDOUT;
				else if (!(err_cmd12 &
					   ARASAN_AUTOCMD12_ERR_TIMEOUT)
					 && (err_cmd12 &
					     ARASAN_AUTOCMD12_ERR_CRC))
					/* CRC Error */
					data->stop->error = -EILSEQ;
				else if ((err_cmd12 &
					  ARASAN_AUTOCMD12_ERR_TIMEOUT)
					 && (err_cmd12 &
					     ARASAN_AUTOCMD12_ERR_CRC))
					DBG("\tCMD line Conflict\n");
			}
			arsan_sw_reset(host, reset_dat_line);
			handled = 1;
		} else {
			if (likely(((status & ARASAN_N_BUFF_READ) ||
				    status & ARASAN_N_BUFF_WRITE))) {
				DBG("\tData R/W interrupts...\n");
				arasan_data_pio(host);
			}

			if (likely(status & ARASAN_N_DMA_IRQ))
				DBG("\tDMA interrupts...\n");

			if (likely(status & ARASAN_N_TRANS_COMPLETE)) {
				DBG("\tData XFER completed interrupts...\n");
				arasan_finish_data(host);
				if (data->stop) {
					u32 opcode = data->stop->opcode;
					data->stop->error =
					    arasan_finish_cmd(err_status,
							      status, opcode);
					arasan_get_resp(data->stop, host);
				}
				handled = 1;
			}
		}
	}
	if (err_status & ARASAN_CURRENT_LIMIT_ERROR) {
		DBG("\tPower Fail...\n");
		cmd->error = -EIO;
	}

	if (likely(host->mrq && handled)) {
		struct mmc_request *mrq = host->mrq;

		arasan_ctrl_led(host, 0);

		del_timer(&host->timer);

		host->mrq = NULL;
		DBG("\tcalling mmc_request_done...\n");
		mmc_request_done(host->mmc, mrq);
	}
out:
	DBG("\tclear status and exit...\n");
	writel(status, host->base + ARASAN_NORMAL_INT_STATUS);

	spin_unlock(&host->lock);

	return IRQ_HANDLED;
}

static void arasan_setup_hc(struct arasan_host *host)
{
	/* Clear all the interrupts before resetting */
	arasan_clear_interrupts(host);

	/* Reset All and get the HC version */
	arsan_sw_reset(host, reset_all);

	/* Print HC version and SPEC */
	arsan_hc_version(host);

	/* Set capabilities and print theri info */
	arasan_capabilities(host);

	/* Enable interrupts */
	arasan_set_interrupts(host);
}

static const struct mmc_host_ops arasan_ops = {
	.request = arasan_request,
	.get_ro = arasan_get_ro,
	.set_ios = arasan_set_ios,
};

static int __init arasan_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc = NULL;
	struct arasan_host *host = NULL;
	struct resource *r;
	int ret, irq;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	irq = platform_get_irq_byname(pdev, "mmcirq");

	if (!r || irq < 0)
		return -ENXIO;

	r = request_mem_region(r->start, (r->end - r->start + 1), pdev->name);
	if (!r) {
		pr_err("%s: ERROR: memory allocation failed\n", __func__);
		return -EBUSY;
		goto out;
	}
	/* Allocate the mmc_host with private data size */
	mmc = mmc_alloc_host(sizeof(struct arasan_host), &pdev->dev);
	if (!mmc) {
		pr_err("%s: ERROR: mmc_alloc_host failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->dev = &pdev->dev;
	host->res = r;

	tasklet_init(&host->card_tasklet, arasan_tasklet_card,
		     (unsigned long)host);

	host->base = ioremap(r->start, (r->end - r->start + 1));
	if (!host->base) {
		pr_err("%s: ERROR: memory mapping failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	ret =
	    request_irq(irq, arasan_irq, IRQF_SHARED, ARASAN_DRIVER_NAME, host);
	if (ret) {
		pr_err("%s: cannot assign irq %d\n", __func__, irq);
		goto out;
	} else
		host->irq = irq;

	spin_lock_init(&host->lock);

	/* Setup the Host Controller according to its capabilities */
	arasan_setup_hc(host);

	mmc->ops = &arasan_ops;

	if (host->cap.voltage33)
		mmc->ocr_avail |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (host->cap.voltage30)
		mmc->ocr_avail |= MMC_VDD_29_30;
	if (host->cap.voltage18)
		mmc->ocr_avail |= MMC_VDD_165_195;

	mmc->caps = MMC_CAP_4_BIT_DATA;

	if (host->cap.high_speed)
		mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;

	host->freq = host->cap.timer_freq * 1000000;
	host->use_pio = pio;
	mmc->f_max = maxfreq;
	mmc->f_min = mmc->f_max / 256;

	/*
	 * Maximum block size. This is specified in the capabilities register.
	 */
	mmc->max_blk_size = host->cap.max_blk_len;
	mmc->max_blk_count = 65535;

	mmc->max_hw_segs = 1;
	mmc->max_phys_segs = 1;
	mmc->max_seg_size = 65535;
	mmc->max_req_size = 524288;

	/* Passing the "pio" option, we force the driver to not
	 * use any DMA engines. */
	if (unlikely(host->use_pio))
		pr_debug("\tPIO mode\n");
	else
		pr_debug("\tSDMA mode\n");

	platform_set_drvdata(pdev, mmc);
	ret = mmc_add_host(mmc);
	if (ret)
		goto out;

	setup_timer(&host->timer, arasan_timeout_timer, (unsigned long)host);

	pr_info("%s: driver initialized... IRQ: %d, Base addr 0x%x\n",
		mmc_hostname(mmc), irq, (unsigned int)host->base);

#ifdef ARASAN_DEBUG
	led = 1;
#endif
	return 0;
out:
	if (host) {
		if (host->irq)
			free_irq(host->irq, host);
		if (host->base)
			iounmap(host->base);
	}
	if (r)
		release_resource(r);
	if (mmc)
		mmc_free_host(mmc);

	return ret;
}

static int __exit arasan_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	if (mmc) {
		struct arasan_host *host = mmc_priv(mmc);

		arasan_clear_interrupts(host);
		tasklet_kill(&host->card_tasklet);
		mmc_remove_host(mmc);
		free_irq(host->irq, host);
		arasan_power_set(host, 0, -1);
		iounmap(host->base);
		release_resource(host->res);
		mmc_free_host(mmc);
	}
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int arasan_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct arasan_host *host = mmc_priv(mmc);
	int ret = 0;

	if (mmc && host->cap.suspend)
		ret = mmc_suspend_host(mmc, state);

	return ret;
}

static int arasan_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct arasan_host *host = mmc_priv(mmc);
	int ret = 0;

	if (mmc && host->cap.suspend)
		ret = mmc_resume_host(mmc);

	return ret;
}
#endif

static struct platform_driver arasan_driver = {
	.remove = __exit_p(arasan_remove),
#ifdef CONFIG_PM
	.suspend = arasan_suspend,
	.resume = arasan_resume,
#endif
	.driver = {
		   .name = ARASAN_DRIVER_NAME,
		   },
};

static int __init arasan_init(void)
{
	return platform_driver_probe(&arasan_driver, arasan_probe);
}

static void __exit arasan_exit(void)
{
	platform_driver_unregister(&arasan_driver);
}

#ifndef MODULE
static int __init arasan_cmdline_opt(char *str)
{
	char *opt;

	if (!str || !*str)
		return -EINVAL;

	while ((opt = strsep(&str, ",")) != NULL) {
		if (!strncmp(opt, "maxfreq:", 8))
			maxfreq = simple_strtoul(opt + 8, NULL, 0);
		else if (!strncmp(opt, "led:", 4))
			led = simple_strtoul(opt + 4, NULL, 0);
		else if (!strncmp(opt, "pio:", 4))
			pio = simple_strtoul(opt + 4, NULL, 0);
	}
	return 0;
}

__setup("arasanmmc=", arasan_cmdline_opt);
#endif

module_init(arasan_init);
module_exit(arasan_exit);

MODULE_AUTHOR("Giuseppe Cavallaro");
MODULE_DESCRIPTION("Arasan MMC/SD/SDIO Host Controller driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:arasan");
