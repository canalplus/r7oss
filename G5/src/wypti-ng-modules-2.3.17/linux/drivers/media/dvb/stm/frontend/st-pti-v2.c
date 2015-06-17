/*
 * STM PTI DVB driver
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
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/bpa2.h>
#include <linux/version.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include "dvb_frontend.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_net.h"

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/stm-dmx.h>

#include "pti.h"
#include "st-common.h"
#include "st-merger.h"

#include "st-pti.h"


//#define USE_PROFILING
#ifdef USE_PROFILING
static int _pkt_idx = 1;
#endif

/* We need to link this the otherway, but for the moment let's do it this way */
extern struct dvb_device *stm_pti_ca_attach(struct stm_pti *pti, struct stfe *stfe);

#define HEADER_SIZE (6)
#define PACKET_SIZE (188+6)

#define PTI_BUFFER_SIZE (16*(PACKET_SIZE)*200) /*(188*1024)*/

/* PTI write ignores byte enables, this assumes that the firmware doesn't
   update variables sharing the same 32bit word otherwise we might have 
   problems */
static void PtiWrite(volatile unsigned short int *addr,unsigned short int value)
{
	unsigned int *addr2 = (unsigned int*)(((unsigned int)addr) & ~3);
	unsigned int old_value  = *addr2;

	if (((unsigned)addr & 2)) old_value = (old_value & 0x0000ffff) | value << 16;
	else old_value = (old_value & 0xffff0000) | value;

	*addr2 = old_value;
}

/***********************************************************************************/

#define TCASM_LM_SUPPORT
#include "tc_code.h"

static void loadtc(struct stm_pti *pti)
{
	int n;

	for (n=0;n<=TCASM_LableMap[0].Value;n+=4)
		writel(transport_controller_code[n/4], pti->pti_io + PTI_IRAM_BASE + n );

	for (n=0;n<(sizeof(transport_controller_data) * 2);n+=4)
		writel(transport_controller_data[n/4], pti->pti_io + PTI_DRAM_BASE + n);
}

static void *getsymbol(struct stm_pti *pti, const char *symbol)
{
	char temp[128];
	void *result = NULL;
	int n;

	temp[120] = 0;
	sprintf(temp,"::_%s",symbol);

	for (n=0;n<TRANSPORT_CONTROLLER_LABLE_MAP_SIZE;n++)
		if (!strcmp(temp,TCASM_LableMap[n].Name))
			result = (void*)(pti->pti_io + TCASM_LableMap[n].Value);

	if (!result)
		printk(KERN_WARNING "%s: Couldn't resolve symbol, we are likely to crash...\n",__FUNCTION__);

	return result;

}

TCASM_LM_t *findsymbol(struct stm_pti *pti, int value)
{
	int min = 65536;
	int index = -1;
	int n;

	for (n=0;n<TRANSPORT_CONTROLLER_LABLE_MAP_SIZE;n++)
		if (TCASM_LableMap[n].Segment == TCASM_LM_CODE)
			if (TCASM_LableMap[n].Value >= value && TCASM_LableMap[n].Value < min)
			{ min = TCASM_LableMap[n].Value; index = n; }

	if (index>=0)
		return &TCASM_LableMap[index];

	return NULL;    
}

/***************************************************************************************/

static void stm_pti_setup_dma(struct stm_pti *pti)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
			dma_cache_inv((void*)pti->back_buffer,PTI_BUFFER_SIZE);
#else
			invalidate_ioremap_region (0, (void*)pti->back_buffer, 0, PTI_BUFFER_SIZE);
#endif
	/* Setup DMA0 to transfer the data to the buffer */
	writel( virt_to_phys(pti->back_buffer), pti->pti_io + PTI_DMA_0_BASE );
	writel( virt_to_phys(pti->back_buffer), pti->pti_io + PTI_DMA_0_WRITE);
	writel( virt_to_phys(pti->back_buffer), pti->pti_io + PTI_DMA_0_READ );
	writel( virt_to_phys(pti->back_buffer + PTI_BUFFER_SIZE - 1), pti->pti_io + PTI_DMA_0_TOP );

	writel( 0x8, pti->pti_io + PTI_DMA_0_SETUP ); /* 8 word burst */
}

static void stm_pti_start_dma(struct stm_pti *pti)
{
	/* Enable the DMA */
	writel( readl(pti->pti_io + PTI_DMA_ENABLE) | 0x1, pti->pti_io + PTI_DMA_ENABLE);

	/* Tell PTI we have lots of room in buffer */
	PtiWrite(pti->pread, 0);
	PtiWrite(pti->discard, 0);
	PtiWrite(pti->pwrite,0);

	/* Reset all the counts */
	pti->loop_count = 0;
	pti->loop_count2 = 0;
	pti->packet_count=0;
}

static void stm_pti_stop_dma(struct stm_pti *pti)
{
	int loop_count = 0;

	/* Stop DMAing data */
	PtiWrite(pti->pread, 0);
	PtiWrite(pti->pwrite, *pti->psize + 2);

	udelay(10);

	while ( readw(pti->pwrite) != (readw(pti->psize) + 2))
	{
		udelay(10);
		PtiWrite(pti->pwrite, *pti->psize + 2);
		dprintk("%s: PW(%u) PS(%u) PR(%u) DIS(%u) \n",
			__FUNCTION__,
			*pti->pwrite,
			*pti->psize,
			*pti->pread,
			*pti->discard);
	};

	udelay(100);

	/* Ensure DMA is enabled */
	writel( readl(pti->pti_io + PTI_DMA_ENABLE) | 0x1, pti->pti_io + PTI_DMA_ENABLE );

	/* Do DMA Done to ensure all data has been transfered */
	writel( PTI_DMA_DONE , pti->pti_io + PTI_DMA_0_STATUS );

	while( (readl(pti->pti_io + PTI_DMA_0_STATUS) & PTI_DMA_DONE) && 
	       (loop_count<100)) 
	{ udelay(1000); loop_count++; };

	/* Disable the DMA */
	writel(readl(pti->pti_io + PTI_DMA_ENABLE) & ~0x1, pti->pti_io + PTI_DMA_ENABLE);

	/* Ensure we will just discard packets... */
	PtiWrite(pti->pwrite, *pti->psize + 2);
}

static void stm_pti_reset_dma(struct stm_pti *pti)
{
	/* Stop the DMA */
	stm_pti_stop_dma(pti);

	/* Reset dma pointers */
	stm_pti_setup_dma(pti);
}


/*

  0x10   ----------------- <--- PTI_DMA_x_BASE
  |               |   |
  |               |  \|/
  |               | <--- PTI_DMA_x_READ  (Last address sent to ADSC)
  |               |
  |               |
  |               | <--- PTI_DMA_x_WRITE (Last address written by PTI)
  |               |   ^
  |               |   |  (free space)
  |               |   |
  0x1000 ----------------- <--- PTI_DMA_x_TOP

  0x10   ----------------- <--- PTI_DMA_x_BASE
  |               |   
  |               |  
  |               | <--- PTI_DMA_x_WRITE (Last address written by PTI)
  |               |   |
  |               |   | (free space)
  |               | <--- PTI_DMA_x_READ  (Last address sent to ADSC)
  |               |   
  |               |   
  |               |   
  0x1000 ----------------- <--- PTI_DMA_x_TOP

*/
/* We should make the PTI interrupt us every sensible packet period */
/* For now we shall just interrupt when ever the timer period is up */
/* And we assume buffer is a multiple of th packet length */

static void stm_pti_timer_interrupt(unsigned long apti)
{
	struct stm_pti *pti = (struct stm_pti*)apti;
	//  struct stpti *stpti = (struct stpti*)stfei2->driver_data;
	unsigned int pti_rp,pti_wp,pti_base,pti_top,size_first,num_packets,new_rp,pti_status,dma_status;
	static unsigned int pti_status_old=0;
	static unsigned int dma_status_old=0;
	int n;

	/* Ok so we take the current time, and the next time we get scheduled is ...
	   time = (buffersize / byterate) / 4 ( so we wait for 1/4th of the buffer ), about every 24ms
	*/
	pti->timer.expires = jiffies + msecs_to_jiffies( PTI_BUFFER_SIZE / (200*1000) );

	stm_tsm_poll(pti->tsm);

	/* Load the read and write pointers, so we know where we are in the buffers */
	pti_rp   = readl(pti->pti_io + PTI_DMA_0_READ);
	pti_wp   = readl(pti->pti_io + PTI_DMA_0_WRITE);
	pti_base = readl(pti->pti_io + PTI_DMA_0_BASE);
	pti_top  = pti_base + PTI_BUFFER_SIZE;

	/* Read status registers */
	pti_status = readl(pti->pti_io + PTI_IIF_FIFO_COUNT);
	dma_status = readl(pti->pti_io + PTI_DMA_0_STATUS);

	/* Error if we overflow */
	if (pti_status & PTI_IIF_FIFO_FULL) printk(KERN_WARNING "%s: IIF Overflow\n",__FUNCTION__);
	if (dma_status != dma_status_old) dprintk("%s: DMA Status %x %x\n",__FUNCTION__,dma_status,pti->loop_count);

	/* If the PTI had to drop packets because we couldn't process in time error */
	if (*pti->discard) 
	{ 
		printk(KERN_WARNING "%s: PTI%01d had to discard %u packets %u %u\n",__FUNCTION__,pti->id,
		       *pti->discard,*pti->pread,*pti->pwrite);
		printk(KERN_WARNING "%s: Reseting DMA\n",__FUNCTION__);

		/* If this happens the PTI stops outputing so we need to reset it */
		stm_pti_reset_dma(pti);
		stm_pti_start_dma(pti);

		add_timer(&pti->timer);
		return;
	}

	pti_status_old = pti_status;
	dma_status_old = dma_status;

	/* If we get to the bottom of the buffer wrap the pointer back to the top */
	if ((pti_rp & ~0xf) == (pti_top & ~0xf)) pti_rp = pti_base;

	/* Calculate the amount of bytes used in the buffer */
	if (pti_rp <= pti_wp) size_first = pti_wp - pti_rp;
	else size_first = pti_top - pti_rp;

	/* Calculate the number of packets in the buffer */
	num_packets = size_first / PACKET_SIZE;
	/* If we have some packets */
	if (num_packets)
	{
		/* And the PTI has acknowledged the updated packets */
		if (!*pti->pread)
		{
			/* Increment the loop counter */
			pti->loop_count++;

			/* We should only read this data so lets invalidate the cache so we can see what is behind it */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
			dma_cache_inv((void*)&pti->back_buffer[pti_rp - pti_base],num_packets * PACKET_SIZE);
#else
			invalidate_ioremap_region (0, (void*)&pti->back_buffer[pti_rp - pti_base], 0, num_packets * PACKET_SIZE);
#endif

			/* This Macro helps us get the current bytes out of the buffer */
#define PR_A(x) (((unsigned int)pti->back_buffer[(pti_rp - pti_base) + (x)]) & 0xff)

			/* The read pointer should always be a multiple of the packet size */
			if ((pti_rp - pti_base) % PACKET_SIZE) 
				printk(KERN_WARNING "%s: 0x%x not multiple of %d\n",__FUNCTION__,(pti_rp - pti_base),PACKET_SIZE);

			/* The first bytes of the packet after the header should always be a 0x47 if not we got problems */
			if (PR_A(HEADER_SIZE) != 0x47) 
				printk(KERN_INFO "%s: @ 0x%p %x %x %u %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",__FUNCTION__,
				       &pti->back_buffer[(pti_rp - pti_base)],
				       pti_status,dma_status,pti->loop_count,
				       PR_A(0),PR_A(1),PR_A(2),PR_A(3),PR_A(4),PR_A(5),PR_A(6),PR_A(7),PR_A(8),PR_A(9));


			/* Now go through each packet, check it's tag and place it in the correct demux buffer */
			for (n=0;n<num_packets;n++) {
			    unsigned char       stream_id       = (PR_A(n * PACKET_SIZE) >> 2) & 0x7;
			    unsigned char       polarity        = (PR_A(n * PACKET_SIZE) >> 5) & 0x1;
#if 0
//printk( "Byte %02x - %d %d %d - %4d - %4d %6d - %08x\n", PR_A(n*PACKET_SIZE), stream_id, polarity, pti->channel_polarity[stream_id], (num_packets - 1 - n), num_packets, size_first, pti_wp );
				printk("%x %x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x\n",pti_rp,pti_base,
				       PR_A(n*PACKET_SIZE),
				       PR_A(n*PACKET_SIZE+1),
				       PR_A(n*PACKET_SIZE+2),
				       PR_A(n*PACKET_SIZE+3),
				       PR_A(n*PACKET_SIZE+4),
				       PR_A(n*PACKET_SIZE+5),
				       PR_A(n*PACKET_SIZE+6),
				       PR_A(n*PACKET_SIZE+7),
				       PR_A(n*PACKET_SIZE+8),
				       PR_A(n*PACKET_SIZE+9));
#endif

#ifdef USE_PROFILING
                if (_pkt_idx == 0) {
                    _pkt_idx--;
                    printk("PTI: PROFILING: OUT: %u\n", jiffies_to_msecs(jiffies));
                }
#endif

				if( pti->mapping[stream_id] &&
				    (polarity == pti->channel_polarity[stream_id]) )
					dvb_dmx_swfilter_packets(&pti->mapping[stream_id]->dvb_demux,
								 &pti->back_buffer[(pti_rp - pti_base) + (n * PACKET_SIZE) + HEADER_SIZE] , 1);
			}
			/* Update the read pointer based on the number of packets we have processed */
			new_rp = pti_rp + num_packets * PACKET_SIZE;

			/* Increment the packet_count, by the number of packets we have processed */
			pti->packet_count+=num_packets;
			if (new_rp > pti_top) dprintk("You b@*tard you killed kenny\n");

			/* If we have gone round the buffer */
			if (new_rp >= pti_top) 
			{ 
				/* Update the read pointer so it now points back to the top */
				new_rp = pti_base + (new_rp - pti_top); pti->loop_count2++; 

				/* Print out some useful debug information when debugging is on */
				dprintk("%s: round the buffer %u times %u=pwrite %u=packet_count %u=num_packets %lu=jiffies %u %u\n",__FUNCTION__, 
					pti->loop_count2,*pti->pwrite,pti->packet_count,num_packets,jiffies,pti_rp,pti_wp); 

				/* Update the packet count */
				pti->packet_count = 0; 
			}

			/* Now update the read pointer in the DMA engine */
			writel(new_rp, pti->pti_io + PTI_DMA_0_READ );

			/* Now tell the firmware how many packets we have read */
			PtiWrite(pti->pread,num_packets);
		}
	}

	/* Schedule us for the next but one tick event (20ms time from when we entered the handler) */
	add_timer(&pti->timer);
}

static void stm_pti_plugin_notify(int start,struct stdemux *stdemux,unsigned int pid)
{
	int i;
	for (i=0;i<sizeof(stfe_plugins)/sizeof(struct stfe_plugin*);i++)
		if ((stfe_plugins[i] != NULL) && (stfe_plugins[i]->type == STFE_PLUGIN_CA))
		{
			if(start)
			{
				if(stfe_plugins[i]->start_feed_notify)
					stfe_plugins[i]->start_feed_notify(stdemux,pid);
			}
			else
			{
				if(stfe_plugins[i]->stop_feed_notify)
					stfe_plugins[i]->stop_feed_notify(stdemux,pid);
			}
		}
}

static int stm_pti_start_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct dvb_demux  *demux        = dvbdmxfeed->demux;
	struct stdemux         *stdemux = container_of( demux, struct stdemux, dvb_demux );
	struct stfe            *stfe    = container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti         *pti     = (struct stm_pti*)stfe->driver_data;
	struct pti_stream_info *info    = &pti->stream[stdemux->stream_channel];

	struct stfe_channel *channel = NULL;

	dprintk("%s: entered %p %p %d(%d)\n",__FUNCTION__,stfe,pti,stdemux->channel, stdemux->stream_channel);

	if (info->NumPids >= STPTI_MAXCHANNEL) {
		printk(KERN_ERR "%s:%d ** Exceded maximum allocated channels, would have probably caused memory corruption before!!\n",__FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	switch (dvbdmxfeed->type) {
	case DMX_TYPE_TS:
		break;
	case DMX_TYPE_SEC:
		break;
	default:
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&pti->lock))
		return -EWOULDBLOCK;

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
			mutex_unlock(&pti->lock);
			return -EINVAL;
		}
	} else {
		channel = stfe_channel_allocate(stfe);
	}

	if (!channel) {
		mutex_unlock(&pti->lock);
		return -EBUSY;
	}

	dvbdmxfeed->priv    = channel;
	channel->dvbdmxfeed = dvbdmxfeed;
	channel->pid        = dvbdmxfeed->pid;
	channel->type       = 1;
	channel->active     = 1;
	channel->stream     = stdemux->stream_channel;
	channel->id         = info->NumPids;

	dprintk("Setting pid %d\n",channel->pid);

	if (channel->pid == 8192 || channel->pid == -1) {
		dprintk("Dumping all channels\n");
		PtiWrite(&info->RawMode,info->RawMode+1);
	}

	PtiWrite(&info->PidTable[info->NumPids],channel->pid);
	PtiWrite(&info->NumPids,info->NumPids + 1);

	if (pti->running_feed_count==0)
	{
		pti->timer.expires = 1 + jiffies;
/*
		if (stdemux->tsm_channel >= 0) {
			stm_tsm_reset_channel ( pti->tsm, stdemux->tsm_channel,-1);//stfe->tsm_channel , -1 );
		}
*/
		stm_pti_start_dma(pti);

		add_timer(&pti->timer);
		dprintk("%s: Starting DMA feed\n",__FUNCTION__);

	}

	if ((stdemux->tsm_channel >= 0) && (channel->id == 0))
		stm_tsm_enable_channel( pti->tsm, stdemux->tsm_channel );

	pti->running_feed_count++;

	stm_pti_plugin_notify(1,stdemux,channel->pid);

	mutex_unlock(&pti->lock);

	return 0;
}

static int stm_pti_stop_feed(struct dvb_demux_feed *dvbdmxfeed)
{
	struct stfe_channel *channel =
		(struct stfe_channel *) dvbdmxfeed->priv;

	struct dvb_demux       *demux   = dvbdmxfeed->demux;
	struct stdemux         *stdemux = container_of( demux, struct stdemux, dvb_demux );
	struct stfe            *stfe    = container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti         *pti     = (struct stm_pti*)stfe->driver_data;
	struct pti_stream_info *info    = &pti->stream[stdemux->stream_channel];
	struct dvb_demux_feed  *feed;

	int n;

	if (mutex_lock_interruptible(&pti->lock))
		return -EWOULDBLOCK;

	if (info->PidTable[channel->id] == 8192 ||
		info->PidTable[channel->id] == -1)
		PtiWrite(&info->RawMode,info->RawMode-1);

	/* Now reallocate the pids, and update id information */
	for (n=channel->id;n<(info->NumPids-1);n++) 
	{
	  /* We have to remember the slot information as well as the 
	     pid table, so we need to be careful about race conditions */

	  /* So invalidate this PID Table whilst we update the slot info. */
	  PtiWrite(&info->PidTable[n], 0x1fff);
	  PtiWrite(&info->Slots[n], info->Slots[n+1]);

	  /* Now we make the next one look like this one */
	  PtiWrite(&info->PidTable[n], info->PidTable[n+1]);
	}

	/* Update PIDs positions in struct stfe_channel */
	list_for_each_entry(feed, &demux->feed_list, list_head) {
	  struct stfe_channel *channel_right = (struct stfe_channel *)feed->priv;

	  /* feed->index--; */
	  if (channel_right->id > channel->id)
	    channel_right->id--;
	}

	/* Now update the number of pids */
	PtiWrite(&info->NumPids , info->NumPids - 1);

	if ((stdemux->tsm_channel >= 0) && (info->NumPids == 0))
		stm_tsm_disable_channel( pti->tsm, stdemux->tsm_channel );

	if (--pti->running_feed_count == 0)
	{
		stm_tsm_disable_channel( pti->tsm, stdemux->tsm_channel );
		del_timer(&pti->timer);
		stm_pti_stop_dma(pti);

		dprintk("%s: Stopping DMA feed\n",__FUNCTION__);
	}

	channel->active = 0;

	stm_pti_plugin_notify(0,stdemux,channel->pid);
	
	mutex_unlock(&pti->lock);

	return 0;
}

static void stm_pti_flip_channel_output_polarity( struct stm_pti          *pti, int n  )
{
    pti->channel_polarity[n]    = 1 - pti->channel_polarity[n];
}

/********************************************************/

struct stm_pti *softpti = NULL;

struct stdemux *stfe_get_handle(int adapter, unsigned int channelid)
{
	if (softpti == NULL)
		return NULL;

	if (channelid > 3)
		return NULL;

	return  &softpti->stfe->demux[channelid];
}
EXPORT_SYMBOL(stfe_get_handle);

int stfe_write_kernel (struct stdemux *stdemux, const char * Buffer, size_t Count, loff_t* ppos)
{
	struct stfe                *stfe    = container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti             *pti     = (struct stm_pti*)stfe->driver_data;

#ifdef USE_PROFILING
    if (_pkt_idx == 1) {
        _pkt_idx--;
        printk("PTI: PROFILING: IN: %u\n", jiffies_to_msecs(jiffies));
    }
#endif

	return stm_tsm_inject_kernel_data( pti->tsm, pti->id, stdemux->stream_channel, (char*)Buffer, Count );
}
EXPORT_SYMBOL(stfe_write_kernel);

int stfe_write_scatterlist (struct stdemux *stdemux, struct scatterlist *sg, int size) 
{
	struct stfe                *stfe    = container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti             *pti     = (struct stm_pti*)stfe->driver_data;

  return stm_tsm_inject_scatterlist(pti->tsm, pti->id, stdemux->stream_channel, sg, size);
}
EXPORT_SYMBOL(stfe_write_scatterlist);

int stfe_write_user (struct stdemux *stdemux, const char * Buffer, size_t Count, loff_t* ppos)
{
	struct stfe	*stfe	= container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti	*pti	= (struct stm_pti*)stfe->driver_data;

return stm_tsm_inject_user_data( pti->tsm, pti->id, stdemux->stream_channel, (char *)Buffer, Count );
}
EXPORT_SYMBOL(stfe_write_user);

static ssize_t stfe_write (struct file *File, const char __user* Buffer, size_t Count, loff_t* ppos)
{
	struct dvb_device*          DvbDevice       = (struct dvb_device*)File->private_data;
	struct dmxdev*              DmxDevice       = (struct dmxdev*)DvbDevice->priv;
	struct dvb_demux*           DvbDemux        = (struct dvb_demux*)DmxDevice->demux->priv;

	struct stdemux             *stdemux = container_of( DvbDemux, struct stdemux, dvb_demux );
	struct stfe                *stfe    = container_of( stdemux, struct stfe, demux[stdemux->channel] );
	struct stm_pti             *pti     = (struct stm_pti*)stfe->driver_data;

	return stm_tsm_inject_user_data( pti->tsm, pti->id, stdemux->stream_channel, Buffer, Count ) ? -EINVAL : Count;
}

// ------------------------------------------------------------------------------------
// Nicks insertion of ioctl code to allow extension to support a dvr flush

static int stfe_ioctl_flush_channel( struct stdemux *stdemux )
{
struct stfe             *stfe           = container_of( stdemux, struct stfe, demux[stdemux->channel] );
struct stm_pti          *pti            = (struct stm_pti*)stfe->driver_data;

    stm_pti_flip_channel_output_polarity( pti, stdemux->stream_channel );
    stm_tsm_flip_channel_input_polarity( pti->tsm, stdemux->stream_channel );
    return 0;
}


static int stfe_ioctl(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
int                      Status         = 0;
struct dvb_device       *DvbDevice      = (struct dvb_device*)file->private_data;
struct dmxdev           *DmxDevice      = (struct dmxdev*)DvbDevice->priv;
struct dvb_demux        *DvbDemux       = (struct dvb_demux*)DmxDevice->demux->priv;

struct stdemux          *stdemux        = container_of( DvbDemux, struct stdemux, dvb_demux );
struct stfe             *stfe           = container_of( stdemux, struct stfe, demux[stdemux->channel] );
struct stm_pti          *pti            = (struct stm_pti*)stfe->driver_data;

    if( mutex_lock_interruptible(&DmxDevice->mutex) )
	return -ERESTARTSYS;

    switch( cmd )
    {
	case DMX_FLUSH_CHANNEL:
		Status  = stfe_ioctl_flush_channel( stdemux );
		break;

	default:
		mutex_unlock( &DmxDevice->mutex );
		return pti->original_dvr_fops.ioctl( inode, file, cmd, arg );
    }

    mutex_unlock( &DmxDevice->mutex );
    return Status;
}

// End of Nicks insertion of ioctl code
// ------------------------------------------------------------------------------------

static int stm_pti_probe(struct platform_device *stm_pti_device_data)
{
	struct stm_pti *pti;
	struct plat_frontend_config *config;
	int    n,i;
	unsigned long start;

	/* Get the platform device information (if it exists) */
	if (!stm_pti_device_data || !stm_pti_device_data->name) 
	{
		printk(KERN_ERR "%s: Device probe failed.  Check your kernel SoC config!!\n",__FUNCTION__);    
		return -ENODEV;
	}

	/* Allocate the stm_pti structure */
	pti = (struct stm_pti*)kzalloc(sizeof(struct stm_pti),GFP_KERNEL);
	if (!pti)
		return -ENOMEM;

	/* Get the configuration information about the tuners */
	config = (struct plat_frontend_config*)stm_pti_device_data->dev.platform_data;

	/* Ok now determine if we are in pure SWTS mode or full mode */
	if (config->nr_channels == 0)
		pti->size_of_header = 0;
	else
		pti->size_of_header = 6;

	/* Allocate the back buffer */
	pti->back_buffer = (char*)bigphysarea_alloc_pages((PTI_BUFFER_SIZE+PAGE_SIZE-1) / PAGE_SIZE,0,GFP_KERNEL);
	if (!pti->back_buffer)
		goto err1;

	/* ioremap the pti address space */
	start = platform_get_resource(stm_pti_device_data,IORESOURCE_MEM,0)->start;
	pti->pti_io = (unsigned long)ioremap( (unsigned long)start, 
					      platform_get_resource(stm_pti_device_data,IORESOURCE_MEM,0)->end - start);
	if (!pti->pti_io)
		goto err2;

	mutex_init(&pti->lock);
	mutex_init(&pti->write_lock);

	/* Now get the id of this pti as we can have two 1 or 0 */
	pti->id = stm_pti_device_data->id;

	/* Resolve pointers we need for later */
	pti->psize              = getsymbol(pti, "psize");
	pti->pread              = getsymbol(pti, "pread");
	pti->pwrite             = getsymbol(pti, "pwrite");
	pti->packet_size        = getsymbol(pti, "packet_size");
	pti->current_stream     = getsymbol(pti, "current_stream");
	pti->discard            = getsymbol(pti, "discard");
	pti->header_size        = getsymbol(pti, "header_size");
	pti->count              = getsymbol(pti, "Count");

	pti->stream             = getsymbol(pti, "stream");
	pti->buffer             = getsymbol(pti, "buffer");
	pti->slots              = getsymbol(pti, "slots");
	pti->multi2_system_key  = getsymbol(pti, "multi2_system_key");

	/* Setup PTI */
	writel(0x1, pti->pti_io + PTI_DMA_PTI3_PROG);

	/* Write instructions and data */
	loadtc(pti);

	PtiWrite(pti->packet_size,PACKET_SIZE);
	PtiWrite(pti->header_size,pti->size_of_header);
        PtiWrite(pti->count, 0);

	/* Enable IIF */
	writel(0x0,         pti->pti_io + PTI_IIF_SYNC_LOCK);
	writel(0x0,         pti->pti_io + PTI_IIF_SYNC_DROP);
	writel(PACKET_SIZE, pti->pti_io + PTI_IIF_SYNC_PERIOD);
	writel(0x0,         pti->pti_io + PTI_IIF_SYNC_CONFIG);
	writel(0x1,         pti->pti_io + PTI_IIF_FIFO_ENABLE);

	/* Setup packet count */
	PtiWrite(pti->psize, (PTI_BUFFER_SIZE / (PACKET_SIZE)) - 2);
	PtiWrite(pti->pwrite, (PTI_BUFFER_SIZE / (PACKET_SIZE)));
	//  PtiWrite(pti->num_pids,0);

	/* Start the TC */
	writel(PTI_MODE_ENABLE, pti->pti_io + PTI_MODE );

	/* Reset the DMA */
	stm_pti_setup_dma(pti);
	stm_pti_reset_dma(pti);

	/* Enable the DMA of data */
	writel( readl(pti->pti_io + PTI_DMA_ENABLE) | 0x1, pti->pti_io + PTI_DMA_ENABLE );  

	/* Setup the transport stream merger based on the configuration */
	pti->tsm = stm_tsm_init( pti->id, config );

	if (!pti->tsm)
		goto err3;

	stm_tuner_register_frontend ( config, &pti->stfe , (void*)pti, stm_pti_start_feed, stm_pti_stop_feed );

	for (n=0;n<config->nr_channels;n++) {
		// Map the demux to the channel, and the tsm channel to the demux ...
		pti->stfe->demux[n].tsm_channel = STM_GET_CHANNEL(config->channels[n].option);
		pti->stfe->demux[n].stream_channel = pti->stfe->demux[n].tsm_channel;
		pti->mapping[pti->stfe->demux[n].tsm_channel] = &pti->stfe->demux[n];

		for (i=0;i<sizeof(stfe_plugins)/sizeof(struct stfe_plugin*);i++)
			if ((stfe_plugins[i] != NULL) && (stfe_plugins[i]->type == STFE_PLUGIN_CA))
				stfe_plugins[i]->ca_attach(&pti->stfe->demux[n], pti->stfe);                    

	}

	if (config->nr_channels == 0 && pti->stfe) {
		/* Ok this overwrites the FOPS for write for dvr0, so we can inject into the PTI */
		softpti = pti;

		for (n=0;n<4;n++) {
			struct file_operations *fops = kmalloc(sizeof(struct file_operations),GFP_KERNEL);
			memcpy(&pti->original_dvr_fops,pti->stfe->demux[n].dmxdev.dvr_dvbdev->fops,sizeof(struct file_operations));
			memcpy(fops,pti->stfe->demux[n].dmxdev.dvr_dvbdev->fops,sizeof(struct file_operations));

			fops->write = stfe_write;
			fops->ioctl = stfe_ioctl;
			pti->stfe->demux[n].dmxdev.dvr_dvbdev->fops = fops;

			pti->channel_polarity[n]        = 0;
			pti->stfe->demux[n].tsm_channel = -1;
			pti->stfe->demux[n].stream_channel = 3 - pti->stfe->demux[n].channel;		// Done deliberately to make sure the mapping/CA stuff gets exercised 
			pti->mapping[pti->stfe->demux[n].stream_channel] = &pti->stfe->demux[n];

			for (i=0;i<sizeof(stfe_plugins)/sizeof(struct stfe_plugin*);i++)
				if ((stfe_plugins[i] != NULL) && (stfe_plugins[i]->type == STFE_PLUGIN_CA))
					stfe_plugins[i]->ca_attach(&pti->stfe->demux[n], pti->stfe);
		}

		for (n=0;n<4;n++) {
			/* Bad code we will lose track of memory */                     

		}

		// Prime the pti in order that the first transfer will go to the right stream

		stm_tsm_inject_kernel_data( pti->tsm, pti->id, 3, NULL, 0 );
	}

	pti->timer.data     = (unsigned long)pti;
	/* Setup timer interrupt */
	spin_lock_init(&pti->timer_lock);
	pti->timer.function = stm_pti_timer_interrupt;
	init_timer(&pti->timer);
	pti->timer.function = stm_pti_timer_interrupt;  

	return 0;

err3:
	iounmap( (void*)pti->pti_io );

err2:
//      kfree(pti->back_buffer);

err1:
	kfree(pti);

	return -ENOMEM;
}

static int stm_pti_remove(struct platform_device *dev)
{
#warning we need to do the deallocation, however in a STB this may not be too much of an issue
	return 0;
}

static struct platform_driver stm_pti_driver = {
       .driver = {
		.name   = "stm-pti",
	       .bus    = &platform_bus_type,
	       .owner  = THIS_MODULE,
       },
       .probe          = stm_pti_probe,
       .remove         = stm_pti_remove
};

static __init int stm_pti_init(void)
{
	return platform_driver_register(&stm_pti_driver);
}

static void stm_pti_cleanup(void)
{
	platform_driver_unregister(&stm_pti_driver);
}

module_init(stm_pti_init);
module_exit(stm_pti_cleanup);

module_param(stm_debug, int,0644);
MODULE_PARM_DESC(stm_debug, "Debug or not");

MODULE_AUTHOR("Peter Bennett <peter.bennett@st.com>");
MODULE_DESCRIPTION("STM PTI DVB Driver");
MODULE_LICENSE("GPL");
