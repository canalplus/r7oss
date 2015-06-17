/*
 * Platform PM Capability STx7105
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
emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 32, 1, 1, "emi pwr req");

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

static int
usb_pwr_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc[2];
	static struct sysconf_field *sc_phy[2];
	int port = dev->id;
	if (!sc[port]) {
		sc_phy[port] = sysconf_claim(SYS_CFG, 32, 6+port, 6+port,
				"USB phy");
		sc[port] = sysconf_claim(SYS_CFG, 32, 4+port, 4+port, "USB");
	}

	sysconf_write(sc[port], (dwn ? 1 : 0));
	sysconf_write(sc_phy[port], (dwn ? 1 : 0));
	return 0;
}

static int
usb_pwr_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc[2];
	int port = dev->id;
	int i;
	if (!sc[port])
		sc[port] = sysconf_claim(SYS_STA, 15, 4 + port, 4 + port, "USB");

	for (i = 5; i; --i) {
		if (sysconf_read(sc[port]) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static struct platform_device_pm stx7105_pm_devices[] = {
pm_plat_dev(&stx7105_emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&stx7105_usb_devices[0], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&stx7105_usb_devices[1], NULL, usb_pwr_req, usb_pwr_ack, NULL),
/*
 * There should be also the SATA entry... but:
 * - on cut 1 they are broken ...
 * - on cut 2 there should be only one ...
 */
};
#endif
