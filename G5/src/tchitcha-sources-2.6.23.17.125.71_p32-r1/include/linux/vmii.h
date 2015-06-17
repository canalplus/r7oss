/*
 * (C) Copyright 2008 WyPlay SAS.
 * Jean-Christophe PLAGNIOL-VILLARD <jcplagniol@wyplay.com>
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

#ifndef _LINUX_VMII_H_
#define _LINUX_VMII_H_

#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/etherdevice.h>
#include <linux/phy.h>

extern struct bus_type vmii_bus_type;

struct vmii_dev;

struct vmii_devs {
	unsigned int nb;
	struct vmii_dev *devs;
};

struct vmii_dev {
	struct device dev;
	unsigned int devid;
	unsigned int switch_id;
	unsigned int phy_port;
	unsigned int phy_addr;
	unsigned int phy_irq;
	unsigned int phy_mask;
	unsigned int bus_id;
	char *name;
	char *master;
	char *prefix;
	struct net_device *netdev;
	struct net_device *master_device;
	struct phy_device *master_phy;
	struct mii_bus *mii;

	int (*reset)(int);
	int (*set_phy_status)(int);
	u16 active;
	u8 addr[ETH_ALEN];
	int is_clone_mac;
	unsigned int vlan_id;
	struct list_head list;
};

#define VMII_DEV(_d)	container_of((_d), struct vmii_dev, dev)

#define vmii_get_drvdata(d)	dev_get_drvdata(&(d)->dev)
#define vmii_set_drvdata(d,p)	dev_set_drvdata(&(d)->dev, p)

static inline struct net_device* vmii_get_netdev(struct vmii_dev *dev)
{
	return dev->netdev;
}

static inline void vmii_set_netdev(struct vmii_dev *dev, struct net_device *data)
{
	dev->netdev = data;
}

static inline struct net_device* vmii_get_master_netdev(struct vmii_dev *dev)
{
	return dev->master_device;
}

static inline void vmii_set_master_netdev(struct vmii_dev *dev, struct net_device *data)
{
	dev->master_device = data;
}

static inline struct phy_device* vmii_get_master_phydev(struct vmii_dev *dev)
{
	return dev->master_phy;
}

static inline void vmii_set_master_phydev(struct vmii_dev *dev, struct phy_device *data)
{
	dev->master_phy = data;
}

static inline struct mii_bus* vmii_get_master_mii_bus(struct vmii_dev *dev)
{
	struct phy_device* phydev = dev->master_phy;

	if(!phydev)
		return NULL;

	return phydev->bus;
}

#ifdef CONFIG_VMII_MDIO
struct port_switch {
	struct list_head node;
	unsigned int	switch_id;
	unsigned int	port_id;

	/* Called to initialize the PHY,
	 * including after a reset */
	int (*config_init)(struct port_switch *);

	/* Configures the advertisement and resets
	 * autonegotiation if phydev->autoneg is on,
	 * forces the speed to the current settings in phydev
	 * if phydev->autoneg is off */
	int (*config_aneg)(struct port_switch *);

	/* Determines the negotiated speed and duplex */
	int (*read_status)(struct port_switch *);

	/* Clears any pending interrupts */
	int (*ack_interrupt)(struct port_switch *);

	/* Enables or disables interrupts */
	int (*config_intr)(struct port_switch *);

	void *private;
};
#endif /* CONFIG_VMII_MDIO */

struct vmii_driver {
	struct device_driver	driver;
	unsigned int		devid;
	int (*probe)(struct vmii_dev *);
	int (*remove)(struct vmii_dev *);
	int (*suspend)(struct vmii_dev *, pm_message_t);
	int (*resume)(struct vmii_dev *);
	struct sk_buff *(*receive)(struct vmii_dev *, struct sk_buff *);
};

#define VMII_DRV(_d)	container_of((_d), struct vmii_driver, driver)

#define VMII_DRIVER_NAME(_ldev) ((_ldev)->dev.driver->name)

#define vmii_get_prefix(vdev, def) ((vdev->prefix) ? vdev->prefix : def)

int vmii_driver_register(struct vmii_driver *);
void vmii_driver_unregister(struct vmii_driver *);

int vmii_device_xmit(struct vmii_dev*, struct sk_buff*, struct net_device_stats*);
int vmii_device_set_active(struct vmii_dev *dev, u8 state);
int vmii_open_master(struct vmii_dev *dev);
void vmii_add_pack(struct packet_type *pt);
void vmii_remove_pack(struct packet_type *pt);
void vmii_dev_set_multicast_list(struct vmii_dev *dev);
int vmii_dev_set_mac_address(struct vmii_dev *dev, u8 *addr);
int vmii_dev_change_mtu(struct vmii_dev *dev, int new_mtu);
int vmii_dev_init_master(struct vmii_dev *dev);
int vmii_dev_init_mac_address(struct vmii_dev *dev);

#ifdef CONFIG_VMII_MDIO
int vmii_switch_port_register(struct port_switch *);
int vmii_switch_port_unregister(struct port_switch *);
int vmii_mdio_register(struct vmii_dev *vdev);
int vmii_mdio_unregister(struct vmii_dev *vdev);
#endif	/* CONFIG_VMII_MDIO */

enum VMII_IDS {
	VMII_GENERIC = 0x1,
	VMII_RT2880 = 0x2,
};
#endif /* _LINUX_VMII_H_ */
