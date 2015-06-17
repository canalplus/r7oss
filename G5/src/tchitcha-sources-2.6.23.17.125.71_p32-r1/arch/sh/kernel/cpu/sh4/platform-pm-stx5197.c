/*
 * Platform PM Capability - STx5197
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/stm/pm.h>
#include <linux/delay.h>
#ifdef CONFIG_PM
static int
usb_pwr_ack(struct platform_device *dev, int host_phy, int on)
{
	static struct sysconf_field *sc;
	int i;
	if (!sc)
		sc = sysconf_claim(CFG_MONITOR_E, 30, 30, "USB");
	for (i = 5; i; --i) {
		if (sysconf_read(sc) == on)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static int
usb_pwr_dwn(struct platform_device *dev, int host_phy, int pwd)
{
	static struct sysconf_field *sc;

	/* Power on USB */
	if (!sc) {
		sc = sysconf_claim(CFG_CTRL_H, 8, 8, "USB");
	}

	sysconf_write(sc, (pwd ? 1 : 0));

	return 0;
}

static int
emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(CFG_CTRL_I, 31, 31, "emi pwd req");

	sysconf_write(sc, (dwn ? 1 : 0));
	return 0;
}

static int
emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;
	if (!sc)
		sc = sysconf_claim(CFG_MONITOR_J, 20, 20, "emi pwr ack");
	for (i = 5; i; --i) {
		if (sysconf_read(sc) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}


static struct platform_device_pm stx5197_pm_devices[] = {
pm_plat_dev(&emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&st_usb, NULL, usb_pwr_dwn, usb_pwr_ack, NULL),
};
#endif
