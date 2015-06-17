/*******************************************************************************

File name   : fvdp_vcd.h

Description : Value Change Dump functions header and defines

COPYRIGHT (C) STMicroelectronics 2012
*******************************************************************************/
#ifndef __VALUE_CHANGE_DUMP_H
#define __VALUE_CHANGE_DUMP_H


#include "fvdp_private.h"


//=============================================================================
//  D E F I N E S
//=============================================================================

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!    NOTE:  This is the global ENABLE flag for VCD output               !!
//              When set to FALSE, all functions are defined as nothing.    !!
// !!                                                                       !!
#ifndef VCD_OUTPUT_ENABLE
#define VCD_OUTPUT_ENABLE           FALSE
#endif
// !!                                                                       !!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#ifdef FVDP_SIMULATION
// in simulation, log VCD data directly to file
#define VCD_LOG_TO_BUFFER           TRUE
#define VCD_PRINT_SUPPORT           FALSE
#define VCD_FILE_IO_SUPPORT         TRUE
#define VCD_EXTENDED_LOG_CONTROLS   TRUE
#else
// otherwise, dump VCD output directly to stdout
// TODO: log to buffer only, and upload data to user space for printing
#define VCD_LOG_TO_BUFFER           TRUE
#define VCD_PRINT_SUPPORT           TRUE
#define VCD_FILE_IO_SUPPORT         FALSE
#define VCD_EXTENDED_LOG_CONTROLS   TRUE
#endif

#if (VCD_FILE_IO_SUPPORT == TRUE)
#define VDC_PRINTF(...)     fprintf(vcd_output_file, __VA_ARGS__)
#elif (VCD_PRINT_SUPPORT == TRUE)
#define VDC_PRINTF(...)     TRC( TRC_ID_MAIN_INFO, __VA_ARGS__ )
#else
#define VDC_PRINTF(...)
#endif

#ifdef FVDP_SIMULATION
#define VCD_DEFAULT_TIMEREF_NUM     VCD_TIMESCALE_NUM_1
#define VCD_DEFAULT_TIMEREF_UNIT    VCD_TIMESCALE_UNIT_us
#define VCD_DEFAULT_CLOCK_DIVISOR   1
#else
#define VCD_DEFAULT_TIMEREF_NUM     VCD_TIMESCALE_NUM_10
#define VCD_DEFAULT_TIMEREF_UNIT    VCD_TIMESCALE_UNIT_us
#define VCD_DEFAULT_CLOCK_DIVISOR   1

#endif

#define VCD_NAME_STR_SIZE           32
#define VCD_MAX_NUM_OF_MODULES      10
#define VCD_MAX_NUM_OF_EVENTS       100
#define VCD_LOG_BUFFER_LENGTH       8192

#define VCD_HOST            0
#define VCD_VCPU            1

//=============================================================================
//  E N U M E R A T I O N S
//=============================================================================
typedef enum
{
// Note:  The enum entries below MUST be in the SAME ORDER
//        as the entries of vcd_values[] array.

    // MCC Info
    VCD_EVENT_C_Y_WR_ADDR,
    VCD_EVENT_C_RGB_UV_WR_ADDR,
    VCD_EVENT_P1_Y_RD_ADDR,
    VCD_EVENT_P1_UV_RD_ADDR,
    VCD_EVENT_P2_Y_RD_ADDR,
    VCD_EVENT_P2_UV_RD_ADDR,
    VCD_EVENT_P3_Y_RD_ADDR,
    VCD_EVENT_P3_UV_RD_ADDR,
    VCD_EVENT_D_Y_WR_ADDR,
    VCD_EVENT_D_UV_WR_ADDR,
    VCD_EVENT_MCDI_WR_ADDR,
    VCD_EVENT_MCDI_RD_ADDR,
    VCD_EVENT_DNR_WR_ADDR,
    VCD_EVENT_DNR_RD_ADDR,
    VCD_EVENT_CCS_WR_ADDR,
    VCD_EVENT_CCS_RD_ADDR,
    VCD_EVENT_OP_Y_RD_ADDR,
    VCD_EVENT_OP_RGB_UV_RD_ADDR,
    VCD_EVENT_OP_Y_WR_ADDR,
    VCD_EVENT_OP_UV_WR_ADDR,
    VCD_EVENT_AUX_RGB_UV_WR,
    VCD_EVENT_AUX_Y_WR,
    VCD_EVENT_AUX_RGB_UV_RD,
    VCD_EVENT_AUX_Y_RD,
    VCD_EVENT_PIP_RGB_UV_WR,
    VCD_EVENT_PIP_Y_WR,
    VCD_EVENT_PIP_RGB_UV_RD,
    VCD_EVENT_PIP_Y_RD,

    // Miscellaneous Info
    VCD_EVENT_DATAPATH_FE,
    VCD_EVENT_INPUT_FRAME_RATE,
    VCD_EVENT_INPUT_SCAN_TYPE,
    VCD_EVENT_OUTPUT_FRAME_RATE,
    VCD_EVENT_FREE_FRAME_ID_1,
    VCD_EVENT_FREE_FRAME_ID_2,
    VCD_EVENT_FREE_FRAME_ID_3,
    VCD_EVENT_FREE_FRAME_ID_4,
    VCD_EVENT_FREE_FRAME_ID_5,
    VCD_EVENT_FREE_FRAME_ID_6,

    VCD_EVENT_FRAME_ID_SET_VID_INP,
    VCD_EVENT_PICTURE_REPEATED,
    VCD_EVENT_FRAME_ID_GET_VID_OUTP,
    VCD_EVENT_INPUT_UPDATE_RET,
    VCD_EVENT_OUTPUT_UPDATE_RET,
    VCD_EVENT_IBD_V_TOTAL,
    VCD_EVENT_IBD_V_ACTIVE,
    VCD_EVENT_FRAME_ID_GET_VID_INP,
    VCD_EVENT_FRAME_ID_SET_VID_OUTP,

    // FBM Input Process
    VCD_EVENT_INPUT_UPDATE,
    VCD_EVENT_INPUT_FRAME_ID,
    VCD_EVENT_INPUT_FIELD_TYPE,
    VCD_EVENT_PROSCAN_FRAME_ID,
    VCD_EVENT_PREV_PROSCAN_FRAME_ID,
    VCD_EVENT_INPUT_PREV1_FRAME_ID,
    VCD_EVENT_INPUT_PREV2_FRAME_ID,
    VCD_EVENT_INPUT_PREV3_FRAME_ID,
    VCD_EVENT_HEARTBIT_COUNT,
    VCD_EVENT_TEST_VALUE,
    VCD_EVENT_QUEUE_FLUSH,
    VCD_EVENT_INP_H_ACTIVE,
    VCD_EVENT_INP_V_ACTIVE,

    // FBM Output Update
    VCD_EVENT_OUTPUT_UPDATE,
    VCD_EVENT_OUTPUT_FRAME_ID,
    VCD_EVENT_OUTPUT_FIELD_TYPE_REQ,
    VCD_EVENT_OUTPUT_SCAN_TYPE_REQ,
    VCD_EVENT_VIDEO_LATENCY,
    VCD_EVENT_OUTPUT_DISCONTINUITY,

    VCD_NUM_OF_EVENTS
} VCDEvent_t;

typedef enum
{
    VCD_VAR_TYPE_INTEGER,
    VCD_VAR_TYPE_EVENT,
    VCD_VAR_TYPE_WIRE
} VCDVarType_t;

typedef enum
{
    VCD_MODULE_DEFAULT,
    VCD_MODULE_MCC_INFO,
    VCD_MODULE_FVDP_MISC_INFO,
    VCD_MODULE_INPUT_UPDATE,
    VCD_MODULE_OUTPUT_UPDATE,
    VCD_NUM_OF_MODULES
} VCDModule_t;

typedef enum
{
    VCD_TIMESCALE_UNIT_s,
    VCD_TIMESCALE_UNIT_ms,
    VCD_TIMESCALE_UNIT_us,
    VCD_TIMESCALE_UNIT_ps,
    VCD_TIMESCALE_UNIT_fs
} VCDTimescaleUnit_t;

typedef enum
{
    VCD_TIMESCALE_NUM_1,
    VCD_TIMESCALE_NUM_10,
    VCD_TIMESCALE_NUM_100
} VCDTimescaleNum_t;

typedef enum
{
    VCD_CH_0,
    VCD_CH_1,
    VCD_CH_2,
    NUM_VCD_CH
} VCDChannel_t;

//=============================================================================
//  S T R U C T   D E F I N I T I O N S
//=============================================================================

typedef struct
{
    char            name[VCD_NAME_STR_SIZE];
    uint8_t         size;
    uint8_t         type;       // VCDVarType_t
    uint8_t         module;     // VCDModule_t
} VCDVal_t;

typedef struct
{
    uint32_t        timeref;
    uint8_t         event;      // VCDEvent_t
    uint32_t        value;
} VCDVAlEntry_t;

#ifndef PACKED
#define PACKED  __attribute__((packed))
#endif

typedef struct PACKED VCDLog_s
{
    uint8_t         num_modules;
    uint8_t         num_events;
    uint8_t         wraparound;
    uint8_t         timescale_num;

    uint8_t         timescale_unit;
    uint8_t         datapack1;
    uint8_t         datapack2;
    uint8_t         datapack3;

    uint16_t        buf_length;
    uint16_t        write_index;

    char            module_str[VCD_MAX_NUM_OF_MODULES][VCD_NAME_STR_SIZE];
    VCDVal_t        vcd_values[VCD_MAX_NUM_OF_EVENTS];

    uint32_t        initialized[(VCD_MAX_NUM_OF_EVENTS >> 5) + 1];
    uint32_t        initial_values[VCD_MAX_NUM_OF_EVENTS];
    VCDVAlEntry_t   buffer[VCD_LOG_BUFFER_LENGTH];
} VCDLog_t;

//=============================================================================
//  F U N C T I O N   P R O T O T Y P E S
//=============================================================================
#if (VCD_OUTPUT_ENABLE == TRUE)
extern VCDChannel_t vcd_channel;
extern VCDLog_t vcd_log;
#endif

//=============================================================================
//  F U N C T I O N   P R O T O T Y P E S
//=============================================================================
#if (VCD_OUTPUT_ENABLE == TRUE)
void    VCD_Init(void);
void    VCD_Term(void);
#if defined(FVDP_SIMULATION)
void    VCD_SetOutputFile(FILE* output_file);
#endif
void    VCD_SetTimeScale(VCDLog_t* vcd_log, VCDTimescaleNum_t num, VCDTimescaleUnit_t unit);
void    VCD_PrintHeader(VCDLog_t* vcd_log);
void    VCD_ValueChange(VCDChannel_t CH, VCDEvent_t event_id, uint32_t value);
void    VCD_PrintLogBuffer(VCDLog_t* vcd_log);
#else
#define VCD_Init()                  ((void)0)
#define VCD_Term()                  ((void)0)
#define VCD_SetOutputFile(A)        ((void)0)
#define VCD_SetTimeScale(A, B)      ((void)0)
#define VCD_PrintHeader()           ((void)0)
#define VCD_ValueChange(A, B, C)    ((void)0)
#define VCD_PrintLogBuffer(A)       ((void)0)
#endif

#endif // __VALUE_CHANGE_DUMP_H
