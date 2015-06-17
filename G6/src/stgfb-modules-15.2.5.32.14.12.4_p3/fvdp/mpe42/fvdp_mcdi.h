/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_madi.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_MCDI_H
#define FVDP_MCDI_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#include "fvdp_private.h"
#include "../fvdp.h"
#include "../fvdp_vqdata.h"
typedef enum Mcdi_Mode_s
{
    MCDI_MODE_OFF = 0,
    MCDI_MODE_FORCED_ON = 1,
    MCDI_MODE_MCDI_LUMA = 2,
    MCDI_MODE_BLEND_AUTO = 3,
}Mcdi_Mode_t;

typedef enum Deint_Mode_s
{
    DEINT_MODE_SPATIAL,
    DEINT_MODE_MADI,
    DEINT_MODE_MCDI,
    DEINT_MODE_MACMADI,
}Deint_Mode_t;
/* Exported Functions ------------------------------------------------------- */
void            McMadiEnableDatapath (bool,FVDP_ScanType_t);
void            Mcdi_Init           (void);
void            Mcdi_Enable         (Mcdi_Mode_t Mode);
void            Mcdi_Disable        (void);
void            Mcdi_Demo           (uint32_t H_Size, uint32_t V_Size, bool enable);
FVDP_Result_t   Mcdi_SetVqData      (const VQ_Mcdi_Parameters_t * vq_data_p);
void            Mcdi_LoadDefaultVqData(void);
void            Deint_Init          (void);
void            Deint_Enable        (Deint_Mode_t Mode ,uint32_t In_Hsize ,uint32_t In_Vsize, uint32_t Out_Vsize);
void            Deint_Disable       (void);
FVDP_Result_t   Deint_DcdiInit      (void);
FVDP_Result_t   Deint_DcdiUpdate    ( bool Enable, uint8_t Strength);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_MCDI_H */

