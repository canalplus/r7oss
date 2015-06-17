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

Source file name : dvb_adapt_demux.h

Header for STLinuxTv demux code

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DVB_DEMUX_H
#define _DVB_DEMUX_H

#include "dvb_adaptation.h"

int stm_dvb_demux_delete(struct stm_dvb_demux_s *stm_demux);
int stm_dvb_demux_attach(struct dvb_adapter *adpat,
			 struct stm_dvb_demux_s **stm_demux,
			 int filters_per_demux, int feeds_per_demux);

#endif /* _DVB_DEMUX_H */
