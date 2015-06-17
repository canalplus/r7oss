/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_ccs.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_CCS_H
#define FVDP_CCS_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
FVDP_Result_t CCS_Init ( FVDP_CH_t  CH);
FVDP_Result_t CCS_SetState(FVDP_CH_t CH, FVDP_PSIState_t selection);
FVDP_Result_t CCS_SetData(FVDP_CH_t CH,
				uint16_t CCSQuantValue0, uint16_t CCSQuantValue1);
bool CCS_IsAvailable(FVDP_CH_t CH);
uint32_t CCS_GetRevision(FVDP_CH_t CH);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_CCS_H */

