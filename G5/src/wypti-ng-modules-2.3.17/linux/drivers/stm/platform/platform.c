/*
 * Frontend Platform registration
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Peter Bennett <peter.bennett@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <asm/processor.h>
#include <asm/irq-ilc.h>
#include <asm/irq.h>
#include <linux/version.h>

#include "stv0360.h"
#include "stb6000.h"
#include "dvb-pll.h"
#include "dvb_frontend.h"

struct fe_config {
	u8 demod_address;
	u8 demod_section;	/* 0 selects the 1st demodulator */
	u8 tuner_address;
	u8 tuner_id;		/* DVB PLL ID, read "dvb-pll.h" */
	u8 lnb_address;
	u8 lnb_section;		/* 0 selects the 1st LNBR */
};


static struct dvb_frontend* demod_362_attach(struct fe_config *config, struct i2c_adapter *i2c)
{
	struct dvb_frontend * new_fe = NULL;

	printk("DVB-T: DEMOD: STV0362...\n");
	
	new_fe = stv0360_attach(i2c, config->demod_address, 1);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: DEMOD: STV0362: not attached!!!\n");
	}

	return new_fe;
}

static struct dvb_frontend* tuner_attach(struct fe_config *config, struct dvb_frontend *fe, struct i2c_adapter *i2c)
{
	struct dvb_frontend * new_fe = NULL;

	printk("DVB-T: TUNER: Generic PLL: %d...\n", config->tuner_id);

	new_fe = dvb_pll_attach(fe, config->tuner_address, i2c, config->tuner_id);
	if (new_fe == NULL) {
		printk(KERN_ERR"DVB-T: TUNER: Generic PLL: %d: not attached!!!\n", config->tuner_id);
	}
	
	return new_fe;	
}


#if defined(CONFIG_SH_WYMDBOX_01)
#   warning Builing mediabox platform setup...
#   include "wymediabox.h"
#elif defined(CONFIG_SH_DORADE)
#   warning Building dorade platform setup...
#   include "dorade.h"
#elif defined(CONFIG_SH_BOSTONA)
#   warning Building bostona platform setup...
#   include "bostona.h"
#elif defined(CONFIG_SH_NANGMAA)
#   warning Building nangmaa platform setup...
#   include "nangmaa.h"
#elif defined(CONFIG_SH_NANGMAB)
#   warning Building nangmab platform setup...
#   include "nangmab.h"
#elif defined(CONFIG_SH_NANGMAC)
#   warning Building nangmac platform setup...
#   include "nangmac.h"
#elif defined(CONFIG_SH_PANDA2B)
#   warning Building panda2b platform setup...
#   include "panda2b.h"
#elif defined(CONFIG_SH_MAROONA)
#   warning Building maroona platform setup...
#   include "maroona.h"
#elif defined(CONFIG_SH_ST_CUSTOM002012)
#   warning "Building tchitcha G5+ platform setup..."
#   include "tchitcha_g5p.h"
#elif defined(CONFIG_SH_ST_CUSTOM002007)
#   warning "Building tchitcha G5 platform setup..."
#   include "tchitcha_g5.h"
#elif defined(CONFIG_SH_ST_S5210)
#   warning "Building gangnam G5 platform setup..."
#   include "gangnam_g5.h"
#elif defined(CONFIG_SH_ST_C275)
#   if defined(SWTS_ONLY)
#       warning "Building tchitcha G5 IP platform setup..."
#       include "tchitchaip.h"
#   else
#       warning "Building tchitcha G5 tnt platform setup..."
#       include "tchitnt.h"
#   endif
#elif defined(CONFIG_SH_ST_CUSTOM003019)
#   warning "Building panty platform setup..."
#   include "panty.h"
#elif defined(CONFIG_CPU_SUBTYPE_STB7100)
#include "mb442.h"
#include "hms1.h"
#elif defined(CONFIG_CPU_SUBTYPE_STX7105)
#include "mb680.h"
#include "pdk7105.h"
#include "hdk7106.h"
#include "cb180.h"
#elif defined(CONFIG_CPU_SUBTYPE_STX7108)
#include "hdk7108.h"
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
#include "hdk7111.h"
#elif defined(CONFIG_CPU_SUBTYPE_STX7141)
#include "mb628.h"
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
#include "mb520.h"
//#include "platform_7200.h"
#endif

static __init int stm_frontend_platform_init(void)
{
	printk("DVB: Frontend platform init...\n");

#ifdef BOARD_SPECIFIC_CONFIG
        register_board_drivers();     
#endif
	return 0;
}
module_init(stm_frontend_platform_init);

MODULE_DESCRIPTION("Frontend Platform Driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.9");
MODULE_LICENSE("GPL");

