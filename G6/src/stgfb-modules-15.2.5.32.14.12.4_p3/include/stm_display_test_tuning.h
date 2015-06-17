/***********************************************************************
 *
 * File: include/stm_display_test_tuning.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Header file name : stm_display_test_tuning.h
 * Decription :       data structures for stmcore display test(set_tuning API)
 ************************************************************************/
#ifndef _STM_DISPLAY_TEST_TUNING_H
#define _STM_DISPLAY_TEST_TUNING_H

#include "stm_display_types.h"


/* Includes ---------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CAPABILITY_STRING_LENGTH   30
#define MAX_CRC_VALUE_COUNTER          24    /* VDP: 2; HQVDP:24; FVDP: 3 */
#define MAX_MISR_COUNTER               8    /* Main MISR type: 8 for 7108; 4 for Orly; Aux MISR type: 4 for 7108; 2 for Orly */
#define NUM_MISR_REGS                  3

typedef enum
{
    MISR_EVERY_VSYNC,
    MISR_ALTERNATE_VSYNC,
    MISR_FORTH_VSYNC,
    NUMBER_OF_VSYNC_TYPES
}misr_vsync_config_t;

typedef enum
{
    MISR_NON_COUNTER,
    MISR_COUNTER
}misr_counter_config_t;

typedef enum
{
    NO_TUNING_SERVICE,
    /* (VDP, HQVDP or FVDP) CRC */
    CRC_COLLECT,
    CRC_CAPABILITY,
    /* MISR */
    MISR_COLLECT,
    MISR_CAPABILITY,
    MISR_SET_CONTROL,
    MISR_STOP,
    /* MDTP CRC */
    MDTP_CRC_COLLECT,
    MDTP_CRC_CAPABILITY,
    MDTP_CRC_SET_CONTROL,
    MDTP_CRC_STOP,
    /* RESET STATISTICS */
    RESET_STATISTICS,
    NUMBER_OF_TUNING_SERVICES
} tuning_service_type_t;

typedef struct MisrControl_s
{
    uint8_t   VsyncVal;
    uint8_t   CounterFlag;
    int32_t   VportMaxLine;
    int32_t   VportMaxPixel;
    int32_t   VportMinLine;
    int32_t   VportMinPixel;
}MisrControl_t;

typedef struct SetTuningInputData_s
{
    uint32_t OutputId;
    uint32_t SourceId;
    uint32_t PlaneId;
    char     ServiceStr[MAX_CAPABILITY_STRING_LENGTH];
    uint8_t  MisrStoreIndex;
    MisrControl_t MisrCtrlVal;
}SetTuningInputData_t;

typedef struct Crc_s
{
    /* general */
    stm_time64_t  LastVsyncTime;
    stm_time64_t  PTS;
    uint32_t      PictureID;
    uint8_t       PictureType;
    bool          Status;
    /* crc */
    uint32_t      CrcValue[MAX_CRC_VALUE_COUNTER];
} Crc_t;

typedef struct MisrReg_s
{
    uint32_t      Reg1;
    uint32_t      Reg2;
    uint32_t      Reg3;
    uint32_t      Status;
    uint32_t      Control;
    uint32_t      LostCnt;
    bool          Valid;
}MisrReg_t;

typedef struct Misr_s
{
    /* general */
    stm_time64_t   LastVsyncTime;
    stm_time64_t   PTS;
    uint32_t       VTGEvt;
    uint8_t        MisrStoreIndex; /* 7108: one of 8 for main; one of 4 for aux */
                                   /* Orly: 4 of 4 for main; 2 of 2 for aux */
    /* misr */
    MisrReg_t      MisrValue[MAX_MISR_COUNTER];
} Misr_t;

typedef struct SetTuningOutputData_s
{
    bool Valid;
    tuning_service_type_t ServiceType;
    char ServiceStr[MAX_CAPABILITY_STRING_LENGTH];
    union
    {
        Crc_t       Crc;
        Misr_t      Misr;
    }Data;
}SetTuningOutputData_t;

#ifdef __cplusplus
}
#endif
#endif/*_STM_DISPLAY_TEST_TUNING_H */

