/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : demod_scan.h
Author :           Shobhit

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _DEMOD_SCAN_H
#define _DEMOD_SCAN_H

void calc_nxt_freq(struct stm_fe_demod_s *priv, bool lock,
		stm_fe_demod_event_t *evt);

void new_scan_setup(struct stm_fe_demod_s *priv);
uint32_t step_freq(struct stm_fe_demod_s *priv);
int evt_notify(struct stm_fe_demod_s *priv, stm_fe_demod_event_t *fe_event);

#endif /* _DEMOD_SCAN_H */
