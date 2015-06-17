
#ifndef __LPM_DEF_H
#define __LPM_DEF_H

/*Message commands */
#define STM_LPM_MSG_NOP           0x0
#define STM_LPM_MSG_VER           0x1
#define STM_LPM_MSG_READ_RTC      0x3
#define STM_LPM_MSG_SET_TRIM      0x4
#define STM_LPM_MSG_ENTER_PASSIVE 0x5
#define STM_LPM_MSG_SET_WDT       0x6
#define STM_LPM_MSG_SET_RTC       0x7
#define STM_LPM_MSG_SET_FP        0x8
#define STM_LPM_MSG_SET_TIMER     0x9
#define STM_LPM_MSG_GET_STATUS    0xA
#define STM_LPM_MSG_GEN_RESET     0xB
#define STM_LPM_MSG_SET_WUD       0xC
#define STM_LPM_MSG_GET_WUD       0xD
#define STM_LPM_MSG_LGWR_OFFSET   0x10
#define STM_LPM_MSG_SET_PIO   0x11
#define STM_LPM_MSG_GET_ADV_FEA 0x12
#define STM_LPM_MSG_SET_ADV_FEA 0x13
#define STM_LPM_MSG_SET_KEY_SCAN 0x14

#define STM_LPM_MSG_SET_IR        0x41
#define STM_LPM_MSG_GET_IRQ       0x42
#define STM_LPM_MSG_TRACE_DATA    0x43
#define STM_LPM_MSG_BKBD_READ     0x44
#define STM_LPM_MSG_BKBD_WRITE    0x45
#define STM_LPM_MSG_REPLY	  	  0x80
#define STM_LPM_MSG_ERR           0x82

#define STM_LPM_MAJOR_PROTO_VER      3
#define STM_LPM_MINOR_PROTO_VER      0
#define STM_LPM_MAJOR_SOFT_VER       1
#define STM_LPM_MINOR_SOFT_VER       2
#define STM_LPM_PATCH_SOFT_VER       0
#define STM_LPM_BUILD_MONTH         06
#define STM_LPM_BUILD_DAY           28
#define STM_LPM_BUILD_YEAR          11
#define STM_LPM_MAX_MSG_DATA 14

#define ADDRESS_OF_EXT_MC 0x94


#define SIZE_DATA_BYTES(n) 5+(n*4)

/*For actual H415*/
#define SBC_DATA_MEMORY_OFFSET 0x010000
#define SBC_PRG_MEMORY_OFFSET 0x018000
#define SBC_MBX_OFFSET 0xB4000
#define SBC_CONFIG_OFFSET 0xB5100
#define LPM_ERROR 0x44
/*To test on 7108 */
//#define SBC_MBX_OFFSET 0
//#define SBC_DATA_MEMORY_OFFSET 0
//#define SBC_PRG_MEMORY_OFFSET 0
//#define SBC_MBX_OFFSET 0


#define MBX_WRITE_STATUS1  (SBC_MBX_OFFSET+0x004)
#define MBX_WRITE_STATUS2  (SBC_MBX_OFFSET+0x008)
#define MBX_WRITE_STATUS3  (SBC_MBX_OFFSET+0x00C)
#define MBX_WRITE_STATUS4  (SBC_MBX_OFFSET+0x010)

#define MBX_WRITE_SET_STATUS1  (SBC_MBX_OFFSET+0x024)
#define MBX_WRITE_CLR_STATUS1  (SBC_MBX_OFFSET+0x044)

#define MBX_READ_STATUS1  (SBC_MBX_OFFSET+0x104)
#define MBX_READ_STATUS2  (SBC_MBX_OFFSET+0x108)
#define MBX_READ_STATUS3  (SBC_MBX_OFFSET+0x10C)
#define MBX_READ_STATUS4  (SBC_MBX_OFFSET+0x110)

#define MBX_READ_SET_STATUS1  (SBC_MBX_OFFSET+0x124)
#define MBX_READ_CLR_STATUS1  (SBC_MBX_OFFSET+0x144)

#define MBX_INT_ENABLE (SBC_MBX_OFFSET+0x164)
#define MBX_INT_SET_ENABLE (SBC_MBX_OFFSET+0x184)
#define MBX_INT_CLR_ENABLE (SBC_MBX_OFFSET+0x1A4)

#define SBC_REPLY_NO 0
#define SBC_REPLY_YES 0x1
#define SBC_REPLY_NO_IRQ 0x2


#define BYTE_MASK  0xFF
#define OWNER_MASK  0x3
#define BRIGHT_MASK 0xF
#define M_BIT_MASK  0x7F
#define MSG_ZERO_SIZE 0
#define MSG_ID_AUTO 0
#define MAX_SEQ_IN_MAILBOX 12

#define lpm_write8(drv, offset, value)     iowrite8(value,(drv)->lpm_mem_base + (offset))
#define lpm_write32(drv, offset, value)    iowrite32(value,(drv)->lpm_mem_base + (offset))
#define lpm_read32(drv, offset)            ioread32((drv)->lpm_mem_base + offset)

struct stm_lpm_message {
        unsigned char command_id;
        unsigned char transaction_id;
        unsigned char msg_data[STM_LPM_MAX_MSG_DATA];
};

struct stlpm_internal_send_msg {
        unsigned char command_id;
        unsigned char *msg;
        unsigned char msg_size;
        unsigned char trans_id;
        unsigned char reply_type;
};

struct stm_lpm_large_trace_data {
        unsigned char commant_id;
        unsigned char transaction_id;
        unsigned char *msg_data;
};

int lpm_exchange_msg(struct stlpm_internal_send_msg * send_msg, struct stm_lpm_message *response);
#define LPM_FILL_MSG(lpm_msg,cmd_id,msg_bug,size,id,reply)  {lpm_msg.command_id=cmd_id; \
                                                            lpm_msg.msg=msg_bug;lpm_msg.msg_size=size; \
                                                            lpm_msg.trans_id=id;\
                                                            lpm_msg.reply_type=reply; }

const char* lpm_get_cpu_type(void);
#endif /*__LPM_DEF_H*/
