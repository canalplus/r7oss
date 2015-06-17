/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2013-2014 STMicroelectronics - All Rights Reserved
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
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_color.h"
#include "fvdp_common.h"
#include "fvdp_scalerlite.h"
#include "fvdp_datapath.h"
#include "fvdp_eventctrl.h"
#include "fvdp_hostupdate.h"
#include "fvdp_debug.h"
#include "fvdp_bcrop.h"
#include "fvdp_tnrlite.h"
/* Private Constants -------------------------------------------------------- */
#define TNR_LITE_DEFAULT_VQ_DATA 0

#define MIN_TIME_BEGINNING_SCALING      6
#define MIN_TIME_TWO_SCALING            10
#define ENC_GPIO_IRQ_NUMBER             466
#define ENC_IRQ_GPIO_PIN                210
#define STIH416_IRQ(irq) ((irq) + 32)

#define FVDP_ENC_INT    STIH416_IRQ(105)

// maximum wait for irq to finish 20ms
#define ENC_IRQ_WAIT_QUEUE_EVENT_TIMEOUT        20

#define MISC_ENC_TESTBUS_CTRL_1        0x00000004
#define MISC_ENC_REG32_Set(Addr,BitsToSet)         REG32_Set(Addr+MISC_ENC_BASE_ADDRESS,BitsToSet)
#define MISC_ENC_REG32_Clear(Addr,BitsToClear)     REG32_Clear(Addr+MISC_ENC_BASE_ADDRESS,BitsToClear)
#define COMM_CLUST_ENC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+COMM_CLUST_ENC_BASE_ADDRESS,BitsToSet)
// ENC state for managing the gpio interrupt indicating the end of the scaling
typedef enum {
    ENC_OFF,                    // initial state
    ENC_IT_ENABLED,             // gpio interrupt is enabled so scaling on-going
    ENC_IT_DISABLED,            // gpio interrupt is disabled so scaling was ending normally
    ENC_FLUSHING_IT_PENDING,    // flush was requested but scaling is on-going. gpio interrupt is always enabled and will arrive soon due to flush.
    ENC_FLUSHING_IT_DISABLED,   // flush was requested after scaling was ending normally and gpio interrupt is disabled.
} ENC_State;

/* Private Structures  -------- ---------------------------------------------- */
typedef struct {
    // Constant
    uint32_t               Gpio;    // gpio used
    uint32_t               IrqNum;  // gpio interrupt number

    // Need protection because they are called by interrupt context or by scaler driver.
    void                 * Lock;             // lock
    FVDP_HandleContext_t * HandleContext_p;  // handle associated to hardware scaler
    ENC_State              EncState;         // ENC state
    vibe_time64_t          InterruptTime;    // save last interrupt time
    VIBE_OS_WaitQueue_t    WaitQueueIntDone; // wait queue used to wait the end of the gpio interrupt inside FVDP_QueueFlush() in case of ENC_FLUSHING_IT_PENDING.
                                             // so to be sure that gpio interrupt is disabled before leaving the FVDP_QueueFlush().
    uint32_t               EvtIntDone;       // event state associated to the wait queue
    bool                   TnrTemporal;
    bool                   TnrUsePrevious2;
    bool                   IrqUseGpio;         // Indicate which irq method used, gpio or not (not for cut1.1)
} ENC_Irq_Handling_t;

static ENC_Irq_Handling_t   EncIrqData;

/* Private Function Prototypes ---------------------------------------------- */

static FVDP_Result_t SYSTEM_ENC_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p);
static FVDP_Result_t SYSTEM_ENC_ConfigureScaler(FVDP_HandleContext_t* HandleContext_p);

static FVDP_Result_t SYSTEM_ENC_BorderCrop(FVDP_HandleContext_t* HandleContext_p);
/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : SYSTEM_FE_Init
Description : Initialize FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_ENC_Open(FVDP_HandleContext_t* HandleContext_p)
{
    EventCtrl_Init(FVDP_ENC);
    HandleContext_p->PSIState[FVDP_TNR] = FVDP_PSI_OFF;

    // Set the Sync Update Event line number
    EventCtrl_SetSyncEvent_LITE(FVDP_ENC, EVENT_ID_ALL_LITE, 1, DISPLAY_SOURCE);

    EventCtrl_SetFrameReset_LITE(FVDP_ENC, FRAME_RESET_ALL_LITE, 2, DISPLAY_SOURCE);
    EventCtrl_EnableFwFrameReset_LITE(FVDP_ENC, FRAME_RESET_ALL_LITE);
    EventCtrl_ConfigOddSignal_LITE(FVDP_ENC, FIELD_ID_VFLITE_IN_FW_FID_LITE, FW_FID);
    BorderCrop_Init(FVDP_ENC, BC_IN_422_444_EN_MASK | BC_OUT_444_422_EN_MASK);
    TnrLite_Init();
    //Enable Cut1.1 irq
    if (EncIrqData.IrqUseGpio == FALSE)
    {
        COMM_CLUST_ENC_REG32_Set(COMMCTRL_IRQ_2_MASK_B,1);
        COMM_CLUST_ENC_REG32_Set(COMMCTRL_IRQ_2_MODE_B,1);
    }

    // Initialize the Scaler Lite HW
    HScaler_Lite_Init(FVDP_ENC);
    VScaler_Lite_Init(FVDP_ENC);

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
FVDP_Result_t SYSTEM_ENC_Init(void)
{
    uint32_t major;
    uint32_t minor;
    EncIrqData.Lock          = vibe_os_create_resource_lock();
    if(!EncIrqData.Lock)
    {
        TRC( TRC_ID_ERROR, "failed to allocate resource lock" );
        return FVDP_ERROR;
    }

    if(vibe_os_allocate_queue_event(&(EncIrqData.WaitQueueIntDone)))
    {
        TRC( TRC_ID_ERROR, "failed to allocate queue event" );
        return FVDP_ERROR;
    }

    vibe_os_get_chip_version(&major,&minor);
    vibe_os_lock_resource(EncIrqData.Lock);
    // true for cut 0, need to use gpio interrupt
    // false for cut 1
    if ((major == 1) && (minor == 0))
        EncIrqData.IrqUseGpio = TRUE;
    else
       EncIrqData.IrqUseGpio = FALSE;

    // FVDP module calls the request_irq() that automatically enable the gpio interrupt.
    // This is done after FVD_InitDriver() so after this function will be finished.
    // But due to the fact that gpio signal is at low level by default, an interrupt will be rised.
    // So the EncIrqData.EncState will be used to filter this unwanted interrupt.

    // Save datas necessary for managing end of FVDP scaling in mem to mem case
    EncIrqData.Gpio           = ENC_IRQ_GPIO_PIN;
    if (EncIrqData.IrqUseGpio)
        EncIrqData.IrqNum         = ENC_GPIO_IRQ_NUMBER;
    else
        EncIrqData.IrqNum         = FVDP_ENC_INT;

    // ENC initial state
    EncIrqData.EncState       = ENC_OFF;

    // Value used for not waiting on first FVDP_QueueBuffer.
    EncIrqData.InterruptTime  = 0;
    EncIrqData.TnrTemporal = FALSE;
    EncIrqData.TnrUsePrevious2 = FALSE;

    vibe_os_unlock_resource(EncIrqData.Lock);
    if (EncIrqData.IrqUseGpio)
        MISC_ENC_REG32_Set(MISC_ENC_TESTBUS_CTRL_1, BIT0);

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
void SYSTEM_ENC_WaitBetweenLastInterruptAndNewRequest(void)
{
    volatile vibe_time64_t CurrentTime, SavedTime;

    vibe_os_lock_resource(EncIrqData.Lock);

    // Locking garantee that value read was well updated by interrupt code
    SavedTime = EncIrqData.InterruptTime;

    vibe_os_unlock_resource(EncIrqData.Lock);

    // Ensure that at least 10 micro seconds separate the call of FVDP_IntEndOfScaling and FVDP_QueueBuffer.
    do
    {
        CurrentTime = vibe_os_get_system_time();
        if((CurrentTime - SavedTime) > MIN_TIME_TWO_SCALING)
        {
            // No need to wait for executing a new scaling
            break;
        }
    } while(1);
}

/*******************************************************************************
Name        :
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :
*******************************************************************************/
FVDP_Result_t SYSTEM_ENC_FlushChannel(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t result = FVDP_OK;

    vibe_os_lock_resource(EncIrqData.Lock);

    if (EncIrqData.HandleContext_p == HandleContext_p)
    {
        // FVDP context currently playing with scaler hardware
        // or the last one playing with scaler hardware
        switch(EncIrqData.EncState)
        {
            case ENC_IT_ENABLED:
                // Interrupt is not yet detected: it can be pending (not yet on the lock) or should appear soon.

                EncIrqData.EncState      = ENC_FLUSHING_IT_PENDING;
                EncIrqData.InterruptTime = 0;  // Needed for not waiting on next QueueBuffer.

                // Disable the MCC client
                MCC_Disable(ENC_Y_RD);
                MCC_Disable(ENC_RGB_UV_RD);
                MCC_Disable(ENC_TNR_Y_RD);
                MCC_Disable(ENC_TNR_UV_RD);
                MCC_Disable(ENC_Y_WR);
                MCC_Disable(ENC_RGB_UV_WR);
                // Perform FORCE UPDATE
                HostUpdate_HardwareUpdate(FVDP_ENC, HOST_CTRL_LITE, FORCE_UPDATE);
                // Apply the previous disabling configuration using FW Frame Reset.
                EventCtrl_SetFwFrameReset_LITE(FVDP_ENC, FRAME_RESET_ALL_LITE);
                // These previous operations should set gpio interrupt signal to 0, so raise the gpio interrupt
                // that is needed just for disabling it.
                vibe_os_unlock_resource(EncIrqData.Lock);

                // Callbacks to call (BufferDone callback should be called the last one as it allows new FVDP_QueueBuffer()):
                // - no need to call ScalingCompleted callback as current scaling is aborted.
                // - if TNR is activated, need to call BufferDone callback for previous buffer of this context. TODO
                // - need to call BufferDone callback for current buffer of this context.
                if(EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone)
                {
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserData)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserData);
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1);
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2);
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1 = 0;
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2 = 0;
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserData = 0;
                }

                // The gpio interrupt should appear immediately due to previous clearing of FVDP ENC
                // Wait that gpio interruption code disable this interrupt.
                if(vibe_os_wait_queue_event(EncIrqData.WaitQueueIntDone,
                                        &EncIrqData.EvtIntDone,
                                        1,
                                        STMIOS_WAIT_COND_EQUAL,
                                        ENC_IRQ_WAIT_QUEUE_EVENT_TIMEOUT))
                {
                    TRC( TRC_ID_ERROR, "failed to wait on queue event" );
                    return FVDP_ERROR;
                }
            break;

            case ENC_IT_DISABLED:
                // Interrupt was already treated so disabled

                EncIrqData.EncState = ENC_FLUSHING_IT_DISABLED;

                // Callback to call:
                // - if TNR is activated, need to call BufferDone callback for previous buffer of this context. TODO

                vibe_os_unlock_resource(EncIrqData.Lock);

                // As the gpio interrupt does not indicate really the end of the scaling,
                // and callback was already called indicating that picture is valid,
                // do the active wait here that ensure that scaling is really finished before resetting FVDP registers.
                SYSTEM_ENC_WaitBetweenLastInterruptAndNewRequest();
                // Release all tnr buffers if in use
                if(EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone)
                {
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1);
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2);
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1 = 0;
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2 = 0;
                }

                // Disable the MCC client
                MCC_Disable(ENC_Y_RD);
                MCC_Disable(ENC_RGB_UV_RD);
                MCC_Disable(ENC_TNR_Y_RD);
                MCC_Disable(ENC_TNR_UV_RD);
                MCC_Disable(ENC_Y_WR);
                MCC_Disable(ENC_RGB_UV_WR);
                // Perform FORCE UPDATE
                HostUpdate_HardwareUpdate(FVDP_ENC, HOST_CTRL_LITE, FORCE_UPDATE);
            break;

            case ENC_FLUSHING_IT_PENDING:
            case ENC_FLUSHING_IT_DISABLED:
                // Flush already done
                vibe_os_unlock_resource(EncIrqData.Lock);

                break;
            default:
                vibe_os_unlock_resource(EncIrqData.Lock);

                TRC( TRC_ID_ERROR, "failed, invalid EncIrqData.EncState=%d", EncIrqData.EncState );
                return FVDP_ERROR;

                break;
        }
    }
    else
    {
        // Already re-scaled context
        vibe_os_unlock_resource(EncIrqData.Lock);

        // Callback to call:
        // - if TNR is activated, need to call BufferDone callback for previous buffer of this context. TODO
        //   (if not already done because user can flush several time the same handle consecutively)
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
FVDP_Result_t SYSTEM_ENC_ProcessMemToMem(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t     ErrorCode = FVDP_OK;
    FVDP_FrameInfo_t* FrameInfo_p = &(HandleContext_p->FrameInfo);
    FVDP_Frame_t*     CurrentInputFrame_p;
    BufferType_t      BufferType = 0;
    uint32_t          idx;
    vibe_time64_t     CurrentTime;
    vibe_time64_t     StartTime;
    bool              RGB_444 = FALSE;

    if (HandleContext_p->PSIState[FVDP_TNR] != FVDP_PSI_OFF)
        EncIrqData.TnrTemporal = TRUE;
    else
        EncIrqData.TnrTemporal = FALSE;

    if (EncIrqData.IrqUseGpio)
        SYSTEM_ENC_WaitBetweenLastInterruptAndNewRequest();

    // Basic ENC channel FBM:     Prev = Curr
    //                            Curr = Next
    //                            Next = AllocFrame()
    if( (FrameInfo_p->NextInputFrame_p) &&
                (FrameInfo_p->NextInputFrame_p->InputVideoInfo.ScanType == INTERLACED))
            FrameInfo_p->Prev2InputFrame_p = FrameInfo_p->Prev1InputFrame_p;

    FrameInfo_p->Prev1InputFrame_p = FrameInfo_p->CurrentInputFrame_p;
    FrameInfo_p->CurrentInputFrame_p = FrameInfo_p->NextInputFrame_p;
    FrameInfo_p->NextInputFrame_p = SYSTEM_CloneFrame(HandleContext_p, FrameInfo_p->CurrentInputFrame_p);
    if (FrameInfo_p->NextInputFrame_p == NULL)
    {
        return FVDP_ERROR;
    }

    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    FVDP_DEBUG_ENC(HandleContext_p);
    if (CurrentInputFrame_p == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  CurrentInputFrame_p is NULL." );
        return FVDP_ERROR;
    }

    // Allocate buffers for the Current Input Frame
    for (BufferType = BUFFER_PRO_Y_G; BufferType <= BUFFER_PRO_U_UV_B; BufferType++)
    {
        // Allocate the buffer based on the buffer index provided by FBM
        CurrentInputFrame_p->FrameBuffer[BufferType] = ObjectPool_Alloc_Object(&(HandleContext_p->FrameInfo.BufferPool[BufferType].BufferObjectPool), 1, &idx);

        // Check that the buffer allocation didn't fail
        if (CurrentInputFrame_p->FrameBuffer[BufferType] == NULL)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  Could not allocate BufferType [%d].", BufferType );
            return FVDP_ERROR;
        }
    }
    // Set the Destination buffer addresses.
    CurrentInputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->Addr = HandleContext_p->QueueBufferInfo.DstAddr1;
    CurrentInputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->Addr = HandleContext_p->QueueBufferInfo.DstAddr2;

    if(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
    {
        EventCtrl_SetOddSignal_LITE (FVDP_ENC, FIELD_ID_VFLITE_IN_FW_FID_LITE,
                                FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FieldType);
    }
    else
    {
        EventCtrl_SetOddSignal_LITE (FVDP_ENC, FIELD_ID_VFLITE_IN_FW_FID_LITE, TOP_FIELD);
    }

    // Configure border
    ErrorCode = SYSTEM_ENC_BorderCrop(HandleContext_p);

    // Configure Scaler
    SYSTEM_ENC_ConfigureScaler(HandleContext_p);

    // Configure CCTRL
    {
        ColorMatrix_Adjuster_t ColorControl;
        SYSTEM_CopyColorControlData(HandleContext_p,&ColorControl);
        CscLight_Update(HandleContext_p->CH,
                       CurrentInputFrame_p->InputVideoInfo.ColorSpace,
                       CurrentInputFrame_p->OutputVideoInfo.ColorSpace,
                       &ColorControl);

    }

    if(CurrentInputFrame_p->InputVideoInfo.ColorSampling == FVDP_444)
        RGB_444 = TRUE;

    if( EncIrqData.TnrTemporal )
    {
       TnrLite_SetVqData(TNR_LITE_DEFAULT_VQ_DATA, FALSE,
                   CurrentInputFrame_p->OutputVideoInfo.Width, CurrentInputFrame_p->OutputVideoInfo.Height);
       EncIrqData.TnrUsePrevious2 = FALSE;
       TnrLite_Disable();

        if(FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType != INTERLACED)
        {
            //Progressive input, one buffer
            if (HandleContext_p->QueueBufferInfo.UserDataPrevious1)
            {
                TnrLite_Enable(FALSE);
            }
        }
        else
        {
            //Interlaced input needs two buffers
            if (FrameInfo_p->Prev1InputFrame_p)
            {
                // if user data is defined (buffer available) we can enable tnr, if P2 is used will determine later
                if (HandleContext_p->QueueBufferInfo.UserDataPrevious1)
                    TnrLite_Enable(FALSE);

                if (FrameInfo_p->Prev2InputFrame_p)
                {
                    //if Current polarity is equal to Previous1, then we don't use Previous2
                    //if Current polarity is equal to Previous2 and user data previous2 is available (Buffer is allocated) then we can use previous2 frame
                    if (( FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FieldType != FrameInfo_p->Prev1InputFrame_p->InputVideoInfo.FieldType) &&
                        ( FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.FieldType == FrameInfo_p->Prev2InputFrame_p->InputVideoInfo.FieldType) &&
                                    (HandleContext_p->QueueBufferInfo.UserDataPrevious2))
                    {
                        TnrLite_Enable(FALSE);
                        EncIrqData.TnrUsePrevious2 = TRUE;
                    }
                }
            }
        }

        DATAPATH_LITE_Connect(FVDP_ENC, RGB_444 ? DATAPATH_LITE_MEMTOMEM_TEMPORAL_444 : DATAPATH_LITE_MEMTOMEM_TEMPORAL);
    }
    else
    {
        TnrLite_Disable();
        // Setup Datapath
        DATAPATH_LITE_Connect(FVDP_ENC, RGB_444 ? DATAPATH_LITE_MEMTOMEM_SPATIAL_444 : DATAPATH_LITE_MEMTOMEM_SPATIAL);
    }
    // Configure MCC
    SYSTEM_ENC_ConfigureMCC(HandleContext_p);

    // Free the Previous frame used for TNR
    if(( FrameInfo_p->CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED) && ( EncIrqData.TnrTemporal ))
    {
        SYSTEM_FreeFrame(HandleContext_p, FrameInfo_p->Prev2InputFrame_p);
    }
    else
    {
        SYSTEM_FreeFrame(HandleContext_p, FrameInfo_p->Prev1InputFrame_p);
        SYSTEM_FreeFrame(HandleContext_p, FrameInfo_p->Prev2InputFrame_p);
    }

    // Configure CCTRL


    // Perform FORCE UPDATE
    HostUpdate_HardwareUpdate(FVDP_ENC, HOST_CTRL_LITE, FORCE_UPDATE);

    // Start the mem to mem operation using FW Frame Reset.
    EventCtrl_SetFwFrameReset_LITE(FVDP_ENC, FRAME_RESET_ALL_LITE);

    // When scaling is started, gpio switch to high level after 5 micro seconds.
    // So only after that we should enable the interrupt on low level for this
    // gpio for detecting end of scaling. Or when we detect that the gpio is
    // at high level.
    if (EncIrqData.IrqUseGpio)
    {
        StartTime = vibe_os_get_system_time();
        do
        {
            if( vibe_os_gpio_get_value(EncIrqData.Gpio) )
            {
                // Exit because gpio is at high level
                // Normal behavior
                break;
            }
            CurrentTime = vibe_os_get_system_time();
            if((CurrentTime - StartTime) > MIN_TIME_BEGINNING_SCALING)
            {
                // Exit after waiting
                // Meaning that high pulse was not seen: too short and/or
                // other interrupt activity or higher priority task running
                // TRC( TRC_ID_ERROR, "WARNING int gpio not high" );
                break;
            }
        } while(1);
    }

    vibe_os_lock_resource(EncIrqData.Lock);

    // Save handle of on-going scaling
    EncIrqData.HandleContext_p = HandleContext_p;
    // Reset event used for waiting the gpio interrupt detection in case of flushing before scaling ending.
    EncIrqData.EvtIntDone = 0;

    switch(EncIrqData.EncState)
    {
        case ENC_OFF:
        case ENC_IT_DISABLED:
        case ENC_FLUSHING_IT_PENDING:
        case ENC_FLUSHING_IT_DISABLED:
            // Enable the interrupt detection, so the end of scaling.
            EncIrqData.EncState = ENC_IT_ENABLED;
            vibe_os_enable_irq( EncIrqData.IrqNum );
            break;
        default:
            TRC( TRC_ID_ERROR, "failed, invalid EncIrqData.EncState=%d", EncIrqData.EncState );
            ErrorCode = FVDP_ERROR;
            break;
    }

    vibe_os_unlock_resource(EncIrqData.Lock);

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
static FVDP_Result_t SYSTEM_ENC_ConfigureMCC(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t     ErrorCode = FVDP_OK;
    FVDP_Frame_t*     CurrentInputFrame_p;
    FVDP_Frame_t*     Prev1InputFrame_p;
    FVDP_Frame_t*     Prev2InputFrame_p;
    FVDP_Frame_t*     TnrInputFrame_p;
    MCC_Config_t*     MccConfig_p;
    MCC_Config_t      MccConfig_RD;
    FVDP_FieldType_t  FieldType = TOP_FIELD;
    uint32_t          BaseAddress;
    FVDP_CropWindow_t * CropWin_p = NULL;

    vibe_os_zero_memory( &MccConfig_RD, sizeof( MccConfig_RD ));
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    Prev1InputFrame_p = SYSTEM_GetPrev1InputFrame(HandleContext_p);
    Prev2InputFrame_p = SYSTEM_GetPrev2InputFrame(HandleContext_p);

    if (CurrentInputFrame_p == NULL)
    {
        TRC( TRC_ID_MAIN_INFO, "ERROR:  CurrentInputFrame_p is NULL." );
        ErrorCode = FVDP_ERROR;
    }
    CropWin_p = &CurrentInputFrame_p->CropWindow;

    if (ErrorCode != FVDP_OK)
    {
        // Disable the MCC client if there is no input frame defined for it
        MCC_Disable(ENC_Y_RD);
        MCC_Disable(ENC_RGB_UV_RD);
        MCC_Disable(ENC_TNR_Y_RD);
        MCC_Disable(ENC_TNR_UV_RD);
        MCC_Disable(ENC_Y_WR);
        MCC_Disable(ENC_RGB_UV_WR);
        return ErrorCode;
    }

    MccConfig_RD.bpp = 8;
    MccConfig_RD.compression = MCC_COMP_NONE;
    MccConfig_RD.rgb_mode = (CurrentInputFrame_p->InputVideoInfo.ColorSampling == FVDP_444);
    if( (CropWin_p->HCropStart + CropWin_p->HCropWidth <= CurrentInputFrame_p->InputVideoInfo.Width) &&
        (CropWin_p->VCropStart + CropWin_p->VCropHeight <= CurrentInputFrame_p->InputVideoInfo.Height) &&
        (CropWin_p->HCropWidth > 0) && (CropWin_p->VCropHeight  > 0))
    {
        MccConfig_RD.crop_window.crop_h_start = CropWin_p->HCropStart;
        MccConfig_RD.crop_window.crop_h_size  = CropWin_p->HCropWidth;
        MccConfig_RD.crop_window.crop_v_start = CropWin_p->VCropStart;
        MccConfig_RD.crop_window.crop_v_size  = CropWin_p->VCropHeight;
        MccConfig_RD.crop_enable = TRUE;
    }
    else
    {
        MccConfig_RD.crop_enable = FALSE;
    }
    MccConfig_RD.buffer_h_size = CurrentInputFrame_p->InputVideoInfo.Width;
    MccConfig_RD.buffer_v_size = CurrentInputFrame_p->InputVideoInfo.Height;
    MccConfig_RD.stride = CurrentInputFrame_p->InputVideoInfo.Stride;
    BaseAddress = HandleContext_p->QueueBufferInfo.SrcAddr1;
    if(CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
    {
        MccConfig_RD.crop_window.crop_v_size *=2;
        MccConfig_RD.Alternated_Line_Read = TRUE;
        FieldType = CurrentInputFrame_p->InputVideoInfo.FieldType;
        MccConfig_RD.buffer_v_size *= 2;
        if(FieldType == BOTTOM_FIELD)
            BaseAddress += MccConfig_RD.stride;/* add stride value in bytes */
    }
    else
        MccConfig_RD.Alternated_Line_Read = FALSE;
    ErrorCode = MCC_Config(ENC_Y_RD, MccConfig_RD);
    if(ErrorCode != FVDP_OK)
        return ErrorCode;
    if(!MccConfig_RD.rgb_mode)/* do not program ENC_Y_RD for RGB input*/
    {

        /* Configure ENC_Y_RD for YUV input*/
        MCC_SetBaseAddr(ENC_Y_RD, BaseAddress, MccConfig_RD);
        MCC_Enable(ENC_Y_RD);

        /* prepare ENC_RGB_UV_RD data for YUV input */
        if (CurrentInputFrame_p->InputVideoInfo.ColorSampling == FVDP_420)
        {
            MccConfig_RD.compression = MCC_COMP_UV_420_TO_422;
            MccConfig_RD.buffer_v_size = (MccConfig_RD.buffer_v_size + 1)/2;
            MccConfig_RD.crop_window.crop_v_start /= 2;
            MccConfig_RD.crop_window.crop_v_size = (MccConfig_RD.crop_window.crop_v_size  + 1)/2;
        }
        BaseAddress = HandleContext_p->QueueBufferInfo.SrcAddr2;
        if(CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
        {
            if(FieldType == BOTTOM_FIELD)
                BaseAddress += MccConfig_RD.stride;/* add stride value in bytes */
        }
    }
    /* Configure ENC_RGB_UV_RD for YUV or RGB inputs*/
    ErrorCode = MCC_Config(ENC_RGB_UV_RD, MccConfig_RD);
    if(ErrorCode != FVDP_OK)
        return ErrorCode;

    MCC_SetBaseAddr(ENC_RGB_UV_RD, BaseAddress, MccConfig_RD);
    MCC_Enable(ENC_RGB_UV_RD);

    // Tnr read client, it uses previous for progressive or previous2 for interlaced
    if ((EncIrqData.TnrTemporal) && (Prev1InputFrame_p != NULL))
    {
        if ((EncIrqData.TnrUsePrevious2 == TRUE) && (Prev2InputFrame_p->FrameBuffer[BUFFER_PRO_Y_G] != NULL))
            TnrInputFrame_p = Prev2InputFrame_p;
        else
            TnrInputFrame_p = Prev1InputFrame_p;

        // ENC_TNR_Y_RD
        MccConfig_p = &(TnrInputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->MccConfig);

        MccConfig_p->bpp = 8;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = TnrInputFrame_p->OutputVideoInfo.Width;
        MccConfig_p->buffer_v_size = TnrInputFrame_p->OutputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = TnrInputFrame_p->OutputVideoInfo.Stride;

        MCC_Config(ENC_TNR_Y_RD, *MccConfig_p);
        MCC_SetBaseAddr(ENC_TNR_Y_RD, TnrInputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->Addr, *MccConfig_p);
        MCC_Enable(ENC_TNR_Y_RD);

        // ENC_TNR_UV_RD
        MccConfig_p = &(TnrInputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->MccConfig);

        MccConfig_p->bpp = 8;
        MccConfig_p->compression = MCC_COMP_NONE;
        MccConfig_p->rgb_mode = FALSE;
        MccConfig_p->buffer_h_size = TnrInputFrame_p->OutputVideoInfo.Width;
        MccConfig_p->buffer_v_size = TnrInputFrame_p->OutputVideoInfo.Height;
        MccConfig_p->crop_enable = FALSE;
        MccConfig_p->Alternated_Line_Read = FALSE;
        MccConfig_p->stride = TnrInputFrame_p->OutputVideoInfo.Stride;

        if (TnrInputFrame_p->OutputVideoInfo.ColorSampling == FVDP_420)
        {
            MccConfig_p->compression = MCC_COMP_UV_420_TO_422;
            MccConfig_p->buffer_v_size = (MccConfig_p->buffer_v_size + 1)/2;
        }

        MCC_Config(ENC_TNR_UV_RD, *MccConfig_p);
        MCC_SetBaseAddr(ENC_TNR_UV_RD, TnrInputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->Addr, *MccConfig_p);
        MCC_Enable(ENC_TNR_UV_RD);
    }
    else
    {
        MCC_Disable(ENC_TNR_Y_RD);
        MCC_Disable(ENC_TNR_UV_RD);
    }

    /* ENC_Y_WR */
    MccConfig_p = &(CurrentInputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->MccConfig);
    MccConfig_p->bpp = 8;
    MccConfig_p->compression = MCC_COMP_NONE;
    MccConfig_p->rgb_mode = FALSE;
    MccConfig_p->buffer_h_size = CurrentInputFrame_p->OutputVideoInfo.Width;
    MccConfig_p->buffer_v_size = CurrentInputFrame_p->OutputVideoInfo.Height;
    MccConfig_p->crop_enable = FALSE;
    MccConfig_p->Alternated_Line_Read = FALSE;
    MccConfig_p->stride = CurrentInputFrame_p->OutputVideoInfo.Stride;
    BaseAddress = CurrentInputFrame_p->FrameBuffer[BUFFER_PRO_Y_G]->Addr;
    if(CurrentInputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
    {
        MccConfig_p->Alternated_Line_Read = TRUE;
        FieldType = CurrentInputFrame_p->InputVideoInfo.FieldType;
        MccConfig_p->buffer_v_size *= 2;
        /* recalculate base address by adding stride for interlaced bottom field */
        if(FieldType == BOTTOM_FIELD)
            BaseAddress += MccConfig_p->stride;/* add stride value in bytes */
    }
    else
    {
        MccConfig_p->Alternated_Line_Read = FALSE;
    }
    ErrorCode = MCC_Config(ENC_Y_WR, *MccConfig_p);
    if(ErrorCode != FVDP_OK)
        return ErrorCode;

    MCC_SetBaseAddr(ENC_Y_WR, BaseAddress, *MccConfig_p);
    MCC_Enable(ENC_Y_WR);

    /* ENC_RGB_UV_WR */
    if (CurrentInputFrame_p->OutputVideoInfo.ColorSampling == FVDP_420)
    {
        MccConfig_p->compression = MCC_COMP_UV_422_TO_420;
        MccConfig_p->buffer_v_size =  (MccConfig_p->buffer_v_size + 1)/2;
    }
    /* Calculate base address by adding stride for interlaced bottom field */
    BaseAddress = CurrentInputFrame_p->FrameBuffer[BUFFER_PRO_U_UV_B]->Addr;
    if(CurrentInputFrame_p->OutputVideoInfo.ScanType == INTERLACED)
    {
        if(FieldType == BOTTOM_FIELD)
            BaseAddress += MccConfig_p->stride;/* add stride value in bytes */
    }

    ErrorCode = MCC_Config(ENC_RGB_UV_WR, *MccConfig_p);
    if(ErrorCode != FVDP_OK)
        return ErrorCode;

    MCC_SetBaseAddr(ENC_RGB_UV_WR, BaseAddress, *MccConfig_p);
    MCC_Enable(ENC_RGB_UV_WR);

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
static FVDP_Result_t SYSTEM_ENC_ConfigureScaler(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t           ErrorCode = FVDP_OK;
    FVDP_Frame_t*           CurrentInputFrame_p = NULL;
    FVDP_ScalerLiteConfig_t ScalerConfig;
    FVDP_OutputWindow_t*    ActWin_p;
    FVDP_CropWindow_t *     CropWin_p = NULL;
    HandleContext_p->QueueBufferInfo.ScalingStatus = TRUE;

    // Get the current input frame
    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    ActWin_p = &CurrentInputFrame_p->OutputActiveWindow;
    if (SYSTEM_IsFrameValid(HandleContext_p, CurrentInputFrame_p) == FALSE)
    {
        return FVDP_ERROR;
    }

    CropWin_p = &CurrentInputFrame_p->CropWindow;

    ScalerConfig.ProcessingRGB = (HandleContext_p->ProcessingType == FVDP_RGB_GRAPHIC);

    // Setup parameters of horizontal scaling
    ScalerConfig.InputFormat.ColorSampling  = CurrentInputFrame_p->InputVideoInfo.ColorSampling;
    ScalerConfig.InputFormat.ColorSpace     = CurrentInputFrame_p->InputVideoInfo.ColorSpace;
    ScalerConfig.InputFormat.HWidth         = CurrentInputFrame_p->InputVideoInfo.Width;
    ScalerConfig.InputFormat.VHeight        = CurrentInputFrame_p->InputVideoInfo.Height;
    ScalerConfig.InputFormat.ScanType       = CurrentInputFrame_p->InputVideoInfo.ScanType;
    ScalerConfig.InputFormat.FieldType      = CurrentInputFrame_p->InputVideoInfo.FieldType;
    if( (CropWin_p->HCropStart + CropWin_p->HCropWidth <= CurrentInputFrame_p->InputVideoInfo.Width) &&
        (CropWin_p->VCropStart + CropWin_p->VCropHeight <= CurrentInputFrame_p->InputVideoInfo.Height) &&
        (CropWin_p->HCropWidth > 0) && (CropWin_p->VCropHeight > 0))
    {
        ScalerConfig.InputFormat.HWidth  = CropWin_p->HCropWidth;
        ScalerConfig.InputFormat.VHeight = CropWin_p->VCropHeight;
    }

    ScalerConfig.OutputFormat.ColorSampling  = CurrentInputFrame_p->OutputVideoInfo.ColorSampling;
    ScalerConfig.OutputFormat.ColorSpace     = CurrentInputFrame_p->OutputVideoInfo.ColorSpace;
    ScalerConfig.OutputFormat.ScanType = CurrentInputFrame_p->OutputVideoInfo.ScanType;
    ScalerConfig.OutputFormat.FieldType = AUTO_FIELD_TYPE;
    if( (ActWin_p->HWidth  <= CurrentInputFrame_p->OutputVideoInfo.Width)  &&
        (ActWin_p->VHeight <= CurrentInputFrame_p->OutputVideoInfo.Height) &&
        (ActWin_p->HWidth  > 0) &&
        (ActWin_p->VHeight > 0))
    {
        ScalerConfig.OutputFormat.HWidth  = ActWin_p->HWidth;
        ScalerConfig.OutputFormat.VHeight = ActWin_p->VHeight;
    }
    else
    {
        ScalerConfig.OutputFormat.HWidth  = CurrentInputFrame_p->OutputVideoInfo.Width;
        ScalerConfig.OutputFormat.VHeight = CurrentInputFrame_p->OutputVideoInfo.Height;
    }
    // Configure the Horizontal  Scaler
    ErrorCode = HScaler_Lite_HWConfig(FVDP_ENC, ScalerConfig);
    if(ErrorCode != FVDP_OK)
    {
        HandleContext_p->QueueBufferInfo.ScalingStatus = FALSE;
        return ErrorCode;
    }


    VScaler_Lite_OffsetUpdate(FVDP_ENC, ScalerConfig);

    // Configure the Vertical Scaler
    ErrorCode = VScaler_Lite_HWConfig(FVDP_ENC, ScalerConfig);
    if(ErrorCode != FVDP_OK)
    {
        HandleContext_p->QueueBufferInfo.ScalingStatus = FALSE;
    }
    if(CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED)
    {
        VScaler_Lite_DcdiUpdate(FVDP_ENC, TRUE, 4);
    }
    else
    {
        VScaler_Lite_DcdiUpdate(FVDP_ENC, FALSE, 0);
    }
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
FVDP_Result_t SYSTEM_ENC_CompleteScalling(void)
{
    FVDP_Result_t result = FVDP_OK;
    vibe_time64_t time;

    // Save interrupt time
    time = vibe_os_get_system_time();

    vibe_os_lock_resource(EncIrqData.Lock);

    // Disable gpio interrupt for low level triggering
    vibe_os_disable_irq_nosync( EncIrqData.IrqNum );
    if (!EncIrqData.IrqUseGpio)
        COMM_CLUST_ENC_REG32_Set(COMMCTRL_IRQ_2_STATUS_B,1);

    switch(EncIrqData.EncState)
    {
        case ENC_OFF:
            // State used to discard first interrupt following the request_irq function that automatically
            // enable the interrupt.
            vibe_os_unlock_resource(EncIrqData.Lock);

            break;

        case ENC_IT_ENABLED:
            // Normal ending of scaling
            EncIrqData.EncState = ENC_IT_DISABLED;

            EncIrqData.InterruptTime = time;

            if (EncIrqData.HandleContext_p != NULL)
            {
                // Indicate that scaling is completed
                if((EncIrqData.HandleContext_p->QueueBufferInfo.ScalingCompleted) && (EncIrqData.HandleContext_p->QueueBufferInfo.UserData))
                    EncIrqData.HandleContext_p->QueueBufferInfo.ScalingCompleted(EncIrqData.HandleContext_p->QueueBufferInfo.UserData,
                                                              EncIrqData.HandleContext_p->QueueBufferInfo.ScalingStatus);

                // In case of TNR, it is the previous output buffer to release here instead of current one. TODO

                if (EncIrqData.TnrTemporal)
                {
                    // we don't need previous2 buffer
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2)
                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2);
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2 = 0;

                    if((EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone) &&
                                (EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1))
                    {
                        if(EncIrqData.HandleContext_p->FrameInfo.CurrentInputFrame_p->InputVideoInfo.ScanType != INTERLACED)
                        {
                            // for progressive we don't need previous1 buffer
                            EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1);
                            EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1 = 0;
                        }
                        else
                            // for interlaced save previous 2 data pointer
                            EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2 = EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1;
                    }
                    // for tnr save previous 1 data pointer
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1 = EncIrqData.HandleContext_p->QueueBufferInfo.UserData;
                }
                else
                {
                    if(EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone)
                    {
                        // non TNR, release all the buffers, and clear user pointers
                        if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1)
                            EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1);

                        EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserData);

                        if(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2)
                            EncIrqData.HandleContext_p->QueueBufferInfo.BufferDone(EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2);
                    }
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious2 = 0;
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserDataPrevious1 = 0;
                    EncIrqData.HandleContext_p->QueueBufferInfo.UserData = 0;
                }

            }
            else
            {
                result = FVDP_ERROR;
            }

            vibe_os_unlock_resource(EncIrqData.Lock);

            break;

        case ENC_FLUSHING_IT_PENDING:
            // Flushing done just before and interrupt is pending due to lock.
            EncIrqData.EvtIntDone = 1;

            vibe_os_unlock_resource(EncIrqData.Lock);

            vibe_os_wake_up_queue_event(EncIrqData.WaitQueueIntDone);

            break;

        default:
            vibe_os_unlock_resource(EncIrqData.Lock);

            result = FVDP_ERROR;

            break;
    }

    return result;
}

/*******************************************************************************
Name        :void SYSTEM_ENC_Close(FVDP_HandleContext_t* HandleContext_p)
Description : Close encoder channel for selected handle, there might be other open handles

Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
void SYSTEM_ENC_Close(FVDP_HandleContext_t* HandleContext_p)
{
    SYSTEM_ENC_FlushChannel(HandleContext_p);
    // this memory is not static since it can be number of handles for encoder channel,
    // so it needed to be dynamically allocated,
    // hence we need free up the memory
    vibe_os_free_memory(HandleContext_p->FrameInfo.FramePool.FramePoolArray);
    vibe_os_free_memory(HandleContext_p->FrameInfo.FramePool.FramePoolStates);
}
/*******************************************************************************
Name        :SYSTEM_ENC_Terminate
Description : Termniate operation and prepare for unloading the module
Parameters  : void
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void SYSTEM_ENC_Terminate(void)
{
    MISC_ENC_REG32_Clear(MISC_ENC_TESTBUS_CTRL_1, BIT0);
    if(EncIrqData.Lock)
    {
        vibe_os_delete_resource_lock(EncIrqData.Lock);
    }
    if(EncIrqData.WaitQueueIntDone)
    {
        vibe_os_release_queue_event(EncIrqData.WaitQueueIntDone);
    }
}
/*******************************************************************************
Name        : SYSTEM_ENC_BorderCrop
Description : Configures and enbles/disables border cropping

Parameters  : HandleContext_p
Assumptions : Borders settings must correspond with scaling settings.
Limitations :
Returns     : FVDP_OK, FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t SYSTEM_ENC_BorderCrop(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Frame_t* CurrentInputFrame_p;
    FVDP_Result_t retvalue = FVDP_OK;
    FVDP_OutputWindow_t * ActWin_p;

    if(!HandleContext_p)
        return FVDP_ERROR;

    CurrentInputFrame_p = SYSTEM_GetCurrentInputFrame(HandleContext_p);
    ActWin_p = (FVDP_OutputWindow_t *)&CurrentInputFrame_p->OutputActiveWindow;

    /*Check if border was requsted*/
    if( (ActWin_p->HWidth  <= CurrentInputFrame_p->OutputVideoInfo.Width) &&
        (ActWin_p->VHeight <= CurrentInputFrame_p->OutputVideoInfo.Height) &&
        (ActWin_p->HWidth  > 0) &&
        (ActWin_p->VHeight > 0))
    {
        /* configure borders */
        retvalue = BorderCrop_Config(FVDP_ENC,
            ActWin_p->HWidth,                           //Input_H_Size,
            ActWin_p->VHeight,                          //Input_V_Size,
            CurrentInputFrame_p->OutputVideoInfo.Width, //Output_H_Size,
            CurrentInputFrame_p->OutputVideoInfo.Height,//Output_V_Size,
            ActWin_p->HStart,                           //H_Start,
            ActWin_p->VStart                            //V_Start
        );

        if(retvalue == FVDP_OK)
        {   /* Enable border HW */
            BorderCrop_Enable(FVDP_ENC, TRUE);
        }
    }
    else
    {   /* Disable border HW */
        BorderCrop_Enable(FVDP_ENC, FALSE);
    }
    return retvalue;
}

/*******************************************************************************
Name        : SYSTEM_ENC_PowerDown
Description : Power down FVDP_FE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_ENC_PowerDown(void)
{
    // Disable IRQ of IP blocks
    MISC_ENC_REG32_Clear(MISC_ENC_TESTBUS_CTRL_1, BIT0);

    // Disable the MCC client
    MCC_Disable(ENC_Y_RD);
    MCC_Disable(ENC_RGB_UV_RD);
    MCC_Disable(ENC_TNR_Y_RD);
    MCC_Disable(ENC_TNR_UV_RD);
    MCC_Disable(ENC_Y_WR);
    MCC_Disable(ENC_RGB_UV_WR);

    // Disable IP blocks

    // Disable clock of IP blocks
    EventCtrl_ClockCtrl_LITE(FVDP_ENC, MODULE_CLK_ALL_LITE, GND_CLK);

    HostUpdate_HardwareUpdate(FVDP_ENC, HOST_CTRL_LITE, FORCE_UPDATE);
    return FVDP_OK;
}

/*******************************************************************************
Name        : SYSTEM_ENC_PowerUp
Description : Power down FVDP_LITE HW blocks

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_ENC_PowerUp(void)
{
    FVDP_HandleContext_t* HandleContext_p;

    HandleContext_p = SYSTEM_GetHandleContextOfChannel(FVDP_ENC);

    // ENC Channel has been never opened before
    if (HandleContext_p == NULL)
    {
        return FVDP_OK;
    }

    //Set Hard reset LITE block
    EventCtrl_HardReset_LITE(FVDP_ENC, HARD_RESET_ALL_LITE, TRUE);

    //INIT local variables

    //Clear Hard reset LITE block
    EventCtrl_HardReset_LITE(FVDP_ENC, HARD_RESET_ALL_LITE, FALSE);

    //Initialize FVDP_ENC software module and call FVDP_FE hardware init function
    SYSTEM_ENC_Open(HandleContext_p);

    // Initialize the MCC HW
    MCC_Init(FVDP_ENC);

    HostUpdate_HardwareUpdate(FVDP_ENC, HOST_CTRL_LITE, FORCE_UPDATE);

    return FVDP_OK;
}
