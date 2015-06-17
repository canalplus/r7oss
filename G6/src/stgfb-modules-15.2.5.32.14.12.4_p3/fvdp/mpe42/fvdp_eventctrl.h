/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_eventctrl.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_EVENTCFG_H
#define FVDP_EVENTCFG_H

/* Includes ----------------------------------------------------------------- */
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "../fvdp.h"

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    EVENT_ID_COMMCTRL_BLK_FE = BIT0,
    EVENT_ID_HFLITE_BLK_FE   = BIT1,
    EVENT_ID_VFLITE_BLK_FE   = BIT2,
    EVENT_ID_CCS_BLK_FE      = BIT3,
    EVENT_ID_DNR_BLK_FE      = BIT4,
    EVENT_ID_MCMADI_BLK_FE   = BIT5,
    EVENT_ID_RSVD_A_BLK_FE   = BIT6,
    EVENT_ID_RSVD_AA_BLK_FE  = BIT7,
    EVENT_ID_MCC_IP_BLK_FE   = BIT8,
    EVENT_ID_MCC_OP_BLK_FE   = BIT9,
    EVENT_ID_ALL_FE          = 0x3FF,  // OR all above
} Event_ID_FE_t;

typedef enum
{
    EVENT_ID_COMMCTRL_BLK_BE = BIT0,
    EVENT_ID_HQSCALER_BLK_BE = BIT1,
    EVENT_ID_CCTRL_BLK_BE    = BIT2,
    EVENT_ID_BDR_BLK_BE      = BIT3,
    EVENT_ID_R2DTO3D_BLK_BE  = BIT4,
    EVENT_ID_RSVD_A_BLK_BE   = BIT5,
    EVENT_ID_SPARE_BE        = BIT6,
    EVENT_ID_MCC_IP_BLK_BE   = BIT7,
    EVENT_ID_MCC_OP_BLK_BE   = BIT8,
    EVENT_ID_ALL_BE          = 0x1FF,  // OR all above
} Event_ID_BE_t;

typedef enum
{
    EVENT_ID_COMMCTRL_BLK_LITE   = BIT0,
    EVENT_ID_TNR_BLK_LITE        = BIT1,
    EVENT_ID_HFLITE_BLK_LITE     = BIT2,
    EVENT_ID_VFLITE_BLK_LITE     = BIT3,
    EVENT_ID_CCTRL_BLK_LITE      = BIT4,
    EVENT_ID_BDR_BLK_LITE        = BIT5,
    EVENT_ID_ZOOMFIFO_BLK_LITE   = BIT6,
    EVENT_ID_MCC_IP_BLK_LITE     = BIT7,
    EVENT_ID_MCC_OP_BLK_LITE     = BIT8,
    EVENT_ID_ALL_LITE            = 0x1FF,  // OR all above
} Event_ID_LITE_t;

typedef enum
{
    FRAME_RESET_VIDEO_1_IN_FE   = BIT0,
    FRAME_RESET_VIDEO_2_IN_FE   = BIT1,
    FRAME_RESET_VIDEO_3_IN_FE   = BIT2,
    FRAME_RESET_VIDEO_4_IN_FE   = BIT3,
    FRAME_RESET_VIDEO_1_OUT_FE  = BIT4,
    FRAME_RESET_VIDEO_2_OUT_FE  = BIT5,
    FRAME_RESET_VIDEO_3_OUT_FE  = BIT6,
    FRAME_RESET_VIDEO_4_OUT_FE  = BIT7,
    FRAME_RESET_HFLITE_FE       = BIT8,
    FRAME_RESET_VFLITE_FE       = BIT9,
    FRAME_RESET_CCS_FE          = BIT10,
    FRAME_RESET_DNR_FE          = BIT11,
    FRAME_RESET_MCMADI_FE       = BIT12,
    FRAME_RESET_ELAFIFO_FE      = BIT13,
    FRAME_RESET_CHLSPLIT_FE     = BIT14,
    FRAME_RESET_MCC_WRITE_FE    = BIT15,
    FRAME_RESET_MCC_READ_FE     = BIT16,
    FRAME_RESET_INPUT_IRQ_FE    = BIT17,
    FRAME_RESET_DISPLAY_IRQ_FE  = BIT18,
    FRAME_RESET_ALL_FE          = 0x7FFFF, // OR all above
} FrameReset_Type_FE_t;

typedef enum
{
    FRAME_RESET_VIDEO_1_IN_BE   = BIT0,
    FRAME_RESET_VIDEO_2_IN_BE   = BIT1,
    FRAME_RESET_VIDEO_3_IN_BE   = BIT2,
    FRAME_RESET_VIDEO_4_IN_BE   = BIT3,
    FRAME_RESET_VIDEO_1_OUT_BE  = BIT4,
    FRAME_RESET_VIDEO_2_OUT_BE  = BIT5,
    FRAME_RESET_VIDEO_3_OUT_BE  = BIT6,
    FRAME_RESET_VIDEO_4_OUT_BE  = BIT7,
    FRAME_RESET_HQSCALER_BE     = BIT8,
    FRAME_RESET_CCTRL_BE        = BIT9,
    FRAME_RESET_BDR_BE          = BIT10,
    FRAME_RESET_R2DTO3D_BE      = BIT11,
    FRAME_RESET_ELAFIFO_BE      = BIT12,
    FRAME_RESET_SPARE_FRST_POS_3D_ENGINE = BIT13,
    FRAME_RESET_MCC_WRITE_BE    = BIT14,
    FRAME_RESET_MCC_READ_BE     = BIT15,
    FRAME_RESET_INPUT_IRQ_BE    = BIT16,
    FRAME_RESET_DISPLAY_IRQ_BE  = BIT17,
    FRAME_RESET_ALL_BE          = 0x3FFFF, // OR all above
} FrameReset_Type_BE_t;

typedef enum
{
    FRAME_RESET_VIDEO_1_IN_LITE = BIT0,
    FRAME_RESET_VIDEO_2_IN_LITE = BIT1,
    FRAME_RESET_VIDEO_3_IN_LITE = BIT2,
    FRAME_RESET_VIDEO_4_IN_LITE = BIT3,
    FRAME_RESET_VIDEO_1_OUT_LITE= BIT4,
    FRAME_RESET_VIDEO_2_OUT_LITE= BIT5,
    FRAME_RESET_VIDEO_3_OUT_LITE= BIT6,
    FRAME_RESET_VIDEO_4_OUT_LITE= BIT7,
    FRAME_RESET_TNR_LITE        = BIT8,
    FRAME_RESET_HFLITE_LITE     = BIT9,
    FRAME_RESET_VFLITE_LITE     = BIT10,
    FRAME_RESET_CCTRL_LITE      = BIT11,
    FRAME_RESET_BDR_LITE        = BIT12,
    FRAME_RESET_ZOOMFIFO_LITE   = BIT13,
    FRAME_RESET_MCC_WRITE_LITE  = BIT14,
    FRAME_RESET_MCC_READ_LITE   = BIT15,
    FRAME_RESET_INPUT_IRQ_LITE  = BIT16,
    FRAME_RESET_DISPLAY_IRQ_LITE= BIT17,
    FRAME_RESET_ALL_LITE        = 0x3FFFF,  // OR all above
} FrameReset_Type_LITE_t;

typedef enum
{
    FIELD_ID_VFLITE_IN_FW_FID_FE  = BIT0,
    FIELD_ID_VFLITE_OUT_FW_FID_FE = BIT1,
    FIELD_ID_CCS_FW_FID_FE        = BIT2,
    FIELD_ID_DNR_FW_FID_FE        = BIT3,
    FIELD_ID_MCMADI_FW_FID_FE     = BIT4,
    FIELD_ID_MCC_PROG_FW_FID_FE   = BIT5,
    FIELD_ID_MCC_C_FW_FID_FE      = BIT6,
    FIELD_ID_MCC_MCMADI_FW_FID_FE = BIT7,
    FIELD_ID_MCC_DNR_FW_FID_FE    = BIT8,
    FIELD_ID_MCC_CCS_FW_FID_FE    = BIT9,
    FIELD_ID_ALL_FE = 0x3FF,
}Field_ID_FE_t;

typedef enum
{
    FIELD_ID_HQSCALER_IN_FW_FID_BE  = BIT0,
    FIELD_ID_HQSCALER_OUT_FW_FID_BE = BIT1,
    FIELD_ID_CCTRL_FW_FID_BE        = BIT2,
    FIELD_ID_R2DTO3D_FW_FID_BE      = BIT3,
    FIELD_ID_SPARE_FW_FID_BE        = BIT4,
    FIELD_ID_ALL_BE = 0x1F,
}Field_ID_BE_t;

typedef enum
{
    FIELD_ID_TNR_FW_FID_LITE        = BIT0,
    FIELD_ID_VFLITE_IN_FW_FID_LITE  = BIT1,
    FIELD_ID_VFLITE_OUT_FW_FID_LITE = BIT2,
    FIELD_ID_CCTRL_LITE_FW_FID_LITE = BIT3,
    FIELD_ID_MCC_WRITE_FW_FID_LITE  = BIT4,
    FIELD_ID_ALL_LITE = 0x1F,
}Field_ID_LITE_t;

typedef enum
{
    MODULE_CLK_HFLITE_FE         = BIT0,
    MODULE_CLK_VFLITE_FE         = BIT1,
    MODULE_CLK_CCS_FE            = BIT2,
    MODULE_CLK_DNR_FE            = BIT3,
    MODULE_CLK_MCMADI_FE         = BIT4,
    MODULE_CLK_ELAFIFO_FE        = BIT5,
    MODULE_CLK_STBUS_T3_FE       = BIT6,
    MODULE_CLK_ALL_FE            = 0x7F,
}Module_Clk_FE_t;

typedef enum
{
    MODULE_CLK_HQSCALERE_BE     = BIT0,
    MODULE_CLK_CCTRL_BE         = BIT1,
    MODULE_CLK_BDR_BE           = BIT2,
    MODULE_CLK_R2DTO3D_BE       = BIT3,
    MODULE_CLK_ELAFIFO_BE       = BIT4,
    MODULE_CLK_SPARE_BE         = BIT5,
    MODULE_CLK_STBUS_T3_BE      = BIT6,
    MODULE_CLK_ALL_BE           = 0x7F,
}Module_Clk_BE_t;

typedef enum
{
    MODULE_CLK_TNR_LITE         = BIT0,
    MODULE_CLK_HFLITE_LITE      = BIT1,
    MODULE_CLK_VFLITE_LITE      = BIT2,
    MODULE_CLK_CCTRL_LITE       = BIT3,
    MODULE_CLK_BRD_LITE         = BIT4,
    MODULE_CLK_ZOOMFIFO_LITE    = BIT5,
    MODULE_CLK_STBUS_T3_LITE    = BIT6,
    MODULE_CLK_ALL_LITE         = 0x7F,
}Module_Clk_LITE_t;

typedef enum
{
    COMMCTRL_HARD_RESET_FE     = BIT0,
    HFLITE_HARD_RESET_FE       = BIT1,
    VFLITE_HARD_RESET_FE       = BIT2,
    CCS_HARD_RESET_FE          = BIT3,
    DNR_HARD_RESET_FE          = BIT4,
    MCMADI_HARD_RESET_FE       = BIT5,
    ELAFIFO_HARD_RESET_FE      = BIT6,
    CHLSPLIT_HARD_RESET_FE     = BIT7,
    MCC_HARD_RESET_FE          = BIT8,
    HARD_RESET_ALL_FE          = 0x1FF,
}Hard_Reset_FE_t;

typedef enum
{
    COMMCTRL_HARD_RESET_BE     = BIT0,
    HQSCALER_HARD_RESET_BE     = BIT1,
    CCTRL_HARD_RESET_BE        = BIT2,
    BDR_HARD_RESET_BE          = BIT3,
    R2DTO3D_HARD_RESET_BE      = BIT4,
    ELAFIFO_HARD_RESET_BE      = BIT5,
    SPARE_HARD_RESET_BE        = BIT6,
    MCC_HARD_RESET_BE          = BIT7,
    HARD_RESET_ALL_BE          = 0xFF,
}Hard_Reset_BE_t;

typedef enum
{
    COMMCTRL_HARD_RESET_LITE    = BIT0,
    TNR_HARD_RESET_LITE         = BIT1,
    HFLITE_HARD_RESET_LITE      = BIT2,
    VFLITE_HARD_RESET_LITE      = BIT3,
    CCTRL_LITE_HARD_RESET_LITE  = BIT4,
    BDR_HARD_RESET_LITE         = BIT5,
    ZOOMFIFO_HARD_RESET_LITE    = BIT6,
    MCC_HARD_RESET_LITE         = BIT7,
    HARD_RESET_ALL_LITE         = 0xFF,
}Hard_Reset_LITE_t;

typedef enum
{
    LEVEL_SENSITIVE,
    EDGE_SENSITIVE,
} FlaglineType_t;

typedef enum
{
    INPUT_SOURCE,
    DISPLAY_SOURCE,
} EventSource_t;

typedef enum
{
    FW_FID,
    SOURCE_IN,
    DISPLAY_OUT,
} FieldSource_t;

typedef enum
{
    FLAGLINE_0 = 0,
    FLAGLINE_1,
    FLAGLINE_2,
    FLAGLINE_3,
    FLAGLINE_5,
    FLAGLINE_6,
    FLAGLINE_7,
}  FlaglineName_t;

typedef enum
{
    T_CLK,
    GND_CLK,
    SYSTEM_CLK,
} ModuleClock_t;

/* Function Prototypes ----------------------------------------------------------- */
//Init event control hard ware module
void EventCtrl_Init(FVDP_CH_t CH);

// FE
// Sync Event
// Set event source, it could be input or display, and position
FVDP_Result_t EventCtrl_SetSyncEvent_FE(Event_ID_FE_t BlockId, uint32_t Position, EventSource_t EventSource);

// Frame Reset
// enables software generation of frame reset
FVDP_Result_t EventCtrl_EnableFwFrameReset_FE(FrameReset_Type_FE_t FrameType);
// disables software generation of frame reset
FVDP_Result_t EventCtrl_DisableFwFrameReset_FE(FrameReset_Type_FE_t FrameType);
//Generates - forces firmware frame reset for given type of frame (if previously enabled)
FVDP_Result_t EventCtrl_SetFwFrameReset_FE(FrameReset_Type_FE_t FrameType);
//Sets up the position for frame reset and source of generating frame reset - Input or Display, and disables fw frame reset.
FVDP_Result_t EventCtrl_SetFrameReset_FE(FrameReset_Type_FE_t  FrameType, uint32_t FrameReset_Pos, EventSource_t EventSource);

//Flagline
// Select Flagline is Level or Edge sensitive, one common selection for all Input flaglines, and other for all Display flaglines
FVDP_Result_t EventCtrl_SetFlaglineEdge_FE(EventSource_t EventSource, FlaglineType_t);
// Sets up on of flaglines
FVDP_Result_t EventCtrl_SetFlaglineCfg_FE(FlaglineName_t Name, EventSource_t EventSource, uint32_t FlaglinePos, uint8_t FlaglineRepeatInterval);
//return requested field type
FVDP_Result_t EventCtrl_GetCurrentFieldType_FE(EventSource_t EventSource, FVDP_FieldType_t *FieldType);

// Set field ID (odd/even polarity)
FVDP_Result_t EventCtrl_SetOddSignal_FE ( Field_ID_FE_t FieldId, FVDP_FieldType_t FieldType);
// Config field ID (odd/even polarity) source
FVDP_Result_t EventCtrl_ConfigOddSignal_FE ( Field_ID_FE_t FieldId, FieldSource_t FieldSource);
//Select Module clock of BE Communication Cluster
FVDP_Result_t EventCtrl_ClockCtrl_FE (Module_Clk_FE_t ModuleClockId, ModuleClock_t ClockSource);
//Control Hard reset of BE Communication Cluster
FVDP_Result_t EventCtrl_HardReset_FE (Hard_Reset_FE_t HardResetId, bool State);

// BE
// Sync Event
// Set event source, it could be input or display, and position
FVDP_Result_t EventCtrl_SetSyncEvent_BE(Event_ID_BE_t BlockId, uint32_t Position, EventSource_t EventSource);

// Frame Reset
// enables software generation of frame reset
FVDP_Result_t EventCtrl_EnableFwFrameReset_BE(FrameReset_Type_BE_t FrameType);
// disables software generation of frame reset
FVDP_Result_t EventCtrl_DisableFwFrameReset_BE(FrameReset_Type_BE_t FrameType);
//Generates - forces firmware frame reset for given type of frame (if previously enabled)
FVDP_Result_t EventCtrl_SetFwFrameReset_BE(FrameReset_Type_BE_t FrameType);
//Sets up the position for frame reset and source of generating frame reset - Input or Display, and disables fw frame reset.
FVDP_Result_t EventCtrl_SetFrameReset_BE(FrameReset_Type_BE_t  FrameType, uint32_t FrameReset_Pos, EventSource_t EventSource);

//Flagline
// Select Flagline is Level or Edge sensitive, one common selection for all Input flaglines, and other for all Display flaglines
FVDP_Result_t EventCtrl_SetFlaglineEdge_BE(EventSource_t EventSource, FlaglineType_t);
// Sets up on of flaglines
FVDP_Result_t EventCtrl_SetFalglineCfg_BE(FlaglineName_t Name, EventSource_t EventSource, uint32_t FlaglinePos, uint8_t FlaglineRepeatInterval);
//return requested field type
FVDP_Result_t EventCtrl_GetCurrentFieldType_BE(EventSource_t EventSource, FVDP_FieldType_t *FieldType);

// Set field ID (odd/even polarity)
FVDP_Result_t EventCtrl_SetOddSignal_BE ( Field_ID_BE_t FieldId, FVDP_FieldType_t FieldType);
// Config field ID (odd/even polarity) source
FVDP_Result_t EventCtrl_ConfigOddSignal_BE ( Field_ID_BE_t FieldId, FieldSource_t FieldSource);
//Select Module clock of BE Communication Cluster
FVDP_Result_t EventCtrl_ClockCtrl_BE (Module_Clk_BE_t ModuleClockId, ModuleClock_t ClockSource);
//Control Hard reset of BE Communication Cluster
FVDP_Result_t EventCtrl_HardReset_BE (Hard_Reset_BE_t HardResetId, bool State);

// LITE
// Sync Event
// Set event source, it could be input or display, and position
FVDP_Result_t EventCtrl_SetSyncEvent_LITE(FVDP_CH_t CH, Event_ID_LITE_t BlockId, uint32_t Position, EventSource_t EventSource);

// Frame Reset
// enables software generation of frame reset
FVDP_Result_t EventCtrl_EnableFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType);
// disables software generation of frame reset
FVDP_Result_t EventCtrl_DisableFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType);
//Generates - forces firmware frame reset for given type of frame (if previously enabled)
FVDP_Result_t EventCtrl_SetFwFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t FrameType);
//Sets up the position for frame reset and source of generating frame reset - Input or Display, and disables fw frame reset.
FVDP_Result_t EventCtrl_SetFrameReset_LITE(FVDP_CH_t CH, FrameReset_Type_LITE_t  FrameType, uint32_t FrameReset_Pos, EventSource_t EventSource);

//Flagline
// Select Flagline is Level or Edge sensitive, one common selection for all Input flaglines, and other for all Display flaglines
FVDP_Result_t EventCtrl_SetFlaglineEdge_LITE(FVDP_CH_t CH, EventSource_t EventSource, FlaglineType_t);
// Sets up on of flaglines
FVDP_Result_t EventCtrl_SetFlaglineCfg_LITE(FVDP_CH_t CH, FlaglineName_t Name, EventSource_t EventSource, uint32_t FlaglinePos, uint8_t FlaglineRepeatInterval);
//return requested field type
FVDP_Result_t EventCtrl_GetCurrentFieldType_LITE(FVDP_CH_t CH, EventSource_t EventSource, FVDP_FieldType_t *FieldType);

 FVDP_Result_t EventCtrl_FieldTypeUpdate_FE ( FVDP_FieldType_t FieldType);
// Set field ID (odd/even polarity)
FVDP_Result_t EventCtrl_SetOddSignal_LITE (FVDP_CH_t CH, Field_ID_LITE_t FieldId, FVDP_FieldType_t FieldType);
// Config field ID (odd/even polarity) source
FVDP_Result_t EventCtrl_ConfigOddSignal_LITE (FVDP_CH_t CH, Field_ID_LITE_t FieldId, FieldSource_t FieldSource);
//Select Module clock of LITE Communication Cluster
FVDP_Result_t EventCtrl_ClockCtrl_LITE (FVDP_CH_t CH, Module_Clk_LITE_t ModuleClockId, ModuleClock_t ClockSource);
//Control Hard reset of LITE Communication Cluster
FVDP_Result_t EventCtrl_HardReset_LITE (FVDP_CH_t CH, Hard_Reset_LITE_t HardResetId, bool State);

#endif //FVDP_EVENTCFG_H
