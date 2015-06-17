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

Source file name : dvb_diseqc.h

Diseqc attach / detach functions

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef _DVB_DISEQC_H
#define _DVB_DISEQC_H

#include "dvb_adaptation.h"

void *diseqc_attach(struct stm_dvb_diseqc_s *dvb_diseqc);
int diseqc_detach(struct stm_dvb_diseqc_s *dvb_diseqc);

#endif /* _DVB_DISEQC_H */
