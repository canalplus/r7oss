/*
 * arch/common/stm_fe_setup.c
 *
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author:Rahul V
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <linux/stm/ip.h>
#include <linux/stm/ipfec.h>
#include <linux/stm/plat_dev.h>
#include <stm_fe_conf.h>
#include "stm_fe_setup.h"

#if (STM_FE_CONF_VERSION == 1)
static struct platform_device demod[] = {
	DEMOD(0),
	DEMOD(1),
	DEMOD(2),
	DEMOD(3),
	DEMOD(4),
	DEMOD(5),
	DEMOD(6),
	DEMOD(7),
};

static struct platform_device lnb[] = {
	LNB(0),
	LNB(1),
	LNB(2),
	LNB(3),
	LNB(4),
	LNB(5),
	LNB(6),
	LNB(7),
};

static struct platform_device diseqc[] = {
	DISEQC(0),
	DISEQC(1),
	DISEQC(2),
	DISEQC(3),
	DISEQC(4),
	DISEQC(5),
	DISEQC(6),
	DISEQC(7),
};

static struct platform_device ip[] = {
	IP(0),
	IP(1),
	IP(2),
	IP(3),
	IP(4),
	IP(5),
};

static struct platform_device ipfec[] = {
	IPFEC(0),
	IPFEC(1),
	IPFEC(2),
	IPFEC(3),
	IPFEC(4),
	IPFEC(5),
};

#ifdef CONFIG_DVB_CORE
static struct platform_device dvb_demod[] = {
	DVB_DEMOD(0),
	DVB_DEMOD(1),
	DVB_DEMOD(2),
	DVB_DEMOD(3),
	DVB_DEMOD(4),
	DVB_DEMOD(5),
	DVB_DEMOD(6),
	DVB_DEMOD(7),
};

static struct platform_device dvb_lnb[] = {
	DVB_LNB(0),
	DVB_LNB(1),
	DVB_LNB(2),
	DVB_LNB(3),
	DVB_LNB(4),
	DVB_LNB(5),
	DVB_LNB(6),
	DVB_LNB(7),
};

static struct platform_device dvb_diseqc[] = {
	DVB_DISEQC(0),
	DVB_DISEQC(1),
	DVB_DISEQC(2),
	DVB_DISEQC(3),
	DVB_DISEQC(4),
	DVB_DISEQC(5),
	DVB_DISEQC(6),
	DVB_DISEQC(7),
};

static struct platform_device dvb_ip[] = {
	DVB_IP(0),
	DVB_IP(1),
	DVB_IP(2),
	DVB_IP(3),
	DVB_IP(4),
	DVB_IP(5),
};

static struct platform_device dvb_ipfec[] = {
	DVB_IPFEC(0),
	DVB_IPFEC(1),
	DVB_IPFEC(2),
	DVB_IPFEC(3),
	DVB_IPFEC(4),
	DVB_IPFEC(5),
};
#endif
#endif

static int stm_fe_dev_no;
struct platform_device *stm_fe_devices[DEVICE_TOTAL];

static int demod_reset(uint32_t reset_pio)
{
	/* Reset the PIO lines connected the demod */
	if (reset_pio != DEMOD_RESET_PIO_NONE) {
		gpio_request(reset_pio, "Nim Reset");
		gpio_direction_output(reset_pio, 0);
		msleep(20);
		gpio_direction_output(reset_pio, 1);
	}
	return 0;
}

void configure_frontend(void)
{
	int ret, id;

	for (id = 0; id < ARRAY_SIZE(demod); id++) {
		struct demod_config_s *demod_conf = CONFIG(demod[id]);

		STM_FE_ADD_DEVICE(stm_fe_dev_no, &demod[id],
							demod_conf->demod_name);
		DVB_ADD_DEVICE(stm_fe_dev_no, &dvb_demod[id],
							demod_conf->demod_name,
							&demod[id]);
		demod_reset(demod_conf->reset_pio);
	}
	for (id = 0; id < ARRAY_SIZE(lnb); id++) {
		struct lnb_config_s *lnb_conf = CONFIG(lnb[id]);

		STM_FE_ADD_DEVICE(stm_fe_dev_no, &lnb[id], lnb_conf->lnb_name);
		DVB_ADD_DEVICE(stm_fe_dev_no, &dvb_lnb[id], lnb_conf->lnb_name,
								&lnb[id]);
	}
	for (id = 0; id < ARRAY_SIZE(diseqc); id++) {
		struct diseqc_config_s *diseqc_conf = CONFIG(diseqc[id]);

		STM_FE_ADD_DEVICE(stm_fe_dev_no, &diseqc[id],
						diseqc_conf->diseqc_name);
		DVB_ADD_DEVICE(stm_fe_dev_no, &dvb_diseqc[id],
					diseqc_conf->diseqc_name, &diseqc[id]);
	}
	for (id = 0; id < ARRAY_SIZE(ip); id++) {
		struct ip_config_s *ip_conf = CONFIG(ip[id]);

		STM_FE_ADD_DEVICE(stm_fe_dev_no, &ip[id], ip_conf->ip_name);
		DVB_ADD_DEVICE(stm_fe_dev_no, &dvb_ip[id], ip_conf->ip_name,
								&ip[id]);
	}
	for (id = 0; id < ARRAY_SIZE(ipfec); id++) {
		struct ipfec_config_s *ipfec_conf = CONFIG(ipfec[id]);

		STM_FE_ADD_DEVICE(stm_fe_dev_no, &ipfec[id],
							ipfec_conf->ipfec_name);
		DVB_ADD_DEVICE(stm_fe_dev_no, &dvb_ipfec[id],
					ipfec_conf->ipfec_name, &ipfec[id]);
	}
	ret = platform_add_devices(stm_fe_devices, stm_fe_dev_no);
	if (ret)
		pr_err("%s: platform_add_devices() = 0x%x\n", __func__, ret);

	ret = stmfe_conf_sysfs_create(stm_fe_devices, stm_fe_dev_no);
	if (ret)
		pr_err("%s: stmfe_conf_sysfs_create fail: %d\n", __func__, ret);

	return;
}

void stm_fe_device_release(struct device *dev)
{
  /* Dummy release function */
}
static int32_t __init stm_fe_setup_init(void)
{
	pr_info("Loading module stm_fe_platform ...\n");
	configure_frontend();

	return 0;
}

module_init(stm_fe_setup_init);

static void __exit stm_fe_setup_term(void)
{
	int cnt = 0;

	pr_info("Removing module stm_fe_platform ...\n");
	stmfe_conf_sysfs_remove(stm_fe_devices, stm_fe_dev_no);

	for (cnt = 0; cnt < stm_fe_dev_no; cnt++)
		platform_device_unregister(stm_fe_devices[cnt]);

}

module_exit(stm_fe_setup_term);

MODULE_DESCRIPTION("Platform device config for frontend devices on non-devicetree boards");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
