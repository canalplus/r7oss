/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_update.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_system.h"
#include "fvdp_update.h"
#include "fvdp_hostupdate.h"
#include "fvdp_vcd.h"
#include "fvdp_driver.h"

extern FVDP_HW_Config_t HW_Config[NUM_FVDP_CH];
/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t UPDATE_InputUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_HandleContext_t* HandleContext_p;
    HostCtrlBlock_t       HostCtrlBlock;
    FVDP_Result_t         ErrorCode = FVDP_OK;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_INPUT_UPDATE, 1);

    // Host Update for FVDP_FE or FVDP_LITE depending on CH
    if (HandleContext_p->CH == FVDP_MAIN)
    {
        ErrorCode = SYSTEM_FE_ProcessInput(HandleContext_p);
        HostCtrlBlock = HOST_CTRL_FE;
    }
    else if ((HandleContext_p->CH == FVDP_PIP) || (HandleContext_p->CH == FVDP_AUX))
    {
        ErrorCode = SYSTEM_LITE_ProcessInput(HandleContext_p);
        HostCtrlBlock = HOST_CTRL_LITE;
    }
    else //FVDP_ENC
    {
         //ErrorCode = SYSTEM_ENC_ProcessInput(HandleContext_p);
         HostCtrlBlock = HOST_CTRL_LITE;
    }

    // Host Update for FVDP_VMUX
    HostUpdate_HardwareUpdate(HandleContext_p->CH,
                              HOST_CTRL_VMUX,
                              SYNC_UPDATE);

    HostUpdate_HardwareUpdate(HandleContext_p->CH,
                              HostCtrlBlock,
                              SYNC_UPDATE);

    // Clear the Input Update Flag
    HandleContext_p->UpdateFlags.InputUpdateFlag = 0;
    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_INPUT_UPDATE, 0);
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
FVDP_Result_t UPDATE_OutputUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_HandleContext_t* HandleContext_p;
    HostCtrlBlock_t       HostCtrlBlock;
    FVDP_Result_t         ErrorCode = FVDP_OK;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_OUTPUT_UPDATE, 1);

    // Host Update for FVDP_BE or FVDP_LITE depending on CH
    if (HandleContext_p->CH == FVDP_MAIN)
    {
        ErrorCode = SYSTEM_BE_ProcessOutput(HandleContext_p);
        HostCtrlBlock = HOST_CTRL_BE;
    }
    else if ((HandleContext_p->CH == FVDP_PIP) || (HandleContext_p->CH == FVDP_AUX))
    {
        ErrorCode = SYSTEM_LITE_ProcessOutput(HandleContext_p);
        HostCtrlBlock = HOST_CTRL_LITE;
    }
    else //FVDP_ENC
    {
         //ErrorCode = SYSTEM_ENC_ProcessInput(HandleContext_p);
         HostCtrlBlock = HOST_CTRL_LITE;
    }

    HostUpdate_HardwareUpdate(HandleContext_p->CH,
                              HostCtrlBlock,
                              SYNC_UPDATE);

    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_OUTPUT_UPDATE, 0);
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
FVDP_Result_t UPDATE_CommitUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_HandleContext_t* HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    TRC( TRC_ID_MAIN_INFO, "WARNING: Commit Update not yet supported on MPE42." );
    HandleContext_p->UpdateFlags.CommitUpdateFlag = UPDATE_INPUT_PSI | UPDATE_OUTPUT_PSI;

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
FVDP_Result_t UPDATE_SetInputSource(FVDP_CH_Handle_t FvdpChHandle,
                                    FVDP_Input_t     Input)
{
    FVDP_HandleContext_t* HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    HandleContext_p->InputSource = Input;
    HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_SOURCE;

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
FVDP_Result_t UPDATE_SetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                       FVDP_VideoInfo_t* NextInputVideoInfo_p)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;
    FVDP_Frame_t*         NextFrame_p;
    FVDP_CH_t             CH;

    if (NextInputVideoInfo_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    CH = HandleContext_p->CH;
    // Get the Current and Next Frame information
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    NextFrame_p = SYSTEM_GetNextInputFrame(HandleContext_p);
    // Check if the Next Frame is not yet allocated (this should not happen)
    if (NextFrame_p == NULL)
    {
        //Buffer overflow is detected, flush buffer
        SYSTEM_FlushChannel(FvdpChHandle, FALSE);
        TRC( TRC_ID_MAIN_INFO, "ERROR:  NextInputFrame is NULL.");
        return (FVDP_ERROR);
    }

    // A Current Frame does not exist yet, so the NextInputVideoInfo should be applied.
    if (CurrFrame_p == NULL)
    {
        NextFrame_p->InputVideoInfo = *NextInputVideoInfo_p;
        HW_Config[CH].HW_IVPConfig.InputVideoInfo = *NextInputVideoInfo_p;
        HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_VIDEO_INFO;
        HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_VIDEO_FIELDTYPE;
    }
    else
    {
        // Check if there is any change to the InputVideoInfo compared to Current Frame
        if (   (HW_Config[CH].HW_IVPConfig.InputVideoInfo.ScanType != NextInputVideoInfo_p->ScanType)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.ColorSpace != NextInputVideoInfo_p->ColorSpace)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.ColorSampling != NextInputVideoInfo_p->ColorSampling)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.FrameRate != NextInputVideoInfo_p->FrameRate)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.Width != NextInputVideoInfo_p->Width)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.Height != NextInputVideoInfo_p->Height)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.AspectRatio != NextInputVideoInfo_p->AspectRatio)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.FVDP3DFormat != NextInputVideoInfo_p->FVDP3DFormat)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.FVDP3DFlag != NextInputVideoInfo_p->FVDP3DFlag)
            || (HW_Config[CH].HW_IVPConfig.InputVideoInfo.FVDP3DSubsampleMode != NextInputVideoInfo_p->FVDP3DSubsampleMode))
        {
            // Set the Next Input Video Info into the Next Frame
            NextFrame_p->InputVideoInfo = *NextInputVideoInfo_p;
            HW_Config[CH].HW_IVPConfig.InputVideoInfo = *NextInputVideoInfo_p;
            HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_VIDEO_INFO;
        }
        else
        {
            // Set FrameID important for setting picture metadata in FVDP caller
            NextFrame_p->InputVideoInfo.FrameID = NextInputVideoInfo_p->FrameID;
            HW_Config[CH].HW_IVPConfig.InputVideoInfo.FrameID = NextInputVideoInfo_p->FrameID;
        }

        // Check if the Field polarity type has changed
        if (HW_Config[CH].HW_IVPConfig.InputVideoInfo.FieldType != NextInputVideoInfo_p->FieldType)
        {
            NextFrame_p->InputVideoInfo.FieldType = NextInputVideoInfo_p->FieldType;
            HW_Config[CH].HW_IVPConfig.InputVideoInfo.FieldType = NextInputVideoInfo_p->FieldType;
            HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_VIDEO_FIELDTYPE;
        }
    }
    NextFrame_p->InputVideoInfo.FVDPDisplayModeFlag = NextInputVideoInfo_p->FVDPDisplayModeFlag;
    HW_Config[CH].HW_IVPConfig.InputVideoInfo.FVDPDisplayModeFlag = NextInputVideoInfo_p->FVDPDisplayModeFlag;

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
FVDP_Result_t UPDATE_SetInputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_RasterInfo_t InputRasterInfo)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_CH_t             CH;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
        return FVDP_ERROR;

    CH = HandleContext_p->CH;
    // Check for any changes in the Input Raster Information
    if (   (HW_Config[CH].HW_IVPConfig.InputRasterInfo.HStart != InputRasterInfo.HStart)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.VStart != InputRasterInfo.VStart)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.HTotal != InputRasterInfo.HTotal)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.VTotal != InputRasterInfo.VTotal)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.HActive != InputRasterInfo.HActive)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.VActive != InputRasterInfo.VActive)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.UVAlign != InputRasterInfo.UVAlign)
        || (HW_Config[CH].HW_IVPConfig.InputRasterInfo.FieldAlign !=InputRasterInfo.FieldAlign))
    {
        // There is a change, so copy the data and set the update flag.
        HandleContext_p->InputRasterInfo = InputRasterInfo;
        HW_Config[CH].HW_IVPConfig.InputRasterInfo = InputRasterInfo;
        HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_RASTER_INFO;
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
FVDP_Result_t UPDATE_SetOutputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_VideoInfo_t* NextOutputVideoInfo_p)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;
    FVDP_Frame_t*         NextFrame_p;

    if (NextOutputVideoInfo_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    // Get the Current and Next Frame information
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    NextFrame_p = SYSTEM_GetNextInputFrame(HandleContext_p);
    // Check if the Next Frame is not yet allocated (this should not happen)
    if (NextFrame_p == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  NextInputFrame is NULL." );
        return (FVDP_ERROR);
    }

    // A Current Frame does not exist yet, so the NextOutputVideoInfo should be applied.
    if (CurrFrame_p == NULL)
    {
        NextFrame_p->OutputVideoInfo = *NextOutputVideoInfo_p;
        HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_OUTPUT_VIDEO_INFO;
    }
    else
    {
        // Check if there is any change to the OutputVideoInfo compared to Current Frame
        if (   (CurrFrame_p->OutputVideoInfo.ScanType != NextOutputVideoInfo_p->ScanType)
            || (CurrFrame_p->OutputVideoInfo.FieldType != NextOutputVideoInfo_p->FieldType)
            || (CurrFrame_p->OutputVideoInfo.ColorSpace != NextOutputVideoInfo_p->ColorSpace)
            || (CurrFrame_p->OutputVideoInfo.ColorSampling != NextOutputVideoInfo_p->ColorSampling)
            || (CurrFrame_p->OutputVideoInfo.FrameRate != NextOutputVideoInfo_p->FrameRate)
            || (CurrFrame_p->OutputVideoInfo.Width != NextOutputVideoInfo_p->Width)
            || (CurrFrame_p->OutputVideoInfo.Height != NextOutputVideoInfo_p->Height)
            || (CurrFrame_p->OutputVideoInfo.AspectRatio != NextOutputVideoInfo_p->AspectRatio)
            || (CurrFrame_p->OutputVideoInfo.FVDP3DFormat != NextOutputVideoInfo_p->FVDP3DFormat)
            || (CurrFrame_p->OutputVideoInfo.FVDP3DFlag != NextOutputVideoInfo_p->FVDP3DFlag)
            || (CurrFrame_p->OutputVideoInfo.FVDP3DSubsampleMode != NextOutputVideoInfo_p->FVDP3DSubsampleMode))
        {
            // Set the Next Input Video Info into the Next Frame
            NextFrame_p->OutputVideoInfo = *NextOutputVideoInfo_p;
            HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_OUTPUT_VIDEO_INFO;
        }
        else
        {
            // Set FrameID important for setting picture metadata in FVDP caller
            NextFrame_p->OutputVideoInfo.FrameID = NextOutputVideoInfo_p->FrameID;
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
FVDP_Result_t UPDATE_SetOutputWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_OutputWindow_t *NextOutputActiveWindow_p)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;
    FVDP_Frame_t*         NextFrame_p;
    if (NextOutputActiveWindow_p == NULL)
    {
        return (FVDP_ERROR);
    }
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    NextFrame_p = SYSTEM_GetNextInputFrame(HandleContext_p);
    if (NextFrame_p == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  NextInputFrame is NULL." );
        return (FVDP_ERROR);
    }

    // A Current Frame does not exist yet, so the NextOutputActiveWindow should be applied.
    if (CurrFrame_p == NULL)
    {
        NextFrame_p->OutputActiveWindow = *NextOutputActiveWindow_p;
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_ACTIVE_WINDOW;
    }
    else
    {
        if((CurrFrame_p->OutputActiveWindow.HStart != NextOutputActiveWindow_p->HStart)
        || (CurrFrame_p->OutputActiveWindow.HWidth != NextOutputActiveWindow_p->HWidth)
        || (CurrFrame_p->OutputActiveWindow.VStart != NextOutputActiveWindow_p->VStart)
        || (CurrFrame_p->OutputActiveWindow.VHeight!= NextOutputActiveWindow_p->VHeight))
        {
            NextFrame_p->OutputActiveWindow = *NextOutputActiveWindow_p;
            HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_ACTIVE_WINDOW;
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
FVDP_Result_t UPDATE_SetCropWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                   FVDP_CropWindow_t NextCropWindow)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;
    FVDP_Frame_t*         NextFrame_p;
    FVDP_CH_t             CH;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    CH = HandleContext_p->CH;
    // Get the Current and Next Frame information
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    NextFrame_p = SYSTEM_GetNextInputFrame(HandleContext_p);
    // Check if the Next Frame is not yet allocated (this should not happen)
    if (NextFrame_p == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  NextInputFrame is NULL." );
        return (FVDP_ERROR);
    }

    // A Current Frame does not exist yet, so the NextOutputVideoInfo should be applied.
    if (CurrFrame_p == NULL)
    {
        NextFrame_p->CropWindow = NextCropWindow;
        HW_Config[CH].HW_IVPConfig.CropWindow = NextCropWindow;
        HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_CROP_WINDOW;
    }
    else
    {
        // Check for any changes in Crop Window compared with the Current Frame
        if (   (HW_Config[CH].HW_IVPConfig.CropWindow.HCropStart != NextCropWindow.HCropStart) // Integer horizontal start position
            || (HW_Config[CH].HW_IVPConfig.CropWindow.HCropStartOffset != NextCropWindow.HCropStartOffset) // Fractional start postion, in 1/32 step increment
            || (HW_Config[CH].HW_IVPConfig.CropWindow.HCropWidth != NextCropWindow.HCropWidth) // Crop window width in number of pixel
            || (HW_Config[CH].HW_IVPConfig.CropWindow.VCropStart != NextCropWindow.VCropStart) // Integer vertical start position
            || (HW_Config[CH].HW_IVPConfig.CropWindow.VCropStartOffset != NextCropWindow.VCropStartOffset) // Fractional start postion, in 1/32 step increment
            || (HW_Config[CH].HW_IVPConfig.CropWindow.VCropHeight != NextCropWindow.VCropHeight))// Crop window height in number of line
        {
            NextFrame_p->CropWindow = NextCropWindow;
            HW_Config[CH].HW_IVPConfig.CropWindow = NextCropWindow;
            HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_CROP_WINDOW;
        }
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_SetPSIState
Description : Set PSI state information
Parameters  : [in] CH Channel to be applied.
              [in] PSI state.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_SetPSIState(FVDP_CH_Handle_t FvdpChHandle,
                                       FVDP_PSIFeature_t   PSIFeature,
                                       FVDP_PSIState_t     PSIState)
{
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    HandleContext_p->PSIState[PSIFeature] = PSIState;

    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_GetPSIState
Description : Get PSI state information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [out] PSI state.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK: No errors
              FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_GetPSIState(FVDP_CH_Handle_t FvdpChHandle,
                                       FVDP_PSIFeature_t   PSIFeature,
                                       FVDP_PSIState_t     *PSIState)
{
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    *PSIState = HandleContext_p->PSIState[PSIFeature];

    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_SetLatencyControl
Description : Update_SetLatencyControl() sets the control level specified Latency control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_SetLatencyType(FVDP_CH_Handle_t  FvdpChHandle,
                                   uint32_t           Value)
{

    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

//    vibe_printf("DID : Update_SetLatencyType : Current MAIN latency mode is %d \n", HandleContext_p->LatencyType);
    switch(Value)
    {
        case(PLANE_LATENCY_CONSTANT) :
            HandleContext_p->LatencyType = FVDP_CONSTANT_LATENCY;
            break;
        case(PLANE_LATENCY_FE_ONLY) :
            HandleContext_p->LatencyType = FVDP_FE_ONLY_LATENCY;
            break;
        case(PLANE_LATENCY_LOW) :
            HandleContext_p->LatencyType = FVDP_MINIMUM_LATENCY;
            break;
        default :
            ErrorCode = FVDP_ERROR;
            break;
    }
//    vibe_printf("DID : Update_SetLatencyType :  New MAIN latency mode is  = %d \n", HandleContext_p->LatencyType);
    return ErrorCode;

}

/*******************************************************************************
Name        : UPDATE_GetLatencyControl
Description : Update_GetLatencyControl() returns the current activecontrol level
                  for specified Latency control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_GetLatencyType(FVDP_CH_Handle_t  FvdpChHandle,
                                   uint32_t*          Value)
{
    FVDP_HandleContext_t * HandleContext_p;
    FVDP_Result_t ErrorCode = FVDP_OK;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    switch(HandleContext_p->LatencyType)
    {
        case(FVDP_CONSTANT_LATENCY) :
            *Value = PLANE_LATENCY_CONSTANT;
            break;
        case(FVDP_FE_ONLY_LATENCY) :
            *Value = PLANE_LATENCY_FE_ONLY;
            break;
        case(FVDP_MINIMUM_LATENCY) :
            *Value = PLANE_LATENCY_LOW;
            break;
        default :
            ErrorCode = FVDP_ERROR;
            break;
    }

//    vibe_printf("DID : Update_GetLatencyType : MAIN latency mode = %d \n", *Value);
    return ErrorCode;
}

/*******************************************************************************
Name        : UPDATE_SetPSIControl
Description : Update_SetPSIControl() sets the control level specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIControl Select Control to set
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_SetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                   FVDP_PSIControl_t PSIControl,
                                   int32_t           Value)
{

    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }
    /*Set update value */
    switch (PSIControl)
    {
        case FVDP_SHARPNESS_LEVEL:
            if(HandleContext_p->PSIControlLevel.SharpnessLevel != Value)
            {
                HandleContext_p->PSIControlLevel.SharpnessLevel = Value;
            }
            break;
        case FVDP_BRIGHTNESS:
            if(HandleContext_p->PSIControlLevel.BrightnessLevel != Value)
            {
                HandleContext_p->PSIControlLevel.BrightnessLevel = Value;
            }
            break;
        case FVDP_CONTRAST:
            if(HandleContext_p->PSIControlLevel.ContrastLevel != Value)
            {
                HandleContext_p->PSIControlLevel.ContrastLevel = Value;
            }
            break;
        case FVDP_SATURATION:
            if(HandleContext_p->PSIControlLevel.SaturationLevel != Value)
            {
                HandleContext_p->PSIControlLevel.SaturationLevel = Value;
            }
            break;
        case FVDP_HUE:
            if(HandleContext_p->PSIControlLevel.HueLevel != Value)
            {
                HandleContext_p->PSIControlLevel.HueLevel = Value;
            }
            break;
        case FVDP_RED_GAIN:
            if(HandleContext_p->PSIControlLevel.RedGain != Value)
            {
                HandleContext_p->PSIControlLevel.RedGain = Value;
            }
            break;
        case FVDP_GREEN_GAIN:
            if(HandleContext_p->PSIControlLevel.GreenGain != Value)
            {
                HandleContext_p->PSIControlLevel.GreenGain = Value;
            }
            break;
        case FVDP_BLUE_GAIN:
            if(HandleContext_p->PSIControlLevel.BlueGain != Value)
            {
                HandleContext_p->PSIControlLevel.BlueGain = Value;
            }
            break;
        case FVDP_RED_OFFSET:
            if(HandleContext_p->PSIControlLevel.RedOffset != Value)
            {
                HandleContext_p->PSIControlLevel.RedOffset = Value;
            }
            break;
        case FVDP_GREEN_OFFSET:
            if(HandleContext_p->PSIControlLevel.GreenOffset != Value)
            {
                HandleContext_p->PSIControlLevel.GreenOffset = Value;
            }
            break;
        case FVDP_BLUE_OFFSET:
            if(HandleContext_p->PSIControlLevel.BlueOffset != Value)
            {
                HandleContext_p->PSIControlLevel.BlueOffset = Value;
            }
            break;
        case FVDP_3D_DEPTH:
            if(HandleContext_p->PSIControlLevel.Depth_3D != Value)
            {
                HandleContext_p->PSIControlLevel.Depth_3D = Value;
            }
            break;
        default:
            ErrorCode = FVDP_ERROR;
            break;
    }
    return ErrorCode;
}

/*******************************************************************************
Name        : UPDATE_GetPSIControl
Description : Update_GetPSIControl() returns the current activecontrol level
                  for specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIControl Select Control to set
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_GetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                   FVDP_PSIControl_t PSIControl,
                                   int32_t*          Value)
{
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }
    /* Get the current active value */
    switch (PSIControl)
    {
        case FVDP_SHARPNESS_LEVEL:
            *Value = HandleContext_p->PSIControlLevel.SharpnessLevel;
            break;
        case FVDP_BRIGHTNESS:
            *Value = HandleContext_p->PSIControlLevel.BrightnessLevel;
            break;
        case FVDP_CONTRAST:
            *Value = HandleContext_p->PSIControlLevel.ContrastLevel;
            break;
        case FVDP_SATURATION:
            *Value = HandleContext_p->PSIControlLevel.SaturationLevel;
            break;
        case FVDP_HUE:
            *Value = HandleContext_p->PSIControlLevel.HueLevel;
            break;
        case FVDP_RED_GAIN:
            *Value = HandleContext_p->PSIControlLevel.RedGain;
            break;
        case FVDP_GREEN_GAIN:
            *Value = HandleContext_p->PSIControlLevel.GreenGain;
            break;
        case FVDP_BLUE_GAIN:
            *Value = HandleContext_p->PSIControlLevel.BlueGain;
            break;
        case FVDP_RED_OFFSET:
            *Value = HandleContext_p->PSIControlLevel.RedOffset;
            break;
        case FVDP_GREEN_OFFSET:
            *Value = HandleContext_p->PSIControlLevel.GreenOffset;
            break;
        case FVDP_BLUE_OFFSET:
            *Value = HandleContext_p->PSIControlLevel.BlueOffset;
            break;
        default:
            return FVDP_ERROR;
            break;
    }
    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_GetPSIControlsRange
Description : returns the minimim and maximum level for specified PSI control.
Parameters :[in] FvdpChHandle Handle to an FVDP Channel
            [in] PSIControlsSelect Selects the Control Range which is requested.
            [out] pMin Pointer to Min Value for the requested Control.
            [out] pMax Pointer to Max Value for the requested Control
            [out] pDefault Pointer to Default Value for the requested Control
            [out] pStep Pointer to Step Value for the requested Control
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_GetPSIControlsRange(FVDP_CH_Handle_t   FvdpChHandle,
                                       FVDP_PSIControl_t  PSISel,
                                       int32_t *pMin,
                                       int32_t *pMax,
                                       int32_t *pDefault,
                                       int32_t *pStep)
{
   FVDP_Result_t ErrorCode = FVDP_OK;
   FVDP_HandleContext_t * HandleContext_p;

   // Get the FVDP Context pointer for this handle
   HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
   if (HandleContext_p == NULL)
   {
       return (FVDP_ERROR);
   }

    switch (PSISel)
    {
        case FVDP_BRIGHTNESS:
            *pDefault = DEFAULT_BRIGHTNESS;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_CONTRAST:
            *pDefault = DEFAULT_CONTRAST;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_SATURATION:
            *pDefault = DEFAULT_SATURATION;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_HUE:
            *pDefault = DEFAULT_HUE;
           // ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_RED_GAIN:
            *pDefault = DEFAULT_REDGAIN;
           // ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_GREEN_GAIN:
            *pDefault = DEFAULT_GREENGAIN;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_BLUE_GAIN:
            *pDefault = DEFAULT_BLUEGAIN;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_RED_OFFSET:
            *pDefault = DEFAULT_REDOFFSET;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_GREEN_OFFSET:
            *pDefault = DEFAULT_GREENOFFSET;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_BLUE_OFFSET:
            *pDefault = DEFAULT_BLUEOFFSET;
            //ErrorCode = OCSC_GetColorRange(PSISel, pMin, pMax, pStep);
            break;
        case FVDP_SHARPNESS_LEVEL:
            *pDefault = DEFAULT_SHARPNESS;
            ErrorCode = SYSTEM_PSI_Sharpness_ControlRange(HandleContext_p->CH, pMin, pMax, pStep);
            break;

       /* TODO:Range for 3D depth */
        case FVDP_3D_DEPTH:
            *pMin = 0;
            *pMax = 0;
            break;

        default:
            ErrorCode = FVDP_ERROR;
            break;
    }
    return ErrorCode;
}

/*******************************************************************************
Name        : UPDATE_SetPSITuningData
Description : UPDATE_SetPSITuningData() writes to hardware psi data selected by PSIFeature

Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIFeature Selects which PSI data is requested.
              [in] PSITuningData Pointer to PSI data structure.
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_SetPSITuningData (FVDP_CH_Handle_t FvdpChHandle,
                                     FVDP_PSIFeature_t       PSIFeature,
                                     FVDP_PSITuningData_t    *PSITuningData)
{
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    if(PSITuningData == NULL)
    {
        return(FVDP_ERROR);
    }

    switch(PSIFeature)
    {
        case FVDP_IP_CSC:
            if(PSITuningData->Signature != FVDP_IP_CSC)
            {
                return(FVDP_ERROR);
            }
            if (PSITuningData->Size != sizeof(VQ_COLOR_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_IP_CSC;
            HandleContext_p->PSIFeature.IPCSCVQData=
                 *(VQ_COLOR_Parameters_t *) PSITuningData;
            break;

        case FVDP_ACC:
            if(PSITuningData->Signature != FVDP_ACC)
            {
                return(FVDP_ERROR);
            }
            if (PSITuningData->Size != sizeof(VQ_ACC_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_ACC;
            HandleContext_p->PSIFeature.ACCVQData=
                *(VQ_ACC_Parameters_t *) PSITuningData;
            break;

        case FVDP_AFM:
            if(PSITuningData->Signature != FVDP_AFM)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_AFM_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_AFM;
            HandleContext_p->PSIFeature.AFMVQData=
                *(VQ_AFM_Parameters_t *) PSITuningData;
            break;

        case FVDP_MADI:
            if(PSITuningData->Signature != FVDP_MADI)
            {
                return(FVDP_ERROR);
            }
            if (PSITuningData->Size != sizeof(VQ_MADI_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_MADI;
            HandleContext_p->PSIFeature.MADIVQData=
                *(VQ_MADI_Parameters_t *) PSITuningData;
            break;

        case FVDP_CCS:
            if(PSITuningData->Signature != FVDP_CCS)
            {
                return(FVDP_ERROR);
            }
            if (PSITuningData->Size != sizeof(VQ_CCS_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_CCS;
            HandleContext_p->PSIFeature.CCSVQData=
                *(VQ_CCS_Parameters_t *) PSITuningData;
            break;

        case FVDP_DNR:
            if(PSITuningData->Signature != FVDP_DNR)
            {
                return(FVDP_ERROR);
            }
            if (PSITuningData->Size != sizeof(VQ_DNR_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_DNR;
            HandleContext_p->PSIFeature.DNRVQData=
                *(VQ_DNR_Parameters_t *) PSITuningData;
            break;

        case FVDP_MNR:
            if(PSITuningData->Signature != FVDP_MNR)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_MNR_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_MNR;
            HandleContext_p->PSIFeature.MNRVQData=
                *(VQ_MNR_Parameters_t *) PSITuningData;
            break;

        case FVDP_TNR:
            if(PSITuningData->Signature != FVDP_TNR)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_TNR_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_TNR;
            HandleContext_p->PSIFeature.TNRVQData=
                *(VQ_TNR_Parameters_t *) PSITuningData;
            break;

        case FVDP_SCALER:
            if(PSITuningData->Signature != FVDP_SCALER)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_SCALER_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_SCALER;
            HandleContext_p->PSIFeature.ScalerVQData=
                *(VQ_SCALER_Parameters_t *) PSITuningData;
           break;

        case FVDP_SHARPNESS:
            if(PSITuningData->Signature != FVDP_SHARPNESS)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_SHARPNESS_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_SHARPNESS;
            HandleContext_p->PSIFeature.SharpnessVQData=
                *(VQ_SHARPNESS_Parameters_t *) PSITuningData;
            break;

        case FVDP_ACM:
            if(PSITuningData->Signature != FVDP_ACM)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_ACM_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_ACM;
            HandleContext_p->PSIFeature.ACMVQData=
                *(VQ_ACM_Parameters_t *) PSITuningData;
            break;

        case FVDP_OP_CSC:
            if(PSITuningData->Signature != FVDP_OP_CSC)
            {
                return(FVDP_ERROR);
            }

            if (PSITuningData->Size != sizeof(VQ_COLOR_Parameters_t))
            {
                return(FVDP_ERROR);
            }

            HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_OP_CSC;
            HandleContext_p->PSIFeature.OPCSCVQData=
                *(VQ_COLOR_Parameters_t *) PSITuningData;
            break;
        default:
            return(FVDP_ERROR);
    }
    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_GetPSITuningData
Description : UPDATE_GetPSITuningData() returns psi data selected by PSIFeature

Parameters  : [in] CH channel for which capabilities is being requested
              [in] PSIFeature Selects which PSI data is requested.
              [out] PSITuningData Pointer to PSI data structure.

Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t UPDATE_GetPSITuningData(FVDP_CH_Handle_t FvdpChHandle,
                                    FVDP_PSIFeature_t       PSIFeature,
                                    FVDP_PSITuningData_t    *PSITuningData)
{
    FVDP_HandleContext_t * HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    if(PSITuningData == NULL)
    {
        return(FVDP_ERROR);
    }

    switch(PSIFeature)
    {
        case FVDP_IP_CSC:
                *(VQ_COLOR_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.IPCSCVQData;
            break;

        case FVDP_ACC:
                *(VQ_ACC_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.ACCVQData;
            break;

        case FVDP_AFM:
                *(VQ_AFM_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.AFMVQData;
            break;

        case FVDP_MADI:
                *(VQ_MADI_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.MADIVQData;
            break;

         case FVDP_CCS:
                *(VQ_CCS_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.CCSVQData;
            break;

        case FVDP_DNR:
                *(VQ_DNR_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.DNRVQData;
            break;

        case FVDP_MNR:
                *(VQ_MNR_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.MNRVQData;
            break;

        case FVDP_TNR:
                *(VQ_TNR_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.TNRVQData;
            break;

        case FVDP_SCALER:
                *(VQ_SCALER_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.ScalerVQData;
            break;

        case FVDP_SHARPNESS:
                *(VQ_SHARPNESS_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.SharpnessVQData;
            break;

        case FVDP_ACM:
                *(VQ_ACM_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.ACMVQData;
            break;

        case FVDP_OP_CSC:
                *(VQ_COLOR_Parameters_t *) PSITuningData=
                    HandleContext_p->PSIFeature.OPCSCVQData;
            break;

        default:
            return(FVDP_ERROR);
    }
    return FVDP_OK;
}


/*******************************************************************************
Name        : UPDATE_SetOutputRasterInfo
Description : Set output display raster information
Parameters  :  [in] FvdpChHandle Handle of Channel to be applied.
               [in] OutputRasterInfo Data structure containing information about the display timing.
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster Attributes specified are not recognized or out of range
*******************************************************************************/
FVDP_Result_t UPDATE_SetOutputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_RasterInfo_t OutputRasterInfo)
{
    FVDP_HandleContext_t* HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
        return FVDP_ERROR;

    // Check for any changes in the Input Raster Information
    if (   (HandleContext_p->OutputRasterInfo.HStart != OutputRasterInfo.HStart)
        || (HandleContext_p->OutputRasterInfo.VStart != OutputRasterInfo.VStart)
        || (HandleContext_p->OutputRasterInfo.HTotal != OutputRasterInfo.HTotal)
        || (HandleContext_p->OutputRasterInfo.VTotal != OutputRasterInfo.VTotal)
        || (HandleContext_p->OutputRasterInfo.HActive!= OutputRasterInfo.HActive)
        || (HandleContext_p->OutputRasterInfo.VActive != OutputRasterInfo.VActive)
        || (HandleContext_p->OutputRasterInfo.UVAlign != OutputRasterInfo.UVAlign)
        || (HandleContext_p->OutputRasterInfo.FieldAlign != OutputRasterInfo.FieldAlign))
    {
        // There is a change, so copy the data and set the update flag.
        HandleContext_p->OutputRasterInfo = OutputRasterInfo;
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_RASTER_INFO;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : UPDATE_SetColorFill
Description : Set output color fill
Parameters  :  [in] FvdpChHandle Handle of Channel to be applied.
               [in] bool option
               [in] uint32_t RGB color data => 8:8:8 bit
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster Attributes specified are not recognized or out of range
*******************************************************************************/
FVDP_Result_t UPDATE_SetColorFill(FVDP_CH_Handle_t  FvdpChHandle, FVDP_CtrlColor_t Enable, uint32_t value)
{
    FVDP_CH_t CH;
    FVDP_HandleContext_t* HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
        return FVDP_ERROR;

    CH = HandleContext_p->CH;

    if (Enable != FVDP_CTRL_COLOR_STATE_DISABLE)
    {
        if (CH == FVDP_MAIN)
        {
            Csc_ColorFullFill(CH, TRUE, value);
        }
        else
        {
            Csc_ColorLiteFill(CH, TRUE, value);
        }
    }
    else
    {
        if (CH == FVDP_MAIN)
        {
            Csc_ColorFullFill(CH, FALSE, value);
        }
        HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_COLOR_CONTROL;
    }
    return FVDP_OK;
}




