/*
 * drivers/net/stmmac/stmmac_mdio.c
 *
 * STMMAC Ethernet Driver -- MDIO bus implementation
 * Provides Bus interface for MII registers
 *
 * Author: Carl Shaw <carl.shaw@st.com>
 * Maintainer: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * Copyright (c) 2006-2007 STMicroelectronics
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/phy.h>

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>

#include "stmmac.h"

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002

/**
 * stmmac_mdio_read
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * Description: it reads data from the MII register from within the phy device.
 * For the 7111 GMAC, we must set the bit 0 in the MII address register while
 * accessing the PHY registers.
 * Fortunately, it seems this has no drawback for the 7109 MAC.
 */
int stmmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = priv->mac_type->hw.mii.addr;
	unsigned int mii_data = priv->mac_type->hw.mii.data;

	int data;
	u16 regValue = (((phyaddr << 11) & (0x0000F800)) |
			((phyreg << 6) & (0x000007C0)));
	regValue |= MII_BUSY;	/* in case of GMAC */

	while (((readl(ioaddr + mii_address)) & MII_BUSY) == 1) {
	}

	writel(regValue, ioaddr + mii_address);

	while (((readl(ioaddr + mii_address)) & MII_BUSY) == 1) {
	}

	/* Read the data from the MII data register */
	data = (int)readl(ioaddr + mii_data);

	return data;
}

/**
 * stmmac_mdio_write
 * @bus: points to the mii_bus structure
 * @phyaddr: MII addr reg bits 15-11
 * @phyreg: MII addr reg bits 10-6
 * @phydata: phy data
 * Description: it writes the data into the MII register from within the device.
 */
int stmmac_mdio_write(struct mii_bus *bus, int phyaddr, int phyreg, u16 phydata)
{
	struct net_device *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = priv->mac_type->hw.mii.addr;
	unsigned int mii_data = priv->mac_type->hw.mii.data;

	u16 value =
	    (((phyaddr << 11) & (0x0000F800)) | ((phyreg << 6) & (0x000007C0)))
	    | MII_WRITE;

	value |= MII_BUSY;

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + mii_address)) & MII_BUSY) == 1) {
	}

	/* Set the MII address register to write */
	writel(phydata, ioaddr + mii_data);
	writel(value, ioaddr + mii_address);

	/* Wait until any existing MII operation is complete */
	while (((readl(ioaddr + mii_address)) & MII_BUSY) == 1) {
	}
	/* This "extra" read was added, in the past, to fix an
	* issue related to the control MII bus specific operation (MDC/MDIO).
	* It forced the close operation of the message on the bus (hw hack
	* was to add a specific pull-up on one of the two MCD/MDIO lines).
	* It can be removed because no new board actually needs it.
	stmmac_mdio_read(bus, phyaddr, phyreg);
	*/
	return 0;
}

/* Resets the MII bus */
int stmmac_mdio_reset(struct mii_bus *bus)
{
	struct net_device *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	unsigned long ioaddr = ndev->base_addr;
	unsigned int mii_address = priv->mac_type->hw.mii.addr;

	printk(KERN_DEBUG "stmmac_mdio_reset: called!\n");

	if (priv->phy_reset) {
		printk(KERN_DEBUG "stmmac_mdio_reset: calling phy_reset\n");
		priv->phy_reset(priv->bsp_priv);
	}

	/* This is a workaround for problems with the STE101P PHY.
	 * It doesn't complete its reset until at least one clock cycle
	 * on MDC, so perform a dummy mdio read.
	 */
	writel(0, ioaddr + mii_address);

	return 0;
}

/**
 * stmmac_mdio_register
 * @ndev: net device structure
 * Description: it registers the MII bus
 */
int stmmac_mdio_register(struct net_device *ndev)
{
	int err = 0;
	struct mii_bus *new_bus = kzalloc(sizeof(struct mii_bus), GFP_KERNEL);
	int *irqlist = kzalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	struct stmmac_priv *priv = netdev_priv(ndev);
	int addr, found;

	if (new_bus == NULL)
		return -ENOMEM;

	/* Assign IRQ to phy at address phy_addr */
	if (priv->phy_addr != -1)
		irqlist[priv->phy_addr] = priv->phy_irq;

	new_bus->name = "STMMAC MII Bus";
	new_bus->read = &stmmac_mdio_read;
	new_bus->write = &stmmac_mdio_write;
	new_bus->reset = &stmmac_mdio_reset;
	new_bus->id = (int)priv->bus_id;
	new_bus->priv = ndev;
	new_bus->irq = irqlist;
	new_bus->phy_mask = priv->phy_mask;
	new_bus->dev = priv->device;
	printk(KERN_DEBUG "calling mdiobus_register\n");
	err = mdiobus_register(new_bus);
	printk(KERN_DEBUG "calling mdiobus_register done\n");
	if (err != 0) {
		printk(KERN_ERR "%s: Cannot register as MDIO bus\n",
		       new_bus->name);
		goto bus_register_fail;
	}

	priv->mii = new_bus;

	found = 0;
	for (addr = 0; addr < 32; addr++) {
		struct phy_device *phydev = new_bus->phy_map[addr];
		if (phydev) {
			if (priv->phy_addr == -1) {
				priv->phy_addr = addr;
				phydev->irq = priv->phy_irq;
				irqlist[addr] = priv->phy_irq;
			}
			printk(KERN_INFO
			       "%s: PHY ID %08x at %d IRQ %d (%s)%s\n",
			       ndev->name, phydev->phy_id, addr,
			       phydev->irq, phydev->dev.bus_id,
			       (addr == priv->phy_addr) ? " active" : "");
			found = 1;
		}
	}

	if (!found)
		printk(KERN_WARNING "%s: No PHY found\n", ndev->name);

	return 0;
bus_register_fail:
	kfree(new_bus);
	return err;
}

/**
 * stmmac_mdio_unregister
 * @ndev: net device structure
 * Description: it unregisters the MII bus
 */
int stmmac_mdio_unregister(struct net_device *ndev)
{
	struct stmmac_priv *priv = netdev_priv(ndev);

	mdiobus_unregister(priv->mii);
	priv->mii->priv = NULL;
	kfree(priv->mii);

	return 0;
}
