/*******************************************************************************
  FPIF Ethernet Driver -- MDIO bus implementation
  Provides Bus interface for MII registers

  Copyright (C) 2012-2014 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: manish rathi <manish.rathi@st.com>
*******************************************************************************/

#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/io.h>

#include "stmfp_main.h"

#define MII_DONE 0x40000000
#define MII_READ 1
#define MII_WRITE 0
#define MII_RW_SHIFT 31
#define MII_PHYADDR_SHIFT 21
#define MII_REGADDR_SHIFT 16
#define MII_REGDATA_SHIFT 0

static int fpif_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg);
static int fpif_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			   u16 phydata);
static int fpif_mdio_reset(struct mii_bus *bus);

/**
 * fpif_mdio_read
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * Description: it reads data from the MII register from within the phy device.
 */
static int fpif_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct fpif_priv *priv = netdev_priv(ndev);
	u32 value;

	value =
	    (MII_READ << MII_RW_SHIFT) | (phyaddr << MII_PHYADDR_SHIFT) |
	    (phyreg << MII_REGADDR_SHIFT);
	writel(value, priv->fpgrp->base + FP_MDIO_CTL1);
	fpif_wait_till_done(priv);
	value = readl(priv->fpgrp->base + FP_MDIO_CTL1) & 0xffff;

	return value;
}

int fpif_wait_till_done(struct fpif_priv *priv)
{
	unsigned long curr;
	unsigned long finish = jiffies + 3 * HZ;

	do {
		curr = jiffies;
		if (readl(priv->fpgrp->base + FP_MDIO_CTL1) & MII_DONE)
			return 0;
		else
			cpu_relax();
	} while (!time_after_eq(curr, finish));
	return -EBUSY;
}

/**
 * fpif_mdio_write
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 29-26
 * @phyreg: MII addr reg bits 20-16
 * @phydata: phy data
 * Description: it writes the data into the MII register from within the device.
 */
static int fpif_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg,
			   u16 phydata)
{
	struct net_device *ndev = bus->priv;
	struct fpif_priv *priv = netdev_priv(ndev);
	u32 value;
	int ret;

	value = (MII_WRITE << MII_RW_SHIFT) | (phyaddr << MII_PHYADDR_SHIFT)
	    | (phyreg << MII_REGADDR_SHIFT) | (phydata << MII_REGDATA_SHIFT);
	writel(value, priv->fpgrp->base + FP_MDIO_CTL1);
	/* Wait until any existing MII operation is complete */
	ret = fpif_wait_till_done(priv);
	return ret;
}

/**
 * fpif_mdio_reset
 * @bus: points to the mii_bus structure
 * Description: reset the MII bus
 */
static int fpif_mdio_reset(struct mii_bus *bus)
{
	mdelay(10);

	return 0;
}

/**
 * fpif_mdio_register
 * @ndev: net device structure
 * Description: it registers the MII bus
 */
int fpif_mdio_register(struct net_device *ndev)
{
	int err = 0;
	struct mii_bus *new_bus;
	struct fpif_priv *priv = netdev_priv(ndev);
	int addr, found;

	new_bus = mdiobus_alloc();
	if (new_bus == NULL)
		return -ENOMEM;

	new_bus->name = "FPIF MII Bus";
	new_bus->read = &fpif_mdio_read;
	new_bus->write = &fpif_mdio_write;
	new_bus->reset = &fpif_mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%x", FP_PHY_BUS_ID);
	new_bus->priv = ndev;
	new_bus->phy_mask = FP_PHY_MASK;
	new_bus->parent = priv->dev;
	err = mdiobus_register(new_bus);
	if (err != 0) {
		pr_err("%s: Cannot register as MDIO bus\n", new_bus->name);
		goto bus_register_fail;
	}

	priv->mii = new_bus;

	found = 0;
	new_bus->irq = kmalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (new_bus->irq == NULL)
		goto bus_register_fail;

	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		struct phy_device *phydev = new_bus->phy_map[addr];
		if (phydev) {
			char irq_num[4];
			char *irq_str;
			/*
			 * If an IRQ was provided to be assigned after
			 * the bus probe, do it here.
			 */
			new_bus->irq[addr] = PHY_POLL;
			phydev->irq = PHY_POLL;
			switch (phydev->irq) {
			case PHY_POLL:
				irq_str = "POLL";
				break;
			case PHY_IGNORE_INTERRUPT:
				irq_str = "IGNORE";
				break;
			default:
				sprintf(irq_num, "%d", phydev->irq);
				irq_str = irq_num;
				break;
			}
			found = 1;
		}
	}

	if (!found)
		pr_warn("%s: No PHY found\n", ndev->name);

	return 0;

bus_register_fail:
	mdiobus_free(new_bus);
	return err;
}

/**
 * fpif_mdio_unregister
 * @ndev: net device structure
 * Description: it unregisters the MII bus
 */
int fpif_mdio_unregister(struct net_device *ndev)
{
	struct fpif_priv *priv = netdev_priv(ndev);
	mdiobus_unregister(priv->mii);
	priv->mii->priv = NULL;
	mdiobus_free(priv->mii);
	priv->mii = NULL;

	return 0;
}
