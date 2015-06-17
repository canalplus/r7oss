/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_ts_index_filter.h

Defines ts index filter specific operations
******************************************************************************/
#ifndef __TE_TS_INDEX_FILTER_H
#define __TE_TS_INDEX_FILTER_H

#include "te_object.h"
#include "te_time.h"

int te_index_sink_testfordata(struct te_obj *sink, uint32_t *bytes);
int attach_pid_to_index(struct te_obj *pid_filter_p, struct te_obj
		*index_filter_p);

struct te_time_points *te_ts_index_get_times(struct te_obj *filter);

int te_ts_index_filter_new(struct te_obj *demux, struct te_obj **new_filter);
#endif
