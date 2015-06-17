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

Source file name : demux_feeds.h

Definition of interface functions for Linux DVB, stm_fe and stm_te modules.

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DVB_DEMUX_FEEDS_H
#define _DVB_DEMUX_FEEDS_H

int stm_dvb_demux_setup_demux(struct stm_dvb_demux_s *stm_demux,
			      struct dvb_device *dev);
int stm_dvb_demux_remove_demux(struct stm_dvb_demux_s *stm_demux);

#undef DEBUG_DMX
#ifdef DEBUG_DMX
#define DMX_DBG(msg, args...)		printk(KERN_DEFAULT "%s(): " msg, __FUNCTION__, ## args)
#else
#define DMX_DBG(msg, args...)
#endif
#define DMX_INFO(msg, args...)		printk(KERN_INFO "%s(): " msg, __FUNCTION__, ## args)
#define DMX_ERR(msg, args...)		printk(KERN_ERR "%s(): " msg, __FUNCTION__, ## args)

#endif /* _DVB_DEMUX_FEEDS_H */
