#ifndef _STM_SCR_PROTOCOL_T1_H
#define _STM_SCR_PROTOCOL_T1_H

#include "scr_internal.h"
#include "scr_io.h"

/* Block types */
#define SCR_BLOCK_TYPE_BIT          0xC0
#define SCR_S_BLOCK                 0xC0
#define SCR_R_BLOCK                 0x80
#define SCR_I_BLOCK_1               0x00
#define SCR_I_BLOCK_2               0x40

/* In S-block */
#define SCR_S_RESYNCH_REQUEST       0x00
#define SCR_S_IFS_REQUEST           0x01
#define SCR_S_ABORT_REQUEST         0x02
#define SCR_S_WTX_REQUEST           0x03

#define SCR_S_RESYNCH_RESPONSE      0x20
#define SCR_S_IFS_RESPONSE          0x21
#define SCR_S_ABORT_RESPONSE        0x22
#define SCR_S_WTX_RESPONSE          0x23

#define SCR_S_RESPONSE              0x20

/* I-block */
#define SCR_I_CHAINING_BIT          0x20
#define SCR_I_SEQUENCE_BIT          0x40

/* R-block */
#define SCR_R_NO_ERROR             0x00
#define SCR_R_EDC_ERROR             0x01
#define SCR_R_OTHER_ERROR           0x02


uint32_t  scr_transfer_sblock(scr_ctrl_t *scr_handle_p,uint8_t type,uint8_t data);

uint32_t scr_transfer_t1(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t  * trans_params_p);

#endif
