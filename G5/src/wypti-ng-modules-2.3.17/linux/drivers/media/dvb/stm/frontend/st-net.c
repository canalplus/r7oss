/*
 * STM NET DVB driver
 *
 * Copyright (c) STMicroelectronics 2009
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
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "dvb_frontend.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_net.h"

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "st-common.h"

/* This driver allows ip packets to be routed to the linuxdvb driver.

   It is intended to initially be a generic driver, but later will use
   specific ST hardware to accelerate certain features.

   It will create an ethernet device driver called dvb? where adapter?

   You can use ip tables to route different packets to different ports however:
     50000 = demux0 (UDP RTP)
     50001 = demux1 (UDP RTP)
     50002 = demux2 (UDP RTP)
     50003 = demux3 (UDP RTP)

   For new protocols simply define new ports.
*/

