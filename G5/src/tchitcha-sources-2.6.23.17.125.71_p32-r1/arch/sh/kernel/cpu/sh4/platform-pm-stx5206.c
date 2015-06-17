/*
 * Platform PM Capability STx5206
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/stm/pm.h>
#ifdef CONFIG_PM
static int emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;

	if (!sc)
		sc = sysconf_claim(SYS_CFG, 32, 1, 1, "emi pwr req");

	sysconf_write(sc, dwn ? 1 : 0);

	return 0;
}

static int emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;

	if (!sc)
		sc = sysconf_claim(SYS_STA, 15, 1, 1, "emi pwr ack");

	for (i = 5; i; --i) {
		if (sysconf_read(sc) == ack)
			return 0;
		mdelay(10);
	}

	return -EINVAL;
}

static int usb_pwr_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc, *sc_phy;

	if (!sc)
		/* usb_power_down_req: power down request for USB Host module */
		sc = sysconf_claim(SYS_CFG, 32, 4, 4, "USB");

	if (!sc_phy)
		/* suspend_from_config: Signal to suspend USB PHY */
		sc_phy = sysconf_claim(SYS_CFG, 10, 5, 5, "USB");

	sysconf_write(sc, dwn ? 1 : 0);
	sysconf_write(sc_phy, dwn ? 1 : 0);

	return 0;
}

static int usb_pwr_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;

	if (!sc)
		sc = sysconf_claim(SYS_STA, 15, 4, 4, "USB");

	for (i = 5; i; --i) {
		if (sysconf_read(sc) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static struct platform_device_pm stx5206_pm_devices[] = {
pm_plat_dev(&stx5206_emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&stx5206_usb_device, NULL, usb_pwr_req, usb_pwr_ack, NULL),
};

#endif
