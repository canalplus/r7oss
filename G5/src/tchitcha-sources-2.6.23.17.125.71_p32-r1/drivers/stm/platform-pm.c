/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/platform-pm.c
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
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/platform_device.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm-generic/bug.h>
#include <linux/stm/pm.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)			\
		printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif


/*
 * More ST devices have extra capability rooted in the
 * sysconf registers (i.e.: EMI, SATA, USB can be turn-off
 * via sysconf register).
 * Each SOC use different pio and sysconf therefore the
 * platform-pm tries to link the generic device driver
 * to the SOCs specific implementation using a
 * predefeined API:
 *
 * - platform_pm_init
 * - platform_pm_pwdn_req
 * - platform_pm_pwdn_ack
 * - platform_pm_reset
 *
 * this API allow us to write generic dvice driver able to
 * use platform specific capability (if supported).
 */
enum {
	RAW_INIT = 0,
	RAW_PWD_REQ,
	RAW_PWD_ACK,
	RAW_RESET,
};

static struct platform_device_pm *pm_devices;
static unsigned long pm_devices_size;

static struct platform_device_pm
*platform_pm_idx(struct platform_device *owner)
{
	int i;
	if (!pm_devices || !pm_devices_size)
		return NULL;

	for (i = 0; i < pm_devices_size; ++i)
		if (pm_devices[i].owner == owner)
			return &(pm_devices[i]);
	return NULL;
}

static int _platform_pm(int id_operation, void *owner,
	int host_phy, int data)
{
	struct platform_device_pm *pm_info;
	int (*fns)(struct platform_device *pdev, int host_phy, int data);
	long *pfns;
	pm_info = platform_pm_idx(owner);
	if (!pm_info) /* no pm capability for this device */
		return 0;
	pfns = (long *)pm_info;
	fns = (void *)pfns[id_operation];
	if (!fns) /* no pm call back */
		return 0;
	return fns((struct platform_device *)owner, host_phy, data);
}

int platform_pm_init(struct platform_device *pdev, int phy)
{
	return _platform_pm(RAW_INIT, (void *)pdev, phy, 0);
}
EXPORT_SYMBOL(platform_pm_init);

int platform_pm_pwdn_req(struct platform_device *pdev, int phy, int pwd)
{
	return _platform_pm(RAW_PWD_REQ, (void *)pdev, phy, pwd);
}
EXPORT_SYMBOL(platform_pm_pwdn_req);

int platform_pm_pwdn_ack(struct platform_device *pdev, int phy, int ack)
{
	return _platform_pm(RAW_PWD_ACK, (void *)pdev, phy, ack);
}
EXPORT_SYMBOL(platform_pm_pwdn_ack);

int platform_pm_reset(struct platform_device *pdev, int phy)
{
	return _platform_pm(RAW_RESET, (void *)pdev, 0, 0);
}
EXPORT_SYMBOL(platform_pm_reset);

int platform_add_pm_devices(struct platform_device_pm *pm, unsigned long size)
{
	if (!pm || !size)
		return -1;
	dbg_print("\n");
	pm_devices = pm;
	pm_devices_size = size;
	return 0;
}
