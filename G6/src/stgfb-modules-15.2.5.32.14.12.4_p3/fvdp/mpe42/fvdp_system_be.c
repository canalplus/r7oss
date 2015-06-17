/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_be.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_ivp.h"
#include "fvdp_color.h"
#include "fvdp_common.h"
#include "fvdp_router.h"
#include "fvdp_scalerlite.h"
#include "fvdp_hqscaler.h"
#include "fvdp_datapath.h"
#include "fvdp_fbm.h"
#include "fvdp_vcpu.h"
#include "fvdp_debug.h"
#include "fvdp_eventctrl.h"
#include "fvdp_vcd.h"
#include "fvdp_hostupdate.h"

/* Local definitions  ------------------------------------------------------ */
// Delay CSC unmuting by (CSC_UNMUTE_DELAY -1) output frames
#define CSC_UNMUTE_DELAY    5
/* Private Function Prototypes ---------------------------------------------- */

static FVDP_Result_t SYSTEM_BE_InitMCC(FVDP_HandleContext_t* HandleContext_p);
static void SYSTEM_BE_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_BE_ConfigureEventCtrl(FVDP_HandleContext_t* HandleContext_p);

/* Private Macros ----------------------------------------------------------- */

#define VCD_MCC_AddrChange(client)                                     \
    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_ ## client ## _ADDR, \
            MCC_IsEnabled(client) ? REG32_Read(FVDP_MCC_BE_BASE_ADDRESS \
                                    + (client ## _ADDR)) : ~0);

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : SYSTEM_BE_Init
Description : Initialize FVDP_BE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_BE_Init(FVDP_HandleContext_t* HandleContext_p)
{
    CscFull_Init();

    // Set the Frame Reset Event line number
    EventCtrl_SetFrameReset_BE(FRAME_RESET_ALL_BE, 2, DISPLAY_SOURCE);

    // Set the Sync Update Event line number
    EventCtrl_SetSyncEvent_BE(EVENT_ID_ALL_BE, 1, DISPLAY_SOURCE);

    Csc_Mute( MC_BLACK);

    // Initialize the HQ Scaler HW
    HQScaler_Init();

    //Init HQ scaler sharpness
    SYSTEM_PSI_Sharpness_Init(HandleContext_p);

    return SYSTEM_BE_InitMCC(HandleContext_p);
}

/*******************************************************************************
Name        : SYSTEM_BE_InitMCC
Description : Initialize FVDP_BE MCC base addresses

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_BE_InitMCC(FVDP_HandleContext_t* HandleContext_p)
{
    const MCCClientToBufferTypeMap_t * Mapping_p;

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_Main[] =
    {
        { OP_Y_RD,              BUFFER_C_Y          },
        { OP_RGB_UV_RD,         BUFFER_C_UV         },
        { OP_Y_WR,              BUFFER_C_Y          },
        { OP_UV_WR,             BUFFER_C_UV         },
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_AUX[] =
    {
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_PIP[] =
    {
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_ENC[] =
    {
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    if (HandleContext_p->CH == FVDP_MAIN)
        Mapping_p = &(ClientToBufferMapping_Main[0]);
    else
    if (HandleContext_p->CH == FVDP_PIP)
        Mapping_p = &(ClientToBufferMapping_PIP[0]);
    else
    if (HandleContext_p->CH == FVDP_AUX)
        Mapping_p = &(ClientToBufferMapping_AUX[0]);
    else
    if (HandleContext_p->CH == FVDP_ENC)
        Mapping_p = &(ClientToBufferMapping_ENC[0]);
    else
        return FVDP_ERROR;

    while (Mapping_p->MCC_Client != MCC_NUM_OF_CLIENTS)
    {
        BufferType_t        BufferType   = Mapping_p->BufferType;
        FVDP_BufferPool_t * BufferPool_p = &HandleContext_p->FrameInfo.BufferPool[BufferType];
        MCC_Config_t        Config       = { .crop_enable = FALSE };
        MCC_SetBaseAddr(Mapping_p->MCC_Client, BufferPool_p->BufferPoolArray[0].Addr, Config);
        Mapping_p ++;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_BE_ProcessOutput(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_Frame_t* CurrentOutputFrame_p = NULL;
    FVDP_Frame_t* PrevOutputFrame_p = NULL;
    FVDP_CH_t     CH = HandleContext_p->CH;

    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpOutputLock);

    // Call the output debug handler
    FVDP_DEBUG_Output(HandleContext_p);

    // Call the FBM for the Output
    ErrorCode = FBM_OUT_Update(HandleContext_p);
    if (ErrorCode != FVDP_OK)
    {
        HandleContext_p->OutputUnmuteDelay = CSC_UNMUTE_DELAY;
        SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);
        return ErrorCode;
    }

    // Get the current output frame
    CurrentOutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);
    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentOutputFrame_p) == FALSE)
    {
        SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);
        return FVDP_ERROR;
    }

    // Get the previous output frame
    PrevOutputFrame_p = SYSTEM_GetPrevOutputFrame(HandleContext_p);
    if (HandleContext_p->OutputUnmuteDelay)
        HandleContext_p->OutputUnmuteDelay --;


     /*
    if ((HandleContext_p->UpdateFlags.OutputUpdateFlag & UPDATE_OUTPUT_RASTER_INFO)
        || (DatapathBE != HandleContext_p->DataPath.DatapathBE))

        used to be conditional, now systematic
     */
    {
        ErrorCode = SYSTEM_BE_ConfigureEventCtrl(HandleContext_p);
        if (ErrorCode != FVDP_OK)
        {
            SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);
            return ErrorCode;
        }

        // Clear UPDATE_OUTPUT_RASTER_INFO bit after setup EventCtrl
        HandleContext_p->UpdateFlags.OutputUpdateFlag &= ~UPDATE_OUTPUT_RASTER_INFO;
    }


    // Configure the BE Data path if it is different from current configuration
    if (DATAPATH_BE_NORMAL != HandleContext_p->DataPath.DatapathBE)
    {
        HandleContext_p->DataPath.DatapathBE = DATAPATH_BE_NORMAL;
#if (DFR_BE_PROGRAMMING_IN_VCPU == TRUE)
        FBM_IF_SendDataPath(HandleContext_p->CH, DATAPATH_MAIN_BE, DATAPATH_BE_NORMAL);
#else
        DATAPATH_BE_Connect(HandleContext_p->DataPath.DatapathBE);
#endif
    }

    // Configure all the FVDP_BE MCCs
    SYSTEM_BE_ConfigureMCC(HandleContext_p);

    // Configure the Color Control
    if((PrevOutputFrame_p == NULL)||(PrevOutputFrame_p->FrameID == INVALID_FRAME_ID))
    {
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_COLOR_CONTROL;
    }
    else if ((PrevOutputFrame_p->InputVideoInfo.ColorSpace != CurrentOutputFrame_p->InputVideoInfo.ColorSpace) ||
              (PrevOutputFrame_p->OutputVideoInfo.ColorSpace != CurrentOutputFrame_p->OutputVideoInfo.ColorSpace))
    {
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_COLOR_CONTROL;
    }

    if(SYSTEM_CheckPSIColorLevel(HandleContext_p))
    {
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_COLOR_CONTROL;
    }

    if ((HandleContext_p->UpdateFlags.OutputUpdateFlag & UPDATE_OUTPUT_COLOR_CONTROL) && (!HandleContext_p->OutputUnmuteDelay))
    {
        ColorMatrix_Adjuster_t ColorControl;
        SYSTEM_CopyColorControlData(HandleContext_p,&ColorControl);

        CscFull_Update(CurrentOutputFrame_p->InputVideoInfo.ColorSpace,
                       CurrentOutputFrame_p->OutputVideoInfo.ColorSpace,
                       &ColorControl);

        // Clear UPDATE_OUTPUT_COLOR_CONTROL bit after update color control
        HandleContext_p->UpdateFlags.OutputUpdateFlag &= ~UPDATE_OUTPUT_COLOR_CONTROL;
    }

    if((PrevOutputFrame_p == NULL)||(PrevOutputFrame_p->FrameID == INVALID_FRAME_ID))
    {
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_HQ_SCALER;
    }
    else if ((PrevOutputFrame_p->InputVideoInfo.Width != CurrentOutputFrame_p->InputVideoInfo.Width)   ||
             (PrevOutputFrame_p->InputVideoInfo.Height != CurrentOutputFrame_p->InputVideoInfo.Height) ||
             (PrevOutputFrame_p->OutputVideoInfo.Width != CurrentOutputFrame_p->OutputVideoInfo.Width) ||
             (PrevOutputFrame_p->OutputVideoInfo.Height != CurrentOutputFrame_p->OutputVideoInfo.Height) ||
             (PrevOutputFrame_p->InputVideoInfo.ColorSpace != CurrentOutputFrame_p->InputVideoInfo.ColorSpace))
    {
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_HQ_SCALER;
    }

    if (HandleContext_p->UpdateFlags.OutputUpdateFlag & UPDATE_OUTPUT_HQ_SCALER)
    {
        FVDP_HQScalerConfig_t HQScalerConfig;

        HQScalerConfig.ProcessingRGB = (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC);

        // Setup parameters of vertical scaling and horizontal scaling
        HQScalerConfig.InputFormat.ColorSampling = CurrentOutputFrame_p->InputVideoInfo.ColorSampling;
        HQScalerConfig.InputFormat.ColorSpace = CurrentOutputFrame_p->InputVideoInfo.ColorSpace;
        HQScalerConfig.InputFormat.ScanType = CurrentOutputFrame_p->InputVideoInfo.ScanType;
        HQScalerConfig.InputFormat.FieldType= CurrentOutputFrame_p->InputVideoInfo.FieldType;
        HQScalerConfig.InputFormat.HWidth = CurrentOutputFrame_p->InputVideoInfo.Width;
        HQScalerConfig.InputFormat.VHeight = CurrentOutputFrame_p->InputVideoInfo.Height;
        HQScalerConfig.OutputFormat.ScanType = CurrentOutputFrame_p->OutputVideoInfo.ScanType;
        HQScalerConfig.OutputFormat.FieldType= CurrentOutputFrame_p->OutputVideoInfo.FieldType;
        HQScalerConfig.OutputFormat.HWidth = CurrentOutputFrame_p->OutputVideoInfo.Width;
        HQScalerConfig.OutputFormat.VHeight = CurrentOutputFrame_p->OutputVideoInfo.Height;

        // Configure the Horizontal HQ Scaler
        HFHQScaler_HWConfig(FVDP_MAIN, HQScalerConfig);

        if((CurrentOutputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
            &&((CurrentOutputFrame_p->OutputVideoInfo.Height * 2) == CurrentOutputFrame_p->InputVideoInfo.Height))
        {
            HQScalerConfig.InputFormat.VHeight = CurrentOutputFrame_p->InputVideoInfo.Height/2;
            HQScalerConfig.OutputFormat.ScanType = PROGRESSIVE;
        }
        // Configure the Vertical HQ Scaler
        VFHQScaler_HWConfig(FVDP_MAIN, HQScalerConfig, HandleContext_p->ProcessingType);

        // Configure the FVDP_DCDi
        if (HandleContext_p->ProcessingType != FVDP_SPATIAL)  // Use DEINT DCDi
        {
            if(CurrentOutputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {
                HQScaler_DcdiUpdate(HandleContext_p->CH, FALSE, 0x0);
            }
            else
            {
                HQScaler_DcdiUpdate(HandleContext_p->CH, TRUE, 0x4);
            }
        }
        else  // Use SCALER LITE DCDi
        {
            if(CurrentOutputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {
                HQScaler_DcdiUpdate(HandleContext_p->CH, TRUE, 0x4);
            }
            else
            {
                HQScaler_DcdiUpdate(HandleContext_p->CH, FALSE, 0);
            }
        }

        // Clear UPDATE_OUTPUT_HQ_SCALER bit after update Scaler setup
        HandleContext_p->UpdateFlags.OutputUpdateFlag &= ~UPDATE_OUTPUT_HQ_SCALER;
    }

    //PSI control
    if(HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_OUTPUT_PSI)
    {
        ErrorCode = SYSTEM_PSI_Sharpness(HandleContext_p);
        if(ErrorCode == FVDP_OK)
        {
            HandleContext_p->UpdateFlags.CommitUpdateFlag &= ~UPDATE_OUTPUT_PSI;
        }
    }

    SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);

    return ErrorCode;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void SYSTEM_BE_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t*    CurrentOutputFrame_p;
    MCC_Config_t*    MccConfig_p;
    bool             AlternatedLineReadStatus = FALSE;
    FVDP_FieldType_t OutputFieldType = BOTTOM_FIELD;

    VCD_MCC_AddrChange(OP_Y_RD);
    VCD_MCC_AddrChange(OP_RGB_UV_RD);

    CurrentOutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);

    // Configure the PRO MCC clients if the current pro scan frame is valid
    if (CurrentOutputFrame_p != NULL)
    {
        if((CurrentOutputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
           &&((CurrentOutputFrame_p->OutputVideoInfo.Height * 2) == CurrentOutputFrame_p->InputVideoInfo.Height))
          {
            EventCtrl_GetCurrentFieldType_BE(DISPLAY_SOURCE, &OutputFieldType);
            AlternatedLineReadStatus = TRUE;
            
            //Need to invert the output field type for the NEXT output event, we must use the INVERSE of the FID_STATUS signal.
            if(OutputFieldType == TOP_FIELD)
              {
                OutputFieldType = BOTTOM_FIELD;
              }
            else
              {
                OutputFieldType = TOP_FIELD;
              }
          }

        // OP_Y_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if ((CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G] != NULL)
            &&(CurrentOutputFrame_p->InputVideoInfo.ColorSpace != sRGB)
            &&(CurrentOutputFrame_p->InputVideoInfo.ColorSpace != RGB)
            &&(CurrentOutputFrame_p->InputVideoInfo.ColorSpace != RGB861))
        {
            // Configure OP_Y_RD MCC Client and set the base address
            MccConfig_p = &(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = AlternatedLineReadStatus;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(OP_Y_RD, *MccConfig_p);

            if((OutputFieldType == BOTTOM_FIELD) && (AlternatedLineReadStatus == TRUE))
            {
                if(MccConfig_p->crop_enable == FALSE)
                {
                    MccConfig_p->crop_window.crop_v_start = 1;
                }
                else
                {
                    MccConfig_p->crop_window.crop_v_start += 1;
                }
                MccConfig_p->crop_enable = TRUE;
            }

            MCC_SetBaseAddr(OP_Y_RD, CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->Addr, *MccConfig_p);
            MCC_SetOddSignal(OP_Y_RD, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(OP_Y_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(OP_Y_RD);
        }

        // OP_RGB_UV_WR
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B] != NULL)
        {
            // Configure PRO_UV_WR MCC Client and set the base address
            MccConfig_p = &(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = AlternatedLineReadStatus;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(OP_RGB_UV_RD, *MccConfig_p);

            if((OutputFieldType == BOTTOM_FIELD) && (AlternatedLineReadStatus == TRUE))
            {
                if(MccConfig_p->crop_enable == FALSE)
                {
                    MccConfig_p->crop_window.crop_v_start = 1;
                }
                else
                {
                    MccConfig_p->crop_window.crop_v_start += 1;
                }

                MccConfig_p->crop_enable = TRUE;
            }

            MCC_SetBaseAddr(OP_RGB_UV_RD, CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->Addr, *MccConfig_p);
            MCC_SetOddSignal(OP_RGB_UV_RD, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(OP_RGB_UV_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(OP_RGB_UV_RD);
        }
    }

}

/*******************************************************************************
Name        : SYSTEM_BE_ConfigureEventCtrl
Description : Set sync event vertical position (COMMCTRL_EVENT_POS)
                  and the frame reset vertical position (COMMCTRL_FRST_POS) based
                  on Display Raster information
Parameters  :  [in] HandleContext_p.pointer to handle context
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster is out of range
*******************************************************************************/
static FVDP_Result_t SYSTEM_BE_ConfigureEventCtrl(FVDP_HandleContext_t* HandleContext_p)
{
    uint32_t Position;

    Position = HandleContext_p->OutputRasterInfo.VStart + HandleContext_p->OutputRasterInfo.VActive;

    if(Position > (HandleContext_p->OutputRasterInfo.VTotal - 1))
    {
        return FVDP_ERROR;
    }

    // Set the Frame Reset Event line number
    EventCtrl_SetFrameReset_BE(FRAME_RESET_ALL_BE, Position + 1, DISPLAY_SOURCE);

    // Set the Sync Update Event line number
    EventCtrl_SetSyncEvent_BE(EVENT_ID_ALL_BE, Position, DISPLAY_SOURCE);

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_BE_PowerDown
Description : Power down FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_BE_PowerDown(void)
{
    // Disable the MCC client
    MCC_Disable(OP_Y_RD);
    MCC_Disable(OP_RGB_UV_RD);

    // Disable IP blocks

    // Disable clock of IP blocks
    EventCtrl_ClockCtrl_BE(MODULE_CLK_ALL_BE, GND_CLK);

    HostUpdate_HardwareUpdate(FVDP_MAIN, HOST_CTRL_BE, FORCE_UPDATE);

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_BE_PowerUp
Description : Power down FVDP_BE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_BE_PowerUp(void)
{
    FVDP_HandleContext_t* HandleContext_p;

    HandleContext_p = SYSTEM_GetHandleContextOfChannel(FVDP_MAIN);

    // MAIN Channel  has been never opened before
    if (HandleContext_p == NULL)
    {
        return FVDP_OK;
    }

    //Set Hard reset BE block
    EventCtrl_HardReset_BE(HARD_RESET_ALL_BE, TRUE);

    //INIT local variables

    //Clear Hard reset BE block
    EventCtrl_HardReset_BE(HARD_RESET_ALL_BE, FALSE);

    // Init BE Event control
    EventCtrl_Init(FVDP_MAIN);

    //Initialize FVDP_FE software module and call FVDP_FE hardware init function
    SYSTEM_BE_Init(HandleContext_p);

    // Initialize the MCC HW
    MCC_Init(FVDP_MAIN);

    HostUpdate_HardwareUpdate(FVDP_MAIN, HOST_CTRL_BE, FORCE_UPDATE);

    return FVDP_OK;
}

