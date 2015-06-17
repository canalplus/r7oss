/*
 * Platform PM Capability - STx7141
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
usb_pwr_req(struct platform_device *dev, int host_phy, int dwn)
{
	static struct sysconf *sc[4];
	int port = dev->id;

	if (!sc[port])
		sc[port] = sysconf_claim(SYS_CFG, 32, 7+port, 7+port, "USB");
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
		sc[port] = sysconf_claim(SYS_STA, 15, 7+port, 7+port, "USB");

	for (i = 5; i; --i) {
                if (sysconf_read(sc[port]) == ack)
                        return 0;
                mdelay(10);
        }
	return -EINVAL;
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

static struct platform_device_pm stx7141_pm_devices[] = {
pm_plat_dev(&emi, NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
pm_plat_dev(&st_usb_device[0], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&st_usb_device[1], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&st_usb_device[2], NULL, usb_pwr_req, usb_pwr_ack, NULL),
pm_plat_dev(&st_usb_device[3], NULL, usb_pwr_req, usb_pwr_ack, NULL),
};
#endif
