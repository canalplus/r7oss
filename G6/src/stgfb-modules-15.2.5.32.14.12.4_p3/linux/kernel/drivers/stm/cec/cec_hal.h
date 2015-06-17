/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cec/cec_hal.h
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _CEC_HAL_H_
#define _CEC_HAL_H_

#include <linux/irq.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include "stm_cec.h"
#include "cec_reg.h"

/* Private Macros ------------------------------------------------------------------------ */
/*Rx/Tx buffer sizes*/
#define CEC_MAX_MESSAGE_SIZE		(1+STM_CEC_MAX_MSG_LEN) /*  Header block plus data*/
#define CEC_BUFFER_SIZE			CEC_MAX_MESSAGE_SIZE  /*  CEC signle Frame buffer size*/
#define CEC_RX_BUFFER_SIZE		40 /*  Rx Circular buffer size*/

/*Initiator and destination masks in header block*/
#define CEC_HEADER_INIT_MASK		0xF0
#define CEC_HEADER_DEST_MASK		0x0F

/* CEC BroadCast Error*/
#define CEC_BROADCAST_LOG_ADDR	15   /* Broadcast message destination logical address*/
#define CEC_SPECIFIC_USE_LOG_ADDR	14
#define CEC_UNREGISTRED_LOG_ADDR	15

/* Signal free time in bit periods (2.4ms)*/
#define CEC_PRESENT_INIT_SFT		7               /*Present initiator wants to send another frame immediately after its previous frame*/
#define CEC_NEW_INIT_SFT		5               /*New initiator wants to send a frame*/
#define CEC_RETRANSMIT_SFT		3               /*Previous attempt to send frame unsuccessful*/


/* Constants    ---------------------------------------------------------- */
#define     CEC_SBIT_TOUT_47MS	    0x02
#define     CEC_SBIT_TOUT_48MS	    0x03
#define     CEC_SBIT_TOUT_50MS	    0x04

#define     CEC_DBIT_TOUT_27MS	    0x01
#define     CEC_DBIT_TOUT_28MS	    0x02
#define     CEC_DBIT_TOUT_29MS	    0x03

#define     CEC_BIT_LPULSE_03MS	    0x02
#define     CEC_BIT_HPULSE_03MS	    0x08

#define     CEC_RX_BEGIN            CEC_BIT0

#define     GET_CEC_FRAME_STATUS(status_mask, status)			((status_mask & status)==status) ?true:false
#define     SET_CEC_FRAME_STATUS(status_mask, status)			status_mask|=(status)
#define     CLR_CEC_FRAME_STATUS(status_mask, status)			status_mask&= ~(status)

#define     GET_CEC_BUS_STATUS(status_mask, status)			((status_mask & status)==status) ?true:false
#define     SET_CEC_BUS_STATUS(status_mask, status)			status_mask|=(status)
#define     CLR_CEC_BUS_STATUS(status_mask, status)			status_mask&= ~(status)

/* Private Types ---------------------------------------------------------- --------------*/
typedef enum{
	CEC_HAL_EVT_RX_DONE = CEC_BIT0,
	CEC_HAL_EVT_TX_DONE = CEC_BIT1,
	CEC_HAL_EVT_TX_NOT_DONE = CEC_BIT2,
	CEC_HAL_EVT_RX_NOT_DONE = CEC_BIT3,
	CEC_HAL_EVT_TX_CANCELLED = CEC_BIT4,
	CEC_HAL_EVT_MSG_ERROR = CEC_BIT5
}cec_hal_evt_status_t;

typedef enum{
    CEC_HAL_FRAME_RX_DONE       =    CEC_BIT0,   /* CEC Rx buffer contains at least one frame that is ready for reading*/
    CEC_HAL_FRAME_RX_FULL       =    CEC_BIT1,   /* CEC Rx buffer is full and receiving is stopped.*/
    CEC_HAL_FRAME_TX_BUSY       =    CEC_BIT2,   /* CEC line was busy at the moment Tx driver wants to start transmission*/
    CEC_HAL_FRAME_TX_ERROR      =    CEC_BIT3,   /* The last CEC frame transmission was unsuccessful*/
    CEC_HAL_FRAME_TX_ARBITER    =    CEC_BIT4,   /* another device capture the line during transmission of initiator address*/
    CEC_HAL_FRAME_TX_ACK        =    CEC_BIT5,   /* The last CEC frame transmission has been acknowledged*/
    CEC_HAL_FRAME_TX_DONE       =    CEC_BIT6    /* The last CEC frame transmission has been completed*/
} cec_hal_frame_status_t;

typedef enum
{
    CEC_HAL_BUS_FREE            =    CEC_BIT0,   /* CEC bus is free and ready for use*/
    CEC_HAL_BUS_RX_BUSY         =    CEC_BIT1,   /* CEC frame is being received*/
    CEC_HAL_BUS_TX_BUSY         =    CEC_BIT2,   /* CEC frame is being transmitted*/
    CEC_HAL_BUS_RX_NACK         =    CEC_BIT3,   /* Respond with NACK for directly addressed message and ACK for broadcast*/
    CEC_HAL_BUS_ERROR           =    CEC_BIT4,   /* CEC Line error*/
    CEC_HAL_BUS_INACTIVE        =    CEC_BIT5,   /* CEC bus is inactive during Free Time*/
}cec_hal_bus_status_t;


typedef enum
{
   CEC_HAL_TX_STATUS_SUCCESS,
   CEC_HAL_TX_STATUS_FAIL_BUSY,
   CEC_HAL_TX_STATUS_FAIL_NACK
}cec_hal_msg_status;

void cec_hal_init(stm_cec_h cec_dev);
void cec_hal_term(stm_cec_h cec_dev);
irqreturn_t cec_irq_isr(int irq, void *dev);
bool cec_hal_ping(stm_cec_h  device, uint8_t  dest_addr);
int cec_hal_send_msg(stm_cec_h  device, stm_cec_msg_t *msg_p);
void cec_hal_abort_tx(stm_cec_h  device);
void cec_hal_set_logical_addr(stm_cec_h cec_dev, uint16_t log_addr);
void cec_hal_suspend(stm_cec_h  device, bool may_wakeup);
int cec_hal_resume(stm_cec_h  device);
#endif
