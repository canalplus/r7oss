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

Source file name : dvb_adapt_ca.h

Header for the CA part of STLinuxTV

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DVB_CA_H
#define _DVB_CA_H

#include "dvb_adaptation.h"
#include <demux/demux_filter.h>

int stm_dvb_ca_attach(struct dvb_adapter *adapter,
		      struct stm_dvb_demux_s *demux,
		      struct stm_dvb_ca_s **allocated_ca);
int stm_dvb_ca_delete(struct stm_dvb_ca_s *stm_ca);

int filter_set_ca_channel(struct stm_dvb_filter_s *filter,
			  struct stm_dvb_ca_channel_s *channel);

int stm_dvb_ca_set_descr(struct stm_dvb_ca_s *stm_ca, ca_descr_t * ca_descr);
int stm_dvb_ca_set_pid(struct stm_dvb_ca_s *stm_ca, ca_pid_t * ca_pid);
int stm_dvb_ca_send_msg(struct stm_dvb_ca_s *stm_ca, dvb_ca_msg_t * msg);
int stm_dvb_ca_get_msg(struct stm_dvb_ca_s *stm_ca, dvb_ca_msg_t *msg);
int stm_dvb_ca_reset(struct stm_dvb_ca_s *stm_ca);
int stm_dvb_ca_get_cap(struct stm_dvb_ca_s *stm_ca, ca_caps_t *caps);
int stm_dvb_ca_get_slot_info(struct stm_dvb_ca_s *stm_ca,
		ca_slot_info_t *info);
int stm_dvb_ca_get_descr_info(struct stm_dvb_ca_s *stm_ca,
		ca_descr_info_t *info);
int stm_dvb_ca_set_filter_channel(struct stm_dvb_ca_s *stm_ca, ca_pid_t * ca_pid,
						struct stm_dvb_filter_s *filter);
#endif
