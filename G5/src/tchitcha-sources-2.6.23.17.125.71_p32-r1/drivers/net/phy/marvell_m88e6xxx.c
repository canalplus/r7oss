/*
 * version 1.0
 *
 * (C) Copyright 2006-2008 WyPlay SAS.
 * Jean-Christophe Plagniol-Villard <jcplagniol@wyplay.com>
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
#include <linux/mii.h>
#include <linux/vmii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/marvell_m88e6xxx.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#define MII_TIMEOUT_RESET	(ulong)(0.5*CFG_HZ) /* 500 ms, as per IEEE 802.u §22.2.4.1.1 */
#define MII_AUTONEG_ATTEMPT_MAX	5

#define MARVELL_88E60X1_MAC_PORT_0	0x18
#define MARVELL_88E60X1_MAC_PORT_1	0x19
#define MARVELL_88E60X1_MAC_PORT_2	0x1A
#define MARVELL_88E60X1_MAC_PORT_3	0x1B
#define MARVELL_88E60X1_MAC_PORT_4	0x1C
#define MARVELL_88E60X1_MAC_PORT_5	0x1D

#define MARVELL_88E60X1_PHY_PORT_0	0x10
#define MARVELL_88E60X1_PHY_PORT_1	0x11
#define MARVELL_88E60X1_PHY_PORT_2	0x12
#define MARVELL_88E60X1_PHY_PORT_3	0x13
#define MARVELL_88E60X1_PHY_PORT_4	0x14

#define MARVELL_88E60X1_GLOBAL_REGISTER			0x1f
#define MARVELL_88E60X1_GLOBAL_REGISTER2		0x1e

#define MARVELL_88E60X1_PHY_PORT_STATUS			0x01
#define MARVELL_88E60X1_PHY_SPECIFIC_STATUS		0x11
#define MARVELL_88E60X1_PHY_INTERRUPT_ENABLE		0x12
#define MARVELL_88E60X1_PHY_INTERRUPT_STATUS		0x13
# define MARVELL_88E60X1_PHY_INTERRUPT_CLEAR		0x0
# define MARVELL_88E60X1_PHY_INTERRUPT_BIT_CARRIER	(1 << 8)
# define MARVELL_88E60X1_PHY_INTERRUPT_BIT_LINK		(1 << 10)
#define MARVELL_88E60X1_SWITCH_PORT_CONTROL		0x04
#define MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL	0x09

#define MARVELL_88E60X1_GLOBAL_MANAGEMENT_CONTROL	0x1a
#define MARVELL_88E60X1_GLOBAL2_INGRESS_RATE_COMMOND	0x09

/* registers MII_PHYSID1 and MII_PHYSID2 */
#define MARVELL_88E6031_ID_MODEL	0x00000310
#define MARVELL_88E6031_ID_MODEL_MASK	0xFFFFFFF0
#define MARVELL_88E6031_REV_MASK	0x0000000F
#define MARVELL_88E6031_REV_SHIFT	0

/* register MII_ADVERTISE */
#define MII_ADV_PAUSE_ASYM	0x0400
#define MII_ADV_PAUSE_SYM	0x0800

#define CONFIG_MARVELL_88E6031
static int mac_port[] = {
#ifdef CONFIG_MARVELL_88E6031
	MARVELL_88E60X1_MAC_PORT_0,
	MARVELL_88E60X1_MAC_PORT_4,
	MARVELL_88E60X1_MAC_PORT_5,
#else
	MARVELL_88E60X1_MAC_PORT_0,
	MARVELL_88E60X1_MAC_PORT_1,
	MARVELL_88E60X1_MAC_PORT_2,
	MARVELL_88E60X1_MAC_PORT_3,
	MARVELL_88E60X1_MAC_PORT_4,
	MARVELL_88E60X1_MAC_PORT_5,
#endif
};

static struct phy_device* _phy = NULL;
static int wlan_port = -1;
static int wlan_port_old = -1;

int m88e6xxx_mii_read (struct phy_device* phy, unsigned int portNumber ,
			unsigned int MIIReg)
{
	int err;

	phy->addr = portNumber;
	err = phy_read(phy, MIIReg);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("%s:%s:%d: portNumber=0x%x, MIIReg=0x%x, value=0x%x\n",
		__FILE__, __FUNCTION__, __LINE__, portNumber, MIIReg, err);
#endif
#ifdef CONFIG_M88E6XXX_DEBUG
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

static unsigned int inline phy_port_to_real_port(int port)
{
	unsigned int real_port = 0xFF;

	if(port < 0)
		return real_port;

	switch(port)
	{
		case 0:
			real_port = MARVELL_88E60X1_PHY_PORT_0;
			break;
		case 1:
			real_port = MARVELL_88E60X1_PHY_PORT_1;
			break;
		case 2:
			real_port = MARVELL_88E60X1_PHY_PORT_2;
			break;
		case 3:
			real_port = MARVELL_88E60X1_PHY_PORT_3;
			break;
		case 4:
			real_port = MARVELL_88E60X1_PHY_PORT_4;
			break;
		default:
			break;
	}
	return real_port;
}

static unsigned int inline mac_port_to_real_port(int port)
{
	unsigned int real_port = 0xFF;

	if(port < 0)
		return real_port;

	switch(port)
	{
		case 0:
			real_port = MARVELL_88E60X1_MAC_PORT_0;
			break;
		case 1:
			real_port = MARVELL_88E60X1_MAC_PORT_1;
			break;
		case 2:
			real_port = MARVELL_88E60X1_MAC_PORT_2;
			break;
		case 3:
			real_port = MARVELL_88E60X1_MAC_PORT_3;
			break;
		case 4:
			real_port = MARVELL_88E60X1_MAC_PORT_4;
			break;
		case 5:
			real_port = MARVELL_88E60X1_MAC_PORT_5;
			break;
		default:
			break;
	}
	return real_port;
}

#ifdef CONFIG_VMII_MDIO
/*
 * Functions used to manage a port.
 */
static int m88e6xxx_config_port_init(struct port_switch *port)
{
	return 0;
}

static int m88e6xxx_config_port_aneg(struct port_switch *port)
{
	return 0;
}

static int m88e6xxx_read_port_status(struct port_switch *port)
{
	unsigned int value;
	int status = 0;
	int addr;

	addr = phy_port_to_real_port(port->port_id);

	/* It is necessary to wait for some time to get the correct status
	 * (To be clarified: autoneg is may be not finish) */
	mdelay (1);
	value = m88e6xxx_mii_read((struct phy_device *)(port->private),
				  addr,
				  MARVELL_88E60X1_PHY_PORT_STATUS);

	if ((value & BMSR_LSTATUS) == 0){
		status = 0;
	} else {
		status = 1;
	}

	return status;}

static int m88e6xxx_ack_port_interrupt(struct port_switch *port)
{
	int err;
	struct phy_device *phydev = port->private;

	/* Clear the interrupts by reading the reg */

	err = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER, 0x0);
	err = m88e6xxx_mii_read(phydev, phy_port_to_real_port(port->port_id),
					MARVELL_88E60X1_PHY_INTERRUPT_STATUS);

	if (err < 0)
		return err;

	return 0;
}

static int m88e6xxx_config_port_intr(struct port_switch *port)
{
	int err;
	struct phy_device *phydev = port->private;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		err = m88e6xxx_mii_write(
			phydev, phy_port_to_real_port(port->port_id),
			MARVELL_88E60X1_PHY_INTERRUPT_ENABLE,
			MARVELL_88E60X1_PHY_INTERRUPT_BIT_CARRIER);
	} else {
		err = m88e6xxx_mii_write(
			phydev, phy_port_to_real_port(port->port_id),
			MARVELL_88E60X1_PHY_INTERRUPT_ENABLE,
			MARVELL_88E60X1_PHY_INTERRUPT_CLEAR);
	}

	return err;
}

static struct port_switch available_port[] = {
	{
		.switch_id = 1,
		.port_id = 0,
		.config_init = &m88e6xxx_config_port_init,
		.config_aneg = &m88e6xxx_config_port_aneg,
		.read_status = &m88e6xxx_read_port_status,
		.ack_interrupt = &m88e6xxx_ack_port_interrupt,
		.config_intr = &m88e6xxx_config_port_intr,
		.private = NULL,
	},
};
#endif	/* CONFIG_VMII_MDIO */

int phy_restart_autoneg(struct phy_device *phydev,int phy_addr)
{
	phydev->addr = phy_addr;
	return genphy_config_aneg(phydev);
}

int phy_read_autoneg_status(struct phy_device *phydev, int phy_addr, int *speed,
				int* full_duplex)
{
	unsigned int value, valuebis;

	value = m88e6xxx_mii_read(phydev, phy_addr, MARVELL_88E60X1_PHY_PORT_STATUS);
	/* now read current link status */
	printk ("PHY : ");
	switch(phy_addr)
	{
		case MARVELL_88E60X1_PHY_PORT_0:
			printk("Port 0 ");
			break;
		case MARVELL_88E60X1_PHY_PORT_1:
			printk("Port 1 ");
			break;
		case MARVELL_88E60X1_PHY_PORT_2:
			printk("Port 2 ");
			break;
		case MARVELL_88E60X1_PHY_PORT_4:
			printk("Port 3 ");
			break;
		default:
			printk("Port Unkown ");
			break;

	}
	if ((value & BMSR_LSTATUS) == 0)
	{
		printk ("*Warning* no link detected\n"); /* will default to 100Mbps */
		return -1;
	}
	else
	{
		valuebis = m88e6xxx_mii_read(phydev, phy_addr, MARVELL_88E60X1_PHY_SPECIFIC_STATUS);
		if( (valuebis & 0x400 ) != 0x400)
		{
			printk("*Warning Link is down\n");
			return -1;
		}
		if( (valuebis & 0x800 ) != 0x800)
		{
			printk("*Warning Auto-Negotiation not resolved\n");
			return -1;
		}
		if( (valuebis & 0x4000 ) == 0x4000)
		{
			*speed = 100;
			printk("100Mbps");
		}
		else
		{
			*speed = 10;
			printk("10Mbps");
		}

		if( (valuebis & 0x2000) == 0x2000)
		{
			*full_duplex =1;
			printk(" full duplex");
		}
		else
		{
			printk(" half duplex");
			*full_duplex =0;
		}
		printk(" link detected\n");
	}
	return 0;
}

static int m88e6xxx_config_aneg(struct phy_device *phydev)
{
	int err;
	err = genphy_config_aneg(phydev);
	return err;
}

static int m88e6xxx_config_init(struct phy_device *phydev)
{
	unsigned int value;
	unsigned long id;
#ifdef CONFIG_VMII_MDIO
	int iii;
#endif

	phydev->adjust_state = 0;

	/* probe MII for a MARVELL_88E6031 */
	/* probe Port 5 as MII */
	id = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_5, MII_PHYSID1);
	id <<= 16;
	id |= m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_5, MII_PHYSID2);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("phy_id = 0x%x\n", id);
#endif
	if(id != 0x310)
	{
		printk("Did not detect any MARVELL 88E6031 MAC Port5.\n");
		return -1;
	}
	/* probe Port 4 as MII */
	id = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_4, MII_PHYSID1);
	id <<= 16;
	id |= m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_4, MII_PHYSID2);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("phy_id = 0x%x\n", id);
#endif
	if(id != 0x310)
	{
		printk("Did not detect any MARVELL 88E6031 MAC Port 4.\n");
		return -1;
	}
	printk("MAC : Port 5 100Mbps full duplex\n");
	/* probe Port 0 as Phy */
	id = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_4, MII_PHYSID1);
	id <<= 16;
	id |= m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_4, MII_PHYSID2);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("phy_id = 0x%x\n", id);
#endif
	/* ignore phy rev */
	if((id & 0xFFFFFFF0) != (0x1410c89 & 0xFFFFFFF0))
	{
		printk("Did not detect any MARVELL 88E6031 PHY Port 0.\n");
		return -1;
	}
	/* Enable All Port */
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_0,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL);
	value |= 0x3;
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_0,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL, value);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_4,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL);
	value |= 0x3;
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_4,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL, value);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_5,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL);
	value |= 0x3;
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_5,
				MARVELL_88E60X1_SWITCH_PORT_CONTROL,value);
	/* set twice Field type RWS */
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER,
				MARVELL_88E60X1_GLOBAL_MANAGEMENT_CONTROL);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_GLOBAL_REGISTER,
				MARVELL_88E60X1_GLOBAL_MANAGEMENT_CONTROL, 0x0);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER,
				MARVELL_88E60X1_GLOBAL_MANAGEMENT_CONTROL);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_GLOBAL_REGISTER,
				MARVELL_88E60X1_GLOBAL_MANAGEMENT_CONTROL, 0x0);

	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_0,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_0,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL, 0x0);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_4,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_4,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL, 0x0);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_MAC_PORT_5,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_MAC_PORT_5,
				MARVELL_88E60X1_SWITCH_INGRESS_RATE_CONTROL, 0x0);

	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER2,
				MARVELL_88E60X1_GLOBAL2_INGRESS_RATE_COMMOND);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_GLOBAL_REGISTER2,
				MARVELL_88E60X1_GLOBAL2_INGRESS_RATE_COMMOND, 0x9000);
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER2,
				MARVELL_88E60X1_GLOBAL2_INGRESS_RATE_COMMOND);

	/* perform a software reset */
	id = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_0, MII_PHYSID1);
	id <<= 16;
	id |= m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_0, MII_PHYSID2);
#ifdef CONFIG_M88E6XXX_DEBUG
	printk("phy_id = 0x%x\n",id);
#endif
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_0, MII_BMCR);
	value = 0x3200;
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_PHY_PORT_0, MII_BMCR, value);
	value= m88e6xxx_mii_read(phydev, MARVELL_88E60X1_PHY_PORT_4, MII_BMCR);
	value = 0x3200;
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_PHY_PORT_4, MII_BMCR, value);

	m88e6xxx_set_wlan_port(4);
	_phy = phydev;

	/* force port 0 down */
	m88e6xxx_set_phy_port_status(0x0, 0);

	phydev->state = PHY_RUNNING;
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
	phydev->link = 1;
	phydev->pause = phydev->asym_pause = 0;
	netif_carrier_on(phydev->attached_dev);

#ifdef CONFIG_VMII_MDIO
	/* The switch registers each port on the vmdio bus */
	for (iii = 0; iii < ARRAY_SIZE (available_port); iii++) {
		available_port[iii].private = _phy;	/* master phy device */
		vmii_switch_port_register(&available_port[iii]);
	}
#endif
#ifdef CONFIG_MARVELL_M88E6XXX_PHY_INTERRUPT
	/* Enable interrupt on phy port 0 */
	value = m88e6xxx_mii_read(phydev, MARVELL_88E60X1_GLOBAL_REGISTER, 0x4);
	m88e6xxx_mii_write(phydev, MARVELL_88E60X1_GLOBAL_REGISTER, 0x4, value | 0x2);
#endif
	return 0;
}

int m88e6xxx_set_phy_port_status(int port, int status)
{
	int real_port;
	int val;

	if(!_phy)
		return -EIO;

	real_port = phy_port_to_real_port(port);
	if(real_port == 0xFF)
		return -EIO;

	val = m88e6xxx_mii_read(_phy, real_port, 0x0);

	if(status) {
		if (!(val & 0x800))
			return 0;

		val = 0x3200;
	} else {
		if (val & 0x800)
			return 0;

		val |= 0x800;
	}

	m88e6xxx_mii_write(_phy, real_port, 0x0, val);

	return 0;
}
EXPORT_SYMBOL(m88e6xxx_set_phy_port_status);

int m88e6xxx_get_phy_port_status(int port, int *status)
{
	unsigned int real_port;
	int val;

	if(!_phy)
		return -EIO;

	real_port = phy_port_to_real_port(port);
	if(real_port == 0xFF)
		return -EIO;

	val = m88e6xxx_mii_read(_phy, real_port, 0x0);
	*status = !(val & 0x800);

	return 0;
}
EXPORT_SYMBOL(m88e6xxx_get_phy_port_status);

int m88e6xxx_set_wlan_port(int port)
{
	unsigned int real_port;
	int i, nb_port;
	int val;

	/* the CPU port can not be WLAN port */
	if(!_phy || port == 5)
		return -EIO;

	real_port = mac_port_to_real_port(port);

	if(real_port == 0xFF)
		return -EIO;

	wlan_port = port;
	wlan_port_old = port;

	nb_port = sizeof(mac_port) / sizeof(int);
#if 0
	/*
	 * set PVID for each port.
	 * the first port(port 0, WAN) has default VID 2 and all others has 1.
	 */
	m88e6xxx_mii_read(_phy, real_port, 0x7);
	m88e6xxx_mii_write(_phy, real_port, 0x7, 0x2);

	for(i = 0; i < nb_port; i++)
	{
		if(mac_port[i] != real_port)
		{
			m88e6xxx_mii_read(_phy, mac_port[i], 0x7);
			m88e6xxx_mii_write(_phy, mac_port[i], 0x7, 0x1);
		}
	}

	/*
	 * set Port VLAN Mapping.
	 *	port 0 (WAN) and cpu port are in a vlan 2.
	 *	And all the rest ports (LAN) and cpu port are in a vlan 1.
	 */
	/* TODO: Caculate it. The follwing value is for port 0 */
	m88e6xxx_mii_read(_phy, real_port, 0x6);
	m88e6xxx_mii_write(_phy, real_port, 0x6, 0x1);

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_0, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_0, 0x6, 0x20);

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_5, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_5, 0x6, 0x11);

	m88e6xxx_mii_read(_phy, real_port, 0x6);
	m88e6xxx_mii_write(_phy, real_port, 0x6, 0x10);
#else
	for(i = 0; i < nb_port; i++)
	{
		m88e6xxx_mii_read(_phy, real_port, 0x7);
		m88e6xxx_mii_write(_phy, mac_port[i], 0x7, 0x1);
	}

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_4, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_4, 0x6, 0x2e);
	m88e6xxx_mii_read(_phy, real_port, 0x6);
	m88e6xxx_mii_write(_phy, real_port, 0x6, 0x2e);

	for(i = 0; i < nb_port; i++)
	{
		val = m88e6xxx_mii_read(_phy, mac_port[i], 0x6);
		m88e6xxx_mii_write(_phy, mac_port[i], 0x6, val | 0x100);
	}

#endif
	return 0;
}
EXPORT_SYMBOL(m88e6xxx_set_wlan_port);

int m88e6xxx_get_wlan_port(void)
{
	return wlan_port;
}
EXPORT_SYMBOL(m88e6xxx_get_wlan_port);

int m88e6xxx_enable_wlan_port(void)
{
	if(wlan_port >= 0)
		return 1;

	return m88e6xxx_set_wlan_port(wlan_port_old);
}

int m88e6xxx_disable_wlan_port(void)
{
	int i, nb_port;

	printk("%s\n", __FUNCTION__);
	wlan_port_old = wlan_port;
	wlan_port = -1;

	if(!_phy)
		return -EIO;

	nb_port = sizeof(mac_port) / sizeof(int);

	for(i = 0; i< nb_port; i++)
	{
		m88e6xxx_mii_read(_phy, mac_port[i], 0x7);
		m88e6xxx_mii_write(_phy, mac_port[i], 0x7, 0x1);
	}

	/* TODO: Caculate it. */

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_0, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_0, 0x6, 0x30);

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_4, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_4, 0x6, 0x21);

	m88e6xxx_mii_read(_phy, MARVELL_88E60X1_MAC_PORT_5, 0x6);
	m88e6xxx_mii_write(_phy, MARVELL_88E60X1_MAC_PORT_5, 0x6, 0x11);

	return 0;
}
EXPORT_SYMBOL(m88e6xxx_disable_wlan_port);

static int m88e6xxx_read_status(struct phy_device *phydev)
{
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
	phydev->pause = phydev->asym_pause = 0;
	phydev->link = 1;
	phydev->state = PHY_RUNNING;
	phydev->irq = PHY_IGNORE_INTERRUPT;

	return 0;
}

/*.phy_id = 0x01410310, */
static struct phy_driver marvell_drivers=
{
	.phy_id = 0x310,
	.phy_id_mask = 0xfff,
	.name = "Marvell 88E6xxx",

	.features = PHY_BASIC_FEATURES,

	/* basic functions */
	.config_init = &m88e6xxx_config_init,
	.config_aneg = &m88e6xxx_config_aneg,
	.read_status = &m88e6xxx_read_status,

	.driver = {.owner = THIS_MODULE,},
};

static int __init marvell_m88e6xxx_init(void)
{
	int ret;

	ret = phy_driver_register(&marvell_drivers);
	if (ret)
	{
		phy_driver_unregister(&marvell_drivers);
		return ret;
	}
	return 0;
}

static void __exit marvell_m88e6xxx_exit(void)
{
	phy_driver_unregister(&marvell_drivers);
}

module_init(marvell_m88e6xxx_init);
module_exit(marvell_m88e6xxx_exit);
