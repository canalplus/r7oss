/*
 * Driver for Realtek RTL8201CP PHY
 *
 * (C) Copyright 2008 WyPlay SAS.
 * Frederic Mazuel <fmazuel@wyplay.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#define RTL8201CP_ID		0x8201

MODULE_DESCRIPTION("Realtek RTL8201CP PHY driver");
MODULE_AUTHOR("Frederic Mazuel <fmazuel@wyplay.com>");
MODULE_LICENSE("GPL");

static int rtl8201cp_config_init(struct phy_device *phydev) 
{
	int value, err;
	int timeout = 1000;

	/* Software Reset PHY */
	value = phy_read(phydev, MII_BMCR);
	if (value < 0)
		return value;

	value |= BMCR_RESET;
	err = phy_write(phydev, MII_BMCR, value);
	if (err < 0)
		return err;

	do {
		mdelay(1);
		value = phy_read(phydev, MII_BMCR);
	} while ((value & BMCR_RESET) && (--timeout));

	if (!timeout) {
		printk(KERN_ERR "RTL8201CP PHY timed out during reset!\n");
		return -1;
	}

	return 0;
}

static struct phy_driver rtl8201cp_driver = {
	.phy_id		= RTL8201CP_ID,
	.phy_id_mask	= 0xffffffff,
	.name		= "rtl8201cp",
	.features	= PHY_BASIC_FEATURES,
	.config_init	= rtl8201cp_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver 	= { .owner = THIS_MODULE, },
};

static int __init rtl8201cp_init(void)
{
	int ret;

	ret = phy_driver_register(&rtl8201cp_driver);
	if (ret) {
		phy_driver_unregister(&rtl8201cp_driver);
		printk(KERN_INFO "rtl8201cp_init KO");
		return ret;
	}
	return 0;
}

static void __exit rtl8201cp_exit(void)
{
	phy_driver_unregister(&rtl8201cp_driver);
}

module_init(rtl8201cp_init);
module_exit(rtl8201cp_exit);
