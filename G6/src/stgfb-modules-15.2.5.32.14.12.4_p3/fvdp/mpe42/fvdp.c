/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "../fvdp_revision.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_update.h"

#include <stm_display.h>
#include <vibe_os.h>
#include "fvdp_ivp.h"
#include "fvdp_debug.h"
#include "fvdp_driver.h"
#include "fvdp_vcd.h"
#include "fvdp_hostupdate.h"

extern void fvdp_register_init(void);

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */
#ifdef ENABLE_FVDP_LOGGING
#define FVDP_LOGGING(a,...) FVDP_LogCollection(a,__VA_ARGS__)
#else
#define FVDP_LOGGING(a,...)
#endif

/* Global Variables --------------------------------------------------------- */

ColorFill_t ColorFillInfo[NUM_FVDP_CH] = {{FVDP_CTRL_COLOR_STATE_DISABLE,0} ,{FVDP_CTRL_COLOR_STATE_DISABLE,0}, {FVDP_CTRL_COLOR_STATE_DISABLE,0}};


/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : FVDP_InitDriver
Description : Initialize driver
Parameters  :  [in] InitParams Driver initialization parameters

Assumptions : Channel is not initialized and not opened.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
                    FVDP_ERROR: Intialization errors
*******************************************************************************/
FVDP_Result_t FVDP_InitDriver(FVDP_Init_t InitParams)
{
    FVDP_LOGGING(__FUNCTION__,InitParams);

    if(!InitParams.pDeviceBaseAddress)
    {
        return FVDP_ERROR;
    }
    SetDeviceBaseAddress((uint32_t)InitParams.pDeviceBaseAddress);

    // Initialize all the FVDP_HandleContext_t structures
    SYSTEM_Init();

#if defined(VSOC_PLATFORM)
    fvdp_register_init();
#endif


    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_GetRequiredVideoMemorySize
Description : Returns the maximum memory required with given parameters
Parameters  :  [in] CH Channel
                    [in] Context.ProcessingType Structure containing information about
                    the level of processing to be performed on the requested channel.
                    [in] Context.MaxWindowSize Specifies the maximum display size
                    for the specified FVDP Channel.
                    [out] *MemSize returns DRAM size required for the specified FVDP Channel.

Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
                    FVDP_ERROR: Invalid Channel/Zeo width or height
                    FVDP_PROC_TYPE_ERROR: Requested ProcessingType is not available on this channel.
*******************************************************************************/
FVDP_Result_t FVDP_GetRequiredVideoMemorySize(FVDP_CH_t      CH,
                                              FVDP_Context_t Context,
                                              uint32_t*      MemSize)
{
    FVDP_Result_t ErrorCode;

    FVDP_LOGGING(__FUNCTION__, CH, Context, MemSize);

    if (CH >= NUM_FVDP_CH)
    {
        return (FVDP_ERROR);
    }

    if ((Context.MaxWindowSize.Width == 0) || (Context.MaxWindowSize.Height == 0))
    {
        return (FVDP_ERROR);
    }

    if (Context.ProcessingType > FVDP_RGB_GRAPHIC)
    {
        return (FVDP_PROC_TYPE_ERROR);
    }

    if (MemSize == NULL)
    {
        return (FVDP_PROC_TYPE_ERROR);
    }

    ErrorCode = SYSTEM_GetRequiredVideoMemorySize(CH, Context.ProcessingType, Context.MaxWindowSize, MemSize);

    return ErrorCode;
}


    // EricB ToDo:  FVDP_OpenChannel should be updated in the FVDP SW functional spec (Context is missing)

/*******************************************************************************
Name        : FVDP_OpenChannel
Description : Open the selected channel
Parameters  : [out] FvdpChHandle - The FVDP handle.
              [in] CH Channel to be opened
              [in] MemBaseAddress Specifies DRAM Base address for the specified FVDP Channel.
              [in] MemSize Specifies DRAM Area Reserved for the specified FVDP Channel.
              [in] Context.ProcessingType Structure containing information about the level of processing to be
                   performed on the requested channel.
              [in] Context.MaxWindowSize Specifies the maximum display size
                   for the specified FVDP Channel.
Assumptions : Channel is not opened.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
                    FVDP_MEM_INIT_ERROR: Not enough memory allocated for the features defined by ProcessingType.
                    FVDP_PROC_TYPE_ERROR: Requested ProcessingType is not available on this channel.
*******************************************************************************/
FVDP_Result_t FVDP_OpenChannel(FVDP_CH_Handle_t* FvdpChHandle_p,
                               FVDP_CH_t         CH,
                               uint32_t          MemBaseAddress,
                               uint32_t          MemSize,
                               FVDP_Context_t    Context)
{
    FVDP_Result_t ErrorCode;
    uint32_t      RequiredMemSize;

    FVDP_LOGGING(__FUNCTION__, CH, MemBaseAddress, MemSize, Context);

    if (CH >= NUM_FVDP_CH)
    {
        return (FVDP_ERROR);
    }

    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    TRC( TRC_ID_MAIN_INFO, "FVDP_OpenChannel(%d) - MemBaseAddress = 0x%08X.  MemSize = 0x%08X.", CH, MemBaseAddress, MemSize );
    // For ENC channel, there is no memory buffer needed so MemSize will be 0
    if ((MemSize != 0) && (MemBaseAddress == 0))
    {
        return (FVDP_MEM_INIT_ERROR);
    }

    // Check that the given memory size (MemSize) is enough for the provided channel Context
    if (SYSTEM_GetRequiredVideoMemorySize(CH, Context.ProcessingType, Context.MaxWindowSize, &RequiredMemSize) == FVDP_OK)
    {
        // Check that the MemSize is correct.
        if (MemSize < RequiredMemSize)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  Not enough memory provided to FVDP." );
            TRC( TRC_ID_MAIN_INFO, "        Required memory size: 0x%08X.", RequiredMemSize );
            TRC( TRC_ID_MAIN_INFO, "        Provided memory size: 0x%08X.", MemSize );
            return FVDP_ERROR;
        }
    }

    // EricB ToDo:  Should add check for PIP, AUX that FVDP_TEMPORAL is not selected.
#if (MCTI_HW_REV != REV_NONE)
    if (Context.ProcessingType > FVDP_RGB_GRAPHIC)
#else
    if(  (Context.ProcessingType == FVDP_TEMPORAL_FRC) ||(Context.ProcessingType > FVDP_RGB_GRAPHIC))
#endif
    {
        return (FVDP_PROC_TYPE_ERROR);
    }

    if (!GetDeviceBaseAddress())
    {
        return (FVDP_MEM_INIT_ERROR);
    }

    ErrorCode = SYSTEM_OpenChannel(FvdpChHandle_p, CH, MemBaseAddress, MemSize, Context);
    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_CloseChannel
Description : Close the selected channel
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
Assumptions : Channel is not opened.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
*******************************************************************************/
FVDP_Result_t FVDP_CloseChannel(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    SYSTEM_CloseChannel(FvdpChHandle);

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_ConnectInput
Description : Connect the selected channel to the input source
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Input Input Source from which FVDP Channel will receive video.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No errors
                    FVDP_ERROR: Cannot connect to input source
*******************************************************************************/
FVDP_Result_t FVDP_ConnectInput(FVDP_CH_Handle_t FvdpChHandle,
                                FVDP_Input_t     Input)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, Input);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return UPDATE_SetInputSource(FvdpChHandle, Input);
}

/*******************************************************************************
Name        : FVDP_DisconnectInput
Description : Disconnect the selected channel from the input source
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel

Assumptions : Channel is open and connected.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No errors
                    FVDP_ERROR: Cannot disconnect connect from input source
*******************************************************************************/
FVDP_Result_t FVDP_DisconnectInput(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);
    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    return FVDP_OK;
}


/*******************************************************************************
Name        : FVDP_SetVideoProcType
Description : Set FVDP processing type
Parameters  : [in] FvdpChHandle     - Handle to an FVDP Channel
[in] VideoProcType    - Select FVDP processing type

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetVideoProcType(FVDP_CH_Handle_t  FvdpChHandle,
                                     FVDP_ProcessingType_t  VideoProcType)
{
    FVDP_Result_t ErrorCode;
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_CH_t             CH;

    if (VideoProcType != FVDP_TEMPORAL && VideoProcType != FVDP_RGB_GRAPHIC) {
        // invalid value
        return FVDP_ERROR;
    }

    // FVDP power on staste
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }
    CH = HandleContext_p->CH;

    vibe_printf("--> FVDP : Entering FVDP_SetVideoProcType, current ProcType %d\n",HandleContext_p->ProcessingType);

    if (CH == FVDP_MAIN)
    {
        ErrorCode =  SYSTEM_FlushChannel(FvdpChHandle,TRUE);
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_CCTRL, FORCE_UPDATE);

        if(ErrorCode == FVDP_OK)
        {
            ErrorCode = SYSTEM_ReInitFramePool(FvdpChHandle, VideoProcType);
        }
    }
    else
    {
        ErrorCode = FVDP_FEATURE_NOT_SUPPORTED;
    }

    if(ErrorCode == FVDP_OK)
        vibe_printf("--> FVDP : ProcessingType has been changed to %d\n",VideoProcType);
    else
        vibe_printf("--> FVDP : Error, could not change ProcessingType to %d\n",VideoProcType);

     return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_SetInputVideoInfo
Description : Set input video information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] InputVideoInfo Data structure containing information about Video attributes.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                     FVDP_VideoInfo_t  InputVideoInfo)
{
    FVDP_Result_t result;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, InputVideoInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    {
        extern void VCPU_LogPrintControl(void);
        VCPU_LogPrintControl();
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check incoming width and height, It should be none 0 value
    if ((InputVideoInfo.Width == 0) || (InputVideoInfo.Height == 0))
    {
        return (FVDP_ERROR);
    }

    result = UPDATE_SetInputVideoInfo(FvdpChHandle, &InputVideoInfo);
    if (result == FVDP_OK)
    {
        VCD_ValueChange(FVDP_MAIN, VCD_EVENT_FRAME_ID_SET_VID_INP, InputVideoInfo.FrameID);
    }

    return result;
}

/*******************************************************************************
Name        : FVDP_GetInputVideoInfo
Description : Get input video information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] InputVideoInfo Data structure containing information about Video attributes.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                     FVDP_VideoInfo_t* InputVideoInfo)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, InputVideoInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (InputVideoInfo == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the Current Frame information
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    if (CurrFrame_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // return the Input Video Info for the Current Frame
    *InputVideoInfo = CurrFrame_p->InputVideoInfo;
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_FRAME_ID_GET_VID_INP, InputVideoInfo->FrameID);

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_SetInputRasterInfo
Description : Set input raster information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] InputRasterInfo Data structure containing information about the raster timing.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster Attributes specified are not recognized or out of range
*******************************************************************************/
FVDP_Result_t FVDP_SetInputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_RasterInfo_t InputRasterInfo)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, InputRasterInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return UPDATE_SetInputRasterInfo(FvdpChHandle, InputRasterInfo);
}

/*******************************************************************************
Name        : FVDP_GetInputRasterInfo
Description : Get input raster information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] InputRasterInfo Data structure containing information about the raster timing.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster Attributes specified are not recognized or out of range
*******************************************************************************/
FVDP_Result_t FVDP_GetInputRasterInfo(FVDP_CH_Handle_t   FvdpChHandle,
                                      FVDP_RasterInfo_t* InputRasterInfo)
{
    FVDP_HandleContext_t* HandleContext_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, InputRasterInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // return the Input Raster Info
    *InputRasterInfo = HandleContext_p->InputRasterInfo;

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_SetOutputVideoInfo
Description : Set output video information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] OutputVideoInfo Data structure containing information about Output Video attributes.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetOutputVideoInfo(FVDP_CH_Handle_t FvdpChHandle,
                                      FVDP_VideoInfo_t OutputVideoInfo)
{
    FVDP_HandleContext_t* HandleContext_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, OutputVideoInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check incoming width and height, It should be none 0 value
    if ((OutputVideoInfo.Width == 0) || (OutputVideoInfo.Height == 0))
    {
        return (FVDP_ERROR);
    }

    // Check that the output window size is not bigger than the Max output window size
    if (OutputVideoInfo.Width > HandleContext_p->MaxOutputWindow.Width ||
        OutputVideoInfo.Height > HandleContext_p->MaxOutputWindow.Height)
    {
        return (FVDP_VIDEO_INFO_ERROR);
    }
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_FRAME_ID_SET_VID_OUTP, OutputVideoInfo.FrameID);

    return UPDATE_SetOutputVideoInfo(FvdpChHandle, &OutputVideoInfo);
}

/*******************************************************************************
Name        : FVDP_GetOutputVideoInfo
Description : Get output video information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] OutputVideoInfo Data structure containing information about Output Video attributes.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetOutputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_VideoInfo_t* OutputVideoInfo)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         OutputFrame_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, OutputVideoInfo);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (OutputVideoInfo == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the Frame information of the Output frame
    OutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);
    if (OutputFrame_p == NULL)
    {
        return (FVDP_NOTHING_TO_RELEASE);
    }

    // Check that the Output frame has valid Output Video Info
    if ((OutputFrame_p->OutputVideoInfo.Width == 0) || (OutputFrame_p->OutputVideoInfo.Height == 0))
    {
        return (FVDP_NOTHING_TO_RELEASE);
    }

    *OutputVideoInfo = OutputFrame_p->OutputVideoInfo;

    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_FRAME_ID_GET_VID_OUTP, OutputVideoInfo->FrameID);


    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_SetOutputWindow
Description : Set border to output video window
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] OutputActiveWindow Data structure containing information
              about Output Active Window settings.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetOutputWindow(FVDP_CH_Handle_t FvdpChHandle,
                                      FVDP_OutputWindow_t *OutputActiveWindow)
{
    FVDP_HandleContext_t* HandleContext_p;

//    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, OutputActiveWindow);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that the output window size is not bigger than the Max output window size
    if (OutputActiveWindow->HWidth > HandleContext_p->MaxOutputWindow.Width ||
        OutputActiveWindow->VHeight > HandleContext_p->MaxOutputWindow.Height)
    {
        return (FVDP_ERROR);
    }

    return UPDATE_SetOutputWindow(FvdpChHandle, OutputActiveWindow);
}

/*******************************************************************************
Name        : FVDP_GetOutputWindow
Description : Get output active window  information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] OutputActiveWindow Data structure containing information
              about Output Active Window settings.
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_ERROR
                 FVDP_NOTHING_TO_RELEASE
*******************************************************************************/

FVDP_Result_t FVDP_GetOutputWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_OutputWindow_t *OutputActiveWindow)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         OutputFrame_p;

//    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, OutputActiveWindow);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (OutputActiveWindow == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the Frame information of the Output frame
    OutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);
    if (OutputFrame_p == NULL)
    {
        return (FVDP_NOTHING_TO_RELEASE);
    }

    // Check that the Output frame has valid Output Video Info
    if ((OutputFrame_p->OutputActiveWindow.HWidth == 0) || (OutputFrame_p->OutputActiveWindow.VHeight == 0))
    {
        return (FVDP_NOTHING_TO_RELEASE);
    }

    *OutputActiveWindow = OutputFrame_p->OutputActiveWindow;

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_GetLatency
Description : FVDP_GetLatency() function returns the min/max number of input VS
              delays and min/max number of output VS delays before it is ready
              to be processed for output.
Parameters  :  [in] FvdpChHandle Channel to query latency
               [in] MinInputLatency pointer to uint8_t to store result
               [in] MaxInputLatency pointer to uint8_t to store result
               [in] MinOutputLatency pointer to uint8_t to store result
               [in] MaxOutputLatency pointer to uint8_t to store result

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetLatency(FVDP_CH_Handle_t FvdpChHandle,
                              uint8_t*         MinInputLatency,
                              uint8_t*         MaxInputLatency,
                              uint8_t*         MinOutputLatency,
                              uint8_t*         MaxOutputLatency)
{
    FVDP_HandleContext_t* HandleContext_p;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that pointers are valid
    if (    MinInputLatency  == NULL
        ||  MaxInputLatency  == NULL
        ||  MinOutputLatency == NULL
        ||  MaxOutputLatency == NULL )
    {
        return (FVDP_ERROR);
    }

    // Return min/max input/output latencies
    if (HandleContext_p->CH == FVDP_MAIN)
    {
        if (HandleContext_p->LatencyType != FVDP_MINIMUM_LATENCY) {
            // note: MCDi temporal processing assumed for FVDP_MAIN
            *MinInputLatency  = 3;
            *MaxInputLatency  = 3;
            *MinOutputLatency = 1;
            *MaxOutputLatency = 2;
        }
        else {
            *MinInputLatency  = 1;
            *MaxInputLatency  = 1;
            *MinOutputLatency = 1;
            *MaxOutputLatency = 2;
        }
    }
    else
    {
        // note: spatial processing assumed for FVDP_PIP/AUX
        *MinInputLatency  = 1;
        *MaxInputLatency  = 1;
        *MinOutputLatency = 1;
        *MaxOutputLatency = 2;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_CommitUpdate
Description : FVDP_CommitUpdate() function triggers the driver to apply any new parameters
                which have been passed through other function calls from the application
                since the last FVDP_CommitUpdate() call.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_CommitUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return UPDATE_CommitUpdate(FvdpChHandle);
}

/*******************************************************************************
Name        : FVDP_InputUpdate
Description : FVDP_InputUpdate() function triggers the driver to apply any new parameters
                which have been passed through other function calls from the application
                since the last FVDP_InputUpdate() call.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR
*******************************************************************************/
#define IVP_REG32_ReadA(Addr)                 REG32_Read(Addr+MIVP_BASE_ADDRESS)

FVDP_Result_t FVDP_InputUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_Result_t ErrorCode = FVDP_OK;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        VCD_ValueChange(FVDP_MAIN, VCD_EVENT_INPUT_UPDATE_RET, FVDP_ERROR);
        return (FVDP_ERROR);
    }

    ErrorCode = UPDATE_InputUpdate(FvdpChHandle);

    FVDP_RegLog_Acquire(REGLOG_SAMPLE_INPUT_UPDATE);
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_INPUT_UPDATE_RET, ErrorCode);

    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_IBD_V_TOTAL, IVP_REG32_ReadA(IVP_IBD_VTOTAL));
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_IBD_V_ACTIVE, IVP_REG32_ReadA(IVP_IBD_VLENGTH));

    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_OutputUpdate
Description : FVDP_OutputUpdate() function triggers the driver to apply any new parameters
                which have been passed through other function calls from the application
                since the last FVDP_OutputUpdate() call.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_OutputUpdate(FVDP_CH_Handle_t FvdpChHandle)
{
    FVDP_Result_t ErrorCode = FVDP_OK;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        VCD_ValueChange(FVDP_MAIN, VCD_EVENT_OUTPUT_UPDATE_RET, FVDP_ERROR);
        return (FVDP_ERROR);
    }

    ErrorCode = UPDATE_OutputUpdate(FvdpChHandle);

    FVDP_RegLog_Acquire(REGLOG_SAMPLE_OUTPUT_UPDATE);
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_OUTPUT_UPDATE_RET, ErrorCode);

    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_IBD_V_TOTAL, IVP_REG32_ReadA(IVP_IBD_VTOTAL));
    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_IBD_V_ACTIVE, IVP_REG32_ReadA(IVP_IBD_VLENGTH));


    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_GetVersion
Description : Can be called anytime after clocks have been configured for the FVDP.
              Typically this function would be called prior to intialization of FVDP channels.
Parameters  : [out] Version Pointer to FVDP Version
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK No errors
*******************************************************************************/
FVDP_Result_t FVDP_GetVersion(FVDP_Version_t* Version)
{
    uint8_t i;

    FVDP_LOGGING(__FUNCTION__, Version);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if(!Version)
        return FVDP_ERROR;

    Version->Major = FVDP_MajorVersion;
    Version->Minor = FVDP_MinorVersion;

    for(i=0; i<sizeof(FVDP_ReleaseID); i++)
    {
        Version->ReleaseID[i] = FVDP_ReleaseID[i];
    }
    /* Version.ReleaseID string format: FVDP-RELx.x.x
                                                        /         \ \ \__patch release number  }
                                               API name         \ \__minor release number  } revision number
                                                                      \__major release number  }
    */
    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_SetPSIState
Description : Set PSI state information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSI state.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetPSIState(FVDP_CH_Handle_t    FvdpChHandle,
                               FVDP_PSIFeature_t   PSIFeature,
                               FVDP_PSIState_t     PSIState)
{
    FVDP_Result_t ErrorCode = FVDP_ERROR;

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return ErrorCode;
    }

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, PSIFeature, PSIState);

    ErrorCode = UPDATE_SetPSIState(FvdpChHandle, PSIFeature, PSIState);
    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_GetPSIState
Description : Get PSI state information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIFeature
              [out] PSIState.

Assumptions : Channel is open.
Limitations : Returned PSI state is represented the latest updated data
              by FVDP_SetPSIState()
              H/W will updated
              after FVDP_CommitUpdate()/FVDP_InputUpdate()/FVDP_OutputUpdate()
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetPSIState(FVDP_CH_Handle_t    FvdpChHandle,
                               FVDP_PSIFeature_t   PSIFeature,
                               FVDP_PSIState_t*    PSIState)
{
    FVDP_Result_t ErrorCode = FVDP_ERROR;

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return ErrorCode;
    }

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, PSIFeature, PSIState);

    ErrorCode = UPDATE_GetPSIState(FvdpChHandle, PSIFeature,PSIState);
    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_SetCropWindow
Description : Set crop window size and position  information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] crop window size and position.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetCropWindow(FVDP_CH_Handle_t   FvdpChHandle,
                                 FVDP_CropWindow_t  CropWindow)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, CropWindow);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check incoming width and height, It should be none 0 value
    if ((CropWindow.HCropWidth == 0) || (CropWindow.VCropHeight == 0))
    {
        return (FVDP_ERROR);
    }

    return UPDATE_SetCropWindow(FvdpChHandle, CropWindow);
}

/*******************************************************************************
Name        : FVDP_GetCropWindow
Description : Get crop window size and position  information
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [out] crop window size and position.

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_VIDEO_INFO_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetCropWindow(FVDP_CH_Handle_t   FvdpChHandle,
                                 FVDP_CropWindow_t* CropWindow)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Frame_t*         CurrFrame_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, CropWindow);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Get the Current Frame information
    CurrFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    if (CurrFrame_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // return the Crop Window for the Current Frame
    *CropWindow = CurrFrame_p->CropWindow;

    return FVDP_OK;
}


/*******************************************************************************
Name        : FVDP_SetLatencyType
Description : FVDP_SetLatencyType() sets the control level specified Latency control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetLatencyType(FVDP_CH_Handle_t  FvdpChHandle,
                                 uint32_t           Value)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle,  Value);

    // FVDP power on staste
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return  UPDATE_SetLatencyType(FvdpChHandle,  Value);
}

/*******************************************************************************
Name        : FVDP_GetLatencyType
Description : FVDP_GetLatencyType() returns the current latencycontrol level
                  for specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [out] Value Value of the selected control

Assumptions : Channel is open.
Limitations : Returned Latency control value is represented the latest updated data
              by FVDP_SetLatencyControl()
              H/W will updated
              after FVDP_CommitUpdate()/FVDP_InputUpdate()/FVDP_OutputUpdate()
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetLatencyType(FVDP_CH_Handle_t  FvdpChHandle,
                                 uint32_t*          Value)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, Value);

    // FVDP power on staste
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }
    return  UPDATE_GetLatencyType(FvdpChHandle,  Value);
}



/*******************************************************************************
Name        : FVDP_SetPSIControl
Description : FVDP_SetPSIControl() sets the control level specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIControl Select Control to set
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t           Value)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, PSIControl, Value);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return  UPDATE_SetPSIControl(FvdpChHandle, PSIControl, Value);
}

/*******************************************************************************
Name        : FVDP_GetPSIControl
Description : FVDP_GetPSIControl() returns the current activecontrol level
                  for specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIControl Select Control to set
              [in] Value Value for the selected control

Assumptions : Channel is open.
Limitations : Returned PSI control value is represented the latest updated data
              by FVDP_SetPSIControl()
              H/W will updated
              after FVDP_CommitUpdate()/FVDP_InputUpdate()/FVDP_OutputUpdate()
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t*          Value)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, PSIControl, Value);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }
    return  UPDATE_GetPSIControl(FvdpChHandle, PSIControl, Value);
}

/*******************************************************************************
Name        : FVDP_GetPSIControlsRange
Description : FVDP_GetPSIControlsRange() returns the minimim and maximum level
              (range) for specified PSI control.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIControlsSelect Selects the Control Range which is requested.
              [out] MinValue Pointer to Min Value for the requested Control.
              [out] MaxValue Pointer to Max Value for the requested Control.
              [out] DefaultValue Pointer to Default Value for the requested Control.
              [out] StepValue Pointer to Step Value for the requested Control.
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
              FVDP_FEATURE_NOT_SUPPORTED
*******************************************************************************/
FVDP_Result_t FVDP_GetPSIControlsRange(FVDP_CH_Handle_t FvdpChHandle,
                                       FVDP_PSIControl_t PSIControlsSelect,
                                       int32_t*          MinValue,
                                       int32_t*          MaxValue,
                                       int32_t*          DefaultValue,
                                       int32_t*          StepValue)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_HandleContext_t * HandleContext_p;

    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, PSIControlsSelect, MinValue, MaxValue, DefaultValue, StepValue);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    if (MinValue == NULL || MaxValue==NULL || DefaultValue == NULL || StepValue == NULL)
        return (FVDP_ERROR);

    switch (PSIControlsSelect)
    {
        case FVDP_BRIGHTNESS:
            *DefaultValue = DEFAULT_BRIGHTNESS;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_CONTRAST:
            *DefaultValue = DEFAULT_CONTRAST;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_SATURATION:
            *DefaultValue = DEFAULT_SATURATION;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_HUE:
            *DefaultValue = DEFAULT_HUE;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_RED_GAIN:
            *DefaultValue = DEFAULT_REDGAIN;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_GREEN_GAIN:
            *DefaultValue = DEFAULT_GREENGAIN;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_BLUE_GAIN:
            *DefaultValue = DEFAULT_BLUEGAIN;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_RED_OFFSET:
            *DefaultValue = DEFAULT_REDOFFSET;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_GREEN_OFFSET:
            *DefaultValue = DEFAULT_GREENOFFSET;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_BLUE_OFFSET:
            *DefaultValue = DEFAULT_BLUEOFFSET;
            ErrorCode = SYSTEM_GetColorRange(PSIControlsSelect, MinValue, MaxValue, StepValue);
            break;
        case FVDP_SHARPNESS_LEVEL:
            *DefaultValue = DEFAULT_SHARPNESS;
            ErrorCode = SYSTEM_PSI_Sharpness_ControlRange(HandleContext_p->CH, MinValue, MaxValue, StepValue);
            break;

       /* TODO:Range for 3D depth */
        case FVDP_3D_DEPTH:
            *MinValue = 0;
            *MaxValue = 0;
            break;
        default:
            ErrorCode = FVDP_ERROR;
            break;
    }

    return ErrorCode;

}

/*******************************************************************************
Name        : FVDP_SetPSITuningData
Description : FVDP_SetPSITuningData() writes to hardware psi data selected by PSIFeature
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIFeature Selects which psi data is requested.
              [in] PSITuningData Pointer to psi data structure.
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                   FVDP_OK No errors
                   FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetPSITuningData(FVDP_CH_Handle_t FvdpChHandle,
                                    FVDP_PSIFeature_t     PSIFeature,
                                    FVDP_PSITuningData_t* PSITuningData)
{
     FVDP_Result_t ErrorCode = FVDP_ERROR;

     // Check FVDP power on state
     if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
     {
         return (FVDP_ERROR);
     }

     // Check that the FVDP Handle is open
     if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
     {
         return ErrorCode;
     }

     ErrorCode = UPDATE_SetPSITuningData ( FvdpChHandle, PSIFeature, PSITuningData);

    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_GetPSITuningData
Description : FVDP_GetPSITuningData() returns psi data selected by PSIFeature
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] PSIFeature Selects which psi data is requested.
              [out] PSITuningData Pointer to psi data structure.
Assumptions :
Limitations : Returned PSI VQ value is represented the latest updated data
              by FVDP_SetPSITuningData()
              H/W will updated
              after FVDP_CommitUpdate()/FVDP_InputUpdate()/FVDP_OutputUpdate()
Returns     : FVDP_Result_t type value :
                   FVDP_OK No errors
                   FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetPSITuningData(FVDP_CH_Handle_t      FvdpChHandle,
                                    FVDP_PSIFeature_t     PSIFeature,
                                    FVDP_PSITuningData_t* PSITuningData)
{
    FVDP_Result_t ErrorCode = FVDP_ERROR;

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return ErrorCode;
    }

    ErrorCode = UPDATE_GetPSITuningData( FvdpChHandle, PSIFeature, PSITuningData);

    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_QueueFlush
Description : Flush FBM queue and mute fvdp output until queue is fill up by new buffer
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
               bool bFlushCurrentNode = true :FVDP make to disable current displaying image
                                      = false :FVDP keep current outputting picture

Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                 FVDP_OK: No errors
                 FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_QueueFlush(FVDP_CH_Handle_t FvdpChHandle, bool bFlushCurrentNode)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    return SYSTEM_FlushChannel(FvdpChHandle, bFlushCurrentNode);
}

/*******************************************************************************
Name        : FVDP_SetCrcState
Description : Set Crc state to enable and disable for the selected crc feature.
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Feature Selects the crc feature which is enabled or disabled.
              [in] State TRUE: Enable FALSE: Disable
Assumptions : FVDP Channel is opened.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetCrcState(FVDP_CH_Handle_t  FvdpChHandle,
                               FVDP_CrcFeature_t Feature,
                               bool              State)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, Feature, State);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_GetCrcInfo
Description : Get Crc information for the selected crc feature.
Parameters  : [in] FvdpChHandle for which crc feature information is returned.
              [in] Feature Selects the crc feature which is to be returned.
              [out] pInfo points to the FVDP_CrcInfo_t structure from caller.
Assumptions : FVDP Channel is opened.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_INFO_NOT_AVAILABLE Crc Status is not enabled
              when info is requested.
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_GetCrcInfo(FVDP_CH_Handle_t   FvdpChHandle,
                              FVDP_CrcFeature_t  Feature,
                              FVDP_CrcInfo_t*    Info)
{
    FVDP_LOGGING(__FUNCTION__, FvdpChHandle, Feature, Info);

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_SetOutputRasterInfo
Description : Set output display raster information
Parameters  : [in] FvdpChHandle     - Handle to an FVDP Channel
              [in] OutputRasterInfo Data structure containing information about the display timing.
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK No errors
                    FVDP_ERROR Raster Attributes specified are not recognized or out of range
*******************************************************************************/
FVDP_Result_t FVDP_SetOutputRasterInfo(FVDP_CH_Handle_t FvdpChHandle,
     FVDP_RasterInfo_t OutputRasterInfo)
{
     FVDP_LOGGING(__FUNCTION__, FvdpChHandle, OutputRasterInfo);

    // FVDP power on staste
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    return UPDATE_SetOutputRasterInfo(FvdpChHandle, OutputRasterInfo);
}

/*******************************************************************************
Name        : FVDP_SetBufferCallback
Description : Sets the callback functions for memory-to-memory operations.
Parameters  : [in] FvdpChHandle     - Handle to an FVDP Channel
              [in] ScalingCompleted - Callback function when the scaling task
                                      is completed.
              [in] BufferDone       - Callback function when the buffer is no longer
                                      needed for any future temporal processing.
Assumptions : FVDP Ch Handle is opened.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_SetBufferCallback(FVDP_CH_Handle_t FvdpChHandle,
                                     void             (*ScalingCompleted)(void* UserData, bool Status),
                                     void             (*BufferDone)      (void* UserData) )
{
    FVDP_HandleContext_t* HandleContext_p;

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that parameters are valid
    if ((ScalingCompleted == NULL) || (BufferDone == NULL))
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
        return FVDP_ERROR;

    HandleContext_p->QueueBufferInfo.ScalingCompleted = ScalingCompleted;
    HandleContext_p->QueueBufferInfo.BufferDone = BufferDone;

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_QueueBuffer
Description : Request to perform a memory-to-memory operation.
Parameters  : [in] FvdpChHandle - Handle to an FVDP Channel
              [in] FrameID      - The Frame ID for this scaling operation
              [in] SrcAddr1     - Luma (or RGB) base address of the source frame buffer
              [in] SrcAddr2     - Chroma address OFFSET of the source frame buffer
              [in] DstAddr1     - Luma (or RGB) base address of the destination frame buffer
              [in] DstAddr2     - Chroma address OFFSET of the destination frame buffer
Assumptions : FVDP Ch Handle is opened.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t FVDP_QueueBuffer(FVDP_CH_Handle_t  FvdpChHandle,
                               void*             UserData,
                               uint32_t          SrcAddr1,
                               uint32_t          SrcAddr2,
                               uint32_t          DstAddr1,
                               uint32_t          DstAddr2)
{
    FVDP_HandleContext_t* HandleContext_p;

    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return (FVDP_ERROR);
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
        return FVDP_ERROR;

    HandleContext_p->QueueBufferInfo.SrcAddr1 = SrcAddr1;
    HandleContext_p->QueueBufferInfo.SrcAddr2 = SrcAddr1 + SrcAddr2;
    HandleContext_p->QueueBufferInfo.DstAddr1 = DstAddr1;
    HandleContext_p->QueueBufferInfo.DstAddr2 = DstAddr1 + DstAddr2;
    HandleContext_p->QueueBufferInfo.UserData = UserData;

    return SYSTEM_ENC_ProcessMemToMem(HandleContext_p);
}
/*******************************************************************************
Name        : FVDP_IntEndOfScaling
Description : This funcion is close to end of memory transfer
Parameters  : [in] FvdpChHandle - Handle to an FVDP Channel
Assumptions : FVDP Ch Handle is opened, and scaling is started.
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK No errors
              FVDP_ERROR
*******************************************************************************/

FVDP_Result_t FVDP_IntEndOfScaling(void)
{
    // Check FVDP power on state
    if(SYSTEM_CheckFVDPPowerUpState() == FALSE)
    {
        return (FVDP_ERROR);
    }

    return SYSTEM_ENC_CompleteScalling();
}

/*******************************************************************************
Name        : FVDP_SetState
Description :  Control power state
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              FVDP_State_t     State
Assumptions : Channel is not opened.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
*******************************************************************************/
FVDP_Result_t FVDP_SetState(FVDP_CH_Handle_t FvdpChHandle,
                            FVDP_State_t     State)
{
    FVDP_Result_t ErrorCode;
    FVDP_Power_State_t PowerState;

    if(State == FVDP_POWERDOWN)
    {
        PowerState = FVDP_SUSPEND;
    }
    else
    {
        PowerState = FVDP_RESUME;
    }

    ErrorCode = FVDP_PowerControl(PowerState);

     return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_PowerControl
Description : Control power state
Parameters  : [in] FVDP_Power_State_t PowerState
Assumptions : Channel is opened.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No Errors
*******************************************************************************/
FVDP_Result_t FVDP_PowerControl(FVDP_Power_State_t PowerState)
{
    FVDP_Result_t ErrorCode;

    ErrorCode = SYSTEM_SetPowerStateUpdate(PowerState);

    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_ColorFill
Description : Fill a video plane with a RGB888 color
Parameters  : [in] FvdpChHandle Handle to an FVDP Channel
              [in] Enable Start/stop color fill
              [in] Value RGB888 Value
Assumptions : Channel is open.
Limitations :
Returns     : FVDP_Result_t type value :
                    FVDP_OK: No errors
                    FVDP_ERROR: Cannot fill the video plane
*******************************************************************************/
FVDP_Result_t FVDP_ColorFill(FVDP_CH_Handle_t FvdpChHandle,
                                FVDP_CtrlColor_t    Enable, uint32_t Value)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_CH_t       CH;

    // Check that the FVDP Handle is open
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return FVDP_ERROR;
    }

    // Get the FVDP Context pointer for this handle
    if (SYSTEM_IsChannelHandleOpen(FvdpChHandle) == FALSE)
    {
        return FVDP_ERROR;
    }

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return (FVDP_ERROR);
    }



    CH = HandleContext_p->CH;

    TRC( TRC_ID_MAIN_INFO, "FVDP_ColorFill(%d) - State = 0x%08X.  Enable = 0x%08X.", CH, ColorFillInfo[CH].State, Enable );

    /* refer to FVDP_CtrlColor_t definition */
    if(( ColorFillInfo[CH].State == FVDP_CTRL_COLOR_STATE_DISABLE)&&(Enable == FVDP_CTRL_COLOR_STATE_ENABLE)) {

            ColorFillInfo[CH].State = FVDP_CTRL_COLOR_STATE_ENABLE; /* Update state */

    } else if (( ColorFillInfo[CH].State == FVDP_CTRL_COLOR_STATE_DISABLE)&&(Enable == FVDP_CTRL_COLOR_VALUE)) {

            ColorFillInfo[CH].RGB_Value = Value;
            ColorFillInfo[CH].State = FVDP_CTRL_COLOR_VALUE; /* store RGB Value to be applied when enable and update State */

    } else if (( ColorFillInfo[CH].State == FVDP_CTRL_COLOR_STATE_ENABLE)&&(Enable == FVDP_CTRL_COLOR_VALUE)) {

            /*call APPLY API and set state */

            ColorFillInfo[CH].RGB_Value = Value;

            UPDATE_SetColorFill(FvdpChHandle, Enable, Value);

    } else if (( ColorFillInfo[CH].State == FVDP_CTRL_COLOR_VALUE)&&(Enable == FVDP_CTRL_COLOR_STATE_ENABLE)) {

            /*call APPLY API and set state */

            ColorFillInfo[CH].State = FVDP_CTRL_COLOR_STATE_ENABLE;

            UPDATE_SetColorFill(FvdpChHandle, Enable, ColorFillInfo[CH].RGB_Value);


    } else if ((ColorFillInfo[CH].State == FVDP_CTRL_COLOR_STATE_ENABLE)&&(Enable == FVDP_CTRL_COLOR_STATE_DISABLE))  {

            /* Stop muting*/
            ColorFillInfo[CH].State = FVDP_CTRL_COLOR_VALUE;

            UPDATE_SetColorFill(FvdpChHandle, Enable, ColorFillInfo[CH].RGB_Value);


    } else if (( ColorFillInfo[CH].State == FVDP_CTRL_COLOR_VALUE)&&(Enable == FVDP_CTRL_COLOR_VALUE)) {

            /* only update RGB Value*/

            ColorFillInfo[CH].RGB_Value = Value;

    } else {
        TRC( TRC_ID_MAIN_INFO, "ERROR - Wrong state in FVDP_ColorFill" );
        return FVDP_ERROR;
    }

    return FVDP_OK;
}


/* end of file */
