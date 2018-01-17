/*
 *
 * Copyright (c) 2002-2005 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *File Name  : bcmmii.c
 *
 *Description: Broadcom PHY/GPHY/Ethernet Switch Configuration
 *Revision:	09/25/2008, L.Sun created.
*/

#include "bcmgenet.h"

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/bitops.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/brcmphy.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>

/* read a value from the MII */
static int bcmgenet_mii_read(struct mii_bus *bus, int phy_id, int location)
{
	int ret;
	struct net_device *dev = bus->priv;
	struct bcmgenet_priv *priv = netdev_priv(dev);
	u32 reg;

	bcmgenet_umac_writel(priv, (MDIO_RD | (phy_id << MDIO_PMD_SHIFT) |
			(location << MDIO_REG_SHIFT)), UMAC_MDIO_CMD);
	/* Start MDIO transaction*/
	reg = bcmgenet_umac_readl(priv, UMAC_MDIO_CMD);
	reg |= MDIO_START_BUSY;
	bcmgenet_umac_writel(priv, reg, UMAC_MDIO_CMD);
	wait_event_timeout(priv->wq,
			!(bcmgenet_umac_readl(priv, UMAC_MDIO_CMD)
				& MDIO_START_BUSY),
			HZ/100);
	ret = bcmgenet_umac_readl(priv, UMAC_MDIO_CMD);

	/* Don't check error codes from switches, as some of them are
	 * known to return MDIO_READ_FAIL on good transactions
	 */
	if (!(bus->phy_ignore_ta_mask & 1 << phy_id) &&
	    (ret & MDIO_READ_FAIL)) {
		netif_dbg(priv, hw, dev, "MDIO read failure\n");
		ret = 0;
	}
	return ret & 0xffff;
}

/* write a value to the MII */
static int bcmgenet_mii_write(struct mii_bus *bus, int phy_id,
			int location, u16 val)
{
	struct net_device *dev = bus->priv;
	struct bcmgenet_priv *priv = netdev_priv(dev);
	u32 reg;

	bcmgenet_umac_writel(priv, (MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
			(location << MDIO_REG_SHIFT) | (0xffff & val)),
			UMAC_MDIO_CMD);
	reg = bcmgenet_umac_readl(priv, UMAC_MDIO_CMD);
	reg |= MDIO_START_BUSY;
	bcmgenet_umac_writel(priv, reg, UMAC_MDIO_CMD);
	wait_event_timeout(priv->wq,
			!(bcmgenet_umac_readl(priv, UMAC_MDIO_CMD) &
				MDIO_START_BUSY),
			HZ/100);

	return 0;
}

/* setup netdev link state when PHY link status change and
 * update UMAC and RGMII block when link up
 */
void bcmgenet_mii_setup(struct net_device *dev)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;
	u32 reg, cmd_bits = 0;
	bool status_changed = false;

	if (priv->old_link != phydev->link) {
		status_changed = 1;
		priv->old_link = phydev->link;
	}

	if (phydev->link) {
		/* check speed/duplex/pause changes */
		if (priv->old_speed != phydev->speed) {
			status_changed = true;
			priv->old_speed = phydev->speed;
		}

		if (priv->old_duplex != phydev->duplex) {
			status_changed = true;
			priv->old_duplex = phydev->duplex;
		}

		if (priv->old_pause != phydev->pause) {
			status_changed = true;
			priv->old_pause = phydev->pause;
		}

		/* done if nothing has changed */
		if (!status_changed)
			return;

		/* program UMAC and RGMII block based on established link
		 * speed, pause, and duplex.
		 * the speed set in umac->cmd tell RGMII block which clock
		 * 25MHz(100Mbps)/125MHz(1Gbps) to use for transmit.
		 * receive clock is provided by PHY.
		 */
		reg = bcmgenet_ext_readl(priv, EXT_RGMII_OOB_CTRL);
		reg &= ~OOB_DISABLE;
		reg |= RGMII_LINK;
		bcmgenet_ext_writel(priv, reg, EXT_RGMII_OOB_CTRL);

		/* speed */
		if (phydev->speed == SPEED_1000)
			cmd_bits = UMAC_SPEED_1000;
		else if (phydev->speed == SPEED_100)
			cmd_bits = UMAC_SPEED_100;
		else
			cmd_bits = UMAC_SPEED_10;
		cmd_bits <<= CMD_SPEED_SHIFT;

		/* duplex */
		if (phydev->duplex != DUPLEX_FULL)
			cmd_bits |= CMD_HD_EN;

		if (priv->old_pause != phydev->pause) {
			status_changed = 1;
			priv->old_pause = phydev->pause;
		}

		/* pause capability */
		if (!phydev->pause)
			cmd_bits |= CMD_RX_PAUSE_IGNORE | CMD_TX_PAUSE_IGNORE;

		reg = bcmgenet_umac_readl(priv, UMAC_CMD);
		reg &= ~((CMD_SPEED_MASK << CMD_SPEED_SHIFT) |
			 CMD_HD_EN | CMD_RX_PAUSE_IGNORE |
			 CMD_TX_PAUSE_IGNORE);
		reg |= cmd_bits;
		bcmgenet_umac_writel(priv, reg, UMAC_CMD);
	} else {
		/* done if nothing has changed */
		if (!status_changed)
			return;

		/* needed for MoCA fixed PHY to reflect correct link status */
		netif_carrier_off(dev);
	}

	phy_print_status(phydev);
}

/* 7366a0 EXT GPHY block comes with the CFG_IDDQ_BIAS and CFG_EXT_PWR_DOWN
 * bits set to 1 at reset, they need to be cleared. A reset must also be
 * issued. An initial reset value of 1500 micro seconds was not enough, 2000
 * micro seconds always works. The post-reset delay of 20 micro seconds could
 * be eliminated but better be safe than sorry.
 */
void bcmgenet_phy_power_set(struct net_device *dev, bool enable)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	u32 reg = 0;

	/* EXT_GPHY_CTRL is only valid for GENETv4 and onward */
	if (!GENET_IS_V4(priv))
		return;

	reg = bcmgenet_ext_readl(priv, EXT_GPHY_CTRL);
	if (enable) {
		reg &= ~EXT_CK25_DIS;
		bcmgenet_ext_writel(priv, reg, EXT_GPHY_CTRL);
		mdelay(1);

		reg &= ~(EXT_CFG_IDDQ_BIAS | EXT_CFG_PWR_DOWN);
		reg |= EXT_GPHY_RESET;
		bcmgenet_ext_writel(priv, reg, EXT_GPHY_CTRL);
		mdelay(1);

		reg &= ~EXT_GPHY_RESET;
	} else {
		reg |= EXT_CFG_IDDQ_BIAS | EXT_CFG_PWR_DOWN | EXT_GPHY_RESET;
		bcmgenet_ext_writel(priv, reg, EXT_GPHY_CTRL);
		mdelay(1);
		reg |= EXT_CK25_DIS;
	}
	bcmgenet_ext_writel(priv, reg, EXT_GPHY_CTRL);
	udelay(60);
}

int bcmgenet_mii_probe(struct net_device *dev)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	struct phy_device *phydev;
	char phy_id[MII_BUS_ID_SIZE + 3];
	const char *fixed_bus = NULL;
	int phy_addr = priv->phy_addr;
	u32 phy_flags;
	int ret;

	if (priv->old_dt_binding) {
		/* Bind to fixed-0 for MOCA and switches */
		if (priv->phy_type == BRCM_PHY_TYPE_MOCA ||
			priv->phy_addr == BRCM_PHY_ID_NONE) {
			fixed_bus = "fixed-0";
		}

		/* Use the second fixed PHY */
		if (priv->phy_addr == BRCM_PHY_ID_NONE)
			phy_addr = 1;

		/* Connect either on the real MII bus or on the fixed one */
		snprintf(phy_id, sizeof(phy_id), PHY_ID_FMT,
			fixed_bus ? fixed_bus : priv->mii_bus->id,
		phy_addr);
	}

	phy_flags = priv->gphy_rev;

	/* Initialize link state variables that bcmgenet_mii_setup() uses */
	priv->old_link = -1;
	priv->old_speed = -1;
	priv->old_duplex = -1;
	priv->old_pause = -1;

	if (priv->old_dt_binding) {
		phydev = phy_connect(dev, phy_id, bcmgenet_mii_setup,
				priv->phy_interface);
	} else {
		phydev = of_phy_connect(dev, priv->phy_dn,
					bcmgenet_mii_setup, phy_flags,
					priv->phy_interface);
	}

	if (!phydev) {
		pr_err("could not attach to PHY\n");
		return -ENODEV;
	}

	priv->phydev = phydev;

	ret = bcmgenet_mii_config(dev);
	if (ret) {
		phy_disconnect(phydev);
		return ret;
	}

	phydev->supported &= priv->phy_supported;
	/* Adjust advertised speeds based on configured speed */
	if (priv->phy_speed == SPEED_1000)
		phydev->advertising = PHY_GBIT_FEATURES;
	else
		phydev->advertising = PHY_BASIC_FEATURES;

	return 0;
}

/* Workaround for integrated BCM7xxx Gigabit PHYs which have a problem with
 * their internal MDIO management controller making them fail to successfully
 * be read from or written to for the first transaction.  We insert a dummy
 * BMSR read here to make sure that phy_get_device() and get_phy_id() can
 * correctly read the PHY MII_PHYSID1/2 registers and successfully register a
 * PHY device for this peripheral.
 *
 * Once the PHY driver is registered, we can workaround subsequent reads from
 * there (e.g: during system-wide power management).
 *
 * bus->reset is invoked before mdiobus_scan during mdiobus_register and is
 * therefore the right location to stick that workaround. Since we do not want
 * to read from non-existing PHYs, we either use bus->phy_mask or do a manual
 * Device Tree scan to limit the search area.
 */
static int bcmgenet_mii_bus_reset(struct mii_bus *bus)
{
	struct net_device *dev = bus->priv;
	struct bcmgenet_priv *priv = netdev_priv(dev);
	struct device_node *np = priv->mdio_dn;
	struct device_node *child = NULL;
	u32 read_mask = 0;
	int addr = 0;

	if (!np) {
		read_mask = 1 << priv->phy_addr;
	} else {
		for_each_available_child_of_node(np, child) {
			addr = of_mdio_parse_addr(&dev->dev, child);
			if (addr < 0)
				continue;

			read_mask |= 1 << addr;
		}
	}

	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		if (read_mask & 1 << addr) {
			dev_dbg(&dev->dev, "Workaround for PHY @ %d\n", addr);
			mdiobus_read(bus, addr, MII_BMSR);
		}
	}

	return 0;
}

static int bcmgenet_mii_alloc(struct bcmgenet_priv *priv)
{
	struct mii_bus *bus;
	int ret = 0;

	if (priv->mii_bus)
		return 0;

	priv->mii_bus = mdiobus_alloc();
	if (!priv->mii_bus) {
		pr_err("failed to allocate\n");
		return -ENOMEM;
	}

	bus = priv->mii_bus;
	bus->priv = priv->dev;
	bus->name = "bcmgenet MII bus";
	bus->parent = &priv->pdev->dev;
	bus->read = bcmgenet_mii_read;
	bus->write = bcmgenet_mii_write;
	bus->reset = bcmgenet_mii_bus_reset;
	if (priv->old_dt_binding)
		bus->phy_mask = ~(1 << priv->phy_addr);
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s-%d",
			priv->pdev->name, priv->pdev->id);

	bus->irq = kzalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!bus->irq) {
		ret = -ENOMEM;
		goto out_mdio_free;
	}

	/* This is the correct thing to do, but libphy needs fixing
	 * with respect to ignoring interrupts
	 */
	if (priv->phy_addr < PHY_MAX_ADDR) {
		if (priv->phy_type == BRCM_PHY_TYPE_INT)
			bus->irq[priv->phy_addr] = PHY_IGNORE_INTERRUPT;
		else
			bus->irq[priv->phy_addr] = PHY_POLL;
	}

	return 0;

out_mdio_free:
	mdiobus_free(priv->mii_bus);
	return ret;
}

static void bcmgenet_mii_free(struct bcmgenet_priv *priv)
{
	mdiobus_unregister(priv->mii_bus);
	kfree(priv->mii_bus->irq);
	mdiobus_free(priv->mii_bus);
}

static int bcmgenet_fixed_phy_link_update(struct net_device *dev,
					  struct fixed_phy_status *status)
{
	if (dev && dev->phydev && status)
		status->link = dev->phydev->link;

	return 0;
}

static void bcmgenet_internal_phy_setup(struct net_device *dev)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	u32 reg;

	/* Power up PHY */
	bcmgenet_phy_power_set(dev, true);
	/* enable APD */
	reg = bcmgenet_ext_readl(priv, EXT_EXT_PWR_MGMT);
	reg |= EXT_PWR_DN_EN_LD;
	bcmgenet_ext_writel(priv, reg, EXT_EXT_PWR_MGMT);
}

static void bcmgenet_moca_phy_setup(struct bcmgenet_priv *priv)
{
	u32 reg;

	/* Speed settings are set in bcmgenet_mii_setup() */
	reg = bcmgenet_sys_readl(priv, SYS_PORT_CTRL);
	reg |= LED_ACT_SOURCE_MAC;
	bcmgenet_sys_writel(priv, reg, SYS_PORT_CTRL);

	/* Register a fixed PHY link_update callback for this interface */
	if (priv->hw_params->flags & GENET_HAS_MOCA_LINK_DET)
		fixed_phy_set_link_update(priv->phydev,
					  bcmgenet_fixed_phy_link_update);
}

int bcmgenet_mii_config(struct net_device *dev)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	const char *phy_name = NULL;
	u32 id_mode_dis = 0;
	u32 port_ctrl;
	u32 reg;

	priv->ext_phy = (priv->phy_type != BRCM_PHY_TYPE_INT) &&
			(priv->phy_type != BRCM_PHY_TYPE_MOCA);

	switch (priv->phy_interface) {
	case PHY_INTERFACE_MODE_NA:
	case PHY_INTERFACE_MODE_MOCA:
		phy_name = "internal PHY";
		/* Irrespective of the actually configured PHY speed (100 or
		 * 1000) GENETv4 only has an internal GPHY so we will just end
		 * up masking the Gigabit features from what we support, not
		 * switching to the EPHY
		 */
		if (GENET_IS_V4(priv)) {
			priv->phy_supported = PHY_GBIT_FEATURES;
			port_ctrl = PORT_MODE_INT_GPHY;
		} else {
			priv->phy_supported = PHY_BASIC_FEATURES;
			port_ctrl = PORT_MODE_INT_EPHY;
		}

		bcmgenet_sys_writel(priv, port_ctrl, SYS_PORT_CTRL);

		if (priv->phy_type == BRCM_PHY_TYPE_INT) {
			phy_name = "internal PHY";
			bcmgenet_internal_phy_setup(dev);
		} else if (priv->phy_type == BRCM_PHY_TYPE_MOCA) {
			phy_name = "MoCA";
			bcmgenet_moca_phy_setup(priv);
		}
		break;

	case PHY_INTERFACE_MODE_MII:
		phy_name = "external MII";
		priv->phy_supported = PHY_BASIC_FEATURES;
		bcmgenet_sys_writel(priv,
				PORT_MODE_EXT_EPHY, SYS_PORT_CTRL);
		break;

	case PHY_INTERFACE_MODE_REVMII:
		phy_name = "external RvMII";
		if (priv->phy_speed == SPEED_100) {
			priv->phy_supported = PHY_BASIC_FEATURES;
			port_ctrl = PORT_MODE_EXT_RVMII_25;
		} else {
			priv->phy_supported = PHY_GBIT_FEATURES;
			port_ctrl = PORT_MODE_EXT_RVMII_50;
		}
		bcmgenet_sys_writel(priv, port_ctrl, SYS_PORT_CTRL);
		break;

	case PHY_INTERFACE_MODE_RGMII:
		/*
		 * RGMII_NO_ID: TXC transitions at the same time as TXD
		 *              (requires PCB or receiver-side delay)
		 * RGMII:       Add 2ns delay on TXC (90 degree shift)
		 *
		 * ID is implicitly disabled for 100Mbps (RG)MII operation.
		 */
		id_mode_dis = BIT(16);
		/* fall through */
	case PHY_INTERFACE_MODE_RGMII_TXID:
		if (id_mode_dis)
			phy_name = "external RGMII (no delay)";
		else
			phy_name = "external RGMII (TX delay)";
		bcmgenet_sys_writel(priv,
				PORT_MODE_EXT_GPHY, SYS_PORT_CTRL);
		priv->phy_supported = PHY_GBIT_FEATURES;
		/*
		 * setup mii based on configure speed and RGMII txclk is set in
		 * umac->cmd, mii_setup() after link established.
		 */
		break;
	default:
		dev_err(&priv->pdev->dev, "unknown phy_interface: %d\n",
			priv->phy_interface);
		return -EINVAL;
	}

	/* This is an external PHY, aka xMII, so we need to enable the RGMII
	 * block for the interface to work
	 */
	if (priv->ext_phy) {
		reg = bcmgenet_ext_readl(priv, EXT_RGMII_OOB_CTRL);
		reg |= RGMII_MODE_EN | id_mode_dis;
		bcmgenet_ext_writel(priv, reg, EXT_RGMII_OOB_CTRL);
	}

	pr_info_once("%s: configuring instance for %s\n",
		     dev_name(&priv->pdev->dev), phy_name);

	return 0;
}

static int bcmgenet_mii_new_dt_init(struct bcmgenet_priv *priv)
{
	struct device_node *dn = priv->pdev->dev.of_node;
	struct device *kdev = &priv->pdev->dev;
	const char *phy_mode_str = NULL;
	struct phy_device *phydev;
	u32 propval;
	int phy_mode;
	int ret;

	priv->mdio_dn = of_get_next_child(dn, NULL);
	if (!priv->mdio_dn) {
		dev_err(kdev, "unable to find MDIO bus node\n");
		return -ENODEV;
	}

	ret = of_mdiobus_register(priv->mii_bus, priv->mdio_dn);
	if (ret) {
		dev_err(kdev, "failed to register MDIO bus\n");
		return ret;
	}

	/* Check if we have an internal or external PHY */
	priv->phy_dn = of_parse_phandle(dn, "phy-handle", 0);
	if (priv->phy_dn) {
		if (!of_property_read_u32(priv->phy_dn, "max-speed", &propval))
			priv->phy_speed = propval;
	} else {
		if (of_phy_is_fixed_link(dn)) {
			ret = of_phy_register_fixed_link(dn);
			if (ret)
				return ret;
		}

		priv->phy_dn = of_node_get(dn);
	}

	/* Get the link mode */
	phy_mode = of_get_phy_mode(dn);
	priv->phy_interface = phy_mode;

	/* This is not a standard "phy-mode" do an explicit look */
	if (phy_mode < 0) {
		ret = of_property_read_string(dn, "phy-mode", &phy_mode_str);
		if (ret < 0) {
			dev_err(kdev, "invalid PHY mode property\n");
			return ret;
		}

		priv->phy_interface = PHY_INTERFACE_MODE_NA;
		if (!strcasecmp(phy_mode_str, "internal"))
			priv->phy_type = BRCM_PHY_TYPE_INT;
		else {
			dev_err(kdev, "invalid PHY mode: %s\n", phy_mode_str);
			return ret;
		}
	}

	if (phy_mode == PHY_INTERFACE_MODE_MOCA) {
		priv->phy_type = BRCM_PHY_TYPE_MOCA;

		/* Make sure we initialize MoCA PHY with a link down */
		phydev = of_phy_find_device(dn);
		if (phydev)
			phydev->link = 0;
	}


	return 0;
}

static int bcmgenet_mii_old_dt_init(struct bcmgenet_priv *priv)
{
	int ret;

	ret = mdiobus_register(priv->mii_bus);
	if (ret) {
		kfree(priv->mii_bus->irq);
		return ret;
	}

	/* Ensure we set a correct phy_type to phy_interface
	 * translation
	 */
	switch (priv->phy_type) {
	case BRCM_PHY_TYPE_MOCA:
		priv->phy_addr = 0;
		/* fall-through */
	case BRCM_PHY_TYPE_INT:
	default:
		priv->phy_interface = PHY_INTERFACE_MODE_NA;
		break;

	case BRCM_PHY_TYPE_EXT_MII:
		priv->phy_interface = PHY_INTERFACE_MODE_MII;
		break;

	case BRCM_PHY_TYPE_EXT_RVMII:
		priv->phy_interface = PHY_INTERFACE_MODE_REVMII;
		break;

	case BRCM_PHY_TYPE_EXT_RGMII_NO_ID:
		priv->phy_interface = PHY_INTERFACE_MODE_RGMII;
		break;

	case BRCM_PHY_TYPE_EXT_RGMII:
	case BRCM_PHY_TYPE_EXT_RGMII_IBS:
		priv->phy_interface = PHY_INTERFACE_MODE_RGMII_TXID;
		break;
	}

	return 0;
}

int bcmgenet_mii_init(struct net_device *dev)
{
	struct bcmgenet_priv *priv = netdev_priv(dev);
	struct device *kdev = &priv->pdev->dev;
	int ret;

	ret = bcmgenet_mii_alloc(priv);
	if (ret)
		return ret;

	if (priv->old_dt_binding) {
		dev_warn(kdev, "using old DT binding, update BOLT!\n");
		ret = bcmgenet_mii_old_dt_init(priv);
	} else
		ret = bcmgenet_mii_new_dt_init(priv);

	if (ret)
		goto out;

	return 0;
out:
	bcmgenet_mii_free(priv);
	return ret;
}

void bcmgenet_mii_exit(struct net_device *dev)
{
	bcmgenet_mii_free(netdev_priv(dev));
}
