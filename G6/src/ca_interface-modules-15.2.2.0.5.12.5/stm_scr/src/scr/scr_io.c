#ifdef CONFIG_ARCH_STI
#include <linux/platform_device.h>
#include "ca_platform.h"
#else
#include <linux/stm/pad.h>
#include <linux/stm/stih416.h>
#endif

#include "stm_asc_scr.h"
#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_io.h"
#include "stm_common.h"

static const uint8_t inverse_convention_lookup_table[] =
{
	0xff, 0x7f, 0xbf, 0x3f, 0xdf, 0x5f, 0x9f, 0x1f, 0xef, 0x6f, 0xaf,
	0x2f, 0xcf, 0x4f, 0x8f, 0x0f, 0xf7, 0x77, 0xb7, 0x37, 0xd7, 0x57,
	0x97, 0x17, 0xe7, 0x67, 0xa7, 0x27, 0xc7, 0x47, 0x87, 0x07, 0xfb,
	0x7b, 0xbb, 0x3b, 0xdb, 0x5b, 0x9b, 0x1b, 0xeb, 0x6b, 0xab, 0x2b,
	0xcb, 0x4b, 0x8b, 0x0b, 0xf3, 0x73, 0xb3, 0x33, 0xd3, 0x53, 0x93,
	0x13, 0xe3, 0x63, 0xa3, 0x23, 0xc3, 0x43, 0x83, 0x03, 0xfd, 0x7d,
	0xbd, 0x3d, 0xdd, 0x5d, 0x9d, 0x1d, 0xed, 0x6d, 0xad, 0x2d, 0xcd,
	0x4d, 0x8d, 0x0d, 0xf5, 0x75, 0xb5, 0x35, 0xd5, 0x55, 0x95, 0x15,
	0xe5, 0x65, 0xa5, 0x25, 0xc5, 0x45, 0x85, 0x05, 0xf9, 0x79, 0xb9,
	0x39, 0xd9, 0x59, 0x99, 0x19, 0xe9, 0x69, 0xa9, 0x29, 0xc9, 0x49,
	0x89, 0x09, 0xf1, 0x71, 0xb1, 0x31, 0xd1, 0x51, 0x91, 0x11, 0xe1,
	0x61, 0xa1, 0x21, 0xc1, 0x41, 0x81, 0x01, 0xfe, 0x7e, 0xbe, 0x3e,
	0xde, 0x5e, 0x9e, 0x1e, 0xee, 0x6e, 0xae, 0x2e, 0xce, 0x4e, 0x8e,
	0x0e, 0xf6, 0x76, 0xb6, 0x36, 0xd6, 0x56, 0x96, 0x16, 0xe6, 0x66,
	0xa6, 0x26, 0xc6, 0x46, 0x86, 0x06, 0xfa, 0x7a, 0xba, 0x3a, 0xda,
	0x5a, 0x9a, 0x1a, 0xea, 0x6a, 0xaa, 0x2a, 0xca, 0x4a, 0x8a, 0x0a,
	0xf2, 0x72, 0xb2, 0x32, 0xd2, 0x52, 0x92, 0x12, 0xe2, 0x62, 0xa2,
	0x22, 0xc2, 0x42, 0x82, 0x02, 0xfc, 0x7c, 0xbc, 0x3c, 0xdc, 0x5c,
	0x9c, 0x1c, 0xec, 0x6c, 0xac, 0x2c, 0xcc, 0x4c, 0x8c, 0x0c, 0xf4,
	0x74, 0xb4, 0x34, 0xd4, 0x54, 0x94, 0x14, 0xe4, 0x64, 0xa4, 0x24,
	0xc4, 0x44, 0x84, 0x04, 0xf8, 0x78, 0xb8, 0x38, 0xd8, 0x58, 0x98,
	0x18, 0xe8, 0x68, 0xa8, 0x28, 0xc8, 0x48, 0x88, 0x08, 0xf0, 0x70,
	0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10, 0xe0, 0x60, 0xa0, 0x20, 0xc0,
	0x40, 0x80, 0x00
};

void scr_bit_convertion(uint8_t *data_p, uint32_t size);


uint32_t scr_set_asc_ctrl(scr_ctrl_t *scr_p)
{
	stm_asc_scr_protocol_t  asc_protocol_params;
	struct scr_ca_ops_s *ca_ops_p = &scr_p->ca_ops;

	if (stm_asc_scr_set_control(scr_p->asc_handle,
			STM_ASC_SCR_FLOW_CONTROL,
			scr_p->flow_control)) {
		scr_error_print(" <%s> :: <%d> ", __func__, __LINE__);
		return -EIO;
	}

	if (stm_asc_scr_set_control(
			scr_p->asc_handle,
			STM_ASC_SCR_AUTO_PARITY_REJECTION,
			scr_p->reset_done)) {
		scr_error_print(" <%s> :: <%d> ", __func__, __LINE__);
		return -EIO;
	}

	if (ca_ops_p->manage_auto_parity)
		ca_ops_p->manage_auto_parity(scr_p->asc_handle,
					scr_p->current_protocol);

	/* Fix it to 3 , Can be changed from sysfs */
	if (stm_asc_scr_set_control(
			scr_p->asc_handle,
			STM_ASC_SCR_TX_RETRIES,
			scr_p->retries)) {
		scr_error_print(" <%s> :: <%d> ", __func__, __LINE__);
		return -EIO;
	}

	scr_debug_print("<%s> :: <%d>  guard delay %d guard time  %d\n",
				__func__,
				__LINE__,
				scr_p->scr_caps.guard_delay,
				scr_p->N);

	if (stm_asc_scr_set_control(
			scr_p->asc_handle,
			STM_ASC_SCR_GUARD_TIME,
			scr_p->N)) {
		scr_error_print(" <%s> :: <%d> ", __func__, __LINE__);
		return -EIO;
	}

	scr_debug_print("<%s> :: <%d>  baud rate %d\n",
		__func__,
		__LINE__,
		scr_p->scr_caps.baud_rate);

	asc_protocol_params.baud = scr_p->scr_caps.baud_rate;

	if (SCR_DISCOVERY == scr_p->scr_caps.bit_convention)
		asc_protocol_params.databits = STM_ASC_SCR_9BITS_UNKNOWN_PARITY;
	else if (SCR_INVERSE == scr_p->scr_caps.bit_convention)
		asc_protocol_params.databits = STM_ASC_SCR_8BITS_ODD_PARITY;
	else if (SCR_DIRECT == scr_p->scr_caps.bit_convention)
		asc_protocol_params.databits = STM_ASC_SCR_8BITS_EVEN_PARITY;

	if ((STM_SCR_DEVICE_CA == scr_p->scr_caps.device_type) &&
		(scr_p->scr_caps.bit_convention != SCR_DISCOVERY))
		if (scr_ca_set_parity)
			scr_ca_set_parity(&asc_protocol_params.databits);

	/* 11etu 'total' time requires use of 0.5 stop bits */
	if (scr_p->N == 1) {
		scr_debug_print("<%s> :: <%d> STOP bit 0.5 for N\n",
			__func__,
			__LINE__);
		asc_protocol_params.stopbits = STM_ASC_SCR_STOP_0_5;
	} else if (STM_SCR_DEVICE_CA == scr_p->scr_caps.device_type) {
		scr_debug_print("<%s> :: <%d> STOP bit %d for CA\n",
			__func__,
			__LINE__,
			scr_ca_set_stopbits(scr_p));
		asc_protocol_params.stopbits = scr_ca_set_stopbits(scr_p);
	} else {
		scr_debug_print("<%s> :: <%d> STOP bit 1.5\n",
			__func__,
			__LINE__);
		asc_protocol_params.stopbits = STM_ASC_SCR_STOP_1_5;
	}

	/* This condition will be always true scr_handle_p->user_forced_parameters == FALSE */
	/* unless sombody called scr_set_asc_params for STM_ASC_SCR_PROTOCOL*/
	/* In scr_set_asc_params we are doing scr_handle_p->user_forced_parameters = TRUE*/
	if (!scr_p->user_forced_parameters)
		if (stm_asc_scr_set_compound_control(scr_p->asc_handle,
				STM_ASC_SCR_PROTOCOL,
				&asc_protocol_params)) {
			scr_error_print(" <%s> :: <%d> ",
			__func__,
			__LINE__);
			return -EIO;
		}
	return 0;
}

void scr_bit_convertion(uint8_t *data_p, uint32_t size)
{
	/* Byte encodings for inverse convention e.g,
	 * 0 => 0xff, 1 => 0x7f, 2 => 0xbf, ... ,
	 * 0xfe => 0x80, 0xff => 0x00
	 */
	uint32_t i;
	for (i = 0; i < size; i++) {
		*data_p = inverse_convention_lookup_table[*data_p];
		data_p++;
	}

}
#ifdef CONFIG_ARCH_STI
static int32_t scr_asc_pad_claim(struct platform_device *asc_platform_device,
					stm_scr_gpio_t gpio,
					enum scr_gpio_dir direction)
{
	int32_t error_code = 0;
	struct stm_plat_asc_data *plat_data = asc_platform_device->dev.platform_data;
	plat_data->c4_selected = false;
	plat_data->c7_selected = false;
	plat_data->c8_selected = false;
	if (STM_SCR_DATA_GPIO_C4_UART & gpio) {
		plat_data->config_name = "config-c4";
		plat_data->c4_selected = true;
		plat_data->c4_pin->gpio_mode = false;
		plat_data->c7_pin->gpio_mode = true;
		plat_data->c8_pin->gpio_mode = true;
		plat_data->c7_pin->gpio_direction = direction;
		plat_data->c8_pin->gpio_direction = direction;
	} else if (STM_SCR_DATA_GPIO_C7_UART & gpio) {
		plat_data->config_name = "config-c7";
		plat_data->c7_selected = true;
		plat_data->c7_pin->gpio_mode = false;
		plat_data->c4_pin->gpio_mode = true;
		plat_data->c8_pin->gpio_mode = true;
		plat_data->c4_pin->gpio_direction = direction;
		plat_data->c8_pin->gpio_direction = direction;
	} else if (STM_SCR_DATA_GPIO_C8_UART & gpio) {
		plat_data->config_name = "config-c8";
		plat_data->c8_selected = true;
		plat_data->c8_pin->gpio_mode = false;
		plat_data->c4_pin->gpio_mode = true;
		plat_data->c7_pin->gpio_mode = true;
		plat_data->c4_pin->gpio_direction = direction;
		plat_data->c7_pin->gpio_direction = direction;
	}
	return error_code;
}
#else
static int32_t scr_asc_pad_claim(struct platform_device *asc_platform_device,
			struct stm_pad_gpio *asc_pad_p,
			stm_scr_gpio_t gpio,
			enum stm_pad_gpio_direction direction)
{
	int32_t error_code = 0;
	struct stm_pad_gpio *gpios_p;
	struct stm_plat_asc_data *plat_data =
			asc_platform_device->dev.platform_data;

	if (plat_data->pad_config->gpios_num == 2) {/*STIH415*/
		scr_debug_print("<%s> :: <%d>"\
			"PAD change not allowed\n",__func__, __LINE__);
		return 0;
	}
	ca_os_copy(plat_data->pad_config->gpios,
			asc_pad_p,
			sizeof(struct stm_pad_gpio) *
			plat_data->pad_config->gpios_num);
	gpios_p = plat_data->pad_config->gpios;

	if (STM_SCR_DATA_GPIO_C4_UART & gpio) {
		gpios_p[STM_ASC_GPIO_C7_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C7_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C7_BIT].name = "C7_GPIO";

		gpios_p[STM_ASC_GPIO_C8_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C8_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C8_BIT].name = "C8_GPIO";
	} else if (STM_SCR_DATA_GPIO_C7_UART & gpio) {
		gpios_p[STM_ASC_GPIO_C4_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C4_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C4_BIT].name = "C4_GPIO";

		gpios_p[STM_ASC_GPIO_C8_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C8_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C8_BIT].name = "C8_GPIO";
	} else if (STM_SCR_DATA_GPIO_C8_UART & gpio) {
		gpios_p[STM_ASC_GPIO_C4_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C4_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C4_BIT].name = "C4_GPIO";

		gpios_p[STM_ASC_GPIO_C7_BIT].direction = direction;
		gpios_p[STM_ASC_GPIO_C7_BIT].function = 0;
		gpios_p[STM_ASC_GPIO_C7_BIT].name = "C7_GPIO";
	}

	scr_debug_print("<%s> :: <%d> pio config\n"\
			"\tc4 --> %s\n"\
			"\tc7 --> %s\n"\
			"\tc8 --> %s\n",
			__func__,
			__LINE__,
			gpios_p[STM_ASC_GPIO_C4_BIT].name,
			gpios_p[STM_ASC_GPIO_C7_BIT].name,
			gpios_p[STM_ASC_GPIO_C8_BIT].name);

	return error_code;
}
#endif
/* Read the gpio for the pins which are in GPIO mode only */
/* If the pins read ! set the HIGH bit and clear the LOW bit int the bit mask */
/* If the pin read 0 clear both HIGH and LOW bit int the bit mask */
int32_t scr_asc_get_pad_config(scr_ctrl_t *scr_p, int *value_p)
{
	uint8_t gpio_status = 0;
	int32_t error_code = 0;
#ifndef CONFIG_ARCH_STI
	struct stm_plat_asc_data *plat_data =
		scr_p->scr_plat_data->asc_platform_device->dev.platform_data;
	if (plat_data->pad_config->gpios_num == 2) { /*STIH415*/
		scr_debug_print("<%s> :: <%d>"\
			"PAD info not allowed\n", __func__, __LINE__);
		return 0;
	}
#endif
	*value_p = scr_p->asc_gpio;

	if (!(STM_SCR_DATA_GPIO_C4_UART & scr_p->asc_gpio)) {
		error_code = stm_asc_scr_get_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C4,
				(uint32_t *)&gpio_status);
		if (gpio_status) {
			*value_p |= STM_SCR_DATA_GPIO_C4_HIGH;
			*value_p &= ~STM_SCR_DATA_GPIO_C4_LOW;
		} else {
			*value_p |= STM_SCR_DATA_GPIO_C4_LOW;
			*value_p &= ~STM_SCR_DATA_GPIO_C4_HIGH;
		}
	}

	if (!(STM_SCR_DATA_GPIO_C7_UART & scr_p->asc_gpio) && !error_code) {
		error_code = stm_asc_scr_get_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C7,
				(uint32_t *)&gpio_status);
		if (gpio_status) {
			*value_p |= STM_SCR_DATA_GPIO_C7_HIGH;
			*value_p &= ~STM_SCR_DATA_GPIO_C7_LOW;
		} else {
			*value_p |= STM_SCR_DATA_GPIO_C7_LOW;
			*value_p &= ~STM_SCR_DATA_GPIO_C7_HIGH;
		}
	}

	if (!(STM_SCR_DATA_GPIO_C8_UART & scr_p->asc_gpio) && !error_code) {
		error_code = stm_asc_scr_get_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C8,
				(uint32_t *)&gpio_status);
		if (gpio_status) {
			*value_p |= STM_SCR_DATA_GPIO_C8_HIGH;
			*value_p &= ~STM_SCR_DATA_GPIO_C8_LOW;
		} else {
			*value_p |= STM_SCR_DATA_GPIO_C8_LOW;
			*value_p &= ~STM_SCR_DATA_GPIO_C8_HIGH;
		}
	}
	return error_code;
}
#ifdef CONFIG_ARCH_STI
static int scr_asc_set_pios(scr_ctrl_t *scr_p, stm_scr_gpio_t asc_gpio)
{
	int32_t error_code = 0;
	int value;

	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);
	scr_debug_print("Which pio set\n C4==%x\n",
		asc_gpio & STM_SCR_DATA_GPIO_C4_HIGH);
	scr_debug_print("\tC7==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C7_HIGH);
	scr_debug_print("\tC8==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C8_HIGH);
	scr_debug_print("Which pio clear\n C4==%d\n",
		asc_gpio & STM_SCR_DATA_GPIO_C4_LOW);
	scr_debug_print("\tC7==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C7_LOW);
	scr_debug_print("\tC8==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C8_LOW);

	if (asc_gpio & STM_SCR_DATA_GPIO_C4_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C4,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C4_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C4,
				0);
	if (error_code)
		goto error;

	if (asc_gpio & STM_SCR_DATA_GPIO_C7_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C7,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C7_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C7,
				0);
	if (error_code)
		goto error;

	if (asc_gpio & STM_SCR_DATA_GPIO_C8_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C8,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C8_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C8,
				0);
error:
	scr_asc_get_pad_config(scr_p, &value);
	scr_debug_print("<%s> :: <%d>", __func__, __LINE__);
	scr_debug_print(" PAD config 0x%x\n", value);
	return error_code;
}
#else
static int scr_asc_set_pios(scr_ctrl_t *scr_p, stm_scr_gpio_t asc_gpio)
{
	int32_t error_code = 0;
	int value;
	scr_debug_print("<%s> :: <%d>\n", __func__, __LINE__);
	scr_debug_print("Which pio set\n C4==%x\n",
		asc_gpio & STM_SCR_DATA_GPIO_C4_HIGH);
	scr_debug_print("\tC7==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C7_HIGH);
	scr_debug_print("\tC8==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C8_HIGH);
	scr_debug_print("Which pio clear\n C4==%d\n",
		asc_gpio & STM_SCR_DATA_GPIO_C4_LOW);
	scr_debug_print("\tC7==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C7_LOW);
	scr_debug_print("\tC8==%x\n", asc_gpio & STM_SCR_DATA_GPIO_C8_LOW);
	if (asc_gpio & STM_SCR_DATA_GPIO_C4_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C4,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C4_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C4,
				0);
	if (error_code)
		goto error;
	if (asc_gpio & STM_SCR_DATA_GPIO_C7_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C7,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C7_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C7,
				0);
	if (error_code)
		goto error;
	if (asc_gpio & STM_SCR_DATA_GPIO_C8_HIGH)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C8,
				1);
	else if (asc_gpio & STM_SCR_DATA_GPIO_C8_LOW)
		error_code = stm_asc_scr_set_control(scr_p->asc_handle,
				STM_ASC_SCR_GPIO_C8,
				0);
error:
	scr_asc_get_pad_config(scr_p, &value);
	scr_debug_print("<%s> :: <%d>", __func__, __LINE__);
	scr_debug_print(" PAD config 0x%x\n", value);
	return error_code;
}
#endif
int32_t scr_asc_pad_config(scr_ctrl_t *scr_p, stm_scr_gpio_t asc_gpio)
{
	uint32_t error_code = 0;
#ifdef CONFIG_ARCH_STI
	error_code = stm_asc_scr_pad_config(scr_p->asc_handle, true);
	if (error_code) {
		scr_error_print("<%s> :: <%d>ASC Pad change failed %d\n",
				__func__, __LINE__, error_code);
		goto error;
	}
	error_code = scr_asc_pad_claim(scr_p->scr_plat_data->asc_platform_device
			, asc_gpio, scr_p->gpio_direction);
#else
	error_code = stm_asc_scr_pad_config(scr_p->asc_handle, true);
	if (error_code) {
		scr_error_print("<%s> :: <%d>ASC Pad change failed %d\n",
				__func__, __LINE__, error_code);
		goto error;
	}

	error_code = scr_asc_pad_claim(
			scr_p->scr_plat_data->asc_platform_device,
			scr_p->asc_pad_p,
			asc_gpio,
			scr_p->gpio_direction);
#endif
	if (error_code) {
		scr_error_print("<%s> :: <%d>Wrong ASC Pad option %d\n",
			__func__, __LINE__, asc_gpio);
		goto error;
	}

	error_code = stm_asc_scr_pad_config(scr_p->asc_handle, false);
	if (error_code) {
		scr_error_print("<%s> :: <%d>ASC Pad change failed %d\n",
				__func__, __LINE__, error_code);
		goto error;
	}

	error_code = scr_asc_set_pios(scr_p, asc_gpio);
	if (error_code) {
		scr_error_print("<%s> :: <%d>ASC Pad set failed %d\n",
				__func__, __LINE__, error_code);
		goto error;
	}
error:
	return error_code;
}

#ifdef CONFIG_ARCH_STI
uint32_t  scr_asc_new(scr_ctrl_t *scr_p)
{
	uint32_t  error_code = 0;
	stm_asc_scr_new_params_t  new_params;
	scr_p->asc_gpio = STM_SCR_DATA_GPIO_C4_LOW |
			STM_SCR_DATA_GPIO_C7_UART |
			STM_SCR_DATA_GPIO_C8_LOW;
	new_params.transfer_type = STM_ASC_SCR_TRANSFER_CPU;
	error_code = scr_asc_pad_claim(scr_p->scr_plat_data->asc_platform_device
			, scr_p->asc_gpio, scr_p->gpio_direction);
	if (error_code)		/*scr_asc_pad_claim() always return 0*/
		return error_code;
	new_params.device_data =
		(void *)scr_p->scr_plat_data->asc_platform_device;
	new_params.asc_fifo_size = scr_p->asc_fifo_size;
	error_code = stm_asc_scr_new(&new_params, &scr_p->asc_handle);
	return error_code;
}
#else
uint32_t  scr_asc_new(scr_ctrl_t *scr_p)
{
	uint32_t  error_code = 0;
	stm_asc_scr_new_params_t  new_params;
	struct platform_device *asc_platform_device =
			scr_p->scr_plat_data->asc_platform_device;
	struct stm_plat_asc_data *plat_data =
			asc_platform_device->dev.platform_data;

	/*By default */
	scr_p->asc_gpio = STM_SCR_DATA_GPIO_C4_LOW |
			STM_SCR_DATA_GPIO_C7_UART |
			STM_SCR_DATA_GPIO_C8_LOW;
	new_params.transfer_type = STM_ASC_SCR_TRANSFER_CPU;

	scr_p->asc_pad_p = (struct stm_pad_gpio *)ca_os_malloc(
				sizeof(struct stm_pad_gpio) *
				plat_data->pad_config->gpios_num);

	if (!scr_p->asc_pad_p) {
		scr_error_print("<%s> :: <%d> Null scr_handle_p\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	ca_os_copy(scr_p->asc_pad_p,
			plat_data->pad_config->gpios,
			sizeof(struct stm_pad_gpio) *
			plat_data->pad_config->gpios_num);

	error_code = scr_asc_pad_claim(
			scr_p->scr_plat_data->asc_platform_device,
			scr_p->asc_pad_p,
			scr_p->asc_gpio,
			scr_p->gpio_direction);
	if (error_code)
		return error_code;
	new_params.device_data =
		(void *)scr_p->scr_plat_data->asc_platform_device;
	new_params.asc_fifo_size = scr_p->asc_fifo_size;

	error_code = stm_asc_scr_new(&new_params, &scr_p->asc_handle);

	return error_code;
}
#endif
uint32_t scr_asc_delete(scr_ctrl_t *scr_p)
{
	uint32_t  error_code = 0;
	error_code = stm_asc_scr_delete(scr_p->asc_handle);
	if (CA_NO_ERROR == error_code)
		scr_p->asc_handle = NULL;
#ifndef CONFIG_ARCH_STI
	ca_os_free(scr_p->asc_pad_p);
	scr_p->asc_pad_p = NULL;
#endif
	return error_code;
}

uint32_t  scr_asc_read(scr_ctrl_t *scr_p,
			uint8_t *buffer_to_read_p,
			uint32_t size_of_buffer,
			uint32_t size_to_read,
			uint32_t  timeout,
			bool change_ctrl_enable)
{
	int32_t error;
	stm_asc_scr_data_params_t read_params;

	if (change_ctrl_enable) {
		if (scr_set_asc_ctrl(scr_p)) {
			scr_error_print(" <%s> :: <%d>\n",
				__func__,
				__LINE__);
			return -EIO;}
	}
	read_params.actual_size = 0;
	read_params.buffer_p = buffer_to_read_p;

	if (size_of_buffer <= size_to_read)
		read_params.size = size_of_buffer;
	else
		read_params.size = size_to_read;

	read_params.timeout_ms = timeout;
	scr_debug_print(" <%s> :: <%d>\n",
			__func__,
			__LINE__);

	error = stm_asc_scr_read(scr_p->asc_handle, &read_params);
	if (error) {
		scr_error_print(" <%s> :: <%d> error %d\n",
			__func__,
			__LINE__, error);
		if (error == -ETIMEDOUT)
			scr_p->last_error = SCR_ERROR_READ_TIMEOUT;
		goto asc_error;
	}
	if (scr_p->scr_caps.bit_convention == SCR_INVERSE)
		scr_bit_convertion(buffer_to_read_p, size_of_buffer);

	scr_debug_print(" <%s> :: <%d>", __func__, __LINE__);
	scr_debug_print("data read till %d\n", read_params.actual_size);
	return read_params.actual_size;

	asc_error:
		return  error;

}

uint32_t  scr_asc_write(scr_ctrl_t *scr_p,
			const uint8_t *buffer_to_write_p,
			uint32_t size_of_buffer,
			uint32_t size_to_write,
			uint32_t timeout)
{
	stm_asc_scr_data_params_t write_params;
	stm_asc_scr_data_params_t read_params;
	uint8_t dummy_buffer[512];
	int32_t error_code = 0;
	if (scr_set_asc_ctrl(scr_p)) {
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	write_params.actual_size = 0;
	write_params.buffer_p = (uint8_t *)buffer_to_write_p;
	if (size_of_buffer < size_to_write)
		write_params.size = size_of_buffer;
	else
		write_params.size = size_to_write;

	write_params.timeout_ms = timeout;
	if (scr_p->scr_caps.bit_convention == SCR_INVERSE) {
		memcpy(dummy_buffer, buffer_to_write_p, size_of_buffer);

		scr_bit_convertion((uint8_t *)dummy_buffer, size_of_buffer);
		write_params.buffer_p = dummy_buffer;
	}

	error_code = stm_asc_scr_write(scr_p->asc_handle, &write_params);
	if (error_code)	{
		if (error_code == -ETIMEDOUT)
			scr_p->last_error = SCR_ERROR_WRITE_TIMEOUT;
		goto asc_error;
	}

	read_params.buffer_p = dummy_buffer;
	read_params.size = write_params.actual_size;
	read_params.timeout_ms = timeout;
	read_params.actual_size = 0;
	if (stm_asc_scr_read(scr_p->asc_handle, &read_params))
		goto asc_error;

	if ((read_params.actual_size != write_params.actual_size) &&
		(write_params.actual_size < 512)) {
		scr_error_print(" <%s> :: <%d> loop back failed\n"\
				"Data writen %d data read %d\n",
				__func__, __LINE__,
				write_params.actual_size,
				read_params.actual_size);
		goto asc_error;
	}
	return write_params.actual_size;

	asc_error:
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
}

uint32_t scr_asc_abort(scr_ctrl_t *scr_p)
{
	if (stm_asc_scr_abort(scr_p->asc_handle)) {
		/* Keep this as debug to avoid unwanted prints */
		scr_debug_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	return 0;
}

uint32_t scr_asc_flush(scr_ctrl_t *scr_p)
{
	if (stm_asc_scr_flush(scr_p->asc_handle)) {
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	return 0;
}

uint32_t scr_asc_freeze(scr_ctrl_t *scr_p)
{
	if (stm_asc_scr_freeze(scr_p->asc_handle)) {
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	return 0;
}

uint32_t scr_asc_restore(scr_ctrl_t *scr_p)
{
	if (stm_asc_scr_restore(scr_p->asc_handle)) {
		scr_error_print(" <%s> :: <%d>\n", __func__, __LINE__);
		return -EIO;
	}
	return 0;
}
