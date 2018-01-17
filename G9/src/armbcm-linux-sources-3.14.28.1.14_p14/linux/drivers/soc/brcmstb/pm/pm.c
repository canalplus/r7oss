/*
 * ARM-specific support for Broadcom STB S2/S3/S5 power management
 *
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

#define pr_fmt(fmt) "brcmstb-pm: " fmt

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/compiler.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/kconfig.h>
#include <linux/sort.h>
#include <linux/notifier.h>
#include <asm/fncpy.h>
#include <asm/suspend.h>
#include <asm/setup.h>

#include <linux/brcmstb/brcmstb.h>
#include <soc/brcmstb/common.h>

#include "pm.h"
#include "aon_defs.h"
#include "xpt_dma.h"

#define BRCMSTB_DDR_PHY_PLL_STATUS	0x04

#define SHIMPHY_DDR_PAD_CNTRL		0x8c
#define SHIMPHY_PAD_PLL_SEQUENCE	BIT(8)
#define SHIMPHY_PAD_GATE_PLL_S3		BIT(9)

#define MAX_NUM_MEMC			3

/* Capped for performance reasons */
#define MAX_HASH_SIZE			SZ_256M
/* Max per bank, to keep some fairness */
#define MAX_HASH_SIZE_BANK		SZ_64M

struct brcmstb_memc {
	void __iomem *ddr_phy_base;
	void __iomem *ddr_shimphy_base;
};

struct brcmstb_pm_control {
	void __iomem *aon_ctrl_base;
	void __iomem *aon_sram;
	size_t aon_sram_len;
	struct brcmstb_memc memcs[MAX_NUM_MEMC];

	void __iomem *boot_sram;
	size_t boot_sram_len;

	bool support_warm_boot;
	int num_memc;

	struct brcmstb_s3_params *s3_params;
	dma_addr_t s3_params_pa;
};

enum bsp_initiate_command {
	BSP_CLOCK_STOP		= 0x00,
	BSP_GEN_RANDOM_KEY	= 0x4A,
	BSP_RESTORE_RANDOM_KEY	= 0x55,
	BSP_GEN_FIXED_KEY	= 0x63,
};

#define PM_INITIATE		0x01
#define PM_INITIATE_SUCCESS	0x00
#define PM_INITIATE_FAIL	0xfe

/* Several chips have an old PM_INITIATE interface that doesn't ACK commands */
#define PM_INITIATE_NO_ACK	IS_ENABLED(CONFIG_BCM74371A0)

static struct brcmstb_pm_control ctrl;
static suspend_state_t suspend_state;

#define MAX_EXCLUDE				16
static int num_exclusions;
static struct dma_region exclusions[MAX_EXCLUDE];

extern const unsigned long brcmstb_pm_do_s2_sz;
extern asmlinkage int brcmstb_pm_do_s2(void __iomem *aon_ctrl_base,
		void __iomem *ddr_phy_pll_status);

static int (*brcmstb_pm_do_s2_sram)(void __iomem *aon_ctrl_base,
		void __iomem *ddr_phy_pll_status);

static int brcmstb_init_sram(struct device_node *dn)
{
	void __iomem *sram;
	struct resource res;
	int ret;

	ret = of_address_to_resource(dn, 0, &res);
	if (ret)
		return ret;

	/* Cached, executable remapping of SRAM */
	sram = __arm_ioremap_exec(res.start, resource_size(&res), true);
	if (!sram)
		return -ENOMEM;

	ctrl.boot_sram = sram;
	ctrl.boot_sram_len = resource_size(&res);

	return 0;
}

/*
 * Latch into the BRCM SRAM compatible property here to be more specific than
 * the standard "mmio-sram"
 *
 * FIXME: As of Linux 3.10, upstream's drivers/misc/sram.c should support
 * allocating SRAM via genalloc
 */
static struct of_device_id sram_dt_ids[] = {
	{ .compatible = "brcm,boot-sram" },
	{}
};

static int do_bsp_initiate_command(enum bsp_initiate_command cmd)
{
	void __iomem *base = ctrl.aon_ctrl_base;
	int ret;
	int timeo = 1000 * 1000; /* 1 second */

	__raw_writel(0, base + AON_CTRL_PM_INITIATE);
	(void)__raw_readl(base + AON_CTRL_PM_INITIATE);

	/* Go! */
	__raw_writel((cmd << 1) | PM_INITIATE, base + AON_CTRL_PM_INITIATE);

	/*
	 * If firmware doesn't support the 'ack', then just assume it's done
	 * after 10ms. Note that this only works for command 0, BSP_CLOCK_STOP
	 */
	if (PM_INITIATE_NO_ACK) {
		(void)__raw_readl(base + AON_CTRL_PM_INITIATE);
		mdelay(10);
		return 0;
	}

	for (;;) {
		ret = __raw_readl(base + AON_CTRL_PM_INITIATE);
		if (!(ret & PM_INITIATE))
			break;
		if (timeo <= 0) {
			pr_err("error: timeout waiting for BSP (%x)\n", ret);
			break;
		}
		timeo -= 50;
		udelay(50);
	}

	return (ret & 0xff) != PM_INITIATE_SUCCESS;
}

static int brcmstb_pm_handshake(void)
{
	void __iomem *base = ctrl.aon_ctrl_base;
	u32 tmp;
	int ret;

	/* BSP power handshake, v1 */
	tmp = __raw_readl(base + AON_CTRL_HOST_MISC_CMDS);
	tmp &= ~1UL;
	__raw_writel(tmp, base + AON_CTRL_HOST_MISC_CMDS);
	(void)__raw_readl(base + AON_CTRL_HOST_MISC_CMDS);

	ret = do_bsp_initiate_command(BSP_CLOCK_STOP);
	if (ret)
		pr_err("BSP handshake failed\n");

	/*
	 * HACK: BSP may have internal race on the CLOCK_STOP command.
	 * Avoid touching the BSP for a few milliseconds.
	 */
	mdelay(3);

	return ret;
}

/*
 * Enable the frontpanel reset functionality and lock it
 *
 * If the feature is not locked, it will be disabled automatically in s3 mode
 */
static void brcmstb_fp_reset_enable(void)
{
	void __iomem *base = ctrl.aon_ctrl_base;
    int val;

	val = __raw_readl(base + AON_CTRL_RESET_CTRL);
	val |= FP_RESET_ENABLE | FP_RESET_ENABLE_LOCK;
	__raw_writel(val, base + AON_CTRL_RESET_CTRL);
}

/*
 * Run a Power Management State Machine (PMSM) shutdown command and put the CPU
 * into a low-power mode
 */
static void brcmstb_do_pmsm_power_down(unsigned long base_cmd)
{
	void __iomem *base = ctrl.aon_ctrl_base;

	/* pm_start_pwrdn transition 0->1 */
	__raw_writel(base_cmd, base + AON_CTRL_PM_CTRL);
	(void)__raw_readl(base + AON_CTRL_PM_CTRL);

	__raw_writel(base_cmd | PM_PWR_DOWN, base + AON_CTRL_PM_CTRL);
	(void)__raw_readl(base + AON_CTRL_PM_CTRL);

	wfi();
}

/* Support S5 cold boot out of "poweroff" */
static void brcmstb_pm_poweroff(void)
{
	brcmstb_pm_handshake();

	/* Clear magic S3 warm-boot value */
	__raw_writel(0, ctrl.aon_sram + AON_REG_MAGIC_FLAGS);
	(void)__raw_readl(ctrl.aon_sram + AON_REG_MAGIC_FLAGS);

	/* Skip wait-for-interrupt signal; just use a countdown */
	__raw_writel(0x10, ctrl.aon_ctrl_base + AON_CTRL_PM_CPU_WAIT_COUNT);
	(void)__raw_readl(ctrl.aon_ctrl_base + AON_CTRL_PM_CPU_WAIT_COUNT);

	brcmstb_do_pmsm_power_down(PM_COLD_CONFIG);
}

static void *brcmstb_pm_copy_to_sram(void *fn, size_t len)
{
	unsigned int size = ALIGN(len, FNCPY_ALIGN);

	if (ctrl.boot_sram_len < size) {
		pr_err("standby code will not fit in SRAM\n");
		return NULL;
	}

	return fncpy(ctrl.boot_sram, fn, size);
}

static int brcmstb_pm_s2(void)
{
	brcmstb_pm_do_s2_sram = brcmstb_pm_copy_to_sram(&brcmstb_pm_do_s2,
			brcmstb_pm_do_s2_sz);
	if (!brcmstb_pm_do_s2_sram)
		return -EINVAL;

	return brcmstb_pm_do_s2_sram(ctrl.aon_ctrl_base,
			ctrl.memcs[0].ddr_phy_base +
			BRCMSTB_DDR_PHY_PLL_STATUS);
}

static int brcmstb_pm_s3_control_hash(struct brcmstb_s3_params *params,
		phys_addr_t params_pa)
{
	size_t hash_len = sizeof(*params) - BOOTLOADER_SCRATCH_SIZE;
	struct dma_region region[] = {
		{
			.addr	= params_pa + BOOTLOADER_SCRATCH_SIZE,
			.len	= hash_len,
		},
	};
	uint32_t hash[BRCMSTB_HASH_LEN / 4];
	int i, ret;

	/* Co-opt bootloader scratch area temporarily */
	ret = memdma_prepare_descs((struct mcpb_dma_desc *)params->scratch,
				   params_pa, region, ARRAY_SIZE(region), true);
	if (ret)
		return ret;

	dma_sync_single_for_device(NULL, params_pa, sizeof(*params),
				   DMA_TO_DEVICE);

	ret = memdma_run(params_pa, 0, false);
	if (ret)
		return ret;

	get_hash(hash, false);

	/* Store hash in AON */
	for (i = 0; i < BRCMSTB_HASH_LEN / 4; i++)
		__raw_writel(hash[i], ctrl.aon_sram + AON_REG_S3_HASH + i * 4);
	__raw_writel(hash_len, ctrl.aon_sram + AON_REG_CONTROL_HASH_LEN);

	return 0;
}

static inline int region_collision(struct dma_region *reg1,
				   struct dma_region *reg2)
{
	return (reg1->addr + reg1->len > reg2->addr) &&
	       (reg2->addr + reg2->len > reg1->addr);
}

/**
 * Check if @regions[0] collides with regions in @exceptions, and modify
 * regions[0..(max-1)] to ensure that they they exclude any area in @exceptions
 *
 * Note that the regions in @exceptions must be sorted into ascending order prior
 * to calling this function
 *
 * Returns the number of @regions used
 *
 * e.g., if @regions[0] and @exceptions do not overlap, return 1 and do nothing
 *       if @exceptions contains two ranges and both are entirely contained
 *          within @regions[0], split @regions[0] into @regions[0],
 *          @regions[1], and @regions[2], and return 3
 */
static int region_handle_collisions(struct dma_region *regions, int max,
				struct dma_region *exceptions, int num_except)
{
	int i;
	struct dma_region *reg = &regions[0];
	int reg_count = 1;

	/*
	 * Since the list of regions is ordered in ascending order we need only
	 * to compare the last entry in regions against each exception region
	 */
	for (i = 0; i < num_except; i++) {
		struct dma_region *except = &exceptions[i];
		dma_addr_t start = reg->addr;
		dma_addr_t end = reg->addr + reg->len;

		if (!region_collision(reg, except))
			/* No collision */
			continue;

		if (start < except->addr && end > except->addr + except->len) {
			reg->len = except->addr - start;
			if (reg_count < max) {
				/* Split in 2 */
				reg++;
				reg_count++;
				reg->addr = except->addr + except->len;
				reg->len = end - reg->addr;
			} else {
				pr_warn("Not enough space to split region\n");
				break;
			}
		} else if (start < except->addr) {
			/* Overlap at right edge; truncate end of 'reg' */
			reg->len = except->addr - start;
		} else if (end > except->addr + except->len) {
			/* Overlap at left edge; truncate beginning of 'reg' */
			reg->addr = except->addr + except->len;
			reg->len = end - reg->addr;
		} else {
			/*
			 * 'reg' is entirely contained within 'except'?  This
			 * should not happen, but trim to zero-length just in
			 * case
			 */
			reg->len = 0;
			reg_count--;
			break;
		}
	}

	return reg_count;
}

static int dma_region_compare(const void *a, const void *b)
{
	struct dma_region *reg_a = (struct dma_region *)a;
	struct dma_region *reg_b = (struct dma_region *)b;

	if (reg_a->addr < reg_b->addr)
		return -1;
	if (reg_a->addr > reg_b->addr)
		return 1;
	return 0;
}

/* Initialize the DMA region list and return the number of regions */
static int configure_main_hash(struct dma_region *regions, int max,
			       struct dma_region *exclude, int num_exclude)
{
	int idx = 0, bank_nr;
	size_t total = 0;

	/*
	 * First sort the excluded regions in ascending order. This makes things
	 * easier when we come to adding the regions since we avoid having to
	 * add entries in the middle of the region list
	 */
	sort(exclude, num_exclude, sizeof(exclude[0]), &dma_region_compare,
			NULL);

	/*
	 * Hash up to MAX_HASH_SIZE_BANK from each memory bank, with a
	 * total limit of MAX_HASH_SIZE. Account for collisions with the
	 * 'exclude' regions.
	 */
	for_each_bank(bank_nr, &meminfo) {
		const struct membank *bank = &meminfo.bank[bank_nr];
		struct dma_region *reg = &regions[idx];
		int i, count;
		size_t bank_total = 0;

		reg->addr = bank->start;
		reg->len = bank->size;

		/*
		 * Check for collisions with the excluded regions.  'reg' may be
		 * split into 0 to (num_exclude + 1) segments, so account
		 * accordingly
		 */
		count = region_handle_collisions(reg, max - idx, exclude,
						 num_exclude);

		/*
		 * Add region length(s) to total. Cap at MAX_HASH_SIZE_BANK
		 * per bank and MAX_HASH_SIZE total.
		 */
		for (i = 0; i < count; i++) {
			/* Don't create 0-sized regions */
			if (total >= MAX_HASH_SIZE)
				break;
			if (bank_total >= MAX_HASH_SIZE_BANK)
				break;
			if (total + reg[i].len > MAX_HASH_SIZE)
				reg[i].len = MAX_HASH_SIZE - total;
			if (bank_total + reg[i].len > MAX_HASH_SIZE_BANK)
				reg[i].len = MAX_HASH_SIZE_BANK - bank_total;
			total += reg[i].len;
			bank_total += reg[i].len;
		}

		idx += i;

		if (idx >= max)
			break;

		/* Apply total cap */
		if (total >= MAX_HASH_SIZE)
			break;
	}

	return idx;
}

/*
 * Run a DMA hash on the given regions, splitting evenly into two channels if
 * possible
 *
 * If 2 channels were run, return the byte offset of the second (from descs_pa)
 * If 1 channel was run, return 0
 * Otherwise (on errors) return negative
 */
static int run_dual_hash(struct dma_region *regions, int numregions,
		struct mcpb_dma_desc *descs, phys_addr_t descs_pa,
		uint32_t *hash)
{
	phys_addr_t pa1, pa2;
	struct mcpb_dma_desc *desc1, *desc2;
	int regions1, regions2;
	int ret;

	/* Split regions into 2 partitions */
	regions2 = numregions / 2;
	regions1 = numregions - regions2;
	desc1 = descs;
	desc2 = desc1 + regions1;
	pa1 = descs_pa;
	pa2 = pa1 + regions1 * sizeof(*desc1);

	/* Prepare both sets of descriptors */
	ret = memdma_prepare_descs(desc1, pa1, &regions[0], regions1, true);
	if (ret)
		return ret;
	ret = memdma_prepare_descs(desc2, pa2, &regions[regions1], regions2, false);
	if (ret)
		return ret;

	dma_sync_single_for_device(NULL, pa1, sizeof(*desc1) * numregions,
				   DMA_TO_DEVICE);

	/* Go! */
	ret = memdma_run(pa1, pa2, !!regions2);
	if (ret)
		return ret;

	get_hash(hash, !!regions2);

	if (regions2)
		return regions1 * sizeof(*desc1);
	else
		return 0;
}

static int brcmstb_pm_s3_main_memory_hash(struct brcmstb_s3_params *params,
		phys_addr_t params_pa, struct dma_region *except,
		int num_except)
{
	struct dma_region regions[40];
	phys_addr_t descs_pa;
	struct mcpb_dma_desc *descs;
	int nregs, ret;

	nregs = configure_main_hash(regions, ARRAY_SIZE(regions), except,
			num_except);
	if (nregs < 0)
		return nregs;

	/* Flush out before hashing main memory */
	flush_cache_all();

	/* Get base pointers */
	descs_pa = params_pa + offsetof(struct brcmstb_s3_params, descriptors);
	descs = (struct mcpb_dma_desc *)params->descriptors;

	/* Split, run hash */
	ret = run_dual_hash(regions, nregs, descs, descs_pa, params->hash);
	if (ret < 0)
		return ret;
	params->desc_offset_2 = ret;

	return 0;
}

int brcmstb_pm_mem_exclude(phys_addr_t addr, size_t len)
{
	if (num_exclusions >= MAX_EXCLUDE) {
		pr_err("exclusion list is full\n");
		return -ENOSPC;
	}

	exclusions[num_exclusions].addr = addr;
	exclusions[num_exclusions].len = len;
	num_exclusions++;

	return 0;
}
EXPORT_SYMBOL(brcmstb_pm_mem_exclude);

/*
 * This function is called on a new stack, so don't allow inlining (which will
 * generate stack references on the old stack)
 */
static noinline int brcmstb_pm_s3_finish(void)
{
	struct brcmstb_s3_params *params = ctrl.s3_params;
	phys_addr_t params_pa = ctrl.s3_params_pa;
	phys_addr_t reentry = virt_to_phys(&cpu_resume);
	u32 flags = 0;
	enum bsp_initiate_command cmd;
	int i, ret, num_exclude;

	ret = brcmstb_pm_mem_exclude(params_pa, sizeof(*params));
	if (ret) {
		pr_err("failed to add parameter exclusion region\n");
		return ret;
	}

	/* Clear parameter structure */
	memset(params, 0, sizeof(*params));

	flags |= S3_FLAG_LOAD_RANDKEY; /* TODO: make this selectable */

	/* Load random / fixed key */
	if (flags & S3_FLAG_LOAD_RANDKEY)
		cmd = BSP_GEN_RANDOM_KEY;
	else
		cmd = BSP_GEN_FIXED_KEY;
	if (do_bsp_initiate_command(cmd)) {
		pr_info("key loading failed\n");
		return -EIO;
	}

	/* Reset exclusion regions */
	num_exclude = num_exclusions;
	num_exclusions = 0;

	/* Hash main memory */
	ret = brcmstb_pm_s3_main_memory_hash(params, params_pa, exclusions,
					     num_exclude);
	if (ret)
		return ret;

	params->magic = BRCMSTB_S3_MAGIC;
	params->reentry = reentry;

	/* No more writes to DRAM */
	flush_cache_all();

	/* Hash S3 saved parameters */
	ret = brcmstb_pm_s3_control_hash(params, params_pa);
	if (ret)
		return ret;

	flags |= BRCMSTB_S3_MAGIC_SHORT;

	__raw_writel(flags, ctrl.aon_sram + AON_REG_MAGIC_FLAGS);
	__raw_writel(lower_32_bits(params_pa),
		     ctrl.aon_sram + AON_REG_CONTROL_LOW);
	__raw_writel(upper_32_bits(params_pa),
		     ctrl.aon_sram + AON_REG_CONTROL_HIGH);

	/* gate PLL | S3 */
	for (i = 0; i < ctrl.num_memc; i++) {
		u32 tmp;
		tmp = __raw_readl(ctrl.memcs[i].ddr_shimphy_base +
				SHIMPHY_DDR_PAD_CNTRL);
		tmp |= SHIMPHY_PAD_GATE_PLL_S3 | SHIMPHY_PAD_PLL_SEQUENCE;
		__raw_writel(tmp, ctrl.memcs[i].ddr_shimphy_base +
				SHIMPHY_DDR_PAD_CNTRL);
	}

	brcmstb_do_pmsm_power_down(PM_WARM_CONFIG);

	/* Must have been interrupted from wfi()? */
	return -EINTR;
}

#define SWAP_STACK(new_sp, saved_sp) \
	__asm__ __volatile__ ( \
		 "mov	%[save], sp\n" \
		 "mov	sp, %[new]\n" \
		 : [save] "=&r" (saved_sp) \
		 : [new] "r" (new_sp) \
		)

static int brcmstb_pm_do_s3(unsigned long sp)
{
	unsigned long save_sp;
	int ret;

	/* Move to a new stack */
	SWAP_STACK(sp, save_sp);

	/* should not return */
	ret = brcmstb_pm_s3_finish();

	SWAP_STACK(save_sp, sp);

	pr_err("Could not enter S3\n");

	return ret;
}

static int brcmstb_pm_s3(void)
{
	void __iomem *sp = ctrl.boot_sram + ctrl.boot_sram_len - 8;

	return cpu_suspend((unsigned long)sp, brcmstb_pm_do_s3);
}

static int brcmstb_pm_standby(bool deep_standby)
{
	int ret;

	if (brcmstb_pm_handshake())
		return -EIO;

	if (deep_standby)
		ret = brcmstb_pm_s3();
	else
		ret = brcmstb_pm_s2();
	if (ret)
		pr_err("%s: standby failed\n", __func__);

	return ret;
}

static int brcmstb_pm_begin(suspend_state_t state)
{
	suspend_state = state;
	return 0;
}

static int brcmstb_pm_prepare(void)
{
	/* TODO: initialize memory hashing (for S3) here? */
	return 0;
}

static int brcmstb_pm_enter(suspend_state_t unused)
{
	int ret = -EINVAL;

	switch (suspend_state) {
	case PM_SUSPEND_STANDBY:
		ret = brcmstb_pm_standby(false);
		break;
	case PM_SUSPEND_MEM:
		ret = brcmstb_pm_standby(true);
		break;
	}

	return ret;
}

static void brcmstb_pm_end(void)
{
	suspend_state = PM_SUSPEND_ON;
}

static int brcmstb_pm_valid(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_STANDBY:
		return true;
	case PM_SUSPEND_MEM:
		return ctrl.support_warm_boot;
	default:
		return false;
	}
}

static const struct platform_suspend_ops brcmstb_pm_ops = {
	.begin		= brcmstb_pm_begin,
	.end		= brcmstb_pm_end,
	.prepare	= brcmstb_pm_prepare,
	.enter		= brcmstb_pm_enter,
	.valid		= brcmstb_pm_valid,
};

static struct of_device_id aon_ctrl_dt_ids[] = {
	{ .compatible = "brcm,brcmstb-aon-ctrl" },
	{}
};

struct ddr_phy_ofdata {
	bool supports_warm_boot;
};

static struct ddr_phy_ofdata ddr_phy_225_1 = { .supports_warm_boot = false, };
static struct ddr_phy_ofdata ddr_phy_240_1 = { .supports_warm_boot = true, };

static struct of_device_id ddr_phy_dt_ids[] = {
	{
		.compatible = "brcm,brcmstb-ddr-phy-v225.1",
		.data = &ddr_phy_225_1,
	},
	{
		.compatible = "brcm,brcmstb-ddr-phy-v240.1",
		.data = &ddr_phy_240_1,
	},
	{
		/* Same as v240.1, for the registers we care about */
		.compatible = "brcm,brcmstb-ddr-phy-v240.2",
		.data = &ddr_phy_240_1,
	},
	{}
};

static struct of_device_id ddr_shimphy_dt_ids[] = {
	{ .compatible = "brcm,brcmstb-ddr-shimphy-v1.0" },
	{}
};

static void __iomem *brcmstb_ioremap_node(struct device_node *dn, int index,
		size_t *retlen)
{
	struct resource res, *res2;
	int ret;
	void __iomem *base;
	size_t len;

	if (retlen)
		*retlen = 0;

	ret = of_address_to_resource(dn, index, &res);
	if (ret < 0)
		return ERR_PTR(ret);

	len = resource_size(&res);

	res2 = request_mem_region(res.start, len, dn->full_name);
	if (!res2)
		return ERR_PTR(-EIO);

	base = ioremap(res2->start, resource_size(res2));
	if (!base) {
		release_mem_region(res.start, len);
		return ERR_PTR(-ENOMEM);
	}

	if (retlen)
		*retlen = len;

	return base;
}

static void __iomem *brcmstb_ioremap_match(struct of_device_id *matches,
					    int index, size_t *retlen,
					    const void **ofdata)
{
	struct device_node *dn;
	const struct of_device_id *match;
	void __iomem *base;

	dn = of_find_matching_node_and_match(NULL, matches, &match);
	if (!dn)
		return ERR_PTR(-EINVAL);

	base = brcmstb_ioremap_node(dn, index, retlen);

	if (ofdata)
		*ofdata = match->data;

	return base;
}

static int brcmstb_pm_panic_notify(struct notifier_block *nb,
		unsigned long action, void *data)
{
	__raw_writel(BRCMSTB_PANIC_MAGIC,
		ctrl.aon_sram + AON_REG_PANIC);

	return NOTIFY_DONE;
}

static struct notifier_block brcmstb_pm_panic_nb = {
	.notifier_call = brcmstb_pm_panic_notify,
};

static int brcmstb_pm_init(void)
{
	struct device_node *dn;
	void __iomem *base;
	int ret, i;
	const struct ddr_phy_ofdata *ddr_phy_data;

	if (!soc_is_brcmstb())
		return 0;

	/* AON ctrl registers */
	base = brcmstb_ioremap_match(aon_ctrl_dt_ids, 0, NULL, NULL);
	if (IS_ERR_OR_NULL(base)) {
		pr_err("error mapping AON_CTRL\n");
		return PTR_ERR(base);
	}
	ctrl.aon_ctrl_base = base;

	/* AON SRAM registers */
	base = brcmstb_ioremap_match(aon_ctrl_dt_ids, 1, &ctrl.aon_sram_len,
				     NULL);
	if (IS_ERR_OR_NULL(base)) {
		/* Assume standard offset */
		ctrl.aon_sram = ctrl.aon_ctrl_base +
				     AON_CTRL_SYSTEM_DATA_RAM_OFS;
	} else {
		ctrl.aon_sram = base;
	}

	__raw_writel(0, ctrl.aon_sram + AON_REG_PANIC);

	/* DDR PHY registers */
	base = brcmstb_ioremap_match(ddr_phy_dt_ids, 0, NULL,
				     (const void **)&ddr_phy_data);
	if (IS_ERR_OR_NULL(base)) {
		pr_err("error mapping DDR PHY\n");
		return PTR_ERR(base);
	}
	ctrl.support_warm_boot = ddr_phy_data->supports_warm_boot;
	/* Only need DDR PHY 0 for now? */
	ctrl.memcs[0].ddr_phy_base = base;

	/* DDR SHIM-PHY registers */
	for_each_matching_node(dn, ddr_shimphy_dt_ids) {
		i = ctrl.num_memc;
		if (i >= MAX_NUM_MEMC) {
			pr_warn("too many MEMCs (max %d)\n", MAX_NUM_MEMC);
			break;
		}
		base = brcmstb_ioremap_node(dn, 0, NULL);
		if (IS_ERR_OR_NULL(base)) {
			if (!ctrl.support_warm_boot)
				break;

			pr_err("error mapping DDR SHIMPHY %d\n", i);
			return PTR_ERR(base);
		}
		ctrl.memcs[i].ddr_shimphy_base = base;
		ctrl.num_memc++;
	}

	dn = of_find_matching_node(NULL, sram_dt_ids);
	if (!dn) {
		pr_err("SRAM not found\n");
		return -EINVAL;
	}

	ret = brcmstb_init_sram(dn);
	if (ret) {
		pr_err("error setting up SRAM for PM\n");
		return ret;
	}

	ctrl.s3_params = kmalloc(sizeof(*ctrl.s3_params), GFP_KERNEL);
	if (!ctrl.s3_params)
		return -ENOMEM;
	ctrl.s3_params_pa = dma_map_single(NULL, ctrl.s3_params,
					   sizeof(*ctrl.s3_params),
					   DMA_TO_DEVICE);
	if (dma_mapping_error(NULL, ctrl.s3_params_pa)) {
		pr_err("error mapping DMA memory\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = brcmstb_regsave_init();
	if (ret)
		goto out2;

	atomic_notifier_chain_register(&panic_notifier_list,
				       &brcmstb_pm_panic_nb);

	pm_power_off = brcmstb_pm_poweroff;
	suspend_set_ops(&brcmstb_pm_ops);

    /* Enable the frontpanel reset functionality */
    brcmstb_fp_reset_enable();

  	return 0;

out2:
	dma_unmap_single(NULL, ctrl.s3_params_pa, sizeof(*ctrl.s3_params),
			 DMA_TO_DEVICE);
out:
	kfree(ctrl.s3_params);

	pr_warn("PM: initialization failed with code %d\n", ret);

	return ret;
}

arch_initcall(brcmstb_pm_init);
