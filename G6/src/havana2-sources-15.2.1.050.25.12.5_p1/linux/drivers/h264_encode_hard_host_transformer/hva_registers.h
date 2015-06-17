/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef HVA_REGISTERS_H
#define HVA_REGISTERS_H

#include <linux/types.h>

//
// Register access macros
//

#define WriteHvaRegister(     Address, Value)      writel( Value, (volatile void *) ( HvaRegisterBase + Address ) )
#define ReadHvaRegister(      Address)             (unsigned int) readl((volatile void *) ( HvaRegisterBase + Address ) )
#define WriteByteHvaRegister( Address, Value)      writeb( Value, (volatile void *) ( HvaRegisterBase + Address ) )
#define ReadByteHvaRegister(  Address)             (unsigned int) readb((volatile void *) ( HvaRegisterBase + Address ) )

//
// Register addresses
//
//Base addresses retrieved from platform file


//#define HVA_REGISTER_BASE     0xFDE26000
//#define HVA_REGISTER_SIZE     0x400

//
// Register description
//

#define HVA_HIF_REG_RST                 0x0100U /* HVA Soft-reset */
#define HVA_HIF_REG_RST_ACK             0x0104U /* HVA Soft-Reset Acknowledge */
#define HVA_HIF_REG_MIF_CFG             0x0108U /* Configuration of STBUS plug EMI and LMI */
#define HVA_HIF_REG_HEC_MIF_CFG         0x010CU /* Configuration of STBUS plug of HEC block */
#define HVA_HIF_REG_CFL                 0x0110U /* FPC Task list level */
#define HVA_HIF_FIFO_CMD                0x0114U /* Command FIFO */
#define HVA_HIF_FIFO_STS                0x0118U /* Status FIFO */
#define HVA_HIF_REG_SFL                 0x011CU /* Status FIFO level */
#define HVA_HIF_REG_IT_ACK              0x0120U /* Interrupt acknowledge */
#define HVA_HIF_REG_ERR_IT_ACK          0x0124U /* Error interrupt acknowledge */
#define HVA_HIF_REG_LMI_ERR             0x0128U /* Error on local memory interface */
#define HVA_HIF_REG_EMI_ERR             0x012CU /* Error on external memory interface */
//#define HVA_HIF_REG_HEC_MIF_ERR         0x0130U
//#define HVA_HIF_REG_HEC_STS             0x0134U
#define HVA_HIF_REG_HVC_STS             0x0138U /* Activity Status of HVC */
#define HVA_HIF_REG_HJE_STS             0x013CU /* Activity Status of HJE */
#define HVA_HIF_REG_CNT                 0x0140U /* Counter */
//#define HVA_HIF_REG_HEC_CHKSYN_DIS      0x0144U
#define HVA_HIF_REG_CLK_GATING          0x0148U /* HW Clock gating */
#define HVA_HIF_REG_VERSION             0x014CU /* HVA version */
#ifdef CONFIG_STM_VIRTUAL_PLATFORM
#define HVA_HIF_REG_EXPECTED_VERSION  0x1397U /* HVA model expected version for cannes/monaco (HVA_CANNES label) */
#else
#define HVA_HIF_REG_EXPECTED_VERSION  0x397U /* HVA expected version for cannes/monaco (HVA_CANNES label) */
#endif

//
// Register section
// defines macros for registers and associated fields specific informations
//

#define HVA_HIF_REG_CLK_GATING_HVC_CLK_EN                    (0x00000001)
#define HVA_HIF_REG_CLK_GATING_HEC_CLK_EN                    (0x00000002)
#define HVA_HIF_REG_CLK_GATING_HJE_CLK_EN                    (0x00000004)

#endif
