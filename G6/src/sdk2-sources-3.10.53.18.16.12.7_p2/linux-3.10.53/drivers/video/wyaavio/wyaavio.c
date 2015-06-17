/*
 * Linux Kernel device driver
 * for Wyplay Analog Audio/Video Input/Output driver.
 *
 * For analog A/V outputs, analog A/V inputs, analog switches,
 * and additional video circuits.
 *
 * (C) Copyright 2007-2014 WyPlay SAS.
 * (C) Copyright 2014 Technicolor  .
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>			/* for character device */
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <asm/uaccess.h>		/* for copy_from_user, copy_to_user */
#include <asm/uaccess.h>		/* for copy_from_user, copy_to_user */
#include <linux/wyaavio.h>

#include "stv6430.h"
#define MSGPREFIX		"wyaavio"
#define PRINTK(fmt, args...)						\
	do {								\
		if (debug >= 1)						\
			printk(KERN_DEBUG "%s %s: " fmt,		\
			       MSGPREFIX, __FUNCTION__, ##args);	\
	} while (0)

#define IO_PARAM_SIZE_MAX	128	
#define DEV_VERSION		"version 0.2"

enum e_amio_discnx{
AMIO_FULL_DISCNX=0,
AMIO_INPUTS_DISCNX,
AMIO_OUTPUTS_DISCNX,
AMIO_SIMPLE_DISCNX
};
enum e_amio_fastbg{
AMIO_FASTBG_CVBS,
AMIO_FASTBG_RGB
};

struct amio_set_fastbg
{
__u8 index;
__u8 output;
__u8 status;
};

static int version = 2;

/* -----status------------------------------------------------------------ */

/* the cells in these tables match enum e_amio_port */
static struct amio_port_info ports[] = {
					{"STB",1,AMIO_MEDIA_ANY,AMIO_MEDIA_NONE},
					{"TV",2,AMIO_MEDIA_NONE,AMIO_MEDIA_ANY}
				       };

/* Connections initialisation [source ][destination]*/
static int connected[AMIO_PORT_COUNT][AMIO_PORT_COUNT] = { 
								{0,1},
								{1,0}
							 };

/* ----------------------------------------------------------------------- */

static int debug = 0;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "Debug level 0/1");

/* ----------------------------------------------------------------------- */
static long wyaavio_ioctl(struct file *file,
                         unsigned int cmd, unsigned long userIoPtr)
{
	unsigned char arg[IO_PARAM_SIZE_MAX];

	int resultCode = 0;
	int dummy;
	void *avswitch = NULL;

	PRINTK("Entering %s\n", __FUNCTION__);

	if (_IOC_TYPE(cmd) != AMIO_IOC_MAGIC)
		return -ENOIOCTLCMD;
	if (_IOC_SIZE(cmd) > IO_PARAM_SIZE_MAX)
		return -ENOIOCTLCMD;

	/* copy the ioctl parameter, if any, from user to kernel memory */
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (copy_from_user(arg, (void __user *)userIoPtr,
		                   _IOC_SIZE(cmd)) != 0)
			return -EFAULT;

	switch (cmd)
	{
	case AMIO_CGET_API_VERSION: {
		memcpy(arg, &version, sizeof(version));
	}	break;

	case AMIO_CGET_PORT_INFO: {
		struct amio_port_info *param = (struct amio_port_info*)arg;


		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_CGET_PORT_INFO\n");
		strcpy(param->name ,ports[param->index -1].name);
		param->input = ports[param->index -1].input;
		param->output = ports[param->index -1].output;
	}	break;
	
	case AMIO_CSET_CONNECT_PORTS:{
		struct amio_connect_ports *param = (struct amio_connect_ports*)arg;

		if ((param->input == AMIO_PORT_STB) && (param->output == AMIO_PORT_TV)){
		PRINTK(" AMIO_CSET_CONNECT_PORTS\n");
		connected[param->input -1][param->output -1] = 1 ;
		dummy = 0; /*to avoid warning*/
		resultCode = stv6430_command(avswitch, STV6430_CSET_CONNECT_PORTS, (void*)&dummy);
		}else{
			return -EINVAL;/*Not supported*/
		}
	}	break;

	case AMIO_CSET_DISCONNECT_PORTS: {
		struct amio_connect_ports *param = (struct amio_connect_ports*)arg;

		if ((param->input == AMIO_PORT_STB) && (param->output == AMIO_PORT_TV)){
		dummy = AMIO_SIMPLE_DISCNX ;
		}else if((param->input == 0) && (param->output == AMIO_PORT_TV)){
		dummy = AMIO_INPUTS_DISCNX;
		}else if((param->input == AMIO_PORT_STB) && (param->output == 0)){
		dummy = AMIO_OUTPUTS_DISCNX;
		}else if((param->input == 0) && (param->output == 0)){
		dummy = AMIO_FULL_DISCNX;
		}else{
			return -EINVAL;/*Not supported*/		
		}
		PRINTK(" AMIO_CSET_DISCONNECT_PORTS\n");
		connected[ AMIO_PORT_STB -1][AMIO_PORT_TV -1] = 0 ;
		resultCode = stv6430_command(avswitch, STV6430_CSET_DISCONNECT_PORTS, (void*)&dummy);
		

	}	break;

	case AMIO_CGET_CONNECT_STATUS: {
		struct amio_connect_status *param = (struct amio_connect_status*)arg;
 
		if ((param->input == AMIO_PORT_STB) && (param->output == AMIO_PORT_TV)){
			param->connected = connected[param->input -1][param->output -1];
		}else if((param->input == 0) && (param->output == AMIO_PORT_STB)){
			param->connected = connected[ AMIO_PORT_STB-1][param->output -1];
			param->connected |= connected[ AMIO_PORT_TV-1][param->output -1];
		}else if((param->input == 0) && (param->output == AMIO_PORT_TV)){
			param->connected = connected[ AMIO_PORT_STB-1][param->output -1];
			param->connected |= connected[ AMIO_PORT_TV-1][param->output -1];
		}else if((param->input == AMIO_PORT_STB) && (param->output == 0)){
			param->connected = connected[param->input -1][ AMIO_PORT_TV -1];
			param->connected |= connected[param->input -1][ AMIO_PORT_STB -1];
		}else if((param->input == 0) && (param->output == 0)){
			param->connected = connected[AMIO_PORT_STB -1][AMIO_PORT_TV -1];
			/*add here all the other possible connection...*/
		}else{
			return -EINVAL;/*Not supported*/		
		}
		PRINTK(" AMIO_CGET_CONNECT_STATUS\n");
	}	break;

	case AMIO_CSET_ENABLE_PORT:{
		int media = 0;
		struct amio_port_section *param = (struct amio_port_section*)arg;
		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;	
		PRINTK(" AMIO_CSET_ENABLE_PORT\n");	
			ports[param->index -1].input |= param->input;
			ports[param->index -1].output |= param->output;	
			/*Awake*/
		if (param->index == AMIO_PORT_STB) media = param->input ;
		if (param->index == AMIO_PORT_TV) media = param->output ;
		resultCode = stv6430_command(avswitch, STV6430_CSET_ENABLE_PORT, (void*)&media);			
	}	break;

	case AMIO_CSET_DISABLE_PORT: {
		int media = 0;
		struct amio_port_section *param = (struct amio_port_section*)arg;
		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_CSET_DISABLE_PORT\n");
			ports[param->index -1].input &= (~param->input);
			ports[param->index -1].output &= (~param->output);	
			/*Standby*/		
		if (param->index == AMIO_PORT_STB) media = param->input;
		if (param->index == AMIO_PORT_TV) media = param->output ;
		resultCode = stv6430_command(avswitch, STV6430_CSET_DISABLE_PORT, (void*)&media);
	}	break;

	case AMIO_CSET_UNMUTE_PORT: 
	case AMIO_CSET_MUTE_PORT: {
			return -ENOIOCTLCMD;
	}	break;

	case AMIO_CGET_PORT_STATUS: {
		struct amio_port_status *param = (struct amio_port_status*)arg;

		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_CGET_PORT_STATUS\n");
		if (param->index == AMIO_PORT_STB){

			if (param->output == AMIO_MEDIA_NONE){
			param->disabled = ((ports[0].input & 0x2)>>1) ? FALSE : TRUE;
			param->muted = (ports[0].input & 0x1) ? FALSE : TRUE;
			}else{
			return -EINVAL;
			}
		}
		if (param->index == AMIO_PORT_TV){

			if (param->input == AMIO_MEDIA_NONE){
			param->disabled = ((ports[0].output & 0x2)>>1) ? FALSE : TRUE;
			param->muted = (ports[0].output & 0x1) ? FALSE : TRUE;
			}else{
			return -EINVAL;
			}
		}		

	}	break;

	case AMIO_CSET_GAIN_MODE: 
	case AMIO_CSET_GAIN: 
	case AMIO_CGET_GAIN: {
			return -ENOIOCTLCMD;
	}	break;

	case AMIO_CSET_VIDEO_MODE: {
		struct amio_medium_mode *param = (struct amio_medium_mode*)arg;
		struct amio_medium_mode akparam = { 0 };
		struct amio_set_fastbg  fbparam = { 0 };

		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_SET VIDEO MODE\n");
		akparam.index = STV6430_PORT_TV;

		if (param->mode & AMIO_VIDEO_RGB) {
			fbparam.status = STV6430_FASTBG_RGB;
			akparam.mode = STV6430_VIDEO_RGB | STV6430_VIDEO_CVBS;
		} else if (param->mode & AMIO_VIDEO_CVBS) {
			fbparam.status = STV6430_FASTBG_CVBS;
			akparam.mode = STV6430_VIDEO_CVBS;
		} else
			return -EINVAL;

		resultCode = stv6430_command(avswitch, STV6430_CSET_VIDEO_MODE, &akparam);
		if (resultCode < 0)
			break;

		resultCode = stv6430_command(avswitch, STV6430_CSET_FAST_BLANKING, &fbparam);
		if (resultCode < 0)
			break;

	}	break;

	case AMIO_CGET_VIDEO_MODE: {
		struct amio_medium_mode *param = (struct amio_medium_mode*)arg;
		struct amio_medium_mode akparam = { 0 };
		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_GET VIDEO MODE\n");
		akparam.index = STV6430_PORT_TV;
		akparam.output = param->output;
		resultCode = stv6430_command(avswitch, STV6430_CGET_VIDEO_MODE, &akparam);
		param->mode = akparam.mode;
	}	break;

	case AMIO_CSET_SLOW_BLANKING: {
		struct amio_set_slowbg *param = (struct amio_set_slowbg*)arg;
		struct amio_set_slowbg akparam = { 0 };
		if (param->index < 1 || param->index > AMIO_PORT_COUNT)
			return -EINVAL;
		PRINTK(" AMIO_SET SLOW BLANKING\n");
		akparam.index = STV6430_PORT_TV;
		akparam.output = param->output;
		akparam.status = param->status;
		resultCode = stv6430_command(avswitch, STV6430_CSET_SLOW_BLANKING, &akparam);
	}	break;
	
	case AMIO_CGET_SLOW_BLANKING: {
			return -ENOIOCTLCMD;
	}	break;

	default:
		resultCode = -ENOIOCTLCMD;
		break;
	}/*switch*/

	/* copy the ioctl result, if any, from kernel to user memory */
	if ( resultCode >= 0 && _IOC_DIR(cmd) & _IOC_READ )
		if (copy_to_user((void __user *)userIoPtr, arg,
		                 _IOC_SIZE(cmd)) != 0)
			return -EFAULT;

	PRINTK("Leaving %s\n", __FUNCTION__);

	return resultCode;
}

/* ----------------------------------------------------------------------- */
static int __init wyaavio_init_avswitch(void)
{
	int resultCode = 0;
	
	PRINTK(KERN_INFO MSGPREFIX " wyaavio initialized");

	return resultCode;
}

/* ----------------------------------------------------------------------- */
static const struct file_operations wyaavio_fops = {
	.owner = THIS_MODULE,	
	.unlocked_ioctl=wyaavio_ioctl
};


static struct miscdevice wyaavio_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wyaavio",
	.fops = &wyaavio_fops
};


static int __init wyaavio_init_module(void)
{
	int resultCode;

	resultCode = misc_register(&wyaavio_device);
	if (resultCode != 0)
		goto Exit;

	resultCode = wyaavio_init_avswitch();

Exit:
	if (resultCode == 0)
		PRINTK(KERN_INFO MSGPREFIX " char device created"
		       " with number %d:%d\n", MISC_MAJOR, wyaavio_device.minor);
	else
		PRINTK(KERN_ERR MSGPREFIX " driver failed to initialize"
		       ", with error %d.\n", resultCode);
	return resultCode != 0 ? -ENODEV : 0;
}

static void __exit wyaavio_release_module(void)
{
	misc_deregister(&wyaavio_device);
}

module_init(wyaavio_init_module);
module_exit(wyaavio_release_module);

MODULE_DESCRIPTION("Analog Audio/Video I/O driver");
MODULE_AUTHOR("legarrec <thomas.legarrec@technicolor.com>");
MODULE_LICENSE("TECHNICOLOR");
MODULE_VERSION(DEV_VERSION);
