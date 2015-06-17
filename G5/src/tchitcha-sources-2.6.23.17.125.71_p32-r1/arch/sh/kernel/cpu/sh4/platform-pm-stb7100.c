/*
 * platform Pm capability - STb710x
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/stm/pm.h>

#ifdef CONFIG_PM
static int
usb_pm_pwd_dwn(struct platform_device *dev, int host_phy, int pwd)
{
	static struct sysconf_field *sc;

	if(!sc)
		sc = sysconf_claim(SYS_CFG, 2, 4, 5, "usb rx/tx");

	sysconf_write(sc, (pwd ? 0 : 3));
	return 0;
}

static int
usb_pm_pwd_ack(struct platform_device *dev, int host_phy, int ack)
{
	/* It isn't clear what the SysSTA_0 does... */
	mdelay(10);
	return 0;
}

static int
usb_pm_sw_reset(struct platform_device *dev)
{
	static struct sysconf_field *sc;
	unsigned long reg;

	if (!sc)
		sc = sysconf_claim(SYS_CFG, 2, 1, 1, "usb reset");
	reg = sysconf_read(sc);
	if (reg) {
		sysconf_write(sc, 0);
		mdelay(30);
	}
	return 0;
}

/*
 * The EMI sysconf capabilities seems not working!
 */
static int
emi_pwd_dwn_req(struct platform_device *pdev, int host_phy, int down)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 32, 1, 1, "emi pwr req");

/*	sysconf_write(sc, (down ? 1 : 0));*/

	return 0;
}

static int
emi_pwd_dwn_ack(struct platform_device *pdev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;

	if (!sc)
		sc = sysconf_claim(SYS_STA, 15, 0, 0, "emi pwr ack");
	for (i = 5; i; --i) {
		if (sysconf_read(sc) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static struct platform_device_pm stx7100_pm_devices[] = {
pm_plat_dev(&st_usb_device, NULL, usb_pm_pwd_dwn, usb_pm_pwd_ack,
	usb_pm_sw_reset),
pm_plat_dev(&emi, NULL, emi_pwd_dwn_req, emi_pwd_dwn_ack, NULL),
};

#endif
