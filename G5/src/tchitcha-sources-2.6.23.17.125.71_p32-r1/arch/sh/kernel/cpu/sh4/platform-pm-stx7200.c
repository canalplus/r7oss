/*
 * Platform PM Capability - STx7200
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
usb_pwr_dwn(struct platform_device *pdev, int host_phy, int pwd)
{
	static struct sysconf_field *sc[3];
	int port = pdev->id;

	/* Power up port */
	if (!sc[port])
		sc[port] = sysconf_claim(SYS_CFG, 22, 3+port, 3+port,
				"usb pwr");
	sysconf_write(sc[port], (pwd ? 1 : 0));
	return 0;
}

static int
usb_pwr_ack(struct platform_device *pdev, int host_phy, int ack)
{
	static struct sysconf_field *sc[3];
	int port = pdev->id;
	int i;
	if (!sc[port])
		sc[port] = sysconf_claim(SYS_STA, 13, 2+port, 2+port,
					"usb ack");
	for (i = 5; i; --i) {
		if (sysconf_read(sc[port]) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static int
usb_sw_reset(struct platform_device *dev, int host_phy)
{
	/* it seems there is no reset on this platform... */
	return 0;
}

static int
emi_pwd_dwn_req(struct platform_device *pdev, int host_phy, int pwd)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(SYS_CFG, 32, 1, 1, "emi pwr req");

	sysconf_write(sc, (pwd ? 1 : 0));
	return 0;
}

static int
emi_pwd_dwn_ack(struct platform_device *pdev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;
	if (!sc)
		sc = sysconf_claim(SYS_STA, 8, 1, 1, "emi pwr ack");
	for (i = 5; i; --i) {
		if (sysconf_read(sc) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static int
sata_pwd_dwn_req(struct platform_device *pdev, int host_phy, int pwd)
{
	static struct sysconf_field *sc[2];
	if (cpu_data->cut_major < 3)
		return 0;

	if (!sc[pdev->id])
		sc[pdev->id] = sysconf_claim(SYS_CFG, 22,
			1 + pdev->id, 1 + pdev->id, "sata");
	sysconf_write(sc[pdev->id], (pwd ? 1 : 0));
	return 0;
}

static int
sata_pwd_dwn_ack(struct platform_device *pdev, int host_phy, int ack)
{
	static struct sysconf_field *sc[2];
	int i;
	if (cpu_data->cut_major < 3)
		return 0;
	if (!sc[pdev->id])
		sc[pdev->id] = sysconf_claim(SYS_STA, 13,
			0 + pdev->id, 0 + pdev->id, "sata");
	for (i = 5; i; --i) {
		if (sysconf_read(sc[pdev->id]) == ack)
			return 0;
		mdelay(10);
	}
	return -EINVAL;
}

static struct platform_device_pm stx7200_pm_devices[] = {
pm_plat_dev(&emi, NULL, emi_pwd_dwn_req, emi_pwd_dwn_ack, NULL),
pm_plat_dev(&st_usb[0], NULL, usb_pwr_dwn, usb_pwr_ack, usb_sw_reset),
pm_plat_dev(&st_usb[1], NULL, usb_pwr_dwn, usb_pwr_ack, usb_sw_reset),
pm_plat_dev(&st_usb[2], NULL, usb_pwr_dwn, usb_pwr_ack, usb_sw_reset),
pm_plat_dev(&sata_device[0], NULL, sata_pwd_dwn_req, sata_pwd_dwn_ack, NULL),
pm_plat_dev(&sata_device[1], NULL, sata_pwd_dwn_req, sata_pwd_dwn_ack, NULL),
};
#endif
