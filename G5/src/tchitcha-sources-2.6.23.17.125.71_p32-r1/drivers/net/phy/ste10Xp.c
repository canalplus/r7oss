/* ============================================================================
 * This is a driver for the STe101p and STe100p PHY controllers.
 *
 * Copyright (C) 2006 by Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * ----------------------------------------------------------------------------
 * Changelog:
 *   March 2008
 *      Added suspend/resume functions.
 *   May 2007
 *      Changed 101p PHY ID and mask to allow REV B variants
 *   Aug  2006
 *	Converted PHY driver to new 2.6.17 PHY device
 *	Added STe100p support
 *	Carl Shaw <carl.shaw@st.com>
 *   July 2006
 *	- Minor fixes.
 *   May 2006
 *	- It's indipendend from the STb7109eth driver.
 *   April 2006 (first release of the driver):
 *	- Added the RMII interface support.
 * 	- The driver has been tested, on the MB411 platform with the DB666
 * 	  daughter board, using:
 *		- the clock through the STPIO3[7],
 *		- the clock through an external clock generator device.
 *	- Added the "fix_mac_speed" function: it is used for changing
 *	  the MAC speed field in the SYS_CFG7 register.
 *	  (required when we are using the RMII interface).
 *
 * ----------------------------------------------------------------------------
 * Known bugs:
 * 	The SMII mode is not supported yet.
 * ===========================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>

#undef PHYDEBUG
#define DEFAULT_PHY_ID  0
#define RESOURCE_NAME	"ste10Xp"

#define MII_XCIIS   	0x11 /* Configuration Info IRQ & Status Reg*/
#define MII_XIE     	0x12 /* Interrupt Enable Register*/
#define MII_XIE_DEFAULT_MASK 0x0070 /* ANE complete, Remote Fault, Link Down */

/* STE101P phy identifier values */
#define STE101P_PHY_ID		0x00061c50

/* STe100P phy identifier values */
#define STE100P_PHY_ID       	0x1c040011

static int ste10Xp_config_init(struct phy_device *phydev)
{
	int value, err;

	/* Software Reset PHY */
	value = phy_read(phydev, MII_BMCR);
	if (value < 0)
		return value;

	value |= BMCR_RESET;
	err = phy_write(phydev, MII_BMCR, value);
	if (err < 0)
		return err;

	do{
		value = phy_read(phydev, MII_BMCR);
	} while (value & BMCR_RESET);

	return 0;
}

static int ste10Xp_config_intr(struct phy_device *phydev)
{
        int err, value;

        if(phydev->interrupts == PHY_INTERRUPT_ENABLED){
		/* Enable all STe101P interrupts (PR12) */
		err = phy_write(phydev, MII_XIE, MII_XIE_DEFAULT_MASK);
		/* clear any pending interrupts */
		if (err == 0){
			value = phy_read(phydev, MII_XCIIS);
			if (value <0){
				err = value;
			}
		}
	} else
		err = phy_write(phydev, MII_XIE, 0);

        return err;
}

static int ste10Xp_ack_interrupt(struct phy_device *phydev)
{
        int err = phy_read(phydev, MII_XCIIS);
        if (err < 0)
                return err;

        return 0;
}

static struct phy_driver ste101p_pdriver = {
        .phy_id         = STE101P_PHY_ID,
        .phy_id_mask    = 0xfffffff0,
        .name           = "STe101p",
        .features       = PHY_BASIC_FEATURES | SUPPORTED_Pause,
        .flags          = PHY_HAS_INTERRUPT,
	.config_init    = ste10Xp_config_init,
        .config_aneg    = genphy_config_aneg,
        .read_status    = genphy_read_status,
        .ack_interrupt  = ste10Xp_ack_interrupt,
        .config_intr    = ste10Xp_config_intr,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver         = { .owner = THIS_MODULE, }
};

static struct phy_driver ste100p_pdriver = {
        .phy_id         = STE100P_PHY_ID,
        .phy_id_mask    = 0xffffffff,
        .name           = "STe100p",
        .features       = PHY_BASIC_FEATURES | SUPPORTED_Pause,
        .flags          = PHY_HAS_INTERRUPT,
	.config_init    = ste10Xp_config_init,
        .config_aneg    = genphy_config_aneg,
        .read_status    = genphy_read_status,
        .ack_interrupt  = ste10Xp_ack_interrupt,
        .config_intr    = ste10Xp_config_intr,
	.suspend	= genphy_suspend,
	.resume 	= genphy_resume,
	.driver         = { .owner = THIS_MODULE, }
};

static int __init ste10Xp_init(void)
{
	int retval;

	retval = phy_driver_register(&ste100p_pdriver);
	if (retval < 0)
		return retval;
	return phy_driver_register(&ste101p_pdriver);
}

static void __exit ste10Xp_exit(void)
{
	phy_driver_unregister(&ste100p_pdriver);
	phy_driver_unregister(&ste101p_pdriver);
}

module_init(ste10Xp_init);
module_exit(ste10Xp_exit);

MODULE_DESCRIPTION("STe10Xp PHY driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
