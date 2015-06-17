/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_mcc.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_MCC_H
#define FVDP_MCC_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */
typedef enum MCC_Client_e
{
    // FVDP_FE MCC Clients
    CCS_WR,
    CCS_RD,
    P1_Y_RD,
    P2_Y_RD,
    P3_Y_RD,
    P1_UV_RD,
    P2_UV_RD,
    P3_UV_RD,
    D_Y_WR,
    D_UV_WR,
    MCDI_WR,
    MCDI_RD,
    C_Y_WR,
    C_RGB_UV_WR,

    // FVDP_BE MCC Clients
    OP_Y_RD,
    OP_RGB_UV_RD,
    OP_Y_WR,
    OP_UV_WR,

    // FVDP_AUX MCC Clients
    AUX_RGB_UV_WR,
    AUX_Y_WR,
    AUX_RGB_UV_RD,
    AUX_Y_RD,

    // FVDP_PIP MCC Clients
    PIP_RGB_UV_WR,
    PIP_Y_WR,
    PIP_RGB_UV_RD,
    PIP_Y_RD,

    // FVDP_ENC MCC Clients
    ENC_RGB_UV_WR,
    ENC_Y_WR,
    ENC_RGB_UV_RD,
    ENC_Y_RD,
    ENC_TNR_Y_RD,
    ENC_TNR_UV_RD,

    MCC_NUM_OF_CLIENTS
} MCC_Client_t;


typedef enum MCC_Compression_e
{
    MCC_COMP_NONE,
    MCC_COMP_LUMA_BASE,
    MCC_COMP_LUMA_AGGRESSIVE,
    MCC_COMP_RGB_565,
    MCC_COMP_UV_422_TO_420,// for write clients
    MCC_COMP_UV_420_TO_422 = MCC_COMP_UV_422_TO_420 // for read clients
} MCC_Compression_t;

typedef struct MCC_CropWindow_s
{
    uint16_t          crop_h_start;
    uint16_t          crop_h_size;
    uint16_t          crop_v_start;
    uint16_t          crop_v_size;
} MCC_CropWindow_t;

typedef struct MCC_Config_s
{
    uint8_t           bpp;
    MCC_Compression_t compression;
    bool              rgb_mode;
    uint16_t          buffer_h_size;
    uint16_t          buffer_v_size;
    uint16_t          stride;
    bool              crop_enable;
    bool              Alternated_Line_Read;
    MCC_CropWindow_t  crop_window;
} MCC_Config_t;

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
FVDP_Result_t MCC_Init(FVDP_CH_t CH);
bool          MCC_IsEnabled(MCC_Client_t client);
FVDP_Result_t MCC_Enable(MCC_Client_t client);
FVDP_Result_t MCC_Disable(MCC_Client_t client);
FVDP_Result_t MCC_Config(MCC_Client_t client, MCC_Config_t config);
FVDP_Result_t MCC_SetBaseAddr(MCC_Client_t client, uint32_t addr, MCC_Config_t config);
FVDP_Result_t MCC_SetOddSignal(MCC_Client_t client, bool odd);
bool          MCC_GetOddSignal(MCC_Client_t client);
FVDP_Result_t MCC_PrintConfiguration(FVDP_CH_t CH);
uint32_t      MCC_GetRequiredStrideValue(uint32_t h_size, uint32_t bpp, bool rgb_mode);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_MCC_H */

