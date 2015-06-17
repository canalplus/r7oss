#ifdef CONFIG_ARCH_STI
#include <linux/gpio.h>
#include <linux/platform_device.h>
#else
#include <linux/stm/gpio.h>
#include <linux/stm/pio.h>
#include <linux/stm/pad.h>
#endif
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/clk.h>

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_data_parsing.h"
#include "scr_io.h"
#include "scr_utils.h"
#include "ca_platform.h"

/******************************************************************************/
/* Private types                                                              */
/******************************************************************************/
/* Fi and Fmax Table as per ISO 7816-3 : 8.3 */
static uint32_t fi_table[16] = {
		372, 372, 558, 744, 1116, 1488, 1860, FiRFU,
		FiRFU, 512, 768, 1024, 1536, 2048, FiRFU, FiRFU};

static uint32_t fmax_table[16] = {
		4000000, 5000000, 6000000, 8000000, 12000000,
		16000000, 20000000, FmRFU, FmRFU, 5000000,
		7500000, 10000000, 15000000, 20000000,
		FmRFU, FmRFU};

/* Di Table as per ISO 7816-3 : 8.3 */
static uint32_t di_table[16] = {
		DiRFU, 1, 2, 4, 8, 16, 32, 64, 12, 20, DiRFU,
		DiRFU, DiRFU, DiRFU, DiRFU, DiRFU};

int scr_send_message_and_wait(scr_ctrl_t *handle_p,
			scr_thread_state_t next_state,
			scr_process_command_t type,
			void *data_p,
			bool at_front)
{
	int32_t error = 0;
	scr_message_t *scr_message;
	scr_message = (scr_message_t *)ca_os_malloc(sizeof(scr_message_t));

	if (!scr_message)
		return -ENOMEM;
	scr_message->scr_handle_p = handle_p;
	scr_message->next_state = next_state;
	ca_os_sema_initialize(&(scr_message->sema), 0);
	scr_message->type = type;
	scr_message->data_p = data_p;

	if (ca_os_message_q_send(handle_p->message_q,
		(void *)scr_message, at_front)) {
		ca_os_sema_terminate(scr_message->sema);
		ca_os_free(scr_message);
		return -EINVAL;
	}

	error = ca_os_sema_wait(scr_message->sema);
	if (error) {
		scr_asc_abort(handle_p);
		/* Wait till we get the actual resource (sema get signalled) */
		while (ca_os_sema_wait(scr_message->sema) == -EINTR);
	}
	ca_os_sema_terminate(scr_message->sema);
	ca_os_free(scr_message);
	return 0;
}

int scr_send_message(scr_ctrl_t *handle_p,
			scr_thread_state_t next_state,
			scr_process_command_t type,
			void *data_p,
			bool at_front)
{
	scr_message_t *scr_message;
	scr_message = (scr_message_t *)ca_os_malloc(sizeof(scr_message_t));
	if (!scr_message)
		return -ENOMEM;
	scr_message->scr_handle_p = handle_p;
	scr_message->next_state = next_state;
	scr_message->type = type;
	scr_message->data_p = data_p;
	scr_message->sema = NULL;

	scr_debug_print("<%s::%d> :: %p\n", __func__, __LINE__,
			handle_p->message_q->sema);
	if (ca_os_message_q_send(handle_p->message_q,
			(void *)scr_message, at_front)) {
		ca_os_free(scr_message);
		return -EINVAL;
	}
	return 0;
}

/* Calculate Baud rate to more accurate value as compare to
	" working_frequency / etu " */
uint32_t scr_calculate_baudrate(uint32_t frequency, uint32_t di, uint32_t fi)
{
	return (frequency * di) / fi;
}

void scr_disable_irq(scr_ctrl_t *scr_handle_p)
{
	int irq;
#ifdef CONFIG_ARCH_STI
	irq = gpio_to_irq(scr_handle_p->detect_gpio);
#else
	irq = gpio_to_irq(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
#endif

	disable_irq_nosync(irq);
}

void scr_enable_irq(scr_ctrl_t *scr_handle_p, uint32_t comp)
{
	int irq;
#ifdef CONFIG_ARCH_STI
	irq = gpio_to_irq(scr_handle_p->detect_gpio);
#else
	irq = gpio_to_irq(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
#endif
	SCR_SET_IRQ_TYPE(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
}

static irqreturn_t scr_detect_handler(int irq, void *dev)
{
	uint32_t pin_value = 0;
	scr_ctrl_t *scr_handle_p = (scr_ctrl_t *)dev;
	 /*send event to scr thread for the insertion or removal of smart card */
	scr_disable_irq(scr_handle_p);

	 /*This cannot be blocking*/
	scr_asc_abort(scr_handle_p);
#ifdef CONFIG_ARCH_STI
	pin_value = __gpio_get_value(scr_handle_p->detect_gpio);
#else
	pin_value =
	__gpio_get_value(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
#endif
	scr_handle_p->card_present = scr_card_present(scr_handle_p);
	if (scr_handle_p->card_present) {
		scr_debug_print(" <%s> :: <%d> :: card inserted\n",
			__func__, __LINE__);
		scr_send_message(scr_handle_p,
			SCR_THREAD_STATE_CARD_PRESENT,
			SCR_PROCESS_CMD_INVALID, NULL, true);
	} else {
		scr_debug_print(" <%s> :: <%d> :: card removed\n",
			__func__, __LINE__);
		scr_send_message(scr_handle_p,
			SCR_THREAD_STATE_CARD_NOT_PRESENT,
			SCR_PROCESS_CMD_INVALID, NULL, true);
	}
	scr_enable_irq(scr_handle_p, pin_value);

	return IRQ_HANDLED;
}

bool scr_card_present(scr_ctrl_t *scr_handle_p)
{
	uint8_t pin_value;
#ifdef CONFIG_ARCH_STI
	pin_value = __gpio_get_value(scr_handle_p->detect_gpio);
#else
	pin_value =
	__gpio_get_value(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
#endif
	scr_debug_print(" <%s> :: <%d> pin %d\n",
			__func__, __LINE__, pin_value);

	if (scr_handle_p->detect_inverted)
		return pin_value ? false : true;
	else
		return pin_value ? true : false;
}

uint32_t scr_initialise_interrupt(scr_ctrl_t *scr_handle_p, uint32_t comp)
{
	uint32_t error_code = 0;
	static uint8_t owner[32];
	uint32_t irq;
#ifdef CONFIG_ARCH_STI
	struct platform_device *pdev = scr_handle_p->scr_plat_data->pdev;
	irq = gpio_to_irq(scr_handle_p->detect_gpio);
	sprintf(owner, "%s-%d", "SCR", SCR_NO(scr_handle_p->magic_num));
	SCR_SET_IRQ_TYPE(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	error_code = devm_request_irq(&pdev->dev, irq,
				scr_detect_handler, 0 ,
				(const char *)owner, (void *)scr_handle_p);
#else
	irq = gpio_to_irq(
		scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);

	sprintf(owner, "%s-%d", "SCR", SCR_NO(scr_handle_p->magic_num));
	SCR_SET_IRQ_TYPE(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);

	error_code = request_irq(irq, scr_detect_handler, 0,
			(const char *)owner, (void *)scr_handle_p);
#endif
	if (error_code)
		scr_error_print("<%s> :: <%d> Interrupt request failed\n",
				__func__, __LINE__);
	return error_code;
}
#ifdef CONFIG_ARCH_STI
uint32_t scr_initialise_pio(scr_ctrl_t *scr_handle_p)
{
	uint8_t pin_value;
	int error_code;
	struct platform_device *pdev = scr_handle_p->scr_plat_data->pdev;

	sprintf(scr_handle_p->owner, "%s-%d",
			"SCR", SCR_NO(scr_handle_p->magic_num));
	if (gpio_is_valid(scr_handle_p->class_sel_gpio)) {
		if (devm_gpio_request(&pdev->dev, scr_handle_p->class_sel_gpio, pdev->name)) {
			dev_err(&pdev->dev, "could not request class_sel  gpio\n");
			return -EBUSY;
		}
		gpio_direction_output(scr_handle_p->class_sel_gpio, 1);
	}
	if (gpio_is_valid(scr_handle_p->reset_gpio)) {
		if (devm_gpio_request(&pdev->dev, scr_handle_p->reset_gpio, pdev->name)) {
			dev_err(&pdev->dev, "could not request reset gpio\n");
			goto err_reset;
		}
		gpio_direction_output(scr_handle_p->reset_gpio, 1);
	}
	if (gpio_is_valid(scr_handle_p->vcc_gpio)) {
		if (devm_gpio_request(&pdev->dev, scr_handle_p->vcc_gpio, pdev->name)) {
			dev_err(&pdev->dev, "could not request VCC gpio\n");
			goto err_vcc;
		}
		gpio_direction_output(scr_handle_p->vcc_gpio, 1);
	}
	if (gpio_is_valid(scr_handle_p->detect_gpio)) {
		if (devm_gpio_request(&pdev->dev, scr_handle_p->detect_gpio, pdev->name)) {
			dev_err(&pdev->dev, "could not request detect gpio\n");
			goto err_detect;
		}
		gpio_direction_input(scr_handle_p->detect_gpio);
	}
	pin_value = __gpio_get_value(scr_handle_p->detect_gpio);
	if (pin_value) {
		scr_debug_print("<%s> :: <%d> :: card present\n",
			__func__, __LINE__);
		scr_handle_p->card_present =true;
	}
	error_code = stm_asc_scr_pad_config(scr_handle_p->asc_handle, false);
	if (error_code) {
		scr_error_print("<%s> :: <%d>ASC INIT PAD failed %d\n",
				__func__, __LINE__, error_code);
		goto error_pad;
	}
	if (scr_initialise_interrupt(scr_handle_p, pin_value)) {
		scr_error_print(" <%s> :: <%d> GPIO irq registration failed\n",
				__func__, __LINE__);
	} else
		return 0;
error_pad:
	devm_gpio_free(&pdev->dev, scr_handle_p->detect_gpio);
err_detect:
	devm_gpio_free(&pdev->dev, scr_handle_p->vcc_gpio);
err_vcc:
	devm_gpio_free(&pdev->dev, scr_handle_p->reset_gpio);
err_reset:
	devm_gpio_free(&pdev->dev, scr_handle_p->class_sel_gpio);
	return -EBUSY;
}
#else
uint32_t scr_initialise_pio(scr_ctrl_t *scr_handle_p)
{
	uint8_t pin_value;

	sprintf(scr_handle_p->owner, "%s-%d",
			"SCR", SCR_NO(scr_handle_p->magic_num));
	scr_handle_p->pad_state = stm_pad_claim(scr_handle_p->pad_config,
						scr_handle_p->owner);
	if (NULL == scr_handle_p->pad_state) {
		scr_error_print(" <%s> :: <%d> Pad claim failed\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	if (scr_claim_class_sel_pad(scr_handle_p)) {
		stm_pad_release(scr_handle_p->pad_state);
		return -EBUSY;
	}

	pin_value =
	__gpio_get_value(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
	if (pin_value) {
		scr_debug_print("<%s> :: <%d> :: card present\n",
			__func__, __LINE__);
		scr_handle_p->card_present =true;
	}

	if (scr_initialise_interrupt(scr_handle_p, pin_value)) {
		stm_pad_release(scr_handle_p->class_sel_pad_state);
		stm_pad_release(scr_handle_p->pad_state);
		scr_error_print(" <%s> :: <%d> GPIO irq registration failed\n",
				__func__, __LINE__);
		return -EBUSY;
	}

	return 0;
}
#endif

#ifndef CONFIG_ARCH_STI
int scr_claim_class_sel_pad(scr_ctrl_t *scr_p)
{
	struct stm_pad_config *class_sel_pad;
	char owner[32];

	class_sel_pad = scr_p->class_sel_pad_config;

	sprintf(owner, "%s-%d-%s", "SCR",
		SCR_NO(scr_p->magic_num), "CLASS_CHANGE");
	scr_p->class_sel_pad_state = stm_pad_claim(class_sel_pad,owner);
	if (NULL == scr_p->class_sel_pad_state) {
		scr_error_print("<%s::%d> Pad claim for Class Change failed\n",\
				__func__, __LINE__);
		return -EBUSY;
	}
	return 0;
}
#endif

int scr_set_ca_default_param(scr_ctrl_t *scr_p,
			struct scr_ca_param_s *ca_param_p)
{
	stm_scr_capabilities_t *caps_p = &(scr_p->scr_caps);
	uint32_t ibwt;

	ibwt = ((132 * (CA_OS_GET_TICKS_PER_SEC * (1000 / HZ))) /
			caps_p->baud_rate) + 1;
	ca_param_p->reset_to_write_delay = ibwt; /* in ms */
	ca_param_p->rst_low_to_clk_low_delay = 0; /* in us */
	return 0;
}

#ifdef CONFIG_ARCH_STI
uint32_t scr_delete_pio(scr_ctrl_t *scr_handle_p)
{
	struct platform_device *pdev = scr_handle_p->scr_plat_data->pdev;
	uint32_t irq =
	gpio_to_irq(scr_handle_p->detect_gpio);
	free_irq(irq, (void *)scr_handle_p);
	devm_gpio_free(&pdev->dev, scr_handle_p->detect_gpio);
	devm_gpio_free(&pdev->dev, scr_handle_p->vcc_gpio);
	devm_gpio_free(&pdev->dev, scr_handle_p->reset_gpio);
	devm_gpio_free(&pdev->dev, scr_handle_p->class_sel_gpio);
	return 0;
}
#else
uint32_t scr_delete_pio(scr_ctrl_t *scr_handle_p)
{
	uint32_t irq =
	gpio_to_irq(scr_handle_p->pad_config->gpios[SCR_DETECT_GPIO].gpio);
	free_irq(irq, (void *)scr_handle_p);

	if (scr_handle_p->pad_state) {
		stm_pad_release(scr_handle_p->pad_state);
		scr_handle_p->pad_state = NULL;
	}

	if (scr_handle_p->class_sel_pad_state) {
		stm_pad_release(scr_handle_p->class_sel_pad_state);
		scr_handle_p->class_sel_pad_state = NULL;
	}

	return 0;
}
#endif
uint32_t scr_check_state(scr_ctrl_t *scr_handle_p, uint32_t next_state)
{
	if (SCR_THREAD_STATE_LOW_POWER == scr_handle_p->state)
		return -EINVAL;
	/* Deactivate not allowed in the idle state */
	if ((SCR_THREAD_STATE_DEACTIVATE == next_state &&
		SCR_THREAD_STATE_IDLE == scr_handle_p->state))
		return -EINVAL;

	/* Process not allowed in the idle(Card not present) state */
	if ((SCR_THREAD_STATE_PROCESS ==  next_state &&
		SCR_THREAD_STATE_IDLE == scr_handle_p->state))
		return -EINVAL;

	return 0; /*every thing OK*/
}

/* Legacy code taken from stapi*/
void scr_change_clock_frequency(scr_ctrl_t *scr_handle_p,
				uint32_t requested_clock)
{
	/* Clock divider, temporary frequency, the last error value */
	uint32_t  clk_div, ClkF, LastError;
	/* Temporary, so we get negative values */
	int32_t Error;
	/* Find the divider which gives the frequency closest to that
	 * requested. This is basically done by starting off with the
	 * fastest, then going down until the error starts increasing
	 * instead of decreasing. Turned out to be easiest that way rather
	 * than finding the 'middle' one and checking each side.
	 */
	clk_div = 1;
	LastError = (uint32_t) -1;
	/* Divider register is 5-bits wide, so max 0b11111 => 0x1f */
	while (clk_div <= 0x1f) {
		ClkF = scr_handle_p->scr_caps.clock_frequency / (clk_div * 2);
		/* get abs error */
		Error = ClkF - requested_clock;
		if (Error < 0)
			Error = -Error;

		if (Error < LastError)
			LastError = Error;
		else {
			/* Undo the last increment */
			clk_div--;
			break;
		}
		clk_div++;
	}

	/* Check for clock divider boundary conditions and round accordingly */
	if (clk_div == 0)                    /* Zero not allowed */
		clk_div = 1;                     /* Min clock div */
	else if (clk_div >= (2 << 5))          /* ClkDiv is only 5 bits */
		clk_div = (2 << 5) - 1;              /* Max clock div */

	/* Calculate new smartcard clock frequency */
	scr_handle_p->new_inuse_freq =
			scr_handle_p->scr_caps.clock_frequency / (clk_div * 2);

	/* Set clock divider register */
	writel(clk_div,
		scr_handle_p->base_address_virtual +
		SCR_CLOCK_VALUE_OFFSET);

	scr_debug_print("<%s> clock register value %d  ctrl %d\n", __func__,
		readl(scr_handle_p->base_address_virtual +
				SCR_CLOCK_VALUE_OFFSET),
		readl(scr_handle_p->base_address_virtual +
				SCR_CLOCK_CONTROL_OFFSET));

	/* update working frequency */
	scr_handle_p->scr_caps.working_frequency =
				scr_handle_p->new_inuse_freq;

	/* update the baud rate */
	scr_handle_p->scr_caps.baud_rate =
		scr_calculate_baudrate(
			scr_handle_p->scr_caps.working_frequency,
			scr_handle_p->di,
			scr_handle_p->fi);

	scr_debug_print("<%s> :: <%d> ->changed"\
				"frequency %d\n"\
				"baud rate %d\n"\
				"new working clock freq %d\n"\
				"work etu  %d\n"\
				"clk div %d\n",
				__func__,
				__LINE__,
				scr_handle_p->scr_caps.clock_frequency,
				scr_handle_p->scr_caps.baud_rate,
				scr_handle_p->new_inuse_freq,
				scr_handle_p->scr_caps.work_etu,
				clk_div);
}

void scr_set_reset_clock_frequency(scr_ctrl_t *scr_handle_p)
{
	uint32_t clk_div;
	uint32_t clock_control = (SCR_CLOCK_ENABLE_MASK) |
					scr_handle_p->clock_source;
	/* Calculate new clock divider (round-up to nearest integer) */
	clk_div = (scr_handle_p->scr_caps.clock_frequency +
		(scr_handle_p->scr_caps.baud_rate *
		scr_handle_p->scr_caps.work_etu)) /
		(scr_handle_p->scr_caps.baud_rate *
		scr_handle_p->scr_caps.work_etu * 2);

	/* Check for clock divider boundary conditions and round accordingly */
	if (clk_div == 0) {                    /* Zero not allowed */
		clk_div = 1;                     /* Min clock div */
	} else if (clk_div >= (2 << 5)) {          /* ClkDiv is only 5 bits */
		clk_div = (2 << 5) - 1;              /* Max clock div */
	}

	/* Calculate new smartcard clock frequency */
	scr_handle_p->reset_frequency =
		scr_handle_p->scr_caps.working_frequency =
			scr_handle_p->scr_caps.clock_frequency / (clk_div * 2);

	scr_debug_print("<%s> :: <%d> ->default\n"\
			"frequency %d\n"\
			"baud rate %d\n"\
			"new working clock freq %d\n"\
			"work etu  %d\n"\
			"clk div %d\n",
			__func__,
			__LINE__,
			scr_handle_p->scr_caps.clock_frequency,
			scr_handle_p->scr_caps.baud_rate,
			scr_handle_p->new_inuse_freq,
			scr_handle_p->scr_caps.work_etu,
			clk_div);

	writel(clk_div, scr_handle_p->base_address_virtual +
			SCR_CLOCK_VALUE_OFFSET);

	/* Enable the clock*/
	writel(((readl(scr_handle_p->base_address_virtual +
		SCR_CLOCK_CONTROL_OFFSET) & 0xFC) | clock_control),
		(scr_handle_p->base_address_virtual +
		SCR_CLOCK_CONTROL_OFFSET));

	scr_debug_print("<%s> clock register value %d  ctrl %d\n", __func__,
			readl(scr_handle_p->base_address_virtual +
			SCR_CLOCK_VALUE_OFFSET),
			readl(scr_handle_p->base_address_virtual +
			SCR_CLOCK_CONTROL_OFFSET));
}

#ifdef CONFIG_ARCH_STI
uint32_t scr_cold_reset(scr_ctrl_t *scr_handle_p)
{
	ca_os_sleep_milli_sec(10);
	gpio_set_value(scr_handle_p->vcc_gpio, 0);
	ca_os_sleep_milli_sec(2);
	scr_set_reset_clock_frequency(scr_handle_p);
	gpio_set_value(scr_handle_p->reset_gpio, 0);
	ca_os_sleep_milli_sec(11);
	gpio_set_value(scr_handle_p->reset_gpio, 1);
	return 0;
}
uint32_t scr_warm_reset(scr_ctrl_t *scr_handle_p)
{
	scr_set_reset_clock_frequency(scr_handle_p);
	gpio_set_value(scr_handle_p->reset_gpio, 0);
	ca_os_sleep_milli_sec(11);
	gpio_set_value(scr_handle_p->reset_gpio, 1);
	return 0;
}
#else
uint32_t scr_cold_reset(scr_ctrl_t *scr_handle_p)
{
	/* Delay to allow for minimum 10ms deactivation activation sequence delay */
	ca_os_sleep_milli_sec(10);

	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_VCC_GPIO].gpio, 0);
	ca_os_sleep_milli_sec(2);

	/* Set clock divider register and Enable the clock */
	scr_set_reset_clock_frequency(scr_handle_p);

	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_RESET_GPIO].gpio, 0);

	ca_os_sleep_milli_sec(11);

	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_RESET_GPIO].gpio, 1);
	return 0;
}

uint32_t scr_warm_reset(scr_ctrl_t *scr_handle_p)
{
	/* Set clock divider register and Enable the clock */
	scr_set_reset_clock_frequency(scr_handle_p);
	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_RESET_GPIO].gpio, 0);
	ca_os_sleep_milli_sec(11);
	gpio_set_value(scr_handle_p->pad_config->gpios[SCR_RESET_GPIO].gpio, 1);
	return 0;
}
#endif
uint32_t scr_no_of_bits_set(uint8_t byte)
{
	int count = 0;
	for (count = 0; byte != 0; count++)
		byte &= byte - 1; /* this clears the most LSB set bit*/
	return count;
}

uint8_t scr_calculate_xor(uint8_t *data_p, uint32_t count)
{
	int i;
	uint8_t xor_data = *(data_p++);
	for (i = 1; i < count; i++)
		xor_data ^= *(data_p++);
	return xor_data;
}

static void scr_update_fi_di(scr_ctrl_t *scr_handle_p,
				uint8_t f_int,
				uint8_t d_int)
{
	scr_handle_p->fi  = fi_table[f_int];
	scr_handle_p->di = di_table[d_int];
}

void scr_calculate_etu(uint32_t *etu, uint32_t fi, uint32_t di)
{
	*etu = (fi / di);
}

void scr_update_pps_parameters(scr_ctrl_t *scr_handle_p,
		scr_pps_info_t pps_info)
{
	stm_scr_capabilities_t *scr_caps_p = &scr_handle_p->scr_caps;

	scr_update_fi_di(scr_handle_p, pps_info.f_int, pps_info.d_int);

	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA
			&& scr_ca_update_fi_di)
		scr_ca_update_fi_di(scr_handle_p,
					pps_info.f_int,
					pps_info.d_int);

	scr_handle_p->current_protocol = pps_info.protocol_set;
	scr_caps_p->max_clock_frequency = fmax_table[pps_info.f_int];

	/* Use the same WI from the last ATR */
	scr_caps_p->work_waiting_time = 960 *
				scr_handle_p->atr_response.parsed_data.wi *
				scr_handle_p->di;

	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_ISO)
		scr_calculate_etu(&scr_caps_p->work_etu,
					scr_handle_p->fi,
					scr_handle_p->di);

	else if (scr_ca_recalculate_etu)
		scr_ca_recalculate_etu(scr_handle_p, pps_info.f_int,
					pps_info.d_int);

	/* Set the new baudrate */
	scr_caps_p->baud_rate =
		scr_calculate_baudrate(
			scr_handle_p->scr_caps.working_frequency,
			scr_handle_p->di,
			scr_handle_p->fi);
}

uint32_t scr_read_tck(scr_ctrl_t *scr_handle_p)
{
	uint32_t error_code = 0;
	/* this is applicable for T1 only */
	error_code = scr_asc_read(scr_handle_p,
			scr_handle_p->atr_response.raw_data +
			scr_handle_p->atr_response.atr_size,
			1,
			1,
			(1 * SCR_DEFAULT_TIMEOUT),
			false);
	if ((signed int)error_code < 0)
		scr_error_print("<%s>:<%d>:Tck failed %d\n ",
				__func__, __LINE__, error_code);
	else
		scr_handle_p->atr_response.atr_size++;
	return error_code;
}

uint32_t scr_read_extrabytes(scr_ctrl_t *scr_handle_p)
{
	uint32_t error_code = 0, count = 0;

	for (count = 0; count < scr_handle_p->atr_response.extra_bytes_in_atr;
			count++) {
		error_code = scr_asc_read(scr_handle_p,
				scr_handle_p->atr_response.raw_data +
				scr_handle_p->atr_response.atr_size,
				1,
				1,
				(1 * SCR_DEFAULT_TIMEOUT),
				false);
		if (((int32_t)error_code) < 0) {
			scr_error_print("<%s>:<%d>:ExtraBytes read %d\n",
						__func__, __LINE__, count);
			return error_code;
		} else
			scr_handle_p->atr_response.atr_size++;
	}
	return error_code;
}

void scr_update_atr_parameters(scr_ctrl_t *scr_handle_p)
{
	stm_scr_capabilities_t *scr_caps_p = &scr_handle_p->scr_caps;
	scr_atr_parsed_data_t *atr_data_p =
			&scr_handle_p->atr_response.parsed_data;

	if (atr_data_p->specific_mode) {
		scr_handle_p->current_protocol =
			atr_data_p->specific_protocol_type;
	} else {
		scr_handle_p->current_protocol =
			atr_data_p->first_protocol;

		/* TA2 absent switch to default value till PPS */
		atr_data_p->f_int = SCR_DEFAULT_F;
		atr_data_p->d_int = SCR_DEFAULT_D;
	}
	scr_update_fi_di(scr_handle_p, atr_data_p->f_int, atr_data_p->d_int);

	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA
			&& scr_ca_update_fi_di)
		scr_ca_update_fi_di(scr_handle_p,
				atr_data_p->f_int,
				atr_data_p->d_int);

	if (!atr_data_p->parameters_to_use) {
		scr_caps_p->max_clock_frequency = fmax_table[atr_data_p->f_int];
		scr_caps_p->work_waiting_time = 960 *
					atr_data_p->wi *
					scr_handle_p->di;

		scr_calculate_etu(&scr_caps_p->work_etu,
				scr_handle_p->fi,
				scr_handle_p->di);
	} else {
		scr_caps_p->max_clock_frequency = fmax_table[SCR_DEFAULT_F];
		scr_caps_p->work_waiting_time = 960 *
					atr_data_p->wi *
					SCR_INITIAL_Di;

		scr_calculate_etu(&scr_caps_p->work_etu,
					SCR_INITIAL_Fi,
				/* This is the initial etu value */
					SCR_INITIAL_Di);
	}

	if (scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA
			&& scr_ca_recalculate_etu)
		scr_ca_recalculate_etu(scr_handle_p,
					atr_data_p->f_int,
					atr_data_p->d_int);

	scr_debug_print("work_etu %d\n", scr_caps_p->work_etu);
	/* Set default retries if T0 */
	if (STM_SCR_PROTOCOL_T0 == scr_handle_p->current_protocol)
		scr_handle_p->retries = 3;
	else
		scr_handle_p->retries = 0; /* Zero otherwise */

	/* Special case - protocol T=1 allows a minimum
	 * "total time" of 11etu, therefore we have to program
	 * UART accordingly.
	 */
	if (((STM_SCR_PROTOCOL_T1 == scr_handle_p->current_protocol) ||
	(STM_SCR_PROTOCOL_T14 == scr_handle_p->current_protocol))
	&& (atr_data_p->minimum_guard_time == true))
		atr_data_p->N = SCR_DEFAULT_N - 1;

	if (((STM_SCR_PROTOCOL_T1 == scr_handle_p->current_protocol) ||
	(STM_SCR_PROTOCOL_T14 == scr_handle_p->current_protocol))
	&& (atr_data_p->N == 1))
		scr_caps_p->guard_delay = 0;
	else
		scr_caps_p->guard_delay = atr_data_p->N;

	scr_handle_p->N = atr_data_p->N;

	scr_caps_p->supported_protocol = atr_data_p->supported_protocol;

	/* Also updated the bit convention in
	reading the remaining byte of ATR */
	scr_caps_p->bit_convention = atr_data_p->bit_convention;

	scr_caps_p->max_baud_rate =
		scr_calculate_baudrate(
			scr_handle_p->scr_caps.max_clock_frequency,
		scr_handle_p->di,
		scr_handle_p->fi);

	scr_caps_p->baud_rate =
		scr_calculate_baudrate(
			scr_handle_p->scr_caps.working_frequency,
		scr_handle_p->di,
		scr_handle_p->fi);

	if (atr_data_p->supported_protocol & (0x1 << STM_SCR_PROTOCOL_T1)) {
		scr_debug_print("T1 protocol supported\n");
		scr_caps_p->character_waiting_time =
			(1 << atr_data_p->character_waiting_index) + 11;

		/* in ms */
		scr_caps_p->block_waiting_time =
				((1 << atr_data_p->block_waiting_index) * 100);
	}
	/* Vendor specific calculation */
	if (scr_ca_recalculate_parameters)
		scr_ca_recalculate_parameters(scr_handle_p);

	scr_debug_print("<%s> :: <%d> ->default"\
			"frequency %d"\
			"baud rate %d"\
			"new working clock freq %d"\
			"working freq %d "\
			"work etu  %d\n",
			__func__,
			__LINE__,
			scr_handle_p->scr_caps.clock_frequency,
			scr_handle_p->scr_caps.baud_rate,
			scr_handle_p->new_inuse_freq,
			scr_handle_p->scr_caps.working_frequency,
			scr_handle_p->scr_caps.work_etu);
}

void scr_set_asc_params(scr_ctrl_t *scr_handle_p,
			uint32_t value,
			stm_scr_ctrl_flags_t ctrl_flags)
{
	if (stm_asc_scr_get_compound_control(
				scr_handle_p->asc_handle,
				STM_ASC_SCR_PROTOCOL,
				&scr_handle_p->asc_protocol))
		scr_error_print(" stm_asc_scr_get_compound_control failed :: "\
				"ctrl flags %d\n", ctrl_flags);

	scr_handle_p->user_forced_parameters = true;
	switch (ctrl_flags) {
	case STM_SCR_CTRL_STOP_BITS:
		scr_handle_p->asc_protocol.stopbits = value;
		break;
	case STM_SCR_CTRL_DATA_PARITY:
		scr_handle_p->asc_protocol.databits = value;
		break;
	default:
		scr_error_print("<%s> failed wrong control flag\n", __func__);
		break;
	}
	scr_handle_p->asc_protocol.baud = scr_handle_p->scr_caps.baud_rate;
	if(stm_asc_scr_set_compound_control(
			scr_handle_p->asc_handle,
			STM_ASC_SCR_PROTOCOL,
			&scr_handle_p->asc_protocol))
		scr_error_print(" stm_asc_scr_set_compound_control failed :: "\
				"ctrl flags %d\n", ctrl_flags);
}

void scr_get_asc_params(scr_ctrl_t *scr_handle_p,
		uint32_t *value_p,
		stm_scr_ctrl_flags_t ctrl_flags)
{
	if (stm_asc_scr_get_compound_control(
			scr_handle_p->asc_handle,
			STM_ASC_SCR_PROTOCOL,
			&scr_handle_p->asc_protocol))
		scr_error_print(" stm_asc_scr_get_compound_control failed ::"\
				"ctrl flags %d\n", ctrl_flags);
	switch (ctrl_flags) {
	case STM_SCR_CTRL_STOP_BITS:
		*value_p = scr_handle_p->asc_protocol.stopbits;
	break;
	case STM_SCR_CTRL_DATA_PARITY:
		*value_p = scr_handle_p->asc_protocol.databits;
	break;
	default:
		scr_error_print("<%s> failed wrong control flag\n", __func__);
	break;
	}
}

void scr_populate_clock_default(scr_ctrl_t *scr_handle_p)
{
	scr_handle_p->clock_source = SCR_CLOCK_SOURCE_COMMS;
	if(scr_handle_p->scr_plat_data->clk)
		scr_handle_p->scr_caps.clock_frequency =
			clk_get_rate(scr_handle_p->scr_plat_data->clk);
	 scr_handle_p->scr_caps.working_frequency = 0;
	 scr_handle_p->new_inuse_freq = 0;
}

void scr_populate_default(scr_ctrl_t *scr_handle_p)
{
	int err = 0;
	struct scr_ca_param_s *ca_param_p = &scr_handle_p->ca_param;
	struct scr_ca_ops_s *ca_ops_p = &scr_handle_p->ca_ops;

	scr_handle_p->reset_done = false;
	scr_handle_p->detect_inverted = false;
	scr_handle_p->current_protocol = 0;

	/* For ATR */
	scr_handle_p->scr_caps.bit_convention = SCR_DISCOVERY;

	/* In terms of ETU */
	scr_handle_p->scr_caps.work_etu = SCR_INITIAL_ETU;

	/* Check teh baudrate getting updated for
	CA in the scr_ca_populate_init_parameters */
	scr_handle_p->scr_caps.baud_rate = SCR_INITIAL_BAUD_RATE;
#ifdef CONFIG_ARCH_STI
	scr_handle_p->scr_caps.class_sel_gpio_value = 0;
#else
	scr_handle_p->scr_caps.class_sel_pad_value = 0;
#endif
	scr_handle_p->scr_caps.character_guard_time = 11; /* etus */
	scr_handle_p->scr_caps.block_guard_time = 22;
	scr_handle_p->scr_caps.work_waiting_time = 960 * SCR_DEFAULT_W;
	scr_handle_p->atr_response.atr_size = 0;
	memset(scr_handle_p->atr_response.raw_data, 0, MAX_ATR_SIZE);
	scr_handle_p->atr_response.extra_bytes_in_atr = 0;

	scr_handle_p->atr_response.parsed_data.block_waiting_index = 4;
	scr_handle_p->atr_response.parsed_data.character_waiting_index = 13;
	scr_handle_p->scr_caps.ifsc = 32;
	scr_handle_p->nad = 0x00;
	scr_handle_p->flow_control = false;
	scr_handle_p->nack = true;
	scr_handle_p->N = SCR_DEFAULT_N;

	/* In term of ETU:GT should be alteast 2 */
	scr_handle_p->scr_caps.guard_delay = 2;
	scr_handle_p->t1_block_details.scr_handle_p = scr_handle_p;
	scr_handle_p->t1_block_details.bytes_read = 0;
	scr_handle_p->t1_block_details.bytes_written = 0;
	scr_handle_p->t1_block_details.bytes_last_written = 0;
	scr_handle_p->t1_block_details.our_sequence = 0;
	scr_handle_p->t1_block_details.their_sequence = 0;
	scr_handle_p->t1_block_details.nad = scr_handle_p->nad;
	scr_handle_p->t1_block_details.first_block = true;
	/*reset retyr reset for the ca modules in
		scr_ca_populate_init_parameters*/
	scr_handle_p->reset_retry = true;

	scr_handle_p->fi = SCR_INITIAL_Fi;
	scr_handle_p->di = SCR_INITIAL_Di;

	ca_param_p->reset_to_write_delay = 0;
	ca_param_p->rst_low_to_clk_low_delay = 0;

	scr_handle_p->wwt_tolerance = 11; /* in ms */

	if (scr_ca_populate_init_parameters)
		scr_ca_populate_init_parameters(scr_handle_p);

	if (ca_ops_p->ca_set_param)
		ca_ops_p->ca_set_param(scr_handle_p, &scr_handle_p->ca_param);

	err = stm_asc_scr_set_control(
		scr_handle_p->asc_handle,
		STM_ASC_SCR_NACK,
		scr_handle_p->nack);
	if (err != 0) {
		scr_error_print(" stm_asc_scr_set_control() failed(%d)\n",
			err);
	}

	scr_handle_p->user_forced_parameters = false;

}

uint32_t scr_read_atr(scr_ctrl_t *scr_handle_p)
{
	uint32_t error_code = 0, bits_set = 0;
	uint32_t byte_read = 0, historybytes = 0;
	uint8_t TDX = 0;
	scr_atr_response_t *atr_response_p;

	atr_response_p = &(scr_handle_p->atr_response);
	atr_response_p->atr_size = 0;

	/*ca_os_sleep_milli_sec(100); */
	atr_response_p->atr_size = scr_asc_read(scr_handle_p,
					atr_response_p->raw_data,
					MAX_ATR_SIZE,
					2,
					SCR_DEFAULT_TIMEOUT,
					false);
	if ((signed int)atr_response_p->atr_size < 0) {
		scr_error_print("<%s>:<%d> : reset failed %d\n",
			__func__, __LINE__, atr_response_p->atr_size);
		scr_handle_p->last_error = SCR_ERROR_NO_ANSWER;
		goto error;
	}
	scr_debug_print("first byte %x  second byte %x size %d\n",
			atr_response_p->raw_data[0],
			atr_response_p->raw_data[1],
			atr_response_p->atr_size);

	/*set the bit convention*/
	if (atr_response_p->raw_data[SCR_PPSS_INDEX] == 0x3B)
		scr_handle_p->scr_caps.bit_convention = SCR_DIRECT;
	else if (atr_response_p->raw_data[SCR_PPSS_INDEX] == 0x3) {
		/* convert the PPSS and PPS0 byte*/
		scr_bit_convertion(&atr_response_p->raw_data[SCR_PPSS_INDEX],
					2);
		scr_handle_p->scr_caps.bit_convention = SCR_INVERSE;
		/* Remaining bytes will be converted in the read call
		base on the flag bit_convention = SCR_INVERSE*/
	} else {
		scr_error_print(" wrong bit code %x\n",
			atr_response_p->raw_data[SCR_PPSS_INDEX]);
		goto error;
	}

	while (1) {
		TDX = atr_response_p->raw_data[atr_response_p->atr_size - 1] &
				0xF0;
		bits_set = scr_no_of_bits_set(TDX);
		scr_debug_print("<%s::%d>  bits set %d TDX %x\n",
			__func__, __LINE__, bits_set, TDX);
		if (bits_set)
			byte_read = scr_asc_read(scr_handle_p,
				(atr_response_p->raw_data +
				atr_response_p->atr_size),
				(MAX_ATR_SIZE - atr_response_p->atr_size),
				bits_set,
				(bits_set * SCR_DEFAULT_TIMEOUT),
				false);
		else
			break;

		if ((signed int)byte_read < 0) {
			scr_error_print("<%s>:<%d> : reset failed(%d)\n",
			__func__, __LINE__, byte_read);
			goto error;
		} else {
			atr_response_p->atr_size += byte_read;
			scr_debug_print("Size of the ATR read till now %d\n",
					atr_response_p->atr_size);
		}

		/* TDX+1 missing and we have reed the other TAX TBX TCX byte and
		 * now go to read history bytes
		 */
		if (!(TDX&0x80))
			break;
	}

	if (atr_response_p->raw_data[1] & 0x0F) {
		scr_debug_print("reading the history bytes %d\n",
			atr_response_p->raw_data[1] & 0x0F);

		historybytes = atr_response_p->raw_data[1] & 0x0F;
		error_code = scr_asc_read(scr_handle_p,
			&atr_response_p->raw_data[atr_response_p->atr_size],
			MAX_ATR_SIZE - atr_response_p->atr_size,
			atr_response_p->raw_data[1] & 0x0F,
			(historybytes * SCR_DEFAULT_TIMEOUT),
			false);

		if ((signed int)error_code < 0) {
			scr_error_print("<%s>:<%d> : reset failed %d\n ",
				__func__, __LINE__, error_code);
			goto error;
		} else
			atr_response_p->atr_size+=error_code;
	}

	scr_debug_print("<%s>:<%d> ATR size %d\n ", __func__, __LINE__,
			atr_response_p->atr_size);
	return 0;

	error:
		scr_debug_print("<%s>:<%d>\n ", __func__, __LINE__);
		error_code = atr_response_p->atr_size;
		return error_code;
}

int32_t scr_set_nack(scr_ctrl_t *scr_handle_p,
			stm_scr_protocol_t current_protocol)
{
	int32_t error_code = 0;
	if(current_protocol == STM_SCR_PROTOCOL_T1) {
		error_code =  stm_asc_scr_set_control(scr_handle_p->asc_handle,
					STM_ASC_SCR_NACK,
					false);
		if (error_code) {
			scr_error_print("<%s>:<%d> stm_asc_scr_set_control"\
					" failed\n",
					__func__, __LINE__);

			goto error;
		}
	}
	else if (current_protocol == STM_SCR_PROTOCOL_T0) {
		error_code =  stm_asc_scr_set_control(scr_handle_p->asc_handle,
					STM_ASC_SCR_NACK,
					true);
		if (error_code) {
			scr_error_print("<%s>:<%d> stm_asc_scr_set_control"\
					" failed\n",
					__func__, __LINE__);

			goto error;
		}
	}
	return 0;
	error :
		return error_code;
}

void scr_gpio_direction(scr_ctrl_t *scr_handle_p)
{
#ifdef CONFIG_ARCH_STI
	scr_handle_p->gpio_direction = GPIO_IN;
#else
	scr_handle_p->gpio_direction = stm_pad_gpio_direction_in;
#endif
	if(scr_ca_gpio_direction &&
		scr_handle_p->scr_caps.device_type == STM_SCR_DEVICE_CA)
		scr_ca_gpio_direction(scr_handle_p);
}
