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
 * File: cmd9118.c
 */
#ifdef USING_LINT
#include "lint.h"
#else //not USING_LINT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <linux/delay.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>

#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#endif //not USING_LINT

#include "ioctl_118.h"

char *iam=NULL;

typedef enum _bool {
  false=0,
  true=1
} bool;

typedef struct _COMMAND_DATA {
	int hSockFD;
	struct ifreq IfReq;
	SMSC9118_IOCTL_DATA IoctlData;
} COMMAND_DATA, *PCOMMAND_DATA;

#define SOCKET	int
#define INVALID_SOCKET	(-1)
#define SOCKET_ERROR	(-1)
#define DEFAULT_PORT_NUMBER		(11118U)
SOCKET server_sock=INVALID_SOCKET;

typedef struct _FLOW_PARAMS
{
    unsigned long MeasuredMaxThroughput;
	unsigned long MeasuredMaxPacketCount;
	unsigned long MaxThroughput;
	unsigned long MaxPacketCount;
	unsigned long PacketCost;
	unsigned long BurstPeriod;
	unsigned long MaxWorkLoad;
	unsigned long IntDeas;
} FLOW_PARAMS, * PFLOW_PARAMS;

bool ParseNumber(const char *str,unsigned long *number);
void DisplayUsage(void);
void GetMacAddress(PCOMMAND_DATA commandData);
void SetMacAddress(PCOMMAND_DATA commandData,unsigned long addrh,unsigned long addrl);
void LoadMacAddress(PCOMMAND_DATA commandData);
void SaveMacAddress(PCOMMAND_DATA commandData,unsigned long addrh,unsigned long addrl);
void LanDumpRegs(PCOMMAND_DATA commandData);
void MacDumpRegs(PCOMMAND_DATA commandData);
void DumpEEPROM(PCOMMAND_DATA commandData);
void DumpTemp(PCOMMAND_DATA commandData);
void PhyDumpRegs(PCOMMAND_DATA commandData);
void SetDebugMode(PCOMMAND_DATA commandData,
				  unsigned long debug_mode);
void SetLinkMode(PCOMMAND_DATA commandData,
				 unsigned long link_mode);
void SetPowerMode(PCOMMAND_DATA commandData,
				  unsigned long power_mode);
void GetLinkMode(PCOMMAND_DATA commandData);
void CheckLink(PCOMMAND_DATA commandData);
void GetPowerMode(PCOMMAND_DATA commandData);
void GetFlowParams(PCOMMAND_DATA commandData);
void GetConfiguration(PCOMMAND_DATA commandData);
void ReadByte(PCOMMAND_DATA commandData,unsigned long address);
void ReadWord(PCOMMAND_DATA commandData, unsigned long address);
void ReadDWord(PCOMMAND_DATA commandData,unsigned long address);
void WriteByte(PCOMMAND_DATA commandData,unsigned long address, unsigned long data);
void WriteWord(PCOMMAND_DATA commandData,unsigned long address, unsigned long data);
void WriteDWord(PCOMMAND_DATA commandData,unsigned long address, unsigned long data);
void LanGetReg(PCOMMAND_DATA commandData,unsigned long address);
void LanSetReg(PCOMMAND_DATA commandData, unsigned long address, unsigned long data);
void MacGetReg(PCOMMAND_DATA commandData, unsigned long address);
void MacSetReg(
	PCOMMAND_DATA commandData,
	unsigned long address, unsigned long data);
void PhyGetReg(PCOMMAND_DATA commandData, unsigned long address);
void PhySetReg(
	PCOMMAND_DATA commandData,
	unsigned long address, unsigned long data);
bool Initialize(PCOMMAND_DATA commandData,const char *ethName);
bool ReceiveULong(SOCKET sock,unsigned long * pDWord);
bool SendULong(SOCKET sock,unsigned long data);
void process_requests(PCOMMAND_DATA commandData);
void RunServer(PCOMMAND_DATA commandData,unsigned short portNumber);
bool ReceiveFlowParams(SOCKET sock,PFLOW_PARAMS flowParams);
bool SendFlowParams(SOCKET sock,PFLOW_PARAMS flowParams);
void DisplayFlowParams(PFLOW_PARAMS flowParams);
unsigned long ReadThroughput(char * fileName);
void RunTuner(const char *hostName,unsigned short portNum);


bool ParseNumber(const char *str,unsigned long *number) {
	if(str==NULL) return false;
	if(str[0]==0) return false;
	if((str[0]=='0')&&(str[1]=='x')) {
		if(sscanf(&(str[2]),"%lx",number)==1) {
			return true;
		}
	}
	if(sscanf(str,"%ld",number)==1) {
		return true;
	}
	return false;
}

void DisplayUsage(void) {
	printf("usage: %s [-h] [-e adaptername] [-c command] [-a address] [-d data]\n",iam);
	printf("       [-H host_address] [-p port_number]\n");
	printf("  -h displays this usage information, other options ignored.\n");
	printf("  -e specifies the adapter name (eth0,eth1...)\n");
	printf("       if not specified then %s will attempt to\n",iam);
	printf("       auto detect.\n");
	printf("  -c specifies the command code\n");
	printf("       SERVER = run in server mode\n");
	printf("          may use -p to specify port to listen on\n");
	printf("       TUNER = connects remotely to server to run\n");
	printf("          automatic flow control tuning.\n");
	printf("          use -H to specify host address\n");
	printf("          use -p to specify port\n");
	printf("       GET_CONFIG = gets internal variables of driver\n");
	printf("       DUMP_REGS = dumps the LAN9118 memory mapped registers\n");
	printf("       DUMP_MAC = dumps the LAN9118 MAC registers\n");
	printf("       DUMP_PHY = dumps the LAN9118 PHY registers\n");
	printf("       DUMP_EEPROM = dumps the first 8 bytes of the EEPROM\n");
	printf("       DUMP_TEMP = dumps temp data space used for debugging\n");
	printf("       GET_MAC = gets MAC address from ADDRH and ADDRL\n");
	printf("       SET_MAC = sets MAC address in ADDRH and ADDRL\n");
	printf("         -a specifies the value to write to ADDRH\n");
	printf("         -d specifies the value to write to ADDRL\n");
	printf("       LOAD_MAC = causes the LAN9118 to reload the MAC address\n");
	printf("           from the external EEPROM. Also displays it\n");
	printf("       SAVE_MAC = writes a MAC address to the EEPROM\n");
	printf("         -a specifies the part of the MAC address that would\n");
	printf("            appear in ADDRH\n");
	printf("         -d specifies the part of the MAC address that would\n");
	printf("            appear in ADDRL\n");
	printf("       SET_DEBUG = sets the driver's internal debug_mode value\n");
	printf("         -d specifies the debug mode\n");
	printf("             0x01, bit 0, enables trace messages\n");
	printf("             0x02, bit 1, enables warning messages\n");
	printf("             0x04, bit 2, enables GPO signals\n");
	printf("          NOTE: trace, and warning messages will only show if\n");
	printf("             they have been turned on at driver compile time.\n");
	printf("       SET_LINK = sets the driver's internal link_mode value\n");
	printf("             and also attempts to relink with the new setting\n");
	printf("         -d specifies the link mode\n");
	printf("             1 = 10HD, 2 = 10FD, 4 = 100HD, 8 = 100FD\n");
	printf("             to specify multiple link modes, add the values\n");
	printf("             of each mode you want and use the sum as the link mode\n");
	printf("       GET_LINK = gets the driver's internal link_mode value\n");
	printf("     Warning!! Power management is not implemented as of version 0.54\n");
	printf("       SET_POWER = sets the LAN9118 power mode\n");
	printf("         -d specifies the power state\n");
	printf("             0 = D0, 1 = D1, 2 = D2, 3 = D3\n");
	printf("       GET_POWER = gets the LAN9118 power mode\n");
	printf("       CHECK_LINK = causes the driver to recheck its link status\n");
	printf("     Warning!! the following read and write commands may cause\n");
	printf("     unpredictable results, including system lock up or crash.\n");
	printf("     Use with caution\n");
	printf("       READ_REG = reads a value from the LAN9118 Memory Map\n");
	printf("         -a specifies offset into LAN9118 Memory Map\n");
	printf("       WRITE_REG = writes a value to the LAN9118 Memory Map\n");
	printf("         -a specifies offset into LAN9118 Memory Map\n");
	printf("         -d specifies data to write in HEX form\n");
	printf("       READ_MAC = reads a value from the LAN9118 Mac registers\n");
	printf("         -a specifies the Mac register index\n");
	printf("       WRITE_MAC = writes a value to the LAN9118 Mac registers\n");
	printf("         -a specifies the Mac register index\n");
	printf("         -d specifies data to write in HEX form\n");
	printf("       READ_PHY = reads a value from the LAN9118 Phy registers\n");
	printf("         -a specifies the Phy register index\n");
	printf("       WRITE_PHY = writes a value to the LAN9118 Phy registers\n");
	printf("         -a specifies the Phy register index\n");
	printf("         -d specifies data to write in HEX form\n");
	printf("       READ_BYTE = reads a byte from a location in memory\n");
	printf("         -a address\n");
	printf("       READ_WORD = reads a word from a location in memory\n");
	printf("         -a address\n");
	printf("       READ_DWORD = reads a dword from a location in memory\n");
	printf("         -a address\n");
	printf("       WRITE_BYTE = write a byte to a location in memory\n");
	printf("         -a address -d data\n");
	printf("       WRITE_WORD = write a word to a location in memory\n");
	printf("         -a address -d data\n");
	printf("       WRITE_DWORD = write a dword to a location in memory\n");
	printf("         -a address -d data\n");
	printf("  -a specifies the address, index, or offset of a register\n");
	printf("  -d specifies the data to write to a register\n");
	printf("       can be decimal or hexadecimal\n");
}

void GetMacAddress(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_GET_MAC_ADDRESS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Mac Address == 0x%04lX%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to Get Mac Address\n");
	}
}
void SetMacAddress(PCOMMAND_DATA commandData,unsigned long addrh,unsigned long addrl)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_SET_MAC_ADDRESS;
	commandData->IoctlData.Data[0]=addrh;
	commandData->IoctlData.Data[1]=addrl;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Set Mac Address\n");
	}
}
void LoadMacAddress(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_LOAD_MAC_ADDRESS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Mac Address == 0x%04lX%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to Load Mac Address\n");
	}
}
void SaveMacAddress(PCOMMAND_DATA commandData,unsigned long addrh,unsigned long addrl)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_SAVE_MAC_ADDRESS;
	commandData->IoctlData.Data[0]=addrh;
	commandData->IoctlData.Data[1]=addrl;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Save Mac Address\n");
	}
}

void LanDumpRegs(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_DUMP_LAN_REGS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("offset 0x50, ID_REV       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_ID_REV]);
		printf("offset 0x54, INT_CFG      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_INT_CFG]);
		printf("offset 0x58, INT_STS      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_INT_STS]);
		printf("offset 0x5C, INT_EN       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_INT_EN]);
		printf("offset 0x64, BYTE_TEST    = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_BYTE_TEST]);
		printf("offset 0x68, FIFO_INT     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_FIFO_INT]);
		printf("offset 0x6C, RX_CFG       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_RX_CFG]);
		printf("offset 0x70, TX_CFG       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_TX_CFG]);
		printf("offset 0x74, HW_CFG       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_HW_CFG]);
		printf("offset 0x78, RX_DP_CTRL   = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_RX_DP_CTRL]);
		printf("offset 0x7C, RX_FIFO_INF  = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_RX_FIFO_INF]);
		printf("offset 0x80, TX_FIFO_INF  = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_TX_FIFO_INF]);
		printf("offset 0x84, PMT_CTRL     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_PMT_CTRL]);
		printf("offset 0x88, GPIO_CFG     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_GPIO_CFG]);
		printf("offset 0x8C, GPT_CFG      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_GPT_CFG]);
		printf("offset 0x90, GPT_CNT      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_GPT_CNT]);
		printf("offset 0x94, FPGA_REV     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_FPGA_REV]);
		printf("offset 0x98, ENDIAN       = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_ENDIAN]);
		printf("offset 0x9C, FREE_RUN     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_FREE_RUN]);
		printf("offset 0xA0, RX_DROP      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_RX_DROP]);
		printf("offset 0xA4, MAC_CSR_CMD  = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_MAC_CSR_CMD]);
		printf("offset 0xA8, MAC_CSR_DATA = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_MAC_CSR_DATA]);
		printf("offset 0xAC, AFC_CFG      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_AFC_CFG]);
		printf("offset 0xB0, E2P_CMD      = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_E2P_CMD]);
		printf("offset 0xB4, E2P_DATA     = 0x%08lX\n",commandData->IoctlData.Data[LAN_REG_E2P_DATA]);
	} else {
		printf("Failed to DUMP registers\n");
	}
}

void MacDumpRegs(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_DUMP_MAC_REGS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("index 1, MAC_CR   = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_MAC_CR]);
		printf("index 2, ADDRH    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_ADDRH]);
		printf("index 3, ADDRL    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_ADDRL]);
		printf("index 4, HASHH    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_HASHH]);
		printf("index 5, HASHL    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_HASHL]);
		printf("index 6, MII_ACC  = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_MII_ACC]);
		printf("index 7, MII_DATA = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_MII_DATA]);
		printf("index 8, FLOW     = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_FLOW]);
		printf("index 9, VLAN1    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_VLAN1]);
		printf("index A, VLAN2    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_VLAN2]);
		printf("index B, WUFF     = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_WUFF]);
		printf("index C, WUCSR    = 0x%08lX\n",commandData->IoctlData.Data[MAC_REG_WUCSR]);
	} else {
		printf("Failed to Dump Mac Registers\n");
	}
}

void DumpEEPROM(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_DUMP_EEPROM;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("EEPROM[0]=0x%02lX\n",commandData->IoctlData.Data[0]);
		printf("EEPROM[1]=0x%02lX\n",commandData->IoctlData.Data[1]);
		printf("EEPROM[2]=0x%02lX\n",commandData->IoctlData.Data[2]);
		printf("EEPROM[3]=0x%02lX\n",commandData->IoctlData.Data[3]);
		printf("EEPROM[4]=0x%02lX\n",commandData->IoctlData.Data[4]);
		printf("EEPROM[5]=0x%02lX\n",commandData->IoctlData.Data[5]);
		printf("EEPROM[6]=0x%02lX\n",commandData->IoctlData.Data[6]);
		printf("EEPROM[7]=0x%02lX\n",commandData->IoctlData.Data[7]);
	} else {
		printf("Failed to Dump EEPROM\n");
	}
}

void DumpTemp(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_DUMP_TEMP;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		unsigned long c=0;
		for(c=0;c<0x10;c++) {
			printf("temp[0x%02lX]=0x%08lX, ",c,commandData->IoctlData.Data[c]);
			printf("temp[0x%02lX]=0x%08lX, ",c+0x10,commandData->IoctlData.Data[c+0x10]);
			printf("temp[0x%02lX]=0x%08lX, ",c+0x20,commandData->IoctlData.Data[c+0x20]);
			printf("temp[0x%02lX]=0x%08lX\n",c+0x30,commandData->IoctlData.Data[c+0x30]);
		}
	} else {
		printf("Failed to dump temp data.\n");
	}
}

void PhyDumpRegs(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_DUMP_PHY_REGS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("index 0, Basic Control Reg = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_0]);
		printf("index 1, Basic Status Reg  = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_1]);
		printf("index 2, PHY identifier 1  = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_2]);
		printf("index 3, PHY identifier 2  = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_3]);
		printf("index 4, Auto Negotiation Advertisement Reg = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_4]);
		printf("index 5, Auto Negotiation Link Partner Ability Reg = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_5]);
		printf("index 6, Auto Negotiation Expansion Register = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_6]);
		printf("index 16, Silicon Revision Reg = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_16]);
		printf("index 17, Mode Control/Status Reg = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_17]);
		printf("index 18, Special Modes = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_18]);
		printf("index 20, TSTCNTL = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_20]);
		printf("index 21, TSTREAD1 = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_21]);
		printf("index 22, TSTREAD2 = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_22]);
		printf("index 23, TSTWRITE = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_23]);
		printf("index 27, Control/Status Indication = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_27]);
		printf("index 28, Special internal testability = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_28]);
		printf("index 29, Interrupt Source Register = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_29]);
		printf("index 30, Interrupt Mask Register = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_30]);
		printf("index 31, PHY Special Control/Status Register = 0x%04lX\n",commandData->IoctlData.Data[PHY_REG_31]);
	} else {
		printf("Failed to DUMP Phy Registers\n");
	}
}

void SetDebugMode(PCOMMAND_DATA commandData,
				  unsigned long debug_mode)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_SET_DEBUG_MODE;
	commandData->IoctlData.Data[0]=debug_mode;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to set debug mode.\n");
	}
}

void SetLinkMode(PCOMMAND_DATA commandData,
				 unsigned long link_mode)
{
	if(link_mode<=0x7F) {
		if(commandData==NULL) return;
		commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
		commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
		commandData->IoctlData.dwCommand=COMMAND_SET_LINK_MODE;
		commandData->IoctlData.Data[0]=link_mode;
		ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
		if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
			printf("Failed to set link mode.\n");
		}
	} else {
		printf("Invalid Link Mode, %ld\n",link_mode);
	}
}

void SetPowerMode(PCOMMAND_DATA commandData,
				  unsigned long power_mode)
{
	if(power_mode<4) {
		if(commandData==NULL) return;
		commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
		commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
		commandData->IoctlData.dwCommand=COMMAND_SET_POWER_MODE;
		commandData->IoctlData.Data[0]=power_mode;
		ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
		if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
			printf("Failed to set power mode.\n");
		}
	} else {
		printf("Invalid Power Mode, %ld\n",power_mode);
	}
}

void GetLinkMode(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_GET_LINK_MODE;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		unsigned long link_mode=commandData->IoctlData.Data[0];
		printf("link_mode == 0x%02lX == %s,%s,%s,%s,%s,%s,%s\n",
			link_mode,
			(link_mode&0x40)?"ANEG":"",
			(link_mode&0x20)?"SYMP":"",
			(link_mode&0x10)?"ASYMP":"",
			(link_mode&0x08)?"100FD":"",
			(link_mode&0x04)?"100HD":"",
			(link_mode&0x02)?"10FD":"",
			(link_mode&0x01)?"10HD":"");
	} else {
		printf("Failed to get link mode\n");
	}
}

void CheckLink(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_CHECK_LINK;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Checked link successfully\n");
	} else {
		printf("Failed to check link\n");
	}
}

void GetPowerMode(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_GET_POWER_MODE;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("PMT_CTRL == 0x%08lX, PM_MODE == D%ld\n",
			commandData->IoctlData.Data[0],
			(((commandData->IoctlData.Data[0])&0x00030000UL)>>16));
	} else {
		printf("Failed to get power mode\n");
	}
}

void GetFlowParams(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_GET_FLOW_PARAMS;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		const unsigned long * data=commandData->IoctlData.Data;
		printf("Flow Control Parameters\n");
		printf("  RxFlowMeasuredMaxThroughput     = 0x%08lX\n",data[0]);
		printf("  RxFlowMeasuredMaxPacketCount    = 0x%08lX\n",data[1]);
		printf("  RxFlowParameters.MaxThroughput  = 0x%08lX\n",data[2]);

		printf("  RxFlowParameters.MaxPacketCount = 0x%08lX\n",data[3]);
		printf("  RxFlowParameters.PacketCost     = 0x%08lX\n",data[4]);
		printf("  RxFlowParameters.BurstPeriod    = 0x%08lX\n",data[5]);
		printf("  RxFlowMaxWorkLoad               = 0x%08lX\n",data[6]);
		printf("  INT_CFG.INT_DEAS                = 0x%08lX\n",data[7]);
	} else {
		printf("Failed to get flow control parameters\n");
	}
}

void GetConfiguration(PCOMMAND_DATA commandData)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_GET_CONFIGURATION;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		const unsigned long * data=commandData->IoctlData.Data;
		printf("Compiled: %s\n",commandData->IoctlData.Strng1);
		printf("Driver Version = %lX.%02lX\n",
			data[0]>>8,data[0]&0xFFUL);
		printf("Driver Parameters\n");
		printf("  lan_base         = 0x%08lX\n",data[1]);
		printf("  bus_width        = 0x%08lX\n",data[2]);
		printf("  link_mode        = 0x%08lX\n",data[3]);
		printf("  irq              = 0x%08lX\n",data[4]);
		printf("  int_deas         = 0x%08lX\n",data[5]);
		printf("  irq_pol          = 0x%08lX\n",data[6]);
		printf("  irq_type         = 0x%08lX\n",data[7]);
		printf("  rx_dma           = 0x%08lX\n",data[8]);
		printf("  tx_dma           = 0x%08lX\n",data[9]);
		printf("  dma_threshold    = 0x%08lX\n",data[10]);
		printf("  mac_addr_hi16    = 0x%08lX\n",data[11]);
		printf("  mac_addr_lo32    = 0x%08lX\n",data[12]);
		printf("  debug_mode       = 0x%08lX\n",data[13]);
		printf("  tx_fif_sz        = 0x%08lX\n",data[14]);
		printf("  afc_cfg          = 0x%08lX\n",data[15]);
		printf("  tasklets         = 0x%08lX\n",data[16]);
		printf("  max_throughput   = 0x%08lX\n",data[17]);
		printf("  max_packet_count = 0x%08lX\n",data[18]);
		printf("  packet_cost      = 0x%08lX\n",data[19]);
		printf("  burst_period     = 0x%08lX\n",data[20]);
		printf("  max_work_load    = 0x%08lX\n",data[21]);
		printf("privateData\n");
		printf("  ifName                       = \"%s\"\n",
			commandData->IoctlData.Strng2);
		printf("  dwIdRev                      = 0x%08lX\n",data[22]);
		printf("  dwFpgaRev                    = 0x%08lX\n",data[23]);
		printf("  bPhyAddress                  = 0x%08lX\n",data[24]);
		printf("  dwPhyId                      = 0x%08lX\n",data[25]);
		printf("  bPhyModel                    = 0x%08lX\n",data[26]);
		printf("  bPhyRev                      = 0x%08lX\n",data[27]);
		printf("  dwLinkSpeed                  = 0x%08lX\n",data[28]);
		printf("  RxFlowMeasuredMaxThroughput  = 0x%08lX\n",data[29]);
		printf("  RxFlowMeasuredMaxPacketCount = 0x%08lX\n",data[30]);
		printf("  RxFlowMaxThroughput          = 0x%08lX\n",data[31]);
		printf("  RxFlowMaxPacketCount         = 0x%08lX\n",data[32]);
		printf("  RxFlowPacketCost             = 0x%08lX\n",data[33]);
		printf("  RxFlowBurstPeriod            = 0x%08lX\n",data[34]);
		printf("  RxFlowMaxWorkLoad            = 0x%08lX\n",data[35]);
	} else {
		printf("Failed to get driver configuration\n");
	}
}

void ReadByte(PCOMMAND_DATA commandData,unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_READ_BYTE;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Memory Address == 0x%08lX, Read Value == 0x%02lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]&0xFFUL);
	} else {
		printf("Failed to Read Memory\n");
	}
}
void ReadWord(PCOMMAND_DATA commandData, unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_READ_WORD;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Memory Address == 0x%08lX, Read Value == 0x%04lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]&0xFFFFUL);
	} else {
		printf("Failed to Read Memory\n");
	}

}
void ReadDWord(PCOMMAND_DATA commandData,unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_READ_DWORD;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Memory Address == 0x%08lX, Read Value == 0x%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to Read Memory\n");
	}
}
void WriteByte(PCOMMAND_DATA commandData,unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_WRITE_BYTE;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data&0xFFUL;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Memory\n");
	}
}
void WriteWord(PCOMMAND_DATA commandData,unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_WRITE_WORD;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data&0xFFFFUL;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Memory\n");
	}
}
void WriteDWord(PCOMMAND_DATA commandData,unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_WRITE_DWORD;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Memory\n");
	}
}

void LanGetReg(PCOMMAND_DATA commandData,unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_LAN_GET_REG;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Mem Map Offset == 0x%08lX, Read Value == 0x%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to Read Register\n");
	}
}
void LanSetReg(PCOMMAND_DATA commandData, unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_LAN_SET_REG;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Register\n");
	}
}
void MacGetReg(PCOMMAND_DATA commandData, unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_MAC_GET_REG;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Mac Index == 0x%08lX, Read Value == 0x%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to read Mac Register\n");
	}
}
void MacSetReg(
	PCOMMAND_DATA commandData,
	unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_MAC_SET_REG;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Mac Register\n");
	}
}
void PhyGetReg(PCOMMAND_DATA commandData, unsigned long address)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_PHY_GET_REG;
	commandData->IoctlData.Data[0]=address;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
		printf("Phy Index == 0x%08lX, Read Value == 0x%08lX\n",
			commandData->IoctlData.Data[0],
			commandData->IoctlData.Data[1]);
	} else {
		printf("Failed to Read Phy Register\n");
	}
}
void PhySetReg(
	PCOMMAND_DATA commandData,
	unsigned long address, unsigned long data)
{
	if(commandData==NULL) return;
	commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
	commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
	commandData->IoctlData.dwCommand=COMMAND_PHY_SET_REG;
	commandData->IoctlData.Data[0]=address;
	commandData->IoctlData.Data[1]=data;
	ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
	if(commandData->IoctlData.dwSignature!=SMSC9118_DRIVER_SIGNATURE) {
		printf("Failed to Write Phy Register\n");
	}
}

bool Initialize(PCOMMAND_DATA commandData,const char *ethName) {

	if(commandData==NULL) return false;
	if(ethName==NULL) return false;
	commandData->hSockFD=socket(AF_INET,SOCK_DGRAM,0);
	if((commandData->hSockFD) < 0) {
		perror("\r\nFailed to create socket !! ->");
		return false;
   	}
	commandData->IfReq.ifr_data= (void *)&(commandData->IoctlData);
	memset(&(commandData->IoctlData),0,sizeof(SMSC9118_IOCTL_DATA));
	if(ethName[0]!=0) {
		strncpy(commandData->IfReq.ifr_name,ethName,IFNAMSIZ);
		commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
		commandData->IoctlData.dwCommand=COMMAND_GET_SIGNATURE;
		ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
		if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
			return true;
		}
		printf("Failed to find 9118 driver on %s\n",commandData->IfReq.ifr_name);
	} else {
		int ifNumber;
		for(ifNumber=0;ifNumber<8;ifNumber++) {
			sprintf(commandData->IfReq.ifr_name,"eth%d",ifNumber);
			commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
			commandData->IoctlData.dwCommand=COMMAND_GET_SIGNATURE;
			commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
			ioctl(commandData->hSockFD,SMSC9118_IOCTL,&(commandData->IfReq));
			if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
				//printf("found 9118 on %s\n",commandData->IfReq.ifr_name);
				return true;
			}
		}
		printf("Failed to find 9118 driver on eth0 .. eth7\n");
	}
	printf("Either the driver has not been installed or there is\n");
	printf("a possible version mismatch between smsc9118.o and cmd9118\n");
	return false;
}

bool ReceiveULong(SOCKET sock,unsigned long * pDWord)
{
	bool result=false;
	unsigned long data=0;
	unsigned char ch=0;
	if(recv(sock,&ch,1,0)>0) {
		data=(unsigned long)ch;
		if(recv(sock,&ch,1,0)>0) {
			data|=(((unsigned long)ch)<<8);
			if(recv(sock,&ch,1,0)>0) {
				data|=(((unsigned long)ch)<<16);
				if(recv(sock,&ch,1,0)>0) {
					data|=(((unsigned long)ch)<<24);
					(*pDWord)=data;
					result=true;
				}
			}
		}
	}
	return result;
}

bool SendULong(SOCKET sock,unsigned long data)
{
	bool result=false;
	unsigned char ch=(unsigned char)(data&0x000000FFUL);
	if(send(sock,&ch,1,0)==1) {
		ch=(unsigned char)((data>>8)&0x000000FFUL);
		if(send(sock,&ch,1,0)==1) {
			ch=(unsigned char)((data>>16)&0x000000FFUL);
			if(send(sock,&ch,1,0)==1) {
				ch=(unsigned char)((data>>24)&0x000000FFUL);
				if(send(sock,&ch,1,0)==1) {
					result=true;
				}
			}
		}
	}
	return result;
}

void process_requests(PCOMMAND_DATA commandData)
{
	unsigned long requestCode=0;
	while(ReceiveULong(server_sock,&requestCode)) {
		switch(requestCode) {
		case COMMAND_GET_FLOW_PARAMS:
			commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
			commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
			commandData->IoctlData.dwCommand=COMMAND_GET_FLOW_PARAMS;
			ioctl(commandData->hSockFD,
				SMSC9118_IOCTL,&(commandData->IfReq));
			if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
				SendULong(server_sock,1);//1==success
				SendULong(server_sock,commandData->IoctlData.Data[0]);//RxFlowMeasuredMaxThroughput
				SendULong(server_sock,commandData->IoctlData.Data[1]);//RxFlowMeasuredMaxPacketCount
				SendULong(server_sock,commandData->IoctlData.Data[2]);//RxFlowParameters.MaxThroughput
				SendULong(server_sock,commandData->IoctlData.Data[3]);//RxFlowParameters.MaxPacketCount
				SendULong(server_sock,commandData->IoctlData.Data[4]);//RxFlowParameters.PacketCost
				SendULong(server_sock,commandData->IoctlData.Data[5]);//RxFlowParameters.BurstPeriod
				SendULong(server_sock,commandData->IoctlData.Data[6]);//RxFlowMaxWorkLoad
				SendULong(server_sock,commandData->IoctlData.Data[7]);//INT_CFG.INT_DEAS
			} else {
				SendULong(server_sock,0);//0==failed
			}
			break;
		case COMMAND_SET_FLOW_PARAMS:
			{
				unsigned long data=0;
				commandData->IfReq.ifr_data=(void *)&(commandData->IoctlData);
				commandData->IoctlData.dwSignature=SMSC9118_APP_SIGNATURE;
				commandData->IoctlData.dwCommand=COMMAND_SET_FLOW_PARAMS;
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[0]=data;//RxFlowMeasuredMaxThroughput
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[1]=data;//RxFlowMeasuredMaxPacketCount
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[2]=data;//RxFlowParameters.MaxThroughput
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[3]=data;//RxFlowParameters.MaxPacketCount
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[4]=data;//RxFlowParameters.PacketCost
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[5]=data;//RxFlowParameters.BurstPeriod
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[6]=data;//RxFlowMaxWorkLoad
				if(!ReceiveULong(server_sock,&data)) break;
				commandData->IoctlData.Data[7]=data;//INT_CFG.INT_DEAS
				ioctl(commandData->hSockFD,
					SMSC9118_IOCTL,&(commandData->IfReq));
				if(commandData->IoctlData.dwSignature==SMSC9118_DRIVER_SIGNATURE) {
					SendULong(server_sock,1);//1==success
				} else {
					SendULong(server_sock,0);//0==failed
				}
			}
			break;
		default:
			printf("WARNING, unknown requestCode=0x%08lX\n",requestCode);
			break;
		}
	}
}

void RunServer(PCOMMAND_DATA commandData,unsigned short portNumber)
{
    struct sockaddr *server;
	struct sockaddr_in server4;
	int sockaddr_len;
	struct sockaddr peeraddr;
	SOCKET server_control;
	int on=1;

	if(portNumber==0) {
		portNumber=DEFAULT_PORT_NUMBER;
	}

	server4.sin_port=htons(portNumber);
	server4.sin_addr.s_addr=INADDR_ANY;
	server4.sin_family=AF_INET;
	sockaddr_len=sizeof(struct sockaddr_in);
	server=(struct sockaddr *)&server4;

	printf("Starting server at port %d\n",portNumber);
	server_control = socket(server->sa_family,SOCK_STREAM,0);
	if(server_control==INVALID_SOCKET)
	{
		printf("Error creating the socket\n");
		exit(1);
	}
	if(setsockopt(server_control,
		SOL_SOCKET,
		SO_REUSEADDR,
		(char *)&on,
		sizeof(on))==SOCKET_ERROR)
	{
		printf("Error: SO_REUSEADDR\n");
		exit(1);
	}
	if(bind(server_control,server,sockaddr_len)==SOCKET_ERROR)
	{
		printf("Error binding the socket\n");
		exit(1);
	}
	if(listen(server_control,5)==SOCKET_ERROR)
	{
		printf("Error listening\n");
		exit(1);
	}
	switch(fork()) {
	case -1:
		printf("Error on fork\n");
		exit(1);
	case 0:
		fclose(stdin);
		fclose(stderr);
		setsid();
		for(;;)
		{
			if((server_sock=accept(server_control,
					&peeraddr,
					&sockaddr_len)) == INVALID_SOCKET)
			{
				printf("Error accept failed\n");
				exit(1);
			}
			switch(fork()) {
			case -1:
				//something went wrong
				exit(1);
			case 0:
				//we are the child process
				close(server_control);
				process_requests(commandData);
				close(server_sock);
				exit(0);
				break;
			default:
				//we are the parent process
				close(server_sock);
				break;
			}
		}

		break;
	default:
		exit(0);
	}
}

bool ReceiveFlowParams(SOCKET sock,PFLOW_PARAMS flowParams)
{
	bool result=false;
	if(SendULong(sock,COMMAND_GET_FLOW_PARAMS)) {
		unsigned long data=0;
		if(!ReceiveULong(sock,&data)) goto DONE;
		if(data) {
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->MeasuredMaxThroughput=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->MeasuredMaxPacketCount=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->MaxThroughput=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->MaxPacketCount=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->PacketCost=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->BurstPeriod=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->MaxWorkLoad=data;
			if(!ReceiveULong(sock,&data)) goto DONE;
			flowParams->IntDeas=data;
			result=true;
		}
	}
DONE:
	return result;
}

bool SendFlowParams(SOCKET sock,PFLOW_PARAMS flowParams)
{
	bool result=false;
	unsigned long timeout=100;
	unsigned long data=0;
AGAIN:
	SendULong(sock,COMMAND_SET_FLOW_PARAMS);
	SendULong(sock,flowParams->MeasuredMaxThroughput);
	SendULong(sock,flowParams->MeasuredMaxPacketCount);
	SendULong(sock,flowParams->MaxThroughput);
	SendULong(sock,flowParams->MaxPacketCount);
	SendULong(sock,flowParams->PacketCost);
	SendULong(sock,flowParams->BurstPeriod);
	SendULong(sock,flowParams->MaxWorkLoad);
	SendULong(sock,flowParams->IntDeas);
	if(ReceiveULong(sock,&data)) {
		if(data) {
			result=true;
		} else {
			//If flow control was active this command will fail
			//  therefore wait and try again later.
			if(timeout>0) {
				timeout--;
				sleep(5);
				goto AGAIN;
			}
		}
	}
	return result;
}

void DisplayFlowParams(PFLOW_PARAMS flowParams)
{
	printf("Flow Control Parameters\n");
	printf("  MaxThroughput          = 0x%08lX\n",flowParams->MaxThroughput);
	printf("  MaxPacketCount         = 0x%08lX\n",flowParams->MaxPacketCount);
	printf("  PacketCost             = 0x%08lX\n",flowParams->PacketCost);
	printf("  BurstPeriod            = 0x%08lX\n",flowParams->BurstPeriod);
	printf("  IntDeas                = 0x%08lX\n",flowParams->IntDeas);
}

unsigned long ReadThroughput(char * fileName)
{
	unsigned long result=0;
	bool clearFlag=true;
	FILE * filePtr=NULL;
	filePtr=fopen(fileName,"r");
	if(filePtr!=NULL) {
		char ch=0;
		while(fread(&ch,1,1,filePtr)!=0) {
			switch(ch) {
			case '0':case '1':case '2':case '3':case '4':

			case '5':case '6':case '7':case '8':case '9':
				if(clearFlag) {
					result=0;
					clearFlag=false;
				}
				result*=10;
				result+=(unsigned long)(ch-'0');
				break;
			case '.':
				break;
			default:
				clearFlag=true;
				break;
			}
		}
		fclose(filePtr);
	} else {
		printf("ReadThroughput: unable to open file\n");
	}
	return result;
}

void RunTuner(const char *hostName,unsigned short portNum)
{
	SOCKET controlSocket=INVALID_SOCKET;
	struct sockaddr_in server;
    unsigned int addr;
	char command[128];
	char outputFile[]="npout.txt";
	FLOW_PARAMS flowParams;
	FLOW_PARAMS origParams;
	FLOW_PARAMS bestParams;
	unsigned long bestMeasurement=0;
	unsigned long currentSetting=0;
	unsigned long currentMeasurement=0;
	unsigned long tempLow=0;
	unsigned long tempHigh=0;
	int salen;
	memset((char *)&server,0,sizeof(server));

	if(portNum==0) {
		portNum=DEFAULT_PORT_NUMBER;
	}
	server.sin_port=htons(portNum);
	if((addr=inet_addr(hostName))==0xFFFFFFFFUL) {
		printf("Invalid: hostName==\"%s\"",hostName);
		printf("   must be in IP form\n");
		return;
	} else {
		server.sin_addr.s_addr=addr;
		server.sin_family=AF_INET;
	}
	salen=sizeof(server);

	controlSocket=socket(AF_INET,SOCK_STREAM,0);
	if(controlSocket==INVALID_SOCKET) {
		printf("error creating control socket\n");
		return;
	}
	if(connect(controlSocket,
		(struct sockaddr *)&server,
		salen) == INVALID_SOCKET)
	{
		printf("failed to connect to %s:%d\n",hostName,portNum);
		goto DONE;
	}

	if(!ReceiveFlowParams(controlSocket,&origParams)) goto FAILED;
	memcpy(&bestParams,&origParams,sizeof(FLOW_PARAMS));
	bestParams.MeasuredMaxThroughput=0;
	bestParams.MeasuredMaxPacketCount=0;
	bestParams.MaxThroughput=0;
	bestParams.MaxPacketCount=0;
	bestParams.BurstPeriod=100;
	bestParams.MaxWorkLoad=0;
	bestParams.IntDeas=0;
	memcpy(&flowParams,&bestParams,sizeof(FLOW_PARAMS));

	printf("Entire tuning process will take about one hour.\n");
	printf("TUNING IntDeas\n");
	sprintf(command,"./netperf -H%s > %s",hostName,outputFile);
	bestMeasurement=0;
	for(currentSetting=0;currentSetting<80;currentSetting++) {
		flowParams.IntDeas=currentSetting;
		if(!SendFlowParams(controlSocket,&flowParams)) {
			printf("Failed to send new setting: IntDeas=%ld\n",currentSetting);
			goto FAILED;
		}
		if(system(command)==0) {
			currentMeasurement=ReadThroughput(outputFile);
			if(currentMeasurement>bestMeasurement) {
				bestMeasurement=currentMeasurement;
				bestParams.IntDeas=currentSetting;
			}
		} else {
			printf("Failed system command: \"%s\"\n",command);
			goto FAILED;
		}
		printf("IntDeas = %ld, Throughput = %ld\n",currentSetting,currentMeasurement);
	}
	printf("Best: IntDeas = %ld, Throughput = %ld\n",bestParams.IntDeas,bestMeasurement);
	flowParams.IntDeas=bestParams.IntDeas;
	if(!SendFlowParams(controlSocket,&flowParams)) {
		goto FAILED;
	}
	//getting the FlowParams back will make sure the measurements are clear.
	if(!ReceiveFlowParams(controlSocket,&flowParams)) goto FAILED;
	if(flowParams.IntDeas!=bestParams.IntDeas) {
		printf("new setting did not stick\n");
		goto FAILED;
	}
	printf("Measuring Max Throughput, and Max Packet Count\n");
	if(system(command)!=0) goto FAILED;
	if(system(command)!=0) goto FAILED;
	if(system(command)!=0) goto FAILED;
	if(!ReceiveFlowParams(controlSocket,&flowParams)) goto FAILED;
	bestParams.MeasuredMaxThroughput=flowParams.MeasuredMaxThroughput;
	bestParams.MeasuredMaxPacketCount=flowParams.MeasuredMaxPacketCount;
	bestParams.MaxThroughput=bestParams.MeasuredMaxThroughput;
	bestParams.MaxPacketCount=bestParams.MeasuredMaxPacketCount;
	bestParams.PacketCost=0;
	bestParams.MaxWorkLoad=0xFFFFFFFFUL;
	bestParams.BurstPeriod=100;
	memcpy(&flowParams,&bestParams,sizeof(FLOW_PARAMS));

	printf("TUNING Burst Period\n");
	sprintf(command,"./netperf -H%s -tUDP_STREAM -l10 -- -m1472 > %s",
		hostName,outputFile);
	bestMeasurement=0;
	for(currentSetting=10;currentSetting<120;currentSetting++) {
		flowParams.BurstPeriod=currentSetting;
		if(!SendFlowParams(controlSocket,&flowParams)) {
			printf("Failed to send new setting: BurstPeriod=%ld\n",currentSetting);
			goto FAILED;
		}
		if(system(command)==0) {
			currentMeasurement=ReadThroughput(outputFile);
			if(currentMeasurement>bestMeasurement) {
				bestMeasurement=currentMeasurement;
				bestParams.BurstPeriod=currentSetting;
			}
		} else {
			printf("Failed system command: \"%s\"\n",command);
			goto FAILED;
		}
		printf("BurstPeriod = %ld, Throughput = %ld\n",
			currentSetting,currentMeasurement);
	}
	printf("Best: BurstPeriod = %ld, Throughput = %ld\n",
		bestParams.BurstPeriod,bestMeasurement);
	flowParams.BurstPeriod=bestParams.BurstPeriod;

	printf("TUNING PacketCost\n");
	sprintf(command,"./netperf -H%s -tUDP_STREAM -l10 -- -m16 > %s",
		hostName,outputFile);
	bestMeasurement=0;
	for(currentSetting=0;currentSetting<500;currentSetting+=10) {
		flowParams.PacketCost=currentSetting;
		if(!SendFlowParams(controlSocket, &flowParams)) {
			printf("Failed to send new setting: PacketCost=%ld\n",currentSetting);
			goto FAILED;
		}
		if(system(command)==0) {
			currentMeasurement=ReadThroughput(outputFile);
			if(currentMeasurement>bestMeasurement) {
				bestMeasurement=currentMeasurement;
				bestParams.PacketCost=currentSetting;
			}
		} else {
			printf("Failed system command: \"%s\"\n",command);
			goto FAILED;
		}
		printf("PacketCost = %ld, Throughput = %ld\n",
			currentSetting,currentMeasurement);
	}
	bestMeasurement=0;
	if(bestParams.PacketCost>20) {
		tempLow=bestParams.PacketCost-20;
	} else {
		tempLow=0;
	}
	tempHigh=bestParams.PacketCost+20;
	for(currentSetting=tempLow;currentSetting<tempHigh;currentSetting++) {
		flowParams.PacketCost=currentSetting;
		if(!SendFlowParams(controlSocket, &flowParams)) {
			printf("Failed to send new setting: PacketCost=%ld\n",currentSetting);
			goto FAILED;
		}
		if(system(command)==0) {
			currentMeasurement=ReadThroughput(outputFile);
			if(currentMeasurement>bestMeasurement) {
				bestMeasurement=currentMeasurement;
				bestParams.PacketCost=currentSetting;
			}
		} else {
			printf("Failed system command: \"%s\"\n",command);
			goto FAILED;
		}
		printf("PacketCost = %ld, Throughput = %ld\n",
			currentSetting,currentMeasurement);
	}
	printf("Best: PacketCost = %ld, Throughput = %ld\n",
		bestParams.PacketCost,bestMeasurement);
	printf("Best ");DisplayFlowParams(&bestParams);
	if(!SendFlowParams(controlSocket, &origParams)) {
		printf("Failed to restore original setting\n");
		goto FAILED;
	}

DONE:
	close(controlSocket);
	return;
FAILED:
	close(controlSocket);
	printf("Something went wrong\n");
}

int main(ac,av)
int ac;
char * av[];
{
	int oc=0;
	bool eSet=false;
	char ethName[IFNAMSIZ];
	COMMAND_DATA commandData;
	bool cSet=false;
	bool aSet=false;
	unsigned long address=0;
	bool dSet=false;
	unsigned long data=0;
	unsigned long commandCode=0;
	char hostName[128];
	bool hSet=false;
	unsigned long portNum=0;
	bool pSet=false;


	iam=av[0];
	ethName[0]=0;
	hostName[0]=0;

	while((oc=getopt(ac,av,"hH:p:e:c:a:d:"))!=-1) {
		switch(oc) {
		case 'h'://help
			goto BAD_USAGE;
		case 'H'://Host address
			if(hSet) goto BAD_USAGE;
			strcpy(hostName,optarg);
			hSet=true;
			break;
		case 'p':
			if(pSet) goto BAD_USAGE;
			if(!ParseNumber(optarg,&portNum)) {
				goto BAD_USAGE;
			}
			if(portNum>0xFFFFUL) goto BAD_USAGE;
			pSet=true;
			break;
		case 'e':
			if(eSet) goto BAD_USAGE;
			eSet=true;
			strncpy(ethName,optarg,IFNAMSIZ);
			ethName[IFNAMSIZ-1]=0;
			break;
		case 'c':
			if(cSet) goto BAD_USAGE;
			if(strcmp(optarg,"GET_CONFIG")==0) {
				commandCode=COMMAND_GET_CONFIGURATION;
			} else if(strcmp(optarg,"DUMP_REGS")==0) {
				commandCode=COMMAND_DUMP_LAN_REGS;
			} else if(strcmp(optarg,"DUMP_MAC")==0) {
				commandCode=COMMAND_DUMP_MAC_REGS;
			} else if(strcmp(optarg,"DUMP_PHY")==0) {
				commandCode=COMMAND_DUMP_PHY_REGS;
			} else if(strcmp(optarg,"DUMP_EEPROM")==0) {
				commandCode=COMMAND_DUMP_EEPROM;
			} else if(strcmp(optarg,"DUMP_TEMP")==0) {
				commandCode=COMMAND_DUMP_TEMP;
			} else if(strcmp(optarg,"GET_MAC")==0) {
				commandCode=COMMAND_GET_MAC_ADDRESS;
			} else if(strcmp(optarg,"SET_MAC")==0) {
				commandCode=COMMAND_SET_MAC_ADDRESS;
			} else if(strcmp(optarg,"LOAD_MAC")==0) {
				commandCode=COMMAND_LOAD_MAC_ADDRESS;
			} else if(strcmp(optarg,"SAVE_MAC")==0) {
				commandCode=COMMAND_SAVE_MAC_ADDRESS;
			} else if(strcmp(optarg,"SET_DEBUG")==0) {
				commandCode=COMMAND_SET_DEBUG_MODE;
			} else if(strcmp(optarg,"SET_POWER")==0) {
				commandCode=COMMAND_SET_POWER_MODE;
			} else if(strcmp(optarg,"GET_POWER")==0) {
				commandCode=COMMAND_GET_POWER_MODE;
			} else if(strcmp(optarg,"SET_LINK")==0) {
				commandCode=COMMAND_SET_LINK_MODE;
			} else if(strcmp(optarg,"GET_LINK")==0) {
				commandCode=COMMAND_GET_LINK_MODE;
			} else if(strcmp(optarg,"CHECK_LINK")==0) {
				commandCode=COMMAND_CHECK_LINK;
			} else if(strcmp(optarg,"READ_REG")==0) {
				commandCode=COMMAND_LAN_GET_REG;
			} else if(strcmp(optarg,"WRITE_REG")==0) {
				commandCode=COMMAND_LAN_SET_REG;
			} else if(strcmp(optarg,"READ_MAC")==0) {
				commandCode=COMMAND_MAC_GET_REG;
			} else if(strcmp(optarg,"WRITE_MAC")==0) {
				commandCode=COMMAND_MAC_SET_REG;
			} else if(strcmp(optarg,"READ_PHY")==0) {
				commandCode=COMMAND_PHY_GET_REG;
			} else if(strcmp(optarg,"WRITE_PHY")==0) {
				commandCode=COMMAND_PHY_SET_REG;
			} else if(strcmp(optarg,"READ_BYTE")==0) {
				commandCode=COMMAND_READ_BYTE;
			} else if(strcmp(optarg,"READ_WORD")==0) {
				commandCode=COMMAND_READ_WORD;
			} else if(strcmp(optarg,"READ_DWORD")==0) {
				commandCode=COMMAND_READ_DWORD;
			} else if(strcmp(optarg,"WRITE_BYTE")==0) {
				commandCode=COMMAND_WRITE_BYTE;
			} else if(strcmp(optarg,"WRITE_WORD")==0) {
				commandCode=COMMAND_WRITE_WORD;
			} else if(strcmp(optarg,"WRITE_DWORD")==0) {
				commandCode=COMMAND_WRITE_DWORD;
			} else if(strcmp(optarg,"SERVER")==0) {
				commandCode=COMMAND_RUN_SERVER;
			} else if(strcmp(optarg,"TUNER")==0) {
				commandCode=COMMAND_RUN_TUNER;
			} else if(strcmp(optarg,"GET_FLOW")==0) {
				commandCode=COMMAND_GET_FLOW_PARAMS;
			} else {
				goto BAD_USAGE;
			}
			cSet=true;
			break;
		case 'a':
			if(aSet) goto BAD_USAGE;
			if(!ParseNumber(optarg,&address)) {
				goto BAD_USAGE;
			}
			aSet=true;
			break;
		case 'd':
			if(dSet) goto BAD_USAGE;
			if(!ParseNumber(optarg,&data)) {
				goto BAD_USAGE;
			}

			dSet=true;
			break;
		default:
			goto BAD_USAGE;
		}
	}

	if(commandCode==COMMAND_RUN_TUNER) {
		//This command must be run before initialize because
		//  it will typically be run on machines that do not
		//  have the smsc9118 driver installed.
		RunTuner(hostName,(unsigned short)portNum);
		return 1;
	}

	if(!Initialize(&commandData,ethName)) {
		return 1;
	}

	switch(commandCode) {
	case COMMAND_RUN_SERVER:
		RunServer(&commandData,(unsigned short)portNum);
		break;
	case COMMAND_GET_FLOW_PARAMS:
		GetFlowParams(&commandData);
		break;
	case COMMAND_GET_CONFIGURATION:
		GetConfiguration(&commandData);
		break;
	case COMMAND_DUMP_LAN_REGS:
		LanDumpRegs(&commandData);
		break;
	case COMMAND_DUMP_MAC_REGS:
		MacDumpRegs(&commandData);
		break;
	case COMMAND_DUMP_PHY_REGS:
		PhyDumpRegs(&commandData);
		break;
	case COMMAND_DUMP_EEPROM:
		DumpEEPROM(&commandData);
		break;
	case COMMAND_DUMP_TEMP:
		DumpTemp(&commandData);
		break;
	case COMMAND_GET_MAC_ADDRESS:
		GetMacAddress(&commandData);
		break;
	case COMMAND_SET_MAC_ADDRESS:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		SetMacAddress(&commandData,address,data);
		break;
	case COMMAND_LOAD_MAC_ADDRESS:
		LoadMacAddress(&commandData);
		break;
	case COMMAND_SAVE_MAC_ADDRESS:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		SaveMacAddress(&commandData,address,data);
		break;
	case COMMAND_SET_DEBUG_MODE:
		if(!dSet) goto BAD_USAGE;
		SetDebugMode(&commandData,data);
		break;
	case COMMAND_SET_LINK_MODE:
		if(!dSet) goto BAD_USAGE;
		SetLinkMode(&commandData,data);
		break;
	case COMMAND_GET_LINK_MODE:
		GetLinkMode(&commandData);
		break;
	case COMMAND_CHECK_LINK:
		CheckLink(&commandData);
		break;
	case COMMAND_SET_POWER_MODE:
		if(!dSet) goto BAD_USAGE;
		SetPowerMode(&commandData,data);
		break;
	case COMMAND_GET_POWER_MODE:
		GetPowerMode(&commandData);
		break;
	case COMMAND_LAN_GET_REG:
		if(!aSet) goto BAD_USAGE;
		LanGetReg(&commandData,address);
		break;
	case COMMAND_LAN_SET_REG:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		LanSetReg(&commandData,address,data);
		break;
	case COMMAND_MAC_GET_REG:
		if(!aSet) goto BAD_USAGE;
		MacGetReg(&commandData,address);
		break;
	case COMMAND_MAC_SET_REG:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		MacSetReg(&commandData,address,data);
		break;
	case COMMAND_PHY_GET_REG:
		if(!aSet) goto BAD_USAGE;
		PhyGetReg(&commandData,address);
		break;
	case COMMAND_PHY_SET_REG:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		PhySetReg(&commandData,address,data);
		break;
	case COMMAND_READ_BYTE:
		if(!aSet) goto BAD_USAGE;
		ReadByte(&commandData,address);
		break;
	case COMMAND_READ_WORD:
		if(!aSet) goto BAD_USAGE;
		ReadWord(&commandData,address);
		break;
	case COMMAND_READ_DWORD:
		if(!aSet) goto BAD_USAGE;
		ReadDWord(&commandData,address);
		break;
	case COMMAND_WRITE_BYTE:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		WriteByte(&commandData,address,data);
		break;
	case COMMAND_WRITE_WORD:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		WriteWord(&commandData,address,data);
		break;
	case COMMAND_WRITE_DWORD:
		if(!aSet) goto BAD_USAGE;
		if(!dSet) goto BAD_USAGE;
		WriteDWord(&commandData,address,data);
		break;
	default:
		goto BAD_USAGE;
	}

	return 1;
BAD_USAGE:
	DisplayUsage();
	return 1;
}
