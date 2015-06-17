/*
 * ST-fei DVB driver
 *
 * Copyright (c) STMicroelectronics 2005
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>

#include <linux/io.h>

#include <linux/bpa2.h>

#include "dvb_frontend.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_net.h"

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <linux/stm/stm-frontend.h>

#include "fei.h"
#include "st-common.h"
#include "st-merger.h"

#undef SWTS_TEST

#define STFEI_MAXCHANNEL 32
#define STFEI_MAXADAPTER 3

struct stfei {
  struct stfe *stfe[STFEI_MAXADAPTER];

  int mapping[16];

  struct semaphore sem;
  
  spinlock_t timer_lock;
  struct timer_list timer;	/* timer interrupts for outputs */

  unsigned char *back_buffer;
  unsigned long back_buffer_physical;
  unsigned long fei_io;
  unsigned long fei_sram;
  unsigned long back_buffer_size;

  unsigned long fei_back_buffer_rp;

  struct stfe_input_record *irec;
};

#define HEADER_SIZE (6)
#define PACKET_SIZE (188+HEADER_SIZE)

#define FEI_BUFFER_SIZE (8*192*340)
//(16*(PACKET_SIZE)*100) /*(188*1024)*/

/***************************************************************************************/


/*

0x10   ----------------- <--- FEI_DMA_x_BASE
       |               |   |
       |               |  \|/
       |               | <--- FEI_DMA_x_READ  (Last address sent to ADSC)
       |               |
       |               |
       |               | <--- FEI_DMA_x_WRITE (Last address written by FEI)
       |               |   ^
       |               |   |  (free space)
       |               |   |
0x1000 ----------------- <--- FEI_DMA_x_TOP

0x10   ----------------- <--- FEI_DMA_x_BASE
       |               |   
       |               |  
       |               | <--- FEI_DMA_x_WRITE (Last address written by FEI)
       |               |   |
       |               |   | (free space)
       |               | <--- FEI_DMA_x_READ  (Last address sent to ADSC)
       |               |   
       |               |   
       |               |   
0x1000 ----------------- <--- FEI_DMA_x_TOP

*/
/* We should make the FEI interrupt us every sensible packet period */
/* For now we shall just interrupt when ever the timer period is up */
/* And we assume buffer is a multiple of th packet length */

static void stfei_timer_interrupt(unsigned long astfei)
{
  struct stfei *fei = (struct stfei*)astfei;
  static int a = 0;

#if 0
  a++;
  printk("%s:%d %x %x %x\n",__FUNCTION__,__LINE__,
	 0,//readl( fei->fei_io + STFE_IB_STAT ),
	 readl( fei->fei_io + STFE_IB_READ_PNT ),
	 readl( fei->fei_io + STFE_IB_WRT_PNT));

  if (a>10)
	  printk("%s:%d %x\n",__FUNCTION__,__LINE__,
		 readl( fei->fei_io + STFE_IB_STAT ));

  printk("%s:%d %x %x %x %x %x\n",__FUNCTION__,__LINE__,
	 readl( fei->fei_io + STFE_SYS_STAT ),
	 fei->irec[0].dma_buswp,
	 fei->irec[0].dma_busbase,
	 readl( fei->fei_io + STFE_DMA_MEMRP ),
	 readl( fei->fei_io + STFE_DMA_BUSWP )

	 );
#endif

  unsigned long wp = fei->irec[0].dma_buswp;
  unsigned long rp = fei->fei_back_buffer_rp;

  if (wp < rp) 
	  wp = fei->back_buffer_physical + fei->back_buffer_size;

  int pos = rp - fei->back_buffer_physical;
  int pos2 = pos;
  int num_packets = (wp - rp) / 192;
  int n;

//  printk("%x %x %d %x\n",wp,rp,num_packets,pos);

#if 1
  for (n=0;n<num_packets;n++) {
	  dvb_dmx_swfilter_packets(&fei->stfe[0]->demux[0].dvb_demux, 
				   &fei->back_buffer[pos] , 1);

#if 1
	  if (fei->back_buffer[pos] != 0x47)
		  if (!(a++ % 100))
			  printk("%s: %d %x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				 __FUNCTION__,
				 a,
				 pos/192,
				 fei->back_buffer[pos],
				 fei->back_buffer[pos+1],
				 fei->back_buffer[pos+2],
				 fei->back_buffer[pos+3],
				 fei->back_buffer[pos+4],
				 fei->back_buffer[pos+5],
				 fei->back_buffer[pos+6],
				 fei->back_buffer[pos+7]);
#endif
	  pos+=192;
  }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
  dma_cache_wback(&fei->back_buffer[pos2],num_packets *192);
#else
  writeback_ioremap_region (0, &fei->back_buffer[pos2], 0, num_packets *19);
#endif

#endif

  if (wp == (fei->back_buffer_physical + fei->back_buffer_size))
	  fei->fei_back_buffer_rp = fei->back_buffer_physical;
  else
	  fei->fei_back_buffer_rp = wp;
  	       
  fei->timer.expires = 2 + jiffies;
  add_timer(&fei->timer);
}

static int stfei_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
  struct stfe  *stfe  = ((struct stfe *) dvbdmxfeed->demux);
  struct stfei *fei = (struct stfei*)stfe->driver_data;
  struct stfe_channel *channel;

  switch (dvbdmxfeed->type) {
  case DMX_TYPE_TS:
    break;
  case DMX_TYPE_SEC:
    break;
  default:
    return -EINVAL;
  }

  if (dvbdmxfeed->type == DMX_TYPE_TS) {
    switch (dvbdmxfeed->pes_type) {
    case DMX_TS_PES_VIDEO:
    case DMX_TS_PES_AUDIO:
    case DMX_TS_PES_TELETEXT:
    case DMX_TS_PES_PCR:
    case DMX_TS_PES_OTHER:
      channel = stfe_channel_allocate(stfe);
      break;
    default:
      return -EINVAL;
    }
  } else {
    channel = stfe_channel_allocate(stfe);
  }
  
  if (!channel)
    return -EBUSY;

  dvbdmxfeed->priv    = channel;
  channel->dvbdmxfeed = dvbdmxfeed;
  channel->pid        = dvbdmxfeed->pid;
  channel->type       = 1;
  channel->id         = 0;//*stfei->num_pids;
  channel->active     = 1;


  if (stfe->running_feed_count==0)
  {
    writel( STFE_SYS_ENABLE, fei->fei_io + STFE_IB_SYS);

    fei->timer.expires = 1 + jiffies;
    add_timer(&fei->timer);

    //stfei_reset_dma();
    //stfei_start_dma(stfei);

    dprintk("%s: Starting DMA feed\n",__FUNCTION__);    
  }
  
  stfe->running_feed_count++;

  return 0;
}

static int stfei_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct stfe_channel *channel =
	    (struct stfe_channel *) dvbdmxfeed->priv;
	struct stfe  *stfe  = ((struct stfe *) dvbdmxfeed->demux);
	struct stfei *fei = (struct stfei*)stfe->driver_data;
	int n;
	
	if (--stfe->running_feed_count == 0)
	{
	  writel( 0 , fei->fei_io + STFE_IB_SYS);

	  del_timer(&fei->timer);

	  dprintk("%s: Stopping DMA feed\n",__FUNCTION__);
	}

	channel->active = 0;

	return 0;
}

/********************************************************/

static int stfei_probe(struct platform_device *stfei_device_data)
{
  struct stfei *fei;
  struct plat_frontend_config *config;
  int    n;
  unsigned long start;

  if (!stfei_device_data || !stfei_device_data->name) 
  {
    printk(KERN_ERR "%s: Device probe failed.  Check your kernel SoC config!!\n",__FUNCTION__);    
    return -ENODEV;
  }

  /* Allocate the stfei structure */
  fei = (struct stfei*)kmalloc(sizeof(struct stfei),GFP_KERNEL);
  if (!fei)
    return -ENOMEM;

  memset((void*)fei, 0, sizeof(struct stfei));
  
  /* Allocate the back buffer */
//  fei->back_buffer = (char*)bigphysarea_alloc_pages((FEI_BUFFER_SIZE+PAGE_SIZE-1) / PAGE_SIZE,0,GFP_KERNEL);
//  if (!fei->back_buffer)
//    goto err1;
  fei->back_buffer_physical = bpa2_alloc_pages(bpa2_find_part("bigphysarea"),(FEI_BUFFER_SIZE+PAGE_SIZE-1) / PAGE_SIZE,0,GFP_KERNEL);
  fei->back_buffer = (char*)ioremap_cache(fei->back_buffer_physical,FEI_BUFFER_SIZE);


  /* ioremap the fei address space */
  start = platform_get_resource(stfei_device_data,IORESOURCE_MEM,0)->start;
  fei->fei_io = (unsigned long)ioremap( (unsigned long)start, platform_get_resource(stfei_device_data,IORESOURCE_MEM,0)->end - start);
  if (!fei->fei_io)
    goto err2;

  start = platform_get_resource(stfei_device_data,IORESOURCE_MEM,1)->start;
  fei->fei_sram = (unsigned long)ioremap( (unsigned long)start, platform_get_resource(stfei_device_data,IORESOURCE_MEM,1)->end - start);
  if (!fei->fei_io)
    goto err2;

  /* Get the configuration information about the tuners */
  config = (struct plat_frontend_config*)stfei_device_data->dev.platform_data;

  stm_tuner_register_frontend ( config, fei->stfe , (void*)fei, stfei_start_feed, stfei_stop_feed );

  for (n=0;n<3;n++)
    if (fei->stfe[n])
      fei->mapping[ STM_GET_CHANNEL(config->channels[fei->stfe[n]->mapping].option) ] = n;

  unsigned int options = config->channels[0].option;

  writel( (options & STM_SERIAL_NOT_PARALLEL ? STFE_SERIAL_NOT_PARALLEL : 0) |
	  (options & STM_INVERT_CLOCK        ? STFE_INVERT_TSCLK        : 0) |
	   (options & STM_PACKET_CLOCK_VALID  ? STFE_AYSNC_NOT_SYNC      : 0) |
	  STFE_ALIGN_BYTE_SOP | STFE_BYTE_ENDIANESS_MSB,fei->fei_io + STFE_IB_IP_FMT_CFG);

  writel( STFE_SYNC(config->channels[0].lock) |
	  STFE_DROP(config->channels[0].drop) |
	  STFE_TOKEN(0x47),
	  fei->fei_io + STFE_IB_SYNCLCKDRP_CFG);

  writel( 188, fei->fei_io + STFE_IB_PKT_LEN );
  
  writel( 4096,      fei->fei_io + STFE_IB_BUFF_STRT);
  writel( 8191,      fei->fei_io + STFE_IB_BUFF_END);
  writel( 4096,      fei->fei_io + STFE_IB_READ_PNT);
  writel( 4096,      fei->fei_io + STFE_IB_WRT_PNT);

  // 4 packets have to be 32 byte aligned, each packets is 8 bytes aligned remember.
  fei->irec = (struct stfe_input_record *)fei->fei_sram;
  fei->irec[0].dma_busbase  = fei->back_buffer_physical;
  fei->irec[0].dma_bustop   = fei->back_buffer_physical + (FEI_BUFFER_SIZE/2) - 1;
  fei->irec[0].dma_buswp    = fei->back_buffer_physical;
  fei->irec[0].dma_pkt_size = (188 + 7) &~7;
  fei->irec[0].dma_busnextbase = fei->back_buffer_physical + (FEI_BUFFER_SIZE/2);
  fei->irec[0].dma_busnexttop = fei->back_buffer_physical + (FEI_BUFFER_SIZE) - 1;;
  fei->irec[0].dma_membase  = 4096;
  fei->irec[0].dma_memtop   = 8191;

  fei->fei_back_buffer_rp = fei->back_buffer_physical;
  fei->back_buffer_size = FEI_BUFFER_SIZE;

  writel( 1<<16,fei->fei_io +  STFE_DMA_ERR_OVER );

  fei->timer.data     = (unsigned long)fei;
  /* Setup timer interrupt */
  spin_lock_init(&fei->timer_lock);
  fei->timer.function = stfei_timer_interrupt;
  init_timer(&fei->timer);
  fei->timer.function = stfei_timer_interrupt;  

  return 0;

 err3:
  iounmap( (void*)fei->fei_io );

 err2:
  kfree(fei->back_buffer);

 err1:
  kfree(fei);

  return -ENOMEM;
}

static int stfei_remove(struct platform_device *stfei_device_data)
{
#warning we need to do the deallocation, however in a STB this may not be too much of an issue
  return 0;
}

static struct platform_driver stfei_driver = {
       .driver = {
        .name           = "fei",
        .bus            = &platform_bus_type,
		.owner  = THIS_MODULE,
	},
        .probe          = stfei_probe,
        .remove         = stfei_remove,
};

static __init int stfei_init(void)
{
  return platform_driver_register(&stfei_driver);
}

static void stfei_cleanup(void)
{
	platform_driver_unregister(&stfei_driver);
}

module_init(stfei_init);
module_exit(stfei_cleanup);

module_param(stm_debug, int,0644);
MODULE_PARM_DESC(stm_debug, "Debug or not");

MODULE_AUTHOR("Peter Bennett <peter.bennett@st.com>");
MODULE_DESCRIPTION("STFEI DVB Driver");
MODULE_LICENSE("GPL");

