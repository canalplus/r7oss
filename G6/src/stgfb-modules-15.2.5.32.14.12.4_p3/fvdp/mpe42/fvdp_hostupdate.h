/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_hostupdate.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_HOSTUPDATE_H
#define FVDP_HOSTUPDATE_H

/* Includes ----------------------------------------------------------------- */
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    HOST_CTRL_VMUX,
    HOST_CTRL_FE,
    HOST_CTRL_BE,
    HOST_CTRL_LITE,

    NUM_HOST_CTRL_BLOCKS
} HostCtrlBlock_t;

typedef enum
{
    SYNC_UPDATE,
    FORCE_UPDATE
} HostUpdateType_t;

typedef enum
{
    FE_HOST_UPDATE_COMMCTRL         = BIT0,
    FE_HOST_UPDATE_HFLITE           = BIT1,
    FE_HOST_UPDATE_VFLITE           = BIT2,
    FE_HOST_UPDATE_CCS              = BIT3,
    FE_HOST_UPDATE_DNR              = BIT4,
    FE_HOST_UPDATE_MCMADI           = BIT5,
    FE_HOST_UPDATE_MCC_IP           = BIT8,
    FE_HOST_UPDATE_MCC_OP           = BIT9,
} HostUpdate_Request_FE_t;

typedef enum
{
    BE_HOST_UPDATE_COMMCTRL         = BIT0,
    BE_HOST_UPDATE_HQSCALER         = BIT1,
    BE_HOST_UPDATE_CCTRL            = BIT2,
    BE_HOST_UPDATE_BDR              = BIT3,
    BE_HOST_UPDATE_R2DTO3D          = BIT4,
    BE_HOST_UPDATE_SPARE            = BIT6,
    BE_HOST_UPDATE_MCC_IP           = BIT7,
    BE_HOST_UPDATE_MCC_OP           = BIT8,
} HostUpdate_Request_BE_t;

typedef enum
{
    LITE_HOST_UPDATE_COMMCTRL       = BIT0,
    LITE_HOST_UPDATE_TNR_SYNC       = BIT1,
    LITE_HOST_UPDATE_HFLITE         = BIT2,
    LITE_HOST_UPDATE_VFLITE         = BIT3,
    LITE_HOST_UPDATE_CCTRL_LITE     = BIT4,
    LITE_HOST_UPDATE_BDR            = BIT5,
    LITE_HOST_UPDATE_ZOOMFIFO       = BIT6,
    LITE_HOST_UPDATE_MCC_IP         = BIT7,
    LITE_HOST_UPDATE_MCC_OP         = BIT8,
} HostUpdate_Request_LITE_t;


/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
// bits in gW_SyncUpdateStatus represent clocks to be updated.  Bits are assigned are defined below.

FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t        CH,
                                        HostUpdateType_t Type);

FVDP_Result_t HostUpdate_SetUpdate_FE(HostUpdate_Request_FE_t UpdateRequestFlags,
                                      HostUpdateType_t        Type);

FVDP_Result_t HostUpdate_SetUpdate_BE(HostUpdate_Request_BE_t UpdateRequestFlags,
                                      HostUpdateType_t        Type);

FVDP_Result_t HostUpdate_SetUpdate_LITE(FVDP_CH_t                 CH,
                                        HostUpdate_Request_LITE_t UpdateRequestFlags,
                                        HostUpdateType_t          Type);

FVDP_Result_t HostUpdate_HardwareUpdate(FVDP_CH_t        CH,
                                        HostCtrlBlock_t  HostCtrlBlock,
                                        HostUpdateType_t UpdateType);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_HOSTUPDATE_H */

