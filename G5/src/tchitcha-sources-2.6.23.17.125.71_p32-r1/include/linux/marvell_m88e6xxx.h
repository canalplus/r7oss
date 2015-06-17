/*
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

#ifndef _MARVELL_M88E6XXX_H_
#define _MARVELL_M88E6XXX_H_
int m88e6xxx_set_phy_port_status(int port, int status);
int m88e6xxx_get_phy_port_status(int port, int *status);
int m88e6xxx_set_wlan_port(int port);
int m88e6xxx_get_wlan_port(void);
int m88e6xxx_disable_wlan_port(void);
int m88e6xxx_enable_wlan_port(void);
#endif /* _MARVELL_M88E6XXX_H_ */
