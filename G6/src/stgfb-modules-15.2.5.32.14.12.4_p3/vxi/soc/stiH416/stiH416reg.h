/***********************************************************************
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STIH416VXIREG_H
#define _STIH416VXIREG_H

/* STiH416 Base addresses ----------------------------------------------------*/
#define STiH416_REGISTER_BASE          0xFD000000                               // OK

/* STiH416 Base address size -------------------------------------------------*/
#define STiH416_VXI_REG_ADDR_SIZE      0x00001000                               // OK

/* STiH416 Base address size -------------------------------------------------*/
#define STiH416_VXI_REGISTER_BASE      0x00190000                               // OK

/* STiH416 VXI register offset -----------------------------------------------*/
#define VXI_CTRL                       0x00000004
#define VXI_DATA_SWAP_1                0x00000014
#define VXI_DATA_SWAP_2                0x00000018
#define VXI_DATA_SWAP_3                0x0000001c
#define VXI_DATA_SWAP_4                0x00000020
#define VXI_DATA_TWIST_SWAP_656        0x00000028
#define VXI_RESET                      0x00000030
#define VXI_CLK_CONTROL                0x0000002c
#define VXIINST_SOFT_RESETS            0x00000060
#define VXI_IFM_CLK_CONTROL            0x00000084

/* VXI Interrupt  ------------------------------------------*/
#define VXI_IRQ_ENABLE_ADDR_OFFSET      (STiH416_VXI_REGISTER_BASE + 0x74)
#define VXI_IRQ_STATUS_ADDR_OFFSET      (STiH416_VXI_REGISTER_BASE + 0x78)
#define VXI_IRQ_ENABLE_VSYNC_VSL_EADGE  (0x1<<9)
#define VXI_IRQ_ENABLE_VSYNC_MASK       VXI_IRQ_ENABLE_VSYNC_VSL_EADGE
#define VXI_IRQ_STATUS_VSYNC_VSL_EADGE  (0x1<<9)
#define VXI_IRQ_STATUS_VSYNC_MASK       VXI_IRQ_STATUS_VSYNC_VSL_EADGE

/* Registers definition ------------------------------------*/
/* VXI_CLK_CONTROL */
#define VXI_CLK_DISABLE                 (0x1<<5)

/* VXI_RESET */
#define VXI_SOFT_RESET                  (0x1)

/* VXIINST_SOFT_RESETS */
#define VXI_IFM_RESET                   (0x1)

/* VXI_IFM_CLK_CONTROL */
#define VXI_IFM_CLK_DISABLE             (0x1)
#endif // _STIH416VXIREG_H
