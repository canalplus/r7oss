/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_datapath.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __FVDP_DATAPATH_H
#define __FVDP_DATAPATH_H

/* Includes ----------------------------------------------------------------- */
#include "fvdp_dfr.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

/* Exported Macros ---------------------------------------------------------- */

#define DATAPATH_DEBUG   TRUE


/* Exported Enums ----------------------------------------------------------- */

typedef enum
{
    DATAPATH_FE_BYPASS_ALGO_BYPASS_MEM,
    DATAPATH_FE_BYPASS_ALGO,
    DATAPATH_FE_PROGRESSIVE_SPATIAL,
    DATAPATH_FE_INTERLACED,
    DATAPATH_FE_INTERLACED_NO_CCS,
    DATAPATH_FE_PROGRESSIVE_TEMPORAL,
    DATAPATH_FE_INTERLACED_SPATIAL,
    DATAPATH_FE_PROGRESSIVE_SPATIAL_RGB,

    DATAPATH_FE_NUM
} DATAPATH_FE_t;

typedef enum
{
    DATAPATH_BE_BYPASS_ALGO_BYPASS_MEM,
    DATAPATH_BE_BYPASS_ALGO,
    DATAPATH_BE_MEM_CCTRL,
    DATAPATH_BE_NORMAL,
    DATAPATH_BE_NORMAL_RGB,

    DATAPATH_BE_NUM
} DATAPATH_BE_t;

typedef enum
{
    DATAPATH_LITE_HZOOM_VZOOM,
    DATAPATH_LITE_HSHRINK_VZOOM,
    DATAPATH_LITE_HZOOM_VSHRINK,
    DATAPATH_LITE_HSHRINK_VSHRINK,
    DATAPATH_LITE_MEMTOMEM_SPATIAL,
    DATAPATH_LITE_MEMTOMEM_TEMPORAL,
    DATAPATH_LITE_MEMTOMEM_SPATIAL_444,
    DATAPATH_LITE_MEMTOMEM_TEMPORAL_444,

    DATAPATH_LITE_NUM
} DATAPATH_LITE_t;


/* Exported Functions ------------------------------------------------------- */

FVDP_Result_t DATAPATH_FE_Connect(DATAPATH_FE_t Datapath);
FVDP_Result_t DATAPATH_BE_Connect(DATAPATH_BE_t Datapath);
FVDP_Result_t DATAPATH_LITE_Connect(FVDP_CH_t CH, DATAPATH_LITE_t Datapath);

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __FVDP_DATAPATH_H */

