/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "../fvdp_revision.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_ivp.h"
#include "fvdp_color.h"
#include "fvdp_common.h"
#include "fvdp_router.h"
#include "fvdp_scalerlite.h"
#include "fvdp_hqscaler.h"
#include "fvdp_datapath.h"
#include "fvdp_fbm.h"
#include "fvdp_vcpu.h"
#include "fvdp_hostupdate.h"


/* Private Macros ----------------------------------------------------------- */

#define MAX_NUM_OF_FVDP_HANDLES      32

#define MAIN_FRAME_POOL_SIZE         20
#define PIP_FRAME_POOL_SIZE          16
#define AUX_FRAME_POOL_SIZE          16
#define ENC_FRAME_POOL_SIZE          4

#define INTERLACED_INPUT_MAX_HEIGHT  540
#define BITS_PER_PACKET              64
#define BYTES_PER_PACKET             8
#define MAX_BPP_PER_CHANNEL          10
#define MCDI_BPP_PER_CHANNEL         4
#define NUM_PRO_VIDEO_BUFFERS        7
#define NUM_PRO_RGB_BUFFERS          4
#define NUM_C_VIDEO_BUFFERS          4
#define NUM_CCS_RAW_BUFFERS          3
#define NUM_MCDI_SB_BUFFERS          4
#define NUM_PIP_BUFFERS              4
#define NUM_AUX_BUFFERS              4
#define NUM_ENC_BUFFERS              5

/* Private Enums ------------------------------------------------------------ */

typedef enum
{
    FVDP_HANDLE_CLOSED,
    FVDP_HANDLE_OPENED
} FVDP_HandleState_t;

/* Private Structure -------------------------------------------------------- */

typedef struct
{
    FVDP_CH_t             Ch;
    FVDP_HandleState_t    State;
    FVDP_HandleContext_t* HandleContext_p;
} FVDP_HandleMap_t;

/* Private Variables (static)------------------------------------------------ */

// FVDP_HandleMap maps the FVDP_CH_Handle_t handle number to the FVDP_HandleContext_t
// context structure
static FVDP_HandleMap_t FVDP_HandleMap[MAX_NUM_OF_FVDP_HANDLES];

// The Frame Pools for MAIN, PIP, and AUX channels.
// Note: the frame pools could be allocated dynamically (and should be if
// multiple handles can be opened on the same channel).
// Since in MPE42 only one handle can be opened for each of MAIN, PIP, and AUX they are
// allocated statically for simplicity.
static FVDP_Frame_t       FramePoolArray_MainCh[MAIN_FRAME_POOL_SIZE];
static FVDP_Frame_t       FramePoolArray_PipCh[PIP_FRAME_POOL_SIZE];
static FVDP_Frame_t       FramePoolArray_AuxCh[AUX_FRAME_POOL_SIZE];
static uint8_t            FramePoolStates_MainCh[MAIN_FRAME_POOL_SIZE];
static uint8_t            FramePoolStates_PipCh[PIP_FRAME_POOL_SIZE];
static uint8_t            FramePoolStates_AuxCh[AUX_FRAME_POOL_SIZE];

//FVDP drive power state
static Power_State_t     FVDPDriverPowerState;
/* Private Function Prototypes ---------------------------------------------- */

static void          SYSTEM_InitBufferPools(FVDP_HandleContext_t* HandleContext_p);
static void          SYSTEM_FreeBufferPoolsMemory(FVDP_HandleContext_t* HandleContext_p);
static uint32_t      SYSTEM_GetVideoBufferSize(uint32_t Width, uint32_t Height, uint32_t BppPerChannel);
#ifdef USE_VCPU
static FVDP_Result_t SYSTEM_InitVCPU(void);
static FVDP_Result_t SYSTEM_StartVCPU(void);
#endif

/* Global Variables --------------------------------------------------------- */
// keep current H/W configuration
FVDP_HW_Config_t HW_Config[NUM_FVDP_CH];

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_Init(void)
{
    uint32_t i;

    for (i = 0; i < MAX_NUM_OF_FVDP_HANDLES; i++)
    {
        FVDP_HandleMap[i].State = FVDP_HANDLE_CLOSED;
        FVDP_HandleMap[i].HandleContext_p = NULL;
    }
    SYSTEM_ENC_Init();

    //Init current H/W configuration
    for (i = 0; i < NUM_FVDP_CH; i++)
    {
        vibe_os_zero_memory(&HW_Config[i], sizeof(FVDP_HW_Config_t));
        HW_Config[i].PowerState = RESUME_POWER_STATE;
        FVDPDriverPowerState = RESUME_POWER_STATE;
    }

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
FVDP_Result_t SYSTEM_InitPsiControlData(FVDP_PSIControlLevel_t * FVDP_PSIControlLevelLocal)
{
    FVDP_PSIControlLevelLocal->SharpnessLevel  = 0x0;
    FVDP_PSIControlLevelLocal->BrightnessLevel = 0x0;
    FVDP_PSIControlLevelLocal->ContrastLevel   = 0x100;
    FVDP_PSIControlLevelLocal->SaturationLevel = 0x100;
    FVDP_PSIControlLevelLocal->HueLevel        = 0x0;
    FVDP_PSIControlLevelLocal->RedGain         = 0x100;
    FVDP_PSIControlLevelLocal->GreenGain       = 0x100;
    FVDP_PSIControlLevelLocal->BlueGain        = 0x100;
    FVDP_PSIControlLevelLocal->RedOffset       = 0x0;
    FVDP_PSIControlLevelLocal->GreenOffset     = 0x0;
    FVDP_PSIControlLevelLocal->BlueOffset      = 0x0;

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
FVDP_Result_t SYSTEM_OpenChannel(FVDP_CH_Handle_t* FvdpChHandle_p,
                                 FVDP_CH_t         CH,
                                 uint32_t          MemBaseAddress,
                                 uint32_t          MemSize,
                                 FVDP_Context_t    Context)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_CH_Handle_t      Handle;
    bool                  AllowMultipleHandlesPerChannel;
    uint32_t              i;

    // Specify which channels are allowed to have multiple handles per channel.
    // In case of MPE42, this is only for ENC channel.
    if (CH == FVDP_ENC)
        AllowMultipleHandlesPerChannel = TRUE;
    else
        AllowMultipleHandlesPerChannel = FALSE;

    // Check that channels that do not support multiple handles, don't already
    // have an open handle.
    if (AllowMultipleHandlesPerChannel == FALSE)
    {
        for (i = 0; i  < MAX_NUM_OF_FVDP_HANDLES; i++)
        {
            if ((FVDP_HandleMap[i].State == FVDP_HANDLE_OPENED) && (FVDP_HandleMap[i].Ch == CH))
            {
                TRC( TRC_ID_MAIN_INFO, "Error:  FVDP Channel (%d) does not support multiple handles.", CH );
                return FVDP_ERROR;
            }
        }
    }

    // Go through the Handle map and find an unused (closed) handle.
    for (i = 0; i  < MAX_NUM_OF_FVDP_HANDLES; i++)
    {
        if (FVDP_HandleMap[i].State == FVDP_HANDLE_CLOSED)
        {
            // Set the Handle number
            Handle = i;
            break;
        }
    }

    if (i >= MAX_NUM_OF_FVDP_HANDLES)
    {
        TRC( TRC_ID_MAIN_INFO, "Error:  Reached MAX number of FVDP handles." );
        return FVDP_ERROR;
    }

    // Allocate memory for a new FVDP_HandleContext_t structure
    HandleContext_p = vibe_os_allocate_memory(sizeof(FVDP_HandleContext_t));
    if (HandleContext_p == 0)
    {
        return FVDP_ERROR;
    }

    vibe_os_zero_memory(HandleContext_p, sizeof(FVDP_HandleContext_t) );

    HandleContext_p->FvdpInputLock = vibe_os_create_resource_lock();
    HandleContext_p->FvdpOutputLock = vibe_os_create_resource_lock();

    if((!HandleContext_p->FvdpInputLock) || (!HandleContext_p->FvdpOutputLock))
    {
        if (HandleContext_p->FvdpInputLock)
        {
            vibe_os_delete_resource_lock(HandleContext_p->FvdpInputLock);
        }
        if (HandleContext_p->FvdpOutputLock)
        {
            vibe_os_delete_resource_lock(HandleContext_p->FvdpOutputLock);
        }

        vibe_os_free_memory(HandleContext_p);

        return FVDP_ERROR;
    }

    // Open the Handle
    FVDP_HandleMap[Handle].HandleContext_p = HandleContext_p;
    FVDP_HandleMap[Handle].Ch = CH;
    FVDP_HandleMap[Handle].State = FVDP_HANDLE_OPENED;

    // Initialize the FVDP Handle Context
    HandleContext_p->CH = CH;
    HandleContext_p->FvdpChHandle = Handle;
    HandleContext_p->MemBaseAddress = MemBaseAddress;
    HandleContext_p->MemSize = MemSize;
    HandleContext_p->ProcessingType = Context.ProcessingType;
    HandleContext_p->LatencyType = FVDP_CONSTANT_LATENCY;
    HandleContext_p->MaxOutputWindow.Height = Context.MaxWindowSize.Height;
    HandleContext_p->MaxOutputWindow.Width = Context.MaxWindowSize.Width;
    HandleContext_p->InputSource = FVDP_VMUX_INPUT7;
    ZERO_MEMORY(&(HandleContext_p->InputRasterInfo),  sizeof(FVDP_RasterInfo_t));
    HandleContext_p->UpdateFlags.InputUpdateFlag = 0;
    HandleContext_p->UpdateFlags.CommitUpdateFlag = 0;
    HandleContext_p->QueueBufferInfo.ScalingCompleted = NULL;
    HandleContext_p->QueueBufferInfo.BufferDone = NULL;
    HandleContext_p->QueueBufferInfo.TimerThreadDescr = NULL;
    HandleContext_p->QueueBufferInfo.TimerSemaphore = 0;

    // Perform Channel specific initialization
    if (CH == FVDP_MAIN)
    {
        // Create the Object Pool for the Frame Pool
        HandleContext_p->FrameInfo.FramePool.FramePoolArray = FramePoolArray_MainCh;
        HandleContext_p->FrameInfo.FramePool.FramePoolStates = FramePoolStates_MainCh;
        ObjectPool_Create(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                          HandleContext_p->FrameInfo.FramePool.FramePoolArray,
                          HandleContext_p->FrameInfo.FramePool.FramePoolStates,
                          sizeof(FVDP_Frame_t),
                          MAIN_FRAME_POOL_SIZE);

        // Create and initialize the Buffer Pools
        SYSTEM_InitBufferPools(HandleContext_p);

        // Set the initial datapath
        HandleContext_p->DataPath.DatapathFE = DATAPATH_FE_BYPASS_ALGO_BYPASS_MEM;
        HandleContext_p->DataPath.DatapathBE = DATAPATH_BE_BYPASS_ALGO_BYPASS_MEM;
        DATAPATH_FE_Connect(HandleContext_p->DataPath.DatapathFE);
        DATAPATH_BE_Connect(HandleContext_p->DataPath.DatapathBE);
    }

    else if (CH == FVDP_PIP)
    {
        // Create the Object Pool for the Frame Pool
        HandleContext_p->FrameInfo.FramePool.FramePoolArray = FramePoolArray_PipCh;
        HandleContext_p->FrameInfo.FramePool.FramePoolStates = FramePoolStates_PipCh;
        ObjectPool_Create(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                          HandleContext_p->FrameInfo.FramePool.FramePoolArray,
                          HandleContext_p->FrameInfo.FramePool.FramePoolStates,
                          sizeof(FVDP_Frame_t),
                          PIP_FRAME_POOL_SIZE);

        // Create and initialize the Buffer Pools
        SYSTEM_InitBufferPools(HandleContext_p);

        HandleContext_p->DataPath.DatapathLite = DATAPATH_LITE_NUM;
    }

    else if (CH == FVDP_AUX)
    {
        // Create the Object Pool for the Frame Pool
        HandleContext_p->FrameInfo.FramePool.FramePoolArray = FramePoolArray_AuxCh;
        HandleContext_p->FrameInfo.FramePool.FramePoolStates = FramePoolStates_AuxCh;
        ObjectPool_Create(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                          HandleContext_p->FrameInfo.FramePool.FramePoolArray,
                          HandleContext_p->FrameInfo.FramePool.FramePoolStates,
                          sizeof(FVDP_Frame_t),
                          AUX_FRAME_POOL_SIZE);

        // Create and initialize the Buffer Pools
        SYSTEM_InitBufferPools(HandleContext_p);

        HandleContext_p->DataPath.DatapathLite = DATAPATH_LITE_NUM;
    }
    else // CH == FVDP_ENC
    {
        FVDP_BufferPool_t* BufferPool_p;
        BufferType_t       BufferType;

        // Allocate memory for the Frame Pool
        HandleContext_p->FrameInfo.FramePool.FramePoolArray = vibe_os_allocate_memory(ENC_FRAME_POOL_SIZE * sizeof(FVDP_Frame_t));
        HandleContext_p->FrameInfo.FramePool.FramePoolStates = vibe_os_allocate_memory(ENC_FRAME_POOL_SIZE * sizeof(uint8_t));
        // Create the Object Pool for the Frame Pool
        ObjectPool_Create(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                          HandleContext_p->FrameInfo.FramePool.FramePoolArray,
                          HandleContext_p->FrameInfo.FramePool.FramePoolStates,
                          sizeof(FVDP_Frame_t),
                          ENC_FRAME_POOL_SIZE);

        // Go though all the buffer types and allocated the necessary buffer pools.
        for (BufferType = BUFFER_PRO_Y_G; BufferType <= BUFFER_PRO_U_UV_B; BufferType++)
        {

            // Set the Buffer Pool pointer
            BufferPool_p = &(HandleContext_p->FrameInfo.BufferPool[BufferType]);
            BufferPool_p->NumberOfBuffers = NUM_ENC_BUFFERS;

            // Allocate memory for the Buffer Pools
            BufferPool_p->BufferPoolArray = vibe_os_allocate_memory(BufferPool_p->NumberOfBuffers * sizeof(FVDP_FrameBuffer_t));
            BufferPool_p->BufferPoolStates = vibe_os_allocate_memory(BufferPool_p->NumberOfBuffers * sizeof(uint8_t));

            // Create the buffer pool
            ObjectPool_Create(&(BufferPool_p->BufferObjectPool),
                              BufferPool_p->BufferPoolArray,
                              BufferPool_p->BufferPoolStates,
                              sizeof(FVDP_FrameBuffer_t),
                              BufferPool_p->NumberOfBuffers);
        }

        // Initialize the memory-to-memory callback functions
        HandleContext_p->QueueBufferInfo.ScalingCompleted = NULL;
        HandleContext_p->QueueBufferInfo.BufferDone = NULL;

        HandleContext_p->DataPath.DatapathLite = DATAPATH_LITE_NUM;
    }

    // Initialize the Frame pointers
    HandleContext_p->FrameInfo.NextInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.PrevProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev1InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev2InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev3InputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentOutputFrame_p = NULL;

    vibe_os_zero_memory(&HandleContext_p->FrameInfo.PrevOutputFrame,sizeof(FVDP_Frame_t));
    HandleContext_p->FrameInfo.PrevOutputFrame.FrameID = INVALID_FRAME_ID;

    // Allocate the Next Frame
    HandleContext_p->FrameInfo.NextInputFrame_p = SYSTEM_AllocateFrame(HandleContext_p);
    if (HandleContext_p->FrameInfo.NextInputFrame_p == NULL)
    {
        return FVDP_ERROR;
    }

    // Start the VCPU - load the code and start execution.
#ifdef USE_VCPU
    if (CH != FVDP_ENC)
    {
        if (VCPU_Initialized() == FALSE)
            SYSTEM_InitVCPU();

        if (SYSTEM_StartVCPU() != FVDP_OK)
        {
            FVDP_HandleMap[Handle].State = FVDP_HANDLE_CLOSED;
            return FVDP_ERROR;
        }

        {
            SendBufferTableCommand_t Command;
            Command.Channel = CH;
            FBM_IF_CreateBufferTable(&(HandleContext_p->FrameInfo),
                                     (BufferTable_t*)&Command.Table[0],
                                     &Command.Rows);
            FBM_IF_SendBufferTable(&Command);
        }
    }
#endif

    SYSTEM_InitPsiControlData(&HandleContext_p->PSIControlLevel);

    //
    // HW Initialization
    //

    // Initialize the VMUX HW
    SYSTEM_VMUX_Init(CH);

    if (CH == FVDP_MAIN)
    {
        //Initialize FVDP_FE software module and call FVDP_FE hardware init function
        SYSTEM_FE_Init(HandleContext_p);

        //Initialize FVDP_BE software module and call FVDP_BE hardware init function
        SYSTEM_BE_Init(HandleContext_p);
    }
    else if((CH == FVDP_PIP)|| (CH == FVDP_AUX))
    {
        //Initialize FVDP_LLYE software module and call FVDP_LITE hardware init function
        SYSTEM_LITE_Init(HandleContext_p);
    }
    else if (CH == FVDP_ENC)
    {
        SYSTEM_ENC_Open(HandleContext_p);
    }

    // Initialize the MCC HW
    MCC_Init(CH);

    // Host Update for FVDP_FE/BE or FVDP_LITE depending on CH
    if (CH == FVDP_MAIN)
    {
        HostUpdate_HardwareUpdate(CH, HOST_CTRL_FE, SYNC_UPDATE);
        HostUpdate_HardwareUpdate(CH, HOST_CTRL_BE, SYNC_UPDATE);
    }
    else
    {
        HostUpdate_HardwareUpdate(CH, HOST_CTRL_LITE, SYNC_UPDATE);
    }

    *FvdpChHandle_p = Handle;
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
FVDP_HandleContext_t* SYSTEM_GetHandleContext(FVDP_CH_Handle_t Handle)
{
    if (Handle < MAX_NUM_OF_FVDP_HANDLES)
        return FVDP_HandleMap[Handle].HandleContext_p;
    else
        return NULL;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
bool SYSTEM_IsChannelHandleOpen(FVDP_CH_Handle_t Handle)
{
    if (Handle >= MAX_NUM_OF_FVDP_HANDLES)
        return FALSE;

    if (FVDP_HandleMap[Handle].State == FVDP_HANDLE_OPENED)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
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
FVDP_Result_t SYSTEM_FlushChannel(FVDP_CH_Handle_t FvdpChHandle, bool bFlushCurrentNode)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Result_t         ErrorCode = FVDP_OK;
    FVDP_CH_t             CH;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_QUEUE_FLUSH, 1);

    CH = HandleContext_p->CH;

    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpInputLock);
    SYSTEM_os_lock_resource(CH, HandleContext_p->FvdpOutputLock);

    if(HandleContext_p->CH != FVDP_ENC)
    {
        if(bFlushCurrentNode == TRUE)
        {
            if(HandleContext_p->CH == FVDP_MAIN)
            {
                Csc_Mute( MC_BLACK);
            }
            else
            {
                CscLight_Mute(HandleContext_p->CH, MC_BLACK);
            }
        }
        // Reset the FBM in the VCPU
        ErrorCode = FBM_Reset(HandleContext_p, bFlushCurrentNode);
        if (ErrorCode != FVDP_OK)
        {
            ErrorCode = FVDP_ERROR;
            goto SYSTEM_FlushChannel_exit;
        }
        if (bFlushCurrentNode == FALSE)
        {
            goto SYSTEM_FlushChannel_exit;
        }
    }
    else
    {
        SYSTEM_ENC_FlushChannel(HandleContext_p);
    }

    HandleContext_p->UpdateFlags.InputUpdateFlag &= UPDATE_INPUT_SOURCE;
    HandleContext_p->UpdateFlags.OutputUpdateFlag = 0;

    // Reset the Object Pool for the Frame Pool
    ObjectPool_Reset(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool));

    // Initialize the Frame pointers
    HandleContext_p->FrameInfo.NextInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.PrevProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev1InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev2InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev3InputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentOutputFrame_p = NULL;

    vibe_os_zero_memory(&HandleContext_p->FrameInfo.PrevOutputFrame,sizeof(FVDP_Frame_t));
    HandleContext_p->FrameInfo.PrevOutputFrame.FrameID = INVALID_FRAME_ID;

    // Allocate the Next Frame
    HandleContext_p->FrameInfo.NextInputFrame_p = SYSTEM_AllocateFrame(HandleContext_p);
    if (HandleContext_p->FrameInfo.NextInputFrame_p == NULL)
    {
        ErrorCode = FVDP_ERROR;
    }
    else
    {
        ErrorCode = FVDP_OK;
    }

SYSTEM_FlushChannel_exit:

    SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpOutputLock);
    SYSTEM_os_unlock_resource(CH, HandleContext_p->FvdpInputLock);

    VCD_ValueChange(FVDP_MAIN, VCD_EVENT_QUEUE_FLUSH, 0);

    return ErrorCode;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void SYSTEM_CloseChannel(FVDP_CH_Handle_t Handle)
{
    if (Handle < MAX_NUM_OF_FVDP_HANDLES)
    {
        if (FVDP_HandleMap[Handle].State == FVDP_HANDLE_OPENED)
        {
            FVDP_HandleContext_t* HandleContext_p = FVDP_HandleMap[Handle].HandleContext_p;
            if (HandleContext_p != NULL)
            {
                // delete lock resource
                vibe_os_delete_resource_lock(HandleContext_p->FvdpInputLock);
                vibe_os_delete_resource_lock(HandleContext_p->FvdpOutputLock);

                // Free the buffer pool memory
                SYSTEM_FreeBufferPoolsMemory(HandleContext_p);
                if(FVDP_HandleMap[Handle].HandleContext_p->CH == FVDP_ENC)
                {
                    SYSTEM_ENC_Close(FVDP_HandleMap[Handle].HandleContext_p);
                }
                // Free the Context Handle memory
                vibe_os_free_memory(FVDP_HandleMap[Handle].HandleContext_p);
            }
            FVDP_HandleMap[Handle].HandleContext_p = NULL;
            FVDP_HandleMap[Handle].State = FVDP_HANDLE_CLOSED;
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
FVDP_Frame_t* SYSTEM_AllocateFrame(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t* Frame;
    uint32_t      Index;

    Frame = ObjectPool_Alloc_Object(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool), 1, &Index);
    if (Frame == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  Failed to allocate frame. (FVDP_CH_Handle_t = %d)", HandleContext_p->FvdpChHandle );
        return NULL;
    }

    Frame->FramePoolIndex = Index;
    ZERO_MEMORY(Frame->FrameBuffer, sizeof(Frame->FrameBuffer));
    return Frame;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_CloneFrame(FVDP_HandleContext_t* HandleContext_p,
                                FVDP_Frame_t*         SourceFrame_p)
{
    FVDP_Frame_t* Frame;

    if (SourceFrame_p == NULL)
    {
        return NULL;
    }

    // Allocate Frame
    Frame = SYSTEM_AllocateFrame(HandleContext_p);
    if (Frame == NULL)
    {
        return NULL;
    }

    // Clone the Source Frame contents
    Frame->Timestamp          = SourceFrame_p->Timestamp;
    Frame->InputVideoInfo     = SourceFrame_p->InputVideoInfo;
    Frame->OutputVideoInfo    = SourceFrame_p->OutputVideoInfo;
    Frame->OutputActiveWindow = SourceFrame_p->OutputActiveWindow;
    Frame->CropWindow         = SourceFrame_p->CropWindow;
    Frame->BWOptions          = SourceFrame_p->BWOptions;

    return Frame;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void SYSTEM_FreeFrame(FVDP_HandleContext_t* HandleContext_p, FVDP_Frame_t* Frame_p)
{
    BufferType_t BufferType = 0;

    if ((HandleContext_p == NULL) || (Frame_p == NULL))
        return;

    // Free the Frame Buffer objects associated with this Frame.
    for (BufferType = 0; BufferType < NUM_BUFFER_TYPES; BufferType++)
    {
        if (Frame_p->FrameBuffer[BufferType] != NULL)
        {
            ObjectPool_Free_Object(&(HandleContext_p->FrameInfo.BufferPool[BufferType].BufferObjectPool),
                                   (void*)Frame_p->FrameBuffer[BufferType],
                                   Frame_p->FrameBuffer[BufferType]->NumOfBuffers);
        }
    }

    ZERO_MEMORY(Frame_p->FrameBuffer, sizeof(Frame_p->FrameBuffer));
    ObjectPool_Free_Object(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                           (void*)Frame_p, 1);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
bool SYSTEM_IsFrameValid(FVDP_HandleContext_t* HandleContext_p, FVDP_Frame_t* Frame_p)
{
    uint8_t ObjectState;

    if ((HandleContext_p == NULL) || (Frame_p == NULL))
    {
        return FALSE;
    }

    if (Frame_p->FrameID == INVALID_FRAME_ID)
    {
        return FALSE;
    }
    // a frame is invalid if it can no longer be found in the FramePool
    ObjectState = HandleContext_p->FrameInfo.FramePool.FramePoolStates[Frame_p->FramePoolIndex];
    return (ObjectState == OBJECT_USED);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetFrameFromIndex(FVDP_HandleContext_t* HandleContext_p, uint8_t Index)
{
    FVDP_FramePool_t*  FramePool_p;
    FramePool_p = &(HandleContext_p->FrameInfo.FramePool);

    // Check that the Frame Pool index is within the Frame Pool size
    if (Index >= FramePool_p->FrameObjectPool.pool_size)
    {
        return NULL;
    }

    if (FramePool_p->FramePoolStates[Index] == OBJECT_FREE)
    {
        return NULL;
    }

    return &(FramePool_p->FramePoolArray[Index]);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_FrameBuffer_t* SYSTEM_AllocateBufferFromIndex(FVDP_HandleContext_t* HandleContext_p,
                                                   BufferType_t          BufferType,
                                                   uint8_t               Index,
                                                   FVDP_ScanType_t       ScanType)
{
    FVDP_BufferPool_t*  BufferPool_p;
    FVDP_FrameBuffer_t* FrameBuffer_p;
    uint32_t            NumOfBuffersToAllocate;

    // Check that the Buffer Type is valid
    if (BufferType >= NUM_BUFFER_TYPES)
    {
        return NULL;
    }

    // Get the Buffer Pool pointer for this buffer type
    BufferPool_p = &(HandleContext_p->FrameInfo.BufferPool[BufferType]);

    // C_Y and C_UV buffer tables are for Fields, so in Progressive case
    // we need to allocate 2 buffers
    if ( ((BufferType == BUFFER_C_Y) || (BufferType == BUFFER_C_UV)) &&
         (ScanType == PROGRESSIVE))
    {
        NumOfBuffersToAllocate = 2;
    }
    else
    {
        NumOfBuffersToAllocate = 1;
    }

    // Check that the Buffer Pool index is within the Buffer Pool size
    if (Index >= BufferPool_p->BufferObjectPool.pool_size)
    {
        return NULL;
    }

    FrameBuffer_p = ObjectPool_Alloc_Object_From_Index(&(BufferPool_p->BufferObjectPool), NumOfBuffersToAllocate, Index);

    if (FrameBuffer_p != NULL)
    {
        FrameBuffer_p->NumOfBuffers = NumOfBuffersToAllocate;
    }

    return FrameBuffer_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetCurrentInputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.CurrentInputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetCurrentProScanFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.CurrentProScanFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void SYSTEM_SetCurrentProScanFrame(FVDP_HandleContext_t* HandleContext_p,
                                   FVDP_Frame_t* CurrentProScanFrame_p)
{
    HandleContext_p->FrameInfo.CurrentProScanFrame_p = CurrentProScanFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetPrevProScanFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.PrevProScanFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetNextInputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.NextInputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void SYSTEM_SetNextInputFrame(FVDP_HandleContext_t* HandleContext_p,
                              FVDP_Frame_t* NextInputFrame_p)
{
    HandleContext_p->FrameInfo.NextInputFrame_p = NextInputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetPrev1InputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.Prev1InputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetPrev2InputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.Prev2InputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetPrev3InputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.Prev3InputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetCurrentOutputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return HandleContext_p->FrameInfo.CurrentOutputFrame_p;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Frame_t* SYSTEM_GetPrevOutputFrame(FVDP_HandleContext_t* HandleContext_p)
{
    return &(HandleContext_p->FrameInfo.PrevOutputFrame);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_GetRequiredVideoMemorySize(FVDP_CH_t              CH,
                                                FVDP_ProcessingType_t  ProcessingType,
                                                FVDP_MaxOutputWindow_t MaxOutputWindow,
                                                uint32_t*              MemSize)
{
    uint32_t buffer_size               = 0;
    uint32_t pro_video_buffer_size     = 0;
    uint32_t c_video_buffer_size       = 0;
    uint32_t ccs_raw_video_buffer_size = 0;
    uint32_t mcdi_sb_video_buffer_size = 0;
    uint32_t num_of_channels;
    uint32_t num_of_pro_buffers;

    if (CH == FVDP_MAIN)
    {
        // For MAIN channel, worst case is 1080i YUV input.
        num_of_channels = 2;
        num_of_pro_buffers = NUM_PRO_VIDEO_BUFFERS;
    }
    else if (CH == FVDP_PIP)
    {
        // For PIP channel, worst case is WUXGA RGB input
        num_of_channels = 3;
        num_of_pro_buffers = NUM_PIP_BUFFERS;
    }
    else if (CH == FVDP_AUX)
    {
        // For AUX channel, worst case is WUXGA RGB input
        num_of_channels = 3;
        num_of_pro_buffers = NUM_AUX_BUFFERS;
    }
    else
    {
        // ENC channel does not require allocation of internal FVDP video memory buffers
        *MemSize = 0;
        return FVDP_OK;
    }

    // Get the buffer size for the PRO MCC clients
    buffer_size = SYSTEM_GetVideoBufferSize(MaxOutputWindow.Width,
                                            MaxOutputWindow.Height,
                                            MAX_BPP_PER_CHANNEL);
    pro_video_buffer_size = buffer_size * num_of_channels * num_of_pro_buffers;

    if ((CH == FVDP_MAIN) && (ProcessingType == FVDP_TEMPORAL))
    {
        // Get the buffer size for the C MCC clients.
        // The worst case is based on 1080i input
        buffer_size = SYSTEM_GetVideoBufferSize(MaxOutputWindow.Width,
                                                INTERLACED_INPUT_MAX_HEIGHT,
                                                MAX_BPP_PER_CHANNEL);
        c_video_buffer_size = buffer_size * 2 * NUM_C_VIDEO_BUFFERS;


        // Get the buffer size for the CCS_RAW MCC clients
        buffer_size = SYSTEM_GetVideoBufferSize(MaxOutputWindow.Width,
                                                MaxOutputWindow.Height,
                                                MAX_BPP_PER_CHANNEL);
        ccs_raw_video_buffer_size = buffer_size * NUM_CCS_RAW_BUFFERS;


        // Get the buffer size for the MCDI_SB MCC clients
        // MCDI buffer are only valid for interlaced input
        buffer_size = SYSTEM_GetVideoBufferSize(MaxOutputWindow.Width,
                                                INTERLACED_INPUT_MAX_HEIGHT,
                                                MCDI_BPP_PER_CHANNEL);
        mcdi_sb_video_buffer_size = buffer_size * NUM_MCDI_SB_BUFFERS;
    }

    *MemSize = pro_video_buffer_size + c_video_buffer_size + ccs_raw_video_buffer_size + mcdi_sb_video_buffer_size;

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
static void SYSTEM_FreeBufferPoolsMemory(FVDP_HandleContext_t* HandleContext_p)
{
    BufferType_t       BufferType = 0;
    FVDP_BufferPool_t* BufferPool_p;

    // Go though all the buffer types and free the buffer pools.
    for (BufferType = 0; BufferType < NUM_BUFFER_TYPES; BufferType++)
    {
        // Set the Buffer Pool pointer based on buffer type
        BufferPool_p = &(HandleContext_p->FrameInfo.BufferPool[BufferType]);

        // Free the Buffer Pools memory
        vibe_os_free_memory(BufferPool_p->BufferPoolArray);
        vibe_os_free_memory(BufferPool_p->BufferPoolStates);

        BufferPool_p->BufferPoolArray = NULL;
        BufferPool_p->BufferPoolStates = NULL;
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
static void SYSTEM_InitBufferPools(FVDP_HandleContext_t* HandleContext_p)
{
    BufferType_t       BufferType = 0;
    FVDP_BufferPool_t* BufferPool_p;
    uint32_t           BufferBaseAddr;
    uint8_t            i;
    uint32_t          BitPerPixel = MAX_BPP_PER_CHANNEL;

    // The buffer base addresses starts at the memory base address provided to this handle.
    BufferBaseAddr = HandleContext_p->MemBaseAddress;

    // Go though all the buffer types and allocated the necessary buffer pools.
    for (BufferType = 0; BufferType < NUM_BUFFER_TYPES; BufferType++)
    {
        // Set the Buffer Pool pointer based on buffer type
        BufferPool_p = &(HandleContext_p->FrameInfo.BufferPool[BufferType]);

        // For SPATIAL processing, only BUFFER_PRO_Y_G and BUFFER_PRO_U_UV_B are needed -- skip all other buffers.
        if ((HandleContext_p->ProcessingType == FVDP_SPATIAL) && (HandleContext_p->CH !=  FVDP_MAIN) &&
            ((BufferType != BUFFER_PRO_Y_G) && (BufferType != BUFFER_PRO_U_UV_B)))
        {
            BufferPool_p->NumberOfBuffers = 0;
            BufferPool_p->BufferSize = 0;
            BufferPool_p->BufferStartAddr = 0;
            continue;
        }

        // Calculate the buffer size and set the number of buffers
        if (BufferType == BUFFER_CCS_UV_RAW)
        {
            BufferPool_p->NumberOfBuffers = NUM_CCS_RAW_BUFFERS;
            BufferPool_p->BufferSize = SYSTEM_GetVideoBufferSize(HandleContext_p->MaxOutputWindow.Width,
                                                                 HandleContext_p->MaxOutputWindow.Height,
                                                                 MAX_BPP_PER_CHANNEL);
        }
        if ((BufferType == BUFFER_C_Y) || (BufferType == BUFFER_C_UV))
        {
            BufferPool_p->NumberOfBuffers = NUM_C_VIDEO_BUFFERS;
            BufferPool_p->BufferSize = SYSTEM_GetVideoBufferSize(HandleContext_p->MaxOutputWindow.Width,
                                                                 INTERLACED_INPUT_MAX_HEIGHT,
                                                                 MAX_BPP_PER_CHANNEL);
        }
        if (BufferType == BUFFER_MCDI_SB)
        {
            BufferPool_p->NumberOfBuffers = NUM_MCDI_SB_BUFFERS;
            BufferPool_p->BufferSize = SYSTEM_GetVideoBufferSize(HandleContext_p->MaxOutputWindow.Width,
                                                                 INTERLACED_INPUT_MAX_HEIGHT,
                                                                 MCDI_BPP_PER_CHANNEL);
        }
        if ((BufferType == BUFFER_PRO_Y_G) || (BufferType == BUFFER_PRO_U_UV_B))
        {
            if (HandleContext_p->CH == FVDP_PIP)
                BufferPool_p->NumberOfBuffers = NUM_PIP_BUFFERS+1;
            else
            if (HandleContext_p->CH == FVDP_AUX)
                BufferPool_p->NumberOfBuffers = NUM_AUX_BUFFERS+1;
            else // FVDP_MAIN
            {
                if(HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC)
                {
                    if(BufferType == BUFFER_PRO_U_UV_B)
                    {
                        BufferPool_p->NumberOfBuffers = NUM_PRO_RGB_BUFFERS;
                        BitPerPixel = MAX_BPP_PER_CHANNEL * 3;
                    }
                    else
                    {
                        BufferPool_p->NumberOfBuffers = 0;
                    }
                }
                else
                {
                    BufferPool_p->NumberOfBuffers = NUM_PRO_VIDEO_BUFFERS;
                }
             }

            if(BufferPool_p->NumberOfBuffers != 0)
            {
                BufferPool_p->BufferSize = SYSTEM_GetVideoBufferSize(HandleContext_p->MaxOutputWindow.Width,
                                                                                                        HandleContext_p->MaxOutputWindow.Height,
                                                                                                        BitPerPixel);
            }
        }

        // Allocate memory for the Buffer Pools
        BufferPool_p->BufferPoolArray = vibe_os_allocate_memory(BufferPool_p->NumberOfBuffers * sizeof(FVDP_FrameBuffer_t));
        BufferPool_p->BufferPoolStates = vibe_os_allocate_memory(BufferPool_p->NumberOfBuffers * sizeof(uint8_t));

        // Create the buffer pool
        ObjectPool_Create(&(BufferPool_p->BufferObjectPool),
                          BufferPool_p->BufferPoolArray,
                          BufferPool_p->BufferPoolStates,
                          sizeof(FVDP_FrameBuffer_t),
                          BufferPool_p->NumberOfBuffers);

        // Set the memory address for each buffer
        BufferPool_p->BufferStartAddr = BufferBaseAddr;
        for (i = 0; i < BufferPool_p->NumberOfBuffers; i++)
        {
            BufferPool_p->BufferPoolArray[i].Addr = BufferBaseAddr;
            BufferBaseAddr += BufferPool_p->BufferSize;
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
static uint32_t SYSTEM_GetVideoBufferSize(uint32_t Width, uint32_t Height, uint32_t BppPerChannel)
{
    uint32_t buffer_size;
    uint32_t packets_per_line;

    packets_per_line = ((Width * BppPerChannel) + (BITS_PER_PACKET - 1)) / BITS_PER_PACKET;
    buffer_size = packets_per_line * BYTES_PER_PACKET * Height;

    return buffer_size;
}

#ifdef USE_VCPU
/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_InitVCPU(void)
{
    FVDP_Result_t result = FVDP_OK;

    // Initialize mailbox system
    MBOX_Init();

    // Display FVDP version
    TRC( TRC_ID_MAIN_INFO, "FVDP VERSION: %s", FVDP_ReleaseID );

    {
        extern const uint32_t vcpu_checksum_exp;
        uint32_t chksum;

        // Initialize VCPU and load initial data and code
        if ((result = VCPU_Init(vcpu_dram_data, vcpu_iram_code)) != FVDP_OK)
        {
            TRC( TRC_ID_MAIN_INFO, "VCPU code load error %d, ABORTED", result );
            return result;
        }

        // Verify that data and code has been loaded correctly
        result = VCPU_CheckLoading(vcpu_dram_data, vcpu_iram_code, &chksum);
        if (result != FVDP_OK)
        {
            TRC( TRC_ID_MAIN_INFO, "VCPU verify error %d, ABORTED", result );
            return result;
        }

        // Calculate and display VCPU checksum
        if (vcpu_checksum_exp == chksum)
        {
            TRC( TRC_ID_MAIN_INFO, "VCPU CHCKSUM: 0x%08x (OK)", chksum );
        }
        else
        {
            TRC( TRC_ID_MAIN_INFO, "VCPU CHCKSUM: 0x%08x (WARNING: should be 0x%08x!!)",  chksum, vcpu_checksum_exp );
        }
    }

    return result;
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
static FVDP_Result_t SYSTEM_StartVCPU(void)
{
    FVDP_Result_t result = FVDP_OK;

    // start the VCPU here if not done so already.  Note that this must
    // be done last to ensure that no VCPU programming is overwritten
    // by the HOST (eg. MOT_DET_HARD_RESET is triggered in motdet_HwInit(),
    // and MIVP_HARD_RESET in ivp_HwInit()).
    if (VCPU_Initialized() == TRUE && VCPU_Started() == FALSE)
    {
        result = VCPU_Start();
        if (result != FVDP_OK)
        {
            TRC( TRC_ID_MAIN_INFO, "VCPU code start error %d, ABORTED", result );
            return result;
        }
    }

    return result;
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
FVDP_Result_t SYSTEM_CopyColorControlData(const FVDP_HandleContext_t* HandleContext_p,
                                           ColorMatrix_Adjuster_t    * ColorCtrl)
{
    FVDP_Result_t result = FVDP_OK;

    ColorCtrl->Brightness  = HandleContext_p->PSIControlLevel.BrightnessLevel;
    ColorCtrl->Contrast    = HandleContext_p->PSIControlLevel.ContrastLevel;
    ColorCtrl->Saturation  = HandleContext_p->PSIControlLevel.SaturationLevel;
    ColorCtrl->Hue         = HandleContext_p->PSIControlLevel.HueLevel;
    ColorCtrl->RedGain     = HandleContext_p->PSIControlLevel.RedGain;
    ColorCtrl->GreenGain   = HandleContext_p->PSIControlLevel.GreenGain;
    ColorCtrl->BlueGain    = HandleContext_p->PSIControlLevel.BlueGain;
    ColorCtrl->RedOffset   = HandleContext_p->PSIControlLevel.RedOffset;
    ColorCtrl->GreenOffset = HandleContext_p->PSIControlLevel.GreenOffset;
    ColorCtrl->BlueOffset  = HandleContext_p->PSIControlLevel.BlueOffset;
    return result;
}

/*******************************************************************************
Name        : SYSTEM_InitMCC
Description : Initialize FVDP MCC base addresses

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_InitMCC(FVDP_HandleContext_t* HandleContext_p)
{
    const MCCClientToBufferTypeMap_t * Mapping_p;

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_Main[] =
    {
        { CCS_WR,               BUFFER_CCS_UV_RAW   },
        { CCS_RD,               BUFFER_CCS_UV_RAW   },
        { P1_Y_RD,              BUFFER_C_Y          },
        { P2_Y_RD,              BUFFER_C_Y          },
        { P3_Y_RD,              BUFFER_C_Y          },
        { P1_UV_RD,             BUFFER_C_UV         },
        { P2_UV_RD,             BUFFER_C_UV         },
        { P3_UV_RD,             BUFFER_C_UV         },
        { D_Y_WR,               BUFFER_C_Y          },
        { D_UV_WR,              BUFFER_C_UV         },
        { MCDI_WR,              BUFFER_MCDI_SB      },
        { MCDI_RD,              BUFFER_MCDI_SB      },
        { C_Y_WR,               BUFFER_PRO_Y_G      },
        { C_RGB_UV_WR,          BUFFER_PRO_U_UV_B   },
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_AUX[] =
    {
        { AUX_RGB_UV_WR,        BUFFER_PRO_U_UV_B   },
        { AUX_Y_WR,             BUFFER_PRO_Y_G      },
        { AUX_RGB_UV_RD,        BUFFER_PRO_U_UV_B   },
        { AUX_Y_RD,             BUFFER_PRO_Y_G      },
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_PIP[] =
    {
        { PIP_RGB_UV_WR,        BUFFER_PRO_U_UV_B   },
        { PIP_Y_WR,             BUFFER_PRO_Y_G      },
        { PIP_RGB_UV_RD,        BUFFER_PRO_U_UV_B   },
        { PIP_Y_RD,             BUFFER_PRO_Y_G      },
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    const MCCClientToBufferTypeMap_t ClientToBufferMapping_ENC[] =
    {
        { ENC_RGB_UV_WR,        BUFFER_PRO_U_UV_B   },
        { ENC_Y_WR,             BUFFER_PRO_Y_G      },
        { ENC_RGB_UV_RD,        BUFFER_PRO_U_UV_B   },
        { ENC_Y_RD,             BUFFER_PRO_Y_G      },
    //  { ENC_TNR_Y_RD,         BUFFER_TNR_Y        },
    //  { ENC_TNR_UV_RD,        BUFFER_TNR_UV       },
        { MCC_NUM_OF_CLIENTS,   NUM_BUFFER_TYPES    },
    };

    if (HandleContext_p->CH == FVDP_MAIN)
        Mapping_p = &(ClientToBufferMapping_Main[0]);
    else
    if (HandleContext_p->CH == FVDP_PIP)
        Mapping_p = &(ClientToBufferMapping_PIP[0]);
    else
    if (HandleContext_p->CH == FVDP_AUX)
        Mapping_p = &(ClientToBufferMapping_AUX[0]);
    else
    if (HandleContext_p->CH == FVDP_ENC)
        Mapping_p = &(ClientToBufferMapping_ENC[0]);
    else
        return FVDP_ERROR;

    while (Mapping_p->MCC_Client != MCC_NUM_OF_CLIENTS)
    {
        BufferType_t        BufferType   = Mapping_p->BufferType;
        FVDP_BufferPool_t * BufferPool_p = &HandleContext_p->FrameInfo.BufferPool[BufferType];
        MCC_Config_t        Config       = { .crop_enable = FALSE };
        MCC_SetBaseAddr(Mapping_p->MCC_Client, BufferPool_p->BufferPoolArray[0].Addr, Config);
        Mapping_p ++;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_os_lock_resource
Description : Disable interrupts locally and provide the spinlock on SMP

Parameters  : CH: channel,  FvdpLock:.handle of lock resource
Assumptions : Lock resource need to be assigned by calling vibe_os_create_resource_lock
Limitations :
Returns     : if handle is NULL return FVDP_ERROR:, other case return FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_os_lock_resource ( FVDP_CH_t        CH, void * FvdpLock )
{
    if (FvdpLock == NULL)
    {
        return FVDP_ERROR;
    }

    if (CH != FVDP_ENC)
    {
        vibe_os_lock_resource(FvdpLock);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_os_unlock_resource
Description : releases the spinlock and restores the interrupts

Parameters  : CH: channel,  FvdpLock:.handle of lock resource
Assumptions : Lock resource need to be assigned by calling vibe_os_create_resource_lock
Limitations :
Returns     : if handle is NULL return FVDP_ERROR:, other case return FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_os_unlock_resource ( FVDP_CH_t      CH, void * FvdpLock )
{
    if (FvdpLock == NULL)
    {
        return FVDP_ERROR;
    }

    if (CH != FVDP_ENC)
    {
        vibe_os_unlock_resource(FvdpLock);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        :SYSTEM_GetHWPSIConfig
Description :return adress of HW PSI config
Parameters  :CH : channel

Assumptions :
Limitations :
Returns     : adress of HW_PSIConfig of HW_Config[]
*******************************************************************************/
FVDP_HW_PSIConfig_t* SYSTEM_GetHWPSIConfig(FVDP_CH_t CH)
{
    return &(HW_Config[CH].HW_PSIConfig);
}
/*******************************************************************************
Name        : SYSTEM_GetHandleContextOfChannel
Description : Return handle context of phisical channel
Parameters  : FVDP_CH_t CH

Assumptions :
Limitations :
Returns     : HandleContext
*******************************************************************************/
FVDP_HandleContext_t* SYSTEM_GetHandleContextOfChannel(FVDP_CH_t CH)
{
    uint32_t i;

    for (i = 0; i  < MAX_NUM_OF_FVDP_HANDLES; i++)
    {
        if ((FVDP_HandleMap[i].State == FVDP_HANDLE_OPENED) && (FVDP_HandleMap[i].Ch == CH))
        {
            return FVDP_HandleMap[i].HandleContext_p;
        }
    }

    return NULL;
}

/*******************************************************************************
Name        :SYSTEM_InitBufferTable
Description :create BufferTable and send buffer informationt to VCPU
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_InitBufferTable(void)
{
    FVDP_HandleContext_t* HandleContext_p;
    SendBufferTableCommand_t Command;
    uint32_t i;

    for (i = 0; i  < MAX_NUM_OF_FVDP_HANDLES; i++)
    {
        if ((FVDP_HandleMap[i].State == FVDP_HANDLE_OPENED) && (FVDP_HandleMap[i].Ch != FVDP_ENC))
        {
             HandleContext_p = FVDP_HandleMap[i].HandleContext_p;
             Command.Channel = HandleContext_p->CH;
             // Initialize the Frame pointers
             HandleContext_p->FrameInfo.NextInputFrame_p = NULL;
             HandleContext_p->FrameInfo.CurrentInputFrame_p = NULL;
             HandleContext_p->FrameInfo.CurrentProScanFrame_p = NULL;
             HandleContext_p->FrameInfo.PrevProScanFrame_p = NULL;
             HandleContext_p->FrameInfo.Prev1InputFrame_p = NULL;
             HandleContext_p->FrameInfo.Prev2InputFrame_p = NULL;
             HandleContext_p->FrameInfo.Prev3InputFrame_p = NULL;
             HandleContext_p->FrameInfo.CurrentOutputFrame_p = NULL;

             vibe_os_zero_memory(&HandleContext_p->FrameInfo.PrevOutputFrame,sizeof(FVDP_Frame_t));
             HandleContext_p->FrameInfo.PrevOutputFrame.FrameID = INVALID_FRAME_ID;

             // Allocate the Next Frame
             HandleContext_p->FrameInfo.NextInputFrame_p = SYSTEM_AllocateFrame(HandleContext_p);

             FBM_IF_CreateBufferTable(&(HandleContext_p->FrameInfo),
                                      (BufferTable_t*)&Command.Table[0],
                                      &Command.Rows);
             FBM_IF_SendBufferTable(&Command);

             HandleContext_p->UpdateFlags.InputUpdateFlag |= UPDATE_INPUT_SOURCE;
             HandleContext_p->UpdateFlags.OutputUpdateFlag |= UPDATE_OUTPUT_RASTER_INFO;
             if(HandleContext_p->CH == FVDP_MAIN)
             {
                 HandleContext_p->DataPath.DatapathFE = DATAPATH_FE_NUM;
                 HandleContext_p->DataPath.DatapathBE = DATAPATH_BE_NUM;
             }
             else
             {
                 HandleContext_p->DataPath.DatapathLite = DATAPATH_LITE_NUM;
             }

         }
     }

    return FVDP_OK;
}

/*******************************************************************************
Name        :SYSTEM_SetPowerStateUpdate
Description :Set power state (RESUME, SUSPEND) for the all channel
Parameters  :PowerState

Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t SYSTEM_SetPowerStateUpdate(FVDP_Power_State_t FvdpPowerState)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    Power_State_t PowerState = UNKNOWN_POWER_STATE;
    uint32_t      i;

   if(((FVDPDriverPowerState  == RESUME_POWER_STATE) && (FvdpPowerState == FVDP_RESUME))
        || ((FVDPDriverPowerState  == SUSPEND_POWER_STATE) && (FvdpPowerState == FVDP_SUSPEND)))
    {
      // Already fvdp driver is in same required power state.
       return ErrorCode;
    }
    // Power down all IP module
    if(FvdpPowerState == FVDP_SUSPEND)
    {
        PowerState= SUSPEND_POWER_STATE;

        // Power down FVDP_FE block
        ErrorCode = SYSTEM_FE_PowerDown();
        if(ErrorCode == FVDP_OK)
        {
            // Power down FVDP_BE block
            ErrorCode = SYSTEM_BE_PowerDown();
            if(ErrorCode == FVDP_OK)
            {
                HW_Config[FVDP_MAIN].PowerState = PowerState;
            }
        }

        // Power down FVDP_PIP block
        ErrorCode = SYSTEM_LITE_PowerDown(FVDP_PIP);
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_PIP].PowerState = PowerState;
        }

        // Power down FVDP_AUX block
        ErrorCode = SYSTEM_LITE_PowerDown(FVDP_AUX);
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_AUX].PowerState = PowerState;
        }

        // Power down FVDP_ENC block
        ErrorCode = SYSTEM_ENC_PowerDown();
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_ENC].PowerState = PowerState;
        }

        //Terminate VCPU
        VCPU_Term();

        //Terminate MBOX
        //ADD Here

    }

    // Power up all IP module
    if(FvdpPowerState == FVDP_RESUME)
    {
        PowerState= RESUME_POWER_STATE;
        // Power up FVDP_FE block
        ErrorCode = SYSTEM_FE_PowerUp();
        if(ErrorCode == FVDP_OK)
        {
            // Power up FVDP_BE block
            ErrorCode = SYSTEM_BE_PowerUp();
            if(ErrorCode == FVDP_OK)
            {
                HW_Config[FVDP_MAIN].PowerState = PowerState;
            }
        }

        // Power up FVDP_PIP block
        ErrorCode = SYSTEM_LITE_PowerUp(FVDP_PIP);
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_PIP].PowerState = PowerState;
        }

        // Power up FVDP_AUX block
        ErrorCode = SYSTEM_LITE_PowerUp(FVDP_AUX);
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_AUX].PowerState = PowerState;
        }

        // Power up FVDP_ENC block
        ErrorCode = SYSTEM_ENC_PowerUp();
        if(ErrorCode == FVDP_OK)
        {
            HW_Config[FVDP_ENC].PowerState = PowerState;
        }

        // Start the VCPU - load the code and start execution.

        if (VCPU_Initialized() == FALSE)
        {
            SYSTEM_InitVCPU();
        }

        ErrorCode = SYSTEM_StartVCPU();

        if(ErrorCode == FVDP_OK)
        {
            SYSTEM_InitBufferTable();
        }

    }

    if((HW_Config[FVDP_MAIN].PowerState == PowerState)
         && (HW_Config[FVDP_PIP].PowerState == PowerState)
         && (HW_Config[FVDP_AUX].PowerState == PowerState)
         && (HW_Config[FVDP_ENC].PowerState == PowerState)
         && ErrorCode == FVDP_OK)
    {
        FVDPDriverPowerState = PowerState;
        ErrorCode = FVDP_OK;
    }
    else
    {
        //Power state control is failed, put power state to unknown
        for (i = 0; i < NUM_FVDP_CH; i++)
        {
            HW_Config[i].PowerState = UNKNOWN_POWER_STATE;
        }
        FVDPDriverPowerState = UNKNOWN_POWER_STATE;
        ErrorCode = FVDP_ERROR;
    }

    return ErrorCode;

}


/*******************************************************************************
Name        :SYSTEM_ReInitFramePool
Description :
Parameters  :

Assumptions : This function is called only for FVDP_MAIN
Limitations :
Returns     : TRUE: FVDP is power on state (RESUME)
              FALSE: FVDP is not power on state
*******************************************************************************/
FVDP_Result_t SYSTEM_ReInitFramePool(FVDP_CH_Handle_t FvdpChHandle, FVDP_ProcessingType_t  VideoProcType)
{
    FVDP_HandleContext_t* HandleContext_p;
    FVDP_Result_t         ErrorCode = FVDP_OK;
    FVDP_CH_t             CH;

    // Get the FVDP Context pointer for this handle
    HandleContext_p = SYSTEM_GetHandleContext(FvdpChHandle);

    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    CH = HandleContext_p->CH;

    if( HandleContext_p->ProcessingType == VideoProcType)
    {
        return FVDP_OK;
    }
    else
    {
        HandleContext_p->ProcessingType = VideoProcType;
    }

    SYSTEM_FreeBufferPoolsMemory(HandleContext_p);

    // Create the Object Pool for the Frame Pool
    HandleContext_p->FrameInfo.FramePool.FramePoolArray = FramePoolArray_MainCh;
    HandleContext_p->FrameInfo.FramePool.FramePoolStates = FramePoolStates_MainCh;
    ObjectPool_Create(&(HandleContext_p->FrameInfo.FramePool.FrameObjectPool),
                      HandleContext_p->FrameInfo.FramePool.FramePoolArray,
                      HandleContext_p->FrameInfo.FramePool.FramePoolStates,
                      sizeof(FVDP_Frame_t),
                      MAIN_FRAME_POOL_SIZE);

    // Create and initialize the Buffer Pools
    SYSTEM_InitBufferPools(HandleContext_p);

    // Initialize the Frame pointers
    HandleContext_p->FrameInfo.NextInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentInputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.PrevProScanFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev1InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev2InputFrame_p = NULL;
    HandleContext_p->FrameInfo.Prev3InputFrame_p = NULL;
    HandleContext_p->FrameInfo.CurrentOutputFrame_p = NULL;

    vibe_os_zero_memory(&HandleContext_p->FrameInfo.PrevOutputFrame,sizeof(FVDP_Frame_t));
    HandleContext_p->FrameInfo.PrevOutputFrame.FrameID = INVALID_FRAME_ID;

    // Allocate the Next Frame
    HandleContext_p->FrameInfo.NextInputFrame_p = SYSTEM_AllocateFrame(HandleContext_p);
    if (HandleContext_p == NULL)
    {
        return FVDP_ERROR;
    }

    // Start the VCPU - load the code and start execution.
#ifdef USE_VCPU
    if (CH != FVDP_ENC)
    {
        SendBufferTableCommand_t Command;
        Command.Channel = CH;
        FBM_IF_CreateBufferTable(&(HandleContext_p->FrameInfo),
                                 (BufferTable_t*)&Command.Table[0],
                                 &Command.Rows);
        FBM_IF_SendBufferTable(&Command);
    }
#endif

    SYSTEM_InitPsiControlData(&HandleContext_p->PSIControlLevel);

    //
    // HW Initialization
    //

    // Initialize the VMUX HW
    SYSTEM_VMUX_Init(CH);

    //Initialize FVDP_FE software module and call FVDP_FE hardware init function
    SYSTEM_FE_Init(HandleContext_p);

    //Initialize FVDP_BE software module and call FVDP_BE hardware init function
    SYSTEM_BE_Init(HandleContext_p);

    // Initialize the MCC HW
    MCC_Init(CH);

    HostUpdate_HardwareUpdate(CH, HOST_CTRL_FE, SYNC_UPDATE);
    HostUpdate_HardwareUpdate(CH, HOST_CTRL_BE, SYNC_UPDATE);

    return ErrorCode;
}


/*******************************************************************************
Name        :SYSTEM_CheckFVDPPowerUpState
Description :Check power state (RESUME) of FVDP
Parameters  :

Assumptions :
Limitations :
Returns     : TRUE: FVDP is power on state (RESUME)
              FALSE: FVDP is not power on state
*******************************************************************************/
bool SYSTEM_CheckFVDPPowerUpState(void)
{
    if(FVDPDriverPowerState == RESUME_POWER_STATE)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/*******************************************************************************
Name        :SYSTEM_CheckPSIColorLevel
Description :Check PSI Color Level
Parameters  :

Assumptions :
Limitations :
Returns     : TRUE: Color level is changed
              FALSE: no change
*******************************************************************************/
bool SYSTEM_CheckPSIColorLevel(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_CH_t     CH = HandleContext_p->CH;
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;
    bool Updateflag = FALSE;
    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);

    if((HandleContext_p->UpdateFlags.CommitUpdateFlag & UPDATE_OUTPUT_PSI) != UPDATE_OUTPUT_PSI)
    {
        return Updateflag;
    }

    if((pHW_PSIConfig->HWPSIControlLevel.BrightnessLevel != HandleContext_p->PSIControlLevel.BrightnessLevel)
        || (pHW_PSIConfig->HWPSIControlLevel.ContrastLevel != HandleContext_p->PSIControlLevel.ContrastLevel)
        || (pHW_PSIConfig->HWPSIControlLevel.SaturationLevel != HandleContext_p->PSIControlLevel.SaturationLevel)
        || (pHW_PSIConfig->HWPSIControlLevel.HueLevel != HandleContext_p->PSIControlLevel.HueLevel)
        || (pHW_PSIConfig->HWPSIControlLevel.RedGain != HandleContext_p->PSIControlLevel.RedGain)
        || (pHW_PSIConfig->HWPSIControlLevel.GreenGain != HandleContext_p->PSIControlLevel.GreenGain)
        || (pHW_PSIConfig->HWPSIControlLevel.BlueGain != HandleContext_p->PSIControlLevel.BlueGain)
        || (pHW_PSIConfig->HWPSIControlLevel.RedOffset != HandleContext_p->PSIControlLevel.RedOffset)
        || (pHW_PSIConfig->HWPSIControlLevel.GreenOffset != HandleContext_p->PSIControlLevel.GreenOffset)
        || (pHW_PSIConfig->HWPSIControlLevel.BlueOffset != HandleContext_p->PSIControlLevel.BlueOffset))
        {
            pHW_PSIConfig->HWPSIControlLevel.BrightnessLevel = HandleContext_p->PSIControlLevel.BrightnessLevel;
            pHW_PSIConfig->HWPSIControlLevel.ContrastLevel   = HandleContext_p->PSIControlLevel.ContrastLevel;
            pHW_PSIConfig->HWPSIControlLevel.SaturationLevel = HandleContext_p->PSIControlLevel.SaturationLevel;
            pHW_PSIConfig->HWPSIControlLevel.HueLevel        = HandleContext_p->PSIControlLevel.HueLevel;
            pHW_PSIConfig->HWPSIControlLevel.RedGain         = HandleContext_p->PSIControlLevel.RedGain;
            pHW_PSIConfig->HWPSIControlLevel.GreenGain       = HandleContext_p->PSIControlLevel.GreenGain;
            pHW_PSIConfig->HWPSIControlLevel.BlueGain        = HandleContext_p->PSIControlLevel.BlueGain;
            pHW_PSIConfig->HWPSIControlLevel.RedOffset       = HandleContext_p->PSIControlLevel.RedOffset;
            pHW_PSIConfig->HWPSIControlLevel.GreenOffset     = HandleContext_p->PSIControlLevel.GreenOffset;
            Updateflag = TRUE;
        }

    return Updateflag;
}

/*******************************************************************************
Name        : SYSTEM_GetColorRange
Description : Updates the color control level on the selected channel.

Parameters  : ColorControlSel selects the adjuster for the selected global color
              pMin Pointer to Min Value for the requested Control.
              pMax Pointer to Max Value for the requested Control
              pStep Pointer to Step Value for the requested Control
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t SYSTEM_GetColorRange(FVDP_PSIControl_t ColorControlSel,int32_t *pMin,int32_t *pMax,int32_t *pStep)
{
    FVDP_Result_t ErrorCode = FVDP_OK;

    if ((pMin == NULL) || (pMax == NULL) || (pStep == NULL))
        return (FVDP_ERROR);

    ErrorCode = ColorMatrix_GetColorRange(ColorControlSel, pMin, pMax, pStep);

    return ErrorCode;
}
