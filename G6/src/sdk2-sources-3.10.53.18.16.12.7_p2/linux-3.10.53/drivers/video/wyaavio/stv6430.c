/*
 * Linux Kernel device driver for STV6430
 *
 * (C) Copyright 2007-2012 WyPlay SAS.
 * Frederic Mazuel <fmazuel@wyplay.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include "stv6430.h"

/* ----------------------------------------------------------------------- */

#define I2C_BUSNO 1

#define MSGPREFIX STV6430_DRV_NAME
#define PRINTK(fmt, args...)			\
    do {					\
      if (debug >= 1)				\
      printk(KERN_DEBUG "%s %s: " fmt,		\
          MSGPREFIX, __FUNCTION__, ##args);	\
    } while (0)

static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level 0/1");

/* ----------------------------------------------------------------------- */
/* i2c registers */
#define STV6430_REG_RR0 0
#define STV6430_REG_WR0 0
#define STV6430_REG_WR1 1
#define STV6430_REG_WR2 2
#define STV6430_REG_WR3 3
#define STV6430_REG_WR4 4
#define STV6430_REG_WR5 5

#define STV6430_RR0_ZCD			(0x01)
#define STV6430_RR0_ID			(0x20)
#define STV6430_RR0_ID_MASK		(0x7B)
#define STV6430_RRO_POR			(1 << 7)
#define STV6430_RR0_POR_MASK		(1 << 7)

#define	STV6430_WR3_ZCD_ENA		0
#define	STV6430_WR3_ZCD_DIS		1
#define	STV6430_WR3_ZCD_SHIFT		7
#define	STV6430_WR3_ZCD_MASK		(1 << 7)
#define	STV6430_WR3_FB_ON		1
#define	STV6430_WR3_FB_OFF		0
#define	STV6430_WR3_FB_STANDBY_SHIFT	4
#define	STV6430_WR3_FB_STANDBY_MASK	(1 << 4)
#define	STV6430_WR3_SB_ON		1
#define	STV6430_WR3_SB_OFF		0
#define	STV6430_WR3_FB_OUTPUT_LOW	0
#define	STV6430_WR3_FB_OUTPUT_HIGH	1
#define	STV6430_WR3_FB_OUTPUT_SHIFT	2
#define	STV6430_WR3_FB_OUTPUT_MASK	(1 << 2)
#define	STV6430_WR3_SB_OUTPUT_INTERNAL	0
/* don't know where specify 1 and 2 are, but values were wrong */
#define STV6430_WR3_SB_OUTPUT_16_9		1		/* datasheet STV6430B_REV2 specify 1 */
#define STV6430_WR3_SB_OUTPUT_4_3		2		/* datasheet STV6430B_REV2 specify 2 */
#define	STV6430_WR3_SB_OUTPUT_SHIFT	0
#define	STV6430_WR3_SB_OUTPUT_MASK	(3 << 0)
#define	STV6430_WR4_RGB_ON			0
#define	STV6430_WR4_RGB_OFF			7
#define	STV6430_WR4_RGB_SHIFT		3
#define	STV6430_WR4_RGB_MASK		(7 << 3)
#define	STV6430_WR4_CVBS_ON			0
#define	STV6430_WR4_CVBS_OFF		1
#define	STV6430_WR4_CVBS_SHIFT		6
#define	STV6430_WR4_CVBS_MASK		(1 << 6)
#define	STV6430_WR4_AUDIO_ON		0
#define	STV6430_WR4_AUDIO_OFF		1
#define	STV6430_WR4_AUDIO_SHIFT		7
#define	STV6430_WR4_AUDIO_MASK		(1 << 7)

/* Local registers copies */
static u8 stv6430_wr0_reg = 0;
static u8 stv6430_wr1_reg = 0;
static u8 stv6430_wr2_reg = 0;
static u8 stv6430_wr3_reg = 0;
static u8 stv6430_wr4_reg = 0;

//struct platform_driver stv6430_pm_driver; 
struct i2c_client *g_i2c_client;

static const struct stv6430_port_info stv6430_ports[STV6430_PORT_COUNT] =
{
	{"TV", STV6430_PORT_TV, STV6430_MEDIA_ANY, STV6430_MEDIA_NONE},
};

/**
 * int connect : 1 for connect, 0 for disconnect
 */
void stv6430_blanking_high_impedance(int connect, char *data)
{
	if (connect) {
		/*SLB and FB out of standby*/
		stv6430_wr3_reg = stv6430_wr3_reg & (~0x18) ;
		data[0] = STV6430_REG_WR3;
		data[1] = stv6430_wr3_reg;
	}
	else {
		/*SLB and FB in standby*/
		stv6430_wr3_reg = stv6430_wr3_reg | 0x18 ;
		data[0] = STV6430_REG_WR3;
		data[1] = stv6430_wr3_reg;
	}
}

int stv6430_command(struct stv6430_handle *handle, unsigned int cmd, void *arg)
{
	int ret = 0;
	char data[2] = { 0 };
	int value = 0;
	struct i2c_client *client = g_i2c_client;

	if (arg == NULL) return (-EINVAL);

	switch (cmd)
	{
	case STV6430_CGET_PORT_INFO: {
		struct stv6430_port_info *param = (struct stv6430_port_info*) arg;
		const uint8_t index = param->index;
		PRINTK("STV6430_CGET_PORT_INFO: %d\n", index);
		if (index < 1 || index > STV6430_PORT_COUNT)
			return -EINVAL;
		*param = stv6430_ports[index - 1];
	}
	break;

	case STV6430_CSET_CONNECT_PORTS:{
		
		/*Unmute all*/
		stv6430_wr2_reg = stv6430_wr2_reg & 0x00 ; 
		data[0] = STV6430_REG_WR2;
		data[1] = stv6430_wr2_reg;
		ret = i2c_master_send(client, data, sizeof(data));
		stv6430_blanking_high_impedance(1, data);
		ret = i2c_master_send(client, data, sizeof(data));

	}	
	break;

	case STV6430_CSET_DISCONNECT_PORTS:{

		/*Mute all*/
		stv6430_wr2_reg = stv6430_wr2_reg | 0xF8 ; 
		data[0] = STV6430_REG_WR2;
		data[1] = stv6430_wr2_reg;
		ret = i2c_master_send(client, data, sizeof(data));
		stv6430_blanking_high_impedance(0, data);
		ret = i2c_master_send(client, data, sizeof(data));
	}
	break;

	case STV6430_CSET_DISABLE_PORT:{
		int* media = (int *) arg;
		printk("STV6430_CSET_DISABLE_PORT: %x\n",*media);
		/*standby media*/
		if ( *media == STV6430_MEDIA_NONE) return 0;
		if ( (*media == STV6430_MEDIA_AUDIO) || (*media == STV6430_MEDIA_ANY) ){
			stv6430_wr4_reg &= ~STV6430_WR4_AUDIO_MASK;
			stv6430_wr4_reg |= (STV6430_WR4_AUDIO_OFF << STV6430_WR4_AUDIO_SHIFT);
			data[0] = STV6430_REG_WR4;
			data[1] = stv6430_wr4_reg;
			ret = i2c_master_send(client, data, sizeof(data));
		}
		if ( *media == STV6430_MEDIA_VIDEO   || (*media == STV6430_MEDIA_ANY)){
			stv6430_wr4_reg &= ~STV6430_WR4_RGB_MASK;
			stv6430_wr4_reg |= ( STV6430_WR4_RGB_OFF << STV6430_WR4_RGB_SHIFT);
			stv6430_wr4_reg &= ~STV6430_WR4_CVBS_MASK;
			stv6430_wr4_reg |= ( STV6430_WR4_CVBS_OFF << STV6430_WR4_CVBS_SHIFT);
			data[0] = STV6430_REG_WR4;
			data[1] = stv6430_wr4_reg;
			ret = i2c_master_send(client, data, sizeof(data));
			stv6430_blanking_high_impedance(0, data);
			ret = i2c_master_send(client, data, sizeof(data));
		}
	}
	break;

	case STV6430_CSET_ENABLE_PORT:{
		int* media = (int *) arg;
		printk("STV6430_CSET_ENABLE_PORT: %x\n",*media);
		/*awake media*/
		if ( *media == STV6430_MEDIA_NONE) return 0;
		if ( (*media == STV6430_MEDIA_AUDIO) || (*media == STV6430_MEDIA_ANY) ){
			stv6430_wr4_reg &= ~STV6430_WR4_AUDIO_MASK;
			stv6430_wr4_reg |= (STV6430_WR4_AUDIO_ON << STV6430_WR4_AUDIO_SHIFT);
			data[0] = STV6430_REG_WR4;
			data[1] = stv6430_wr4_reg;
			ret = i2c_master_send(client, data, sizeof(data));
		}
		if ( (*media == STV6430_MEDIA_VIDEO)  || (*media == STV6430_MEDIA_ANY)){
			stv6430_wr4_reg &= ~STV6430_WR4_RGB_MASK;
			stv6430_wr4_reg |= ( STV6430_WR4_RGB_ON << STV6430_WR4_RGB_SHIFT);
			stv6430_wr4_reg &= ~STV6430_WR4_CVBS_MASK;
			stv6430_wr4_reg |= ( STV6430_WR4_CVBS_ON << STV6430_WR4_CVBS_SHIFT);
			data[0] = STV6430_REG_WR4;
			data[1] = stv6430_wr4_reg;
			ret = i2c_master_send(client, data, sizeof(data));
			stv6430_blanking_high_impedance(1, data);
			ret = i2c_master_send(client, data, sizeof(data));
		}
	}	
	break;

	case STV6430_CSET_VIDEO_MODE: {
		struct stv6430_medium_mode *param = (struct stv6430_medium_mode*) arg;
		printk("STV6430_CSET_VIDEO_MODE: %x\n",param->mode);

		value = (param->mode & STV6430_VIDEO_RGB) ?
				STV6430_WR4_RGB_ON : STV6430_WR4_RGB_OFF;
		stv6430_wr4_reg &= ~STV6430_WR4_RGB_MASK;
		stv6430_wr4_reg |= (value << STV6430_WR4_RGB_SHIFT);

		value = (param->mode & STV6430_VIDEO_CVBS) ?
				STV6430_WR4_CVBS_ON : STV6430_WR4_CVBS_OFF;
		stv6430_wr4_reg &= ~STV6430_WR4_CVBS_MASK;
		stv6430_wr4_reg |= (value << STV6430_WR4_CVBS_SHIFT);

		data[0] = STV6430_REG_WR4;
		data[1] = stv6430_wr4_reg;
		ret = i2c_master_send(client, data, sizeof(data));
	}
	break;

	case STV6430_CGET_VIDEO_MODE: {
		struct stv6430_medium_mode *param = (struct stv6430_medium_mode*) arg;
		printk("STV6430_CGET_VIDEO_MODE\n");
		param->mode = 0;

		if ((stv6430_wr4_reg & STV6430_WR4_RGB_MASK) == STV6430_WR4_RGB_ON)
			param->mode |= STV6430_VIDEO_RGB;
		if ((stv6430_wr4_reg & STV6430_WR4_CVBS_MASK) == STV6430_WR4_CVBS_ON)
			param->mode |= STV6430_VIDEO_CVBS;
		printk(KERN_WARNING MSGPREFIX " VIDEO MODE = %x\n",param->mode);
	}
	break;

	case STV6430_CSET_SLOW_BLANKING: {
		struct stv6430_set_slowbg *param = (struct stv6430_set_slowbg*) arg;
		printk("STV6430_CSET_SLOW_BLANKING: %d\n",param->status);
		if (param->status == STV6430_SLOWBG_EXTERNAL_4_3)
			value = STV6430_WR3_SB_OUTPUT_4_3;
		else if (param->status == STV6430_SLOWBG_EXTERNAL_16_9)
			value = STV6430_WR3_SB_OUTPUT_16_9;
		else if (param->status == STV6430_SLOWBG_INTERNAL)
			value = STV6430_WR3_SB_OUTPUT_INTERNAL;
		else {
			ret = -EINVAL;
			break;
		}

		stv6430_wr3_reg &= ~STV6430_WR3_SB_OUTPUT_MASK;
		stv6430_wr3_reg |= (value << STV6430_WR3_SB_OUTPUT_SHIFT);

		data[0] = STV6430_REG_WR3;
		data[1] = stv6430_wr3_reg;
		ret = i2c_master_send(client, data, sizeof(data));
	}
	break;

	case STV6430_CSET_FAST_BLANKING: {
		struct stv6430_set_fastbg *param = (struct stv6430_set_fastbg*) arg;
		printk("STV6430_CSET_FAST_BLANKING: %d\n",param->status);
		if (param->status == STV6430_FASTBG_RGB)
			value = STV6430_WR3_FB_OUTPUT_HIGH;
		else if (param->status == STV6430_FASTBG_CVBS)
			value = STV6430_WR3_FB_OUTPUT_LOW;
		else {
			ret = -EINVAL;
			break;
		}

		stv6430_wr3_reg &= ~STV6430_WR3_FB_OUTPUT_MASK;
		stv6430_wr3_reg |= (value << STV6430_WR3_FB_OUTPUT_SHIFT);

		data[0] = STV6430_REG_WR3;
		data[1] = stv6430_wr3_reg;
		ret = i2c_master_send(client, data, sizeof(data));
	}
	break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}/*switch*/

	return ret;
}
EXPORT_SYMBOL(stv6430_command);

/************************************PM********************************/

#ifdef CONFIG_PM
static int stv6430_suspend(struct device *dev)
{
	/*nothing to do*/
	printk( "STV6430_PM_SUSPEND\n" );
	return (0);
}
static int stv6430_resume(struct device *dev)
{
	struct i2c_client *client = g_i2c_client;
	char data[6] = { 0 };
	int ret = 0;

	printk( "STV6430_PM_RESUME\n" );
	
	data[0] = STV6430_REG_WR0;
	data[1] = stv6430_wr0_reg;
	data[2] = stv6430_wr1_reg;
	data[3] = stv6430_wr2_reg;
	data[4] = stv6430_wr3_reg;
	data[5] = stv6430_wr4_reg;

	ret = i2c_master_send(client, data, sizeof(data));
	if (ret != sizeof(data)) {
		printk("resume has failed err = %d\n",ret);
		ret = -ENODEV;
	} else {
		ret = 0;
	}
	return ret;
}

static SIMPLE_DEV_PM_OPS(stv6430_pm,stv6430_suspend,stv6430_resume);
#define STV6430_PM 	(&stv6430_pm)
#else
#define STV6430_PM	NULL
#endif

static struct platform_driver stv6430_pm_driver = {
	.driver ={
		.name = "stv6430_pm",
		.owner= THIS_MODULE,
		.pm   = STV6430_PM,
	},
};
/**********************************************************************/

static int stv6430_setup(struct i2c_client *client)
{

	char data[6] = { 0 };
	int ret = 0;

	printk("stv6430 setup\n");

	data[0] = STV6430_REG_RR0;
	ret = i2c_master_recv(client, data, 1);
	if (ret != 1)
		printk(KERN_ERR MSGPREFIX "failed to read data, error %d\n", ret);
	else {
		uint8_t reg = (uint8_t)(data[0] & STV6430_RR0_ID_MASK);
		printk("checked identification code: %02x, %s\n",
				reg, reg == STV6430_RR0_ID ? "PASS" : "FAIL");
		reg = (uint8_t)(data[0] & STV6430_RR0_POR_MASK);
		printk("checked initial condition status: %s\n",
				reg == STV6430_RRO_POR ? "PASS" : "FAIL");
	}

	g_i2c_client = client;

	/*
	 * The driver set this default configuration
	 * HD LPF control : 7MHz LPF enabled
	 * gain: 0db for normal audio inputs
	 * SD & HD video gain control: 6dB
	 * CVBS input to CVBS output
	 * R/C input to R/C output
	 * No mute on Pb, Y, Pr, B, G, R/C, CVBS and audio
	 * Slow Blanking: internal and ON
	 * Fast Blanking: Low level and ON
	 * Zero Cross Detection active
	 * Pb video standby ON (OFF after a reset)
	 * Y video standby ON (OFF after a reset)
	 * Pr video standby ON (OFF after a reset)
	 * B video standby ON
	 * G video standby ON
	 * R/C video standby ON
	 * CVBS video standby ON
	 * Audio video standby ON
	 * B video standby ON
	 * Full stop mode: 0xFF : Only I2C bus working (?)
	 * HD (YPbPr) outputs buffer bandwidth: Large BW1
	 */
	stv6430_wr0_reg = 0x60; /* +16db audio gain */
//	stv6430_wr0_reg = 0; /* 0db audio gain */
	stv6430_wr1_reg = 0;
	stv6430_wr2_reg = 0;
	stv6430_wr3_reg = 0;
	stv6430_wr4_reg = (
			(STV6430_WR4_RGB_ON << STV6430_WR4_RGB_SHIFT) |
			(STV6430_WR4_CVBS_ON << STV6430_WR4_CVBS_SHIFT) |
			(STV6430_WR4_AUDIO_ON << STV6430_WR4_AUDIO_SHIFT)
	);


	data[0] = STV6430_REG_WR0;
	data[1] = stv6430_wr0_reg;
	data[2] = stv6430_wr1_reg;
	data[3] = stv6430_wr2_reg;
	data[4] = stv6430_wr3_reg;
	data[5] = stv6430_wr4_reg;

	ret = i2c_master_send(client, data, sizeof(data));
	if (ret != sizeof(data)) {
		printk("init has failed err = %d\n",ret);
		ret = -ENODEV;
	} else {
		ret = 0;
	}
	return ret;
}

/* ----------------------------------------------------------------------- */
static int stv6430_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("probing client %s driver %s\n",client->name, client->name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	if (stv6430_setup(client))
		return -ENODEV;

	return 0;
}

static int stv6430_remove(struct i2c_client *client)
{
	PRINTK("removing\n");
	return 0;
}


static struct i2c_device_id stv6430_ids[] = {
	{ "stv6430", 0 },
	{ }
};

static struct i2c_driver stv6430_driver =
{
		.driver = {
				.name = "stv6430"
		},
		.id_table = stv6430_ids,
		.probe = stv6430_probe,
		.remove = stv6430_remove,
};

static struct i2c_board_info i2c_info =
{
		I2C_BOARD_INFO("stv6430", 0x4A)

};

static struct platform_device *stv6430_pm_device;

/* ----------------------------------------------------------------------- */
static int __init stv6430_init(void)
{
	int ret = 0;
	struct i2c_adapter *i2c_adap;
	struct i2c_client *i2c_client;

	printk("modules is loading\n");

	i2c_adap = i2c_get_adapter(I2C_BUSNO);

	if ((ret = i2c_add_driver(&stv6430_driver)) < 0 ) {
		printk(KERN_ERR MSGPREFIX "Failed to add i2c driver\n");
		goto exit;
	}

	if ((i2c_client = i2c_new_device(i2c_adap, &i2c_info)) == NULL) {
		printk(KERN_ERR MSGPREFIX "Failed to add i2c device\n");
		ret = -ENODEV;
		goto exit;
	}
/*power management*/
	ret = platform_driver_register(&stv6430_pm_driver);
	if (ret)
		goto exit;
	stv6430_pm_device = platform_device_register_simple("stv6430_pm", -1,
							NULL, 0);

exit:
	return ret ;
}

static void __exit stv6430_exit(void)
{
	PRINTK("modules is unloading\n");
	i2c_del_driver(&stv6430_driver);
/*power management*/
	platform_device_unregister(stv6430_pm_device);
	platform_driver_unregister(&stv6430_pm_driver);

}

MODULE_DESCRIPTION("STV6430");
MODULE_AUTHOR("Frederic Mazuel <fmazuel@wyplay.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(stv6430_init);
module_exit(stv6430_exit);
