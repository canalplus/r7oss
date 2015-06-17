/*
 * ST DVB driver
 *
 * Copyright (c) STMicroelectronics 2005
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 */
#ifndef _ST_COMMON_H
#define _ST_COMMON_H

#include "dvb_frontend.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_net.h"

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include <linux/stm/stm-frontend.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
#include <linux/stm/pio.h>
#else
#include <linux/gpio.h>
#endif

extern int stm_debug ;
#define dprintk(x...) do { if (stm_debug) printk(KERN_WARNING x); } while (0)

/* Maximum number of channels */
#define STFE_MAXADAPTER (4)
#define STFE_MAXCHANNEL 128

#define STFE_PLUGIN_NONE 0
#define STFE_PLUGIN_CA   1
#define STFE_PLUGIN_NET  2

struct stfe;
struct stdemux;

struct stfe_plugin {
	int type;
	
	union {
		struct dvb_device * (*ca_attach)(struct stdemux *, struct stfe *);
	};
	void (*start_feed_notify)(struct stdemux *,unsigned int);
	void (*stop_feed_notify)(struct stdemux *,unsigned int);
};

extern struct stfe_plugin        *stfe_plugins[4];

struct stdemux {
	struct dvb_demux        dvb_demux;
	struct dmxdev           dmxdev;
	struct dmx_frontend     hw_frontend;
	struct dmx_frontend     mem_frontend;
	int                     tsm_channel;
	int                     stream_channel;
	int                     channel;       // You need to know which demux structure your in
};

struct stfe {
	/* Each frontend can have upto four devices (demux|frontend|dvr[0|1|2|3]) */
	int a;

	struct stdemux demux[4];

	int    mapping;
	
	/* our semaphore, for channel allocation/deallocation */
	struct mutex lock;
  
	struct dvb_adapter adapter;
  
	struct stfe_channel {
		void                  *havana_id;  // Havana_id must be the first as havana does not have access to this structure
		struct stfe           *stfe;
		struct dvb_demux_feed *dvbdmxfeed;
    
		int active;
		int stream;
		int id;
		int pid;
		int type;       /* 1 - TS, 2 - Filter */
	} channel[STFE_MAXCHANNEL];
  

	struct dvb_frontend* fe;

	int running_feed_count;
  
	void *tuner_data;
	void *driver_data;
    
	struct device* device;  
    
};

struct stfe_channel *stfe_channel_allocate(struct stfe *stfe);
//struct stfe *stfe_init(struct dvb_adapter *adapter,void *private_data, void *start_feed, void *stop_feed, void *write_to_decoder);

struct stfe *stfe_create( void *private_data, void *start_feed, void *stop_feed, int num_feeds );

/* Channel registration */
int stm_tuner_register_frontend ( struct plat_frontend_config *config, 
				  struct stfe ** stfe,
				  void         * private_data,
				  void         * start_feed,
				  void         * stop_feed );

int  stm_register_plugin  (struct stfe_plugin *plugin);
void stm_unregister_plugin(struct stfe_plugin *plugin);

#endif
