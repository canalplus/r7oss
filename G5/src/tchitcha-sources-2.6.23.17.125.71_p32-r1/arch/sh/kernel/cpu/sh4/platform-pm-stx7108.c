/*
 * Platform PM Capability STx7108
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
static int
emi_pwr_dwn_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG_BANK2, 30, 0, 0, "EMI - PM");

	sysconf_write(sc, (dwn ? 1 : 0));
	return 0;
}

static int
emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;
	if (!sc)
		sc = sysconf_claim(SYS_STA_BANK2, 1, 0, 0, "EMI - PM");
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
	static struct sysconf_field *sc[3];
	int port = dev->id;
	if (!sc[port])
		sc[port] = sysconf_claim(SYS_CFG_BANK4, 46,
			port, port, "USB - PM");

	sysconf_write(sc[port], (dwn ? 1 : 0));
	return 0;
}

static int
usb_pwr_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf *sc[4];
	int port = dev->id;
	int i;

	if (!sc[port])
		sc[port] = sysconf_claim(SYS_STA_BANK4, 2,
			port, port, "USB - PM");

	for (i = 3; i; --i) {
		if (sysconf_read(sc[port]) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}
/*
 * An USB per port reset is possible using the Bank0_Conf_13
 */

static int
sata_pwr_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf *sc[2];
	int port = dev->id;

	if (!sc[port])
		sc[port] = sysconf_claim(SYS_CFG_BANK4, 46,
				port + 3, port + 3, "SATA - PM");
	sysconf_write(sc[port], (dwn ? 1 : 0));

	return 0;
}

static int
sata_pwr_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf *sc[2];
	int i, port = dev->id;

	if (!sc[port])
		sc[port] = sysconf_claim(SYS_STA_BANK4, 2,
				port + 3, port + 3, "SATA - PM");
	for (i = 3; i; --i) {
		if (sysconf_read(sc[port]) == ack)
			return 0;
		mdelay(10);
	}
	return 0;
}


static struct platform_device_pm stx7108_pm_devices[] = {
pm_plat_dev(&stx7108_emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&stx7108_usb_devices[0], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&stx7108_usb_devices[1], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&stx7108_usb_devices[2], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&stx7108_sata_devices[0], NULL, sata_pwr_req, sata_pwr_ack, NULL),
pm_plat_dev(&stx7108_sata_devices[0], NULL, sata_pwr_req, sata_pwr_ack, NULL),
};
#endif
