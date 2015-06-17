/***********************************************************************
 *
 * File: display/soc/stiH416/stiH416_hdmirx_reg.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <display/soc/stiH416/stiH416reg.h>
#ifndef _STIH416_REG_H
#define _STIH416_REG_H

#define     HDRX_CSM_MODULE_ADDRS_OFFSET            0x003CCUL
#define     HDRX_I2C_MASTER_MODULE_ADDRS_OFFSET     0x000F4UL
#define     HDRX_IFM_INSTRUMENT_PU_ADDRS_OFFSET     0x00500UL
#define     HDRX_PACKET_MEMORY_ADDRS_BASE_OFFSET    0x01000UL

/* STiH416 Base addresses ----------------------------------------------------*/
#define STiH416_VTAC_HDMIRX_TX_SAS_BASE         0x01E55000   //OK
#define STiH416_VTAC_HDMIRX_RX_MPE_BASE         0x00349400   //OK

#define VTAC_RX_PHY_CONFIG0                     (0x00000844) /* SAS Address:    SYSCFG_FRONTBaseAddress + 0x00000844*/
#define VTAC_RX_PHY_CONFIG1                     (0x00000848) /* SAS Address:    SYSCFG_FRONTBaseAddress + 0x00000848*/
#define VTAC_TX_PHY_CONFIG0                     (0x00000850) /* SAS Address:    SYSCFG_FRONTBaseAddress + 0x00000850*/
#define VTAC_TX_PHY_CONFIG1                     (0x00000854) /* SAS Address:    SYSCFG_FRONTBaseAddress + 0x00000854*/

#define VTAC_ENABLE_BIT                         (1L << 0)
#define VTAC_SW_RST_BIT                         (1L << 1)
#define VTAC_FIFO_MAIN_CONFIG_VAL               (0x00000004)

// VTAC TX
#define VTAC_TX_CONFIG                          (0x00000000)
#define VTAC_TX_FIFO_USAGE                      (0x00000030)
#define VTAC_TX_STA_CLR                         (0x00000040)
#define VTAC_TX_STA                             (0x00000044)
#define VTAC_TX_ITS                             (0x00000048)
#define VTAC_TX_ITS_BCLR                        (0x0000004C)
#define VTAC_TX_ITS_BSET                        (0x00000050)
#define VTAC_TX_ITM                             (0x00000054)
#define VTAC_TX_ITM_BCLR                        (0x00000058)
#define VTAC_TX_ITM_BSET                        (0x0000005C)
#define VTAC_TX_DFV0                            (0x00000060)
#define VTAC_TX_DEBUG_PAD_CTRL                  (0x00000080)

// VTAC RX
#define VTAC_RX_CONFIG                          (0x00000000)
#define VTAC_RX_FIFO_CONFIG                     (0x00000004)
#define VTAC_RX_FIRST_ODD_PARITY_ERROR          (0x00000010)
#define VTAC_RX_FIRST_EVEN_PARITY_ERROR         (0x00000014)
#define VTAC_RX_TOTAL_PARITY_ERROR              (0x00000018)
#define VTAC_RX_FIFO_USAGE                      (0x00000030)
#define VTAC_RX_STA_CLR                         (0x00000040)
#define VTAC_RX_STA                             (0x00000044)
#define VTAC_RX_ITS                             (0x00000048)
#define VTAC_RX_ITS_BCLR                        (0x0000004C)
#define VTAC_RX_ITS_BSET                        (0x00000050)
#define VTAC_RX_ITM                             (0x00000054)
#define VTAC_RX_ITM_BCLR                        (0x00000058)
#define VTAC_RX_ITM_BSET                        (0x0000005C)
#define VTAC_RX_DFV0                            (0x00000060)
#define VTAC_RX_DEBUG_PAD_CTRL                  (0x00000080)


/* VTAC RX PHY config*/
#define  CONFIG_VTAC_RX_DISABLE_ODT             (0x01 << 1)
#define  CONFIG_VTAC_RX_PDD                     (0x01 << 10)
#define  CONFIG_VTAC_RX_PDR                     (0x01 << 11)
#define  CONFIG_VTAC_RX_CK_DIFF_CAPTURE_INV     (0x01 << 13)
#define  CONFIG_VTAC0_RX_CONF                   (0x02 << 15)
#define  CONFIG_VTAC1_RX_CONF                   (0x01 << 15)

/* VTAC TX PHY config*/
#define  CONFIG_VTAC_TX_ENABLE_CLK_PHY          (0x01 << 0)
#define  CONFIG_VTAC_TX_ENABLE_CLK_DLL          (0x01 << 1)

#define  CONFIG_VTAC_TX_RST_N_DLL_SWITCH        (0x01 << 4)
#define  CONFIG_VTAC_TX_PLL_NOT_OSC_MODE        (0x01 << 3)

#endif
