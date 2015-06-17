/*
 * ST DVB On-Chip DiseqC driver
 *
 * Copyright (c) STMicroelectronics 2009
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
#include <linux/stm/pio.h>
#else
#include <linux/stm/pad.h>
#endif

#include <linux/device.h>
#include <linux/io.h>

#include <linux/version.h>
#include <linux/dvb/frontend.h>
#include <linux/i2c.h>
#include <linux/stm/stm-frontend.h>

#include "dvb_frontend.h"
#include "st-diseqc.h"

static int debug = 0;
#define dprintk(x...) do { if (debug) printk(KERN_WARNING x); } while (0)


static unsigned long diseqc_registers;
static volatile int diseqc_int_flag;

#define DSQ_TX_EN             ( diseqc_registers + 0x00 )
#define DSQ_TX_MSG_CFG        ( diseqc_registers + 0x04 )/* 00 [2:8] */
#define DSQ_TX_PRESCALER      ( diseqc_registers + 0x08 )/* 2 */
#define DSQ_TX_SUBCARR_DIV    ( diseqc_registers + 0x0C )/* 1511 */
#define DSQ_TX_SILENCE_PER    ( diseqc_registers + 0x10 )/* ?? 1000 */
#define DSQ_TX_DATA_BUF       ( diseqc_registers + 0x14 )
#define DSQ_TX_SYMBOL_PER     ( diseqc_registers + 0x18 )/* 33 */
#define DSQ_TX_SYMBOL0_ONTIME ( diseqc_registers + 0x1c )/* 11 */
#define DSQ_TX_SYMBOL1_ONTIME ( diseqc_registers + 0x20 )/* 11 */
#define DSQ_TX_SW_RST         ( diseqc_registers + 0x24 )
#define DSQ_TX_START          ( diseqc_registers + 0x28 )

#define DSQ_TX_INT_EN         ( diseqc_registers + 0x2c )
#define DSQ_TX_INT_STA        ( diseqc_registers + 0x30 )
#define DSQ_TX_STA            ( diseqc_registers + 0x34 )
#define DSQ_TX_CLR_INT_STA    ( diseqc_registers + 0x38 )
#define DSQ_TX_AGC            ( diseqc_registers + 0x40 )

static int stm_wait_diseqc_idle ( int timeout )
{
	unsigned long start = jiffies;
	int status;

	dprintk ("%s\n", __FUNCTION__);

	while ((status = readl(DSQ_TX_STA)) & 0x8 ) {
		if (jiffies - start > timeout) {
			dprintk ("%s: timeout!!\n", __FUNCTION__);
			return -ETIMEDOUT;
		}
		msleep(10);
	};

	return 0;
}


/*
  SEC_MINI_A = 0x00 = Unmodulated
  SEC_MINI_B = 0xff = Modulated
*/

static int stm_send_diseqc_burst (struct dvb_frontend* fe, fe_sec_mini_cmd_t burst)
{
  dprintk ("%s\n", __FUNCTION__);
  
  //fe->ops->set_voltage(fe, SEC_VOLTAGE_OFF);
  
  if (stm_wait_diseqc_idle ( 100) < 0)
    return -ETIMEDOUT;
  
  
  diseqc_int_flag = 0;

  switch(burst)
    {
    case SEC_MINI_A:
      writel ( 0x01, DSQ_TX_MSG_CFG );
      writel ( 0x01 | (0x09 << 9), DSQ_TX_MSG_CFG );
      break;
    case SEC_MINI_B:
	  writel ( 0x01, DSQ_TX_MSG_CFG );
	  writel ( 0x01 | (0x09 << 9), DSQ_TX_MSG_CFG );
	  break;
    }
  
  writel( 1   , DSQ_TX_EN );
  writel( 0x9 , DSQ_TX_CLR_INT_STA);
  writel( 1   , DSQ_TX_START);
  writel( 0x9 , DSQ_TX_INT_EN);
  
  while ( !diseqc_int_flag ) { dprintk("%s: waiting for interrupt\n",__FUNCTION__); msleep(10); }
  
  writel( 0   , DSQ_TX_EN );
  
  return 0;
}

static int stm_diseqc_send_msg ( struct dvb_frontend* fe,
				 struct dvb_diseqc_master_cmd *m)
{
  int n,i,j;
  int buffer;

  dprintk("%s: entered %x\n",__FUNCTION__,m->msg_len);

  writel(   22, DSQ_TX_SILENCE_PER );

  writel( 0                , DSQ_TX_MSG_CFG);
  writel( (m->msg_len) << 2, DSQ_TX_MSG_CFG);

  diseqc_int_flag = 0;

  for (n=0;n<m->msg_len;n+=4)
  {
    buffer = 0;

    for (j=0;j<4;j++)
    {
      unsigned char c = m->msg[j+n];
      unsigned char d = 0;
      for (i=0;i<8;i++)
      {
	if (i>4)
	  d |= ( c & ( 1 << i)) >> (2*i - 7);
	else
	  d |= ( c & ( 1 << i)) << (7 - 2*i);

      }

      buffer |= d << (j*8);
    }
    
    writel( buffer,  DSQ_TX_DATA_BUF );
  } 
  
  writel( 1   , DSQ_TX_EN );
  writel( 0x9 , DSQ_TX_CLR_INT_STA);
  writel( 1   , DSQ_TX_START);
  writel( 0x9 , DSQ_TX_INT_EN);

  while ( !diseqc_int_flag ) { dprintk("%s: waiting for interrupt\n",__FUNCTION__); msleep(10); }

  writel( 0   , DSQ_TX_EN );

  return 0;
}

static irqreturn_t stm_diseqc_irq(int irq, void *dev_id)
{
  unsigned int status;

  status = readl( DSQ_TX_INT_STA );

  dprintk("%s: %x\n",__FUNCTION__,status);

  /* TX Ready */
  if ( status & 0x8 )
  {
    writel ( 0x8, DSQ_TX_CLR_INT_STA );
    writel ( readl( DSQ_TX_INT_EN ) &~ 0x8, DSQ_TX_INT_EN );
    diseqc_int_flag = 1;
  }

  return IRQ_HANDLED;
}

/* 0x18068000 */
static int stm_diseqc_probe ( struct platform_device *stm_diseqc_device_data)
{
  unsigned long start,end;

  if (!stm_diseqc_device_data || !stm_diseqc_device_data->name) 
  {
    printk(KERN_ERR "%s: Device probe failed.  Check your kernel SoC config!!\n",__FUNCTION__);    
    return -ENODEV;
  }

  /* Get the configuration information about the merger */

  start = platform_get_resource(stm_diseqc_device_data,IORESOURCE_MEM,0)->start;
  end   = platform_get_resource(stm_diseqc_device_data,IORESOURCE_MEM,0)->end;  
  diseqc_registers = (unsigned int)ioremap( start, (end-start)+1 );
  
  if (request_irq(platform_get_resource(stm_diseqc_device_data,IORESOURCE_IRQ,0)->start,
		  stm_diseqc_irq ,IRQF_DISABLED , "Diseqc", NULL)) 
    goto err1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
  {
    struct plat_diseqc_config *config	= (struct plat_diseqc_config*)stm_diseqc_device_data->dev.platform_data;
    stpio_request_pin( config->diseqc_pio_bank, config->diseqc_pio_pin,"DiseqC", STPIO_ALT_OUT); 
  }
#else
  {
    struct stm_pad_config *config	= (struct stm_pad_config*)stm_diseqc_device_data->dev.platform_data;
    stm_pad_claim( config, "DiseqC" );
  }
#endif
  
  writel(    2, DSQ_TX_PRESCALER );
  writel( 1511, DSQ_TX_SUBCARR_DIV );
  
  writel(   33, DSQ_TX_SYMBOL_PER );
  writel(   22, DSQ_TX_SYMBOL0_ONTIME );
  writel(   11, DSQ_TX_SYMBOL1_ONTIME );
  
	//writel(   22, DSQ_TX_SILENCE_PER );

  writel( 0   , DSQ_TX_SW_RST );
  writel( 0   , DSQ_TX_EN );
  writel( 0   , DSQ_TX_INT_EN );
 
  return 0;

 err1:
  iounmap( (void*)diseqc_registers );
  return -ENODEV;
}

static int stm_diseqc_remove( struct platform_device *stm_diseqc_device_data)
{
  iounmap( (void*)diseqc_registers );
  return 0;
}

static struct platform_driver stm_diseqc_driver = {
	.driver = {
        .name           = "stm-diseqc",
        .bus            = &platform_bus_type,
		.owner  = THIS_MODULE,
	},
        .probe          = stm_diseqc_probe,
        .remove         = stm_diseqc_remove,
};

static __init int stm_diseqc_init(void)
{
  return platform_driver_register(&stm_diseqc_driver);
}

static void stm_diseqc_cleanup(void)
{
  platform_driver_unregister(&stm_diseqc_driver);
}

module_init(stm_diseqc_init);
module_exit(stm_diseqc_cleanup);

module_param(debug, int,0644);
MODULE_PARM_DESC(debug, "Debug or not");

MODULE_AUTHOR("Peter Bennett <peter.bennett@st.com>");
MODULE_DESCRIPTION("STM DiseqC Driver");
MODULE_LICENSE("GPL");


void stm_diseqc_register_frontend ( struct dvb_frontend *fe )
{
  if (!diseqc_registers)
    return;
  
  fe->ops.diseqc_send_master_cmd = stm_diseqc_send_msg;
  fe->ops.diseqc_send_burst      = stm_send_diseqc_burst;
}
EXPORT_SYMBOL(stm_diseqc_register_frontend);

