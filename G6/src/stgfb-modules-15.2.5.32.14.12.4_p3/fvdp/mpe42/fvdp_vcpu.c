/***********************************************************************
 *
 * File: fvdp_vcpu.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_vcpu.h"
#include "fvdp_mbox.h"
#include "fvdp_vcd.h"
#include "fvdp_debug.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

// TIMEOUT values (theoretical): to be valided on real platform
#if defined(FVDP_SIMULATION)
#define WAIT_FOR_VCPU_INIT_TIME         0x10000000
#else
#define WAIT_FOR_VCPU_INIT_TIME         50000L
#endif
#define WAIT_FOR_VCPU_COMMAND_TIME      300000L    // 3.33ns/cycle, approx 1 ms
#define VCPUTOCCPU_SEM_TIMEOUT          1515151    // 300MHz tick = 3.3ns
                                                   // 1515151 ticks = 5ms

#define VCPU_PROGRAM_MEMORY_SIZE        0x10000     //in bytes
#define VCPU_DATA_MEMORY_SIZE           0x8000      //in bytes

#define WORD_SIZE 4
#define TCDM_BASE_ADDRESS   0x0
#define TCPM_BASE_ADDRESS   0x400000

#define NUM_FBM_LOG_ELEMENTS 200 //

/* Private Macros ----------------------------------------------------------- */
//#define DEBUG_OUTPUT_IN_VCD_FORMAT

//#define TrVCPU(level,...)  TraceModuleOn(0, 2, __VA_ARGS__)
//#define TrVCPU(level,...)   vibe_printf(__VA_ARGS__)
#define TrVCPU(level,...)   TRC( TRC_ID_MAIN_INFO, __VA_ARGS__ )

/* Private Variables (static)------------------------------------------------ */

bool        VcpuInitialized = FALSE;
bool        VcpuStarted = FALSE;
#ifndef FVDP_SIMULATION
static DMA_Area    m_VcdLog;
#endif

#ifdef USE_VCPU
/* Private Function Prototypes ---------------------------------------------- */

#if (VCD_EXTENDED_LOG_CONTROLS == FALSE)
static void DebugPrintFBMInputCurrIn( FBMDebugData_t* ptr);
static void DebugPrintFBMInputCurrOut( FBMDebugData_t* ptr);
static void DebugPrintFBMInputPrevIn( FBMDebugData_t* ptr);
//static void DebugPrintFBMInputPrevOut( FBMDebugData_t* ptr);
static void DebugPrintFBMOutputIn( FBMDebugData_t* ptr);
static void DebugPrintFBMOutputOut( FBMDebugData_t* ptr);
static void DebugPrintFBMInputFrameReset(  FBMDebugData_t* ptr);
static void DebugPrintFBMOutputFrameReset(  FBMDebugData_t* ptr);
#endif // (VCD_EXTENDED_LOG_CONTROLS == FALSE)
#endif // USE_VCPU

/* Global Variables --------------------------------------------------------- */
//DMA_Area m_FBMLogArea;

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : vcpu_InitPlugCtrl
Description : Programs plug control registers
Parameters  : none
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
static void vcpu_InitPlugCtrl (void)
{
    /*
    ** program RD_PLUG_CTRL as follows:
    ** MINIMUM_MSG_SPACE = 0
    ** MAXIMUM_OPCODE_SIZE = 3
    ** MINIMUM_IPCODE_SIZE = 3
    ** MAXIMUM_CHUNK_SIZE = 0
    ** MAXIMUM_MESSAGE_SPACE = 0
    ** PAGE_SIZE = 0
    */
    MBOX_REG32_SetField( RD_PLUG_CTRL, MINIMUM_MSG_SPACE,      0);
    MBOX_REG32_SetField( RD_PLUG_CTRL, MAXIMUM_OPCODE_SIZE,    3);
    //MBOX_REG32_SetField( RD_PLUG_CTRL, MINIMUM_IPCODE_SIZE,    3);
    MBOX_REG32_SetField( RD_PLUG_CTRL, MAXIMUM_CHUNK_SIZE,     0);
    //MBOX_REG32_SetField( RD_PLUG_CTRL, MAXIMUM_MESSAGE_SPACE,   0);
    MBOX_REG32_SetField( RD_PLUG_CTRL, PAGE_SIZE, 0);

    /*
    ** Program WR_PLUG_CTRL as follows:
    ** MINIMUM_MSG_SPACE = 0
    ** MAXIMUM_OPCODE_SIZE = 3
    ** MINIMUM_IPCODE_SIZE = 3
    ** MAXIMUM_CHUNK_SIZE = 0
    ** MAXIMUM_MESSAGE_SPACE = 0
    ** PAGE_SIZE = 0
    */
    MBOX_REG32_SetField( WR_PLUG_CTRL, MINIMUM_MSG_SPACE,      0);
    MBOX_REG32_SetField( WR_PLUG_CTRL, MAXIMUM_OPCODE_SIZE,    3);
    //MBOX_REG32_SetField( WR_PLUG_CTRL, MINIMUM_IPCODE_SIZE,    3);
    MBOX_REG32_SetField( WR_PLUG_CTRL, MAXIMUM_CHUNK_SIZE,     0);
    //MBOX_REG32_SetField( WR_PLUG_CTRL, MAXIMUM_MESSAGE_SPACE,   0);
    MBOX_REG32_SetField( WR_PLUG_CTRL, PAGE_SIZE, 0);

    /* Program MC_CONFIG to 1 */
    STREAMER_REG32_SetField(MC, LMAM, 1);

    //MBOX_REG32_Write(IT_CONTROL, 0x7F);    // TODO: ?? in original code
}

/*******************************************************************************
Name        : FVDP_Result_t VCPU_Init(void)
Description : Load VCPU code and data into TCPM/TCDM via streamer
Parameters  : none
Assumptions : VCPU code not initialized nor started
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VCPU_Init (const uint32_t* const XP70Dram_p,
                         const uint32_t* const XP70Iram_p)
{
    FVDP_Result_t status;
    uint32_t XP70ProgramMemoryPhyAddr;
    uint32_t XP70DataMemoryPhyAddr;

    DMA_Area m_VcpuLoad;

    /* check that VCPU has not already initialized */
    if (VcpuInitialized == TRUE)
    {
        TrVCPU(DBG_ERROR, "already initialized\n");
        return FVDP_ERROR;
    }

    /* check that VCPU has not already started */
    if (VcpuStarted == TRUE)
    {
        TrVCPU(DBG_ERROR, "already started\n");
        return FVDP_ERROR;
    }

    /*
    ** Allocate a DMA_Area. You can specify some alignment constraints.
    */
    if(!XP70Dram_p || !XP70Iram_p)
        return FVDP_ERROR;

    vibe_os_allocate_dma_area(&m_VcpuLoad,
                            VCPU_PROGRAM_MEMORY_SIZE+VCPU_DATA_MEMORY_SIZE,
                            32, SDAAF_UNCACHED);
    if(!m_VcpuLoad.pMemory)
    {
        TrVCPU(DBG_ERROR, "Failed to allocate VCPU loading memory\n");
        vibe_os_free_dma_area(&m_VcpuLoad);
        return FVDP_MEM_INIT_ERROR;
    }

    /* Copy the data and code portions */
    vibe_os_memcpy_to_dma_area(&m_VcpuLoad, 0, XP70Dram_p, VCPU_DATA_MEMORY_SIZE);
    vibe_os_memcpy_to_dma_area(&m_VcpuLoad, VCPU_DATA_MEMORY_SIZE, XP70Iram_p,
                            VCPU_PROGRAM_MEMORY_SIZE);

    XP70DataMemoryPhyAddr =  m_VcpuLoad.ulPhysical;
    XP70ProgramMemoryPhyAddr = m_VcpuLoad.ulPhysical + VCPU_DATA_MEMORY_SIZE ;

    if ((status = VCPU_Reset()) != FVDP_OK)
    {
        TrVCPU(DBG_ERROR, "Error: VCPU reset time-out\n");
        vibe_os_free_dma_area(&m_VcpuLoad);
        return status;
    }

    vcpu_InitPlugCtrl();

    status = STREAMER_Tx(XP70DataMemoryPhyAddr,
                         TCDM_BASE_ADDRESS, VCPU_DATA_MEMORY_SIZE);
    if (status != FVDP_OK)
    {
        TrVCPU(DBG_ERROR, "Error: data loading time-out\n");
        vibe_os_free_dma_area(&m_VcpuLoad);
        return status;
    }

    status = STREAMER_Tx(XP70ProgramMemoryPhyAddr,
                         TCPM_BASE_ADDRESS, VCPU_PROGRAM_MEMORY_SIZE);
    if (status != FVDP_OK)
    {
        TrVCPU(DBG_ERROR, "Error: code loading time-out\n");
        vibe_os_free_dma_area(&m_VcpuLoad);
        return status;
    }

    vibe_os_free_dma_area(&m_VcpuLoad);

#ifndef FVDP_SIMULATION
    vibe_os_allocate_dma_area(&m_VcdLog, sizeof(VCDLog_t), 32, SDAAF_UNCACHED);
    if(!m_VcdLog.pMemory)
    {
        TrVCPU(DBG_ERROR, "Failed to allocate VCD log memory\n");
        vibe_os_free_dma_area(&m_VcdLog);
        return FVDP_MEM_INIT_ERROR;
    }
#endif

    FVDP_RegLog_Init();

    VcpuInitialized = TRUE;

    return status;
}

/*******************************************************************************
Name        : VCPU_Start
Description : Start VCPU instruction fetching
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VCPU_Start(void)
{
    uint32_t TimeCount = 0;

    /* check that VCPU is already initialized */
    if (VcpuInitialized == FALSE)
    {
        TrVCPU(DBG_ERROR, "not initialized\n");
        return FALSE;
    }

    /* check that VCPU has not already started */
    if (VcpuStarted == TRUE)
    {
        TrVCPU(DBG_ERROR, "already started\n");
        return FALSE;
    }

    TrVCPU(DBG_INFO, "enabling fetch...\n");

    /* Start VCPU */
    MBOX_REG32_Write(OUTBOX_CTRL0_REG,0);
    MBOX_REG32_ClearAndSet(STARTUP_CTRL_1, FETCH_EN_MASK, FETCH_EN_OFFSET, 1);

    while (MBOX_REG32_Read(OUTBOX_CTRL0_REG) != MBOX_ACK)
    {
#if defined(FVDP_SIMULATION)
        TimeCount++;
#else
        if (++ TimeCount > WAIT_FOR_VCPU_INIT_TIME)
        {
            TrVCPU(DBG_ERROR, "!!!!! TIMED-OUT !!!!!\n");
            REG32_Clear(STARTUP_CTRL_1, FETCH_EN_MASK);
            return FVDP_ERROR;
        }
#endif
    }

    TrVCPU(DBG_INFO, "started (in %d counts)\n", TimeCount);

    VcpuStarted = TRUE;

    return FVDP_OK;
}

/*******************************************************************************
Name        : VCPU_Term
Description : Reset the VCPU FW and HW
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VCPU_Term( void)
{
    MBOX_REG32_Write( IT_CONTROL,0);
    MBOX_REG32_Clear( STARTUP_CTRL_1, FETCH_EN_MASK);
    MBOX_REG32_SetField( SW_RESET_CTRL, SW_RESET_FULL_VCPU, 1);
    TrVCPU(DBG_INFO, "VCPU fetch disable and termination. \n");

#ifndef FVDP_SIMULATION
    vibe_os_free_dma_area(&m_VcdLog);
#endif

    VcpuStarted = FALSE;
    VcpuInitialized = FALSE;

    return FVDP_OK;
}

/*******************************************************************************
Name        : VCPU_Reset
Description : VCPU SW reset, all VCPU registers return to POD state.
              VCPU code and data need to be reloaded.
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VCPU_Reset (void)
{
    uint8_t Busy = MBX_BIT_CLEARED;
    uint32_t TimeCount = 0;

    TrVCPU(DBG_INFO, "resetting...\n");

    MBOX_REG32_Write( IT_CONTROL,0);
    MBOX_REG32_Clear( STARTUP_CTRL_1, FETCH_EN_MASK);
    MBOX_REG32_Write( SW_RESET_CTRL, SW_RESET_FULL_VCPU_MASK);    // SW reset

    do
    {
        Busy = MBOX_REG32_Read( STARTUP_CTRL_0) & VCPU_RESET_DONE_MASK;
        if (++ TimeCount > WAIT_FOR_VCPU_INIT_TIME)
        {
            TrVCPU(DBG_ERROR, "!!!!! TIMED-OUT !!!!!\n");
            return FVDP_ERROR;
        }
    }
    while (Busy == MBX_BIT_CLEARED);

    TrVCPU(DBG_INFO, "reset done (in %d counts)\n", TimeCount);

    VcpuStarted = FALSE;
    VcpuInitialized = FALSE;

    return FVDP_OK;
}

/*******************************************************************************
Name        : bool VCPU_MemCompare(uint32_t* Addr1_p, uint32_t* Addr2_p,
                                   uint32_t Size, uint32_t* chksum_p)
Description : Compare memory contents and produce checksum
Parameters  :
Assumptions :
Limitations :
Returns     : ST_ErrorCode_t
*******************************************************************************/
bool VCPU_MemCompare(uint32_t* Addr1_p, uint32_t* Addr2_p,
                     uint32_t Size, uint32_t* chksum_p)
{
    uint32_t i;

    for (i = 0; i < Size; i++, Addr1_p++, Addr2_p++)
    {
        *chksum_p += *Addr1_p;
        if ( *Addr1_p != *Addr2_p )
        {
            TrVCPU(DBG_INFO, "failed at count:0x%X, "
                "got value:0x%X, original value:0x%X\n",
                i, *Addr1_p, *Addr2_p);
            return FALSE;
        }
    }
    return TRUE;
}

/*******************************************************************************
Name        : FVDP_Result_t VCPU_CheckMemory( void)
Description : Check VCPU code/data loading
Parameters  : none
Assumptions :
Limitations :
Returns     : ST_ErrorCode_t
*******************************************************************************/
FVDP_Result_t VCPU_CheckLoading(const uint32_t* const XP70Dram_p,
                                const uint32_t* const XP70Iram_p,
                                uint32_t* chksum_p)
{
    FVDP_Result_t   status = FVDP_OK;
    DMA_Area        m_VcpuLoad;

    if(!XP70Dram_p || !XP70Iram_p)
        return FVDP_ERROR;

    vcpu_InitPlugCtrl();
    *chksum_p = 0;

    // check data memory area

    vibe_os_allocate_dma_area(&m_VcpuLoad, VCPU_DATA_MEMORY_SIZE, 32, SDAAF_UNCACHED);

#ifndef FVDP_SIMULATION
    status = STREAMER_Rx(m_VcpuLoad.ulPhysical, TCDM_BASE_ADDRESS, VCPU_DATA_MEMORY_SIZE);
    if (status != FVDP_OK)
    {
        vibe_os_free_dma_area(&m_VcpuLoad);
        return status;
    }
#else
    m_VcpuLoad.pData = (void*) XP70Dram_p;
#endif

    if (!VCPU_MemCompare((uint32_t*) m_VcpuLoad.pData, (uint32_t*) XP70Dram_p,
                         VCPU_DATA_MEMORY_SIZE/WORD_SIZE, chksum_p))
    {
        vibe_os_free_dma_area(&m_VcpuLoad);
        return FVDP_ERROR;
    }

    vibe_os_free_dma_area(&m_VcpuLoad);

    // check program memory area

    vibe_os_allocate_dma_area(&m_VcpuLoad, VCPU_PROGRAM_MEMORY_SIZE, 32, SDAAF_UNCACHED);

#ifndef FVDP_SIMULATION
    status = STREAMER_Rx(m_VcpuLoad.ulPhysical, TCPM_BASE_ADDRESS, VCPU_PROGRAM_MEMORY_SIZE);
    if (status != FVDP_OK)
    {
        vibe_os_free_dma_area(&m_VcpuLoad);
        return status;
    }
#else
    m_VcpuLoad.pData = (void*) XP70Iram_p;
#endif

    /* code mem check */
    if (!VCPU_MemCompare((uint32_t*) m_VcpuLoad.pData, (uint32_t*) XP70Iram_p,
                         VCPU_PROGRAM_MEMORY_SIZE/WORD_SIZE, chksum_p))
    {
        vibe_os_free_dma_area(&m_VcpuLoad);
        return FVDP_ERROR;
    }

    *chksum_p = ~(*chksum_p);

    vibe_os_free_dma_area(&m_VcpuLoad);
    return status;
}

/*******************************************************************************
Name        : bool VCPU_Initialized(void)
Description : Check that VCPU has already been initialized
Parameters  : none
Assumptions :
Limitations :
Returns     : bool - TRUE if initialized
*******************************************************************************/
bool VCPU_Initialized(void)
{
    return VcpuInitialized;
}

/*******************************************************************************
Name        : bool VCPU_Started(void)
Description : Check that VCPU has already started
Parameters  : none
Assumptions :
Limitations :
Returns     : bool - TRUE if started
*******************************************************************************/
bool VCPU_Started(void)
{
    return VcpuStarted;
}

#ifdef USE_VCPU
#if (VCD_EXTENDED_LOG_CONTROLS == FALSE)
#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintVCDFormatHeader(void)
{
    TrVCPU(DBG_INFO, "$version VCD FVDP Frame Buffer Management debugger $end\n");
    TrVCPU(DBG_INFO, "$timescale 10 us $end\n");

    TrVCPU(DBG_INFO, "$scope module FBMInputCurrln $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  a ProcessingType $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  b InputFrameID $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  c InputScanType $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  d InputFieldType $end\n");
    TrVCPU(DBG_INFO, "$var integer 16 e WriteMemWidth $end\n");
    TrVCPU(DBG_INFO, "$var integer 16 f WriteMemHeight $end\n");
    TrVCPU(DBG_INFO, "$var integer 16 g InputFrameRate $end\n");
    TrVCPU(DBG_INFO, "$var integer 16 h OutputFrameRate $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  i Input3DFormat $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  j Input3DFlag $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  k Output3DFormat $end\n");
    TrVCPU(DBG_INFO, "$var integer 8  l OutputScanType $end\n");
    TrVCPU(DBG_INFO, "$upscope $end\n");

    TrVCPU(DBG_INFO, "$enddefinitions $end\n");


}
#endif

#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMInputCurrIn( FBMDebugData_t* ptr)
{
    InputFrameData_t  *InData_p = ( InputFrameData_t  *) &(ptr->Data);
    TrVCPU(DBG_INFO, "#%d\n", (uint32_t)(ptr->TimeStamp / 3525));

    TrVCPU(DBG_INFO, "b%b a\n", InData_p->ProcessingType);
    TrVCPU(DBG_INFO, "b%b b\n", InData_p->InputFrameID);
    TrVCPU(DBG_INFO, "b%b c\n", InData_p->InputScanType);
    TrVCPU(DBG_INFO, "b%b d\n", InData_p->InputFieldType);
    TrVCPU(DBG_INFO, "b%b e\n", InData_p->WriteMemWidth);
    TrVCPU(DBG_INFO, "b%b f\n", InData_p->WriteMemHeight);
    TrVCPU(DBG_INFO, "b%b g\n", InData_p->InputFrameRate);
    TrVCPU(DBG_INFO, "b%b h\n", InData_p->OutputFrameRate);
    TrVCPU(DBG_INFO, "b%b i\n", InData_p->Input3DFormat);
    TrVCPU(DBG_INFO, "b%b j\n", InData_p->Input3DFlag);
    TrVCPU(DBG_INFO, "b%b k\n", InData_p->Output3DFormat);
    TrVCPU(DBG_INFO, "b%b l\n", InData_p->OutputScanType);

}
#else
static void DebugPrintFBMInputCurrIn( FBMDebugData_t* ptr)
{
    InputFrameData_t  *InData_p = ( InputFrameData_t  *) &(ptr->Data);
    TrVCPU(DBG_INFO, " InCurrIn %d:", ptr->TimeStamp);

    TrVCPU(DBG_INFO, " %d %d %d %d %d %d %d %d %d %d\n",
                          InData_p->ProcessingType,
                          InData_p->InputFrameID,
                          InData_p->InputScanType,
                          InData_p->InputFieldType,
                          InData_p->InputFrameRate,
                          InData_p->OutputFrameRate,
                          InData_p->Input3DFormat,
                          InData_p->Input3DFlag,
                          InData_p->Output3DFormat,
                          InData_p->OutputScanType);

}
#endif


#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMInputCurrOut(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMInputCurrOut(  FBMDebugData_t* ptr)
{
/* To update for new data structure definition:
    FrameBufferIndices_t  *OutData_p = ( FrameBufferIndices_t  *) &(ptr->Data);

    TrVCPU(DBG_INFO, " InCurrOut %d:", ptr->TimeStamp);

    TrVCPU(DBG_INFO, " cId:%d cImI:%d cMVI:%d cMSI:%d cRawI:%d p1Id:%d p1ImI:%d p1MSI:%d p2Id:%d p2ImI:%d p2RawI:%d p1afm:%d\n",
                          OutData_p->InCurrFrameID,
                          OutData_p->InCurrIndices.ImageBufferIndex,
                          OutData_p->InCurrIndices.MADiMVBufferIndex,
                          OutData_p->InCurrIndices.MADiStaticBufferIndex,
                          OutData_p->InCurrIndices.UVRawBufferIndex,
                          OutData_p->InP1FrameID,
                          OutData_p->InP1BufferIndices.ImageBufferIndex,
                          OutData_p->InP1BufferIndices.MADiStaticBufferIndex,
                          OutData_p->InP2FrameID,
                          OutData_p->InP2BufferIndices.ImageBufferIndex,
                          OutData_p->InP2BufferIndices.UVRawBufferIndex,
                          OutData_p->P1AFMState);
*/
}
#endif


#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMInputPrevIn(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMInputPrevIn( FBMDebugData_t* ptr)
{
    FBMDebugInputPrevData_t  *InputPrev_p = ( FBMDebugInputPrevData_t  *) &(ptr->Data);
    TrVCPU(DBG_INFO, " InPrevIn %d:", ptr->TimeStamp);

    TrVCPU(DBG_INFO, " %d %d %d %d %d %d\n",
                          InputPrev_p->RVal,
                          InputPrev_p->SVal,
                          InputPrev_p->Afp1Mode,
                          InputPrev_p->Afp1Phase,
                          InputPrev_p->Afp2Mode,
                          InputPrev_p->Afp2Phase);
}
#endif

#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMInputPrevOut(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#elif 0
static void DebugPrintFBMInputPrevOut( FBMDebugData_t* ptr)
{
    TrVCPU(DBG_INFO, " InPrevOut %d: \n", ptr->TimeStamp);
}
#endif


#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMInputFrameReset(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMInputFrameReset( FBMDebugData_t* ptr)
{
    TrVCPU(DBG_INFO, " InFR %d: \n", ptr->TimeStamp);
}
#endif

#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMOutputIn(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMOutputIn( FBMDebugData_t* ptr)
{
    TrVCPU(DBG_INFO, " OutputIn %d: \n", ptr->TimeStamp);
}
#endif


#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMOutputOut(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMOutputOut( FBMDebugData_t* ptr)
{
/* To update for new data structure definition:
    OutputFrameData_t  *OutData_p = ( OutputFrameData_t  *) &(ptr->Data);
    TrVCPU(DBG_INFO, " OutputOut %d:", ptr->TimeStamp);

    TrVCPU(DBG_INFO, " o1:%d_%d o2:%d_%d o3:%d_%d o4:%d_%d interp:%d " \
                     "3dlr:%d ipol:%d " \
                     "ff1:%d ff2:%d ff3:%d ff4:%d ff5:%d ff6:%d " \
                     "mvf:%d mvb:%d\n",
                    OutData_p->OutFrameID[0], OutData_p->OutPolarity[0],
                    OutData_p->OutFrameID[1], OutData_p->OutPolarity[1],
                    OutData_p->OutFrameID[2], OutData_p->OutPolarity[2],
                    OutData_p->OutFrameID[3], OutData_p->OutPolarity[3],
                    OutData_p->InterpolationRatio,
                    OutData_p->ThreeDLRIndex,
                    OutData_p->InterlacedOutPolarity,
                    OutData_p->FreeFrameID[0], OutData_p->FreeFrameID[1],
                    OutData_p->FreeFrameID[2], OutData_p->FreeFrameID[3],
                    OutData_p->FreeFrameID[4], OutData_p->FreeFrameID[5],
                    OutData_p->MVFPBufferIndex, OutData_p->MVBPBufferIndex);
*/
}
#endif

#ifdef DEBUG_OUTPUT_IN_VCD_FORMAT
static void DebugPrintFBMOutputFrameReset(  FBMDebugData_t* ptr)
{
    ptr = ptr;
}
#else
static void DebugPrintFBMOutputFrameReset( FBMDebugData_t* ptr)
{
    TrVCPU(DBG_INFO, " OutFR %d: \n", ptr->TimeStamp);
}
#endif // DEBUG_OUTPUT_IN_VCD_FORMAT
#endif // (VCD_EXTENDED_LOG_CONTROLS == FALSE)

#if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
#define LOG_ROW_SIZE        sizeof(VCDVAlBufEntry_t)
#define LOG_LENGTH          VCD_LOG_BUFFER_LENGTH
#define PRINT_LOG_BUFFER    vcpu_PrintVCDLogBuffer
#else
#define LOG_ROW_SIZE        sizeof(FBMDebugData_t)
#define LOG_LENGTH          NUM_FBM_LOG_ELEMENTS
#define PRINT_LOG_BUFFER    vcpu_PrintFBMLogBuffer
#endif

#ifdef FVDP_SIMULATION
/*******************************************************************************
Name        : static FVDP_Result_t vcpu_DownloadLogShmem(void)
Description : Download VCD data via shmem
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
static FVDP_Result_t vcpu_DownloadLogShmem(void *Log_p)
{
    memcpy(Log_p, Linux_Sim_GetVCDLogShmemAddr(), sizeof(VCDLog_t));
    return FVDP_OK;
}
#endif

#if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
/*******************************************************************************
Name        : VCPU_DownloadVCDLog
Description : Download VCD data from the VCPU, and return the address and size
              of the destination buffer (on the HOST-side)
Parameters  : log_pp [out] - pointer to type VCDLog_t* where the log is stored
              size_p [out] - pointer to uint32_t
Assumptions : m_VcdLog is initialized and allocated for
Limitations :
Returns     : FVDP_OK if successful
*******************************************************************************/
FVDP_Result_t VCPU_DownloadVCDLog(VCDLog_t** log_pp, uint32_t* size_p)
{
    FVDP_Result_t status    = FVDP_OK;
#ifndef FVDP_SIMULATION
    uint32_t vcpu_src_addr  = REG32_Read(DEBUG_LOG_DATA_OFFSET_ADDR);
    uint32_t host_dest_addr = m_VcdLog.ulPhysical;
    uint32_t header_size    = offsetof(VCDLog_t,initialized);
    uint32_t full_size      = 0;

    *log_pp = NULL;
    if (size_p) *size_p = 0;

    if (vcpu_src_addr == 0)
        return FVDP_ERROR;

    vcpu_InitPlugCtrl();

    // first, get only header information
    status = STREAMER_Rx(host_dest_addr, vcpu_src_addr, header_size);
    if (status != FVDP_OK)
        return status;

    // get virtual address of download location into *log_pp
    *log_pp = (VCDLog_t*) m_VcdLog.pData;

    // determine full size of VCDLog_t
    full_size = offsetof(VCDLog_t,buffer) + (*log_pp)->buf_length * sizeof(VCDVAlEntry_t);

    // now, download complete VCDLog_t from VCPU
    status = STREAMER_Rx(host_dest_addr, vcpu_src_addr, full_size);

    if (size_p != NULL)
        *size_p = full_size;
#endif  // FVDP_SIMULATION

    return status;
}

/*******************************************************************************
Name        : FVDP_Result_t vcpu_PrintVCDLogBuffer(void)
Description : Print FBM log
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t vcpu_PrintVCDLogBuffer(LogPrintSelect_t LogSelect)
{
    FVDP_Result_t   status = FVDP_OK;
    uint32_t LogSource;

    LogSource = REG32_Read(DEBUG_LOG_SOURCE_SELECT) & 0x0F;

#if (VCD_FILE_IO_SUPPORT == TRUE)
    FILE*           stream = stderr;

    if (LogSource == 1)
        stream = fopen("vcpu_log.vcd","w+");
    else
        stream = fopen("arm_log.vcd","w+");

    if (stream == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "file open error" );
        return FVDP_ERROR;
    }
    VCD_SetOutputFile(stream);
#endif

#ifndef FVDP_SIMULATION
    if (LogSource == VCD_VCPU)
    {
        FVDP_Result_t status;
        VCDLog_t* Log_p;

        status = VCPU_DownloadVCDLog(&Log_p, NULL);
        if (status == FVDP_OK && Log_p != NULL)
        {
            VCD_PrintLogBuffer(Log_p);
        }
        else
        {
            TRC( TRC_ID_MAIN_INFO, "Error in transferring log via streamer" );
        }
    }
    else if(LogSource == 0)
    {
        VCD_PrintLogBuffer(&vcd_log);
    }
#else
    if (LogSource == 1)
    {
        VCDLog_t vcd_log;

        // download data
        if (vcpu_DownloadLogShmem(&vcd_log) != FVDP_OK)
        {
            return FVDP_ERROR;
        }
        VCD_PrintLogBuffer(&vcd_log);
    }
    else if(LogSource == 0)
    {
        VCD_PrintLogBuffer(&vcd_log);
    }
#endif

#if (VCD_FILE_IO_SUPPORT == TRUE)
    fclose(stream);
#endif

    return status;
}

#else
/*******************************************************************************
Name        : FVDP_Result_t vcpu_PrintFBMLogBuffer(void)
Description : Print FBM log
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t vcpu_PrintFBMLogBuffer(LogPrintSelect_t LogSelect)
{
    uint32_t i;
    FBMDebugData_t (*FBMLog_p)[NUM_FBM_LOG_ELEMENTS];
    FVDP_Result_t status = FVDP_ERROR;
    FBMDebugData_t* ptr = NULL;

    FBMLog_p = vibe_os_allocate_memory(sizeof(*FBMLog_p));
    if (!FBMLog_p)
    {
        TrVCPU(DBG_ERROR, "*** unable to allocate memory for log ***");
        return FVDP_MEM_INIT_ERROR;
    }

    // download data
    if (vcpu_DownloadLog(FBMLog_p) != FVDP_OK)
    {
        vibe_os_free_memory(FBMLog_p);
        return FVDP_ERROR;
    }

    for ( i = 0; i < NUM_FBM_LOG_ELEMENTS; i++)
    {
        //TrVCPU(DBG_INFO, " e:%d ", FBMLog[i].LogEvent);
        //TrVCPU(DBG_INFO, " %d\n", FBMLog[i].TimeStamp);

        //ptr = (uint8_t *) (FBMLog_p + i * sizeof(FBMDebugData_t) );
        ptr = &((*FBMLog_p)[i]);

        if ( ptr->LogEvent == FBM_LOG_INPUT_CURR_IN)
        {
            if ( LogSelect != LOG_PRINT_OUTPUT)
                DebugPrintFBMInputCurrIn(ptr);
        }
        else if ( ptr->LogEvent == FBM_LOG_INPUT_CURR_OUT)
        {
            if ( LogSelect != LOG_PRINT_INPUT)
                DebugPrintFBMInputCurrOut(ptr);
        }
        else if ( ptr->LogEvent == FBM_LOG_INPUT_PREV)
        {
            if ( LogSelect != LOG_PRINT_OUTPUT)
                DebugPrintFBMInputPrevIn(ptr);
            //if ( LogSelect != LOG_PRINT_INPUT)
            //  DebugPrintFBMInputPrevOut(ptr);
        }
        else if ( ptr->LogEvent == FBM_LOG_OUTPUT)
        {
            if ( LogSelect != LOG_PRINT_OUTPUT)
                DebugPrintFBMOutputIn(ptr);
            if ( LogSelect != LOG_PRINT_INPUT)
                DebugPrintFBMOutputOut(ptr);
        }
        else if ( ptr->LogEvent == FBM_LOG_INPUT_FRAME_RESET)
        {
            if ( LogSelect != LOG_PRINT_OUTPUT)
                DebugPrintFBMInputFrameReset(ptr);
        }
        else if ( ptr->LogEvent == FBM_LOG_OUTPUT_FRAME_RESET)
        {
            if ( LogSelect != LOG_PRINT_OUTPUT)
                DebugPrintFBMOutputFrameReset(ptr);
        }
    }

    vibe_os_free_memory(FBMLog_p);

    return status;
}
#endif
#endif // USE_VCPU

/*******************************************************************************
Name        : FVDP_Result_t VCPU_LogPrintControl(void)
Description : This routine (a) captures the debug log (in either VCD format or
              legacy debuglog format from the VCPU, and (b) prints it.
              The capture/print mechanism is controlled by DEBUG_LOG_CTRL,
              which is mapped to a SPARE register.
Parameters  : none
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t VCPU_LogPrintControl(void)
{
    FVDP_Result_t status = FVDP_OK;
#ifdef USE_VCPU
    static uint32_t     prevLogCtrl = 0;
    uint8_t             LogCtrl;
    uint32_t            TrigSelect;

    LogCtrl    = REG32_Read(DEBUG_LOG_CTRL);
    TrigSelect = REG32_Read(DEBUG_LOG_TRIG_SELECT);

    if (LogCtrl != prevLogCtrl)
    {
        prevLogCtrl = LogCtrl;

        switch (LogCtrl)
        {
        case LOGMODE_STOP:
            TrVCPU(DBG_INFO, "[----- vcpu log STOP -----]\n");
            break;
        case LOGMODE_AUTO:
            TrVCPU(DBG_INFO, "[----- vcpu log trig AUTO -----]\n");
            break;
        case LOGMODE_TRIG:
            TrVCPU(DBG_INFO, "[----- vcpu log trig on event %d -----]\n",
                   TrigSelect);
            break;
        case LOGMODE_SINGLE:
            TrVCPU(DBG_INFO, "[----- vcpu log trig SINGLE -----]\n");
            break;
        case LOGMODE_PRINT:
            TrVCPU(DBG_INFO, "[----- vcpu log print requested -----]\n");
            REG32_Write(DEBUG_LOG_CTRL, LOGMODE_STOP);
            PRINT_LOG_BUFFER(LOG_PRINT_ALL);
            break;
        }
    }
#endif
    return status;
}

