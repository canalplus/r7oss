/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/fw_gen/awg_gen.c
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

#define DEFAULT_SCALING_FACTOR_Y  0x256
#define DEFAULT_OFFSET_FACTOR_Y  0xC5

#define DEFAULT_SCALING_FACTOR_CB  0x249
#define DEFAULT_OFFSET_FACTOR_CB  0xDB

#define DEFAULT_SCALING_FACTOR_CR  0x249
#define DEFAULT_OFFSET_FACTOR_CR  0xDB

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
    uint32_t                 instruction_offset;
    stm_analog_sync_setup_t *AnalogSyncSetup_p;
    HDFParams_t             *HDFParams_p;
} Ana_EDHDRAMGenerationParams_t;

typedef STAWGFW_ErrorCode_t (* Ana_GenerateAwgCodeFunction_t) (stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);

typedef struct Ana_GenerateAwgCode_s
{
    stm_display_mode_id_t           TimingMode;
    Ana_GenerateAwgCodeFunction_t   Ana_GenerateAwgCode;
} Ana_GenerateAwgCode_t;

static STAWGFW_ErrorCode_t Amplitude_Adjust(Ana_EDHDRAMGenerationParams_t* FmwGenParams_p, video_t *video_p);
static STAWGFW_ErrorCode_t TryAmplitude_Adjust(Ana_EDHDRAMGenerationParams_t *FmwGenParams_p, video_t *video_p);
static uint32_t Ana_GenerateInstruction_EDHD(int opcode,long int arg,int mux_sel,Ana_EDHDRAMGenerationParams_t *fmwGenParams_p);

/* RGB Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_RGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
/* SD Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480I60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode576I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
/* ED Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480P60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480P60_25_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode576P50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
/* HD Modes */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode720P50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode720P60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I50_72_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P24_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P25_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P30_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1152I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p);


static const Ana_GenerateAwgCode_t Ana_GenerateAwgCodeFunction[] =
{
/* SD Modes */
    { STM_TIMING_MODE_480I59940_12273,      Ana_GenerateAwgCode480I60_YPbPr     }
  , { STM_TIMING_MODE_480I59940_13500,      Ana_GenerateAwgCode480I60_YPbPr     }
  , { STM_TIMING_MODE_576I50000_13500,      Ana_GenerateAwgCode576I50_YPbPr     }
  , { STM_TIMING_MODE_576I50000_14750,      Ana_GenerateAwgCode576I50_YPbPr     }
/* ED Modes */
  , { STM_TIMING_MODE_480P59940_27000,      Ana_GenerateAwgCode480P60_YPbPr     }
  , { STM_TIMING_MODE_480P60000_25200,      Ana_GenerateAwgCode480P60_25_YPbPr  }
  , { STM_TIMING_MODE_480P60000_27027,      Ana_GenerateAwgCode480P60_YPbPr     }
  , { STM_TIMING_MODE_576P50000_27000,      Ana_GenerateAwgCode576P50_YPbPr     }
/* HD Modes */
  , { STM_TIMING_MODE_720P59940_74176,      Ana_GenerateAwgCode720P60_YPbPr     }
  , { STM_TIMING_MODE_720P50000_74250,      Ana_GenerateAwgCode720P50_YPbPr     }
  , { STM_TIMING_MODE_720P60000_74250,      Ana_GenerateAwgCode720P60_YPbPr     }
  , { STM_TIMING_MODE_1080I50000_72000,     Ana_GenerateAwgCode1080I50_72_YPbPr }
  , { STM_TIMING_MODE_1080I50000_74250_274M,Ana_GenerateAwgCode1080I50_YPbPr    }
  , { STM_TIMING_MODE_1080I59940_74176,     Ana_GenerateAwgCode1080I60_YPbPr    }
  , { STM_TIMING_MODE_1080I60000_74250,     Ana_GenerateAwgCode1080I60_YPbPr    }
  , { STM_TIMING_MODE_1080P23976_74176,     Ana_GenerateAwgCode1080P24_YPbPr    }
  , { STM_TIMING_MODE_1080P24000_74250,     Ana_GenerateAwgCode1080P24_YPbPr    }
  , { STM_TIMING_MODE_1080P25000_74250,     Ana_GenerateAwgCode1080P25_YPbPr    }
  , { STM_TIMING_MODE_1080P29970_74176,     Ana_GenerateAwgCode1080P30_YPbPr    }
  , { STM_TIMING_MODE_1080P30000_74250,     Ana_GenerateAwgCode1080P30_YPbPr    }
  , { STM_TIMING_MODE_1152I50000_48000,     Ana_GenerateAwgCode1152I50_YPbPr    }
};
static const uint8_t Ana_GenerateAwgCodeFunction_Count = N_ELEMENTS(Ana_GenerateAwgCodeFunction);


static STAWGFW_ErrorCode_t Amplitude_Adjust(Ana_EDHDRAMGenerationParams_t* FmwGenParams_p, video_t *video_p)
{
    STAWGFW_ErrorCode_t Error = STAWGFW_NO_ERROR;

    int scaling_factor_Cb;
    int scaling_factor_Y;
    int scaling_factor_Cr;

    short int ui_offset_factor_Cb;
    short int ui_offset_factor_Y;
    short int ui_offset_factor_Cr;

    short int final_offset_factor_Cb;
    short int final_offset_factor_Y;
    short int final_offset_factor_Cr;

    uint16_t final_scaling_factor_Cb;
    uint16_t final_scaling_factor_Y;
    uint16_t final_scaling_factor_Cr;

    int tmp;
    uint32_t *temp;
    uint32_t blank, blank_scaled;

    int Denc_DAC_Volt = 0;

    if(FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_13500)
        Denc_DAC_Volt = 1305;
    else if (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_13500)
        Denc_DAC_Volt = 1280;

    /* --------------------------------------------------------------------------------------------------------
     * Offset Factor
     * --------------------------------------------------------------------------------------------------------*/
    ui_offset_factor_Cb = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Cb;
    ui_offset_factor_Y  = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Y;
    ui_offset_factor_Cr = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Cr;

    /* ---------------------------------------------------------------------------------------------------------
     * Scaling Factor
     * ---------------------------------------------------------------------------------------------------------*/
    scaling_factor_Cb = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cb;
    scaling_factor_Y  = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Y;
    scaling_factor_Cr = FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cr;

    if( (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_12273)
     || (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_480I59940_13500)
     || (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_13500)
     || (FmwGenParams_p->AnalogSyncSetup_p->TimingMode == STM_TIMING_MODE_576I50000_14750))
    {
        /* final offset_factors to be programmed in the register */
        final_offset_factor_Cb = ui_offset_factor_Cb;
        final_offset_factor_Y  = ui_offset_factor_Y;
        final_offset_factor_Cr = ui_offset_factor_Cr;
        /* final scaling_factors to be programmed in the register */
        final_scaling_factor_Cb = (scaling_factor_Cb*Denc_DAC_Volt)/1400;
        final_scaling_factor_Y = (scaling_factor_Y*Denc_DAC_Volt)/1400;
        final_scaling_factor_Cr = (scaling_factor_Cr*Denc_DAC_Volt)/1400;
    }
    else
    {
        /* Computation of final offset_factors to be programmed in the register */
        if (FmwGenParams_p->IsRGB)
        {
            final_offset_factor_Cb = (DEFAULT_OFFSET_FACTOR_Y*scaling_factor_Cb + 512)/1024;
            final_offset_factor_Cb = final_offset_factor_Cb + ui_offset_factor_Cb;

            final_offset_factor_Cr = (DEFAULT_OFFSET_FACTOR_Y*scaling_factor_Cr + 512)/1024;
            final_offset_factor_Cr = final_offset_factor_Cr + ui_offset_factor_Cr;

            /* Computation of final scaling_factors to be programmed in the register */
            final_scaling_factor_Cb = (DEFAULT_SCALING_FACTOR_Y*scaling_factor_Cb + 512)/1024;
            final_scaling_factor_Cr = (DEFAULT_SCALING_FACTOR_Y*scaling_factor_Cr + 512)/1024;
        }
        else
        {
            final_offset_factor_Cb = (DEFAULT_OFFSET_FACTOR_CB*scaling_factor_Cb + 512)/1024;
            final_offset_factor_Cb = final_offset_factor_Cb + ui_offset_factor_Cb;


            final_offset_factor_Cr = (DEFAULT_OFFSET_FACTOR_CR*scaling_factor_Cr + 512)/1024;
            final_offset_factor_Cr = final_offset_factor_Cr + ui_offset_factor_Cr;
            /* Computation of final scaling_factors to be programmed in the register */
            final_scaling_factor_Cb = (DEFAULT_SCALING_FACTOR_CB*scaling_factor_Cb + 512)/1024;
            final_scaling_factor_Cr  = (DEFAULT_SCALING_FACTOR_CR*scaling_factor_Cr + 512)/1024;
        }

        final_offset_factor_Y = (DEFAULT_OFFSET_FACTOR_Y*scaling_factor_Y + 512)/1024;
        final_offset_factor_Y = final_offset_factor_Y + ui_offset_factor_Y;

        /* Computation of final scaling_factors to be programmed in the register */
        final_scaling_factor_Y  = (DEFAULT_SCALING_FACTOR_Y*scaling_factor_Y + 512)/1024;
    }

    if (final_offset_factor_Cb < -1024)
    {
        final_offset_factor_Cb = -1024;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (final_offset_factor_Cb > 1023)
    {
        final_offset_factor_Cb = 1023;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (final_offset_factor_Y < -1024)
    {
        final_offset_factor_Y = -1024;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (final_offset_factor_Y > 1023)
    {
        final_offset_factor_Y = 1023;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (final_offset_factor_Cr < -1024)
    {
        final_offset_factor_Cr = -1024;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }
    else if (final_offset_factor_Cr > 1023)
    {
        final_offset_factor_Cr = 1023;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (final_scaling_factor_Cb >= 2048)
    {
        final_scaling_factor_Cb = 2047;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (final_scaling_factor_Y >= 2048)
    {
        final_scaling_factor_Y = 2047;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    if (final_scaling_factor_Cr >= 2048)
    {
        final_scaling_factor_Cr = 2047;
        Error=STAWGFW_ERROR_REG_OVERFLOW;
    }

    /* Writing in to the register starts */

    /* ------------------------------------------------------------------------------------------------------
     * Writing to ANA_SCALE_CTRL_Cb
     * ------------------------------------------------------------------------------------------------------*/
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb = final_offset_factor_Cb;

    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb << 16;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb + final_scaling_factor_Cb;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cb & 0x07FF07FF;

    /* ------------------------------------------------------------------------------------------------------
     * Writing to ANA_SCALE_CTRL_Y
     * ------------------------------------------------------------------------------------------------------*/
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y = final_offset_factor_Y;

    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y << 16;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y + final_scaling_factor_Y;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Y & 0x07FF07FF;

    /* ------------------------------------------------------------------------------------------------------
     * Writing to ANA_SCALE_CTRL_Cr
     * ------------------------------------------------------------------------------------------------------*/
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr = final_offset_factor_Cr;

    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr << 16;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr + final_scaling_factor_Cr;
    FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr =
        FmwGenParams_p->HDFParams_p->ANA_SCALE_CTRL_DAC_Cr & 0x07FF07FF;


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
        /* add offset */
        tmp = tmp + ui_offset_factor_Y;
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
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cb = final_scaling_factor_Cb;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Y  = final_scaling_factor_Y;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Scaling_factor_Cr = final_scaling_factor_Cr;

        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Cb  = final_offset_factor_Cb;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Y   = final_offset_factor_Y;
        FmwGenParams_p->AnalogSyncSetup_p->AnalogFactors.Offset_factor_Cr  = final_offset_factor_Cr;
    }

    return(Error);
}


static STAWGFW_ErrorCode_t TryAmplitude_Adjust(Ana_EDHDRAMGenerationParams_t *FmwGenParams_p, video_t *video_p)
{
    STAWGFW_ErrorCode_t Error;

    TRC( TRC_ID_MAIN_INFO, "In : TimingMode=%d, IsRGB=%d.",FmwGenParams_p->AnalogSyncSetup_p->TimingMode, FmwGenParams_p->IsRGB );

    if(!Get_AS_Levels(FmwGenParams_p->AnalogSyncSetup_p->TimingMode, video_p))
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


static uint32_t Ana_GenerateInstruction_EDHD(int opcode,long int arg,int mux_sel,Ana_EDHDRAMGenerationParams_t *fmwGenParams_p)
{
    uint32_t instruction;
    uint32_t mux = (mux_sel<<8)&0x1ff;
    uint32_t opcode_offset=10;

    if (fmwGenParams_p->instruction_offset >= AWG_MAX_SIZE)
    {
        return 0;
    }

    if(opcode==SKIP || opcode==REPEAT || opcode==REPLAY || opcode==JUMP)
    {
        mux=0;
        arg = (arg << 22) >> 22;
        arg &= (0x3ff);
    }
    else if(opcode==STOP)
    {
        arg=0;
    }
    else
    {
        arg = (arg << 24) >> 24;
        arg &= (0x0ff);
    }
    arg = arg + mux;

    instruction=((opcode)<<opcode_offset) | arg;
    fmwGenParams_p->HDFParams_p->ANARAMRegisters[fmwGenParams_p->instruction_offset]=instruction & (0x3fff);
    fmwGenParams_p->instruction_offset+=1;

    return instruction;
}


/* RGB Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCodeXXXYZZ_RGB
   Description: Update the DencMult Config Registers
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCodeXXXYZZ_RGB(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = TRUE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);

    return(Error) ;
}



/* SD Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode480I60_YPbPr
   Description: Update the DencMult Config Registers
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480I60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode576I50_YPbPr
   Description: Update the DencMult Config Registers
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode576I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);

    return(Error) ;
}


/* ED Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode480P60_YPbPr
   Description: Generate firmware for 480P/60/59.94
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480P60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
/*********
 * This is the original design firmware which is defined with
 * line 1 as the first line of the front porch _not_ the first line of the
 * vsync lines. The rest of the display driver defines 480p in HD SMPTE
 * style to match all other modes (and the hardware) where line 1 is the first
 * line of the vsync lines, which is how the VTG and Mixer active areas are set
 * up.
 *
 * So we are changing the design firmware for now to match what we did
 * before we integrated them, where the first line of the program generates the
 * first line of the vsync. You will note the similarity to the 576p program
 * below.

        Ana_GenerateInstruction_EDHD(RPLSET , video.sync_low        , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP   , 61                    , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET    , video.blank_act_vid   , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY , 5                     , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET , video.sync_low        , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP   , 700                   , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP   , 92                    , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET    , video.blank_act_vid   , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY , 5                     , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET , video.sync_low        , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP   , 61                    , 1 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET    , video.blank_act_vid   , 0 , &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY , 512                   , 0 , &FmwGenParams);
 **********/

        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     792,                  1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid,  1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   5,                    1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     61,                   1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid,  0,   &FmwGenParams); /* Force blank value during back porch */
        Ana_GenerateInstruction_EDHD(REPLAY,   518,                  1,   &FmwGenParams);

    }

    return(Error);
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode480P60_25_YPbPr
   Description: Generate firmware for 480P/60 at pixel clock 25.175MHz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode480P60_25_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   1,                1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.blank_act_vid,  1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   24,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     94,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid,  0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   495,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.blank_act_vid,  1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   2,                1,   &FmwGenParams);
    }

    return(Error);
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode576P50_YPbPr
   Description: Generate firmware for 576p/50Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode576P50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     799,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid,  1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,                1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,       1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     61,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid,  0,   &FmwGenParams); /* Force blank value during back porch */
        Ana_GenerateInstruction_EDHD(REPLAY,   619,              1,   &FmwGenParams);
    }

    return(Error);
}


/* HD Modes */
/*==========*/

/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode720P50_YPbPr
   Description: Generate firmware for 720P/50Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode720P50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     38,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     218,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     638,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     639,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     398,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     38,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     948,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     949,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   744,             1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode720P60_YPbPr
   Description: Generate firmware for 720P/60/59.94Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode720P60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     38,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     218,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     638,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     639,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     68,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     38,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     783,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     784,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   744,             1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080I50_YPbPr
   Description: Generate firmware for 1080I/50Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     86,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     878,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     262,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     41,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   9,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     614,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     615,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     41,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   1,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     702,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     923,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     923,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   555,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+13,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+18,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+1,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+11,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+13,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+18,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+21,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+26,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   556,             1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080I50_72_YPbPr
   Description: Generate firmware for 1080I/50Hz at 72MHz pixel clock
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I50_72_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     982,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     166,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     166,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     982,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     166,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   622,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+5,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+8,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+2,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+4,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     166,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   624,             1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080I60_YPbPr
   Description: Generate firmware for 1080I/60/59.94Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080I60_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     86,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     878,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     41,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   9,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     1010,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     41,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   1,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     702,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     702,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     704,             0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   555,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+13,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+17,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+1,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+11,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+13,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+16,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(STOP,     0x01,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+20,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+25,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   556,             1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080P24_YPbPr
   Description: Generate firmware for 1080P/24Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P24_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     86,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     988,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     989,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     592,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     886,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     886,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     886,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   559,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 12,         1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 19,         1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080P25_YPbPr
   Description: Generate firmware for 1080P/25Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P25_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     86,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     988,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     989,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     482,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     849,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     849,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     850,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   559,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 12,         1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 19,         1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1080P30_YPbPr
   Description: Generate firmware for 1080P/30Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1080P30_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     86,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     988,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     989,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   4,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_high,     1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     42,              1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     702,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     703,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     703,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   559,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 12,         1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63 + 19,         1,   &FmwGenParams);
    }

    return(Error) ;
}


/* ==========================================================================================================
   Name:        Ana_GenerateAwgCode1152I50_YPbPr
   Description: Generate firmware for 1152I/50Hz
   ========================================================================================================== */
static STAWGFW_ErrorCode_t Ana_GenerateAwgCode1152I50_YPbPr(stm_analog_sync_setup_t *AnalogSyncSetup_p, HDFParams_t *HDFParams_p)
{
    STAWGFW_ErrorCode_t Error;
    Ana_EDHDRAMGenerationParams_t FmwGenParams;
    video_t video;

    FmwGenParams.IsRGB               = FALSE;
    FmwGenParams.instruction_offset  = 0;
    FmwGenParams.HDFParams_p         = HDFParams_p;
    FmwGenParams.AnalogSyncSetup_p   = AnalogSyncSetup_p;

    Error = TryAmplitude_Adjust(&FmwGenParams, &video);
    if(Error == STAWGFW_NO_ERROR)
    {
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     654,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     109,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   9,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPTSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     110,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     653,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPEAT,   1,               1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     110,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   617,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     110,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     654,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+1,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+5,            1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+16,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(JUMP,     63+18,           1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(RPLSET,   video.sync_low,      1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SKIP,     110,             1,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(SET,      video.blank_act_vid, 0,   &FmwGenParams);
        Ana_GenerateInstruction_EDHD(REPLAY,   619,             1,   &FmwGenParams);
    }

    return(Error) ;
}




/* Public AS_AWG_FW API */
int Ana_GenerateAwgCodeRGB(stm_analog_sync_setup_t *AnalogSyncSetup_p , HDFParams_t *HDFParams_p)
{
    return Ana_GenerateAwgCodeXXXYZZ_RGB(AnalogSyncSetup_p, HDFParams_p);
}


int Ana_GenerateAwgCodeYUV(stm_analog_sync_setup_t *AnalogSyncSetup_p , HDFParams_t *HDFParams_p)
{
    uint32_t ModeIndex;

    for(ModeIndex = 0; ModeIndex < Ana_GenerateAwgCodeFunction_Count; ModeIndex++)
    {
        if (Ana_GenerateAwgCodeFunction[ModeIndex].TimingMode == AnalogSyncSetup_p->TimingMode)
            return Ana_GenerateAwgCodeFunction[ModeIndex].Ana_GenerateAwgCode(AnalogSyncSetup_p, HDFParams_p);
    }

    return(STAWGFW_ERROR_FEATURE_NOT_SUPPORTED);
}
