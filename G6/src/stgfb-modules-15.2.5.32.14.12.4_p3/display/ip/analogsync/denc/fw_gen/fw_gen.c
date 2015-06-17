/***********************************************************************
 *
 * File: display/ip/analogsync/denc/fw_gen/awg_gen.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "fw_gen.h"
#include "getlevel.h"

#include <vibe_os.h>
#include <vibe_debug.h>

#define N_ELEMENTS(array)   (sizeof (array) / sizeof ((array)[0]))

typedef enum STAWGFW_ERROR_CODE_e
{
  STAWGFW_NO_ERROR,                        /* Success */
  STAWGFW_ERROR_BAD_PARAMETER,             /* Bad parameter passed  */
  STAWGFW_ERROR_FEATURE_NOT_SUPPORTED,     /* Feature unavailable   */
  STAWGFW_ERROR_WAVEFORM_CLIPPED,          /* Min and/or Max levels are clipped */
  STAWGFW_ERROR_REG_OVERFLOW               /* When any of registers overflow */
} STAWGFW_ErrorCode_t;

enum { SET,RPTSET,RPLSET,SKIP,STOP,REPEAT,REPLAY,JUMP,HOLD,mux_sel};

typedef struct
{
    bool IsRGB;
    stm_analog_sync_setup_t *AnalogSyncSetup_p;
    DACMult_Config_t        *DACMult_Config_p;
} Ana_EDHDRAMGenerationParams_t;

typedef STAWGFW_ErrorCode_t (* Ana_GenerateAwgCodeFunction_t) (stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p);

typedef struct Ana_GenerateAwgCode_s
{
    stm_display_mode_id_t           TimingMode;
    Ana_GenerateAwgCodeFunction_t   Ana_GenerateAwgCode;
} Ana_GenerateAwgCode_t;

static STAWGFW_ErrorCode_t Amplitude_Adjust(Ana_EDHDRAMGenerationParams_t* FmwGenParams_p, video_t *video_p);
static STAWGFW_ErrorCode_t TryAmplitude_Adjust(Ana_EDHDRAMGenerationParams_t *FmwGenParams_p, video_t *video_p);

/* RGB Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_RGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p);
/* SD Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p);

static const Ana_GenerateAwgCode_t Ana_GenerateAwgCodeFunction[] =
{
/* SD Modes */
    { STM_TIMING_MODE_480I59940_12273,      Ana_GenerateAwgCodeXXXYZZ_YPbPr     }
  , { STM_TIMING_MODE_480I59940_13500,      Ana_GenerateAwgCodeXXXYZZ_YPbPr     }
  , { STM_TIMING_MODE_576I50000_13500,      Ana_GenerateAwgCodeXXXYZZ_YPbPr     }
  , { STM_TIMING_MODE_576I50000_14750,      Ana_GenerateAwgCodeXXXYZZ_YPbPr     }
};
static const uint8_t Ana_GenerateAwgCodeFunction_Count = N_ELEMENTS(Ana_GenerateAwgCodeFunction);


static STAWGFW_ErrorCode_t Amplitude_Adjust(Ana_EDHDRAMGenerationParams_t* FmwGenParams_p, video_t *video_p)
{
    STAWGFW_ErrorCode_t Error = STAWGFW_NO_ERROR;

    int scaling_factor_Cb;
    int scaling_factor_Y;
    int scaling_factor_Cr;

    int DAC_Mult_Min_value;
    int DAC_Mult_Max_value;
    int DAC_Mult_slope;
    int DAC_Mult_offset;

    int Denc_DAC_Volt = 0;

    int tmp;
    uint32_t *temp;
    uint32_t blank, blank_scaled;

    if( (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_12273)
     || (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_13500))
        Denc_DAC_Volt = 1305;
    else if( (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_13500)
          || (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_14750) )
        Denc_DAC_Volt = 1280;

    /* ---------------------------------------------------------------------------------------------------------
     * Scaling Factor
     * ---------------------------------------------------------------------------------------------------------*/
    scaling_factor_Cb = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cb;
    scaling_factor_Y  = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Y;
    scaling_factor_Cr = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cr;

    scaling_factor_Cb = (scaling_factor_Cb*Denc_DAC_Volt)/1400;
    scaling_factor_Y = (scaling_factor_Y*Denc_DAC_Volt)/1400;
    scaling_factor_Cr = (scaling_factor_Cr*Denc_DAC_Volt)/1400;

    if (FmwGenParams_p->IsRGB)
    {
        DAC_Mult_Min_value = 832;
        DAC_Mult_Max_value = 1210;
        DAC_Mult_slope = 171;
        DAC_Mult_offset = 139;
    }
    else
    {
        DAC_Mult_Min_value = 768;
        DAC_Mult_Max_value = 1272;
        DAC_Mult_slope = 128;
        DAC_Mult_offset = 96;
    }

    if (scaling_factor_Cb < DAC_Mult_Min_value)
    {
        scaling_factor_Cb = DAC_Mult_Min_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (scaling_factor_Cb > DAC_Mult_Max_value)
    {
        scaling_factor_Cb = DAC_Mult_Max_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (scaling_factor_Y < DAC_Mult_Min_value)
    {
        scaling_factor_Y = DAC_Mult_Min_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (scaling_factor_Y > DAC_Mult_Max_value)
    {
        scaling_factor_Y = DAC_Mult_Max_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (scaling_factor_Cr < DAC_Mult_Min_value)
    {
        scaling_factor_Cr = DAC_Mult_Min_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (scaling_factor_Cr > DAC_Mult_Max_value)
    {
        scaling_factor_Cr = DAC_Mult_Max_value;
        Error = STAWGFW_ERROR_REG_OVERFLOW;
    }

    if(FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_13500
    || FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_13500)
    {
        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cb =
            (DAC_Mult_slope*scaling_factor_Cb - DAC_Mult_offset*1024)/1024;
        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cb =
            FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cb & 0x0000003F;

        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Y =
            (DAC_Mult_slope*scaling_factor_Y - DAC_Mult_offset*1024)/1024;
        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Y =
            FmwGenParams_p->DACMult_Config_p->DACMult_Config_Y & 0x0000003F;

        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cr =
            (DAC_Mult_slope*scaling_factor_Cr - DAC_Mult_offset*1024)/1024;
        FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cr =
            FmwGenParams_p->DACMult_Config_p->DACMult_Config_Cr & 0x0000003F;
    }
    else
        Error = STAWGFW_ERROR_BAD_PARAMETER;

    /*levels
    ================= */
    blank = video_p->blank_act_vid;
    blank_scaled = (blank*scaling_factor_Y+512)/1024;

    /* Scaling and offseting Analog sync  and video levels */
    temp = &(video_p->sync_low);
    while(temp <= &(video_p->peak_luma))
    {
        tmp = *temp;
        /* subtracting blanking and then scaling*/
        tmp = tmp - blank;
        if (tmp < 0)
            tmp = (tmp*scaling_factor_Y - 512)/1024;
        else
            tmp = (tmp*scaling_factor_Y + 512)/1024;
        /* add back the scaled blank level */
        tmp = tmp + blank_scaled;
        /* rounding operation and conversion to 8 bits */
        if (tmp < 0)
            tmp = (tmp - 2)/4;
        else
            tmp = (tmp + 2)/4;
        /* clipping operation*/
        if (tmp > 255)
        {
            tmp = 255;
            Error = STAWGFW_ERROR_WAVEFORM_CLIPPED;
        }
        else if (tmp < 0)
        {
            tmp = 0;
            Error = STAWGFW_ERROR_WAVEFORM_CLIPPED;
        }
        *temp = tmp;
        temp++;
    }

    /* Report Errors to uper level */
    if(Error == STAWGFW_ERROR_REG_OVERFLOW)
    {
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cb = (scaling_factor_Cb*1400)/Denc_DAC_Volt;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Y  = (scaling_factor_Y*1400)/Denc_DAC_Volt;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cr = (scaling_factor_Cr*1400)/Denc_DAC_Volt;
    }

    return(Error);
}


static STAWGFW_ErrorCode_t TryAmplitude_Adjust(Ana_EDHDRAMGenerationParams_t *FmwGenParams_p, video_t *video_p)
{
    STAWGFW_ErrorCode_t Error;

    TRC( TRC_ID_MAIN_INFO, "In : TimingMode=%d, IsRGB=%d.",FmwGenParams_p->AnalogSyncSetup_p->TimingMode, FmwGenParams_p->IsRGB );

    if(!GetDencLevels(FmwGenParams_p->AnalogSyncSetup_p->TimingMode, video_p))
        return STAWGFW_ERROR_FEATURE_NOT_SUPPORTED;

    Error = Amplitude_Adjust(FmwGenParams_p, video_p);

    switch(Error)
    {
        case STAWGFW_ERROR_WAVEFORM_CLIPPED:
        {
            TRC( TRC_ID_MAIN_INFO, "Warning : Min and/or Max levels are clipped." );
            Error = STAWGFW_NO_ERROR;
            break;
        }
        case STAWGFW_ERROR_REG_OVERFLOW:
        {
            TRC( TRC_ID_MAIN_INFO, "Warning : registers overflow." );
            Error = STAWGFW_NO_ERROR;
            break;
        }
        default:
    TRC( TRC_ID_MAIN_INFO, "Out." );
    }

    return Error;
}


/* RGB Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCodeXXXYZZ_RGB
   Description: Update the DencMult Config Registers
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_RGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = TRUE;
    FmwGenParams.DACMult_Config_p    = DACMult_Config_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);

    return(Error) ;
}


/* SD Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCodeXXXYZZ_YPbPr
   Description: Update the DencMult Config Registers
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, DACMult_Config_t *DACMult_Config_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.DACMult_Config_p    = DACMult_Config_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);

    return(Error) ;
}


/* Public AS_AWG_FW API */
int Ana_GenerateDencCodeRGB(stm_analog_sync_setup_t *AnalogSyncSetup_p , DACMult_Config_t *DACMult_Config_p)
{
    return Ana_GenerateAwgCodeXXXYZZ_RGB(AnalogSyncSetup_p, DACMult_Config_p);
}


int Ana_GenerateDencCodeYUV(stm_analog_sync_setup_t *AnalogSyncSetup_p , DACMult_Config_t *DACMult_Config_p)
{
    uint32_t ModeIndex;

    for(ModeIndex = 0; ModeIndex < Ana_GenerateAwgCodeFunction_Count; ModeIndex++)
    {
        if (Ana_GenerateAwgCodeFunction[ModeIndex].TimingMode == AnalogSyncSetup_p->TimingMode)
            return Ana_GenerateAwgCodeFunction[ModeIndex].Ana_GenerateAwgCode(AnalogSyncSetup_p, DACMult_Config_p);
    }

    return(STAWGFW_ERROR_FEATURE_NOT_SUPPORTED);
}
