/*
 * Platform PM Capability - STx7111
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
usb_pwr_ack(struct platform_device *dev, int host_phy, int on)
{
	static struct sysconf_field *sc;
	int i;

	if (!sc)
		sc = sysconf_claim(SYS_STA, 15, 4, 4, "USB_PW_ACK");
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
	static struct sysconf_field *sc, *sc_clk;

	/* Power on USB */
	if (!sc) {
		sc = sysconf_claim(SYS_CFG, 32, 4, 4, "USB_PW_REQ");
		sc_clk = sysconf_claim(SYS_CFG, 40, 2, 3, "usb_clk");
	}

	sysconf_write(sc, (pwd ? 1 : 0));
	sysconf_write(sc_clk, (pwd ? 3 : 0));

	return 0;
}

static int
usb_sw_reset(struct platform_device *dev, int host_phy)
{
	static struct sysconf_field *sc;

	/* Reset USB */
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 4, 4, 4, "USB_RST");
	sysconf_write(sc, 0);
	mdelay(10);
	sysconf_write(sc, 1);
	mdelay(10);

	return 0;
}


static int
emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 32, 1, 1, "emi pwr");

	sysconf_write(sc, (dwn ? 1 : 0));
	return 0;
}

static int
emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
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


static struct platform_device_pm stx7111_pm_devices[] = {
pm_plat_dev(&emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&st_usb, NULL, usb_pwr_dwn, usb_pwr_ack, usb_sw_reset),
};
#endif
