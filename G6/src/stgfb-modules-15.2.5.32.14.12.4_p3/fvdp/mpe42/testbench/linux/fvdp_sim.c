/***********************************************************************
 *
 * File: fvdp_sim.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*  NOTES:
 *  This is a stand-alone compilation for simulation purposes only, and
 *  is not intended for release.
 */

/* Includes ----------------------------------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <pthread.h>
#include "vibe_debug.h"
#include "fvdp_private.h"
#include "fvdp.h"
#if defined(CONFIG_MPE42)
#include "mpe42/fvdp_router.h"
#include "mpe42/fvdp_common.h"
#include "mpe42/fvdp_regs.h"
#include "mpe42/fvdp_vcpu.h"
#include "mpe42/fvdp_fbm.h"
#include "mpe42/fvdp_driver.h"
#else
#error "Only MPE42 hardware supprted"
#endif
#include "fvdp_revision.h"
#include <readline/readline.h>  // needs "yum install readline-devel"
#include <readline/history.h>

#define DEVICE_BASE_ADDRESS 0xFD000000

#if defined(CONFIG_MPE42)
#define FIRST_FVDP_REG      (MDTP0_BASE_ADDRESS + MDTP_CTRL)
#define LAST_FVDP_REG       (BDR_ENC_BASE_ADDRESS)
#else
#define FIRST_FVDP_REG      (BR0EA)
#define LAST_FVDP_REG       (VPI_TST_PRB_CTL)
#endif

#define REG_BASE_SIZE       (LAST_FVDP_REG - FIRST_FVDP_REG + 1)
#define START_REG_ADDR      (FIRST_FVDP_REG + gRegisterBaseAddress)
#define END_REG_ADDR        (LAST_FVDP_REG + gRegisterBaseAddress)

#define DEBUG_INFO_START            REG_BASE_SIZE
#define DEBUG_INFO_SIZE             32
#define DEBUG_VCD_LOG_START         DEBUG_INFO_START + DEBUG_INFO_SIZE
#define DEBUG_VCD_LOG_SIZE          131072L
#define DEBUG_LOCK                  0
#define DEBUG_INDEX                 4
#define DEBUG_SIM_TIME              8
#define DEBUG_LEVEL_REG             12
#define DEBUG_PRINT_VCPU_STATUS     16

#define SHMEM_SIZE                  (REG_BASE_SIZE + DEBUG_INFO_SIZE + DEBUG_VCD_LOG_SIZE)

#define MAX_LENGTH_ARG 255
#define MAX_LENGTH_FCT 128

extern uint32_t gRegisterBaseAddress;
volatile uint32_t* regbase=0;
volatile uint32_t* pRegDebugLevel;

FVDP_CH_Handle_t  ChHandle[NUM_FVDP_CH];
FVDP_VideoInfo_t  CurrentInputVideoInfo[NUM_FVDP_CH];
FVDP_RasterInfo_t CurrentRasterInfo[NUM_FVDP_CH];
FVDP_CropWindow_t CurrentCropWindow[NUM_FVDP_CH];
FVDP_VideoInfo_t  CurrentOutputVideoInfo[NUM_FVDP_CH];

int shmid = -1;
void *shared_memory = (void *)0;
int num_pids=0;
int pid[8];

unsigned int trcmask[2] = { ( 1 << TRC_ID_BY_CONSOLE ) | ( 1 << TRC_ID_ERROR ) | ( 1 << TRC_ID_MAIN_INFO ), 0 };

void* setup_shared_mem(int key, uint32_t size)
{
    shmid = shmget((key_t)key, size, 0666 |
                    IPC_CREAT); // | IPC_EXCL);

    if (shmid == -1) {
        perror("shmget failed\n");
        exit(-1);
    }

    shared_memory = shmat(shmid, (void *)0, 0);
    if (shared_memory == (void *)-1) {
        perror("shmat failed\n");
        exit(-1);
    }

    // Good idea to initialize shared memory
    memset(shared_memory, 0, size);

    return (void*) shared_memory;
}

void free_resources (void* memory)
{
    if (shmdt(memory) == -1) {
        perror("shmdt failed\n");
    }
}

void  ZeroMemory(void* pMem, uint32_t count)
{
    memset((char*)pMem,0,count);
}

void* AllocateMemory(uint32_t size)
{
    char* buf;
    buf = malloc(size);
    return (buf);
}

void  AllocateDMAArea(DMA_Area *mem, uint32_t size, uint32_t alignment, STM_DMA_AREA_ALLOC_FLAGS flags)
{
    mem->pMemory = (void*) 1;
}

void  FreeMemory(void* mem)
{
    free(mem);
}

void  FreeDMAArea(DMA_Area *mem)
{
}

uint32_t ReadRegister(volatile uint32_t*  reg)
{
    uint32_t start_addr = START_REG_ADDR;
    uint32_t end_addr = END_REG_ADDR;
    uint32_t ret_val;

    if (reg < (uint32_t*)start_addr || reg > (uint32_t*)end_addr)
    {
        printf("BAD MEMORY WRITE ADDRESS 0x%x\n", (uint32_t)reg);
        while (1);  // spin
    }
#if defined(CONFIG_MPE42)
    if ((uint32_t) reg == FVDP_STREAMER_BASE_ADDRESS + BSF + gRegisterBaseAddress ||
        (uint32_t) reg == FVDP_MAILBOX_BASE_ADDRESS + STARTUP_CTRL_0 + gRegisterBaseAddress)
#else
    // animate status fields of BSF register or IRQ_TO_HOST
    if ((uint32_t) reg == BSF + gRegisterBaseAddress ||
        //(uint32_t) reg == IRQ_TO_HOST + gRegisterBaseAddress ||
        (uint32_t) reg == STARTUP_CTRL_0 + gRegisterBaseAddress ||
        (uint32_t) reg == MAIN_STATUS + gRegisterBaseAddress ||
        (uint32_t) reg == PIP_STATUS + gRegisterBaseAddress ||
        (uint32_t) reg == VCR_STATUS + gRegisterBaseAddress)
#endif
    {
        uint8_t val;
        val = *((volatile uint32_t*)((uint32_t)regbase + ((uint32_t)reg-START_REG_ADDR)));
        val ^= ~0;  // just flip all bits
        *((volatile uint32_t*)((uint32_t)regbase + ((uint32_t)reg-START_REG_ADDR))) = val;
    }

    ret_val = *((volatile uint32_t*)((uint32_t)regbase + ((uint32_t)reg-START_REG_ADDR)));
    return ret_val;
}

void DebugLock(void)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_LOCK;
    while (*((volatile uint32_t*) ((uint32_t)regbase + reg_offset)) == 0xA5);
    *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) ) = 0xA5;
}

void DebugUnlock(void)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_LOCK;
    *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) ) = 0;
}

uint32_t GetDebugIndex(void)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_INDEX;
    uint32_t idx = *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) );
    *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) ) = ++idx;
    return idx;
}

void Linux_Sim_SetTime(uint32_t sim_time)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_SIM_TIME;
    *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) ) = sim_time;
}

uint32_t Linux_Sim_GetTime(void)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_SIM_TIME;
    return *( (volatile uint32_t*) ((uint32_t)regbase + reg_offset) );
}

void Linux_Sim_SetPrintVcpuStatusRequest(void)
{
    uint32_t reg_offset = DEBUG_INFO_START + DEBUG_PRINT_VCPU_STATUS;
    uint32_t* pStatus   = (uint32_t*) ((uint32_t)regbase + reg_offset);
    *pStatus = 1;
}

void* Linux_Sim_GetVCDLogShmemAddr(void)
{
    uint32_t reg_offset = DEBUG_VCD_LOG_START;
    void*    pVCDLog    = (uint32_t*) ((uint32_t)regbase + reg_offset);
    return   pVCDLog;
}

uint32_t BreakOnRegWrite_addr = 0;
uint32_t BreakOnRegWrite_val = 0;

#define READ_REG(addr)       *((volatile uint32_t*)((uint32_t)regbase + ((uint32_t)(addr)-START_REG_ADDR)))
#define WRITE_REG(addr,val)  *((volatile uint32_t*)((uint32_t)regbase + ((uint32_t)(addr)-START_REG_ADDR))) = (val);

void WriteRegister(volatile uint32_t *reg, uint32_t  val)
{
    uint32_t start_addr = START_REG_ADDR;
    uint32_t end_addr = END_REG_ADDR;

    if (reg < (uint32_t*)start_addr || reg > (uint32_t*)end_addr)
    {
        printf("WARNING: BAD MEMORY WRITE ADDRESS 0x%x (IGNORED)\n", (uint32_t)reg);
        return;
    }

    while (     (uint32_t)reg == BreakOnRegWrite_addr
            &&  val == BreakOnRegWrite_val);    // spin

    DebugLock();

    if (*pRegDebugLevel != DBG_ERROR)
    {
        TRC( TRC_ID_MAIN_INFO, CPU_STRING " %u [0x%08X] := 0x%X", GetDebugIndex(), (uint32_t) reg, val );
    }

#if defined(CONFIG_MPE42)   // applies to MPE42 only, as interrupts are dependent on COMM_CLUST IRQ status
    if ((uint32_t) reg > gRegisterBaseAddress)
    {
        uint32_t rel_addr = (uint32_t) reg - gRegisterBaseAddress;
        // for these registers, reset bits that are set to 1
        if (    rel_addr == FVDP_MAILBOX_BASE_ADDRESS + IRQ_TO_HOST
            &&  (val & IT_TO_HOST_MASK)                             )
        {
            // set IRQ_mb_host (BIT23 of COMMCTRL_FE_IRQ_0_STATUS_A)
            WRITE_REG(COMM_CLUST_FE_BASE_ADDRESS + COMMCTRL_FE_IRQ_0_STATUS_A + gRegisterBaseAddress, BIT23);
        }
    }

    WRITE_REG(reg, val);

    val = READ_REG(COMM_CLUST_FE_BASE_ADDRESS + COMMCTRL_FE_IRQ_0_STATUS_A + gRegisterBaseAddress);

    DebugUnlock();

    val = READ_REG(COMM_CLUST_FE_BASE_ADDRESS + COMMCTRL_FE_IRQ_0_STATUS_A + gRegisterBaseAddress);
#else
    WRITE_REG(reg, val);
    DebugUnlock();
#endif
}

void  MemcpyToDMAArea(DMA_Area *mem, uint32_t offset, const void* pSrc, uint32_t count)
{
    // do nothing
}

FILE* log_stream = NULL;

void IDebugPrintf(const char *fmt,...)
{
    va_list argptr;
    va_start(argptr,fmt);
    vfprintf(stderr,fmt,argptr);
    if (log_stream)
        vfprintf(log_stream,fmt,argptr);

    va_end(argptr);
}

uint32_t vibe_os_read_register(volatile uint32_t *baseAddress, uint32_t offset)
{
    return ReadRegister((volatile uint32_t*)((uint32_t)baseAddress+(uint32_t)offset));
}

void vibe_os_write_register(volatile uint32_t *baseAddress, uint32_t offset, uint32_t val)
{
    WriteRegister((volatile uint32_t*)((uint32_t)baseAddress+(uint32_t)offset),val);
}

void * vibe_os_allocate_memory(uint32_t size)
{
    return AllocateMemory(size);
}

bool vibe_os_create_thread(VIBE_OS_ThreadFct_t fct, void *parameter, const char *name, const VIBE_OS_thread_settings *thread_settings, void **thread_desc)
{
    return FALSE;
}

uint32_t vibe_os_create_resource_lock(void)
{
    return 1;
}

void vibe_os_delete_resource_lock(uint32_t ulHandle)
{
}

void vibe_os_lock_resource(uint32_t ulHandle)
{
}

void vibe_os_unlock_resource(uint32_t ulHandle)
{
}

uint32_t vibe_os_create_semaphore(int initVal)
{
    return 0;
}

void vibe_os_delete_semaphore(uint32_t ulHandle)
{
}

int vibe_os_down_semaphore(uint32_t ulHandle)
{
    return 0;
}

void vibe_os_up_semaphore(uint32_t ulHandle)
{
}

void vibe_os_sleep_ms(uint32_t ms)
{
}

int vibe_os_stop_thread(void *thread_desc)
{
    return 0;
}

void vibe_os_free_memory(void *mem)
{
    FreeMemory(mem);
}

bool vibe_os_get_chip_version(uint32_t *major, uint32_t *minor)
{
    *major = 1;
    *minor = 0;
    return 1;
}

void vibe_os_allocate_dma_area(DMA_Area *mem, uint32_t size, uint32_t align, STM_DMA_AREA_ALLOC_FLAGS flags)
{
    AllocateDMAArea(mem,size,align,flags);
}

void vibe_os_free_dma_area(DMA_Area *mem)
{
    FreeDMAArea(mem);
}

void vibe_os_zero_memory(void* pMem, uint32_t count)
{
    memset(pMem, 0, count);
}

void vibe_os_memcpy_to_dma_area(DMA_Area *mem, uint32_t offset, const void* pSrc, uint32_t count)
{
    // do nothing
}

vibe_time64_t   vibe_os_get_system_time(void)
{
    return Linux_Sim_GetTime();
}

int  vibe_os_allocate_queue_event(VIBE_OS_WaitQueue_t *queue)
{
    return 0;
}

void vibe_os_release_queue_event(VIBE_OS_WaitQueue_t queue)
{
}

void * vibe_os_get_wait_queue_data(VIBE_OS_WaitQueue_t queue)
{
    return NULL;
}

void vibe_os_wake_up_queue_event(VIBE_OS_WaitQueue_t queue)
{
}

int vibe_os_wait_queue_event(VIBE_OS_WaitQueue_t queue,
                        volatile uint32_t* var, uint32_t value,
                        int cond, unsigned int timeout_ms)
{
    return 0;
}

void vibe_os_enable_irq(unsigned int irq)
{
}

void vibe_os_disable_irq_nosync(unsigned int irq)
{
}

void vibe_os_disable_irq(unsigned int irq)
{
}

int32_t vibe_os_gpio_get_value(uint32_t gpio)
{
    return 0;
}

void vibe_printf(const char *fmt,...)
{
    va_list argptr;
    va_start(argptr,fmt);
    vfprintf(stderr,fmt,argptr);
    if (log_stream)
        vfprintf(log_stream,fmt,argptr);

    va_end(argptr);
}

void vibe_trace( int id, char c, const char *pf, const char *fmt, ... )
{
    char arg[MAX_LENGTH_ARG];
    char fct[MAX_LENGTH_FCT];
    int i, j;
    va_list ap;

    if ( ! ( trcmask[0] & (( 1 << TRC_ID_BY_CONSOLE ) | ( 1 << TRC_ID_BY_KPTRACE ) | ( 1 << TRC_ID_BY_STM )))) {
        return; /* no output selected */
    }

    fct[0] = 0;

    if ( c != ' ' ) { // not done for TRCBL
        /* get the function name (class name::function name in c++) from __PRETTY_FUNCTION__ */
        for ( i = 0, j = 0; i < MAX_LENGTH_FCT; i++ ) {
            if (( pf[i] == '(' ) || ( pf[i] == 0 )) { /* begin of parameters area */
                break;
            }
            else if (( pf[i] == ' ' ) || ( pf[i] == '*' )) { /* not still in function name */
                j = 0;
            }
            else {
                fct[j++] = pf[i];
            }
        }

        fct[j++] = ' '; /* add " -" at the end of function name */
        fct[j++] = '-';
        fct[j] = 0; /* terminate function name string */
    }

    va_start( ap, fmt );

    if ( trcmask[0] & ( 1 << TRC_ID_BY_CONSOLE )) {
        char color_in[11];
        char color_out[5];
        color_in[0] = 0;
        color_out[0] = 0;

        if ( id == TRC_ID_ERROR ) {
            snprintf( color_in, 11, "%c[%d;%dm", SET_COLOR, BOLD, RED );
            snprintf( color_out, 5, "%c[%dm", SET_COLOR, ATTR_OFF );
        }

        if (log_stream) {
            vsnprintf( arg, MAX_LENGTH_ARG, fmt, ap );
            /* no blank needed between fct and arg because fmt always begins with a blank */
            fprintf(log_stream, "vibe  %c %s%s%s%s\n", c, color_in, fct, arg, color_out );
        }
    }

    va_end( ap );
}

void DumpRegArea (FILE* stream, uint8_t columns,
                  void* regbase, uint32_t regbase_offset, uint32_t regbase_size)
{
    uint32_t i;

    for (i = 0; i < regbase_size/sizeof(uint32_t); i ++)
    {
        uint32_t val = *((uint32_t*) (regbase + regbase_offset - START_REG_ADDR) + i);
        if ((i % columns) == 0)
            fprintf(stream, "0x%08x:\t", regbase_offset+i*sizeof(uint32_t));
        fprintf(stream, "0x%08x",val);
        fprintf(stream, ((i % columns) == columns - 1) ? "\n" : "\t");
    }
    fprintf(stream, "\n");
}

static uint32_t timestamp = 0;
static bool user_quit = FALSE;

bool hex2dec (char* str, uint32_t* val)
{
    uint32_t ret = 0;
    char c;
    int v;

    if (!(*str == '0' && *(str+1) == 'x'))
    {
        *val = atoi(str);
        return TRUE;
    }

    str += 2;

    while ((c = *str++))
    {
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'A' && c <= 'F')
            v = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            v = c - 'a' + 10;
        else
        {
            return FALSE;
        }

        ret <<= 4;
        ret += v;
    }

    *val = ret;
    return TRUE;
}

// convert string to uint32 (dec and hex supported)
bool atoi32(char* str, uint32_t* val)
{
    if (*str == '0' && *(str+1) == 'x')
    {
        if (sscanf(str + 2, "%x", val) == 0)
            return FALSE;
    }
    else
    {
        if (sscanf(str, "%d", val) == 0)
            return FALSE;
    }

    return TRUE;
}

// get next white-space delimited argument
bool parse_GetToken(char** cmd_str, char* arg)
{
    char* s = *cmd_str;
    char c;

    while ((c = *(s++)) != '\0')
        if (!isspace(c)) break;
    if (c == '\0') return FALSE;

    *(arg++) = c;
    while ((c = *(s++)) != '\0' && !isspace(c))
        *(arg++) = c;

    *arg = '\0';
    *cmd_str = --s;
    return TRUE;
}

bool parse_GetIntegerToken(char** cmd_str, uint32_t* pIntVal)
{
    char token[80];
    bool result = parse_GetToken(cmd_str, token);
    if (result == TRUE)
       result = atoi32(token, pIntVal);
    return result;
}

// get next floating precision number
bool parse_GetFloat(char** cmd_str, double* arg)
{
    char* s = *cmd_str;
    char c = *s;

    while (c != '\0' && (isdigit(c) || c == '.')) c = *(++s);

    if (*cmd_str == s) return FALSE;

    *arg = atof(*cmd_str);

    *cmd_str = s;
    return TRUE;
}

typedef enum
{
    EVT_INPUT_UPDATE_CH_A,
    EVT_INPUT_FRAME_READY_CH_A,
    EVT_OUTPUT_UPDATE_CH_A,
    EVT_INPUT_UPDATE_CH_B,
    EVT_INPUT_FRAME_READY_CH_B,
    EVT_OUTPUT_UPDATE_CH_B,
    EVT_INPUT_UPDATE_CH_C,
    EVT_INPUT_FRAME_READY_CH_C,
    EVT_OUTPUT_UPDATE_CH_C,
    NUM_EVT
} SimEvent_t;

uint32_t CurrentTime = 0;
uint32_t NextEvtTime[NUM_EVT];
uint32_t EvtPeriod[NUM_EVT];

SimEvent_t InputUpdateEvent[NUM_FVDP_CH] = { EVT_INPUT_UPDATE_CH_A, EVT_INPUT_UPDATE_CH_B, EVT_INPUT_UPDATE_CH_C };
SimEvent_t InputFrameReadyEvent[NUM_FVDP_CH] = { EVT_INPUT_FRAME_READY_CH_A, EVT_INPUT_FRAME_READY_CH_B, EVT_INPUT_FRAME_READY_CH_C };
SimEvent_t OutputUpdateEvent[NUM_FVDP_CH] = { EVT_OUTPUT_UPDATE_CH_A, EVT_OUTPUT_UPDATE_CH_B, EVT_OUTPUT_UPDATE_CH_C };

void SimEvent_LoadEventPeriod(SimEvent_t event, uint32_t freq_mHz)
{
    // T(us) = (1/freq_mHz) * (1000mHz/Hz) * (10^6us/s) = 10^9/freq_mHz [us]
    EvtPeriod[event] = 1000000000L / freq_mHz;
}

uint32_t SimEvent_GetPeriod(SimEvent_t event)
{
    return EvtPeriod[event];
}

void SimEvent_DelayEvent(SimEvent_t event, uint32_t offset)
{
    NextEvtTime[event] += offset;
}

void SimEvent_FastForwardOtherEvents(SimEvent_t pivot)
{
    SimEvent_t evt;
    uint32_t pivot_proximity = NextEvtTime[pivot] - CurrentTime;

    for (evt = 0; evt < NUM_EVT; evt ++)
    {
        uint32_t proximity = NextEvtTime[evt] - CurrentTime;
        while (    EvtPeriod[evt] != 0
               &&  proximity < pivot_proximity )
        {
            NextEvtTime[evt] += EvtPeriod[evt];
            proximity = NextEvtTime[evt] - CurrentTime;
        }
    }
}

SimEvent_t SimEvent_FindClosestEvent(void)
{
    SimEvent_t simevt;
    SimEvent_t closest = NUM_EVT;
    uint32_t closest_proximity = ~0;

    for (simevt = 0; simevt < NUM_EVT; simevt++)
    {
        uint32_t proximity = NextEvtTime[simevt] - CurrentTime;

        if (    proximity < closest_proximity
            &&  EvtPeriod[simevt] != 0 )
        {
            closest = simevt;
            closest_proximity = proximity;
        }
    }
    return closest;
}

void SimEvent_GetNext(SimEvent_t* pEvent, uint32_t* pTime)
{
    *pEvent = SimEvent_FindClosestEvent();
    *pTime = NextEvtTime[*pEvent];
    CurrentTime = *pTime;
}

void mode_1080P (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = PROGRESSIVE;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = hdYUV;
    VideoInfo_p->ColorSampling = FVDP_420;
    VideoInfo_p->FrameRate = 60000;
    VideoInfo_p->Width = 1920;
    VideoInfo_p->Height = 1080;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_16_9;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 312;
    RasterInfo_p->VStart = 26;
    RasterInfo_p->HTotal = 2544;
    RasterInfo_p->VTotal = 1112;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_1080i (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = INTERLACED;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = hdYUV;
    VideoInfo_p->ColorSampling = FVDP_420;
    VideoInfo_p->FrameRate = 30000;
    VideoInfo_p->Width = 1920;
    VideoInfo_p->Height = 540;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_16_9;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 312;
    RasterInfo_p->VStart = 26;
    RasterInfo_p->HTotal = 2544;
    RasterInfo_p->VTotal = 556;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_720P (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = PROGRESSIVE;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = hdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 60000;
    VideoInfo_p->Width = 1280;
    VideoInfo_p->Height = 720;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_16_9;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 276;
    RasterInfo_p->VStart = 19;
    RasterInfo_p->HTotal = 1650;
    RasterInfo_p->VTotal = 750;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_480P (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = PROGRESSIVE;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = sdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 60000;
    VideoInfo_p->Width = 720;
    VideoInfo_p->Height = 480;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_4_3;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 130;
    RasterInfo_p->VStart = 18;
    RasterInfo_p->HTotal = 858;
    RasterInfo_p->VTotal = 525;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_576P (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = PROGRESSIVE;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = sdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 50000;
    VideoInfo_p->Width = 720;
    VideoInfo_p->Height = 576;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_4_3;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 130;
    RasterInfo_p->VStart = 18;
    RasterInfo_p->HTotal = 858;
    RasterInfo_p->VTotal = 625;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_480i (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = INTERLACED;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = sdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 30000;
    VideoInfo_p->Width = 720;
    VideoInfo_p->Height = 240;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_4_3;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 130;
    RasterInfo_p->VStart = 18;
    RasterInfo_p->HTotal = 858;
    RasterInfo_p->VTotal = 262;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_576i (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = INTERLACED;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = sdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 25000;
    VideoInfo_p->Width = 720;
    VideoInfo_p->Height = 288;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_4_3;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 130;
    RasterInfo_p->VStart = 18;
    RasterInfo_p->HTotal = 858;
    RasterInfo_p->VTotal = 312;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

void mode_XGA (FVDP_VideoInfo_t* VideoInfo_p, FVDP_RasterInfo_t* RasterInfo_p)
{
    VideoInfo_p->FrameID = timestamp;
    VideoInfo_p->ScanType = PROGRESSIVE;
    VideoInfo_p->FieldType = AUTO_FIELD_TYPE;
    VideoInfo_p->ColorSpace = sdYUV;
    VideoInfo_p->ColorSampling = FVDP_422;
    VideoInfo_p->FrameRate = 60000;
    VideoInfo_p->Width = 1024;
    VideoInfo_p->Height = 768;
    VideoInfo_p->AspectRatio = FVDP_ASPECT_RATIO_4_3;
    VideoInfo_p->FVDP3DFormat = FVDP_2D;
    VideoInfo_p->FVDP3DFlag = FVDP_AUTO3DFLAG;
    VideoInfo_p->FVDP3DSubsampleMode = FVDP_3D_NO_SUBSAMPLE;

    if (RasterInfo_p == NULL) return;

    RasterInfo_p->HStart = 10;
    RasterInfo_p->VStart = 10;
    RasterInfo_p->HTotal = 1152;
    RasterInfo_p->VTotal = 800;
    RasterInfo_p->UVAlign = 0;
    RasterInfo_p->FieldAlign = 1;
}

typedef struct
{
    char* name;
    void (*func)(FVDP_VideoInfo_t*, FVDP_RasterInfo_t*);
} mode_func_t;

mode_func_t mode_func_list[] =
{
    {   "480i",  &mode_480i  },
    {   "480P",  &mode_480P  },
    {   "576i",  &mode_576i  },
    {   "576P",  &mode_576P  },
    {   "720P",  &mode_720P  },
    {   "1080i", &mode_1080i },
    {   "1080P", &mode_1080P },
    {   "XGA",   &mode_XGA   },
    {   "",      NULL        }
};

mode_func_t* mode_SearchFuncList (char* name)
{
    int i = 0;

    while (mode_func_list[i].func != NULL)
    {
        if (!strncasecmp(name, mode_func_list[i].name, strlen(mode_func_list[i].name)))
        {
            return &mode_func_list[i];
        }
        i++;
    }

    return NULL;
}

mode_func_t* parse_GetVideoMode (char** mode_str)
{
    mode_func_t* result = mode_SearchFuncList (*mode_str);
    if (result)
        *mode_str += strlen(result->name);
    return result;
}

bool mode_Setup (FVDP_CH_t CH, char** mode_str)
{
    FVDP_VideoInfo_t    InputVideoInfo;
    FVDP_RasterInfo_t   VideoRasterInfo;
    FVDP_CropWindow_t   CropWindowInfo;
    FVDP_VideoInfo_t    OutputVideoInfo;

    mode_func_t * mode_func;
    double        frame_rate;
    char          setup;

    memset (&InputVideoInfo,  0, sizeof(FVDP_VideoInfo_t ));
    memset (&VideoRasterInfo, 0, sizeof(FVDP_RasterInfo_t));
    memset (&CropWindowInfo,  0, sizeof(FVDP_CropWindow_t ));
    memset (&OutputVideoInfo, 0, sizeof(FVDP_VideoInfo_t ));

    setup = (mode_str && *mode_str) ? **mode_str : 0xff;

    switch (setup)
    {
    case '(':
        (*mode_str) ++;
        mode_func = parse_GetVideoMode (mode_str);
        if (mode_func)
        {
            (mode_func->func)(&InputVideoInfo, &VideoRasterInfo);
            if (parse_GetFloat (mode_str, &frame_rate))
            {
                InputVideoInfo.FrameRate = (uint32_t) (frame_rate * 1000);
                if (InputVideoInfo.ScanType == INTERLACED)
                    InputVideoInfo.FrameRate >>= 1;
            }
        }

        if (*mode_str)
        {
            if (**mode_str == ')')
            {
                (*mode_str) ++;
                break;
            }

            if (**mode_str == ':')
            {
                (*mode_str) ++;
                mode_func = parse_GetVideoMode (mode_str);
                if (mode_func)
                {
                    (mode_func->func)(&OutputVideoInfo, NULL);
                    if (parse_GetFloat (mode_str, &frame_rate))
                    {
                        OutputVideoInfo.FrameRate = (uint32_t) (frame_rate * 1000);
                        if (OutputVideoInfo.ScanType == INTERLACED)
                            OutputVideoInfo.FrameRate >>= 1;
                    }
                }
            }
        }
        if (**mode_str != ')')
        {
            printf("missing end bracket\n");
            return FALSE;
        }
        break;

    case '0':
        mode_1080P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 60000;

        mode_1080P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        break;

    case '1':
        mode_720P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 60000;

        mode_720P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        break;

    case '2':
        mode_480i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 30000;

        mode_480P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        break;

    case '3':
        mode_720P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 60000;
        InputVideoInfo.ColorSampling = FVDP_444;

        mode_720P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        OutputVideoInfo.ColorSampling = FVDP_422;
        break;

    case '4':
        mode_1080i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 30000;

        mode_720P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        break;

    case '5':
        mode_480P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 60000;
        InputVideoInfo.ColorSpace = sdYUV;

        mode_480P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        OutputVideoInfo.ColorSpace = RGB;
        break;

    case '6':
        mode_480P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 25000;
        InputVideoInfo.Width = 704;

        mode_720P(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 60000;
        OutputVideoInfo.ColorSpace = RGB;
        break;

    case '7':
        mode_1080P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 50000;
        VideoRasterInfo.HStart = 312;
        VideoRasterInfo.VStart = 26;
        VideoRasterInfo.HTotal = 2544;
        VideoRasterInfo.VTotal = 1112;

        mode_1080i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 30000;
        OutputVideoInfo.ColorSpace = RGB;
        break;

    case '8':
        mode_480i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 30000;

        mode_480i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 30000;
        break;

    case '9':
        mode_480i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 25000;

        mode_480i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 30000;
        break;

    case 'A':
        mode_480i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 30000;

        mode_480i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 25000;
        break;

    case 'B':
        mode_576i(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 25000;

        mode_576i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 25000;
        break;

    case 'C':
        mode_576P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 25000;

        mode_576i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 25000;
        break;

    case 'D':
        mode_720P(&InputVideoInfo, &VideoRasterInfo);
        InputVideoInfo.FrameRate = 59940;

        mode_576i(&OutputVideoInfo, NULL);
        OutputVideoInfo.FrameRate = 25000;
        break;

    default:
        printf ("please specify valid setup:\n");
        printf ("  0: 1920x1080P60 YUV --> 1024x768P60\n");
        printf ("  1: 1280x720P60 YUV  --> 1280x720P60\n");
        printf ("  2: 720x480i60 YUV   --> 720x480P60\n");
        printf ("  3: 1280x720P60 RGB  --> 1280x720P60\n");
        printf ("  4: 1920x1080i60 YUV --> 1280x720P60\n");
        printf ("  5: 720x480P60 YUV   --> 720x480P60\n");
        printf ("  6: 704x480P25 YUV   --> 1280x720P60\n");
        printf ("  7: 1920x1080P50 YUV --> 1920x1080i60\n");
        printf ("  8: 720x480i60 YUV   --> 720x480i60\n");
        printf ("  9: 720x480i50 YUV   --> 720x480i60\n");
        printf ("  A: 720x480i60 YUV   --> 720x480i50\n");
        printf ("  B: 720x576i50 YUV   --> 720x576i50\n");
        printf ("  C: 720x576P25 YUV   --> 720x576i50\n");
        printf ("  D: 1280x720P60 YUV  --> 720x576i50\n");
        printf ("or use s<CH>(<input_mode>:<output_mode>)\n"
                "eg. s0(1080i59.94:576P50)\n");
        return FALSE;
    }

    (*mode_str) ++;

    printf ("(");
    if (InputVideoInfo.FrameRate > 0)
        printf ("in:%dx%d%c%d",
            InputVideoInfo.Width,
            (InputVideoInfo.ScanType == PROGRESSIVE) ?
             InputVideoInfo.Height : InputVideoInfo.Height * 2,
            (InputVideoInfo.ScanType == PROGRESSIVE) ? 'P' : 'i',
            ((InputVideoInfo.ScanType == PROGRESSIVE) ?
              InputVideoInfo.FrameRate : InputVideoInfo.FrameRate * 2) / 1000);
    if (OutputVideoInfo.FrameRate > 0)
        printf (" --> out:%dx%d%c%d",
            OutputVideoInfo.Width,
            (OutputVideoInfo.ScanType == PROGRESSIVE) ?
             OutputVideoInfo.Height : OutputVideoInfo.Height * 2,
            (OutputVideoInfo.ScanType == PROGRESSIVE) ? 'P' : 'i',
            ((OutputVideoInfo.ScanType == PROGRESSIVE) ?
              OutputVideoInfo.FrameRate : OutputVideoInfo.FrameRate * 2) / 1000);
    printf (")");
    printf ("\n");

    if (CropWindowInfo.HCropWidth == 0 && CropWindowInfo.VCropHeight == 0)
    {
        CropWindowInfo.HCropWidth = InputVideoInfo.Width   - CropWindowInfo.HCropStart;
        CropWindowInfo.VCropHeight = InputVideoInfo.Height - CropWindowInfo.VCropStart;
        printf ("(assuming crop window size %dx%d)\n", CropWindowInfo.HCropWidth, CropWindowInfo.VCropHeight);
    }
    printf ("\n");

    if (   InputVideoInfo.FrameRate > 0
        || OutputVideoInfo.FrameRate > 0 )
    {
        uint8_t event;
        if (CH == FVDP_MAIN) event = EVT_INPUT_UPDATE_CH_A;
        else if (CH == FVDP_PIP) event = EVT_INPUT_UPDATE_CH_B;
        else if (CH == FVDP_AUX) event = EVT_INPUT_UPDATE_CH_C;

        SimEvent_LoadEventPeriod(event,
            (InputVideoInfo.ScanType == PROGRESSIVE) ?
             InputVideoInfo.FrameRate : InputVideoInfo.FrameRate * 2);
        SimEvent_LoadEventPeriod(event + 1,
            (InputVideoInfo.ScanType == PROGRESSIVE) ?
             InputVideoInfo.FrameRate : InputVideoInfo.FrameRate * 2);
        SimEvent_LoadEventPeriod(event + 2,
            (OutputVideoInfo.ScanType == PROGRESSIVE) ?
             OutputVideoInfo.FrameRate : OutputVideoInfo.FrameRate * 2);

        NextEvtTime[event]     = CurrentTime;
        NextEvtTime[event + 1] = CurrentTime;
        NextEvtTime[event + 2] = CurrentTime;
        SimEvent_DelayEvent(event + 1, SimEvent_GetPeriod(event) * 19 / 20);
        SimEvent_DelayEvent(event + 2, SimEvent_GetPeriod(event) / 2);

        CurrentInputVideoInfo[CH]  = InputVideoInfo;
        CurrentRasterInfo[CH]      = VideoRasterInfo;
        CurrentCropWindow[CH]      = CropWindowInfo;
        CurrentOutputVideoInfo[CH] = OutputVideoInfo;
    }

    return TRUE;
}


static int command_counter = 0;
static bool PictureRepeated = FALSE;
static FVDP_FieldType_t InputFieldType[NUM_FVDP_CH] =
    {BOTTOM_FIELD,BOTTOM_FIELD,BOTTOM_FIELD};
static bool SourceSetupComplete[NUM_FVDP_CH] =
    {FALSE,FALSE,FALSE};
static bool mode_SetupComplete[NUM_FVDP_CH] =
    {FALSE,FALSE,FALSE};

static bool parse_GetChannel (char** ppStr, FVDP_CH_t* pCH)
{
    char* pStr = *ppStr;
    uint8_t c = *pStr++;
    *ppStr = pStr;

    if (c >= '0' && c <= '2')
    {
        *pCH = c-'0';
        return TRUE;
    }
    else
    {
        if (c < ' ') c = '?';
        printf("  *** invalid channel %c ***\n",c);
        return FALSE;
    }
}

static void InputUpdate (FVDP_CH_t CH)
{
    FVDP_VideoInfo_t InputVideoInfo = CurrentInputVideoInfo[CH];

    InputVideoInfo.FrameID = timestamp++;
    if (InputVideoInfo.ScanType == INTERLACED)
    {
        //if (InputVideoInfo.FieldType == AUTO_FIELD_TYPE)
        {
            InputVideoInfo.FieldType = InputFieldType[CH];
        }
    }
    else
    {
        InputVideoInfo.FieldType = TOP_FIELD;
        InputFieldType[CH] = BOTTOM_FIELD;
    }

    if (PictureRepeated == TRUE)
    {
        InputVideoInfo.FVDPDisplayModeFlag |= FVDP_FLAGS_PICTURE_REPEATED;
    }
    else
    {
        InputVideoInfo.FVDPDisplayModeFlag &= ~FVDP_FLAGS_PICTURE_REPEATED;
    }

    FVDP_SetInputVideoInfo(ChHandle[CH], CurrentInputVideoInfo[CH]);
    FVDP_SetInputRasterInfo(ChHandle[CH], CurrentRasterInfo[CH]);
    FVDP_SetCropWindow(ChHandle[CH], CurrentCropWindow[CH]);
    FVDP_SetOutputVideoInfo(ChHandle[CH], CurrentOutputVideoInfo[CH]);

    if (InputVideoInfo.ScanType == INTERLACED)
    {
        if (InputVideoInfo.FieldType == TOP_FIELD)
            printf (" (top)");
        else
            printf (" (bottom)");
    }
    printf ("\n");

    FVDP_InputUpdate(ChHandle[CH]);
    FVDP_CommitUpdate(ChHandle[CH]);
    InputFieldType[CH] = (InputFieldType[CH] == BOTTOM_FIELD) ?
                         TOP_FIELD : BOTTOM_FIELD;

    // reload input update and frame ready events
    {
        uint8_t InputUpdate = InputUpdateEvent[CH];
        uint8_t FrameReady  = InputFrameReadyEvent[CH];
        NextEvtTime[FrameReady] = CurrentTime;
        SimEvent_DelayEvent(FrameReady, SimEvent_GetPeriod(InputUpdate) * 19 / 20);
        NextEvtTime[InputUpdate] = CurrentTime + SimEvent_GetPeriod(InputUpdate);
    }
}

static void InputFrameReady(FVDP_CH_t CH)
{
#if defined(CONFIG_MPE42)   // MPE42 interrupts based on COMM_CLUST
    uint32_t reg, mask;
    if (CH == FVDP_MAIN)
    {
        reg = COMM_CLUST_FE_BASE_ADDRESS + COMMCTRL_FE_IRQ_0_STATUS_A;
        mask = BIT0;    // IRQ_Src_Flagline_0
    }
    else if (CH == FVDP_PIP)
    {
        reg = COMM_CLUST_PIP_BASE_ADDRESS + COMMCTRL_IRQ_0_STATUS_A;
        mask = BIT0;    // IRQ_Src_Flagline_0
    }
    else if (CH == FVDP_AUX)
    {
        reg = COMM_CLUST_AUX_BASE_ADDRESS + COMMCTRL_IRQ_0_STATUS_A;
        mask = BIT0;    // IRQ_Src_Flagline_0
    }

    vibe_os_write_register((volatile uint32_t*) gRegisterBaseAddress, reg, mask);
    while (vibe_os_read_register((volatile uint32_t*) gRegisterBaseAddress, reg) & mask);
#else
    if (CH == FVDP_MAIN)
    {
        REG32_Write(MOT_DET_STATUS,0x3);
        while (REG32_Read(MOT_DET_STATUS) == 0x3);
    }
#endif

    // reload frame ready event
    {
        uint8_t InputUpdate = InputUpdateEvent[CH];
        uint8_t FrameReady = InputFrameReadyEvent[CH];
        NextEvtTime[FrameReady] = CurrentTime + SimEvent_GetPeriod(InputUpdate);
    }
}

static void OutputUpdate(FVDP_CH_t CH)
{
    FVDP_VideoInfo_t OutputVideoInfo;
    FVDP_OutputUpdate(ChHandle[CH]);
    FVDP_CommitUpdate(ChHandle[CH]);
    FVDP_GetOutputVideoInfo(ChHandle[CH], &OutputVideoInfo);

    // reload output update event
    {
        uint8_t OutputUpdate = OutputUpdateEvent[CH];
        NextEvtTime[OutputUpdate] = CurrentTime + SimEvent_GetPeriod(OutputUpdate);
    }
}

char* ChannelNames[] =
{
    "main",
    "PiP",
    "Aux",
    "Enc"
};

typedef enum
{
    SC_COMMENT,
    SC_HELP,
    SC_2,
    SC_SUBCOMMAND_ENTER,
    SC_SUBCOMMAND_EXIT,
    SC_QUIT,
    SC_SOURCE_FILE,
    SC_DEVICE_INIT,
    SC_OPEN_CHANNEL,
    SC_CLOSE_CHANNEL,
    SC_SETUP_INPUT_SOURCE,
    SC_SETUP_VIDEO_MODES,
    SC_AUTO_UPDATE,
    SC_INPUT_UPDATE,
    SC_OUTPUT_UPDATE,
    SC_FRAME_READY,
    SC_TOGGLE_FIELD_POLARITY,
    SC_SET_PICTURE_REPEATED,
    SC_VCPU_STATUS,
    SC_VCD_CAPTURE,
    SC_GTKWAVE,
    SC_INFO,
    SC_LIST_MODULES,
    SC_REG_ACCESS,
    SC_SHOW_REG_WRITES,
    SC_FLUSH_QUEUE,
    SC_MAX_COMMANDS
} SimCommand_t;

typedef struct
{
    const char*    token;
    const char*    name;
    const bool     ch_param;
} SimCommandItem_t;

const SimCommandItem_t command_list[SC_MAX_COMMANDS] =
{
    {   "#"     , "comment"               , FALSE },  // SC_COMMENT
    {   "?"     , "help"                  , FALSE },  // SC_HELP
    {   "dbg"   , "set debug level"       , FALSE },  // SC_2
    {   "{"     , NULL                    , FALSE },  // SC_SUBCOMMAND_ENTER
    {   "}"     , NULL                    , FALSE },  // SC_SUBCOMMAND_EXIT
    {   "q"     , "quit"                  , FALSE },  // SC_QUIT
    {   "source", "read actions from file", FALSE },  // SC_SOURCE_FILE
    {   "D"     , "init device"           , FALSE },  // SC_DEVICE_INIT
    {   "O"     , "open channel"          , TRUE  },  // SC_OPEN_CHANNEL
    {   "C"     , "close channel"         , TRUE  },  // SC_CLOSE_CHANNEL
    {   "S"     , "setup input source"    , TRUE  },  // SC_SETUP_INPUT_SOURCE
    {   "s"     , "setup video modes"     , TRUE  },  // SC_SETUP_VIDEO_MODES
    {   "a"     , "auto event generation" , FALSE },  // SC_AUTO_UPDATE
    {   "i"     , "input update"          , TRUE  },  // SC_INPUT_UPDATE
    {   "o"     , "output update"         , TRUE  },  // SC_OUTPUT_UPDATE
    {   "R"     , "frame ready"           , TRUE  },  // SC_FRAME_READY
    {   "p"     , "toggle field polarity" , TRUE  },  // SC_TOGGLE_FIELD_POLARITY
    {   "~"     , "set picture repeated"  , FALSE },  // SC_SET_PICTURE_REPEATED
    {   "vcpu"  , "request VCPU status"   , FALSE },  // SC_VCPU_STATUS
    {   "vcd"   , "vcd capture control"   , FALSE },  // SC_VCD_CAPTURE
    {   "gtk"   , "load gtkwave"          , FALSE },  // SC_GTKWAVE,
    {   "I"     , "generic info"          , FALSE },  // SC_INFO
    {   "M"     , "list modules"          , TRUE  },  // SC_LIST_MODULES
    {   "dw"    , "dword reg read/write"  , FALSE },  // SC_REG_ACCESS
    {   "r"     , "show register writes"  , FALSE },  // SC_SHOW_REG_WRITES
    {   "F"     , "flush queue"           , TRUE  },  // SC_FLUSH_QUEUE
};

void PrintCommands(void)
{
    int i;
    for (i = 0; i < SC_MAX_COMMANDS; i++)
    {
        const SimCommandItem_t* pPtr = &command_list[i];
        if (pPtr->name != NULL)
        {
            printf ("        %-10s %s", pPtr->token, pPtr->name);
            if (pPtr->ch_param)
                printf (" <CH>");
            printf ("\n");
        }
    }
}

static bool parse_GetCommand (char** ppStr, SimCommand_t* pCommand)
{
    // search for matching command
    for (*pCommand = 0;
         (   (*pCommand < SC_MAX_COMMANDS)
          && strncmp(*ppStr,command_list[*pCommand].token,
                     strlen(command_list[*pCommand].token) ));
         (*pCommand)++);

    if (*pCommand < SC_MAX_COMMANDS)
    {
        *ppStr += strlen(command_list[*pCommand].token);
        return TRUE;
    }

    return FALSE;
}

void ParseCommandString (char* command_str)
{
    bool            help = FALSE;
    static bool     mode_changed = FALSE;
    uint32_t        repeat_count = 0;
    int             i,j,level;
    char            c,subcommand[80];
    uint32_t        Modules;
    uint32_t        DebugLevel;
    FVDP_CH_t       CH;
    SimCommand_t    command;
    uint32_t        reg_val, reg_addr;

    while ((c = *command_str))
    {
        if (isspace(c))
        {
            command_str++;
            continue;
        }

        if (isdigit(c))
        {
            repeat_count *= 10;
            repeat_count += c - '0';
            command_str++;
            continue;
        }

        // if command found...
        if (parse_GetCommand(&command_str,&command) == TRUE)
        {
            if (command != SC_COMMENT)
                printf("\nProcess command %d: %s", ++command_counter, command_list[command].name);

            // if channel parameter required...
            if (command_list[command].ch_param == TRUE)
            {
                // get channel param... if error...
                if (!parse_GetChannel(&command_str, &CH))
                {
                    // abort.
                    printf ("\nMissing channel parameter\n");
                    printf ("ABORTED\n");
                    return;
                }
                else
                {
                    printf (" (%s channel)", ChannelNames[CH]);
                }
            }
            printf ("\n");
        }

        switch (command)
        {
        case SC_COMMENT:
            return;

        case SC_DEVICE_INIT:
            {
                FVDP_Init_t InitParams;
                InitParams.pDeviceBaseAddress = (uint32_t*)DEVICE_BASE_ADDRESS;
                FVDP_InitDriver(InitParams);
            }
            break;

        case SC_OPEN_CHANNEL:
            {
                FVDP_Context_t   Context;

                switch (CH)
                {
                    case FVDP_MAIN:
                        Context.ProcessingType = FVDP_TEMPORAL;
                        //MemSize = 40 * 1024 * 1024;
                        break;

                    case FVDP_PIP:
                        Context.ProcessingType = FVDP_SPATIAL;
                        //MemSize = 23 * 1024 * 1024;
                        break;

                    case FVDP_AUX:
                        Context.ProcessingType = FVDP_SPATIAL;
                        //MemSize = 23 * 1024 * 1024;
                        break;

                    default:
                        printf("invalid channel!\n");
                        return;
                }

                Context.MaxWindowSize.Width = MAX_DISPLAY_HSIZE;
                Context.MaxWindowSize.Height = MAX_DISPLAY_VSIZE;

                i = FVDP_TEMPORAL;
                if (CH > FVDP_MAIN) i = FVDP_SPATIAL;
                if (FVDP_OpenChannel(&ChHandle[CH], CH, 0x80000000, 0x4000000, Context) != FVDP_OK)
                {
                    printf("  *** unable to open channel %d ***\n",CH);
                    printf ("ABORTED\n");
                    return;
                }
            }
            break;

        case SC_SETUP_INPUT_SOURCE:
            c = *command_str++;
            if (c >= '0' && c <= '9')
            {
                if (FVDP_ConnectInput(ChHandle[CH], (FVDP_Input_t)(c-'0')) != FVDP_OK)
                {
                    printf("  *** unable to setup source %c ***\n",c);
                    printf ("ABORTED\n");
                    return;
                }
                else
                {
                    SourceSetupComplete[CH] = TRUE;
                }
            }
            else
            {
                printf ("please specify valid input source:\n");
                printf ("  0: FVDP_MEM_INPUT_SINGLE_BUFF\n"
                        "  1: FVDP_MEM_INPUT_DUAL_BUFF\n"
                        "  2: FVDP_VMUX_INPUT1\n"
                        "  3: FVDP_VMUX_INPUT2\n"
                        "  4: FVDP_VMUX_INPUT3\n"
                        "  5: FVDP_VMUX_INPUT4\n"
                        "  6: FVDP_VMUX_INPUT5\n"
                        "  7: FVDP_VMUX_INPUT6\n");
            }
            break;

        case SC_SETUP_VIDEO_MODES:
            c = *command_str;
            if (c)
            {
                if (SourceSetupComplete[CH] == FALSE)
                {
                    printf("(Input not connected, defaulting FVDP_VMUX_INPUT1)\n");
                    if (FVDP_ConnectInput(ChHandle[CH], 2) != FVDP_OK)
                    {
                        printf("  *** unable to setup source %c ***\n",2);
                        printf("  *** unable to setup mode %c ***\n",c);
                        printf ("ABORTED\n");
                        return;
                    }
                    SourceSetupComplete[CH] = TRUE;
                }

                if (!mode_Setup(CH, &command_str))
                {
                    if (c < ' ') c = '?';
                    printf("  *** unable to setup mode %c ***\n",c);
                    mode_Setup(CH, NULL);
                }
                else
                {
                    mode_SetupComplete[CH] = TRUE;
                }
            }
            else
            {
                mode_Setup(CH, NULL);
                mode_changed = TRUE;
            }
            break;

        case SC_INPUT_UPDATE:
            if (mode_SetupComplete[CH] == FALSE)
            {
                printf("\nNeed to setup input/output modes first (eg. \"s%dA\")\n",CH);
                printf ("ABORTED\n");
                return;
            }

            if (repeat_count == 0)
                repeat_count = 1;

            while (repeat_count)
            {
                SimEvent_t event_i  = InputUpdateEvent[CH];

                usleep(10);
                if (mode_changed)
                {
                    mode_changed = FALSE;
                    InputFieldType[CH] = BOTTOM_FIELD;
                }
                CurrentTime = NextEvtTime[event_i];
                Linux_Sim_SetTime(CurrentTime);

                InputUpdate(CH);
                SimEvent_FastForwardOtherEvents(event_i);

                repeat_count --;
            }
            break;

        case SC_OUTPUT_UPDATE:
            if (mode_SetupComplete[CH] == FALSE)
            {
                printf("\nNeed to setup input/output modes first (eg. s%dA)\n",CH);
                printf ("ABORTED\n");
                return;
            }

            if (repeat_count == 0)
                repeat_count = 1;

            while (repeat_count)
            {
                SimEvent_t event_o  = OutputUpdateEvent[CH];

                CurrentTime = NextEvtTime[event_o];
                Linux_Sim_SetTime(CurrentTime);

                OutputUpdate(CH);
                SimEvent_FastForwardOtherEvents(event_o);

                repeat_count --;
            }
            break;

        case SC_TOGGLE_FIELD_POLARITY:
            printf ("(next is ");
            if (InputFieldType[CH] == TOP_FIELD)
            {
                printf ("bottom)\n");
                InputFieldType[CH] = BOTTOM_FIELD;
            }
            else
            {
                printf ("top)\n");
                InputFieldType[CH] = TOP_FIELD;
            }
            break;

        case SC_SET_PICTURE_REPEATED:
            PictureRepeated = !PictureRepeated;
            if (PictureRepeated == TRUE)
                printf ("(input frames will now be marked as PICTURE_REPEATED)\n");
            else
                printf ("(input frames will now be NOT marked as PICTURE_REPEATED)\n");
            break;

        case SC_FRAME_READY:
            if (repeat_count == 0)
                repeat_count = 1;

            while (repeat_count)
            {
                SimEvent_t event_fr = InputFrameReadyEvent[CH];

                CurrentTime = NextEvtTime[event_fr];
                Linux_Sim_SetTime(CurrentTime);

                InputFrameReady(CH);
                SimEvent_FastForwardOtherEvents(event_fr);

                repeat_count --;
            }
            break;

        case SC_AUTO_UPDATE:
            if (repeat_count == 0)
                repeat_count = 1;

            while (repeat_count)
            {
                SimEvent_t simevt;
                uint32_t t;
                SimEvent_GetNext(&simevt,&t);
                Linux_Sim_SetTime(t);
                printf("time: %u  ",t);
                switch (simevt)
                {
                case EVT_INPUT_UPDATE_CH_A:
                    printf("input update main channel\n");
                    InputUpdate(FVDP_MAIN);
                    break;
                case EVT_INPUT_FRAME_READY_CH_A:
                    printf("frame-ready main channel\n");
                    InputFrameReady(FVDP_MAIN);
                    break;
                case EVT_OUTPUT_UPDATE_CH_A:
                    printf("output update main channel\n");
                    OutputUpdate(FVDP_MAIN);
                    break;
                case EVT_INPUT_UPDATE_CH_B:
                    printf("input update PiP channel\n");
                    InputUpdate(FVDP_PIP);
                    break;
                case EVT_INPUT_FRAME_READY_CH_B:
                    printf("frame-ready PiP channel\n");
                    InputFrameReady(FVDP_PIP);
                    break;
                case EVT_OUTPUT_UPDATE_CH_B:
                    printf("output update PiP channel\n");
                    OutputUpdate(FVDP_PIP);
                    break;
                case EVT_INPUT_UPDATE_CH_C:
                    printf("input update aux channel\n");
                    InputUpdate(FVDP_AUX);
                    break;
                case EVT_INPUT_FRAME_READY_CH_C:
                    printf("frame-ready aux channel\n");
                    InputFrameReady(FVDP_AUX);
                    break;
                case EVT_OUTPUT_UPDATE_CH_C:
                    printf("output update aux channel\n");
                    OutputUpdate(FVDP_AUX);
                    break;
                default:
                    printf("unknown event!\n");
                    break;
                }

                repeat_count --;
            }
            break;

        case SC_CLOSE_CHANNEL:
            if (FVDP_CloseChannel(ChHandle[CH]) != FVDP_OK)
            {
                printf("  *** unable to close channel %d ***\n",CH);
                printf ("ABORTED\n");
                return;
            }
            break;

        case SC_VCPU_STATUS:
            Linux_Sim_SetPrintVcpuStatusRequest();
            break;

#ifdef DEBUG_LOG_SOURCE_SELECT
        case SC_VCD_CAPTURE:
        {
            const char* module[] = { "HOST", "VCPU" };
            const char* channel[] = { "MAIN", "PIP", "AUX", "ENC" };
            const char* mode[] = { "STOP", "AUTO", "TRIG", "SINGLE" };
            char subtoken[80];
            bool found_token = FALSE;
            bool show_help = FALSE;
            bool show_status = TRUE;

            while (parse_GetToken(&command_str, subtoken))
            {
                found_token = TRUE;

                if (!strcasecmp(subtoken,"HOST"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0xf0;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
                else
                if (!strcasecmp(subtoken,"VCPU"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0xf0;
                    source |= 0x01;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
                else
                if (!strcasecmp(subtoken,"MAIN"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0x0f;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
                else
                if (!strcasecmp(subtoken,"PIP"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0x0f;
                    source |= 0x10;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
                else
                if (!strcasecmp(subtoken,"AUX"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0x0f;
                    source |= 0x20;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
#ifdef CONFIG_MPE42
                else
                if (!strcasecmp(subtoken,"ENC"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);
                    source &= 0x0f;
                    source |= 0x30;
                    REG32_Write(DEBUG_LOG_SOURCE_SELECT, source);
                }
#endif
                else
                if (!strcasecmp(subtoken,"STOP"))
                {
                    uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                    log_ctrl &= 0xf0;
                    REG32_Write(DEBUG_LOG_CTRL, log_ctrl);
                }
                else
                if (!strcasecmp(subtoken,"AUTO"))
                {
                    uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                    log_ctrl &= 0xf0;
                    log_ctrl |= 0x01;
                    REG32_Write(DEBUG_LOG_CTRL, log_ctrl);
                }
                else
                if (!strcasecmp(subtoken,"TRIG"))
                {
                    uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                    uint8_t event = REG32_Read(DEBUG_LOG_TRIG_SELECT);
                    if (parse_GetIntegerToken(&command_str, &reg_val))
                    {
                        event = reg_val;
                    }
                    else
                    {
                        printf("no trigger event selected, assuming VCDEvent ID=%d\n", event);
                    }
                    log_ctrl &= 0xf0;
                    log_ctrl |= 0x02;
                    REG32_Write(DEBUG_LOG_TRIG_SELECT, event);
                    REG32_Write(DEBUG_LOG_CTRL, log_ctrl);
                }
                else
                if (!strcasecmp(subtoken,"SINGLE"))
                {
                    uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                    log_ctrl &= 0xf0;
                    log_ctrl |= 0x03;
                    REG32_Write(DEBUG_LOG_CTRL, log_ctrl);
                }
                else
                if (!strcasecmp(subtoken,"SHOW"))
                {
                    uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT) & 0x0f;
                    uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                    log_ctrl &= 0xf0;
                    log_ctrl |= 0x04;
                    REG32_Write(DEBUG_LOG_CTRL, log_ctrl);

                    VCPU_LogPrintControl();

                    printf ("automatically loading gtkwave to display results...\n");
                    if (source == 1)
                        system("gtkwave vcpu_log.vcd --save=vcpu_log.gtkw &");
                    else
                        system("gtkwave arm_log.vcd --save=arm_log.gtkw &");
                }
                else
                {
                    printf("invalid token \"%s\"\n", subtoken);
                    show_help = TRUE;
                    while (parse_GetToken(&command_str, subtoken)); // flush
                    break;
                }
            }

            if (found_token == FALSE)
                show_help = TRUE;

            if (show_help == TRUE)
            {
                printf("parameters may include one or more of\n"
                       "\tHOST          (for Host or ARM processor for logging FVDP events\n"
                       "\tVCPU          (for STxP70 for logging VCPU events)\n"
                       "\tMAIN          (for main channel)\n"
                       "\tPIP           (for PIP channel)\n"
                       "\tAUX           (for AUX channel)\n"
#ifdef CONFIG_MPE42
                       "\tENC           (for encoder channel)\n"
#endif
                       "\tSTOP          (to force stop logging)\n"
                       "\tAUTO          (to continuously log until STOP requested\n"
                       "\tTRIG <event>  (to log with <event> centered\n"
                       "\tSINGLE        (to log until buffer is full\n"
                       "\tSHOW          (to display VCD waveform using gtkwave\n");
            }

            if (show_status == TRUE)
            {
                uint8_t log_ctrl = REG32_Read(DEBUG_LOG_CTRL);
                uint8_t source = REG32_Read(DEBUG_LOG_SOURCE_SELECT);

                printf("current trigger mode is %s on %s for %s channel\n",
                       mode[log_ctrl],
                       module[source & 0x0f],
                       channel[(source >> 4) & 0x0f]);

                if ((log_ctrl & 0x0f) == 2)
                {
                    uint8_t event = REG32_Read(DEBUG_LOG_TRIG_SELECT);
                    printf("trigger event is %d\n", event);
                }
            }

            break;
        }

        case SC_GTKWAVE:
            if (REG32_Read(DEBUG_LOG_SOURCE_SELECT) == 1)
                system("gtkwave vcpu_log.vcd --save=vcpu_log.gtkw &");
            else
                system("gtkwave arm_log.vcd --save=arm_log.gtkw &");
            break;
#endif

        case SC_SOURCE_FILE:
        {
            FILE* stream;
            char filename[80];

            if (parse_GetToken(&command_str, filename))
            {
                stream = fopen(filename,"r");
                if (stream != NULL)
                {
                    uint32_t line = 0;
                    do
                    {
                        char str[1024];
                        if (fgets(str,1024,stream) == NULL)
                            break;

                        printf("fvdp_sim:%s:%u> %s",filename,++line,str);

                        // Do stuff...
                        ParseCommandString(str);
                    } while (TRUE);

                    fclose(stream);
                }
            }
            break;
        }

        case SC_QUIT:
            user_quit = TRUE;
            break;

        case SC_INFO:
            printf("revision string = \"%s\"\n",FVDP_ReleaseID);
            printf("build date time = \"%s,%s\"\n", __DATE__, __TIME__);
            break;

        case SC_LIST_MODULES:
            ROUTER_DebugPrintModules(CH);
            break;

        case SC_REG_ACCESS:
            if (parse_GetIntegerToken(&command_str, &reg_addr))
            {
                if (parse_GetIntegerToken(&command_str, &reg_val))
                {
                    WriteRegister((volatile uint32_t*) reg_addr,reg_val);
                }
                else
                {
                    reg_val = ReadRegister((volatile uint32_t*) reg_addr);
                    printf ("0x%x\n",reg_val);
                }
            }
            else
            {
                printf ("syntax: %s <reg_addr> [data]\n"
                        "this c either reads a dword from, or writes a dword to, a register address\n"
                        "if data is not specified, then a read operation is performed.\n",
                        command_list[command].token);
            }
            break;

        case SC_2:
            // set debug level of certain modules using mask
            if (    parse_GetIntegerToken(&command_str,&Modules)
                &&  parse_GetIntegerToken(&command_str,&DebugLevel) )
            {
                ROUTER_SetDebugLevel (CH, Modules, (uint8_t) DebugLevel);
            }
            else
            {
                printf("expecting module mask and debug level, "
                       "or just debug level\n");
                printf("ABORTED\n");
                return;
            }
            break;

        case SC_SHOW_REG_WRITES:
            // toggle display of register writes
            if (*pRegDebugLevel == DBG_ERROR)
            {
                printf ("(enabled)\n");
                *pRegDebugLevel = DBG_INFO;
            }
            else
            {
                printf ("(disabled)\n");
                *pRegDebugLevel = DBG_ERROR;
            }
            break;

        case SC_FLUSH_QUEUE:
            FVDP_QueueFlush(ChHandle[CH],TRUE);
            break;

        case SC_SUBCOMMAND_ENTER:
            level = 1;
            j = 0;
            subcommand[j] = 0;
            do
            {
                subcommand[j] = *(command_str+j);
                if (subcommand[j] == 0 && level > 0)
                {
                    printf("missing end-bracket!\n");
                    return;
                }
                if (subcommand[j] == '}')
                {
                    if (--level == 0)
                    {
                        subcommand[j] = 0;

                        if (repeat_count == 0)
                            repeat_count = 1;

                        for (i = 0; i < repeat_count; i++)
                        {
                            //printf("Handle sub-command: %s\n", subcommand);
                            ParseCommandString(subcommand);
                        }
                        repeat_count = 0;
                        command_str += j+1;
                    }
                }
                if (subcommand[j] == '{')
                {
                    level++;
                }
                if (++j >= 80)
                {
                    printf("max sub-command length of 80 reached!\n");
                    return;
                }
            } while (level > 0);

            break;

        case SC_SUBCOMMAND_EXIT:
            printf ("unmatched sub-command bracket\n");
            printf("ABORTED\n");
            return;

        default:
            printf("Unknown command beginning with '%c'\n",c);

        case SC_HELP:
            help = TRUE;
            break;
        }

        if (repeat_count != 0)
        {
            printf("ERROR: repeat count not applicable\n");
            repeat_count = 0;
        }

        if (help == TRUE)
        {
            printf("\navailable commands:\n");
            PrintCommands();
            break;
        }
    }
}

int main (int argc, char** argv)
{
    char* reg_file_out = NULL;
    char* log_file_out = NULL;
    char* command_str = NULL;
    uint8_t columns = 4;
    uint8_t debug_level = 4;
    bool help_mode = FALSE;
    bool interactive_mode = TRUE;
    uint32_t startaddr = 0, endaddr = 0;

    if (argc > 1)
    {
        uint8_t i=0;
        while (argc--)
        {
            if (!strcmp(argv[i],"--?") ||
                !strcmp(argv[i],"-?") ||
                !strcmp(argv[i],"--h") ||
                !strcmp(argv[i],"-h") ||
                !strcmp(argv[i],"--help") ||
                !strcmp(argv[i],"-help"))
            {
                help_mode = TRUE;
            }
            if (!strcmp(argv[i],"-i"))
            {
                interactive_mode = TRUE;
            }
            if (!strcmp(argv[i],"-col"))
            {
                if (argc > 0)
                {
                    columns = atoi(argv[i+1]);
                    argc --;
                    i ++;
                }
            }
            if (!strcmp(argv[i],"-d"))
            {
                if (argc > 0)
                {
                    debug_level = atoi(argv[i+1]);
                    argc --;
                    i ++;
                }
            }

            if (!strcmp(argv[i],"-rf"))
            {
                if (argc > 0)
                {
                    reg_file_out = argv[i+1];
                    argc --;
                    i ++;
                }
            }
            if (!strcmp(argv[i],"-lf"))
            {
                if (argc > 0)
                {
                    log_file_out = argv[i+1];
                    argc --;
                    i ++;
                }
            }

            if (!strcmp(argv[i],"-range"))
            {
                if (argc > 0)
                {
                    if (!hex2dec(argv[++i],&startaddr))
                        startaddr = 0;
                    argc --;
                }
                if (argc > 0)
                {
                    if (!hex2dec(argv[++i],&endaddr))
                        endaddr = 0;
                    argc --;
                }
                if (endaddr < startaddr ||
                    endaddr > END_REG_ADDR ||
                    startaddr < START_REG_ADDR)
                {
                    printf ("INVALID ADDR RANGE/FORMAT\n");
                    printf ("got range [0x%X,0x%X]\n",
                            startaddr, endaddr);
                    printf ("expecting hex values within [0x%X,0x%X]\n",
                            START_REG_ADDR, END_REG_ADDR);
                    help_mode = TRUE;
                }
            }
            if (!strcmp(argv[i],"-cmd"))
            {
                if (argc > 0)
                {
                    command_str = argv[i+1];
                    argc --;
                    i ++;
                }
            }
            i ++;
        }
    }

    //sim_IDebug_setup();
    //sim_IOS_setup();

    printf("\n%s [-rf <regfile> -lf <logfile> -col <columns> -range <startaddr> <endaddr>] [-cmd <commands>] [-i]\n\n",argv[0]);
    printf("notes:\n");
    printf("    use -d <debug_lev> to specify the default debug level (4 by default)\n");
    printf("    use -rf <regfile> to specify register dump file\n");
    printf("    use -lf <logfile> to specify output log file\n");
    printf("    use -col <columns> to specify columns in <regfile>\n");
    printf("    use -range <startaddr> <endaddr> to specify register range to dump\n");
    printf("    use -cmd <commands> to specify c string\n");
    printf("    use -i for interactive mode\n\n");
    printf("    available <commands>:\n");
    PrintCommands();
    printf("eg. %s -rf regdump.txt -col 4 ",argv[0]);
    printf("-range 0xfd430000 0xfd430800 ");
    printf("-cmd DO0S06s00i0o0i0o0i0o0C0\n");
    printf("(open channel main_ch, select ch_main source_6, mode setup ch_main mode_0, "
           "perform InputUpdate and OutputUpdate 3 times, "
           "dump register image of range [0xfd430000,0xfd430800] to regdump.txt in 4 columns, "
           "then close the channel\n\n");

    if (help_mode) return -1;

#if defined(CONFIG_MPE42)
    regbase = (uint32_t*)setup_shared_mem(getuid() + 128,SHMEM_SIZE);
#else
    regbase = (uint32_t*)setup_shared_mem(getuid() + 126,SHMEM_SIZE);
#endif

    if(!regbase)
    {
        printf("FVDP simulation - register area is not allocated.\n");
    }

    printf("===== simulation started =====\n");

    pRegDebugLevel = (uint32_t*) ((uint8_t*) regbase + DEBUG_INFO_START + DEBUG_LEVEL_REG);
    *pRegDebugLevel = DBG_ERROR;

    ROUTER_SetDebugLevel (FVDP_MAIN, ~0, debug_level);
    ROUTER_SetDebugLevel (FVDP_PIP, ~0, debug_level);
    ROUTER_SetDebugLevel (FVDP_AUX, ~0, debug_level);

    if (log_file_out)
    {
        log_stream = fopen(log_file_out, "w+");
        if (!log_file_out)
        {
            printf ("error writing to %s...\n", log_file_out);
            fclose(log_stream);
        }
    }

    if (!interactive_mode)
    {
        if (!command_str)
            command_str = "DO0S06s00o0i0o0i0o0i0o0i0o0i0o0C0";
        ParseCommandString (command_str);
    }

    while (interactive_mode && !user_quit)
    {
        static char prev_input[256] = "";
        char* input, shell_prompt[100];

        // Configure readline to auto-complete paths when the tab key is hit.
        rl_bind_key('\t', rl_complete);

        // Create prompt string from user name and current working directory.
        //snprintf(shell_prompt, sizeof(shell_prompt), "%s:%s $ ", getenv("USER"), getcwd(NULL, 1024));
        snprintf(shell_prompt, sizeof(shell_prompt), "fvdp_sim> ");

        // Display prompt and read input (n.b. input must be freed after use)...
        input = readline(shell_prompt);

        // Check for EOF.
        if (!input)
            break;

        // Add input to history.
        if (strcmp(prev_input,input))
        {
            add_history(input);
            strcpy(prev_input,input);
        }

        // Do stuff...
        ParseCommandString(input);

        // Free input.
        free(input);
    }

    if (reg_file_out)
    {
        FILE* stream = fopen(reg_file_out, "w+");

        printf("===== dumping registers =====\n");

        if (stream)
        {
            if (!startaddr || !endaddr)
            {
                startaddr = START_REG_ADDR;
                endaddr = END_REG_ADDR;
            }
            DumpRegArea(stream,columns,
                        (void*)regbase,startaddr,endaddr - startaddr);
            fclose(stream);
        }
        else
        {
            printf ("error writing to %s...\n", reg_file_out);
        }
    }

    VCPU_Term();

    free_resources((void*) regbase);

    if (log_stream)
        fclose(log_stream);

    printf("\n===== simulation completed =====\n\n");
    return -1;
}

