/***********************************************************************
 *
 * File: fvdp_vcpu.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_VCPU_H
#define FVDP_VCPU_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_fbm.h"
#include "fvdp_vcd.h"

/* Exported Constants ------------------------------------------------------- */

/* defines -------------------------------------------------------------------*/

/*
 *  spare registers reserved for debug log capture and print controls
 */

// DEBUG_LOG_SOURCE_SELECT          : 0-host, 1-vcpu
// DEBUG_LOG_CTRL                   : 0-stop, 1-auto, 2-trig, 3-single, 4-print
// DEBUG_LOG_TRIG_SELECT            : LogTrigEvent_t / VCDEvent_t
//
// for DEBUG_LOG_MODE = 1 (auto),   capture is continuous
//     DEBUG_LOG_MODE = 2 (trig),   capture is continuous until trigger is detected,
//                                  in which case it will continue to capture until
//                                  another half-log is captured (so that trigger point
//                                  can be near the middle)
//     DEBUG_LOG_MODE = 3 (single), capture will stop at full-length of the log
//     DEBUG_LOG_MODE = 4 (print),  print the log to stderr or file, if available
//
#define DEBUG_LOG_SOURCE_SELECT             TNR_FE_BASE_ADDRESS + TNR_SW_SPARE_1
#define DEBUG_LOG_TRIG_SELECT               TNR_FE_BASE_ADDRESS + TNR_SW_SPARE_2
#define DEBUG_LOG_CTRL                      TNR_FE_BASE_ADDRESS + TNR_SW_SPARE_3
#define DEBUG_LOG_DATA_OFFSET_ADDR          TNR_FE_BASE_ADDRESS + TNR_SW_SPARE_4
#define DEBUG_LOG_DATA_SIZE                 TNR_FE_BASE_ADDRESS + TNR_SW_SPARE_5

#define DEBUG_LOG_SOURCE_SELECT_CPU_MASK    0x0F
#define DEBUG_LOG_SOURCE_SELECT_CPU_OFFSET  0
#define DEBUG_LOG_SOURCE_SELECT_CPU_WIDTH   4
#define DEBUG_LOG_SOURCE_SELECT_CH_MASK     0xF0
#define DEBUG_LOG_SOURCE_SELECT_CH_OFFSET   4
#define DEBUG_LOG_SOURCE_SELECT_CH_WIDTH    4


/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    LOGMODE_STOP = 0,
    LOGMODE_AUTO,
    LOGMODE_TRIG,
    LOGMODE_SINGLE,
    LOGMODE_PRINT
} LogMode_t;

typedef enum
{
    FBM_LOG_INPUT_CURR_IN,
    FBM_LOG_INPUT_CURR_OUT,
    FBM_LOG_INPUT_PREV,
    FBM_LOG_INPUT_FRAME_RESET,
    FBM_LOG_OUTPUT,
    FBM_LOG_OUTPUT_FRAME_RESET
} FBMLogEvents_t;

typedef enum
{
    LOG_PRINT_DISABLED,
    LOG_PRINT_ALL,
    LOG_PRINT_INPUT,   // input to the FBM
    LOG_PRINT_OUTPUT  // output of the FBM
} LogPrintSelect_t;

/* Exported Variables ------------------------------------------------------- */

extern const uint32_t vcpu_dram_data[];
extern const uint32_t vcpu_iram_code[];

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

extern FVDP_Result_t VCPU_Init          (const uint32_t* const XP70Dram_p,
                                         const uint32_t* const XP70Iram_p);
extern FVDP_Result_t VCPU_Start         (void);
extern FVDP_Result_t VCPU_Term          (void);
extern FVDP_Result_t VCPU_Reset         (void);
extern FVDP_Result_t VCPU_CheckLoading  (const uint32_t* const XP70Dram_p,
                                         const uint32_t* const XP70Iram_p,
                                         uint32_t* chksum_p);

extern bool          VCPU_Initialized   (void);
extern bool          VCPU_Started       (void);

FVDP_Result_t        VCPU_DownloadVCDLog(VCDLog_t** log_pp,
                                         uint32_t* size_p);

extern FVDP_Result_t VCPU_LogPrintControl (void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_VCPU_H */

