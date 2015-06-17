/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2011-2014 STMicroelectronics - All Rights Reserved
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

#include "fvdp_mbox.h"
#include "fvdp_vcpu.h"
//#include "fvdp_afm.h"

//#define DEBUG_TRAP_MBOX_ANOMALY

/* Private Types ------------------------------------------------------------ */

typedef enum
{
    OUTBOX_IDLE,
    OUTBOX_SEND,
    OUTBOX_WAIT_ACK
} OutboxState_t;

/* Private Constants -------------------------------------------------------- */

// operation ID
#define CMD0_OP_MASK        0x0000000F
#define CMD0_OP_SIZE        4
#define CMD0_OP_OFFSET      0

// sender ID
#define CMD0_SID_MASK       0x000000F0
#define CMD0_SID_SIZE       4
#define CMD0_SID_OFFSET     4

// receiver ID
#define CMD0_RID_MASK       0x00000F00
#define CMD0_RID_SIZE       4
#define CMD0_RID_OFFSET     8

// sub-operation ID
#define CMD0_SOP_MASK       0x00007000
#define CMD0_SOP_SIZE       3
#define CMD0_SOP_OFFSET     12

// payload send done bit (to trigger action)
#define CMD0_DONE_MASK      0x00008000
#define CMD0_DONE_SIZE      1
#define CMD0_DONE_OFFSET    15

// data offset
#define CMD1_OFFSET_MASK    0x0000FFFF
#define CMD1_OFFSET_SIZE    16
#define CMD1_OFFSET_OFFSET  0

// data size
#define CMD1_SIZE_MASK      0xFFFF0000
#define CMD1_SIZE_SIZE      16
#define CMD1_SIZE_OFFSET    16

#define MBX_REGS            32
#define MBX_COMM_SIZE       sizeof(INBOX_COMM_MSG)

#define MBOX_MAX_RETRIES    3

#define WORD_SIZE                       4
#define MAX_STREAMER_TRANSFER_SIZE      0x2000 /* 8K */

#define WAIT_FOR_VCPU_STREAMER_TIME     30000   // streamer tx=70us, limit=100us
#define WAIT_FOR_BITSET_TIME            5       // wait for the bit to be set

#define OUTBOX_IDLE_MAX                 500000L

/* Private Macros ----------------------------------------------------------- */

#define TrMBOX(level,...)
//#define TrMBOX(level,...)  TraceModuleOn(level, 2, __VA_ARGS__)
//#define TrMBOX(level,...)   g_pIDebug->IDebugPrintf(__VA_ARGS__)

/* Private Variables (static)------------------------------------------------ */

static void     * MBoxLock;
static uint32_t   INBOX_COMM_MSG  [MBX_REGS];
static uint32_t   OUTBOX_COMM_MSG [MBX_REGS];

static void*            outbox_tx_addr = NULL;
static OutboxState_t    outbox_state = OUTBOX_IDLE;
static MBOX_ID_t        outbox_tx_target;
static MBOX_CMD_t       outbox_tx_cmd;
static MBOX_SUB_CMD_t   outbox_tx_sub_cmd;
static uint16_t         outbox_tx_offset;
static uint16_t         outbox_tx_size;
static uint16_t         outbox_tx_size_orig;
static uint8_t          outbox_retries;
static uint32_t         outbox_idle_count;

/* Private Function Prototypes ---------------------------------------------- */

static void mbox_handle_anomaly (uint32_t line, uint32_t ref);

/* Global Variables --------------------------------------------------------- */

uint8_t comm_buff[256];

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : MBOX_Init
Description : MBOX initialization routine
Parameters  : none
Assumptions :
Limitations :
Returns     : TRUE if successful, otherwise FALSE
*******************************************************************************/
bool MBOX_Init (void)
{
#if (RUNTIME_PLATFORM == PLATFORM_ARM)
    MBoxLock = vibe_os_create_resource_lock();
    if (!MBoxLock)
    {
        TrMBOX(DBG_ERROR,"unable to create resource lock\n");
        return FALSE;
    }
#endif

    MBOX_REG32_Write(OUTBOX_CTRL0_REG,MBOX_NO_ACTION);

#if (RUNTIME_PLATFORM == PLATFORM_XP70)
    MBOX_REG32_Write(INBOX_CTRL0_REG,MBOX_ACK);
#endif

    return TRUE;
}

/*******************************************************************************
Name        : MBOX_Term
Description : MBOX termination routine
Parameters  : none
Assumptions : All channels are already closed and ready for power-down
Limitations :
Returns     : none
*******************************************************************************/
void MBOX_Term (void)
{
#if (RUNTIME_PLATFORM == PLATFORM_ARM)
    vibe_os_delete_resource_lock(MBoxLock);
#endif
}

/*******************************************************************************
Name        : copy_to/from_IN_COMM, copy_to/from_OUT_COMM
Description : helper routines to copy data between COMM_X mailbox and data
              memory.
Parameters  : none
Assumptions :
Limitations :
Returns     : none
*******************************************************************************/
bool copy_to_IN_COMM (void* addr, uint16_t offset, uint16_t size)
{
    uint8_t  i,mbox_regs;
    mbox_regs = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    if (mbox_regs > INBOX_PAYLD_WIDTH) return FALSE;
    loop_copy((uint8_t*)INBOX_COMM_MSG,(uint8_t*)addr+offset,size);
    for (i = 0; i < mbox_regs; i++)
        MBOX_REG32_Write(INBOX_PAYLD_REG + i * sizeof(uint32_t),INBOX_COMM_MSG[i]);
    return TRUE;
}

bool copy_from_IN_COMM (void* addr, uint16_t offset, uint16_t size)
{
    uint8_t  i,mbox_regs;
    mbox_regs = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    if (mbox_regs > INBOX_PAYLD_WIDTH) return FALSE;
    for (i = 0; i < mbox_regs; i++)
        INBOX_COMM_MSG[i] = MBOX_REG32_Read(INBOX_PAYLD_REG + i * sizeof(uint32_t));
    loop_copy((uint8_t*)addr+offset,(uint8_t*)INBOX_COMM_MSG,size);
    return TRUE;
}

bool copy_to_OUT_COMM (void* addr, uint16_t offset, uint16_t size)
{
    uint8_t  i,mbox_regs;
    loop_copy((uint8_t*)OUTBOX_COMM_MSG,(uint8_t*)addr+offset,size);
    mbox_regs = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    if (mbox_regs > OUTBOX_PAYLD_WIDTH) return FALSE;
    for (i = 0; i < mbox_regs; i++)
        MBOX_REG32_Write(OUTBOX_PAYLD_REG + i * sizeof(uint32_t),OUTBOX_COMM_MSG[i]);
    return TRUE;
}

bool copy_from_OUT_COMM (void* addr, uint16_t offset, uint16_t size)
{
    uint8_t  i,mbox_regs;
    mbox_regs = (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    if (mbox_regs > OUTBOX_PAYLD_WIDTH) return FALSE;
    for (i = 0; i < mbox_regs; i++)
        OUTBOX_COMM_MSG[i] = MBOX_REG32_Read(OUTBOX_PAYLD_REG + i * sizeof(uint32_t));
    loop_copy((uint8_t*)addr+offset,(uint8_t*)OUTBOX_COMM_MSG,size);
    return TRUE;
}

/*******************************************************************************
Name        : inbox_CheckValidMessage
Description : Routine to check for valid messages
Parameters  : none
Assumptions :
Limitations :
Returns     : TRUE iff valid message is available
*******************************************************************************/
bool inbox_CheckValidMessage (void)
{
    uint16_t cmd0, op;

    if ((MBOX_REG32_Read(INBOX_IRQ_REG) & INBOX_IRQ_MASK) == 0)
        return FALSE;

    MBOX_REG32_Clear(INBOX_IRQ_REG, INBOX_IRQ_MASK);

    cmd0   = MBOX_REG32_Read(INBOX_CTRL0_REG);
    op     = (cmd0 & CMD0_OP_MASK      ) >> CMD0_OP_OFFSET;

    if (op == MBOX_NO_ACTION ||     // don't respond when no request
        op == MBOX_ERR ||           // don't respond to ERR
        op == MBOX_ACK)             // don't respond to own ACK
    {
        TrMBOX(DBG_INFO,"invalid op=%d\n",op);
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
Name        : inbox_Process
Description : Process message that was received via MBOX
Parameters  : none
Assumptions :
Limitations :
Returns     : bool - TRUE iff transmission is complete
*******************************************************************************/
bool inbox_Process (void)
{
    FVDP_Result_t result  = FVDP_OK;
#if (RUNTIME_PLATFORM == PLATFORM_XP70)
    FBMError_t fbm_status = FBM_ERROR_NONE;
#endif

    uint16_t cmd0   = MBOX_REG32_Read(INBOX_CTRL0_REG);
    uint32_t cmd1   = MBOX_REG32_Read(INBOX_CTRL1_REG);

    // extract command elements
    uint16_t op     = (cmd0 & CMD0_OP_MASK      ) >> CMD0_OP_OFFSET;
    uint16_t sop    = (cmd0 & CMD0_SOP_MASK     ) >> CMD0_SOP_OFFSET;
    bool     done   = (cmd0 & CMD0_DONE_MASK    ) >> CMD0_DONE_OFFSET;
    uint16_t offset = (cmd1 & CMD1_OFFSET_MASK  ) >> CMD1_OFFSET_OFFSET;
    uint16_t size   = (cmd1 & CMD1_SIZE_MASK    ) >> CMD1_SIZE_OFFSET;

    TrMBOX(DBG_INFO,"(%d): got INBOX_CTRL0_REG=%x INBOX_CTRL1_REG=%x "
                    "op=%d sop=%d offset=%d size=%d\n",
                    MY_MBOX_ID,cmd0,cmd1,op,sop,offset,size);

    // exit, if payload size is erroneous
    if (size > INBOX_PAYLD_WIDTH * sizeof(uint32_t))
    {
        TrMBOX(DBG_ERROR,"(%d): payload size %d exceeds %d\n",
               MY_MBOX_ID,size,INBOX_PAYLD_WIDTH);
        MBOX_REG32_SetField(INBOX_CTRL0_REG,CMD0_OP,MBOX_ERR);
        return TRUE;
    }

    // exit, if payload location is erroneous
    {
        uint32_t limit_compare;

#if (RUNTIME_PLATFORM == PLATFORM_XP70)
        if ( sop == SHMEM_RD_FBM_LOG)
        {
    #if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
            limit_compare = sizeof(vcd_log);
    #else
            limit_compare = sizeof(FBMLog.Data);
    #endif
        }
        else
#endif
        {
            sop = sop;  // warning suppression
            limit_compare = sizeof(comm_buff);
        }

        if ( (uint32_t) offset + size > limit_compare)
        {
            TrMBOX(DBG_ERROR,"(%d): offset and size out-of-bounds, "
                             "offset=%d size=%d limit=%d\n",
                   MY_MBOX_ID,offset,size,sizeof(comm_buff));
            mbox_handle_anomaly(__LINE__, cmd1);
            MBOX_REG32_SetField(INBOX_CTRL0_REG,CMD0_OP,MBOX_ERR);
            return TRUE;
        }
    }

    // processing, indicate busy
    MBOX_REG32_SetField(INBOX_CTRL0_REG,CMD0_OP,MBOX_BUSY);

    // switch on message operation
    switch((uint8_t) op)
    {
    case MBOX_SHMEM_RD:
        {
            uint32_t *buffer_p;
#if (RUNTIME_PLATFORM == PLATFORM_XP70)
    #if (VCD_EXTENDED_LOG_CONTROLS == TRUE)
            if ( sop == SHMEM_RD_FBM_LOG)
            {
                buffer_p = (uint32_t *) &vcd_log;
            }
            else
    #elif (FBM_LOG_OUTPUT_ENABLE == TRUE)
            if ( sop == SHMEM_RD_FBM_LOG)
            {
                // adjust offset to "unwrap" the debug log while reading it
                uint16_t start_row;
                start_row = FBMLog.Ptr + NUM_FBM_LOG_ELEMENTS - FBMLog.Count;
                if (start_row >= NUM_FBM_LOG_ELEMENTS)
                    start_row -= NUM_FBM_LOG_ELEMENTS;
                offset += start_row * sizeof(FBMLog.Data[0]);
                if (offset >= sizeof(FBMLog.Data))
                    offset -= sizeof(FBMLog.Data);

                buffer_p = (uint32_t *) &(FBMLog.Data[0]);
            }
            else
    #endif
#endif
                buffer_p = (uint32_t *) &comm_buff;

            // put data from buffer to COMM registers
            copy_to_IN_COMM( buffer_p, offset, size);
        }
        break;

    case MBOX_SHMEM_WR:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);
        break;

#if (RUNTIME_PLATFORM == PLATFORM_XP70)
    case MBOX_SEND_AFM_PARAMS:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);

        // if this is the last packet, invoke AFP parameter setup
        if (done)
        {
            AFP1_LoadParameters(&((afp_param_t*)comm_buff)->afp1);
            AFP2_LoadParameters(&((afp_param_t*)comm_buff)->afp2);
        }
        break;

    case MBOX_SEND_BUFFER_TABLE:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);

        // if this is the last packet, invoke AFP parameter setup
        if (done)
        {
            SendBufferTableCommand_t* pCommand = (SendBufferTableCommand_t*) comm_buff;
            FBMContext_t* pCtxt = FBM_Common_GetContext(pCommand->Channel);
            fbm_status = FBM_Common_SetBufferTable(pCtxt,(BufferTable_t*) &pCommand->Table[0],
                                                   pCommand->Rows);
        }
        break;

    case MBOX_SEND_PROCESSING_STATE:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);

        // if this is the last packet, set processing state
        if (done)
        {
            FbmStateConfig_t* pFbmState = (FbmStateConfig_t*) &comm_buff;
            FBMContext_t* pCtxt = FBM_Common_GetContext(pFbmState->Channel);

            if (pCtxt != NULL)
                FBM_Common_SetProcessingState(pCtxt, pFbmState->State);
        }
        break;

    case MBOX_FBM_INPUT_PROCESS:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);

        // if this is the last packet, invoke input process and
        // store return data into COMM registers
        if (done)
        {
            InputFrameData_t* pInFrameData = (InputFrameData_t*) &comm_buff;
            FBMContext_t* pCtxt = FBM_Common_GetContext(pInFrameData->Channel);
            FrameBufferIndices_t fbi;

            memset(&fbi, 0xFF, sizeof(FrameBufferIndices_t));

            if (pCtxt != NULL)
            {
                // if FBM_Input_Process() was called but was not followed-up with
                // FBM_Input_Frame_Ready(), then call FBM_Input_Frame_Ready() here.
                // Note that this can happen if AFM is not enabled, or temporarily
                // disabled for the first few frames in which previous or
                // previous-1 frames are not yet available.
                if (pCtxt->InputProcessTriggered == TRUE)
                {
                    fbm_status = FBM_Input_FrameReady(pCtxt);
                    #if (EXTRA_DEBUG_INFO_IN_SPARE_REGS == TRUE)
                        if (CH_DEBUG == pCtxt->Channel)
                        {
                            fbm_status |= REG32_GetField(FBM_STATUS0, FBM_STATUS_PREV);
                            REG32_SetField(FBM_STATUS0, FBM_STATUS_PREV, fbm_status);
                        }
                    #endif
                }

                fbm_status = FBM_Input_Process((InputFrameData_t*) &comm_buff,&fbi);
                #if (EXTRA_DEBUG_INFO_IN_SPARE_REGS == TRUE)
                    if (CH_DEBUG == pCtxt->Channel)
                    {
                        fbm_status |= REG32_GetField(FBM_STATUS0, FBM_STATUS_INPUT);
                        DBG32_SetField(pCtxt, FBM_STATUS0, FBM_STATUS_INPUT, fbm_status);
                    }
                #endif
            }

            copy_to_IN_COMM(&fbi, 0, sizeof(FrameBufferIndices_t));
        }
        break;

    case MBOX_FBM_OUTPUT_PROCESS:
        // write data from COMM registers to buffer
        copy_from_IN_COMM(&comm_buff,offset,size);

        // invoke output process and store return data into COMM registers
        if (done)
        {
            OutputFrameSelect_t* pOutputFrameSelect = (OutputFrameSelect_t*) &comm_buff;
            FBMContext_t* pCtxt = FBM_Common_GetContext(pOutputFrameSelect->Channel);
            OutputFrameData_t ofd;

            memset(&ofd, 0xFF, sizeof(OutputFrameData_t));

            if (pCtxt != NULL)
            {
                fbm_status = FBM_Output_Process((OutputFrameSelect_t*) &comm_buff,&ofd);
                #if (EXTRA_DEBUG_INFO_IN_SPARE_REGS == TRUE)
                    fbm_status |= REG32_GetField(FBM_STATUS1, FBM_STATUS_OUTPUT);
                    DBG32_SetField(pCtxt, FBM_STATUS1, FBM_STATUS_OUTPUT, fbm_status);
                #endif
            }
            copy_to_IN_COMM(&ofd, 0, sizeof(OutputFrameData_t));
        }
        break;

    case MBOX_FBM_RESET:
        copy_from_IN_COMM(&comm_buff,offset,size);

        if (done)
        {
            FbmReset_t* pFbmReset = (FbmReset_t*)&comm_buff;
            FBMContext_t* pCtxt = FBM_Common_GetContext(pFbmReset->Channel);
            if (pCtxt != NULL)
                FBM_Common_Reset(pCtxt);
        }
        break;
#endif

    default:
        // unsupportted command - indicate error
        TrMBOX(DBG_WARN,"(%d): unsupportted command %d\n",
               MY_MBOX_ID,op);
        mbox_handle_anomaly(__LINE__, (uint32_t) op << 16);
        MBOX_REG32_SetField(INBOX_CTRL0_REG,CMD0_OP,MBOX_ERR);
        result = FVDP_ERROR;
        break;
    }

    // write status and send acknowledge
#if (RUNTIME_PLATFORM == PLATFORM_XP70)
    if ((fbm_status & ~FBM_WARN_MASK) != FBM_ERROR_NONE)
        result = FVDP_ERROR;
#endif

    MBOX_REG32_Write(INBOX_CTRL1_REG,result);
    MBOX_REG32_SetField(INBOX_CTRL0_REG,CMD0_OP,MBOX_ACK);

    if (done)
    {
        TrMBOX(DBG_INFO,"done\n");
    }
    return done;
}

/*******************************************************************************
Name        : outbox_SendPacket
Description : Sends a command through the mailbox
Parameters  : target - target mbox
              cmd - command of type MBOX_CMD_t
              sub_cmd - sub command of type MBOX_SUB_CMD_t
              offset - sofware register buffer offset
              size - sofware register buffer size
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
void outbox_SendPacket (MBOX_ID_t target, MBOX_CMD_t cmd, MBOX_SUB_CMD_t sub_cmd,
                        uint16_t offset, uint16_t size, bool done)
{
    uint16_t cmd0 = 0;
    uint32_t cmd1 = 0;

    cmd0 |= cmd         << CMD0_OP_OFFSET;
    cmd0 |= sub_cmd     << CMD0_SOP_OFFSET;
    cmd0 |= MY_MBOX_ID  << CMD0_SID_OFFSET;
    cmd0 |= target      << CMD0_RID_OFFSET;
    cmd0 |= done        << CMD0_DONE_OFFSET;

    cmd1 |= offset      << CMD1_OFFSET_OFFSET;
    cmd1 |= size        << CMD1_SIZE_OFFSET;

    MBOX_REG32_Write(OUTBOX_CTRL1_REG, cmd1);

    TrMBOX(DBG_INFO, "(%d): OUTBOX_CTRL1_REG=%x\n", MY_MBOX_ID,
         REG32_Read(OUTBOX_CTRL1_REG));

    MBOX_REG32_Write(OUTBOX_CTRL0_REG, cmd0);

    MBOX_REG32_Write(OUTBOX_IRQ_REG, OUTBOX_IRQ_MASK);
}

/*******************************************************************************
Name        : outbox_CheckAck
Description : checks for acknowledge from receiver
Parameters  : none
Assumptions :
Limitations :
Returns     : MBOX_CMD_t
*******************************************************************************/
MBOX_CMD_t outbox_CheckAck (void)
{
    static uint32_t prev_result = ~0;
    uint32_t result;

    // message received?
    if (MBOX_REG32_Read(OUTBOX_IRQ_REG) & OUTBOX_IRQ_MASK)
        return FALSE;

    result = MBOX_REG32_Read(OUTBOX_CTRL0_REG);

    if (result != prev_result)
    {
        TrMBOX(DBG_INFO, "(%d): OUTBOX_CTRL0_REG=%x\n",
               MY_MBOX_ID,result);
        prev_result = result;
    }

    // wait for message to be processed
    result = (result & CMD0_OP_MASK) >> CMD0_OP_OFFSET;

    if (result == MBOX_ACK || result == MBOX_ERR)
    {
        prev_result = ~0;
        MBOX_REG32_Write(OUTBOX_CTRL0_REG,MBOX_NO_ACTION);
    }

    return (MBOX_CMD_t) result;
}

/*******************************************************************************
Name        : OUTBOX_Send
Description : Writes-to and reads-from software register buffer at a particular
              offset and size
              Note that for read commands such as MBOX_SHMEM_RD, data is loaded
              into payload_addr, and NOT the reply_addr.  An analogy would be
              sending an empty bus, filling it with passengers, and having it
              return to the start location.
Parameters  : target_mbox - destination mbox to handle request
              cmd - mailbox command request
              sub_cmd - mailbox subcommand request
              payload_offset - sofware register buffer offset
              payload_size - sofware register buffer size
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t OUTBOX_Send (MBOX_ID_t target_mbox, MBOX_CMD_t cmd,
                           MBOX_SUB_CMD_t sub_cmd,
                           void* payload_addr, uint16_t payload_size)
{
    if (outbox_state != OUTBOX_IDLE)
        return FVDP_ERROR;

    TrMBOX(DBG_INFO, "(%d): to=%d cmd=%d addr=%d size=%d\n",
           MY_MBOX_ID, target_mbox, cmd, (uint32_t)payload_addr,payload_size);

    outbox_tx_target  = target_mbox;
    outbox_tx_cmd     = cmd;
    outbox_tx_sub_cmd = sub_cmd;
    outbox_tx_addr    = payload_addr;
    outbox_tx_offset  = 0;
    outbox_tx_size    = payload_size;
    outbox_tx_size_orig = payload_size;

    outbox_state = OUTBOX_SEND;
    return FVDP_OK;
}

/*******************************************************************************
Name        : OUTBOX_SendWaitReply
Description : Writes-to and reads-from software register buffer at a particular
              offset and size, waits for acknowledgement, and loads the reply
              using specified address and size.
              Note that for read commands such as MBOX_SHMEM_RD, data is loaded
              into payload_addr, and NOT the reply_addr.  An analogy would be
              sending an empty bus, filling it with passengers, and having it
              return to the start location.
Parameters  : target_mbox - destination mbox to handle request
              cmd - mailbox command request
              sub_cmd - mailbox subcommand request
              payload_addr - buffer offset
              payload_size - buffer size
              reply_offset - buffer offset
              reply_size - buffer size
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t OUTBOX_SendWaitReply (MBOX_ID_t target_mbox, MBOX_CMD_t cmd,
                                    MBOX_SUB_CMD_t sub_cmd,
                                    void* payload_addr, uint16_t payload_size,
                                    void* reply_addr, uint16_t reply_size)
{
    FVDP_Result_t result;

    result = OUTBOX_Send(target_mbox, cmd, sub_cmd, payload_addr, payload_size);
    if (result == FVDP_OK)
    {
        while (MBOX_Process() == FALSE);

        if (reply_addr && reply_size)
            copy_from_OUT_COMM(reply_addr, 0, reply_size);

        result = MBOX_REG32_Read(OUTBOX_CTRL1_REG);
    }
    return result;
}

/*******************************************************************************
Name        : outbox_Process
Description : state machine to perform outbox activities
Parameters  : none
Assumptions :
Limitations :
Returns     : bool - returns TRUE iff completed transmission
*******************************************************************************/
bool outbox_Process (void)
{
    uint16_t        max_size = OUTBOX_PAYLD_WIDTH * sizeof(uint32_t);
    uint16_t        chunk_size;
    bool            done;
    MBOX_CMD_t      mbox_status = MBOX_NO_ACTION;
    FVDP_Result_t   result = FVDP_OK;

    if (outbox_state == OUTBOX_IDLE)
        return TRUE;

    chunk_size = (outbox_tx_size > max_size) ? max_size : outbox_tx_size;
    done = (outbox_tx_size > max_size) ? 0 : 1;

    if (outbox_state == OUTBOX_SEND)
    {
        TrMBOX(DBG_INFO, "(%d): SENDING to=%d cmd=%d offset=%d size=%d\n",
             MY_MBOX_ID,outbox_tx_target,
             outbox_tx_cmd,outbox_tx_offset,chunk_size);

        if (outbox_tx_cmd != MBOX_SHMEM_RD)
        {
            if (outbox_tx_offset + chunk_size <= sizeof(comm_buff))
            {
                copy_to_OUT_COMM(outbox_tx_addr,outbox_tx_offset,chunk_size);
            }
            else
            {
                TrMBOX(DBG_ERROR, "(%d): write operation out-of-range, "
                                  "offset=%d size=%d limit=%d\n",
                                  MY_MBOX_ID,
                                  outbox_tx_offset,chunk_size,
                                  sizeof(comm_buff));
                outbox_state = OUTBOX_IDLE;
                result = FVDP_ERROR;
                goto outbox_Process_exit;
            }
        }

        outbox_SendPacket(outbox_tx_target,outbox_tx_cmd, outbox_tx_sub_cmd,
                          outbox_tx_offset,chunk_size,done);
        outbox_state = OUTBOX_WAIT_ACK;
        outbox_retries = 0;
        outbox_idle_count = 0;
    }
    if (outbox_state == OUTBOX_WAIT_ACK)
    {
        mbox_status = outbox_CheckAck();
        if (mbox_status == MBOX_ACK)
        {
            if (outbox_tx_cmd == MBOX_SHMEM_RD)
            {
                uint32_t limit_compare;

                if ( outbox_tx_sub_cmd == SHMEM_RD_FBM_LOG)
                {
                    limit_compare = outbox_tx_size_orig;
                }
                else
                {
                    limit_compare = sizeof(comm_buff);
                }

                if ( (uint32_t) (outbox_tx_offset + chunk_size) <= limit_compare)
                {
                    copy_from_OUT_COMM(outbox_tx_addr,outbox_tx_offset,chunk_size);
                }
                else
                {
                    TrMBOX(DBG_ERROR, "(%d): read operation out-of-range, "
                                      "offset=%d size=%d limit=%d\n",
                                      MY_MBOX_ID,
                                      outbox_tx_offset,chunk_size,
                                      sizeof(comm_buff));
                    outbox_state = OUTBOX_IDLE;
                    result = FVDP_ERROR;
                    goto outbox_Process_exit;
                }
            }

            outbox_tx_size -= chunk_size;
            outbox_tx_offset += chunk_size;
            if (outbox_tx_size > 0)
            {
                outbox_state = OUTBOX_SEND;
            }
            else
            {
                TrMBOX(DBG_INFO, "(%d): IDLE\n", MY_MBOX_ID);
                outbox_state = OUTBOX_IDLE;
            }
        }
        else if (mbox_status == MBOX_ERR)
        {
            if (++outbox_retries < MBOX_MAX_RETRIES)
            {
                TrMBOX(DBG_INFO, "(%d): RETRYING\n", MY_MBOX_ID);
                outbox_state = OUTBOX_SEND;
            }
            else
                result = FVDP_ERROR;
        }
        else
        {
            // waiting for response
#if (RUNTIME_PLATFORM == PLATFORM_ARM)
            if (++outbox_idle_count > OUTBOX_IDLE_MAX)
            {
                if (++outbox_retries < MBOX_MAX_RETRIES)
                {
                    TrMBOX(DBG_INFO, "still waiting for response...\n");
                    outbox_idle_count = 0;
                }
                else
                    result = FVDP_ERROR;
            }
#endif
        }
    }

outbox_Process_exit:
    if (result != FVDP_OK)
    {
        // critical error handling
        mbox_handle_anomaly(__LINE__, 0
                                      | ((uint32_t) outbox_tx_cmd << 16)
                                      | mbox_status);
        MBOX_REG32_Write(OUTBOX_CTRL0_REG, MBOX_ERR);
        outbox_state = OUTBOX_IDLE;
    }

    return (outbox_state == OUTBOX_IDLE);
}

/*******************************************************************************
Name        : MBOX_Process
Description : Immediately process INBOX and OUTBOX activity
Parameters  : none
Assumptions :
Limitations :
Returns     : bool - TRUE iff both input and output processes are complete
*******************************************************************************/
bool MBOX_Process (void)
{
    bool done = TRUE;

    vibe_os_lock_resource(MBoxLock);

#if defined(FVDP_SIMULATION)
    //_do_nothing();
#endif

    do
    {
        if (inbox_CheckValidMessage())
            done = inbox_Process();
    } while (!done);

#if (RUNTIME_PLATFORM != PLATFORM_XP70)
    done = done && outbox_Process();
#endif

    vibe_os_unlock_resource(MBoxLock);

    return done;
}

/*******************************************************************************
Name        : MBOX_SharedMemRead
Description : mailbox software register read operation
Parameters  : target_mbox - target mbox to read from
               sub_cmd - sub command
              payload_offset - address read offset in comm_buff
              payload_size - size in bytes to be transferred
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t MBOX_SharedMemRead(MBOX_ID_t target_mbox, MBOX_SUB_CMD_t sub_cmd,
                                 void* payload_addr, uint16_t payload_size)
{
    FVDP_Result_t result;

    result = OUTBOX_SendWaitReply(target_mbox,MBOX_SHMEM_RD, sub_cmd,
                                  payload_addr,payload_size,NULL,0);
    return result;
}

/*******************************************************************************
Name        : MBOX_SharedMemWrite
Description : mailbox software register write operation
Parameters  : target_mbox - target mbox to write to
              payload_offset - address write offset in comm_buff
              payload_size - size in bytes to be transferred
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t MBOX_SharedMemWrite(MBOX_ID_t target_mbox,void* payload_addr,
                                  uint16_t payload_size)
{
    FVDP_Result_t result;

    result = OUTBOX_SendWaitReply(target_mbox,MBOX_SHMEM_WR, MBOX_SUB_CMD_NONE,
                                  payload_addr,payload_size,NULL,0);
    return result;
}

/*******************************************************************************
Name        : void mbox_handle_anomaly (uint32_t line, uint32_t ref)
Description : This routine handles the task of dealing with unexpected behaviour
              with respects to mailbox communications
Parameters  : line - line in which the error occurred
              ref - generic debug parameter
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void mbox_handle_anomaly (uint32_t line, uint32_t ref)
{
#if (RUNTIME_PLATFORM == PLATFORM_ARM)
    TrMBOX(DBG_ERROR,"%s: unexpected result (line %d, ref 0x%08x)\n",
        __FILE__, line, ref);

  #ifdef DEBUG_TRAP_MBOX_ANOMALY
    {
        bool debug_lock = 1;
        while(debug_lock);
    }
  #endif

#endif
}

/*******************************************************************************
Name        : STREAMER_Tx
Description : Streamer logical channel write
Parameters  : source_phys_addr - source address destination
              dest_base_addr - desitnation base address in VCPU
              size - size in bytes to be transferred
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t STREAMER_Tx (uint32_t source_phys_addr, uint32_t dest_base_addr,
                           uint32_t size)
{
    uint32_t count = 0, chunk_size;

    /* max streamer transfer is 8k, need to break up data and code memory
       into 8k chunks for the transfer */
    while ( size > 0 )
    {
        uint8_t  StreamerBusy = MBX_BIT_CLEARED;
        uint32_t StreamerDoneIRQStatus = 0;
        uint32_t TimeCount;

        chunk_size = size;
        if (chunk_size > MAX_STREAMER_TRANSFER_SIZE)
            chunk_size = MAX_STREAMER_TRANSFER_SIZE;

        /*
        ** load vcpu data memory image from ddr to TCPM/TCDM via streamer
        ** Program BRrblEA_0 to -> external source address
        ** Program BRrblPM_0 to -> external source transfer size
        ** Program BRrblIA_0 to -> internal start address
        ** Program BRrblRA_0 to -> internal reload address
        ** Program BRrblll_0 to 1
        */
        STREAMER_REG32_Write(BR0EA, (source_phys_addr +
                            (uint32_t) (count * MAX_STREAMER_TRANSFER_SIZE)));
        STREAMER_REG32_Write(BR0PM, (chunk_size - 1));
        STREAMER_REG32_Write(BR0IA, (dest_base_addr +
                            (uint32_t)( count * MAX_STREAMER_TRANSFER_SIZE) ));
        STREAMER_REG32_Write(BR0RA, 0);
        STREAMER_REG32_Write(BR0II, 1);

        /* Program BSF:STBUSRSF to 1 */
        STREAMER_REG32_SetField( BSF, STBUSRSF, 1);

        /* wait for data loading to begin */
        TimeCount = 0;
        do
        {
            StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSRSF);
            TimeCount++;
        } while ( ( StreamerBusy == MBX_BIT_CLEARED) &&
                  ( TimeCount < WAIT_FOR_BITSET_TIME) );

        TimeCount = 0;

        /* wait for STBUSRSF bit to be cleared */
        do
        {
            StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSRSF);
            TimeCount++;
        } while ( ( StreamerBusy == MBX_BIT_SET) &&
                  ( TimeCount < WAIT_FOR_VCPU_STREAMER_TIME));

        if ( TimeCount == WAIT_FOR_VCPU_STREAMER_TIME)
        {
            TrMBOX(DBG_ERROR, "Streamer loading timed out.\n");
            return FVDP_INFO_NOT_AVAILABLE;
        }

        StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSRSF);
        TrMBOX(DBG_INFO, "StreamerBusy = %d, chunk = %d\n", \
             StreamerBusy, count);

        /* clear streamer done interrupt */
        StreamerDoneIRQStatus = REG32_Read(STR_IT_CONTROL);
        TrMBOX(DBG_INFO, "StreamerDoneIRQStatus = %d, chunk = %d\n", \
            StreamerDoneIRQStatus, count);

        MBOX_REG32_Write(STR_IT_CONTROL, StreamerDoneIRQStatus);

        size -= chunk_size;
        count ++;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : STREAMER_Rx
Description : Streamer logical channel read
Parameters  : dest_phys_addr - destination physical address
              src_base_addr - source base address in VCPU
              size - size in bytes to be transferred
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t STREAMER_Rx (uint32_t dest_phys_addr, uint32_t src_base_addr,
                           uint32_t size)
{
    uint32_t count = 0, chunk_size;

    /* max streamer transfer is 8k, need to break up data and code memory
       into 8k chunks for the transfer */
    while ( size > 0 )
    {
        uint8_t  StreamerBusy = MBX_BIT_CLEARED;
        uint32_t StreamerDoneIRQStatus = 0;
        uint32_t TimeCount;

        chunk_size = size;
        if (chunk_size > MAX_STREAMER_TRANSFER_SIZE)
            chunk_size = MAX_STREAMER_TRANSFER_SIZE;

        /*
        ** load vcpu data memory image from ddr to TCPM/TCDM via streamer
        ** Program BWrblEA_0 to -> external destination address
        ** Program BWrblPM_0 to -> external source transfer size
        ** Program BWrblIA_0 to -> internal start address
        ** Program BWrblRA_0 to -> internal reload address
        ** Program BWrblll_0 to 1
        */
        STREAMER_REG32_Write(BW0EA, (dest_phys_addr +
                            (uint32_t) (count * MAX_STREAMER_TRANSFER_SIZE)));
        STREAMER_REG32_Write(BW0PM, (chunk_size - 1));
        STREAMER_REG32_Write(BW0IA, (src_base_addr +
                            (uint32_t)( count * MAX_STREAMER_TRANSFER_SIZE) ));
        STREAMER_REG32_Write(BW0RA, 0);
        STREAMER_REG32_Write(BW0II, 1);

        /* Program BSF:STBUSWSF to 1 */
        STREAMER_REG32_SetField( BSF, STBUSWSF, 1);

        /* wait for data loading to begin */
        TimeCount = 0;
        do
        {
            StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSWSF);
            TimeCount++;
        } while ( ( StreamerBusy == MBX_BIT_CLEARED) &&
                  ( TimeCount < WAIT_FOR_BITSET_TIME) );

        TimeCount = 0;
        /* wait for STBUSRSF bit to be cleared */
        do
        {
            StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSWSF);
            TimeCount++;
        } while ( ( StreamerBusy == MBX_BIT_SET) &&
                  ( TimeCount < WAIT_FOR_VCPU_STREAMER_TIME));

        if ( TimeCount == WAIT_FOR_VCPU_STREAMER_TIME)
        {
            TrMBOX(DBG_ERROR, "Streamer loading timed out.\n");
            return FVDP_INFO_NOT_AVAILABLE;
        }

        StreamerBusy = STREAMER_REG32_GetField(BSF,STBUSWSF);
        TrMBOX(DBG_INFO, "StreamerBusy = %d, chunk = %d\n",
             StreamerBusy, count);

        /* clear streamer done interrupt */
        StreamerDoneIRQStatus = MBOX_REG32_Read(STR_IT_CONTROL);
        TrMBOX(DBG_INFO, "StreamerDoneIRQStatus = %d, chunk = %d\n",
            StreamerDoneIRQStatus, count);

        MBOX_REG32_Write(STR_IT_CONTROL, StreamerDoneIRQStatus);

        size -= chunk_size;
        count ++;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : STREAMERQ_Init
Description : initialize streamer logical queue
Parameters  : id - queue ID
              buf_addr - buffer address to read/write data
              buf_size - size in bytes to transfer
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
void STREAMERQ_Init (StreamerChannel_t id, uint32_t buf_addr, uint16_t buf_size)
{
    const uint32_t IA[] = { Q0R0IA, Q0R1IA };
    const uint32_t RA[] = { Q0R0RA, Q0R1IA };
    const uint32_t PM[] = { Q0R0PM, Q0R1IA };

    // (set start address, reload addr, tx size and tx width)
    STREAMER_REG32_SetField(IA[id], INTSA,   buf_addr);
    STREAMER_REG32_SetField(RA[id], INTRA,   buf_addr);
    STREAMER_REG32_SetField(PM[id], RDQSZ,   buf_size);
    STREAMER_REG32_SetField(PM[id], QTW,     2);     // 4 bytes
}

/*******************************************************************************
Name        : STREAMERQ_Arm
Description : arm streamer logical queue
Parameters  : id - queue ID
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
void STREAMERQ_Arm (StreamerChannel_t id)
{
#if 0
    const uint8_t MASK[] = { BIT0, BIT1 };
    STREAMER_REG32_SetField(RQSF0, RQSFrqp0, MASK[id]);     // queue synchronization flag
#endif
}

/*******************************************************************************
Name        : STREAMERQ_Disarm
Description : disarm streamer logical queue
Parameters  : id - queue ID
Assumptions :
Limitations : may have a side effect of firing off streamer interrupts
Returns     : FVDP_Result_t
*******************************************************************************/
void STREAMERQ_Disarm (StreamerChannel_t id)
{
#if 0
    const uint8_t MASK[] = { BIT0, BIT1 };
    STREAMER_REG32_SetField(CRQSF0, CRQSFrqp0, MASK[id]);   // clear synchronization flag
#endif
}


/* end of file */
