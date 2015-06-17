/*
 * arch/common/stm_fe_setup.h
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author:Rahul V
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _STM_FE_SETUP_H
#define _STM_FE_SETUP_H

#ifdef CONFIG_MACH_STM_STIH416
#include <linux/stm/stih416.h>
#define STMFE_GPIO(port, pin) stih416_gpio(port, pin)
#else
#define STMFE_GPIO(port, pin) stm_gpio(port, pin)
#endif

void stm_fe_device_release(struct device *dev);

#define DEMOD(i)                                        \
{                                                         \
	.name = STM_FE_DEMOD_NAME,                             \
	.id = i,                                               \
	.num_resources = 0,                                     \
	.dev.release = stm_fe_device_release,                    \
	.dev.platform_data = &(struct stm_platdev_data) {       \
		.ver = STM_FE_CONF_VERSION,               \
		.config_data = &(struct demod_config_s){               \
			.demod_name = STM_FE_DEMOD_NAME_##i,                   \
			.tuner_name = STM_FE_DEMOD_TUNER_NAME_##i,     \
			.demod_io  =  {                                        \
				.io = STM_FE_DEMOD_IO_TYPE_##i,        \
				.route = STM_FE_DEMOD_I2C_ROUTE_##i,   \
				.bus = STM_FE_DEMOD_I2C_DEVICE_##i,         \
				.baud = STM_FE_DEMOD_I2C_BAUD_##i,     \
				.address = STM_FE_DEMOD_ADDRESS_##i    \
			},                                                     \
			.tuner_io  =  {                                \
				.io = STM_FE_DEMOD_TUNER_IO_TYPE_##i,   \
				.route = STM_FE_DEMOD_TUNER_I2C_ROUTE_##i, \
				.bus = STM_FE_DEMOD_TUNER_I2C_DEVICE_##i,  \
				.baud = STM_FE_DEMOD_TUNER_I2C_BAUD_##i,   \
				.address = STM_FE_DEMOD_TUNER_ADDRESS_##i  \
			},                                                     \
			.clk_config = {                                        \
				.tuner_clk = STM_FE_DEMOD_TUNER_CLOCK_##i, \
				.clk_type = STM_FE_DEMOD_DEMOD_CLOCK_TYPE_##i, \
				.demod_clk = STM_FE_DEMOD_DEMOD_CLOCK_##i,     \
				.tuner_clkout_divider = \
				STM_FE_DEMOD_DEMOD_TUNER_CLOCK_DIV_##i \
			},                                                     \
			.ts_out = STM_FE_DEMOD_TS_OUT_CONFIG_##i,             \
			.ts_clock = STM_FE_DEMOD_TS_CLOCK_##i,                \
			.ts_tag = STM_FE_DEMOD_TS_TAG_##i,                    \
			.demux_tsin_id = STM_FE_DEMOD_DEMUX_TSIN_ID_##i,      \
			.custom_flags = STM_FE_DEMOD_CUSTOMISATIONS_##i,      \
			.tuner_if = STM_FE_DEMOD_TUNER_IF_##i,                \
			.roll_off = STM_FE_DEMOD_ROLL_OFF_##i,                \
			.reset_pio = STM_FE_DEMOD_RESET_PIO_##i,               \
			.vglna_config = {                                    \
				.type = STM_FE_DEMOD_VGLNA_TYPE_##i,           \
				.vglna_io = {                                  \
					.io = STM_FE_DEMOD_VGLNA_IO_TYPE_##i, \
					.route = \
					     STM_FE_DEMOD_VGLNA_I2C_ROUTE_##i, \
					.bus = \
					    STM_FE_DEMOD_VGLNA_I2C_DEVICE_##i, \
					.baud = \
					      STM_FE_DEMOD_VGLNA_I2C_BAUD_##i, \
					.address = \
					    STM_FE_DEMOD_VGLNA_I2C_ADDRESS_##i \
				},                                        \
				.rep_bus = STM_FE_DEMOD_VGLNA_I2C_REP_BUS_##i \
			},                                                   \
		.remote_ip = STM_FE_TS_REMOTE_IP_##i,                      \
		.remote_port = STM_FE_TS_REMOTE_PORT_##i                      \
		}                                                     \
	}                                                       \
}                                                         \

#define LNB(i)                                            \
{                                                           \
	.name = STM_FE_LNB_NAME,                                 \
	.id = i,                                                  \
	.num_resources = 0,                                       \
	.dev = {                                                   \
			.release = stm_fe_device_release,                    \
			.platform_data = &(struct stm_platdev_data) {          \
			.ver = STM_FE_CONF_VERSION,                            \
			.config_data = &(struct lnb_config_s) {                \
				.lnb_name = STM_FE_LNB_NAME_##i,               \
				.lnb_io  =  {                                  \
					.io = STM_FE_LNB_IO_TYPE_##i,          \
					.route = STM_FE_LNB_I2C_ROUTE_##i,     \
					.bus = STM_FE_LNB_I2C_DEVICE_##i,      \
					.baud = STM_FE_LNB_I2C_BAUD_##i,       \
					.address = STM_FE_LNB_ADDRESS_##i      \
				},                                             \
				.cust_flags = STM_FE_LNB_CUSTOMISATIONS_##i,   \
				.be_pio = {                                    \
					.volt_sel = \
						STM_FE_LNB_VOLTAGE_SEL_PIN_##i,\
					.volt_en = \
						STM_FE_LNB_VOLTAGE_EN_PIN_##i, \
					.tone_sel = \
						STM_FE_LNB_TONE_SEL_PIN_##i    \
				}                                         \
			}                                                     \
		}                                                       \
	}                                                          \
}                                                            \

#define DISEQC(i)                                             \
{                                                             \
	.name = STM_FE_DISEQC_NAME,                                 \
	.id = i,                                                     \
	.num_resources = 0,                                          \
	.dev = {                                                     \
		.release = stm_fe_device_release,                    \
		.platform_data = &(struct stm_platdev_data) {              \
			.ver = STM_FE_CONF_VERSION,                          \
			.config_data = &(struct diseqc_config_s) {        \
				.diseqc_name = STM_FE_DISEQC_NAME_##i,         \
				.ver = STM_FE_DISEQC_VERSION_##i,              \
				.cust_flags = STM_FE_DISEQC_CUSTOMISATIONS_##i \
			}                                                      \
		}                                                         \
	}                                                           \
}                                                             \

#define IP(i)                                             \
{                                                             \
	.name = STM_FE_IP_NAME,                                 \
	.id = i,                                                     \
	.num_resources = 0,                                          \
	.dev = {                                                     \
		.release = stm_fe_device_release,                    \
		.platform_data = &(struct stm_platdev_data) {              \
			.ver = STM_FE_CONF_VERSION,                        \
			.config_data = &(struct ip_config_s) {             \
				.ip_name = STM_FE_IP_NAME_##i,             \
				.ip_addr = STM_FE_IP_ADDRESS_##i,           \
				.ip_port = STM_FE_IP_PORT_NO_##i,	    \
				.protocol = STM_FE_IP_PROTOCOL_##i,	    \
				.ethdev = STM_FE_IP_ETHDEVICE_##i           \
			}                                                  \
		}                                                          \
	} \
}

#define IPFEC(i)                                             \
{                                                             \
	.name = STM_FE_IPFEC_NAME,                                 \
	.id = i,                                                     \
	.num_resources = 0,                                          \
	.dev = {                                                     \
		.release = stm_fe_device_release,                    \
		.platform_data = &(struct stm_platdev_data) {              \
			.ver = STM_FE_CONF_VERSION,                        \
			.config_data = &(struct ipfec_config_s) {             \
				.ipfec_name = STM_FE_IPFEC_NAME_##i,           \
				.ipfec_addr = STM_FE_IPFEC_ADDRESS_##i,        \
				.ipfec_port = STM_FE_IPFEC_PORT_NO_##i,	    \
				.ipfec_scheme = STM_FE_IPFEC_SCHEME_##i	    \
			}                                                  \
		}                                                          \
	} \
}

#define STM_FE_ADD_DEVICE(no, dev, name)  \
	do { \
		if (strlen(name)) { \
			stm_fe_devices[no++] = dev;                    \
			pr_info("stm_fe: device %s ...\n", name); \
		} \
	} while (0)

#define CONFIG(platdev) \
	(((struct stm_platdev_data *)(platdev.dev.platform_data))->config_data)

#ifdef CONFIG_DVB_CORE
#define DVB_DEMOD(ins)                                               \
{                                                                    \
	.name = "stm_dvb_demod",                                            \
	.id = ins,                                                          \
	.num_resources = 0,                                                 \
	.dev = {                                                            \
		.release = stm_fe_device_release,                              \
		.platform_data = &(struct stm_platdev_data) {                  \
			.ver = STM_FE_CONF_VERSION,                            \
			.config_data = NULL,                                   \
		}                                                          \
	}                                                                 \
}

#define DVB_LNB(ins)                                                 \
{                                                                    \
	.name = "stm_dvb_lnb",                                              \
	.id = ins,                                                          \
	.num_resources = 0,                                                 \
	.dev = {                                                            \
		.release = stm_fe_device_release,                             \
		.platform_data = &(struct stm_platdev_data) {                 \
			.ver = STM_FE_CONF_VERSION,                           \
			.config_data = NULL,                                  \
		}                                                          \
	}                                                                 \
}

#define DVB_DISEQC(ins)                                                 \
{                                                                    \
	.name = "stm_dvb_diseqc",                                              \
	.id = ins,                                                          \
	.num_resources = 0,                                                 \
	.dev = {                                                            \
		.release = stm_fe_device_release,                             \
		.platform_data = &(struct stm_platdev_data) {                  \
			.ver = STM_FE_CONF_VERSION,		\
			.config_data = NULL,                                  \
		}                                                          \
	}                                                                 \
}

#define DVB_IP(ins)                                                 \
{                                                                    \
	.name = "stm_dvb_ip",                                              \
	.id = ins,                                                          \
	.num_resources = 0,                                                 \
	.dev = {                                                            \
		.release = stm_fe_device_release,                   \
		.platform_data = &(struct stm_platdev_data) {        \
			.ver = STM_FE_CONF_VERSION,		\
			.config_data = &(struct ip_config_s) {               \
				.ip_name = STM_FE_IP_NAME_##ins,     \
				.ip_addr = STM_FE_IP_ADDRESS_##ins,           \
				.ip_port = STM_FE_IP_PORT_NO_##ins,	   \
				.protocol = STM_FE_IP_PROTOCOL_##ins,	   \
				.ethdev = STM_FE_IP_ETHDEVICE_##ins           \
			}                                            \
		}                                                    \
	}                                                            \
}

#define DVB_IPFEC(ins)                                                 \
{                                                                    \
	.name = "stm_dvb_ipfec",                                              \
	.id = ins,                                                          \
	.num_resources = 0,                                                 \
	.dev = {                                                            \
		.release = stm_fe_device_release,                   \
		.platform_data = &(struct stm_platdev_data) {        \
			.ver = STM_FE_CONF_VERSION,	\
			.config_data = &(struct ipfec_config_s) {             \
				.ipfec_name = STM_FE_IPFEC_NAME_##ins,     \
				.ipfec_addr = STM_FE_IPFEC_ADDRESS_##ins,      \
				.ipfec_port = STM_FE_IPFEC_PORT_NO_##ins,  \
				.ipfec_scheme = STM_FE_IPFEC_SCHEME_##ins  \
			}                                            \
		}                                                    \
	}                                                            \
}

#define DVB_ADD_DEVICE(no, dvbdev, name, stmfedev)  \
	do { \
		if (strlen(name)) { \
			stm_fe_devices[no++] = dvbdev;                    \
			CONFIG(*dvbdev) = CONFIG(*stmfedev);               \
			pr_info("dvb_adaptation: stm dvb device %s ...\n",\
								       name); \
		}  \
	} while (0)
#else
#define DVB_ADD_DEVICE(no, dvbdev, name, stmfedev) do {} while (0)
#endif

#ifdef CONFIG_DVB_CORE
#define DEVICE_TOTAL (ARRAY_SIZE(demod) + \
		ARRAY_SIZE(lnb) + \
		ARRAY_SIZE(diseqc) + \
		ARRAY_SIZE(ip) + \
		ARRAY_SIZE(ipfec) + \
		ARRAY_SIZE(dvb_demod) + \
		ARRAY_SIZE(dvb_lnb) + \
		ARRAY_SIZE(dvb_diseqc) + \
		ARRAY_SIZE(dvb_ip) + \
		ARRAY_SIZE(dvb_ipfec))
#else
#define DEVICE_TOTAL (ARRAY_SIZE(demod) + \
		ARRAY_SIZE(lnb) + \
		ARRAY_SIZE(diseqc) + \
		ARRAY_SIZE(ip))
#endif

void configure_frontend(void);
int stmfe_conf_sysfs_create(struct platform_device **platdev, int num);
void stmfe_conf_sysfs_remove(struct platform_device **platdev, int num);

#endif /* _STM_FE_SETUP_H */
/* End of stm_fe_setup.h  */
