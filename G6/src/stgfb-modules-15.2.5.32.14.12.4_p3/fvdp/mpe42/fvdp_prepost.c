/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_prepost.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_update.h"
#include "fvdp_common.h"
#include "fvdp_prepost.h"

#include "fvdp_dfr.h"
//#include "fvdp_motdet.h"
//#include "fvdp_scaler.h"
//#include "fvdp_mcc.h"
//#include "fvdp_mbox.h"
//#include "fvdp_vcpu.h"


/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

// use this compile option to dynamically adjust crop window using spare registers
//#define DEBUG_CROP_TESTING

// temp patch to fix 1088i GAM file issue
//#define PATCH_1088I

#define VSIZE_1080I 540 // vertical line number
#define VSIZE_1080P 1080 // vertical line number
#define FRAME_RATE_25Hz 25000
/* Private Macros ----------------------------------------------------------- */

#define TrPRE(level,...)    TraceModuleOn(level,PRE_MODULE[CH].DebugLevel,__VA_ARGS__)
#define TrPOST(level,...)   TraceModuleOn(level,POST_MODULE[CH].DebugLevel,__VA_ARGS__)

/* Private Functions -------------------------------------------------------- */

static FVDP_Result_t PRE_Init (FVDP_CH_t CH, InitOptions_t options);
static FVDP_Result_t PRE_Term (FVDP_CH_t CH, TermOptions_t options);
static FVDP_Result_t PRE_Update (FVDP_CH_t CH, UpdateOptions_t options);
static FVDP_Result_t POST_Init (FVDP_CH_t CH, InitOptions_t options);
static FVDP_Result_t POST_Term (FVDP_CH_t CH, TermOptions_t options);
static FVDP_Result_t POST_Update (FVDP_CH_t CH, UpdateOptions_t options);
#ifdef DEBUG_CROP_TESTING
static void          DebugCropTesting (void);
#endif

/* Private Variables (static)------------------------------------------------ */

DataPathModule_t PRE_MODULE[NUM_FVDP_CH] =
{
    {
        PRE_MODULE_ID,      // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &PRE_Init,          // pInit
        &PRE_Term,          // pTerm
        &PRE_Update,        // pUpdate
        NULL                // pAux
    },
    {
        PRE_MODULE_ID,      // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &PRE_Init,          // pInit
        &PRE_Term,          // pTerm
        &PRE_Update,        // pUpdate
        NULL                // pAux
    },
    {
        PRE_MODULE_ID,      // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &PRE_Init,          // pInit
        &PRE_Term,          // pTerm
        &PRE_Update,        // pUpdate
        NULL                // pAux
    }
};

DataPathModule_t POST_MODULE[NUM_FVDP_CH] =
{
    {
        POST_MODULE_ID,     // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &POST_Init,         // pInit
        &POST_Term,         // pTerm
        &POST_Update,       // pUpdate
        NULL                // pAux
    },
    {
        POST_MODULE_ID,     // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &POST_Init,         // pInit
        &POST_Term,         // pTerm
        &POST_Update,       // pUpdate
        NULL                // pAux
    },
    {
        POST_MODULE_ID,     // pModuleID
        2,        // DebugLevel
        NULL, NULL,         // pPrev, pNext
        NULL,               // pConfig
        {0, 0},             // DramConfig
        NULL,               // pPrivate
        &POST_Init,         // pInit
        &POST_Term,         // pTerm
        &POST_Update,       // pUpdate
        NULL                // pAux
    }
};

static DataPathConfig_t PrevInputConfig[NUM_FVDP_CH];
static uint16_t AdjInpWidth[NUM_FVDP_CH];
static uint16_t AdjInpHeight[NUM_FVDP_CH];

/* Functions ---------------------------------------------------------------- */

FVDP_Result_t PRE_Init (FVDP_CH_t CH, InitOptions_t options)
{
    // reset previous configuration info
    ZERO_MEMORY(&PrevInputConfig[CH],sizeof(DataPathConfig_t));

    return FVDP_OK;
}

FVDP_Result_t PRE_Term (FVDP_CH_t CH, TermOptions_t options)
{
    return FVDP_OK;
}

FVDP_Result_t PRE_Update (FVDP_CH_t CH, UpdateOptions_t options)
{
    DataPathConfig_t* pConfig = pThisModule->pConfig;
    FVDP_CropWindow_t* pWin = &pConfig->CropWindow;
    int16_t Posoffset = 0;

    #ifdef DEINT_BYPASS_SEL
    bool deint_bypass_mode = DEINT_BYPASS_SEL;
    #else
    bool deint_bypass_mode = FALSE;
    #endif

#ifdef DEBUG_CROP_TESTING
    DebugCropTesting();
#endif
#if (0)
    if(WA_PIP_FBM_INTERLACED_OUT == TRUE && \
    CH==FVDP_PIP && pConfig->OutputVideoInfo.ScanType==INTERLACED)
    {
        /* Locate the scaler after memory so that even/odd can be alternated. */
        pConfig->InternalFlags.vscaler_force_update_stage = TRUE;
        pConfig->InternalFlags.vscaler_input_stage = FALSE;
    }
#endif
#ifdef PATCH_1088I
    // temp patch to fix 1088i issue
    if (pConfig->InputVideoInfo.ScanType == INTERLACED &&
        pConfig->InputVideoInfo.Height >= 544 && pWin->VCropHeight == pConfig->InputVideoInfo.Height)
        pWin->VCropHeight = 540;
#endif

// DEINT interlace output enable when 1080i/p input ==> 1080i
    if((CH == FVDP_MAIN)
        &&(((pConfig->InputVideoInfo.ScanType == INTERLACED)&&(pConfig->InputVideoInfo.Height >= VSIZE_1080I)) /*1080i input*/
            ||((pConfig->InputVideoInfo.ScanType == PROGRESSIVE)&&(pConfig->InputVideoInfo.Height >= VSIZE_1080P)&&(pConfig->InputVideoInfo.FrameRate > 30000/*mHz*/))) /*1080p input*/
        && ((pConfig->OutputVideoInfo.ScanType == INTERLACED)&&(pConfig->OutputVideoInfo.Height >= VSIZE_1080I)) /*1080i output*/
        && (deint_bypass_mode == FALSE))
    {
        pConfig->InternalFlags.deint_interlaced_out= TRUE;
    }
    // restraint crop width to even values only
    pWin->HCropWidth &= ~BIT0;

    // limit crop window horizontal position and size to active window
    if (pWin->HCropWidth > pConfig->InputVideoInfo.Width)
        pWin->HCropWidth = pConfig->InputVideoInfo.Width;
    if (pWin->HCropWidth + pWin->HCropStart > pConfig->InputVideoInfo.Width)
        pWin->HCropStart = pConfig->InputVideoInfo.Width - pWin->HCropWidth;

    // limit crop window vertical position and size to active window
    if (pWin->VCropHeight > pConfig->InputVideoInfo.Height)
        pWin->VCropHeight = pConfig->InputVideoInfo.Height;
    if (pWin->VCropHeight + pWin->VCropStart > pConfig->InputVideoInfo.Height)
        pWin->VCropStart = pConfig->InputVideoInfo.Height - pWin->VCropHeight;

    // update internal flags based on original input structure differences
    UpdateInternalFlags (pConfig, &PrevInputConfig[CH]);
    PrevInputConfig[CH] = *pConfig;

    if (pConfig->InternalFlags.hcrop_touched ||
        pConfig->InternalFlags.hscaler_touched)
    {
        // adjust the horizontal input size for required cropping on output side
        if ((pConfig->InputVideoInfo.Width != pWin->HCropWidth) &&
            (pWin->HCropWidth != 0))
        {
            Posoffset = (pWin->HCropWidth * MAX_MEMORY_CROP_H_START*100) /
                        pConfig->OutputVideoInfo.Width;
            if(Posoffset > (MAX_MEMORY_CROP_H_START*100))
            {
                Posoffset = MAX_MEMORY_CROP_H_START;
            }
            else
            {
                Posoffset /= 100;
            }
            if(((pConfig->InputVideoInfo.Width -pWin->HCropWidth) > (Posoffset*2))
             &&(pConfig->InputVideoInfo.Width > (pWin->HCropWidth +pWin->HCropStart + MAX_MEMORY_CROP_H_START))
                &&((MAX_MEMORY_CROP_H_START) < 2048))
            {
                if((pWin->HCropWidth+(Posoffset*2))<pConfig->InputVideoInfo.Width)
                    AdjInpWidth[CH] = pWin->HCropWidth+(Posoffset*2);
                else
                    AdjInpWidth[CH] = pConfig->InputVideoInfo.Width;
            }
            else
                AdjInpWidth[CH] = pWin->HCropWidth;
        }
        else
            AdjInpWidth[CH] = pConfig->InputVideoInfo.Width;
    }

    pConfig->InputVideoInfo.Width = AdjInpWidth[CH];

    if (pConfig->InternalFlags.vcrop_touched ||
        pConfig->InternalFlags.vscaler_touched)
    {
        // adjust the vertical input size for required cropping on output side
        if ((pConfig->InputVideoInfo.Height!= pWin->VCropHeight) &&
            (pWin->VCropHeight != 0))
        {
            Posoffset = (pWin->VCropHeight * MAX_MEMORY_CROP_V_START*100) /
                        pConfig->OutputVideoInfo.Height;
            if(Posoffset > (MAX_MEMORY_CROP_V_START*100))
            {
                Posoffset = MAX_MEMORY_CROP_V_START;
            }
            else
            {
                Posoffset /= 100;
            }

            if(((pConfig->InputVideoInfo.Height -pWin->VCropHeight) > (Posoffset*2))
                &&(pConfig->InputVideoInfo.Height > (pWin->VCropHeight +pWin->VCropStart + MAX_MEMORY_CROP_V_START))
                &&((MAX_MEMORY_CROP_V_START) < 2048))
            {
                if((pWin->VCropHeight+(Posoffset*2))<pConfig->InputVideoInfo.Height)
                    AdjInpHeight[CH] = pWin->VCropHeight+(Posoffset*2);
                else
                    AdjInpHeight[CH] = pConfig->InputVideoInfo.Height;
            }
            else
                AdjInpHeight[CH] = pWin->VCropHeight;
        }
        else
            AdjInpHeight[CH] = pConfig->InputVideoInfo.Height;
    }

    pConfig->InputVideoInfo.Height = AdjInpHeight[CH];

    pConfig->InternalFlags.deint_bypass = deint_bypass_mode;

    if (gProcessingType[CH] == FVDP_SPATIAL)
        pConfig->InternalFlags.deint_bypass = TRUE;
#if (0)
#if (RGB_DATA_PATH_TEST ==TRUE)
#define RGB_RAMP_PATTERN 0x831
   if(CH == FVDP_MAIN)
   {
       pConfig->InputVideoInfo.ColorSpace = RGB;
       pConfig->InputVideoInfo.ColorSampling=FVDP_444;
       REG32_Write(MIVP_PATGEN_CONTROL, RGB_RAMP_PATTERN);
   }
#endif
    pConfig->InternalFlags.rgb30bitprocess = RGB_30BITPROC_SEL;

    if( pConfig->InputVideoInfo.ColorSpace ==  RGB ||
        pConfig->InputVideoInfo.ColorSpace == sRGB )
    {
        /* Bypass DEINT for 30 bit RGB process*/
        if(pConfig->InternalFlags.rgb30bitprocess == TRUE)
        {
            pConfig->InternalFlags.deint_bypass = TRUE;
        }
    }

    if (pConfig->OutputVideoInfo.ScanType == INTERLACED &&
        DEINT_BYPASS_INTERLACE_OUT && CH == FVDP_MAIN)
        pConfig->InternalFlags.deint_bypass = TRUE;

#if (FORCED_HV_SCALER_OUTPUT_STAGE==TRUE)
    pConfig->InternalFlags.hscaler_force_update_stage = TRUE;
    pConfig->InternalFlags.vscaler_force_update_stage = TRUE;
    pConfig->InternalFlags.hscaler_input_stage = FALSE;
    pConfig->InternalFlags.vscaler_input_stage = FALSE;
#endif
#endif
    return FVDP_OK;
}

FVDP_Result_t POST_Init (FVDP_CH_t CH, InitOptions_t options)
{
    if (POST_MODULE[CH].pPrev)
    {
        POST_MODULE[CH].pConfig = POST_MODULE[CH].pPrev->pConfig;
        POST_MODULE[CH].DramConfig.StartAddr = \
                POST_MODULE[CH].pPrev->DramConfig.StartAddr + \
                POST_MODULE[CH].pPrev->DramConfig.Size;

#if 0 //defined(USE_VCPU)
        // start the VCPU here if not done so already.  Note that this must
        // be done last to ensure that no VCPU programming is overwritten
        // by the HOST (eg. MOT_DET_HARD_RESET is triggered in motdet_HwInit(),
        // and MIVP_HARD_RESET in ivp_HwInit()).
        if (CH == FVDP_MAIN &&
            VCPU_Initialized() == TRUE && VCPU_Started() == FALSE)
        {
            FVDP_Result_t result = VCPU_Start();
            if (result != FVDP_OK)
            {
                TRC( TRC_ID_MAIN_INFO, "VCPU code start error %d, ABORTED", result );
                return result;
            }
        }

        if(VCPU_Started() == TRUE)
        {
            FBM_Reset(CH);
        }

        // Send size and number of buffers to VCPU.  This is performed in
        // POST_Init() and not in FBM_Init() because modules following FBM must
        // have their say on their own individual buffer requirements first.
        {
            BufferTable_t BufferTable[NUM_BUFFER_TYPES];
            //FBM_IF_CreateBufferTable(CH,&BufferTable);
            FBM_IF_SendBufferTable(CH,&BufferTable);
        }
#endif
    }

    return FVDP_OK;
}

FVDP_Result_t POST_Term (FVDP_CH_t CH, TermOptions_t options)
{
    return FVDP_OK;
}

FVDP_Result_t POST_Update (FVDP_CH_t CH, UpdateOptions_t options)
{
    //DataPathConfig_t* pConfig = pThisModule->pConfig;

    //if (pConfig->InternalFlags.skip_this == FALSE)
    //OutputWindowInfo[CH] = pConfig->OutputVideoInfo;

    return FVDP_OK;
}

#ifdef DEBUG_CROP_TESTING
#define CROP_TEST_DELAY         5
#define CROP_TEST_MIN_SIZE      10
#define CROP_TEST_STEP_SIZE     1
#define CROP_TEST_FULL_COUNT    200

void DebugCropTesting (void)
{
#if (0)
        DataPathConfig_t* pConfig = pThisModule->pConfig;
        FVDP_CropWindow_t* pWin = &pConfig->CropWindow;
        static uint8_t delay;
        static uint8_t testcount = 0;
        uint32_t h_crop = REG32_Read(SCALERS_SW_SPARE_1);
        uint32_t v_crop = REG32_Read(SCALERS_SW_SPARE_2);
        uint32_t h_pos  = REG32_Read(SCALERS_SW_SPARE_3);
        uint32_t v_pos  = REG32_Read(SCALERS_SW_SPARE_4);
        uint32_t auto_test_state = REG32_Read(SCALERS_SW_SPARE_5);
        uint8_t incr_width = 0, incr_height = 0, incr_hpos = 0, incr_vpos = 0;

        switch (auto_test_state)
        {
        case 1:
            auto_test_state = 2;
            REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            testcount = CROP_TEST_FULL_COUNT;
            incr_width = testcount;
            incr_height = testcount;
            incr_hpos = 0;
            incr_vpos = 0;
            delay = CROP_TEST_DELAY;
            break;

        case 2: // shrink to top-left
            if (--delay == 0)
            {
                testcount -= CROP_TEST_STEP_SIZE;
                if (testcount < CROP_TEST_MIN_SIZE)
                    testcount = CROP_TEST_MIN_SIZE;
                delay = CROP_TEST_DELAY;
            }
            incr_width = testcount;
            incr_height = testcount;
            incr_hpos = 0;
            incr_vpos = 0;
            if (testcount == CROP_TEST_MIN_SIZE)
            {
                testcount = 0;
                auto_test_state = 3;
                REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            }
            break;

        case 3: // slide to bottom-right
            if (--delay == 0)
            {
                testcount += CROP_TEST_STEP_SIZE;
                if (testcount > CROP_TEST_FULL_COUNT-CROP_TEST_MIN_SIZE)
                    testcount = CROP_TEST_FULL_COUNT-CROP_TEST_MIN_SIZE;
                delay = CROP_TEST_DELAY;
            }
            incr_width = CROP_TEST_MIN_SIZE;
            incr_height = CROP_TEST_MIN_SIZE;
            incr_hpos = testcount;
            incr_vpos = testcount;
            if (testcount == CROP_TEST_FULL_COUNT-CROP_TEST_MIN_SIZE)
            {
                testcount = CROP_TEST_MIN_SIZE;
                auto_test_state = 4;
                REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            }
            break;

        case 4: // expand to full-size
            if (--delay == 0)
            {
                testcount += CROP_TEST_STEP_SIZE;
                if (testcount > CROP_TEST_FULL_COUNT)
                    testcount = CROP_TEST_FULL_COUNT;
                delay = CROP_TEST_DELAY;
            }
            incr_width  = testcount;
            incr_height = testcount;
            incr_hpos   = CROP_TEST_FULL_COUNT-testcount;
            incr_vpos   = CROP_TEST_FULL_COUNT-testcount;
            if (testcount == CROP_TEST_FULL_COUNT)
            {
                auto_test_state = 5;
                REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            }
            break;

        case 5: // shrink center
            if (--delay == 0)
            {
                testcount -= 2;
                if (testcount < CROP_TEST_MIN_SIZE)
                    testcount = CROP_TEST_MIN_SIZE;
                delay = CROP_TEST_DELAY;
            }
            incr_width  = testcount;
            incr_height = testcount;
            incr_hpos   = (CROP_TEST_FULL_COUNT-testcount) / 2;
            incr_vpos   = (CROP_TEST_FULL_COUNT-testcount) / 2;
            if (testcount == CROP_TEST_MIN_SIZE)
            {
                auto_test_state = 6;
                REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            }
            break;

        case 6: // expand center quickly
            testcount += 2;
            if (testcount > CROP_TEST_FULL_COUNT)
                testcount = CROP_TEST_FULL_COUNT;

            incr_width  = testcount;
            incr_height = testcount;
            incr_hpos   = (CROP_TEST_FULL_COUNT-testcount) / 2;
            incr_vpos   = (CROP_TEST_FULL_COUNT-testcount) / 2;
            if (testcount == CROP_TEST_FULL_COUNT)
            {
                testcount = 0;
                auto_test_state = 7;
                REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            }
            break;

        case 7:
            auto_test_state = 0;
            REG32_Write(SCALERS_SW_SPARE_5, auto_test_state);
            break;

        default:
            break;
        }

        if (auto_test_state > 0)
        {
            pWin->HCropWidth  = pConfig->InputVideoInfo.Width *
                                incr_width / CROP_TEST_FULL_COUNT;
            pWin->VCropHeight = pConfig->InputVideoInfo.Height *
                                incr_height / CROP_TEST_FULL_COUNT;
            pWin->HCropStart  = pConfig->InputVideoInfo.Width *
                                incr_hpos / CROP_TEST_FULL_COUNT;
            pWin->VCropStart  = pConfig->InputVideoInfo.Height *
                                incr_vpos / CROP_TEST_FULL_COUNT;
            if (pWin->HCropWidth < 74)
                pWin->HCropWidth = 74;
            if (pWin->VCropHeight < 16)
                pWin->VCropHeight = 16;
        }

        if (h_crop > 0 && h_crop < pConfig->InputVideoInfo.Width)
            pWin->HCropWidth = pConfig->InputVideoInfo.Width - h_crop;

        if (h_pos > 0)
        {
            pWin->HCropStart = h_pos & 0xFFFF;
            pWin->HCropStartOffset = h_pos >> 16;
        }

        if (v_crop > 0 && v_crop < pConfig->InputVideoInfo.Height)
            pWin->VCropHeight = pConfig->InputVideoInfo.Height - v_crop;

        if (v_pos > 0)
        {
            pWin->VCropStart = v_pos & 0xFFFF;
            pWin->VCropStartOffset = v_pos >> 16;
        }
#endif
}
#endif

