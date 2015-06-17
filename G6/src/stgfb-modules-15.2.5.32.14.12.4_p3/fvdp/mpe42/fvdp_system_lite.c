/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_fe.c
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
#include "fvdp_hostupdate.h"

/* Private Constants -------------------------------------------------------- */
#define FRAME_RESET_POS           5
#define UPDATE_EVENT_POS           FRAME_RESET_POS - 1

/* Private Macros ----------------------------------------------------------- */

#define VCD_MCC_AUX_AddrChange(client)                                          \
    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_AUX_ ## client,              \
            MCC_IsEnabled(AUX_ ## client) ? REG32_Read(FVDP_MCC_AUX_BASE_ADDRESS\
                                    + (AUX_ ## client ## _ADDR)) : ~0);

#define VCD_MCC_PIP_AddrChange(client)                                          \
    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_PIP_ ## client,              \
            MCC_IsEnabled(PIP_ ## client) ? REG32_Read(FVDP_MCC_PIP_BASE_ADDRESS\
                                    + (AUX_ ## client ## _ADDR)) : ~0);

/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t SYSTEM_LITE_DatapathConfig(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_LITE_ConfigureScaler_Input(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_LITE_ConfigureScaler_Output(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_LITE_ConfigureMCC_WR(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_LITE_ConfigureMCC_RD(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_LITE_ConfigureEventCtrl(FVDP_HandleContext_t* HandleContext_p);

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : SYSTEM_LITE_Init
Description : Initialize FVDP_LITE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_LITE_Init(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_CH_t CH = HandleContext_p->CH;

    EventCtrl_Init(HandleContext_p->CH);

    // Set the Frame Reset Event line number
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_1_IN_LITE , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_2_IN_LITE , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_3_IN_LITE , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_4_IN_LITE , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_1_OUT_LITE, 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_2_OUT_LITE, 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_3_OUT_LITE, 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_4_OUT_LITE, 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_TNR_LITE        , 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_HFLITE_LITE     , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VFLITE_LITE     , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_CCTRL_LITE      , 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_BDR_LITE        , 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_ZOOMFIFO_LITE   , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_MCC_WRITE_LITE  , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_MCC_READ_LITE   , 2, DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_INPUT_IRQ_LITE  , 2, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_DISPLAY_IRQ_LITE, 2, DISPLAY_SOURCE);

    // Set the Sync Update Event line number
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_COMMCTRL_BLK_LITE, 1, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_TNR_BLK_LITE     , 1, DISPLAY_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_HFLITE_BLK_LITE  , 1, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_VFLITE_BLK_LITE  , 1, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_CCTRL_BLK_LITE   , 1, DISPLAY_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_BDR_BLK_LITE     , 1, DISPLAY_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_ZOOMFIFO_BLK_LITE, 1, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_MCC_IP_BLK_LITE  , 1, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_MCC_OP_BLK_LITE  , 1, DISPLAY_SOURCE);

    // mute the output
    CscLight_Mute(HandleContext_p->CH, MC_BLACK);

    // Initialize the Scaler Lite HW
    HScaler_Lite_Init(CH);
    VScaler_Lite_Init(CH);


    return SYSTEM_InitMCC(HandleContext_p);
}

/*******************************************************************************
Name        : SYSTEM_LITE_ProcessInput
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_LITE_ProcessInput(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t     ErrorCode = FVDP_OK;
    FVDP_FrameInfo_t* FrameInfo_p = &(HandleContext_p->FrameInfo);
//    DATAPATH_LITE_t     DatapathLITE;
    FVDP_CH_t             CH = HandleContext_p->CH;

    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpInputLock);

    // Call the input debug handler
    FVDP_DEBUG_Input(HandleContext_p);

    // During InputUpdate, the Next Input frame becomes the Current Input frame, and
    // a new Next Input frame is allocated (cloned)
    FrameInfo_p->CurrentInputFrame_p = FrameInfo_p->NextInputFrame_p;
    FrameInfo_p->NextInputFrame_p = SYSTEM_CloneFrame(HandleContext_p, FrameInfo_p->CurrentInputFrame_p);

    if (FrameInfo_p->NextInputFrame_p == NULL)
    {
        SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);
        return FVDP_ERROR;
    }

    // temp work around to disable VCPU
    FrameInfo_p->PrevProScanFrame_p = FrameInfo_p->CurrentProScanFrame_p;

    // Convert 3D to 2D
    if(FrameInfo_p->CurrentInputFrame_p->OutputVideoInfo.FVDP3DFormat == FVDP_2D)
    {
        if(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FVDP3DFormat == FVDP_3D_SIDE_BY_SIDE)
        {
            FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropStart /= 2;
            FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropStartOffset /= 2;
            FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropWidth /= 2;
        }
        else if(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FVDP3DFormat == FVDP_3D_TOP_BOTTOM)
        {
            FrameInfo_p->CurrentInputFrame_p->CropWindow.VCropStart /= 2;
            FrameInfo_p->CurrentInputFrame_p->CropWindow.VCropStartOffset /= 2;
            FrameInfo_p->CurrentInputFrame_p->CropWindow.VCropHeight /= 2;
        }
    }

    // Update Crop window info to current input video info (aligned)
    FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropWidth = (FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropWidth) & ~1L;
    FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.Width = FrameInfo_p->CurrentInputFrame_p->CropWindow.HCropWidth;
    FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.Height = FrameInfo_p->CurrentInputFrame_p->CropWindow.VCropHeight;

    // Configure the VMUX
    SYSTEM_VMUX_Config(HandleContext_p);

    // Call the FBM Input Process
    FBM_IN_Update(HandleContext_p);

    // Select the Datapath for FVDP_FE
    SYSTEM_LITE_DatapathConfig(HandleContext_p);

   // if (HandleContext_p->UpdateFlags.OutputUpdateFlag & UPDATE_OUTPUT_RASTER_INFO)
    {
        ErrorCode = SYSTEM_LITE_ConfigureEventCtrl(HandleContext_p);
        if (ErrorCode != FVDP_OK)
       {
            SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);
            return ErrorCode;
       }

       // Clear UPDATE_OUTPUT_RASTER_INFO bit after setup EventCtrl
        HandleContext_p->UpdateFlags.OutputUpdateFlag &= ~UPDATE_OUTPUT_RASTER_INFO;
    }

    // scaler is affecting output size so it should be before mcc
    // Configure H/VScale Lite
    SYSTEM_LITE_ConfigureScaler_Input(HandleContext_p);

    // Configure all the FVDP_FE MCCs
     SYSTEM_LITE_ConfigureMCC_WR(HandleContext_p);

    //PSI control
    if(HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_INPUT_PSI)
    {
       //Add PSI system fucntion
        if(ErrorCode == FVDP_OK)
        {
            HandleContext_p->UpdateFlags.CommitUpdateFlag &= ~UPDATE_INPUT_PSI;
        }
    }

    SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);
    return ErrorCode;
}

/*******************************************************************************
Name        : SYSTEM_LITE_ProcessOutput
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_LITE_ProcessOutput(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    //DATAPATH_LITE_t DatapathLITE;
    FVDP_Frame_t* CurrentOutputFrame_p = NULL;
    FVDP_Frame_t* PrevOutputFrame_p = NULL;
    FVDP_CH_t CH = HandleContext_p->CH;
    FVDP_FieldType_t InputFieldType = AUTO_FIELD_TYPE;
    FVDP_FieldType_t OutputFieldType = AUTO_FIELD_TYPE;

    uint32_t VFLITE_BASE_ADDR[] = {VFLITE_FE_BASE_ADDRESS, VFLITE_PIP_BASE_ADDRESS, VFLITE_AUX_BASE_ADDRESS, VFLITE_ENC_BASE_ADDRESS};
    uint32_t VSV = REG32_Read(VFLITE_BASE_ADDR[CH] + VFLITE_SCALE_VALUE);

    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpOutputLock);

    // Call the output debug handler
    FVDP_DEBUG_Output(HandleContext_p);

    // Call the FBM for the Output
    ErrorCode = FBM_OUT_Update(HandleContext_p);
    if (ErrorCode != FVDP_OK)
    {
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

    if ((CurrentOutputFrame_p->InputVideoInfo.ScanType == INTERLACED)
        && (SW_ODD_FIELD_CONTROL == TRUE))
    {
        OutputFieldType = CurrentOutputFrame_p->OutputVideoInfo.FieldType;

        //if CurrentOutputFrame_p->InputVideoInfo.FieldType is not matched,
        //Remove different line store effect by FID
        if((OutputFieldType == CurrentOutputFrame_p->InputVideoInfo.FieldType)
            || (OUTPUT_FIELD_SWAP_COMPENSATION == FALSE))
        {
            if ((VSV > 0x20000) && (VSV < 0x40000))
            {
                if(CurrentOutputFrame_p->OutputVideoInfo.FieldType == TOP_FIELD)
                {
                    OutputFieldType = BOTTOM_FIELD;
                }
                else
                {
                    OutputFieldType = TOP_FIELD;
                }
            }
        }

        //Input Vertical lite scaline need to set only for shrink case
        if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VZOOM)
        || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VZOOM))
        {
            //FVDP_VMUX_INPUT2 = VTAC1,  FVDP_VMUX_INPUT3 = VTAC2 ==> HDMI
            if((HandleContext_p->InputSource == FVDP_VMUX_INPUT2)||(HandleContext_p->InputSource == FVDP_VMUX_INPUT3))
            {
                EventCtrl_ConfigOddSignal_LITE(CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, SOURCE_IN);
                EventCtrl_SetOddSignal_LITE (CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, TOP_FIELD);
            }
            else // All other input expect HDMI
            {
                EventCtrl_ConfigOddSignal_LITE(CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, FW_FID);
                EventCtrl_SetOddSignal_LITE (CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, InputFieldType);
            }
        }
    }
    // Get the previous output frame
    PrevOutputFrame_p = SYSTEM_GetPrevOutputFrame(HandleContext_p);

    if ((CurrentOutputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
        &&(CurrentOutputFrame_p->InputVideoInfo.ScanType != INTERLACED))
    {
        EventCtrl_ConfigOddSignal_LITE(CH, FIELD_ID_VFLITE_OUT_FW_FID_LITE, DISPLAY_OUT);
    }

    // Configure all the FVDP_BE MCCs
    SYSTEM_LITE_ConfigureMCC_RD(HandleContext_p);

    // Configure V/HScale Lite
    SYSTEM_LITE_ConfigureScaler_Output(HandleContext_p);

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

//TODO : This code is temporal patch untill fixing Aux compositor color space
    if((HandleContext_p->CH == FVDP_AUX) && (BUG25599_AUX_WRONG_COLOR_COMPENSATION == TRUE))
    {
        CurrentOutputFrame_p->InputVideoInfo.ColorSpace = RGB861;
    }
//end of TODO

    if (HandleContext_p->UpdateFlags.OutputUpdateFlag & UPDATE_OUTPUT_COLOR_CONTROL)
    {
        ColorMatrix_Adjuster_t ColorControl;
        SYSTEM_CopyColorControlData(HandleContext_p,&ColorControl);
        CscLight_Update(HandleContext_p->CH,
                       CurrentOutputFrame_p->InputVideoInfo.ColorSpace,
                       CurrentOutputFrame_p->OutputVideoInfo.ColorSpace,
                       &ColorControl);

        // Clear UPDATE_OUTPUT_COLOR_CONTROL bit after update color control
        HandleContext_p->UpdateFlags.OutputUpdateFlag &= ~UPDATE_OUTPUT_COLOR_CONTROL;
    }

    //PSI control
    if(HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_OUTPUT_PSI)
    {
       //Add PSI system fucntion
        if(ErrorCode == FVDP_OK)
        {
            HandleContext_p->UpdateFlags.CommitUpdateFlag &= ~UPDATE_OUTPUT_PSI;
        }
    }

    SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);

    return ErrorCode;
}

/*******************************************************************************
Name        :SYSTEM_LITE_DatapathConfig
Description :Select the Datapath for FVDP_LITE
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static FVDP_Result_t SYSTEM_LITE_DatapathConfig(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_Frame_t* CurrentProScanFrame_p = NULL;
    DATAPATH_LITE_t DatapathLITE = DATAPATH_LITE_HSHRINK_VSHRINK;
    bool HShrink = FALSE;
    bool VShrink = FALSE;
    FVDP_CH_t CH = HandleContext_p->CH;

    // Get the ProScan input frame
    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentProScanFrame_p) == FALSE)
    {
        return FVDP_ERROR;
    }

    // Set Zoom/Shrink state of FVDP_LITE
    if(CurrentProScanFrame_p->InputVideoInfo.Width > CurrentProScanFrame_p->OutputVideoInfo.Width)
    {
        HShrink = TRUE;
    }
    if(CurrentProScanFrame_p->InputVideoInfo.Height > CurrentProScanFrame_p->OutputVideoInfo.Height)
    {
        VShrink = TRUE;
    }

    // Select the Datapath for FVDP_LITE
    if((HShrink != TRUE) && (VShrink != TRUE))
    {
        DatapathLITE = DATAPATH_LITE_HZOOM_VZOOM;
    }
    else if((HShrink != TRUE) && (VShrink == TRUE))
    {
        DatapathLITE = DATAPATH_LITE_HZOOM_VSHRINK;
    }
    else if((HShrink == TRUE) && (VShrink != TRUE))
    {
        DatapathLITE = DATAPATH_LITE_HSHRINK_VZOOM;
    }
    else if((HShrink == TRUE) && (VShrink == TRUE))
    {
        DatapathLITE = DATAPATH_LITE_HSHRINK_VSHRINK;
    }

    // Configure the FVDP_LITE Data path if it is different from current configuration
    if (DatapathLITE!= HandleContext_p->DataPath.DatapathLite)
    {
        HandleContext_p->DataPath.DatapathLite= DatapathLITE;
        if (CH == FVDP_PIP)
        {
#if (DFR_PIP_PROGRAMMING_IN_VCPU == TRUE)
            FBM_IF_SendDataPath(CH, DATAPATH_MAIN_FE_LITE, DatapathLITE);
#else
            DATAPATH_LITE_Connect(CH, HandleContext_p->DataPath.DatapathLite);
#endif
        }
        if (CH == FVDP_AUX)
        {
#if (DFR_AUX_PROGRAMMING_IN_VCPU == TRUE)
            FBM_IF_SendDataPath(CH, DATAPATH_MAIN_FE_LITE, DatapathLITE);
#else
            DATAPATH_LITE_Connect(CH, HandleContext_p->DataPath.DatapathLite);
#endif
        }
    }

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
static FVDP_Result_t SYSTEM_LITE_ConfigureMCC_WR(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t* CurrentProScanFrame_p;
    MCC_Config_t* MccConfig_p;
    FVDP_FrameInfo_t* FrameInfo_p = &HandleContext_p->FrameInfo;
    MCC_Client_t RGB_UV_wr;
    MCC_Client_t Y_wr;
    FVDP_CH_t CH = HandleContext_p->CH;

    VCD_MCC_AUX_AddrChange(RGB_UV_WR);
    VCD_MCC_AUX_AddrChange(Y_WR);
    VCD_MCC_PIP_AddrChange(RGB_UV_WR);
    VCD_MCC_PIP_AddrChange(Y_WR);

    if (CH == FVDP_PIP)
    {
        RGB_UV_wr = PIP_RGB_UV_WR;
        Y_wr = PIP_Y_WR;
    }
    else if(CH == FVDP_AUX)
    {
        RGB_UV_wr = AUX_RGB_UV_WR;
        Y_wr = AUX_Y_WR;
    }
    else
    {
         return FVDP_ERROR;
    }

    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);

    // Configure the C MCC clients if the current input frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, CurrentProScanFrame_p) == TRUE)
    {
        // C_Y_WR
        // Configure C_Y_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_PRO_Y_G];
        MccConfig_p->bpp = 10;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = CurrentProScanFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentProScanFrame_p->InputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);
        MCC_Config(Y_wr, *MccConfig_p);
        MCC_Enable(Y_wr);

        // C_UV_WR
        // Configure C_UV_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_PRO_U_UV_B];
        MccConfig_p->bpp = 10;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = CurrentProScanFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentProScanFrame_p->InputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);
        MCC_Config(RGB_UV_wr, *MccConfig_p);
        MCC_Enable(RGB_UV_wr);
    }
    else
    {
        // Disable the MCC client if there is no input frame defined for it
        MCC_Disable(Y_wr);
        MCC_Disable(RGB_UV_wr);
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
static FVDP_Result_t SYSTEM_LITE_ConfigureMCC_RD(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t* CurrentOutputFrame_p;
    MCC_Config_t* MccConfig_p;
    MCC_Client_t RGB_UV_rd;
    MCC_Client_t Y_rd;
    FVDP_CH_t CH = HandleContext_p->CH;
    bool             AlternatedLineReadStatus = FALSE;
    FVDP_FieldType_t OutputFieldType = BOTTOM_FIELD;
    uint32_t VFLITE_BASE_ADDR[] = {VFLITE_FE_BASE_ADDRESS, VFLITE_PIP_BASE_ADDRESS, VFLITE_AUX_BASE_ADDRESS, VFLITE_ENC_BASE_ADDRESS};
    uint32_t VSV = REG32_Read(VFLITE_BASE_ADDR[CH] + VFLITE_SCALE_VALUE);

    VCD_MCC_AUX_AddrChange(RGB_UV_RD);
    VCD_MCC_AUX_AddrChange(Y_RD);
    VCD_MCC_PIP_AddrChange(RGB_UV_RD);
    VCD_MCC_PIP_AddrChange(Y_RD);

    if (CH == FVDP_PIP)
    {
        RGB_UV_rd = PIP_RGB_UV_RD;
        Y_rd = PIP_Y_RD;
    }
    else if(CH == FVDP_AUX)
    {
        RGB_UV_rd = AUX_RGB_UV_RD;
        Y_rd = AUX_Y_RD;
    }
    else
    {
         return FVDP_ERROR;
    }

    CurrentOutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentOutputFrame_p) == FALSE)
    {
        return FVDP_ERROR;
    }

    // Configure the PRO MCC clients if the current pro scan frame is valid
    if (CurrentOutputFrame_p != NULL)
    {
        if ((CurrentOutputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
            &&((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VSHRINK)
               || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VSHRINK)))
          {
            if (CurrentOutputFrame_p->InputVideoInfo.ScanType == INTERLACED)
              {
                if ((VSV >= 0x20000) && (VSV < 0x40000))
                  {
                    OutputFieldType = TOP_FIELD;
                  }
                else
                  {
                    OutputFieldType = CurrentOutputFrame_p->OutputVideoInfo.FieldType;
                  }
              }
            else
              {
                OutputFieldType = CurrentOutputFrame_p->OutputVideoInfo.FieldType;
              }

            AlternatedLineReadStatus = TRUE;
          }

        // OP_Y_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G] != NULL)
        {
            // Configure OP_Y_RD MCC Client and set the base address
            MccConfig_p = &(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = AlternatedLineReadStatus;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(Y_rd, *MccConfig_p);

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

            MCC_SetBaseAddr(Y_rd, CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->Addr, *MccConfig_p);
            MCC_SetOddSignal(Y_rd, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(Y_rd);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(Y_rd);
        }

        // PRO_UV_WR
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B] != NULL)
        {
            // Configure PRO_UV_WR MCC Client and set the base address
            MccConfig_p = &(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = AlternatedLineReadStatus;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(RGB_UV_rd, *MccConfig_p);

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

            MCC_SetBaseAddr(RGB_UV_rd, CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->Addr, *MccConfig_p);
            MCC_SetOddSignal(RGB_UV_rd, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(RGB_UV_rd);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(RGB_UV_rd);
        }
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
static FVDP_Result_t SYSTEM_LITE_ConfigureScaler_Input(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_Frame_t* CurrentProScanFrame_p = NULL;
    FVDP_Frame_t* PrevProScanFrame_p = NULL;
    bool          LiteScalerUpdadeFlag = FALSE;
    FVDP_CH_t CH = HandleContext_p->CH;

    // Get the current Input frame
    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentProScanFrame_p) == FALSE)
    {
        return FVDP_ERROR;
    }

     // Get the previous output frame
    PrevProScanFrame_p = SYSTEM_GetPrevProScanFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,PrevProScanFrame_p) == FALSE)
    {
        LiteScalerUpdadeFlag |= TRUE;
    }
    else if ((PrevProScanFrame_p->InputVideoInfo.Width != CurrentProScanFrame_p->InputVideoInfo.Width)   ||
             (PrevProScanFrame_p->InputVideoInfo.Height != CurrentProScanFrame_p->InputVideoInfo.Height) ||
             (PrevProScanFrame_p->OutputVideoInfo.Width != CurrentProScanFrame_p->OutputVideoInfo.Width) ||
             (PrevProScanFrame_p->OutputVideoInfo.Height != CurrentProScanFrame_p->OutputVideoInfo.Height))
    {
        LiteScalerUpdadeFlag |= TRUE;
    }

//    if (LiteScalerUpdadeFlag )
    {
        FVDP_ScalerLiteConfig_t ScalerConfig;
        uint16_t                ScalerOut_InputHWidth;
        uint16_t                ScalerOut_InputVHeight;

        ScalerConfig.CheckDatapathLite = HandleContext_p->DataPath.DatapathLite;
        ScalerConfig.ProcessingRGB = (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC);

        //Input Horizontal lite scaline need to set only for shrink case
        if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VZOOM)
        || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VSHRINK))
        {
            // Setup parameters of horizontal scaling
            ScalerConfig.InputFormat.ColorSampling = CurrentProScanFrame_p->InputVideoInfo.ColorSampling;
            ScalerConfig.InputFormat.ColorSpace = CurrentProScanFrame_p->InputVideoInfo.ColorSpace;
            ScalerConfig.InputFormat.HWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
            ScalerConfig.OutputFormat.HWidth = CurrentProScanFrame_p->OutputVideoInfo.Width;
            ScalerConfig.InputFormat.VHeight = CurrentProScanFrame_p->InputVideoInfo.Height;
            ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;

            ScalerOut_InputHWidth = ScalerConfig.OutputFormat.HWidth;
            // Configure the Horizontal  Scaler
            if (LiteScalerUpdadeFlag )
                HScaler_Lite_HWConfig(CH, ScalerConfig);
        }
        else
        {
            ScalerOut_InputHWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
        }

        //Input Vertical lite scaline need to set only for shrink case
        if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VSHRINK)
        || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VSHRINK))
        {
            // Setup parameters of vertical scaling
            ScalerConfig.InputFormat.ColorSampling = CurrentProScanFrame_p->InputVideoInfo.ColorSampling;
            ScalerConfig.InputFormat.ColorSpace = CurrentProScanFrame_p->InputVideoInfo.ColorSpace;
            ScalerConfig.InputFormat.ScanType = CurrentProScanFrame_p->InputVideoInfo.ScanType;
            ScalerConfig.InputFormat.FieldType = CurrentProScanFrame_p->InputVideoInfo.FieldType;
            ScalerConfig.InputFormat.HWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
            ScalerConfig.InputFormat.VHeight = CurrentProScanFrame_p->InputVideoInfo.Height;
            ScalerConfig.OutputFormat.ScanType = CurrentProScanFrame_p->OutputVideoInfo.ScanType;
            ScalerConfig.OutputFormat.FieldType = AUTO_FIELD_TYPE;
            ScalerConfig.OutputFormat.HWidth = CurrentProScanFrame_p->OutputVideoInfo.Width;
            ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;

           if((ScalerConfig.InputFormat.VHeight / ScalerConfig.OutputFormat.VHeight > 1)
                &&(ScalerConfig.InputFormat.ScanType == INTERLACED)
                &&(ScalerConfig.OutputFormat.ScanType == INTERLACED))
           {
               EventCtrl_ConfigOddSignal_LITE(HandleContext_p->CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, SOURCE_IN);
           }
           else
           {
               EventCtrl_ConfigOddSignal_LITE(HandleContext_p->CH,FIELD_ID_VFLITE_IN_FW_FID_LITE, FW_FID);
           }

           //The interlace output case, Sclaer Lite output set to two times bigger than output
            if(CurrentProScanFrame_p->OutputVideoInfo.ScanType == INTERLACED)
            {
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height * 2;
            }
            else
            {
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;
            }

            ScalerOut_InputVHeight = ScalerConfig.OutputFormat.VHeight;
            VScaler_Lite_OffsetUpdate(CH, ScalerConfig);

            // Configure the Horizontal  Scaler
            if (LiteScalerUpdadeFlag)
            {
                VScaler_Lite_HWConfig(CH, ScalerConfig);

                if(CurrentProScanFrame_p->InputVideoInfo.ScanType == INTERLACED)
                {
                    VScaler_Lite_DcdiUpdate(CH, TRUE, 0x4);
                }
                else
                {
                    VScaler_Lite_DcdiUpdate(CH, FALSE, 0);
                }
            }
        }
        else
        {
            ScalerOut_InputVHeight = CurrentProScanFrame_p->InputVideoInfo.Height;
        }

        CurrentProScanFrame_p->InputVideoInfo.Width = ScalerOut_InputHWidth;
        CurrentProScanFrame_p->InputVideoInfo.Height = ScalerOut_InputVHeight;

    }

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
static FVDP_Result_t SYSTEM_LITE_ConfigureScaler_Output(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    //FVDP_Frame_t* CurrentProScanFrame_p = NULL;
    //FVDP_Frame_t* PrevProScanFrame_p = NULL;
    FVDP_Frame_t* CurrentOutputFrame_p = NULL;
    FVDP_Frame_t* PrevOutputFrame_p = NULL;
    bool          LiteScalerUpdadeFlag = FALSE;
    FVDP_CH_t CH = HandleContext_p->CH;
    FVDP_ScalerLiteConfig_t ScalerConfig;

    // Get the current output frame
    CurrentOutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentOutputFrame_p) == FALSE)
    {
        return FVDP_ERROR;
    }
    // Get the previous output frame
    PrevOutputFrame_p = SYSTEM_GetPrevOutputFrame(HandleContext_p);

    if((PrevOutputFrame_p == NULL)||(PrevOutputFrame_p->FrameID == INVALID_FRAME_ID))
    {
        LiteScalerUpdadeFlag |= TRUE;
    }
    else if ((PrevOutputFrame_p->InputVideoInfo.Width != CurrentOutputFrame_p->InputVideoInfo.Width)   ||
             (PrevOutputFrame_p->InputVideoInfo.Height != CurrentOutputFrame_p->InputVideoInfo.Height) ||
             (PrevOutputFrame_p->OutputVideoInfo.Width != CurrentOutputFrame_p->OutputVideoInfo.Width) ||
             (PrevOutputFrame_p->OutputVideoInfo.Height != CurrentOutputFrame_p->OutputVideoInfo.Height) ||
             (PrevOutputFrame_p->InputVideoInfo.ColorSpace != CurrentOutputFrame_p->InputVideoInfo.ColorSpace))
    {
        LiteScalerUpdadeFlag |= TRUE;
    }

    ScalerConfig.CheckDatapathLite = HandleContext_p->DataPath.DatapathLite;
    ScalerConfig.ProcessingRGB = (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC);

    //Input Vertical lite scaline need to set only for shrink case
    if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VZOOM)
    || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VZOOM))
    {
        // Setup parameters of vertical scaling
        ScalerConfig.InputFormat.ColorSampling = CurrentOutputFrame_p->InputVideoInfo.ColorSampling;
        ScalerConfig.InputFormat.ColorSpace = CurrentOutputFrame_p->InputVideoInfo.ColorSpace;
        ScalerConfig.InputFormat.ScanType = CurrentOutputFrame_p->InputVideoInfo.ScanType;
        ScalerConfig.InputFormat.FieldType = CurrentOutputFrame_p->InputVideoInfo.FieldType;
        ScalerConfig.InputFormat.HWidth = CurrentOutputFrame_p->InputVideoInfo.Width;
        ScalerConfig.InputFormat.VHeight = CurrentOutputFrame_p->InputVideoInfo.Height;
        ScalerConfig.OutputFormat.ScanType = CurrentOutputFrame_p->OutputVideoInfo.ScanType;
        ScalerConfig.OutputFormat.FieldType = CurrentOutputFrame_p->OutputVideoInfo.FieldType;
        ScalerConfig.OutputFormat.HWidth = CurrentOutputFrame_p->OutputVideoInfo.Width;
        ScalerConfig.OutputFormat.VHeight = CurrentOutputFrame_p->OutputVideoInfo.Height;

        VScaler_Lite_OffsetUpdate(CH, ScalerConfig);

        // Treat the "interlace in to interlace out" case same as "progressive in to progressive out"
        if((ScalerConfig.InputFormat.ScanType == INTERLACED)
           && (ScalerConfig.OutputFormat.ScanType == INTERLACED))
        {
            ScalerConfig.InputFormat.ScanType = PROGRESSIVE;
            ScalerConfig.OutputFormat.ScanType = PROGRESSIVE;
        }

        // Configure the Horizontal  Scaler
        if (LiteScalerUpdadeFlag)
        {
            VScaler_Lite_HWConfig(CH, ScalerConfig);

            if(CurrentOutputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {
                VScaler_Lite_DcdiUpdate(CH, TRUE, 0x4);
            }
            else
            {
                VScaler_Lite_DcdiUpdate(CH, FALSE, 0);
            }
        }
    }

    //Input Horizontal lite scaline need to set only for shrink case
    if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VZOOM)
    || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VSHRINK))
    {
        // Setup parameters of horizontal scaling
        ScalerConfig.InputFormat.ColorSampling = CurrentOutputFrame_p->InputVideoInfo.ColorSampling;
        ScalerConfig.InputFormat.HWidth = CurrentOutputFrame_p->InputVideoInfo.Width;
        ScalerConfig.OutputFormat.HWidth = CurrentOutputFrame_p->OutputVideoInfo.Width;
        ScalerConfig.InputFormat.VHeight = CurrentOutputFrame_p->InputVideoInfo.Height;
        ScalerConfig.OutputFormat.VHeight = CurrentOutputFrame_p->OutputVideoInfo.Height;

        // Configure the Horizontal  Scaler
        if (LiteScalerUpdadeFlag)
        {
            HScaler_Lite_HWConfig(CH, ScalerConfig);
        }
    }

    CurrentOutputFrame_p->InputVideoInfo.Width = CurrentOutputFrame_p->OutputVideoInfo.Width;
    CurrentOutputFrame_p->InputVideoInfo.Height = CurrentOutputFrame_p->OutputVideoInfo.Height;

    return ErrorCode;
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
static FVDP_Result_t SYSTEM_LITE_ConfigureEventCtrl(FVDP_HandleContext_t* HandleContext_p)
{
    uint32_t Position;
    bool HShrink = FALSE;
    bool VShrink = FALSE;
    FVDP_CH_t CH = HandleContext_p->CH;

    Position = HandleContext_p->OutputRasterInfo.VStart + HandleContext_p->OutputRasterInfo.VActive;

    if(Position > (HandleContext_p->OutputRasterInfo.VTotal - 1))
    {
        return FVDP_ERROR;
    }

    if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VZOOM)
    || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VSHRINK))
    {
        HShrink = TRUE;
    }

    //Input Vertical lite scaline need to set only for shrink case
    if((HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HZOOM_VSHRINK)
    || (HandleContext_p->DataPath.DatapathLite == DATAPATH_LITE_HSHRINK_VSHRINK))
    {
        VShrink = TRUE;
    }

    // Set the Frame Reset Event line number
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_1_IN_LITE , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_2_IN_LITE , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_3_IN_LITE , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_4_IN_LITE , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_1_OUT_LITE, (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_2_OUT_LITE, (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_3_OUT_LITE, (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VIDEO_4_OUT_LITE, (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_TNR_LITE        , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_CCTRL_LITE      , (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_BDR_LITE        , (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_ZOOMFIFO_LITE   , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_MCC_WRITE_LITE  , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_MCC_READ_LITE   , (Position + 1), DISPLAY_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_INPUT_IRQ_LITE  , FRAME_RESET_POS, INPUT_SOURCE);
    EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_DISPLAY_IRQ_LITE, (Position + 1), DISPLAY_SOURCE);

    // Set the Sync Update Event line number
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_COMMCTRL_BLK_LITE, UPDATE_EVENT_POS, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_TNR_BLK_LITE     , UPDATE_EVENT_POS, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_CCTRL_BLK_LITE   , Position, DISPLAY_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_BDR_BLK_LITE     , Position, DISPLAY_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_ZOOMFIFO_BLK_LITE, UPDATE_EVENT_POS, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_MCC_IP_BLK_LITE  , UPDATE_EVENT_POS, INPUT_SOURCE);
    EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_MCC_OP_BLK_LITE  , Position, DISPLAY_SOURCE);


    //Reprogram Frame Reset and Updata Event Source
    if(HShrink == TRUE)
    {
        EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_HFLITE_LITE  , FRAME_RESET_POS, INPUT_SOURCE);
        EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_HFLITE_BLK_LITE  , UPDATE_EVENT_POS, INPUT_SOURCE);
    }
    else
    {
        EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_HFLITE_LITE  , (Position + 1), DISPLAY_SOURCE);
        EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_HFLITE_BLK_LITE  , Position, DISPLAY_SOURCE);
    }

    if(VShrink == TRUE)
    {
        EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VFLITE_LITE  , FRAME_RESET_POS, INPUT_SOURCE);
        EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_VFLITE_BLK_LITE  , UPDATE_EVENT_POS, INPUT_SOURCE);
    }
    else
    {
        EventCtrl_SetFrameReset_LITE(CH, FRAME_RESET_VFLITE_LITE  , (Position + 1), DISPLAY_SOURCE);
        EventCtrl_SetSyncEvent_LITE(CH, EVENT_ID_VFLITE_BLK_LITE  , Position, DISPLAY_SOURCE);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_LITE_PowerDown
Description : Power down FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_LITE_PowerDown(FVDP_CH_t CH)
{

    if((CH != FVDP_PIP)&& (CH != FVDP_AUX))
    {
        return FVDP_ERROR;
    }

    // Disable the MCC client
    if(CH == FVDP_PIP)
    {
        MCC_Disable(PIP_RGB_UV_WR);
        MCC_Disable(PIP_Y_WR);
        MCC_Disable(PIP_RGB_UV_RD);
        MCC_Disable(PIP_Y_RD);
    }
    if(CH == FVDP_AUX)
    {
        MCC_Disable(AUX_RGB_UV_WR);
        MCC_Disable(AUX_Y_WR);
        MCC_Disable(AUX_RGB_UV_RD);
        MCC_Disable(AUX_Y_RD);
     }

    // Disable IP blocks

    // Disable clock of IP blocks
    EventCtrl_ClockCtrl_LITE(CH, MODULE_CLK_ALL_LITE, GND_CLK);

    HostUpdate_HardwareUpdate(CH, HOST_CTRL_LITE, FORCE_UPDATE);

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_LITE_PowerUp
Description : Power down FVDP_LITE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_LITE_PowerUp(FVDP_CH_t CH)
{
    FVDP_HandleContext_t* HandleContext_p;

    HandleContext_p = SYSTEM_GetHandleContextOfChannel(CH);

    // PIP or AUX Channel has been never opened before
    if (HandleContext_p == NULL)
    {
        return FVDP_OK;
    }

    //Set Hard reset LITE block
    EventCtrl_HardReset_LITE(CH, HARD_RESET_ALL_LITE, TRUE);

    //INIT local variables

    //Clear Hard reset LITE block
    EventCtrl_HardReset_LITE(CH, HARD_RESET_ALL_LITE, FALSE);

    // Initialize the VMUX HW
    SYSTEM_VMUX_Init(CH);

    //Initialize FVDP_PIP/FVDP_AUX software module and call FVDP_PIP/FVDP_AUX hardware init function
    SYSTEM_LITE_Init(HandleContext_p);

    // Initialize the MCC HW
    MCC_Init(CH);

    HostUpdate_HardwareUpdate(CH, HOST_CTRL_LITE, FORCE_UPDATE);

    return FVDP_OK;
}
