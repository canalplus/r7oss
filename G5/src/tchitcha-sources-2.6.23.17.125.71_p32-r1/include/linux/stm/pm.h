/*
 * -------------------------------------------------------------------------
 * <linux_root>/include/linux/stm/pm.h
 * -------------------------------------------------------------------------
 * STMicroelectronics
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __pm_st_h__
#define __pm_st_h__

#include <linux/pm.h>

/*
 * Several devices (i.e.: USB-SATA-PCI) have extra power capability
 * based on sysconf register and pio
 * the following struct should link the generic device driver
 * to the platform specific power capability implementation
 */
struct platform_device;
#define HOST_PM				0x1
#define PHY_PM				0x2
struct platform_device_pm {
	int (*pwdn_init)(struct platform_device *pdev, int host_phy);
	int (*pwdn_req)(struct platform_device *pdev, int host_phy, int down);
	int (*pwdn_ack)(struct platform_device *pdev, int host_phy, int ack);
	int (*reset)(struct platform_device *pdev, int host_phy);
/* pwdn_init:	to register/request pio/sysconf */
/* pwdn_req:	to raise a power down request */
/* pwdn_ack: 	to check the status of pwdn request */
/* reset:	to reset the device (i.e: USB) */
	struct platform_device *owner;
/* owner is (most of the time) the platform_device but
 * unfortunatelly some device (i.e.: EMI) has no
 * platform device... In this case I will do a serch by name
 */
};

/*
 * platform_pm_ function
 *
 * to enable/disable power down
 * @pdev: the device
 * @phy: also the phy is involved in pm activity?
 * @data: what we require
 */
#ifdef CONFIG_PM
int platform_pm_init(struct platform_device *pdev, int phy);
int platform_pm_pwdn_req(struct platform_device *pdev, int phy, int pwd);
int platform_pm_pwdn_ack(struct platform_device *pdev, int phy, int ack);
int platform_pm_reset(struct platform_device *pdev, int phy);

int platform_add_pm_devices(struct platform_device_pm *pm, unsigned long size);

#define pm_plat_dev(_pdev, _p_init, _p_req, _p_ack, _p_reset)	\
{								\
	.owner = (void *)_pdev,					\
	.pwdn_init = _p_init,					\
	.pwdn_req = _p_req,					\
	.pwdn_ack = _p_ack,					\
	.reset = _p_reset,					\
}

#else
#define platform_pm_init(pdev, phy)		do { } while (0)
#define platform_pm_pwdn_req(pdev, phy, pwd)	do { } while (0)
#define platform_pm_pwdn_ack(pdev, phy, ack)	do { } while (0)
#define platform_pm_reset(pdev, phy)		do { } while (0)
#define platform_add_pm_devices(pm, size)	do { } while (0)
#endif

#endif
