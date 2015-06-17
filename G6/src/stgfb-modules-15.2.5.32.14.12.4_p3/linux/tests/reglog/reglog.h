/*******************************************************************************
File name   : reglog.h

Description : register log utility

COPYRIGHT (C) STMicroelectronics 2012
*******************************************************************************/

#ifndef __REG_LOG_H__
#define __REG_LOG_H__

#include <stdint.h>

#ifndef NULL
#define NULL    0
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    1
#endif

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

#endif // __REG_LOG_H__

