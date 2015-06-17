/*
 * Platform PM Capability Freeman 510 (FLI7510)
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
		sc = sysconf_claim(CFG_PWR_DWN_CTL, 0, 0,
				"global_emipci_pwrdwn_req");

	sysconf_write(sc, (dwn ? 1 : 0));

	return 0;
}

static int
emi_pwr_dwn_ack(struct platform_device *dev, int host_phy, int ack)
{
	static struct sysconf_field *sc;
	int i;

	if (!sc)
		sc = sysconf_claim(CFG_PCI_ROPC_STATUS, 16, 16,
				"status_emipciss_global_pwrdwn_ack");

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
	static struct sysconf_field *sc;

	if (!sc)
		sc = sysconf_claim(CFG_COMMS_CONFIG_1, 8, 8,
				"usb_powerdown_req");

	sysconf_write(sc, (dwn ? 1 : 0));

	return 0;
}

static struct platform_device_pm stx7105_pm_devices[] = {
	pm_plat_name("emi", NULL, emi_pwr_dwn_req, emi_pwr_dwn_ack, NULL),
	pm_plat_dev(&fli7510_usb_device, NULL, usb_pwr_req, NULL, NULL),
};
#endif
