#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "dvb_frontend.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
#include <linux/stm/pio.h>
#else	/* STLinux 2.2 or older */
#include <linux/stpio.h>        /*!< for stpio_set_pin */
#endif

#include "mxl101sf.h"
#include "./src/MxL101SF_PhyDefs.h"
#include "./src/MxL101SF_PhyCtrlApi.h"
#include "./src/MxL101SF_OEM_Drv.h"

struct mxl101sf_state {
	struct mxl101sf_config*	config;
	u8			demod_address;
	struct i2c_adapter*	i2c;
	struct dvb_frontend_ops ops;
	struct dvb_frontend	frontend;
};

struct mxl101sf_api_ops
{
	MXL_STATUS (*get_device_status)(MXL_CMD_TYPE_E type, void *param);
	MXL_STATUS (*get_demod_status)(MXL_CMD_TYPE_E type, void *param);
	MXL_STATUS (*config_demod)(MXL_CMD_TYPE_E type, void *param);
	MXL_STATUS (*get_tuner_status)(MXL_CMD_TYPE_E type, void *param);
	MXL_STATUS (*config_tuner)(MXL_CMD_TYPE_E type, void *param);
	MXL_STATUS (*config_device)(MXL_CMD_TYPE_E type, void *param);
};

static int debug = 0;
static int trace = 0;

static struct mxl101sf_api_ops g_mxl101sf_ops = {
	.get_device_status	=	MxLWare_API_GetDeviceStatus,
	.get_demod_status	=	MxLWare_API_GetDemodStatus,
	.config_demod		=	MxLWare_API_ConfigDemod,
	.get_tuner_status	=	MxLWare_API_GetTunerStatus,
	.config_tuner		=	MxLWare_API_ConfigTuner,
	.config_device		=	MxLWare_API_ConfigDevice
};

/*
 * MACROS
 */
#define dprintk(fmt,args...) \
	do { \
		if (debug) printk(KERN_DEBUG "%s(): " fmt, __func__, ## args); \
	} while (0)
#define wyprintk(fmt,args...) \
	do { \
		if (trace) printk(fmt, ## args); \
	} while (0)

/*
  * METHODS
 */

static u8 demod_addr;
static struct i2c_adapter *i2c_adapter = NULL;

static void set_demod_addr(struct mxl101sf_state *state)
{
	demod_addr = state->demod_address;
}

u8 get_demod_addr(void)
{
	return demod_addr;
}

void set_i2c_adapter(struct i2c_adapter *i2c)
{
	if (i2c != NULL)
		i2c_adapter = i2c;
	else
		printk(KERN_ERR "We get a Null pointer in function %s\n", __FUNCTION__);
}

struct i2c_adapter * get_i2c_adapter(void)
{
	if (i2c_adapter != NULL)
		return i2c_adapter;
	else
		printk(KERN_ERR "We get a Null pointer in function %s\n", __FUNCTION__);
	return (NULL);
}


static int mxl101sf_readreg (struct mxl101sf_state* state, u8 reg)
{
        int ret;
	u8 b0 [] = {0xFB, reg };
	u8 b1 [] = { 0 };
	struct i2c_msg msg [] = { { .addr = state->demod_address, .flags = 0, .buf = b0, .len = 2},
	                         { .addr = state->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 } };
	
	ret = i2c_transfer (state->i2c, msg, 1);
	ret |= i2c_transfer (state->i2c, msg+1, 1);
	
	if (ret != 1) {
		printk("%s: error (reg == 0x%02x,  ret == %i)\n", __FUNCTION__, reg, ret);
		return -EIO;
        }

        return b1[0];
}

static int mxl101sf_init (struct dvb_frontend* fe)
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;
	MXL_STATUS status;
	MXL_XTAL_CFG_T xtal_cfg;
	MXL_DEV_MODE_CFG_T mode_cfg;
	MXL_MPEG_CFG_T mpeg_cfg;
	MXL_TOP_MASTER_CFG_T tm_cfg;

	set_demod_addr(state);
	status = g_mxl101sf_ops.config_device(MXL_DEV_SOFT_RESET_CFG, NULL);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to reset device!\n");
		return -1;
	}

	//init tuner and demod
	status = g_mxl101sf_ops.config_device(MXL_DEV_101SF_OVERWRITE_DEFAULTS_CFG, NULL);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "Failed to init tuner and demod\n");
		return -1;
	}
	
	xtal_cfg.XtalFreq = XTAL_24MHz;

	if (state->config->type == 0)   //for master
		xtal_cfg.LoopThruEnable = MXL_DISABLE;
	else
		xtal_cfg.LoopThruEnable = MXL_ENABLE;

	xtal_cfg.XtalBiasCurrent = I787_8uA;
	xtal_cfg.XtalCap = 0x18;

	if (state->config->type == 0)	//for master
		xtal_cfg.XtalClkOutEnable = MXL_ENABLE;
	else
		xtal_cfg.XtalClkOutEnable = MXL_DISABLE;

	xtal_cfg.XtalClkOutGain = CLK_OUT_0dB;
	
	//config Xtal capacitance value;
	status = g_mxl101sf_ops.config_device(MXL_DEV_XTAL_SETTINGS_CFG, &xtal_cfg);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to config Xtal!\n");
		return -1;
	}

	//config as SOC mode to use it as a full NIM
	mode_cfg.DeviceMode = MXL_SOC_MODE;
	status = g_mxl101sf_ops.config_device(MXL_DEV_OPERATIONAL_MODE_CFG, &mode_cfg);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to config operation mode!\n");
		return -1;
	}
	
	mpeg_cfg.MpegClkFreq = MPEG_CLOCK_18_285714MHz;
	mpeg_cfg.MpegClkPhase = MPEG_CLK_INVERTED;
	mpeg_cfg.MpegSyncPol = MPEG_CLK_IN_PHASE;
	mpeg_cfg.MpegValidPol = MPEG_CLK_IN_PHASE;
	mpeg_cfg.SerialOrPar = (state->config->serial_not_parallel ? MPEG_DATA_SERIAL : MPEG_DATA_PARALLEL);

	status = g_mxl101sf_ops.config_device(MXL_DEV_MPEG_OUT_CFG, &mpeg_cfg);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to config MPEG output!\n");
		return -1;
	}

	tm_cfg.TopMasterEnable = MXL_ENABLE;
	status = g_mxl101sf_ops.config_tuner(MXL_TUNER_TOP_MASTER_CFG, &tm_cfg);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to enable top master\n");
		return -1;
	}

	return 0;
}

static int mxl101sf_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;
	MXL_STATUS stat;
	MXL_DEMOD_LOCK_STATUS_T lock_status;

	set_demod_addr((struct mxl101sf_state*) fe->demodulator_priv);

	*status |= FE_HAS_CARRIER;
	*status |= FE_HAS_VITERBI;

	stat = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_SYNC_LOCK_REQ, &lock_status);
	if (stat == MXL_FALSE) {
		printk(KERN_INFO "failed to get MXL_DEMOD_SYNC_LOCK_REQ\n");
		return -1;
	}

	if (lock_status.Status == MXL_LOCKED)
		*status |= FE_HAS_SYNC;

	stat = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_LOCK_REQ, &lock_status);
	if (stat == MXL_FALSE) {
		printk(KERN_INFO "failed to get MXL_DEMOD_TPS_LOCK_REQ\n");
		return -1;
	}

	if (lock_status.Status == MXL_LOCKED)
		*status |= FE_HAS_SIGNAL;

	stat = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_RS_LOCK_REQ, &lock_status);
	if (stat == MXL_FALSE) {
		printk(KERN_INFO "failed to get MXL_DEMOD_RS_LOCK_REQ\n");
		return -1;
	}

	if (lock_status.Status == MXL_LOCKED)
		*status |= FE_HAS_LOCK;

	return 0;
}

static int mxl101sf_read_ber(struct dvb_frontend* fe, u32* ber)
{
	MXL_DEMOD_BER_INFO_T demod_ber;
	MXL_STATUS status;

	set_demod_addr((struct mxl101sf_state*) fe->demodulator_priv);

	status = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_BER_REQ, &demod_ber);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "get demod ber failed\n");
		return -1;
	}

	*ber = (u32)demod_ber.BER;
	return 0;
}

static int mxl101sf_read_signal_strength(struct dvb_frontend* fe, u16* signal_strength)
{
	MXL_SIGNAL_STATS_T demod_signal;
	MXL_STATUS status;

	set_demod_addr((struct mxl101sf_state*) fe->demodulator_priv);

	status = g_mxl101sf_ops.get_tuner_status(MXL_TUNER_SIGNAL_STRENGTH_REQ, &demod_signal);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "get signal strength failed\n");
		return -1;
	}

	*signal_strength = (u16)demod_signal.SignalStrength;

	return 0;
}

static int mxl101sf_read_snr(struct dvb_frontend* fe, u16* snr)
{
	MXL_DEMOD_SNR_INFO_T demod_snr;
	MXL_STATUS status;

	set_demod_addr((struct mxl101sf_state*) fe->demodulator_priv);

	status = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_SNR_REQ, &demod_snr);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "get demod snr failed\n");
		return -1;
	}

	*snr = (u16)demod_snr.SNR;

	return 0;
}

static int mxl101sf_enable_high_lnb_voltage(struct dvb_frontend* fe,long arg )
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;

	if(arg == 0) {
		/* Deactivate antenna power supply */

	} else {
		/* Activate antenna power supply */

	}

	return 0;
}
static int mxl101sf_read_ucblocks(struct dvb_frontend* fe, u32* ucblocks)
{
	*ucblocks = 0;

	return 0;
}

static int mxl101sf_set_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;
	u8 bandwidth;
	MXL_RF_TUNE_CFG_T channel_cfg;
	MXL_STATUS status;

	set_demod_addr(state);

	switch (p->u.ofdm.bandwidth) {
	case BANDWIDTH_6_MHZ:
		bandwidth = 6;
		break;

	case BANDWIDTH_7_MHZ:
		bandwidth = 7;
		break;

	case BANDWIDTH_8_MHZ:
		bandwidth = 8;
		break;

	default:
		wyprintk("Frontend: Bandwidth not supported\n");
	}

	channel_cfg.Bandwidth = bandwidth;
	channel_cfg.Frequency = p->frequency;
	channel_cfg.TpsCellIdRbCtrl = MXL_ENABLE;

	status = g_mxl101sf_ops.config_tuner(MXL_TUNER_CHAN_TUNE_CFG, &channel_cfg);
	if (status == MXL_FALSE)
		printk("%s failed\n", __FUNCTION__);

	msleep(200);

	return ((status == MXL_FALSE) ? -1 : 0) ;
}

static int mxl101sf_get_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;
	struct dvb_ofdm_parameters *q = &p->u.ofdm;
	static const fe_modulation_t qam_tab[] = {QPSK, QAM_16, QAM_64};
	static const fe_code_rate_t fec_tab[] = {FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8};
	MXL_STATUS status;
	MXL_DEMOD_TPS_INFO_T tps_info;

	set_demod_addr(state);

	status = g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_CONSTELLATION_REQ, &tps_info);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "failed to get demod status MXL_DEMOD_TPS_CONSTELLATION_REQ\n");
		return -1;
	}
	
	if (tps_info.TpsInfo > 2)
	{
		printk(KERN_DEBUG "unsupport constellation detected\n");
		return -1;			
	}
	else
	{
		q->constellation = qam_tab[tps_info.TpsInfo];
	}
	
	status |= g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_CODE_RATE_REQ, &tps_info);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "failed to get demod status MXL_DEMOD_TPS_CODE_RATE_REQ\n");
		return -1;
	}	
	
	if (tps_info.TpsInfo > 4)
	{
		printk(KERN_ERR "unsupport code rate detected\n");
		return -1;
	}
	else
	{
		q->code_rate_HP = q->code_rate_LP = fec_tab[tps_info.TpsInfo]; 
	}

	status |= g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_HIERARCHY_REQ, &tps_info);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "failed to get demod status MXL_DEMOD_TPS_HIERARCHY_REQ\n");
		return -1;	
	}

	if (tps_info.TpsInfo > 3)
	{
		printk(KERN_ERR "unsupported hierarchy detected\n");
		return -1;
	}

	q->hierarchy_information = HIERARCHY_NONE + tps_info.TpsInfo;

	status |= g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_GUARD_INTERVAL_REQ, &tps_info);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "failed to get demod status MXL_DEMOD_TPS_GUARD_INTERVAL_REQ\n");
		return -1;	
	}

	if (tps_info.TpsInfo > 3)
	{
		printk(KERN_ERR "unsupported guard interval detected\n");
		return -1;
	}
	else
	{
		q->guard_interval = GUARD_INTERVAL_1_32 + tps_info.TpsInfo;
	}

	status |= g_mxl101sf_ops.get_demod_status(MXL_DEMOD_TPS_FFT_MODE_REQ, &tps_info);
	if (status == MXL_FALSE)
	{
		printk(KERN_ERR "failed to get demod status MXL_DEMOD_TPS_FFT_MODE_REQ\n");
		return -1;
	}

	if (tps_info.TpsInfo > 2)
	{
		printk(KERN_ERR "unsupported transmition mode detected\n");
		return -1;
	}
	else
	{
		q->transmission_mode = TRANSMISSION_MODE_2K + tps_info.TpsInfo;
	}

	return 0;
}

static int mxl101sf_get_tune_settings(struct dvb_frontend* fe, struct dvb_frontend_tune_settings* fesettings)
{
	fesettings->min_delay_ms = 1000;
	fesettings->step_size = 0;
	fesettings->max_drift = 166667*2;
        
	return 0;
}

static void mxl101sf_release(struct dvb_frontend* fe)
{
	struct mxl101sf_state* state = (struct mxl101sf_state*) fe->demodulator_priv;
	
	state = (struct mxl101sf_state*) fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops mxl101sf_ops;

struct dvb_frontend* mxl101sf_attach(const struct mxl101sf_config* config,
				    				   struct i2c_adapter* i2c, u8 demod_address)
{
	struct mxl101sf_state* state = NULL;
	int reg;
	MXL_DEV_INFO_T mxl_info;
	MXL_STATUS status;

	/* allocate memory for the internal state */
	state = (struct mxl101sf_state*) kmalloc(sizeof(struct mxl101sf_state), GFP_KERNEL);
	if (state == NULL) goto error;

	/* setup the state */
	state->config        = config;
	state->i2c           = i2c;
	state->demod_address = demod_address;
	memcpy(&state->ops, &mxl101sf_ops, sizeof(struct dvb_frontend_ops));


    /* set i2c address of demod for Mxl firmware API using */
	set_i2c_adapter(i2c);
	set_demod_addr(state);

	/* do soft reset before access registers of mxl101sf */
	status = g_mxl101sf_ops.config_device(MXL_DEV_SOFT_RESET_CFG, NULL);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to reset device!\n");
		goto error;
	}

	status = g_mxl101sf_ops.get_device_status(MXL_DEV_ID_VERSION_REQ, &mxl_info);
	if (status == MXL_FALSE)
	{
		printk (KERN_ERR "Failed to get device info!\n");
		goto error;
	} 

	//expect chip ID = 0x61, version = 0x06
	if (mxl_info.DevId != 0x61 || mxl_info.DevVer != 0x36)
	{
		printk(KERN_DEBUG "Unexpect device ID and version detected, ID : 0x%02x, Version : 0x%02x\n", 
				(int)mxl_info.DevId, (int)mxl_info.DevVer);
		goto error;
	}

	/* create dvb_frontend */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
	state->frontend.ops = state->ops;
	state->frontend.demodulator_priv = state;
#else
	state->frontend.ops = &state->ops;
	state->frontend.demodulator_priv = state;
#endif

	return &state->frontend;

error:
	if (state) kfree(state);
	return NULL;
}

static struct dvb_frontend_ops mxl101sf_ops = {

	.info = {
		.name 			= "LinuxDVB MXL101SF driver",
		.type 			= FE_OFDM,
		.frequency_min 		= 104000000,
		.frequency_max 		= 862000000,
        .frequency_stepsize 	= 166667,
		.caps = FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		        FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
		        FE_CAN_QPSK | FE_CAN_QAM_16 | FE_CAN_QAM_64 | FE_CAN_TRANSMISSION_MODE_AUTO |
		        FE_CAN_GUARD_INTERVAL_AUTO |
	            FE_CAN_RECOVER
	},

	.release = mxl101sf_release,
	.init = mxl101sf_init,

	.set_frontend = mxl101sf_set_frontend,
	.get_frontend = mxl101sf_get_frontend,
	.get_tune_settings = mxl101sf_get_tune_settings,

	.read_status = mxl101sf_read_status,
	.read_ber = mxl101sf_read_ber,
	.read_signal_strength = mxl101sf_read_signal_strength,
	.read_snr = mxl101sf_read_snr,
	.read_ucblocks = mxl101sf_read_ucblocks,

    .enable_high_lnb_voltage = mxl101sf_enable_high_lnb_voltage,
};

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off frontend debugging (default:off).");

module_param(trace, int, 0644);
MODULE_PARM_DESC(trace, "Turn on/off frontend traces (default:on).");

MODULE_DESCRIPTION("MXL101SF driver");
MODULE_AUTHOR("Hu Gang <hug@wyplayasia.com>");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(mxl101sf_attach);
