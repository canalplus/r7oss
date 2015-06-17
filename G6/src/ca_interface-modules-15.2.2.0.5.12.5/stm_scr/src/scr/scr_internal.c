#ifdef CONFIG_ARCH_STI
#include <linux/platform_device.h>
#include <linux/gpio.h>
#else
#include <linux/stm/pio.h>
#include <linux/stm/pad.h>
#include <linux/stm/gpio.h>
#include <linux/stm/platform.h>
#endif
#include <linux/clk.h>
#include <linux/ioport.h>
#include <linux/pm_runtime.h>
#include "ca_platform.h"

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"
#include "scr_utils.h"
#include "scr_debugfs.h"
#include "scr_protocol_t0.h"
#include "scr_protocol_t1.h"

#include "stm_asc_scr.h"
#include "scr_plugin.h"

extern struct scr_platform_data_s scr_data[];
extern ca_os_semaphore_t g_scr_sem_lock;

static int thread_dmx_scr[2] = { SCHED_RR, 85 };
module_param_array_named(thread_DMX_Scr,thread_dmx_scr, int, NULL, 0644);
MODULE_PARM_DESC(thread_DMX_Scr, "DMX-Scr thread:s(Mode),p(Priority)");

extern int sti_scr_set_config(struct stm_scr_platform_data *data, int id);
int32_t scr_internal_new(uint32_t scr_dev_id,
			stm_scr_device_type_t device_type,
			stm_scr_h *handle_p)
{
	int error_code = 0;
	scr_ctrl_t *scr_handle_p = NULL;
	char scr_thread_name[20];

	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);

	if (device_type == STM_SCR_DEVICE_CA)
		if (!scr_ca_populate_init_parameters)
			return -EPERM;

	if (!(scr_data[scr_dev_id].base_address &&
		(scr_data[scr_dev_id].init == false))) {

		scr_error_print("<%s> :: <%d> Already initialized\n",
				__func__, __LINE__);
		return -EBUSY;
	}
	scr_handle_p = (scr_ctrl_t *)ca_os_malloc(sizeof(scr_ctrl_t));
	if (!scr_handle_p) {
		scr_error_print("<%s> :: <%d> Null scr_handle_p\n",
				__func__, __LINE__);
		return -ENOMEM;
	}
	error_code = ca_os_sema_wait(g_scr_sem_lock);
	if (error_code != 0) {
		ca_os_free(scr_handle_p);
		scr_error_print("<%s:%d> Init could not be completed\n",
				__func__, __LINE__);
		return error_code;
	}
	scr_handle_p->magic_num = 0xababab00 | scr_dev_id;

	scr_get_ca_ops(&scr_handle_p->ca_ops);

	scr_handle_p->scr_plat_data = &scr_data[scr_dev_id];
#ifdef CONFIG_ARCH_STI
	scr_handle_p->pinctrl =	scr_data[scr_dev_id].scr_platform_data->pinctrl;
	scr_handle_p->class_sel_gpio =
		scr_data[scr_dev_id].scr_platform_data->class_sel_gpio;
	scr_handle_p->reset_gpio =
		scr_data[scr_dev_id].scr_platform_data->reset_gpio;
	scr_handle_p->vcc_gpio =
		scr_data[scr_dev_id].scr_platform_data->vcc_gpio;
	scr_handle_p->detect_gpio =
		scr_data[scr_dev_id].scr_platform_data->detect_gpio;
#else
	scr_handle_p->pad_config =
		scr_data[scr_dev_id].scr_platform_data->pad_config;
	scr_handle_p->class_sel_pad_config =
		scr_data[scr_dev_id].scr_platform_data->class_sel_pad;
	scr_handle_p->device_state =
		scr_data[scr_dev_id].device_state;
#endif
	scr_handle_p->asc_fifo_size =
		scr_data[scr_dev_id].scr_platform_data->asc_fifo_size;
	scr_data[scr_dev_id].init = true;
	scr_handle_p->scr_caps.device_type = device_type;
	scr_handle_p->card_present = false;

	/*Setting the states for the SCR thread */
	scr_handle_p->state = SCR_THREAD_STATE_IDLE;

	scr_handle_p->base_address_physical =
		scr_data[scr_dev_id].base_address->start;
	scr_handle_p->base_address_virtual =
		ioremap_nocache(scr_data[scr_dev_id].base_address->start,
				(scr_data[scr_dev_id].base_address->end -
				scr_data[scr_dev_id].base_address->start + 1));

	if (!scr_handle_p->base_address_virtual) {
		scr_error_print("Memory mapping failed\n");
		error_code = -1;
		goto error_iomap;
	}
#ifdef CONFIG_ARCH_STI
	error_code = sti_scr_set_config(
			scr_handle_p->scr_plat_data->scr_platform_data,
			scr_data[scr_dev_id].id);
	if(error_code)
		goto error_asc;
#endif
	platform_set_drvdata(scr_handle_p->scr_plat_data->pdev, scr_handle_p);
	pm_runtime_get_sync(&(scr_handle_p->scr_plat_data->pdev->dev));

	scr_gpio_direction(scr_handle_p);

	error_code = scr_asc_new(scr_handle_p);
	if (error_code)
		goto error_asc;

	scr_populate_clock_default(scr_handle_p);
	scr_populate_default(scr_handle_p);

	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);
	error_code = ca_os_message_q_initialize(&scr_handle_p->message_q);
	if (error_code)
		goto error_message_q;
	scr_debug_print("<%s> :: <%d> %p\n",
			__func__, __LINE__, scr_handle_p->message_q);

	/* initialising the gpio */
	error_code = scr_initialise_pio(scr_handle_p);
	if (error_code) {
		scr_error_print("<%s> :: <%d> %d\n",
				__func__, __LINE__, error_code);
		goto error_gpio;
	}

#ifdef CONFIG_ARCH_STI
	scr_handle_p->scr_caps.class_sel_gpio_value =
		scr_internal_read_class_sel_pad(scr_handle_p);
	scr_debug_print("<%s> :: < %d> Initial class sel pad value %d\n",
				__func__, __LINE__,
			(uint8_t)scr_handle_p->scr_caps.class_sel_gpio_value);
#else
	if (scr_handle_p->class_sel_pad_state) {
		/* set the actual value */
		scr_handle_p->scr_caps.class_sel_pad_value =
			scr_internal_read_class_sel_pad(scr_handle_p);
		scr_debug_print("<%s> :: < %d> Initial class sel"\
				"pad value %d\n",
				__func__, __LINE__,
		(uint8_t)scr_handle_p->scr_caps.class_sel_pad_value);
	}
#endif

	sprintf(scr_thread_name, "%s-%d", "DMX-Scr", scr_dev_id);
	error_code = ca_os_thread_create(&scr_handle_p->thread,
				scr_thread,
				scr_handle_p,
				(const char *)scr_thread_name,
				thread_dmx_scr);
	if (error_code != 0) {
		scr_error_print("<%s> :: <%d> thread create failed\n",
				__func__, __LINE__);
		goto error_thread;
	}
	scr_send_message_and_wait(scr_handle_p,
				SCR_THREAD_STATE_IDLE,
				SCR_PROCESS_CMD_INVALID,
				NULL,
				false);

#ifdef CONFIG_PROC_FS
	error_code = scr_create_procfs(scr_dev_id, scr_handle_p);
	if (error_code) {
		scr_error_print("<%s> :: <%d> %d\n",
				__func__, __LINE__, error_code);
		goto error_procfs;
	}
#endif

	*handle_p = scr_handle_p;

	/* Still need to review this for the spike */
	scr_internal_deactivate(scr_handle_p);

	scr_debug_print(" <%s>::<%d> Every thing ok\n",
				__func__, __LINE__);

	ca_os_sema_signal(g_scr_sem_lock);
	return 0;

#ifdef CONFIG_PROC_FS
	error_procfs:
		scr_send_message_and_wait(scr_handle_p,
			SCR_THREAD_STATE_DELETE,
			SCR_PROCESS_CMD_INVALID,
			NULL,
			false);
#endif
	error_thread:
		scr_delete_pio(scr_handle_p);

	error_gpio:
		ca_os_message_q_terminate(scr_handle_p->message_q);

	error_message_q:
		scr_asc_delete(scr_handle_p);

	error_asc:
		iounmap(scr_handle_p->base_address_virtual);

	error_iomap:
		scr_handle_p->magic_num=0;
		ca_os_free(scr_handle_p);
		scr_handle_p = NULL;
		scr_data[scr_dev_id].init = false;
		ca_os_sema_signal(g_scr_sem_lock);
		return error_code;
}

int32_t scr_internal_delete(scr_ctrl_t *scr_handle_p)
{
	int error_code = 0;
	scr_debug_print("<%s> :: <%d> for %d\n",
		__func__, __LINE__, SCR_NO(scr_handle_p->magic_num));

	error_code = ca_os_sema_wait(g_scr_sem_lock);
	if (error_code != 0) {
		scr_error_print("<%s:%d> Init could not be completed\n",
			__func__, __LINE__);
		return error_code;
	}

	error_code = scr_asc_abort(scr_handle_p);

	scr_send_message_and_wait(scr_handle_p,
			SCR_THREAD_STATE_DELETE,
			SCR_PROCESS_CMD_INVALID,
			NULL,
			false);

	ca_os_wait_thread(&(scr_handle_p->thread));

	error_code = scr_asc_delete(scr_handle_p);
	if (error_code) {
		scr_error_print("<%s> :: <%d> error\n", __func__, __LINE__);
		goto error;
	}

	error_code = scr_internal_deactivate(scr_handle_p);
	if (error_code) {
		scr_error_print("<%s> :: <%d> error\n", __func__, __LINE__);
		goto error;
	}

	error_code = scr_delete_pio(scr_handle_p);
	if (error_code) {
		scr_error_print("<%s> :: <%d> error\n", __func__, __LINE__);
		goto error;
	}

	scr_data[SCR_NO(scr_handle_p->magic_num)].init = false;

	ca_os_message_q_terminate(scr_handle_p->message_q);
	scr_handle_p->message_q = NULL;

	iounmap(scr_handle_p->base_address_virtual);
#ifdef CONFIG_PROC_FS
	scr_delete_procfs(SCR_NO(scr_handle_p->magic_num), scr_handle_p);
#endif
	scr_handle_p->magic_num=0;

	platform_set_drvdata(scr_handle_p->scr_plat_data->pdev, NULL);
	pm_runtime_put_sync(&(scr_handle_p->scr_plat_data->pdev->dev));

	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);


	ca_os_free(scr_handle_p);
	scr_handle_p = NULL;

	error:
	ca_os_sema_signal(g_scr_sem_lock);
	return error_code;
}

int32_t scr_internal_deactivate(scr_ctrl_t *scr_handle_p)
{
	struct scr_ca_param_s *ca_param_p = &scr_handle_p->ca_param;
	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);
#ifdef CONFIG_ARCH_STI
	gpio_set_value(scr_handle_p->reset_gpio, 0);
#else
	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_RESET_GPIO].gpio, 0);
#endif
	if (ca_param_p->rst_low_to_clk_low_delay)
		ca_os_sleep_usec(0, ca_param_p->rst_low_to_clk_low_delay);

	writel((readl(scr_handle_p->base_address_virtual +
			SCR_CLOCK_CONTROL_OFFSET) &
			(~SCR_CLOCK_ENABLE_MASK)),
			(scr_handle_p->base_address_virtual +
			SCR_CLOCK_CONTROL_OFFSET));
#ifdef CONFIG_ARCH_STI
	gpio_set_value(scr_handle_p->vcc_gpio, 1);
#else
	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_VCC_GPIO].gpio, 1);
#endif
	ca_os_sleep_milli_sec(1);
	scr_handle_p->reset_done = false;
	return 0;
}

int32_t scr_internal_reset(scr_ctrl_t *scr_handle_p, uint32_t reset_type)
{
	int32_t error_code = 0;
	struct scr_ca_param_s *ca_param_p = &scr_handle_p->ca_param;

	scr_populate_default(scr_handle_p);

	if (scr_set_asc_ctrl(scr_handle_p)) {
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	error_code = scr_asc_flush(scr_handle_p);
	if (error_code)
		goto reset_error;

	if (STM_SCR_RESET_COLD == reset_type)
		scr_internal_deactivate(scr_handle_p);

	/* reset sequence to be added*/
	if (reset_type == STM_SCR_RESET_COLD)
		scr_cold_reset(scr_handle_p);
	else
		scr_warm_reset(scr_handle_p);
	error_code = scr_read_atr(scr_handle_p);
	if (error_code)
		goto reset_error;

	scr_debug_print("ATR reading done %d\n"\
			"ATR data",
			scr_handle_p->atr_response.atr_size);
	{
		int i;
		for (i = 0; i < scr_handle_p->atr_response.atr_size; i++) {
			scr_debug_print(" %x \t",
			scr_handle_p->atr_response.raw_data[i]);
		}
	}
	scr_debug_print("\n ATR data dump done\n");
	error_code = scr_process_atr(&scr_handle_p->atr_response);
	if (error_code)
		goto reset_error;

	/* TCK character should also be present only if list of
	* supported protocol types is not t=0 by itself.
	*/
	if (!(scr_handle_p->atr_response.parsed_data.supported_protocol == 1)) {
		if(!(scr_read_tck(scr_handle_p))) {
			if (scr_validate_tck(&scr_handle_p->atr_response) != 0)	{
				error_code = -1;
				goto reset_error;
			}
		}
	}

	/*Extra ATR bytes availability depends on the smart card and it is an optional 
	  feature.Thus,no error checking needed*/
	if (scr_handle_p->atr_response.extra_bytes_in_atr)
		scr_read_extrabytes(scr_handle_p);

	scr_handle_p->scr_caps.history	=
		scr_handle_p->atr_response.parsed_data.history_bytes_p;
	scr_handle_p->scr_caps.size_of_history_bytes =
		scr_handle_p->atr_response.parsed_data.size_of_history_bytes;

#ifdef CONFIG_PROC_FS
	scr_dump_atr(scr_handle_p->atr_response.parsed_data);
#endif
	scr_update_atr_parameters(scr_handle_p);

	if (scr_set_nack(scr_handle_p, scr_handle_p->current_protocol))
		goto reset_error;

	if (ca_param_p->reset_to_write_delay)
		ca_os_sleep_milli_sec(ca_param_p->reset_to_write_delay);

	return 0;

	reset_error :
		scr_handle_p->last_status=error_code;
		return error_code;

}
#ifdef CONFIG_ARCH_STI
void scr_internal_change_class(scr_ctrl_t *scr_handle_p)
{
	gpio_set_value(scr_handle_p->class_sel_gpio,
			 !(gpio_get_value(scr_handle_p->class_sel_gpio)));
}

uint8_t scr_internal_read_class_sel_pad(scr_ctrl_t *scr_handle_p)
{
	return(gpio_get_value(scr_handle_p->class_sel_gpio));
}
#else
void scr_internal_change_class(scr_ctrl_t *scr_handle_p)
{
	struct stm_pad_config *class_sel_pad;
	class_sel_pad =
		scr_handle_p->class_sel_pad_config;

	gpio_set_value(class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio,
	!(gpio_get_value(class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio)));
}

uint8_t scr_internal_read_class_sel_pad(scr_ctrl_t *scr_handle_p)
{
	struct stm_pad_config *class_sel_pad;
	class_sel_pad =
		scr_handle_p->class_sel_pad_config;

	return(gpio_get_value(
		class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio));
}
#endif
int32_t scr_internal_pps(scr_ctrl_t *scr_handle_p, uint8_t *data_p)
{
	uint32_t timeout, count;
	uint8_t PTSx = 0, PTS0 = 0, no_of_bit_set = 0;

	scr_pps_data_t *pps_data = &scr_handle_p->pps_data;
	/* store data in the capabilities for future reference */
	scr_handle_p->scr_caps.pts_bytes =
		&pps_data->pps_response[SCR_PPS0_INDEX];
	pps_data->pps_request[0] = 0xFF;
	pps_data->pps_size = scr_no_of_bits_set(*data_p & 0x70) + 1;
	scr_debug_print(" <%s> : <%d> no of bytes %d data %x\n",
		__func__, __LINE__, pps_data->pps_size, *data_p);

	memcpy(&pps_data->pps_request[1], data_p, pps_data->pps_size);

	pps_data->pps_size += 1;

	pps_data->pps_request[pps_data->pps_size] =
			scr_calculate_xor(pps_data->pps_request,
			pps_data->pps_size);

	pps_data->pps_size += 1; /*PPSS and PCK bytes*/

	scr_debug_print(" <%s> : <%d> the PPS data xored %d\n",
			__func__, __LINE__,
			pps_data->pps_request[pps_data->pps_size]);

	if ((signed int)scr_asc_flush(scr_handle_p)) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}

	timeout = SCR_TX_TIMEOUT(scr_handle_p->scr_caps.work_waiting_time,
			scr_handle_p->N,
			scr_handle_p->scr_caps.baud_rate,
			0);

	scr_debug_print("PPS command data\n");
	for (count = 0; count << pps_data->pps_size; count++)
		scr_debug_print("%x ", pps_data->pps_request[count]);
	scr_debug_print("\n time out for sending PPS %d\n", timeout);

	if ((signed int)scr_asc_write(scr_handle_p, pps_data->pps_request,
				SCR_PPS_SIZE,
				pps_data->pps_size,
				timeout) < 0) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	timeout = SCR_RX_TIMEOUT(scr_handle_p->scr_caps.work_waiting_time,
			scr_handle_p->scr_caps.baud_rate,
			0,
			scr_handle_p->wwt_tolerance);

	scr_debug_print("timeout for receiving PPS response %d\n", timeout);

	/* Receive PPSS and PTSO first 2 bytes */
	if ((signed int)scr_asc_read(scr_handle_p,
					&pps_data->pps_response[PTSx],
					2,
					2,
					timeout,
					false) < 0) {
			scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
			return -EIO;
	}
	PTS0 = pps_data->pps_response[1];
	no_of_bit_set = scr_no_of_bits_set(PTS0 & 0x70);
	PTSx = 2;
	while (PTSx < (3 + no_of_bit_set)) {
		if ((signed int)scr_asc_read(scr_handle_p,
					&pps_data->pps_response[PTSx],
					1,
					1,
					timeout,
					false) < 0) {
			scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
			return -EIO;
		}
		/* Process next PTS byte */
		PTSx++;
	}
	pps_data->pps_size = PTSx;

	if (scr_process_pps_response(pps_data)) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}

	scr_update_pps_parameters(scr_handle_p, pps_data->pps_info);

	scr_debug_print("PPS response data of size %d\n",
			pps_data->pps_size);
	for (count = 0; count < pps_data->pps_size; count++)
		scr_debug_print("%x ", pps_data->pps_response[count]);
	scr_debug_print("\n");

	/* store size in the capabilities for future reference */
	/* Do not store the PPSS and PCK */
	scr_handle_p->scr_caps.pps_size = pps_data->pps_size - 2;
	return 0;
}

int32_t scr_internal_read(scr_ctrl_t *scr_handle_p,
			stm_scr_transfer_params_t *trans_params_p)
{
	uint32_t byte_read = 0, error_code = 0;

	if (scr_handle_p->current_protocol == STM_SCR_PROTOCOL_T14) {
		if (scr_ca_read) {
			byte_read = scr_ca_read(scr_handle_p,
			trans_params_p->response_p,
			trans_params_p->response_buffer_size,
			trans_params_p->response_buffer_size,
			trans_params_p->timeout_ms,
			false);
			if ((signed int)byte_read < 0) {
				scr_error_print(" <%s>::<%d> Failed %d\n",
						__func__,
						__LINE__,
						byte_read);

				error_code = byte_read;
			} else
				trans_params_p->actual_response_size =
					byte_read;
		} else
			error_code = -EINVAL;
	} else {
		byte_read=scr_asc_read(scr_handle_p,
				trans_params_p->response_p,
				trans_params_p->response_buffer_size,
				trans_params_p->response_buffer_size,
				trans_params_p->timeout_ms,
				false);

		if ((signed int)byte_read < 0) {
			scr_error_print(" <%s>::<%d> Failed %d\n",
					__func__, __LINE__, byte_read);
			error_code = byte_read;
		}
		else
			trans_params_p->actual_response_size = byte_read;
	}
	return error_code;
}

int32_t scr_internal_write(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t * trans_params_p)
{
	uint32_t error_code = 0, byte_writen = 0;

	if ((signed int)scr_asc_flush(scr_handle_p)) {
		scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}

	byte_writen = scr_asc_write(scr_handle_p,
			trans_params_p->buffer_p,
			trans_params_p->buffer_size,
			trans_params_p->buffer_size,
			trans_params_p->timeout_ms);

	if ((signed int)byte_writen < 0) {
		scr_error_print(" <%s>::<%d> Failed %d\n",
					__func__, __LINE__, byte_writen);
		error_code=byte_writen;
	} else
		trans_params_p->actual_response_size = byte_writen;

	return error_code;
}

int32_t scr_internal_transfer(scr_ctrl_t *scr_handle_p,
				stm_scr_transfer_params_t *trans_params_p)
{
	uint32_t error = 0;
	scr_debug_print(" <%s> :: <%d> Current protocol %d\n",
					__func__,
					__LINE__,
					scr_handle_p->current_protocol);
	switch (scr_handle_p->current_protocol)
	{
		case STM_SCR_PROTOCOL_T0:
			error = scr_transfer_t0(scr_handle_p, trans_params_p);
			break;

		case STM_SCR_PROTOCOL_T1:
			error = scr_transfer_t1(scr_handle_p, trans_params_p);
			break;

		case STM_SCR_PROTOCOL_T14:
			error = scr_transfer_t14(scr_handle_p, trans_params_p);
			break;

		default:
			scr_error_print(" <%s>::<%d> wrong protocol "\
			"selection %d\n",
			__func__,
			__LINE__,
			scr_handle_p->current_protocol);
			break;
	}

	return error;
}

int32_t scr_internal_get_ctrl(scr_ctrl_t *scr_handle_p,
				stm_scr_ctrl_flags_t ctrl_flags,
				uint32_t *value_p)
{
	switch (ctrl_flags) {

	case STM_SCR_CTRL_BAUD_RATE:
		*value_p = scr_handle_p->scr_caps.baud_rate;
	break;

	case STM_SCR_CTRL_WORKING_CLOCK_FREQUENCY:
	*value_p = scr_handle_p->scr_caps.working_frequency;
	break;
#ifdef CONFIG_ARCH_STI
	case STM_SCR_CTRL_CONTACT_VCC:
		*value_p = __gpio_get_value(scr_handle_p->vcc_gpio);
	break;
	case STM_SCR_CTRL_CLASS_SELECTION:
		*value_p = scr_handle_p->scr_caps.class_sel_gpio_value;
	break;

#else
	case STM_SCR_CTRL_CONTACT_VCC:
		*value_p = __gpio_get_value(
			scr_handle_p->pad_config->gpios[SCR_VCC_GPIO].gpio);
	break;
	case STM_SCR_CTRL_CLASS_SELECTION:
		*value_p = scr_handle_p->scr_caps.class_sel_pad_value;
	break;
#endif
	case STM_SCR_CTRL_SET_IFSC:
		*value_p = scr_handle_p->scr_caps.ifsc;
	break;

	case STM_SCR_CTRL_NAD:
		*(uint8_t *)value_p = scr_handle_p->nad;
	break;

	case STM_SCR_CTRL_PARITY_RETRIES:
		stm_asc_scr_get_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES, value_p);
	break;
	case STM_SCR_CTRL_NAK_RETRIES:
		stm_asc_scr_get_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES, value_p);
	break;
	case STM_SCR_CTRL_WWT:
		/*unit is 10msec */
		*value_p = (scr_handle_p->scr_caps.work_waiting_time * 1000)/
				(scr_handle_p->scr_caps.baud_rate * 10);
	break;
	case STM_SCR_CTRL_GUARD_TIME:
		*value_p = scr_handle_p->N;
	break;
	case STM_SCR_CTRL_CONVENTION:
		*value_p = scr_handle_p->scr_caps.bit_convention;
	break;
	case STM_SCR_CTRL_PROTOCOL:
		*value_p = scr_handle_p->current_protocol;
	break;
	case STM_SCR_CTRL_EXTCLK:
		*value_p = scr_handle_p->scr_caps.clock_frequency;
	break;

	case STM_SCR_CTRL_STOP_BITS:
		scr_get_asc_params(scr_handle_p, value_p, ctrl_flags);
	break;

	case STM_SCR_CTRL_DATA_PARITY:
		scr_get_asc_params(scr_handle_p, value_p, ctrl_flags);
	break;

	case STM_SCR_CTRL_DATA_GPIO:
		scr_asc_get_pad_config(scr_handle_p, value_p);
	break;
	}
	scr_debug_print("<%s> :: <%d> value %d\n",
		__func__, __LINE__, *value_p);
	return 0;
}

int32_t scr_internal_set_ctrl(scr_ctrl_t *scr_handle_p,
				stm_scr_ctrl_flags_t ctrl_flag,
				uint32_t value)
{
	uint32_t error = 0;
	void *data_p;
#ifndef CONFIG_ARCH_STI
	struct stm_pad_config *class_sel_pad;
#endif

	switch (ctrl_flag) {
	case STM_SCR_CTRL_BAUD_RATE:
		scr_handle_p->scr_caps.baud_rate = value;
	break;

	case STM_SCR_CTRL_WORKING_CLOCK_FREQUENCY:
		scr_change_clock_frequency(scr_handle_p, value);

	break;
#ifdef CONFIG_ARCH_STI
	case STM_SCR_CTRL_CONTACT_VCC:
		__gpio_set_value(scr_handle_p->vcc_gpio, value);
		if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA
			&& scr_ca_enable_clock){
			scr_ca_enable_clock(scr_handle_p);
		}
	break;
	case STM_SCR_CTRL_CLASS_SELECTION:
		if (value)
			gpio_set_value(scr_handle_p->class_sel_gpio, 1);
		else
			gpio_set_value(scr_handle_p->class_sel_gpio,0);

		scr_handle_p->scr_caps.class_sel_gpio_value = gpio_get_value(
				scr_handle_p->class_sel_gpio);
	break;
#else
	case STM_SCR_CTRL_CONTACT_VCC:
		__gpio_set_value(
		scr_handle_p->pad_config->gpios[SCR_VCC_GPIO].gpio, value);
		if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA
			&& scr_ca_enable_clock){
			scr_ca_enable_clock(scr_handle_p);
		}
	break;
	case STM_SCR_CTRL_CLASS_SELECTION:
		class_sel_pad = scr_handle_p->class_sel_pad_config;
		if (value)
			gpio_set_value(
			class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio,
			1);
		else
			gpio_set_value(
			class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio,
			0);

		scr_handle_p->scr_caps.class_sel_pad_value = gpio_get_value(
			class_sel_pad->gpios[SCR_CLASS_SELECTION_GPIO].gpio);
	break;
#endif
	case STM_SCR_CTRL_SET_IFSC:
		/* send the ifsc block*/
		if (!scr_handle_p->reset_done) {
			scr_error_print("<%s> :: <%d>\n", __func__, __LINE__);
			scr_handle_p->last_error = SCR_ERROR_NOT_RESET;
			return -EPERM;  /* SCR_ERROR_NOT_RESET */
		}
		data_p = (void*)&value;
		scr_send_message_and_wait(scr_handle_p,
			SCR_THREAD_STATE_PROCESS,
			SCR_PROCESS_CMD_IFSC,
			data_p,
			false);

	break;

	case STM_SCR_CTRL_NAD:
		scr_handle_p->nad = (uint8_t)value;
	break;

	case STM_SCR_CTRL_PARITY_RETRIES:
		stm_asc_scr_set_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES, value);
	break;
	case STM_SCR_CTRL_NAK_RETRIES:
		stm_asc_scr_set_control(scr_handle_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES, value);
	break;
	case STM_SCR_CTRL_WWT:
		scr_handle_p->scr_caps.work_waiting_time = (value *
				scr_handle_p->scr_caps.baud_rate * 10)/1000;
	break;
	case STM_SCR_CTRL_GUARD_TIME:
		scr_handle_p->N = value;
		if (scr_handle_p->N == 0 &&
		(scr_handle_p->current_protocol == STM_SCR_PROTOCOL_T1 ||
		scr_handle_p->current_protocol == STM_SCR_PROTOCOL_T14))
			scr_handle_p->N = 1;
		else if(scr_handle_p->N == 0 &&
		scr_handle_p->current_protocol == STM_SCR_PROTOCOL_T0)
			scr_handle_p->N = 2;
	break;

	case STM_SCR_CTRL_CONVENTION:
		return -EPERM;
	break;

	case STM_SCR_CTRL_PROTOCOL:
		scr_handle_p->current_protocol = value;
		return scr_set_nack(scr_handle_p,
			scr_handle_p->current_protocol);
	break;

	case STM_SCR_CTRL_EXTCLK:
		if (value) {
			if (scr_handle_p->scr_plat_data->clk_ext) {
				scr_debug_print("<%s> :: <%d> "
				"scr_handle_p->scr_plat_data->clk_ext=%p \n",
				__func__, __LINE__,
				scr_handle_p->scr_plat_data->clk_ext);
				if (clk_prepare_enable(
					scr_handle_p->scr_plat_data->clk_ext)) {
					scr_error_print("<%s> :: <%d> failed to"
					"enable clock\n", __func__, __LINE__);
					return -EPERM;
				}
				if (clk_set_rate(
					scr_handle_p->scr_plat_data->clk_ext,
					value)) {
					scr_error_print("<%s> :: <%d> failed to"
					"set clock rate\n", __func__, __LINE__);
					return -EPERM;
				}
				scr_debug_print("<%s> :: <%d> "
					"set external clock rate=%d\n",
					__func__, __LINE__, value);

				scr_handle_p->clock_source =
					SCR_CLOCK_SOURCE_EXTERNAL;
				scr_handle_p->scr_caps.clock_frequency = value;
			} else {
				scr_error_print("<%s> :: <%d> "
				"no valid ext clk \n", __func__, __LINE__);
#ifndef CONFIG_ARCH_STI
				scr_error_print("<%s> :: <%d> no valid ext clk "
				"3.4 kernel : DSS clock to be enabled and rate "
				"should be set by the application\n",
				__func__, __LINE__);
#endif

				return -EPERM;
			}
		} else {
			scr_handle_p->clock_source = SCR_CLOCK_SOURCE_COMMS;
			if (scr_handle_p->scr_plat_data->clk)
				scr_handle_p->scr_caps.clock_frequency =
				clk_get_rate(scr_handle_p->scr_plat_data->clk);
			scr_handle_p->reset_frequency_set = false;
		}
	break;

	case STM_SCR_CTRL_STOP_BITS:
		scr_set_asc_params(scr_handle_p, value, ctrl_flag);
	break;

	case STM_SCR_CTRL_DATA_PARITY:
		scr_set_asc_params(scr_handle_p, value, ctrl_flag);
	break;

	case STM_SCR_CTRL_DATA_GPIO:
		if (value & STM_SCR_DATA_GPIO_C4_UART) {
			if ((value & STM_SCR_DATA_GPIO_C7_UART) |
			(value & STM_SCR_DATA_GPIO_C8_UART))
				return -EINVAL;
		} else if (value & STM_SCR_DATA_GPIO_C7_UART) {
			if (value & STM_SCR_DATA_GPIO_C8_UART)
				return -EINVAL;
		}

		error = scr_asc_pad_config(scr_handle_p, value);
		if (error == 0)
			scr_handle_p->asc_gpio = value;
	break;



	}
	return 0;
}

uint32_t scr_internal_send_ifsc(scr_ctrl_t *scr_handle_p, uint8_t data)
{
	uint32_t error = 0;
	error = scr_transfer_sblock(scr_handle_p, SCR_S_IFS_REQUEST, data);
	return error;
}
