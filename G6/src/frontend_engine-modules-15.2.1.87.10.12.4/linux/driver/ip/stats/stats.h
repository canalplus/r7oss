/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stats.h
Author :           SD

fe ip statistics reporting via sysfs

Date        Modification                                    Name
----        ------------                                    --------
05-Jun-12   Created                                         SD

************************************************************************/

#ifndef _STATS_H
#define _STATS_H

int stm_ipfe_stats_init(struct stm_fe_ip_s *ipfe);
int stm_ipfe_stats_term(struct stm_fe_ip_s *ipfe);

#endif /*_STATS_H*/
