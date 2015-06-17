/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_fe.c
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
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
#include "fvdp_tnr.h"
#include "fvdp_madi.h"
#include "fvdp_mcdi.h"
#include "fvdp_vcd.h"
#include "fvdp_hostupdate.h"
#include "fvdp_ccs.h"

/* Private Constants -------------------------------------------------------- */
#define MAXIMUM_SHRINK_RATIO         2  // the Maximum shrink ration of SCLAER Lite in FVDP_FE


/* Private Macros ----------------------------------------------------------- */
#define BYPASS_LITE_SCALER

#define VCD_MCC_AddrChange(client)                                     \
    VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_ ## client ## _ADDR, \
            MCC_IsEnabled(client) ? REG32_Read(FVDP_MCC_FE_BASE_ADDRESS \
                                    + (client ## _ADDR)) : ~0);

/* Private Type Definitions ------------------------------------------------- */


/* Private Function Prototypes ---------------------------------------------- */

static FVDP_Result_t SYSTEM_FE_ConfigureScaler(FVDP_HandleContext_t* HandleContext_p);
static void          SYSTEM_FE_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_FE_ConfigureMcMadi(FVDP_HandleContext_t* HandleContext_p);

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : SYSTEM_FE_Init
Description : Initialize FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_FE_Init(FVDP_HandleContext_t* HandleContext_p)
{
    EventCtrl_Init(FVDP_MAIN);

    Deint_Init();
    //Initialize TNR module
    Tnr_Init(HandleContext_p);
    Tnr_LoadDefaultVqData();

    // Initialize Madi Module
    Madi_Init();
    Madi_LoadDefaultVqData();

    // Initialize Mcdi module
    Mcdi_Init();
    Mcdi_LoadDefaultVqData();

    //Initialize CCS module
    SYSTEM_PSI_CCS_Init(HandleContext_p);

    //The input of Scaler is always progressive because Interlace input become progressive by MCMADI block
    //Set to 1(TOP_FIELD) to select scaler offset0
    EventCtrl_ConfigOddSignal_FE(FIELD_ID_VFLITE_IN_FW_FID_FE, FW_FID);
    EventCtrl_SetOddSignal_FE (FIELD_ID_VFLITE_IN_FW_FID_FE, TOP_FIELD);

    // Initialize the Scaler Lite HW
    HScaler_Lite_Init(FVDP_MAIN);
    VScaler_Lite_Init(FVDP_MAIN);

    return SYSTEM_InitMCC(HandleContext_p);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
 static int MadiCount = 0;
// in case of discontinuity, MadiCount is assigned M_DELAY, then every frame is decremented, the same time
// FE module is in spatial mode. If M_DELAY is decremented (delayed by) SPAC_DELAY times, then FE module switches
// to full Madi operation
#define SPAC_DELAY 6
#define M_DELAY 32
#define MADI_DELAY (M_DELAY-SPAC_DELAY)

bool SYSTEM_FE_IsInterlacedDiscontinuity(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_FrameInfo_t* FrameInfo_p = &(HandleContext_p->FrameInfo);
    FVDP_Frame_t* CurrentInputFrame_p = NULL;
    FVDP_Frame_t* PrevScanFrame_p = NULL;
    bool bReturn = FALSE;
    uint16_t      H_Size;
    uint16_t      V_Size;

    if(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType != INTERLACED)
        return FALSE;
    if (FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FVDPDisplayModeFlag & FVDP_FLAGS_PICTURE_REPEATED)
        bReturn = TRUE;
    // Get the current output frame
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    PrevScanFrame_p  = SYSTEM_GetPrev1InputFrame(HandleContext_p);

    H_Size = CurrentInputFrame_p->InputVideoInfo.Width;
    V_Size = CurrentInputFrame_p->InputVideoInfo.Height;

    // Check that frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p,PrevScanFrame_p) == FALSE)
        bReturn = TRUE;
    else
    {
        if (H_Size != PrevScanFrame_p->InputVideoInfo.Width)
            bReturn = TRUE;
        if (V_Size != PrevScanFrame_p->InputVideoInfo.Height)
            bReturn = TRUE;
    }

    PrevScanFrame_p  = SYSTEM_GetPrev2InputFrame(HandleContext_p);

    // Check that frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p,PrevScanFrame_p) == FALSE)
        bReturn = TRUE;
    else
    {
        if (H_Size != PrevScanFrame_p->InputVideoInfo.Width)
            bReturn = TRUE;
        if (V_Size != PrevScanFrame_p->InputVideoInfo.Height)
            bReturn = TRUE;
    }

    if (bReturn )
        FrameInfo_p-> CurrentInputFrame_p->InputVideoInfo.FVDPDisplayModeFlag |= FVDP_FLAGS_PICTURE_TEMPORAL_DISCONTINUITY;

    return bReturn;
}
/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_FE_ProcessInput(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t     ErrorCode = FVDP_OK;
    FVDP_FrameInfo_t* FrameInfo_p = &(HandleContext_p->FrameInfo);
    DATAPATH_FE_t     DatapathFE = HandleContext_p->DataPath.DatapathFE;
    FVDP_FieldType_t        FieldType;
    FVDP_CH_t             CH = HandleContext_p->CH;

    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpInputLock);

    // Call the input debug handler
    FVDP_DEBUG_Input(HandleContext_p);

    // During InputUpdate, the Next Input frame becomes the Current Input frame, and
    // a new Next Input frame is allocated (cloned)
    FrameInfo_p->Prev3InputFrame_p = FrameInfo_p->Prev2InputFrame_p;
    FrameInfo_p->Prev2InputFrame_p = FrameInfo_p->Prev1InputFrame_p;
    FrameInfo_p->Prev1InputFrame_p = FrameInfo_p->CurrentInputFrame_p;
    FrameInfo_p->CurrentInputFrame_p = FrameInfo_p->NextInputFrame_p;
    FrameInfo_p->NextInputFrame_p = SYSTEM_CloneFrame(HandleContext_p, FrameInfo_p->CurrentInputFrame_p);

    if (FrameInfo_p->NextInputFrame_p == NULL)
    {
        SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);
        return FVDP_ERROR;
    }

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

    VCD_ValueChange(CH, VCD_EVENT_INP_H_ACTIVE,  FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.Width);
    VCD_ValueChange(CH, VCD_EVENT_INP_V_ACTIVE, FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.Height);

    // Configure the VMUX
    SYSTEM_VMUX_Config(HandleContext_p);

    if (SYSTEM_FE_IsInterlacedDiscontinuity(HandleContext_p))
    {
        MadiCount = M_DELAY;
    }

    // Call the FBM Input Process
    FBM_IN_Update(HandleContext_p);

    if ((FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
        && (SW_ODD_FIELD_CONTROL == TRUE))
    {
        //FVDP_VMUX_INPUT2 = VTAC1,  FVDP_VMUX_INPUT3 = VTAC2 ==> HDMI
        if((HandleContext_p->InputSource == FVDP_VMUX_INPUT2)||(HandleContext_p->InputSource == FVDP_VMUX_INPUT3))
        {
            EventCtrl_ConfigOddSignal_FE(FIELD_ID_MCMADI_FW_FID_FE, SOURCE_IN);
            EventCtrl_SetOddSignal_FE (FIELD_ID_MCMADI_FW_FID_FE, TOP_FIELD);
        }
        else // All other inputs expect HDMI
        {
            FieldType= FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FieldType;
            EventCtrl_ConfigOddSignal_FE(FIELD_ID_MCMADI_FW_FID_FE, FW_FID);
            EventCtrl_SetOddSignal_FE (FIELD_ID_MCMADI_FW_FID_FE, FieldType);
        }
    }

    // Select the Datapath for FVDP_FE
    if (SYSTEM_IsFrameValid(HandleContext_p,FrameInfo_p->CurrentInputFrame_p) == TRUE)
    {
        if (   (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC)
            && ((FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ColorSpace == sRGB)
                || (FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ColorSpace == RGB)
                || (FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ColorSpace == RGB861)))
        {
            DatapathFE = DATAPATH_FE_PROGRESSIVE_SPATIAL_RGB;
        }
        else
        {
            if (FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {

                if(HandleContext_p->LatencyType != FVDP_MINIMUM_LATENCY)
                {
                    if (MadiCount > MADI_DELAY)
                        DatapathFE = DATAPATH_FE_INTERLACED_SPATIAL;
                    else
                        DatapathFE = DATAPATH_FE_INTERLACED_NO_CCS;
                }
                else
                {
                    // If Low latency mode is selected, FE pipeline is purely spatial
                    DatapathFE = DATAPATH_FE_INTERLACED_SPATIAL;
                }
            }
            else
            {
                DatapathFE = DATAPATH_FE_PROGRESSIVE_SPATIAL;
            }
        }
    }

    // Configure the FE Data path if it is different from current configuration
    if (DatapathFE != HandleContext_p->DataPath.DatapathFE)
    {
        HandleContext_p->DataPath.DatapathFE = DatapathFE;
#if (DFR_FE_PROGRAMMING_IN_VCPU == TRUE)
        FBM_IF_SendDataPath(HandleContext_p->CH, DATAPATH_MAIN_FE_LITE, DatapathFE);
#else
        DATAPATH_FE_Connect(HandleContext_p->DataPath.DatapathFE);
#endif
        if (HandleContext_p->LatencyType != FVDP_MINIMUM_LATENCY)
        {
            if ((FVDP_SPATIAL == HandleContext_p->ProcessingType) && (FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType != INTERLACED))
                McMadiEnableDatapath(FALSE,(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType));
            else
                McMadiEnableDatapath(TRUE,(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType));
        }
        else
        {
            // If Low latency mode is selected, DEI is forced off
            McMadiEnableDatapath(FALSE,(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType));
        }

        VCD_ValueChange(CH, VCD_EVENT_DATAPATH_FE, DatapathFE);
    }

    // Configure CCS
    if(HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_INPUT_PSI)
    {
        ErrorCode = SYSTEM_PSI_CCS(HandleContext_p) ;
        if(ErrorCode == FVDP_OK)
        {
            HandleContext_p->UpdateFlags.CommitUpdateFlag &= ~UPDATE_INPUT_PSI;
        }
    }

    if (MadiCount)
    {
        MadiCount --;
    }

    if(Tnr_UpdateTnrControlRegister() != FVDP_OK)
    {
         TRC( TRC_ID_MAIN_INFO, "Error: TNR enable and disable are call at same input update Require Re-configure TNR." );
         SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);
         return FVDP_ERROR;
    }

    // Configure McMADi
    // Deinterlacing control, if deint is enabled will double pro vertical size.
    SYSTEM_FE_ConfigureMcMadi(HandleContext_p);

    // Configure HScale/VScale Lite
    // scaler is affecting output size so it should be before mcc
    SYSTEM_FE_ConfigureScaler(HandleContext_p);
    // Configure all the FVDP_FE MCCs
    SYSTEM_FE_ConfigureMCC(HandleContext_p);

    //PSI control
    if(HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_INPUT_PSI)
    {

        // If new CCS vq data present then load it to hardware.
        if (HandleContext_p->UpdateFlags.PSIVQUpdateFlag & UPDATE_PSI_VQ_CCS)
        {
            ErrorCode = CCS_SetData(CH, HandleContext_p->PSIFeature.CCSVQData.StdMotionTh0,
                            HandleContext_p->PSIFeature.CCSVQData.StdMotionTh1);


            if(ErrorCode == FVDP_OK)
                HandleContext_p->UpdateFlags.PSIVQUpdateFlag &= ~UPDATE_PSI_VQ_CCS;
        }


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
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :

*******************************************************************************/
static FVDP_Result_t  SYSTEM_FE_ConfigureMcMadi(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_Frame_t* CurrentInputFrame_p = NULL;
    FVDP_Frame_t* PrevScanFrame_p = NULL;
    FVDP_Frame_t* CurrentProScanFrame_p = NULL;
    FVDP_CH_t     CH = HandleContext_p->CH;
    static bool   McMadiUpdateFlag[NUM_FVDP_CH] = {TRUE, TRUE, TRUE, TRUE};

    uint16_t      H_Size;
    uint16_t      V_Size;
    uint32_t      Out_Vsize;

    // Get the current output frame
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    PrevScanFrame_p  = SYSTEM_GetPrev1InputFrame(HandleContext_p);

    // Get the current output pro frame for scaler adjustment
    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);

    // Check that frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentInputFrame_p) == FALSE)
    {
        return FVDP_OK;
    }

    // Raise the update flag if current and previous frames are different in size
    if (SYSTEM_IsFrameValid(HandleContext_p,PrevScanFrame_p) == FALSE)
    {
        McMadiUpdateFlag[CH] |= TRUE;
    }
    else if ((PrevScanFrame_p->InputVideoInfo.Width != CurrentInputFrame_p->InputVideoInfo.Width)   ||
             (PrevScanFrame_p->InputVideoInfo.Height != CurrentInputFrame_p->InputVideoInfo.Height) ||
             (PrevScanFrame_p->OutputVideoInfo.Width != CurrentInputFrame_p->OutputVideoInfo.Width) ||
             (PrevScanFrame_p->OutputVideoInfo.Height != CurrentInputFrame_p->OutputVideoInfo.Height))
    {
        McMadiUpdateFlag[CH] |= TRUE;
    }

    // check that proscan frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p,CurrentProScanFrame_p) == FALSE)
    {
        return FVDP_OK;
    }

    H_Size = CurrentInputFrame_p->InputVideoInfo.Width;
    V_Size = CurrentInputFrame_p->InputVideoInfo.Height;

    if((CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED) && (HandleContext_p->LatencyType != FVDP_MINIMUM_LATENCY))
    {
        // if interlaced input, the output size would be 2x larger
        Out_Vsize =  2 * V_Size;
        Deint_DcdiUpdate ( TRUE, 0x4);
    }
    else
    {
        Out_Vsize =  V_Size;
        Deint_DcdiUpdate (FALSE, 0x0);
    }

    if (McMadiUpdateFlag[CH])
    {
        McMadiUpdateFlag[CH] = FALSE;

        if((CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED) && (HandleContext_p->LatencyType != FVDP_MINIMUM_LATENCY))
        {
            Madi_Enable();
            Mcdi_Enable(MCDI_MODE_BLEND_AUTO);
//            Mcdi_Enable(MCDI_MODE_OFF);
            Deint_Enable(DEINT_MODE_MACMADI, H_Size, V_Size, Out_Vsize);

            Madi_SetPixelNumberForMDThr(H_Size, V_Size);
            Madi_SetLASDThr(H_Size, V_Size);
        }
        else
        {
            Madi_Disable();
            Mcdi_Disable();
            Deint_Disable();
        }
    }

    // DID : TBC, dont think TNR need to be switched off for LowLat, but not sure it is usefull if no Madi
    if ((MadiCount) || (HandleContext_p->LatencyType == FVDP_MINIMUM_LATENCY))
    {
        Tnr_Bypas(H_Size, V_Size);
    }
    else
    {
        Tnr_Enable((CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED) ,H_Size,V_Size);
    }
    // If in interlace spatial mode, we use actual input size (not multiplied by 2, as for madi case)
    if (MadiCount > MADI_DELAY-1)
        Out_Vsize =   V_Size;
    // Update Deinterlace the output size
    CurrentProScanFrame_p->InputVideoInfo.Height = Out_Vsize;

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
static void SYSTEM_FE_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t* CurrentInputFrame_p;
    FVDP_Frame_t* CurrentProScanFrame_p;
    FVDP_Frame_t* Prev1InputFrame_p;
    FVDP_Frame_t* Prev2InputFrame_p;
    FVDP_Frame_t* Prev3InputFrame_p;
    MCC_Config_t* MccConfig_p;
    FVDP_FrameInfo_t* FrameInfo_p = &HandleContext_p->FrameInfo;

    VCD_MCC_AddrChange(C_Y_WR);
    VCD_MCC_AddrChange(C_RGB_UV_WR);
    VCD_MCC_AddrChange(D_Y_WR);
    VCD_MCC_AddrChange(D_UV_WR);
    VCD_MCC_AddrChange(P1_Y_RD);
    VCD_MCC_AddrChange(P1_UV_RD);
    VCD_MCC_AddrChange(P2_Y_RD);
    VCD_MCC_AddrChange(P2_UV_RD);
    VCD_MCC_AddrChange(P3_Y_RD);
    VCD_MCC_AddrChange(P3_UV_RD);
    VCD_MCC_AddrChange(MCDI_RD);
    VCD_MCC_AddrChange(MCDI_WR);

    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);
    Prev1InputFrame_p = SYSTEM_GetPrev1InputFrame(HandleContext_p);
    Prev2InputFrame_p = SYSTEM_GetPrev2InputFrame(HandleContext_p);
    Prev3InputFrame_p = SYSTEM_GetPrev3InputFrame(HandleContext_p);

    // Configure the PRO MCC clients if the current pro scan frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, CurrentProScanFrame_p) == TRUE)
    {
        if((CurrentProScanFrame_p->InputVideoInfo.ColorSpace == sRGB )
            || (CurrentProScanFrame_p->InputVideoInfo.ColorSpace == RGB)
            || (CurrentProScanFrame_p->InputVideoInfo.ColorSpace == RGB861))
        {
            // Disable the MCC client if there is no pro scan frame defined for it
            MCC_Disable(C_Y_WR);
        }
        else
        {
            // PRO_Y_WR
            // Configure PRO_Y_WR MCC Client (VCPU will set the base address)
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
            MCC_Config(C_Y_WR, *MccConfig_p);
            MCC_Enable(C_Y_WR);
            // VCD for C_Y_WR_ADDR
            {
                uint32_t buffaddr = 0;
                // DID TBC : if line commented below is functionally equivalent to uncommented one.
                // buffaddr = VCD_MCC_REG32_Read(FVDP_MCC_FE_BASE_ADDRESS, C_Y_WR_ADDR);
                buffaddr = REG32_Read(FVDP_MCC_FE_BASE_ADDRESS + C_Y_WR_ADDR);
                VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_C_Y_WR_ADDR, buffaddr);
             }
        }

        // PRO_UV_WR
        // Configure PRO_UV_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_PRO_U_UV_B];
        if((CurrentProScanFrame_p->InputVideoInfo.ColorSpace == sRGB )
            || (CurrentProScanFrame_p->InputVideoInfo.ColorSpace == RGB)
            || (CurrentProScanFrame_p->InputVideoInfo.ColorSpace == RGB861))
        {
            MccConfig_p->bpp = 10;
            MccConfig_p->rgb_mode = TRUE;
        }
        else
        {
            MccConfig_p->bpp = 10;
            MccConfig_p->rgb_mode = FALSE;
        }

        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->buffer_h_size = CurrentProScanFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentProScanFrame_p->InputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);
        MCC_Config(C_RGB_UV_WR, *MccConfig_p);
        MCC_Enable(C_RGB_UV_WR);
    }
    else
    {
        // Disable the MCC client if there is no pro scan frame defined for it
        MCC_Disable(C_Y_WR);
        MCC_Disable(C_RGB_UV_WR);
    }

    // Configure the C MCC clients if the current input frame is valid
    if ((SYSTEM_IsFrameValid(HandleContext_p, CurrentInputFrame_p) == TRUE)
        && (CurrentInputFrame_p->InputVideoInfo.ColorSpace != sRGB)
        && (CurrentInputFrame_p->InputVideoInfo.ColorSpace != RGB)
        && (CurrentInputFrame_p->InputVideoInfo.ColorSpace != RGB861))
    {
        // C_Y_WR
        // Configure C_Y_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_C_Y];
        MccConfig_p->bpp = 10;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);
        MCC_Config(D_Y_WR, *MccConfig_p);
        MCC_Enable(D_Y_WR);

        // C_UV_WR
        // Configure C_UV_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_C_UV];
        MccConfig_p->bpp = 10;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);
        MCC_Config(D_UV_WR, *MccConfig_p);
        MCC_Enable(D_UV_WR);
    }
    else
    {
        // Disable the MCC client if there is no input frame defined for it
        MCC_Disable(D_Y_WR);
        MCC_Disable(D_UV_WR);
    }

    // Configure the P1 MCC clients if the Prev1 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev1InputFrame_p) == TRUE)
    {
        // P1_Y_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev1InputFrame_p->FrameBuffer[BUFFER_C_Y] != NULL)
        {
            // Configure P1_Y_RD MCC Client
            MccConfig_p = &(Prev1InputFrame_p->FrameBuffer[BUFFER_C_Y]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(P1_Y_RD, *MccConfig_p);
            MCC_SetBaseAddr(P1_Y_RD, Prev1InputFrame_p->FrameBuffer[BUFFER_C_Y]->Addr, *MccConfig_p);
            MCC_SetOddSignal(P1_Y_RD, (Prev1InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(P1_Y_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P1_Y_RD);
        }

        // P1_UV_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev1InputFrame_p->FrameBuffer[BUFFER_C_UV] != NULL)
        {
            // Configure P1_Y_RD MCC Client
            MccConfig_p = &(Prev1InputFrame_p->FrameBuffer[BUFFER_C_UV]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(P1_UV_RD, *MccConfig_p);
            MCC_SetBaseAddr(P1_UV_RD, Prev1InputFrame_p->FrameBuffer[BUFFER_C_UV]->Addr, *MccConfig_p);
            MCC_Enable(P1_UV_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P1_UV_RD);
        }
    }
    else
    {
        MCC_Disable(P1_Y_RD);
        MCC_Disable(P1_UV_RD);
    }

    // Configure the P2 MCC clients if the Prev2 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev2InputFrame_p) == TRUE)
    {
        // P2_Y_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev2InputFrame_p->FrameBuffer[BUFFER_C_Y] != NULL)
        {
            // Configure P2_Y_RD MCC Client
            MccConfig_p = &(Prev2InputFrame_p->FrameBuffer[BUFFER_C_Y]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(P2_Y_RD, *MccConfig_p);
            MCC_SetBaseAddr(P2_Y_RD, Prev2InputFrame_p->FrameBuffer[BUFFER_C_Y]->Addr, *MccConfig_p);
            MCC_SetOddSignal(P2_Y_RD, (Prev2InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(P2_Y_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P2_Y_RD);
        }

        // P2_UV_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev2InputFrame_p->FrameBuffer[BUFFER_C_UV] != NULL)
        {
            // Configure P1_Y_RD MCC Client
            MccConfig_p = &(Prev2InputFrame_p->FrameBuffer[BUFFER_C_UV]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);
            MCC_Config(P2_UV_RD, *MccConfig_p);
            MCC_SetBaseAddr(P2_UV_RD, Prev2InputFrame_p->FrameBuffer[BUFFER_C_UV]->Addr, *MccConfig_p);
            MCC_Enable(P2_UV_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P2_UV_RD);
        }
    }
    else
    {
        MCC_Disable(P2_Y_RD);
        MCC_Disable(P2_UV_RD);
    }

    // Configure the P3 MCC clients if the Prev3 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev3InputFrame_p) == TRUE)
    {
        // P3_Y_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev3InputFrame_p->FrameBuffer[BUFFER_C_Y] != NULL)
        {
            // Configure P1_Y_RD MCC Client
            MccConfig_p = &(Prev3InputFrame_p->FrameBuffer[BUFFER_C_Y]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);

            // The
            MCC_Config(P3_Y_RD, *MccConfig_p);
            MCC_SetBaseAddr(P3_Y_RD, Prev3InputFrame_p->FrameBuffer[BUFFER_C_Y]->Addr, *MccConfig_p);
            MCC_SetOddSignal(P3_Y_RD, (Prev3InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(P3_Y_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P3_Y_RD);
        }

        // P3_UV_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev3InputFrame_p->FrameBuffer[BUFFER_C_UV] != NULL)
        {
            // Configure P1_Y_RD MCC Client
            MccConfig_p = &(Prev3InputFrame_p->FrameBuffer[BUFFER_C_UV]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);

            // The
            MCC_Config(P3_UV_RD, *MccConfig_p);
            MCC_SetBaseAddr(P3_UV_RD, Prev3InputFrame_p->FrameBuffer[BUFFER_C_UV]->Addr, *MccConfig_p);
            MCC_Enable(P3_UV_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(P3_UV_RD);
        }
    }
    else
    {
        MCC_Disable(P3_Y_RD);
        MCC_Disable(P3_UV_RD);
    }

    // Configure the MCDI_SB MCC clients if the Prev1 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev1InputFrame_p) == TRUE)
    {
        // MCDI_SB_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev1InputFrame_p->FrameBuffer[BUFFER_MCDI_SB] != NULL)
        {
            // Configure MCDI_SB_RD MCC Client
            MccConfig_p = &(Prev1InputFrame_p->FrameBuffer[BUFFER_MCDI_SB]->MccConfig);
            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);

            MCC_Config(MCDI_RD, *MccConfig_p);
            MCC_SetBaseAddr(MCDI_RD, Prev1InputFrame_p->FrameBuffer[BUFFER_MCDI_SB]->Addr, *MccConfig_p);
            MCC_SetOddSignal(MCDI_RD, (Prev1InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(MCDI_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(MCDI_RD);
        }
    }
    else
    {
        MCC_Disable(MCDI_RD);
    }

    // Configure the MCDI_SB_WR MCC clients if the current input frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, CurrentInputFrame_p) == TRUE)
    {
        // MCDI_SB_WR
        // Configure MCDI_SB_WR MCC Client (VCPU will set the base address)
        MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_MCDI_SB];
        MccConfig_p->bpp = 4;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
        MccConfig_p->buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height*2;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                MccConfig_p->bpp, MccConfig_p->rgb_mode);

        MCC_Config(MCDI_WR, *MccConfig_p);
        MCC_Enable(MCDI_WR);
    }
    else
    {
        // Disable the MCC client if there is no input frame defined for it
        MCC_Disable(MCDI_WR);
    }

    // Configure the CCS_RD MCC clients if the Prev2 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev2InputFrame_p) == TRUE)
    {
        // CCS_RD
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW] != NULL)
        {
            // Configure CCS_RD MCC Client
            MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_CCS_UV_RAW];
            MccConfig_p->bpp = 10;
            MccConfig_p->compression = MCC_COMP_NONE;
            MccConfig_p->rgb_mode = FALSE;
            MccConfig_p->buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
            MccConfig_p->buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height;

            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);

            MCC_Config(CCS_RD, *MccConfig_p);
            MCC_SetBaseAddr(CCS_RD, Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW]->Addr, *MccConfig_p);
            MCC_SetOddSignal(CCS_RD, (Prev2InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(CCS_RD);

            VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_CCS_RD_ADDR, Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW]->Addr);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(CCS_RD);
        }
    }
    else
    {
        MCC_Disable(CCS_RD);
    }

    // Configure the CCS_WR MCC clients if the Prev2 frame is valid
    if (SYSTEM_IsFrameValid(HandleContext_p, Prev2InputFrame_p) == TRUE)
    {
        // CCS_WR
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW] != NULL)
        {
            // Configure CCS_WR MCC Client
            MccConfig_p = &FrameInfo_p->CurrentMccConfig[BUFFER_CCS_UV_RAW];
            MccConfig_p->bpp = 10;
            MccConfig_p->compression = MCC_COMP_NONE;
            MccConfig_p->rgb_mode = FALSE;
            MccConfig_p->buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
            MccConfig_p->buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height;

            MccConfig_p->crop_enable = FALSE;
            MccConfig_p->Alternated_Line_Read = FALSE;
            MccConfig_p->stride = MCC_GetRequiredStrideValue(MccConfig_p->buffer_h_size,
                                    MccConfig_p->bpp, MccConfig_p->rgb_mode);

            MCC_Config(CCS_WR, *MccConfig_p);
            MCC_SetBaseAddr(CCS_WR, Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW]->Addr, *MccConfig_p);
            MCC_SetOddSignal(CCS_WR, (Prev2InputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(CCS_WR);

            VCD_ValueChange(HandleContext_p->CH, VCD_EVENT_CCS_WR_ADDR, Prev2InputFrame_p->FrameBuffer[BUFFER_CCS_UV_RAW]->Addr);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(CCS_WR);
        }
    }
    else
    {
        MCC_Disable(CCS_WR);
    }
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :

*******************************************************************************/
static FVDP_Result_t SYSTEM_FE_ConfigureScaler(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_Frame_t* CurrentProScanFrame_p = NULL;
    FVDP_Frame_t* CurrentInputFrame_p = NULL;
    FVDP_Frame_t* PrevProScanFrame_p = NULL;
    FVDP_CH_t     CH = HandleContext_p->CH;
    static bool   LiteScalerUpdateFlag[NUM_FVDP_CH] = {TRUE, TRUE, TRUE, TRUE};
    FVDP_ScalerLiteConfig_t ScalerConfig;
    uint16_t                BE_InputHWidth;
    uint16_t                BE_InputVHeight;

    // Get the current input and pro scan frames
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    CurrentProScanFrame_p = SYSTEM_GetCurrentProScanFrame(HandleContext_p);

    if ((SYSTEM_IsFrameValid(HandleContext_p,CurrentInputFrame_p) == FALSE) ||
        (SYSTEM_IsFrameValid(HandleContext_p,CurrentProScanFrame_p) == FALSE))
    {
        return FVDP_ERROR;
    }

    // Get the previous pro scan frame
    PrevProScanFrame_p = SYSTEM_GetPrevProScanFrame(HandleContext_p);

    if (SYSTEM_IsFrameValid(HandleContext_p,PrevProScanFrame_p) == FALSE )
    {
        LiteScalerUpdateFlag[CH] |= TRUE;
    }
    else if ((HandleContext_p->FrameInfo.PrevProScanWidth != CurrentProScanFrame_p->InputVideoInfo.Width)   ||
             (HandleContext_p->FrameInfo.PrevProScanHeight != CurrentProScanFrame_p->InputVideoInfo.Height) ||
             (PrevProScanFrame_p->OutputVideoInfo.Width != CurrentProScanFrame_p->OutputVideoInfo.Width) ||
             (PrevProScanFrame_p->OutputVideoInfo.Height != CurrentProScanFrame_p->OutputVideoInfo.Height))
    {
        LiteScalerUpdateFlag[CH] |= TRUE;
    }
    HandleContext_p->FrameInfo.PrevProScanWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
    HandleContext_p->FrameInfo.PrevProScanHeight = CurrentProScanFrame_p->InputVideoInfo.Height;

    ScalerConfig.ProcessingRGB = (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC);

    // Setup parameters of horizontal scaling
    ScalerConfig.InputFormat.ColorSampling = CurrentProScanFrame_p->InputVideoInfo.ColorSampling;
    ScalerConfig.InputFormat.ColorSpace = CurrentProScanFrame_p->InputVideoInfo.ColorSpace;
    ScalerConfig.InputFormat.HWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
    ScalerConfig.OutputFormat.HWidth = CurrentProScanFrame_p->OutputVideoInfo.Width;
    ScalerConfig.InputFormat.VHeight = CurrentProScanFrame_p->InputVideoInfo.Height;
    ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;

    //Horizontal lite
    if (CurrentProScanFrame_p->InputVideoInfo.Width <= CurrentProScanFrame_p->OutputVideoInfo.Width)
    {
        // not shrink, lite is in 1:1 condition
        ScalerConfig.OutputFormat.HWidth = CurrentProScanFrame_p->InputVideoInfo.Width;
    }

    BE_InputHWidth = ScalerConfig.OutputFormat.HWidth;
    // Configure the Horizontal  Scaler
    if (LiteScalerUpdateFlag[CH])
    {
        HScaler_Lite_HWConfig(FVDP_MAIN, ScalerConfig);
    }

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
    if (MadiCount == ( MADI_DELAY-2))
        LiteScalerUpdateFlag[CH] |= TRUE;
    //Vertical lite
    if((CurrentProScanFrame_p->InputVideoInfo.ScanType == INTERLACED)
        && (CurrentProScanFrame_p->OutputVideoInfo.ScanType == PROGRESSIVE))
    {
        if((CurrentProScanFrame_p->OutputVideoInfo.Height > (CurrentProScanFrame_p->InputVideoInfo.Height * 2))
            || (MadiCount < ( MADI_DELAY-1)))
        {
            ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->InputVideoInfo.Height * 2;
        }
        else
        {
            ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;
        }

        BE_InputVHeight = ScalerConfig.OutputFormat.VHeight;
        CurrentProScanFrame_p->InputVideoInfo.ScanType = PROGRESSIVE;
    }
    else
    {
        if (CurrentProScanFrame_p->InputVideoInfo.Height <= CurrentProScanFrame_p->OutputVideoInfo.Height)
        {
            if (MadiCount < ( MADI_DELAY-1))
            {
                // not shrink, lite is in 1:1 condition
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->InputVideoInfo.Height;
            }
            else
            {
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;
            }
        }
        else
        {
            //The interlace output case, Sclaer Lite output set to two times bigger than output
            if(CurrentProScanFrame_p->OutputVideoInfo.ScanType == INTERLACED)
            {
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height * MAXIMUM_SHRINK_RATIO;
            }
            else
            {
                ScalerConfig.OutputFormat.VHeight = CurrentProScanFrame_p->OutputVideoInfo.Height;
            }
        }
        BE_InputVHeight = ScalerConfig.OutputFormat.VHeight;
    }

    VScaler_Lite_OffsetUpdate(FVDP_MAIN, ScalerConfig);

    if (LiteScalerUpdateFlag[CH])
    {
        LiteScalerUpdateFlag[CH] = FALSE;

        // Configure the Vertical Scaler
        VScaler_Lite_HWConfig(FVDP_MAIN, ScalerConfig);

        // Configure the FVDP_DCDi
        if (HandleContext_p->ProcessingType != FVDP_SPATIAL)  // Use DEINT DCDi
        {
            if(CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {
                VScaler_Lite_DcdiUpdate(HandleContext_p->CH, FALSE, 0x0);
            }
            else
            {
                VScaler_Lite_DcdiUpdate(HandleContext_p->CH, TRUE, 0xF);
            }
        }
        else  // Use SCALER LITE DCDi
        {
            if(CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
            {
                VScaler_Lite_DcdiUpdate(HandleContext_p->CH, TRUE, 0xF);
            }
            else
            {
                VScaler_Lite_DcdiUpdate(HandleContext_p->CH, FALSE, 0);
            }
        }
    }

    CurrentProScanFrame_p->InputVideoInfo.Width = BE_InputHWidth;
    CurrentProScanFrame_p->InputVideoInfo.Height = BE_InputVHeight;

    return ErrorCode;

}

/*******************************************************************************
Name        : SYSTEM_FE_PowerDown
Description : Power down FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_FE_PowerDown(void)
{
    // Disable the MCC client
    MCC_Disable(C_Y_WR);
    MCC_Disable(C_RGB_UV_WR);
    MCC_Disable(D_Y_WR);
    MCC_Disable(D_UV_WR);
    MCC_Disable(P1_Y_RD);
    MCC_Disable(P1_UV_RD);
    MCC_Disable(P2_Y_RD);
    MCC_Disable(P2_UV_RD);
    MCC_Disable(P3_Y_RD);
    MCC_Disable(P3_UV_RD);
    MCC_Disable(MCDI_RD);
    MCC_Disable(MCDI_WR);

    // Disable IP blocks
    Madi_Disable();
    Mcdi_Disable();
    Deint_Disable();

    // Disable clock of IP blocks
    EventCtrl_ClockCtrl_FE(MODULE_CLK_ALL_FE, GND_CLK);

    HostUpdate_HardwareUpdate(FVDP_MAIN, HOST_CTRL_FE, FORCE_UPDATE);

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_FE_PowerUp
Description : Power down FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_FE_PowerUp(void)
{
    FVDP_HandleContext_t* HandleContext_p;

    HandleContext_p = SYSTEM_GetHandleContextOfChannel(FVDP_MAIN);

    // MAIN Channel has been never opened before
    if (HandleContext_p == NULL)
    {
        return FVDP_OK;
    }

    //Set Hard reset FE block
    EventCtrl_HardReset_FE(HARD_RESET_ALL_FE, TRUE);

    //INIT local variables

    //Clear Hard reset FE block
    EventCtrl_HardReset_FE(HARD_RESET_ALL_FE, FALSE);

    // Initialize the VMUX HW
    SYSTEM_VMUX_Init(FVDP_MAIN);

    //Initialize FVDP_FE software module and call FVDP_FE hardware init function
    SYSTEM_FE_Init(HandleContext_p);

    // Initialize the MCC HW is done in SYSTEM_BE_PowerUp

    HostUpdate_HardwareUpdate(FVDP_MAIN, HOST_CTRL_FE, FORCE_UPDATE);

    return FVDP_OK;
}
