/***********************************************************************
 *
 * File: fvdp_debug.c
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_regs.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_update.h"
#include "fvdp_hostupdate.h"
#include <stm_display.h>
#include <vibe_os.h>
#include "fvdp_ivp.h"
#include <stdarg.h>
#include "fvdp_debug.h"
#include "fvdp_vcd.h"
#include "fvdp_vcpu.h"
#include "fvdp_mcc.h"
#include "fvdp_color.h"
#include "fvdp_eventctrl.h"
#include "fvdp_tnr.h"
#include "fvdp_madi.h"
#include "fvdp_mcdi.h"


// VMUX register access Macros
#define VMUX_REG32_Write(Addr,Val)              REG32_Write(Addr+VMUX_BASE_ADDRESS,Val)
#define VMUX_REG32_Read(Addr)                   REG32_Read(Addr+VMUX_BASE_ADDRESS)
#define VMUX_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+VMUX_BASE_ADDRESS,BitsToSet)
#define VMUX_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+VMUX_BASE_ADDRESS,BitsToClear)
#define VMUX_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+VMUX_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

static const uint32_t ISM_BASE_ADDR[] = {ISM_MAIN_BASE_ADDRESS, ISM_PIP_BASE_ADDRESS, ISM_VCR_BASE_ADDRESS, ISM_ENC_BASE_ADDRESS};
#define ISM_REG32_Write(Ch,Addr,Val)            REG32_Write(Addr+ISM_BASE_ADDR[Ch],Val)
#define ISM_REG32_Read(Ch,Addr)                 REG32_Read(Addr+ISM_BASE_ADDR[Ch])
#define ISM_REG32_Set(Ch,Addr,BitsToSet)        REG32_Set(Addr+ISM_BASE_ADDR[Ch],BitsToSet)
#define ISM_REG32_Clear(Ch,Addr,BitsToClear)    REG32_Clear(Addr+ISM_BASE_ADDR[Ch],BitsToClear)
#define ISM_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+ISM_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
// IVP register access Macros
static const uint32_t IVP_BASE_ADDR[] = {MIVP_BASE_ADDRESS, PIVP_BASE_ADDRESS, VIVP_BASE_ADDRESS, EIVP_BASE_ADDRESS};
#define IVP_REG32_Write(Ch,Addr,Val)            REG32_Write(Addr+IVP_BASE_ADDR[Ch],Val)
#define IVP_REG32_Read(Ch,Addr)                 REG32_Read(Addr+IVP_BASE_ADDR[Ch])
#define IVP_REG32_Set(Ch,Addr,BitsToSet)        REG32_Set(Addr+IVP_BASE_ADDR[Ch],BitsToSet)
#define IVP_REG32_Clear(Ch,Addr,BitsToClear)    REG32_Clear(Addr+IVP_BASE_ADDR[Ch],BitsToClear)
#define IVP_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+IVP_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)


// COMM_CLUST_FE register access Macros
#define COMM_CLUST_FE_REG32_Write(Addr,Val)              REG32_Write(Addr+COMM_CLUST_FE_BASE_ADDRESS,Val)
#define COMM_CLUST_FE_REG32_Read(Addr)                   REG32_Read(Addr+COMM_CLUST_FE_BASE_ADDRESS)
#define COMM_CLUST_FE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToSet)
#define COMM_CLUST_FE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define DFR_FE_REG32_Write(Addr,Val)              REG32_Write(Addr+DFR_FE_BASE_ADDRESS,Val)
#define DFR_FE_REG32_Read(Addr)                   REG32_Read(Addr+DFR_FE_BASE_ADDRESS)
#define DFR_FE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+DFR_FE_BASE_ADDRESS,BitsToSet)
#define DFR_FE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+DFR_FE_BASE_ADDRESS,BitsToClear)
#define DFR_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DFR_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define FVDP_MCC_FE_REG32_Write(Addr,Val)              REG32_Write(Addr+FVDP_MCC_FE_BASE_ADDRESS,Val)
#define FVDP_MCC_FE_REG32_Read(Addr)                   REG32_Read(Addr+FVDP_MCC_FE_BASE_ADDRESS)
#define FVDP_MCC_FE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+FVDP_MCC_FE_BASE_ADDRESS,BitsToSet)
#define FVDP_MCC_FE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+FVDP_MCC_FE_BASE_ADDRESS,BitsToClear)
#define FVDP_MCC_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+FVDP_MCC_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define COMM_CLUST_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+COMM_CLUST_BE_BASE_ADDRESS,Val)
#define COMM_CLUST_BE_REG32_Read(Addr)                   REG32_Read(Addr+COMM_CLUST_BE_BASE_ADDRESS)
#define COMM_CLUST_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToSet)
#define COMM_CLUST_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define DFR_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+DFR_BE_BASE_ADDRESS,Val)
#define DFR_BE_REG32_Read(Addr)                   REG32_Read(Addr+DFR_BE_BASE_ADDRESS)
#define DFR_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+DFR_BE_BASE_ADDRESS,BitsToSet)
#define DFR_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+DFR_BE_BASE_ADDRESS,BitsToClear)
#define DFR_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DFR_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define FVDP_MCC_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+FVDP_MCC_BE_BASE_ADDRESS,Val)
#define FVDP_MCC_BE_REG32_Read(Addr)                   REG32_Read(Addr+FVDP_MCC_BE_BASE_ADDRESS)
#define FVDP_MCC_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+FVDP_MCC_BE_BASE_ADDRESS,BitsToSet)
#define FVDP_MCC_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+FVDP_MCC_BE_BASE_ADDRESS,BitsToClear)
#define FVDP_MCC_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+FVDP_MCC_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define HQS_HF_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+HQS_HF_BE_BASE_ADDRESS,Val)
#define HQS_HF_BE_REG32_Read(Addr)                   REG32_Read(Addr+HQS_HF_BE_BASE_ADDRESS)
#define HQS_HF_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToSet)
#define HQS_HF_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToClear)
#define HQS_HF_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define HQS_VF_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+HQS_VF_BE_BASE_ADDRESS,Val)
#define HQS_VF_BE_REG32_Read(Addr)                   REG32_Read(Addr+HQS_VF_BE_BASE_ADDRESS)
#define HQS_VF_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToSet)
#define HQS_VF_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToClear)
#define HQS_VF_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define CCTRL_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+CCTRL_BE_BASE_ADDRESS,Val)
#define CCTRL_BE_REG32_Read(Addr)                   REG32_Read(Addr+CCTRL_BE_BASE_ADDRESS)
#define CCTRL_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+CCTRL_BE_BASE_ADDRESS,BitsToSet)
#define CCTRL_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+CCTRL_BE_BASE_ADDRESS,BitsToClear)
#define CCTRL_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+CCTRL_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


#define MISC_FE_REG32_Write(Addr,Val)              REG32_Write(Addr+MISC_FE_BASE_ADDRESS,Val)
#define MISC_FE_REG32_Read(Addr)                   REG32_Read(Addr+MISC_FE_BASE_ADDRESS)
#define MISC_FE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+MISC_FE_BASE_ADDRESS,BitsToSet)
#define MISC_FE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+MISC_FE_BASE_ADDRESS,BitsToClear)
#define MISC_FE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                   REG32_ClearAndSet(Addr+MISC_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define MISC_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+MISC_BE_BASE_ADDRESS,Val)
#define MISC_BE_REG32_Read(Addr)                   REG32_Read(Addr+MISC_BE_BASE_ADDRESS)
#define MISC_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+MISC_BE_BASE_ADDRESS,BitsToSet)
#define MISC_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+MISC_BE_BASE_ADDRESS,BitsToClear)
#define MISC_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                   REG32_ClearAndSet(Addr+MISC_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define MISC_ENC_REG32_Write(Addr,Val)             REG32_Write(Addr+MISC_ENC_BASE_ADDRESS,Val)
#define MISC_ENC_REG32_Read(Addr)                  REG32_Read(Addr+MISC_ENC_BASE_ADDRESS)
#define MISC_ENC_REG32_Set(Addr,BitsToSet)         REG32_Set(Addr+MISC_ENC_BASE_ADDRESS,BitsToSet)
#define MISC_ENC_REG32_Clear(Addr,BitsToClear)     REG32_Clear(Addr+MISC_ENC_BASE_ADDRESS,BitsToClear)
#define MISC_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                   REG32_ClearAndSet(Addr+MISC_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define COMM_CLUST_ENC_REG32_Write(Addr,Val)              REG32_Write(Addr+COMM_CLUST_ENC_BASE_ADDRESS,Val)
#define COMM_CLUST_ENC_REG32_Read(Addr)                   REG32_Read(Addr+COMM_CLUST_ENC_BASE_ADDRESS)
#define COMM_CLUST_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_ENC_BASE_ADDRESS,BitsToSet)
#define COMM_CLUST_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+COMM_CLUST_ENC_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+COMM_CLUST_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define HFLITE_ENC_REG32_Write(Addr,Val)             REG32_Write(Addr+HFLITE_ENC_BASE_ADDRESS,Val)
#define HFLITE_ENC_REG32_Read(Addr)                   REG32_Read(Addr+HFLITE_ENC_BASE_ADDRESS)
#define HFLITE_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+HFLITE_ENC_BASE_ADDRESS,BitsToSet)
#define HFLITE_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+HFLITE_ENC_BASE_ADDRESS,BitsToClear)
#define HFLITE_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+HFLITE_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define VFLITE_ENC_REG32_Write(Addr,Val)             REG32_Write(Addr+VFLITE_ENC_BASE_ADDRESS,Val)
#define VFLITE_ENC_REG32_Read(Addr)                   REG32_Read(Addr+VFLITE_ENC_BASE_ADDRESS)
#define VFLITE_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+VFLITE_ENC_BASE_ADDRESS,BitsToSet)
#define VFLITE_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+VFLITE_ENC_BASE_ADDRESS,BitsToClear)
#define VFLITE_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+VFLITE_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define MCC_ENC_REG32_Write(Addr,Val)             REG32_Write(Addr+FVDP_MCC_ENC_BASE_ADDRESS,Val)
#define MCC_ENC_REG32_Read(Addr)                   REG32_Read(Addr+FVDP_MCC_ENC_BASE_ADDRESS)
#define MCC_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+FVDP_MCC_ENC_BASE_ADDRESS,BitsToSet)
#define MCC_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+FVDP_MCC_ENC_BASE_ADDRESS,BitsToClear)
#define MCC_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+FVDP_MCC_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define DFR_ENC_REG32_Write(Addr,Val)             REG32_Write(Addr+DFR_ENC_BASE_ADDRESS,Val)
#define DFR_ENC_REG32_Read(Addr)                   REG32_Read(Addr+DFR_ENC_BASE_ADDRESS)
#define DFR_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+DFR_ENC_BASE_ADDRESS,BitsToSet)
#define DFR_ENC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+DFR_ENC_BASE_ADDRESS,BitsToClear)
#define DFR_ENC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DFR_ENC_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)

#define DEBUG_COMMAND_TYPE          MISC_FE_SW_SPARE_1
#define INPUT_UPDATE_COUNTER        MISC_FE_SW_SPARE_8
#define DEBUG_BE_TEST_ID            MISC_BE_SW_SPARE_1
#define MCC_CROP_BE_ENABLE          MISC_BE_SW_SPARE_3
#define MCC_CROP_BE_V_START         MISC_BE_SW_SPARE_4
#define MCC_CROP_BE_V_LENGTH        MISC_BE_SW_SPARE_5
#define MCC_CROP_BE_H_START         MISC_BE_SW_SPARE_6
#define MCC_CROP_BE_H_LENGTH        MISC_BE_SW_SPARE_7
#define OUTPUT_UPDATE_COUNTER       MISC_BE_SW_SPARE_8
#define MCC_BE_CROP_TEST            1

#define CSC_BE_TEST_CASE           MISC_BE_SW_SPARE_2
#define CSC_BE_PARAMETER           MISC_BE_SW_SPARE_3
#define CSC_BE_VALUE               MISC_BE_SW_SPARE_4


#define SW_SPARE_1 MISC_BE_SW_SPARE_1
#define SW_SPARE_2 MISC_BE_SW_SPARE_2
#define SW_SPARE_3 MISC_BE_SW_SPARE_3
#define SW_SPARE_4 MISC_BE_SW_SPARE_4
#define SW_SPARE_5 MISC_BE_SW_SPARE_5
#define SW_SPARE_6 MISC_BE_SW_SPARE_6
#define SW_SPARE_7 MISC_BE_SW_SPARE_7
#define SW_SPARE_8 MISC_BE_SW_SPARE_8






/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
   V C D   L O G G I N G

   This section provides an interface for accessing VCD logging functionality
   in both the HOST and VCPU.

*******************************************************************************/

/*******************************************************************************
Name        : FVDP_VCDLog_GetConfig
Description :
Parameters  : config_p[out] - current VCDLog configuration returned
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void FVDP_VCDLog_GetConfig(vcdlog_config_t* config_p)
{
#if (VCD_OUTPUT_ENABLE == TRUE)
    config_p->source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
    config_p->trig   = REG32_Read(DEBUG_LOG_TRIG_SELECT);
    config_p->mode   = REG32_Read(DEBUG_LOG_CTRL);
    config_p->size   = REG32_Read(DEBUG_LOG_DATA_SIZE);
#else
    config_p->mode   = 0xFF;
#endif
}

/*******************************************************************************
Name        : FVDP_VCDLog_SetSource
Description :
Parameters  : source [in]: 0 - HOST, 1 - VCPU
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void FVDP_VCDLog_SetSource(uint8_t source)
{
#if (VCD_OUTPUT_ENABLE == TRUE)
    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
#endif
}

/*******************************************************************************
Name        : FVDP_VCDLog_SetMode
Description :
Parameters  : mode [in]
              trig [in]
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void FVDP_VCDLog_SetMode(uint8_t mode, uint8_t trig)
{
#if (VCD_OUTPUT_ENABLE == TRUE)
    if (mode == LOGMODE_TRIG)
        REG32_Write(DEBUG_LOG_TRIG_SELECT, trig);

    REG32_Write(DEBUG_LOG_CTRL, mode);
    if (mode != LOGMODE_STOP)
        REG32_Write(DEBUG_LOG_DATA_SIZE, 0);
#endif
}

/*******************************************************************************
Name        : FVDP_VCDLog_GetInfo
Description : returns address and size of VCD data
Parameters  : source [in]  - 0:HOST, 1:VCPU
              addr_p [out] - addr of data (NULL = failure)
              size_p [out] - size of data
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void FVDP_VCDLog_GetInfo(uint8_t source, void** addr_p, uint32_t* size_p)
{
    *addr_p = NULL;

#if (VCD_OUTPUT_ENABLE == TRUE)
    if (source == 0)
    {
        *addr_p = (void*) &vcd_log;
        *size_p = sizeof(VCDLog_t);
    }
    else if (source == 1)
    {
        FVDP_Result_t status;
        VCDLog_t *log_p;

        status = VCPU_DownloadVCDLog(&log_p, size_p);
        if (status == FVDP_OK)
            *addr_p = (void*) log_p;
    }
#endif
}


/*******************************************************************************
   D E B U G   D A T A   L O G G I N G

   This section provides generic, run-time configurable data logging for
   validation purposes.

*******************************************************************************/

#if (DEBUG_REGLOG == TRUE)
static reglog_t     reglog;
static uint32_t     reglog_max_rows = 0;
#endif

/*******************************************************************************
Name        : FVDP_RegLog_Init
Description : initialize register debug log
Parameters  : none
Assumptions : maximum fixed size allocated
Limitations : none
Returns     : void
*******************************************************************************/
void FVDP_RegLog_Init(void)
{
#if (DEBUG_REGLOG == TRUE)
#ifndef FVDP_SIMULATION
    static DMA_Area     m_RegLogData;
    vibe_os_allocate_dma_area(&m_RegLogData, REGLOG_MAX_SIZE, 32, SDAAF_UNCACHED);
    if(!m_RegLogData.pMemory)
    {
        TRC( TRC_ID_MAIN_INFO, "Failed to allocate reg log memory" );
    }
    vibe_os_zero_memory(&reglog, sizeof(reglog));
    reglog.data_p = m_RegLogData.pData;
#else

    vibe_os_zero_memory(&reglog, sizeof(reglog));
    reglog.data_p = malloc(REGLOG_MAX_SIZE);
#endif
#endif //(DEBUG_REGLOG == TRUE)
}

/*******************************************************************************
Name        : FVDP_RegLog_GetPtr
Description : return pointer to reglog
Parameters  : none
Assumptions : none
Limitations : none
Returns     : reglog_t*
*******************************************************************************/
reglog_t* FVDP_RegLog_GetPtr(void)
{
#if (DEBUG_REGLOG == TRUE)
    return &reglog;
#else
    return NULL;
#endif
}

/*******************************************************************************
Name        : FVDP_RegLog_Start
Description : start logging data
Parameters  : none
Assumptions : debug log buffer is already allocated
Limitations : none
Returns     : bool - TRUE if success, FALSE otherwise
*******************************************************************************/
bool FVDP_RegLog_Start (void)
{
#if (DEBUG_REGLOG == TRUE)
    if (!reglog.data_p)
    {
        TRC( TRC_ID_MAIN_INFO, "log uninitialized" );
        return FALSE;
    }

    if (reglog.row_size <= 0)
    {
        TRC( TRC_ID_MAIN_INFO, "no columns defined" );
        return FALSE;
    }

    reglog.active = TRUE;
    reglog_max_rows = REGLOG_MAX_SIZE / reglog.row_size;
    reglog.rows_recorded = 0;

    return TRUE;
#else
    return FALSE;
#endif
}

/*******************************************************************************
Name        : FVDP_RegLog_Stop
Description : stop logging data
Parameters  : none
Assumptions : debug log buffer is already allocated, and is actively logging
Limitations : none
Returns     : bool - TRUE if success, FALSE otherwise
*******************************************************************************/
bool FVDP_RegLog_Stop (void)
{
#if (DEBUG_REGLOG == TRUE)
    if (!reglog.active)
    {
        TRC( TRC_ID_MAIN_INFO, "not active" );
        return FALSE;
    }

    reglog.active = FALSE;

    return TRUE;
#else
    return FALSE;
#endif
}

/*******************************************************************************
Name        : FVDP_RegLog_Acquire
Description : sample register data
Parameters  : location - location to sample
Assumptions : called by ISR to sample data, assumes debug log is active, and
              columns are properly defined.
Limitations : none
Returns     : bool - TRUE if success, FALSE otherwise
*******************************************************************************/
bool FVDP_RegLog_Acquire (reglog_pos_t pos)
{
#if (DEBUG_REGLOG == TRUE)
    uint8_t  num;
    uint8_t  col;
    uint16_t row;
    uint8_t* addr;

    if (!reglog.active || reglog.pos != pos)
        return FALSE;

    // program setup regs prior to sampling
    for (num = 0; num < REGLOG_MAX_SETUP_REGS; num ++)
    {
        if (reglog.setup[num].reg_addr)
        {
            REG32_Write(reglog.setup[num].reg_addr,
                        reglog.setup[num].reg_value);
        }
    }

    row = reglog.rows_recorded;
    addr = reglog.data_p + ((uint32_t) row * reglog.row_size);

    // sample data by reading specified registers
    for (col = 0; col < REGLOG_MAX_COLS; col ++)
    {
        if (reglog.reg_addr[col])
        {
            *((uint32_t*) addr) = REG32_Read(reglog.reg_addr[col]);
            addr += sizeof(uint32_t);
        }
    }

    reglog.rows_recorded ++;
    if (reglog.rows_recorded >= reglog_max_rows)
        reglog.active = FALSE;
#endif // (DEBUG_REGLOG == TRUE)

    return TRUE;
}

typedef enum
{
    DEBUG_COMMAND_NO_COMMAND,
    DEBUG_COMMAND_PRINT_DFR_FE,
    DEBUG_COMMAND_PRINT_DFR_BE,
    DEBUG_COMMAND_PRINT_MCC,
    DEBUG_COMMAND_MCC_CROPPING,
    DEBUG_COMMAND_TEST_CSC_BE,
    DEBUG_COMMAND_TEST_MCMADI_FE,
    DEBUG_COMMAND_TEST_ENC_FREESE       = 7,
    DEBUG_COMMAND_TEST_ENC_MEMTOMEM     = 8,
    DEBUG_COMMAND_TEST_ENC_INIT         = 9,
    DEBUG_COMMAND_TEST_ENC_QUEUEBUFF    = 10,
    ENC_DBG_OFF                         = 11,
    ENC_DBG_PRINT_MCC_CONF              = 12,
    ENC_DBG_PRINT_MEMBUF_ADDR           = 13,
    ENC_DBG_BORDER                      = 18,
    ENC_DBG_CROP                        = 20,
    ENC_PRINT_CCTRL                     = 21,
    DEBUG_NUM_OF_COMMANDS
} FVDP_DEBUG_InputCommand_t;


/* Global Variables---------------------------------------------------------- */
#ifdef FVDP_ENC_DEBUG_EN
static FVDP_VideoInfo_t    DebugInputVideoInfo;
static FVDP_VideoInfo_t    DebugOutputVideoInfo;
static FVDP_CH_Handle_t    EncHandle;
#endif

static void FVDP_DEBUG_TestCsc_BE(void);
static void FVDP_DEBUG_McMadi_FE(FVDP_HandleContext_t* HandleContext_p);
#ifdef FVDP_ENC_DEBUG_EN
static void FVDP_DEBUG_EncTest(FVDP_HandleContext_t* HandleContext_p, uint32_t test_id);
static void FVDP_DEBUG_EncInit(FVDP_HandleContext_t* HandleContext_p);
static void FVDP_DEBUG_EncQueueBuffer(FVDP_HandleContext_t* HandleContext_p);
static void Debug_ScalingCompleted_test(void* UserData, bool Status);
static void Debug_BufferDone_test(void* UserData);
#endif

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_DEBUG_Input(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_DEBUG_InputCommand_t InputDebugCommand;
    uint32_t                  InputUpdateCounter;

    InputUpdateCounter = MISC_FE_REG32_Read(INPUT_UPDATE_COUNTER);
    InputUpdateCounter++;
    MISC_FE_REG32_Write(INPUT_UPDATE_COUNTER, InputUpdateCounter);

    // Check if there is a debug command to execute
    InputDebugCommand = MISC_FE_REG32_Read(DEBUG_COMMAND_TYPE);
    if (InputDebugCommand != DEBUG_COMMAND_NO_COMMAND)
    {
        if (InputDebugCommand == DEBUG_COMMAND_PRINT_DFR_FE)
        {
            DFR_PrintConnections(DFR_FE);
        }
        else if (InputDebugCommand == DEBUG_COMMAND_PRINT_DFR_BE)
        {
            DFR_PrintConnections(DFR_BE);
        }
        else if (InputDebugCommand == DEBUG_COMMAND_PRINT_MCC)
        {
            MCC_PrintConfiguration(FVDP_MAIN);
        }
        else if (InputDebugCommand == DEBUG_COMMAND_TEST_MCMADI_FE)
        {
            FVDP_DEBUG_McMadi_FE(HandleContext_p);
        }


        // Reset the Debug Command
        MISC_FE_REG32_Write(DEBUG_COMMAND_TYPE, 0);
    }

}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_DEBUG_Output(FVDP_HandleContext_t* HandleContext_p)
{
    uint32_t OutputUpdateCounter;
    uint32_t test_id = MISC_BE_REG32_Read(DEBUG_BE_TEST_ID);

    OutputUpdateCounter = MISC_BE_REG32_Read(OUTPUT_UPDATE_COUNTER);
    OutputUpdateCounter++;
    MISC_BE_REG32_Write(OUTPUT_UPDATE_COUNTER, OutputUpdateCounter);
    if(test_id)
        TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_Output: %d ", test_id );
    else
        return;

    switch(test_id)
    {
        case MCC_BE_CROP_TEST:

            break;
        case DEBUG_COMMAND_TEST_CSC_BE:
            FVDP_DEBUG_TestCsc_BE();
            TRC( TRC_ID_MAIN_INFO, "--> FVDP_DEBUG_TestCsc_BE" );
            break;
#ifdef FVDP_ENC_DEBUG_EN
        case DEBUG_COMMAND_TEST_ENC_FREESE:
        case DEBUG_COMMAND_TEST_ENC_MEMTOMEM:
            TRC( TRC_ID_MAIN_INFO, "--> FVDP_DEBUG_EncTest" );
            FVDP_DEBUG_EncTest(HandleContext_p,test_id);
            break;
        case DEBUG_COMMAND_TEST_ENC_INIT:
            TRC( TRC_ID_MAIN_INFO, "--> FVDP_DEBUG_EncTest" );
            FVDP_DEBUG_EncInit(HandleContext_p);
            break;
        case DEBUG_COMMAND_TEST_ENC_QUEUEBUFF:
            TRC( TRC_ID_MAIN_INFO, "--> FVDP_DEBUG_EncTest" );
            FVDP_DEBUG_EncQueueBuffer(HandleContext_p);
            break;
#endif
        default:
            if(test_id)
                TRC( TRC_ID_MAIN_INFO, "--> unknown ID" );
        break;
    }
    // Reset the Debug Command
    MISC_BE_REG32_Write(DEBUG_BE_TEST_ID, 0);
}
/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_DEBUG_ENC(FVDP_HandleContext_t* HandleContext_p)
{
    uint32_t command_id = MISC_ENC_REG32_Read(SW_SPARE_1);
    static uint32_t current_command;
    FVDP_Frame_t* CurrentInputFrame_p;
    static bool print_mcc_adr;
    static uint32_t cnt;
    MISC_ENC_REG32_Write(SW_SPARE_1,0);

    if(!HandleContext_p)
    {
        return;
    }
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);

    if(command_id)
        TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_ENC: command %d ", command_id );
    switch(command_id)
    {
        case DEBUG_COMMAND_NO_COMMAND:
        break;
        case ENC_PRINT_CCTRL:
            {
                FVDP_PSIControlLevel_t * cctrl = &HandleContext_p->PSIControlLevel;

                TRC( TRC_ID_MAIN_INFO,
                    "SharpnessLevel  %u\n" \
                    "BrightnessLevel %u\n" \
                    "ContrastLevel   %u\n" \
                    "SaturationLevel %u\n" \
                    "HueLevel        %u\n" \
                    "RedGain         %u\n" \
                    "GreenGain       %u\n" \
                    "BlueGain        %u\n" \
                    "RedOffset       %u\n" \
                    "GreenOffset     %u\n" \
                    "BlueOffset      %u\n" \
                    "Depth_3D        %u\n",
                    cctrl->SharpnessLevel,
                    cctrl->BrightnessLevel,
                    cctrl->ContrastLevel,
                    cctrl->SaturationLevel,
                    cctrl->HueLevel,
                    cctrl->RedGain,
                    cctrl->GreenGain,
                    cctrl->BlueGain,
                    cctrl->RedOffset,
                    cctrl->GreenOffset,
                    cctrl->BlueOffset,
                    cctrl->Depth_3D );
                TRC( TRC_ID_MAIN_INFO,
                    "Input  Color space    %u\n" \
                    "Input  Color sampling %u\n" \
                    "Output Color space    %u\n" \
                    "Output Color sampling %u\n",
                    CurrentInputFrame_p->InputVideoInfo.ColorSpace,
                    CurrentInputFrame_p->InputVideoInfo.ColorSampling,
                    CurrentInputFrame_p->OutputVideoInfo.ColorSpace,
                    CurrentInputFrame_p->OutputVideoInfo.ColorSampling );
            }

            break;
        case ENC_DBG_BORDER:
            {
                TRC( TRC_ID_MAIN_INFO, "-> ENC_DBG_BORDER" );

                CurrentInputFrame_p->OutputActiveWindow.HStart = MISC_ENC_REG32_Read(SW_SPARE_2);
                CurrentInputFrame_p->OutputActiveWindow.HWidth = MISC_ENC_REG32_Read(SW_SPARE_3);
                CurrentInputFrame_p->OutputActiveWindow.VStart = MISC_ENC_REG32_Read(SW_SPARE_4);
                CurrentInputFrame_p->OutputActiveWindow.VHeight= MISC_ENC_REG32_Read(SW_SPARE_5);
                current_command = ENC_DBG_BORDER;

            }
            return;
        case ENC_DBG_CROP:
            {
                TRC( TRC_ID_MAIN_INFO, "-> ENC_DBG_CROP" );

                CurrentInputFrame_p->CropWindow.HCropStart  = MISC_ENC_REG32_Read(SW_SPARE_2);
                CurrentInputFrame_p->CropWindow.HCropWidth  = MISC_ENC_REG32_Read(SW_SPARE_3);
                CurrentInputFrame_p->CropWindow.VCropStart  = MISC_ENC_REG32_Read(SW_SPARE_4);
                CurrentInputFrame_p->CropWindow.VCropHeight = MISC_ENC_REG32_Read(SW_SPARE_5);
                current_command = ENC_DBG_CROP;
                break;

            }

        case ENC_DBG_PRINT_MCC_CONF:
        {
            FVDP_Result_t err = MCC_PrintConfiguration(FVDP_ENC);
            TRC( TRC_ID_MAIN_INFO, "-> MCC_PrintConfiguration - %s", (err == FVDP_OK) ? "OK" : "ERR" );
            break;
        }
        case ENC_DBG_PRINT_MEMBUF_ADDR:
            print_mcc_adr = TRUE;
            cnt = 12;
            TRC( TRC_ID_MAIN_INFO, "-> ENC_DBG_PRINT_MEMBUF_ADDR, %u cycles", cnt );
            break;
        case ENC_DBG_OFF:
            if(current_command == ENC_DBG_BORDER)
            {
                current_command = DEBUG_COMMAND_NO_COMMAND;
                CurrentInputFrame_p->OutputActiveWindow.HStart = 0;
                CurrentInputFrame_p->OutputActiveWindow.HWidth = 0;
                CurrentInputFrame_p->OutputActiveWindow.VStart = 0;
                CurrentInputFrame_p->OutputActiveWindow.VHeight= 0;
            }
            if(current_command == ENC_DBG_CROP)
            {
                current_command = DEBUG_COMMAND_NO_COMMAND;
                CurrentInputFrame_p->CropWindow.HCropStart  = 0;
                CurrentInputFrame_p->CropWindow.HCropWidth  = 0;
                CurrentInputFrame_p->CropWindow.VCropStart  = 0;
                CurrentInputFrame_p->CropWindow.VCropHeight = 0;
            }
            TRC( TRC_ID_MAIN_INFO, "-> debug OFF" );
        break;

        default:
            TRC( TRC_ID_MAIN_INFO, "?? Sorry - Unknown command" );
        break;
    }

    if(current_command == ENC_DBG_BORDER)
    {
        CurrentInputFrame_p->OutputActiveWindow.HStart = MISC_ENC_REG32_Read(SW_SPARE_2);
        CurrentInputFrame_p->OutputActiveWindow.HWidth = MISC_ENC_REG32_Read(SW_SPARE_3);
        CurrentInputFrame_p->OutputActiveWindow.VStart = MISC_ENC_REG32_Read(SW_SPARE_4);
        CurrentInputFrame_p->OutputActiveWindow.VHeight= MISC_ENC_REG32_Read(SW_SPARE_5);
    }
    if(current_command == ENC_DBG_CROP)
    {
        CurrentInputFrame_p->CropWindow.HCropStart  = MISC_ENC_REG32_Read(SW_SPARE_2);
        CurrentInputFrame_p->CropWindow.HCropWidth  = MISC_ENC_REG32_Read(SW_SPARE_3);
        CurrentInputFrame_p->CropWindow.VCropStart  = MISC_ENC_REG32_Read(SW_SPARE_4);
        CurrentInputFrame_p->CropWindow.VCropHeight = MISC_ENC_REG32_Read(SW_SPARE_5);
    }

    if(print_mcc_adr)
    {
        if(cnt > 0)
        {

            TRC( TRC_ID_MAIN_INFO, "./frame -a 0x%x 0x%x -d %u %u -b 8 -c -f out_frame%u",  HandleContext_p->QueueBufferInfo.DstAddr1, HandleContext_p->QueueBufferInfo.DstAddr2, CurrentInputFrame_p->OutputVideoInfo.Width, CurrentInputFrame_p->OutputVideoInfo.Height, cnt  );
            cnt--;
        }
        else if(!cnt)
        {
            print_mcc_adr = FALSE;
            TRC( TRC_ID_MAIN_INFO, "done" );
        }
    }
}
/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
#ifdef FVDP_ENC_DEBUG_EN
bool FVDP_DEBUG_IsEncTestOn(void)
{
    return  (MISC_ENC_REG32_Read(SW_SPARE_1) != 0);
}

static void Debug_ScalingCompleted_test(void* UserData, bool Status)
{
    TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_EncTest: Debug_ScalingCompleted_test %d %d", (uint32_t)UserData, Status );
}

static void Debug_BufferDone_test(void* UserData)
{
    TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_EncTest: Debug_BufferDone_test %d", (uint32_t)UserData );
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void FVDP_DEBUG_EncInit(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t err;
    uint32_t MemSize;
    FVDP_Context_t Context = {FVDP_SPATIAL, {1920, 1080}};
    FVDP_HandleContext_t * EncHandleContext_p;
    if(!FVDP_DEBUG_IsEncTestOn())
        return;

    //      a.  FVDP_OpenChannel()
    err = FVDP_GetRequiredVideoMemorySize(FVDP_ENC, Context, &MemSize);
    TRC( TRC_ID_MAIN_INFO, "FVDP_GetRequiredVideoMemorySize returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    err = FVDP_OpenChannel(&EncHandle, FVDP_ENC, HandleContext_p->MemBaseAddress, MemSize,Context);
    TRC( TRC_ID_MAIN_INFO, "FVDP_OpenChannel returns %s", (err == FVDP_OK)? "OK" : "ERROR" );
    TRC( TRC_ID_MAIN_INFO, "\tmembase = 0x%X; memsize %d", HandleContext_p->MemBaseAddress, MemSize );
    // /*

    //b.    FVDP_ConnectInput(FVDP_MEM_INPUT_DUAL_BUFF)
    err = FVDP_ConnectInput(EncHandle, FVDP_MEM_INPUT_DUAL_BUFF);
    TRC( TRC_ID_MAIN_INFO, "FVDP_ConnectInput returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    //c.    FVDP_SetInputVideoInfo
    err = FVDP_SetInputVideoInfo(EncHandle, DebugInputVideoInfo);
    TRC( TRC_ID_MAIN_INFO, "FVDP_SetInputVideoInfo returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    //d.    FVDP_SetOutputVideoInfo
    err = FVDP_SetOutputVideoInfo(EncHandle, DebugOutputVideoInfo);
    TRC( TRC_ID_MAIN_INFO, "FVDP_SetOutputVideoInfo returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    //e.    FVDP_SetCropWindow  (optional)

    //f.    FVDP_SetBufferCallback
    err = FVDP_SetBufferCallback(EncHandle, Debug_ScalingCompleted_test, Debug_BufferDone_test);
    TRC( TRC_ID_MAIN_INFO, "FVDP_SetBufferCallback returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    EncHandleContext_p = SYSTEM_GetHandleContext(EncHandle);
    EncHandleContext_p->QueueBufferInfo.TimerSemaphore = HandleContext_p->QueueBufferInfo.TimerSemaphore;
    EncHandleContext_p->QueueBufferInfo.TimerThreadDescr= HandleContext_p->QueueBufferInfo.TimerThreadDescr;
    HandleContext_p->QueueBufferInfo.ScalingCompleted = EncHandleContext_p->QueueBufferInfo.ScalingCompleted;
    HandleContext_p->QueueBufferInfo.BufferDone= EncHandleContext_p->QueueBufferInfo.BufferDone;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void FVDP_DEBUG_EncQueueBuffer(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t err;
    uint32_t dst_y;
    uint32_t  dst_uv_offset;
    uint32_t src_y;
    uint32_t  src_uv_offset;
    //g.  FVDP_QueueBuffer(FVDP_CH_Handle_t  FvdpChHandle,
        if(!FVDP_DEBUG_IsEncTestOn())
            return;

    DebugOutputVideoInfo.Width = MISC_ENC_REG32_Read(SW_SPARE_2);
    DebugOutputVideoInfo.Height= MISC_ENC_REG32_Read(SW_SPARE_3);

    err = FVDP_SetOutputVideoInfo(EncHandle, DebugOutputVideoInfo);
    TRC( TRC_ID_MAIN_INFO, "FVDP_SetOutputVideoInfo returns %s", (err == FVDP_OK)? "OK" : "ERROR" );

    src_y = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_Y_G].BufferPoolArray[0].Addr;
    src_uv_offset = (uint32_t)(HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_U_UV_B].BufferPoolArray[0].Addr - src_y);
    dst_y = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_Y_G].BufferPoolArray[1].Addr;
    dst_uv_offset = (uint32_t)(HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_U_UV_B].BufferPoolArray[1].Addr - dst_y);

    TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_EncQueueBuffer 0x%08X, 0x%08X, 0x%08X, 0x%08X", src_y, src_uv_offset, dst_y, dst_uv_offset );
    err = FVDP_QueueBuffer(EncHandle, 0xABC, src_y, src_uv_offset, dst_y, dst_uv_offset);
    TRC( TRC_ID_MAIN_INFO, "FVDP_QueueBuffer returns %s", (err == FVDP_OK)? "OK" : "ERROR" );
    TRC( TRC_ID_MAIN_INFO, "\n./frame -a 0x%X 0x%X -d %d %d -b 10 -f dst", dst_y, dst_y + dst_uv_offset, DebugOutputVideoInfo.Width, DebugOutputVideoInfo.Height );
    TRC( TRC_ID_MAIN_INFO, "./frame -a 0x%X 0x%X -d %d %d -b 10 -f src\n", src_y, src_y + src_uv_offset, DebugInputVideoInfo.Width, DebugInputVideoInfo.Height );
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void FVDP_DEBUG_EncTest(FVDP_HandleContext_t* HandleContext_p, uint32_t testenc)
{
#ifdef FVDP_ENC_DEBUG_EN
    uint32_t dst_addr_y;
    uint32_t dst_addr_uv;
    uint32_t src_addr_y;
    uint32_t src_addr_uv;
    FVDP_Frame_t* CurrentOutputFrame_p;
    MCC_Config_t* MccConfig_p;

//    uint32_t testenc = MISC_ENC_REG32_Read(SW_SPARE_1);
    if(HandleContext_p->CH != FVDP_MAIN)
        return;



    //1. Stop MAIN channel FVDP_FE PRO_WR MCC client.  Main is no longer writing to PRO video buffers.
    //In SYSTEM_FE_ConfigureMCC()
    if(testenc == DEBUG_COMMAND_TEST_ENC_FREESE)
    {
        MISC_ENC_REG32_Write(SW_SPARE_1,FVDP_DEBUG_IsEncTestOn() ? 0 : 1);/* enc test is on, stop writing to PRO memory client*/
        TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_EncTest: FREESE; mode %d", MISC_ENC_REG32_Read(SW_SPARE_1) );


        //2.    Set FVDP_BE OP_RD MCC client to read from buffer 'x'
        //SYSTEM_BE_ConfigureMCC
        dst_addr_y = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_Y_G].BufferPoolArray[1].Addr;
        dst_addr_uv = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_U_UV_B].BufferPoolArray[1].Addr;

        CurrentOutputFrame_p = SYSTEM_GetCurrentOutputFrame(HandleContext_p);
        DebugOutputVideoInfo = CurrentOutputFrame_p->OutputVideoInfo;
        DebugInputVideoInfo = CurrentOutputFrame_p->InputVideoInfo;
        if (1)//CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G] != NULL)
        {
            // Configure OP_Y_RD MCC Client and set the base address
            TRC( TRC_ID_MAIN_INFO, "MCC_Config; OP_Y_RD" );
            MccConfig_p = &HandleContext_p->FrameInfo.CurrentMccConfig[BUFFER_PRO_Y_G];//&(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->MccConfig);
            MccConfig_p->crop_enable = FALSE;

            MCC_Config(OP_Y_RD, *MccConfig_p);
            MCC_SetBaseAddr(OP_Y_RD, dst_addr_y, *MccConfig_p);
            MCC_SetOddSignal(OP_Y_RD, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(OP_Y_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(OP_Y_RD);
        }

        // PRO_UV_WR
        // Check if MCC client should be allocated -- it has a buffer associated with it.
        if (1)//CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B] != NULL)
        {
            // Configure PRO_UV_WR MCC Client and set the base address
            TRC( TRC_ID_MAIN_INFO, "MCC_Config; OP_Y_RD" );
            MccConfig_p = &HandleContext_p->FrameInfo.CurrentMccConfig[BUFFER_PRO_U_UV_B];//&(CurrentOutputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->MccConfig);
            MccConfig_p->crop_enable = FALSE;

            MCC_Config(OP_RGB_UV_RD, *MccConfig_p);
            MCC_SetBaseAddr(OP_RGB_UV_RD, dst_addr_uv, *MccConfig_p);
            MCC_SetOddSignal(OP_RGB_UV_RD, (CurrentOutputFrame_p->InputVideoInfo.FieldType == TOP_FIELD));
            MCC_Enable(OP_RGB_UV_RD);
        }
        else
        {
            // Disable the MCC client if there is no buffer for it.
            MCC_Disable(OP_RGB_UV_RD);
        }

        REG32_Write(0x4040c,0x1800000);//COMMCTRL_BE_UPDATE_CTRL
    }
    else if(FVDP_DEBUG_IsEncTestOn())
    {
        dst_addr_y = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_Y_G].BufferPoolArray[1].Addr;
        dst_addr_uv = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_U_UV_B].BufferPoolArray[1].Addr;
        src_addr_y = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_Y_G].BufferPoolArray[0].Addr;
        src_addr_uv = HandleContext_p->FrameInfo.BufferPool[BUFFER_PRO_U_UV_B].BufferPoolArray[0].Addr;
        DebugOutputVideoInfo.Width = MISC_ENC_REG32_Read(SW_SPARE_2);
        DebugOutputVideoInfo.Height= MISC_ENC_REG32_Read(SW_SPARE_3);
        TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_EncTest: Memory to Memory" );
        //3.    Call ENC channel FVDP API
        FVDP_DEBUG_EncInit(HandleContext_p);
        FVDP_DEBUG_EncQueueBuffer(HandleContext_p);
    }
    else
    {
        TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_Output: ALARM!!! Image wasn't frosen" );
    }
    #else
    TRC( TRC_ID_MAIN_INFO, "FVDP_DEBUG_Output: DEBUG_COMMAND_TEST_ENC not available" );
    #endif
}
#endif

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/

static void FVDP_DEBUG_McMadi_FE(FVDP_HandleContext_t* HandleContext_p)
{
    uint32_t TestCase;
    uint32_t Paramter;
    uint32_t Value;

    TestCase = MISC_BE_REG32_Read(CSC_BE_TEST_CASE);
    Paramter = MISC_BE_REG32_Read(CSC_BE_PARAMETER);
    Value = MISC_BE_REG32_Read(CSC_BE_VALUE);
    if (TestCase == 6)
    {
        switch(Value)
        {
            case 1:
            if (Paramter & 1)
                Tnr_Init(HandleContext_p);
            if (Paramter & 2)
                Tnr_LoadDefaultVqData();
            if (Paramter & 4)
                Tnr_Enable(1,1280,720);

                break;
            case 2:
            if (Paramter & 1)
                Madi_Init();
            if (Paramter & 2)
                Madi_LoadDefaultVqData();
            if (Paramter & 4)
                Madi_Enable();

                break;
            case 3:
            if (Paramter & 1)
                Mcdi_Init();
            if (Paramter & 2)
                Mcdi_LoadDefaultVqData();
            if (Paramter & 4)
                Mcdi_Enable(MCDI_MODE_OFF);

                break;
            case 4:
            if (Paramter & 1)
                Deint_Init();
            if (Paramter & 2)
                Deint_Enable(DEINT_MODE_SPATIAL,1280,720,720);

                break;

        }

    }
}
/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void FVDP_DEBUG_TestCsc_BE(void)
{
    uint32_t TestCase;
    uint32_t Paramter;
    uint32_t Value;
    FVDP_ColorSpace_t InputColorSpace = sdYUV;
    FVDP_ColorSpace_t OutputColorSpace = RGB;

    union
    {
        ColorMatrix_Adjuster_t ColorControl;
        uint16_t ColorParameter[10];
    }ColorSet;

    // default color parameters in 4.8 format
    ColorSet.ColorControl.Brightness = 0x100;
    ColorSet.ColorControl.Contrast = 0x100;
    ColorSet.ColorControl.Saturation = 0x100;
    ColorSet.ColorControl.Hue = 0x0;
    ColorSet.ColorControl.RedGain = 0x100;
    ColorSet.ColorControl.GreenGain = 0x100;
    ColorSet.ColorControl.BlueGain = 0x100;
    ColorSet.ColorControl.RedOffset = 0x10;
    ColorSet.ColorControl.GreenOffset = 0x10;
    ColorSet.ColorControl.BlueOffset = 0x10;

    TestCase = MISC_BE_REG32_Read(CSC_BE_TEST_CASE);
    Paramter = MISC_BE_REG32_Read(CSC_BE_PARAMETER);
    Value = MISC_BE_REG32_Read(CSC_BE_VALUE);

    if (TestCase == 1)
    {
        if (Paramter < 10)
            ColorSet.ColorParameter[Paramter] = Value;
    }
    else   if (TestCase == 2)
    {
        InputColorSpace = Value;
    }
    else   if (TestCase == 3)
    {
        OutputColorSpace = Value;
    }
    else   if (TestCase == 4)
    {
        switch(Value)
            {
                case 1:
                    InputColorSpace = RGB;
                    OutputColorSpace = RGB;
                    break;
                case 2:
                    InputColorSpace = sdYUV;
                    OutputColorSpace = RGB;
                    break;
                case 3:
                    InputColorSpace = hdYUV;
                    OutputColorSpace = RGB;
                    break;
                case 4:
                    InputColorSpace = sdYUV;
                    OutputColorSpace = RGB861;
                    break;
                case 5:
                    InputColorSpace = RGB;
                    OutputColorSpace = RGB;
                    break;
                case 6:
                    InputColorSpace = RGB;
                    OutputColorSpace = RGB;
                    break;
            }

    }
    else   if (TestCase == 5)
    {
        Csc_SetWindowMapping((CscMatrixToWindowMapping_t)Value);
    }

    CscFull_Update(InputColorSpace,
                   OutputColorSpace,
                   &ColorSet.ColorControl);
    CCTRL_BE_REG32_Write(CCF_UPDATE_CTRL,1); // force update color control

}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void fvdp_register_init(void)
{

    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_IRQ_4_MASK_A, 0x80000);//0xfd020534 ESW_WRITE32(0xFD020534, 0x80000)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_IRQ_4_MASK_B, 0x1);//0xfd020538 ESW_WRITE32(0xFD020538, 0x1)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_IRQ_5_MASK_A, 0x20000);//0xfd02054c ESW_WRITE32(0xFD02054c, 0x20000)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FR_MASK, 0x468FF);//0xfd020434 ESW_WRITE32(0xFD020434, 0x468FF)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FR_MASK, 0x23CFF);//0xfd040434 ESW_WRITE32(0xFD040434, 0x23CFF)
    REG32_Write(0x00000100, 0x3);
    REG32_Write(0x00000044, 0xf0);
    REG32_Write(0x00000128, 0x0);
    REG32_Write(0x00000124, 0x0);
    REG32_Write(0x00000070, 0x16920);
    REG32_Write(0x00000074, 0x16920);
    REG32_Write(0x00000078, 0x0);
    REG32_Write(0x0000007c, 0x0);
    REG32_Write(0x00000080, 0x0);
    REG32_Write(0x00000084, 0x45a);
    REG32_Write(0x00000088, 0x0);
    REG32_Write(0x0000008c, 0x0);
    REG32_Write(0x00000094, 0x16809);
    REG32_Write(0x00000098, 0x16809);
    REG32_Write(0x0000009c, 0x0);
    REG32_Write(0x000000a0, 0x0);
    REG32_Write(0x000000a4, 0x0);
    REG32_Write(0x000000a8, 0x45a);
    REG32_Write(0x000000ac, 0x0);
    REG32_Write(0x000000b0, 0x0);
    REG32_Write(0x000000b4, 0x2);
    REG32_Write(0x00000110, 0x1);
    REG32_Write(0x00000114, 0x3);
    REG32_Write(0x00000058, 0x0);
    REG32_Write(0x000000c4, 0x0);
    REG32_Write(0x000000c8, 0x24002d0);
    REG32_Write(0x000000cc, 0x50000000);
    REG32_Write(0x00000024, 0x4);
    REG32_Write(0x00000014, 0x9);
    REG32_Write(0x00000030, 0x802);
    REG32_Write(0x0000002c, 0x240016);
    REG32_Write(0x00000028, 0x240016);
    REG32_Write(0x0000001c, 0x5a0058);
    REG32_Write(0x00000018, 0x2d0058);
    REG32_Write(0x00000010, 0x332);
    REG32_Write(0x00000020, 0x25b);
    REG32_Write(0x00000004, 0x1);
    REG32_Write(0x00000118, 0x0);
    REG32_Write(0x0000010c, 0x1);
    REG32_Write(0x00000108, 0x1);
    REG32_Write(0x00000064, 0x4d628a00);
    REG32_Write(0x00000068, 0x4D68DE00);
    REG32_Write(0x0000006c, 0x0);
    REG32_Write(0x00000090, 0x0);
    REG32_Write(0x00000060, 0x2d0);
    REG32_Write(0x0000005c, 0x40008170);
    REG32_Write(0x00000000, 0x31);
    REG32_Write(0x000000c0, 0x2);
    REG32_Write(0x00000178, 0x2);
#if 1
    ISM_REG32_Write(FVDP_MAIN,IMP_SOURCE, 0xc); //0xfd010414 ESW_WRITE32(0xfd010414, 0xc)
    IVP_REG32_Write(FVDP_MAIN,IVP_CONTROL, 0x403);//0xfd012048 ESW_WRITE32(0xfd012048, 0x403)
    IVP_REG32_Write(FVDP_MAIN,IVP_H_ACT_START, 0x0); //0xfd01210c ESW_WRITE32(0xfd01210c, 0x0)
    IVP_REG32_Write(FVDP_MAIN,IVP_H_ACT_WIDTH, 0x2d0); //0xfd012110 ESW_WRITE32(0xfd012110, 0x2d0)
    IVP_REG32_Write(FVDP_MAIN,IVP_V_ACT_START_ODD, 0x15); //0xfd012114 ESW_WRITE32(0xfd012114, 0x15)
    IVP_REG32_Write(FVDP_MAIN,IVP_V_ACT_START_EVEN, 0x15); //0xfd012118 ESW_WRITE32(0xfd012118, 0x15)
    IVP_REG32_Write(FVDP_MAIN,IVP_V_ACT_LENGTH, 0x240);//0xfd01211c ESW_WRITE32(0xfd01211c, 0x240)
    IVP_REG32_Write(FVDP_MAIN,IVP_FRAME_RESET_LINE, 0x13); //0xfd012158 ESW_WRITE32(0xfd012158, 0x13)
    IVP_REG32_Write(FVDP_MAIN,IVP_PATGEN_CONTROL, 0x97); //0xfd012388 ESW_WRITE32(0xfd012388, 0x97)
    IVP_REG32_Write(FVDP_MAIN,IVP_PATGEN_COLOR, 0x0);//0xfd01238c ESW_WRITE32(0xfd01238c, 0x0)
    IVP_REG32_Write(FVDP_MAIN,IVP_UPDATE_CTRL, 0x1); //0xfd012004ESW_WRITE32(0xfd012004, 0x1)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x0);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_1, 0x0);//0xfd0205c0 ESW_WRITE32(0xfd0205c0, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x3);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x3)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x3);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x3)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_0, 0x403);//0xfd0205c4 ESW_WRITE32(0xfd0205c4, 0x403)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_0, 0x403);//0xfd0205c4 ESW_WRITE32(0xfd0205c4, 0x403)
    DFR_FE_REG32_Write(DFR_FE_COLOUR_CTRL, 0x12);//0xfd0205f0 ESW_WRITE32(0xfd0205f0, 0x12)
    DFR_FE_REG32_Write(DFR_FE_CHL_CTRL, 0x0);//0xfd0205f4 ESW_WRITE32(0xfd0205f4, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x1b);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x1b)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x1b);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x1b)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_0, 0x25000403);//0xfd0205c4 ESW_WRITE32(0xfd0205c4, 0x25000403)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_1, 0x26);//0xfd0205c8 ESW_WRITE32(0xfd0205c8, 0x26)
    DFR_FE_REG32_Write(DFR_FE_COLOUR_CTRL, 0x32);//0xfd0205f0 ESW_WRITE32(0xfd0205f0, 0x32)
    DFR_FE_REG32_Write(DFR_FE_CHL_CTRL, 0x0);//0xfd0205f4 ESW_WRITE32(0xfd0205f4, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0xdb);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0xdb)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0xdb);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0xdb)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_1, 0x7060026);//0xfd0205c8 ESW_WRITE32(0xfd0205c8, 0x7060026)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_2, 0x0);//0xfd0205cc ESW_WRITE32(0xfd0205cc, 0x0)
    DFR_FE_REG32_Write(DFR_FE_COLOUR_CTRL, 0x76);//0xfd0205f0 ESW_WRITE32(0xfd0205f0, 0x76)
    DFR_FE_REG32_Write(DFR_FE_CHL_CTRL, 0x0);//0xfd0205f4 ESW_WRITE32(0xfd0205f4, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x6db);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x6db)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_0, 0x6db);//0xfd0205bc ESW_WRITE32(0xfd0205bc, 0x6db)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_2, 0xa0900);//0xfd0205cc ESW_WRITE32(0xfd0205cc, 0xa0900)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_2, 0xa0900);//0xfd0205cc ESW_WRITE32(0xfd0205cc, 0xa0900)
    DFR_FE_REG32_Write(DFR_FE_COLOUR_CTRL, 0xfe);//0xfd0205f0 ESW_WRITE32(0xfd0205f0, 0xfe)
    DFR_FE_REG32_Write(DFR_FE_CHL_CTRL, 0x0);//0xfd0205f4 ESW_WRITE32(0xfd0205f4, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_1, 0x600);//0xfd0205c0 ESW_WRITE32(0xfd0205c0, 0x600)
    DFR_FE_REG32_Write(DFR_FE_MUX_EN_1, 0x600);//0xfd0205c0ESW_WRITE32(0xfd0205c0, 0x600)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_10, 0x10000);//0xfd0205ec ESW_WRITE32(0xfd0205ec, 0x10000)
    DFR_FE_REG32_Write(DFR_FE_MUX_SEL_10, 0x10000);//0xfd0205ec ESW_WRITE32(0xfd0205ec, 0x10000)
    DFR_FE_REG32_Write(DFR_FE_COLOUR_CTRL, 0xff);//0xfd0205f0 ESW_WRITE32(0xfd0205f0, 0xff)
    DFR_FE_REG32_Write(DFR_FE_CHL_CTRL, 0x0);//0xfd0205f4 ESW_WRITE32(0xfd0205f4, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_MODULE_CLK_EN, 0x2000); //0xfd020594 ESW_WRITE32(0xfd020594, 0x2000)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_CFG, 0x0);//0xfd020440 ESW_WRITE32(0xfd020440, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_0, 0x20002);//0xfd020444 ESW_WRITE32(0xfd020444, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_1, 0x20002);//0xfd020448 ESW_WRITE32(0xfd020448, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_2, 0x20002);//0xfd02044c ESW_WRITE32(0xfd02044c, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_3, 0x20002);//0xfd020450 ESW_WRITE32(0xfd020450, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_4, 0x20002);//0xfd020454 ESW_WRITE32(0xfd020454, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_5, 0x20002);//0xfd020458 ESW_WRITE32(0xfd020458, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_6, 0x20002);//0xfd02045c ESW_WRITE32(0xfd02045c, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_7, 0x20002);//0xfd020460 ESW_WRITE32(0xfd020460, 0x20002)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FRST_POS_8, 0x2);//0xfd020464 ESW_WRITE32(0xfd020464, 0x2)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_CLIENT_CTRL, 0x2400421);//0xfd0208d8 ESW_WRITE32(0xfd0208d8, 0x2400421)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_ADDR_CTRL, 0x70000);//0xfd0208e0 ESW_WRITE32(0xfd0208e0, 0x70000)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_STRIDE, 0x71); //0xfd0208e4 ESW_WRITE32(0xfd0208e4, 0x71)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_OFFSET, 0x2000); //0xfd0208e8 ESW_WRITE32(0xfd0208e8, 0x2000)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_PART_PKT, 0x400000);//0xfd0208ec ESW_WRITE32(0xfd0208ec, 0x400000)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_ADDR, 0x4C000000);//0xfd0208f0 ESW_WRITE32(0xfd0208f0, 0x4C000000)
    FVDP_MCC_FE_REG32_Write(P1_Y_RD_THROTTLE_CTRL, 0x0);//0xfd0208f4 ESW_WRITE32(0xfd0208f4, 0x0)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_CLIENT_CTRL, 0x2400021);//0xfd0208f8 ESW_WRITE32(0xfd0208f8, 0x2400021)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_HOR_SIZE, 0x2d0);//0xfd020918 ESW_WRITE32(0xfd020918, 0x2d0)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_ADDR_CTRL, 0x70000);//0xfd020900 ESW_WRITE32(0xfd020900, 0x70000)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_STRIDE, 0x71); //0xfd020904 ESW_WRITE32(0xfd020904, 0x71)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_OFFSET, 0x2000); //0xfd020908 ESW_WRITE32(0xfd020908, 0x2000)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_PART_PKT, 0x400000);//0xfd02090c ESW_WRITE32(0xfd02090c, 0x400000)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_ADDR, 0x4cF00000);//0xfd020910 ESW_WRITE32(0xfd020910, 0x4cF00000)
    FVDP_MCC_FE_REG32_Write(P1_UV_RD_THROTTLE_CTRL, 0x0);//0xfd020914 ESW_WRITE32(0xfd020914, 0x0)
    FVDP_MCC_FE_REG32_Write(D_Y_WR_CLIENT_CTRL, 0x2407c21);//0xfd020860 ESW_WRITE32(0xfd020860, 0x2407c21)
    FVDP_MCC_FE_REG32_Write(D_Y_WR_ADDR_CTRL, 0x70000);//0xfd020864 ESW_WRITE32(0xfd020864, 0x70000)
    FVDP_MCC_FE_REG32_Write(D_Y_WR_STRIDE, 0x71); //0xfd020868 ESW_WRITE32(0xfd020868, 0x71)
    FVDP_MCC_FE_REG32_Write(D_Y_WR_ADDR, 0x4C000000);//0xfd02086c ESW_WRITE32(0xfd02086c, 0x4C000000)
    FVDP_MCC_FE_REG32_Write(D_Y_WR_THROTTLE_CTRL, 0x0);//0xfd020870 ESW_WRITE32(0xfd020870, 0x0)
    FVDP_MCC_FE_REG32_Write(D_UV_WR_CLIENT_CTRL, 0x2400021);//0xfd020878 ESW_WRITE32(0xfd020878, 0x2400021)
    FVDP_MCC_FE_REG32_Write(D_UV_WR_ADDR_CTRL, 0x70000);//0xfd02087c ESW_WRITE32(0xfd02087c, 0x70000)
    FVDP_MCC_FE_REG32_Write(D_UV_WR_STRIDE, 0x71); //0xfd020880 ESW_WRITE32(0xfd020880, 0x71)
    FVDP_MCC_FE_REG32_Write(D_UV_WR_ADDR, 0x4CF00000);//0xfd020884 ESW_WRITE32(0xfd020884, 0x4CF00000)
    FVDP_MCC_FE_REG32_Write(D_UV_WR_THROTTLE_CTRL, 0x0);//0xfd020888 ESW_WRITE32(0xfd020888, 0x0)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_PLUG_CTRL, 0x1);//0xfd020800 ESW_WRITE32(0xfd020800, 0x1)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_0_RD_PLUG_TRF_CTRL, 0x5e2);//0xfd020804 ESW_WRITE32(0xfd020804, 0x5e2)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_0_WR_PLUG_TRF_CTRL, 0x5e2);//0xfd02081c ESW_WRITE32(0xfd02081c, 0x5e2)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_1_RD_PLUG_TRF_CTRL, 0x5e2);//0xfd02080c ESW_WRITE32(0xfd02080c, 0x5e2)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_2_RD_PLUG_TRF_CTRL, 0x5e2);//0xfd020814 ESW_WRITE32(0xfd020814, 0x5e2)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_1_WR_PLUG_TRF_CTRL, 0x5e2);//0xfd020824 ESW_WRITE32(0xfd020824, 0x5e2)
    FVDP_MCC_FE_REG32_Write(FVDP_FE_MCC_BYPASS, 0x0);//0xfd020a18 ESW_WRITE32(0xfd020a18, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_UPDATE_CTRL, 0x1000000);//0xfd02040c ESW_WRITE32(0xfd02040c, 0x1000000)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_UPDATE_CTRL, 0x0);//0xfd02040c ESW_WRITE32(0xfd02040c, 0x0)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FID_CFG_0, 0x11000);//0xfd02059c ESW_WRITE32(0xfd02059c, 0x11000)
    COMM_CLUST_FE_REG32_Write(COMMCTRL_FE_FW_FIELD_ID, 0x3);//0xfd020598 ESW_WRITE32(0xfd020598, 0x3)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x0);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x0)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_CFG, 0x8000); //0xfd040440 ESW_WRITE32(0xfd040440, 0x8000)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_CFG, 0x8010); //0xfd040440 ESW_WRITE32(0xfd040440, 0x8010)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x3);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x3)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x3);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x3)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_0, 0x1e1d);//0xfd0405c0 ESW_WRITE32(0xfd0405c0, 0x1e1d)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_0, 0x1e1d);//0xfd0405c0 ESW_WRITE32(0xfd0405c0, 0x1e1d)
    DFR_BE_REG32_Write(DFR_BE_COLOUR_CTRL, 0x10); //0xfd0405e0 ESW_WRITE32(0xfd0405e0, 0x10)
    DFR_BE_REG32_Write(DFR_BE_CHL_CTRL, 0x0);//0xfd0405e4 ESW_WRITE32(0xfd0405e4, 0x0)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_CFG, 0x8010); //0xfd040440 ESW_WRITE32(0xfd040440, 0x8010)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_CFG, 0x8010); //0xfd040440 ESW_WRITE32(0xfd040440, 0x8010)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x60000003);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x60000003)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x60000003);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x60000003)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_7, 0x40300);//0xfd0405dc ESW_WRITE32(0xfd0405dc, 0x40300)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_7, 0x40300);//0xfd0405dc ESW_WRITE32(0xfd0405dc, 0x40300)
    DFR_BE_REG32_Write(DFR_BE_COLOUR_CTRL, 0x12); //0xfd0405e0 ESW_WRITE32(0xfd0405e0, 0x12)
    DFR_BE_REG32_Write(DFR_BE_CHL_CTRL, 0x0);//0xfd0405e4 ESW_WRITE32(0xfd0405e4, 0x0)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_MODULE_CLK_EN, 0x2000); //0xfd040594 ESW_WRITE32(0xfd040594, 0x2000)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_0, 0x20002);//0xfd040444 ESW_WRITE32(0xfd040444, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_1, 0x20002);//0xfd040448 ESW_WRITE32(0xfd040448, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_2, 0x20002);//0xfd04044c ESW_WRITE32(0xfd04044c, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_3, 0x20002);//0xfd040450 ESW_WRITE32(0xfd040450, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_4, 0x20002);//0xfd040454 ESW_WRITE32(0xfd040454, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_5, 0x20002);//0xfd040458 ESW_WRITE32(0xfd040458, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_6, 0x20002);//0xfd04045c ESW_WRITE32(0xfd04045c, 0x20002)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_FRST_POS_7, 0x20002);//0xfd040460 ESW_WRITE32(0xfd040460, 0x20002)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_CLIENT_CTRL, 0x2400421);//0xfd040820 ESW_WRITE32(0xfd040820, 0x2400421)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_ADDR_CTRL, 0x70000);//0xfd040828 ESW_WRITE32(0xfd040828, 0x70000)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_STRIDE, 0x71); //0xfd04082c ESW_WRITE32(0xfd04082c, 0x71)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_OFFSET, 0x2000); //0xfd040830 ESW_WRITE32(0xfd040830, 0x2000)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_PART_PKT, 0x400000);//0xfd040834 ESW_WRITE32(0xfd040834, 0x400000)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_ADDR, 0x8002a600);//0xfd040838 ESW_WRITE32(0xfd040838, 0x8002a600)
    FVDP_MCC_BE_REG32_Write(OP_Y_RD_THROTTLE_CTRL, 0x0);//0xfd04083c ESW_WRITE32(0xfd04083c, 0x0)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_CLIENT_CTRL, 0x2400081);//0xfd040840 ESW_WRITE32(0xfd040840, 0x2400081)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_HOR_SIZE, 0x2d0);//0xfd040864 ESW_WRITE32(0xfd040864, 0x2d0)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_420_COEF, 0x8);//0xfd040860 ESW_WRITE32(0xfd040860, 0x8)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_ADDR_CTRL, 0x70000);//0xfd040848 ESW_WRITE32(0xfd040848, 0x70000)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_STRIDE, 0x71); //0xfd04084c ESW_WRITE32(0xfd04084c, 0x71)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_OFFSET, 0x2000); //0xfd040850 ESW_WRITE32(0xfd040850, 0x2000)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_PART_PKT, 0x400000);//0xfd040854 ESW_WRITE32(0xfd040854, 0x400000)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_ADDR, 0x8003f900);//0xfd040858 ESW_WRITE32(0xfd040858, 0x8003f900)
    FVDP_MCC_BE_REG32_Write(OP_RGB_UV_RD_THROTTLE_CTRL, 0x0);//0xfd04085c ESW_WRITE32(0xfd04085c, 0x0)
    FVDP_MCC_BE_REG32_Write(OP_Y_WR_CLIENT_CTRL, 0x2407c21);  //0xfd040868 ESW_WRITE32(0xfd040868, 0x2407c21)
    FVDP_MCC_BE_REG32_Write(OP_Y_WR_ADDR_CTRL, 0x70000);//0xfd04086c ESW_WRITE32(0xfd04086c, 0x70000)
    FVDP_MCC_BE_REG32_Write(OP_Y_WR_STRIDE, 0x71); //0xfd040870 ESW_WRITE32(0xfd040870, 0x71)
    FVDP_MCC_BE_REG32_Write(OP_Y_WR_ADDR, 0x8002a600);//0xfd040874 ESW_WRITE32(0xfd040874, 0x8002a600)
    FVDP_MCC_BE_REG32_Write(OP_Y_WR_THROTTLE_CTRL, 0x0);//0xfd040878 ESW_WRITE32(0xfd040878, 0x0)
    FVDP_MCC_BE_REG32_Write(OP_UV_WR_CLIENT_CTRL, 0x2400021);  //0xfd040880 ESW_WRITE32(0xfd040880, 0x2400021)
    FVDP_MCC_BE_REG32_Write(OP_UV_WR_ADDR_CTRL, 0x70000);//0xfd040884 ESW_WRITE32(0xfd040884, 0x70000)
    FVDP_MCC_BE_REG32_Write(OP_UV_WR_STRIDE, 0x71); //0xfd040888 ESW_WRITE32(0xfd040888, 0x71)
    FVDP_MCC_BE_REG32_Write(OP_UV_WR_ADDR, 0x8003f900);//0xfd04088c ESW_WRITE32(0xfd04088c, 0x8003f900)
    FVDP_MCC_BE_REG32_Write(OP_UV_WR_THROTTLE_CTRL, 0x0);//0xfd040890 ESW_WRITE32(0xfd040890, 0x0)
    FVDP_MCC_BE_REG32_Write(FVDP_BE_PLUG_CTRL, 0x1);//0xfd040800 ESW_WRITE32(0xfd040800, 0x1)
    FVDP_MCC_BE_REG32_Write(FVDP_BE_RD_0_PLUG_TRF_CTRL, 0x5e2);//0xfd040804 ESW_WRITE32(0xfd040804, 0x5e2)
    FVDP_MCC_BE_REG32_Write(FVDP_BE_RD_1_PLUG_TRF_CTRL, 0x5e2);//0xfd04080c ESW_WRITE32(0xfd04080c, 0x5e2)
    FVDP_MCC_BE_REG32_Write(FVDP_BE_WR_PLUG_TRF_CTRL, 0x5e2);//0xfd040814 ESW_WRITE32(0xfd040814, 0x5e2)
    FVDP_MCC_BE_REG32_Write(FVDP_BE_MCC_BYPASS, 0x0);//0xfd04089c ESW_WRITE32(0xfd04089c, 0x0)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_UPDATE_CTRL, 0x0800000);//0xfd04040c ESW_WRITE32(0xfd04040c, 0x800000)
    COMM_CLUST_BE_REG32_Write(COMMCTRL_BE_UPDATE_CTRL, 0x1800000);//0xfd04040c ESW_WRITE32(0xfd04040c, 0x1800000)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x3);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x3)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_0, 0x403);//0xfd0405c0 ESW_WRITE32(0xfd0405c0, 0x403)
    DFR_BE_REG32_Write(DFR_BE_MUX_EN_0, 0x1b003);//0xfd0405bc ESW_WRITE32(0xfd0405bc, 0x1b003)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_0, 0x100f);//0xfd0405c0 ESW_WRITE32(0xfd0405c0, 0x100f)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_1, 0x0);//0xfd0405c4 ESW_WRITE32(0xfd0405c4, 0x0)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_2, 0x0);//0xfd0405c8 ESW_WRITE32(0xfd0405c8, 0x0)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_3, 0xc000403);//0xfd0405cc ESW_WRITE32(0xfd0405cc, 0xc000403)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_4, 0xd);//0xfd0405d0 ESW_WRITE32(0xfd0405d0, 0xd)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_5, 0x0);//0xfd0405d4 ESW_WRITE32(0xfd0405d4, 0x0)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_6, 0x0);//0xfd0405d8ESW_WRITE32(0xfd0405d8, 0x0)
    DFR_BE_REG32_Write(DFR_BE_MUX_SEL_7, 0x0);//0xfd0405dc ESW_WRITE32(0xfd0405dc, 0x0)
#endif
#if 1
    HQS_HF_BE_REG32_Write(HFHQS_SHARPNESS, 0x0);//0xfd048018 ESW_WRITE32(0xfd048018, 0x0)
    HQS_HF_BE_REG32_Write(HFHQS_NOISE_CORING_CTRL, 0x0);//0xfd04801c ESW_WRITE32(0xfd04801c, 0x0)
    HQS_HF_BE_REG32_Write(HFHQS_NONLINEAR_CTRL, 0x400047ff);//0xfd048020 ESW_WRITE32(0xfd048020, 0x400047ff)
    HQS_HF_BE_REG32_Write(HFHQS_SCALE_VALUE, 0x0);//0xfd048028 ESW_WRITE32(0xfd048028, 0x0)
    HQS_HF_BE_REG32_Write(HFHQS_XLUMA_CTRL1, 0x00001040);//0xfd048110 ESW_WRITE32(0xfd048110, 0x00001040)
    HQS_HF_BE_REG32_Write(HFHQS_XLUMA_CTRL2, 0x1e120810);//0xfd048114 ESW_WRITE32(0xfd048114, 0x1e120810)
    HQS_HF_BE_REG32_Write(HFHQS_XLUMA_CTRL3, 0x00002410);//0xfd048118 ESW_WRITE32(0xfd048118, 0x00002410)
    HQS_HF_BE_REG32_Write(HFHQS_XLUMA_CTRL4, 0x18450200);//0xfd04811c ESW_WRITE32(0xfd04811c, 0x18450200)
    HQS_VF_BE_REG32_Write(VFHQS_CTRL, 0x00000001);//0xfd048418 ESW_WRITE32(0xfd048418, 0x00000001)
    HQS_VF_BE_REG32_Write(VFHQS_SHARPNESS, 0x0);//0xfd048420 ESW_WRITE32(0xfd048420, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_NOISE_CORING_CTRL, 0x0);//0xfd048424 ESW_WRITE32(0xfd048424, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_NONLINEAR_CTRL, 0x47ff47ff);//0xfd048428 ESW_WRITE32(0xfd048428, 0x47ff47ff)
    HQS_VF_BE_REG32_Write(VFHQS_SCALE_VALUE, 0x0);//0xfd048430 ESW_WRITE32(0xfd048430, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_OFFSET0, 0x0);//0xfd048434 ESW_WRITE32(0xfd048434, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_OFFSET1, 0x0);//0xfd048438 ESW_WRITE32(0xfd048438, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_XLUMA_CTRL1, 0x00001040);//0xfd048530 ESW_WRITE32(0xfd048530, 0x00001040)
    HQS_VF_BE_REG32_Write(VFHQS_XLUMA_CTRL3, 0x00002410);//0xfd048538 ESW_WRITE32(0xfd048538, 0x00002410)
    HQS_HF_BE_REG32_Write(HFHQS_CFG, 0xb);//0xfd04800c ESW_WRITE32(0xfd04800c, 0xb)
    HQS_HF_BE_REG32_Write(HFHQS_SIZE, 0x02d002d0);//0xfd048010 ESW_WRITE32(0xfd048010, 0x02d002d0)
    HQS_VF_BE_REG32_Write(VFHQS_INPUT_SIZE, 0x024002d0);//0xfd048410 ESW_WRITE32(0xfd048410, 0x024002d0)
    HQS_VF_BE_REG32_Write(VFHQS_SCALED_SIZE, 0x00000240);//0xfd048414 ESW_WRITE32(0xfd048414, 0x00000240)
    HQS_VF_BE_REG32_Write(VFHQS_DCDI_CTRL1, 0x0);//0xfd048444 ESW_WRITE32(0xfd048444, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_DCDI_CTRL2, 0x0);//0xfd048448 ESW_WRITE32(0xfd048448, 0x0)
    HQS_VF_BE_REG32_Write(VFHQS_CFG, 0x7);//0xfd04840c ESW_WRITE32(0xfd04840c, 0x7)
    HQS_HF_BE_REG32_Write(HFHQS_CTRL, 0x1);//0xfd048014 ESW_WRITE32(0xfd048014, 0x1)
    HQS_HF_BE_REG32_Write(HFHQS_UPDATE_CTRL, 0x1);//0xfd048008 ESW_WRITE32(0xfd048008, 0x1)
    HQS_VF_BE_REG32_Write(VFHQS_UPDATE_CTRL, 0x1);//0xfd048408 ESW_WRITE32(0xfd048408, 0x1)
#endif
#if 1
    CCTRL_BE_REG32_Write(CCF_CTRL, 0x0); //0xfd05000C ESW_WRITE32(0xFD05000C, 0x0)
    CCTRL_BE_REG32_Write(CCF_P_IN_OFFSET2, 0x11); //0xfd050250 ESW_WRITE32(0xFD050250, 0x11)
    CCTRL_BE_REG32_Write(CCF_P_IN_OFFSET3, 0xfffffc00);//0xfd050254 ESW_WRITE32(0xFD050254, 0xfffffc00)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF11, 0xffffe000);//0xfd050258 ESW_WRITE32(0xFD050258, 0xffffe000)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF12, 0xffffe000);//0xfd05025C ESW_WRITE32(0xFD05025C, 0xffffe000)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF13, 0x4a8);//0xfd050260 ESW_WRITE32(0xFD050260, 0x4a8)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF21, 0x4a8);//0xfd050264 ESW_WRITE32(0xFD050264, 0x4a8)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF22, 0x4a8);//0xfd050268 ESW_WRITE32(0xFD050268, 0x4a8)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF23, 0x0);//0xfd05026C ESW_WRITE32(0xFD05026C, 0x0)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF31, 0xfffffe70);//0xfd050270 ESW_WRITE32(0xFD050270, 0xfffffe70)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF32, 0x812);//0xfd050274 ESW_WRITE32(0xFD050274, 0x812)
    CCTRL_BE_REG32_Write(CCF_P_MULT_COEF33, 0x664);//0xfd050278 ESW_WRITE32(0xFD050278, 0x664)
    CCTRL_BE_REG32_Write(CCF_P_OUT_OFFSET1, 0xfffffcbf);//0xfd05027C ESW_WRITE32(0xFD05027C, 0xfffffcbf)
    CCTRL_BE_REG32_Write(CCF_P_OUT_OFFSET2, 0x0);//0xfd050280 ESW_WRITE32(0xFD050280, 0x0)
    CCTRL_BE_REG32_Write(CCF_P_OUT_OFFSET3, 0x0);//0xfd050284 ESW_WRITE32(0xFD050284, 0x0)
    CCTRL_BE_REG32_Write(CCF_S_IN_OFFSET1, 0x0);//0xfd050288 ESW_WRITE32(0xFD050288, 0x0)
    CCTRL_BE_REG32_Write(CCF_S_IN_OFFSET2, 0x0);//0xfd05028C ESW_WRITE32(0xFD05028C, 0x0)
    CCTRL_BE_REG32_Write(CCF_UPDATE_CTRL, 0x1);//0xfd050008 ESW_WRITE32(0xFD050008, 0x1)
#endif

/*

    REG32_Write(0x00000008, 0x2);
    REG32_Write(0x00340700, 0x0);
    REG32_Write(0x00340704, 0x480);
    REG32_Write(0x0034070C, 0x1A0104);
    REG32_Write(0x00340710, 0x25903d3);
    REG32_Write(0x00340728, 0x0);
    REG32_Write(0x0034072C, 0x0);
    REG32_Write(0x00340c00, 0x3);
    REG32_Write(0x00340c34, 0x1);
    REG32_Write(0x00000008, 0x2);
*/

}

#ifdef ENABLE_FVDP_LOGGING
/* local defines -------------------------------------------------------------*/
#define dbg_printBUF_SIZE 32768
#define INIT_dbg_printBUF_SIZE 8096
static FVDP_Result_t add_to_buff(char * buff, uint32_t * offset, void * source, uint32_t size, uint32_t bufsize);

/* static variables ----------------------------------------------------------*/
static uint8_t dbg_printbuf[dbg_printBUF_SIZE];
static uint8_t ilgbuf[INIT_dbg_printBUF_SIZE];
static uint32_t loffs = 0;
static uint32_t ioffs = 0;
static bool logflag = FALSE;
#define ADD_TO_dbg_print_BUFF(a,b) if(add_to_buff(dbg_printbuf,&loffs,&a,b,dbg_printBUF_SIZE)>FVDP_OK){return ;}
#define ADD_TO_INIT_BUF(a,b) if(add_to_buff(ilgbuf,&ioffs,&a,b,INIT_dbg_printBUF_SIZE)>FVDP_OK){return ;}
/* Private Types ------------------------------------------------------------ */

/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_LogControl(FVDP_LogControl_t logcontrol)
{
    switch(logcontrol)
    {
        case FVDP_LogInit:      /*log init*/
            loffs = 0;
            ioffs = 0;
            break;
        case FVDP_LogStart:     /*log start*/
            logflag = TRUE;
        case FVDP_LogStop:      /*log stop*/
            logflag = FALSE;
            break;
        case FVDP_LogPrint:     /*print main log*/
            logflag = FALSE;
            FVDP_LogMainPrint();
            loffs = 0;
            break;
        case FVDP_InitLogPrint: /*print init log*/
            logflag = FALSE;
            FVDP_LogInitPrint();
            ioffs = 0;
            break;
    }
}

/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_LogCollection(const char * function, ...)
{

    va_list ap;
    int32_t i;

//  ERICB - TODO:
//  vibe_os_get_system_time returns vibe_time64_t which is a 64-bit type, so this line
//  should be fixed.
//  uint32_t time = (uint32_t)(0xFFFFFFFF & g_pIOS->GetSystemTime());
    uint32_t time = (uint32_t)(0xFFFFFFFF & (uint32_t)vibe_os_get_system_time());


//    char cmdstring[48];
    command_description_t * cd = NULL;
    /*get function*/
    for(i = 0; command_list[i].id != zero_cmd; i++)
    {
        if(!strcmp(command_list[i].command, function))
        {
            cd = &command_list[i];
            break;
        }
    }
    if(!cd)
    {/*function name wasn't find in the command list*/
        TRC( TRC_ID_MAIN_INFO, "FVDP_dbg_printGING Error: %s - function name is not listed", function );
        return;
    }
//    strcpy(cmdstring,cd->command);
//    strcat(cmdstring,"\n{\n");
    va_start(ap, function);
    switch(cd->id)
    {
        case FVDP_InitDriver_cmd:
        {
            FVDP_Init_t * InitParam = va_arg(ap, FVDP_Init_t*);
            ADD_TO_INIT_BUF(cd->id,sizeof(command_id_t));
            ADD_TO_INIT_BUF(time,sizeof(uint32_t));
            ADD_TO_INIT_BUF(InitParam,sizeof(FVDP_Init_t));
            break;
        }
        case FVDP_GetRequiredVideoMemorySize_cmd:
        {
            FVDP_CH_t      CH        = va_arg(ap, FVDP_CH_t);
            FVDP_Context_t Context   = va_arg(ap, FVDP_Context_t);
            uint32_t*      MemSize   = va_arg(ap, uint32_t*);

            ADD_TO_INIT_BUF(cd->id,     sizeof(command_id_t));
            ADD_TO_INIT_BUF(time,sizeof(uint32_t));
            ADD_TO_INIT_BUF(CH,         sizeof(FVDP_CH_t));
            ADD_TO_INIT_BUF(Context,    sizeof(FVDP_Context_t));
            ADD_TO_INIT_BUF(MemSize,    sizeof(uint32_t*));
            break;
        }

        case FVDP_OpenChannel_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle   = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_CH_t                  CH   = va_arg(ap, FVDP_CH_t);
            uint32_t       MemBaseAddress   = va_arg(ap, uint32_t);
            uint32_t              MemSize   = va_arg(ap, uint32_t);
            FVDP_Context_t        Context   = va_arg(ap, FVDP_Context_t);

            ADD_TO_INIT_BUF(cd->id,      sizeof(command_id_t));
            ADD_TO_INIT_BUF(time,        sizeof(uint32_t));
            ADD_TO_INIT_BUF(FvdpChHandle,sizeof(FVDP_CH_Handle_t));
            ADD_TO_INIT_BUF(CH,          sizeof(FVDP_CH_t));
            ADD_TO_INIT_BUF(MemBaseAddress,    sizeof(uint32_t));
            ADD_TO_INIT_BUF(MemSize,           sizeof(uint32_t));
            ADD_TO_INIT_BUF(Context,           sizeof(FVDP_Context_t));
            break;
        }
        case FVDP_CloseChannel_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle        = va_arg(ap, FVDP_CH_Handle_t);
            ADD_TO_INIT_BUF(cd->id,     sizeof(command_id_t));
            ADD_TO_INIT_BUF(time,sizeof(uint32_t));
            ADD_TO_INIT_BUF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            break;
        }
        case FVDP_QueueFlush_cmd:
        case FVDP_InputUpdate_cmd:
        case FVDP_OutputUpdate_cmd:
        case FVDP_CommitUpdate_cmd:
        case FVDP_DisconnectInput_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle        = va_arg(ap, FVDP_CH_Handle_t);
            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            break;
        }
        case FVDP_ConnectInput_cmd:
        {
            FVDP_CH_Handle_t  FvdpChHandle    = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_Input_t      Input           = va_arg(ap, FVDP_Input_t);
            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(Input,      sizeof(FVDP_Input_t));
            break;
        }
        case FVDP_SetOutputVideoInfo_cmd:
        case FVDP_SetInputVideoInfo_cmd:
        {
            FVDP_CH_Handle_t           FvdpChHandle    = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_VideoInfo_t           VideoInfo    = va_arg(ap, FVDP_VideoInfo_t);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(VideoInfo,  sizeof(FVDP_VideoInfo_t));
            break;
        }
        case FVDP_GetCropWindow_cmd:
        case FVDP_GetInputVideoInfo_cmd:
        case FVDP_GetOutputVideoInfo_cmd:
        case FVDP_GetInputRasterInfo_cmd:
        {
            FVDP_CH_Handle_t      FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            void*                 Pointer = va_arg(ap, void*);
            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(Pointer,    sizeof(void*));
            break;
        }

        case FVDP_SetInputRasterInfo_cmd:
        {
            FVDP_CH_Handle_t  FvdpChHandle    = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_RasterInfo_t InputRasterInfo = va_arg(ap, FVDP_RasterInfo_t);

            ADD_TO_dbg_print_BUFF(cd->id,         sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,             sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(InputRasterInfo,sizeof(FVDP_RasterInfo_t));

            break;
        }
        case FVDP_SetCropWindow_cmd:
        {
            FVDP_CH_Handle_t  FvdpChHandle  = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_CropWindow_t CropWindow    = va_arg(ap, FVDP_CropWindow_t);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(CropWindow, sizeof(FVDP_CropWindow_t));

            break;
        }
        case FVDP_SetPSIControl_cmd:
        {
            FVDP_CH_Handle_t  FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_PSIControl_t   PSIControl = va_arg(ap, FVDP_PSIControl_t);
            int32_t                  Value = va_arg(ap, int32_t);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(PSIControl, sizeof(FVDP_PSIControl_t));
            ADD_TO_dbg_print_BUFF(Value,      sizeof(int32_t));

            break;
        }
        case FVDP_GetPSIControl_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_PSIControl_t  PSIControl = va_arg(ap, FVDP_PSIControl_t);
            int32_t*                Value = va_arg(ap, int32_t*);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(PSIControl, sizeof(FVDP_PSIControl_t));
            ADD_TO_dbg_print_BUFF(Value,      sizeof(int32_t*));

            break;
        }
        case FVDP_SetPSIState_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_PSIFeature_t  PSIFeature = va_arg(ap, FVDP_PSIFeature_t);
            FVDP_PSIState_t      PSIState = va_arg(ap, FVDP_PSIState_t);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(PSIFeature, sizeof(FVDP_PSIFeature_t));
            ADD_TO_dbg_print_BUFF(PSIState,   sizeof(FVDP_PSIState_t));

            break;
        }
        case FVDP_GetPSIState_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_PSIFeature_t  PSIFeature = va_arg(ap, FVDP_PSIFeature_t);
            FVDP_PSIState_t*     PSIState = va_arg(ap, FVDP_PSIState_t*);

            ADD_TO_dbg_print_BUFF(cd->id,     sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,         sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(PSIFeature, sizeof(FVDP_PSIFeature_t));
            ADD_TO_dbg_print_BUFF(PSIState,   sizeof(FVDP_PSIState_t*));

            break;
        }
        case FVDP_GetPSIControlsRange_cmd:
        {
            FVDP_CH_Handle_t        FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_PSIControl_t  PSIControlsSelect = va_arg(ap, FVDP_PSIControl_t);
            int32_t                    *MinValue = va_arg(ap, uint32_t*);
            int32_t                    *MaxValue = va_arg(ap, uint32_t*);
            ADD_TO_dbg_print_BUFF(cd->id,             sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,                 sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(PSIControlsSelect,  sizeof(FVDP_PSIControl_t));
            ADD_TO_dbg_print_BUFF(MinValue,           sizeof(FVDP_PSIState_t*));
            ADD_TO_dbg_print_BUFF(MaxValue,           sizeof(FVDP_PSIState_t*));
            break;
        }
        case FVDP_GetVersion_cmd:
        {
            void*      Pointer  = va_arg(ap, void*);
            ADD_TO_dbg_print_BUFF(cd->id,             sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(Pointer,            sizeof(void*));
            break;
        }
        case FVDP_GetCrcInfo_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            FVDP_CrcFeature_t     Feature = va_arg(ap, FVDP_CrcFeature_t);
            FVDP_CrcInfo_t*       Pointer = va_arg(ap, FVDP_CrcInfo_t*);

            ADD_TO_dbg_print_BUFF(cd->id,             sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,                 sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(Feature,            sizeof(FVDP_CrcFeature_t));
            ADD_TO_dbg_print_BUFF(Pointer,            sizeof(FVDP_CrcInfo_t*));

            break;
        }
        case  FVDP_SetBufferCallback_cmd:
        {
            FVDP_CH_Handle_t FvdpChHandle = va_arg(ap, FVDP_CH_Handle_t);
            int32_t *    ScalingCompleted = va_arg(ap, uint32_t*);
            int32_t *          BufferDone = va_arg(ap, uint32_t*);

            ADD_TO_dbg_print_BUFF(cd->id,           sizeof(command_id_t));
            ADD_TO_dbg_print_BUFF(time,             sizeof(uint32_t));
            ADD_TO_dbg_print_BUFF(FvdpChHandle,     sizeof(FVDP_CH_Handle_t));
            ADD_TO_dbg_print_BUFF(ScalingCompleted, sizeof(int32_t*));
            ADD_TO_dbg_print_BUFF(BufferDone,       sizeof(int32_t*));

        }

        default:
            TRC( TRC_ID_MAIN_INFO, "FVDP_dbg_printGING: command parsing error" );
            va_end(ap);
            return;

    }
    va_end(ap);
}

/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static FVDP_Result_t add_to_buff(char * buff, uint32_t * offset, void * source, uint32_t size, uint32_t bufsize)
{
    if((*offset + size) >= bufsize)
        return FVDP_INVALID_MEM_ADDR;/*buffer overflow*/

    memcpy((void*)&buff[*offset],source,size);
    *offset += size;
    return FVDP_OK;
}

/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static void dbg_print_print_out(char * buf, uint32_t size)
{
    uint32_t i,index = 0;
    while(index < size)
    {
        command_id_t cmdid = *(command_id_t*)&buf[index];
        char cmdstring[256];
        command_description_t * cd_p = NULL;
        function_description_t * fd_p = NULL;

        for(i = 0; command_list[i].id != zero_cmd; i++)
        {
            if(command_list[i].id == cmdid)
            {
                cd_p = &command_list[i];
                break;
            }
        }
        if(!cd_p)
        {
            TRC( TRC_ID_MAIN_INFO, "FVDP_Debug dbg_print_out error: command not found" );
            return;
        }

        index += sizeof(uint32_t);

        sprintf(cmdstring,"%s   // timestamp: %u\n{\n",cd_p->command,*(uint32_t*)&buf[index]);

        fd_p = (function_description_t *)cd_p->function;
        switch(cmdid)
        {
            case FVDP_InitDriver_cmd:
            {
                FVDP_Init_t InitParam = *(FVDP_Init_t*)&buf[index += sizeof(uint32_t)];
                index += sizeof(InitParam);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[0].argument, InitParam.pDeviceBaseAddress );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_GetRequiredVideoMemorySize_cmd:
            {
                FVDP_CH_t           CH  = *(FVDP_CH_t*)&buf[index += sizeof(uint32_t)];
                FVDP_Context_t Context  = *(FVDP_Context_t*)&buf[index += sizeof(FvdpChHandle)];
                uint32_t*      MemSize  = *(uint32_t**)&buf[index += sizeof(Context)];
                index +=sizeof(MemSize);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, Context.ProcessingType );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, Context.MaxWindowSize.Height );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[3].argument, Context.MaxWindowSize.Width );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[4].argument, MemSize );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }

            case FVDP_OpenChannel_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                uint32_t       MemBaseAddress = *(uint32_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                uint32_t              MemSize = *(uint32_t*)&buf[index += sizeof(uint32_t)];
                FVDP_Context_t        Context = *(FVDP_Context_t*)&buf[index += sizeof(uint32_t)];
                index +=sizeof(Context);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, MemBaseAddress );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, MemSize );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[3].argument, Context.ProcessingType );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[4].argument, Context.MaxWindowSize.Width );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[5].argument, Context.MaxWindowSize.Height );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_QueueFlush_cmd:
            case FVDP_InputUpdate_cmd:
            case FVDP_CloseChannel_cmd:
            case FVDP_OutputUpdate_cmd:
            case FVDP_CommitUpdate_cmd:
            case FVDP_DisconnectInput_cmd:
            {
                FVDP_CH_Handle_t  FvdpChHandle  = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                index += sizeof(FvdpChHandle);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_ConnectInput_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_Input_t            Input = *(FVDP_Input_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                index += sizeof(Input);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, Input );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_SetOutputVideoInfo_cmd:
            case FVDP_SetInputVideoInfo_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle   = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_VideoInfo_t    VideoInfo   = *(FVDP_VideoInfo_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                index += sizeof(VideoInfo);

                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %u", fd_p[1].argument, VideoInfo.FrameID              );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, VideoInfo.ScanType             );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[3].argument, VideoInfo.FieldType            );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[4].argument, VideoInfo.ColorSpace           );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[5].argument, VideoInfo.ColorSampling        );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[6].argument, VideoInfo.FrameRate            );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[7].argument, VideoInfo.Width                );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[8].argument, VideoInfo.Height               );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[9].argument, VideoInfo.AspectRatio          );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[10].argument, VideoInfo.FVDP3DFormat         );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[11].argument, VideoInfo.FVDP3DFlag           );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[12].argument, VideoInfo.FVDP3DSubsampleMode  );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[13].argument, VideoInfo.FVDPDisplayModeFlag  );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_GetCropWindow_cmd:
            case FVDP_GetInputVideoInfo_cmd:
            case FVDP_GetOutputVideoInfo_cmd:
            case FVDP_GetInputRasterInfo_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                void*                 Pointer = *(void**)&buf[index += sizeof(FVDP_CH_Handle_t)];
                index +=sizeof(Pointer);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[1].argument, Pointer );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_SetInputRasterInfo_cmd:
            {
                FVDP_CH_Handle_t     FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_RasterInfo_t InputRasterInfo = *(FVDP_RasterInfo_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                index +=sizeof(InputRasterInfo);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, InputRasterInfo.HStart     );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, InputRasterInfo.VStart     );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[3].argument, InputRasterInfo.HTotal     );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[4].argument, InputRasterInfo.VTotal     );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[5].argument, InputRasterInfo.UVAlign    );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[6].argument, InputRasterInfo.FieldAlign );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_SetCropWindow_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_CropWindow_t  CropWindow = *(FVDP_CropWindow_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                index +=sizeof(CropWindow);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, CropWindow.HCropStart       );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, CropWindow.HCropStartOffset );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[3].argument, CropWindow.HCropWidth       );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[4].argument, CropWindow.VCropStart       );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[5].argument, CropWindow.VCropStartOffset );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[6].argument, CropWindow.VCropHeight      );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_SetPSIControl_cmd:
            {
                FVDP_CH_Handle_t  FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_PSIControl_t   PSIControl = *(FVDP_PSIControl_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                int32_t                  Value = *(int32_t*)&buf[index += sizeof(PSIControl)];
                index +=sizeof(Value);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, PSIControl );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, Value      );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_GetPSIControl_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_PSIControl_t  PSIControl = *(FVDP_PSIControl_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                int32_t*                Value = *(int32_t**)&buf[index += sizeof(PSIControl)];
                index +=sizeof(Value);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, PSIControl );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[2].argument, Value    );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_SetPSIState_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_PSIFeature_t  PSIFeature = *(FVDP_PSIFeature_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                FVDP_PSIState_t      PSIState = *(FVDP_PSIControl_t*)&buf[index += sizeof(PSIFeature)];
                index +=sizeof(PSIState);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, PSIFeature );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[2].argument, PSIState   );

                break;
            }
            case FVDP_GetPSIState_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_PSIFeature_t  PSIFeature = *(FVDP_PSIFeature_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                FVDP_PSIState_t*     PSIState = *(FVDP_PSIState_t**)&buf[index += sizeof(PSIFeature)];
                index +=sizeof(PSIState);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, PSIState );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[2].argument, PSIFeature );
                TRC( TRC_ID_MAIN_INFO, "}" );
                break;
            }
            case FVDP_GetPSIControlsRange_cmd:
            {
                FVDP_CH_Handle_t       FvdpChHandle   = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_PSIControl_t PSIControlsSelect   = *(FVDP_PSIControl_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                int32_t                   *MinValue   = *(int32_t**)&buf[index += sizeof(FVDP_PSIControl_t)];
                int32_t                   *MaxValue   = *(int32_t**)&buf[index += sizeof(int32_t*)];
                index +=sizeof(int32_t*);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, PSIControlsSelect );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[2].argument, MinValue );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[3].argument, MaxValue );
                TRC( TRC_ID_MAIN_INFO, "}" );

                break;
            }
            case FVDP_GetVersion_cmd:
            {
                void*      Pointer  = *(void**)&buf[index += sizeof(uint32_t)];
                index +=sizeof(void*);
                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[0].argument, Pointer );
                TRC( TRC_ID_MAIN_INFO, "}" );

                break;
            }
            case FVDP_GetCrcInfo_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                FVDP_CrcFeature_t     Feature = *(FVDP_CrcFeature_t*)&buf[index += sizeof(FVDP_CH_Handle_t)];
                FVDP_CrcInfo_t*       Pointer = *(FVDP_CrcInfo_t**)&buf[index += sizeof(FVDP_CrcFeature_t)];
                index +=sizeof(FVDP_CrcInfo_t*);

                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, Feature );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[2].argument, Pointer );
                TRC( TRC_ID_MAIN_INFO, "}" );

                break;
            }
            case FVDP_SetBufferCallback_cmd:
            {
                FVDP_CH_Handle_t FvdpChHandle = *(FVDP_CH_Handle_t*)&buf[index += sizeof(uint32_t)];
                int32_t     *ScalingCompleted = *(int32_t**)&buf[index += sizeof(FVDP_PSIControl_t)];
                int32_t     *BufferDone       = *(int32_t**)&buf[index += sizeof(int32_t*)];
                index +=sizeof(FVDP_CrcInfo_t*);

                TRC( TRC_ID_MAIN_INFO, "%s", cmdstring );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[0].argument, FvdpChHandle );
                TRC( TRC_ID_MAIN_INFO, "\t%s = %d", fd_p[1].argument, ScalingCompleted );
                TRC( TRC_ID_MAIN_INFO, "\t%s = 0x%X", fd_p[2].argument, BufferDone );
                TRC( TRC_ID_MAIN_INFO, "}" );
            }

            default:
                TRC( TRC_ID_MAIN_INFO, "FVDP_Debug dbg_print_out error: log is not suppurted for command %s", cd_p->command );
                return;

        }
    }

    TRC( TRC_ID_MAIN_INFO, "FVDP_Debug: log printing is completed" );
    // */
}
/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_LogMainPrint(void)
{
    dbg_print_print_out(dbg_printbuf, loffs);
}
/*******************************************************************************
Name        :
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void FVDP_LogInitPrint(void)
{
    dbg_print_print_out(ilgbuf, ioffs);
}
#endif
