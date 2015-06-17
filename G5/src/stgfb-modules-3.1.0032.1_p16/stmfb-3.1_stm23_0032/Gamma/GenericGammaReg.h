/***********************************************************************
 *
 * File: Gamma/GenericGammaReg.h
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
\***********************************************************************/

#ifndef GENERICGAMMAREG_H
#define GENERICGAMMAREG_H

//CURSOR
#define CUR_CTL_REG_OFFSET   0x00
#define CUR_VPO_REG_OFFSET   0x0c
#define CUR_PML_REG_OFFSET   0x14
#define CUR_PMP_REG_OFFSET   0x18
#define CUR_SIZE_REG_OFFSET  0x1C
#define CUR_CML_REG_OFFSET   0x20
#define CUR_AWS_REG_OFFSET   0x28
#define CUR_AWE_REG_OFFSET   0x2C

//MIXER
#define MXn_CTL_REG_OFFSET 0x00
#define MXn_BKC_REG_OFFSET 0x04
#define MXn_BCO_REG_OFFSET 0x0C 
#define MXn_BCS_REG_OFFSET 0x10
#define MXn_AVO_REG_OFFSET 0x28
#define MXn_AVS_REG_OFFSET 0x2C
#define MXn_CRB_REG_OFFSET 0x34
#define MXn_ACT_REG_OFFSET 0x38

//GDP - Only some of the registers are defined as common
#define GDPn_NVN_OFFSET    0x24
#define GDPn_PKZ_OFFSET    0xfc

/*
 * Modern GDP implementations use these instead of the PKZ register to
 * configure memory bus transactions.
 */ 
#define GDPn_PAS           0xEC
#define GDPn_MAOS          0xF0
#define GDPn_MIOS          0xF4
#define GDPn_MACS          0xF8
#define GDPn_MAMS          0xFC

//Video Display
#define DISP_CTL          0x000
#define DISP_LUMA_HSRC    0x004
#define DISP_LUMA_VSRC    0x008
#define DISP_CHR_HSRC     0x00c
#define DISP_CHR_VSRC     0x010
#define DISP_TARGET_SIZE  0x014
#define DISP_NLZZD_Y      0x018
#define DISP_NLZZD_C      0x01c
#define DISP_PDELTA       0x020
#define DISP_STATUS       0x07c
#define DISP_MA_CTL       0x080
#define DISP_LUMA_BA      0x084
#define DISP_CHR_BA       0x088
#define DISP_PMP          0x08c
#define DISP_LUMA_XY      0x090
#define DISP_CHR_XY       0x094
#define DISP_LUMA_SIZE    0x098
#define DISP_CHR_SIZE     0x09c
#define DISP_HFP          0x0a0
#define DISP_VFP          0x0a4
#define DISP_PKZ          0x0fc

#define DISP_LHF_COEF     0x100
#define DISP_LVF_COEF     0x18c
#define DISP_CHF_COEF     0x200
#define DISP_CVF_COEF     0x28c

#endif //GENERICGAMMAREG_H
