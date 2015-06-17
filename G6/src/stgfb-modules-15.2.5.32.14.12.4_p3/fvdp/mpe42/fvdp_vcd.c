/*******************************************************************************

File name   : vcd.c

Description : Value Change Dump - IEEE 1364-2005 section 18.2.1

COPYRIGHT (C) STMicroelectronics 2013

*******************************************************************************/

//******************************************************************************
//  I N C L U D E    F I L E S
//******************************************************************************
#include "fvdp_vcpu.h"
#include "fvdp_vcd.h"
#if defined(FVDP_SIMULATION)
#include <time.h>
#endif

#if (VCD_OUTPUT_ENABLE == TRUE)

//******************************************************************************
//  D E F I N E S
//******************************************************************************
#define VCD_VAR_ASCII_START 33      // The identifier code is a code composed of the printable
                                    // characters, which are in the ASCII character set
                                    // from ! to ~ (decimal 33 to 126).
#define VCD_VAR_ASCII_STOP  126     // Max ASCII code before increasing number of characters

#define VCD_SELF            VCD_HOST

//******************************************************************************
//  G L O B A L S
//******************************************************************************
uint8_t vcd_manual_trig = 0;

#if (VCD_FILE_IO_SUPPORT == TRUE)
static FILE*                vcd_output_file = NULL;
#endif

static uint32_t             last_value[VCD_NUM_OF_EVENTS];

#if (VCD_LOG_TO_BUFFER == TRUE)
VCDLog_t __attribute__ ((aligned(4))) vcd_log =
{
    VCD_NUM_OF_MODULES,             // num_modules
    VCD_NUM_OF_EVENTS,              // num_events
    0,                              // wraparound
    VCD_DEFAULT_TIMEREF_NUM,        // timescale_num
    VCD_DEFAULT_TIMEREF_UNIT,       // timescale_unit
    0,                              // datapack (not used)
    0,                              // datapack (not used)
    0,                              // datapack (not used)

    VCD_LOG_BUFFER_LENGTH,          // buf_length
    0,                              // write_index

    {                               // module_str
    // Note:  The table entries below MUST be in the SAME ORDER
    //        as the entries of VCDModule_t enum.
        "DEFAULT",
        "MCC_INFO",
        "MISC_INFO",
        "INPUT_UPDATE",
        "OUTPUT_UPDATE",
    },

    {                               // vcd_values
    // Note:  The table entries below MUST be in the SAME ORDER
    //        as the entries of VCDEvent_t enum.

    //|   Event Name              |Size|        Type         |          Module             |
    //|---------------------------|----|---------------------|-----------------------------|

    // MCC Info
        {"C_Y_WR_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"C_RGB_UV_WR_ADDR",        32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P1_Y_RD_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P1_UV_RD_ADDR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P2_Y_RD_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P2_UV_RD_ADDR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P3_Y_RD_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"P3_UV_RD_ADDR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"D_Y_WR_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"D_UV_WR_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"MCDI_WR_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"MCDI_RD_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"DNR_WR_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"DNR_RD_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"CCS_WR_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"CCS_RD_ADDR",             32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"OP_Y_RD_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"OP_RGB_UV_RD_ADDR",       32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"OP_Y_WR_ADDR",            32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"OP_UV_WR_ADDR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"AUX_RGB_UV_WR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"AUX_Y_WR",                32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"AUX_RGB_UV_RD",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"AUX_Y_RD",                32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"PIP_RGB_UV_WR",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"PIP_Y_WR",                32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"PIP_RGB_UV_RD",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },
        {"PIP_Y_RD",                32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_MCC_INFO          },

    // Miscellaneous Info
        {"DATAPATH_FE",              8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"INPUT_FRAME_RATE",        16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"INPUT_SCAN_TYPE",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"OUTPUT_FRAME_RATE",       16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_1",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_2",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_3",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_4",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_5",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FREE_FRAME_ID_6",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },

        {"FRAME_ID_SET_VID_INP",    16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"PICTURE_REPEATED",         1, VCD_VAR_TYPE_EVENT,   VCD_MODULE_FVDP_MISC_INFO    },
        {"FRAME_ID_GET_VID_OUTP",   16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"INPUT_UPDATE_RET",        16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"OUTPUT_UPDATE_RET",       16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"IBD_V_TOTAL",             16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"IBD_V_ACTIVE",            16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FRAME_ID_GET_VID_INP",    16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },
        {"FRAME_ID_SET_VID_OUTP",   16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_FVDP_MISC_INFO    },

    // FBM Input Process
#ifdef FVDP_SIMULATION
        {"INPUT_UPDATE",             1, VCD_VAR_TYPE_EVENT,   VCD_MODULE_INPUT_UPDATE      },
#else
        {"INPUT_UPDATE",             1, VCD_VAR_TYPE_WIRE,    VCD_MODULE_INPUT_UPDATE      },
#endif
        {"INPUT_FRAME_ID",           8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"INPUT_FIELD_TYPE",         8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"PROSCAN_FRAME_ID",         8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"PREV_PROSCAN_FRAME_ID",    8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"INPUT_PREV1_FRAME_ID",     8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"INPUT_PREV2_FRAME_ID",     8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"INPUT_PREV3_FRAME_ID",     8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"VCD_EVENT_HEARTBIT_COUNT",32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"VCD_EVENT_TEST_VALUE",    16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"VCD_EVENT_QUEUE_FLUSH",    1, VCD_VAR_TYPE_WIRE,    VCD_MODULE_INPUT_UPDATE      },
        {"INP_H_ACTIVE",            16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },
        {"INP_V_ACTIVE",            16, VCD_VAR_TYPE_INTEGER, VCD_MODULE_INPUT_UPDATE      },

    // FBM Output Update
#ifdef FVDP_SIMULATION
        {"OUTPUT_UPDATE",            1, VCD_VAR_TYPE_EVENT,   VCD_MODULE_OUTPUT_UPDATE     },
#else
        {"OUTPUT_UPDATE",            1, VCD_VAR_TYPE_WIRE,    VCD_MODULE_OUTPUT_UPDATE     },
#endif
        {"OUTPUT_FRAME_ID",          8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_OUTPUT_UPDATE     },
        {"OUTPUT_FIELD_TYPE",        8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_OUTPUT_UPDATE     },
        {"OUTPUT_SCAN_TYPE",         8, VCD_VAR_TYPE_INTEGER, VCD_MODULE_OUTPUT_UPDATE     },
        {"VIDEO_LATENCY",           32, VCD_VAR_TYPE_INTEGER, VCD_MODULE_OUTPUT_UPDATE     },
        {"OUTPUT_DISCONTINUITY",     1, VCD_VAR_TYPE_EVENT,   VCD_MODULE_OUTPUT_UPDATE     },
    }
};

VCDChannel_t vcd_channel = VCD_CH_0;
VCDLog_t* pVcdLog = &vcd_log;
#endif

//******************************************************************************
// S T A T I C   F U N C T I O N   H E A D E R
//******************************************************************************
#if (VCD_PRINT_SUPPORT == TRUE) || (VCD_FILE_IO_SUPPORT == TRUE)
static void VCD_PrintEvent(uint32_t timeref, VCDEvent_t event, uint32_t value, uint8_t size);
static void VCD_PrintBinary(uint32_t val, uint8_t size);
#endif

//******************************************************************************
//  E X T E R N
//******************************************************************************

/*******************************************************************************
Name        : uint32_t GetTimer(void);
Description : Get the timer
Parameters  : None
Assumptions :
Limitations :
Returns     : TimerVal
*******************************************************************************/
#if (VCD_SELF == VCD_VCPU)
extern uint32_t GetTimer(void);
#else
uint32_t GetTimer(void)
{
    uint32_t TimerVal;

    #if defined(FVDP_SIMULATION)
        TimerVal = Linux_Sim_GetTime();
    #elif (RUNTIME_PLATFORM == PLATFORM_WIN32)
        TimerVal = sim_clock;
    #elif (RUNTIME_PLATFORM == PLATFORM_XP70)
        TimerVal = (uint32_t) getcc();
    #else
        TimerVal = (uint32_t) vibe_os_get_system_time();
        TimerVal /= 10 ;
    #endif


    return TimerVal;
}
#endif

//******************************************************************************
//  S O U R C E     C O D E
//******************************************************************************

//=============================================================================
//Method:       VCD_Init
//Description:  Initialize the internal variables
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
void VCD_Init(void)
{
#if (VCD_SELF == VCD_VCPU)
    // pass debug log address to host
    REG32_Write (DEBUG_LOG_DATA_OFFSET_ADDR, (uint32_t) pVcdLog);
#endif
}

//=============================================================================
//Method:       VCD_Term
//Description:  Terminates the VCD output by closing the output file descriptor
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
void VCD_Term(void)
{
}

//=============================================================================
//Method:       VCD_SetOutputFile
//Description:  Set the output file descriptor
//
//In Param:     FILE* output_file - output file descriptor (already opened)
//Out Param:
//Return:       void
//=============================================================================
#if (VCD_FILE_IO_SUPPORT == TRUE)
void VCD_SetOutputFile(FILE* output_file)
{
    if (output_file != NULL)
        vcd_output_file = output_file;
}
#endif

//=============================================================================
//Method:       VCD_SetTimeScale
//Description:  Interface to set the timescale (overwrite the default timescale)
//
//In Param:     VCDTimescaleNum_t   num  - timescale number (1 | 10 | 100)
//              VCDTimescaleUnit_t  unit - timescale unit (s | ms | us | ns | ps | fs)
//Out Param:
//Return:
//=============================================================================
void VCD_SetTimeScale(VCDLog_t* vcd_log, VCDTimescaleNum_t num, VCDTimescaleUnit_t unit)
{
    vcd_log->timescale_num = num;
    vcd_log->timescale_unit = unit;
}

#if (VCD_LOG_TO_BUFFER == TRUE)

uint8_t     log_source = 0xFF;
bool        log_init = FALSE;
bool        log_trig = FALSE;
bool        log_run  = FALSE;
uint16_t    log_write_stop;
VCDEvent_t  log_trig_event;

#if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
typedef enum
{
    VCDLOGMODE_STOP = 0,    // stop capture
    VCDLOGMODE_AUTO,        // continuous capture, with wrap-around
    VCDLOGMODE_TRIG,        // capture on specified trigger (centered)
    VCDLOGMODE_SINGLE,      // single capture
} VcdLogMode_t;

VcdLogMode_t    log_mode = VCDLOGMODE_STOP;

static void vcd_Reset(void)
{
    uint8_t i;
    for (i = 0; i < (VCD_MAX_NUM_OF_EVENTS >> 5) + 1; i++)
        pVcdLog->initialized[i] = 0;
    ZERO_MEMORY(last_value,sizeof(last_value));
    pVcdLog->buf_length = 0;
}

static void vcd_SetModeAuto(void)
{
    vcd_Reset();

    log_trig        = FALSE;
    log_run         = TRUE;
    log_write_stop  = VCD_LOG_BUFFER_LENGTH;   // for continuous wrap-around
    log_mode        = VCDLOGMODE_AUTO;

    pVcdLog->wraparound  = FALSE;
    pVcdLog->write_index = 0;
    pVcdLog->buf_length  = 0;
}

static void vcd_SetModeTrigStart(VCDEvent_t TrigEvent)
{
    vcd_Reset();

    log_trig        = TRUE;
    log_trig_event  = TrigEvent;
    log_run         = TRUE;
    log_write_stop  = VCD_LOG_BUFFER_LENGTH;   // for continuous wrap-around
    log_mode        = VCDLOGMODE_TRIG;

    pVcdLog->wraparound  = FALSE;
    pVcdLog->write_index = 0;
    pVcdLog->buf_length  = 0;
}

#define VCD_TRIG_LEFT       0
#define VCD_TRIG_RIGHT      1
#define VCD_TRIG_CENTER     2

#define VCD_TRIG_POS        VCD_TRIG_LEFT       // TODO: make user-selectable

static void vcd_SetModeTrigStop(void)
{
    log_trig        = FALSE;
    log_run         = TRUE;
#if (VCD_TRIG_POS == VCD_TRIG_LEFT)
    log_write_stop  =  pVcdLog->write_index;     // "left": wait for complete log
#elif (VCD_TRIG_POS == VCD_TRIG_RIGHT)
    log_write_stop  = (pVcdLog->write_index + 1) // "right": wait for next position
                      % VCD_LOG_BUFFER_LENGTH
#elif (VCD_TRIG_POS == VCD_TRIG_CENTER)
    log_write_stop  = (pVcdLog->write_index + VCD_LOG_BUFFER_LENGTH/2)
                      % VCD_LOG_BUFFER_LENGTH;  // "center": wait for next half log
#else
#error Invalid VCD trigger position
#endif
}

static void vcd_SetModeSingle(void)
{
    vcd_Reset();

    log_trig        = FALSE;
    log_run         = TRUE;
    log_write_stop  = pVcdLog->write_index;
    log_mode        = VCDLOGMODE_SINGLE;

    pVcdLog->buf_length  = 0;
}

static void vcd_Stop(void)
{
    log_run         = FALSE;
    log_mode        = VCDLOGMODE_STOP;
}

//=============================================================================
//Method:       VCD_LogCaptureControl
//Description:  Control log capture mode via spare register.
//
//In Param:     VCDEvent_t  event_id - The event ID
//Out Param:
//Return:       void
//=============================================================================
void VCD_LogCaptureControl(VCDEvent_t event_id)
{
    static uint8_t      PrevLogSource = 0;
    static VcdLogMode_t PrevLogMode = VCDLOGMODE_STOP;
    static VCDEvent_t   PrevTrigEvent = 0;

    if (log_init == FALSE)
    {
        vcd_Reset();
        vcd_SetModeSingle();
        log_init = TRUE;
    }

    if (    log_run  == TRUE
        &&  log_trig == TRUE
        &&  event_id == log_trig_event)
    {
        // mark log pointer position to stop capture
        vcd_SetModeTrigStop();
    }

    // check if user change the log source.  if so, reload
    // DEBUG_LOG_CTRL and DEBUG_LOG_TRIG_SELECT for user visibility.

    log_source = REG32_GetField(DEBUG_LOG_SOURCE_SELECT,
                                DEBUG_LOG_SOURCE_SELECT_CPU);
    if (log_source != PrevLogSource && log_source == VCD_SELF)
    {
        REG32_Write(DEBUG_LOG_CTRL, log_mode);
        REG32_Write(DEBUG_LOG_TRIG_SELECT, log_trig_event);
        REG32_Write(DEBUG_LOG_DATA_SIZE, pVcdLog->buf_length);
    }
    PrevLogSource = log_source;

    // the following section carries out the user-specified action in
    // DEBUG_LOG_CTRL and DEBUG_LOG_TRIG_SELECT.

    if (log_source == VCD_SELF)
    {
        if (vcd_manual_trig != 0)
        {
            REG32_Write(DEBUG_LOG_CTRL, vcd_manual_trig);
            vcd_manual_trig = 0;
        }

        vcd_channel    = REG32_GetField(DEBUG_LOG_SOURCE_SELECT, DEBUG_LOG_SOURCE_SELECT_CH);
        log_mode       = REG32_Read(DEBUG_LOG_CTRL);
        log_trig_event = REG32_Read(DEBUG_LOG_TRIG_SELECT);
    }

    if (PrevLogMode != log_mode)
    {
        if (log_source == VCD_SELF)
        {
            switch (log_mode)
            {
            case VCDLOGMODE_STOP:      vcd_Stop();                           break;
            case VCDLOGMODE_AUTO:      vcd_SetModeAuto();                    break;
            case VCDLOGMODE_TRIG:      vcd_SetModeTrigStart(log_trig_event); break;
            case VCDLOGMODE_SINGLE:    vcd_SetModeSingle();                  break;
            default:
                break;
            }

            REG32_Write(DEBUG_LOG_CTRL, log_mode);
            REG32_Write(DEBUG_LOG_DATA_SIZE, pVcdLog->buf_length);
        }

        PrevLogMode   = log_mode;
        PrevTrigEvent = log_trig_event;
    }
}
#endif // (VCD_EXTENDED_LOG_CONTROLS == TRUE)

#endif // (VCD_LOG_TO_BUFFER == TRUE)

//=============================================================================
//Method:       VCD_ValueChange
//Description:  Log a VCD Value Change event.  Call this function for each event to log
//
//In Param:     VCDChannel_t CH - VCD Channel
//              VCDEvent_t  event_id - The event ID
//              uint32_t    value    - The value of the variable for this event
//Out Param:
//Return:       void
//=============================================================================
void VCD_ValueChange(VCDChannel_t CH, VCDEvent_t event_id, uint32_t value)
{
    uint32_t timestamp = GetTimer() / VCD_DEFAULT_CLOCK_DIVISOR;

#if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
    VCD_LogCaptureControl(event_id);
#endif  // (VCD_EXTENDED_LOG_CONTROLS == TRUE)

    if(CH != vcd_channel)
        return;

#if (VCD_LOG_TO_BUFFER == TRUE)
        // don't capture value if log is not running
        if (log_run == FALSE)
            return;

        // if the entry at write_index was previously used, store the value
        // as the new initial value for that event
        if (    pVcdLog->buf_length == VCD_LOG_BUFFER_LENGTH
            &&  pVcdLog->write_index != log_write_stop )
        {
            VCDEvent_t PrevEvent = pVcdLog->buffer[pVcdLog->write_index].event;
            uint32_t   PrevVal   = pVcdLog->buffer[pVcdLog->write_index].value;

            if (CHECKBIT_VECT(pVcdLog->initialized,PrevEvent))
                pVcdLog->initial_values[PrevEvent] = PrevVal;

            pVcdLog->wraparound = TRUE;
        }
#endif

    if (    (CHECKBIT_VECT(pVcdLog->initialized,event_id))
        &&  (last_value[event_id] == value)
        &&  (pVcdLog->vcd_values[event_id].type != VCD_VAR_TYPE_EVENT) )
    {
        // there is no change in the value, so just return
        return;
    }

#if (VCD_LOG_TO_BUFFER == TRUE)
    // store the event and its value into the log
    pVcdLog->buffer[pVcdLog->write_index].timeref = timestamp;
    pVcdLog->buffer[pVcdLog->write_index].event   = event_id;
    pVcdLog->buffer[pVcdLog->write_index].value   = value;
    SETBIT_VECT(pVcdLog->initialized,event_id);

    if (++pVcdLog->write_index >= VCD_LOG_BUFFER_LENGTH)
        pVcdLog->write_index = 0;

    if (++pVcdLog->buf_length >= VCD_LOG_BUFFER_LENGTH)
        pVcdLog->buf_length = VCD_LOG_BUFFER_LENGTH;

#if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
    if (log_source == VCD_SELF)
    {
        REG32_Write(DEBUG_LOG_DATA_SIZE,pVcdLog->buf_length);
    }

    // if we reach StopPtr then stop logging activity
    if (pVcdLog->write_index == log_write_stop)
    {
        log_run = FALSE;
        log_mode = VCDLOGMODE_STOP;

        if (log_source == VCD_SELF)
            REG32_Write(DEBUG_LOG_CTRL, log_mode);
    }
#endif  // (VCD_EXTENDED_LOG_CONTROLS == TRUE)

#elif (VCD_PRINT_SUPPORT == TRUE) || (VCD_FILE_IO_SUPPORT == TRUE)

    VCD_PrintEvent(timestamp, event_id, value, pVcdLog->vcd_values[event_id].size);

#endif

    last_value[event_id] = value;
}

#if (VCD_PRINT_SUPPORT == TRUE) || (VCD_FILE_IO_SUPPORT == TRUE)
//=============================================================================
//Method:       vcd_GetIdentifierName
//Description:  Determine the VCD identifier name
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
static void vcd_GetIdentifierName (uint8_t var_id, char* name)
{
    uint8_t base = VCD_VAR_ASCII_STOP - VCD_VAR_ASCII_START + 1;
    uint8_t chr2 = var_id / base;
    uint8_t chr1 = var_id - (chr2 * base);

    if (chr2 > 0)  *(name++) = VCD_VAR_ASCII_START + chr2;
    *(name++) = VCD_VAR_ASCII_START + chr1;
    *(name++) = '\0';
}

//=============================================================================
//Method:       VCD_PrintHeader
//Description:  Print the VCD header information.  Should only be called by user
//              if VCD_LOG_TO_BUFFER is set to FALSE
//
//In Param:     vcd_log_ptr - pointer to type VCDLog_t
//Out Param:
//Return:       void
//=============================================================================
void VCD_PrintHeader(VCDLog_t* vcd_log_ptr)
{
    uint8_t      var_id;
    VCDModule_t  module = VCD_MODULE_DEFAULT;
    char         var_id_name[3];

    VDC_PRINTF("$version VCD FVDP/FBM debugger $end\n");

    VDC_PRINTF("$timescale");
    switch(vcd_log_ptr->timescale_num)
    {
        case VCD_TIMESCALE_NUM_1:
        {
            VDC_PRINTF(" 1");
            break;
        }
        case VCD_TIMESCALE_NUM_10:
        {
            VDC_PRINTF(" 10");
            break;
        }
        case VCD_TIMESCALE_NUM_100:
        {
            VDC_PRINTF(" 100");
            break;
        }
    }
    switch(vcd_log_ptr->timescale_unit)
    {
        case VCD_TIMESCALE_UNIT_s:
        {
            VDC_PRINTF(" s ");
            break;
        }
        case VCD_TIMESCALE_UNIT_ms:
        {
            VDC_PRINTF(" ms ");
            break;
        }
        case VCD_TIMESCALE_UNIT_us:
        {
            VDC_PRINTF(" us ");
            break;
        }
        case VCD_TIMESCALE_UNIT_ps:
        {
            VDC_PRINTF(" ps ");
            break;
        }
        case VCD_TIMESCALE_UNIT_fs:
        {
            VDC_PRINTF(" fs ");
            break;
        }
    }
    VDC_PRINTF("$end\n\n");

    //VDC_PRINTF("$scope module top $end\n");

    for (var_id = 0; var_id < vcd_log_ptr->num_events; var_id++)
    {

        if (vcd_log_ptr->vcd_values[var_id].module != module)
        {
            if (module != VCD_MODULE_DEFAULT)
            {
                VDC_PRINTF("$upscope $end\n");
            }
            VDC_PRINTF("$scope module %s $end\n", vcd_log_ptr->module_str[vcd_log_ptr->vcd_values[var_id].module]);
            module = vcd_log_ptr->vcd_values[var_id].module;
        }

        if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_INTEGER)
        {
            VDC_PRINTF("$var integer ");
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_EVENT)
        {
            VDC_PRINTF("$var event ");
        }
        else if (vcd_log_ptr->vcd_values[var_id].type == VCD_VAR_TYPE_WIRE)
        {
            VDC_PRINTF("$var wire ");
        }

        vcd_GetIdentifierName(var_id, var_id_name);
        VDC_PRINTF("%d %s %s $end\n", vcd_log_ptr->vcd_values[var_id].size, var_id_name, vcd_log_ptr->vcd_values[var_id].name);
    }

    if (module != VCD_MODULE_DEFAULT)
    {
        VDC_PRINTF("$upscope $end\n");
    }

    VDC_PRINTF("$enddefinitions $end\n\n");
}

//=============================================================================
//Method:       VCD_PrintLogBuffer
//Description:  Prints the events logged in the buffer in VCD format as per
//              IEEE 1364-2005 section 18.2.1
//
//In Param:     void
//Out Param:
//Return:       void
//=============================================================================
void VCD_PrintLogBuffer(VCDLog_t* vcd_log_ptr)
{
    uint16_t index, print_stop_index;
    uint32_t initial_time_ref;

    VCD_PrintHeader(vcd_log_ptr);

    if (vcd_log_ptr->buf_length == 0)
    {
        VDC_PRINTF("empty log.\n");
        return;
    }

    if (vcd_log_ptr->wraparound == TRUE)
    {
        for (index = 0; index < vcd_log_ptr->num_events; index ++)
        {
            if (CHECKBIT_VECT(vcd_log_ptr->initialized,index))
            {
                VCD_PrintEvent(0,
                               index,
                               vcd_log_ptr->initial_values[index],
                               vcd_log_ptr->vcd_values[index].size);
            }
        }
    }

    index = (vcd_log_ptr->wraparound == TRUE) ? vcd_log_ptr->write_index : 0;
    print_stop_index = (index + vcd_log_ptr->buf_length) % vcd_log_ptr->buf_length;
    initial_time_ref = vcd_log_ptr->buffer[index].timeref;

    do
    {
        VCD_PrintEvent(vcd_log_ptr->buffer[index].timeref - initial_time_ref,
                       vcd_log_ptr->buffer[index].event,
                       vcd_log_ptr->buffer[index].value,
                       vcd_log_ptr->vcd_values[vcd_log_ptr->buffer[index].event].size);

        index = (index + 1) % vcd_log_ptr->buf_length;
    } while (index != print_stop_index);
}

//=============================================================================
//Method:       VCD_PrintEvent
//Description:  static function for printing the event in VCD format as per
//              IEEE 1364-2005 section 18.2.1
//
//In Param:     uint32_t   timeref - the time reference for the event
//              VCDEvent_t event   - the event ID
//              uint32_t   value   - the value of the event
//              uint8_t    size    - size of value (in number of bits)
//Out Param:
//Return:       void
//=============================================================================
static void VCD_PrintEvent(uint32_t timeref, VCDEvent_t event, uint32_t value, uint8_t size)
{
    char var_id_name[3];

    VDC_PRINTF("#%u\n", timeref);

    vcd_GetIdentifierName(event, var_id_name);

    if (size > 1)
    {
        VCD_PrintBinary(value, size);
        VDC_PRINTF(" %s\n", var_id_name);
    }
    else
    {
        VDC_PRINTF("%u%s\n", (value & 0x1), var_id_name);
    }
}

//=============================================================================
//Method:       VCD_PrintBinary
//Description:  static function for printing in binary format (with a 'b' prefix)
//
//In Param:     uint32_t val - value that should be printed in binary format
//              uint8_t size - size of value (in number of bits)
//Out Param:
//Return:       void
//=============================================================================
static void VCD_PrintBinary(uint32_t val, uint8_t size)
{
    uint8_t i;

    VDC_PRINTF("b");
    for (i = 1; i <= size; i++)
    {
        if (((val >> (size-i)) & 1) == 1)
            VDC_PRINTF("1");
        else
            VDC_PRINTF("0");
    }

}

#endif // (VCD_PRINT_SUPPORT == TRUE) || (VCD_FILE_IO_SUPPORT == TRUE)
#endif // (VCD_OUTPUT_ENABLE == TRUE)

