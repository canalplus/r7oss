/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_MBOX_H
#define FVDP_MBOX_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Includes ----------------------------------------------------------------- */

#include "fvdp_private.h"
#include "fvdp_common.h"
#include "fvdp_router.h"
#include "fvdp_config.h"
#include "fvdp_fbm.h"

/* Exported Constants ------------------------------------------------------- */

#if (RUNTIME_PLATFORM == PLATFORM_XP70)
#define MY_MBOX_ID          MBOX_ID_VCPU

#define INBOX_IRQ_REG       IRQ_TO_HOST
#define INBOX_IRQ_MASK      IT_TO_HOST_MASK
#define INBOX_CTRL0_REG     INFO_HOST
#define INBOX_CTRL1_REG     COMM_0
#define INBOX_PAYLD_REG     COMM_1
#define INBOX_PAYLD_WIDTH   15

#define OUTBOX_IRQ_REG      IRQ_TO_VCPU
#define OUTBOX_IRQ_MASK     ITTOVCPU_MASK
#define OUTBOX_CTRL0_REG    INFO_VCPU
#define OUTBOX_CTRL1_REG    COMM_16
#define OUTBOX_PAYLD_REG    COMM_17
#define OUTBOX_PAYLD_WIDTH  15
#else
#define MY_MBOX_ID          MBOX_ID_HOST

#define INBOX_IRQ_REG       IRQ_TO_VCPU
#define INBOX_IRQ_MASK      ITTOVCPU_MASK
#define INBOX_CTRL0_REG     INFO_VCPU
#define INBOX_CTRL1_REG     COMM_16
#define INBOX_PAYLD_REG     COMM_17
#define INBOX_PAYLD_WIDTH   15

#define OUTBOX_IRQ_REG      IRQ_TO_HOST
#define OUTBOX_IRQ_MASK     IT_TO_HOST_MASK
#define OUTBOX_CTRL0_REG    INFO_HOST
#define OUTBOX_CTRL1_REG    COMM_0
#define OUTBOX_PAYLD_REG    COMM_1
#define OUTBOX_PAYLD_WIDTH  15
#endif

#define MBX_BIT_SET         1
#define MBX_BIT_CLEARED     0

#ifndef PACKED
#define PACKED  __attribute__((packed))
#endif

/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    MBOX_ID_HOST = 0,
    MBOX_ID_CCPU,
    MBOX_ID_VCPU,
} MBOX_ID_t;

typedef enum
{
    STM_RDQ_LCH0 = 0,
    STM_RDQ_LCH1,
    NUM_STM_RDQ
} StreamerChannel_t;

typedef enum
{
    MBOX_NO_ACTION = 0,
    MBOX_ACK,
    MBOX_BUSY,
    MBOX_SHMEM_RD,
    MBOX_SHMEM_WR,
    MBOX_SEND_AFM_PARAMS,
    MBOX_SEND_BUFFER_TABLE,
    MBOX_SEND_PROCESSING_STATE,
    MBOX_FBM_INPUT_PROCESS,
    MBOX_FBM_OUTPUT_PROCESS,
    MBOX_FBM_RESET,
    MBOX_SET_DATAPATH,
    MBOX_CMD_LAST,
    MBOX_ERR  = 15
} MBOX_CMD_t;

typedef enum
{
    MBOX_SUB_CMD_NONE = 0,
    SHMEM_RD_FBM_LOG,
} MBOX_SUB_CMD_t;

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

#ifndef offsetof
#define offsetof(st,m) \
     ((uint32_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#endif

// mailbox-related register access macros
#define MBOX_REG32_Write(Addr,Val)  REG32_Write(FVDP_MAILBOX_BASE_ADDRESS+(Addr),(Val))
#define MBOX_REG32_Read(Addr)       REG32_Read(FVDP_MAILBOX_BASE_ADDRESS+(Addr))
#define MBOX_REG32_Set(Addr,Mask)   REG32_Set(FVDP_MAILBOX_BASE_ADDRESS+(Addr),(Mask))
#define MBOX_REG32_Clear(Addr,Mask) REG32_Clear(FVDP_MAILBOX_BASE_ADDRESS+(Addr),(Mask))
#define MBOX_REG32_ClearAndSet(Addr,Mask,Offset,ValueToSet) \
    REG32_ClearAndSet(FVDP_MAILBOX_BASE_ADDRESS+(Addr),(Mask),(Offset),(ValueToSet))
#define MBOX_REG32_SetField(Addr,Field,ValueToSet) \
    REG32_ClearAndSet(FVDP_MAILBOX_BASE_ADDRESS+(Addr),Field ## _MASK,Field ## _OFFSET,ValueToSet)
#define MBOX_REG32_GetField(Addr,Field) \
    ((REG32_Read(FVDP_MAILBOX_BASE_ADDRESS+(Addr)) & (Field ## _MASK)) >> (Field ## _OFFSET))

// streamer-related register access macros
#define STREAMER_REG32_Write(Addr,Val)  REG32_Write(FVDP_STREAMER_BASE_ADDRESS+(Addr),(Val))
#define STREAMER_REG32_Read(Addr)       REG32_Read(FVDP_STREAMER_BASE_ADDRESS+(Addr))
#define STREAMER_REG32_Set(Addr,Mask)   REG32_Set(FVDP_STREAMER_BASE_ADDRESS+(Addr),(Mask))
#define STREAMER_REG32_Clear(Addr,Mask) REG32_Clear(FVDP_STREAMER_BASE_ADDRESS+(Addr),(Mask))
#define STREAMER_REG32_ClearAndSet(Addr,Mask,Offset,ValueToSet) \
    REG32_ClearAndSet(FVDP_STREAMER_BASE_ADDRESS+(Addr),(Mask),(Offset),(ValueToSet))
#define STREAMER_REG32_SetField(Addr,Field,ValueToSet) \
    REG32_ClearAndSet(FVDP_STREAMER_BASE_ADDRESS+(Addr),Field ## _MASK,Field ## _OFFSET,ValueToSet)
#define STREAMER_REG32_GetField(Addr,Field) \
    ((REG32_Read(FVDP_STREAMER_BASE_ADDRESS+(Addr)) & (Field ## _MASK)) >> (Field ## _OFFSET))

/* Exported Functions ------------------------------------------------------- */

extern bool          MBOX_Init          (void);
extern void          MBOX_Term          (void);
extern bool          MBOX_Process       (void);

extern FVDP_Result_t OUTBOX_Send           (MBOX_ID_t target_mbox,
                                            MBOX_CMD_t cmd,
                                            MBOX_SUB_CMD_t sub_cmd,
                                            void* payload_addr,
                                            uint16_t payload_size);
extern FVDP_Result_t OUTBOX_SendWaitReply  (MBOX_ID_t target_mbox,
                                            MBOX_CMD_t cmd,
                                            MBOX_SUB_CMD_t sub_cmd,
                                            void* payload_addr,
                                            uint16_t payload_size,
                                            void* reply_addr,
                                            uint16_t reply_size);
extern FVDP_Result_t MBOX_SharedMemRead    (MBOX_ID_t target_mbox,
                                            MBOX_SUB_CMD_t sub_cmd,
                                            void* payload_addr,
                                            uint16_t payload_size);
extern FVDP_Result_t MBOX_SharedMemWrite   (MBOX_ID_t target_mbox,
                                            void* payload_addr,
                                            uint16_t payload_size);

extern FVDP_Result_t STREAMER_Tx        (uint32_t source_phys_addr,
                                         uint32_t dest_base_addr,
                                         uint32_t size);
extern FVDP_Result_t STREAMER_Rx        (uint32_t dest_phys_addr,
                                         uint32_t src_base_addr,
                                         uint32_t size);

extern void          STREAMERQ_Init     (StreamerChannel_t id,
                                         uint32_t buf_addr,
                                         uint16_t buf_size);
extern void          STREAMERQ_Arm      (StreamerChannel_t id);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_MBOX_H */


/* end of file */
