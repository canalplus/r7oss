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
 * File: smsc9118.c
 *   see readme.txt for programmers guide
 */


#ifndef __KERNEL__
#	define __KERNEL__
#endif

#ifdef USING_LINT
#include "lint.h"
#else //not USING_LINT
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/bitops.h>
#include <linux/version.h>

#endif //not USING_LINT

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#define IRQ_RETVAL(x)
#else
#define LINUX_2_6_OR_NEWER
#endif

#ifdef USE_DEBUG
//select debug modes
#define USE_WARNING
#define USE_TRACE
#define USE_ASSERT
#endif //USE_DEBUG

#define	USE_LED1_WORK_AROUND	// 10/100 LED link-state inversion
#define	USE_PHY_WORK_AROUND		// output polarity inversion

typedef long TIME_SPAN;
#define MAX_TIME_SPAN	((TIME_SPAN)(0x7FFFFFFFUL))
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned char BOOLEAN;
#define TRUE	((BOOLEAN)1)
#define FALSE	((BOOLEAN)0)

#define HIBYTE(word)  ((BYTE)(((WORD)(word))>>8))
#define LOBYTE(word)  ((BYTE)(((WORD)(word))&0x00FFU))
#define HIWORD(dWord) ((WORD)(((DWORD)(dWord))>>16))
#define LOWORD(dWord) ((WORD)(((DWORD)(dWord))&0x0000FFFFUL))

#define TRANSFER_PIO			(256UL)
#define TRANSFER_REQUEST_DMA	(255UL)
//these are values that can be assigned to
//PLATFORM_RX_DMA
//PLATFORM_TX_DMA
// in addition to any specific dma channel

/*******************************************************
* Macro: SMSC_TRACE
* Description:
*    This macro is used like printf.
*    It can be used anywhere you want to display information
*    For any release version it should not be left in
*      performance sensitive Tx and Rx code paths.
*    To use this macro define USE_TRACE and set bit 0 of debug_mode
*******************************************************/
#ifdef USING_LINT
extern void SMSC_TRACE(const char * a, ...);
#else //not USING_LINT
#ifdef USE_TRACE
extern DWORD debug_mode;
#ifndef USE_WARNING
#define USE_WARNING
#endif
#	define SMSC_TRACE(msg,args...)			\
	if(debug_mode&0x01UL) {					\
		printk("SMSC: " msg "\n", ## args);	\
	}
#else
#	define SMSC_TRACE(msg,args...)
#endif
#endif //not USING_LINT

/*******************************************************
* Macro: SMSC_WARNING
* Description:
*    This macro is used like printf.
*    It can be used anywhere you want to display warning information
*    For any release version it should not be left in
*      performance sensitive Tx and Rx code paths.
*    To use this macro define USE_TRACE or
*      USE_WARNING and set bit 1 of debug_mode
*******************************************************/
#ifdef USING_LINT
extern void SMSC_WARNING(const char * a, ...);
#else //not USING_LINT
#ifdef USE_WARNING
extern DWORD debug_mode;
#ifndef USE_ASSERT
#define USE_ASSERT
#endif
#	define SMSC_WARNING(msg, args...)				\
	if(debug_mode&0x02UL) {							\
		printk("SMSC_WARNING: " msg "\n",## args);	\
	}
#else
#	define SMSC_WARNING(msg, args...)
#endif
#endif //not USING_LINT


/*******************************************************
* Macro: SMSC_ASSERT
* Description:
*    This macro is used to test assumptions made when coding.
*    It can be used anywhere, but is intended only for situations
*      where a failure is fatal.
*    If code execution where allowed to continue it is assumed that
*      only further unrecoverable errors would occur and so this macro
*      includes an infinite loop to prevent further corruption.
*    Assertions are only intended for use during developement to
*      insure consistency of logic through out the driver.
*    A driver should not be released if assertion failures are
*      still occuring.
*    To use this macro define USE_TRACE or USE_WARNING or
*      USE_ASSERT
*******************************************************/
#ifdef USING_LINT
extern void SMSC_ASSERT(BOOLEAN condition);
#else //not USING_LINT
#ifdef USE_ASSERT
#	define SMSC_ASSERT(condition)													\
	if(!(condition)) {																\
		printk("SMSC_ASSERTION_FAILURE: File=" __FILE__ ", Line=%d\n",__LINE__);	\
		while(1);																	\
	}
#else
#	define SMSC_ASSERT(condition)
#endif
#endif //not USING_LINT

//Below are the register offsets and bit definitions
//  of the Lan9118 memory space
#define RX_DATA_FIFO	    (0x00UL)

#define TX_DATA_FIFO        (0x20UL)
#define		TX_CMD_A_INT_ON_COMP_		(0x80000000UL)
#define		TX_CMD_A_INT_BUF_END_ALGN_	(0x03000000UL)
#define		TX_CMD_A_INT_4_BYTE_ALGN_	(0x00000000UL)
#define		TX_CMD_A_INT_16_BYTE_ALGN_	(0x01000000UL)
#define		TX_CMD_A_INT_32_BYTE_ALGN_	(0x02000000UL)
#define		TX_CMD_A_INT_DATA_OFFSET_	(0x001F0000UL)
#define		TX_CMD_A_INT_FIRST_SEG_		(0x00002000UL)
#define		TX_CMD_A_INT_LAST_SEG_		(0x00001000UL)
#define		TX_CMD_A_BUF_SIZE_			(0x000007FFUL)
#define		TX_CMD_B_PKT_TAG_			(0xFFFF0000UL)
#define		TX_CMD_B_ADD_CRC_DISABLE_	(0x00002000UL)
#define		TX_CMD_B_DISABLE_PADDING_	(0x00001000UL)
#define		TX_CMD_B_PKT_BYTE_LENGTH_	(0x000007FFUL)

#define RX_STATUS_FIFO      (0x40UL)
#define		RX_STS_ES_			(0x00008000UL)
#define		RX_STS_MCAST_		(0x00000400UL)
#define RX_STATUS_FIFO_PEEK (0x44UL)
#define TX_STATUS_FIFO		(0x48UL)
#define TX_STATUS_FIFO_PEEK (0x4CUL)
#define ID_REV              (0x50UL)
#define		ID_REV_CHIP_ID_		(0xFFFF0000UL)	// RO
#define		ID_REV_REV_ID_		(0x0000FFFFUL)	// RO

#define INT_CFG				(0x54UL)
#define		INT_CFG_INT_DEAS_	(0xFF000000UL)	// R/W
#define		INT_CFG_IRQ_INT_	(0x00001000UL)	// RO
#define		INT_CFG_IRQ_EN_		(0x00000100UL)	// R/W
#define		INT_CFG_IRQ_POL_	(0x00000010UL)	// R/W Not Affected by SW Reset
#define		INT_CFG_IRQ_TYPE_	(0x00000001UL)	// R/W Not Affected by SW Reset

#define INT_STS				(0x58UL)
#define		INT_STS_SW_INT_		(0x80000000UL)	// R/WC
#define		INT_STS_TXSTOP_INT_	(0x02000000UL)	// R/WC
#define		INT_STS_RXSTOP_INT_	(0x01000000UL)	// R/WC
#define		INT_STS_RXDFH_INT_	(0x00800000UL)	// R/WC
#define		INT_STS_RXDF_INT_	(0x00400000UL)	// R/WC
#define		INT_STS_TX_IOC_		(0x00200000UL)	// R/WC
#define		INT_STS_RXD_INT_	(0x00100000UL)	// R/WC
#define		INT_STS_GPT_INT_	(0x00080000UL)	// R/WC
#define		INT_STS_PHY_INT_	(0x00040000UL)	// RO
#define		INT_STS_PME_INT_	(0x00020000UL)	// R/WC
#define		INT_STS_TXSO_		(0x00010000UL)	// R/WC
#define		INT_STS_RWT_		(0x00008000UL)	// R/WC
#define		INT_STS_RXE_		(0x00004000UL)	// R/WC
#define		INT_STS_TXE_		(0x00002000UL)	// R/WC
#define		INT_STS_ERX_		(0x00001000UL)	// R/WC
#define		INT_STS_TDFU_		(0x00000800UL)	// R/WC
#define		INT_STS_TDFO_		(0x00000400UL)	// R/WC
#define		INT_STS_TDFA_		(0x00000200UL)	// R/WC
#define		INT_STS_TSFF_		(0x00000100UL)	// R/WC
#define		INT_STS_TSFL_		(0x00000080UL)	// R/WC
#define		INT_STS_RDFO_		(0x00000040UL)	// R/WC
#define		INT_STS_RDFL_		(0x00000020UL)	// R/WC
#define		INT_STS_RSFF_		(0x00000010UL)	// R/WC
#define		INT_STS_RSFL_		(0x00000008UL)	// R/WC
#define		INT_STS_GPIO2_INT_	(0x00000004UL)	// R/WC
#define		INT_STS_GPIO1_INT_	(0x00000002UL)	// R/WC
#define		INT_STS_GPIO0_INT_	(0x00000001UL)	// R/WC

#define INT_EN				(0x5CUL)
#define		INT_EN_SW_INT_EN_		(0x80000000UL)	// R/W
#define		INT_EN_TXSTOP_INT_EN_	(0x02000000UL)	// R/W
#define		INT_EN_RXSTOP_INT_EN_	(0x01000000UL)	// R/W
#define		INT_EN_RXDFH_INT_EN_	(0x00800000UL)	// R/W
#define		INT_EN_RXDF_INT_EN_		(0x00400000UL)	// R/W
#define		INT_EN_TIOC_INT_EN_		(0x00200000UL)	// R/W
#define		INT_EN_RXD_INT_EN_		(0x00100000UL)	// R/W
#define		INT_EN_GPT_INT_EN_		(0x00080000UL)	// R/W
#define		INT_EN_PHY_INT_EN_		(0x00040000UL)	// R/W
#define		INT_EN_PME_INT_EN_		(0x00020000UL)	// R/W
#define		INT_EN_TXSO_EN_			(0x00010000UL)	// R/W
#define		INT_EN_RWT_EN_			(0x00008000UL)	// R/W
#define		INT_EN_RXE_EN_			(0x00004000UL)	// R/W
#define		INT_EN_TXE_EN_			(0x00002000UL)	// R/W
#define		INT_EN_ERX_EN_			(0x00001000UL)	// R/W
#define		INT_EN_TDFU_EN_			(0x00000800UL)	// R/W
#define		INT_EN_TDFO_EN_			(0x00000400UL)	// R/W
#define		INT_EN_TDFA_EN_			(0x00000200UL)	// R/W
#define		INT_EN_TSFF_EN_			(0x00000100UL)	// R/W
#define		INT_EN_TSFL_EN_			(0x00000080UL)	// R/W
#define		INT_EN_RDFO_EN_			(0x00000040UL)	// R/W
#define		INT_EN_RDFL_EN_			(0x00000020UL)	// R/W
#define		INT_EN_RSFF_EN_			(0x00000010UL)	// R/W
#define		INT_EN_RSFL_EN_			(0x00000008UL)	// R/W
#define		INT_EN_GPIO2_INT_		(0x00000004UL)	// R/W
#define		INT_EN_GPIO1_INT_		(0x00000002UL)	// R/W
#define		INT_EN_GPIO0_INT_		(0x00000001UL)	// R/W

#define BYTE_TEST				(0x64UL)
#define FIFO_INT				(0x68UL)
#define		FIFO_INT_TX_AVAIL_LEVEL_	(0xFF000000UL)	// R/W
#define		FIFO_INT_TX_STS_LEVEL_		(0x00FF0000UL)	// R/W
#define		FIFO_INT_RX_AVAIL_LEVEL_	(0x0000FF00UL)	// R/W
#define		FIFO_INT_RX_STS_LEVEL_		(0x000000FFUL)	// R/W

#define RX_CFG					(0x6CUL)
#define		RX_CFG_RX_END_ALGN_		(0xC0000000UL)	// R/W
#define			RX_CFG_RX_END_ALGN4_		(0x00000000UL)	// R/W
#define			RX_CFG_RX_END_ALGN16_		(0x40000000UL)	// R/W
#define			RX_CFG_RX_END_ALGN32_		(0x80000000UL)	// R/W
#define		RX_CFG_RX_DMA_CNT_		(0x0FFF0000UL)	// R/W
#define		RX_CFG_RX_DUMP_			(0x00008000UL)	// R/W
#define		RX_CFG_RXDOFF_			(0x00001F00UL)	// R/W
#define		RX_CFG_RXBAD_			(0x00000001UL)	// R/W

#define TX_CFG					(0x70UL)
#define		TX_CFG_TX_DMA_LVL_		(0xE0000000UL)	// R/W
#define		TX_CFG_TX_DMA_CNT_		(0x0FFF0000UL)	// R/W Self Clearing
#define		TX_CFG_TXS_DUMP_		(0x00008000UL)	// Self Clearing
#define		TX_CFG_TXD_DUMP_		(0x00004000UL)	// Self Clearing
#define		TX_CFG_TXSAO_			(0x00000004UL)	// R/W
#define		TX_CFG_TX_ON_			(0x00000002UL)	// R/W
#define		TX_CFG_STOP_TX_			(0x00000001UL)	// Self Clearing

#define HW_CFG					(0x74UL)
#define		HW_CFG_TTM_				(0x00200000UL)	// R/W
#define		HW_CFG_SF_				(0x00100000UL)	// R/W
#define		HW_CFG_TX_FIF_SZ_		(0x000F0000UL)	// R/W
#define		HW_CFG_TR_				(0x00003000UL)	// R/W
#define     HW_CFG_PHY_CLK_SEL_		(0x00000060UL)  // R/W
#define         HW_CFG_PHY_CLK_SEL_INT_PHY_	(0x00000000UL) // R/W
#define         HW_CFG_PHY_CLK_SEL_EXT_PHY_	(0x00000020UL) // R/W
#define         HW_CFG_PHY_CLK_SEL_CLK_DIS_ (0x00000040UL) // R/W
#define     HW_CFG_SMI_SEL_			(0x00000010UL)  // R/W
#define     HW_CFG_EXT_PHY_DET_		(0x00000008UL)  // RO
#define     HW_CFG_EXT_PHY_EN_		(0x00000004UL)  // R/W
#define		HW_CFG_32_16_BIT_MODE_	(0x00000004UL)	// RO
#define     HW_CFG_SRST_TO_			(0x00000002UL)  // RO
#define		HW_CFG_SRST_			(0x00000001UL)	// Self Clearing

#define RX_DP_CTRL				(0x78UL)
#define		RX_DP_CTRL_RX_FFWD_		(0x00000FFFUL)	// R/W
#define		RX_DP_CTRL_FFWD_BUSY_	(0x80000000UL)	// RO

#define RX_FIFO_INF				(0x7CUL)
#define		RX_FIFO_INF_RXSUSED_	(0x00FF0000UL)	// RO
#define		RX_FIFO_INF_RXDUSED_	(0x0000FFFFUL)	// RO

#define TX_FIFO_INF				(0x80UL)
#define		TX_FIFO_INF_TSUSED_		(0x00FF0000UL)  // RO
#define		TX_FIFO_INF_TSFREE_		(0x00FF0000UL)	// RO
#define		TX_FIFO_INF_TDFREE_		(0x0000FFFFUL)	// RO

#define PMT_CTRL				(0x84UL)
#define		PMT_CTRL_PM_MODE_			(0x00018000UL)	// Self Clearing
#define		PMT_CTRL_PHY_RST_			(0x00000400UL)	// Self Clearing
#define		PMT_CTRL_WOL_EN_			(0x00000200UL)	// R/W
#define		PMT_CTRL_ED_EN_				(0x00000100UL)	// R/W
#define		PMT_CTRL_PME_TYPE_			(0x00000040UL)	// R/W Not Affected by SW Reset
#define		PMT_CTRL_WUPS_				(0x00000030UL)	// R/WC
#define			PMT_CTRL_WUPS_NOWAKE_		(0x00000000UL)	// R/WC
#define			PMT_CTRL_WUPS_ED_			(0x00000010UL)	// R/WC
#define			PMT_CTRL_WUPS_WOL_			(0x00000020UL)	// R/WC
#define			PMT_CTRL_WUPS_MULTI_		(0x00000030UL)	// R/WC
#define		PMT_CTRL_PME_IND_		(0x00000008UL)	// R/W
#define		PMT_CTRL_PME_POL_		(0x00000004UL)	// R/W
#define		PMT_CTRL_PME_EN_		(0x00000002UL)	// R/W Not Affected by SW Reset
#define		PMT_CTRL_READY_			(0x00000001UL)	// RO

#define GPIO_CFG				(0x88UL)
#define		GPIO_CFG_LED3_EN_		(0x40000000UL)	// R/W
#define		GPIO_CFG_LED2_EN_		(0x20000000UL)	// R/W
#define		GPIO_CFG_LED1_EN_		(0x10000000UL)	// R/W
#define		GPIO_CFG_GPIO2_INT_POL_	(0x04000000UL)	// R/W
#define		GPIO_CFG_GPIO1_INT_POL_	(0x02000000UL)	// R/W
#define		GPIO_CFG_GPIO0_INT_POL_	(0x01000000UL)	// R/W
#define		GPIO_CFG_EEPR_EN_		(0x00E00000UL)	// R/W
#define		GPIO_CFG_GPIOBUF2_		(0x00040000UL)	// R/W
#define		GPIO_CFG_GPIOBUF1_		(0x00020000UL)	// R/W
#define		GPIO_CFG_GPIOBUF0_		(0x00010000UL)	// R/W
#define		GPIO_CFG_GPIODIR2_		(0x00000400UL)	// R/W
#define		GPIO_CFG_GPIODIR1_		(0x00000200UL)	// R/W
#define		GPIO_CFG_GPIODIR0_		(0x00000100UL)	// R/W
#define		GPIO_CFG_GPIOD4_		(0x00000020UL)	// R/W
#define		GPIO_CFG_GPIOD3_		(0x00000010UL)	// R/W
#define		GPIO_CFG_GPIOD2_		(0x00000004UL)	// R/W
#define		GPIO_CFG_GPIOD1_		(0x00000002UL)	// R/W
#define		GPIO_CFG_GPIOD0_		(0x00000001UL)	// R/W

#define GPT_CFG					(0x8CUL)
#define		GPT_CFG_TIMER_EN_		(0x20000000UL)	// R/W
#define		GPT_CFG_GPT_LOAD_		(0x0000FFFFUL)	// R/W

#define GPT_CNT					(0x90UL)
#define		GPT_CNT_GPT_CNT_		(0x0000FFFFUL)	// RO

#define FPGA_REV				(0x94UL)
#define		FPGA_REV_FPGA_REV_		(0x0000FFFFUL)	// RO

#define ENDIAN					(0x98UL)
#define FREE_RUN				(0x9CUL)
#define RX_DROP					(0xA0UL)
#define MAC_CSR_CMD				(0xA4UL)
#define		MAC_CSR_CMD_CSR_BUSY_	(0x80000000UL)	// Self Clearing
#define		MAC_CSR_CMD_R_NOT_W_	(0x40000000UL)	// R/W
#define		MAC_CSR_CMD_CSR_ADDR_	(0x000000FFUL)	// R/W

#define MAC_CSR_DATA			(0xA8UL)
#define AFC_CFG					(0xACUL)
#define		AFC_CFG_AFC_HI_			(0x00FF0000UL)	// R/W
#define		AFC_CFG_AFC_LO_			(0x0000FF00UL)	// R/W
#define		AFC_CFG_BACK_DUR_		(0x000000F0UL)	// R/W
#define		AFC_CFG_FCMULT_			(0x00000008UL)	// R/W
#define		AFC_CFG_FCBRD_			(0x00000004UL)	// R/W
#define		AFC_CFG_FCADD_			(0x00000002UL)	// R/W
#define		AFC_CFG_FCANY_			(0x00000001UL)	// R/W

#define E2P_CMD					(0xB0UL)
#define		E2P_CMD_EPC_BUSY_		(0x80000000UL)	// Self Clearing
#define		E2P_CMD_EPC_CMD_		(0x70000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_READ_	(0x00000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_EWDS_	(0x10000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_EWEN_	(0x20000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_WRITE_	(0x30000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_WRAL_	(0x40000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_ERASE_	(0x50000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_ERAL_	(0x60000000UL)	// R/W
#define			E2P_CMD_EPC_CMD_RELOAD_	(0x70000000UL)  // R/W
#define		E2P_CMD_EPC_TIMEOUT_	(0x00000200UL)	// R
#define		E2P_CMD_MAC_ADDR_LOADED_	(0x00000100UL)	// RO
#define		E2P_CMD_EPC_ADDR_		(0x000000FFUL)	// R/W

#define E2P_DATA				(0xB4UL)
#define		E2P_DATA_EEPROM_DATA_	(0x000000FFUL)	// R/W
//end of lan register offsets and bit definitions

#define LAN_REGISTER_EXTENT		(0x00002000UL)

//The following describes the synchronization policies used in this driver.
//Register Name				Policy
//RX_DATA_FIFO				Only used by the Rx Thread, Rx_ProcessPackets
//TX_DATA_FIFO				Only used by the Tx Thread, Tx_SendSkb
//RX_STATUS_FIFO			Only used by the Rx Thread, Rx_ProcessPackets
//RX_STATUS_FIFO_PEEK		Not used.
//TX_STATUS_FIFO			Used in	Tx_CompleteTx in Tx_UpdateTxCounters.
//							protected by TxCounterLock
//TX_STATUS_FIFO_PEEK		Not used.
//ID_REV					Read only
//INT_CFG					Set in Lan_Initialize,
//							protected by IntEnableLock
//INT_STS					Sharable,
//INT_EN					Initialized at startup,
//							Used in Rx_ProcessPackets
//							otherwise protected by IntEnableLock
//BYTE_TEST					Read Only
//FIFO_INT					Initialized at startup,
//                          During run time only accessed by
//                              Tx_HandleInterrupt, and Tx_SendSkb and done in a safe manner
//RX_CFG					Used during initialization
//                          During runtime only used by Rx Thread
//TX_CFG					Only used during initialization
//HW_CFG					Only used during initialization
//RX_DP_CTRL				Only used in Rx Thread, in Rx_FastForward
//RX_FIFO_INF				Read Only, Only used in Rx Thread, in Rx_PopRxStatus
//TX_FIFO_INF				Read Only, Only used in Tx Thread, in Tx_GetTxStatusCount, Tx_SendSkb, Tx_CompleteTx
//PMT_CTRL					Not Used
//GPIO_CFG					used during initialization, in Lan_Initialize
//                          used for debugging
//                          used during EEPROM access.
//                          safe enough to not require a lock
//GPT_CFG					protected by GpTimerLock
//GPT_CNT					Not Used
//ENDIAN					Not Used
//FREE_RUN					Read only
//RX_DROP					Used in Rx Interrupt Handler,
//                          and get_stats.
//                          safe enough to not require a lock.
//MAC_CSR_CMD				Protected by MacPhyLock
//MAC_CSR_DATA				Protected by MacPhyLock
//                          Because the two previous MAC_CSR_ registers are protected
//                            All MAC, and PHY registers are protected as well.
//AFC_CFG					Used during initialization, in Lan_Initialize
//                          During run time, used in timer call back, in Phy_UpdateLinkMode
//E2P_CMD					Used during initialization, in Lan_Initialize
//                          Used in EEPROM functions
//E2P_DATA					Used in EEPROM functions

//DMA Transfer structure
typedef struct _DMA_XFER
{
	DWORD dwLanReg;
	DWORD *pdwBuf;
	DWORD dwDmaCh;
	DWORD dwDwCnt;
	BOOLEAN fMemWr;
} DMA_XFER;

typedef struct _FLOW_CONTROL_PARAMETERS
{
	DWORD MaxThroughput;
	DWORD MaxPacketCount;
	DWORD PacketCost;
	DWORD BurstPeriod;
	DWORD IntDeas;
} FLOW_CONTROL_PARAMETERS, *PFLOW_CONTROL_PARAMETERS;

#include PLATFORM_SOURCE
//PLATFORM_SOURCE is defined from the command line
//  with the -D option
//  example: -D"PLATFORM_SOURCE=\"platform.c\""

#define Lan_GetRegDW(dwOffset)	\
	Platform_GetRegDW(privateData->dwLanBase,dwOffset)

#define Lan_SetRegDW(dwOffset,dwVal) \
	Platform_SetRegDW(privateData->dwLanBase,dwOffset,dwVal)

#define Lan_ClrBitsDW(dwOffset,dwBits)						\
	Platform_SetRegDW(privateData->dwLanBase,				\
		dwOffset,Platform_GetRegDW(privateData->dwLanBase,	\
		dwOffset)&(~dwBits))

#define Lan_SetBitsDW(dwOffset,dwBits)						\
	Platform_SetRegDW(privateData->dwLanBase,				\
		dwOffset,Platform_GetRegDW(privateData->dwLanBase,	\
		dwOffset)|dwBits);

#define LINK_OFF				(0x00UL)
#define LINK_SPEED_10HD			(0x01UL)
#define LINK_SPEED_10FD			(0x02UL)
#define LINK_SPEED_100HD		(0x04UL)
#define LINK_SPEED_100FD		(0x08UL)
#define LINK_SYMMETRIC_PAUSE	(0x10UL)
#define LINK_ASYMMETRIC_PAUSE	(0x20UL)
#define LINK_AUTO_NEGOTIATE		(0x40UL)

#define MAX_RX_SKBS 10

typedef unsigned long VL_KEY;
typedef struct _VERIFIABLE_LOCK {
	spinlock_t Lock;
	VL_KEY KeyCode;
} VERIFIABLE_LOCK, * PVERIFIABLE_LOCK;

void Vl_InitLock(PVERIFIABLE_LOCK pVl);
BOOLEAN Vl_CheckLock(PVERIFIABLE_LOCK pVl,VL_KEY keyCode);
VL_KEY Vl_WaitForLock(PVERIFIABLE_LOCK pVl,DWORD *pdwIntFlags);
void Vl_ReleaseLock(PVERIFIABLE_LOCK pVl,VL_KEY keyCode,DWORD *pdwIntFlags);

typedef struct _PRIVATE_DATA {
	DWORD dwLanBase;
	DWORD dwLanBasePhy;
	DWORD dwIdRev;
	DWORD dwFpgaRev;
	struct net_device *dev;
	DWORD dwGeneration;//used to decide which workarounds apply

	spinlock_t IntEnableLock;
	BOOLEAN LanInitialized;
	VERIFIABLE_LOCK MacPhyLock;

	DWORD dwTxDmaCh;
	BOOLEAN TxDmaChReserved;
	DMA_XFER TxDmaXfer;
	DWORD dwTxDmaThreshold;
	DWORD dwTxQueueDisableMask;
	struct sk_buff *TxSkb;
	spinlock_t TxSkbLock;
	spinlock_t TxQueueLock;
	spinlock_t TxCounterLock;
	BOOLEAN TxInitialized;

	DWORD dwRxDmaCh;
	struct sk_buff *RxSkbs[MAX_RX_SKBS];
	struct scatterlist RxSgs[MAX_RX_SKBS];
	DWORD RxSkbsCount;
	DWORD RxSkbsMax;
	DWORD RxDropOnCallback;
	BOOLEAN RxDmaChReserved;
	DWORD dwRxDmaThreshold;
	BOOLEAN RxCongested;
	DWORD dwRxOffCount;
	BOOLEAN RxOverrun;
	DWORD RxOverrunCount;
	DWORD RxStatusDWReadCount;
	DWORD RxDataDWReadCount;
	DWORD RxPacketReadCount;
	DWORD RxFastForwardCount;
	DWORD RxPioReadCount;
	DWORD RxDmaReadCount;
	DWORD RxCongestedCount;
	DWORD RxDumpCount;
	DWORD LastReasonForReleasingCPU;
	DWORD LastRxStatus1;
	DWORD LastRxStatus2;
	DWORD LastRxStatus3;
	DWORD LastIntStatus1;
	DWORD LastIntStatus2;
	DWORD LastIntStatus3;
	DWORD RxUnloadProgress;
	DWORD RxUnloadPacketProgress;
	DWORD RxMaxDataFifoSize;

	DWORD RxFlowCurrentThroughput;
	DWORD RxFlowCurrentPacketCount;
	DWORD RxFlowCurrentWorkLoad;
	BOOLEAN MeasuringRxThroughput;
	DWORD RxFlowMeasuredMaxThroughput;
	DWORD RxFlowMeasuredMaxPacketCount;

//RX_FLOW_ACTIVATION specifies the percentage that RxFlowCurrentWorkLoad must exceed
//     RxFlowMaxWorkLoad in order to activate flow control
#define RX_FLOW_ACTIVATION	(4UL)

//RX_FLOW_DEACTIVATION specifies the percentage that RxFlowCurrentWorkLoad must reduce
//     from RxFlowMaxWorkLoad in order to deactivate flow control
#define RX_FLOW_DEACTIVATION (25UL)
	DWORD RxFlowMaxWorkLoad;

	FLOW_CONTROL_PARAMETERS RxFlowParameters;

	DWORD RxFlowBurstWorkLoad;
	DWORD RxFlowBurstMaxWorkLoad;
	BOOLEAN RxFlowControlActive;
	BOOLEAN RxFlowBurstActive;
	DWORD RxInterrupts;

#define GPT_SCHEDULE_DEPTH	(3)
	void *GptFunction[GPT_SCHEDULE_DEPTH];
	DWORD GptCallTime[GPT_SCHEDULE_DEPTH];
	DWORD Gpt_scheduled_slot_index;
	spinlock_t GpTimerLock;

	BOOLEAN Running;
	struct net_device_stats stats;

	DWORD dwPhyAddress;
	DWORD dwPhyId;
#ifdef USE_LED1_WORK_AROUND
	DWORD NotUsingExtPhy;
#endif
	BYTE bPhyModel;
	BYTE bPhyRev;
	DWORD dwLinkSpeed;
	DWORD dwLinkSettings;
	DWORD dwRemoteFaultCount;
	struct timer_list LinkPollingTimer;
	BOOLEAN StopLinkPolling;
	WORD wLastADV;
	WORD wLastADVatRestart;
#ifdef USE_PHY_WORK_AROUND
#define MIN_PACKET_SIZE (64)
	DWORD dwTxStartMargen;
	BYTE LoopBackTxPacket[MIN_PACKET_SIZE];
	DWORD dwTxEndMargen;
	DWORD dwRxStartMargen;
	BYTE LoopBackRxPacket[MIN_PACKET_SIZE];
	DWORD dwRxEndMargen;
	DWORD dwResetCount;
#endif

	BOOLEAN SoftwareInterruptSignal;

	PLATFORM_DATA PlatformData;

#define SMSC_IF_NAME_SIZE	(10)
	char ifName[SMSC_IF_NAME_SIZE];

	/* for Rx Multicast work around */
	volatile DWORD HashLo;
	volatile DWORD HashHi;
	volatile BOOLEAN MulticastUpdatePending;
	volatile DWORD set_bits_mask;
	volatile DWORD clear_bits_mask;

} PRIVATE_DATA, *PPRIVATE_DATA;


/*
 ****************************************************************************
 ****************************************************************************
 *	MAC Control and Status Register (Indirect Address)
 *	Offset (through the MAC_CSR CMD and DATA port)
 ****************************************************************************
 ****************************************************************************
 *
 */
#define MAC_CR				(0x01UL)	// R/W

	/* MAC_CR - MAC Control Register */
	#define MAC_CR_RXALL_		(0x80000000UL)
	#define MAC_CR_HBDIS_		(0x10000000UL)
	#define MAC_CR_RCVOWN_		(0x00800000UL)
	#define MAC_CR_LOOPBK_		(0x00200000UL)
	#define MAC_CR_FDPX_		(0x00100000UL)
	#define MAC_CR_MCPAS_		(0x00080000UL)
	#define MAC_CR_PRMS_		(0x00040000UL)
	#define MAC_CR_INVFILT_		(0x00020000UL)
	#define MAC_CR_PASSBAD_		(0x00010000UL)
	#define MAC_CR_HFILT_		(0x00008000UL)
	#define MAC_CR_HPFILT_		(0x00002000UL)
	#define MAC_CR_LCOLL_		(0x00001000UL)
	#define MAC_CR_BCAST_		(0x00000800UL)
	#define MAC_CR_DISRTY_		(0x00000400UL)
	#define MAC_CR_PADSTR_		(0x00000100UL)
	#define MAC_CR_BOLMT_MASK_	(0x000000C0UL)
	#define MAC_CR_DFCHK_		(0x00000020UL)
	#define MAC_CR_TXEN_		(0x00000008UL)
	#define MAC_CR_RXEN_		(0x00000004UL)

#define ADDRH				(0x02UL)	// R/W mask 0x0000FFFFUL
#define ADDRL				(0x03UL)	// R/W mask 0xFFFFFFFFUL
#define HASHH				(0x04UL)	// R/W
#define HASHL				(0x05UL)	// R/W

#define MII_ACC				(0x06UL)	// R/W
	#define MII_ACC_PHY_ADDR_	(0x0000F800UL)
	#define MII_ACC_MIIRINDA_	(0x000007C0UL)
	#define MII_ACC_MII_WRITE_	(0x00000002UL)
	#define MII_ACC_MII_BUSY_	(0x00000001UL)

#define MII_DATA			(0x07UL)	// R/W mask 0x0000FFFFUL

#define FLOW				(0x08UL)	// R/W
	#define FLOW_FCPT_			(0xFFFF0000UL)
	#define FLOW_FCPASS_		(0x00000004UL)
	#define FLOW_FCEN_			(0x00000002UL)
	#define FLOW_FCBSY_			(0x00000001UL)

#define VLAN1				(0x09UL)	// R/W mask 0x0000FFFFUL
#define VLAN2				(0x0AUL)	// R/W mask 0x0000FFFFUL

#define WUFF				(0x0BUL)	// WO

#define WUCSR				(0x0CUL)	// R/W
	#define WUCSR_GUE_			(0x00000200UL)
	#define WUCSR_WUFR_			(0x00000040UL)
	#define WUCSR_MPR_			(0x00000020UL)
	#define WUCSR_WAKE_EN_		(0x00000004UL)
	#define WUCSR_MPEN_			(0x00000002UL)

BOOLEAN Mac_Initialize(PPRIVATE_DATA privateData);
static BOOLEAN MacNotBusy(PPRIVATE_DATA privateData,VL_KEY keyCode);
DWORD Mac_GetRegDW(PPRIVATE_DATA privateData,DWORD dwRegOffset,VL_KEY keyCode);
void Mac_SetRegDW(PPRIVATE_DATA privateData,DWORD dwRegOffset,DWORD dwVal,VL_KEY keyCode);

/*
 ****************************************************************************
 *	Chip Specific MII Defines
 ****************************************************************************
 *
 *	Phy register offsets and bit definitions
 *
 */
#define LAN9118_PHY_ID	(0x00C0001C)

#define PHY_BCR		((DWORD)0U)
#define PHY_BCR_RESET_					((WORD)0x8000U)
#define PHY_BCR_LOOPBACK_			((WORD)0x4000U)
#define PHY_BCR_SPEED_SELECT_		((WORD)0x2000U)
#define PHY_BCR_AUTO_NEG_ENABLE_	((WORD)0x1000U)
#define PHY_BCR_RESTART_AUTO_NEG_	((WORD)0x0200U)
#define PHY_BCR_DUPLEX_MODE_		((WORD)0x0100U)

#define PHY_BSR		((DWORD)1U)
	#define PHY_BSR_LINK_STATUS_	((WORD)0x0004U)
	#define PHY_BSR_REMOTE_FAULT_	((WORD)0x0010U)
	#define PHY_BSR_AUTO_NEG_COMP_	((WORD)0x0020U)

#define PHY_ID_1	((DWORD)2U)
#define PHY_ID_2	((DWORD)3U)

#define PHY_ANEG_ADV    ((DWORD)4U)
#define PHY_ANEG_ADV_PAUSE_ ((WORD)0x0C00)
#define PHY_ANEG_ADV_ASYMP_	((WORD)0x0800)
#define PHY_ANEG_ADV_SYMP_	((WORD)0x0400)
#define PHY_ANEG_ADV_10H_	((WORD)0x20)
#define PHY_ANEG_ADV_10F_	((WORD)0x40)
#define PHY_ANEG_ADV_100H_	((WORD)0x80)
#define PHY_ANEG_ADV_100F_	((WORD)0x100)
#define PHY_ANEG_ADV_SPEED_	((WORD)0x1E0)

#define PHY_ANEG_LPA	((DWORD)5U)
#define PHY_ANEG_LPA_ASYMP_		((WORD)0x0800)
#define PHY_ANEG_LPA_SYMP_		((WORD)0x0400)
#define PHY_ANEG_LPA_100FDX_	((WORD)0x0100)
#define PHY_ANEG_LPA_100HDX_	((WORD)0x0080)
#define PHY_ANEG_LPA_10FDX_		((WORD)0x0040)
#define PHY_ANEG_LPA_10HDX_		((WORD)0x0020)

#define PHY_MODE_CTRL_STS		((DWORD)17)	// Mode Control/Status Register
	#define MODE_CTRL_STS_FASTRIP_		((WORD)0x4000U)
	#define MODE_CTRL_STS_EDPWRDOWN_	((WORD)0x2000U)
	#define MODE_CTRL_STS_LOWSQEN_		((WORD)0x0800U)
	#define MODE_CTRL_STS_MDPREBP_		((WORD)0x0400U)
	#define MODE_CTRL_STS_FARLOOPBACK_	((WORD)0x0200U)
	#define MODE_CTRL_STS_FASTEST_		((WORD)0x0100U)
	#define MODE_CTRL_STS_REFCLKEN_		((WORD)0x0010U)
	#define MODE_CTRL_STS_PHYADBP_		((WORD)0x0008U)
	#define MODE_CTRL_STS_FORCE_G_LINK_	((WORD)0x0004U)
	#define MODE_CTRL_STS_ENERGYON_		((WORD)0x0002U)

#define PHY_INT_SRC			((DWORD)29)
#define PHY_INT_SRC_ENERGY_ON_			((WORD)0x0080U)
#define PHY_INT_SRC_ANEG_COMP_			((WORD)0x0040U)
#define PHY_INT_SRC_REMOTE_FAULT_		((WORD)0x0020U)
#define PHY_INT_SRC_LINK_DOWN_			((WORD)0x0010U)

#define PHY_INT_MASK		((DWORD)30)
#define PHY_INT_MASK_ENERGY_ON_		((WORD)0x0080U)
#define PHY_INT_MASK_ANEG_COMP_		((WORD)0x0040U)
#define PHY_INT_MASK_REMOTE_FAULT_	((WORD)0x0020U)
#define PHY_INT_MASK_LINK_DOWN_		((WORD)0x0010U)

#define PHY_SPECIAL			((DWORD)31)
#define PHY_SPECIAL_SPD_	((WORD)0x001CU)
#define PHY_SPECIAL_SPD_10HALF_		((WORD)0x0004U)
#define PHY_SPECIAL_SPD_10FULL_		((WORD)0x0014U)
#define PHY_SPECIAL_SPD_100HALF_	((WORD)0x0008U)
#define PHY_SPECIAL_SPD_100FULL_	((WORD)0x0018U)

BOOLEAN Phy_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwPhyAddress,
	DWORD dwLinkMode);
void Phy_SetLink(PPRIVATE_DATA privateData,
				 DWORD dwLinkRequest);
WORD Phy_GetRegW(
	PPRIVATE_DATA privateData,
	DWORD dwRegIndex,
	VL_KEY keyCode);
void Phy_SetRegW(
	PPRIVATE_DATA privateData,
	DWORD dwRegIndex,
	WORD wVal,
	VL_KEY keyCode);
void Phy_UpdateLinkMode(
	PPRIVATE_DATA privateData);
void Phy_GetLinkMode(
	PPRIVATE_DATA privateData,
	VL_KEY keyCode);
void Phy_CheckLink(unsigned long ptr);

TIME_SPAN Gpt_FreeRunCompare(DWORD time1,DWORD time2);
void Gpt_ScheduleInterrupt(PPRIVATE_DATA privateData,TIME_SPAN timeSpan);
void Gpt_CancelInterrupt(PPRIVATE_DATA privateData);
void Gpt_CancelCallBack(
	PPRIVATE_DATA privateData,
	void (*callBackFunction)(PPRIVATE_DATA privateData));
void Gpt_ScheduleCallBack(
	PPRIVATE_DATA privateData,
	void (*callBackFunction)(PPRIVATE_DATA privateData),
	DWORD callBackTime);//100uS units relative to now
BOOLEAN Gpt_HandleInterrupt(
	PPRIVATE_DATA privateData,DWORD dwIntSts);
void GptCB_RxCompleteMulticast(PPRIVATE_DATA privateData);
void GptCB_RestartBurst(PPRIVATE_DATA privateData);
void GptCB_MeasureRxThroughput(PPRIVATE_DATA privateData);

void Tx_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwTxDmaCh,
	DWORD dwTxDmaThreshold);
void Tx_SendSkb(
	PPRIVATE_DATA privateData,
	struct sk_buff *skb);
BOOLEAN Tx_HandleInterrupt(
	PPRIVATE_DATA privateData,DWORD dwIntSts);

void Tx_StopQueue(
	PPRIVATE_DATA privateData,DWORD dwSource);
void Tx_WakeQueue(
	PPRIVATE_DATA privateData,DWORD dwSource);

static DWORD Tx_GetTxStatusCount(
	PPRIVATE_DATA privateData);
static void Tx_DmaCompletionCallback(void* param);
static DWORD Tx_CompleteTx(
	PPRIVATE_DATA privateData);
void Tx_UpdateTxCounters(
	PPRIVATE_DATA privateData);

void Tx_CompleteDma(
	PPRIVATE_DATA privateData);

void Rx_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwRxDmaCh,
	DWORD dwDmaThreshold);

void Rx_CompleteMulticastUpdate (PPRIVATE_DATA privateData);
static void Rx_HandleOverrun(PPRIVATE_DATA privateData);
static void Rx_HandOffSkb(
	PPRIVATE_DATA privateData,
	struct sk_buff *skb);
static DWORD Rx_PopRxStatus(
	PPRIVATE_DATA privateData);
void Rx_CountErrors(PPRIVATE_DATA privateData,DWORD dwRxStatus);
void Rx_FastForward(PPRIVATE_DATA privateData,DWORD dwDwordCount);
void Rx_ProcessPackets(PPRIVATE_DATA privateData);
void Rx_BeginMulticastUpdate (PPRIVATE_DATA privateData);

unsigned long Rx_TaskletParameter=0;

void Rx_ProcessPacketsTasklet(unsigned long data);
DECLARE_TASKLET(Rx_Tasklet,Rx_ProcessPacketsTasklet,0);

static void Rx_DmaCompletionCallback(void* param);

BOOLEAN RxStop_HandleInterrupt(
	PPRIVATE_DATA privateData,
	DWORD dwIntSts);
BOOLEAN Rx_HandleInterrupt(
	PPRIVATE_DATA privateData,
	DWORD dwIntSts);
static DWORD Rx_Hash(BYTE addr[6]);

void Rx_SetMulticastList(
	struct net_device *dev);
void Rx_ReceiverOff(
	PPRIVATE_DATA privateData);
void Rx_ReceiverOn(
	PPRIVATE_DATA privateData, VL_KEY callerKeyCode);


void Eeprom_EnableAccess(PPRIVATE_DATA privateData);
void Eeprom_DisableAccess(PPRIVATE_DATA privateData);

BOOLEAN Eeprom_IsMacAddressLoaded(PPRIVATE_DATA privateData);
BOOLEAN Eeprom_IsBusy(PPRIVATE_DATA privateData);
BOOLEAN Eeprom_Timeout(PPRIVATE_DATA privateData);

BOOLEAN Eeprom_ReadLocation(
	PPRIVATE_DATA privateData,BYTE address, BYTE * data);
BOOLEAN Eeprom_EnableEraseAndWrite(
	PPRIVATE_DATA privateData);
BOOLEAN Eeprom_DisableEraseAndWrite(
	PPRIVATE_DATA privateData);
BOOLEAN Eeprom_WriteLocation(
	PPRIVATE_DATA privateData,BYTE address,BYTE data);
BOOLEAN Eeprom_EraseAll(
	PPRIVATE_DATA privateData);
BOOLEAN Eeprom_Reload(
	PPRIVATE_DATA privateData);

BOOLEAN Eeprom_SaveMacAddress(
	PPRIVATE_DATA privateData,
	DWORD dwHi16,DWORD dwLo32);


#define OLD_REGISTERS(privData) (((privData->dwIdRev)==0x01180000UL)&& \
								 ((privData->dwFpgaRev)>=0x01)&& \
								 ((privData->dwFpgaRev)<=0x25))

extern volatile DWORD g_GpioSetting;
extern DWORD debug_mode;
#define GP_0	(0x01UL)
#define GP_1	(0x02UL)
#define GP_2	(0x04UL)
#define GP_3	(0x08UL)
#define GP_4	(0x10UL)
#define GP_OFF  (0x00UL)
#define GP_ISR	GP_OFF
#define GP_RX	GP_OFF
#define GP_TX	GP_OFF
#define GP_BEGIN_MULTICAST_UPDATE		GP_OFF
#define GP_COMPLETE_MULTICAST_UPDATE	GP_OFF

#define SET_GPIO(gpioBit)					\
if(debug_mode&0x04UL) {						\
	g_GpioSetting|=gpioBit;					\
	Lan_SetRegDW(GPIO_CFG,g_GpioSetting);	\
}

#define CLEAR_GPIO(gpioBit)					\
if(debug_mode&0x04UL) {						\
	g_GpioSetting&=(~gpioBit);				\
	Lan_SetRegDW(GPIO_CFG,g_GpioSetting);	\
}

#define PULSE_GPIO(gpioBit,count)	\
if(debug_mode&0x04UL) {				\
	DWORD pulseNum=0;				\
	/*make first pulse longer */	\
	SET_GPIO(gpioBit);				\
	while(pulseNum<count) {			\
		SET_GPIO(gpioBit);			\
		CLEAR_GPIO(gpioBit);		\
		pulseNum++;					\
	}								\
}
#ifdef USE_LED1_WORK_AROUND
volatile DWORD g_GpioSettingOriginal;
#endif

BOOLEAN Lan_Initialize(
	PPRIVATE_DATA privateData,DWORD dwIntCfg,
	DWORD dwTxFifSz,DWORD dwAfcCfg);
void Lan_EnableInterrupt(PPRIVATE_DATA privateData,DWORD dwIntEnMask);
void Lan_DisableInterrupt(PPRIVATE_DATA privateData,DWORD dwIntEnMask);
void Lan_EnableIRQ(PPRIVATE_DATA privateData);
void Lan_DisableIRQ(PPRIVATE_DATA privateData);
void Lan_SetIntDeas(PPRIVATE_DATA privateData,DWORD dwIntDeas);
void Lan_SetTDFL(PPRIVATE_DATA privateData,BYTE level);
void Lan_SetTSFL(PPRIVATE_DATA privateData,BYTE level);
void Lan_SetRDFL(PPRIVATE_DATA privateData,BYTE level);
void Lan_SetRSFL(PPRIVATE_DATA privateData,BYTE level);

void Lan_SignalSoftwareInterrupt(PPRIVATE_DATA privateData);
BOOLEAN Lan_HandleSoftwareInterrupt(PPRIVATE_DATA privateData,DWORD dwIntSts);

void Lan_ShowRegs(PPRIVATE_DATA privateData);

#include "ioctl_118.h"

DWORD lan_base=0x0UL;
module_param(lan_base,ulong,0);
MODULE_PARM_DESC(lan_base,"Base Address of LAN9118, (default: choosen by platform code)");

DWORD bus_width=0UL;
module_param(bus_width,ulong,0);
MODULE_PARM_DESC(bus_width,"Force bus width of 16 or 32 bits, default: autodetect");

DWORD link_mode=0x7FUL;
module_param(link_mode,ulong,0);
MODULE_PARM_DESC(link_mode,"Set Link speed and Duplex, 1=10HD,2=10FD,4=100HD,8=100FD,default=0xF");

DWORD irq=PLATFORM_IRQ;
module_param(irq,ulong,0);
MODULE_PARM_DESC(irq,"Force use of specific IRQ, (default: choosen by platform code)");

DWORD int_deas=0xFFFFFFFFUL;
module_param(int_deas,ulong,0);
MODULE_PARM_DESC(int_deas,"Interrupt Deassertion Interval in 10uS units");

DWORD irq_pol=PLATFORM_IRQ_POL;
module_param(irq_pol,ulong,0);
MODULE_PARM_DESC(irq_pol,"IRQ Polarity bit, see definition of INT_CFG register");

DWORD irq_type=PLATFORM_IRQ_TYPE;
module_param(irq_type,ulong,0);
MODULE_PARM_DESC(irq_type,"IRQ Buffer Type bit, see definition of INT_CFG register");

DWORD rx_dma=PLATFORM_RX_DMA;
module_param(rx_dma,ulong,0);
MODULE_PARM_DESC(rx_dma,"Receiver DMA Channel, 255=find available channel, 256=use PIO");

DWORD tx_dma=PLATFORM_TX_DMA;
module_param(tx_dma,ulong,0);
MODULE_PARM_DESC(tx_dma,"Transmitter DMA Channel, 255=find available channel, 256=use PIO");

DWORD dma_threshold=PLATFORM_DMA_THRESHOLD;
module_param(dma_threshold,ulong,0);
MODULE_PARM_DESC(dma_threshold,"Specifies the minimum packet size for DMA to be used.");

DWORD mac_addr_hi16=0xFFFFFFFFUL;
module_param(mac_addr_hi16,ulong,0);
MODULE_PARM_DESC(mac_addr_hi16,"Specifies the high 16 bits of the mac address");

DWORD mac_addr_lo32=0xFFFFFFFFUL;
module_param(mac_addr_lo32,ulong,0);
MODULE_PARM_DESC(mac_addr_lo32,"Specifies the low 32 bits of the mac address");

#ifdef USE_DEBUG
DWORD debug_mode=0x7UL;
#else
DWORD debug_mode=0x0UL;
#endif
module_param(debug_mode,ulong,0);
MODULE_PARM_DESC(debug_mode,"bit 0 enables trace points, bit 1 enables warning points, bit 2 enables gpios");

DWORD tx_fif_sz=0x00050000UL;
module_param(tx_fif_sz,ulong,0);
MODULE_PARM_DESC(tx_fif_sz,"Specifies TX_FIF_SZ of the HW_CFG register");

DWORD afc_cfg=0xFFFFFFFFUL;
module_param(afc_cfg,ulong,0);
MODULE_PARM_DESC(afc_cfg,"Specifies the setting for the AFC_CFG register");

DWORD tasklets=1UL;
module_param(tasklets,ulong,0);
MODULE_PARM_DESC(tasklets,"non-zero== use tasklets for receiving packets, zero==receive packets in ISR");

DWORD phy_addr=0xFFFFFFFFUL;
module_param(phy_addr,ulong,0);
MODULE_PARM_DESC(phy_addr,"phy_addr, 0xFFFFFFFF=use interal phy, 0-31=use external phy with specified address, else autodetect external phy addr");

DWORD max_throughput=0xFFFFFFFFUL;
module_param(max_throughput,ulong,0);
MODULE_PARM_DESC(max_throughput,"See readme.txt");

DWORD max_packet_count=0xFFFFFFFFUL;
module_param(max_packet_count,ulong,0);
MODULE_PARM_DESC(max_packet_count,"See Readme.txt");

DWORD packet_cost=0xFFFFFFFFUL;
module_param(packet_cost,ulong,0);
MODULE_PARM_DESC(packet_cost,"See Readme.txt");

DWORD burst_period=0xFFFFFFFFUL;
module_param(burst_period,ulong,0);
MODULE_PARM_DESC(burst_period,"See Readme.txt");

DWORD max_work_load=0xFFFFFFFFUL;
module_param(max_work_load,ulong,0);
MODULE_PARM_DESC(max_work_load,"See Readme.txt");

MODULE_LICENSE("GPL");

int Smsc9118_init(struct net_device *dev);
int Smsc9118_open(struct net_device *dev);
int Smsc9118_stop(struct net_device *dev);
int Smsc9118_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
struct net_device_stats * Smsc9118_get_stats(struct net_device *dev);
void Smsc9118_set_multicast_list(struct net_device *dev);
int Smsc9118_do_ioctl(struct net_device *dev, struct ifreq *ifr,int cmd);
irqreturn_t Smsc9118_ISR(int irq,void *dev_id);

#ifdef USING_LINT
struct net_device SMSC9118;
#else //not USING_LINT
struct net_device SMSC9118 = {init: Smsc9118_init,};
#endif //not USING_LINT

static int Smsc9118_probe(struct platform_device *pdev)
{
	int result=0;
	int device_present=0;

	SMSC_TRACE("--> probe()");

	if (lan_base == 0) {
		struct resource *res = platform_get_resource(pdev,
				IORESOURCE_MEM, 0);

		if (res)
			lan_base = res->start;
	}

	if (irq == PLATFORM_IRQ) {
		int res = platform_get_irq(pdev, 0);

		if (res >= 0)
			irq = res;
	}

	if (irq_pol == PLATFORM_IRQ_POL) {
		int res = platform_get_irq_byname(pdev, "polarity");

		if (res >= 0)
			irq_pol = res;
		else
			irq_pol = 0;
	}

	if (irq_type == PLATFORM_IRQ_TYPE) {
		int res = platform_get_irq_byname(pdev, "type");

		if (res >= 0)
			irq_type = res;
		else
			irq_type = 0;
	}

	SMSC_TRACE("Driver Version = %lX.%02lX",
		(DRIVER_VERSION>>8),(DRIVER_VERSION&0xFFUL));
	SMSC_TRACE("Compiled: %s, %s",__DATE__,__TIME__);
	SMSC_TRACE("Platform: %s",PLATFORM_NAME);
	SMSC_TRACE("Date Code: %s",date_code);
	SMSC_TRACE("Driver Parameters");
	if(lan_base==0UL) {
		SMSC_TRACE("  lan_base         = 0x%08lX, driver will decide",lan_base);
	} else {
		SMSC_TRACE("  lan_base         = 0x%08lX",lan_base);
	}
	if((bus_width==16UL)||(bus_width==32UL)) {
		SMSC_TRACE("  bus_width        = %ld",bus_width);
	} else {
		SMSC_TRACE("  bus_width        = %ld, driver will autodetect",bus_width);
	}
	if(link_mode>0x7FUL) {
		SMSC_WARNING("  link_mode     = %ld, Unknown",link_mode);
		link_mode=0x7FUL;
		SMSC_WARNING("    resetting link_mode to %ld, 100FD,100HD,10FD,10HD,ASYMP,SYMP,ANEG",link_mode);
	} else if(link_mode==0UL) {
		SMSC_TRACE("  link_mode        = %ld, LINK_OFF",link_mode);
	} else {
		SMSC_TRACE("  link_mode        = 0x%lX, %s,%s,%s,%s,%s,%s,%s",
			link_mode,
			(link_mode&LINK_SPEED_10HD)?"10HD":"",
			(link_mode&LINK_SPEED_10FD)?"10FD":"",
			(link_mode&LINK_SPEED_100HD)?"100HD":"",
			(link_mode&LINK_SPEED_100FD)?"100FD":"",
			(link_mode&LINK_ASYMMETRIC_PAUSE)?"ASYMP":"",
			(link_mode&LINK_SYMMETRIC_PAUSE)?"SYMP":"",
			(link_mode&LINK_AUTO_NEGOTIATE)?"ANEG":"");
	}
	SMSC_TRACE(    "  irq              = %ld",irq);
	if(int_deas!=0xFFFFFFFFUL) {
		if(int_deas>0xFFUL) {
			SMSC_WARNING("  int_deas     = %ld, too high",int_deas);
			int_deas=0xFFFFFFFFUL;
			SMSC_WARNING("    resetting int_deas to %ld",int_deas);
		}
	}
	if(int_deas==0xFFFFFFFFUL) {
		SMSC_TRACE(    "  int_deas         = 0x%08lX, use platform default",int_deas);
	} else {
		SMSC_TRACE(    "  int_deas         = %ld, %lduS",int_deas,10UL*int_deas);
	}
	if(irq_pol) {
		SMSC_TRACE("  irq_pol          = %ld, IRQ output is active high",irq_pol);
	} else {
		SMSC_TRACE("  irq_pol          = %ld, IRQ output is active low",irq_pol);
	}
	if(irq_type) {
		SMSC_TRACE("  irq_type         = %ld, IRQ output is Push-Pull driver",irq_type);
	} else {
		SMSC_TRACE("  irq_type         = %ld, IRQ output is Open-Drain buffer",irq_type);
	}
	if(rx_dma<TRANSFER_REQUEST_DMA) {
		if(Platform_IsValidDmaChannel(rx_dma)) {
			SMSC_TRACE(
				   "  rx_dma           = %ld, DMA Channel %ld",rx_dma,rx_dma);
		} else {
			SMSC_WARNING("  rx_dma        = %ld, Invalid Dma Channel",rx_dma);
			rx_dma=TRANSFER_PIO;
			SMSC_WARNING("    resetting rx_dma to %ld, RX will use PIO",rx_dma);
		}
	} else if(rx_dma==TRANSFER_REQUEST_DMA) {
		SMSC_TRACE("  rx_dma           = %ld, RX will try to find available channel",rx_dma);
	} else {
		SMSC_TRACE("  rx_dma           = %ld, RX will use PIO",rx_dma);
	}
	if(tx_dma<TRANSFER_REQUEST_DMA) {
		if(Platform_IsValidDmaChannel(tx_dma)) {
			if(tx_dma!=rx_dma) {
				SMSC_TRACE(
				   "  tx_dma           = %ld, DMA Channel %ld",tx_dma,tx_dma);
			} else {
				SMSC_WARNING("  tx_dma == rx_dma");
				tx_dma=TRANSFER_PIO;
				SMSC_WARNING("    resetting tx_dma to %ld, TX will use PIO",tx_dma);
			}
		} else {
			SMSC_WARNING("  tx_dma        = %ld, Invalid Dma Channel",tx_dma);
			tx_dma=TRANSFER_PIO;
			SMSC_WARNING("    resetting tx_dma to %ld, TX will use PIO",tx_dma);
		}
	} else if(tx_dma==TRANSFER_REQUEST_DMA) {
		SMSC_TRACE("  tx_dma           = %ld, TX will try to find available channel",tx_dma);
	} else {
		SMSC_TRACE("  tx_dma           = %ld, TX will use PIO",tx_dma);
	}
	SMSC_TRACE(    "  dma_threshold    = %ld",dma_threshold);

	if(mac_addr_hi16==0xFFFFFFFFUL) {
		SMSC_TRACE("  mac_addr_hi16    = 0x%08lX, will attempt to read from LAN9118",mac_addr_hi16);
		SMSC_TRACE("  mac_addr_lo32    = 0x%08lX, will attempt to read from LAN9118",mac_addr_lo32);
	} else {
		if(mac_addr_hi16&0xFFFF0000UL) {
			//The high word is reserved
			SMSC_WARNING("  mac_addr_hi16 = 0x%08lX, reserved bits are high.",mac_addr_hi16);
			mac_addr_hi16&=0x0000FFFFUL;
			SMSC_WARNING("    reseting to mac_addr_hi16 = 0x%08lX",mac_addr_hi16);
		}
		if(mac_addr_lo32&0x00000001UL) {
			//bit 0 is the I/G bit
			SMSC_WARNING("  mac_addr_lo32 = 0x%08lX, I/G bit is set.",mac_addr_lo32);
			mac_addr_lo32&=0xFFFFFFFEUL;
			SMSC_WARNING("    reseting to mac_addr_lo32 = 0x%08lX",mac_addr_lo32);
		}
		SMSC_TRACE("  mac_addr_hi16    = 0x%08lX",mac_addr_hi16);
		SMSC_TRACE("  mac_addr_lo32    = 0x%08lX",mac_addr_lo32);
	}
	SMSC_TRACE(    "  debug_mode       = 0x%08lX",debug_mode);
	if(tx_fif_sz&(~HW_CFG_TX_FIF_SZ_)) {
		SMSC_WARNING("tx_fif_sz = 0x%08lX is invalid",tx_fif_sz);
		tx_fif_sz&=HW_CFG_TX_FIF_SZ_;
		SMSC_WARNING("  resetting tx_fif_sz to 0x%08lX",tx_fif_sz);
	}
	if(tx_fif_sz>0x000E0000UL) {
		SMSC_WARNING("tx_fif_sz = 0x%08lX is too high",tx_fif_sz);
		tx_fif_sz=0x000E0000UL;
		SMSC_WARNING(" resetting tx_fif_sz to 0x%08lX",tx_fif_sz);
	}
	if(tx_fif_sz<0x00020000UL) {
		SMSC_WARNING("tx_fif_sz = 0x%08lX is too low",tx_fif_sz);
		tx_fif_sz=0x00020000UL;
		SMSC_WARNING(" resetting tx_fif_sz to 0x%08lX",tx_fif_sz);
	}
	SMSC_TRACE(    "  tx_fif_sz        = 0x%08lX",tx_fif_sz);
	if(afc_cfg==0xFFFFFFFFUL) {
		SMSC_TRACE("  afc_cfg          = 0x%08lX, driver will decide",afc_cfg);
	} else {
		if(afc_cfg&0xFF000000UL) {
			SMSC_WARNING("afc_cfg = 0x%08lX is invalid",afc_cfg);
			afc_cfg&=0xFFFFFFFFUL;
			SMSC_WARNING(" resetting to afc_cfg = 0x%08lX, driver will decide",afc_cfg);
		} else {
			SMSC_TRACE(
				   "  afc_cfg          = 0x%08lX",afc_cfg);
		}
	}
	if(tasklets) {
		SMSC_TRACE("  tasklets         = 0x%08lX, Tasklets enabled",tasklets);
	} else {
		SMSC_TRACE("  tasklets         = 0, Tasklets disabled");
	}
	if(phy_addr==0xFFFFFFFFUL) {
		SMSC_TRACE("  phy_addr         = 0xFFFFFFFF, Use internal phy");
	} else if(phy_addr<=31UL) {
		SMSC_TRACE("  phy_addr         = 0x%08lX, use this address for external phy",phy_addr);
	} else {
		SMSC_TRACE("  phy_addr         = 0x%08lX, auto detect external phy",phy_addr);
	}
	if(max_throughput) {
		SMSC_TRACE("  max_throughput   = 0x%08lX, Use platform default",max_throughput);
	} else {
		SMSC_TRACE("  max_throughput   = 0x%08lX",max_throughput);
	}
	if(max_packet_count) {
		SMSC_TRACE("  max_packet_count = 0x%08lX, Use platform default",max_packet_count);
	} else {
		SMSC_TRACE("  max_packet_count = 0x%08lX",max_packet_count);
	}
	if(packet_cost) {
		SMSC_TRACE("  packet_cost      = 0x%08lX, Use platform default",packet_cost);
	} else {
		SMSC_TRACE("  packet_cost      = 0x%08lX",packet_cost);
	}
	if(burst_period) {
		SMSC_TRACE("  burst_period     = 0x%08lX, Use platform default",burst_period);
	} else {
		SMSC_TRACE("  burst_period     = 0x%08lX",burst_period);
	}
	if(max_work_load) {
		SMSC_TRACE("  max_work_load    = 0x%08lX, Use platform default",max_work_load);
	} else {
		SMSC_TRACE("  max_work_load    = 0x%08lX",max_work_load);
	}

	strcpy(SMSC9118.name,"eth%d");

	result=register_netdev(&SMSC9118);
	if(result) {
		SMSC_WARNING("error %i registering device",result);
	} else {
		device_present=1;
		SMSC_TRACE("  Interface Name = \"%s\"",SMSC9118.name);
	}
	result=result;//make lint happy
	SMSC_TRACE("<-- probe()");
	return device_present ? 0 : -ENODEV;
}

static int Smsc9118_remove(struct platform_device *pdev)
{
	SMSC_TRACE("--> remove()");
	if(SMSC9118.priv!=NULL) {
		PPRIVATE_DATA privateData=(PPRIVATE_DATA)SMSC9118.priv;
		PPLATFORM_DATA platformData=(PPLATFORM_DATA)&(privateData->PlatformData);
		Platform_CleanUp(platformData);
		kfree(SMSC9118.priv);
		SMSC9118.priv=NULL;
	}
	unregister_netdev(&SMSC9118);
	SMSC_TRACE("<-- remove()");
	return 0;
}

int Smsc9118_init(struct net_device *dev)
{
	DWORD dwIdRev=0UL;
	DWORD dwFpgaRev=0UL;
	PPRIVATE_DATA privateData=NULL;
	PPLATFORM_DATA platformData=NULL;
	BOOLEAN platformInitialized=FALSE;
	int result=-ENODEV;
	SMSC_TRACE("-->Smsc9118_init(dev=0x%08lX)",(DWORD)dev);

	if(dev==NULL) {
		SMSC_WARNING("Smsc9118_init(dev==NULL)");
		result=-EFAULT;
		goto DONE;
	}

	if(dev->priv!=NULL) {
		SMSC_WARNING("dev->priv!=NULL, going to overwrite pointer");
	}
	dev->priv=kmalloc(sizeof(PRIVATE_DATA),GFP_KERNEL);
	if(dev->priv==NULL) {
		SMSC_WARNING("Unable to allocate PRIVATE_DATA");
		result=-ENOMEM;
		goto DONE;
	}
	memset(dev->priv,0,sizeof(PRIVATE_DATA));
	privateData=(PPRIVATE_DATA)(dev->priv);
	platformData=&(privateData->PlatformData);

	privateData->dwLanBasePhy=Platform_Initialize(
		platformData,
		lan_base,bus_width);

	privateData->dwLanBase = (DWORD)ioremap(privateData->dwLanBasePhy, 0x100);

	SMSC_TRACE("Lan Base at 0x%08lX",privateData->dwLanBase);

	if(privateData->dwLanBase==0UL) {
		SMSC_WARNING("dwLanBase==0x00000000");
		result=-ENODEV;
		goto DONE;
	}
	platformInitialized=TRUE;
	SMSC_TRACE("dwLanBase=0x%08lX",privateData->dwLanBase);

	if(check_mem_region(privateData->dwLanBase,LAN_REGISTER_EXTENT)!=0) {
		SMSC_WARNING("  Memory Region specified (0x%08lX to 0x%08lX) is not available.",
			privateData->dwLanBase,privateData->dwLanBase+LAN_REGISTER_EXTENT-1UL);
		result=-ENOMEM;
		goto DONE;
	}

	dwIdRev=Lan_GetRegDW(ID_REV);
	privateData->PlatformData.dwIdRev = dwIdRev;

	if(HIWORD(dwIdRev)==LOWORD(dwIdRev)) {
		//this may mean the chip is set for 32 bit
		//  while the bus is reading as 16 bit
UNKNOWN_CHIP:
		SMSC_WARNING("  LAN9118 Family NOT Identified, dwIdRev==0x%08lX",dwIdRev);
		result=-ENODEV;
		goto DONE;
	}
	switch(dwIdRev&0xFFFF0000UL) {
	case 0x01180000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9118 Beacon identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=0;
			break;
		case 1UL:
			SMSC_TRACE("  LAN9118 Concord A0 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=1;
			break;
		case 2UL:
			SMSC_TRACE("  LAN9118 Concord A1 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		default:
			SMSC_TRACE("  LAN9118 Concord A1 identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		};break;
	case 0x01170000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9117 Beacon identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=0;
			break;
		case 1UL:
			SMSC_TRACE("  LAN9117 Concord A0 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=1;
			break;
		case 2UL:
			SMSC_TRACE("  LAN9117 Concord A1 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		default:
			SMSC_TRACE("  LAN9117 Concord A1 identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		};break;
	case 0x01160000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			goto UNKNOWN_CHIP;
		case 1UL:
			SMSC_TRACE("  LAN9116 Concord A0 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=1;
			break;
		case 2UL:
			SMSC_TRACE("  LAN9116 Concord A1 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		default:
			SMSC_TRACE("  LAN9116 Concord A1 identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		};break;
	case 0x01150000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			goto UNKNOWN_CHIP;
		case 1UL:
			SMSC_TRACE("  LAN9115 Concord A0 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=1;
			break;
		case 2UL:
			SMSC_TRACE("  LAN9115 Concord A1 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		default:
			SMSC_TRACE("  LAN9115 Concord A1 identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		};break;
	case 0x01120000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			goto UNKNOWN_CHIP;
		case 1UL:
			SMSC_TRACE("  LAN9112 Concord A0 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=1;
			break;
		case 2UL:
			SMSC_TRACE("  LAN9112 Concord A1 identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		default:
			SMSC_TRACE("  LAN9112 Concord A1 identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=2;
			break;
		};break;
	case 0x118A0000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9218 Boylston identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		default:
			SMSC_TRACE("  LAN9218 Boylston identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		};break;
	case 0x117A0000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9217 Boylston identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		default:
			SMSC_TRACE("  LAN9217 Boylston identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		};break;
	case 0x116A0000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9216 Boylston identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		default:
			SMSC_TRACE("  LAN9216 Boylston identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		};break;
	case 0x115A0000UL:
		switch(dwIdRev&0x0000FFFFUL) {
		case 0UL:
			SMSC_TRACE("  LAN9215 Boylston identified, dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		default:
			SMSC_TRACE("  LAN9215 Boylston identified (NEW), dwIdRev==0x%08lX",dwIdRev);
			privateData->dwGeneration=3;
			break;
		};break;
	default:
		SMSC_TRACE("  unrecognised dwIdRev=0x%08lX",dwIdRev);
		break;
	}
	dwFpgaRev=Lan_GetRegDW(FPGA_REV);
	SMSC_TRACE("  FPGA_REV == 0x%08lX",dwFpgaRev);

	ether_setup(dev);
	dev->open=				Smsc9118_open;
	dev->stop=				Smsc9118_stop;
	dev->hard_start_xmit=	Smsc9118_hard_start_xmit;
	dev->get_stats=			Smsc9118_get_stats;
	dev->do_ioctl=			Smsc9118_do_ioctl;
	dev->set_multicast_list=Smsc9118_set_multicast_list;
	dev->flags|=IFF_MULTICAST;

	// set an invalid MAC address (so we can observe if its value is changed)
	memset(dev->dev_addr, 0xff, 6);

	SET_MODULE_OWNER(dev);

	privateData->dwIdRev=dwIdRev;
	privateData->dwFpgaRev=dwFpgaRev&(0x000000FFUL);
	privateData->dev=dev;

	sprintf(privateData->ifName,"%s",dev->name);
	result=0;

DONE:
	if(result!=0) {
		if(dev!=NULL) {
			if(dev->priv!=NULL) {
				if(platformInitialized) {
					Platform_CleanUp(platformData);
				}
				kfree(dev->priv);
				dev->priv=NULL;
			}
		}
	}
	SMSC_TRACE("<--Smsc9118_init(), result=%d",result);
	return result;
}

int Smsc9118_open(struct net_device *dev)
{
	int i;
	int result=-ENODEV;
	PPRIVATE_DATA privateData=NULL;
	PPLATFORM_DATA platformData=NULL;
	BOOLEAN acquired_mem_region=FALSE;
	BOOLEAN acquired_isr=FALSE;
	SMSC_TRACE("-->Smsc9118_open(dev=0x%08lX)",(DWORD)dev);
	if(dev==NULL) {
		SMSC_WARNING("Smsc9118_open(dev==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	privateData=(PPRIVATE_DATA)(dev->priv);
	if(privateData==NULL) {
		SMSC_WARNING("Smsc9118_open(privateData==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	platformData=&(privateData->PlatformData);

	for (i = 0; i < GPT_SCHEDULE_DEPTH; i++) {
		privateData->GptFunction [i] = NULL;
	}
	privateData->Gpt_scheduled_slot_index = GPT_SCHEDULE_DEPTH;

	//get memory region
	if(check_mem_region(privateData->dwLanBase,LAN_REGISTER_EXTENT)!=0)
	{
		SMSC_WARNING("Device memory is already in use.");
		result=-ENOMEM;
		goto DONE;
	}
	request_mem_region(privateData->dwLanBase,LAN_REGISTER_EXTENT,"SMSC_LAN9118");
	acquired_mem_region=TRUE;

	//initialize the LAN9118
	{
		DWORD dwIntCfg=0;
		if(irq_pol) {
			dwIntCfg|=INT_CFG_IRQ_POL_;
		}
		if(irq_type) {
			dwIntCfg|=INT_CFG_IRQ_TYPE_;
		}
		if(!Lan_Initialize(privateData,dwIntCfg,tx_fif_sz,afc_cfg))
		{
			SMSC_WARNING("Failed Lan_Initialize");
			result=-ENODEV;
			goto DONE;
		}
	}

	if(!Platform_RequestIRQ(platformData,irq,Smsc9118_ISR,privateData)) {
		result=-ENODEV;
		goto DONE;
	}
	acquired_isr=TRUE;

	//must now test the IRQ connection to the ISR
	SMSC_TRACE("Testing ISR using IRQ %ld",Platform_CurrentIRQ(platformData));
	{
		DWORD dwTimeOut=100000;
		Lan_SignalSoftwareInterrupt(privateData);
		do {
			udelay(10);
			dwTimeOut--;
		} while((dwTimeOut)&&(!(privateData->SoftwareInterruptSignal)));
		if(!(privateData->SoftwareInterruptSignal)) {
			SMSC_WARNING("ISR failed signaling test");
			result=-ENODEV;
			goto DONE;
		}
	}
	SMSC_TRACE("ISR passed test using IRQ %ld",Platform_CurrentIRQ(platformData));

	if(!Mac_Initialize(privateData)) {
		SMSC_WARNING("Failed Mac_Initialize");
		result=-ENODEV;
		goto DONE;
	}
	{//get mac address
		DWORD dwHigh16=0;
		DWORD dwLow32=0;
		DWORD dwIntFlags=0;
		VL_KEY keyCode;

		// if the dev_addr has been set use via set_mac_address then let this override everything
		if (!(dev->dev_addr[0] == 0xff && dev->dev_addr[1] == 0xff &&
		      dev->dev_addr[2] == 0xff && dev->dev_addr[3] == 0xff &&
		      dev->dev_addr[4] == 0xff && dev->dev_addr[5] == 0xff)) {
			mac_addr_lo32 = dev->dev_addr[0]       | dev->dev_addr[1] <<  8 |
			                dev->dev_addr[2] << 16 | dev->dev_addr[3] << 24;
			mac_addr_hi16 = dev->dev_addr[4]       | dev->dev_addr[5] <<  8;
		}

		keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
		if(mac_addr_hi16==0xFFFFFFFF) {
			dwHigh16=Mac_GetRegDW(privateData,ADDRH,keyCode);
			dwLow32=Mac_GetRegDW(privateData,ADDRL,keyCode);
			if((dwHigh16==0x0000FFFFUL)&&(dwLow32==0xFFFFFFFF))
			{
				dwHigh16=0x00000070UL;
				dwLow32=0x110F8000UL;
				Mac_SetRegDW(privateData,ADDRH,dwHigh16,keyCode);
				Mac_SetRegDW(privateData,ADDRL,dwLow32,keyCode);
				SMSC_TRACE("Mac Address is set by default to 0x%04lX%08lX",
					dwHigh16,dwLow32);
			} else {
				SMSC_TRACE("Mac Address is read from LAN9118 as 0x%04lX%08lX",
					dwHigh16,dwLow32);
			}
		} else {
			//SMSC_ASSERT((mac_addr_hi16&0xFFFF8000UL)==0);
			dwHigh16=mac_addr_hi16;
			dwLow32=mac_addr_lo32;
			Mac_SetRegDW(privateData,ADDRH,dwHigh16,keyCode);
			Mac_SetRegDW(privateData,ADDRL,dwLow32,keyCode);
			SMSC_TRACE("Mac Address is set by parameter to 0x%04lX%08lX",
				dwHigh16,dwLow32);
		}
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
		dev->dev_addr[0]=LOBYTE(LOWORD(dwLow32));
		dev->dev_addr[1]=HIBYTE(LOWORD(dwLow32));
		dev->dev_addr[2]=LOBYTE(HIWORD(dwLow32));
		dev->dev_addr[3]=HIBYTE(HIWORD(dwLow32));
		dev->dev_addr[4]=LOBYTE(LOWORD(dwHigh16));
		dev->dev_addr[5]=HIBYTE(LOWORD(dwHigh16));
	}

	privateData->MulticastUpdatePending = FALSE;

#ifdef USE_PHY_WORK_AROUND
	netif_carrier_off(dev);
	if(!Phy_Initialize(
		privateData,
		phy_addr,
		link_mode))
	{
		SMSC_WARNING("Failed to initialize Phy");
		result=-ENODEV;
		goto DONE;
	}
#endif

	{
		DWORD dwRxDmaCh=rx_dma;
		DWORD dwTxDmaCh=tx_dma;
		privateData->RxDmaChReserved=FALSE;

	spin_lock_init(&(privateData->GpTimerLock));

		if(rx_dma==TRANSFER_REQUEST_DMA) {
			dwRxDmaCh=Platform_RequestDmaChannelSg(&(privateData->PlatformData));
			SMSC_ASSERT(dwRxDmaCh!=TRANSFER_REQUEST_DMA);
			if(dwRxDmaCh<TRANSFER_REQUEST_DMA) {
				privateData->RxDmaChReserved=TRUE;
			}
		}
		privateData->TxDmaChReserved=FALSE;
		if(tx_dma==TRANSFER_REQUEST_DMA) {
			dwTxDmaCh=Platform_RequestDmaChannel(&(privateData->PlatformData));
			SMSC_ASSERT(dwTxDmaCh!=TRANSFER_REQUEST_DMA);
			if(dwTxDmaCh<TRANSFER_REQUEST_DMA) {
				privateData->TxDmaChReserved=TRUE;
			}
		}
		Tx_Initialize(privateData,dwTxDmaCh,dma_threshold);
		Rx_Initialize(privateData,dwRxDmaCh,dma_threshold);
	}

#ifndef LINUX_2_6_OR_NEWER
	MOD_INC_USE_COUNT;
#endif
	privateData->Running=TRUE;
	netif_start_queue(dev);
	Tx_StopQueue(privateData,0x01UL);


	Lan_EnableInterrupt(privateData,INT_EN_GPT_INT_EN_);
#ifndef USE_PHY_WORK_AROUND
	netif_carrier_off(dev);
	if(!Phy_Initialize(
		privateData,
		phy_addr,
		link_mode))
	{
		SMSC_WARNING("Failed to initialize Phy");
		result=-ENODEV;
		goto DONE;
	}
#endif

	result=0;

DONE:
	if(result!=0) {
#ifndef LINUX_2_6_OR_NEWER
		MOD_DEC_USE_COUNT;
#endif
		if(privateData!=NULL) {
			if(privateData->TxDmaChReserved) {
				Platform_ReleaseDmaChannel(platformData,
					privateData->dwTxDmaCh);
				privateData->TxDmaChReserved=FALSE;
			}
			if(privateData->RxDmaChReserved) {
				Platform_ReleaseDmaChannel(platformData,
					privateData->dwRxDmaCh);
				privateData->RxDmaChReserved=FALSE;
			}
			if(acquired_isr) {
				Platform_FreeIRQ(platformData);
			}
			if(acquired_mem_region) {
				release_mem_region(
					privateData->dwLanBase,
					LAN_REGISTER_EXTENT);
			}
		}
	}
	SMSC_TRACE("<--Smsc9118_open, result=%d",result);
	return result;
}

int Smsc9118_stop(struct net_device *dev)
{
	int result=0;
	PPRIVATE_DATA privateData=NULL;
	SMSC_TRACE("-->Smsc9118_stop(dev=0x%08lX)",(DWORD)dev);
	if(dev==NULL) {
		SMSC_WARNING("Smsc9118_stop(dev==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	privateData=(PPRIVATE_DATA)(dev->priv);
	if(privateData==NULL) {
		SMSC_WARNING("Smsc9118_stop(privateData==NULL)");
		result=-EFAULT;
		goto DONE;
	}

	privateData->StopLinkPolling=TRUE;
	del_timer_sync(&(privateData->LinkPollingTimer));

	Lan_DisableInterrupt(privateData,INT_EN_GPT_INT_EN_);

	Tx_UpdateTxCounters(privateData);
	privateData->Running=FALSE;
	Lan_DisableIRQ(privateData);

	Tx_CompleteDma(privateData);

	Tx_StopQueue(privateData,0x01UL);

#ifndef LINUX_2_6_OR_NEWER
	MOD_DEC_USE_COUNT;
#endif

	if(privateData->TxDmaChReserved) {
		Platform_ReleaseDmaChannel(
			&(privateData->PlatformData),
			privateData->dwTxDmaCh);
		privateData->TxDmaChReserved=FALSE;
	}
	if(privateData->RxDmaChReserved) {
		Platform_ReleaseDmaChannel(
			&(privateData->PlatformData),
			privateData->dwRxDmaCh);
		privateData->RxDmaChReserved=FALSE;
	}

	Platform_FreeIRQ(&(privateData->PlatformData));
	release_mem_region(privateData->dwLanBase,LAN_REGISTER_EXTENT);

	{
		const DWORD dwLanBase=privateData->dwLanBase;
		const DWORD dwIdRev=privateData->dwIdRev;
		const DWORD dwFpgaRev=privateData->dwFpgaRev;
		struct net_device * const tempDev=privateData->dev;
		char ifName[SMSC_IF_NAME_SIZE];
		PLATFORM_DATA platformDataBackup;
		memcpy(ifName,privateData->ifName,SMSC_IF_NAME_SIZE);
		memcpy(&platformDataBackup,&(privateData->PlatformData),sizeof(PLATFORM_DATA));

		memset(privateData,0,sizeof(PRIVATE_DATA));

		privateData->dwLanBase=dwLanBase;
		privateData->dwIdRev=dwIdRev;
		privateData->dwFpgaRev=dwFpgaRev;
		privateData->dev=tempDev;
		memcpy(privateData->ifName,ifName,SMSC_IF_NAME_SIZE);
		memcpy(&(privateData->PlatformData),&platformDataBackup,sizeof(PLATFORM_DATA));
	}

DONE:
	SMSC_TRACE("<--Smsc9118_stop, result=%d",result);
	return result;
}

int Smsc9118_hard_start_xmit(
	struct sk_buff *skb, struct net_device * const dev)
{
	int result=0;
	PPRIVATE_DATA privateData=NULL;
//	SMSC_TRACE("-->Smsc9118_hard_start_xmit(skb=0x%08lX,dev=0x%08lX)",(DWORD)skb,(DWORD)dev);
	if(skb==NULL) {
		SMSC_WARNING("Smsc9118_hard_start_xmit(skb==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	if(dev==NULL) {
		SMSC_WARNING("Smsc9118_hard_start_xmit(dev==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	if(dev->priv==NULL) {
		SMSC_WARNING("Smsc9118_hard_start_xmit(dev->priv==NULL)");
		result=-EFAULT;
		goto DONE;
	}
	privateData=(PPRIVATE_DATA)(dev->priv);
//	SET_GPIO(GP_TX);

	Tx_SendSkb(privateData,skb);

//	CLEAR_GPIO(GP_TX);
DONE:
//	SMSC_TRACE("<--Smsc9118_hard_start_xmit, result=%d",result);
	return result;
}

struct net_device_stats * Smsc9118_get_stats(struct net_device * const dev)
{
	PPRIVATE_DATA privateData=NULL;
	if(dev==NULL) {
		SMSC_WARNING("Smsc9118_get_stats(dev==NULL)");
		return NULL;
	}
	if(dev->priv==NULL) {
	//	SMSC_WARNING("Smsc9118_get_stats(dev->priv==NULL)");
		return NULL;
	}

	privateData=(PPRIVATE_DATA)(dev->priv);
	if(privateData->Running) {
		privateData->stats.rx_dropped+=Lan_GetRegDW(RX_DROP);
		Tx_UpdateTxCounters(privateData);
	}
	return &(privateData->stats);
}

void Smsc9118_set_multicast_list(struct net_device *dev)
{
	SMSC_ASSERT(dev!=NULL);
	Rx_SetMulticastList(dev);
}

int Smsc9118_do_ioctl(
	struct net_device *dev,
	struct ifreq *ifr,
	int cmd)
{
	PPRIVATE_DATA privateData=NULL;
	PSMSC9118_IOCTL_DATA ioctlData=NULL;
	BOOLEAN success=FALSE;
//	SMSC_TRACE("-->Smsc9118_do_ioctl");
//	SMSC_TRACE("cmd=%d,SIOCGMIIPHY=%d,SIOCDEVPRIVATE=%d",
//		cmd,SIOCGMIIPHY,SIOCDEVPRIVATE);
	SMSC_ASSERT(dev!=NULL);
	SMSC_ASSERT(dev->priv!=NULL);
	privateData=((PPRIVATE_DATA)dev->priv);
	if(ifr==NULL) {
//		SMSC_WARNING("Smsc9118_do_ioctl(ifr==NULL)");
		goto DONE;
	}

	if(privateData->LanInitialized) {
		// standard MII IOC's
		struct mii_ioctl_data * const data=
			(struct mii_ioctl_data *) & ifr->ifr_data;
		switch(cmd) {
		case SIOCGMIIPHY:

		case SIOCDEVPRIVATE:
			data->phy_id=1;
//			SMSC_TRACE("SIOCGMIIPHY: phy_id set to 0x%04X",data->phy_id);
			return 0;
		case SIOCGMIIREG:
		case SIOCDEVPRIVATE+1:
			{
				DWORD dwIntFlags=0;
				VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
				data->val_out=Phy_GetRegW(
					privateData,data->reg_num,keyCode);
				Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			}
//			SMSC_TRACE("SIOCGMIIREG: phy_id=0x%04X, reg_num=0x%04X, val_out set to 0x%04X",
//				data->phy_id,data->reg_num,data->val_out);
			return 0;
		case SIOCSMIIREG:
		case SIOCDEVPRIVATE+2:
//			SMSC_TRACE("SIOCSMIIREG: phy_id=0x%04X, reg_num=0x%04X, val_in=0x%04X",
//				data->phy_id,data->reg_num,data->val_in);
			{
				DWORD dwIntFlags=0;
				VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
				Phy_SetRegW(
					privateData,data->reg_num,((WORD)(data->val_in)),keyCode);
				Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			}
			return 0;
		default:break;//make lint happy
		}
	}
	if(ifr->ifr_data==NULL) {
//		SMSC_WARNING("Smsc9118_do_ioctl(ifr->ifr_data==NULL)");
		goto DONE;
	}
	if(cmd!=((int)SMSC9118_IOCTL)) goto DONE;
	ioctlData=(PSMSC9118_IOCTL_DATA)(ifr->ifr_data);
	if(ioctlData->dwSignature!=SMSC9118_APP_SIGNATURE) {
		goto DONE;
	}

	switch(ioctlData->dwCommand) {
	case COMMAND_GET_SIGNATURE:
		success=TRUE;
		break;
	case COMMAND_GET_FLOW_PARAMS:
		ioctlData->Data[0]=privateData->RxFlowMeasuredMaxThroughput;
		ioctlData->Data[1]=privateData->RxFlowMeasuredMaxPacketCount;
		ioctlData->Data[2]=privateData->RxFlowParameters.MaxThroughput;
		ioctlData->Data[3]=privateData->RxFlowParameters.MaxPacketCount;
		ioctlData->Data[4]=privateData->RxFlowParameters.PacketCost;
		ioctlData->Data[5]=privateData->RxFlowParameters.BurstPeriod;
		ioctlData->Data[6]=privateData->RxFlowMaxWorkLoad;
		ioctlData->Data[7]=Lan_GetRegDW(INT_CFG)>>24;
		privateData->RxFlowMeasuredMaxThroughput=0;
		privateData->RxFlowMeasuredMaxPacketCount=0;
		success=TRUE;
		break;
	case COMMAND_SET_FLOW_PARAMS:
		if(!(privateData->RxFlowControlActive)) {
			privateData->RxFlowParameters.MaxThroughput=ioctlData->Data[2];
			privateData->RxFlowParameters.MaxPacketCount=ioctlData->Data[3];
			privateData->RxFlowParameters.PacketCost=ioctlData->Data[4];
			privateData->RxFlowParameters.BurstPeriod=ioctlData->Data[5];
			if(ioctlData->Data[6]==0xFFFFFFFFUL) {
				privateData->RxFlowMaxWorkLoad=
					privateData->RxFlowParameters.MaxThroughput+
					(privateData->RxFlowParameters.MaxPacketCount*
					privateData->RxFlowParameters.PacketCost);
			} else {
				privateData->RxFlowMaxWorkLoad=ioctlData->Data[6];
			}
			Lan_SetIntDeas(privateData,ioctlData->Data[7]);
			privateData->RxFlowBurstMaxWorkLoad=
				(privateData->RxFlowMaxWorkLoad*
				privateData->RxFlowParameters.BurstPeriod)/1000;
			success=TRUE;
		};break;
	case COMMAND_GET_CONFIGURATION:
		ioctlData->Data[0]=DRIVER_VERSION;
		ioctlData->Data[1]=lan_base;
		ioctlData->Data[2]=bus_width;
		ioctlData->Data[3]=link_mode;
		ioctlData->Data[4]=irq;
		ioctlData->Data[5]=int_deas;
		ioctlData->Data[6]=irq_pol;
		ioctlData->Data[7]=irq_type;
		ioctlData->Data[8]=rx_dma;
		ioctlData->Data[9]=tx_dma;
		ioctlData->Data[10]=dma_threshold;
		ioctlData->Data[11]=mac_addr_hi16;
		ioctlData->Data[12]=mac_addr_lo32;
		ioctlData->Data[13]=debug_mode;
		ioctlData->Data[14]=tx_fif_sz;
		ioctlData->Data[15]=afc_cfg;
		ioctlData->Data[16]=tasklets;
		ioctlData->Data[17]=max_throughput;
		ioctlData->Data[18]=max_packet_count;
		ioctlData->Data[19]=packet_cost;
		ioctlData->Data[20]=burst_period;
		ioctlData->Data[21]=max_work_load;
		ioctlData->Data[22]=privateData->dwIdRev;
		ioctlData->Data[23]=privateData->dwFpgaRev;
		ioctlData->Data[24]=1;
		ioctlData->Data[25]=privateData->dwPhyId;
		ioctlData->Data[26]=privateData->bPhyModel;
		ioctlData->Data[27]=privateData->bPhyRev;
		ioctlData->Data[28]=privateData->dwLinkSpeed;
		ioctlData->Data[29]=privateData->RxFlowMeasuredMaxThroughput;
		ioctlData->Data[30]=privateData->RxFlowMeasuredMaxPacketCount;
		ioctlData->Data[31]=privateData->RxFlowParameters.MaxThroughput;
		ioctlData->Data[32]=privateData->RxFlowParameters.MaxPacketCount;
		ioctlData->Data[33]=privateData->RxFlowParameters.PacketCost;
		ioctlData->Data[34]=privateData->RxFlowParameters.BurstPeriod;
		ioctlData->Data[35]=privateData->RxFlowMaxWorkLoad;
		sprintf(ioctlData->Strng1,"%s, %s",__DATE__,__TIME__);
		sprintf(ioctlData->Strng2,"%s",privateData->ifName);
		privateData->RxFlowMeasuredMaxThroughput=0;
		privateData->RxFlowMeasuredMaxPacketCount=0;
		success=TRUE;
		break;
	case COMMAND_LAN_GET_REG:
		if((ioctlData->Data[0]<LAN_REGISTER_EXTENT)&&
			((ioctlData->Data[0]&0x3UL)==0))
		{
			ioctlData->Data[1]=
				(*((volatile DWORD *)(privateData->dwLanBase+
						ioctlData->Data[0])));
			success=TRUE;
		} else {
			SMSC_WARNING("Reading LAN9118 Mem Map Failed");
			goto MEM_MAP_ACCESS_FAILED;
		}
		break;
	case COMMAND_LAN_SET_REG:
		if((ioctlData->Data[0]<LAN_REGISTER_EXTENT)&&
			((ioctlData->Data[0]&0x3UL)==0))
		{
			(*((volatile DWORD *)(privateData->dwLanBase+
						ioctlData->Data[0])))=ioctlData->Data[1];
			success=TRUE;
		} else {
			SMSC_WARNING("Reading LAN9118 Mem Map Failed");
MEM_MAP_ACCESS_FAILED:
			SMSC_WARNING("  Invalid offset == 0x%08lX",ioctlData->Data[0]);
			if(ioctlData->Data[0]>=LAN_REGISTER_EXTENT) {
				SMSC_WARNING("    Out of range");
			}
			if(ioctlData->Data[0]&0x3UL) {
				SMSC_WARNING("    Not DWORD aligned");
			}
		}
		break;
	case COMMAND_MAC_GET_REG:
		if((ioctlData->Data[0]<=0xC)&&(privateData->LanInitialized)) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[1]=
				Mac_GetRegDW(privateData,ioctlData->Data[0],keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Reading Mac Register Failed");
			goto MAC_ACCESS_FAILURE;
		}
		break;
	case COMMAND_MAC_SET_REG:
		if((ioctlData->Data[0]<=0xC)&&(privateData->LanInitialized)) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			Mac_SetRegDW(
				privateData,
				ioctlData->Data[0],
				ioctlData->Data[1],
				keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Writing Mac Register Failed");
MAC_ACCESS_FAILURE:
			if(!(privateData->LanInitialized)) {

				SMSC_WARNING("  LAN Not Initialized,");
				SMSC_WARNING("    Use ifconfig to bring interface UP");
			}
			if(!(ioctlData->Data[0]<=0xC)) {
				SMSC_WARNING("  Invalid index == 0x%08lX",ioctlData->Data[0]);
			}
		}
		break;
	case COMMAND_PHY_GET_REG:
		if((ioctlData->Data[0]<32)&&(privateData->LanInitialized)) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[1]=((DWORD)
				Phy_GetRegW(privateData,ioctlData->Data[0],keyCode));
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Reading Phy Register Failed");
			goto PHY_ACCESS_FAILURE;
		}
		break;
	case COMMAND_PHY_SET_REG:
		if((ioctlData->Data[0]<32)&&(privateData->LanInitialized)) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			Phy_SetRegW(
				privateData,
				ioctlData->Data[0],
				((WORD)(ioctlData->Data[1])),
				keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Writing Phy Register Failed");
PHY_ACCESS_FAILURE:
			if(!(privateData->LanInitialized)) {
				SMSC_WARNING("  Lan Not Initialized,");
				SMSC_WARNING("    Use ifconfig to bring interface UP");
			}
			if(!(ioctlData->Data[0]<32)) {
				SMSC_WARNING("  Invalid index == 0x%ld",ioctlData->Data[0]);
			}
		}
		break;
//	case COMMAND_DUMP_TEMP:
//		{
//			DWORD c=0;
//			for(c=0;c<0x40;c++)
//				ioctlData->Data[c]=privateData->temp[c];
//		}
//		success=TRUE;
//		break;
	case COMMAND_DUMP_LAN_REGS:
		ioctlData->Data[LAN_REG_ID_REV]=Lan_GetRegDW(ID_REV);
		ioctlData->Data[LAN_REG_INT_CFG]=Lan_GetRegDW(INT_CFG);
		ioctlData->Data[LAN_REG_INT_STS]=Lan_GetRegDW(INT_STS);
		ioctlData->Data[LAN_REG_INT_EN]=Lan_GetRegDW(INT_EN);
		ioctlData->Data[LAN_REG_BYTE_TEST]=Lan_GetRegDW(BYTE_TEST);
		ioctlData->Data[LAN_REG_FIFO_INT]=Lan_GetRegDW(FIFO_INT);
		ioctlData->Data[LAN_REG_RX_CFG]=Lan_GetRegDW(RX_CFG);
		ioctlData->Data[LAN_REG_TX_CFG]=Lan_GetRegDW(TX_CFG);
		ioctlData->Data[LAN_REG_HW_CFG]=Lan_GetRegDW(HW_CFG);
		ioctlData->Data[LAN_REG_RX_DP_CTRL]=Lan_GetRegDW(RX_DP_CTRL);
		ioctlData->Data[LAN_REG_RX_FIFO_INF]=Lan_GetRegDW(RX_FIFO_INF);
		ioctlData->Data[LAN_REG_TX_FIFO_INF]=Lan_GetRegDW(TX_FIFO_INF);
		ioctlData->Data[LAN_REG_PMT_CTRL]=Lan_GetRegDW(PMT_CTRL);
		ioctlData->Data[LAN_REG_GPIO_CFG]=Lan_GetRegDW(GPIO_CFG);
		ioctlData->Data[LAN_REG_GPT_CFG]=Lan_GetRegDW(GPT_CFG);
		ioctlData->Data[LAN_REG_GPT_CNT]=Lan_GetRegDW(GPT_CNT);
		ioctlData->Data[LAN_REG_FPGA_REV]=Lan_GetRegDW(FPGA_REV);
		ioctlData->Data[LAN_REG_ENDIAN]=Lan_GetRegDW(ENDIAN);
		ioctlData->Data[LAN_REG_FREE_RUN]=Lan_GetRegDW(FREE_RUN);
		ioctlData->Data[LAN_REG_RX_DROP]=Lan_GetRegDW(RX_DROP);
		if(privateData->LanInitialized) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[LAN_REG_MAC_CSR_CMD]=Lan_GetRegDW(MAC_CSR_CMD);
			ioctlData->Data[LAN_REG_MAC_CSR_DATA]=Lan_GetRegDW(MAC_CSR_DATA);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
		} else {
			ioctlData->Data[LAN_REG_MAC_CSR_CMD]=Lan_GetRegDW(MAC_CSR_CMD);
			ioctlData->Data[LAN_REG_MAC_CSR_DATA]=Lan_GetRegDW(MAC_CSR_DATA);
		}
		ioctlData->Data[LAN_REG_AFC_CFG]=Lan_GetRegDW(AFC_CFG);
		ioctlData->Data[LAN_REG_E2P_CMD]=Lan_GetRegDW(E2P_CMD);
		ioctlData->Data[LAN_REG_E2P_DATA]=Lan_GetRegDW(E2P_DATA);
		success=TRUE;
		break;
	case COMMAND_DUMP_MAC_REGS:
		if(privateData->LanInitialized) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[MAC_REG_MAC_CR]=Mac_GetRegDW(privateData,MAC_CR,keyCode);
			ioctlData->Data[MAC_REG_ADDRH]=Mac_GetRegDW(privateData,ADDRH,keyCode);
			ioctlData->Data[MAC_REG_ADDRL]=Mac_GetRegDW(privateData,ADDRL,keyCode);
			ioctlData->Data[MAC_REG_HASHH]=Mac_GetRegDW(privateData,HASHH,keyCode);
			ioctlData->Data[MAC_REG_HASHL]=Mac_GetRegDW(privateData,HASHL,keyCode);
			ioctlData->Data[MAC_REG_MII_ACC]=Mac_GetRegDW(privateData,MII_ACC,keyCode);
			ioctlData->Data[MAC_REG_MII_DATA]=Mac_GetRegDW(privateData,MII_DATA,keyCode);
			ioctlData->Data[MAC_REG_FLOW]=Mac_GetRegDW(privateData,FLOW,keyCode);
			ioctlData->Data[MAC_REG_VLAN1]=Mac_GetRegDW(privateData,VLAN1,keyCode);
			ioctlData->Data[MAC_REG_VLAN2]=Mac_GetRegDW(privateData,VLAN2,keyCode);
			ioctlData->Data[MAC_REG_WUFF]=Mac_GetRegDW(privateData,WUFF,keyCode);
			ioctlData->Data[MAC_REG_WUCSR]=Mac_GetRegDW(privateData,WUCSR,keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Mac Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		}
		break;
	case COMMAND_DUMP_PHY_REGS:
		if(privateData->LanInitialized) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[PHY_REG_0]=Phy_GetRegW(privateData,0,keyCode);
			ioctlData->Data[PHY_REG_1]=Phy_GetRegW(privateData,1,keyCode);
			ioctlData->Data[PHY_REG_2]=Phy_GetRegW(privateData,2,keyCode);
			ioctlData->Data[PHY_REG_3]=Phy_GetRegW(privateData,3,keyCode);
			ioctlData->Data[PHY_REG_4]=Phy_GetRegW(privateData,4,keyCode);
			ioctlData->Data[PHY_REG_5]=Phy_GetRegW(privateData,5,keyCode);
			ioctlData->Data[PHY_REG_6]=Phy_GetRegW(privateData,6,keyCode);
			ioctlData->Data[PHY_REG_16]=Phy_GetRegW(privateData,16,keyCode);
			ioctlData->Data[PHY_REG_17]=Phy_GetRegW(privateData,17,keyCode);
			ioctlData->Data[PHY_REG_18]=Phy_GetRegW(privateData,18,keyCode);
			ioctlData->Data[PHY_REG_20]=Phy_GetRegW(privateData,20,keyCode);
			ioctlData->Data[PHY_REG_21]=Phy_GetRegW(privateData,21,keyCode);
			ioctlData->Data[PHY_REG_22]=Phy_GetRegW(privateData,22,keyCode);
			ioctlData->Data[PHY_REG_23]=Phy_GetRegW(privateData,23,keyCode);
			ioctlData->Data[PHY_REG_27]=Phy_GetRegW(privateData,27,keyCode);
			ioctlData->Data[PHY_REG_28]=Phy_GetRegW(privateData,28,keyCode);
			ioctlData->Data[PHY_REG_29]=Phy_GetRegW(privateData,29,keyCode);
			ioctlData->Data[PHY_REG_30]=Phy_GetRegW(privateData,30,keyCode);
			ioctlData->Data[PHY_REG_31]=Phy_GetRegW(privateData,31,keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Phy Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		}
		break;
	case COMMAND_DUMP_EEPROM:
		{
			BYTE data=0;
			BYTE index=0;
			Eeprom_EnableAccess(privateData);
			success=TRUE;
			for(index=0;index<8;index++) {
				if(Eeprom_ReadLocation(privateData,index,&data)) {
					ioctlData->Data[index]=(DWORD)data;
				} else {
					success=FALSE;
					break;
				}
			}
			Eeprom_DisableAccess(privateData);
		};break;
	case COMMAND_GET_MAC_ADDRESS:
		if(privateData->LanInitialized) {
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			ioctlData->Data[0]=Mac_GetRegDW(privateData,ADDRH,keyCode);
			ioctlData->Data[1]=Mac_GetRegDW(privateData,ADDRL,keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Lan Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		}
		break;

	case COMMAND_SET_MAC_ADDRESS:
		if(privateData->LanInitialized)
		{
			DWORD dwLow32=ioctlData->Data[1];
			DWORD dwHigh16=ioctlData->Data[0];
			DWORD dwIntFlags=0;
			VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
			Mac_SetRegDW(privateData,ADDRH,dwHigh16,keyCode);
			Mac_SetRegDW(privateData,ADDRL,dwLow32,keyCode);
			Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
			success=TRUE;
		} else {
			SMSC_WARNING("Lan Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		};break;
	case COMMAND_LOAD_MAC_ADDRESS:
		if(privateData->LanInitialized) {
			Eeprom_EnableAccess(privateData);
			if(Eeprom_Reload(privateData)) {
				if(Eeprom_IsMacAddressLoaded(privateData)) {
					DWORD dwIntFlags=0;
					VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
					ioctlData->Data[0]=Mac_GetRegDW(privateData,ADDRH,keyCode);
					ioctlData->Data[1]=Mac_GetRegDW(privateData,ADDRL,keyCode);
					Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
					success=TRUE;
				} else {
					SMSC_WARNING("Failed to Load Mac Address(1)");
				}
			} else {
				SMSC_WARNING("Failed to Load Mac Address(2)");
			}
			Eeprom_DisableAccess(privateData);
		} else {
			SMSC_WARNING("Lan Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		};break;
	case COMMAND_SAVE_MAC_ADDRESS:
		if(privateData->LanInitialized) {
			if(Eeprom_SaveMacAddress(privateData,
				ioctlData->Data[0],ioctlData->Data[1])) {
				success=TRUE;
			}
		} else {
			SMSC_WARNING("Lan Not Initialized,");
			SMSC_WARNING("  Use ifconfig to bring interface UP");
		};break;
	case COMMAND_SET_DEBUG_MODE:
		debug_mode=ioctlData->Data[0];
		if(debug_mode&0x04UL) {
			if(OLD_REGISTERS(privateData))
			{
				g_GpioSetting=0x00270700UL;
			} else {
				g_GpioSetting=0x00670700UL;
			}
			Lan_SetRegDW(GPIO_CFG,g_GpioSetting);
		} else {
			Lan_SetRegDW(GPIO_CFG,0x70070000);
		}
		success=TRUE;
		break;
	case COMMAND_SET_LINK_MODE:
		link_mode=(ioctlData->Data[0]&0x7FUL);
		if(privateData->LanInitialized) {
			Phy_SetLink(privateData,link_mode);
		}
		success=TRUE;
		break;
	case COMMAND_GET_LINK_MODE:
		ioctlData->Data[0]=link_mode;
		success=TRUE;
		break;
	case COMMAND_CHECK_LINK:
		Phy_UpdateLinkMode(privateData);
		success=TRUE;
		break;
	case COMMAND_READ_BYTE:
		ioctlData->Data[1]=(*((volatile BYTE *)(ioctlData->Data[0])));
		success=TRUE;
		break;
	case COMMAND_READ_WORD:
		ioctlData->Data[1]=(*((volatile WORD *)(ioctlData->Data[0])));
		success=TRUE;
		break;
	case COMMAND_READ_DWORD:
		ioctlData->Data[1]=(*((volatile DWORD *)(ioctlData->Data[0])));
		success=TRUE;
		break;
	case COMMAND_WRITE_BYTE:
		(*((volatile BYTE *)(ioctlData->Data[0])))=
			((BYTE)(ioctlData->Data[1]));
		success=TRUE;
		break;
	case COMMAND_WRITE_WORD:
		(*((volatile WORD *)(ioctlData->Data[0])))=
			((WORD)(ioctlData->Data[1]));
		success=TRUE;
		break;
	case COMMAND_WRITE_DWORD:
		(*((volatile DWORD *)(ioctlData->Data[0])))=
			((DWORD)(ioctlData->Data[1]));
		success=TRUE;
		break;
	default:break;//make lint happy
	}

DONE:
	if((success)&&(ioctlData!=NULL)) {
		ioctlData->dwSignature=SMSC9118_DRIVER_SIGNATURE;
		return 0;
	}
//	SMSC_TRACE("<--Smsc9118_do_ioctl");
	return -1;
}

//returns time1-time2;
TIME_SPAN Gpt_FreeRunCompare(DWORD time1,DWORD time2)
{
	return ((TIME_SPAN)(time1-time2));
}
void Gpt_ScheduleInterrupt(PPRIVATE_DATA privateData,TIME_SPAN timeSpan)
{
	DWORD timerValue=0;
	if(timeSpan<0) timeSpan=0;
	timerValue=(DWORD)timeSpan;
	if((timerValue%2500)>=1250) {
		timerValue=(timerValue/2500)+1;
	} else {
		timerValue=(timerValue/2500);
	}
	if(timerValue>0x0000FFFFUL) {
		timerValue=0x0000FFFF;
	}
	Lan_SetRegDW(GPT_CFG,(timerValue|GPT_CFG_TIMER_EN_));
	Lan_SetRegDW(INT_STS,INT_STS_GPT_INT_);
}

void Gpt_CancelInterrupt(PPRIVATE_DATA privateData)
{
	Lan_SetRegDW(GPT_CFG,0UL);
	Lan_SetRegDW(INT_STS,INT_STS_GPT_INT_);
}

void Gpt_ScheduleCallBack(
	PPRIVATE_DATA privateData,
	void (*callBackFunction)(PPRIVATE_DATA privateData),
	DWORD callBackTime)
{
	DWORD slot_index=GPT_SCHEDULE_DEPTH;
	BOOLEAN result=FALSE;
	if((callBackFunction!=NULL)&&(callBackTime!=0)) {
		DWORD dwIntFlags=0;
		SMSC_ASSERT(privateData!=NULL);
		spin_lock_irqsave(&(privateData->GpTimerLock),dwIntFlags);
		{
			DWORD index=0;
			DWORD currentTime=Lan_GetRegDW(FREE_RUN);
			TIME_SPAN nextCallTime=MAX_TIME_SPAN;
			TIME_SPAN timeSpan=MAX_TIME_SPAN;
			BOOLEAN rescheduleRequired=FALSE;
			for(index=0;index<GPT_SCHEDULE_DEPTH;index++) {
				if(privateData->GptFunction[index]==NULL) {
					if(!result) {
						result=TRUE;
						//lint -save
						//lint -e611 //suspicious cast
						privateData->GptFunction[index]=(void *)callBackFunction;
						//lint -restore_
						privateData->GptCallTime[index]=currentTime+(2500*callBackTime);
						timeSpan=Gpt_FreeRunCompare(privateData->GptCallTime[index],currentTime);
						if(nextCallTime>timeSpan) {
							nextCallTime=timeSpan;
							rescheduleRequired=TRUE;
							slot_index = index;
						}
					}
				} else {
					timeSpan=Gpt_FreeRunCompare(privateData->GptCallTime[index],currentTime);
					if(nextCallTime>=timeSpan) {
						nextCallTime=timeSpan;
						rescheduleRequired=FALSE;
					}
				}
			}
			if(rescheduleRequired) {
				privateData->Gpt_scheduled_slot_index = slot_index;
				Gpt_ScheduleInterrupt(privateData,nextCallTime);
			}
		}
		spin_unlock_irqrestore(&(privateData->GpTimerLock),dwIntFlags);
	}
	if(!result) {
		SMSC_WARNING("Gpt_ScheduleCallBack: Failed");
	}
}

void Gpt_CancelCallBack(
	PPRIVATE_DATA privateData,
	void (*callBackFunction)(PPRIVATE_DATA privateData))
{
	BOOLEAN result=FALSE;
	if(callBackFunction!=NULL) {
		DWORD dwIntFlags=0;
		SMSC_ASSERT(privateData!=NULL);
		spin_lock_irqsave(&(privateData->GpTimerLock),dwIntFlags);
		{
			DWORD index=0;
			DWORD currentTime=Lan_GetRegDW(FREE_RUN);
			TIME_SPAN nextCallTime=MAX_TIME_SPAN;
			TIME_SPAN timeSpan=MAX_TIME_SPAN;
			BOOLEAN rescheduleRequired=FALSE;
			for(index=0;index<GPT_SCHEDULE_DEPTH;index++) {
				if(privateData->GptFunction[index]==callBackFunction) {
					result=TRUE;
					//lint -save
					//lint -e611 //suspicious cast
					privateData->GptFunction[index]=(void *)NULL;
					// cancelled time will not need a
					// re-scheduled

					// re-scheduled is done at other
					// non-null slots
				}
				else if(privateData->GptFunction[index]!=NULL) {
					timeSpan=Gpt_FreeRunCompare(privateData->GptCallTime[index],currentTime);
					// if this scheduled time is earlier
					// than current scheduled time
					// AND not a duplicated one
					if(nextCallTime>=timeSpan && privateData->Gpt_scheduled_slot_index != index) {
						nextCallTime=timeSpan;
						rescheduleRequired=TRUE;
						privateData->Gpt_scheduled_slot_index = index;
					}
				}
			}
			if(rescheduleRequired) {
				Gpt_ScheduleInterrupt(privateData,nextCallTime);
			}
			else if (privateData->Gpt_scheduled_slot_index==GPT_SCHEDULE_DEPTH) {
				Gpt_CancelInterrupt(privateData);
			}
		}
		spin_unlock_irqrestore(&(privateData->GpTimerLock),dwIntFlags);
	}
	if(!result) {
		SMSC_WARNING("Gpt_CancelCallBack: Failed");
	}
}

BOOLEAN Gpt_HandleInterrupt(
	PPRIVATE_DATA privateData,DWORD dwIntSts)
{
	SMSC_ASSERT(privateData!=NULL);
	if(dwIntSts&INT_STS_GPT_INT_)
	{
		DWORD dwIntFlags=0;
		Lan_SetRegDW(INT_STS,INT_STS_GPT_INT_);
		spin_lock_irqsave(&(privateData->GpTimerLock),dwIntFlags);
		{
			DWORD index=0;
			DWORD currentTime=Lan_GetRegDW(FREE_RUN);
			TIME_SPAN timeSpan=MAX_TIME_SPAN;
			TIME_SPAN nextCallTime=MAX_TIME_SPAN;
			BOOLEAN rescheduleRequired=FALSE;
			for(index=0;index<GPT_SCHEDULE_DEPTH;index++) {
				if(privateData->GptFunction[index]!=NULL) {
					timeSpan=Gpt_FreeRunCompare(privateData->GptCallTime[index],currentTime);
					if(timeSpan<1250) {
						void (*callBackFunction)(PPRIVATE_DATA privateData);
						callBackFunction=privateData->GptFunction[index];
						privateData->GptFunction[index]=NULL;
						spin_unlock_irqrestore(&(privateData->GpTimerLock),dwIntFlags);
						privateData->Gpt_scheduled_slot_index = GPT_SCHEDULE_DEPTH;
						callBackFunction(privateData);
						spin_lock_irqsave(&(privateData->GpTimerLock),dwIntFlags);
					}
				}
			}
			for(index=0;index<GPT_SCHEDULE_DEPTH;index++) {
				if(privateData->GptFunction[index]!=NULL) {
					rescheduleRequired=TRUE;
					timeSpan=Gpt_FreeRunCompare(privateData->GptCallTime[index],currentTime);
					if(nextCallTime>timeSpan) {
						nextCallTime=timeSpan;
						privateData->Gpt_scheduled_slot_index = index;
					}
				}
			}
			if(rescheduleRequired) {
				Gpt_ScheduleInterrupt(privateData,nextCallTime);
			}
		}
		spin_unlock_irqrestore(&(privateData->GpTimerLock),dwIntFlags);
		return TRUE;
	}
	return FALSE;
}

void GptCB_RxCompleteMulticast(PPRIVATE_DATA privateData)
{
	Rx_CompleteMulticastUpdate (privateData);
}

void GptCB_RestartBurst(PPRIVATE_DATA privateData)
{
	if(privateData->RxFlowControlActive) {
		privateData->RxFlowBurstActive=TRUE;
		if(privateData->RxFlowBurstWorkLoad>privateData->RxFlowBurstMaxWorkLoad) {
			privateData->RxFlowBurstWorkLoad-=privateData->RxFlowBurstMaxWorkLoad;
		} else {
			privateData->RxFlowBurstWorkLoad=0;
		}
		Gpt_ScheduleCallBack(privateData,GptCB_RestartBurst,
				privateData->RxFlowParameters.BurstPeriod);
	}
	Lan_EnableInterrupt(privateData,privateData->RxInterrupts);
}

void GptCB_MeasureRxThroughput(PPRIVATE_DATA privateData)
{
	if(privateData->RxFlowMeasuredMaxThroughput<privateData->RxFlowCurrentThroughput) {
		privateData->RxFlowMeasuredMaxThroughput=privateData->RxFlowCurrentThroughput;
	}
	if(privateData->RxFlowMeasuredMaxPacketCount<privateData->RxFlowCurrentPacketCount) {
		privateData->RxFlowMeasuredMaxPacketCount=privateData->RxFlowCurrentPacketCount;
	}
	if(privateData->RxFlowCurrentThroughput!=0) {
		if(privateData->RxFlowMaxWorkLoad!=0) {
			if(!(privateData->RxFlowControlActive)) {
				DWORD activationLevel=
					(privateData->RxFlowMaxWorkLoad*(100+RX_FLOW_ACTIVATION))/100;
				if(privateData->RxFlowCurrentWorkLoad>=activationLevel) {
					privateData->RxFlowControlActive=TRUE;
					privateData->RxFlowBurstActive=TRUE;
					privateData->RxFlowBurstWorkLoad=0;
					Gpt_ScheduleCallBack(privateData,GptCB_RestartBurst,
							privateData->RxFlowParameters.BurstPeriod);
					//SET_GPIO(GP_TX);
				}
			} else {
				DWORD deactivationLevel=
					(privateData->RxFlowMaxWorkLoad*(100-RX_FLOW_DEACTIVATION))/100;
				if(privateData->RxFlowCurrentWorkLoad<=deactivationLevel) {
					privateData->RxFlowControlActive=FALSE;
					//CLEAR_GPIO(GP_TX);
				}
			}
		}
		privateData->RxFlowCurrentThroughput=0;
		privateData->RxFlowCurrentPacketCount=0;
		privateData->RxFlowCurrentWorkLoad=0;
		Gpt_ScheduleCallBack(privateData,GptCB_MeasureRxThroughput,1000);
	} else {
		if(privateData->RxFlowMaxWorkLoad!=0) {
			if(privateData->RxFlowControlActive) {
				privateData->RxFlowControlActive=FALSE;
				//CLEAR_GPIO(GP_TX);
			}
		}
		privateData->MeasuringRxThroughput=FALSE;
	}
}

irqreturn_t Smsc9118_ISR(int Irq, void *dev_id)
{
	DWORD dwIntCfg=0;
	DWORD dwIntSts=0;
	DWORD dwIntEn=0;
	DWORD dwIntBits=0;
	PPRIVATE_DATA privateData=(PPRIVATE_DATA)dev_id;
	BOOLEAN serviced=FALSE;

	Irq=Irq;//make lint happy

	if(privateData==NULL) {
		SMSC_WARNING("Smsc9118_ISR(privateData==NULL)");
		goto DONE;
	}
	if(privateData->dwLanBase==0) {
		SMSC_WARNING("Smsc9118_ISR(dwLanBase==0)");
		goto DONE;
	}
	SET_GPIO(GP_ISR);
	dwIntCfg=Lan_GetRegDW(INT_CFG);
	if((dwIntCfg&0x00001100)!=0x00001100) {
		SMSC_TRACE("In ISR, not my interrupt, dwIntCfg=0x%08lX",
			dwIntCfg);
		goto ALMOST_DONE;
	}

	{
		DWORD reservedBits;
		if(OLD_REGISTERS(privateData)) {
			reservedBits=0x00FFEEEEUL;
		} else {
			reservedBits=0x00FFCEEEUL;
		}
		if(dwIntCfg&reservedBits) {
			SMSC_WARNING("In ISR, reserved bits are high.");
			//this could mean surprise removal
			goto ALMOST_DONE;
		}
	}

	dwIntSts=Lan_GetRegDW(INT_STS);
	dwIntEn=Lan_GetRegDW(INT_EN);
	dwIntBits=dwIntSts&dwIntEn;
	privateData->LastIntStatus3=privateData->LastIntStatus2;
	privateData->LastIntStatus2=privateData->LastIntStatus1;
	privateData->LastIntStatus1=dwIntBits;
	if(Lan_HandleSoftwareInterrupt(privateData,dwIntBits)) {
		serviced=TRUE;
	}
	if(Gpt_HandleInterrupt(privateData,dwIntBits)) {
		serviced=TRUE;
	}
	if(Tx_HandleInterrupt(privateData,dwIntBits)) {
		serviced=TRUE;
	}
	if(RxStop_HandleInterrupt(privateData,dwIntBits)) {
		serviced=TRUE;
	}
	if(Rx_HandleInterrupt(privateData,dwIntBits)) {
		serviced=TRUE;
	}

	if(!serviced) {
		SMSC_WARNING("unserviced interrupt dwIntCfg=0x%08lX,dwIntSts=0x%08lX,dwIntEn=0x%08lX,dwIntBits=0x%08lX",
			dwIntCfg,dwIntSts,dwIntEn,dwIntBits);
	}

ALMOST_DONE:
	CLEAR_GPIO(GP_ISR);
DONE:
	return IRQ_RETVAL(serviced);
}

#ifdef USE_PHY_WORK_AROUND
BOOLEAN Phy_Reset(PPRIVATE_DATA privateData,VL_KEY keyCode)
{
	BOOLEAN result=FALSE;
	WORD wTemp=0;
	DWORD dwLoopCount=100000;
	SMSC_TRACE("Performing PHY BCR Reset");
	Phy_SetRegW(privateData,PHY_BCR,PHY_BCR_RESET_,keyCode);
	do {
		udelay(10);
		wTemp=Phy_GetRegW(privateData,PHY_BCR,keyCode);
		dwLoopCount--;
	} while((dwLoopCount>0)&&(wTemp&PHY_BCR_RESET_));
	if(wTemp&PHY_BCR_RESET_) {
		SMSC_WARNING("Phy Reset failed to complete.");
		goto DONE;
	}
	//extra delay required because the phy may not be completed with its reset
	//  when PHY_BCR_RESET_ is cleared.
	//  They say 256 uS is enough delay but I'm using 500 here to be safe
	udelay(500);
	result=TRUE;
DONE:
	return result;
}

DWORD Phy_LBT_GetTxStatus(PPRIVATE_DATA privateData)
{
	DWORD result=Lan_GetRegDW(TX_FIFO_INF);
	if(OLD_REGISTERS(privateData)) {
		result&=TX_FIFO_INF_TSFREE_;
		if(result!=0x00800000UL) {
			result=Lan_GetRegDW(TX_STATUS_FIFO);
		} else {
			result=0;
		}
	} else {
		result&=TX_FIFO_INF_TSUSED_;
		if(result!=0x00000000UL) {
			result=Lan_GetRegDW(TX_STATUS_FIFO);
		} else {
			result=0;
		}
	}
	return result;
}

DWORD Phy_LBT_GetRxStatus(PPRIVATE_DATA privateData)
{
	DWORD result=Lan_GetRegDW(RX_FIFO_INF);
	if(result&0x00FF0000UL) {
		//Rx status is available, read it
		result=Lan_GetRegDW(RX_STATUS_FIFO);
	} else {
		result=0;
	}
	return result;
}

BOOLEAN Phy_TransmitTestPacket(PPRIVATE_DATA privateData)
{
	BOOLEAN result=FALSE;
	DWORD dwLoopCount=0;
	DWORD dwTxCmdA=0;
	DWORD dwTxCmdB=0;
	DWORD dwStatus=0;

	//write Tx Packet to 118
	dwTxCmdA=
		((((DWORD)(privateData->LoopBackTxPacket))&0x03UL)<<16) | //DWORD alignment adjustment
		TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
		((DWORD)(MIN_PACKET_SIZE));
	dwTxCmdB=
		(((DWORD)(MIN_PACKET_SIZE))<<16) |
		((DWORD)(MIN_PACKET_SIZE));
	Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdA);
	Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdB);
	Platform_WriteFifo(
		privateData->dwLanBase,
		(DWORD *)(((DWORD)(privateData->LoopBackTxPacket))&0xFFFFFFFCUL),
		(((DWORD)(MIN_PACKET_SIZE))+3+
		(((DWORD)(privateData->LoopBackTxPacket))&0x03UL))>>2);

	//wait till transmit is done
	dwLoopCount=60;
	while((dwLoopCount>0)&&((dwStatus=Phy_LBT_GetTxStatus(privateData))==0)) {
		udelay(5);
		dwLoopCount--;
	}
	if(dwStatus==0) {
		SMSC_WARNING("Failed to Transmit during Packet Test");
		goto DONE;
	}
	if(dwStatus&0x00008000UL) {
		SMSC_WARNING("Transmit encountered errors during Packet Test");
		goto DONE;
	}
DONE:
	return result;
}

BOOLEAN Phy_CheckLoopBackPacket(PPRIVATE_DATA privateData)

{
	BOOLEAN result=FALSE;
	DWORD tryCount=0;
	DWORD dwLoopCount=0;
	for(tryCount=0;tryCount<10;tryCount++)
	{
		DWORD dwTxCmdA=0;
		DWORD dwTxCmdB=0;
		DWORD dwStatus=0;
		DWORD dwPacketLength=0;

		//zero-out Rx Packet memory
		memset(privateData->LoopBackRxPacket,0,MIN_PACKET_SIZE);

		//write Tx Packet to 118
		dwTxCmdA=
			((((DWORD)(privateData->LoopBackTxPacket))&0x03UL)<<16) | //DWORD alignment adjustment
			TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
			((DWORD)(MIN_PACKET_SIZE));
		dwTxCmdB=
			(((DWORD)(MIN_PACKET_SIZE))<<16) |
			((DWORD)(MIN_PACKET_SIZE));
		Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdA);
		Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdB);
		Platform_WriteFifo(
			privateData->dwLanBase,
			(DWORD *)(((DWORD)(privateData->LoopBackTxPacket))&0xFFFFFFFCUL),
			(((DWORD)(MIN_PACKET_SIZE))+3+
			(((DWORD)(privateData->LoopBackTxPacket))&0x03UL))>>2);

		//wait till transmit is done
		dwLoopCount=60;
		while((dwLoopCount>0)&&((dwStatus=Phy_LBT_GetTxStatus(privateData))==0)) {
			udelay(5);
			dwLoopCount--;
		}
		if(dwStatus==0) {
			SMSC_WARNING("Failed to Transmit during Loop Back Test");
			continue;
		}
		if(dwStatus&0x00008000UL) {
			SMSC_WARNING("Transmit encountered errors during Loop Back Test");
			continue;
		}

		//wait till receive is done
		dwLoopCount=60;
		while((dwLoopCount>0)&&((dwStatus=Phy_LBT_GetRxStatus(privateData))==0))
		{
	         udelay(5);
	         dwLoopCount--;
		}
		if(dwStatus==0) {
			SMSC_WARNING("Failed to Receive during Loop Back Test");
			continue;
		}
		if(dwStatus&RX_STS_ES_)
		{
			SMSC_WARNING("Receive encountered errors during Loop Back Test");
			continue;
		}

		dwPacketLength=((dwStatus&0x3FFF0000UL)>>16);

		Platform_ReadFifo(
			privateData->dwLanBase,
			((DWORD *)(privateData->LoopBackRxPacket)),
			(dwPacketLength+3+(((DWORD)(privateData->LoopBackRxPacket))&0x03UL))>>2);

		if(dwPacketLength!=(MIN_PACKET_SIZE+4)) {
			SMSC_WARNING("Unexpected packet size during loop back test, size=%ld, will retry",dwPacketLength);
		} else {
			DWORD byteIndex=0;
			BOOLEAN foundMissMatch=FALSE;
			for(byteIndex=0;byteIndex<MIN_PACKET_SIZE;byteIndex++) {
				if(privateData->LoopBackTxPacket[byteIndex]!=privateData->LoopBackRxPacket[byteIndex])
				{
					foundMissMatch=TRUE;
					break;
				}
			}
			if(!foundMissMatch) {
				SMSC_TRACE("Successfully Verified Loop Back Packet");
				result=TRUE;
				goto DONE;
			} else {
				SMSC_WARNING("Data miss match during loop back test, will retry.");
			}
		}
	}
DONE:
	return result;
}

BOOLEAN Phy_LoopBackTest(PPRIVATE_DATA privateData)
{
	BOOLEAN result=FALSE;
	DWORD byteIndex=0;
	DWORD tryCount=0;
//	DWORD failed=0;
	//Initialize Tx Packet
	for(byteIndex=0;byteIndex<6;byteIndex++) {
		//use broadcast destination address
		privateData->LoopBackTxPacket[byteIndex]=(BYTE)0xFF;
	}
	for(byteIndex=6;byteIndex<12;byteIndex++) {
		//use incrementing source address
		privateData->LoopBackTxPacket[byteIndex]=(BYTE)byteIndex;
	}
	//Set length type field
	privateData->LoopBackTxPacket[12]=0x00;
	privateData->LoopBackTxPacket[13]=0x00;
	for(byteIndex=14;byteIndex<MIN_PACKET_SIZE;byteIndex++)
	{
		privateData->LoopBackTxPacket[byteIndex]=(BYTE)byteIndex;
	}
//TRY_AGAIN:
	{
		DWORD dwRegVal=Lan_GetRegDW(HW_CFG);
		dwRegVal&=(HW_CFG_TX_FIF_SZ_|0x00000FFFUL);
		dwRegVal|=HW_CFG_SF_;
		Lan_SetRegDW(HW_CFG,dwRegVal);
	}
	Lan_SetRegDW(TX_CFG,TX_CFG_TX_ON_);

	Lan_SetRegDW(RX_CFG,(((DWORD)(privateData->LoopBackRxPacket))&0x03)<<8);

	{

	DWORD dwIntFlags=0;
	VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
	//Set Phy to 10/FD, no ANEG,
	Phy_SetRegW(privateData,PHY_BCR,0x0100,keyCode);

	//enable MAC Tx/Rx, FD
	Mac_SetRegDW(privateData,MAC_CR,MAC_CR_FDPX_|MAC_CR_TXEN_|MAC_CR_RXEN_,keyCode);

//	Phy_TransmitTestPacket(privateData);

	//set Phy to loopback mode
	Phy_SetRegW(privateData,PHY_BCR,0x4100,keyCode);

	for(tryCount=0;tryCount<10;tryCount++) {
		if(Phy_CheckLoopBackPacket(privateData))
		{
			result=TRUE;
			goto DONE;
		}
		privateData->dwResetCount++;
		//disable MAC rx
		Mac_SetRegDW(privateData,MAC_CR,0UL,keyCode);
		Phy_Reset(privateData,keyCode);

		//Set Phy to 10/FD, no ANEG, and Loopbackmode
		Phy_SetRegW(privateData,PHY_BCR,0x4100,keyCode);

		//enable MAC Tx/Rx, FD
		Mac_SetRegDW(privateData,MAC_CR,MAC_CR_FDPX_|MAC_CR_TXEN_|MAC_CR_RXEN_,keyCode);
	}
//	if(failed<2) {
//		if(tryCount>=10) {
//			DWORD timeOut=10000;
//			Lan_ShowRegs(privateData);
//			SMSC_TRACE("Performing full reset");
//			privateData->Lan9118->HW_CFG=HW_CFG_SRST_;
//			while((timeOut>0)&&(privateData->Lan9118->HW_CFG&HW_CFG_SRST_)) {
//				udelay(1);
//				timeOut--;
//			}
//			failed++;
//			goto TRY_AGAIN;
//		}
//	}
DONE:
	//disable MAC
	Mac_SetRegDW(privateData,MAC_CR,0UL,keyCode);
	//Cancel Phy loopback mode
	Phy_SetRegW(privateData,PHY_BCR,0U,keyCode);
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}

	Lan_SetRegDW(TX_CFG,0UL);
	Lan_SetRegDW(RX_CFG,0UL);

	return result;
}

#endif //USE_PHY_WORK_AROUND
void Phy_SetLink(PPRIVATE_DATA privateData,
				 DWORD dwLinkRequest)
{
	DWORD dwIntFlags=0;
	VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
	if(dwLinkRequest&LINK_AUTO_NEGOTIATE) {
		WORD wTemp;
		wTemp=Phy_GetRegW(privateData,
			PHY_ANEG_ADV,keyCode);
		wTemp&=~PHY_ANEG_ADV_PAUSE_;
		if(dwLinkRequest&LINK_ASYMMETRIC_PAUSE) {
			wTemp|=PHY_ANEG_ADV_ASYMP_;
		}
		if(dwLinkRequest&LINK_SYMMETRIC_PAUSE) {
			wTemp|=PHY_ANEG_ADV_SYMP_;
		}
		wTemp&=~PHY_ANEG_ADV_SPEED_;
		if(dwLinkRequest&LINK_SPEED_10HD) {
			wTemp|=PHY_ANEG_ADV_10H_;
		}
		if(dwLinkRequest&LINK_SPEED_10FD) {
			wTemp|=PHY_ANEG_ADV_10F_;
		}
		if(dwLinkRequest&LINK_SPEED_100HD) {
			wTemp|=PHY_ANEG_ADV_100H_;
		}
		if(dwLinkRequest&LINK_SPEED_100FD) {
			wTemp|=PHY_ANEG_ADV_100F_;
		}
		Phy_SetRegW(privateData,PHY_ANEG_ADV,wTemp,keyCode);

		// begin to establish link
		privateData->dwRemoteFaultCount=0;
		Phy_SetRegW(privateData,
			PHY_BCR,
			PHY_BCR_AUTO_NEG_ENABLE_|
			PHY_BCR_RESTART_AUTO_NEG_,
			keyCode);
	} else {
		WORD wTemp=0;
		if(dwLinkRequest&(LINK_SPEED_100FD)) {
			dwLinkRequest=LINK_SPEED_100FD;
		} else if(dwLinkRequest&(LINK_SPEED_100HD)) {
			dwLinkRequest=LINK_SPEED_100HD;
		} else if(dwLinkRequest&(LINK_SPEED_10FD)) {
			dwLinkRequest=LINK_SPEED_10FD;
		} else if(dwLinkRequest&(LINK_SPEED_10HD)) {
			dwLinkRequest=LINK_SPEED_10HD;
		}
		if(dwLinkRequest&(LINK_SPEED_10FD|LINK_SPEED_100FD)) {
			wTemp|=PHY_BCR_DUPLEX_MODE_;
		}
		if(dwLinkRequest&(LINK_SPEED_100HD|LINK_SPEED_100FD)) {
			wTemp|=PHY_BCR_SPEED_SELECT_;
		}
		Phy_SetRegW(privateData,PHY_BCR,wTemp,keyCode);
	}
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
}

BOOLEAN Phy_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwPhyAddr,
	DWORD dwLinkRequest)
{
	BOOLEAN result=FALSE;
	DWORD dwTemp=0;
	WORD wTemp=0;
	DWORD dwLoopCount=0;

	SMSC_TRACE("-->Phy_Initialize");
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);
	SMSC_ASSERT(dwLinkRequest<=0x7FUL);

	if(dwPhyAddr!=0xFFFFFFFFUL) {
		switch(privateData->dwIdRev&0xFFFF0000) {
		case 0x117A0000UL:
		case 0x115A0000UL:
			goto EXTERNAL_PHY_SUPPORTED;
		case 0x01170000UL:
		case 0x01150000UL:
			if(privateData->dwIdRev&0x0000FFFF) {
				DWORD dwHwCfg=0;
EXTERNAL_PHY_SUPPORTED:
				dwHwCfg=Lan_GetRegDW(HW_CFG);
				if(dwHwCfg&HW_CFG_EXT_PHY_DET_) {
                    //External phy is requested, supported, and detected
					//Attempt to switch
					//NOTE: Assuming Rx and Tx are stopped
					//  because Phy_Initialize is called before
					//  Rx_Initialize and Tx_Initialize
					WORD wPhyId1=0;
					WORD wPhyId2=0;

					//Disable phy clocks to the mac
					dwHwCfg&= (~HW_CFG_PHY_CLK_SEL_);
					dwHwCfg|= HW_CFG_PHY_CLK_SEL_CLK_DIS_;
					Lan_SetRegDW(HW_CFG,dwHwCfg);
					udelay(10);//wait for clocks to acutally stop

					dwHwCfg|=HW_CFG_EXT_PHY_EN_;
					Lan_SetRegDW(HW_CFG,dwHwCfg);

					dwHwCfg&= (~HW_CFG_PHY_CLK_SEL_);
					dwHwCfg|= HW_CFG_PHY_CLK_SEL_EXT_PHY_;
					Lan_SetRegDW(HW_CFG,dwHwCfg);
					udelay(10);//wait for clocks to actually start

					dwHwCfg|=HW_CFG_SMI_SEL_;
					Lan_SetRegDW(HW_CFG,dwHwCfg);

					{
						DWORD dwIntFlags=0;
						VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
						if(dwPhyAddr<=31) {
							//only check the phy address specified
							privateData->dwPhyAddress=dwPhyAddr;
							wPhyId1=Phy_GetRegW(privateData,PHY_ID_1,keyCode);
							wPhyId2=Phy_GetRegW(privateData,PHY_ID_2,keyCode);
						} else {
							//auto detect phy
							DWORD address=0;
							for(address=0;address<=31;address++) {
								privateData->dwPhyAddress=address;
								wPhyId1=Phy_GetRegW(privateData,PHY_ID_1,keyCode);
								wPhyId2=Phy_GetRegW(privateData,PHY_ID_2,keyCode);
								if((wPhyId1!=0xFFFFU)||(wPhyId2!=0xFFFFU)) {
									SMSC_TRACE("Detected Phy at address = 0x%02lX = %ld",
										address,address);
									break;
								}
							}
							if(address>=32) {
								SMSC_WARNING("Failed to auto detect external phy");
							}
						}
						Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
					}
					if((wPhyId1==0xFFFFU)&&(wPhyId2==0xFFFFU)) {
						SMSC_WARNING("External Phy is not accessable");
						SMSC_WARNING("  using internal phy instead");
						//revert back to interal phy settings.

						//Disable phy clocks to the mac
						dwHwCfg&= (~HW_CFG_PHY_CLK_SEL_);
						dwHwCfg|= HW_CFG_PHY_CLK_SEL_CLK_DIS_;
						Lan_SetRegDW(HW_CFG,dwHwCfg);
						udelay(10);//wait for clocks to actually stop

						dwHwCfg&=(~HW_CFG_EXT_PHY_EN_);
						Lan_SetRegDW(HW_CFG,dwHwCfg);

						dwHwCfg&= (~HW_CFG_PHY_CLK_SEL_);
						dwHwCfg|= HW_CFG_PHY_CLK_SEL_INT_PHY_;
						Lan_SetRegDW(HW_CFG,dwHwCfg);
						udelay(10);//wait for clocks to actually start

						dwHwCfg&=(~HW_CFG_SMI_SEL_);
						Lan_SetRegDW(HW_CFG,dwHwCfg);
						goto USE_INTERNAL_PHY;
					} else {
						SMSC_TRACE("Successfully switched to external phy");
#ifdef USE_LED1_WORK_AROUND
						privateData->NotUsingExtPhy=0;
#endif
					}
				} else {
					SMSC_WARNING("No External Phy Detected");
					SMSC_WARNING("  using internal phy instead");
					goto USE_INTERNAL_PHY;
				}
			} else {
				SMSC_WARNING("External Phy is not supported");
				SMSC_WARNING("  using internal phy instead");
				goto USE_INTERNAL_PHY;
			};break;
		default:
			SMSC_WARNING("External Phy is not supported");
			SMSC_WARNING("  using internal phy instead");
			goto USE_INTERNAL_PHY;
		}
	} else {
USE_INTERNAL_PHY:
		privateData->dwPhyAddress=1;
#ifdef USE_LED1_WORK_AROUND
		if(privateData->dwGeneration<=2) {
			privateData->NotUsingExtPhy=1;
		} else {
			//Generation 3 or higher has the LED problem fixed
			//  to disable the workaround pretend the phy is external
			privateData->NotUsingExtPhy=0;
		}
#endif
	}

	{
		DWORD dwIntFlags=0;
		VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
		dwTemp=Phy_GetRegW(privateData,PHY_ID_2,keyCode);
		privateData->bPhyRev=((BYTE)(dwTemp&(0x0FUL)));
		privateData->bPhyModel=((BYTE)((dwTemp>>4)&(0x3FUL)));
		privateData->dwPhyId=((dwTemp&(0xFC00UL))<<8);
		dwTemp=Phy_GetRegW(privateData,PHY_ID_1,keyCode);
		privateData->dwPhyId|=((dwTemp&(0x0000FFFFUL))<<2);

		SMSC_TRACE("dwPhyId==0x%08lX,bPhyModel==0x%02X,bPhyRev==0x%02X",
			privateData->dwPhyId,
			privateData->bPhyModel,
			privateData->bPhyRev);

		privateData->dwLinkSpeed=LINK_OFF;
		privateData->dwLinkSettings=LINK_OFF;
		//reset the PHY
		Phy_SetRegW(privateData,PHY_BCR,PHY_BCR_RESET_,keyCode);
	    dwLoopCount=100000;
		do {

			udelay(10);
			wTemp=Phy_GetRegW(privateData,PHY_BCR,keyCode);
			dwLoopCount--;
		} while((dwLoopCount>0) && (wTemp&PHY_BCR_RESET_));
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}

	if(wTemp&PHY_BCR_RESET_) {
		SMSC_WARNING("PHY reset failed to complete.");
		goto DONE;
	}

#ifdef USE_PHY_WORK_AROUND
	if(privateData->dwGeneration<=2) {
		if(!Phy_LoopBackTest(privateData)) {
			SMSC_WARNING("Failed Loop back test");
			goto DONE;
		} else {
			SMSC_TRACE("Passed Loop Back Test");
		}
	}
#endif
	Phy_SetLink(privateData,dwLinkRequest);

	init_timer(&(privateData->LinkPollingTimer));
	privateData->LinkPollingTimer.function=Phy_CheckLink;
	privateData->LinkPollingTimer.data=(unsigned long)privateData;
	privateData->LinkPollingTimer.expires=jiffies+HZ;
	add_timer(&(privateData->LinkPollingTimer));

	result=TRUE;
DONE:
	SMSC_TRACE("<--Phy_Initialize, result=%s",result?"TRUE":"FALSE");
	return result;
}

WORD Phy_GetRegW(
	PPRIVATE_DATA privateData,
	DWORD dwRegIndex,
	VL_KEY keyCode)
{
	DWORD dwAddr=0;
	int i=0;
	WORD result=0xFFFFU;

	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
	SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),keyCode));

	// confirm MII not busy
	if ((Mac_GetRegDW(privateData, MII_ACC,keyCode) & MII_ACC_MII_BUSY_) != 0UL)
	{
		SMSC_WARNING("MII is busy in Phy_GetRegW???");
		result=0;
		goto DONE;
	}

	// set the address, index & direction (read from PHY)
	dwAddr = ((privateData->dwPhyAddress&0x1FUL)<<11) | ((dwRegIndex & 0x1FUL)<<6);
	Mac_SetRegDW(privateData, MII_ACC, dwAddr,keyCode);

	// wait for read to complete w/ timeout
	for(i=0;i<100;i++) {
		// see if MII is finished yet
		if ((Mac_GetRegDW(privateData, MII_ACC,keyCode) & MII_ACC_MII_BUSY_) == 0UL)
		{
			// get the read data from the MAC & return i
			result=((WORD)Mac_GetRegDW(privateData, MII_DATA,keyCode));
			goto DONE;
		}
	}
	SMSC_WARNING("timeout waiting for MII write to finish");

DONE:
	return result;
}

void Phy_SetRegW(
	PPRIVATE_DATA privateData,
	DWORD dwRegIndex,WORD wVal,
	VL_KEY keyCode)
{
	DWORD dwAddr=0;
	int i=0;

	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);

	SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),keyCode));

	if(dwRegIndex==0) {
		if((wVal&0x1200)==0x1200) {
			privateData->wLastADVatRestart=privateData->wLastADV;
		}
	}
	if(dwRegIndex==4) {
		privateData->wLastADV=wVal;
	}

	// confirm MII not busy
	if ((Mac_GetRegDW(privateData, MII_ACC,keyCode) & MII_ACC_MII_BUSY_) != 0UL)
	{
		SMSC_WARNING("MII is busy in Phy_SetRegW???");
		goto DONE;
	}

	// put the data to write in the MAC
	Mac_SetRegDW(privateData, MII_DATA, (DWORD)wVal,keyCode);

	// set the address, index & direction (write to PHY)
	dwAddr = ((privateData->dwPhyAddress&0x1FUL)<<11) | ((dwRegIndex & 0x1FUL)<<6) | MII_ACC_MII_WRITE_;
	Mac_SetRegDW(privateData, MII_ACC, dwAddr,keyCode);

	// wait for write to complete w/ timeout
	for(i=0;i<100;i++) {
		// see if MII is finished yet
		if ((Mac_GetRegDW(privateData, MII_ACC,keyCode) & MII_ACC_MII_BUSY_) == 0UL)
		{
			goto DONE;
		}
	}
	SMSC_WARNING("timeout waiting for MII write to finish");
DONE:
	return;
}

void Phy_UpdateLinkMode(PPRIVATE_DATA privateData)
{
	DWORD dwOldLinkSpeed=privateData->dwLinkSpeed;
	DWORD dwIntFlags=0;
	VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);

	Phy_GetLinkMode(privateData,keyCode);

	if(dwOldLinkSpeed!=(privateData->dwLinkSpeed)) {
		if(privateData->dwLinkSpeed!=LINK_OFF) {
			DWORD dwRegVal=0;
			switch(privateData->dwLinkSpeed) {
			case LINK_SPEED_10HD:
				SMSC_TRACE("Link is now UP at 10Mbps HD");
				break;
			case LINK_SPEED_10FD:
				SMSC_TRACE("Link is now UP at 10Mbps FD");
				break;
			case LINK_SPEED_100HD:
				SMSC_TRACE("Link is now UP at 100Mbps HD");
				break;
			case LINK_SPEED_100FD:
				SMSC_TRACE("Link is now UP at 100Mbps FD");
				break;
			default:
				SMSC_WARNING("Link is now UP at Unknown Link Speed, dwLinkSpeed=0x%08lX",
					privateData->dwLinkSpeed);
				break;
			}

			dwRegVal=Mac_GetRegDW(privateData,MAC_CR,keyCode);
			dwRegVal&=~(MAC_CR_FDPX_|MAC_CR_RCVOWN_);
			switch(privateData->dwLinkSpeed) {
			case LINK_SPEED_10HD:
			case LINK_SPEED_100HD:
				dwRegVal|=MAC_CR_RCVOWN_;
				break;
			case LINK_SPEED_10FD:
			case LINK_SPEED_100FD:
				dwRegVal|=MAC_CR_FDPX_;
				break;
			default:break;//make lint happy
			}

			Mac_SetRegDW(privateData,
				MAC_CR,dwRegVal,keyCode);

			if(privateData->dwLinkSettings&LINK_AUTO_NEGOTIATE) {
				WORD linkPartner=0;
				WORD localLink=0;
				localLink=Phy_GetRegW(privateData,4,keyCode);
				linkPartner=Phy_GetRegW(privateData,5,keyCode);
				switch(privateData->dwLinkSpeed) {
				case LINK_SPEED_10FD:
				case LINK_SPEED_100FD:
					if(((localLink&linkPartner)&((WORD)0x0400U)) != ((WORD)0U)) {
						//Enable PAUSE receive and transmit
						Mac_SetRegDW(privateData,FLOW,0xFFFF0002UL,keyCode);
						Lan_SetBitsDW(AFC_CFG,(afc_cfg&0x0000000FUL));
					} else if(((localLink&((WORD)0x0C00U))==((WORD)0x0C00U)) &&
							((linkPartner&((WORD)0x0C00U))==((WORD)0x0800U)))
					{
						//Enable PAUSE receive, disable PAUSE transmit
						Mac_SetRegDW(privateData,FLOW,0xFFFF0002UL,keyCode);
						Lan_ClrBitsDW(AFC_CFG,0x0000000FUL);
					} else {
						//Disable PAUSE receive and transmit
						Mac_SetRegDW(privateData,FLOW,0UL,keyCode);
						Lan_ClrBitsDW(AFC_CFG,0x0000000FUL);
					};break;
				case LINK_SPEED_10HD:
				case LINK_SPEED_100HD:
					Mac_SetRegDW(privateData,FLOW,0UL,keyCode);
					Lan_SetBitsDW(AFC_CFG,0x0000000FUL);
					break;
				default:break;//make lint happy
				}
				SMSC_TRACE("LAN9118: %s,%s,%s,%s,%s,%s",
					(localLink&PHY_ANEG_ADV_ASYMP_)?"ASYMP":"     ",
					(localLink&PHY_ANEG_ADV_SYMP_)?"SYMP ":"     ",
					(localLink&PHY_ANEG_ADV_100F_)?"100FD":"     ",
					(localLink&PHY_ANEG_ADV_100H_)?"100HD":"     ",
					(localLink&PHY_ANEG_ADV_10F_)?"10FD ":"     ",
					(localLink&PHY_ANEG_ADV_10H_)?"10HD ":"     ");

				SMSC_TRACE("Partner: %s,%s,%s,%s,%s,%s",
					(linkPartner&PHY_ANEG_LPA_ASYMP_)?"ASYMP":"     ",
					(linkPartner&PHY_ANEG_LPA_SYMP_)?"SYMP ":"     ",
					(linkPartner&PHY_ANEG_LPA_100FDX_)?"100FD":"     ",
					(linkPartner&PHY_ANEG_LPA_100HDX_)?"100HD":"     ",
					(linkPartner&PHY_ANEG_LPA_10FDX_)?"10FD ":"     ",
					(linkPartner&PHY_ANEG_LPA_10HDX_)?"10HD ":"     ");
			} else {
				switch(privateData->dwLinkSpeed) {
				case LINK_SPEED_10HD:
				case LINK_SPEED_100HD:
					Mac_SetRegDW(privateData,FLOW,0x0UL,keyCode);
					Lan_SetBitsDW(AFC_CFG,0x0000000FUL);
					break;
				default:
					Mac_SetRegDW(privateData,FLOW,0x0UL,keyCode);
					Lan_ClrBitsDW(AFC_CFG,0x0000000FUL);
					break;
				}
			}
			netif_carrier_on(privateData->dev);
			Tx_WakeQueue(privateData,0x01);
#ifdef USE_LED1_WORK_AROUND
			if ((g_GpioSettingOriginal & GPIO_CFG_LED1_EN_) &&
				privateData->NotUsingExtPhy)
			{
				// Restore orginal GPIO configuration
				g_GpioSetting = g_GpioSettingOriginal;
				Lan_SetRegDW(GPIO_CFG,g_GpioSetting);
			}
#endif // USE_LED1_WORK_AROUND
		} else {
			SMSC_TRACE("Link is now DOWN");
			Tx_StopQueue(privateData,0x01);
			netif_carrier_off(privateData->dev);
			Mac_SetRegDW(privateData,FLOW,0UL,keyCode);
			Lan_ClrBitsDW(AFC_CFG,0x0000000FUL);
#ifdef USE_LED1_WORK_AROUND
			// Check global setting that LED1 usage is 10/100 indicator
//			g_GpioSetting = Lan_GetRegDW(GPIO_CFG);
			if ((g_GpioSetting & GPIO_CFG_LED1_EN_) &&
				privateData->NotUsingExtPhy)
			{
				//Force 10/100 LED off, after saving orginal GPIO configuration
				g_GpioSettingOriginal = g_GpioSetting;

				g_GpioSetting &= ~GPIO_CFG_LED1_EN_;
				g_GpioSetting |=
					(GPIO_CFG_GPIOBUF0_|GPIO_CFG_GPIODIR0_|GPIO_CFG_GPIOD0_);
				Lan_SetRegDW(GPIO_CFG,g_GpioSetting);
			}
#endif // USE_LED1_WORK_AROUND
		}
	}
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
}

void Phy_CheckLink(unsigned long ptr)
{
	PPRIVATE_DATA privateData=(PPRIVATE_DATA)ptr;
	if(privateData==NULL) {
		SMSC_WARNING("Phy_CheckLink(ptr==0)");
		return;
	}

	//must call this twice
	Phy_UpdateLinkMode(privateData);
	Phy_UpdateLinkMode(privateData);

	if(!(privateData->StopLinkPolling)) {
		privateData->LinkPollingTimer.expires=jiffies+HZ;
		add_timer(&(privateData->LinkPollingTimer));
	}
}

void Phy_GetLinkMode(
	PPRIVATE_DATA privateData,
	VL_KEY keyCode)
{
	DWORD result=LINK_OFF;
	WORD wRegVal=0;
	WORD wRegBSR=Phy_GetRegW(
		privateData,
		PHY_BSR,keyCode);
	privateData->dwLinkSettings=LINK_OFF;
	if(wRegBSR&PHY_BSR_LINK_STATUS_) {
		wRegVal=Phy_GetRegW(
			privateData,
			PHY_BCR,keyCode);
		if(wRegVal&PHY_BCR_AUTO_NEG_ENABLE_) {
			DWORD linkSettings=LINK_AUTO_NEGOTIATE;
			WORD wRegADV=privateData->wLastADVatRestart;
//					Phy_GetRegW(
//						privateData,
//						PHY_ANEG_ADV,keyCode);
			WORD wRegLPA=Phy_GetRegW(
				privateData,
				PHY_ANEG_LPA,keyCode);
			if(wRegADV&PHY_ANEG_ADV_ASYMP_) {
				linkSettings|=LINK_ASYMMETRIC_PAUSE;
			}
			if(wRegADV&PHY_ANEG_ADV_SYMP_) {
				linkSettings|=LINK_SYMMETRIC_PAUSE;
			}
			if(wRegADV&PHY_ANEG_LPA_100FDX_) {
				linkSettings|=LINK_SPEED_100FD;
			}
			if(wRegADV&PHY_ANEG_LPA_100HDX_) {
				linkSettings|=LINK_SPEED_100HD;
			}
			if(wRegADV&PHY_ANEG_LPA_10FDX_) {
				linkSettings|=LINK_SPEED_10FD;
			}
			if(wRegADV&PHY_ANEG_LPA_10HDX_) {
				linkSettings|=LINK_SPEED_10HD;
			}
			privateData->dwLinkSettings=linkSettings;
			wRegLPA&=wRegADV;
			if(wRegLPA&PHY_ANEG_LPA_100FDX_) {
				result=LINK_SPEED_100FD;
			} else if(wRegLPA&PHY_ANEG_LPA_100HDX_) {
				result=LINK_SPEED_100HD;
			} else if(wRegLPA&PHY_ANEG_LPA_10FDX_) {
				result=LINK_SPEED_10FD;
			} else if(wRegLPA&PHY_ANEG_LPA_10HDX_) {
				result=LINK_SPEED_10HD;
			}
		} else {
			if(wRegVal&PHY_BCR_SPEED_SELECT_) {
				if(wRegVal&PHY_BCR_DUPLEX_MODE_) {
					privateData->dwLinkSettings=result=LINK_SPEED_100FD;
				} else {
					privateData->dwLinkSettings=result=LINK_SPEED_100HD;
				}
			} else {
				if(wRegVal&PHY_BCR_DUPLEX_MODE_) {
					privateData->dwLinkSettings=result=LINK_SPEED_10FD;
				} else {
					privateData->dwLinkSettings=result=LINK_SPEED_10HD;
				}
			}
		}
	}
	privateData->dwLinkSpeed=result;
}

BOOLEAN Mac_Initialize(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	//This function is kept only as a place holder

	return TRUE;
}

static BOOLEAN MacNotBusy(PPRIVATE_DATA privateData, VL_KEY keyCode)
{
	int i=0;
	SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),keyCode));
	// wait for MAC not busy, w/ timeout
	for(i=0;i<40;i++)
	{
		if((Lan_GetRegDW(MAC_CSR_CMD) & MAC_CSR_CMD_CSR_BUSY_)==(0UL)) {
			return TRUE;
		}
	}
	SMSC_WARNING("timeout waiting for MAC not BUSY. MAC_CSR_CMD = 0x%08lX",
		Lan_GetRegDW(MAC_CSR_CMD));
	return FALSE;
}

DWORD Mac_GetRegDW(PPRIVATE_DATA privateData,DWORD dwRegOffset,VL_KEY keyCode)
{
	DWORD result=0xFFFFFFFFUL;
	DWORD dwTemp=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
	SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),keyCode));
	SMSC_ASSERT(privateData->dwLanBase!=0);

	// wait until not busy
	if (Lan_GetRegDW(MAC_CSR_CMD) & MAC_CSR_CMD_CSR_BUSY_)
	{
		SMSC_WARNING("Mac_GetRegDW() failed, MAC already busy at entry");
		goto DONE;
	}

	// send the MAC Cmd w/ offset
	Lan_SetRegDW(MAC_CSR_CMD,
		((dwRegOffset & 0x000000FFUL) |
		MAC_CSR_CMD_CSR_BUSY_ | MAC_CSR_CMD_R_NOT_W_));
	dwTemp=Lan_GetRegDW(BYTE_TEST);//to flush previous write
	dwTemp=dwTemp;

	// wait for the read to happen, w/ timeout
	if (!MacNotBusy(privateData,keyCode))
	{
		SMSC_WARNING("Mac_GetRegDW() failed, waiting for MAC not busy after read");
		goto DONE;
	} else {
		// finally, return the read data
		result=Lan_GetRegDW(MAC_CSR_DATA);
	}
DONE:
	return result;
}

void Mac_SetRegDW(PPRIVATE_DATA privateData,DWORD dwRegOffset,DWORD dwVal,VL_KEY keyCode)
{
	DWORD dwTemp=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
	SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),keyCode));
	SMSC_ASSERT(privateData->dwLanBase!=0);

	if (Lan_GetRegDW(MAC_CSR_CMD) & MAC_CSR_CMD_CSR_BUSY_)
	{
		SMSC_WARNING("Mac_SetRegDW() failed, MAC already busy at entry");
		goto DONE;
	}

	// send the data to write
	Lan_SetRegDW(MAC_CSR_DATA,dwVal);

	// do the actual write
	Lan_SetRegDW(MAC_CSR_CMD,((dwRegOffset & 0x000000FFUL) | MAC_CSR_CMD_CSR_BUSY_));
	dwTemp=Lan_GetRegDW(BYTE_TEST);//force flush of previous write
	dwTemp=dwTemp;

	// wait for the write to complete, w/ timeout
	if (!MacNotBusy(privateData,keyCode))
	{
		SMSC_WARNING("Mac_SetRegDW() failed, waiting for MAC not busy after write");
	}
DONE:
	return;
}

#define TX_FIFO_LOW_THRESHOLD	(1600)

void Tx_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwTxDmaCh,
	DWORD dwDmaThreshold)
{
	DWORD dwRegVal=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);

	dwRegVal=Lan_GetRegDW(HW_CFG);
	dwRegVal&=(HW_CFG_TX_FIF_SZ_|0x00000FFFUL);
	dwRegVal|=HW_CFG_SF_;
	Lan_SetRegDW(HW_CFG,dwRegVal);

	Lan_SetTDFL(privateData,0xFF);
	Lan_EnableInterrupt(privateData,INT_EN_TDFA_EN_);

	privateData->dwTxDmaThreshold=dwDmaThreshold;
	privateData->dwTxDmaCh=dwTxDmaCh;
	if(dwTxDmaCh>=TRANSFER_PIO) {
		SMSC_TRACE("Tx will use PIO");
	} else {
		SMSC_TRACE("Tx will use DMA channel %ld",dwTxDmaCh);
		SMSC_ASSERT(Platform_IsValidDmaChannel(dwTxDmaCh));
		if(!Platform_DmaInitialize(
			&(privateData->PlatformData),
			dwTxDmaCh))
		{
			SMSC_WARNING("Failed Platform_DmaInitialize, dwTxDmaCh=%lu",dwTxDmaCh);
		}
		privateData->TxDmaXfer.dwLanReg=privateData->dwLanBasePhy+TX_DATA_FIFO;
		privateData->TxDmaXfer.pdwBuf=NULL;//this will be reset per dma request
		privateData->TxDmaXfer.dwDmaCh=privateData->dwTxDmaCh;
		privateData->TxDmaXfer.dwDwCnt=0;//this will be reset per dma request
		privateData->TxDmaXfer.fMemWr=FALSE;
	}

	{
		DWORD dwIntFlags=0;
		VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
		DWORD dwMacCr=Mac_GetRegDW(privateData,MAC_CR,keyCode);
		dwMacCr|=(MAC_CR_TXEN_|MAC_CR_HBDIS_);
		Mac_SetRegDW(privateData,MAC_CR,dwMacCr,keyCode);
		Lan_SetRegDW(TX_CFG,TX_CFG_TX_ON_);
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}

	privateData->TxSkb=NULL;
	spin_lock_init(&(privateData->TxSkbLock));
	privateData->dwTxQueueDisableMask=0;
	spin_lock_init(&(privateData->TxQueueLock));
	spin_lock_init(&(privateData->TxCounterLock));
	privateData->TxInitialized=TRUE;
}

BOOLEAN Tx_HandleInterrupt(
	PPRIVATE_DATA privateData,DWORD dwIntSts)
{
	SMSC_ASSERT(privateData!=NULL);
	if(dwIntSts&INT_STS_TDFA_)
	{
		Lan_SetTDFL(privateData,0xFF);
		Lan_SetRegDW(INT_STS,INT_STS_TDFA_);
		Tx_WakeQueue(privateData,0x02UL);
		return TRUE;
	}
	return FALSE;
}

void Tx_StopQueue(
	PPRIVATE_DATA privateData,DWORD dwSource)
{
	DWORD intFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dev!=NULL);
	SMSC_ASSERT(privateData->TxInitialized);
	spin_lock_irqsave(&(privateData->TxQueueLock),intFlags);
	if(privateData->dwTxQueueDisableMask==0) {
		netif_stop_queue(privateData->dev);
	}
	privateData->dwTxQueueDisableMask|=dwSource;
	spin_unlock_irqrestore(&(privateData->TxQueueLock),intFlags);
}

void Tx_WakeQueue(
	PPRIVATE_DATA privateData,DWORD dwSource)
{
	DWORD intFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dev!=NULL);
	SMSC_ASSERT(privateData->TxInitialized);
	spin_lock_irqsave(&(privateData->TxQueueLock),intFlags);
	privateData->dwTxQueueDisableMask&=(~dwSource);
	if(privateData->dwTxQueueDisableMask==0) {
		netif_wake_queue(privateData->dev);
	}
	spin_unlock_irqrestore(&(privateData->TxQueueLock),intFlags);
}

static DWORD Tx_GetTxStatusCount(
	PPRIVATE_DATA privateData)
{
	DWORD result=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);
	result=Lan_GetRegDW(TX_FIFO_INF);
	if(OLD_REGISTERS(privateData)) {
		result&=TX_FIFO_INF_TSFREE_;
		result>>=16;
		if(result>0x80) {
			SMSC_WARNING("TX_FIFO_INF_TSFREE_>0x80");
			result=0x80;
		}
		result=0x80-result;
	} else {
		result&=TX_FIFO_INF_TSUSED_;
		result>>=16;
	}
	return result;
}

void Tx_SendSkb(
	PPRIVATE_DATA privateData,
	struct sk_buff *skb)
{
	DWORD dwFreeSpace=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);
	if(privateData->dwTxDmaCh>=TRANSFER_PIO)
	{
		//Use PIO only
		DWORD dwTxCmdA=0;
		DWORD dwTxCmdB=0;
		dwFreeSpace=Lan_GetRegDW(TX_FIFO_INF);
		dwFreeSpace&=TX_FIFO_INF_TDFREE_;
		if(dwFreeSpace<TX_FIFO_LOW_THRESHOLD) {
			SMSC_WARNING("Tx Data Fifo Low, space available = %ld",dwFreeSpace);
		}
		dwTxCmdA=
			((((DWORD)(skb->data))&0x03UL)<<16) | //DWORD alignment adjustment
			TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
			((DWORD)(skb->len));
		dwTxCmdB=
			(((DWORD)(skb->len))<<16) |
			((DWORD)(skb->len));
		Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdA);
		Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdB);
		Platform_WriteFifo(
			privateData->dwLanBase,
			(DWORD *)(((DWORD)(skb->data))&0xFFFFFFFCUL),
			(((DWORD)(skb->len))+3+
			(((DWORD)(skb->data))&0x03UL))>>2);
		dwFreeSpace-=(skb->len+32);
		dev_kfree_skb(skb);
	}
	else
	{
		//Use DMA and PIO
		PPLATFORM_DATA platformData=&(privateData->PlatformData);
		SMSC_ASSERT(TX_FIFO_LOW_THRESHOLD>(skb->len+32));

		BUG_ON((privateData->dwTxQueueDisableMask & 0x04UL) != 0);
		BUG_ON(privateData->TxSkb != NULL);

		if(skb->len>=privateData->dwTxDmaThreshold)
		{
			//use DMA
			DWORD dwTxCmdA;
			DWORD dwTxCmdB;

			//prepare for 16 byte alignment
			dwTxCmdA=
#if (PLATFORM_CACHE_LINE_BYTES == 16)
				(0x01UL<<24)|//16 byte end alignment
#endif
#if (PLATFORM_CACHE_LINE_BYTES == 32)
				(0x02UL<<24)|//32 byte end alignment
#endif
				((((DWORD)(skb->data))&(PLATFORM_CACHE_LINE_BYTES-1))<<16) |//16 Byte start alignment
				TX_CMD_A_INT_FIRST_SEG_ |
				TX_CMD_A_INT_LAST_SEG_ |
				((DWORD)(skb->len));//buffer length
			dwTxCmdB=
				(((DWORD)(skb->len))<<16)|
				((DWORD)(skb->len)&0x7FFUL);
			privateData->TxDmaXfer.pdwBuf=
				(DWORD *)(((DWORD)(skb->data))&
					(~(PLATFORM_CACHE_LINE_BYTES-1)));
			privateData->TxDmaXfer.dwDwCnt=
				((((DWORD)(skb->len))+
				(PLATFORM_CACHE_LINE_BYTES-1)+
				(((DWORD)(skb->data))&
					(PLATFORM_CACHE_LINE_BYTES-1)))&
					(~(PLATFORM_CACHE_LINE_BYTES-1)))>>2;
			Platform_CachePurge(
				platformData,
				privateData->TxDmaXfer.pdwBuf,
				(privateData->TxDmaXfer.dwDwCnt)<<2);

			spin_lock(&(privateData->TxSkbLock));
			{
				dwFreeSpace=Lan_GetRegDW(TX_FIFO_INF);
				dwFreeSpace&=TX_FIFO_INF_TDFREE_;
				if(dwFreeSpace<TX_FIFO_LOW_THRESHOLD) {
					SMSC_WARNING("Tx DATA FIFO LOW, space available = %ld",dwFreeSpace);
				}

				Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdA);
				Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdB);
				if(!Platform_DmaStartXfer(platformData,&(privateData->TxDmaXfer), Tx_DmaCompletionCallback, privateData))
				{
					SMSC_WARNING("Failed Platform_DmaStartXfer");
				}

				dwFreeSpace-=(skb->len+32);
				privateData->TxSkb=skb;
			}
			spin_unlock(&(privateData->TxSkbLock));

			Tx_StopQueue(privateData,0x04UL);
		}
		else
		{
			//use PIO
			DWORD dwTxCmdA=0;
			DWORD dwTxCmdB=0;
			dwFreeSpace=Lan_GetRegDW(TX_FIFO_INF);
			dwFreeSpace&=TX_FIFO_INF_TDFREE_;
			if(dwFreeSpace<TX_FIFO_LOW_THRESHOLD) {
				SMSC_WARNING("Tx DATA FIFO LOW, space available = %ld",dwFreeSpace);
			}
			dwTxCmdA=
				((((DWORD)(skb->data))&0x03UL)<<16) | //DWORD alignment adjustment
				TX_CMD_A_INT_FIRST_SEG_ | TX_CMD_A_INT_LAST_SEG_ |
				((DWORD)(skb->len));
			dwTxCmdB=
				(((DWORD)(skb->len))<<16) |
				((DWORD)(skb->len));
			Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdA);
			Lan_SetRegDW(TX_DATA_FIFO,dwTxCmdB);
			Platform_WriteFifo(
				privateData->dwLanBase,
				(DWORD *)(((DWORD)(skb->data))&0xFFFFFFFCUL),
				(((DWORD)(skb->len))+3+
				(((DWORD)(skb->data))&0x03UL))>>2);

			dwFreeSpace-=(skb->len+32);
			dev_kfree_skb(skb);
		}
	}
	if(Tx_GetTxStatusCount(privateData)>=30)
	{
		Tx_UpdateTxCounters(privateData);
	}
	if(dwFreeSpace<TX_FIFO_LOW_THRESHOLD) {
		Tx_StopQueue(privateData,0x02UL);
		Lan_SetTDFL(privateData,0x32);
	}
}

static void Tx_DmaCompletionCallback(void* param)
{
	PPRIVATE_DATA privateData = param;

	BUG_ON(privateData->TxSkb == NULL);
	dev_kfree_skb(privateData->TxSkb);
	privateData->TxSkb = NULL;
	Tx_WakeQueue(privateData,0x04UL);
}

static DWORD Tx_CompleteTx(
	PPRIVATE_DATA privateData)
{
	DWORD result=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);
	SMSC_ASSERT(privateData->TxInitialized==TRUE);
	result=Lan_GetRegDW(TX_FIFO_INF);
	if(OLD_REGISTERS(privateData)) {
		result&=TX_FIFO_INF_TSFREE_;
		if(result!=0x00800000UL) {
			result=Lan_GetRegDW(TX_STATUS_FIFO);
		} else {
			result=0;
		}
	} else {
		result&=TX_FIFO_INF_TSUSED_;
		if(result!=0x00000000UL) {
			result=Lan_GetRegDW(TX_STATUS_FIFO);
		} else {
			result=0;
		}
	}
	return result;
}

void Tx_UpdateTxCounters(
	PPRIVATE_DATA privateData)
{

	DWORD dwTxStatus=0;
	SMSC_ASSERT(privateData!=NULL);
	spin_lock(&(privateData->TxCounterLock));
	while((dwTxStatus=Tx_CompleteTx(privateData))!=0)
	{
		if(dwTxStatus&0x80000000UL) {
			SMSC_WARNING("Packet tag reserved bit is high");
			privateData->stats.tx_errors++;
		} else if(dwTxStatus&0x00007080UL) {
			SMSC_WARNING("Tx Status reserved bits are high");
			privateData->stats.tx_errors++;
		} else {
			if(dwTxStatus&0x00008000UL) {
				privateData->stats.tx_errors++;
			} else {
				privateData->stats.tx_packets++;
				privateData->stats.tx_bytes+=(dwTxStatus>>16);
			}
			if(dwTxStatus&0x00000100UL) {
				privateData->stats.collisions+=16;
				privateData->stats.tx_aborted_errors+=1;
			} else {
				privateData->stats.collisions+=
					((dwTxStatus>>3)&0xFUL);
			}
			if(dwTxStatus&0x00000800UL) {
				privateData->stats.tx_carrier_errors+=1;
			}
			if(dwTxStatus&0x00000200UL) {
				privateData->stats.collisions++;
				privateData->stats.tx_aborted_errors++;
			}
		}
	}
	spin_unlock(&(privateData->TxCounterLock));
}

void Tx_CompleteDma(
	PPRIVATE_DATA privateData)
{
	DWORD dwTimeOut=100000;

	SMSC_ASSERT(privateData!=NULL);

	while ((privateData->TxSkb) && (dwTimeOut)) {
		udelay(10);
		dwTimeOut--;
	}
	if (dwTimeOut == 0)
		SMSC_WARNING("Timed out waiting for Tx DMA complete");
}

void Rx_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwRxDmaCh,
	DWORD dwDmaThreshold)
{
	SMSC_ASSERT(privateData!=NULL);

	privateData->dwRxDmaCh=dwRxDmaCh;
	if(dwRxDmaCh>=TRANSFER_PIO) {
		SMSC_TRACE("Rx will use PIO");
		Platform_GetFlowControlParameters(
			&(privateData->PlatformData),
			&(privateData->RxFlowParameters),
			FALSE);
	} else {
		SMSC_TRACE("Rx will use DMA Channel %ld",dwRxDmaCh);
		SMSC_ASSERT(Platform_IsValidDmaChannel(dwRxDmaCh));
		if(!Platform_DmaInitialize(
			&(privateData->PlatformData),
			dwRxDmaCh))
		{
			SMSC_WARNING("Failed Platform_DmaInitialize, dwRxDmaCh=%lu",dwRxDmaCh);
		}
		Platform_GetFlowControlParameters(
			&(privateData->PlatformData),
			&(privateData->RxFlowParameters),
			TRUE);
	}
	if(max_throughput!=0xFFFFFFFFUL) {
		privateData->RxFlowParameters.MaxThroughput=max_throughput;
	}
	if(max_packet_count!=0xFFFFFFFFUL) {
		privateData->RxFlowParameters.MaxPacketCount=max_packet_count;
	}
	if(packet_cost!=0xFFFFFFFFUL) {
		privateData->RxFlowParameters.PacketCost=packet_cost;
	}
	if(burst_period!=0xFFFFFFFFUL) {
		privateData->RxFlowParameters.BurstPeriod=burst_period;
	}
	if(privateData->RxFlowParameters.BurstPeriod==0) {
		SMSC_WARNING("burst_period of 0 is not allowed");
		SMSC_WARNING(" resetting burst_period to 100");
		privateData->RxFlowParameters.BurstPeriod=100;
	}
	if(max_work_load!=0xFFFFFFFFUL) {
		privateData->RxFlowMaxWorkLoad=max_work_load;
	} else {
		privateData->RxFlowMaxWorkLoad=
			privateData->RxFlowParameters.MaxThroughput+
			(privateData->RxFlowParameters.MaxPacketCount*
			privateData->RxFlowParameters.PacketCost);
	}
	privateData->RxFlowBurstMaxWorkLoad=
			(privateData->RxFlowMaxWorkLoad*
			privateData->RxFlowParameters.BurstPeriod)/1000;
	if(int_deas!=0xFFFFFFFFUL) {
		Lan_SetIntDeas(privateData,int_deas);
	} else {
		Lan_SetIntDeas(privateData,privateData->RxFlowParameters.IntDeas);
	}

	//initially the receiver is off
	//  a following link up detection will turn the receiver on
	privateData->dwRxOffCount=1;
	Lan_SetRegDW(RX_CFG,0x00000200UL);
	Rx_ReceiverOn(privateData, 0);

	privateData->dwRxDmaThreshold=dwDmaThreshold;
	Lan_SetRDFL(privateData,0x01);
	Lan_SetRSFL(privateData,0x00);
	privateData->RxInterrupts=INT_EN_RSFL_EN_;
	privateData->RxInterrupts|=INT_EN_RXE_EN_;
	if(privateData->dwGeneration==0) {
		privateData->RxInterrupts|=INT_EN_RDFL_EN_;
		privateData->RxSkbsMax = 1;
	} else {
		privateData->RxInterrupts|=INT_EN_RDFO_EN_;
		privateData->RxSkbsMax = MAX_RX_SKBS;
	}
	privateData->RxInterrupts|=INT_EN_RXDFH_INT_EN_;
	Lan_EnableInterrupt(privateData,privateData->RxInterrupts);
}

static void Rx_HandleOverrun(PPRIVATE_DATA privateData)
{
	if(privateData->dwGeneration==0) {
		if(privateData->RxOverrun==FALSE) {
			Rx_ReceiverOff(privateData);
			privateData->RxUnloadProgress=
					(((((privateData->LastRxStatus1)&0x3FFF0000UL)>>16)+2+3)&0xFFFFFFFCUL);
			if(privateData->dwRxDmaCh<TRANSFER_REQUEST_DMA) {
				privateData->RxUnloadProgress+=
					(((((privateData->LastRxStatus2)&0x3FFF0000UL)>>16)+2+3)&0xFFFFFFFCUL);
			}
			privateData->RxUnloadPacketProgress=0;
			privateData->RxOverrun=TRUE;
			privateData->RxOverrunCount++;
		}
	} else {
		privateData->RxOverrunCount++;
	}
}

static void Rx_HandOffSkb(
	PPRIVATE_DATA privateData,
	struct sk_buff *skb)
{
	int result=0;

	skb->dev=privateData->dev;
	skb->protocol= eth_type_trans(skb,privateData->dev);
	skb->ip_summed = CHECKSUM_NONE;

	result=netif_rx(skb);

	switch(result)
	{
	case NET_RX_SUCCESS:
		break;
	case NET_RX_CN_LOW:
	case NET_RX_CN_MOD:
	case NET_RX_CN_HIGH:
	case NET_RX_DROP:
		privateData->RxCongested=TRUE;
		privateData->RxCongestedCount++;
		break;
	default:
		privateData->RxCongested=TRUE;
		privateData->RxCongestedCount++;
		SMSC_WARNING("Unknown return value from netif_rx, result=%d",result);
		break;
	}
}

void Rx_CompleteMulticastUpdate (PPRIVATE_DATA privateData)
{
	DWORD local_MACCR;
	VL_KEY keyCode=0;
	DWORD dwIntFlags=0;

	keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
	if (privateData->MulticastUpdatePending) {
		SET_GPIO(GP_COMPLETE_MULTICAST_UPDATE);
		Mac_SetRegDW(privateData,HASHH,privateData->HashHi,keyCode);
		Mac_SetRegDW(privateData,HASHL,privateData->HashLo,keyCode);
		local_MACCR = Mac_GetRegDW(privateData,MAC_CR,keyCode);
		local_MACCR |= privateData->set_bits_mask;
		local_MACCR &= ~(privateData->clear_bits_mask);
		Mac_SetRegDW(privateData,MAC_CR,local_MACCR,keyCode);
		Rx_ReceiverOn(privateData, keyCode);
		privateData->MulticastUpdatePending = FALSE;
		CLEAR_GPIO(GP_COMPLETE_MULTICAST_UPDATE);
	}
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
}

void Rx_BeginMulticastUpdate (PPRIVATE_DATA privateData)
{
	DWORD startTime, currentTime;
	DWORD timeout;
	DWORD flags;

	SET_GPIO(GP_BEGIN_MULTICAST_UPDATE);

	//NOTE: we can't rely on privateData->dwLinkSpeed because
	// it updates only once per second and may be out dated.

	local_irq_save(flags);
	Rx_ReceiverOff(privateData);
	if(privateData->dwGeneration>0) {
		//since this is concord or later there is no
		// overrun processing that might turn off the receiver.
		// there for we can rely on RxStop Int.

		//if the speed is 100Mb then lets poll rx stop to get the
		//  quickest response.
		timeout = 200UL;
		while ((timeout)&&(!(Lan_GetRegDW(INT_STS)&(INT_STS_RXSTOP_INT_)))) {
			// wait 1 uSec
			startTime=Lan_GetRegDW(FREE_RUN);
			while (1) {
				currentTime=Lan_GetRegDW(FREE_RUN);
				if (currentTime-startTime >= 25UL)
					break;
			}
			timeout--;
		}
		if(timeout==0) {
			//this is probably a 10Mb link, therefore prepare
			// interrupt for update later.
			Lan_EnableInterrupt(privateData,INT_EN_RXSTOP_INT_EN_);

			// if this is a 10Mbps half duplex connection
			//  then Rx stop is only 99.6%  reliable
			//  Therefor we must schedule Gpt callback as
			//  back up

			// using 18*(100uS) because we already waited 200uS
			Gpt_ScheduleCallBack(privateData,GptCB_RxCompleteMulticast, 18UL);
		} else {
			//Rx is stopped
			Lan_SetRegDW(INT_STS,INT_STS_RXSTOP_INT_);//clear interrupt signal
			Rx_CompleteMulticastUpdate(privateData);
		}
	} else {
		// for generation 0 we can't rely on Rx stop because
		// the receiver may have already been stopped due to
		// overflow processing

		// for the same reason we can't just wait 200uS and
		// check stopped status there for we must rely on GP timer
		// and we must assume a worse case of 10Mb speed

		Gpt_ScheduleCallBack(privateData,GptCB_RxCompleteMulticast, 20UL);
	}
	local_irq_restore (flags);
	CLEAR_GPIO(GP_BEGIN_MULTICAST_UPDATE);
}

static DWORD Rx_PopRxStatus(
	PPRIVATE_DATA privateData)
{
	DWORD result=Lan_GetRegDW(RX_FIFO_INF);
	if((privateData->RxCongested==FALSE)||
		((privateData->RxCongested==TRUE)&&((result&0x00FF0000UL)==0UL)))
	{
		if(result&0x00FF0000UL) {
			DWORD dwIntSts=Lan_GetRegDW(INT_STS);
			if(privateData->dwGeneration==0) {
				if(dwIntSts&INT_STS_RDFL_) {
					Lan_SetRegDW(INT_STS,INT_STS_RDFL_);
					Rx_HandleOverrun(privateData);
				}
			} else {
				if(dwIntSts&INT_STS_RDFO_) {
					Lan_SetRegDW(INT_STS,INT_STS_RDFO_);
					Rx_HandleOverrun(privateData);
				}
			}
			if((privateData->RxFlowControlActive==FALSE)||
				((privateData->RxFlowControlActive==TRUE)&&
				 (privateData->RxFlowBurstActive==TRUE)))
			{
				//Rx status is available, read it
				result=Lan_GetRegDW(RX_STATUS_FIFO);
				privateData->RxStatusDWReadCount++;
				privateData->LastRxStatus3=
					privateData->LastRxStatus2;
				privateData->LastRxStatus2=
					privateData->LastRxStatus1;
				privateData->LastRxStatus1=result;

				if(privateData->RxOverrun) {
					DWORD dwPacketLength=((result&0x3FFF0000UL)>>16);
					DWORD dwByteCount=((dwPacketLength+2+3)&0xFFFFFFFCUL);
					if((privateData->RxUnloadProgress+dwByteCount)>=
						((privateData->RxMaxDataFifoSize)-16))
					{
						//This is the packet that crosses the corruption point
						//  so just ignore it and complete the overrun processing.
						result=0;
						goto FINISH_OVERRUN_PROCESSING;
					}
					privateData->RxUnloadProgress+=dwByteCount;
					privateData->RxUnloadPacketProgress++;
				}

				privateData->RxFlowCurrentThroughput+=
						((((result&0x3FFF0000UL)>>16)-4UL));
				privateData->RxFlowCurrentPacketCount++;
				privateData->RxFlowCurrentWorkLoad+=
						((((result&0x3FFF0000UL)>>16)-4UL)+privateData->RxFlowParameters.PacketCost);
				if(privateData->RxFlowControlActive) {
					privateData->RxFlowBurstWorkLoad+=
						((((result&0x3FFF0000UL)>>16)-4UL)+privateData->RxFlowParameters.PacketCost);
					if(privateData->RxFlowBurstWorkLoad>=
						privateData->RxFlowBurstMaxWorkLoad)
					{
						privateData->RxFlowBurstActive=FALSE;
						Lan_DisableInterrupt(privateData,privateData->RxInterrupts);
					}
				}
			} else {
				result=0;
			}
		}
		else
		{
			if(privateData->RxOverrun) {
				DWORD timeOut;
				DWORD temp;
FINISH_OVERRUN_PROCESSING:
				temp=0;
				{
					timeOut=2000;
					while((timeOut>0)&&(!(Lan_GetRegDW(INT_STS)&(INT_STS_RXSTOP_INT_)))) {
						udelay(1);
						timeOut--;
					}
					if(timeOut==0) {
//						privateData->RxStopTimeOutCount++;
//						PULSE_GPIO(GP_TX,1);
						SMSC_WARNING("Timed out waiting for Rx to Stop\n");
					}
					Lan_SetRegDW(INT_STS,INT_STS_RXSTOP_INT_);
				}

				if(privateData->dwRxDmaCh<TRANSFER_REQUEST_DMA) {
					//make sure DMA has stopped before doing RX Dump
					DWORD dwTimeOut=100000;

					while ((privateData->RxSkbsCount != 0) && (dwTimeOut)) {
						udelay(10);
						timeOut--;
					}
					if (dwTimeOut == 0)
						SMSC_WARNING("Timed out waiting for Rx DMA complete");
				}

				temp=Lan_GetRegDW(RX_CFG);
				Lan_SetRegDW(RX_CFG,(temp&0x3FFFFFFFUL));
				timeOut=10000000;
				Lan_SetBitsDW(RX_CFG,RX_CFG_RX_DUMP_);
				while((timeOut>0)&&(Lan_GetRegDW(RX_CFG)&(RX_CFG_RX_DUMP_))) {
					udelay(1);
					timeOut--;
				}
				if(timeOut==0) {
					SMSC_WARNING("Timed out waiting for Rx Dump to complete\n");
				}
				Lan_SetRegDW(RX_CFG,temp);

				privateData->RxDumpCount++;
				Lan_SetRegDW(INT_STS,INT_STS_RDFL_);
				Rx_ReceiverOn(privateData, 0);
				privateData->RxOverrun=FALSE;
			}
			result=0;
			privateData->LastReasonForReleasingCPU=1;//Status FIFO Empty
		}
	} else {
		//disable and reenable the INT_EN
		//  This will allow the deassertion interval to begin
		DWORD temp=Lan_GetRegDW(INT_EN);
		Lan_SetRegDW(INT_EN,0);
		Lan_SetRegDW(INT_EN,temp);
		result=0;
		privateData->LastReasonForReleasingCPU=2;//High Congestion
	}
	return result;
}

void Rx_CountErrors(PPRIVATE_DATA privateData,DWORD dwRxStatus)
{
	BOOLEAN crcError=FALSE;
	if(dwRxStatus&0x00008000UL) {
		privateData->stats.rx_errors++;
		if(dwRxStatus&0x00000002UL) {
			privateData->stats.rx_crc_errors++;
			crcError=TRUE;
		}
	}
	if(!crcError) {
		if((dwRxStatus&0x00001020UL)==0x00001020UL) {
			//Frame type indicates length, and length error is set
			privateData->stats.rx_length_errors++;
		}
		if(dwRxStatus&RX_STS_MCAST_) {
			privateData->stats.multicast++;
		}
	}
}

void Rx_FastForward(PPRIVATE_DATA privateData,DWORD dwDwordCount)
{
	privateData->RxFastForwardCount++;
	if((dwDwordCount>=4)
		&& (
			(((privateData->dwIdRev&0x0000FFFFUL)==0x00000000UL)
				&& (privateData->dwFpgaRev>=0x36))
			||
			((privateData->dwIdRev&0x0000FFFFUL)!=0UL)
			)
		)
	{
		DWORD dwTimeOut=500;
		Lan_SetRegDW(RX_DP_CTRL,(dwDwordCount|RX_DP_CTRL_FFWD_BUSY_));
		while((dwTimeOut)&&(Lan_GetRegDW(RX_DP_CTRL)&
				RX_DP_CTRL_FFWD_BUSY_))
		{
			udelay(1);
			dwTimeOut--;
		}
		if(dwTimeOut==0) {

			SMSC_WARNING("timed out waiting for RX FFWD to finish, RX_DP_CTRL=0x%08lX",
				Lan_GetRegDW(RX_DP_CTRL));
		}
	} else {
		while(dwDwordCount) {
			DWORD dwTemp=Lan_GetRegDW(RX_DATA_FIFO);
			dwTemp=dwTemp;
			dwDwordCount--;
		}
	}
}

//Rx_ReceiverOff, and Rx_ReceiverOn use a reference counter
//  because they are used in both the Rx code and the link management count
void Rx_ReceiverOff(PPRIVATE_DATA privateData)
{
	DWORD dwIntFlags=0;
	VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
	if(privateData->dwRxOffCount==0) {
		DWORD dwMacCr=Mac_GetRegDW(privateData,MAC_CR,keyCode);
		if(!(dwMacCr&MAC_CR_RXEN_)) {
			SMSC_WARNING("Rx_ReceiverOff: Receiver is already Off");
		}
		dwMacCr&=(~MAC_CR_RXEN_);
		Mac_SetRegDW(privateData,MAC_CR,dwMacCr,keyCode);
		//CLEAR_GPIO(GP_RX);
	}
	privateData->dwRxOffCount++;
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
}

//Rx_ReceiverOff, and Rx_ReceiverOn use a reference counter
//  because they are used in both the Rx code and the link management count
void Rx_ReceiverOn(PPRIVATE_DATA privateData, VL_KEY callerKeyCode)
{
	DWORD dwIntFlags=0;
	VL_KEY keyCode=0;

	if (callerKeyCode == 0) {
		keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
	}
	else {
		SMSC_ASSERT(Vl_CheckLock(&(privateData->MacPhyLock),callerKeyCode));
		keyCode = callerKeyCode;
	}
	if(privateData->dwRxOffCount>0) {
		privateData->dwRxOffCount--;
		if(privateData->dwRxOffCount==0) {
			DWORD dwMacCr=Mac_GetRegDW(privateData,MAC_CR,keyCode);
			if(dwMacCr&MAC_CR_RXEN_) {
				SMSC_WARNING("Rx_ReceiverOn: Receiver is already on");
			}
			dwMacCr|=MAC_CR_RXEN_;
			Mac_SetRegDW(privateData,MAC_CR,dwMacCr,keyCode);
			//SET_GPIO(GP_RX);
		}
	} else {
		SMSC_ASSERT(FALSE);
	}
	if (callerKeyCode == 0) {
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}
}

unsigned int RxPacketDepth[MAX_RX_SKBS+1];

/* This function is called from the interrupt handler or from a tasklet triggered by the
 * interrupt handler when the status register has INT_STS_RSFL_ set, or RxOverrun is set.
 */
void Rx_ProcessPackets(PPRIVATE_DATA privateData)
{
	DWORD dwRxStatus=0;
	PPLATFORM_DATA platformData=NULL;
//	SET_GPIO(GP_RX);
	privateData->RxCongested=FALSE;
	platformData=&(privateData->PlatformData);
	if(privateData->dwRxDmaCh>=TRANSFER_PIO) {
		//Use PIO only
		Lan_SetRegDW(RX_CFG,0x00000200UL);
		while((dwRxStatus=Rx_PopRxStatus(privateData))!=0)
		{
			DWORD dwPacketLength=((dwRxStatus&0x3FFF0000UL)>>16);
			Rx_CountErrors(privateData,dwRxStatus);
			if((dwRxStatus&RX_STS_ES_)==0) {
				struct sk_buff *skb=NULL;
				skb=dev_alloc_skb(dwPacketLength+2);
				if(skb!=NULL) {
					skb->data=skb->head;
					skb->tail=skb->head;
					skb_reserve(skb,2); // align IP on 16B boundary
					skb_put(skb,dwPacketLength-4UL);

					//update counters
					privateData->stats.rx_packets++;
					privateData->stats.rx_bytes+=(dwPacketLength-4);

					privateData->RxPacketReadCount++;
					privateData->RxPioReadCount++;
					privateData->RxDataDWReadCount+=
						(dwPacketLength+2+3)>>2;

					Platform_ReadFifo(
						privateData->dwLanBase,
						((DWORD *)(skb->head)),
						(dwPacketLength+2+3)>>2);

					Rx_HandOffSkb(privateData,skb);
					continue;
				} else {
					SMSC_WARNING("Unable to allocate sk_buff for RX Packet, in PIO path");
					privateData->stats.rx_dropped++;
				}
			}
			//if we get here then the packet is to be read
			//  out of the fifo and discarded
			dwPacketLength+=(2+3);
			dwPacketLength>>=2;
			Rx_FastForward(privateData,dwPacketLength);
		}
	} else {
		//Use DMA and PIO
		DWORD dwDmaCh=privateData->dwRxDmaCh;
		//struct sk_buff *dmaSkb=NULL;//use privateData->RxDmaSkb
		DMA_XFER dmaXfer;
		DWORD packets;

		BUG_ON(privateData->RxSkbsCount != 0);

		//set end alignment and offset
		switch(PLATFORM_CACHE_LINE_BYTES)
		{
		case 16:Lan_SetRegDW(RX_CFG,0x40000200UL);break;
		case 32:Lan_SetRegDW(RX_CFG,0x80001200UL);break;
		default:SMSC_ASSERT(FALSE);
		}

		privateData->RxDropOnCallback = 0;

		for (packets=0;
		     (packets < MAX_RX_SKBS) &&
			     ((dwRxStatus=Rx_PopRxStatus(privateData))!=0);
		     packets++) {
			DWORD dwDwordCount;
			DWORD dwPacketLength;
			struct sk_buff *skb;
			struct scatterlist* sg;

			dwPacketLength=((dwRxStatus&0x3FFF0000UL)>>16);
			dwDwordCount =
				(dwPacketLength+
				  (PLATFORM_CACHE_LINE_BYTES-14)+
				  PLATFORM_CACHE_LINE_BYTES-1)&
				(~(PLATFORM_CACHE_LINE_BYTES-1));

			Rx_CountErrors(privateData,dwRxStatus);
			if((dwRxStatus&RX_STS_ES_)!=0) {
				privateData->stats.rx_dropped++;
				privateData->RxDropOnCallback = dwPacketLength;
				break;
			}

			skb=alloc_skb(dwPacketLength+2*PLATFORM_CACHE_LINE_BYTES, GFP_ATOMIC);
			if (skb == NULL) {
				privateData->stats.rx_dropped++;
				privateData->RxDropOnCallback = dwPacketLength;
				break;
			}

			privateData->RxSkbs[packets] = skb;
			skb_reserve(skb,PLATFORM_CACHE_LINE_BYTES-14);
			skb_put(skb,dwPacketLength-4UL);

			privateData->stats.rx_packets++;
			privateData->stats.rx_bytes+=dwPacketLength;

			sg=&privateData->RxSgs[packets];
			sg->page = virt_to_page(skb->head);
			sg->offset = (long)skb->head & ~PAGE_MASK;
			sg->length = dwDwordCount;

			privateData->RxDataDWReadCount+=dwDwordCount;
			privateData->RxPacketReadCount++;
			privateData->RxDmaReadCount++;
		}

		if (packets != 0) {
			dmaXfer.dwLanReg=privateData->dwLanBasePhy+RX_DATA_FIFO;
			dmaXfer.pdwBuf=(DWORD*)privateData->RxSgs;
			dmaXfer.dwDmaCh=dwDmaCh;
			dmaXfer.dwDwCnt=packets;
			dmaXfer.fMemWr=TRUE;

			Lan_DisableInterrupt(privateData,privateData->RxInterrupts);

			RxPacketDepth[packets]++;
			privateData->RxSkbsCount = packets;
			if(!Platform_DmaStartSgXfer(platformData, &dmaXfer, Rx_DmaCompletionCallback, privateData)) {
				SMSC_WARNING("Failed Platform_DmaStartXfer");
			}
		} else if (privateData->RxDropOnCallback != 0) {
			DWORD dwPacketLength;
			Lan_SetRegDW(RX_CFG,0x00000200UL);
			dwPacketLength = privateData->RxDropOnCallback+2+3;
			dwPacketLength>>=2;
			Rx_FastForward(privateData,dwPacketLength);
		}
	}
	Lan_SetRegDW(INT_STS,INT_STS_RSFL_);
//	CLEAR_GPIO(GP_RX);
}

void Rx_ProcessPacketsTasklet(unsigned long data)
{
    PPRIVATE_DATA privateData=(PPRIVATE_DATA)Rx_TaskletParameter;
	data=data;//make lint happy
	if(privateData==NULL) {
		SMSC_WARNING("Rx_ProcessPacketsTasklet(privateData==NULL)");
		return;
	}
	Rx_ProcessPackets(privateData);
	Lan_EnableIRQ(privateData);
}

static void Rx_DmaCompletionCallback(void* param)
{
	PPRIVATE_DATA privateData = param;
	int i;

	BUG_ON(privateData->RxSkbsCount == 0);
	for (i=0; i<privateData->RxSkbsCount; i++) {
		Rx_HandOffSkb(privateData,privateData->RxSkbs[i]);
	}

	if (privateData->RxDropOnCallback != 0) {
		DWORD dwPacketLength;
		Lan_SetRegDW(RX_CFG,0x00000200UL);
		dwPacketLength = privateData->RxDropOnCallback+2+3;
		dwPacketLength>>=2;
		Rx_FastForward(privateData,dwPacketLength);
	}

	privateData->RxSkbsCount = 0;
	Lan_EnableInterrupt(privateData,privateData->RxInterrupts);
}

BOOLEAN Rx_HandleInterrupt(
	PPRIVATE_DATA privateData,
	DWORD dwIntSts)
{
	BOOLEAN result=FALSE;
	SMSC_ASSERT(privateData!=NULL);

	privateData->LastReasonForReleasingCPU=0;

	if(dwIntSts&INT_STS_RXE_) {
		/*SMSC_TRACE("Rx_HandleInterrupt: RXE signalled");*/
		privateData->stats.rx_errors++;
		Lan_SetRegDW(INT_STS,INT_STS_RXE_);
		result=TRUE;
	}

	if(dwIntSts&INT_STS_RXDFH_INT_) {
		privateData->stats.rx_dropped+=Lan_GetRegDW(RX_DROP);
		Lan_SetRegDW(INT_STS,INT_STS_RXDFH_INT_);
		result=TRUE;
	}

	if(privateData->dwGeneration==0) {
		if(dwIntSts&(INT_STS_RDFL_)) {
			Lan_SetRegDW(INT_STS,INT_STS_RDFL_);
			Rx_HandleOverrun(privateData);
			result=TRUE;
		}
	} else {
		if(dwIntSts&(INT_STS_RDFO_)) {
			Lan_SetRegDW(INT_STS,INT_STS_RDFO_);
			Rx_HandleOverrun(privateData);
			result=TRUE;
		}
	}

	if((!(dwIntSts&INT_STS_RSFL_))&&(privateData->RxOverrun==FALSE)) {
		return result;
	}
	if (privateData->RxSkbsCount != 0) {
		/* We are still DMAing the previous packet from the RX
		 * FIFO, and waiting for the DMA completion callback.
		 * We got here because another interrupt was active,
		 * even though Rx interrupts are disabled.
		 */
		return result;
	}

	result=TRUE;

	if(privateData->MeasuringRxThroughput==FALSE) {
		privateData->MeasuringRxThroughput=TRUE;
		Gpt_ScheduleCallBack(privateData,GptCB_MeasureRxThroughput,1000);
		privateData->RxFlowCurrentThroughput=0;
		privateData->RxFlowCurrentPacketCount=0;
		privateData->RxFlowCurrentWorkLoad=0;
	}
	if(tasklets) {
		Lan_DisableIRQ(privateData);
		Rx_TaskletParameter=(unsigned long)privateData;
		tasklet_schedule(&Rx_Tasklet);
	} else {
		Rx_ProcessPackets(privateData);
	}
	return result;
}

BOOLEAN RxStop_HandleInterrupt(
	PPRIVATE_DATA privateData,
	DWORD dwIntSts)
{
	BOOLEAN result=FALSE;
	SMSC_ASSERT(privateData!=NULL);

	if(dwIntSts&INT_STS_RXSTOP_INT_) {
		result=TRUE;
		Gpt_CancelCallBack (privateData, GptCB_RxCompleteMulticast);
		Rx_CompleteMulticastUpdate (privateData);
		Lan_SetRegDW(INT_STS,INT_STS_RXSTOP_INT_);
		Lan_DisableInterrupt(privateData,INT_EN_RXSTOP_INT_EN_);
	}
	return result;
}

//returns hash bit number for given MAC address
//example:
//   01 00 5E 00 00 01 -> returns bit number 31
static DWORD Rx_Hash(BYTE addr[6])
{
	int i;
	DWORD crc=0xFFFFFFFFUL;
	DWORD poly=0xEDB88320UL;
	DWORD result=0;
	for(i=0;i<6;i++)
	{
		int bit;
		DWORD data=((DWORD)addr[i]);
		for(bit=0;bit<8;bit++)
		{
			DWORD p = (crc^((DWORD)data))&1UL;
			crc >>= 1;
			if(p!=0) crc ^= poly;
			data >>=1;
		}
	}
	result=((crc&0x01UL)<<5)|
		((crc&0x02UL)<<3)|
		((crc&0x04UL)<<1)|
		((crc&0x08UL)>>1)|
		((crc&0x10UL)>>3)|
		((crc&0x20UL)>>5);
	return result;
}

void Rx_SetMulticastList(
	struct net_device *dev)
{
	PPRIVATE_DATA privateData=NULL;
	VL_KEY keyCode=0;
	DWORD dwIntFlags=0;
	SMSC_ASSERT(dev!=NULL);

	privateData=((PPRIVATE_DATA)(dev->priv));
    	SMSC_ASSERT(privateData!=NULL);
	keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);

	if(dev->flags & IFF_PROMISC) {
//		SMSC_TRACE("Promiscuous Mode Enabled");
		privateData->set_bits_mask = MAC_CR_PRMS_;
		privateData->clear_bits_mask = (MAC_CR_MCPAS_ | MAC_CR_HPFILT_);

		privateData->HashHi = 0UL;
		privateData->HashLo = 0UL;
		goto PREPARE;
	}

	if(dev->flags & IFF_ALLMULTI) {
//		SMSC_TRACE("Receive all Multicast Enabled");
		privateData->set_bits_mask = MAC_CR_MCPAS_;
		privateData->clear_bits_mask = (MAC_CR_PRMS_ | MAC_CR_HPFILT_);

		privateData->HashHi = 0UL;
		privateData->HashLo = 0UL;
		goto PREPARE;
	}


	if(dev->mc_count>0) {
		DWORD dwHashH=0;
		DWORD dwHashL=0;
		DWORD dwCount=0;
		struct dev_mc_list *mc_list=dev->mc_list;

		privateData->set_bits_mask = MAC_CR_HPFILT_;
		privateData->clear_bits_mask = (MAC_CR_PRMS_ | MAC_CR_MCPAS_);

		while(mc_list!=NULL) {
			dwCount++;
			if((mc_list->dmi_addrlen)==6) {
				DWORD dwMask=0x01UL;
				DWORD dwBitNum=Rx_Hash(mc_list->dmi_addr);
			//	SMSC_TRACE("Multicast: enable dwBitNum=%ld,addr=%02X %02X %02X %02X %02X %02X",
			//		dwBitNum,
			//		((BYTE *)(mc_list->dmi_addr))[0],
			//		((BYTE *)(mc_list->dmi_addr))[1],
			//		((BYTE *)(mc_list->dmi_addr))[2],
			//		((BYTE *)(mc_list->dmi_addr))[3],
			//		((BYTE *)(mc_list->dmi_addr))[4],
			//		((BYTE *)(mc_list->dmi_addr))[5]);
				dwMask<<=(dwBitNum&0x1FUL);
				if(dwBitNum&0x20UL) {
					dwHashH|=dwMask;
				} else {
					dwHashL|=dwMask;
				}
			} else {
				SMSC_WARNING("dmi_addrlen!=6");
			}
			mc_list=mc_list->next;
		}
		if(dwCount!=((DWORD)(dev->mc_count))) {
			SMSC_WARNING("dwCount!=dev->mc_count");
		}
		// SMSC_TRACE("Multicast: HASHH=0x%08lX,HASHL=0x%08lX",dwHashH,dwHashL);
		privateData->HashHi = dwHashH;
		privateData->HashLo = dwHashL;
	}
	else
	{
		privateData->set_bits_mask = 0L;
		privateData->clear_bits_mask = (MAC_CR_PRMS_ | MAC_CR_MCPAS_ | MAC_CR_HPFILT_);

		// SMSC_TRACE("Receive own packets only.");
		privateData->HashHi = 0UL;
		privateData->HashLo = 0UL;
	}

PREPARE:
	if(privateData->dwGeneration<=1) {
		if (privateData->MulticastUpdatePending == FALSE) {
			privateData->MulticastUpdatePending = TRUE;
			// prepare to signal software interrupt
			Lan_SignalSoftwareInterrupt(privateData);
		}
		else {
			// Rx_CompleteMulticastUpdate has not yet been called
			// therefore these latest settings will be used instead
		}
	} else {
		DWORD local_MACCR;
		Mac_SetRegDW(privateData,HASHH,privateData->HashHi,keyCode);
		Mac_SetRegDW(privateData,HASHL,privateData->HashLo,keyCode);
		local_MACCR = Mac_GetRegDW(privateData,MAC_CR,keyCode);
		local_MACCR |= privateData->set_bits_mask;
		local_MACCR &= ~(privateData->clear_bits_mask);
		Mac_SetRegDW(privateData,MAC_CR,local_MACCR,keyCode);
	}
	Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	return;
}

void Eeprom_EnableAccess(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	if(debug_mode&0x04UL) {
		Lan_SetRegDW(GPIO_CFG,(g_GpioSetting&0xFF0FFFFFUL));
	} else {
		Lan_ClrBitsDW(GPIO_CFG,0x00F00000UL);
	}
	udelay(100);
}

void Eeprom_DisableAccess(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	if(debug_mode&0x04UL) {
		Lan_SetRegDW(GPIO_CFG,g_GpioSetting);
	}
}

BOOLEAN Eeprom_IsMacAddressLoaded(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	return (Lan_GetRegDW(E2P_CMD)&
		E2P_CMD_MAC_ADDR_LOADED_)?TRUE:FALSE;
}

BOOLEAN Eeprom_IsBusy(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	return (Lan_GetRegDW(E2P_CMD)&
		E2P_CMD_EPC_BUSY_)?TRUE:FALSE;
}

BOOLEAN Eeprom_Timeout(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	return (Lan_GetRegDW(E2P_CMD)&
		E2P_CMD_EPC_TIMEOUT_)?TRUE:FALSE;
}

BOOLEAN Eeprom_ReadLocation(
	PPRIVATE_DATA privateData,
	BYTE address, BYTE * data)
{
	DWORD timeout=100000;
	DWORD temp=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(data!=NULL);
	if((temp=Lan_GetRegDW(E2P_CMD))&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_ReadLocation: Busy at start, E2P_CMD=0x%08lX",temp);
		return FALSE;
	}
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_READ_|((DWORD)address)));
	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	(*data)=(BYTE)(Lan_GetRegDW(E2P_DATA));
	return TRUE;
}

BOOLEAN Eeprom_EnableEraseAndWrite(
	PPRIVATE_DATA privateData)
{
	DWORD timeout=100000;
	SMSC_ASSERT(privateData!=NULL);
	if(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_EnableEraseAndWrite: Busy at start");
		return FALSE;
	}
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_EWEN_));

	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Eeprom_DisableEraseAndWrite(
	PPRIVATE_DATA privateData)
{
	DWORD timeout=100000;
	SMSC_ASSERT(privateData!=NULL);
	if(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_DisableEraseAndWrite: Busy at start");
		return FALSE;
	}
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_EWDS_));

	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Eeprom_WriteLocation(
	PPRIVATE_DATA privateData,BYTE address,BYTE data)
{
	DWORD timeout=100000;
	SMSC_ASSERT(privateData!=NULL);
	if(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_WriteLocation: Busy at start");
		return FALSE;
	}
	Lan_SetRegDW(E2P_DATA,((DWORD)data));
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_WRITE_|((DWORD)address)));

	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Eeprom_EraseAll(
	PPRIVATE_DATA privateData)
{
	DWORD timeout=100000;
	SMSC_ASSERT(privateData!=NULL);
	if(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_EraseAll: Busy at start");
		return FALSE;
	}
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_ERAL_));

	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Eeprom_Reload(
	PPRIVATE_DATA privateData)
{
	DWORD timeout=100000;
	SMSC_ASSERT(privateData!=NULL);
	if(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_) {
		SMSC_WARNING("Eeprom_Reload: Busy at start");
		return FALSE;
	}
	Lan_SetRegDW(E2P_CMD,
		(E2P_CMD_EPC_BUSY_|E2P_CMD_EPC_CMD_RELOAD_));

	while((timeout>0)&&
		(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_))
	{
		udelay(10);
		timeout--;
	}
	if(timeout==0) {
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Eeprom_SaveMacAddress(
	PPRIVATE_DATA privateData,
	DWORD dwHi16,DWORD dwLo32)
{
	BOOLEAN result=FALSE;
	SMSC_ASSERT(privateData!=NULL);
	Eeprom_EnableAccess(privateData);
	if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
	if(!Eeprom_EraseAll(privateData)) goto DONE;
	if(privateData->dwGeneration==0) {
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,0,0xA5)) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,1,LOBYTE(LOWORD(dwLo32)))) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,2,HIBYTE(LOWORD(dwLo32)))) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,3,LOBYTE(HIWORD(dwLo32)))) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,4,HIBYTE(HIWORD(dwLo32)))) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,5,LOBYTE(LOWORD(dwHi16)))) goto DONE;
		if(!Eeprom_EnableEraseAndWrite(privateData)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,6,HIBYTE(LOWORD(dwHi16)))) goto DONE;
	} else {
		if(!Eeprom_WriteLocation(privateData,0,0xA5)) goto DONE;
		if(!Eeprom_WriteLocation(privateData,1,LOBYTE(LOWORD(dwLo32)))) goto DONE;
		if(!Eeprom_WriteLocation(privateData,2,HIBYTE(LOWORD(dwLo32)))) goto DONE;
		if(!Eeprom_WriteLocation(privateData,3,LOBYTE(HIWORD(dwLo32)))) goto DONE;
		if(!Eeprom_WriteLocation(privateData,4,HIBYTE(HIWORD(dwLo32)))) goto DONE;
		if(!Eeprom_WriteLocation(privateData,5,LOBYTE(LOWORD(dwHi16)))) goto DONE;
		if(!Eeprom_WriteLocation(privateData,6,HIBYTE(LOWORD(dwHi16)))) goto DONE;
	}
	if(!Eeprom_DisableEraseAndWrite(privateData)) goto DONE;

	if(!Eeprom_Reload(privateData)) goto DONE;
	if(!Eeprom_IsMacAddressLoaded(privateData)) goto DONE;
	{
		DWORD dwIntFlags=0;
		VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
		if(dwHi16!=Mac_GetRegDW(privateData,ADDRH,keyCode)) goto DONE;
		if(dwLo32!=Mac_GetRegDW(privateData,ADDRL,keyCode)) goto DONE;
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}
	result=TRUE;
DONE:
	Eeprom_DisableAccess(privateData);
	return result;
}

volatile DWORD g_GpioSetting=0x00000000UL;
#ifdef USE_LED1_WORK_AROUND
volatile DWORD g_GpioSettingOriginal=0x00000000UL;
#endif

BOOLEAN Lan_Initialize(
	PPRIVATE_DATA privateData,
	DWORD dwIntCfg,
	DWORD dwTxFifSz,
	DWORD dwAfcCfg)
{
	BOOLEAN result=FALSE;
	DWORD dwTimeOut=0;
	DWORD dwTemp=0;
	DWORD dwResetCount=3;

    SMSC_TRACE("-->Lan_Initialize(dwIntCfg=0x%08lX)",dwIntCfg);
	SMSC_ASSERT(privateData!=NULL);

	//Reset the LAN9118
	if(privateData->dwGeneration>0) {
		dwResetCount=1;
	}
	while(dwResetCount>0) {
		Lan_SetRegDW(HW_CFG,HW_CFG_SRST_);
		dwTimeOut=1000000;
		do {
			udelay(10);
			dwTemp=Lan_GetRegDW(HW_CFG);
			dwTimeOut--;
		} while((dwTimeOut>0)&&(dwTemp&HW_CFG_SRST_));
		if(dwTemp&HW_CFG_SRST_) {
			SMSC_WARNING("  Failed to complete reset.");
			goto DONE;
		}
		dwResetCount--;
	}

	SMSC_ASSERT(dwTxFifSz>=0x00020000UL);
	SMSC_ASSERT(dwTxFifSz<=0x000E0000UL);
	SMSC_ASSERT((dwTxFifSz&(~HW_CFG_TX_FIF_SZ_))==0);
	Lan_SetRegDW(HW_CFG,dwTxFifSz);
	privateData->RxMaxDataFifoSize=0;
	switch(dwTxFifSz>>16) {
	case 2:privateData->RxMaxDataFifoSize=13440;break;
	case 3:privateData->RxMaxDataFifoSize=12480;break;
	case 4:privateData->RxMaxDataFifoSize=11520;break;
	case 5:privateData->RxMaxDataFifoSize=10560;break;
	case 6:privateData->RxMaxDataFifoSize=9600;break;
	case 7:privateData->RxMaxDataFifoSize=8640;break;
	case 8:privateData->RxMaxDataFifoSize=7680;break;
	case 9:privateData->RxMaxDataFifoSize=6720;break;
	case 10:privateData->RxMaxDataFifoSize=5760;break;
	case 11:privateData->RxMaxDataFifoSize=4800;break;
	case 12:privateData->RxMaxDataFifoSize=3840;break;
	case 13:privateData->RxMaxDataFifoSize=2880;break;
	case 14:privateData->RxMaxDataFifoSize=1920;break;
	default:SMSC_ASSERT(FALSE);break;
	}

	if(dwAfcCfg==0xFFFFFFFF) {
		switch(dwTxFifSz) {

		//AFC_HI is about ((Rx Data Fifo Size)*2/3)/64
		//AFC_LO is AFC_HI/2
		//BACK_DUR is about 5uS*(AFC_LO) rounded down
		case 0x00020000UL://13440 Rx Data Fifo Size
			dwAfcCfg=0x008C46AF;break;
		case 0x00030000UL://12480 Rx Data Fifo Size
			dwAfcCfg=0x0082419F;break;
		case 0x00040000UL://11520 Rx Data Fifo Size

			dwAfcCfg=0x00783C9F;break;
		case 0x00050000UL://10560 Rx Data Fifo Size
//			dwAfcCfg=0x006E378F;break;
			dwAfcCfg=0x006E374F;break;
		case 0x00060000UL:// 9600 Rx Data Fifo Size
			dwAfcCfg=0x0064328F;break;
		case 0x00070000UL:// 8640 Rx Data Fifo Size
			dwAfcCfg=0x005A2D7F;break;
		case 0x00080000UL:// 7680 Rx Data Fifo Size
			dwAfcCfg=0x0050287F;break;
		case 0x00090000UL:// 6720 Rx Data Fifo Size
			dwAfcCfg=0x0046236F;break;
		case 0x000A0000UL:// 5760 Rx Data Fifo Size
			dwAfcCfg=0x003C1E6F;break;
		case 0x000B0000UL:// 4800 Rx Data Fifo Size
			dwAfcCfg=0x0032195F;break;

		//AFC_HI is ~1520 bytes less than RX Data Fifo Size
		//AFC_LO is AFC_HI/2
		//BACK_DUR is about 5uS*(AFC_LO) rounded down
		case 0x000C0000UL:// 3840 Rx Data Fifo Size
			dwAfcCfg=0x0024124F;break;
		case 0x000D0000UL:// 2880 Rx Data Fifo Size
			dwAfcCfg=0x0015073F;break;
		case 0x000E0000UL:// 1920 Rx Data Fifo Size
			dwAfcCfg=0x0006032F;break;
		default:SMSC_ASSERT(FALSE);break;
		}
	}
	Lan_SetRegDW(AFC_CFG,(dwAfcCfg&0xFFFFFFF0UL));

	//make sure EEPROM has finished loading before setting GPIO_CFG
	dwTimeOut=1000;
	while((dwTimeOut>0)&&(Lan_GetRegDW(E2P_CMD)&E2P_CMD_EPC_BUSY_)) {
		udelay(5);
		dwTimeOut--;
	}
	if(dwTimeOut==0) {
		SMSC_WARNING("Lan_Initialize: Timed out waiting for EEPROM busy bit to clear\n");
	}

	if(debug_mode&0x04UL) {
		if(OLD_REGISTERS(privateData))
		{
			g_GpioSetting=0x00270700UL;
		} else {
			g_GpioSetting=0x00670700UL;
		}
	} else {
		g_GpioSetting = 0x70070000UL;
	}
	Lan_SetRegDW(GPIO_CFG,g_GpioSetting);

	//initialize interrupts
	Lan_SetRegDW(INT_EN,0);
	Lan_SetRegDW(INT_STS,0xFFFFFFFFUL);
	dwIntCfg|=INT_CFG_IRQ_EN_;
	Lan_SetRegDW(INT_CFG,dwIntCfg);

	Vl_InitLock(&(privateData->MacPhyLock));
	spin_lock_init(&(privateData->IntEnableLock));
	privateData->LanInitialized=TRUE;

	result=TRUE;

DONE:
	SMSC_TRACE("<--Lan_Initialize");
	return result;
}

void Lan_EnableInterrupt(PPRIVATE_DATA privateData,DWORD dwIntEnMask)
{
	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	Lan_SetBitsDW(INT_EN,dwIntEnMask);
	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_DisableInterrupt(PPRIVATE_DATA privateData,DWORD dwIntEnMask)
{
	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	Lan_ClrBitsDW(INT_EN,dwIntEnMask);
	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

//Spin locks for the following functions have been commented out
//  because at this time they are not necessary.
//These function are
//  Lan_SetTDFL
//  Lan_SetTSFL
//  Lan_SetRDFL
//  Lan_SetRSFL
//Both the Rx and Tx side of the driver use the FIFO_INT,
//  but the Rx side only touches is during initialization,
//  so it is sufficient that Tx side simple preserve the Rx setting

void Lan_SetTDFL(PPRIVATE_DATA privateData,BYTE level) {
//	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
//	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		DWORD temp=Lan_GetRegDW(FIFO_INT);
		temp&=0x00FFFFFFUL;
		temp|=((DWORD)level)<<24;
		Lan_SetRegDW(FIFO_INT,temp);
	}
//	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_SetTSFL(PPRIVATE_DATA privateData,BYTE level) {
//	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
//	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		DWORD temp=Lan_GetRegDW(FIFO_INT);
		temp&=0xFF00FFFFUL;
		temp|=((DWORD)level)<<16;
		Lan_SetRegDW(FIFO_INT,temp);
	}
//	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_SetRDFL(PPRIVATE_DATA privateData,BYTE level) {
//	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
//	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		DWORD temp=Lan_GetRegDW(FIFO_INT);
		temp&=0xFFFF00FFUL;
		temp|=((DWORD)level)<<8;
		Lan_SetRegDW(FIFO_INT,temp);
	}
//	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_SetRSFL(PPRIVATE_DATA privateData,BYTE level) {
//	DWORD dwIntFlags=0;
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->LanInitialized==TRUE);
//	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		DWORD temp=Lan_GetRegDW(FIFO_INT);
		temp&=0xFFFFFF00UL;
		temp|=((DWORD)level);
		Lan_SetRegDW(FIFO_INT,temp);
	}
//	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_EnableIRQ(PPRIVATE_DATA privateData)
{
	DWORD dwIntFlags=0;
	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		Lan_SetBitsDW(INT_CFG,INT_CFG_IRQ_EN_);
	}
	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_DisableIRQ(PPRIVATE_DATA privateData)
{
	DWORD dwIntFlags=0;
	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		Lan_ClrBitsDW(INT_CFG,INT_CFG_IRQ_EN_);
	}
	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_SetIntDeas(PPRIVATE_DATA privateData, DWORD dwIntDeas)
{
	DWORD dwIntFlags=0;
	spin_lock_irqsave(&(privateData->IntEnableLock),dwIntFlags);
	{
		Lan_ClrBitsDW(INT_CFG,INT_CFG_INT_DEAS_);
		Lan_SetBitsDW(INT_CFG,(dwIntDeas<<24));
	}
	spin_unlock_irqrestore(&(privateData->IntEnableLock),dwIntFlags);
}

void Lan_SignalSoftwareInterrupt(PPRIVATE_DATA privateData)
{
	SMSC_ASSERT(privateData!=NULL);
	SMSC_ASSERT(privateData->dwLanBase!=0);
	privateData->SoftwareInterruptSignal=FALSE;
	Lan_EnableInterrupt(privateData,INT_EN_SW_INT_EN_);
}

BOOLEAN Lan_HandleSoftwareInterrupt(
	PPRIVATE_DATA privateData,
	DWORD dwIntSts)
{
	if(dwIntSts&INT_STS_SW_INT_) {
		SMSC_ASSERT(privateData!=NULL);
		Lan_DisableInterrupt(privateData,INT_EN_SW_INT_EN_);
		Lan_SetRegDW(INT_STS,INT_STS_SW_INT_);
		privateData->SoftwareInterruptSignal=TRUE;
		if (privateData->MulticastUpdatePending) {
			Rx_BeginMulticastUpdate (privateData);
		}
		return TRUE;
	}
	return FALSE;
}

typedef struct _SHOW_REG
{
	char  szName[20];
	DWORD dwOffset;
} SHOW_REG;
/*
FUNCTION: Lan_ShowRegs
    This function is used to display the registers.
	Except the phy.
*/
void Lan_ShowRegs(PPRIVATE_DATA privateData)
{
	//	Make these const struct's static to keep them off the stack.
	//	Otherwise, gcc will try to use _memcpy() to initialize them,
	//	which will *NOT* work in our RunTime environment.
	static const SHOW_REG sysCsr[] = {
		{ "ID_REV",			0x50UL		},
		{ "INT_CFG",		0x54UL		},
		{ "INT_STS",		0x58UL		},
		{ "INT_EN",			0x5CUL		},
		{ "DMA_CFG",		0x60UL		},
		{ "BYTE_TEST",		0x64UL		},
		{ "FIFO_INT",		0x68UL		},
		{ "RX_CFG",			0x6CUL		},
		{ "TX_CFG",			0x70UL		},
		{ "HW_CFG",			0x74UL		},
		{ "RX_DP_CTRL",		0x78UL		},
		{ "RX_FIFO_INF",	0x7CUL		},
		{ "TX_FIFO_INF",	0x80UL		},
		{ "PMT_CTRL",		0x84UL		},
		{ "GPIO_CFG",		0x88UL		},
		{ "GPT_CFG",		0x8CUL		},
		{ "GPT_CNT",		0x90UL		},
		{ "FPGA_REV",		0x94UL		},
		{ "ENDIAN",			0x98UL		},
		{ "FREE_RUN",		0x9CUL		},
		{ "RX_DROP",		0xA0UL		},
		{ "MAC_CSR_CMD",	0xA4UL		},
		{ "MAC_CSR_DATA",	0xA8UL		},
		{ "AFC_CFG",		0xACUL		},
		{ "E2P_CMD",		0xB0UL		},
		{ "E2P_DATA",		0xB4UL		},
		{ "TEST_REG_A",		0xC0UL		}};

	static const SHOW_REG macCsr[] = {
		{ "MAC_CR",		MAC_CR		},
		{ "ADDRH",		ADDRH		},
		{ "ADDRL",		ADDRL		},
		{ "HASHH",		HASHH		},
		{ "HASHL",		HASHL		},
		{ "MII_ACC",	MII_ACC		},
		{ "MII_DATA",	MII_DATA	},
		{ "FLOW",		FLOW		},
		{ "VLAN1",		VLAN1		},
		{ "VLAN2",		VLAN2		},
		{ "WUFF",		WUFF		},
		{ "WUCSR",		WUCSR		}};

	int i, iNumSysRegs, iNumMacRegs;
	DWORD dwOldMacCmdReg, dwOldMacDataReg;

	iNumSysRegs = (int)(sizeof(sysCsr) / sizeof(SHOW_REG));
	iNumMacRegs = (int)(sizeof(macCsr) / sizeof(SHOW_REG));

	// preserve MAC cmd/data reg's
	dwOldMacCmdReg = Lan_GetRegDW(MAC_CSR_CMD);
	dwOldMacDataReg = Lan_GetRegDW(MAC_CSR_DATA);

	SMSC_TRACE("");
	SMSC_TRACE("               LAN91C118 CSR's");
	SMSC_TRACE("                     SYS CSR's                     MAC CSR's");

	{
		DWORD dwIntFlags=0;
		VL_KEY keyCode=Vl_WaitForLock(&(privateData->MacPhyLock),&dwIntFlags);
		for (i=0; i<iNumMacRegs; i++)
		{
			SMSC_TRACE(
				"%16s (0x%02lX) = 0x%08lX, %8s (0x%02lX) + 0x%08lX",
				sysCsr[i].szName,
				sysCsr[i].dwOffset,
				*((volatile DWORD *)(privateData->dwLanBase+sysCsr[i].dwOffset)),
				macCsr[i].szName,
				macCsr[i].dwOffset,
				Mac_GetRegDW(privateData,macCsr[i].dwOffset,keyCode));

			// restore original mac cmd/data reg's after each usage
			Lan_SetRegDW(MAC_CSR_CMD,dwOldMacCmdReg);
			Lan_SetRegDW(MAC_CSR_DATA,dwOldMacDataReg);
		}
		Vl_ReleaseLock(&(privateData->MacPhyLock),keyCode,&dwIntFlags);
	}
	for (i=iNumMacRegs; i<iNumSysRegs; i++)
	{
		SMSC_TRACE("%16s (0x%02lX) = 0x%08lX",
			sysCsr[i].szName,
			sysCsr[i].dwOffset,
			*((volatile DWORD *)(privateData->dwLanBase+sysCsr[i].dwOffset)));
	}
}

void Vl_InitLock(PVERIFIABLE_LOCK pVl)
{
	SMSC_ASSERT(pVl!=NULL);
	spin_lock_init(&(pVl->Lock));
	pVl->KeyCode=0;
}

BOOLEAN Vl_CheckLock(PVERIFIABLE_LOCK pVl,VL_KEY keyCode)
{
	BOOLEAN result=FALSE;
	SMSC_ASSERT(pVl!=NULL);
	if(keyCode==pVl->KeyCode)
		result=TRUE;
	return result;
}

VL_KEY Vl_WaitForLock(PVERIFIABLE_LOCK pVl,DWORD *pdwIntFlags)
{
	VL_KEY result=0;
	SMSC_ASSERT(pVl!=NULL);
	spin_lock_irqsave(
		&(pVl->Lock),
		(*pdwIntFlags));
	pVl->KeyCode++;
	if(pVl->KeyCode>0x80000000UL) {
		pVl->KeyCode=1;
	}
	result=pVl->KeyCode;
	return result;
}

void Vl_ReleaseLock(PVERIFIABLE_LOCK pVl,VL_KEY keyCode,DWORD *pdwIntFlags)
{
	SMSC_ASSERT(pVl!=NULL);
	SMSC_ASSERT(pVl->KeyCode==keyCode);
	spin_unlock_irqrestore(&(pVl->Lock),(*pdwIntFlags));
}

#ifndef USING_LINT
/* Platform device registration */

static struct platform_driver Smsc9118_driver = {
	.probe = Smsc9118_probe,
	.remove = Smsc9118_remove,
	/* Driver name is intentionally identical to the one used
	 * by "driver/net/smc911x.c", so you can define common
	 * platform device and decide which driver you want to use
	 * later... The only difference is that in case of this
	 * driver you can provide additional IRQ resources,
	 * following IRQ number; name them "polarity" and "type" -
	 * they will be used as IRQ_POL and IRQ_TYPE values (both
	 * in IRQ_CFG register). If not defined default value (zero)
	 * will be used. */
	.driver = {
		.name = "smc911x",
	},
};

static int __init Smsc9118_init_module(void)
{
	return platform_driver_register(&Smsc9118_driver);
}

static void __exit Smsc9118_cleanup_module(void)
{
	platform_driver_unregister(&Smsc9118_driver);
}

module_init(Smsc9118_init_module);
module_exit(Smsc9118_cleanup_module);
#endif
