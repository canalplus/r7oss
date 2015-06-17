/*
 * drivers/net/phy/marvell_m88e6161.c
 *
 * (C) Copyright 2009 WyPlay SAS.
 * Jean-Christophe PLAGNIOL-VILLARD <jcplagniol@wyplay.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>

MODULE_DESCRIPTION("Marvell 88E6161 Switch driver");
MODULE_AUTHOR("Jean-Christophe PLAGNIOL-VILLARD <jcplagniol@wyplay.com>");
MODULE_LICENSE("GPL");
#undef CONFIG_M88E6XXX_DEBUG

int m88e6xxx_mii_read (struct phy_device* phy, unsigned int portNumber ,
			unsigned int MIIReg)
{
	int err;

	phy->addr = portNumber;
	err = phy_read(phy, MIIReg);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("%s:%s:%d: portNumber=0x%x, MIIReg=0x%x, value=0x%x\n",
		__FILE__, __FUNCTION__, __LINE__, portNumber, MIIReg, err);
	if (err >= 0x0)
		printk("OK\n");
#endif
	return err;
}

int m88e6xxx_mii_write (struct phy_device* phy, unsigned int portNumber ,
			unsigned int MIIReg,unsigned int value)
{
	phy->addr = portNumber;
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("%s:%s:%d: portNumber=0x%x, MIIReg=0x%x, value=0x%x\n",
		__FILE__, __FUNCTION__, __LINE__, portNumber, MIIReg, value);
#endif
	return phy_write(phy, MIIReg,value);
}


static int m88e6161_read_status(struct phy_device *phydev)
{
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = phydev->asym_pause = 0;
	phydev->link = 1;
	phydev->state = PHY_RUNNING;
	phydev->irq = PHY_IGNORE_INTERRUPT;

	return 0;
}

static int m88e6161_config_init(struct phy_device *phydev)
{
	unsigned int value;
	int i;

	phydev->adjust_state = 0;

	printk ("Marvell switch 88E6%x found\n", 0xFFF & (phydev->phy_id >> 4));

	value = m88e6xxx_mii_read(phydev, 0x1c, 0x0);

	if ((value & 0xC800) == 0xC800)
		return 0;

	/* Enanle all Port to forwarding */
	m88e6xxx_mii_write(phydev, 0x1c, 0x19, 0x1140);
	for (i = 0; i < 4 ; i++) {
		m88e6xxx_mii_write(phydev, 0x1c, 0x18 , 0x9400 + (i << 5));
		udelay(500);
	}

	for (i = 0; i < 6 ; i++) {
		value = m88e6xxx_mii_read(phydev, 0x10 + i, 4);
		value |= 0x3;
		m88e6xxx_mii_write(phydev, 0x10 + i, 4 , value);
	}
	/* Special init P5 */
	m88e6xxx_mii_write(phydev, 0x15, 0x17, 0x8080);
	value = m88e6xxx_mii_read(phydev, 0x15, 0);
	value &= 0xfff8;
	value |= 0x2;
	m88e6xxx_mii_write(phydev, 0x15, 0, value);
	/* Forced Link up, Full duplex 100Mbps */
	value = m88e6xxx_mii_read(phydev, 0x15, 0x1);
	value &= 0xfffd;
	value |= 0x3d;
	m88e6xxx_mii_write(phydev, 0x15, 0x1, value);

	phydev->state = PHY_RUNNING;
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
	phydev->link = 1;
	phydev->pause = phydev->asym_pause = 0;
	netif_carrier_on(phydev->attached_dev);

	return 0;
}

static int m88e6161_config_aneg(struct phy_device *phydev)
{
	return 0;
}

static struct phy_driver m88e6161_driver = {
	.phy_id = 0x00001610,
	.phy_id_mask = 0x0000fff0,
	.name = "Marvell 88E6161",
	.features = PHY_BASIC_FEATURES,
	/* basic functions */
	.config_init = m88e6161_config_init,
	.config_aneg = m88e6161_config_aneg,
	.read_status = m88e6161_read_status,
	.driver = {
		.owner = THIS_MODULE,
	},
};

static struct phy_driver m88e6061_driver = {
	.phy_id = 0x00000610,
	.phy_id_mask = 0x0000fff0,
	.name = "Marvell 88E0161",
	.features = PHY_BASIC_FEATURES,
	/* basic functions */
	.config_init = m88e6161_config_init,
	.config_aneg = m88e6161_config_aneg,
	.read_status = m88e6161_read_status,
	.driver = {
		.owner = THIS_MODULE,
	},
};

static int __init m88e6161_init(void)
{
	int ret = phy_driver_register(&m88e6161_driver);

	if (ret)
		return ret;

	return phy_driver_register(&m88e6061_driver);
}

static void __exit m88e6161_exit(void)
{
	phy_driver_unregister(&m88e6161_driver);
	phy_driver_unregister(&m88e6061_driver);
}

module_init(m88e6161_init);
module_exit(m88e6161_exit);
