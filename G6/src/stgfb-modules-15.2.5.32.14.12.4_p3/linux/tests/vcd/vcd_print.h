/*******************************************************************************

File name   : vcd_print.h

Description : Value Change Dump Print Functions and Header.
This is a copy of all relevant defines, enums, structures, and functions from fvdp/mpe42/fvdp_vcd.h
which involve printing to an output stream in the vcd file format.

COPYRIGHT (C) STMicroelectronics 2013

*******************************************************************************/

#ifndef _VCD_TEMP_H
#define _VCD_TEMP_H

#include <stdint.h>

//=============================================================================
//  D E F I N E S
//=============================================================================
#ifndef NULL
#define NULL    0
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef SETBIT_VECT
#define SETBIT_VECT(bv32,pos)   (bv32)[(pos) >> 5] |= 1 << ((pos) & (0x20-1))
#define CLRBIT_VECT(bv32,pos)   (bv32)[(pos) >> 5] &= ~(1 << ((pos) & (0x20-1)))
#define CHECKBIT_VECT(bv32,pos) ((((bv32)[(pos) >> 5] >> ((pos) & (0x20-1))) & 0x1) != 0)
#endif

#ifdef FVDP_SIMULATION
#define VCD_DEFAULT_TIMEREF_NUM     VCD_TIMESCALE_NUM_1
#define VCD_DEFAULT_TIMEREF_UNIT    VCD_TIMESCALE_UNIT_us
#define VCD_DEFAULT_CLOCK_DIVISOR   1
#else
#define VCD_DEFAULT_TIMEREF_NUM     VCD_TIMESCALE_NUM_10
#define VCD_DEFAULT_TIMEREF_UNIT    VCD_TIMESCALE_UNIT_us
#define VCD_DEFAULT_CLOCK_DIVISOR   3500
#endif

#define VCD_NAME_STR_SIZE           32
#define VCD_MAX_NUM_OF_MODULES      10
#define VCD_MAX_NUM_OF_EVENTS       100
#define VCD_LOG_BUFFER_LENGTH       8192

//=============================================================================
//  E N U M E R A T I O N S
//=============================================================================
typedef enum
{
    // note: events defined in fvdp_vcd.h or within VCPU
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
    VCDLOGMODE_STOP = 0,    // stop capture
    VCDLOGMODE_AUTO,        // continuous capture, with wrap-around
    VCDLOGMODE_TRIG,        // capture on specified trigger (centered)
    VCDLOGMODE_SINGLE,      // single capture
} VcdLogMode_t;

typedef struct
{
    uint8_t     source;
    uint8_t     mode;
    uint8_t     trig;
    uint32_t    size;
} vcdlog_config_t;

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
void    VCD_PrintHeader         (VCDLog_t* vcd_log);
void    VCD_PrintLogBuffer      (VCDLog_t* vcd_log);
void    VCD_SetOutputFile       (FILE* output_file);
void    VCD_PrintModulesEvents  (VCDLog_t* vcd_log_ptr);

#endif // _VCD_TEMP_H
