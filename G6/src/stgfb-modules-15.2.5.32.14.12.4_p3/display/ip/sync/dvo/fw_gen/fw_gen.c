/***********************************************************************
 *
 * File: display/ip/sync/dvo/fw_gen/fw_gen.c
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "fw_gen.h"
#include "vibe_debug.h"


#define OUTPUT_IS_YCBCR422_8BITS(out)         ((out) == (STM_VIDEO_OUT_YUV|STM_VIDEO_OUT_ITUR656))

#define MODE_IS_SD(mode)                      ( ((mode) == STM_TIMING_MODE_480I59940_13500) || ((mode) == STM_TIMING_MODE_576I50000_13500) )

#define DVO_AWG_MAX_SIZE 64

enum
{
    SET
  , RPTSET
  , RPLSET
  , SKIP
  , STOP
  , REPEAT
  , REPLAY
  , JUMP
  , HOLD
/*, mux_sel
  , data_en*/
};

enum
{
    FWGEN_NO_ERROR
  , FWGEN_ERROR_BAD_PARAMETER
};


typedef struct
{
  stm_display_mode_id_t TimingMode;
  bool                  IsModeProgressive;
  uint32_t              Total_Lines;
  uint32_t              Blanking_Lines;
  uint32_t              Trailing_Lines;
  uint32_t              Total_Pixels;
  uint32_t              Blanking_Pixels;
  uint32_t              Trailing_Pixels;
  uint32_t              Blanking_Level;
} stm_dvo_awg_standard_specification_t;

typedef struct
{
  uint8_t  Data_Enable;
  uint8_t  Mux_Sel;
  uint16_t Sav_Eav_Start;
  uint16_t Sav_Eav_Mid;
  uint16_t Sav_Blanking_End;
  uint16_t Eav_Blanking_End;
  uint16_t Sav_Active_End;
  uint16_t Eav_Active_End;
} stm_dvo_awg_sav_eav_params_t;


typedef struct
{
  uint32_t *               AWGParams_p;
  stm_display_mode_id_t    TimingMode;
  uint32_t                 uFormat;
  stm_awg_dvo_parameters_t sAwgCodeParams;
  uint8_t                  Instruction_Offset;
  int                      Error;
} stm_dvo_awg_code_generation_params_t;

static uint32_t stm_dvo_awg_generate_instruction(int Opcode,long int Arg,long int Mux_Sel,long int Data_En, stm_dvo_awg_code_generation_params_t *FmwGenParams_p) ;
void stm_dvo_awg_set_spec_params(stm_dvo_awg_code_generation_params_t *FmwGenParams_p);


/*  VTG_Mode, Total_Lines, Blanking_Lines, Trailing_Lines, Total_Pixels, Blanking_Pixels, Trailing_Pixels, Blanking_Level  */
static const stm_dvo_awg_standard_specification_t Dvo_Standards[] = {
    { STM_TIMING_MODE_480I59940_13500,        FALSE, 262,  18, 4,   858,  119, 19,  16 }/* _CEA861 */
  , { STM_TIMING_MODE_576I50000_13500,        FALSE, 312,  22, 2,   864,  132, 12,  16 }
  , { STM_TIMING_MODE_480P60000_27027,        TRUE,  525,  36, 9,   858,  122, 16,  16 }/* _CEA861 */
  , { STM_TIMING_MODE_480P59940_27000,        TRUE,  525,  36, 9,   858,  122, 16,  16 }/* _CEA861 */
  , { STM_TIMING_MODE_540P59940_36343,        TRUE,  583,  10, 33,  1040, 60,  20,  16 }
  , { STM_TIMING_MODE_540P49993_36343,        TRUE,  699,  10, 149, 1040, 60,  20,  16 }
  , { STM_TIMING_MODE_576P50000_27000,        TRUE,  625,  44, 5,   864,  132, 12,  16 }
  , { STM_TIMING_MODE_720P50000_74250,        TRUE,  750,  25, 5,   1980, 260, 440, 16 }
  , { STM_TIMING_MODE_720P59940_74176,        TRUE,  750,  25, 5,   1650, 260, 110, 16 }
  , { STM_TIMING_MODE_720P60000_74250,        TRUE,  750,  25, 5,   1650, 260, 110, 16 }
  , { STM_TIMING_MODE_1080I50000_74250_274M,  FALSE, 563,  20, 3,   2640, 192, 528, 16 }
  , { STM_TIMING_MODE_1080I59940_74176,       FALSE, 563,  20, 3,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080I60000_74250,       FALSE, 563,  20, 3,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080P23976_74176,       TRUE,  1125, 41, 4,   2750, 192, 638, 16 }
  , { STM_TIMING_MODE_1080P24000_74250,       TRUE,  1125, 41, 4,   2750, 192, 638, 16 }
  , { STM_TIMING_MODE_1080P25000_74250,       TRUE,  1125, 41, 4,   2640, 192, 528, 16 }
  , { STM_TIMING_MODE_1080P29970_74176,       TRUE,  1125, 41, 4,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080P30000_74250,       TRUE,  1125, 41, 4,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080P60000_148500,      TRUE,  1125, 41, 4,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080P59940_148352,      TRUE,  1125, 41, 4,   2200, 192, 88,  16 }
  , { STM_TIMING_MODE_1080P50000_148500,      TRUE,  1125, 41, 4,   2200, 192, 88,  16 }
};
static const uint8_t Dvo_Standards_Count = sizeof(Dvo_Standards)/sizeof(stm_dvo_awg_standard_specification_t);

static stm_dvo_awg_standard_specification_t spec;
static uint32_t AWGCode[DVO_AWG_MAX_SIZE];

static int stm_dvo_awg_get_spec_index( stm_dvo_awg_code_generation_params_t *FmwGenParams_p, uint32_t* ModeIndex )
{
    int     ErrorCode = FWGEN_NO_ERROR;
    while( (*ModeIndex < Dvo_Standards_Count) && (Dvo_Standards[*ModeIndex].TimingMode != (FmwGenParams_p->TimingMode)))
    {
        (*ModeIndex)++;
    }
    if ( *ModeIndex >= Dvo_Standards_Count)
        ErrorCode = FWGEN_ERROR_BAD_PARAMETER;
    return (ErrorCode);
}

static uint32_t stm_dvo_awg_generate_instruction(int Opcode,long int Arg,long int Mux_Sel,long int Data_En, stm_dvo_awg_code_generation_params_t *FmwGenParams_p)
{
    uint32_t Instruction = 0;
    uint32_t Mux = (Mux_Sel<<8)&0x1ff;
    uint32_t Data_Enable = (Data_En << 9)&0x2ff;
    uint32_t Opcode_Offset = 10;

    if (FmwGenParams_p->Instruction_Offset < DVO_AWG_MAX_SIZE)
    {
        switch(Opcode)
        {
            case SKIP:
            case REPEAT:
            case REPLAY:
            case JUMP:
                Mux = 0;
                Data_Enable = 0;
                Arg = (Arg << 22) >> 22;
                Arg &= (0x3ff);
                break;
            case STOP:
                Arg = 0;
                break;
            case SET:
            case RPTSET:
            case RPLSET:
            case HOLD:
            default:
                Arg = (Arg << 24) >> 24;
                Arg &= (0x0ff);
                break;
        }

        Arg = ((Arg + Mux) + Data_Enable);

        Instruction = ((Opcode)<<Opcode_Offset) | Arg;
        FmwGenParams_p->AWGParams_p[FmwGenParams_p->Instruction_Offset] = Instruction & (0x3fff);
        FmwGenParams_p->Instruction_Offset += 1;
        TRC( TRC_ID_UNCLASSIFIED, "instruction offset is %d", FmwGenParams_p->Instruction_Offset );
    }
    return (Instruction);
}

static void stm_dvo_awg_generate_code_progressive_mode ( stm_dvo_awg_sav_eav_params_t Sav_Eav, stm_dvo_awg_code_generation_params_t *FmwGenParams)
{
    uint32_t Start1 = 0, Start2 = 0;
    uint32_t End1 = 0, End2 = 0;
    uint32_t Count = 0, Count1 = 0;

    /* BLANKING 12 */
    stm_dvo_awg_generate_instruction(((spec.Trailing_Lines == 0)?SET:RPLSET), Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, ((spec.Trailing_Lines == 0)?Sav_Eav.Data_Enable:0), FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    Start2 =  FmwGenParams->Instruction_Offset;
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 2)/2), 0, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 4)/2), 0, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
    End2 = FmwGenParams->Instruction_Offset;
    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
    if (spec.Trailing_Lines == 0)
    {
        stm_dvo_awg_generate_instruction(STOP, 0x00, 0, 0, FmwGenParams);
        stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
        stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
        stm_dvo_awg_generate_instruction(JUMP, 64 + Start2, 0, Sav_Eav.Data_Enable, FmwGenParams);
        stm_dvo_awg_generate_instruction(JUMP, 64 + End2, 0, Sav_Eav.Data_Enable, FmwGenParams);
    }
    stm_dvo_awg_generate_instruction(REPLAY, (spec.Blanking_Lines - ((spec.Trailing_Lines == 0)?2:1)), 0, 0, FmwGenParams);
    /* ACTIVE 24 */
    for (Count = 0; Count < 2; Count++)
    {
        /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : STARTS */
        stm_dvo_awg_generate_instruction(((Count == 0)?SET:RPLSET), Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, ((Count == 0)?0:((FmwGenParams->sAwgCodeParams.OutputClkDealy == 0)?0:Sav_Eav.Data_Enable)), FmwGenParams);
        stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, (((Count != 0)&&(FmwGenParams->sAwgCodeParams.OutputClkDealy == 2))?0:Sav_Eav.Data_Enable), FmwGenParams);
        /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : ENDS */

        if (Count == 0)
        {
            Start1 = FmwGenParams->Instruction_Offset;
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Active_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 2)/2), 0, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 4)/2), 0, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Active_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);

            /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : STARTS */
            stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, ((FmwGenParams->sAwgCodeParams.OutputClkDealy == 0)?Sav_Eav.Data_Enable:0), FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, ((FmwGenParams->sAwgCodeParams.OutputClkDealy == 2)?0:Sav_Eav.Data_Enable), FmwGenParams);
            /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : ENDS */

            End1 = FmwGenParams->Instruction_Offset;
            stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, Sav_Eav.Data_Enable, FmwGenParams);
            stm_dvo_awg_generate_instruction(STOP, 0x00, 0, Sav_Eav.Data_Enable, FmwGenParams);
        }
        else
        {
            stm_dvo_awg_generate_instruction(JUMP, 64 + Start1, 0, Sav_Eav.Data_Enable, FmwGenParams);
            stm_dvo_awg_generate_instruction(JUMP, 64 + End1, 0, Sav_Eav.Data_Enable, FmwGenParams);
            stm_dvo_awg_generate_instruction(REPLAY, (((spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines)/2) - (((((spec.Total_Lines) - ((spec.Blanking_Lines) - (spec.Trailing_Lines))) % 2) == 0)?2:1)), 0, Sav_Eav.Data_Enable, FmwGenParams);

            stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, (((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 0)?0:Sav_Eav.Data_Enable), FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);

            stm_dvo_awg_generate_instruction(JUMP, 64 + Start1, 0, Sav_Eav.Data_Enable, FmwGenParams);
            stm_dvo_awg_generate_instruction(JUMP, 64 + End1, 0, Sav_Eav.Data_Enable, FmwGenParams);
            stm_dvo_awg_generate_instruction(REPLAY, (((spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines)/2) - 1), 0, Sav_Eav.Data_Enable, FmwGenParams);
            /* TRAILING  5 */
            for (Count1 = 0; (spec.Trailing_Lines != 0) && (Count1 < 2); Count1++)
            {
                stm_dvo_awg_generate_instruction(((Count1 == 0)?SET:RPLSET), Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, ((Count1 != 0)?0:((FmwGenParams->sAwgCodeParams.OutputClkDealy == 0)?0:Sav_Eav.Data_Enable)), FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, ((Count1 != 0)?0:((FmwGenParams->sAwgCodeParams.OutputClkDealy == 2)?Sav_Eav.Data_Enable:0)), FmwGenParams);

                stm_dvo_awg_generate_instruction(JUMP, 64 + Start2, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(JUMP, 64 + End2, 0, 0, FmwGenParams);

                stm_dvo_awg_generate_instruction(((Count1 == 0)?STOP:REPLAY), ((Count1 == 0)?0x00:(spec.Trailing_Lines - 2)), 0, 0, FmwGenParams);
            }
        }
    }
}

static void stm_dvo_awg_generate_code_interlaced_mode( stm_dvo_awg_sav_eav_params_t Sav_Eav, stm_dvo_awg_code_generation_params_t *FmwGenParams)
{
    uint32_t Start1 = 0, Start2 = 0, Start3 = 0;
    uint32_t End1 = 0, End2 = 0, End3 = 0;
    uint32_t Count = 0, Count1 = 0, Count2 = 0;

    for (Count = 0; Count<2; Count++)
    {
        if (Count == 0)
        {
           /* BLANKING 12 */
            if (FmwGenParams->sAwgCodeParams.bAllowEmbeddedSync == TRUE)
            {
                stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                Start2 = FmwGenParams->Instruction_Offset;
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                Start3 = FmwGenParams->Instruction_Offset;
                stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 2)/2), 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 8) - 4)/2), 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                End3 =   FmwGenParams->Instruction_Offset;
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                End2 =   FmwGenParams->Instruction_Offset;
                stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Blanking_Lines - 1), 0, 0, FmwGenParams);
            }
            else
            {
                stm_dvo_awg_generate_instruction(RPLSET, spec.Blanking_Level, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Blanking_Lines - 1), 0, 0, FmwGenParams);
            }
        }
        else
        {
            /* BLANKING 9 */
            if (FmwGenParams->sAwgCodeParams.bAllowEmbeddedSync == TRUE)
            {
                stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(JUMP, 64 + Start3, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(JUMP, 64 + End3, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Blanking_Lines - (MODE_IS_SD(spec.TimingMode)?0:1)), 0, 0, FmwGenParams); /* changed */
            }
            else if ((FmwGenParams->sAwgCodeParams.bEnableData == TRUE)&&(spec.Trailing_Lines == 0))
            {
                for (Count1 = 0; Count1 < 2; Count1++)
                {
                    stm_dvo_awg_generate_instruction(((Count1 != 0)?RPLSET:SET), spec.Blanking_Level, Sav_Eav.Mux_Sel, ((Count1 != 0)?0:(((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 0)?0:Sav_Eav.Data_Enable)), FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, Sav_Eav.Mux_Sel, ((Count1 != 0)?0:(((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 2)?Sav_Eav.Data_Enable:0)), FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(((Count1 == 0)?STOP:REPLAY), ((Count1 == 0)?0x00:(spec.Blanking_Lines - (MODE_IS_SD(spec.TimingMode)?0:1))), 0, 0, FmwGenParams);
                }
            }
            else
            {
                stm_dvo_awg_generate_instruction(RPLSET, spec.Blanking_Level, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Blanking_Lines - (MODE_IS_SD(spec.TimingMode)?0:1)), 0, 0, FmwGenParams); /* changed */
            }
        }
        /* ACTIVE */
        if (FmwGenParams->sAwgCodeParams.bAllowEmbeddedSync == TRUE)
        {
            stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            Start1 = FmwGenParams->Instruction_Offset;
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Active_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(JUMP, 64 + Start3, 0, 1, FmwGenParams);
            stm_dvo_awg_generate_instruction(JUMP, 64 + End3, 0, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Active_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
            stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, Sav_Eav.Data_Enable, FmwGenParams);
            if ((spec.Trailing_Lines) == 0)
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines - 2), 0, Sav_Eav.Data_Enable, FmwGenParams);
            else
                stm_dvo_awg_generate_instruction(REPLAY, (spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines - 1), 0, Sav_Eav.Data_Enable, FmwGenParams);

            /* TRAILING 4-4 */
            if ((spec.Trailing_Lines) != 0)
            {
                stm_dvo_awg_generate_instruction(RPLSET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                if (Count == 0)
                {
                    stm_dvo_awg_generate_instruction(JUMP, 64 + Start2, 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(JUMP, 64 + End2, 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(REPLAY, (spec.Trailing_Lines - 1), 0, 0, FmwGenParams);
                }
                else
                {
                    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Eav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(JUMP, 64 + Start3, 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(JUMP, 64 + End3, 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Blanking_End, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(REPLAY, (spec.Trailing_Lines - ((MODE_IS_SD(spec.TimingMode))?1:2)), 0, 0, FmwGenParams); /* changed */
                }
            }
        }
        else if (FmwGenParams->sAwgCodeParams.bEnableData == TRUE)
        {
            for (Count1 = 0; Count1 < 2; Count1++)
            {
                /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : STARTS */
                    stm_dvo_awg_generate_instruction(((Count1 == 0)?SET:RPLSET), spec.Blanking_Level, Sav_Eav.Mux_Sel, ((Count1 == 0)?0:(((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 0)?0:Sav_Eav.Data_Enable)), FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, Sav_Eav.Mux_Sel, (((FmwGenParams->sAwgCodeParams.OutputClkDealy == 2)&&(Count1 != 0))?Sav_Eav.Data_Enable:0), FmwGenParams);
                /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : ENDS */
                if (Count1 == 0)
                {
                    Start1 = FmwGenParams->Instruction_Offset;
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, Sav_Eav.Mux_Sel, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SKIP, (((spec.Blanking_Pixels + spec.Trailing_Pixels) - 4)/2), 0, 0, FmwGenParams);
                    stm_dvo_awg_generate_instruction(SKIP, ((((spec.Blanking_Pixels + spec.Trailing_Pixels) - 4)/2) - 1), 0, 0, FmwGenParams);
                    /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : STARTS */
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, ((FmwGenParams->sAwgCodeParams.OutputClkDealy == 0)?Sav_Eav.Data_Enable:0), FmwGenParams);
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, ((FmwGenParams->sAwgCodeParams.OutputClkDealy == 2)?0:Sav_Eav.Data_Enable), FmwGenParams);
                    /* LOGIC TO TAKE INTO ACCOUNT DELAY IN OUTPUT CLOCK : ENDS */
                    End1 = FmwGenParams->Instruction_Offset;
                    stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, Sav_Eav.Data_Enable, FmwGenParams);
                    stm_dvo_awg_generate_instruction(STOP, 0x00, 0, Sav_Eav.Data_Enable, FmwGenParams);
                }
                else
                {
                    stm_dvo_awg_generate_instruction(JUMP, 64 + Start1, 0, Sav_Eav.Data_Enable, FmwGenParams);
                    stm_dvo_awg_generate_instruction(JUMP, 64 + End1, 0, Sav_Eav.Data_Enable, FmwGenParams);
                    if ((spec.Trailing_Lines) == 0)
                        stm_dvo_awg_generate_instruction(REPLAY, (spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines - 3), 0, Sav_Eav.Data_Enable, FmwGenParams);
                    else
                        stm_dvo_awg_generate_instruction(REPLAY, (spec.Total_Lines - spec.Blanking_Lines - spec.Trailing_Lines - 2), 0, Sav_Eav.Data_Enable, FmwGenParams);

                   /* TRAILING  5 */
                    if ((spec.Trailing_Lines) != 0)
                    {
                        for (Count2 = 0; Count2 < 2; Count2++)
                        {
                            if (Count2 == 0)
                            {
                                stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Start, Sav_Eav.Mux_Sel, (((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 0)?0:Sav_Eav.Data_Enable), FmwGenParams);
                                if ((FmwGenParams->sAwgCodeParams.OutputClkDealy) == 2)
                                    stm_dvo_awg_generate_instruction(SET, Sav_Eav.Sav_Eav_Mid,   Sav_Eav.Mux_Sel, Sav_Eav.Data_Enable,FmwGenParams);
                                stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);
                                stm_dvo_awg_generate_instruction(STOP, 0x00, 0, 0, FmwGenParams);
                            }
                            else
                            {
                                if ((spec.Trailing_Lines - (Count+2)) > 0)
                                    stm_dvo_awg_generate_instruction(RPLSET, spec.Blanking_Level, 0, 0, FmwGenParams);
                                else
                                    stm_dvo_awg_generate_instruction(SET,    spec.Blanking_Level, 0, 0, FmwGenParams);

                                stm_dvo_awg_generate_instruction(SET, spec.Blanking_Level, 0, 0, FmwGenParams);

                                if ((Count == 0) && ((spec.Trailing_Lines -2) > 0))
                                    stm_dvo_awg_generate_instruction(REPLAY, (spec.Trailing_Lines - 2), 0, 0, FmwGenParams);
                                else
                                {
                                    if ((spec.Trailing_Lines -3) > 0)
                                    {
                                        if (MODE_IS_SD(spec.TimingMode))
                                            stm_dvo_awg_generate_instruction(REPLAY, (spec.Trailing_Lines - 2), 0, 0, FmwGenParams);         /* changed */
                                        else
                                            stm_dvo_awg_generate_instruction(REPLAY, (spec.Trailing_Lines - 3), 0, 0, FmwGenParams);
                                    }
                                    else
                                    {
                                        stm_dvo_awg_generate_instruction(STOP, 0x00, 0, 0, FmwGenParams);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Sav_Eav.Sav_Blanking_End = 0xEC;
        Sav_Eav.Eav_Blanking_End = 0xF1;
        Sav_Eav.Sav_Active_End   = 0xC7;
        Sav_Eav.Eav_Active_End   = 0xDA;
    }
}
/******************************************************************************/
/* Module public functions                                                    */
/******************************************************************************/
int stm_dvo_awg_generate_code ( const stm_display_mode_t * const pMode
                              , const uint32_t                   uFormat
                              , stm_awg_dvo_parameters_t         sAwgCodeParams
                              , uint8_t *                        pRamSize
                              , uint32_t **                      pRamCode)
{
    stm_dvo_awg_sav_eav_params_t         Sav_Eav = {0,0,0,0,0,0,0,0};
    stm_dvo_awg_code_generation_params_t FmwGenParams;

    FmwGenParams.AWGParams_p    = AWGCode;
    FmwGenParams.TimingMode     = pMode->mode_id;
    FmwGenParams.uFormat        = uFormat;
    FmwGenParams.sAwgCodeParams = sAwgCodeParams;

    if ((sAwgCodeParams.bEnableData == TRUE) && (sAwgCodeParams.bAllowEmbeddedSync == FALSE))
        Sav_Eav.Data_Enable = 1;
    else
        Sav_Eav.Data_Enable = 0;

    stm_dvo_awg_set_spec_params(&FmwGenParams);

    if (FmwGenParams.Error == FWGEN_NO_ERROR)
    {
        if (sAwgCodeParams.bAllowEmbeddedSync == TRUE)
        {
            Sav_Eav.Sav_Eav_Start    = 0xFF;
            Sav_Eav.Sav_Eav_Mid      = 0x00;
            Sav_Eav.Sav_Blanking_End = 0xAB;
            Sav_Eav.Eav_Blanking_End = 0xB6;
            Sav_Eav.Sav_Active_End   = 0x80;
            Sav_Eav.Eav_Active_End   = 0x9D;
            Sav_Eav.Mux_Sel          = 0x1;
        }
        else
        {
            Sav_Eav.Sav_Eav_Start    = spec.Blanking_Level;
            Sav_Eav.Sav_Eav_Mid      = spec.Blanking_Level;
            Sav_Eav.Sav_Blanking_End = spec.Blanking_Level;
            Sav_Eav.Eav_Blanking_End = spec.Blanking_Level;
            Sav_Eav.Sav_Active_End   = spec.Blanking_Level;
            Sav_Eav.Eav_Active_End   = spec.Blanking_Level;
            Sav_Eav.Mux_Sel          = 0x0;
        }

        FmwGenParams.Instruction_Offset = 0;

        if (pMode->mode_params.scan_type == STM_PROGRESSIVE_SCAN)
            stm_dvo_awg_generate_code_progressive_mode(Sav_Eav, &FmwGenParams);
        else
            stm_dvo_awg_generate_code_interlaced_mode (Sav_Eav, &FmwGenParams);

        *pRamCode = FmwGenParams.AWGParams_p;
        *pRamSize = FmwGenParams.Instruction_Offset;
    }

    return (FmwGenParams.Error);
}


void stm_dvo_awg_set_spec_params(stm_dvo_awg_code_generation_params_t *FmwGenParams_p)
{
  uint32_t            ModeIndex = 0;

  FmwGenParams_p->Error = stm_dvo_awg_get_spec_index(FmwGenParams_p, &ModeIndex);
  if (FmwGenParams_p->Error == FWGEN_NO_ERROR)
  {
    spec                    = Dvo_Standards[ModeIndex];
    spec.Total_Pixels       = spec.Total_Pixels    * ((OUTPUT_IS_YCBCR422_8BITS(FmwGenParams_p->uFormat))?2:1);
    spec.Blanking_Pixels    = spec.Blanking_Pixels * ((OUTPUT_IS_YCBCR422_8BITS(FmwGenParams_p->uFormat))?2:1);
    spec.Trailing_Pixels    = spec.Trailing_Pixels * ((OUTPUT_IS_YCBCR422_8BITS(FmwGenParams_p->uFormat))?2:1);
  }
}
