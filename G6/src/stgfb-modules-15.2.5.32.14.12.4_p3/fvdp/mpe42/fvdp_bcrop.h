/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_bcrop.h
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */
#include "fvdp_private.h"

/* Function Prototypes ---------------------------------------------- */
void BorderCrop_Init(FVDP_CH_t CH, uint32_t BorderCtrlInitVal);

FVDP_Result_t BorderCrop_Enable(FVDP_CH_t CH, bool Enable);

FVDP_Result_t BorderCrop_Config(FVDP_CH_t  CH,
                                                                uint32_t Input_H_Size,
                                                                uint32_t Input_V_Size,
                                                                uint32_t Output_H_Size,
                                                                uint32_t Output_V_Size,
                                                                uint32_t H_Start,
                                                                uint32_t V_Start);


