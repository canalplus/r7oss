/*
 * STM TSMerger driver
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

#include <linux/device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/stm/stm-dma.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/io.h>
#include <asm/cacheflush.h>

#include <linux/platform_device.h>
#include <linux/stm/stm-frontend.h>
#include "tsmerger.h"

#ifndef TSM_NUM_PTI_ALT_OUT
unsigned long TSM_NUM_PTI_ALT_OUT;
unsigned long TSM_NUM_1394_ALT_OUT;
#define LOAD_TSM_DATA
#endif

static int debug;
#define dprintk(x...) if (debug) printk(KERN_WARNING x)

/* Maximum number of pages we send in 1 FDMA transfer */
#define MAX_SWTS_PAGES 260
#define MAX_SWTS_CHANNELS 4

struct stm_tsm_handle {
  unsigned long tsm_io;
  unsigned long tsm_swts;
  unsigned long tsm_swts_bus;

  int swts_channel;

  /* FDMA for SWTS */
  int                        fdma_channel;
  struct stm_dma_params      fdma_params;  
  unsigned long              fdma_reqline;
  struct stm_dma_req        *fdma_req;

  /* Stuffying packets */
  unsigned char             *stuffing;
  unsigned char             *channel[MAX_SWTS_CHANNELS];
  unsigned char              channel_polarity[MAX_SWTS_CHANNELS];

  struct stm_dma_params      stuffing_params;
  struct stm_dma_params      channel_params[MAX_SWTS_CHANNELS];

  struct page               *swts_pages[MAX_SWTS_PAGES];
  struct stm_dma_params      swts_params[MAX_SWTS_PAGES];
  struct scatterlist         swts_sg[MAX_SWTS_PAGES];

  struct mutex               swts_lock;

  unsigned int		     nodes_in_transfer;
  unsigned int		     transfer_granularity;
};

static struct stm_tsm_handle *tsm_handle = NULL;

struct stm_dma_req_config fdma_req_config_4 = {
  .rw        = REQ_CONFIG_WRITE,
  .opcode    = REQ_CONFIG_OPCODE_4,
  .count     = 1,
  .increment = 0,
  .hold_off  = 0,
  .initiator = 0,
};

struct stm_dma_req_config fdma_req_config_32 = {
  .rw        = REQ_CONFIG_WRITE,
  .opcode    = REQ_CONFIG_OPCODE_32,
  .count     = 1,
  .increment = 0,
  .hold_off  = 0,
  .initiator = 0,
};

static const char *fdmac_id[]    = { STM_DMAC_ID, NULL };
static const char *fdma_cap_hb[] = { STM_DMA_CAP_HIGH_BW, NULL };

#define SWTS_BLOCK_SIZE 4

void stm_tsm_inject_data( struct stm_tsm_handle *handle, u32 *data, off_t size)
{
  int blocks = (size + SWTS_BLOCK_SIZE - 1 ) / SWTS_BLOCK_SIZE;
  int count  = size;
  int words;
  u32 *p = data;
  int n;
  int m;
  u32 *addr = (u32*)handle->tsm_swts;

  for (n=0;n<blocks;n++) {
    while( !(readl(handle->tsm_io + TSM_SWTS_CFG(0)) & TSM_SWTS_REQ) ) {
	    dprintk("%s: Waiting for FIFO %x\n",__FUNCTION__,readl(handle->tsm_io + TSM_SWTS_CFG(0)));
	    msleep(10);
    }

    if (count > SWTS_BLOCK_SIZE)
      words = SWTS_BLOCK_SIZE/4;
    else
      words = count / 4;

    count -= words * 4;

    for (m=0;m<words;m++) {
	    *addr = *p;
	    p++;
    }
  }

}

    //
    // The scatterlist 2 functionality, inserted by nick
    // this uses subsiduary fns to accumulate transfers from a scatter list,
    // that require the same transfer granularity, switching when optimal.
    //

static void initialise_node_list( struct stm_tsm_handle *handle )
{
    handle->nodes_in_transfer	= 0;
}

//

static void add_transfer( struct stm_tsm_handle *handle, unsigned int address, unsigned int size )
{
int		Status;
unsigned int	transfer_granularity;

    //
    // First do we need to perform the ones we already have
    //

    transfer_granularity	= ((size & 0x1f) == 0) ? 32 : 4;

    if( (handle->nodes_in_transfer != 0) &&
	((handle->transfer_granularity != transfer_granularity) || (handle->nodes_in_transfer >= MAX_SWTS_PAGES) || (size == 0)) )
    {
	dma_compile_list( handle->fdma_channel, &handle->swts_params[0], GFP_ATOMIC);
	Status      = dma_xfer_list( handle->fdma_channel, &handle->swts_params[0] );
	if( Status )
	{
	    printk( "stm_tsm_dma_xfer_scatterlist->add_transfer - Failed to inject packet (%d).\n", Status );
	    handle->nodes_in_transfer	= 0;
	    return;
	}

	dma_wait_for_completion(handle->fdma_channel);
	dma_req_free( handle->fdma_channel, handle->fdma_req );

	handle->nodes_in_transfer	= 0;
    }

    //
    // Now proceed to add next transfer
    //

    if( size == 0 )
	return;

    handle->transfer_granularity	= transfer_granularity;

    if( handle->nodes_in_transfer == 0 )
    {
	handle->fdma_req    = dma_req_config( handle->fdma_channel, handle->fdma_reqline, 
						(handle->transfer_granularity == 32) ? &fdma_req_config_32 : &fdma_req_config_4 );
    }
    else
    {
	dma_params_link( &handle->swts_params[handle->nodes_in_transfer - 1], &handle->swts_params[handle->nodes_in_transfer] );
    }

    dma_params_link(  &handle->swts_params[handle->nodes_in_transfer], NULL );
    dma_params_req(   &handle->swts_params[handle->nodes_in_transfer], handle->fdma_req );
    dma_params_addrs( &handle->swts_params[handle->nodes_in_transfer], address, handle->tsm_swts_bus, size );

    handle->nodes_in_transfer++;
}

//

int stm_tsm_inject_kernel_data( struct stm_tsm_handle *handle, int pti_id, int stream_id, char *data, off_t size )
{

    //
    // Lock the access mutex
    //

#if 0
   // Don't want to avoid zero size, as this is used in priming the pti
   if( size == 0 ) 
	return 0;
#endif

    if (mutex_lock_interruptible(&handle->swts_lock))
	  return -EAGAIN;

    //
    // Update the header to contain the current polarity,
    // and writeback the region to be output.
    //

    handle->channel[stream_id][3]       = 0x10 | (handle->channel_polarity[stream_id] << 3) | stream_id;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
    dma_cache_wback( handle->channel[stream_id], 4);
    dma_cache_wback(data,size);
#else
    writeback_ioremap_region( 0, handle->channel[stream_id], 0, 4);
    writeback_ioremap_region( 0, data, 0, size);
#endif

    //
    // Transfer starting with the header, the body, and the stuffing tail
    //

    initialise_node_list( handle );

    add_transfer( handle, virt_to_phys(handle->channel[stream_id]), 188 );
    add_transfer( handle, virt_to_phys(data), size );
    add_transfer( handle, virt_to_phys(handle->stuffing), 188 );

    add_transfer( handle, 0, 0 ); // Flush

    //
    // Exit releasing the lock
    //

    mutex_unlock(&handle->swts_lock);
    return 0;
}
EXPORT_SYMBOL(stm_tsm_inject_kernel_data);

//

static int stm_tsm_dma_xfer_scatterlist( struct stm_tsm_handle *handle, int pti_id, int stream_id, struct scatterlist *sg, int size)
{
int			 i;
int                      PageCount;

    //
    // Update the header to contain the current polarity
    //

    handle->channel[stream_id][3]       = 0x10 | (handle->channel_polarity[stream_id] << 3) | stream_id;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
    dma_cache_wback( handle->channel[stream_id], 4);
#else
    writeback_ioremap_region( 0, handle->channel[stream_id], 0, 4);
#endif

    //
    // Get the pages, and transfer starting with the header, the body, and the stuffing tail
    //

    PageCount	= dma_map_sg(NULL, &sg[0], size, DMA_TO_DEVICE);

    initialise_node_list( handle );

    add_transfer( handle, virt_to_phys(handle->channel[stream_id]), 188 );

    for( i=0; i<PageCount; i++ )
	add_transfer( handle, sg_dma_address(&sg[i]), sg_dma_len(&sg[i]) );

    add_transfer( handle, virt_to_phys(handle->stuffing), 188 );
    add_transfer( handle, 0, 0 ); // Flush

    //
    // Exit unmapping the pages
    //

    dma_unmap_sg( NULL, sg, size, DMA_TO_DEVICE );
    return 0;
}

//

int stm_tsm_inject_scatterlist( struct stm_tsm_handle *handle, int pti_id, int stream_id, struct scatterlist *sg, int size)
{
  int n;
  int ret = 0;
  struct scatterlist *curr_sg;

  if (mutex_lock_interruptible(&handle->swts_lock))
	  return -EAGAIN;

  for (n=0; n<size; n++) {
    curr_sg = sg + n;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
    dma_cache_wback(page_address(curr_sg->page) + curr_sg->offset, sg_dma_len(curr_sg));
#else
    writeback_ioremap_region( 0, sg_virt(curr_sg), 0, sg_dma_len(curr_sg));
#endif
  }

  ret = stm_tsm_dma_xfer_scatterlist(handle, pti_id, stream_id, sg, size);

  mutex_unlock(&handle->swts_lock);

  return ret;
}
EXPORT_SYMBOL(stm_tsm_inject_scatterlist);

int stm_tsm_inject_user_data( struct stm_tsm_handle *handle, int pti_id, int stream_id, const char __user *data, off_t size)
{
  int nr_pages;
  int ret = 0;
  int page_offset;
  int remaining;
  int n;
  unsigned long start = (unsigned long)data;
  unsigned long len = size;

  //
  // Nick - Lazy fudge to handle very large transfers
  //

  while( len > ((MAX_SWTS_PAGES-2) * PAGE_SIZE) )
  {
    off_t       SubSize = (((MAX_SWTS_PAGES-2) * PAGE_SIZE) / 188) * 188;

	ret     = stm_tsm_inject_user_data( handle, pti_id, stream_id, (const char __user *)start, SubSize );
	if( ret )
	    return ret;

	len     -= SubSize;
	start   += SubSize;
  }

  //
  // Nick - Check allignment of data, must be at least word aligned due 
  //        to nature of swts port also write upto the 32 byte boundary
  //        to allow the dma to use REQ_CONFIG_OPCODE_32
  //        Finally recalculate align length as the portion less than 
  //        32 bytes after the main block, to be written when the dmas have 
  //        finished.
  //

  if( ((unsigned int)data & 0x3) != 0 )
  {
    printk( "stm_tsm_inject_user_data - data written to dvr port must be at least word aligned.\n" );
    return -EINVAL;
  } 

  if (mutex_lock_interruptible(&handle->swts_lock))
	  return -EAGAIN;

  //
  // Enter normal loop processing
  //

  if (len) {
    nr_pages = (PAGE_ALIGN(start + len) -
		(start & PAGE_MASK)) >> PAGE_SHIFT;

    down_read(&current->mm->mmap_sem);
    ret = get_user_pages(current, current->mm, start,
			 nr_pages, READ, 0, handle->swts_pages, NULL);
    up_read(&current->mm->mmap_sem);

    if (ret < nr_pages) {
      nr_pages = ret;
      ret = -EINVAL;
      goto out_unmap;
    }

    page_offset = start & (PAGE_SIZE-1);
    remaining = len;

    for (n=0; n<nr_pages; n++) {
      int copy = min_t(int, PAGE_SIZE - page_offset, remaining);



#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
      handle->swts_sg[n].page   = handle->swts_pages[n];
      handle->swts_sg[n].offset = page_offset;
      handle->swts_sg[n].length = copy;     
#else
      sg_set_page( &handle->swts_sg[n], handle->swts_pages[n], copy, page_offset );
#endif

      page_offset = 0;
      remaining -= copy;
    }

    //

    ret = stm_tsm_dma_xfer_scatterlist( handle, pti_id, stream_id , &handle->swts_sg[0], nr_pages );

out_unmap:

    for (n = 0; n < nr_pages; n++)
	page_cache_release(handle->swts_pages[n]);
  }

  mutex_unlock(&handle->swts_lock);
  return ret;
}
EXPORT_SYMBOL(stm_tsm_inject_user_data);

void stm_tsm_reset_channel ( struct stm_tsm_handle *handle, int n, int status )
{
unsigned int stream_on;

  if (status == -1)
    status = readl(handle->tsm_io + TSM_STREAM_STATUS(n));

  stream_on = readl(handle->tsm_io + TSM_STREAM_CONF(n)) & TSM_STREAM_ON;

  writel( readl(handle->tsm_io + TSM_STREAM_CONF(n)) & ~TSM_STREAM_ON,
	  handle->tsm_io + TSM_STREAM_CONF(n));

  udelay(500);
  //

  writel( TSM_CHANNEL_RESET, handle->tsm_io + TSM_STREAM_CONF2(n) );
  udelay(500);
  writel( status & ~TSM_RAM_OVERFLOW, handle->tsm_io + TSM_STREAM_STATUS(n) );

  writel( 0, handle->tsm_io + TSM_STREAM_STATUS(n) );

  udelay(500);

  writel( 0, handle->tsm_io + TSM_STREAM_CONF2(n) );

  udelay(500);

  /* Reset the merger */
  writel( 0x0, handle->tsm_io + TSM_SW_RESET);
  writel( 0x6, handle->tsm_io + TSM_SW_RESET);

  udelay(500);

  writel( readl(handle->tsm_io + TSM_STREAM_CONF(n)) | stream_on,
	  handle->tsm_io + TSM_STREAM_CONF(n));

}
EXPORT_SYMBOL(stm_tsm_reset_channel);

void stm_tsm_poll ( struct stm_tsm_handle *handle )
{
  int n;
  for (n=0;n<3;n++)
    {
      int status = readl(handle->tsm_io + TSM_STREAM_STATUS(n));

      if (status & (TSM_RAM_OVERFLOW | TSM_INPUTFIFO_OVERFLOW ))
      {
	stm_tsm_reset_channel(handle,n,status);
	printk(KERN_WARNING"%s: TSMerger RAM Overflow on channel %d(%x), resetting buffer\n",__FUNCTION__,n,status);
      }
    }
}
EXPORT_SYMBOL(stm_tsm_poll);

void  stm_tsm_enable_channel( struct stm_tsm_handle *handle, int chan )
{
	writel( readl(handle->tsm_io + TSM_STREAM_CONF(chan)) | TSM_STREAM_ON,
		handle->tsm_io + TSM_STREAM_CONF(chan));        
}
EXPORT_SYMBOL(stm_tsm_enable_channel);

void  stm_tsm_disable_channel( struct stm_tsm_handle *handle, int chan )
{
	writel( readl(handle->tsm_io + TSM_STREAM_CONF(chan)) & ~TSM_STREAM_ON,
		handle->tsm_io + TSM_STREAM_CONF(chan));        
}
EXPORT_SYMBOL(stm_tsm_disable_channel);

void  stm_tsm_flip_channel_input_polarity( struct stm_tsm_handle *handle, int n)
{
    handle->channel_polarity[n] = 1 - handle->channel_polarity[n];
}
EXPORT_SYMBOL(stm_tsm_flip_channel_input_polarity);

struct stm_tsm_handle *stm_tsm_init( int pti_id, struct plat_frontend_config *config )
{
	int n;

	if (pti_id <0 || pti_id >1)
		return NULL;

	if (!tsm_handle) 
		return NULL;       

	for (n=0;n<config->nr_channels;n++)
	{
		enum plat_frontend_options options = config->channels[n].option; 
		int chan = STM_GET_CHANNEL(options);

		writel( readl(tsm_handle->tsm_io + TSM_DESTINATION(pti_id)) | (1 << chan),
			tsm_handle->tsm_io + TSM_DESTINATION(pti_id));

		writel( (readl(tsm_handle->tsm_io + TSM_STREAM_CONF(chan)) & TSM_RAM_ALLOC_START(0xff)) |
			(options & STM_SERIAL_NOT_PARALLEL ? TSM_SERIAL_NOT_PARALLEL : 0 ) |
			(options & STM_INVERT_CLOCK        ? TSM_INVERT_BYTECLK : 0 ) |
			(options & STM_PACKET_CLOCK_VALID  ? TSM_SYNC_NOT_ASYNC : 0 ) |
			TSM_ADD_TAG_BYTES | TSM_ALIGN_BYTE_SOP | TSM_PRIORITY(0xf),
			tsm_handle->tsm_io + TSM_STREAM_CONF(chan));

		writel( TSM_SYNC(config->channels[n].lock) |
			TSM_DROP(config->channels[n].drop) |
			TSM_SOP_TOKEN(0x47)                |
			TSM_PACKET_LENGTH(188) 
			,tsm_handle->tsm_io + TSM_STREAM_SYNC(chan));
	}

	if (config->nr_channels == 0)
	{
		/* Setup the SWTS */
		int chan = tsm_handle->swts_channel;

		writel( TSM_SWTS_REQ_TRIG(224/16) | TSM_SWTS_AUTO_PACE,                 // Note write 32 s
			tsm_handle->tsm_io + TSM_SWTS_CFG(0));

		writel( readl(tsm_handle->tsm_io + TSM_DESTINATION(pti_id)) | (1 << chan),
			tsm_handle->tsm_io + TSM_DESTINATION(pti_id));  

		writel( (readl(tsm_handle->tsm_io + TSM_STREAM_CONF(chan)) & TSM_RAM_ALLOC_START(0xff)) |
			TSM_PRIORITY(0x7),
			tsm_handle->tsm_io + TSM_STREAM_CONF(chan));

		writel( TSM_SYNC(1) |
			TSM_DROP(1) |
			TSM_SOP_TOKEN(0x47)  |
			TSM_PACKET_LENGTH(188) 
			,tsm_handle->tsm_io + TSM_STREAM_SYNC(chan));

		writel( readl(tsm_handle->tsm_io + TSM_STREAM_CONF(chan)) | TSM_STREAM_ON,
			tsm_handle->tsm_io + TSM_STREAM_CONF(chan));

	}

	return tsm_handle;
}
EXPORT_SYMBOL(stm_tsm_init);

int stm_tsm_probe (struct platform_device *stm_tsm_device_data)
{
  int n;
  unsigned long start,end,size;
  struct plat_tsm_config *config;
  struct stm_tsm_handle *handle;

  if (!stm_tsm_device_data || !stm_tsm_device_data->name) 
  {
    printk(KERN_ERR "%s: Device probe failed.  Check your kernel SoC config!!\n",__FUNCTION__);    
    return -ENODEV;
  }

  /* Get the configuration information about the merger */
  config = (struct plat_tsm_config*)stm_tsm_device_data->dev.platform_data;

  /* Allocate the handle */
  handle = kzalloc(sizeof(*handle),GFP_KERNEL);

  if (!handle)
    return -ENOMEM;

  start = platform_get_resource(stm_tsm_device_data,IORESOURCE_MEM,0)->start;
  end   = platform_get_resource(stm_tsm_device_data,IORESOURCE_MEM,0)->end;
  handle->tsm_io   = (unsigned long)ioremap (start,(end-start)+1);

  start = platform_get_resource(stm_tsm_device_data,IORESOURCE_MEM,1)->start;
  end   = platform_get_resource(stm_tsm_device_data,IORESOURCE_MEM,1)->end;
  handle->tsm_swts     = (unsigned long)ioremap_nocache(start,(end-start)+1);
  handle->tsm_swts_bus = (unsigned long)start;

#ifdef LOAD_TSM_DATA
  TSM_NUM_PTI_ALT_OUT  = config->tsm_num_pti_alt_out;
  TSM_NUM_1394_ALT_OUT = config->tsm_num_1394_alt_out;
#endif

  /* Reset the merger */
  writel( 0x0, handle->tsm_io + TSM_SW_RESET);
  writel( 0x6, handle->tsm_io + TSM_SW_RESET);

  size = (config->tsm_sram_buffer_size / config->tsm_num_channels) >> 6; /* Should be 5 = 32 */
  size--;

  for (n=0;n<config->tsm_num_channels;n++) {
    writel( TSM_RAM_ALLOC_START( size * n ), handle->tsm_io + TSM_STREAM_CONF(n));

    writel( TSM_SYNC(0)          |
	    TSM_DROP(0)          |
	    TSM_SOP_TOKEN(0x47)  |
	    TSM_PACKET_LENGTH(188) 
	    ,handle->tsm_io + TSM_STREAM_SYNC(n));
  }

  writel( TSM_CFG_BYPASS_NORMAL, handle->tsm_io + TSM_SYSTEM_CFG);

  handle->swts_channel = config->tsm_swts_channel;

  /* Now lets get the SWTS info and setup an FDMA channel */
  handle->fdma_reqline    = platform_get_resource(stm_tsm_device_data,IORESOURCE_DMA,0)->start;
  handle->fdma_channel  = request_dma_bycap(fdmac_id, fdma_cap_hb, stm_tsm_device_data->name);  

  /* Initilise the parameters for the FDMA SWTS data injection */
  for (n=0;n<MAX_SWTS_PAGES;n++) {
    dma_params_init(&handle->swts_params[n], MODE_PACED, STM_DMA_LIST_OPEN);
    dma_params_DIM_1_x_0(&handle->swts_params[n]);
    dma_params_req(&handle->swts_params[n],NULL);
  }

  for (n=0;n<MAX_SWTS_CHANNELS;n++) {
	  unsigned char array[] = { 0x47, 0x5f, 0xff, 0x10, 0x00, 0x00, 0xde, 0xad };
	  handle->channel[n] = kzalloc(188,GFP_KERNEL);
	  memcpy(handle->channel[n], array , 8);
	  (handle->channel[n])[3] = 0x10 | n;
	  handle->channel_polarity[n] = 0x00;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
	  dma_cache_wback( handle->channel[n], 188);
#else
	  writeback_ioremap_region( 0, handle->channel[n], 0, 188 );
#endif

	  dma_params_init(&(handle->channel_params[n]), MODE_PACED, STM_DMA_LIST_OPEN);
	  dma_params_DIM_1_x_0(&(handle->channel_params[n]));
	  dma_params_addrs(&(handle->channel_params[n]), 
			     virt_to_phys( handle->channel[n] ), handle->tsm_swts_bus,188);
  }

  handle->stuffing = kzalloc(188,GFP_KERNEL);
  handle->stuffing[0] = 0x47;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
  dma_cache_wback( handle->stuffing, 188);
#else
  writeback_ioremap_region( 0, handle->stuffing, 0, 188 );
#endif
  dma_params_init(&handle->stuffing_params, MODE_PACED, STM_DMA_LIST_OPEN);
  dma_params_DIM_1_x_0(&handle->stuffing_params);
  dma_params_addrs(&handle->stuffing_params, 
		     virt_to_phys( handle->stuffing) , handle->tsm_swts_bus,188);

  mutex_init(&handle->swts_lock);

  tsm_handle = handle;

  return 0;
}

int stm_tsm_remove (struct device *dev)
{
  int n;

	iounmap( (void*)tsm_handle->tsm_io );
	iounmap( (void*)tsm_handle->tsm_swts );
	kfree (tsm_handle);
	if (tsm_handle->stuffing) kfree(tsm_handle->stuffing);
	for (n=0;n<MAX_SWTS_CHANNELS;n++)
		if (tsm_handle->channel[n])
			kfree(tsm_handle->channel[n]);

	mutex_destroy(&tsm_handle->swts_lock);
	tsm_handle = NULL;

  return 0;
}

static struct platform_driver stm_tsm_driver = {
	.driver = {
		.name   = "stm-tsm",
		.bus    = &platform_bus_type,
		.owner  = THIS_MODULE,
	},
	.probe          = stm_tsm_probe,
	.remove         = stm_tsm_remove,
};

static __init int stm_tsm_initialise(void)
{
	return platform_driver_register(&stm_tsm_driver);
}

static void stm_tsm_cleanup(void)
{
	platform_driver_unregister(&stm_tsm_driver);
}

module_init(stm_tsm_initialise);
module_exit(stm_tsm_cleanup);

module_param(debug, int,0644);
MODULE_PARM_DESC(debug, "Debug or not");

MODULE_AUTHOR("Peter Bennett <peter.bennett@st.com>");
MODULE_DESCRIPTION("STM TSM Driver");
MODULE_LICENSE("GPL");
