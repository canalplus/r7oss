/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/suspend-st40.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/kobject.h>
#include <linux/stat.h>
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <asm/system.h>
#include <asm/cpu/cacheflush.h>
#include <asm/io.h>
#include <asm-generic/bug.h>
#include <asm/pm.h>
#include <linux/reboot.h>

#ifdef CONFIG_STM_LPFP
#include "lowpower_frontpanel.h"
#endif

#ifdef CONFIG_STM_LPIR
#include "lowpower-ir-wakeup.h"
#endif
#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)		\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

#define ST7105_PTI_BASE_ADDRESS 0xFE230000
#define WAKEUP_PTI_OFFSET 0x9984
#define register_write32(addr, data32) \
		(*(volatile unsigned int*)(addr)=(int)data32)
#define register_read32(addr) (*(volatile unsigned int*)(addr))

#define WAKEUP_SRC_TIMER	0x10
#define WAKEUP_SRC_FP_POWER	74
#define WAKEUP_SRC_FP_PROG	0x71
#define WAKEUP_SRC_IR		0x7d

extern struct kset power_subsys;
extern unsigned int __text_lpm_start;
extern unsigned int __text_lpm_end;

unsigned int wokenup_by ;

unsigned long sh4_suspend(struct sh4_suspend_t *pdata,
	unsigned long instr_tbl, unsigned long instr_tbl_end);

static inline unsigned long _10_ms_lpj(void)
{
	static struct clk *sh4_clk;
	if (!sh4_clk)
		sh4_clk = clk_get(NULL, "sh4_clk");
	return clk_get_rate(sh4_clk) / (100 * 2);
}

#define ICACHE_TAG   0xF0000000
#define ICACHE_VAL   0xF1000000

static void __uses_jump_to_uncached load_text_to_icache(unsigned long start, unsigned long end)
{
	unsigned long flags, ccr;
	unsigned int iTemp,tmp;

	local_irq_save(flags);
	jump_to_uncached();

	/* Flush I-cache */
	ccr = ctrl_inl(CCR);
	ccr |= CCR_CACHE_ICI;
	ctrl_outl(ccr, CCR);

	ctrl_barrier();

	for ( iTemp = ( unsigned long ) start; iTemp < end; iTemp+=4 ) {
		tmp = * ( volatile unsigned int* ) (  ( iTemp ) );
		* ( volatile unsigned int* ) ( ICACHE_VAL + ( ( iTemp ) & 0x7fe0 ) + ( iTemp&0x1c ) ) =tmp;
		* ( volatile unsigned int* ) ( ICACHE_TAG + ( ( iTemp ) & 0x7fe0 ) ) = ( ( iTemp ) & 0x1ffffc00 )  | 0x40000001;
	}

	back_to_cached();
	local_irq_restore(flags);
}

static struct sh4_suspend_t *data;
static int sh4_suspend_enter(suspend_state_t state)
{
	unsigned long flags;
	unsigned long instr_tbl, instr_tbl_end;
#if defined(CONFIG_STM_LPFP) || defined(CONFIG_STM_LPIR)
	unsigned long addr, tmp;
#endif

	data->l_p_j = _10_ms_lpj();

	/* Must wait for serial buffers to clear */
	printk(KERN_INFO "sh4 is sleeping...\n");
	mdelay(500);

	flush_cache_all();

	local_irq_save(flags);
	if (data->lprtc_enter)
		data->lprtc_enter();

	/* sets the right instruction table */
	if (state == PM_SUSPEND_STANDBY) {
		instr_tbl     = data->stby_tbl;
		instr_tbl_end = data->stby_size;
	} else {
		instr_tbl     = data->mem_tbl;
		instr_tbl_end = data->mem_size;
	}

	BUG_ON(in_irq());

#if defined(CONFIG_STM_LPFP)
	/* load fp lowpower data into dcache */
	for (addr = 0; addr < ARRAY_SIZE(lpfp_data); addr += 8)
		tmp = *(volatile unsigned long *)&lpfp_data[addr];
#endif

#if defined(CONFIG_STM_LPIR)
	/* load ir lowpower data into dcache */
	for (addr = 0; addr < ARRAY_SIZE(lpir_data); addr += 8)
		tmp = *(volatile unsigned long *)&lpir_data[addr];
#endif

#if defined(CONFIG_STM_LPFP) || defined(CONFIG_STM_LPIR)
	for (addr = (unsigned long)&__text_lpm_start;
	     addr < (unsigned long)&__text_lpm_end; addr += L1_CACHE_BYTES)
//		tmp = *(volatile unsigned long *)addr;
		prefetch((void *)addr);

	load_text_to_icache((unsigned long)&__text_lpm_start, (unsigned long)&__text_lpm_end);
#endif

	wokenup_by = sh4_suspend(data, instr_tbl, instr_tbl_end);

/*
 *  without the evt_to_irq function the INTEVT is returned
 */
	if (data->evt_to_irq)
		wokenup_by = data->evt_to_irq(wokenup_by);

	BUG_ON(in_irq());

	local_irq_restore(flags);
	if (data->lprtc_exit)
		data->lprtc_exit();

	printk(KERN_INFO "sh4 woken up by: 0x%x\n", wokenup_by);

    return 0;
}

static int sh4_suspend_finish(suspend_state_t state)
{
	printk(KERN_INFO "sh4 suspend finish\n");
	register_write32(ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET,
			ST7105_PTI_BASE_ADDRESS);

	register_write32(ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET + 4,
			wokenup_by);

	kernel_restart(NULL);
	return 0;
}

static ssize_t power_wokenupby_show(struct kset *subsys, char *buf)
{
	return sprintf(buf, "%d\n", wokenup_by);
}

static ssize_t power_wokenupby_set(struct kset *subsys, const char *buf, size_t count)
{
	ssize_t value = simple_strtoul(buf, NULL, 10);
	switch (value) {
	case WAKEUP_SRC_TIMER:
	case WAKEUP_SRC_FP_POWER:
	case WAKEUP_SRC_FP_PROG:
	case WAKEUP_SRC_IR:
		wokenup_by = value;
		break;
	default:
		wokenup_by = 0;
	}

	register_write32(ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET,
			ST7105_PTI_BASE_ADDRESS);

	register_write32(ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET + 4,
			wokenup_by);

	return count;
}

static struct subsys_attribute wokenup_by_attr =
__ATTR(wokenup-by, S_IRUGO | S_IWUSR, power_wokenupby_show, power_wokenupby_set);

static int sh4_suspend_valid_both(suspend_state_t state)
{
	return 1;
}

int __init sh4_suspend_register(struct sh4_suspend_t *pdata)
{
	int dummy;

	if (!pdata)
		return -EINVAL;
	data = pdata;
	data->ops.enter = sh4_suspend_enter;
	if (data->stby_tbl && data->stby_size)
		data->ops.valid = sh4_suspend_valid_both;
	else
		data->ops.valid = pm_valid_only_mem;

	data->ops.finish = sh4_suspend_finish;

	pm_set_ops(&data->ops);

	dummy = subsys_create_file(&power_subsys, &wokenup_by_attr);

	printk(KERN_INFO "sh4 suspend support registered\n");

	return 0;
}
