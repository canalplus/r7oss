/*
 * Copyright © 2014 Broadcom Corporation
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

#include <linux/brcmstb/brcmstb.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include "xpt_dma.h"

/* descriptor flags and shifts */
#define MCPB_DW2_LAST_DESC		(1 << 0)

#define MCPB_DW4_PUSH_PARTIAL_PKT	(1 << 28)

#define MCPB_DW5_ENDIAN_STRAP_INV  (1 << 21)
#define MCPB_DW5_PID_CHANNEL_VALID (1 << 19)
#define MCPB_DW5_SCRAM_END         (1 << 18)
#define MCPB_DW5_SCRAM_START       (1 << 17)
#define MCPB_DW5_SCRAM_INIT        (1 << 16)

#define MEMDMA_DRAM_REQ_SIZE	256

#define XPT_MAC_OFFSET		0x14

#define MCPB_CHx_SPACING(channel) \
	((BCHP_XPT_MEMDMA_MCPB_CH1_REG_START - \
	 BCHP_XPT_MEMDMA_MCPB_CH0_REG_START) * (channel))

#define XPT_CHANNEL_A	0
#define XPT_CHANNEL_B	1

#define PID_CHANNEL_A	1022
#define PID_CHANNEL_B	1023

#define XPT_MAC_A	0
#define XPT_MAC_B	1

#define MAX_HASH_WAIT_US	(15 * 1000 * 1000) /* 15 seconds */

static inline void xpt_set_power(int on)
{
#ifdef BCHP_XPT_PMU_FE_SP_PD_MEM_PWR_DN_CTRL
	uint32_t val = on ? 0 : ~0;

	/* Power on/off everything */
	BDEV_WR(BCHP_XPT_PMU_FE_SP_PD_MEM_PWR_DN_CTRL, val);
	BDEV_WR(BCHP_XPT_PMU_MCPB_SP_PD_MEM_PWR_DN_CTRL, val);
	BDEV_WR(BCHP_XPT_PMU_MEMDMA_SP_PD_MEM_PWR_DN_CTRL, val);
#endif
}

static void mcpb_run(int enable, int channel)
{
	BDEV_WR_RB(BCHP_XPT_MEMDMA_MCPB_RUN_SET_CLEAR,
			(!!enable << 8) | channel);
}

static int mcpb_soft_init(void)
{
	int timeo = 1000 * 1000; /* 1 second timeout */

	BDEV_WR(BCHP_XPT_BUS_IF_SUB_MODULE_SOFT_INIT_DO_MEM_INIT,
			1 << 1); /* MEMDMA */
	BDEV_WR(BCHP_XPT_BUS_IF_SUB_MODULE_SOFT_INIT_SET,
			1 << 1); /* MEMDMA */

	for (;;) {
		if (!BDEV_RD_F(XPT_BUS_IF_SUB_MODULE_SOFT_INIT_STATUS,
					MEMDMA_MCPB_SOFT_INIT_STATUS))
			break;
		if (timeo <= 0)
			return -EIO;

		timeo -= 10;
		udelay(10);
	}

	BDEV_WR(BCHP_XPT_BUS_IF_SUB_MODULE_SOFT_INIT_CLEAR,
			1 << 1); /* MEMDMA */
	return 0;
}

static void memdma_init_mcpb_channel(int channel)
{
	unsigned long offs = MCPB_CHx_SPACING(channel);
	unsigned long parser_ctrl = BCHP_XPT_MEMDMA_MCPB_CH0_SP_PARSER_CTRL + offs;
	unsigned long packet_len = BCHP_XPT_MEMDMA_MCPB_CH0_SP_PKT_LEN + offs;
	unsigned long dma_buf_ctrl = BCHP_XPT_MEMDMA_MCPB_CH0_DMA_BBUFF_CTRL + offs;
	unsigned long dma_data_ctrl = BCHP_XPT_MEMDMA_MCPB_CH0_DMA_DATA_CONTROL + offs;

	mcpb_run(0, channel);

	/* setup for block mode */
	BDEV_WR(parser_ctrl,
			(1 << 0) |		/* parser enable */
			(6 << 1) |		/* block mode */
			(channel << 6) |	/* band ID */
			(1 << 14));		/* select playback parser */
	BDEV_WR(packet_len, 208); /* packet length */
	BDEV_WR(dma_buf_ctrl, 224); /* stream feed size */
	BDEV_WR(dma_data_ctrl, (MEMDMA_DRAM_REQ_SIZE << 0) |
			       (0 << 11));	/* disable run version match */
}

static void xpt_init_ctx(unsigned int channel, unsigned int pid_channel)
{
	/* configure PID channel */
	BDEV_WR(BCHP_XPT_FE_PID_TABLE_i_ARRAY_BASE + 4 * pid_channel,
			(1 << 14) |		/* enable PID channel */
			(channel << 16) |	/* input parser band */
			(1 << 23) |		/* playback parser */
			(1 << 28));		/* direct to XPT security */

	BDEV_WR(BCHP_XPT_FE_SPID_TABLE_i_ARRAY_BASE + 4 * pid_channel, 0);

	BDEV_WR(BCHP_XPT_FE_SPID_EXT_TABLE_i_ARRAY_BASE +
		     4 * pid_channel, 1); /* G pipe */
}

static void memdma_init_hw(int channel, int pid)
{
	memdma_init_mcpb_channel(channel);
	xpt_init_ctx(channel, pid);
}

static int mcpb_init_desc(struct mcpb_dma_desc *desc, dma_addr_t next,
		dma_addr_t buf, size_t len, int first, int last,
		unsigned int pid_channel)
{
	memset(desc, 0, sizeof(*desc));

	/* 5 LSBs must be 0; can only handle 32-bit addresses */
	if (WARN_ON((next & 0x1f) || (upper_32_bits(next) != 0)))
		return -EINVAL;

	desc->buf_hi = upper_32_bits(buf);
	desc->buf_lo = lower_32_bits(buf); /* BUFF_ST_RD_ADDR [31:0] */
	desc->next_offs = lower_32_bits(next); /* NEXT_DESC_ADDR [31:5] */
	desc->size = len; /* BUFF_SIZE [31:0] */
	desc->opts2 = MCPB_DW5_PID_CHANNEL_VALID | MCPB_DW5_ENDIAN_STRAP_INV;
	desc->pid_channel = pid_channel;

	if (first)
		desc->opts2 |= (MCPB_DW5_SCRAM_INIT | MCPB_DW5_SCRAM_START);

	if (last) {
		desc->next_offs = MCPB_DW2_LAST_DESC;
		desc->opts1 |= MCPB_DW4_PUSH_PARTIAL_PKT;
		desc->opts2 |= MCPB_DW5_SCRAM_END;
	}

	return 0;
}

/*
 * memdma_prepare_descs - prepare a MEMDMA descriptor chain
 *
 * @descs: array of descriptors
 * @descs_pa: physical address of @descs
 * @regions: the address ranges to set up for MEMDMA
 * @numregions: number of regions (in @descs and @regions)
 * @channel_A: if true, use the first MAC channel (a.k.a. "channel A"); if
 *     false, use the second MAC channel (a.k.a. "channel B")
 */
int memdma_prepare_descs(struct mcpb_dma_desc *descs, dma_addr_t descs_pa,
		struct dma_region *regions, int numregions, bool channel_A)
{
	int i;
	int pid = channel_A ? PID_CHANNEL_A : PID_CHANNEL_B;
	dma_addr_t next_pa = descs_pa;
	int ret;

	for (i = 0; i < numregions; i++) {
		int first = (i == 0);
		int last = (i == (numregions - 1));

		if (last)
			next_pa = 0;
		else
			next_pa += sizeof(*descs);

		ret = mcpb_init_desc(&descs[i], next_pa, regions[i].addr,
				     regions[i].len, first, last, pid);
		if (ret)
			return ret;
	}

	return 0;
}

static bool hash_is_ready(int mac)
{
	if (mac)
		return BDEV_RD_F(XPT_SECURITY_NS_INTR2_0_CPU_STATUS, MAC1_READY);
	else
		return BDEV_RD_F(XPT_SECURITY_NS_INTR2_0_CPU_STATUS, MAC0_READY);
}

static void clear_hash_interrupt(int mac)
{
	if (mac)
		BDEV_WR_F(XPT_SECURITY_NS_INTR2_0_CPU_CLEAR, MAC1_READY, 1);
	else
		BDEV_WR_F(XPT_SECURITY_NS_INTR2_0_CPU_CLEAR, MAC0_READY, 1);
}

static int memdma_wait_for_hash(int mac)
{
	int timeo = MAX_HASH_WAIT_US;
	for (;;) {
		if (hash_is_ready(mac))
			break;
		if (timeo <= 0) {
			pr_err("error: timeout waiting for MAC%d\n", mac);
			return -EIO;
		}

		timeo -= 10;
		udelay(10);
	}

	/* Clear status */
	clear_hash_interrupt(mac);

	return 0;
}

static void memdma_start(dma_addr_t desc, int channel)
{
	unsigned long reg = BCHP_XPT_MEMDMA_MCPB_CH0_DMA_DESC_CONTROL +
		MCPB_CHx_SPACING(channel);

	BDEV_WR(reg, (uint32_t)desc);
	mcpb_run(1, channel);
}

/*
 * memdma_run - Run the MEMDMA MAC on up to 2 DMA descriptor chains
 *
 * @desc1: the physical address of the first descriptor chain
 * @desc2: the physical address of the second descriptor chain (optional)
 * @dual_channel: if true, then use desc2 with a second DMA channel; otherwise,
 *     ignore desc2
 */
int memdma_run(dma_addr_t desc1, dma_addr_t desc2, bool dual_channel)
{
	int ret, ret2 = 0;

	xpt_set_power(1);

	ret = mcpb_soft_init();
	if (ret)
		goto out;

	memdma_init_hw(0, PID_CHANNEL_A);
	mb();
	memdma_start(desc1, XPT_CHANNEL_A);

	if (dual_channel) {
		memdma_init_hw(1, PID_CHANNEL_B);
		mb();
		memdma_start(desc2, XPT_CHANNEL_B);
	}

	ret = memdma_wait_for_hash(XPT_MAC_A);
	if (dual_channel)
		ret2 = memdma_wait_for_hash(XPT_MAC_B);

	/* Report the 1st non-zero return code */
	if (!ret)
		ret = ret2;

out:
	xpt_set_power(0);

	return ret;
}

static uint32_t get_hash_idx(int mac_num, int word)
{
	int len = 128 / 32;
	unsigned long reg_base;

	if (word >= len)
		return 0;

	reg_base = BCHP_XPT_SECURITY_NS_MAC0_0 + mac_num * XPT_MAC_OFFSET;
	return BDEV_RD(reg_base + word * 4);
}

void get_hash(uint32_t *hash, bool dual_channel)
{
	/* 128-bit AES CMAC hash */
	int i, len = 128 / 8;

	/* MAC0 */
	for (i = 0; i < len / sizeof(*hash); i++)
		hash[i] = get_hash_idx(0, i);

	if (dual_channel)
		/* MAC1 */
		for (i = 0; i < len / sizeof(*hash); i++)
			hash[i] ^= get_hash_idx(1, i);
}
