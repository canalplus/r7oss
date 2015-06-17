/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_ivp.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_regs.h"
#include "fvdp_ivp.h"
#include "fvdp_hostupdate.h"

/* Register Access Macros------------------------------------------------ */

// VMUX register access Macros
#define VMUX_REG32_Write(Addr,Val)              REG32_Write(Addr+VMUX_BASE_ADDRESS,Val)
#define VMUX_REG32_Read(Addr)                   REG32_Read(Addr+VMUX_BASE_ADDRESS)
#define VMUX_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+VMUX_BASE_ADDRESS,BitsToSet)
#define VMUX_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+VMUX_BASE_ADDRESS,BitsToClear)
#define VMUX_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+VMUX_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

static const uint32_t ISM_BASE_ADDR[] = {ISM_MAIN_BASE_ADDRESS, ISM_PIP_BASE_ADDRESS, ISM_VCR_BASE_ADDRESS, ISM_ENC_BASE_ADDRESS};
#define ISM_REG32_Write(Ch,Addr,Val)            REG32_Write(Addr+ISM_BASE_ADDR[Ch],Val)
#define ISM_REG32_Read(Ch,Addr)                 REG32_Read(Addr+ISM_BASE_ADDR[Ch])
#define ISM_REG32_Set(Ch,Addr,BitsToSet)        REG32_Set(Addr+ISM_BASE_ADDR[Ch],BitsToSet)
#define ISM_REG32_Clear(Ch,Addr,BitsToClear)    REG32_Clear(Addr+ISM_BASE_ADDR[Ch],BitsToClear)
#define ISM_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+ISM_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
// IVP register access Macros
static const uint32_t IVP_BASE_ADDR[] = {MIVP_BASE_ADDRESS, PIVP_BASE_ADDRESS, VIVP_BASE_ADDRESS, EIVP_BASE_ADDRESS};
#define IVP_REG32_Write(Ch,Addr,Val)            REG32_Write(Addr+IVP_BASE_ADDR[Ch],Val)
#define IVP_REG32_Read(Ch,Addr)                 REG32_Read(Addr+IVP_BASE_ADDR[Ch])
#define IVP_REG32_Set(Ch,Addr,BitsToSet)        REG32_Set(Addr+IVP_BASE_ADDR[Ch],BitsToSet)
#define IVP_REG32_Clear(Ch,Addr,BitsToClear)    REG32_Clear(Addr+IVP_BASE_ADDR[Ch],BitsToClear)
#define IVP_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+IVP_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)

// MISC_FE register access Macros
#define MISC_FE_REG32_Write(Addr,Val)           REG32_Write(Addr+MISC_FE_BASE_ADDRESS,Val)
#define MISC_FE_REG32_Read(Addr)                REG32_Read(Addr+MISC_FE_BASE_ADDRESS)
#define MISC_FE_REG32_Set(Addr,BitsToSet)       REG32_Set(Addr+MISC_FE_BASE_ADDRESS,BitsToSet)
#define MISC_FE_REG32_Clear(Addr,BitsToClear)   REG32_Clear(Addr+MISC_FE_BASE_ADDRESS,BitsToClear)
#define MISC_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+MISC_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

// ISM_MAIN, ISM_PIP, ISM_VCR, ISM_ENC register access Macros
//
// The 4 IVP_SOURCE register blocks (IMP_SOURCE, IPP_SOURCE, VCR_SOURCE, ENC_SOURCE)
// only contain one idential register with relative address 0.
// Therefore, we define a register name "IVP_SOURCE" with relative address 0
// and use the register block base addresses based on Channel.
#define IVP_SOURCE                                                                        0x00000000 //RW POD:0x0
#define IVP_SRC_SEL_MASK                                                                  0x0000001f
#define IVP_SRC_SEL_OFFSET                                                                         0
#define IVP_SRC_SEL_WIDTH                                                                          5
#define IVP_HS_INV_MASK                                                                   0x00000020
#define IVP_HS_INV_OFFSET                                                                          5
#define IVP_HS_INV_WIDTH                                                                           1
#define IVP_VS_INV_MASK                                                                   0x00000040
#define IVP_VS_INV_OFFSET                                                                          6
#define IVP_VS_INV_WIDTH                                                                           1
#define IVP_DV_INV_MASK                                                                   0x00000080
#define IVP_DV_INV_OFFSET                                                                          7
#define IVP_DV_INV_WIDTH                                                                           1
//#define IVP_ODD_INV_MASK                                                                  0x00000100
//#define IVP_ODD_INV_OFFSET                                                                         8
//#define IVP_ODD_INV_WIDTH                                                                          1
#define IVP_SOURCE_RSVD_MASK                                                              0x00000200
#define IVP_SOURCE_RSVD_OFFSET                                                                     9
#define IVP_SOURCE_RSVD_WIDTH                                                                      1
#define IVP_EXT_AFB_AFR_SRC_SEL_MASK                                                      0x00000400
#define IVP_EXT_AFB_AFR_SRC_SEL_OFFSET                                                            10
#define IVP_EXT_AFB_AFR_SRC_SEL_WIDTH                                                              1
#define IVP_EXT_AFB_MASK                                                                  0x00000800
#define IVP_EXT_AFB_OFFSET                                                                        11
#define IVP_EXT_AFB_WIDTH                                                                          1

static const uint32_t IVP_SOURCE_BASE_ADDR[] = {ISM_MAIN_BASE_ADDRESS, ISM_PIP_BASE_ADDRESS, ISM_VCR_BASE_ADDRESS, ISM_ENC_BASE_ADDRESS};
#define IVP_SOURCE_REG32_Write(Ch,Addr,Val)           REG32_Write(Addr+IVP_SOURCE_BASE_ADDR[Ch],Val)
#define IVP_SOURCE_REG32_Read(Ch,Addr)                REG32_Read(Addr+IVP_SOURCE_BASE_ADDR[Ch])
#define IVP_SOURCE_REG32_Set(Ch,Addr,BitsToSet)       REG32_Set(Addr+IVP_SOURCE_BASE_ADDR[Ch],BitsToSet)
#define IVP_SOURCE_REG32_Clear(Ch,Addr,BitsToClear)   REG32_Clear(Addr+IVP_SOURCE_BASE_ADDR[Ch],BitsToClear)
#define IVP_SOURCE_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                      REG32_ClearAndSet(Addr+IVP_SOURCE_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */
#define IVP_FRAME_RESET_VALUE                  0x5
#define MAX_MEMORY_CROP_H_START                32
#define MAX_MEMORY_CROP_V_START                16

/* Private Macros ----------------------------------------------------------- */

//#define TrIVP(level,...)  TraceModuleOn(level,IVP_MODULE[CH].DebugLevel,__VA_ARGS__)
#define TrIVP(level,...)

/* Private Variables (static)------------------------------------------------ */
static uint32_t VStartOdd[NUM_FVDP_CH] = {0,0,0,0};

/*
static HostUpdate_SyncUpdateRequestFlags_t const syncupd[NUM_FVDP_CH] = \
    {SYNC_UPDATE_REQ_IVP_MAIN, SYNC_UPDATE_REQ_IVP_PIP, SYNC_UPDATE_REQ_IVP_VCR};
*/
//static bool DataCheckEnableStatus[NUM_FVDP_CH] = {FALSE,FALSE,FALSE};
static bool ivp_IsDEModeApplicable(FVDP_CH_t CH);
static void IVP_MeasureCtrlUpdate(FVDP_CH_t CH);
//static FVDP_Result_t ivp_SetDataCheckState(FVDP_CH_t CH, bool state);
//static bool ivp_GetDataCheckState(FVDP_CH_t CH);
//static uint32_t ivp_GetDataCheckResult(FVDP_CH_t CH);
/* Private Function Prototypes ---------------------------------------------- */



/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t IVP_Init (FVDP_CH_t CH)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    IVP_IbdSet(CH, IVP_IBD_ENABLE|IVP_IBD_DE, 1);
    IVP_MeasureSet(CH, IVP_MEASURE_ENABLE|IVP_MEASURE_WIN_ENABLE, 0, 0);

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
FVDP_Result_t IVP_Term (FVDP_CH_t CH)
{
    FVDP_Result_t result = FVDP_OK;

    return result;
}

/*******************************************************************************
Name        : static FVDP_Result_t ivp_HWSetup(FVDP_CH_t CH, FVDP_Frame_t* Frame_p)
Description : Set input configuration - attribute and window
Parameters  :   IN: CH
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t IVP_Update(FVDP_CH_t          CH,
                        FVDP_VideoInfo_t*  InputVideoInfo_p,
                        FVDP_CropWindow_t* CropWindow_p,
                        FVDP_RasterInfo_t* InputRasterInfo_p,
                        FVDP_ProcessingType_t ProcessingType)
{
    FVDP_Result_t Error = FVDP_OK;
    IVP_CscFlag_t IVP_CscFlag = 0;
    IVP_Attr_t IVP_AttrFlag = IVP_FREEZE_OFF|IVP_SET_BLACK_OFFS_OFF;

    if (ivp_IsDEModeApplicable(CH))
    {
        IVP_AttrFlag |= IVP_SET_DV_ON;
    }
    else
    {
        IVP_AttrFlag |= IVP_SET_DV_OFF;
    }

    if (InputVideoInfo_p->ScanType == INTERLACED)
    {
        IVP_AttrFlag |= IVP_SET_INTERLACED;
    }
    else
    {
        IVP_AttrFlag |= IVP_SET_PROGRESSIVE;
    }

    if ((Error = IVP_AttrSet(CH, IVP_AttrFlag)) != FVDP_OK)
    {
        TrIVP(DBG_ERROR, "(%d): IVP_AttrSet Flag:0x%x ErrorCode=%d...\n",
                    CH,IVP_AttrFlag,Error);
        return Error;
    }

    Error = IVP_WindowAdjust(CH, InputVideoInfo_p, CropWindow_p, InputRasterInfo_p);
    if (Error != FVDP_OK)
    {
        TrIVP(DBG_ERROR, "(%d): IVP_WindowAdjust ErrorCode=%d...\n",
                    CH, Error);
        return Error;
    }

    if ((InputVideoInfo_p->ColorSpace == RGB) || (InputVideoInfo_p->ColorSpace == sRGB) || (InputVideoInfo_p->ColorSpace == RGB861))
    {
        IVP_CscFlag |= IVP_444_RGB;
    }
    else
    {
        if(InputVideoInfo_p->ColorSampling==FVDP_444)
        {
            IVP_CscFlag |= IVP_444_YUV;
            InputVideoInfo_p->ColorSampling= FVDP_422;
        }
        else if(InputVideoInfo_p->ColorSampling==FVDP_422)
            IVP_CscFlag |= IVP_422_YUV;
        else if(InputVideoInfo_p->ColorSampling==FVDP_420)
        {
            IVP_CscFlag |= IVP_420_YUV;
            InputVideoInfo_p->ColorSampling = FVDP_422;
        }
        else
            return FVDP_ERROR;
    }

    if ((InputVideoInfo_p->ColorSpace == RGB) || (InputVideoInfo_p->ColorSpace==sRGB) || (InputVideoInfo_p->ColorSpace==RGB861) )
    {
        if(ProcessingType != FVDP_RGB_GRAPHIC)
        {
            // EricB ToDo:  Need to handle RGB path and not always convert to YUV at the input
            IVP_CscFlag |= IVP_RGB_TO_YUV_940;
            InputVideoInfo_p->ColorSpace = sdYUV;
            InputVideoInfo_p->ColorSampling= FVDP_422;
        }
    }

    if ((Error = IVP_CscSet(CH, IVP_CscFlag, 0)) != FVDP_OK)
    {
        TrIVP(DBG_ERROR, "(%d): IVP_CscSet Flag:0x%x ErrorCode=%d...\n",
                    CH,IVP_CscFlag,Error);
        return Error;
    }

    /* Input Capture Enable */
    if ((Error = IVP_AttrSet(CH, IVP_ENABLED)) != FVDP_OK)
    {
        TrIVP(DBG_ERROR, "(%d): IVP_AttrSet Flag:0x%x ErrorCode=%d...\n",
                    CH,IVP_ENABLED,Error);
        return Error;
    }

    return Error;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_FieldType_t IVP_GetPrevFieldType(FVDP_CH_t CH)
{
    // This register value contain previous field infomation
    if (IVP_REG32_Read(CH, IVP_IBD_VTOTAL)&0x1)
        return TOP_FIELD;
    else
        return BOTTOM_FIELD;
}

/*******************************************************************************
Name        : static FVDP_Result_t IVP_WindowAdjust(FVDP_CH_t CH)
Description : Adjust input window adjust
Parameters  :   IN: CH
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t IVP_WindowAdjust(FVDP_CH_t          CH,
                              FVDP_VideoInfo_t*  InputVideoInfo_p,
                              FVDP_CropWindow_t* CropWindow_p,
                              FVDP_RasterInfo_t* InputRasterInfo_p)
{
    IVP_Capture_t Capture;
    IVP_CaptureFlag_t CaptureFlag = 0;
    FVDP_Result_t Error = FVDP_OK;
    int16_t PositionX = 0;
    int16_t PositionY = 0;

// vmux move begin
    PositionX = CropWindow_p->HCropStart;
    PositionY = CropWindow_p->VCropStart;

    /* Input capture */
    InputVideoInfo_p->Width = (InputVideoInfo_p->Width + 1) & ~1L; // ceil at even

    // limit crop window horizontal position and size to active window
    if (CropWindow_p->HCropWidth > InputVideoInfo_p->Width)
        CropWindow_p->HCropWidth = InputVideoInfo_p->Width;
    if (CropWindow_p->HCropWidth + CropWindow_p->HCropStart > InputVideoInfo_p->Width)
        CropWindow_p->HCropStart = InputVideoInfo_p->Width - CropWindow_p->HCropWidth;

    // limit crop window vertical position and size to active window
    if (CropWindow_p->VCropHeight > InputVideoInfo_p->Height)
        CropWindow_p->VCropHeight = InputVideoInfo_p->Height;
    if (CropWindow_p->VCropHeight + CropWindow_p->VCropStart > InputVideoInfo_p->Height)
        CropWindow_p->VCropStart = InputVideoInfo_p->Height - CropWindow_p->VCropHeight;

    // adjust the horizontal input size for required cropping
    if ((InputVideoInfo_p->Width != CropWindow_p->HCropWidth) && (CropWindow_p->HCropWidth != 0))
    {
        CropWindow_p->HCropWidth = (CropWindow_p->HCropWidth) & ~1L;
        Capture.Width = CropWindow_p->HCropWidth;
        InputVideoInfo_p->Width = CropWindow_p->HCropWidth;
    }
    else
    {
        Capture.Width = InputVideoInfo_p->Width;
    }

    // adjust the vertical input size for required cropping
    if ((InputVideoInfo_p->Height != CropWindow_p->VCropHeight) && (CropWindow_p->VCropHeight != 0))
    {
        Capture.Height = CropWindow_p->VCropHeight;
        InputVideoInfo_p->Height = CropWindow_p->VCropHeight;
    }
    else
    {
        Capture.Height = InputVideoInfo_p->Height;
    }

    if (ivp_IsDEModeApplicable(CH))
    {
        if (PositionX >= 0)
            Capture.HSyncDelay = PositionX;
        else
            return FVDP_ERROR;

        if (PositionY >= 0)
        {
            Capture.VSyncDelay = PositionY;

            // FVDP H/W feature: VSyncDelay need to align with even number for YUV 4:2:0.
            // Reason: YUV 4:2:0 upstream convert moved to FE block from IVP.Valid for Main channel.
            // PIP/AUX: [Capture window] -> [color matrix] -> [420-to-422 converter]
            if (InputVideoInfo_p->ColorSampling == FVDP_420)
            {
                Capture.VSyncDelay = (Capture.VSyncDelay) & ~1L;
            }
        }
        else
            return FVDP_ERROR;

        if (InputVideoInfo_p->ColorSampling != FVDP_444)//only valid for 4:2:2
        {
            if (InputRasterInfo_p->UVAlign == TRUE)
            {
                Capture.HSyncDelay |= 1L;                   // Make Odd
            }
            else
            {
                Capture.HSyncDelay &= ~1L;                  // Make Even
            }
        }
        Capture.HStart = InputRasterInfo_p->HStart;
        Capture.VStart = InputRasterInfo_p->VStart;
    }
    else
    {
        /* Video */
        if (InputVideoInfo_p->ColorSpace==sdYUV||InputVideoInfo_p->ColorSpace==hdYUV||InputVideoInfo_p->ColorSpace==xvYUV)
        {
            Capture.HSyncDelay = 0;
            Capture.VSyncDelay = 0;
            if (PositionX + InputRasterInfo_p->HStart >= 0)
                Capture.HStart = PositionX + InputRasterInfo_p->HStart;
            else
                return FVDP_ERROR;

            if (PositionY + InputRasterInfo_p->VStart >= 0)
                Capture.VStart = PositionY + InputRasterInfo_p->VStart;
            else
                return FVDP_ERROR;

            if (InputVideoInfo_p->ColorSampling!=FVDP_444)//only valid for 4:2:2
            {
                if (InputRasterInfo_p->UVAlign==TRUE)
                    Capture.HStart |= 1L;    // Make Odd
                else
                    Capture.HStart &= ~1L;   // Make Even
            }
        }
        else
        {
            if (PositionX >= 0)
            {
                Capture.HSyncDelay = PositionX;
                Capture.HStart = InputRasterInfo_p->HStart;
            }
            else
            {
                Capture.HSyncDelay = 0;
                if (PositionX + InputRasterInfo_p->HStart >= 0)
                    Capture.HStart = PositionX + InputRasterInfo_p->HStart;
                else
                    return FVDP_ERROR;
            }

            if (PositionY >= 0)
            {
                Capture.VSyncDelay = PositionY;
                Capture.VStart = InputRasterInfo_p->VStart;
            }
            else
            {
                Capture.VSyncDelay = 0;
                if (PositionY + InputRasterInfo_p->VStart >= 0)
                    Capture.VStart = PositionY + InputRasterInfo_p->VStart;
                else
                    return FVDP_ERROR;
            }
            if (InputVideoInfo_p->ColorSampling!=FVDP_444)//only valid for 4:2:2
            {
                if (PositionX >= 0)
                {
                    // sum of HSyncDelay and Hstart should follow UVAlign setting
                    if ((Capture.HSyncDelay + Capture.HStart) & 1L)// if odd
                    {
                        if (InputRasterInfo_p->UVAlign==FALSE)
                        {
                            Capture.HSyncDelay += 1L;   // Make Even
                        }
                    }
                    else // if even
                    {
                        if (InputRasterInfo_p->UVAlign==TRUE)
                        {
                            Capture.HSyncDelay += 1L;   // Make Odd
                        }
                    }
                }
                else
                {
                    if (InputRasterInfo_p->UVAlign==TRUE)
                        Capture.HStart |= 1L;    // Make Odd
                    else
                        Capture.HStart &= ~1L;   // Make Even
                }
            }
        }
    }

    // update to input video info
    InputVideoInfo_p->Width = Capture.Width;
    InputVideoInfo_p->Height = Capture.Height;

// vmux move end
    if(InputRasterInfo_p->FieldAlign)
        CaptureFlag |= IVP_CAPTURE_SYNC_EVEN_OFFSET;

    if ((Error = IVP_CaptureSet(CH, CaptureFlag, &Capture)) != FVDP_OK)
    {
        TrIVP(DBG_ERROR, "(%d): IVP_CaptureSet Flag:0x%x ErrorCode=%d...\n",
                    CH, CaptureFlag, Error);
        return Error;
    }

    return FVDP_OK;
}


/*******************************************************************************
Description:    Should DE mode be used for the selected source

Parameters:     CH: Input Chanel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)

Returns:        TRUE:  The source supports using DE mode.
                FALSE: The source does not support DE mode.
*******************************************************************************/
static bool ivp_IsDEModeApplicable(FVDP_CH_t CH)
{
    IVP_Src_t Source;

    Source = IVP_SourceGet(CH);

    // EricB ToDo:  Since VTAC input can either be digital or analog, how do we
    //              determine if DE mode is to be used?

    if ((Source == IVP_SRC_MDTP0_YUV) ||
        (Source == IVP_SRC_MDTP0_RGB) ||
        (Source == IVP_SRC_MDTP0_YUV_3D) ||
        (Source == IVP_SRC_MDTP1_YUV) ||
        (Source == IVP_SRC_MDTP1_RGB) ||
        (Source == IVP_SRC_MDTP2_YUV) ||
        (Source == IVP_SRC_MDTP2_RGB) ||
        (Source == IVP_SRC_VTAC1) ||
        (Source == IVP_SRC_VTAC2) ||
        (Source == IVP_SRC_DIP))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
Description:    Set input source

Parameters:     CH: Input Chanel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)
                Source: see IVP_Src_t

Returns:        FVDP_ERROR/FVDP_OK
*******************************************************************************/
FVDP_Result_t IVP_SourceSet(FVDP_CH_t CH, IVP_Src_t Source)
{
    uint32_t const ivp_clk_sel_mask[NUM_FVDP_CH] = {MAIN_CLK_SEL_MASK, PIP_CLK_SEL_MASK, VCR_CLK_SEL_MASK, ENC_CLK_SEL_MASK};
    uint32_t const ivp_clk_sel_offs[NUM_FVDP_CH] = {MAIN_CLK_SEL_OFFSET, PIP_CLK_SEL_OFFSET, VCR_CLK_SEL_OFFSET, ENC_CLK_SEL_OFFSET};

    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    if (Source >= IVP_SRC_NUM)
    {
        return FVDP_ERROR;
    }

    IVP_SOURCE_REG32_ClearAndSet(CH, IVP_SOURCE, IVP_SRC_SEL_MASK, IVP_SRC_SEL_OFFSET, Source);
    // Set the IVP Channel clock as selected by the ISM
    VMUX_REG32_ClearAndSet(VMUX_CLOCK_CONFIG, ivp_clk_sel_mask[CH], ivp_clk_sel_offs[CH], 2);

    return FVDP_OK;
}

/*******************************************************************************
Description:    Return input source enumerator based on channel and h/w settings

Parameters:     CH: Input Chanel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)

Returns:        IVP_Src_t - The currently selected channel.
*******************************************************************************/
IVP_Src_t IVP_SourceGet(FVDP_CH_t CH)
{
    IVP_Src_t Source;

    if (CH >= NUM_FVDP_CH)
    {
        return IVP_SRC_NUM;
    }

    Source = (IVP_Src_t)(IVP_SOURCE_REG32_Read(CH, IVP_SOURCE) & IVP_SRC_SEL_MASK);

    return Source;
}

/*******************************************************************************
Description:    Set up input attributes

Parameters:   CH: Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)

                Flag - Control flags
                    IVP_ENABLED: set input capture enabled
                    IVP_DISABLED: set input capture disabled
                    IVP_SET_INTERLACED: turn on Interlaced input scan
                    IVP_SET_PROGRESSIVE: turn on Progressive input
                    IVP_SET_DV_ON: turn Input to be slave to Data Valid (DVI/HDMI/DP)
                    IVP_SET_DV_OFF: turn Input from DE slave to normal Sync
                    IVP_FREEZE_ON: freeze window
                    IVP_FREEZE_OFF: unfreeze window
                    IVP_SET_BLACK_OFFS_ON: add 0x40 offset to incoming luma data.set black level of data captured from ADC
                    IVP_SET_BLACK_OFFS_OFF: turn black offset off


Returns:        FVDP_ERROR/FVDP_OK

Example:
                //to setup sync driven interlaced input for main channel
                IVPutAttrSet(FVDP_MAIN, IVP_SET_INTERLACED|IVP_SET_DV_OFF);

                //to unfreeze input capture on Main channel:
                IVPutAttrSet(FVDP_MAIN, IVP_FREEZE_OFF);

*******************************************************************************/
FVDP_Result_t IVP_AttrSet(FVDP_CH_t CH, IVP_Attr_t Flag)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    if (Flag & (IVP_DISABLED | IVP_ENABLED)) /*do turn on input capture ?*/
    {
        if (Flag & IVP_ENABLED)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_RUN_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_RUN_EN_MASK);
        }
    }

    // EricB ToDo:  IVP_CONTROL:IVP_INTC_EN flag may need to be changed for dynamic intc/prog input changes
    //               However since InputUpdate() calls may happen during active video time and IVP_CONTROL
    //               is a RW register, this could be a problem.
    if (Flag & (IVP_SET_INTERLACED|IVP_SET_PROGRESSIVE)) /*do turn on Interlaced input?*/
    {
        if (Flag & IVP_SET_INTERLACED)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_INTLC_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_INTLC_EN_MASK);
        }
    }

    if (Flag & (IVP_SET_DV_OFF|IVP_SET_DV_ON)) /*do we need switch Capture mode to slave to Data Valid signal*/
    {
        if (Flag & IVP_SET_DV_ON)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_EXT_DV_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_EXT_DV_EN_MASK);
        }
    }

    if (Flag & (IVP_SET_BLACK_OFFS_OFF|IVP_SET_BLACK_OFFS_ON))
    {
        if (Flag & IVP_SET_BLACK_OFFS_ON)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_BLACK_OFFSET_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_BLACK_OFFSET_MASK);
        }
    }

    if (Flag & (IVP_FREEZE_OFF|IVP_FREEZE_ON)) /*do Freeze/UnFreeze ?*/
    {
        if (Flag & IVP_FREEZE_ON)
        {
            IVP_REG32_Set(CH, IVP_FREEZE_VIDEO, IVP_FREEZE_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_FREEZE_VIDEO, IVP_FREEZE_MASK);
        }

        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    return FVDP_OK;
}

/*******************************************************************************
//MIVP_IP_MEASURE_CTRL:MIVP_TOTAL_PIX requires support of firmware, particularly:
//to be updated everytime when input measure window size is changed.
//That function is being called from IVP_MeasureSet and IVP_CaptureSet functions.
*******************************************************************************/
static void IVP_MeasureCtrlUpdate(FVDP_CH_t CH)
{
    uint32_t u1, u2;

    VMUX_REG32_Clear(VMUX_PBUS_CTRL, IVP_READ_ACTIVE_MASK); /*to read pending register.*/
    if (IVP_REG32_Read(CH, IVP_IP_MEASURE_CTRL) & IVP_IP_MEASURE_WIN_EN_MASK)
    {
        u1 = IVP_REG32_Read(CH, IVP_HMEASURE_WIDTH);
        u2 = IVP_REG32_Read(CH, IVP_VMEASURE_LENGTH);
    }
    else
    {
        u1 = IVP_REG32_Read(CH, IVP_H_ACT_WIDTH);
        u2 = IVP_REG32_Read(CH, IVP_V_ACT_LENGTH);
    }

    /*calculation TOTAL_PIX param = ceil(log2(width*heights))*/
    u1 *= u2;
    for (u2 = 0; u1; u2++,u1>>=1);

    IVP_REG32_ClearAndSet(CH, IVP_IP_MEASURE_CTRL, IVP_TOTAL_PIX_MASK, IVP_TOTAL_PIX_OFFSET, u2);

    HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
}

/*******************************************************************************
Description:
    Control Active(Capture) Window hardware block settings

Parameters:
    CH - Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX)

    Flag - Control flags:
        IVP_CAPTURE_NO_HORIZ_UPDATE: function will not update horizontal settings (HSyncDelay, HStart, Width),
        IVP_CAPTURE_NO_VERT_UPDATE: function will not update vertical settings (VSyncDelay, VStart, Height),
        IVP_CAPTURE_NO_POSITION_UPDATE: function will not update position settings (HDelay, HStart, VDelay, VStart),
        IVP_CAPTURE_NO_SIZE_UPDATE: function will not update size settings (Width, Height),
        IVP_CAPTURE_SYNC_EVEN_OFFSET: port related even field sync alignment (legacy: gmd_SYNC_EVEN_OFFSET)

    IVPutCapture_t* pVal - Pointer to numerical value structure.

Return: FVDP_OK/FVDP_ERROR

examples:
    //to update PIP input capture window coordinates :
    const IVP_Capture_t cptWnd = {0,0,10,5, 1024, 800};
    IVP_CaptureSet(FVDP_PIP, 0, &cptWnd);

    //to update VCR input only horizontal coordinates:
    IVP_Capture_t cWnd;
    cWnd.HSyncDelay = 0;
    cWnd.HStart = 10;
    cWnd.Width = 1024;
    IVP_CaptureSet(FVDP_AUX, IVP_CAPTURE_NO_VERT_UPDATE, & cptWnd);

note: Start<->Delay programming:
 if (non-DV-slave mode)
 {
    hwDelay = Delay
    hwStart = Start
    hwMeasureWindowStart = Start
 }
 else (DV-slave-mode
 {
    hwDelay = 0
    hwStart = Delay
    hwMeasureWindowStart = Start+Delay
 }
*******************************************************************************/
FVDP_Result_t IVP_CaptureSet(FVDP_CH_t CH,
                           IVP_CaptureFlag_t Flag,
                           IVP_Capture_t const* pVal)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    if (pVal != NULL)
    {
        if ((Flag & IVP_CAPTURE_NO_HORIZ_UPDATE) == 0)
        {
            if ((Flag & IVP_CAPTURE_NO_POSITION_UPDATE) == 0)
            {
                if (IVP_REG32_Read(CH, IVP_CONTROL) & IVP_EXT_DV_EN_MASK)
                {
                    IVP_REG32_Write(CH, IVP_HS_DELAY, 0);
                    IVP_REG32_Write(CH, IVP_H_ACT_START, pVal->HSyncDelay);
                    IVP_REG32_Write(CH, IVP_HMEASURE_START, pVal->HStart + pVal->HSyncDelay);
                }
                else
                {
                    IVP_REG32_Write(CH, IVP_HS_DELAY, pVal->HSyncDelay);
                    IVP_REG32_Write(CH, IVP_H_ACT_START, pVal->HStart);
                    IVP_REG32_Write(CH, IVP_HMEASURE_START, pVal->HStart);
                }
            }
            if ((Flag & IVP_CAPTURE_NO_SIZE_UPDATE) == 0)
            {
                IVP_REG32_Write(CH, IVP_H_ACT_WIDTH, pVal->Width);
                IVP_REG32_Write(CH, IVP_HMEASURE_WIDTH, pVal->Width);
            }
        }
        if ((Flag & IVP_CAPTURE_NO_VERT_UPDATE) == 0)
        {
            if ((Flag & IVP_CAPTURE_NO_POSITION_UPDATE) == 0)
            {   /* in dv-slave mode delay is written into act_start */
                if (IVP_REG32_Read(CH, IVP_CONTROL) & IVP_EXT_DV_EN_MASK)
                {
                    IVP_REG32_Write(CH, IVP_VS_DELAY, 0);
                    IVP_REG32_Write(CH, IVP_V_ACT_START_ODD, pVal->VSyncDelay);
                    IVP_REG32_Write(CH, IVP_V_ACT_START_EVEN,
                        (Flag & IVP_CAPTURE_SYNC_EVEN_OFFSET) ? (pVal->VSyncDelay + 1) : pVal->VSyncDelay);
                    IVP_REG32_Write(CH, IVP_FRAME_RESET_LINE, IVP_FRAME_RESET_VALUE);
                    IVP_REG32_Write(CH, IVP_VMEASURE_START, pVal->VStart + pVal->VSyncDelay);
                }
                else
                {
                    IVP_REG32_Write(CH, IVP_VS_DELAY, pVal->VSyncDelay);
                    IVP_REG32_Write(CH, IVP_V_ACT_START_ODD, pVal->VStart);
                    IVP_REG32_Write(CH, IVP_V_ACT_START_EVEN,
                        (Flag & IVP_CAPTURE_SYNC_EVEN_OFFSET) ? (pVal->VStart + 1) : pVal->VStart);
                    IVP_REG32_Write(CH, IVP_FRAME_RESET_LINE, (pVal->VStart > 2) ? (pVal->VStart - 2) : pVal->VStart);
                    IVP_REG32_Write(CH, IVP_VMEASURE_START, pVal->VStart);
                }
            }
            if ((Flag & IVP_CAPTURE_NO_SIZE_UPDATE) == 0)
            {
                IVP_REG32_Write(CH, IVP_V_ACT_LENGTH, pVal->Height);
                IVP_REG32_Write(CH, IVP_VMEASURE_LENGTH, pVal->Height);
            }

            VStartOdd[CH] = IVP_REG32_Read(CH, IVP_V_ACT_START_ODD);

            /*Set up VLOCK marker*/
            VMUX_REG32_Clear(VMUX_PBUS_CTRL, IVP_READ_ACTIVE_MASK); /*to read pending register.*/

            IVP_REG32_Write(CH, IVP_VLOCK_EVENT, IVP_REG32_Read(CH, IVP_V_ACT_START_ODD) +
                                                 (IVP_REG32_Read(CH, IVP_V_ACT_LENGTH) >> 4));
            }

        if (((Flag & IVP_CAPTURE_NO_SIZE_UPDATE) == 0)
            && (((Flag & IVP_CAPTURE_NO_HORIZ_UPDATE) == 0)
                ||((Flag & IVP_CAPTURE_NO_VERT_UPDATE) == 0)))
        {
            IVP_MeasureCtrlUpdate(CH);
        }

        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    return FVDP_OK;
}

/********************************************************************************
Description:
    Control Color space conversion hardware block settings
Parameters:
    CH - Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX)

    Flag - Control flags:
    One of the IVP_422_YUV, IVP_444_RGB, IVP_444_YUV flags
    must be set to indicate the type of input signal: 4:2:2(YUV), 4:4:4(RGB), 4:4:4(YUV).

        Only one of the following 3 flags should be selected:
        IVP_RGB_TO_YUV: set to convert RGB input to YUV color space (CSC is not used)
        IVP_RGB_TO_YUV_940: video is encoded as RGB with dynamic range of 64-940
            as per CCIR861 (CSC is not used)
        IVP_RGB_TO_YUV_CSC convert RGB to YUV thru CSC matrix: when that flag is used pVal pointer
            must point to buffer with 	matrix. For all other cases pVal is not used.

        pVal - Pointer to a CSC matrix structure. Non-zero value causes matrix loading

Return:
    FVDP_OK/FVDP_ERROR
Examples:
    //to setup 4:4:4 RGB input on Main channel:
    IVP_CscSet(FVDP_MAIN, IVP_444_RGB, 0);

    //to setup 4:4:4 YUV input on PIP channel with RGB to YUV conversion with dynamic
    //range of 64-940 without use of CSC:
    IVP_CscSet(FVDP_PIP, IVP_444_YUV|IVP_RGB_TO_YUV, 0);

    //to setup 4:4:4 YUV input on VCR channel with conversion RGB to YUV thru user CSC matrix :
    IVP_Csc_t const csc_mtrx =
        {0,0,0, 0x81,0x19,0x42, 0x7b6,0x70,0x7da, 0x7a2,0x7ee,0x70, 0x40,0x200,0x200};
    IVP_CscSet(FVDP_AUX, IVP_444_YUV|IVP_RGB_TO_YUV_CSC, &csc_mtrx);

********************************************************************************/
FVDP_Result_t IVP_CscSet(FVDP_CH_t CH, IVP_CscFlag_t Flag, IVP_CSCMatrixCoef_t* CSC_p)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    /*LUT control should not be used*/
    IVP_REG32_Write(CH, IVP_LUT_CONTROL, 0);

    if (Flag & IVP_444_RGB)
    {
        IVP_REG32_Set(CH, IVP_CONTROL, IVP_444_EN_MASK);
    }
    else
    {
        IVP_REG32_Clear(CH, IVP_CONTROL, IVP_444_EN_MASK);
    }

    if (Flag & (IVP_420_YUV|IVP_422_YUV|IVP_RGB_TO_YUV|IVP_RGB_TO_YUV_CSC))
    {
        IVP_REG32_Set(CH, IVP_CONTROL, IVP_YUV_EN_MASK);
    }
    else
    {
        IVP_REG32_Clear(CH, IVP_CONTROL, IVP_YUV_EN_MASK);
    }

    if (Flag & (IVP_420_YUV))
    {
        if (CH == FVDP_MAIN)
        {
            MISC_FE_REG32_Write(YUV420TO422_CTRL, (1<<VIDEO1_IN_YUV420TO422_MODE_OFFSET)|VIDEO1_IN_YUV420TO422_EN_MASK);
        }
        else
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_420_TO_422_EN_MASK);
        }
    }
    else
    {
        if (CH == FVDP_MAIN)
        {
            MISC_FE_REG32_Clear(YUV420TO422_CTRL, VIDEO1_IN_YUV420TO422_MODE_MASK|VIDEO1_IN_YUV420TO422_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_420_TO_422_EN_MASK);
        }
    }

    if (CSC_p != NULL)
    {
        // IVP CSC input offsets
        IVP_REG32_Write(CH, IVP_IN_OFFSET1, CSC_p->InOffset1);
        IVP_REG32_Write(CH, IVP_IN_OFFSET2, CSC_p->InOffset2);
        IVP_REG32_Write(CH, IVP_IN_OFFSET3, CSC_p->InOffset3);
        // IVP CSC coefficients
        IVP_REG32_Write(CH, IVP_MULT_COEF11, CSC_p->MultCoef11);
        IVP_REG32_Write(CH, IVP_MULT_COEF12, CSC_p->MultCoef12);
        IVP_REG32_Write(CH, IVP_MULT_COEF13, CSC_p->MultCoef13);
        IVP_REG32_Write(CH, IVP_MULT_COEF21, CSC_p->MultCoef21);
        IVP_REG32_Write(CH, IVP_MULT_COEF22, CSC_p->MultCoef22);
        IVP_REG32_Write(CH, IVP_MULT_COEF23, CSC_p->MultCoef23);
        IVP_REG32_Write(CH, IVP_MULT_COEF31, CSC_p->MultCoef31);
        IVP_REG32_Write(CH, IVP_MULT_COEF32, CSC_p->MultCoef32);
        IVP_REG32_Write(CH, IVP_MULT_COEF33, CSC_p->MultCoef33);
        // IVP CSC output offsets
        IVP_REG32_Write(CH, IVP_OUT_OFFSET1, CSC_p->OutOffset1);
        IVP_REG32_Write(CH, IVP_OUT_OFFSET2, CSC_p->OutOffset2);
        IVP_REG32_Write(CH, IVP_OUT_OFFSET3, CSC_p->OutOffset3);
    }

    if (Flag & IVP_RGB_TO_YUV_CSC)
    {
        /*Multiplex Chroma, and provide 422 yuv data out of CSC*/
        IVP_REG32_Write(CH, IVP_CSC_CTRL, IVP_CSC_OUT_444TO422_EN_MASK);
        IVP_REG32_Set(CH, IVP_CONTROL, IVP_CSC_OVERRIDE_MASK);
    }
    else
    {
        if (Flag & IVP_RGB_TO_YUV)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_RGB_TO_YUV_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_RGB_TO_YUV_EN_MASK);
        }


        if ((Flag & IVP_RGB_TO_YUV_940) == IVP_RGB_TO_YUV_940)
        {
            IVP_REG32_Set(CH, IVP_CONTROL, IVP_CCIR861_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_CONTROL, IVP_CCIR861_EN_MASK);
        }

        IVP_REG32_Clear(CH, IVP_CONTROL, IVP_CSC_OVERRIDE_MASK);
    }

    HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    return FVDP_OK;
}

/*******************************************************************************
Description:    Control test pattern generator of Input block
Parameters:    CH - Input channel information (CH_MIPV, FVDP_PIP, FVDP_AUX)
                Flag:
                IVP_PATGEN_TEST_INV - Inverts test pattern data
                IVP_PATGEN_ODD_POL_INV - Invert ODD polarity for new patterns sensitive to field ID
                IVP_PATGEN_NO_COLOR_UPDATE - colors fields are not updated
                pVal - pointer to the parameter structure
Return:      FVDP_OK/FVDP_ERROR
example:
                //select blue screen background for main channel
                IVP_PatGen_t pg = {1, 0, 0xFF,0,0};
                IVP_PatGenSet(FVDP_MAIN, IVP_PATGEN_ENABLE, &pg);
                ...
                //select checker board, do not update color
                IVP_PatGen_t pg = {2,0,0,0,0};
                IVP_PatGenSet(FVDP_MAIN, IVP_PATGEN_ENABLE|IVP_PATGEN_NO_COLOR_UPDATE, &pg);
********************************************************************************/
FVDP_Result_t IVP_PatGenSet(FVDP_CH_t CH, IVP_PatGenFlag_t Flag, IVP_PatGen_t const* pVal)
{
    uint32_t val;
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    val = IVP_PAT_SET_OFFSET |
        (pVal->pattern & IVP_PAT_SEL_MASK) |
        (((pVal->pattern & IVP_PAT_SET_MASK) >> IVP_PAT_SET_OFFSET) << IVP_PAT_SEL_WIDTH) |
        ((Flag & IVP_PATGEN_ENABLE) ? IVP_TPG_EN_MASK : 0) |
        ((Flag & IVP_PATGEN_TEST_INV) ? IVP_PAT_INV_DATA_MASK : 0) |
        ((Flag & IVP_PATGEN_ODD_POL_INV) ? IVP_PATGEN_ODD_INV_MASK : 0);
    IVP_REG32_Write(CH, IVP_PATGEN_CONTROL, val);

    if ((Flag & IVP_PATGEN_NO_COLOR_UPDATE) == 0)
    {
        val = ((uint32_t)(((uint16_t)pVal->red << 8) + pVal->green) << 8) + pVal->blue;
        IVP_REG32_Write(CH, IVP_PATGEN_COLOR, val);
    }

    return FVDP_OK;
}

/*******************************************************************************
Description:    Set up pixel grab coordinates
Parameters:    CH - Input channel information (CH_MIPV, FVDP_PIP, FVDP_AUX)
                X - pixel Grab X Co-ordinate programmed relative to the horizontal sync
                    (after being delayed by MIVP_HS_DELAY).
                Y - pixel Grab Y Co-ordinate programmed relative to the vertical sync
                    (after being delayed by MIVP_VS_DELAY).
Return:          FVDP_ERROR
example:        IVP_PixGrabSet(FVDP_MAIN, 100, 100);
********************************************************************************/
FVDP_Result_t IVP_PixGrabSet(FVDP_CH_t CH, uint16_t X, uint16_t Y)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    IVP_REG32_Write(CH, IVP_PIXGRAB_H, X);
    IVP_REG32_Write(CH, IVP_PIXGRAB_V, Y);
    IVP_REG32_Set(CH, IVP_CONTROL, IVP_PIXGRAB_EN_MASK);
    return FVDP_OK;
}

/*******************************************************************************
Description:    Return grabbed pixel value from pixel grab module
Parameters:    CH - IVP Chanel information (CH_MIPV, FVDP_PIP, FVDP_AUX)
                Pixel - pointer to the pixel structure, where result will be written.
Return:      FVDP_ERROR
********************************************************************************/
FVDP_Result_t IVP_PixGrabRead(FVDP_CH_t CH, IVP_PixGrab_t* const Pixel)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }
    if (Pixel == NULL)
    {
        return FVDP_ERROR;
    }

    Pixel->red = IVP_REG32_Read(CH, IVP_PIXGRAB_RED);
    Pixel->green = IVP_REG32_Read(CH, IVP_PIXGRAB_GRN);
    Pixel->blue= IVP_REG32_Read(CH, IVP_PIXGRAB_BLU);
    return FVDP_OK;
}

/*******************************************************************************
Description:
    Control Input measure window hardware block

Parameters:
    CH - Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX)

    Flag - Control flags:
        IVP_MEASURE_ENABLE: Enable the IP_MEASURE functionality (does not affect IBD/MinMax/SumDif/Feat)
        IVP_MEASURE_DISABLE: Disable the IP_MEASURE functionality (does not affect IBD/MinMax/SumDif/Feat)

        IVP_MEASURE_WIN_ENABLE: turn on mode: to measure only-measure-window-data (does not affect IBD/MinMax/SumDif/Feat)
        IVP_MEASURE_WIN_DISABLE: turn on mode:  to measure all-active-data (does not affect IBD/MinMax/SumDif/Feat)

        IVP_MEASURE_CLK_DISABLE: Disable the Clock per Frame measure functionality
        IVP_MEASURE_CLK_ENABLE: Enable the Clock per Frame measure functionality
        IVP_MEASURE_CLK_INTLC: Count CLK's per frame between every two input Vsync pulses

        IVP_MEASURE_SC_ENABLE: Scene Change checking enable
        IVP_MEASURE_SC_DISABLE: Scene Change checking disable
        IVP_MEASURE_SC_SRC_EXT: Scene Change Source = external detector
        IVP_MEASURE_SC_SRC_INT: Scene Change Source = internal detector
        IVP_MEASURE_SC_EXT: Scene change bit used for external control
        IVP_MEASURE_SC_INT: Scene change bit used for internal control

    pVal - Pointer to numerical structure.
        If pVal=0 - window position will not be updated

    SCThreshold - Scene change threshold for internal detector

Return: FVDP_OK/FVDP_ERROR

examples:
    //to setup PIP input measure window coordinates and enable block:
    const IVP_Measure_t msrWnd = {100,100,400,300};
    IVP_MeasureSet(FVDP_PIP, IVP_MEASURE_EN|IVP_MEASURE_WIN_EN, &msrWnd, IVP_SC_KEEP_THRESHOLD);

    //to disable the input measure block:
    IVP_MeasureSet(FVDP_MAIN, IVP_MEASURE_DISABLE, 0, IVP_SC_KEEP_THRESHOLD);

    //to enable the input measure block without window position update:
    IVP_MeasureSet(FVDP_MAIN, IVP_MEASURE_EN, 0, IVP_SC_KEEP_THRESHOLD);

    //to update window position update without change of mode
    IVP_MeasureSet(FVDP_MAIN, 0, &msrWnd, IVP_SC_KEEP_THRESHOLD);

    //to enable Scene change check from internal detector and setup SC threshold to 10
    IVP_MeasureSet(FVDP_MAIN,
        IVP_MEASURE_SC_ENABLE | IVP_MEASURE_SC_SRC_INT | IVP_MEASURE_SC_INT,
        &msrWnd, 10);

*******************************************************************************/
FVDP_Result_t IVP_MeasureSet(FVDP_CH_t CH,
                            IVP_MeasureFlag_t Flag,
                            IVP_Measure_t const* pVal,
                            uint32_t SCThreshold)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    if (Flag & (IVP_MEASURE_DISABLE|IVP_MEASURE_ENABLE)) /*do change enable bit?*/
    {
        if (Flag & IVP_MEASURE_ENABLE)
        {
            IVP_REG32_Set(CH, IVP_IP_MEASURE_CTRL, IVP_IP_MEASURE_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IP_MEASURE_CTRL, IVP_IP_MEASURE_EN_MASK);
        }
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    if (Flag & (IVP_MEASURE_WIN_DISABLE|IVP_MEASURE_WIN_ENABLE)) /*do change enable bit?*/
    {
        if (Flag & IVP_MEASURE_WIN_ENABLE)
        {
            IVP_REG32_Set(CH, IVP_IP_MEASURE_CTRL, IVP_IP_MEASURE_WIN_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IP_MEASURE_CTRL, IVP_IP_MEASURE_WIN_EN_MASK);
        }
    }

    if (Flag & IVP_MEASURE_CLK_ENABLE)
    {
        IVP_REG32_Set(CH, IVP_IP_MEASURE_CTRL, IVP_CLK_PER_FRAME_EN_MASK);
        if ((Flag & IVP_MEASURE_CLK_INTLC) == IVP_MEASURE_CLK_INTLC)
        {
            IVP_REG32_Set(CH, IVP_CLK_PER_FRAME_CTRL, IVP_CLK_PER_FRAME_INTLC_EN_MASK);
        }
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }
    else if (Flag & IVP_MEASURE_CLK_DISABLE)
    {
        IVP_REG32_Clear(CH, IVP_IP_MEASURE_CTRL, IVP_CLK_PER_FRAME_EN_MASK);
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    if (pVal)
    {
        IVP_REG32_Write(CH, IVP_HMEASURE_START, pVal->HStart);
        IVP_REG32_Write(CH, IVP_HMEASURE_WIDTH, pVal->Width);
        IVP_REG32_Write(CH, IVP_VMEASURE_START, pVal->VStart);
        IVP_REG32_Write(CH, IVP_VMEASURE_LENGTH, pVal->Height);
    }

    if ((Flag & (IVP_MEASURE_WIN_DISABLE|IVP_MEASURE_WIN_ENABLE)) || pVal)
    {
        IVP_MeasureCtrlUpdate(CH); /*to recalculate total_pix....*/
    }

    if (Flag & (IVP_MEASURE_SC_ENABLE | IVP_MEASURE_SC_DISABLE))
    {
        if (Flag & IVP_MEASURE_SC_ENABLE)
        {
            IVP_REG32_Set(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_CHK_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_CHK_EN_MASK);
        }
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    if (Flag & (IVP_MEASURE_SC_SRC_EXT| IVP_MEASURE_SC_SRC_INT))
    {
        if (Flag & IVP_MEASURE_SC_SRC_INT)
        {
            IVP_REG32_Set(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_SOURCE_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_SOURCE_MASK);
        }
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    if (Flag & (IVP_MEASURE_SC_EXT| IVP_MEASURE_SC_INT))
    {
        if (Flag & IVP_MEASURE_SC_EXT)
        {
            IVP_REG32_Set(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_EXT_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_EXT_MASK);
        }
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    if (SCThreshold != IVP_SC_KEEP_THRESHOLD)
    {
        IVP_REG32_ClearAndSet(CH, IVP_IP_MEAS_SC_CTRL, IVP_IP_MEAS_SC_THRESH_MASK,
                              IVP_IP_MEAS_SC_THRESH_OFFSET, SCThreshold);
        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);
    }

    return FVDP_OK;
}

/*******************************************************************************
Description:
    Minmax search feature: The programmed active input window is searched for
    the maximum (or minimum) red, green, and blue pixel values. A complete input
    field/frame must be input efore read actual value.

Parameters:
    CH - Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)

    Flag - Control flags:
        IVP_MINMAX_DISABLE - Disable the minmax search feature
        IVP_MINMAX_MIN - Enable minmax search feature. Search for minimum RGB pixel values
        IVP_MINMAX_MAX - Enable minmax search feature. Search for maximum RGB pixel values

Return: FVDP_OK/FVDP_ERROR

examples:
    //to setup the Main Maximum measurement:
    IVP_MinMaxSet(FVDP_MAIN, IVP_MINMAX_MAX);

    //to disable the SumDiff measurement:
    IVP_MinMaxSet(FVDP_MAIN, IVP_MINMAX_DISABLE);

*******************************************************************************/
FVDP_Result_t IVP_MinMaxSet(FVDP_CH_t CH, IVP_MinMax_t Flag)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    IVP_REG32_Set(CH, IVP_SUMDIFF_MINMAX_CONFIG, IVP_CLEAR_MAX_MASK);

    if ((Flag == IVP_MINMAX_MIN) || (Flag == IVP_MINMAX_MAX))
    {
        IVP_REG32_Set(CH,IVP_SUMDIFF_MINMAX_CONFIG, IVP_MINMAX_EN_MASK);
    }
    else
    {
        IVP_REG32_Clear(CH,IVP_SUMDIFF_MINMAX_CONFIG, IVP_MINMAX_EN_MASK);
    }

    if (Flag == IVP_MINMAX_MAX)
    {
        IVP_REG32_Set(CH,IVP_SUMDIFF_MINMAX_CONFIG, IVP_MINMAX_MAX_EN_MASK);
    }
    else
    {
        IVP_REG32_Clear(CH,IVP_SUMDIFF_MINMAX_CONFIG, IVP_MINMAX_MAX_EN_MASK);
    }

    IVP_REG32_Clear(CH, IVP_SUMDIFF_MINMAX_CONFIG, IVP_CLEAR_MAX_MASK);

    return FVDP_OK;
}

/*******************************************************************************
Description:

Parameters:
    CH - Input channel information (FVDP_MAIN, FVDP_PIP, FVDP_AUX, FVDP_ENC)

Return: FVDP_OK/FVDP_ERROR
*******************************************************************************/
FVDP_Result_t IVP_IbdSet(FVDP_CH_t CH, IVP_Ibd_t Flag, uint32_t Threshold)
{
    if (CH >= NUM_FVDP_CH)
    {
        return FVDP_ERROR;
    }

    if (Flag & IVP_IBD_DISABLE)
    {
        IVP_REG32_Clear(CH, IVP_IBD_CONTROL, IVP_IBD_EN_MASK);
    }

    if (Flag & (IVP_IBD_DATA|IVP_IBD_DE))
    {
        if (Flag & IVP_IBD_DE)
        {
            IVP_REG32_Set(CH, IVP_IBD_CONTROL, IVP_MEASURE_DE_NDATA_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IBD_CONTROL, IVP_MEASURE_DE_NDATA_MASK);
        }
    }

    if (Flag & (IVP_IBD_MEASWIN_EN|IVP_IBD_MEASWIN_DIS))
    {
        if (Flag & IVP_IBD_MEASWIN_EN)
        {
            IVP_REG32_Set(CH, IVP_IBD_CONTROL, IVP_IBD_MEASWINDOW_EN_MASK);
        }
        else
        {
            IVP_REG32_Clear(CH, IVP_IBD_CONTROL, IVP_IBD_MEASWINDOW_EN_MASK);
        }
    }

    if (Threshold < IVP_IBD_KEEP_THRESHOLD)
    {
        IVP_REG32_ClearAndSet(CH, IVP_IBD_CONTROL, IVP_IBD_RGB_SEL_MASK, IVP_IBD_RGB_SEL_OFFSET, Threshold);
    }

    if (Flag & IVP_IBD_ENABLE)
    {
        IVP_REG32_Set(CH, IVP_IBD_CONTROL, IVP_IBD_EN_MASK);
    }

    return FVDP_OK;
}

#if 0
/*******************************************************************************
Name        : ivp_SetDataCheckState
Description : Updates the data check registers(ivp crc) to enable or disable.

Parameters  : CH selects the channel to enable or disable ivp crc feature
              state TRUE: enable FALSE: disable
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK no error
*******************************************************************************/
static FVDP_Result_t ivp_SetDataCheckState(FVDP_CH_t CH, bool state)
{
    if(state == TRUE)
    {
        DataCheckEnableStatus[CH] = TRUE;
        IVP_REG32_Write(CH, IVP_DATA_CHECK_CTRL, (IVP_DATA_CHECK_EN_MASK | IVP_DATA_CHECK_COLOR_SELECT_MASK));
    }
    else
    {
        DataCheckEnableStatus[CH] = FALSE;
        IVP_REG32_Write(CH, IVP_DATA_CHECK_CTRL, 0);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : ivp_GetDataCheckState
Description : Returns the current data check state if enabled or disabled.

Parameters  : CH selects the channel to check crc feature state

Assumptions :
Limitations :
Returns     : bool value :
              TRUE: enabled FALSE: disabled
*******************************************************************************/
static bool ivp_GetDataCheckState(FVDP_CH_t CH)
{
    return (DataCheckEnableStatus[CH]);
}

/*******************************************************************************
Name        : ivp_GetDataCheckResult
Description : Returns the data check value.
                DATA_CHECK_RESULT register is updated at EOF
                and value gets latched. Value is the info for the previous input
                when value is red by input update.
Parameters  : CH selects the channel to check crc value

Assumptions :
Limitations :
Returns     : uint32_t value :
              reading DATA_CHECK_RESULT register
*******************************************************************************/
static uint32_t ivp_GetDataCheckResult(FVDP_CH_t CH)
{
    //uint32_t const ivp_data_check_result[NUM_FVDP_CH] =
    //    {MIVP_DATA_CHECK_RESULT, IVP_DATA_CHECK_RESULT, VIVP_DATA_CHECK_RESULT};

    return IVP_REG32_Read(CH, IVP_DATA_CHECK_RESULT);
}
#endif

/*******************************************************************************
Name        : IVP_FieldControl
Description :
Parameters  : CH selects the channel

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t IVP_FieldControl(FVDP_CH_t CH, uint32_t Val)
{

    IVP_REG32_ClearAndSet(CH, IVP_FIELD_CONTROL, IVP_ODD_INV_MASK, IVP_ODD_INV_OFFSET, Val);

    return FVDP_OK;
}

/*******************************************************************************
Name        : ivp_VStartOffsetHandler

Description : The function determines the number of line to skip to from the input
              buffer for interlace to interace, shrink mode for spatial process
              - this skip line is to compensate the HW limitation in the Offset
                registers that can only handle maximum of 1 line offset; for interlace
                interlace shrink case, we may need more than 1 line offset
                expect offset = skipped lines + Offset-register-value
Parameters  :
Assumptions :
Limitations :
Returns     :  FVDP_OK
*******************************************************************************/
FVDP_Result_t IVP_VStartSetOffset (FVDP_CH_t CH, FVDP_VideoInfo_t* InputVideoInfo_p,  FVDP_VideoInfo_t*  OutputVideoInfo_p)
{
    uint32_t vstart_offset;
    uint32_t input_height;
    uint32_t output_height;

    if((CH !=FVDP_PIP)&&(CH != FVDP_AUX))
    {
        return FVDP_OK;
    }
    /*
     * Determien if it is spatial process, interlace to interlace, shrink mode
     */
    if((InputVideoInfo_p->ScanType == INTERLACED)
        && (OutputVideoInfo_p->ScanType == INTERLACED))
    {
        input_height = IVP_REG32_Read(CH, IVP_V_ACT_LENGTH);
        output_height = OutputVideoInfo_p->Height;

        /* The offset apply to the bottom field with shrink ratio more than 3:1, the number of skipped line
         * is determined by the integral portion of the ratio, the remainder is to be programmed to offset register
         * - To skip line, it is adjust by the VStart register
         * - To set offset register, it will be handled by scarler module
         */
        if((input_height > output_height) && (InputVideoInfo_p->FieldType == BOTTOM_FIELD))
        {
            vstart_offset = ((input_height/output_height) )/2;
        }
        else
        {
            vstart_offset = 0;  // no skip line for top field
        }

        IVP_REG32_Write(CH, IVP_V_ACT_START_ODD, VStartOdd[CH] + vstart_offset);

        HostUpdate_SetUpdate_VMUX(CH, SYNC_UPDATE);

    }
    return FVDP_OK;
}


