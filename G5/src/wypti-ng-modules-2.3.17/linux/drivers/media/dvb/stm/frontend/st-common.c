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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/device.h>

#include <linux/io.h>

#include <linux/ioport.h>
#include <linux/bpa2.h>

#include "dvb_frontend.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_net.h"

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "st-common.h"
#include "st-diseqc.h"

#include "dvb-pll.h"

/* There are at most 17 PIO */
#define PIO_COUNT	17

int stm_debug = 0;
EXPORT_SYMBOL(stm_debug);

struct stfe_plugin *stfe_plugins[4];
EXPORT_SYMBOL(stfe_plugins);

struct stfe_channel *stfe_channel_allocate(struct stfe *stfe)
{
	int i;

	if (mutex_lock_interruptible(&stfe->lock))
		return NULL;

	/* lock! */
	for (i = 0; i < STFE_MAXCHANNEL; ++i) {
		if (!stfe->channel[i].active) {
			stfe->channel[i].active = 1;
			mutex_unlock(&stfe->lock);
			return stfe->channel + i;
		}
	}

	mutex_unlock(&stfe->lock);

	return NULL;
}
EXPORT_SYMBOL(stfe_channel_allocate);

struct stfe *stfe_create( void *private_data, void *start_feed, void *stop_feed, int num_feeds )
{
	struct stfe *stfe;
	int channel;
	int result;
	short int ids[] = { -1 };
	int i;

	if (!(stfe = kzalloc(sizeof(struct stfe), GFP_KERNEL)))
		return NULL;

	mutex_init(&stfe->lock);

	for (channel = 0; channel < STFE_MAXCHANNEL; ++channel) {
		stfe->channel[channel].id        = channel;
		stfe->channel[channel].stfe      = stfe;
		stfe->channel[channel].havana_id = private_data;
	}
 
	dvb_register_adapter(&stfe->adapter, "ST Generic Front End Driver", THIS_MODULE,NULL,ids);
	stfe->adapter.priv = stfe;

	for (i=0;i<num_feeds;i++) {
		stfe->demux[i].dvb_demux.dmx.capabilities =
			DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
		stfe->demux[i].dvb_demux.priv = private_data;
		stfe->demux[i].channel        = i;
		
		stfe->demux[i].dvb_demux.filternum = STFE_MAXCHANNEL;

		stfe->demux[i].dvb_demux.feednum = STFE_MAXCHANNEL;

		stfe->demux[i].dvb_demux.start_feed       = start_feed;
		stfe->demux[i].dvb_demux.stop_feed        = stop_feed;
		stfe->demux[i].dvb_demux.write_to_decoder = NULL;//write_to_decoder;
		
		if ((result = dvb_dmx_init(&stfe->demux[i].dvb_demux)) < 0) {
			printk("stfe_init: dvb_dmx_init failed (errno = %d)\n",
			       result);
			goto err;
		}

		stfe->demux[i].dmxdev.filternum = stfe->demux[i].dvb_demux.filternum;
		stfe->demux[i].dmxdev.demux = &stfe->demux[i].dvb_demux.dmx;
		stfe->demux[i].dmxdev.capabilities = 0;
 
		if ((result = dvb_dmxdev_init(&stfe->demux[i].dmxdev, &stfe->adapter)) < 0) {
			printk("stfe_init: dvb_dmxdev_init failed (errno = %d)\n",
			       result);
			dvb_dmx_release(&stfe->demux[i].dvb_demux);
			goto err;
		}

		stfe->demux[i].hw_frontend.source = DMX_FRONTEND_0 + i;

		result = stfe->demux[i].dvb_demux.dmx.add_frontend(&stfe->demux[i].dvb_demux.dmx, &stfe->demux[i].hw_frontend);
		if (result < 0)
			return NULL;


		stfe->demux[i].mem_frontend.source = DMX_MEMORY_FE;
		result = stfe->demux[i].dvb_demux.dmx.add_frontend(&stfe->demux[i].dvb_demux.dmx, &stfe->demux[i].mem_frontend);
		if (result<0)
            return NULL;

		result = stfe->demux[i].dvb_demux.dmx.connect_frontend(&stfe->demux[i].dvb_demux.dmx, &stfe->demux[i].hw_frontend);
		if (result < 0)
			return NULL;
	}

	stfe->driver_data = private_data;

	return stfe;

err:
	return NULL;	


#if 0
	if (!(stfe = kmalloc(sizeof(struct stfe), GFP_KERNEL)))
		return NULL;

	dprintk("%s: Allocated stfe @ 0x%p\n",__FUNCTION__,stfe);
	memset(stfe, 0, sizeof(struct stfe));

	sema_init(&stfe->sem, 0);
	up(&stfe->sem);

	for (channel = 0; channel < STFE_MAXCHANNEL; ++channel) {
		stfe->channel[channel].id        = channel;
		stfe->channel[channel].stfe      = stfe;
		stfe->channel[channel].havana_id = private_data;
	}

	dvb_register_adapter(&stfe->adapter, "ST Generic Front End Driver", THIS_MODULE,NULL,ids);
    
	stfe->adapter.priv = stfe;

	memset(&stfe->dvb_demux, 0, sizeof(stfe->dvb_demux));
 
	stfe->dvb_demux.dmx.capabilities =
		DMX_TS_FILTERING | DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
	stfe->dvb_demux.priv = private_data;

	stfe->dvb_demux.filternum = 32;

	stfe->dvb_demux.feednum = STFE_MAXCHANNEL;

	stfe->dvb_demux.start_feed       = start_feed;
	stfe->dvb_demux.stop_feed        = stop_feed;
	stfe->dvb_demux.write_to_decoder = NULL;//write_to_decoder;

	if ((result = dvb_dmx_init(&stfe->dvb_demux)) < 0) {
		printk("stfe_init: dvb_dmx_init failed (errno = %d)\n",
		       result);
		goto err;
	}

	stfe->dmxdev.filternum = stfe->dvb_demux.filternum;
	stfe->dmxdev.demux = &stfe->dvb_demux.dmx;
	stfe->dmxdev.capabilities = 0;
 
	if ((result = dvb_dmxdev_init(&stfe->dmxdev, &stfe->adapter)) < 0) {
		printk("stfe_init: dvb_dmxdev_init failed (errno = %d)\n",
		       result);
		dvb_dmx_release(&stfe->dvb_demux);
		goto err;
	}

	for (n=0;n<num_feeds;n++) {
		stfe->hw_frontend[n].source = DMX_FRONTEND_0 + n;

		result = stfe->dvb_demux.dmx.add_frontend(&stfe->dvb_demux.dmx, &stfe->hw_frontend[n]);
		if (result < 0)
			return NULL;
	}

	stfe->mem_frontend.source = DMX_MEMORY_FE;
	result = stfe->dvb_demux.dmx.add_frontend(&stfe->dvb_demux.dmx, &stfe->mem_frontend);
	if (result<0)
		return NULL;

	for (n=0;n<num_feeds;n++) {

		result = stfe->dvb_demux.dmx.connect_frontend(&stfe->dvb_demux.dmx, &stfe->hw_frontend[n]);
		if (result < 0)
			return NULL;

	}

	stfe->driver_data = private_data;
	//i2c_add_driver(&st_tuner_i2c_driver);

	return stfe;

err:
	return NULL;

#endif
}

struct stm_i2c_adap {
	struct list_head   list;
	struct i2c_adapter *adap;
};

static LIST_HEAD(i2c_adapter_list);
static DEFINE_SPINLOCK(i2c_adapter_list_lock);

static int stm_tuner_i2c_attach(struct i2c_adapter *adap)
{
	struct stm_i2c_adap *i2c_adap;

	if (!(i2c_adap = kzalloc(sizeof(*i2c_adap), GFP_KERNEL)))
		return -ENOMEM;

	i2c_adap->adap = adap;

	//dprintk(KERN_INFO "%s: %s = id:%d nr:%d\n",__FUNCTION__,adap->name,adap->id,adap->nr);

	spin_lock(&i2c_adapter_list_lock);
	list_add_tail(&i2c_adap->list, &i2c_adapter_list);
	spin_unlock(&i2c_adapter_list_lock);

	return 0;
}

static struct i2c_adapter *stm_i2c_adapter_get_by_index(unsigned index)
{
	struct stm_i2c_adap *adapter;

	spin_lock(&i2c_adapter_list_lock);

	list_for_each_entry(adapter, &i2c_adapter_list, list) {
		if (adapter->adap->nr == index) {
			spin_unlock(&i2c_adapter_list_lock);
			return adapter->adap;
		}
	}

	spin_unlock(&i2c_adapter_list_lock);
	return NULL;
}

static struct i2c_adapter *stm_i2c_adapter_get_by_name(const char *name)
{
	struct stm_i2c_adap *adapter;

	spin_lock(&i2c_adapter_list_lock);

	list_for_each_entry(adapter, &i2c_adapter_list, list)
		if (adapter->adap->name && !strcmp(adapter->adap->name,name)) {
			spin_unlock(&i2c_adapter_list_lock);
			return adapter->adap;
		}

	spin_unlock(&i2c_adapter_list_lock);
	return NULL;
}

static int stm_tuner_i2c_detach(struct i2c_adapter *adap)
{
	/*
	  struct i2c_adapter *adapter = stm_i2c_adapter_get_by_index(adap->nr);

	  if (!adapter)
	  return 0;

	  spin_lock(&i2c_adapter_list_lock);
	  list_del(adapter->list);
	  spin_unlock(&i2c_adapter_list_lock);

	  free(adapter);
	*/
	return 0;
}


struct i2c_driver stm_tuner_i2c_driver = {
    //    .id = 129,
	.attach_adapter = stm_tuner_i2c_attach,
	.detach_adapter = stm_tuner_i2c_detach,
	.command        = 0,
	.driver         = {
		.owner          = THIS_MODULE,
		.name           = "i2c_stm_frontend"
	}
};

static int scanned_i2c = 0;

int stm_tuner_register_frontend ( struct plat_frontend_config *config, 
                                  struct stfe ** stfe,
                                  void         * private_data,
                                  void         * start_feed,
                                  void         * stop_feed )

{
	int m=0;
	int n;

	for (n=0;n<config->nr_channels;n++)
		if ((config->channels[n].pio_reset_bank < PIO_COUNT) && 
		    !((config->channels[n].pio_reset_bank == 0x00) && (config->channels[n].pio_reset_pin == 0x00)))
		{  
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) )    // STLinux 2.3
			struct stpio_pin *reset = stpio_request_pin( config->channels[n].pio_reset_bank,
                                                         config->channels[n].pio_reset_pin,
                                                         "Nim Reset", 
                                                         STPIO_OUT);

			if (reset != NULL) {
				if (config->channels[n].pio_reset_n){
					stpio_set_pin(reset,1);
					udelay(10000);
					stpio_set_pin(reset,0);
					udelay(10000);
					stpio_set_pin(reset,1);
                } else {
					stpio_set_pin(reset,0);
					udelay(10000);
					stpio_set_pin(reset,1);
					udelay(10000);
					stpio_set_pin(reset,0);
                }
			} else {
				printk(KERN_WARNING "STM Frontend, couldn't get pin for reset\n");
			}
#else
#define NIM_RESET_PIN	stm_gpio(config->channels[n].pio_reset_bank, config->channels[n].pio_reset_pin)
			gpio_request(NIM_RESET_PIN, "Nim Reset");
			gpio_direction_output(NIM_RESET_PIN, 0);
			udelay(10000);
			gpio_direction_output(NIM_RESET_PIN, 1);
#endif
        }

    if (!scanned_i2c) {
        i2c_add_driver(&stm_tuner_i2c_driver);
        scanned_i2c = 1;
    }	


    /* If the config specifies no channels, then we assume they just
       want to use the SWTS */
    if (config->nr_channels == 0) {
        *stfe = stfe_create( private_data, start_feed, stop_feed, 4 );

        if (!stfe[0]) {
            printk("%s: Couldn't create SWTS devices, something went wrong\n",__FUNCTION__);
            return -ENODEV;
        }
//		*stfe->mapping = 0;

        return 0;
    }

    /* else create the frontend devices (even if we didn't detect them!!!) */

    *stfe = stfe_create( private_data, start_feed, stop_feed, config->nr_channels );

    for (n=0;n<config->nr_channels;n++) {
        struct plat_frontend_channel *channel  = &config->channels[n];
        struct i2c_adapter           *adap     = NULL;
        struct dvb_frontend          *frontend = NULL;

        /* Locate the demodulator i2c adapter */
        if (channel->demod_i2c_bus_name)
            adap = stm_i2c_adapter_get_by_name(channel->demod_i2c_bus_name);
        else if (channel->demod_i2c_bus != STM_NONE)
            adap = stm_i2c_adapter_get_by_index(channel->demod_i2c_bus);

        /* If we find it go on and try and attach it */
        if (adap && channel->demod_attach) {
            frontend = channel->demod_attach(channel->config,adap);

            /* Now look for a tuner */
            adap = NULL;

            if (channel->tuner_i2c_bus_name)
                adap = stm_i2c_adapter_get_by_name(channel->tuner_i2c_bus_name);
            else if (channel->tuner_i2c_bus != STM_NONE)
                adap = stm_i2c_adapter_get_by_index(channel->tuner_i2c_bus);

            if (channel->tuner_attach && frontend && adap)
                frontend = channel->tuner_attach( channel->config, frontend, adap );

            /* Now look for an lnb */
            adap = NULL;
    
            if (channel->lnb_i2c_bus_name)
                adap = stm_i2c_adapter_get_by_name(channel->lnb_i2c_bus_name);
            else if (channel->lnb_i2c_bus != STM_NONE)
                adap = stm_i2c_adapter_get_by_index(channel->lnb_i2c_bus);

            if (channel->lnb_attach && frontend && adap)
                frontend = channel->lnb_attach( channel->config, frontend, adap);
	      
        }

        /* Let's see if we should register it */
        if (frontend || channel->demod_i2c_bus == STM_NONE) {
            //stfe[m] = stfe_create( private_data, start_feed, stop_feed, 1 );
			
            if (!*stfe) {
                printk("%s: Couldn't create frontend devices, something went wrong\n",__FUNCTION__);
                continue;
            }

            //*stfe->mapping = n;
      
            if (frontend) dvb_register_frontend( &stfe[0]->adapter, frontend);
            if (frontend && channel->option & STM_DISEQC_STONCHIP)
                stm_diseqc_register_frontend(frontend);
	    
            m++;
        }

    }

    return 0;
}
EXPORT_SYMBOL(stm_tuner_register_frontend);

int stm_register_plugin(struct stfe_plugin *plugin)
{
    int n;

    for (n=0;n<sizeof(stfe_plugins)/sizeof(struct stfe_plugin*);n++)
        if (!stfe_plugins[n]) {				
            stfe_plugins[n] = plugin;
            return 0;
        }
		
    return -ENOMEM;
}
EXPORT_SYMBOL(stm_register_plugin);

void stm_unregister_plugin(struct stfe_plugin *plugin)
{

}
EXPORT_SYMBOL(stm_unregister_plugin);

MODULE_AUTHOR("Peter Bennett <peter.bennett@st.com>");
MODULE_DESCRIPTION("STM Common DVB Driver");
MODULE_LICENSE("GPL");
