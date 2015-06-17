/*
    Driver for Intel CE6353  demodulator

    Copyright (C) 2001-2002 Convergence Integrated Media GmbH
	<ralph@convergence.de>,
	<holger@convergence.de>,
	<js@convergence.de>
    

    Philips SU1278/SH

    Copyright (C) 2002 by Peter Schildmann <peter.schildmann@web.de>


    LG TDQF-S001F

    Copyright (C) 2002 Felix Domke <tmbinc@elitedvb.net>
                     & Andreas Oberritter <obi@linuxtv.org>


    Support for Samsung TBMU24112IMB used on Technisat SkyStar2 rev. 2.6B

    Copyright (C) 2003 Vadim Catana <skystar@moldova.cc>:

    Support for Philips SU1278 on Technotrend hardware

    Copyright (C) 2004 Andrew de Quincey <adq_dvb@lidskialf.net>

    Copyright (C) 2009 Hu Gang <hug@wyplayasia.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#include "semstbcp.h"
#include "semosal.h"
#include "mxl5007-ce6353.h"
#include "sembsl_mxl5007.h"
#include "semhal_fe.h"
#include "sembsl_ce6353.h"
#include "sembsl_mxl5007ce6353.h"
#include "semapal_fe_simplex.h"
#include <dvb/frontend_status.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
#include <linux/stm/pio.h>
#else   /* STLinux 2.2 or older */
#include <linux/stpio.h>
#endif
#include <asm/io.h>

//#define __SWITCH_ON_POWER
#ifdef __SWITCH_ON_POWER
#warning defined micro __SWITCH_ON_POWER
#endif

#define TUNER_STANDBY_PIO_PORT	4
#define TUNER_STANDBY_PIO_PIN	2

#define ANT_POWER_PIO_PORT	4
#define ANT_POWER_PIO_PIN	6
static int debug = 0;

#define dprintk(fmt, args...) \
	do { \
		if (debug) printk(KERN_DEBUG "%s: "fmt, __FUNCTION__, ## args); \
	} while (0)


struct semsosal_fe_ops 
{
	int (*init)(u32 unit, struct semosal_i2c_obj *p_i2c);

	int (*deinit)(u32 unit);

	int (*get_sw_version)(u32 unit, u32 *ver);

	int (*get_fe_info)(u32 unit, struct semhal_frontend_info_s *p_fe_info, u32 *size);

	int (*tune)(u32 unit, u32 freq_khz, u32 bw, u32 pr);

	BOOL (*get_lock_status)(u32 unit);

	int (*get_signal_strength)(u32 unit, u32 *val);

	int (*get_snr)(u32 unit, u32 *val);

	int (*get_ber)(u32 unit, u32 *val);

	int (*get)(u32 unit, u32 item, u32 *val);

	int (*set)(u32 unit, u32 item, u32 val);
};

static struct semsosal_fe_ops sem_ops = {
	.init = 		SEMFE_Init,
	.deinit = 		SEMFE_DeInit,
	.get_sw_version = 	SEMFE_GetSWVersion,
	.get_fe_info = 		SEMFE_GetFEInfo,
	.tune = 		SEMFE_Tune_T,
	.get_lock_status = 	SEMFE_GetLockStatus,
	.get_signal_strength = 	SEMFE_GetSignalStrength,
	.get_snr = 		SEMFE_GetSNRQuality,
	.get_ber = 		SEMFE_GetBERQuality,
	.get = 			SEMFE_Get,
	.set = 			SEMFE_Set
};

static struct semsosal_fe_ops *intel_get_fe_instance(void)
{
	return &sem_ops;
}

// i2c operation functions declaration
static int intel_i2c_open(struct semosal_i2c_obj *p_obj, U32 addr, U32 addr_10bits);
static int intel_i2c_close(struct semosal_i2c_obj *p_obj);
static int intel_i2c_lock(struct semosal_i2c_obj *p_obj, BOOL lflag);
static int intel_i2c_readwrite(struct semosal_i2c_obj *p_obj, U32 mode, U8 *pbytes, U32 nbytes);

static struct semosal_i2c_obj g_i2c_object[1] = {
	{
		.open = intel_i2c_open,
		.close = intel_i2c_close,
		.lock = intel_i2c_lock,
		.readwrite = intel_i2c_readwrite,
		.h_i2c_handle = (HI2C_T)0,
		.p_data = (void *)0,
	}
};

struct attached_frontend {
	struct intel_state* state;
	struct list_head list;
};

/* repository of attached frontends */
static struct attached_frontend attached_frontends  = {NULL, LIST_HEAD_INIT(attached_frontends.list)};

static void intel_set_i2c(struct intel_state *state)
{
	g_i2c_object[0].p_data = (void *)state;
}

/*static void intel_wait(unsigned short millisecond)
{
	msleep(millisecond);
}*/

static int intel_i2c_open(struct semosal_i2c_obj *p_obj, U32 addr, U32 addr_10bits)
{
	return SEM_SOK;
}

static int intel_i2c_close(struct semosal_i2c_obj *p_obj)
{
	p_obj=p_obj;

	return SEM_SOK;
}

static int intel_i2c_lock(struct semosal_i2c_obj *p_obj, BOOL lflag)
{
	p_obj=p_obj;
	lflag=lflag;

	return SEM_SOK;
}

static int intel_i2c_readwrite(struct semosal_i2c_obj *p_obj, U32 mode, U8 *pbytes, U32 nbytes)
{
	switch (mode)
	{
	case SEM_I2C_READ:
		break;

	case SEM_I2C_WRITE:
	case SEM_I2C_WRITENOSTOP:
		break;
		
	default:
		goto exit;
	}

	return SEM_SOK;

exit:
	return SEM_EI2C_ERR;
}

static int intel_writereg (struct intel_state* state, u8 reg, u8 data)
{
	int ret  = 0;
	u8 buf [] = { reg, data };

	struct i2c_msg msg = { .addr = state->demod_address, .flags = 0, .buf = buf, .len = 2 };

	ret = i2c_transfer (state->demod_i2c, &msg, 1);

	if (ret != 1)
	{
		printk("%s: error (addr == 0x%02x,reg == 0x%02x, val == 0x%02x, ret == %i)\n", __FUNCTION__, msg.addr,reg, data, ret);
	}

	return (ret != 1) ? -1 : 0;
}

static int intel_readreg (struct intel_state* state, u8 reg)
{
	int ret;
	u8 b0 [] = { reg };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { .addr = state->demod_address, .flags = 0, .buf = b0, .len = 1 },
				{ .addr = state->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };

	ret = i2c_transfer (state->demod_i2c, msg, 2);

	if (ret != 2) 
	{
		printk("%s: error (reg == 0x%02x,  ret == %i)\n", __FUNCTION__, reg, ret);
		return -EIO;
	}

	return (int)b1[0];
}

void intel_set_i2c_bypass(struct intel_state *state, int val)
{
	if (val)
	{
		intel_writereg(state, 0x62, 0x12);
	}
	else
	{
		intel_writereg(state, 0x62, 0x0A);
	}
}

#define REG_TPS_GIVEN_1	0x6E
#define REG_TPS_GIVEN_0	0x6F
#define REG_ACQ_CTL	0x6F

// TO BE TESTED
//static int set_hierarchy(struct intel_state *state, const fe_hierarchy_t hierarchy_info)
//{
//
//	/* NOTE: alpha={1,2,4} is the constellation offset */
//	u8 hierarchy;
//	u8 tps_given_1;
//	int ret;
//
//	switch (hierarchy_info) {
//		case HIERARCHY_NONE:
//			hierarchy = 0;
//			break;
//		case HIERARCHY_1:
//			hierarchy = 1;
//			break;
//		case HIERARCHY_2:
//			hierarchy = 2;
//			break;
//		case HIERARCHY_4:
//			hierarchy = 3;
//			break;
//		default:
//			printk("%s: hierarchy auto is not supported\n", __FUNCTION__);
//			return 1;
//	}
//	tps_given_1 = (u8)intel_readreg(state, REG_TPS_GIVEN_1);
//	/* masks the 3 bits reserved for hierarchy */
//	tps_given_1 &= ~(0x7 << 2);
//	/* sets the hierarchy */
//	tps_given_1 |= (hierarchy << 2);
//	ret = intel_writereg(state, REG_TPS_GIVEN_1, tps_given_1);
//	if (ret != 0) {
//		return ret;
//	}
//
//	/* enables automatic search of guard ratio and transmission mode */
//	/*return intel_writereg(state, REG_ACQ_CTL, 0);*/
//}

/*static int intel_set_tps (struct intel_state *state, struct dvb_ofdm_parameters *p)
{
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	intel_set_i2c(state);

	if ((p->code_rate_HP < FEC_1_2) || ((p->code_rate_HP > FEC_7_8) && (p->code_rate_HP != FEC_AUTO)))
	{
		return -EINVAL;
	}

	if ((p->code_rate_LP < FEC_1_2) || ((p->code_rate_LP > FEC_7_8) && (p->code_rate_LP != FEC_AUTO)))
	{
		return -EINVAL;
	}
	
	if ((p->code_rate_HP == FEC_4_5) || (p->code_rate_LP == FEC_4_5))
	{
		return -EINVAL;
	}

	if ((p->guard_interval < GUARD_INTERVAL_1_32) ||
	  ((p->guard_interval > GUARD_INTERVAL_1_4) && (p->guard_interval != GUARD_INTERVAL_AUTO)))
	{
		return -EINVAL;
	}

	if ((p->transmission_mode != TRANSMISSION_MODE_2K) &&
	   (p->transmission_mode != TRANSMISSION_MODE_8K) &&
	   (p->transmission_mode != TRANSMISSION_MODE_AUTO))
	{
		return -EINVAL;
	}

	if ((p->constellation != QPSK) &&
		(p->constellation != QAM_16) &&
		(p->constellation != QAM_64))
	{
		 return -EINVAL;
	}

	if ((p->hierarchy_information < HIERARCHY_NONE) ||
	   (p->hierarchy_information > HIERARCHY_4))
	{
		return -EINVAL;
	}

	if ((p->bandwidth > BANDWIDTH_6_MHZ) || (p->bandwidth < BANDWIDTH_8_MHZ))
	{
		printk(KERN_INFO "we don't support %d M bandwidth(only 6MHZ, 7MHZ and 8MHZ\n", p->bandwidth);
		return -EINVAL;
	}
	
	ret = obj->set(0, SEMHAL_FE_STATE_BW, (8 - p->bandwidth) *1000000);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}
	return set_hierarchy(state, p->hierarchy_information);
}*/

static int intel_get_tps (struct intel_state* state, struct dvb_ofdm_parameters *p)
{
	static const fe_modulation_t qam_tab [3] = { QPSK, QAM_16, QAM_64 };
	static const fe_code_rate_t fec_tab [6] = { FEC_1_2, FEC_2_3, FEC_3_4,FEC_5_6, FEC_7_8, FEC_8_9};
	struct semsosal_fe_ops *obj = intel_get_fe_instance();
	u32 val;
	int ret;

	intel_set_i2c(state);

	ret = obj->get(0, SEMHAL_FE_STATE_HR, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_HIERARCHY_NONE) && 
		(val != SEMHAL_FE_HIERARCHY_1) && 
		(val != SEMHAL_FE_HIERARCHY_2) && 
		(val != SEMHAL_FE_HIERARCHY_4))
	{
		 printk(KERN_ERR "%s: Invalid Hierarchy detected\n",__FUNCTION__);
	}
	else
	{
		switch (val)
		{
		case SEMHAL_FE_HIERARCHY_NONE:
			dprintk("found SEMHAL_FE_HIERARCHY_NONE\n");
			p->hierarchy_information = HIERARCHY_NONE;
			break;
		case SEMHAL_FE_HIERARCHY_1:
			dprintk("found SEMHAL_FE_HIERARCHY_1\n");
			p->hierarchy_information = HIERARCHY_1;
			break;
		case SEMHAL_FE_HIERARCHY_2:
			dprintk("found SEMHAL_FE_HIERARCHY_2\n");
			p->hierarchy_information = HIERARCHY_2;
			break;
//		case SEMHAL_FE_HIERARCHY_4:
		default:
			dprintk("found SEMHAL_FE_HIERARCHY_4\n");
			p->hierarchy_information = HIERARCHY_4;
			break;
		}
	}

	ret = obj->get(0, SEMHAL_FE_STATE_MODULATION, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_QAM_4) &&
		(val != SEMHAL_FE_QAM_16) &&
		(val != SEMHAL_FE_QAM_64))
	{
		printk(KERN_ERR "%s: Invalid Modulation detected\n",__FUNCTION__);
	}
	else
	{
		switch (val)
		{
		case SEMHAL_FE_QAM_4:
			p->constellation = qam_tab[0];
			dprintk("found SEMHAL_FE_QAM_4\n");
			break;
		case SEMHAL_FE_QAM_16:
			p->constellation = qam_tab[1];
			dprintk("found SEMHAL_FE_QAM_16\n");
			break;
//		case SEMHAL_FE_QAM_64:
		default:
			p->constellation = qam_tab[2];
			dprintk("found SEMHAL_FE_QAM_64\n");
			break;
		}
	}
															
	ret = obj->get(0, SEMHAL_FE_STATE_CR, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_FEC_1_2) &&
		(val != SEMHAL_FE_FEC_2_3) &&
		(val != SEMHAL_FE_FEC_3_4) &&
		(val != SEMHAL_FE_FEC_5_6) &&
		(val != SEMHAL_FE_FEC_7_8) &&
		(val != SEMHAL_FE_FEC_8_9))
	{
		printk(KERN_ERR "%s: Invalid FEC rate 0x%02x detected\n", __FUNCTION__, val);
	}
	else
	{
		switch (val)
		{
		case SEMHAL_FE_FEC_1_2:
			p->code_rate_HP = fec_tab[0];
			dprintk(" found HP SEMHAL_FE_FEC_1_2\n");
			break;
		case SEMHAL_FE_FEC_2_3:
			p->code_rate_HP = fec_tab[1];
			dprintk(" found HP SEMHAL_FE_FEC_2_3\n");
			break;
		case SEMHAL_FE_FEC_3_4:
			p->code_rate_HP = fec_tab[2];
			dprintk(" found HP SEMHAL_FE_FEC_3_4\n");
			break;
		case SEMHAL_FE_FEC_5_6:
			p->code_rate_HP = fec_tab[3];
			dprintk(" found HP SEMHAL_FE_FEC_5_6\n");
			break;
		case SEMHAL_FE_FEC_7_8:
			p->code_rate_HP = fec_tab[4];
			dprintk(" found HP SEMHAL_FE_FEC_7_8\n");
			break;
//		case SEMHAL_FE_FEC_8_9:
		default:
			p->code_rate_HP = fec_tab[5];
			dprintk(" found HP SEMHAL_FE_FEC_8_9\n");
			break;
		}
	}

	ret = obj->get(0, SEMHAL_FE_STATE_CRLP, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_FEC_1_2) &&
		(val != SEMHAL_FE_FEC_2_3) &&
		(val != SEMHAL_FE_FEC_3_4) &&
		(val != SEMHAL_FE_FEC_5_6) &&
		(val != SEMHAL_FE_FEC_7_8) &&
		(val != SEMHAL_FE_FEC_8_9))
	{
		printk(KERN_ERR "%s: Invalid FEC rate detected\n", __FUNCTION__);
	}
	else
	{
		switch (val)
		{
		case SEMHAL_FE_FEC_1_2:
			p->code_rate_HP = fec_tab[0];
			dprintk(" found LP SEMHAL_FE_FEC_1_2\n");
			break;
		case SEMHAL_FE_FEC_2_3:
			p->code_rate_HP = fec_tab[1];
			dprintk(" found LP SEMHAL_FE_FEC_2_3\n");
			break;
		case SEMHAL_FE_FEC_3_4:
			p->code_rate_HP = fec_tab[2];
			dprintk(" found LP SEMHAL_FE_FEC_3_4\n");
			break;
		case SEMHAL_FE_FEC_5_6:
			p->code_rate_HP = fec_tab[3];
			dprintk(" found LP SEMHAL_FE_FEC_5_6\n");
			break;
		case SEMHAL_FE_FEC_7_8:
			p->code_rate_HP = fec_tab[4];
			dprintk(" found LP SEMHAL_FE_FEC_7_8\n");
			break;
//		case SEMHAL_FE_FEC_8_9:
		default:
			p->code_rate_HP = fec_tab[5];
			dprintk(" found LP SEMHAL_FE_FEC_8_9\n");
			break;
		}
	}

	ret = obj->get(0, SEMHAL_FE_STATE_GI, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_GI_1_32) &&
		(val != SEMHAL_FE_GI_1_16) &&
		(val != SEMHAL_FE_GI_1_8) &&
		(val != SEMHAL_FE_GI_1_4) &&
		(val != SEMHAL_FE_GI_NONE))
	{
		printk(KERN_ERR "%s: Invalid guard interval detected\n", __FUNCTION__);
	}
	else
	{
		switch (val)
		{
		case SEMHAL_FE_GI_1_32:
			p->guard_interval = GUARD_INTERVAL_1_32;
			dprintk(" found SEMHAL_FE_GI_1_32\n");
			break;
		case SEMHAL_FE_GI_1_16:
			p->guard_interval = GUARD_INTERVAL_1_16;
			dprintk(" found SEMHAL_FE_GI_1_16\n");
			break;
		case SEMHAL_FE_GI_1_8:
			p->guard_interval = GUARD_INTERVAL_1_8;
			dprintk(" found SEMHAL_FE_GI_1_8\n");
			break;
		case SEMHAL_FE_GI_1_4:
			p->guard_interval = GUARD_INTERVAL_1_4;
			dprintk(" found SEMHAL_FE_GI_1_4\n");
			break;
//		case SEMHAL_FE_GI_NONE:
		default:
			printk(KERN_ERR "%s: Invalid GI mode GI_NONE\n", __FUNCTION__);
			break;
		}
	}

	ret = obj->get(0, SEMHAL_FE_STATE_FFT, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val != SEMHAL_FE_FFT_8K) &&
		(val != SEMHAL_FE_FFT_2K))
	{
		printk(KERN_ERR "%s: Invalid transmission mode detected\n",__FUNCTION__);
	}
	else
	{
		if (val == SEMHAL_FE_FFT_8K)
		{
			p->transmission_mode = TRANSMISSION_MODE_8K;
			dprintk(" found SEMHAL_FE_FFT_8K\n");
		}
		else
		{
			p->transmission_mode = TRANSMISSION_MODE_2K;
			dprintk(" found SEMHAL_FE_FFT_2K\n");
		}
	}
	
	return 0;																																				
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int intel_init(struct dvb_frontend* fe)
{
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();
/*#ifdef __SWITCH_ON_POWER
	struct stpio_pin *pin = (struct stpio_pin*)NULL;
#endif*/

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

/*#ifdef  __SWITCH_ON_POWER
	pin = stpio_request_pin(TUNER_STANDBY_PIO_PORT, TUNER_STANDBY_PIO_PIN,"Tuner Standby", STPIO_OUT);
	if (pin)
	{
		stpio_set_pin(pin, 1);
	}
	else
	{
		printk(KERN_ERR "%s couldn't get pin for reset\n", __FUNCTION__);
	}
#endif*/
	ret = obj->deinit(0);
	if((ret != SEM_SOK) && (ret != SEM_ENOT_INITIALIZED))
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	ret = obj->init(0, &g_i2c_object[0]);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	return 0;
}

static int intel_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	u32 val;
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	*status = 0;

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

	ret = obj->get(0, SEMHAL_FE_STATE_LOCK, &val);
	if (ret != SEM_SOK)
	{
		dprintk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	if ((val & SEMHAL_FE_HAS_SIGNAL) == SEMHAL_FE_HAS_SIGNAL)
	{
		*status |= FE_HAS_SIGNAL;
		dprintk("%s: FE_HAS_SIGNAL was found\n", __FUNCTION__);
	}
	else
	{
		dprintk(" %s: FE_HAS_SIGNAL not found!\n", __FUNCTION__);
	}

	if ((val & SEMHAL_FE_HAS_SYNC) == SEMHAL_FE_HAS_SYNC)
	{
		*status |= FE_HAS_SYNC;
		dprintk("%s: FE_HAS_SYNC was found\n", __FUNCTION__);
	}
	else
	{
		dprintk("%s: FE_HAS_SYNC not found!\n", __FUNCTION__);
	}

	if ((val & SEMHAL_FE_HAS_CARRIER) == SEMHAL_FE_HAS_CARRIER)
	{
		*status |= FE_HAS_CARRIER;
		dprintk("%s: FE_HAS_CARRIER was found\n", __FUNCTION__);
	}
	else
	{
		dprintk("%s: FE_HAS_CARRIER not found!\n", __FUNCTION__);
	}

	if ((val & SEMHAL_FE_HAS_VITERBI) == SEMHAL_FE_HAS_VITERBI)
	{
		*status |= FE_HAS_VITERBI;
		dprintk("%s: FE_HAS_VITERBI was found\n", __FUNCTION__);
	}
	else
	{
		dprintk(" %s: FE_HAS_VITERBI not found!\n", __FUNCTION__);
	}

	if ((val & SEMHAL_FE_HAS_LOCK) == SEMHAL_FE_HAS_LOCK)
	{
		*status |= FE_HAS_LOCK;
		dprintk("%s: FE_HAS_LOCK was found\n", __FUNCTION__);
	}
	else
	{
		dprintk("%s :FE_HAS_LOCK not found!\n", __FUNCTION__);
	}

	return 0;
}

static int intel_read_signal_strength(struct dvb_frontend* fe, u16* signal_strength)
{
	u32 val;
	int ret;

	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	*signal_strength = 0;

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

	ret = obj->get_signal_strength(0, &val);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	*signal_strength = (u16)val;
	return 0;
}
 
static int intel_read_snr(struct dvb_frontend* fe, u16* snr)
{
	u32 val;
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	*snr = 0;

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

	ret = obj->get_snr(0, &val);
	if (ret != SEM_SOK)
	{
		dprintk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	*snr = val;
	return 0;
}

static int intel_read_ber(struct dvb_frontend* fe, u32 *ber)
{
	u32 val;
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	*ber = 0;

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

	ret = obj->get_ber(0, &val);
	if (ret != SEM_SOK)
	{
		dprintk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	*ber = val;
	return 0;
}

static int intel_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	u32 value;
	int ret;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();

	*ucblocks = 0;

	intel_set_i2c((struct intel_state *)fe->demodulator_priv);

	ret = obj->get(0, SEMHAL_FE_STATE_UCB, &value);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	*ucblocks = value;
	return 0;
}

static int intel_enable_high_lnb_voltage(struct dvb_frontend* fe,long arg)
{
	struct stpio_pin *pin = (struct stpio_pin *)NULL;
	
	pin = stpio_request_set_pin(ANT_POWER_PIO_PORT, ANT_POWER_PIO_PIN,"Antenna nSupply", STPIO_OUT, arg);

	if (!pin)
	{
		printk(KERN_ERR "%s couldn't get pin for antenna power supply!\n", __FUNCTION__);
	}
	else
	{
		stpio_free_pin(pin);
	}

	return 0;
}

static int intel_set_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct intel_state* state = (struct intel_state*) fe->demodulator_priv;
	struct semsosal_fe_ops *obj = intel_get_fe_instance();
	int ret;

	intel_set_i2c(state);
	dprintk("%s current frequency is %d MHz, bandwidth is %d MHz\n", 
		__FUNCTION__, p->frequency/1000000, (8 - p->u.ofdm.bandwidth));

	ret = obj->tune(0, p->frequency/1000, (8 - p->u.ofdm.bandwidth) * 1000000, SEMHAL_FE_HP);
	if (ret != SEM_SOK)
	{
		printk("%s failed in line %d, return value is 0x%x\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	dprintk("%s succeeded!\n", __FUNCTION__);
	return 0;
}

static int intel_get_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct intel_state* state = (struct intel_state*) fe->demodulator_priv;

	return intel_get_tps (state, &p->u.ofdm);
}

static int intel_get_tune_settings(struct dvb_frontend* fe, struct dvb_frontend_tune_settings* fesettings)
{
	fesettings->min_delay_ms = 250;
	fesettings->step_size = 0;
	fesettings->max_drift = 166667 * 2;
    	
	return 0;
}

static void intel_release(struct dvb_frontend* fe)
{
	struct intel_state* state = (struct intel_state*) fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops intel_ops;

struct dvb_frontend* intel_attach(const struct intel_config* config,
				    struct i2c_adapter* i2c, u8 demod_address)
{
	struct intel_state* state = NULL;
	MxL5007_TunerConfigS tuner;
	u32 pid = 0;
	/* new attached frontend to be inserted into the repository */
	struct attached_frontend *new_frontend;
	U32 err;
#ifdef __SWITCH_ON_POWER
	struct stpio_pin *pin = (struct stpio_pin*)NULL;
#endif

	/* allocate memory for the internal state */
	state = (struct intel_state*) kmalloc(sizeof(struct intel_state), GFP_KERNEL);
	if (state == NULL) {
		printk(KERN_ERR "%s Out of memory\n", __FUNCTION__);
		goto error;
	}
	/* setup the state */
	state->config = config;
	state->demod_i2c = i2c;
	state->demod_address = demod_address;

	memcpy(&state->ops, &intel_ops, sizeof(struct dvb_frontend_ops));

#ifdef  __SWITCH_ON_POWER
	pin = stpio_request_pin(TUNER_STANDBY_PIO_PORT, TUNER_STANDBY_PIO_PIN,"Tuner Standby", STPIO_OUT);
	if (pin)
	{
		stpio_set_pin(pin, 1);
		printk(KERN_INFO "%s succeeded in set gpio\n", __FUNCTION__);
		msleep(1000);
	}
	else
	{
		printk(KERN_ERR "%s couldn't get pin for reset\n", __FUNCTION__);
	}
#endif
	intel_set_i2c(state);
	tuner.i2c.p_data = (void *)state;

	/* check if the tuner is there */
	intel_set_i2c_bypass(state, 1);
	err = MxL5007_Check_ChipVersion(&tuner, &pid);
	intel_set_i2c_bypass(state, 0);
	state->version = pid;

 	if ((err != MxL_OK) || (pid != 0x14)) {
		printk(KERN_ERR "%s MaxLinear 5007 not detected (pid 0x%x)\n", __FUNCTION__, pid);
		goto error;
	} else {
		printk("MaxLinear 5007 detected\n");
	}

	/* check if the dmod is there */
	pid = 0;
	pid = intel_readreg(state, 0x7f);
	if (pid != 0x14) {
		printk(KERN_ERR "%s Intel CE6353 not detected (pid 0x%x)\n", __FUNCTION__, pid);
		goto error;
	} else {
		printk("Intel CE6353 detected\n");
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
        state->frontend.ops = state->ops;
        state->frontend.demodulator_priv = state;
#else /* STLinux 2.2 or older */
	/* create dvb_frontend */
        state->frontend.ops = &state->ops;
        state->frontend.demodulator_priv = state;
#endif

	/* adds the front end to the repository */
	new_frontend = (struct attached_frontend*) kmalloc(sizeof(struct attached_frontend), GFP_KERNEL);
	if (new_frontend == NULL) {
		printk(KERN_ERR "%s out of memory\n", __FUNCTION__);
		goto error;
	}
	new_frontend->state = state;
	list_add_tail(&(new_frontend->list), &(attached_frontends.list));

	return &state->frontend;

error:
	if (state) kfree(state);
	return NULL;
}

/**
 * dvb_frontend_get_count - Gets the number of attached frontends.
 * @count number of attached frontends
 */
static int dvb_frontend_get_count(int *count)
{
	struct list_head *pos;
	*count = 0;

	__list_for_each(pos, &attached_frontends.list) {
		*count += 1;
	}
	return 0;
}

#define REG_CHIP_ID	0x7F
/**
 * dvb_frontend_get_status - Gets the status of specified front end.
 * @frontend_num Frontend number (id)
 * @query Type of query
 * @result Result of query
 */
static int dvb_frontend_get_status(int frontend_num, frontend_query_t query, int *result)
{
	struct list_head *pos;

	list_for_each(pos, &attached_frontends.list) {
		struct intel_state* state;
		struct attached_frontend *current_frontend = NULL;

		current_frontend = list_entry(pos, struct attached_frontend, list);
		state = current_frontend->state;
		//printk("dmod address 0x%x, tuner address 0x%x, adapter 0x%x, frontend 0x%x):\n", state->demod_address, state->config->tuner_address, state->frontend.dvb->num, state->frontend.id);
		if (state->frontend.id == frontend_num) {
			/* found the right front end */
			switch (query) {
				case STATUS_DMOD:
				{
					*result = (intel_readreg(state, REG_CHIP_ID) == 0x14);
					return 0;
				}
				case STATUS_TUNER:
				{
					u32 pid;
					MxL5007_TunerConfigS tuner;

					tuner.i2c.p_data = (void *)state;
					intel_set_i2c_bypass(state, 1);
					MxL5007_Check_ChipVersion(&tuner, &pid);
					intel_set_i2c_bypass(state, 0);
					*result = (pid == 0x14);
					return 0;
				}
				default:
					printk("%s(): unknown query", __func__);
					return 1;
			}
		}
	}
	printk("%s(): unknown frontend num", __func__);
	return 1;
}

static struct dvb_frontend_ops intel_ops = {

	.info = {
		.name 			= "Maxlinear5007 with Intel6353",
		.type 			= FE_OFDM,
		.frequency_min 		= 170000000,
		.frequency_max 		= 860000000,
	    	.frequency_stepsize 	= 166667,
		.caps =  FE_CAN_FEC_AUTO |
		      FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 | FE_CAN_TRANSMISSION_MODE_AUTO |
		      FE_CAN_GUARD_INTERVAL_AUTO |
	          FE_CAN_RECOVER
	},

	.release = intel_release,

	.init = intel_init,

	.enable_high_lnb_voltage = intel_enable_high_lnb_voltage,

	.set_frontend = intel_set_frontend,
	.get_frontend = intel_get_frontend,
	.get_tune_settings = intel_get_tune_settings,

	.read_status = intel_read_status,
	.read_signal_strength = intel_read_signal_strength,
	.read_snr = intel_read_snr,
	.read_ber = intel_read_ber,
	.read_ucblocks = intel_read_ucblocks,
};

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");

MODULE_DESCRIPTION("Maxlinear5007 with Inter CE6353 full nim driver");
MODULE_AUTHOR("Hu Gang");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(intel_attach);
EXPORT_SYMBOL(dvb_frontend_get_status);
EXPORT_SYMBOL(dvb_frontend_get_count);
