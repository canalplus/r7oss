#ifndef __RLK_INIC_H__
#define __RLK_INIC_H__

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/module.h>
//#include <linux/config.h>
#include <linux/autoconf.h>//peter : for FC6
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/byteorder/generic.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/if_ether.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/wait.h>
//#include <linux/pci.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/wireless.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <net/iw_handler.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
#include <linux/cred.h>
#endif
#include <asm/types.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "rlk_inic_def.h"

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_PCI)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
#include <linux/irqreturn.h>
#endif
#include <linux/pci.h>
#include "rlk_inic_reg.h"
#include "rlk_inic_pci.h"

#ifdef IKANOS_VX_1X0
#include "vr_ikans.h"
#endif

#endif

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
#include <linux/ethtool.h>
#include <linux/usb.h>
#include "../usb/rt_usb_dev.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/workqueue.h>
#endif
#endif

#include "raconfig.h"



#define xdef_to_str(s)   def_to_str(s) 
#define def_to_str(s)    #s

#define def_num_to_hexstr(s) "0x" xdef_to_str(s)

#define DRV_NAME		"RT" xdef_to_str(CONFIG_CHIP_NAME) "iNIC"
#define CHIP_NAME 		"RT" xdef_to_str(CONFIG_CHIP_NAME)
#define DEVICE_NAME		xdef_to_str(CONFIG_CHIP_NAME)
#define DEVICE_ID		simple_strtol(def_num_to_hexstr(CONFIG_CHIP_NAME), 0, 16)
#define DRV_VERSION		"2.4.0.6"
#define DRV_RELDATE		"Feb. 09, 2011"



#if (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
#define INTERFACE 		"USB"
#else
#define INTERFACE 		""
#endif

#define DRIVER_VERSION   DRV_VERSION

/*
 *	Display a 6 byte device address (MAC) in a readable format.
 */
#define MAC_BUF_SIZE	18

#define PFX			DRV_NAME ": "

#define RLK_INIC_DEF_MSG_ENABLE	(NETIF_MSG_DRV		| \
				 NETIF_MSG_PROBE 	| \
				 NETIF_MSG_LINK)


#endif /* __RLK_INIC_H__ */
