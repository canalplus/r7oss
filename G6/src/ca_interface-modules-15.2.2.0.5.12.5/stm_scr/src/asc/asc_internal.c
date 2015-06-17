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
@File asc_scr_generic.h
@brief

*/
#include "asc_internal.h"
#include "asc_hal.h"
#ifdef CONFIG_ARCH_STI
#include <linux/gpio.h>
#include "ca_platform.h"
#else
#include <linux/stm/gpio.h>
#include <linux/stm/pio.h>
#endif

#define NAME	"stm_asc_scr"

#if ASC_INTERRUPT_DEBUG
uint32_t status_reg[100];
uint32_t interrupt_status[100];
uint32_t interrupt_exit_status[100];
uint32_t count;
#endif /*ASC_INTERRUPT_DEBUG*/

int interrupt_exit;

static void asc_copy_data_from_circbuf(asc_ctrl_t *asc_ctrl_p,
		struct asc_transaction *trns,
		uint32_t num_to_copy);
static uint32_t asc_get_bytes_in_circbuf(asc_ctrl_t *asc_ctrl_p,
			struct asc_transaction *trns);

void asc_interrupt_debug(char *position)
{
	#if ASC_INTERRUPT_DEBUG
	int i = 0;
	for (i = 0; i < count; i++) {
		asc_debug_print("%s status %x int enter %x intexit status %x\n",
			position, status_reg[i],
			interrupt_status[i], interrupt_exit_status[i]);
	}
	#endif
}

static __inline uint32_t asc_writeblock_oneless(asc_ctrl_t *asc_ctrl_p);

static ca_error_code_t asc_circbuf_initialize(asc_ctrl_t *asc_ctrl_p)
{
	asc_ctrl_p->circbuf.base =
			(uint8_t *)ca_os_malloc(ASC_CIRC_BUFFER_SIZE);
	if (asc_ctrl_p->circbuf.base == NULL) {
		asc_error_print("%s@%d Circular buffer init failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	/* flush the empty buffer */
	asc_ctrl_p->circbuf.get_p =
		asc_ctrl_p->circbuf.put_p =
			asc_ctrl_p->circbuf.base;
	asc_ctrl_p->circbuf.charsfree =
		asc_ctrl_p->circbuf.bufsize =
			ASC_CIRC_BUFFER_SIZE;
	return CA_NO_ERROR;
}

static ca_error_code_t asc_allocate_resources(asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	asc_ctrl_p->handle = (void *)asc_ctrl_p;
	error = ca_os_hrt_sema_initialize(&asc_ctrl_p->flow_hrt_sema_p, 0);

	spin_lock_init(&asc_ctrl_p->lock);
	error |= asc_circbuf_initialize(asc_ctrl_p);
	return error;
}

static ca_error_code_t asc_handle_abort(asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	switch (asc_ctrl_p->status_error) {
	case ASC_STATUS_ERROR_ABORT:
		asc_debug_print(" <%s>:: <%d> -asc status error %d\n",
			__func__, __LINE__, asc_ctrl_p->status_error);
		/* should be set to no error for next transfer */
		asc_ctrl_p->status_error = ASC_STATUS_NO_ERROR;
		error = -EIO;
	break;

	/* go in this section if called from ISR */
	case ASC_STATUS_ERROR_RETRIES:
	case ASC_STATUS_ERROR_PARITY:
	case ASC_STATUS_ERROR_FRAME:
	case ASC_STATUS_ERROR_OVERRUN:
		asc_debug_print(" <%s>:: <%d> -asc status error %d\n",
			__func__, __LINE__, asc_ctrl_p->status_error);
		asc_ctrl_p->status_error = ASC_STATUS_NO_ERROR;
		error = -EIO;
	break;
	default:
	break;
	}
	return error;
}

void asc_restore_parity(asc_ctrl_t *asc_ctrl_p, uint8_t RxData)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	if (RxData == ASC_SC_DIRECT_TS_CHAR) {
		ASC_8BITS_PARITY(asc_regs_p);
		ASC_PARITY_EVEN(asc_regs_p);
		asc_ctrl_p->device.params.protocol.databits =
				STM_ASC_SCR_8BITS_EVEN_PARITY;
	} else if (RxData == ASC_SC_INVERSE_TS_CHAR) {
		ASC_8BITS_PARITY(asc_regs_p);
		ASC_PARITY_ODD(asc_regs_p);
		asc_ctrl_p->device.params.protocol.databits =
				STM_ASC_SCR_8BITS_ODD_PARITY;
	} else {
		ASC_9BITS(asc_regs_p);
		ASC_SMARTCARD_DISABLE(asc_regs_p);
		asc_ctrl_p->device.params.protocol.databits =
				STM_ASC_SCR_9BITS_UNKNOWN_PARITY;
	}

	if (asc_ctrl_p->device.params.protocol.databits !=
			STM_ASC_SCR_9BITS_UNKNOWN_PARITY)
		ASC_SMARTCARD_ENABLE(asc_regs_p);
}

static inline void asc_scr_receive_chars(asc_ctrl_t *asc_ctrl_p)
{
	uint8_t c = 0;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_transaction *read_trns = &asc_ctrl_p->trns;
	uint32_t byte_available = asc_get_bytes_in_circbuf(asc_ctrl_p,
						read_trns);
	asc_circbuf *circbuf = &asc_ctrl_p->circbuf;
	if (asc_ctrl_p->currstate == ASC_STATE_READ) {
		asc_debug_print("ASC ISR: read data into user buf from "\
			"circular buffer bytes to read %d\n",
			read_trns->remaining_size);
		byte_available = read_trns->remaining_size < byte_available ?
			read_trns->remaining_size : byte_available;
		asc_copy_data_from_circbuf(asc_ctrl_p,
			read_trns, byte_available);

		asc_debug_print("ASC ISR:read data into user buf "\
			":bytes to read %d\n",
			read_trns->remaining_size);

		/* Pass the new bytes to the read operation buffer */
		while (((ASC_STATUS(asc_regs_p) & STATUS_RX_AVAILABLE) != 0)
				&& read_trns->remaining_size) {
			c = ASC_READ_RXBUF(asc_regs_p);
			ASC_ByteReceivedStatistics(asc_ctrl_p);
			if (asc_ctrl_p->device.params.protocol.databits ==
				STM_ASC_SCR_9BITS_UNKNOWN_PARITY)
				asc_restore_parity(asc_ctrl_p, c);

			asc_debug_print("0x%x\n", c);
			*read_trns->userbuffer++ = c;
			read_trns->remaining_size--;
			if (read_trns->remaining_size == 0)
				break;
		}
		if (read_trns->remaining_size == 0) {
			asc_ctrl_p->status_error = ASC_STATUS_NO_ERROR;
			asc_ctrl_p->currstate = ASC_STATE_OP_COMPLETE;
			ca_os_hrt_sema_signal(asc_ctrl_p->flow_hrt_sema_p);
		}
	} else {
		asc_debug_print("ASC ISR: read data into Circular buf\n");
		/* copy the data into swbuffer */
		while (((ASC_STATUS(asc_regs_p) & STATUS_RX_AVAILABLE) != 0)
				&& circbuf->charsfree > 0) {
			c = ASC_READ_RXBUF(asc_regs_p);
			ASC_ByteReceivedStatistics(asc_ctrl_p);
			asc_debug_print("Circ 0x%x\n", c);
			if (asc_ctrl_p->device.params.protocol.databits ==
				STM_ASC_SCR_9BITS_UNKNOWN_PARITY)
				asc_restore_parity(asc_ctrl_p, c);

			/* Copy the byte from RX buffer */
			*circbuf->put_p++ = c;
			circbuf->charsfree--;
			/* Check for put pointer buffer-wrap */
			if (circbuf->put_p >=
			(circbuf->base + circbuf->bufsize))
				circbuf->put_p = circbuf->base ;
		}
	}
}

static inline bool asc_scr_transmit_chars(asc_ctrl_t *asc_ctrl_p)
{
	uint32_t txsent = 0, interrupt_en = 0;
	bool txcomplete = false;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_transaction *write_trns = &asc_ctrl_p->trns;
	interrupt_exit |= 0x8;
	if (asc_ctrl_p->currstate == ASC_STATE_WRITE) {
		/* Pass the new bytes to the write operation buffer */
		while ((ASC_STATUS(asc_regs_p) &
			(STATUS_TXEMPTY | STATUS_TXHALFEMPTY)) != 0) {
			if (write_trns->remaining_size == 0) {
				interrupt_exit |= 0x10;
				txcomplete = true;
				asc_ctrl_p->status_error = ASC_STATUS_NO_ERROR;
				asc_ctrl_p->currstate = ASC_STATE_OP_COMPLETE;
				asc_debug_print("ASC ISR transmit complete\n");
				ca_os_hrt_sema_signal(
						asc_ctrl_p->flow_hrt_sema_p);
				break;
			} else {
				txsent = asc_writeblock_oneless(asc_ctrl_p);
				if (write_trns->remaining_size == 0) {
					interrupt_exit |= 0x20;
					asc_debug_print("ASC ISR :Tx empty "\
					"interrupt , written = %d\n", txsent);
					interrupt_en =
						ASC_READ_INTENABLE(asc_regs_p);

					interrupt_en = (interrupt_en &
						~IE_TXHALFEMPTY) | IE_TXEMPTY;
					/* All the byte writen so wait for TX empty*/
					break;

				} else {
					interrupt_exit |= 0x40;
					asc_debug_print("ASC ISR :Tx half and"\
						"tx empty interrupt, "\
						"Written = %d\n", txsent);
					interrupt_en = IE_TX_INTERRUPT;
				}
				ASC_ENABLE_INTS(asc_regs_p, interrupt_en);
			}
		}
	} else {
		interrupt_exit |= 0x80;
		ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);
	}
	return txcomplete;
}

static void dump_statistics(asc_ctrl_t *asc_ctrl_p, uint32_t asc_status)
{
	uint32_t i = 0;
	for (i = 0; i < 32; i++) {
		if (asc_status & (1 << i))
			asc_ctrl_p->statistics.KStats[i]++;
	}
}
static irqreturn_t asc_scr_stm_irq(int irq, void *dev_id)
{
	u_long status = 0, asc_status = 0, intenable = 0;
	bool txcomplete = false;
	asc_ctrl_t *asc_ctrl_p = (asc_ctrl_t *)dev_id;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	spin_lock(&asc_ctrl_p->lock);
	interrupt_exit = 0;

#if ASC_INTERRUPT_DEBUG
	if (count >= 100)
		count = 0;
	interrupt_status[count] = ASC_READ_INTENABLE(asc_regs_p) |
			(asc_ctrl_p->currstate << 12);
#endif /*ASC_INTERRUPT_DEBUG*/

	intenable = ASC_READ_INTENABLE(asc_regs_p);

	ASC_GLOBAL_DISABLEINTS(asc_regs_p);

	ASC_ITCountStatistics(asc_ctrl_p);
	status = ASC_STATUS(asc_regs_p);
	/* take the status only for which interrupts
	*have been enabled */
	asc_status = status & intenable;
	dump_statistics(asc_ctrl_p, asc_status);
#if ASC_INTERRUPT_DEBUG
	status_reg[count++] = status;
#endif /*ASC_INTERRUPT_DEBUG*/
	if (status & STATUS_ASC_ERRORS) {
		asc_debug_print("ASC ISR: ASC error %ld\n", status);

		if (status & STATUS_FRAMEERROR) {
			asc_ctrl_p->status_error = ASC_STATUS_ERROR_FRAME;
			ASC_RESET_RXFIFO(asc_regs_p);
			asc_debug_print("ASC ISR: ASC STATUS_FRAMEERROR %ld\n",
					status);

			if ((asc_ctrl_p->currstate == ASC_STATE_WRITE) ||\
				(asc_ctrl_p->currstate == ASC_STATE_READ)) {
				asc_ctrl_p->currstate = ASC_STATE_ABORT;
				ca_os_hrt_sema_signal(
					asc_ctrl_p->flow_hrt_sema_p);
			} else {
				asc_ctrl_p->currstate = ASC_STATE_ABORT;
			}
			interrupt_exit |= (asc_ctrl_p->currstate <<12);
			spin_unlock(&asc_ctrl_p->lock);

			ASC_FramingIntStatistics(asc_ctrl_p);
			return IRQ_HANDLED;
		}

		if (status & STATUS_OVERRUNERROR) {
			asc_ctrl_p->status_error = ASC_STATUS_ERROR_OVERRUN;
			ASC_RESET_RXFIFO(asc_regs_p);
			asc_debug_print("ASC ISR: ASC STATUS_OVERRUNERROR"\
				"%ld\n", status);

			/* after write is finished(say driver is in READY state)
			* and overrun error occurs due to loopback bytes then
			* signal the semaphore :looped back read will
			* immediately receive the signal and scr_asc_write()
			* willreturn with -EOVERFLOW error */
			if (asc_ctrl_p->currstate != ASC_STATE_WRITE) {
				asc_ctrl_p->currstate = ASC_STATE_ABORT;
				ca_os_hrt_sema_signal(
						asc_ctrl_p->flow_hrt_sema_p);
			}
			interrupt_exit |= (asc_ctrl_p->currstate <<12);
			spin_unlock(&asc_ctrl_p->lock);
			ASC_OverrunIntStatistics(asc_ctrl_p);
			return IRQ_HANDLED;
		}
		if (status & STATUS_NACKED) {
			if (asc_ctrl_p->currstate == ASC_STATE_WRITE) {
				asc_ctrl_p->status_error =
					ASC_STATUS_ERROR_RETRIES;
				ASC_RESET_TXFIFO(asc_regs_p);
				asc_debug_print("ASC ISR: ASC STATUS_NACKED"\
					"%ld\n", status);
				asc_ctrl_p->currstate = ASC_STATE_ABORT;
				ca_os_hrt_sema_signal(
					asc_ctrl_p->flow_hrt_sema_p);
				interrupt_exit |= (asc_ctrl_p->currstate << 12);
				spin_unlock(&asc_ctrl_p->lock);
				ASC_NackWriteErrorStatistics(asc_ctrl_p);
				return IRQ_HANDLED;
			}
			ASC_NackErrorStatistics(asc_ctrl_p);
		}

	}
	/* Interrupt fired when TX shift register becomes empty */
	if ((status & (STATUS_TXEMPTY | STATUS_TXHALFEMPTY)) != 0) {
		/* Transmitteir FIFO at least half empty */
		interrupt_exit |= 0x2;
		txcomplete = asc_scr_transmit_chars(asc_ctrl_p);
		if (txcomplete == true) {
			interrupt_exit |= 0x4;
			/* no need to enable TXEMPTY in this case */
			ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);
		}
	}
	/* Receive FIFO not empty */
	if (status & STATUS_RX_AVAILABLE) {
		interrupt_exit = 0x1;
		asc_scr_receive_chars(asc_ctrl_p);
		ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);
	}

	interrupt_exit |= 0x100;
	interrupt_exit |= (asc_ctrl_p->currstate << 12);
#if ASC_INTERRUPT_DEBUG
	interrupt_exit_status[count - 1] = interrupt_exit;
#endif /*ASC_INTERRUPT_DEBUG*/
	spin_unlock(&asc_ctrl_p->lock);
	return IRQ_HANDLED;
}

/*****************************************************************************
asc_writeblock_oneless()
Description:
	This routine transmits a block of data through the UART's ASCnTxBuffer
	register until either the UART's FIFO is full or all required bytes have
	been written. In UART IP Revision 2, data cannt be written to the nth
	position of the TxFifo. so This routine writes till nth-1 position.
*****************************************************************************/
static __inline uint32_t  asc_writeblock_oneless(asc_ctrl_t *asc_ctrl_p)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	uint32_t txsent = 0, i = 0;
	uint32_t halfhwfifosize = (asc_ctrl_p->device.hwfifosize / 2);
	struct asc_transaction *trns = &asc_ctrl_p->trns;

	/* initailly remaining_size = size */
	if (trns->remaining_size > (halfhwfifosize)) {
		for (i = 0; i < halfhwfifosize; i++)
			ASC_WRITE_TXBUF(asc_regs_p, trns->userbuffer[i]);

	/* no need to do spin_lock as interrupts are disabled */
		trns->userbuffer += halfhwfifosize;
		trns->remaining_size -= halfhwfifosize;
	} else if (trns->remaining_size > 0) {

		for (i = 0; i < trns->remaining_size -1; i++)
			ASC_WRITE_TXBUF(asc_regs_p, trns->userbuffer[i]);
		ASC_WRITE_TXBUF(asc_regs_p, (trns->userbuffer[i] | 0x1<<8));

		trns->userbuffer += trns->remaining_size;
		trns->remaining_size = 0;
	} else {
		/*Set the guard time to default(2) before sending last byte*/
		/*On next write the SCR will set
		  the guard time to user specified via asc_set_ctrl call*/
		ASC_SETGUARDTIME(asc_regs_p, DEFAULT_GUARD_TIME);
		ASC_WRITE_TXBUF(asc_regs_p, trns->userbuffer[0]);

		trns->userbuffer += trns->remaining_size;
		trns->remaining_size = 0;
	}

	txsent = trns->size - trns->remaining_size;
	ASC_ByteTransmittedStatistics(asc_ctrl_p, txsent);

	return txsent;
}

/*****************************************************************************
check_and_set_next()
Description:
	Checks if the call to this API valid
*****************************************************************************/
static ca_error_code_t check_and_set_next(enum asc_state_e desired_nextstate,
		asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	enum asc_state_e currstate = asc_ctrl_p->currstate;

	switch (desired_nextstate) {
	case ASC_STATE_ABORT:
	case ASC_STATE_OP_COMPLETE:
		if ((currstate != ASC_STATE_READ) &&
			(currstate != ASC_STATE_WRITE))
			error = -EINVAL;
	break;
	case ASC_STATE_READ:
	case ASC_STATE_WRITE:
	case ASC_STATE_FLUSH:
		/* In abort state: new read or write or flush could be issued */
		if ((currstate != ASC_STATE_READY) &&
		(currstate != ASC_STATE_ABORT))
			error = -EINVAL;
	break;

	case ASC_STATE_READY:
		if ((currstate != ASC_STATE_IDLE))
			error = -EINVAL;
	break;
	case ASC_STATE_IDLE:
	case ASC_STATE_RESERVED:
	break;
	}
	return error;
}

static void update_cur_state(enum asc_state_e desired, asc_ctrl_t *asc_ctrl_p)
{
	asc_ctrl_p->currstate = desired;
}

static ca_error_code_t asc_extract_platform_data(asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	struct resource *res;
#ifdef CONFIG_ARCH_STI
	int err = 0;
#endif
	struct clk *clk;
	struct asc_device *device = &asc_ctrl_p->device;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;

	/* Get resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		asc_error_print("failed to find IOMEM resource %p\n",
			&pdev->dev);
		return -ENOENT;
	}
	device->r_mem = *res;

	device->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!device->base) {
		asc_error_print("ioremap memory failed [%p] 0x%x\n",
					&pdev->dev, res->start);
		error = -ENXIO;
		goto err2;
	}
	asc_debug_print("ASC baseaddress = %p\n", device->base);

	/* Get IRQ number from setup.c */
#ifdef CONFIG_ARCH_STI
	device->irq = platform_get_irq(pdev, 0);
	err = devm_request_irq(&pdev->dev, device->irq, asc_scr_stm_irq, 0,
				dev_name(&pdev->dev), asc_ctrl_p);
	if (err) {
		asc_error_print("irq request failed %p\n", &pdev->dev);
		error = -EBUSY;
		goto err3;
	}
	if (plat_data->pinctrl == NULL) {
		plat_data->pinctrl = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(plat_data->pinctrl))
			return PTR_ERR(plat_data->pinctrl);
	}
	plat_data->pins_default = pinctrl_lookup_state(plat_data->pinctrl,
					PINCTRL_STATE_DEFAULT);
	if (IS_ERR(plat_data->pins_default)) {
		dev_err(&pdev->dev, "could not get default pinstate\n");
		return -EINVAL;
	} else
		pinctrl_select_state(plat_data->pinctrl,
					plat_data->pins_default);
	clk = devm_clk_get(&pdev->dev, NULL);
#else
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		asc_error_print("failed to find IRQ resource  [%p]\n",
				&pdev->dev);
		error = -ENOENT;
		goto err3;
	}
	device->r_irq = *res;

	if (request_irq(res->start, asc_scr_stm_irq, 0,
			dev_name(&pdev->dev), asc_ctrl_p)) {
		asc_error_print("irq request failed %p\n", &pdev->dev);
		error = -EBUSY;
		goto err3;
	}

	asc_ctrl_p->device.pad_state =
			stm_pad_claim(plat_data->pad_config, "asc");
	if (NULL == asc_ctrl_p->device.pad_state) {
		asc_error_print(" <%s> :: <%d> Pad claim failed\n",
				__func__, __LINE__);
		goto err4;
	}

	clk = clk_get(&pdev->dev, "comms_clk");
#endif
	if (IS_ERR(clk)) {
		asc_error_print("Comms clock not found!%p\n", &pdev->dev);
		goto err5;
	}
	clk_prepare_enable(clk);
	device->uartclk = clk_get_rate(clk);
	return error;

err5:
#ifndef CONFIG_ARCH_STI
	stm_pad_release(asc_ctrl_p->device.pad_state);
err4:
#endif
#ifdef CONFIG_ARCH_STI
#else
	free_irq(device->r_irq.start, asc_ctrl_p);
#endif
err3:
	iounmap(device->base);
err2:
	/*release_mem_region(device->r_mem.start,
	* resource_size(&device->r_mem));*/
	return error;
}

void dumpreg(asc_ctrl_t *asc_ctrl_p)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	UNUSED_PARAMETER(asc_regs_p);
	asc_regdump_print("ASC Baudrate 0x%x\n",
			ASC_READ_BAUDRATE(asc_regs_p));
	asc_regdump_print("ASC control 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p));
	asc_regdump_print("ASC interrupt enable 0x%x\n",
			ASC_READ_INTENABLE(asc_regs_p));
	asc_regdump_print("ASC status 0x%x\n", ASC_STATUS(asc_regs_p));
	asc_regdump_print("ASC Guardtime 0x%x\n",
			ASC_READ_GUARDTIME(asc_regs_p));
	asc_regdump_print("ASC Timeout 0x%x\n",
			ASC_READ_TIMEOUT(asc_regs_p));
	asc_regdump_print("ASC Threshold 0x%x\n",
			ASC_READ_THRESHOLD(asc_regs_p));

	/* bit 10:1:enabled 0 :disabled */
	asc_regdump_print("Flow control disabled 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x400);
	/* bit 14:1:enabled(The words with Parity Error will not
	* be stored in FIFO) 0 :disabled */
	asc_regdump_print("Autoparity Rejection enabled 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x4000);
	/* bit 13:0:enabled 1 :disabled */
	asc_regdump_print("NACK enabled 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x2000);
	/* bit 11:1:enabled 0 :ignored */
	asc_regdump_print("CTS disabled 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x800);
	/*00: >0.5 stop bits, 10: 1.5 stop bits, 01: >1stop bit ,
	* 11: 2 stop bits*/
	asc_regdump_print("Stop bits 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x18);
	/*100 :9-bit data 101 :8-bit data + wake up bit
	110 :Reserved 111 :8-bit data + parity*/
	asc_regdump_print("Mode bits 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x7);
	/*0:>Even parity (parity bit set on odd number of \911\92s in data)
	1:>Odd parity (parity bit set on even number of \911\92s in data*/
	asc_regdump_print("Parity bits 0x%x\n",
			ASC_READ_CONTROL(asc_regs_p) & 0x20);
}

static void asc_set_smartcard_mode(asc_ctrl_t *asc_ctrl_p)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	/* If smartcard mode is enabled, we must be careful to ensure
	* that the TXD pin is set in bidirectional mode i.e., the
	* ASC is effectively operating in half-duplex mode. Otherwise
	* we'll be driving the line when the card is trying to transmit
	*/
	/* Enable smart card mode */
	if (asc_ctrl_p->device.params.protocol.databits ==
		STM_ASC_SCR_9BITS_UNKNOWN_PARITY)
		ASC_SMARTCARD_DISABLE(asc_regs_p);
	else
		ASC_SMARTCARD_ENABLE(asc_regs_p);

	/* Set guard time appropriately */
	ASC_SETGUARDTIME(asc_regs_p, DEFAULT_GUARD_TIME);

	if (asc_ctrl_p->device.hwfifosize != 1)
		ASC_SET_THRESHOLD(asc_regs_p, asc_ctrl_p->device.hwfifosize);

	/* Enable the hardware FIFO */
	ASC_FIFO_ENABLE(asc_regs_p);
	ASC_SET_TIMEOUT(asc_regs_p, ASC_TIMEOUT_VALUE);

	/* Clear FIFOs */
	ASC_RESET_TXFIFO(asc_regs_p);
	ASC_RESET_RXFIFO(asc_regs_p);

	ASC_GLOBAL_DISABLEINTS(asc_regs_p);

	/* Enable run to start the UART */
	ASC_RUN_ENABLE(asc_regs_p);

	/* Enable receiver */
	ASC_RECEIVER_ENABLE(asc_regs_p);

	/* Enable receive interrupts/errors to begin with */
	/* Tx interrupts will be enabled while writing */
	ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);
}

static void asc_populate_default_params(asc_ctrl_t *asc_ctrl_p)
{
	asc_params_t *asc_params = &asc_ctrl_p->device.params;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	asc_params->flowcontrol = 1;
	asc_params->autoparityrejection = 0;
	asc_params->nack = 1;
	asc_params->txretries = TX_RETRIES_COUNT;
	asc_params->protocol.baud = DEFAULT_BAUD_RATE;
	asc_params->protocol.stopbits = STM_ASC_SCR_STOP_1_5;
	asc_params->protocol.databits = STM_ASC_SCR_9BITS_UNKNOWN_PARITY;

	/* Clear out UART statistics */
	memset(&asc_ctrl_p->statistics, 0, sizeof(struct asc_statistics_t));

	/*Disable flow control*/
	asc_set_flow_control(asc_ctrl_p, 1);
	/*Disable Auto parity rejection for reset */
	asc_set_autoparity_rejection(asc_ctrl_p, 0);

	ASC_NACK_ENABLE(asc_regs_p);

	asc_set_protocol(asc_ctrl_p, asc_params->protocol);

	ASC_CTS_DISABLE(asc_regs_p);
}

static void asc_populate_op(stm_asc_scr_data_params_t *dataparams,
			struct asc_transaction *trns)
{
	trns->userbuffer = dataparams->buffer_p;
	trns->size = dataparams->size;
	trns->remaining_size = dataparams->size;
}

/*****************************************************************************
asc_copy_data_from_circbuf()
Description:
	This routine safely copies bytes from the software FIFO of the
	given UART - as many bytes as available, up to the number of bytes
	requested. If we empty the SW FIFO without fulfilling the request,
	the operation is passed to the ISR

*****************************************************************************/
static void asc_copy_data_from_circbuf(asc_ctrl_t *asc_ctrl_p,
			struct asc_transaction *trns,
			uint32_t num_to_copy)

{
	uint32_t num_to_read = num_to_copy;

	/* Copy bytes to the destination buffer */
	while (num_to_read) {
		/* Read next available character */
		*trns->userbuffer++ = *asc_ctrl_p->circbuf.get_p++;

		/* Check for a read pointer buffer-wrap */
		if (asc_ctrl_p->circbuf.get_p >=
		(asc_ctrl_p->circbuf.base + asc_ctrl_p->circbuf.bufsize))
			asc_ctrl_p->circbuf.get_p = asc_ctrl_p->circbuf.base;

		/* Update count of characters */
		num_to_read--;
	}
	trns->remaining_size -= num_to_copy;
	/* ReceiveBufferCharsFree increment must be atomic */
	/* ISR relies on ReceiveBufferCharsFree to determine if it can write */
	asc_ctrl_p->circbuf.charsfree += num_to_copy;
}

static uint32_t asc_get_bytes_in_circbuf(asc_ctrl_t *asc_ctrl_p,
			struct asc_transaction *trns)
{
	uint32_t num_available = 0;
	num_available = (asc_ctrl_p->circbuf.bufsize) -
			(asc_ctrl_p->circbuf.charsfree);

	asc_debug_print("%s@%d number of bytes in circbuf %d\n",
			__func__, __LINE__, num_available);
	return num_available;
}

static void asc_handle_read_timeout(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_data_params_t *params ,
		struct asc_transaction *trns, ca_error_code_t *error)
{
	u_long flags, status = 0;
	uint32_t i = 0, remaining_size = 0, residuebytes = 0;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	remaining_size = trns->remaining_size;

	status = ASC_STATUS(asc_regs_p);
	residuebytes = (status & 0x003FF000);
	residuebytes >>= 12;

	/*0xff indicates empty fifo, we have a genuine timeout case here. */
	if (residuebytes < ((asc_ctrl_p->device.hwfifosize * 2) - 1)) {
		if ((residuebytes >= remaining_size) && remaining_size > 0) {
			spin_lock_irqsave(&asc_ctrl_p->lock, flags);
			for (i = 0 ; i < remaining_size ; i++)
				params->buffer_p[params->size -
					(remaining_size - i)]
						= ASC_READ_RXBUF(asc_regs_p);

			spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);
			params->actual_size += remaining_size;
			*error = CA_NO_ERROR;
		}
	}
}

ca_error_code_t asc_create_new(stm_asc_scr_new_params_t *new_params_p,
				stm_asc_h *handle_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	asc_ctrl_t *asc_ctrl_p = NULL;
	u_long flags;
	struct platform_device *plat_asc =
			(struct platform_device *)new_params_p->device_data;

	asc_ctrl_p = (asc_ctrl_t*)ca_os_malloc(sizeof(asc_ctrl_t));
	if (NULL == asc_ctrl_p) {
		asc_error_print("%s@%d:Error(%d) in asc scr port allocation\n",
				__func__, __LINE__, error);
		return -ENOMEM;
	}
	error = asc_allocate_resources(asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d: Error(%d) in asc_allocate_resources\n",
				__func__, __LINE__, error);
		goto asc_error;
	}
	asc_ctrl_p->device.transfer_type = new_params_p->transfer_type;
	asc_ctrl_p->device.pdev = (struct platform_device *)plat_asc;
	error = asc_extract_platform_data(asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d:Error(%d)in asc_extract_platform_data\n",
				__func__, __LINE__, error);
		goto asc_error;
	}
	asc_ctrl_p->status_error = ASC_STATUS_NO_ERROR;

	asc_ctrl_p->currstate = ASC_STATE_IDLE;

	error = check_and_set_next(ASC_STATE_READY, asc_ctrl_p);

	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d: Error(%d) in check_and_set_next\n",
				__func__, __LINE__, error);
		goto asc_error;
	}
	asc_populate_default_params(asc_ctrl_p);

	asc_ctrl_p->device.hwfifosize = new_params_p->asc_fifo_size;

	asc_set_smartcard_mode(asc_ctrl_p);

	/*Copy the subscription handle*/
	*handle_p = asc_ctrl_p;
	asc_debug_print("%s@%d:Error(%d)<<<<\n",
		__func__, __LINE__, error);
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_READY, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	return CA_NO_ERROR;

	asc_error:
		if(asc_ctrl_p->circbuf.base)
			ca_os_free((void *)asc_ctrl_p->circbuf.base);

		error = ca_os_hrt_sema_terminate(asc_ctrl_p->flow_hrt_sema_p);
		asc_ctrl_p->flow_hrt_sema_p = NULL;
		/*dealloc the asc scr port memory*/
		ca_os_free((void *)asc_ctrl_p);

	return error;

}

ca_error_code_t asc_abort(asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	volatile asc_regs_t *asc_regs_p =
			(asc_regs_t *)asc_ctrl_p->device.base;

	error = check_and_set_next(ASC_STATE_ABORT, asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_debug_print("%s@%d: Currstate not read or write <<<<\n",
				__func__, __LINE__);
		return error;
	}
	asc_debug_print("%s@%d: Currstate is read or write <<<<\n",
				__func__, __LINE__);

	ASC_RECEIVER_DISABLE(asc_regs_p);
	ASC_GLOBAL_DISABLEINTS(asc_regs_p);

	asc_ctrl_p->status_error = ASC_STATUS_ERROR_ABORT;
	asc_ctrl_p->currstate = ASC_STATE_ABORT;
	ca_os_hrt_sema_signal(asc_ctrl_p->flow_hrt_sema_p);

	ASC_RESET_TXFIFO(asc_regs_p);
	ASC_RESET_RXFIFO(asc_regs_p);

	ASC_RECEIVER_ENABLE(asc_regs_p);
	ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);
	/* state will be changed to READY by write or read api */
	return error;
}

ca_error_code_t asc_flush(asc_ctrl_t *asc_ctrl_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	u_long flags;
	volatile asc_regs_t *asc_regs_p =
				(asc_regs_t *)asc_ctrl_p->device.base;

	/* Ensure a read is not in progress */
	error = check_and_set_next(ASC_STATE_FLUSH,
				asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d: error:%d <<<<\n",
				__func__, __LINE__, error);
		return error;
	}

	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_FLUSH, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	ASC_GLOBAL_DISABLEINTS(asc_regs_p);
	ASC_RECEIVER_DISABLE(asc_regs_p);

	asc_ctrl_p->circbuf.charsfree = ASC_CIRC_BUFFER_SIZE;
	asc_ctrl_p->circbuf.put_p =
		asc_ctrl_p->circbuf.get_p =
			asc_ctrl_p->circbuf.base;

	ASC_RESET_RXFIFO(asc_regs_p);

	/* Re-enable receiver */
	ASC_RECEIVER_ENABLE(asc_regs_p);
	ASC_ENABLE_INTS(asc_regs_p, IE_RX_INTERRUPT);

	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_READY, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	return error;
}

void asc_delete_intance(asc_ctrl_t *asc_ctrl_p)
{
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;

	/* Abort all activity on the UART */
	asc_abort(asc_ctrl_p);

	/* Nullify the handle to ensure no further API calls can be made
	* with the handle.
	*/
	asc_ctrl_p->handle = 0;

	/* Disable the UART i.e., disable interrupts and receiver */
	ASC_GLOBAL_DISABLEINTS(asc_regs_p);

	ASC_RECEIVER_DISABLE(asc_regs_p);
}

#ifdef CONFIG_ARCH_STI
void asc_deallocate_platform_data(asc_ctrl_t *asc_ctrl_p)
{
	struct clk *clk = NULL;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;

	ca_os_hrt_sema_terminate(asc_ctrl_p->flow_hrt_sema_p);
	asc_ctrl_p->flow_hrt_sema_p = NULL;

	if (plat_data->pinctrl != NULL) {
		devm_pinctrl_put(plat_data->pinctrl);
		plat_data->pinctrl = NULL;
	}
	free_irq(asc_ctrl_p->device.irq, asc_ctrl_p);
	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		asc_error_print("Comms clock not found!%p\n", &pdev->dev);
	}
	else
		clk_disable_unprepare(clk);

	if (asc_ctrl_p->device.base)
		iounmap(asc_ctrl_p->device.base);
	if (asc_ctrl_p->circbuf.base) {
		ca_os_free((void *)asc_ctrl_p->circbuf.base);
		asc_ctrl_p->circbuf.base = NULL;
	}
}
#else
void asc_deallocate_platform_data(asc_ctrl_t *asc_ctrl_p)
{
	struct clk *clk = NULL;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;

	ca_os_hrt_sema_terminate(asc_ctrl_p->flow_hrt_sema_p);
	asc_ctrl_p->flow_hrt_sema_p = NULL;

	if (asc_ctrl_p->device.pad_state) {
		stm_pad_release(asc_ctrl_p->device.pad_state);
		asc_ctrl_p->device.pad_state = NULL;
	}

	free_irq(asc_ctrl_p->device.r_irq.start, asc_ctrl_p);

	clk = clk_get(&pdev->dev, "comms_clk");
	if (IS_ERR(clk)) {
		asc_error_print("Comms clock not found!%p\n", &pdev->dev);
	}
	else
		clk_disable_unprepare(clk);

	if (asc_ctrl_p->device.base)
		iounmap(asc_ctrl_p->device.base);

	if (asc_ctrl_p->circbuf.base) {
		ca_os_free((void *)asc_ctrl_p->circbuf.base);
		asc_ctrl_p->circbuf.base = NULL;
	}
}
#endif

ca_error_code_t asc_set_compound_ctrl(asc_ctrl_t *asc_ctrl_p,
	stm_asc_scr_ctrl_flags_t ctrl_flag, void *value_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_device *device = &asc_ctrl_p->device;

	ASC_RECEIVER_DISABLE(asc_regs_p);
	switch (ctrl_flag) {
	case STM_ASC_SCR_PROTOCOL:
		error = asc_set_protocol(asc_ctrl_p,
			*(stm_asc_scr_protocol_t *)value_p);
		if (error == CA_NO_ERROR)
			device->params.protocol =
				*(stm_asc_scr_protocol_t *)value_p;
	break;
	default:
		error = -EINVAL;
	break;
	}
	ASC_RECEIVER_ENABLE(asc_regs_p);
	return error;
}

ca_error_code_t asc_set_ctrl(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_ctrl_flags_t ctrl_flag, uint32_t value)
{
	ca_error_code_t error = CA_NO_ERROR;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	struct asc_device *device = &asc_ctrl_p->device;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;

	switch (ctrl_flag) {
	case STM_ASC_SCR_FLOW_CONTROL:
		asc_debug_print("%s@%d: Flow control data(%d)<<<<\n",
			__func__, __LINE__, (uint8_t)value);
		ASC_RECEIVER_DISABLE(asc_regs_p);
		error = asc_set_flow_control(asc_ctrl_p, (uint8_t)value);
		ASC_RECEIVER_ENABLE(asc_regs_p);
	break;

	case STM_ASC_SCR_AUTO_PARITY_REJECTION:
		asc_debug_print("%s@%d: Auto parity data(%d)<<<<\n",
				__func__, __LINE__, (uint8_t)value);
		ASC_RECEIVER_DISABLE(asc_regs_p);
		asc_set_autoparity_rejection(asc_ctrl_p, (uint8_t)value);
		ASC_RECEIVER_ENABLE(asc_regs_p);
	break;

	case STM_ASC_SCR_NACK:
		asc_debug_print("%s@%d: NACK data(%d)<<<<\n",
				__func__, __LINE__, (uint8_t)value);
		ASC_RECEIVER_DISABLE(asc_regs_p);
		error = asc_set_nack(asc_ctrl_p, (uint8_t)value);
		ASC_RECEIVER_ENABLE(asc_regs_p);
	break;

	case STM_ASC_SCR_TX_RETRIES:
		asc_debug_print("%s@%d: Retries data(%d)<<<<\n",
				__func__, __LINE__, (uint8_t)value);
		ASC_RECEIVER_DISABLE(asc_regs_p);
		ASC_SET_RETRIES(asc_regs_p, (uint8_t)value);
		ASC_RECEIVER_ENABLE(asc_regs_p);
		device->params.txretries = (uint8_t)value;
	break;

	case STM_ASC_SCR_GUARD_TIME:
		ASC_RECEIVER_DISABLE(asc_regs_p);
		ASC_SETGUARDTIME(asc_regs_p, (uint8_t)value);
		ASC_RECEIVER_ENABLE(asc_regs_p);
	break;
#ifdef CONFIG_ARCH_STI
	case STM_ASC_SCR_GPIO_C4:
		asc_debug_print("%s@%d: set c4 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(plat_data->c4_pin->gpio_num, (uint8_t)value);
	break;

	case STM_ASC_SCR_GPIO_C7:
		asc_debug_print("%s@%d: set c7 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(plat_data->c7_pin->gpio_num, (uint8_t)value);
	break;
	case STM_ASC_SCR_GPIO_C8:
		asc_debug_print("%s@%d: set c8 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(plat_data->c8_pin->gpio_num, (uint8_t)value);
	break;
#else
	case STM_ASC_SCR_GPIO_C4:
		asc_debug_print("%s@%d: set c4 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C4_BIT].gpio,
		(uint8_t)value);
	break;

	case STM_ASC_SCR_GPIO_C7:
		asc_debug_print("%s@%d: set c7 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C7_BIT].gpio,
		(uint8_t)value);
	break;

	case STM_ASC_SCR_GPIO_C8:
		asc_debug_print("%s@%d: set c8 data(%d)<<<<\n",
					__func__, __LINE__, (uint8_t)value);
		gpio_set_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C8_BIT].gpio,
		(uint8_t)value);
	break;
#endif
	default:
	break;
	}
	return error;
}

void asc_get_ctrl(asc_ctrl_t *asc_ctrl_p,
	stm_asc_scr_ctrl_flags_t ctrl_flag, uint32_t *value_p)
{
	struct asc_device *device = &asc_ctrl_p->device;
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;

	switch (ctrl_flag) {
	case STM_ASC_SCR_FLOW_CONTROL:
		*(uint8_t *)value_p = device->params.flowcontrol;
		asc_debug_print("%s@%d: Flow control value_p(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_AUTO_PARITY_REJECTION:
		*(uint8_t *)value_p = device->params.autoparityrejection;
		asc_debug_print("%s@%d: Auto parity (%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_NACK:
		*(uint8_t *)value_p = device->params.nack;
		asc_debug_print("%s@%d: NACK (%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_TX_RETRIES:
		*(uint8_t *)value_p = device->params.txretries;
		asc_debug_print("%s@%d: Retries (%d) <<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_PROTOCOL:
		*(stm_asc_scr_protocol_t *)value_p = device->params.protocol;
	break;

	case STM_ASC_SCR_GUARD_TIME:
		*(uint8_t *)value_p = device->params.guardtime;
	break;
#ifdef CONFIG_ARCH_STI
	case STM_ASC_SCR_GPIO_C4:
		if (plat_data->c4_pin->gpio_mode)
			*(uint8_t *)value_p =
				__gpio_get_value(plat_data->c4_pin->gpio_num);
		asc_debug_print("%s@%d: get c4 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);

	break;
	case STM_ASC_SCR_GPIO_C7:
		if (plat_data->c7_pin->gpio_mode)
			*(uint8_t *)value_p =
				__gpio_get_value(plat_data->c7_pin->gpio_num);
		asc_debug_print("%s@%d: get c7 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;
	case STM_ASC_SCR_GPIO_C8:
		if (plat_data->c8_pin->gpio_mode)
			*(uint8_t *)value_p =
				__gpio_get_value(plat_data->c8_pin->gpio_num);
		asc_debug_print("%s@%d: get c8 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;
#else
	case STM_ASC_SCR_GPIO_C4:
		*(uint8_t *)value_p =
		__gpio_get_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C4_BIT].gpio);
		asc_debug_print("%s@%d: get c4 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_GPIO_C7:
		*(uint8_t *)value_p =
		__gpio_get_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C7_BIT].gpio);
		asc_debug_print("%s@%d: get c7 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;

	case STM_ASC_SCR_GPIO_C8:
		*(uint8_t *)value_p =
		__gpio_get_value(
		plat_data->pad_config->gpios[STM_ASC_GPIO_C8_BIT].gpio);
		asc_debug_print("%s@%d: get c8 data(%d)<<<<\n",
				__func__, __LINE__, *(uint8_t *)value_p);
	break;
#endif
	default:
	break;
	}
}

void asc_get_compound_ctrl(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_ctrl_flags_t ctrl_flag, void *value_p)
{
	struct asc_device *device = &asc_ctrl_p->device;

	switch (ctrl_flag) {
	case STM_ASC_SCR_PROTOCOL:
		*(stm_asc_scr_protocol_t *)value_p = device->params.protocol;
	break;
	default:
	break;
	}
}

ca_error_code_t asc_write(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_data_params_t *params)
{
	ca_error_code_t error = CA_NO_ERROR;
	u_long flags;
	uint32_t txsent = 0;
	volatile asc_regs_t *asc_regs_p = (asc_regs_t *)asc_ctrl_p->device.base;
	asc_debug_print(" ASC_write entered\n");
	error = check_and_set_next(ASC_STATE_WRITE, asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d: error:%d <<<<\n",
				__func__, __LINE__, error);
		return error;
	}
#if ASC_INTERRUPT_DEBUG
	count = 0;
#endif /*ASC_INTERRUPT_DEBUG*/
	/* to disable any RX interrupt while tx data */
	ASC_GLOBAL_DISABLEINTS(asc_regs_p);

	/*curr_state = next_state */
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_WRITE, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	asc_populate_op(params, &asc_ctrl_p->trns);
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	txsent = asc_writeblock_oneless(asc_ctrl_p);
	params->actual_size = txsent;
	if(asc_ctrl_p->trns.remaining_size == 0)
		ASC_ENABLE_INTS(asc_regs_p, IE_TXEMPTY);
	else
		ASC_ENABLE_INTS(asc_regs_p, IE_TX_INTERRUPT);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);
	/* curr state is WRITE now */
	/*Reserved here signifies, That I am waiting for an
	external person[ISR or ABORT] to give me a command*/
	error = ca_os_hrt_sema_wait_timeout(asc_ctrl_p->flow_hrt_sema_p,
					params->timeout_ms);
	if (error != CA_NO_ERROR)
		error = -ETIMEDOUT;
	else {
		switch (asc_ctrl_p->currstate) {
		case ASC_STATE_ABORT:
			asc_error_print("%s@%d: write aborted <<<<\n",
					__func__, __LINE__);
			error = asc_handle_abort(asc_ctrl_p);
			params->actual_size = 0;
			error = -EIO;
		break;
		case ASC_STATE_OP_COMPLETE:
			params->actual_size = params->size;
			error = asc_ctrl_p->status_error;
		break;
		default:
		break;
		}

	}
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_READY, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	asc_debug_print(" ASC_write exit\n");
	return error;
}

ca_error_code_t asc_read(asc_ctrl_t *asc_ctrl_p,
		stm_asc_scr_data_params_t *params)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint32_t number_available = 0;
	u_long flags;
	struct asc_transaction *trns = &asc_ctrl_p->trns;
	params->actual_size = 0;
	error = check_and_set_next(ASC_STATE_READ, asc_ctrl_p);
	if (CA_NO_ERROR != error) {
		asc_error_print("%s@%d: error:%d <<<<\n",
				__func__, __LINE__, error);
		return error;
	}
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	asc_populate_op(params, trns);
	number_available = asc_get_bytes_in_circbuf(asc_ctrl_p, trns);
	/* copy all data frm cicrbuf and return*/
	if (number_available >= trns->size) {
		asc_debug_print("All %d bytes read\n", trns->size);
		asc_copy_data_from_circbuf(asc_ctrl_p, trns, trns->size);

		params->actual_size = trns->size;
		update_cur_state(ASC_STATE_READY, asc_ctrl_p);
		spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);
		asc_interrupt_debug("from circ");
		return error;
	}
	/* curr state is READ now */
	if(trns->remaining_size != 0) {
		update_cur_state(ASC_STATE_READ, asc_ctrl_p);
		spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);
		/* curr state is READ now */
		error = ca_os_hrt_sema_wait_timeout(asc_ctrl_p->flow_hrt_sema_p,
			params->timeout_ms);
	}
	else{
		update_cur_state(ASC_STATE_OP_COMPLETE, asc_ctrl_p);
		spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);
	}
	asc_debug_print("Data still to be read in ISR %d bytes\n",
				trns->remaining_size);

	if (error != CA_NO_ERROR) {
		if (asc_ctrl_p->status_error == ASC_STATUS_NO_ERROR) {
			params->actual_size =
				trns->size - asc_ctrl_p->trns.remaining_size;
			asc_handle_read_timeout(asc_ctrl_p,
				params, trns, &error);
			if (error != CA_NO_ERROR)
				error = -ETIMEDOUT;
		}
	} else {
		switch (asc_ctrl_p->currstate) {
			case ASC_STATE_ABORT:
				asc_error_print("%s@%d: read aborted <<<<\n",
						__func__, __LINE__);
				error = asc_handle_abort(asc_ctrl_p);
				params->actual_size = 0;
				error = -EIO;
			break;

			case ASC_STATE_OP_COMPLETE:
				asc_debug_print("Still to read %d bytes, "\
				"bytes already read %d\n",
				trns->remaining_size,
				((trns->size) - (trns->remaining_size)));
				error = asc_ctrl_p->status_error;
				params->actual_size = trns->size;
			break;
			default:
			break;
		}
	}
	spin_lock_irqsave(&asc_ctrl_p->lock, flags);
	update_cur_state(ASC_STATE_READY, asc_ctrl_p);
	spin_unlock_irqrestore(&asc_ctrl_p->lock, flags);

	asc_debug_print(" ASC_read exit\n");
	return error;
}
#ifdef CONFIG_ARCH_STI
static void asc_free_gpio(struct platform_device *pdev,
		struct stm_plat_asc_data *plat_data)
{
	if (plat_data->c4_pin->gpio_mode == true) {
		devm_gpio_free(&pdev->dev, plat_data->c4_pin->gpio_num);
		plat_data->c4_pin->gpio_requested = false;
	}
	if (plat_data->c7_pin->gpio_mode == true) {
		devm_gpio_free(&pdev->dev, plat_data->c7_pin->gpio_num);
		plat_data->c7_pin->gpio_requested = false;
	}
	if (plat_data->c8_pin->gpio_mode == true) {
		devm_gpio_free(&pdev->dev, plat_data->c8_pin->gpio_num);
		plat_data->c8_pin->gpio_requested = false;
	}

}
static int asc_set_gpio_direction(struct stm_asc_gpio *asc_gpio)
{
	if (asc_gpio->gpio_direction == GPIO_IN)
		gpio_direction_input(asc_gpio->gpio_num);
	else
		return -EINVAL;
	return 0;
}
static int asc_gpio_is_valid(struct stm_plat_asc_data *plat_data)
{
	if (plat_data->c4_pin->gpio_mode == true)
		if (!gpio_is_valid(plat_data->c4_pin->gpio_num))
			return -EINVAL;
	if (plat_data->c7_pin->gpio_mode == true)
		if (!gpio_is_valid(plat_data->c7_pin->gpio_num))
			return -EINVAL;
	if (plat_data->c8_pin->gpio_mode == true)
		if (!gpio_is_valid(plat_data->c8_pin->gpio_num))
			return -EINVAL;
	return 0;
}
static int asc_request_gpio(struct platform_device *pdev,
		struct stm_plat_asc_data *plat_data)
{
	int err = 0;
	asc_debug_print("%s direction %d %d %d\n",
			__FUNCTION__,
			plat_data->c4_pin->gpio_direction,
			plat_data->c7_pin->gpio_direction,
			plat_data->c8_pin->gpio_direction);

	if (asc_gpio_is_valid(plat_data))
		return -EINVAL;

	if ((plat_data->c4_pin->gpio_mode == true) &&
			(plat_data->c4_pin->gpio_requested == false)) {
		if(plat_data->c4_pin->gpio_direction == GPIO_BIDIR){
			asc_debug_print(" C4 is to be configured in"
					"bidir ALT mode\n");
			if(devm_gpio_request_one(&pdev->dev,
						plat_data->c4_pin->gpio_num,
						GPIOF_OPEN_DRAIN,
						pdev->name))
				return -EBUSY;
		}
		else {
			if (devm_gpio_request(&pdev->dev,
				plat_data->c4_pin->gpio_num, pdev->name))
				return -EBUSY;
			err = asc_set_gpio_direction(plat_data->c4_pin);
		}
		plat_data->c4_pin->gpio_requested = true;
	}
	if ((plat_data->c7_pin->gpio_mode == true) &&
			(plat_data->c7_pin->gpio_requested == false)) {
		if(plat_data->c7_pin->gpio_direction == GPIO_BIDIR){
			asc_debug_print(" C7 is to be configured in"
					"bidir ALT mode\n");

			if(devm_gpio_request_one(&pdev->dev,
						plat_data->c7_pin->gpio_num,
						GPIOF_OPEN_DRAIN,
						pdev->name))
				return -EBUSY;
		}
		else {
			if (devm_gpio_request(&pdev->dev,
				plat_data->c7_pin->gpio_num, pdev->name))
				return -EBUSY;
			err |= asc_set_gpio_direction(plat_data->c7_pin);
		}
		plat_data->c7_pin->gpio_requested = true;
	}
	if ((plat_data->c8_pin->gpio_mode == true) &&
			(plat_data->c8_pin->gpio_requested == false)) {
		if(plat_data->c8_pin->gpio_direction == GPIO_BIDIR){
			asc_debug_print(" C8 is to be configured in"
					"bidir ALT mode\n");
			if(devm_gpio_request_one(&pdev->dev,
						plat_data->c8_pin->gpio_num,
						GPIOF_OPEN_DRAIN,
						pdev->name))
				return -EBUSY;
		}
		else {
			if (devm_gpio_request(&pdev->dev,
				plat_data->c8_pin->gpio_num, pdev->name))
				return -EBUSY;
			err |= asc_set_gpio_direction(plat_data->c8_pin);
		}
		plat_data->c8_pin->gpio_requested = true;
	}
	return err;
}
static int asc_request_pinctrl(struct platform_device *pdev,
		struct stm_plat_asc_data *plat_data)
{
	if (plat_data->pinctrl == NULL)
		plat_data->pinctrl = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR(plat_data->pinctrl))
				return PTR_ERR(plat_data->pinctrl);
			plat_data->pins_state =
				pinctrl_lookup_state(plat_data->pinctrl,
					plat_data->config_name);
		if (IS_ERR(plat_data->pins_state)) {
			asc_error_print("<%s> :: <%d> could not get"
				" %s pinstate\n",
				__func__, __LINE__, plat_data->config_name);
			return -EBUSY;
		}
		else
			pinctrl_select_state(plat_data->pinctrl,
					plat_data->pins_state);
		asc_debug_print(" <%s> :: <%d> pinctrl selected %s\n",
			__func__, __LINE__, plat_data->config_name);
	return 0;
}
int asc_pad_config(asc_ctrl_t *asc_ctrl_p, bool release)
{
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;
	int err = 0;
	if (!release) {
		err = asc_request_gpio(pdev, plat_data);
		if (err)
			return err;
		err = asc_request_pinctrl(pdev, plat_data);
		if (err)
			return err;
	} else {
		asc_free_gpio(pdev, plat_data);
		devm_pinctrl_put(plat_data->pinctrl);
		plat_data->pinctrl = NULL;
		asc_error_print(" <%s> :: <%d> pinctrl DEselected\n",
			__func__, __LINE__);
	}
	return 0;
}
#else
int asc_pad_config(asc_ctrl_t *asc_ctrl_p, bool release)
{
	struct platform_device *pdev = asc_ctrl_p->device.pdev;
	struct stm_plat_asc_data *plat_data = pdev->dev.platform_data;
	if (release) {
		asc_debug_print(" <%s> :: <%d> pad released\n",
				__func__, __LINE__);
		if (asc_ctrl_p->device.pad_state) {
			stm_pad_release(asc_ctrl_p->device.pad_state);
			asc_ctrl_p->device.pad_state = NULL;
		} else
			asc_error_print(" <%s> :: <%d> No Pad claimed\n",
					__func__, __LINE__);
	} else {
		asc_debug_print(" <%s> :: <%d> pad claimed\n",
				__func__, __LINE__);
		asc_ctrl_p->device.pad_state =
			stm_pad_claim(plat_data->pad_config, "asc");
		if (NULL == asc_ctrl_p->device.pad_state) {
			asc_error_print(" <%s> :: <%d> Pad claim failed\n",
					__func__, __LINE__);
			return -EBUSY;
		}
	}
	return 0;
}
#endif

void asc_store_reg_set(asc_regs_t *asc_regs_src_p, asc_regs_t *asc_regs_des_p)
{
	asc_debug_print(" <%s> :: <%d> ASC store registers\n",
			__func__, __LINE__);
	asc_regs_des_p->asc_control = ASC_READ_CONTROL(asc_regs_src_p);
	asc_regs_des_p->asc_intenable = ASC_READ_INTENABLE(asc_regs_src_p);
	asc_regs_des_p->asc_guardtime = ASC_READ_GUARDTIME(asc_regs_src_p);
	asc_regs_des_p->asc_timeout = ASC_READ_TIMEOUT(asc_regs_src_p);
	asc_regs_des_p->asc_retries = ASC_GET_RETRIES(asc_regs_src_p);
	asc_regs_des_p->asc_threshold = ASC_READ_THRESHOLD(asc_regs_src_p);
	ASC_RESET_TXFIFO(asc_regs_src_p);
	ASC_RESET_RXFIFO(asc_regs_src_p);
	asc_debug_print("ctrl=%x inten=%x Guardtime=%x retries=%x\n",
			asc_regs_des_p->asc_control,
			asc_regs_des_p->asc_intenable,
			asc_regs_des_p->asc_guardtime,
			asc_regs_des_p->asc_retries);

	/* No need to save remaining register as they are not consistent*/
}

void asc_load_reg_set(asc_regs_t *asc_regs_src_p, asc_regs_t *asc_regs_des_p)
{
	asc_debug_print(" <%s> :: <%d> ASC load registers\n",
			__func__, __LINE__);
	asc_debug_print("ctrl=%x inten=%x Guardtime=%x retries=%x\n",
			asc_regs_src_p->asc_control,
			asc_regs_src_p->asc_intenable,
			asc_regs_src_p->asc_guardtime,
			asc_regs_src_p->asc_retries);
	ASC_RESET_TXFIFO(asc_regs_src_p);
	ASC_RESET_RXFIFO(asc_regs_src_p);
	ASC_SET_CONTROL(asc_regs_des_p, asc_regs_src_p->asc_control);
	ASC_ENABLE_INTS(asc_regs_des_p, asc_regs_src_p->asc_intenable);
	ASC_SETGUARDTIME(asc_regs_des_p, asc_regs_src_p->asc_guardtime);
	ASC_SET_TIMEOUT(asc_regs_des_p, ASC_TIMEOUT_VALUE);
	ASC_SET_RETRIES(asc_regs_des_p, asc_regs_src_p->asc_retries);
	ASC_SET_THRESHOLD(asc_regs_des_p, asc_regs_src_p->asc_threshold);
}
