/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_driver.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_DRIVER_H
#define FVDP_DRIVER_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */


/* Exported Macros ---------------------------------------------------------- */
#define DEFAULT_BRIGHTNESS           0
#define DEFAULT_CONTRAST             256
#define DEFAULT_HUE                 0
#define DEFAULT_SATURATION           256
#define DEFAULT_REDGAIN              256
#define DEFAULT_GREENGAIN            256
#define DEFAULT_BLUEGAIN             256
#define DEFAULT_REDOFFSET            0
#define DEFAULT_GREENOFFSET          0
#define DEFAULT_BLUEOFFSET           0
#define DEFAULT_SHARPNESS            0


#define MAX_DISPLAY_HSIZE     1920
#define MAX_DISPLAY_VSIZE     1080

/* Exported Functions ------------------------------------------------------- */
FVDP_Result_t fvdp_QueueFlush(FVDP_CH_t CH);
FVDP_Result_t fvdp_MuteControl (FVDP_CH_t CH, bool options);
bool fvdp_IsQueueFull(FVDP_CH_t CH);
FVDP_Result_t fvdp_GetPSIControlsRange(
    FVDP_CH_t           CH,
    FVDP_PSIControl_t  PSIControlsSelect,
    int32_t                 *pMin,
    int32_t                 *pMax);
FVDP_Result_t fvdp_GetRequiredVideoMemorySize(
    FVDP_CH_t CH,
    FVDP_ProcessingType_t   ProcessingType,
    FVDP_MaxOutputWindow_t MaxOutputWindow,
    uint32_t *MemSize);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_DRIVER_H */

