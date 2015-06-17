/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   ASC_INTERNAL
 @brief
*/


#ifndef _ASC_INTERNAL_H
#define _ASC_INTERNAL_H

#ifndef CONFIG_ARCH_STI
#include <linux/stm/platform.h>
#include <linux/stm/pio.h>
#endif
#include <linux/platform_device.h>
#include <linux/clk.h>
#include "stm_asc_scr.h"
#include "asc_regs.h"
#include "ca_os_wrapper.h"
#include "ca_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASC_DEBUG  0
#define ASC_ERROR  1
#define ASC_INTERRUPT_DEBUG 0
#define ASC_REG_DUMP  1
#if ASC_ERROR
#define asc_error_print(fmt, ...)		printk(fmt,  ##__VA_ARGS__)
#else
#define asc_error_print(fmt, ...)		while(0)
#endif

#if ASC_DEBUG
#define asc_debug_print(fmt, ...)		printk(fmt,  ##__VA_ARGS__)
#else
#define asc_debug_print(fmt, ...)		while(0)
#endif

#if ASC_REG_DUMP
#define asc_regdump_print(fmt, ...)               printk(fmt,  ##__VA_ARGS__)
#else
#define asc_regdump_print(fmt, ...)               while (0)
#endif

#define DEFAULT_BAUD_RATE       9600
#define DEFAULT_GUARD_TIME      2
#define ASC_CIRC_BUFFER_SIZE    (64*1024) /* can be changed from sysfs define it under platfrom device*/
#define TX_RETRIES_COUNT           3

/* Baud rates limits */
#define ASC_SC_BAUDRATE_MIN     9600
#define ASC_TIMEOUT_VALUE          16
/* SC ATR Initial Character */
#define ASC_SC_DIRECT_TS_CHAR  0x3B
#define ASC_SC_INVERSE_TS_CHAR 0x03    /* 0x3F, Converted into inverse convention */

#define ASC_ITCountStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.ITCount++)
#define ASC_NackErrorStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.NumberNackError++)
#define ASC_NackWriteErrorStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.NumberNackWriteError++)
#define ASC_OverrunIntStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.NumberOverrunErrors++)
#define ASC_ParityIntStatistics(asc_ctrl_p)  \
	(asc_ctrl_p->statistics.NumberParityErrors++)
#define ASC_FramingIntStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.NumberFramingErrors++)
#define ASC_ByteReceivedStatistics(asc_ctrl_p) \
	(asc_ctrl_p->statistics.NumberBytesReceived++)
#define ASC_ByteTransmittedStatistics(asc_ctrl_p, NW) \
	(asc_ctrl_p->statistics.NumberBytesTransmitted += NW)

struct asc_statistics_t {
	uint32_t                 NumberBytesReceived;
	uint32_t                 NumberBytesTransmitted;
	uint32_t                 NumberOverrunErrors;
	uint32_t                 NumberFramingErrors;
	uint32_t                 NumberParityErrors;
	uint32_t                 NumberNackError;
	uint32_t                 NumberNackWriteError;
	uint32_t                 ITCount;
	uint32_t		 KStats[32];
};

typedef struct asc_params_s {
	uint8_t flowcontrol;
	uint8_t autoparityrejection;
	uint8_t nack;
	uint8_t txretries;
	uint8_t guardtime;
	stm_asc_scr_protocol_t	protocol;
} asc_params_t;

enum asc_state_e {
	ASC_STATE_IDLE = 0,
	ASC_STATE_READY,
	ASC_STATE_READ,
	ASC_STATE_WRITE,
	ASC_STATE_OP_COMPLETE,
	ASC_STATE_ABORT,
	ASC_STATE_FLUSH,
	ASC_STATE_RESERVED
};

enum asc_status_error_e {
	ASC_STATUS_NO_ERROR = 0,
	ASC_STATUS_ERROR_ABORT,
	ASC_STATUS_ERROR_PARITY,
	ASC_STATUS_ERROR_OVERRUN,
	ASC_STATUS_ERROR_FRAME,
	ASC_STATUS_ERROR_RETRIES
};

/*
 * With the struct asc_transaction more information
 * on the required transaction are moved on
 */
struct asc_transaction {
	uint8_t  *userbuffer;    /* Read/write buffer pointer */
	uint32_t size;   /* Bytes read/written */
	uint32_t remaining_size;/* Bytes remaining */
};

typedef struct asc_circbuf_s {
	uint8_t  *base;
	uint8_t  *put_p;
	uint8_t  *get_p;
	uint32_t  charsfree;
	uint32_t  bufsize;
} asc_circbuf;



struct asc_device {
	unsigned long uartclk;
	stm_asc_scr_transfer_type_t transfer_type;
	asc_params_t params;/* Serial parameters used or set */
	uint32_t hwfifosize;
	struct device *dev;/* parent device */
	struct platform_device      *pdev;
	uint8_t  __iomem *base;
	struct resource r_mem;
#ifdef CONFIG_ARCH_STI
	unsigned int irq;
#else
	struct resource r_irq;
	struct stm_pad_state  *pad_state;
#endif
};

struct asc_lpm_data_s {
	asc_regs_t asc_regs;
	uint32_t baudrate;
};
typedef struct stm_asc_s {
	void                   *handle; /*Refers to a single instance. Used for sanity check.*/
	spinlock_t              lock;   /* port lock */
	struct asc_device              device;
	ca_os_hrt_semaphore_t           flow_hrt_sema_p;
	struct asc_transaction         trns;
	asc_circbuf                    circbuf;
	enum asc_state_e               currstate;
	enum asc_status_error_e        status_error;
	struct asc_lpm_data_s lpm_data;
	struct asc_statistics_t statistics;
} asc_ctrl_t;

ca_error_code_t asc_create_new(stm_asc_scr_new_params_t  *new_params_p,
			stm_asc_h *handle_p);

ca_error_code_t  asc_abort(asc_ctrl_t *asc_ctrl_p);

void  asc_delete_intance(asc_ctrl_t *asc_ctrl_p);
void  asc_deallocate_platform_data(asc_ctrl_t *asc_ctrl_p);

ca_error_code_t asc_flush(asc_ctrl_t *asc_ctrl_p);

ca_error_code_t asc_read(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_data_params_t *params);
ca_error_code_t asc_write(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_data_params_t *params);

ca_error_code_t asc_set_compound_ctrl(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_ctrl_flags_t ctrl_flag,
			void *value_p);
ca_error_code_t asc_set_ctrl(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_ctrl_flags_t ctrl_flags,
			uint32_t value);

void asc_get_ctrl(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_ctrl_flags_t ctrl_flags,
			uint32_t *data);
void asc_get_compound_ctrl(asc_ctrl_t *asc_ctrl_p,
			stm_asc_scr_ctrl_flags_t ctrl_flag,
			void *value_p);

int asc_pad_config(asc_ctrl_t *asc_ctrl_p, bool release);

void asc_store_reg_set(asc_regs_t *asc_regs_src_p, asc_regs_t *asc_regs_des_p);

void asc_load_reg_set(asc_regs_t *asc_regs_src_p, asc_regs_t *asc_regs_des_p);

void dumpreg(asc_ctrl_t *asc_ctrl_p);

#ifdef __cplusplus
}
#endif

#endif /**/
