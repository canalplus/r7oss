/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_hostupdate.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_regs.h"
#include "fvdp_common.h"
#include "fvdp_hostupdate.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */
/* Each bit represent a clock source, set to 1 after writing to a PA register.                 */
static uint32_t  VMUX_HostUpdateStatus = 0;
static uint32_t  FE_HostUpdateStatus = 0;
static uint32_t  BE_HostUpdateStatus = 0;
static uint32_t  PIP_HostUpdateStatus = 0;
static uint32_t  AUX_HostUpdateStatus = 0;
static uint32_t  ENC_HostUpdateStatus = 0;

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
#define HOSTUPDATE_FORCE_UPDATE_BIT_OFFSET   16

/* Register Access Macros------------------------------------------------ */

// VMUX register access Macros
#define VMUX_REG32_Set(Addr,BitsToSet)                  REG32_Set(Addr+VMUX_BASE_ADDRESS,BitsToSet)
// COMM register access Macros
#define COMMCLUST_FE_REG32_Set(Ch,Addr,BitsToSet)       REG32_Set(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToSet)
#define COMMCLUST_BE_REG32_Set(Ch,Addr,BitsToSet)       REG32_Set(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToSet)

static const uint32_t COMM_CLUST_AUX_BASE_ADDR[] = {0, COMM_CLUST_PIP_BASE_ADDRESS, COMM_CLUST_AUX_BASE_ADDRESS, COMM_CLUST_ENC_BASE_ADDRESS};
#define COMMCLUST_AUX_REG32_Write(Ch,Addr,Val)          REG32_Write(Addr+COMM_CLUST_AUX_BASE_ADDR[Ch],Val)
#define COMMCLUST_AUX_REG32_Set(Ch,Addr,BitsToSet)      REG32_Set(Addr+COMM_CLUST_AUX_BASE_ADDR[Ch],BitsToSet)



/* Functions ---------------------------------------------------------------- */

//******************************************************************************
//
// FUNCTION       : FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t CH, HostUpdateType_t UpdateType)
//
// USAGE          : Sets host update flags for the requested clock domain.
//
//
// INPUT          :   HostUpdateRequestFlags - sync update flags to set
//
// OUTPUT         : None
//
// GLOBALS        : None
//
// USED_REGS      : None
//
// PRE_CONDITION  : Associated register should be set before calling this routine
//
// POST_CONDITION :
//
//******************************************************************************
FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t        CH,
                                        HostUpdateType_t UpdateType)
{
    if (CH >= NUM_FVDP_CH)
        return (FVDP_ERROR);

    SETBIT(VMUX_HostUpdateStatus, (BIT0<<CH)<<(UpdateType*sizeof(uint8_t)));
    //VMUX_REG32_Set(VMUX_UPDATE_CTRL, (BIT0<<CH)<<(UpdateType*sizeof(uint8_t)));
    return ( FVDP_OK);
}

//******************************************************************************
//
// FUNCTION       : FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t CH, HostUpdateType_t UpdateType)
//
// USAGE          : Sets host update flags for the requested clock domain.
//
//
// INPUT          :   HostUpdateRequestFlags - sync update flags to set
//
// OUTPUT         : None
//
// GLOBALS        : None
//
// USED_REGS      : None
//
// PRE_CONDITION  : Associated register should be set before calling this routine
//
// POST_CONDITION :
//
//******************************************************************************
FVDP_Result_t HostUpdate_SetUpdate_FE(HostUpdate_Request_FE_t UpdateRequestFlags,
                                      HostUpdateType_t        UpdateType)
{
    if (UpdateType == SYNC_UPDATE)
    {
        SETBIT(FE_HostUpdateStatus, (UpdateRequestFlags));
    }
    else
    {
        SETBIT(FE_HostUpdateStatus, (UpdateRequestFlags << HOSTUPDATE_FORCE_UPDATE_BIT_OFFSET));
    }

    return ( FVDP_OK);
}

//******************************************************************************
//
// FUNCTION       : FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t CH, HostUpdateType_t UpdateType)
//
// USAGE          : Sets host update flags for the requested clock domain.
//
//
// INPUT          :   HostUpdateRequestFlags - sync update flags to set
//
// OUTPUT         : None
//
// GLOBALS        : None
//
// USED_REGS      : None
//
// PRE_CONDITION  : Associated register should be set before calling this routine
//
// POST_CONDITION :
//
//******************************************************************************
FVDP_Result_t HostUpdate_SetUpdate_BE(HostUpdate_Request_BE_t UpdateRequestFlags,
                                      HostUpdateType_t        UpdateType)
{
    if (UpdateType == SYNC_UPDATE)
    {
        SETBIT(BE_HostUpdateStatus, (UpdateRequestFlags));
    }
    else
    {
        SETBIT(BE_HostUpdateStatus, (UpdateRequestFlags << HOSTUPDATE_FORCE_UPDATE_BIT_OFFSET));
    }

    return ( FVDP_OK);
}

//******************************************************************************
//
// FUNCTION       : FVDP_Result_t HostUpdate_SetUpdate_VMUX(FVDP_CH_t CH, HostUpdateType_t UpdateType)
//
// USAGE          : Sets host update flags for the requested clock domain.
//
//
// INPUT          :   HostUpdateRequestFlags - sync update flags to set
//
// OUTPUT         : None
//
// GLOBALS        : None
//
// USED_REGS      : None
//
// PRE_CONDITION  : Associated register should be set before calling this routine
//
// POST_CONDITION :
//
//******************************************************************************
FVDP_Result_t HostUpdate_SetUpdate_LITE(FVDP_CH_t                 CH,
                                        HostUpdate_Request_LITE_t UpdateRequestFlags,
                                        HostUpdateType_t          UpdateType)
{
    uint32_t* HostUpdateStatus_p;

    if ((CH >= NUM_FVDP_CH)||(CH == FVDP_MAIN))
    {
        return (FVDP_ERROR);
    }

    if (CH == FVDP_PIP)
    {
        HostUpdateStatus_p = &PIP_HostUpdateStatus;
    }
    else if(CH == FVDP_AUX)
    {
        HostUpdateStatus_p = &AUX_HostUpdateStatus;
    }
    else if(CH == FVDP_ENC)
    {
        HostUpdateStatus_p = &ENC_HostUpdateStatus;
    }

    if (UpdateType == SYNC_UPDATE)
    {
        SETBIT(*HostUpdateStatus_p, (UpdateRequestFlags));
    }
    else
    {
        SETBIT(*HostUpdateStatus_p, (UpdateRequestFlags << HOSTUPDATE_FORCE_UPDATE_BIT_OFFSET));
    }

    return ( FVDP_OK);
}

//******************************************************************************
//
// FUNCTION       : FVDP_Result_t HostUpdate_HardwareUpdate( HostUpdate_SyncUpdateRequestFlags_t SyncUpdateRequestFlags)
//
// USAGE          : Updates Hardware driver periodically by application.  This
//                  function should be called when sync updates are to be applied to the HW.
//
// INPUT          : In: SyncUpdateRequestFlags - caller can select domain to update (also contingent on wether bit is set in SyncUpdateStatus).
//
// OUTPUT         : None
//
// GLOBALS        : SyncUpdateStatus               (R/W)
//
// USED_REGS      : MOT_DET_UPDATE_CTRL               (W)
//                          DEINT_UPDATE_CTRL       (W)
//                          OVP_UPDATE_CTRL           (W)
//                          SCALERS_UPDATE_CTRL     (W)
//                          MVE_UPDATE_CTRL           (W)
//
// PRE_CONDITION  : None
//
// POST_CONDITION : None
//
//******************************************************************************
FVDP_Result_t HostUpdate_HardwareUpdate(FVDP_CH_t        CH,
                                        HostCtrlBlock_t  HostCtrlBlock,
                                        HostUpdateType_t UpdateType)
{
    // Perform requested host register force update
    uint32_t VMUX_UpdateCtrl         = 0;
    uint32_t FE_UpdateCtrl           = 0;
    uint32_t BE_UpdateCtrl           = 0;
    uint32_t LITE_UpdateCtrl[]       = {0, 0, 0, 0};
    uint32_t LITE_HostUpdateStatus[] = {0, PIP_HostUpdateStatus, AUX_HostUpdateStatus, ENC_HostUpdateStatus};

    if (HostCtrlBlock >= NUM_HOST_CTRL_BLOCKS)
        return (FVDP_ERROR);

    //
    // FVDP_VMUX Host Control
    //
    if (HostCtrlBlock == HOST_CTRL_VMUX)
    {
        if (CH == FVDP_MAIN)
        {
            // HostUpdate for VMUX (MAIN/PIP/AUX/ENC)
            if (CHECKBIT(VMUX_HostUpdateStatus, MIVP_SYNC_UPDATE_MASK) && (UpdateType == SYNC_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, MIVP_SYNC_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, MIVP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(VMUX_HostUpdateStatus, MIVP_FORCE_UPDATE_MASK) && (UpdateType == FORCE_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, MIVP_FORCE_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, MIVP_FORCE_UPDATE_MASK);
            }

            // Register set for VMUX
            if (VMUX_UpdateCtrl)
            {
                VMUX_REG32_Set(VMUX_UPDATE_CTRL, VMUX_UpdateCtrl);
            }

        }
        else if (CH == FVDP_PIP)
        {
            if (CHECKBIT(VMUX_HostUpdateStatus, PIVP_SYNC_UPDATE_MASK) && (UpdateType == SYNC_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, PIVP_SYNC_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, PIVP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(VMUX_HostUpdateStatus, PIVP_FORCE_UPDATE_MASK) && (UpdateType == FORCE_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, PIVP_FORCE_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, PIVP_FORCE_UPDATE_MASK);
            }

            // Register set for VMUX
            if (VMUX_UpdateCtrl)
            {
                VMUX_REG32_Set(VMUX_UPDATE_CTRL, VMUX_UpdateCtrl);
            }
        }
        else if (CH == FVDP_AUX)
        {
            if (CHECKBIT(VMUX_HostUpdateStatus, VIVP_SYNC_UPDATE_MASK) && (CH == FVDP_AUX) && (UpdateType == SYNC_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, VIVP_SYNC_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, VIVP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(VMUX_HostUpdateStatus, VIVP_FORCE_UPDATE_MASK) && (CH == FVDP_AUX) && (UpdateType == FORCE_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, VIVP_FORCE_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, VIVP_FORCE_UPDATE_MASK);
            }

            // Register set for VMUX
            if (VMUX_UpdateCtrl)
            {
                VMUX_REG32_Set(VMUX_UPDATE_CTRL, VMUX_UpdateCtrl);
            }

        }
        else // (CH == FVDP_ENC)
        {
            if (CHECKBIT(VMUX_HostUpdateStatus, EIVP_SYNC_UPDATE_MASK) && (CH == FVDP_ENC) && (UpdateType == SYNC_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, EIVP_SYNC_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, EIVP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(VMUX_HostUpdateStatus, EIVP_FORCE_UPDATE_MASK) && (CH == FVDP_ENC) && (UpdateType == FORCE_UPDATE))
            {
                SETBIT(VMUX_UpdateCtrl, EIVP_FORCE_UPDATE_MASK);
                CLRBIT(VMUX_HostUpdateStatus, EIVP_FORCE_UPDATE_MASK);
            }

            // Register set for VMUX
            if (VMUX_UpdateCtrl)
            {
                VMUX_REG32_Set(VMUX_UPDATE_CTRL, VMUX_UpdateCtrl);
            }
        }

    }

    //
    // FVDP_FE Host Control
    //
    if (HostCtrlBlock == HOST_CTRL_FE)
    {
        // SyncUpdate for FE (MAIN)
        if (UpdateType == SYNC_UPDATE)
        {
            if (CHECKBIT(FE_HostUpdateStatus, COMMCTRL_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, COMMCTRL_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, COMMCTRL_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, HFLITE_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, HFLITE_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, HFLITE_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, VFLITE_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, VFLITE_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, VFLITE_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, CCS_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, CCS_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, CCS_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, DNR_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, DNR_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, DNR_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCMADI_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCMADI_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCMADI_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCC_IP_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCC_IP_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCC_IP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCC_OP_SYNC_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCC_OP_SYNC_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCC_OP_SYNC_UPDATE_MASK);
            }
        }

        // ForceUpdate for FE (MAIN)
        if (UpdateType == FORCE_UPDATE)
        {
            if (CHECKBIT(FE_HostUpdateStatus, COMMCTRL_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, COMMCTRL_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, COMMCTRL_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, HFLITE_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, HFLITE_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, HFLITE_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, VFLITE_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, VFLITE_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, VFLITE_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, CCS_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, CCS_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, CCS_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, DNR_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, DNR_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, DNR_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCMADI_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCMADI_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCMADI_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCC_IP_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCC_IP_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCC_IP_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(FE_HostUpdateStatus, MCC_OP_FORCE_UPDATE_MASK))
            {
                SETBIT(FE_UpdateCtrl, MCC_OP_FORCE_UPDATE_MASK);
                CLRBIT(FE_HostUpdateStatus, MCC_OP_FORCE_UPDATE_MASK);
            }
        }

        // Register set for FE
        if (FE_UpdateCtrl)
        {
            COMMCLUST_FE_REG32_Set(FVDP_MAIN, COMMCTRL_UPDATE_CTRL, FE_UpdateCtrl);
        }
    }

    //
    // FVDP_BE Host Control
    //
    if (HostCtrlBlock == HOST_CTRL_BE)
    {
        // SyncUpdate for BE (MAIN)
        if (UpdateType == SYNC_UPDATE)
        {
            if (CHECKBIT(BE_HostUpdateStatus, COMMCTRL_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, COMMCTRL_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, COMMCTRL_SYNC_UPDATE_MASK);
            }
            if (CHECKBIT(BE_HostUpdateStatus, HQSCALER_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, HQSCALER_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, HQSCALER_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, CCTRL_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, CCTRL_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, CCTRL_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, BDR_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, BDR_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, BDR_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, R2DTO3D_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, R2DTO3D_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, R2DTO3D_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, SPARE_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, SPARE_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, SPARE_SYNC_UPDATE_MASK);
            }
            if (CHECKBIT(BE_HostUpdateStatus, MCC_IP_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, MCC_IP_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, MCC_IP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, MCC_OP_SYNC_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, MCC_OP_SYNC_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, MCC_OP_SYNC_UPDATE_MASK);
            }

            // Register set for BE
            if (BE_UpdateCtrl)
            {
                COMMCLUST_BE_REG32_Set(FVDP_MAIN, COMMCTRL_UPDATE_CTRL, BE_UpdateCtrl);
            }
        }

        // ForceUpdate for BE (MAIN)
        if (UpdateType == FORCE_UPDATE)
        {
            if (CHECKBIT(BE_HostUpdateStatus, COMMCTRL_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, COMMCTRL_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, COMMCTRL_FORCE_UPDATE_MASK);
            }
            if (CHECKBIT(BE_HostUpdateStatus, HQSCALER_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, HQSCALER_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, HQSCALER_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, CCTRL_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, CCTRL_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, CCTRL_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, BDR_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, BDR_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, BDR_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, R2DTO3D_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, R2DTO3D_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, R2DTO3D_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, SPARE_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, SPARE_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, SPARE_FORCE_UPDATE_MASK);
            }
            if (CHECKBIT(BE_HostUpdateStatus, MCC_IP_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, MCC_IP_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, MCC_IP_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(BE_HostUpdateStatus, MCC_OP_FORCE_UPDATE_MASK))
            {
                SETBIT(BE_UpdateCtrl, MCC_OP_FORCE_UPDATE_MASK);
                CLRBIT(BE_HostUpdateStatus, MCC_OP_FORCE_UPDATE_MASK);
            }

            // Register set for BE
            if (BE_UpdateCtrl)
            {
                COMMCLUST_BE_REG32_Set(FVDP_MAIN, COMMCTRL_UPDATE_CTRL, BE_UpdateCtrl);
            }
        }
    }

    //
    // FVDP_LITE Host Control
    //
    if (HostCtrlBlock == HOST_CTRL_LITE)
    {
        // SyncUpdate for PIP/AUX/ENC
        if (UpdateType == SYNC_UPDATE)
        {
            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_COMMCTRL_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_COMMCTRL_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_COMMCTRL_SYNC_UPDATE_MASK);
            }
            if (CHECKBIT(LITE_HostUpdateStatus[CH], TNR_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], TNR_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], TNR_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_HFLITE_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_HFLITE_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_HFLITE_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_VFLITE_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_VFLITE_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_VFLITE_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], CCTRL_LITE_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], CCTRL_LITE_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], CCTRL_LITE_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_BDR_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_BDR_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_BDR_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], ZOOMFIFO_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], ZOOMFIFO_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], ZOOMFIFO_SYNC_UPDATE_MASK);
            }
            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_IP_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_MCC_IP_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_IP_SYNC_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_OP_SYNC_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_MCC_OP_SYNC_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_OP_SYNC_UPDATE_MASK);
            }

            // Register set for PIP/AUX/ENC
            if (LITE_UpdateCtrl[CH])
            {
                COMMCLUST_AUX_REG32_Set(CH, COMMCTRL_UPDATE_CTRL, LITE_UpdateCtrl[CH]);
            }
        }

        // ForceUpdate for PIP/AUX/ENC
        if (UpdateType == FORCE_UPDATE)
        {
            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_COMMCTRL_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_COMMCTRL_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_COMMCTRL_FORCE_UPDATE_MASK);
            }
            if (CHECKBIT(LITE_HostUpdateStatus[CH], TNR_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], TNR_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], TNR_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_HFLITE_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_HFLITE_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_HFLITE_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_VFLITE_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_VFLITE_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_VFLITE_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], CCTRL_LITE_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], CCTRL_LITE_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], CCTRL_LITE_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_BDR_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_BDR_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_BDR_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], ZOOMFIFO_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], ZOOMFIFO_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], ZOOMFIFO_FORCE_UPDATE_MASK);
            }
            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_IP_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_MCC_IP_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_IP_FORCE_UPDATE_MASK);
            }

            if (CHECKBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_OP_FORCE_UPDATE_MASK))
            {
                SETBIT(LITE_UpdateCtrl[CH], COMM_CLUST_PIP_MCC_OP_FORCE_UPDATE_MASK);
                CLRBIT(LITE_HostUpdateStatus[CH], COMM_CLUST_PIP_MCC_OP_FORCE_UPDATE_MASK);
            }

            // Register set for PIP/AUX/ENC
            if (LITE_UpdateCtrl[CH])
            {
                COMMCLUST_AUX_REG32_Set(CH, COMMCTRL_UPDATE_CTRL, LITE_UpdateCtrl[CH]);
            }
        }
    }

    return (FVDP_OK);
}

