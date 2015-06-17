/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_debug.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Define to prevent recursive inclusion */
#ifndef __FVDP_DEBUG_H
#define __FVDP_DEBUG_H

//#define FVDP_ENC_DEBUG_EN

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

#include "fvdp_system.h"

#define DEBUG_REGLOG    TRUE

#define REGLOG_MAX_COLS     32
#define REGLOG_MAX_ENTRIES  16384L
#define REGLOG_MAX_SIZE     REGLOG_MAX_ENTRIES * sizeof(uint32_t)

#define REGLOG_MAX_SETUP_REGS    10

typedef enum
{
    REGLOG_SAMPLE_INPUT_UPDATE  = 0,
    REGLOG_SAMPLE_OUTPUT_UPDATE = 1,
} reglog_pos_t;

typedef struct
{
    uint32_t reg_addr;
    uint32_t reg_value;
} reglog_setup_t;

typedef struct reglog_s
{
    reglog_pos_t    pos;                            // location to perform sampling
    reglog_setup_t  setup[REGLOG_MAX_SETUP_REGS];   // acquisition setup
    uint32_t        reg_addr[REGLOG_MAX_COLS];      // array of registers to sample
    uint8_t*        data_p;                         // pointer to logged data
    uint8_t         columns;                        // number of columns
    uint16_t        row_size;                       // size of each row
    uint32_t        rows_recorded;                  // rows recorded in log
    bool            active;                         // sampling is active
 } reglog_t;

typedef struct
{
    uint8_t     source;
    uint8_t     mode;
    uint8_t     trig;
    uint32_t    size;
} vcdlog_config_t;

void FVDP_VCDLog_GetConfig(vcdlog_config_t* config_p);
void FVDP_VCDLog_SetSource(uint8_t source);
void FVDP_VCDLog_SetMode(uint8_t mode, uint8_t trig);
void FVDP_VCDLog_GetInfo(uint8_t source, void** addr_p, uint32_t* size_p);

void FVDP_RegLog_Init(void);
reglog_t* FVDP_RegLog_GetPtr(void);
bool FVDP_RegLog_Start (void);
bool FVDP_RegLog_Stop (void);
bool FVDP_RegLog_Acquire (reglog_pos_t location);

typedef enum
{
    FVDP_LogInit,
    FVDP_LogStart,
    FVDP_LogStop,
    FVDP_LogPrint,
    FVDP_InitLogPrint
}FVDP_LogControl_t;

typedef enum
{
    FVDP_SelectInput,
    FVDP_SelectOutput,
}FVDP_DebugSelect_t;


// enable FVDP api logging and writing the result to file
// #define ENABLE_FVDP_LOGGING


void FVDP_DEBUG_Input(FVDP_HandleContext_t* HandleContext_p);
void FVDP_DEBUG_Output(FVDP_HandleContext_t* HandleContext_p);
void FVDP_DEBUG_ENC(FVDP_HandleContext_t* HandleContext_p);


void FVDP_LogCollection(const char * function,...);
void FVDP_LogControl(FVDP_LogControl_t logcontrol);
void FVDP_LogMainPrint(void);
void FVDP_LogInitPrint(void);
#ifdef FVDP_ENC_DEBUG_EN
bool FVDP_DEBUG_IsEncTestOn(void);
#else
#define FVDP_DEBUG_IsEncTestOn FALSE
#endif

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __FVDP_DEBUG_H */

/* End of fvdp_debug.h */
