/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_dfr.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_DFR_H
#define FVDP_DFR_H

/* Includes ----------------------------------------------------------------- */
#include "fvdp_private.h"


#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/* Exported Enums ----------------------------------------------------------- */

typedef enum
{
    DFR_FE,
    DFR_BE,
    DFR_PIP,
    DFR_AUX,
    DFR_ENC,

    DFR_NUM_CORES
} DFR_Core_t;

typedef enum
{
    DFR_FE_VIDEO1_IN,
    DFR_FE_VIDEO2_IN,
    DFR_FE_VIDEO3_IN,
    DFR_FE_VIDEO4_IN,
    DFR_FE_HSCALE_LITE_OUTPUT,
    DFR_FE_VSCALE_LITE_OUTPUT,
    DFR_FE_CCS_CURR_OUTPUT,
    DFR_FE_CCS_P2_OUTPUT,
    DFR_FE_CCS_RAW_OUTPUT,
    DFR_FE_MCMADI_DEINT,
    DFR_FE_MCMADI_TNR,
    DFR_FE_ELA_FIFO_OUTPUT,
    DFR_FE_CHLSPLIT_CH_A_OUTPUT,
    DFR_FE_CHLSPLIT_CH_B_OUTPUT,
    DFR_FE_MCC_P1_RD,
    DFR_FE_MCC_P2_RD,
    DFR_FE_MCC_P3_RD,
    DFR_FE_MCC_CCS_RAW_RD,

    DFR_FE_NUM_MUX_SOURCES
} DFR_FE_MuxSourceType_t;

typedef enum
{
    DFR_FE_VIDEO1_OUT,
    DFR_FE_VIDEO2_OUT,
    DFR_FE_VIDEO3_OUT,
    DFR_FE_VIDEO4_OUT,
    DFR_FE_HSCALE_LITE_INPUT,
    DFR_FE_VSCALE_LITE_INPUT,
    DFR_FE_CCS_CURR_INPUT,
    DFR_FE_CCS_P2_INPUT,
    DFR_FE_CCS_P2_RAW_INPUT,
    DFR_FE_MCMADI_CURR,
    DFR_FE_MCMADI_P1,
    DFR_FE_MCMADI_P2,
    DFR_FE_MCMADI_P3,
    DFR_FE_ELA_FIFO_INPUT,
    DFR_FE_CHLSPLIT_INPUT,
    DFR_FE_MCC_PROG_WR,
    DFR_FE_MCC_C_WR,
    DFR_FE_MCC_CCS_RAW_WR,

    DFR_FE_NUM_OF_MUXES
} DFR_FE_MuxType_t;

typedef enum
{
    DFR_BE_VIDEO1_IN,
    DFR_BE_VIDEO2_IN,
    DFR_BE_VIDEO3_IN,
    DFR_BE_VIDEO4_IN,
    DFR_BE_HQSCALER_OUTPUT,
    DFR_BE_COLOR_CTRL_OUTPUT,
    DFR_BE_BORDER_CROP_OUTPUT,
    DFR_BE_R2DTO3D_OUTPUT,
    DFR_BE_ELAFIFO_OUTPUT,
    DFR_BE_MCC_RD,

    DFR_BE_NUM_MUX_SOURCES
} DFR_BE_MuxSourceType_t;

typedef enum
{
    DFR_BE_VIDEO1_OUT,
    DFR_BE_VIDEO2_OUT,
    DFR_BE_VIDEO3_OUT,
    DFR_BE_VIDEO4_OUT,
    DFR_BE_HQSCALER_INPUT,
    DFR_BE_COLOR_CTRL_INPUT,
    DFR_BE_BORDER_CROP_INPUT,
    DFR_BE_R2DTO3D_INPUT,
    DFR_BE_ELAFIFO_INPUT,
    DFR_BE_MCC_WR,

    DFR_BE_NUM_OF_MUXES
} DFR_BE_MuxType_t;

typedef enum
{
    DFR_LITE_VIDEO1_IN,
    DFR_LITE_VIDEO2_IN,
    DFR_LITE_VIDEO3_IN,
    DFR_LITE_VIDEO4_IN,
    DFR_LITE_TNR_OUTPUT,
    DFR_LITE_HSCALE_LITE_OUTPUT,
    DFR_LITE_VSCALE_LITE_OUTPUT,
    DFR_LITE_COLOR_CONTROL_OUTPUT,
    DFR_LITE_BORDER_CROP_OUTPUT,
    DFR_LITE_ZOOMFIFO_OUTPUT,
    DFR_LITE_MCC_RD,
    DFR_LITE_MCC_TNR_RD,

    DFR_LITE_NUM_MUX_SOURCES
} DFR_LITE_MuxSourceType_t;

typedef enum
{
    DFR_LITE_VIDEO1_OUT,
    DFR_LITE_VIDEO2_OUT,
    DFR_LITE_VIDEO3_OUT,
    DFR_LITE_VIDEO4_OUT,
    DFR_LITE_TNR_CURR,
    DFR_LITE_TNR_PREV,
    DFR_LITE_HSCALE_LITE_INPUT,
    DFR_LITE_VSCALE_LITE_INPUT,
    DFR_LITE_COLOR_CONTROL_INPUT,
    DFR_LITE_BORDER_CROP_INPUT,
    DFR_LITE_ZOOMFIFO_INPUT,
    DFR_LITE_MCC_WR,

    DFR_LITE_NUM_OF_MUXES
} DFR_LITE_MuxType_t;

typedef enum
{
    DFR_RGB_YUV444,
    DFR_YUV422,
    DFR_Y_ONLY,
    DFR_UV_ONLY,

    DFR_NUM_CONNECTION_TYPES
} DFR_ConnectionType_t;

/* Exported Structure ------------------------------------------------------- */

/*
     ___________          ___________          ___________
    |           |        |           |        |           |
    |           |        |   D F R   |        |           |
    |  BLOCK A  | =====> |           | =====> |  BLOCK B  |
    |           |        |  C O R E  |        |           |
    |___________|        |___________|        |___________|
*/

typedef struct
{
    union
    {
        DFR_FE_MuxSourceType_t   FE_Block_A;
        DFR_BE_MuxSourceType_t   BE_Block_A;
        DFR_LITE_MuxSourceType_t LITE_Block_A;
        uint32_t                 Block_A;
    };
    DFR_ConnectionType_t         Block_A_Type;

    union
    {
        DFR_FE_MuxType_t         FE_Block_B;
        DFR_BE_MuxType_t         BE_Block_B;
        DFR_LITE_MuxType_t       LITE_Block_B;
        uint32_t                 Block_B;
    };
    DFR_ConnectionType_t         Block_B_Type;
} DFR_Connection_t;


/* Exported Functions ------------------------------------------------------- */

FVDP_Result_t DFR_Reset(DFR_Core_t DfrCore);
FVDP_Result_t DFR_Connect(DFR_Core_t DfrCore, DFR_Connection_t Connection);
FVDP_Result_t DFR_PrintConnections(DFR_Core_t DfrCore);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_DFR_H */
