#ifdef CONFIG_ARCH_STI
#else
#include <linux/stm/pio.h>
#include <linux/stm/pad.h>
#include <linux/stm/gpio.h>
#include <linux/stm/platform.h>
#endif
#include <linux/clk.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include "stm_ci.h"
#include "ci_internal.h"
#include "ci_utils.h"
#include "ci_hal.h"
#include "ci_debugfs.h"
#include "ci_protocol.h"

ca_error_code_t ci_check_state(stm_ci_t *ci_p, ci_state_t desired_state)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	ci_state_t		cur_state;
	u_long			flags;
	uint8_t			status = 0;

	spin_lock_irqsave(&ci_p->lock, flags);
	cur_state = ci_p->currstate;

	switch (desired_state) {
	case CI_STATE_CARD_NOT_PRESENT:
		/*default state	can come fom any state*/
		break;

	/* from	isr :when card is inserted CNP->CP*/
	case CI_STATE_CARD_PRESENT:
		if ((cur_state != CI_STATE_CARD_NOT_PRESENT) &&\
				(cur_state != CI_STATE_OPEN)) {
			error =	-EINVAL;
		}
		break;
	case CI_STATE_CARD_READY:
		if ((cur_state != CI_STATE_CARD_PRESENT) &&\
			(cur_state != CI_STATE_PROCESS)) {
			error =	-EINVAL;
		}
		break;
	case CI_STATE_PROCESS:
		if (cur_state != CI_STATE_CARD_READY) {
			error =	-EINVAL;
			break;
		}
		/* check for IIR bit */
		if (ci_p->device.ci_caps.cam_type == STM_CI_CAM_DVBCI_PLUS)
		{
			error =	ci_internal_get_status(ci_p, STM_CI_STATUS_IIR,
						&status);
			if (status == BIT_VALUE_HIGH) {
				ci_error_trace(CI_API_RUNTIME,
						"IIR Set:Issue SW Reset %d\n",
						error);
				error =	-EPROTO;
			}
		}
		break;
	case CI_STATE_CLOSE:
		/* Changing the	state to close wothout any state check*/
		break;
	default:
		ci_debug_trace(CI_API_RUNTIME,
				"INVALID CURR STATE %d\n",
				ci_p->currstate);
		error =	-EINVAL;
		break;
	}
	if (error == CA_NO_ERROR)
		ci_p->currstate	= desired_state;

	spin_unlock_irqrestore(&ci_p->lock, flags);
	return error;
}

static int ci_wait_if_status(stm_ci_t *ci_p, uint8_t waitfor, int timeout_hz)
{
	unsigned long timeout;
	unsigned long start;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");

	/* loop	until timeout elapsed */
	start =	jiffies;
	timeout	= jiffies + timeout_hz;

	while (1) {
		/* read	the status and check for error */
		uint8_t	status ;
		ci_read_status_register(ci_p, &status);

		/* if we got the flag , it was successful! */
		if (status & waitfor) {
			ci_debug_trace(CI_INTERNAL_RUNTIME,
				"succeeded timeout:%lu status	= 0x%x\n",
				jiffies	- start, status);
			return 0;
		}
		/* check for timeout */
		if (time_after(jiffies, timeout)) {
			break;
		}
		/* wait	for a bit */
		msleep(1);
	}

	ci_debug_trace(CI_INTERNAL_RUNTIME,
			"failed timeout:%lu\n",
			jiffies - start);

	/* if we get here, we've timed out */
	return -ETIMEDOUT;
}

ca_error_code_t waitforFR(stm_ci_t *ci_p)
{
	uint8_t	status = 0;
	ca_error_code_t error = 0;
	ci_read_status_register(ci_p, &status);

	/* FR bit is set no need to poll */
	if ((status & CI_FR) != 0)
		return error;

	/* FR bit not set wait*/
	if (ci_p->device.ci_caps.cam_type == STM_CI_CAM_DVBCI_PLUS) {
		if (ci_p->ciplus_in_polling == false) {
			/* enable FR and DA not	to miss	DA interrupts */
			ci_debug_trace(CI_INTERNAL_RUNTIME,
					" before sema wait");
			error =	ca_os_sema_wait_timeout(ci_p->ctrl_sem_p,
					ci_p->fr_timeout);
			if (error != CA_NO_ERROR) {
				error =	-ETIMEDOUT;
				return error;
			} else {
				ci_read_status_register(ci_p, &status);
				ci_debug_trace(CI_INTERNAL_RUNTIME,
					"after sema wait:CAM status 0x%x",
						status);
				return error;
			}
		} else {
			ci_debug_trace(CI_INTERNAL_RUNTIME,
					"Polling mode used for CI+");
		}
	}

	/* poll	for FR bit */
	error =	ci_wait_if_status(ci_p, CI_FR, ci_p->fr_timeout);
	return error;
}
ca_error_code_t ci_internal_new(uint32_t dev_id , stm_ci_t *ci_p)
{
	ca_error_code_t	error = CA_NO_ERROR;
	struct ci_hw_data_s	*hw_data_p;
	struct stm_ci_platform_data *platform_data_p;
	struct resource		*base_address_info_p;
	uint32_t	ip_mem_size;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");
	hw_data_p = ci_p->ci_hw_data_p;
	platform_data_p = hw_data_p->platform_data;

	base_address_info_p = hw_data_p->ci_base;
	if (!base_address_info_p) {
		error =	-ENODEV;
		return error;
	}
	ip_mem_size = (base_address_info_p->end	-
				base_address_info_p->start + 1);
	ci_p->hw_addr.base_addr_v = ioremap_nocache(base_address_info_p->start,
				ip_mem_size);
	if (!ci_p->hw_addr.base_addr_v) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"CI base mapping failed\n");
		error =	-ENOMEM;
		goto err1;
	}

	ci_debug_trace(CI_INTERNAL_RUNTIME,
			"ci base start = %x ci base end	= %x\n",
			base_address_info_p->start, base_address_info_p->end);
	/* Map DVBCI EPLD base address */
	if (platform_data_p->epld_en_flag) {
		base_address_info_p = hw_data_p->epld_base;
		ip_mem_size = (base_address_info_p->end	-
				base_address_info_p->start + 1);

		ci_p->hw_addr.epld_addr_v = ioremap_nocache(
				base_address_info_p->start, ip_mem_size);
		if (!ci_p->hw_addr.epld_addr_v) {
			ci_error_trace(CI_INTERNAL_RUNTIME,
					"CI EPLD memory mapping failed\n");
			error =	-ENOMEM;
			goto error_iomap1;
		}
	}

	/* Map DVBCI IO	base address */
	ci_p->hw_addr.attr_addr_v = ci_p->hw_addr.base_addr_v +
					hw_data_p->attribute_mem_offset->start;
	ci_p->hw_addr.io_addr_v	= ci_p->hw_addr.base_addr_v +
					hw_data_p->iomem_offset->start;

	if ((!ci_p->hw_addr.attr_addr_v) || (!ci_p->hw_addr.io_addr_v)) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"CI Attribute and IO memory\
				mapping failed\n");
		error =	-ENOMEM;
		goto error_iomap2;
	}
	ci_debug_trace(CI_INTERNAL_RUNTIME,
			"ci base = %p epld base	= %p io	base = %p\
			attribute base = %p\n",
			ci_p->hw_addr.base_addr_v,
			ci_p->hw_addr.epld_addr_v,
			ci_p->hw_addr.io_addr_v,
			ci_p->hw_addr.attr_addr_v);

	/* check signal	config parametes */
	error = ci_set_vcc_type(platform_data_p, ci_p);
	if (error != CA_NO_ERROR)
		goto error_iomap2;
	error =	ci_create_debugfs(dev_id, ci_p);
	if (error) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"proc entry failed\n");
		goto error_iomap2;
	}
	ci_register_interface_fptr(ci_p);
	error =	ci_hal_slot_init(ci_p);
	if (error) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
			"interface init failed\n");
		goto error_iomap3;
	}
	return error;

error_iomap3:
	ci_delete_debugfs(dev_id, ci_p);
error_iomap2:
	iounmap(ci_p->hw_addr.epld_addr_v);
error_iomap1:
	iounmap(ci_p->hw_addr.base_addr_v);
err1:
	ci_p->magic_num = 0;
	ca_os_free(ci_p);
	return error;
}

void ci_internal_delete(stm_ci_t *ci_p)
{
	if (ci_p->hw_addr.base_addr_v != NULL)
		iounmap(ci_p->hw_addr.base_addr_v);

	if (ci_p->hw_addr.epld_addr_v != NULL)
		iounmap(ci_p->hw_addr.epld_addr_v);

	ci_p->device.cam_present = false;
	ci_p->device.daie_set =	false;
	ci_p->device.frie_set =	false;
	ci_p->device.link_initialized =	false;
	ci_p->device.cam_voltage = CI_VOLT_5000;
	memset(&ci_p->device.ci_caps, 0x00, sizeof(stm_ci_capabilities_t));

	ci_delete_debugfs(CI_NO(ci_p->magic_num), ci_p);

	ci_hal_slot_shutdown(ci_p);
}

ca_error_code_t ci_internal_card_configure(stm_ci_t	*ci_p)
{
	ca_error_code_t error = 0;
	ci_debug_trace(CI_INTERNAL_RUNTIME, "In\n");

	error =	ci_hal_power_disable(ci_p);

	ca_os_sleep_milli_sec(300);	/*PC Card Spec 2.0 Power down*/
	if (error == CA_NO_ERROR)
		error =	ci_hal_power_enable(ci_p);

	ca_os_sleep_milli_sec(120);	/*PC Card Spec 2.0 Power up and Reset*/
	if (error == CA_NO_ERROR)
		error =	ci_hal_slot_reset(ci_p);

	/* This delay is necessary after hardware reset,
	* value of delay depends
	from module to module. Here we have taken the worst
	case value */
	ca_os_sleep_milli_sec(1500);	/* 1000	ms does	not work MG */
	if (error == CA_NO_ERROR) {
		error =	ci_configure_card(ci_p);
		if (error == CA_NO_ERROR)
			ca_os_sleep_milli_sec(2);
	}
	ci_debug_trace(CI_INTERNAL_RUNTIME, "Out %d\n", error);
	return error;
}

ca_error_code_t ci_internal_link_init(stm_ci_t *ci_p)
{
	ca_error_code_t error = 0;
	stm_ci_msg_params_t params;
	int buf_size;
	uint8_t buf[2], status = 0;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");

	/* Reset the interface and iir bit*/
	error =	ci_internal_soft_reset(ci_p);
	if (error != CA_NO_ERROR) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
			"CAM SOFT Reset	failed\n");
		return error;
	}

	/* read	the buffer size	from the CAM SR=1 */
	ci_write_command_reg(ci_p, CI_SETSR);
	ci_read_status_register(ci_p, &status);

	/* Interrupts not used for negotiation for CI+ modules */
	/* if DA not set */
	if ((status & CI_DA) == 0) {
		/* poll	for DA bit */
		error =	ci_wait_if_status(ci_p, CI_DA, ci_p->da_timeout);
		if (error != CA_NO_ERROR)
			return error;
	}
	params.length =	2;
	params.msg_p = buf;
	error =	ci_read_data(ci_p, &params);
	if (error != CA_NO_ERROR)
		goto out;

	/* Host	resets SR=0 */
	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);

	/* store it, and choose	the minimum of our buffer and
	* the CAM's	buffer size */
	buf_size = (buf[0] << 8) | buf[1];
	buf[0] = buf_size >> 8;
	buf[1] = buf_size & 0xff;

	ci_write_command_reg(ci_p, CI_SETSW);
	ci_read_status_register(ci_p, &status);

	/* if FR not set then wait*/
	if ((status & CI_FR) == 0) {
		/*poll for FR bit */
		error = ci_wait_if_status(ci_p, CI_FR, ci_p->fr_timeout);
		if (error != CA_NO_ERROR)
			goto out;
	}

	ci_write_command_reg(ci_p, CI_SETHC|CI_SETSW);
	ci_read_status_register(ci_p, &status);

	/* CAM has already some	data available */
	if ((status & CI_DA) != 0) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"CAM DA	bit set\n");
		error =	-EBUSY;
		goto out;
	}

	/* The Host takes control before the transfer */
	ci_read_status_register(ci_p, &status);

	/* if FR not set then wait*/
	if ((status & CI_FR) == 0) {
		/*poll for FR bit */
		error =	ci_wait_if_status(ci_p, CI_FR, ci_p->fr_timeout);
		if (error != CA_NO_ERROR)
			goto out;
	}
	params.length =	2;
	params.msg_p = buf;
	error =	ci_write_data(ci_p, &params);
	if (error != CA_NO_ERROR)
		goto out;

	/* Host	resets SW=0 and HC=0*/
	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);

	ci_p->device.ci_caps.negotiated_buf_size = buf_size;
	ci_debug_trace(CI_INTERNAL_RUNTIME,
			"Chosen link buffer size of %i\n",
			buf_size);

	ca_os_sleep_milli_sec(10);
	ci_read_status_register(ci_p, &status);
	/* if FR not set then wait*/
	if ((status & CI_FR) == 0) {
		/*poll for FR bit */
		error =	ci_wait_if_status(ci_p, CI_FR, ci_p->fr_timeout);
		if (error != CA_NO_ERROR)
			goto out;
	}

	return error;

out:
	ci_p->device.ci_caps.negotiated_buf_size = 0x0;
	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);
	ci_error_trace(CI_INTERNAL_RUNTIME, "Out %d\n", error);
	return error;

}

ca_error_code_t ci_internal_read(stm_ci_t *ci_p,
		stm_ci_msg_params_t *params_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	uint8_t	status = 0;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");
	if (ci_p->device.access_mode != CI_IO_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
				CI_IO_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_IO_ACCESS;
	}

	if (ci_p->device.ci_caps.cam_type == STM_CI_CAM_DVBCI_PLUS) {
		if (ci_p->ciplus_in_polling == true) {
			/* poll	for DA bit */
			error =	ci_wait_if_status(ci_p,
				CI_DA, ci_p->da_timeout);
		} else {
		/* enable DA interrupts	:this shd have enabled
			before read for obvious reasons */
			ci_write_command_reg(ci_p, CI_SETDAIE);

			ci_debug_trace(CI_INTERNAL_RUNTIME,
					"before sema wait");

			error =	ca_os_sema_wait_timeout(
					ci_p->ctrl_sem_p,
					ci_p->da_timeout);

			if (error != CA_NO_ERROR) {
				error =	-ETIMEDOUT;
				ci_read_status_register(ci_p, &status);
				ci_error_trace(CI_INTERNAL_RUNTIME,
						"after sema wait :\
						CAM status 0x%x",
						status);
			} else{
				ci_read_status_register(ci_p, &status);
				ci_debug_trace(CI_INTERNAL_RUNTIME,
						"after sema wait :\
						CAM status 0x%x",
						status);
			}
		}
	} else {
		/* poll	for DA bit */
		error =	ci_wait_if_status(ci_p, CI_DA,
				ci_p->da_timeout);
	}

	if (error == CA_NO_ERROR)
		error =	ci_read_data(ci_p, params_p);

	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);
	/* all bits to 0 disable DAIE*/
	return error;
}

ca_error_code_t ci_internal_read_cis(stm_ci_t *ci_p,
			unsigned int *actual_cis_size,
			unsigned char *cis_buf_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");
	*actual_cis_size = 0;

	/* Check and switch to Attribute access	mode */
	if (ci_p->device.access_mode != CI_ATTRIB_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)
				(&ci_p->ca, CI_ATTRIB_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_ATTRIB_ACCESS;
	}
	ca_os_copy(cis_buf_p, ci_p->device.cis_buf_p,
			ci_p->device.ci_caps.cis_size);

	if (error == CA_NO_ERROR)
		*actual_cis_size = ci_p->device.ci_caps.cis_size;
	return error;
}

ca_error_code_t ci_internal_write(stm_ci_t *ci_p,
		stm_ci_msg_params_t *params_p)
{
	ca_error_code_t error = 0;
	uint8_t	status = 0;

	ci_debug_trace(CI_INTERNAL_RUNTIME, "\n");

	if (ci_p->device.access_mode != CI_IO_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
					CI_IO_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_IO_ACCESS;
	}
	ci_read_status_register(ci_p, &status);

	/* CAM has already some	data available */
	if ((status & CI_DA) != 0) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"CAM DA bit set\n");
		error =	-EBUSY;
		return error;
	}
	/* The Host takes control before the transfer */
	/* No need to enable DAIE now, as data to be read will not be
	* transferred to reader , untilreader reads it from data
	* register */
	if ((ci_p->device.ci_caps.cam_type == STM_CI_CAM_DVBCI_PLUS) &&\
		(ci_p->ciplus_in_polling == false))
		ci_write_command_reg(ci_p, (CI_SETFRIE | CI_SETHC));
	else
		ci_write_command_reg(ci_p, CI_SETHC);

	if (ci_p->device.ci_caps.cam_type == STM_CI_CAM_DVBCI_PLUS) {
		if (ci_p->ciplus_in_polling == true) {
			/*poll for FR bit */
			error =	ci_wait_if_status(ci_p, CI_FR,
					ci_p->fr_timeout);
		} else{
			ci_debug_trace(CI_INTERNAL_RUNTIME,
					" before sema wait");
			error =	ca_os_sema_wait_timeout(
					ci_p->ctrl_sem_p,
					ci_p->fr_timeout);
			if (error != CA_NO_ERROR) {
				error =	-ETIMEDOUT;
				ci_read_status_register(ci_p, &status);
				ci_error_trace(CI_INTERNAL_RUNTIME,
						" Write	Timeout	:\
						CAM status 0x%x",
						status);
				return error;
			} else {
				ci_read_status_register(ci_p, &status);
				ci_debug_trace(CI_INTERNAL_RUNTIME,
						" after	sema wait :\
						CAM status 0x%x",
						status);
			}
		}
	} else {
		/*poll for FR bit */
		error =	ci_wait_if_status(ci_p, CI_FR,
				ci_p->fr_timeout);
	}

	if (error == CA_NO_ERROR)
		error =	ci_write_data(ci_p, params_p);
	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);

	return error;
}

ca_error_code_t ci_internal_get_slot_status(stm_ci_t *ci_p,
		bool *is_present_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	error =	ci_hal_slot_status(ci_p);
	*is_present_p =	ci_p->device.cam_present;
	return error;
}

ca_error_code_t ci_internal_set_command(stm_ci_t *ci_p,
		stm_ci_command_t command,
		uint8_t	value)
{
	ca_error_code_t error = 0;

	if (command != STM_CI_COMMAND_DA_INT_ENABLE) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"wrong command %d\n",
				command);
		return -EINVAL;
	}
	if (ci_p->device.access_mode != CI_IO_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
					CI_IO_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_IO_ACCESS;
	}
	error =	ci_write_command(ci_p, command, value);
	return error;
}

ca_error_code_t ci_internal_get_status(stm_ci_t *ci_p,
		stm_ci_status_t		status,
		uint8_t			*value_p)
{
	ca_error_code_t error = CA_NO_ERROR;

	if ((status < STM_CI_STATUS_IIR) || \
		(status > STM_CI_STATUS_DATA_AVAILABLE)) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"wrong	status type %d\n",
				status);
		return -EINVAL;
	}
	if (ci_p->device.access_mode != CI_IO_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
					CI_IO_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_IO_ACCESS;
	}
	error =	ci_read_status(ci_p, status, value_p);
	return error;
}

ca_error_code_t ci_internal_soft_reset(stm_ci_t *ci_p)
{
	uint8_t	status = 0;
	uint32_t count = 0;
	ca_error_code_t error = CA_NO_ERROR;
	if (ci_p->device.access_mode != CI_IO_ACCESS) {
		if (ci_p->ca.switch_access_mode != NULL) {
			error =	(*ci_p->ca.switch_access_mode)(&ci_p->ca,
					CI_IO_ACCESS);
			if (error != CA_NO_ERROR)
				return error;
		}
		ci_p->device.access_mode = CI_IO_ACCESS;
	}
	/* Make	the Reset bit high */
	ci_write_command_reg(ci_p, CI_SETRS);

	/* delay increased to 10 ms as per the specs */
	ca_os_sleep_milli_sec(10);

	/* Make	the Reset bit low */
	ci_write_command_reg(ci_p, CI_RESET_COMMAND_REG);

	/* Get the Status register value */
	ca_os_sleep_milli_sec(10);

	ci_read_status_register(ci_p, &status);

	if ((status & CI_FR) == CI_FR) {
		ci_debug_trace(CI_INTERNAL_RUNTIME,
				"After Reset, FR set\n");
		return CA_NO_ERROR;
	}
	/* if FR not set then wait*/
	/* Verify and retry to set the FR bit */
	for (count = 0;	count <	CI_MAX_RESET_RETRY; count++) {
		/* Return if there is no module	*/
		if (ci_p->currstate == CI_STATE_CARD_NOT_PRESENT) {
			ci_error_trace(CI_INTERNAL_RUNTIME,
					"No card present\n");
			return -EPERM;
		}
		if (!(status & CI_FR)) {
			ca_os_sleep_milli_sec(1);
			ci_read_status_register(ci_p, &status);
		} else {
			ci_debug_trace(CI_INTERNAL_RUNTIME,
					"Status FR bit set = 0x%x\n",
					status);
			break;
		}
	}
	if (count == CI_MAX_RESET_RETRY) {
		ci_error_trace(CI_INTERNAL_RUNTIME,
				"Soft Reset failed\n");
		error =	-EIO;
	}
	return error;
}

