/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : cec_hal.c
Author :           bharatj

API definitions for CEC device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#include "cec_hal.h"
#include "cec_core.h"
#include "infra_platform.h"
#include "cec_debug.h"
#include <linux/stm/sysconf.h>

#define CEC_HAL_USE_BUS_FREE_TIME_INTR

static void
cec_handle_free_time_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static void
cec_hal_handle_tx_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static void
cec_hal_handle_rx_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static void
cec_hal_handle_arbitration(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static inline void
cec_hal_enbale_free_time_intr(stm_cec_t *cec_p,
				uint8_t free_time);

static inline void
cec_hal_handle_tx_ack(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static inline void
cec_hal_handle_tout_error(stm_cec_t  *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static inline void
cec_hal_handle_pulse_error(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static inline void
cec_hal_handle_rx_eom_status(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

static inline void
hal_handle_rx_done(stm_cec_t  *cec_p,
		struct cec_hal_isr_param_s *isr_param_p);

static inline void
hal_handle_tx_error(stm_cec_t  *cec_p,
		struct cec_hal_isr_param_s *isr_param_p);

static inline void hal_tx_error_do_retry(stm_cec_t  *cec_p);

static inline void
hal_tx_error_retry_expired(stm_cec_t  *cec_p,
			struct cec_hal_isr_param_s *isr_param_p);

/*#ifndef CEC_HAL_USE_BUS_FREE_TIME_INTR*/
static inline void
cec_hal_start_free_time_count(stm_cec_t *cec_p,
			uint8_t free_time);
/*#endif*/
static void cec_hal_enbale_intr(stm_cec_t *cec_p);

#define __handle_tx_intr_ping(cec_p, ack_stat, tx_done)	\
do { \
	tx_done = !(ack_status); \
} while (0)

#define __handle_tx_intr_broadcast(cec_p, ack_stat, tx_done) \
do { \
	/*if auto ack is true then h/w manages brodcast messages spec rule
	 * hence ack status is only true if no follower acked it.*/ \
	if (cec_p->auto_ack_for_broadcast_tx) \
		tx_done = ack_status; \
	else \
		tx_done = !(ack_status); \
} while (0)

#define __handle_tx_intr_direct(cec_p, ack_stat, tx_done) \
do { \
	tx_done = ack_stat; \
} while (0);



void cec_hal_init(stm_cec_t *cec_p)
{
	uint16_t clk_div;

	struct cec_hw_data_s *hw_data_p;
	struct stm_cec_platform_data *platform_data_p;
#ifndef CONFIG_OF
	uint32_t						val;
	unsigned char __iomem			*mapped_config_address;
#endif

	hw_data_p = cec_p->cec_hw_data_p ;
	platform_data_p =
		(struct stm_cec_platform_data *)hw_data_p->platform_data;
	

	/* non DT implementation is kept as it is  : This should change
	 * to sysconfig in non DT also */
#ifndef CONFIG_OF
	cec_debug_trace(CEC_HW,
			"platform_data_p->config_data[0].address:0x%x\n",
			platform_data_p->config_data[0].address);
	mapped_config_address = ioremap_nocache(platform_data_p->config_data[0].address, 32);
	cec_debug_trace(CEC_HW,"mappedAddr:%p\n", mapped_config_address);
	//spin_lock_irqsave(&(cec_p->intr_lock),flags);
	/*UNRESET LPM [SBC : SYSTEM_CONFIG15]*/
	val = INFRA_READ_REG_WORD(mapped_config_address);
	if((val & platform_data_p->config_data[0].mask) == 0){
		//val &= 0xFFFFFFFB;
		val |= platform_data_p->config_data[0].mask;
		INFRA_SET_REGBITS_WORD(mapped_config_address, val);
	}
#else
	sysconf_write(platform_data_p->sysconf, 1);
#endif
	cec_debug_trace(CEC_HW, "cec_p->clk_freq:%d\n", cec_p->clk_freq);
	/*CEC Clock Divider is programmed to generate
	 * 0.1 msec reference clock*/
	clk_div = (uint16_t)((cec_p->clk_freq) / 10000);

	INFRA_WRITE_REG_WORD((cec_p->base_addr_v + CEC_CLK_DIV), clk_div);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2),
			(CEC_AUTO_BUS_ERR_EN | CEC_STOP_ON_ARB_ERR_EN));
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2),
			(CEC_TX_REQ_WAIT_EN | (CEC_NEW_INIT_SFT << 4)));
	INFRA_SET_REGBITS_WORD((cec_p->base_addr_v+CEC_BIT_TOUT_THRESH),
			CEC_SBIT_TOUT_47MS | (CEC_DBIT_TOUT_28MS << 4));
	/*  2.75 according to spec */
	INFRA_SET_REGBITS_WORD((cec_p->base_addr_v+CEC_BIT_PULSE_THRESH),
			CEC_BIT_LPULSE_03MS | CEC_BIT_HPULSE_03MS);

	/*CEC Bit period threshold*/
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_TX_CTRL),
				CEC_BIT5 | CEC_BIT7);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_DATA_ARRAY_CTRL),
				CEC_TX_ARRAY_EN |
				CEC_RX_ARRAY_EN |
				CEC_TX_STOP_ON_NACK);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_TX_ARRAY_CTRL),
			CEC_TX_AUTO_SOM_EN | CEC_TX_AUTO_EOM_EN);
	/*Enable CEC*/
	/*??powwr save mode not enabled??*/
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
			CEC_IN_FILTER_EN /*| CEC_PWR_SAVE_EN*/);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
			CEC_EN | CEC_RX_RESET_EN);
	/* unregistered */
	cec_hal_set_logical_addr(cec_p, CEC_BROADCAST_LOG_ADDR);
	cec_hal_enbale_intr(cec_p);

#ifndef CONFIG_OF
	iounmap(mapped_config_address);
#endif

	return;
}





void cec_hal_set_logical_addr(stm_cec_t *cec_p, uint8_t	log_addr)
{
	uint8_t	set_log_addr;

	set_log_addr =
	(log_addr == CEC_BROADCAST_LOG_ADDR) ? 0x0 : cec_p->logical_addr_set;
	INFRA_WRITE_REG_WORD((cec_p->base_addr_v + CEC_ADDR_TABLE),
				set_log_addr);
}

bool cec_hal_ping(stm_cec_h  device, uint8_t  dest_addr)
{
	uint8_t	bus_free_time;
	stm_cec_t *cec_p = (stm_cec_t *)device;

	INFRA_WRITE_REG_BYTE((cec_p->base_addr_v + CEC_TX_DATA_BASE),
	dest_addr);

	bus_free_time = (cec_p->prev_logical_addr == cec_p->cur_logical_addr) ?
						(CEC_PRESENT_INIT_SFT << 4) :
						(CEC_NEW_INIT_SFT << 4);
	INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2), 0xF0);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2),
							bus_free_time);

	cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED] = true;
	SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY);
	CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_FREE);
	CLR_CEC_FRAME_STATUS((cec_p->frame_status), CEC_HAL_FRAME_TX_ERROR |
						CEC_HAL_FRAME_TX_ACK |
						CEC_HAL_FRAME_TX_DONE |
						CEC_HAL_FRAME_TX_ARBITER);
	INFRA_WRITE_REG_BYTE((cec_p->base_addr_v + CEC_CTRL2),
						CEC_TX_AUTO_SOM_EN |
						CEC_TX_AUTO_EOM_EN |
						CEC_TX_START | 0x1);

	return true;
}


static void
cec_handle_free_time_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	uint8_t		isr_status = isr_param_p->isr_status;
	uint8_t		time_irq_enable;
	uint8_t		signal = false;

	time_irq_enable =
		(INFRA_READ_REG_BYTE(cec_p->base_addr_v + CEC_IRQ_CTRL)) &
		CEC_FREE_TIME_IRQ_EN ;
	if (!((isr_status & CEC_FREE_TIME_IRQ_STS) && time_irq_enable))
		return;

	/* CEC free time IRQ*/
	/* For exclude CEC_FREE_TIME_IRQ_STS bit set*/
	INFRA_WRITE_REG_BYTE((cec_p->base_addr_v+CEC_FREE_TIME_THRESH), 0x0);
	INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_FREE_TIME_IRQ_EN);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_RX_SOM_IRQ_EN);

	if (GET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_INACTIVE)) {
		SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_FREE);
		INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
					CEC_ACK_CTRL);
		CLR_CEC_BUS_STATUS(cec_p->bus_status,
				CEC_HAL_BUS_INACTIVE | CEC_HAL_BUS_RX_BUSY);
		signal = true;
		/*We reached here, so either TX is done or retry has expired
		  so pls signal the waiting user thread.*/
		if (cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED]  == true)
			infra_os_sema_signal(cec_p->tx_done_sema_p);

	} else {
		/* Start retransmission*/
		INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_TX_ARRAY_CTRL),
					CEC_TX_START);
	}

	return;
}


irqreturn_t cec_irq_isr(int irq, void *dev)
{
	uint16_t status = 0;
	stm_cec_t *cec_p = (stm_cec_t *)dev;
	struct cec_hal_isr_param_s *isr_param_p;

	status = INFRA_READ_REG_WORD(cec_p->base_addr_v + CEC_STATUS);
	cec_debug_trace(CEC_HW, "ISR status:0x%x\n", status);
	spin_lock(&cec_p->intr_lock);
	/*Lock the ISR Hook Up Functions*/
	isr_param_p = (struct cec_hal_isr_param_s *) &cec_p->isr_param;
	/*get the CEC Address*/
	status = INFRA_READ_REG_WORD(cec_p->base_addr_v + CEC_STATUS);
	isr_param_p->isr_status = status;

	/*Check for CEC TX Frame Status*/
	if (status & (CEC_TX_DONE_STS|
			CEC_TX_ACK_GET_STS|
			CEC_TX_ERROR|
			CEC_TX_ARB_ERROR)) {
		cec_hal_handle_tx_intr(cec_p, isr_param_p);
	}

	/*Check for CEC Frame Rx Status*/
	if (status & (CEC_RX_DONE_STS|
			CEC_RX_SOM_STS|
			CEC_RX_EOM_STS|
			CEC_RX_ERROR_MIN|
			CEC_RX_ERROR_MAX)) {
		cec_hal_handle_rx_intr(cec_p, isr_param_p);
	}

	/*Check CEC Free Time*/
	if (status & CEC_FREE_TIME_IRQ_STS)
		cec_handle_free_time_intr(cec_p, isr_param_p);


	/* Clear CEC Interrupts*/
	INFRA_WRITE_REG_WORD((cec_p->base_addr_v+CEC_STATUS), status);

	spin_unlock(&cec_p->intr_lock);
	return IRQ_HANDLED;
}

#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
static inline void
cec_hal_enbale_free_time_intr(stm_cec_t *cec_p,
				uint8_t free_time)
{
	INFRA_WRITE_REG_BYTE((cec_p->base_addr_v+CEC_FREE_TIME_THRESH),
				free_time);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
				CEC_FREE_TIME_IRQ_EN);
}
#endif

static inline void
cec_hal_start_free_time_count(stm_cec_t *cec_p,
				uint8_t free_time)
{
	INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2), 0xF0);
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL2),
				((free_time << 4)|0x8));
}

static void
cec_hal_handle_arbitration(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY);
	SET_CEC_FRAME_STATUS(cec_p->frame_status,
			CEC_HAL_FRAME_TX_ARBITER | CEC_HAL_FRAME_TX_DONE);

	/*set to invalid address because we can't read address of
	 * new initiator during loss of arbitration*/
	cec_p->prev_logical_addr = CEC_BROADCAST_LOG_ADDR + 1;
	if (cec_p->trials < cec_p->retries) {
		/*Unsuccessful Tx, try again*/
		hal_tx_error_do_retry(cec_p);
	} else {
		/*Unsuccessful Tx, stop Tx*/
		hal_tx_error_retry_expired(cec_p, isr_param_p);
		/*Forcefully set it to erroneous condition
		In some case TX error is not set while
		getting arbitration error
		unset the NOT_SENT error also*/
		isr_param_p->notify &= ~CEC_HAL_EVT_TX_NOT_DONE;
		isr_param_p->notify |= CEC_HAL_EVT_MSG_ERROR;
	}
	isr_param_p->isr_status  &= ~CEC_FREE_TIME_IRQ_STS;

	return ;
}

static inline void
cec_hal_handle_tx_ack(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	/* byte was sent and recived by RX*/
	CLR_CEC_BUS_STATUS((cec_p->bus_status), CEC_HAL_BUS_TX_BUSY);
	SET_CEC_FRAME_STATUS((cec_p->frame_status),
			CEC_HAL_FRAME_TX_ACK | CEC_HAL_FRAME_TX_DONE);
	SET_CEC_BUS_STATUS((cec_p->bus_status), CEC_HAL_BUS_INACTIVE);
	isr_param_p->isr_status &= ~CEC_FREE_TIME_IRQ_STS;
	if (cec_p->retries) {
#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
		cec_hal_enbale_free_time_intr(cec_p, CEC_NEW_INIT_SFT);
#endif
		isr_param_p->notify |= CEC_HAL_EVT_TX_DONE;
	INFRA_CLR_REGBITS_BYTE(
			(cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_TX_DONE_IRQ_EN);

	}
	cec_p->status_arr[CEC_STATUS_TX_IN_PROCESS] = false;
}

static inline void hal_tx_error_do_retry(stm_cec_t  *cec_p)
{
	/*Unsuccessful Tx, try again*/
	cec_p->trials++;
#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
	cec_hal_enbale_free_time_intr(cec_p, CEC_RETRANSMIT_SFT);
#else
	cec_hal_start_free_time_count(cec_p, CEC_RETRANSMIT_SFT);
#endif
}

static inline void
hal_tx_error_retry_expired(stm_cec_t  *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	/*Unsuccessful Tx, stop Tx*/
	CLR_CEC_BUS_STATUS((cec_p->bus_status), CEC_HAL_BUS_TX_BUSY);
	if (isr_param_p->isr_status & CEC_TX_ERROR) {
		SET_CEC_FRAME_STATUS((cec_p->frame_status),
		CEC_HAL_FRAME_TX_ERROR);
		isr_param_p->notify |= CEC_HAL_EVT_MSG_ERROR;
	} else {
		isr_param_p->notify |= CEC_HAL_EVT_TX_NOT_DONE;
	}
	INFRA_CLR_REGBITS_BYTE(
			(cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_TX_DONE_IRQ_EN);

	SET_CEC_FRAME_STATUS((cec_p->frame_status), CEC_HAL_FRAME_TX_DONE);
	SET_CEC_BUS_STATUS((cec_p->bus_status), CEC_HAL_BUS_INACTIVE);
#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
	cec_hal_enbale_free_time_intr(cec_p, CEC_PRESENT_INIT_SFT);
#endif
	cec_p->status_arr[CEC_STATUS_TX_IN_PROCESS] = false;
}

static inline void
hal_handle_tx_error(stm_cec_t  *cec_p,
		struct cec_hal_isr_param_s *isr_param_p)
{
	/* Follower did not recives this byte...*/
	if (isr_param_p->isr_status & CEC_TX_ERROR) {
		CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY);
		SET_CEC_FRAME_STATUS(cec_p->frame_status,
						CEC_HAL_FRAME_TX_DONE);
		/*set to invalid address because we can't read addres
		 * of new initiator during loss of arbitration*/
		cec_p->prev_logical_addr = CEC_BROADCAST_LOG_ADDR + 1;
	}

	if (cec_p->trials < cec_p->retries)
		hal_tx_error_do_retry(cec_p);
	else
		hal_tx_error_retry_expired(cec_p, isr_param_p);

	isr_param_p->isr_status &= ~CEC_FREE_TIME_IRQ_STS;
}


static void
cec_hal_handle_tx_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	uint16_t	status = isr_param_p->isr_status;
	uint8_t		ack_status = false;
	uint8_t		tx_done = false;

	/* Cec Arbitration Error*/
	if (isr_param_p->isr_status & CEC_TX_ARB_ERROR)
		cec_hal_handle_arbitration(cec_p, isr_param_p);


	if (!(status & CEC_TX_DONE_STS)) {
		/*this is not aTX intr*/
		return;
	}

	/*Check if right now there was an error in the bus*/
	if ((GET_CEC_BUS_STATUS(cec_p->bus_status,
				CEC_HAL_BUS_ERROR) == true))
		return;

	ack_status = status&CEC_TX_ACK_GET_STS;

	if (cec_p->status_arr[CEC_STATUS_MSG_PING])
		__handle_tx_intr_ping(cec_p, ack_status, tx_done);
	else if (cec_p->status_arr[CEC_STATUS_MSG_BROADCAST])
		__handle_tx_intr_broadcast(cec_p, ack_status, tx_done);
	else
		__handle_tx_intr_direct(cec_p, ack_status, tx_done);

	if (tx_done && !(status & CEC_TX_ERROR))
		cec_hal_handle_tx_ack(cec_p, isr_param_p);
		/* byte was sent and recived by RX*/
	else
		hal_handle_tx_error(cec_p, isr_param_p);

}

static inline void
cec_hal_handle_pulse_error(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	/* For CEC hw NACK*/
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_CTRL), CEC_ACK_CTRL);
	SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_INACTIVE);
	/*New initiator wants to send a frame*/
	isr_param_p->isr_status &= ~CEC_FREE_TIME_IRQ_STS;

#ifdef CEC_FREE_TIME_HW
	/* Clear Readable Format*/
#else
	cec_hal_enbale_free_time_intr(cec_p, CEC_NEW_INIT_SFT);
#endif
}

static inline void
hal_handle_rx_done(stm_cec_t  *cec_p,
		struct cec_hal_isr_param_s *isr_param_p)
{
	if (!(isr_param_p->msg_status & CEC_RX_BEGIN) &&
			(isr_param_p->msg_status
			 & CEC_RX_SOM_STS)) {
		/*Need to check...CEC_DATA is
		 * invalid for Array Transmission*/
		isr_param_p->msg_status |= CEC_RX_BEGIN;

		/* Add the Flow Control by sending the NACK incase
		 * CEC Rx is not ready to accept the incoming msg*/
		if ((GET_CEC_BUS_STATUS(cec_p->bus_status,
					CEC_HAL_BUS_RX_NACK)) ||
			(GET_CEC_FRAME_STATUS(cec_p->frame_status,
						CEC_HAL_FRAME_RX_FULL))) {
			INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
								CEC_ACK_CTRL);
		} else {
			INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
								CEC_ACK_CTRL);
		}
	}
}



static inline void
cec_hal_handle_tout_error(stm_cec_t  *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	cec_debug_trace(CEC_HW, "\n");
	CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_RX_BUSY);
	SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_INACTIVE);

	/*New initiator wants to send a frame*/
	isr_param_p->isr_status  &= ~CEC_FREE_TIME_IRQ_STS;
	/*enable the free time interrupt*/

#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
	cec_hal_enbale_free_time_intr(cec_p, CEC_NEW_INIT_SFT);
#endif
	if (!(isr_param_p->isr_status & CEC_TX_DONE_STS) &&
		(isr_param_p->isr_status & CEC_RX_ERROR_MIN)) {
		isr_param_p->notify |= CEC_HAL_EVT_RX_NOT_DONE;
	}

	if (isr_param_p->msg_status & CEC_RX_BEGIN) {
		isr_param_p->notify |= CEC_HAL_EVT_RX_NOT_DONE;
		isr_param_p->msg_status &= ~(CEC_RX_SOM_STS | CEC_RX_BEGIN);
		/*Stop receiving*/
		isr_param_p->exit = true;
	}

	if (isr_param_p->notify & CEC_HAL_EVT_RX_NOT_DONE)
		queue_work(cec_p->w_q, &(cec_p->cec_isr_work));


	return;
}


static inline void
cec_hal_handle_rx_eom_status(stm_cec_t *cec_p,
				struct cec_hal_isr_param_s *isr_param_p)
{
	uint8_t		rx_size;
	uint8_t		idx;
	uint8_t		write_buffer_idx;
	stm_cec_msg_t			*msg_buf_p;

	rx_size = INFRA_READ_REG_BYTE((cec_p->base_addr_v +
				CEC_DATA_ARRAY_STATUS)) & 0x1F;
	cec_debug_trace(CEC_HW, "rx_size:%d\n", rx_size);

	if ((cec_p->msg_list_tail - cec_p->msg_list_head) >
							CEC_MSG_LIST_SIZE) {
		SET_CEC_FRAME_STATUS(cec_p->frame_status,
							CEC_HAL_FRAME_RX_FULL);
		INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_CTRL),
							CEC_ACK_CTRL);
	}

	if (rx_size > 1 && !(GET_CEC_FRAME_STATUS(cec_p->frame_status,
					CEC_HAL_FRAME_RX_FULL))) {
		write_buffer_idx = (cec_p->msg_list_tail%CEC_MSG_LIST_SIZE);
		cec_debug_trace(CEC_HW, "tail:%d head:%d write_idx:%d\n",
						cec_p->msg_list_tail,
						cec_p->msg_list_head,
						write_buffer_idx);
		cec_p->msg_list_tail++;
		msg_buf_p = &(cec_p->message_list[write_buffer_idx]);
		msg_buf_p->msg_len = rx_size;
		for (idx = 0; idx < rx_size; idx++) {
			msg_buf_p->msg[idx] =
				INFRA_READ_REG_BYTE((cec_p->base_addr_v +
							CEC_RX_DATA_BASE+idx));
		}
		SET_CEC_FRAME_STATUS(cec_p->frame_status,
				CEC_HAL_FRAME_RX_DONE);
		isr_param_p->notify |= CEC_HAL_EVT_RX_DONE;
	}

	CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_RX_BUSY);
	SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_INACTIVE);

	if (isr_param_p->notify == CEC_HAL_EVT_RX_DONE)
		queue_work(cec_p->w_q, &(cec_p->cec_isr_work));

	/*New initiator wants to send a frame*/
	isr_param_p->isr_status &= ~CEC_FREE_TIME_IRQ_STS;

#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
	cec_hal_enbale_free_time_intr(cec_p, CEC_NEW_INIT_SFT);
#endif
	isr_param_p->msg_status &= ~CEC_RX_SOM_STS;
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_RX_SOM_IRQ_EN); /*Need to check*/

}


static void
cec_hal_handle_rx_intr(stm_cec_t *cec_p,
			struct cec_hal_isr_param_s *isr_param_p)
{
	/*Frame receiver buffer is full or not*/
	if (GET_CEC_FRAME_STATUS(cec_p->frame_status,
				CEC_HAL_FRAME_RX_FULL)) {
		/*No space to copy data*/
		cec_debug_trace(CEC_HW, "No space to copy data\n");
		return;
	}

	/* Time Out error*/
	if (isr_param_p->isr_status & (CEC_SBIT_TOUT_STS |
					CEC_DBIT_TOUT_STS)) {
		INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_CTRL),
					 /* For CEC hw NACK*/
					CEC_ACK_CTRL | CEC_RX_RESET_EN);
		cec_debug_trace(CEC_HW,
			"Either CEC_SBIT_TOUT_STS or CEC_DBIT_TOUT_STS\n");
	}

	if (isr_param_p->isr_status & (CEC_SBIT_TOUT_STS |
					CEC_DBIT_TOUT_STS |
					CEC_RX_ERROR_MIN |
					CEC_RX_ERROR_MAX)) {
		cec_hal_handle_tout_error(cec_p, isr_param_p);
		if (isr_param_p->exit)
			return;
	}

	if (isr_param_p->isr_status & CEC_RX_SOM_STS) {
		isr_param_p->msg_status = CEC_RX_SOM_STS;
		/* For case if address isn't our address*/
		cec_debug_trace(CEC_HW, "\n");
		SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_RX_BUSY);
		INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
					CEC_RX_SOM_IRQ_EN); /*Need to check*/
	}

	if (isr_param_p->isr_status & CEC_RX_DONE_STS)
		hal_handle_rx_done(cec_p, isr_param_p);
	/*End of RX_DONE_STATUS*/

	if ((isr_param_p->isr_status & CEC_RX_EOM_STS))
		cec_hal_handle_rx_eom_status(cec_p, isr_param_p);
	/*End of EOM*/
}



infra_error_code_t cec_hal_send_msg(stm_cec_h  device, stm_cec_msg_t *msg_p)
{
	uint8_t		idx;
	uint8_t		free_time, time;
	stm_cec_t *cec_p = (stm_cec_t *)device;

#ifdef CEC_HAL_USE_BUS_FREE_TIME_INTR
	if (!GET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_FREE)) {
		/*Rx is in progress*/
		cec_debug_trace(CEC_HW, "Bus is busy for Rx\n");
		return -EBUSY;
	}
#endif

	cec_p->status_arr[CEC_STATUS_TX_IN_PROCESS] = true;

	/*Copy message body to registers*/
	for (idx = 0 ; idx < msg_p->msg_len ; idx++) {
		INFRA_WRITE_REG_BYTE(
			(cec_p->base_addr_v + CEC_TX_DATA_BASE + idx),
			*((msg_p->msg) + idx));
	}

	free_time = (cec_p->cur_logical_addr == cec_p->prev_logical_addr)
			? CEC_PRESENT_INIT_SFT : CEC_NEW_INIT_SFT;
	SET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY);
	CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_FREE);
	CLR_CEC_FRAME_STATUS(cec_p->frame_status,
					CEC_HAL_FRAME_TX_ERROR |
					CEC_HAL_FRAME_TX_ACK |
					CEC_HAL_FRAME_TX_DONE |
					CEC_HAL_FRAME_TX_ARBITER);

	time = free_time -
		(INFRA_READ_REG_BYTE((cec_p->base_addr_v + CEC_EXT_STATUS))
					& 0xF);
	while ((time > 0) && (time < 0xF)) {
		msleep(3);
		time = free_time - (INFRA_READ_REG_BYTE(
					(cec_p->base_addr_v + CEC_EXT_STATUS))
					& 0xF);
	}

	INFRA_WRITE_REG_BYTE((cec_p->base_addr_v+CEC_TX_ARRAY_CTRL),
					CEC_TX_AUTO_SOM_EN |
					CEC_TX_AUTO_EOM_EN |
					CEC_TX_START |
					(msg_p->msg_len & CEC_TX_N_OF_BYTES));
	INFRA_SET_REGBITS_BYTE(
			(cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_TX_DONE_IRQ_EN);

	cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED] = true;

	return 0;
}


void cec_hal_abort_tx(stm_cec_h  device)
{
	u_long		flags;

	stm_cec_t *cec_p = (stm_cec_t *)device;
	struct cec_hal_isr_param_s	*isr_param_p;

	isr_param_p = &(cec_p->isr_param);
	isr_param_p->notify |= CEC_HAL_EVT_TX_CANCELLED;
	spin_lock_irqsave(&cec_p->intr_lock, flags);

	INFRA_SET_REGBITS_WORD((cec_p->base_addr_v+CEC_DATA_ARRAY_CTRL),
			CEC_TX_ARRAY_RESET);
	INFRA_WRITE_REG_WORD((cec_p->base_addr_v+CEC_TX_ARRAY_CTRL), 0x0);
	INFRA_CLR_REGBITS_BYTE(
			(cec_p->base_addr_v+CEC_IRQ_CTRL),
			CEC_TX_DONE_IRQ_EN);

	spin_unlock_irqrestore(&cec_p->intr_lock, flags);

	infra_os_sema_signal(cec_p->tx_done_sema_p);

}

static void cec_hal_enbale_intr(stm_cec_t *cec_p)
{
	uint16_t	reg_data;
	/*Clear any pending interrupt status register*/
	INFRA_CLR_REGBITS_WORD((cec_p->base_addr_v+CEC_STATUS), 0xffff);

	/*Enable the interrupt*/
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
						(CEC_RX_DONE_IRQ_EN
						| CEC_RX_SOM_IRQ_EN
						| CEC_RX_EOM_IRQ_EN
						| CEC_ERROR_IRQ_EN));

	reg_data = INFRA_READ_REG_WORD((cec_p->base_addr_v + CEC_IRQ_CTRL));
}

void cec_hal_suspend(stm_cec_h  device, bool may_wakeup)
{
	stm_cec_t *cec_p = (stm_cec_t *)device;
	struct cec_hal_isr_param_s	*isr_param_p;

	isr_param_p = &(cec_p->isr_param);
	cec_debug_trace(CEC_HW, "CEC going into suspend\n");
	/*Disable the interrupts*/
	INFRA_CLR_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
			(CEC_TX_DONE_IRQ_EN
			 | CEC_RX_DONE_IRQ_EN
			 | CEC_RX_SOM_IRQ_EN
			 | CEC_RX_EOM_IRQ_EN
			 | CEC_ERROR_IRQ_EN));

	if (cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED]  == true) {
		INFRA_SET_REGBITS_WORD(
				(cec_p->base_addr_v+CEC_DATA_ARRAY_CTRL),
				CEC_TX_ARRAY_RESET);
		INFRA_WRITE_REG_WORD((cec_p->base_addr_v+CEC_TX_ARRAY_CTRL),
				0x0);
		isr_param_p->notify |= CEC_HAL_EVT_TX_CANCELLED;
	}
	if (may_wakeup) {
		/*Enable only the RX interupt to wakeup*/
		cec_debug_trace(CEC_HW,
				"Enabling RX intr as user set CEC Wakeup:%d\n",
				may_wakeup);
		INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v + CEC_IRQ_CTRL),
				(CEC_RX_SOM_IRQ_EN));
	}
}

int cec_hal_resume(stm_cec_h  device)
{
	stm_cec_t *cec_p = (stm_cec_t *)device;
	cec_debug_trace(CEC_HW, "Resuming CEC\n");

	/*Check if free interrupt has not been triggered and TX is loaded*/
	if (!(GET_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_FREE)) &&
	cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED] == true) {
		/*Release the user*/
		infra_os_sema_signal(cec_p->tx_done_sema_p);

	}

	/*Check if a TX was in progress during Suspend
	 * If yes, then set the Xt related flags to erroneous conditions.
	 * The logic is similar to the TX retry expired*/
	if (cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED]  == true) {
		CLR_CEC_BUS_STATUS(cec_p->bus_status, CEC_HAL_BUS_TX_BUSY);
		SET_CEC_FRAME_STATUS(cec_p->frame_status,
				CEC_HAL_FRAME_TX_DONE);
		/*set to invalid address because we can't read addres of new
		 * initiator during loss of arbitration*/
		cec_p->prev_logical_addr = CEC_BROADCAST_LOG_ADDR + 1;
		SET_CEC_FRAME_STATUS((cec_p->frame_status),
				CEC_HAL_FRAME_TX_ERROR);
		SET_CEC_FRAME_STATUS((cec_p->frame_status),
				CEC_HAL_FRAME_TX_DONE);
		SET_CEC_BUS_STATUS((cec_p->bus_status), CEC_HAL_BUS_INACTIVE);
		cec_p->status_arr[CEC_STATUS_TX_IN_PROCESS] = false;
		/*Unset the loading flag.
		 * This is avoid duplicate signalling from free time intr*/
		cec_p->status_arr[CEC_STATUS_TX_MSG_LOADED]  = false;
	}
	/*Enable the interrupt*/
	INFRA_SET_REGBITS_BYTE((cec_p->base_addr_v+CEC_IRQ_CTRL),
			(CEC_RX_DONE_IRQ_EN
			 | CEC_RX_SOM_IRQ_EN
			 | CEC_RX_EOM_IRQ_EN
			 | CEC_ERROR_IRQ_EN));
	return 0;
}
