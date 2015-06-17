/***************************************************************************
 *
 * Copyright (C) 2004-2005  SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************
 * File: ioctl_118.h
 */

#ifndef IOCTL_118_H
#define IOCTL_118_H

#define DRIVER_VERSION	(0x00000125UL)

#define SMSC9118_DRIVER_SIGNATURE	(0x82745BACUL+DRIVER_VERSION)
#define SMSC9118_APP_SIGNATURE		(0x987BEF28UL+DRIVER_VERSION)

#define SMSC9118_IOCTL				(SIOCDEVPRIVATE + 0xB)

#define COMMAND_BASE				(0x974FB832UL)

#define COMMAND_GET_SIGNATURE		(COMMAND_BASE+0)

#define COMMAND_LAN_GET_REG			(COMMAND_BASE+1)
#define COMMAND_LAN_SET_REG			(COMMAND_BASE+2)

#define COMMAND_MAC_GET_REG			(COMMAND_BASE+3)
#define COMMAND_MAC_SET_REG			(COMMAND_BASE+4)

#define COMMAND_PHY_GET_REG			(COMMAND_BASE+5)
#define COMMAND_PHY_SET_REG			(COMMAND_BASE+6)

#define COMMAND_DUMP_LAN_REGS		(COMMAND_BASE+7)
#define LAN_REG_ID_REV			(0)
#define LAN_REG_INT_CFG			(1)
#define LAN_REG_INT_STS			(2)
#define LAN_REG_INT_EN			(3)
#define LAN_REG_BYTE_TEST		(4)
#define LAN_REG_FIFO_INT		(5)
#define LAN_REG_RX_CFG			(6)
#define LAN_REG_TX_CFG			(7)
#define LAN_REG_HW_CFG			(8)
#define LAN_REG_RX_DP_CTRL		(9)
#define LAN_REG_RX_FIFO_INF		(10)
#define LAN_REG_TX_FIFO_INF		(11)
#define LAN_REG_PMT_CTRL		(12)
#define LAN_REG_GPIO_CFG		(13)
#define LAN_REG_GPT_CFG			(14)
#define LAN_REG_GPT_CNT			(15)
#define LAN_REG_FPGA_REV		(16)
#define LAN_REG_ENDIAN			(17)
#define LAN_REG_FREE_RUN		(18)
#define LAN_REG_RX_DROP			(19)
#define LAN_REG_MAC_CSR_CMD		(21)
#define LAN_REG_MAC_CSR_DATA	(22)
#define LAN_REG_AFC_CFG			(23)
#define LAN_REG_E2P_CMD			(24)
#define LAN_REG_E2P_DATA		(25)

#define COMMAND_DUMP_MAC_REGS		(COMMAND_BASE+8)
#define MAC_REG_MAC_CR			(0)
#define MAC_REG_ADDRH			(1)
#define MAC_REG_ADDRL			(2)
#define MAC_REG_HASHH			(3)
#define MAC_REG_HASHL			(4)
#define MAC_REG_MII_ACC			(5)
#define MAC_REG_MII_DATA		(6)
#define MAC_REG_FLOW			(7)
#define MAC_REG_VLAN1			(8)
#define MAC_REG_VLAN2			(9)
#define MAC_REG_WUFF			(10)
#define MAC_REG_WUCSR			(11)

#define COMMAND_DUMP_PHY_REGS		(COMMAND_BASE+9)
#define PHY_REG_0				(0)
#define PHY_REG_1				(1)
#define PHY_REG_2				(2)
#define PHY_REG_3				(3)
#define PHY_REG_4				(4)
#define PHY_REG_5				(5)
#define PHY_REG_6				(6)
#define PHY_REG_16				(7)
#define PHY_REG_17				(8)
#define PHY_REG_18				(9)
#define PHY_REG_20				(10)
#define PHY_REG_21				(11)
#define PHY_REG_22				(12)
#define PHY_REG_23				(13)
#define PHY_REG_27				(14)
#define PHY_REG_28				(15)
#define PHY_REG_29				(16)
#define PHY_REG_30				(17)
#define PHY_REG_31				(18)

#define COMMAND_DUMP_EEPROM			(COMMAND_BASE+10)

#define COMMAND_GET_MAC_ADDRESS		(COMMAND_BASE+11)
#define COMMAND_SET_MAC_ADDRESS		(COMMAND_BASE+12)
#define COMMAND_LOAD_MAC_ADDRESS	(COMMAND_BASE+13)
#define COMMAND_SAVE_MAC_ADDRESS	(COMMAND_BASE+14)
#define COMMAND_SET_DEBUG_MODE		(COMMAND_BASE+15)

#define COMMAND_SET_POWER_MODE		(COMMAND_BASE+16)
#define COMMAND_GET_POWER_MODE		(COMMAND_BASE+17)

#define COMMAND_SET_LINK_MODE		(COMMAND_BASE+18)
#define COMMAND_GET_LINK_MODE		(COMMAND_BASE+19)
#define COMMAND_GET_CONFIGURATION	(COMMAND_BASE+20)
#define COMMAND_DUMP_TEMP			(COMMAND_BASE+21)
#define COMMAND_READ_BYTE			(COMMAND_BASE+22)
#define COMMAND_READ_WORD			(COMMAND_BASE+23)
#define COMMAND_READ_DWORD			(COMMAND_BASE+24)
#define COMMAND_WRITE_BYTE			(COMMAND_BASE+25)
#define COMMAND_WRITE_WORD			(COMMAND_BASE+26)
#define COMMAND_WRITE_DWORD			(COMMAND_BASE+27)
#define COMMAND_CHECK_LINK			(COMMAND_BASE+28)

//the following codes are intended for cmd9118 only
//  they are not intended to have any use in the driver
#define COMMAND_RUN_SERVER			(COMMAND_BASE+29)
#define COMMAND_RUN_TUNER			(COMMAND_BASE+30)

#define COMMAND_GET_FLOW_PARAMS		(COMMAND_BASE+31)
#define COMMAND_SET_FLOW_PARAMS		(COMMAND_BASE+32)

typedef struct _SMSC9118_IOCTL_DATA {
	unsigned long dwSignature;
	unsigned long dwCommand;
	unsigned long Data[0x60];
	char Strng1[30];
	char Strng2[10];
} SMSC9118_IOCTL_DATA, *PSMSC9118_IOCTL_DATA;

#endif

