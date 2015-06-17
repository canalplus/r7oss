/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_demux.c

Demux attach/detach functions

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#include "dvb_adapt_demux.h"
#include <demux/demux_feeds.h>
#include "dvb_data.h"
#include "dvb_adaptation.h"

static struct dvb_device dvr_device_stm = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1
};

static int demux_count = 0;

int stm_dvb_demux_attach(struct dvb_adapter *stm_dvb_adapter,
			 struct stm_dvb_demux_s **allocated_demux,
			 int filters_per_demux, int feeds_per_demux)
{
	int ret = 0;

	struct dvb_demux *dvbdemux;
	struct stm_dvb_demux_s *stm_demux = NULL;

	stm_demux = kzalloc(sizeof(struct stm_dvb_demux_s), GFP_KERNEL);
	if (!stm_demux) {
		ret = -ENOMEM;
		goto error_alloc;
	}

	stm_demux->frontend = kzalloc(sizeof(struct dmx_frontend), GFP_KERNEL);
	if (!(stm_demux->frontend)) {
		ret = -ENOMEM;
		goto error_frontend;
	}

	dvbdemux = &(stm_demux->dvb_demux);

	dvbdemux->filternum = filters_per_demux;
	dvbdemux->feednum = feeds_per_demux;
	dvbdemux->dmx.capabilities = DMX_TS_FILTERING |
	    DMX_SECTION_FILTERING | DMX_MEMORY_BASED_FILTERING;
	dvbdemux->write_to_decoder = NULL;

	stm_demux->frontend->source = DMX_MEMORY_FE;

	ret = snprintf(stm_demux->dvb_demux_name,
		       MAX_NAME_LEN, "stm_dvb_demux.%u", demux_count);

	stm_demux->demux_id = demux_count++;

	ret = dvb_dmx_init(dvbdemux);
	if (ret < 0) {
		ret = -ENODEV;
		goto error_init;
	}

	stm_demux->dmxdev.demux = &dvbdemux->dmx;
	stm_demux->dmxdev.filternum = dvbdemux->filternum;
	stm_demux->dmxdev.capabilities = 0;

	ret = dvb_dmxdev_init(&stm_demux->dmxdev, stm_dvb_adapter);
	if (!stm_demux->dmxdev.dvbdev) {
		ret = -ENODEV;
		goto error_add;
	}

	stm_dvb_demux_setup_demux(stm_demux, &dvr_device_stm);

	dvb_unregister_device(stm_demux->dmxdev.dvr_dvbdev);

	dvb_register_device(stm_dvb_adapter,
			    &stm_demux->dmxdev.dvr_dvbdev,
			    &dvr_device_stm,
			    &stm_demux->dmxdev, DVB_DEVICE_DVR);

	ret =
	    dvbdemux->dmx.add_frontend(&stm_demux->dvb_demux.dmx,
				       stm_demux->frontend);
	if (ret < 0) {
		printk(KERN_ERR "Unable to add frontend to demux\n");
		goto error;
	}

	ret =
	    dvbdemux->dmx.connect_frontend(&stm_demux->dvb_demux.dmx,
					   stm_demux->frontend);
	if (ret < 0) {
		printk(KERN_ERR "Unable to connect frontend to demux\n");
		goto error;
	}

	stm_demux->pcr_type = DMX_PES_OTHER;

	printk(KERN_INFO "Added device %s\n", stm_demux->dvb_demux_name);

	*allocated_demux = stm_demux;

	return 0;

error:
	dvb_dmxdev_release(&stm_demux->dmxdev);
error_add:
	dvb_dmx_release(&stm_demux->dvb_demux);
error_init:
	kfree(stm_demux->frontend);
error_frontend:
	kfree(stm_demux);
error_alloc:
	*allocated_demux = NULL;
	return ret;
}

int stm_dvb_demux_delete(struct stm_dvb_demux_s *stm_demux)
{
	struct dvb_demux *dvb_demux;
	struct dmxdev    *dmxdev;

	/* For each of the stm_dvb_demux_s objects we need to go through
	 * the lits of the filters, buffers and outputs, detach and free them
	 * then delete the object */

	dvb_demux = &stm_demux->dvb_demux;
	dmxdev    = &stm_demux->dmxdev;

	printk(KERN_DEBUG "In demux_delete\n");

	/* Remove frontend from the demux device */
	dvb_demux->dmx.disconnect_frontend(&dvb_demux->dmx);
	dvb_demux->dmx.remove_frontend(&dvb_demux->dmx, stm_demux->frontend);

#if 0
	/* Unregister the DVR DVB device. A call to
 	 * dvb_dmxdev_release does that. */
	dvb_unregister_device(dmxdev->dvr_dvbdev);
#endif

	stm_dvb_demux_remove_demux(stm_demux);
	dvb_dmxdev_release(dmxdev);
	dvb_dmx_release(&stm_demux->dvb_demux);
	kfree(stm_demux->frontend);
	kfree(stm_demux);

	printk(KERN_DEBUG "All the obejcts freed\n");

	return 0;
}
