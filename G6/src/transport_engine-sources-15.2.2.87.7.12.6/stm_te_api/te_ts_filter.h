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

Source file name : te_ts_filter.h

Defines TS filter specific structures
******************************************************************************/
#ifndef __TE_TS_FILTER_H
#define __TE_TS_FILTER_H

#include "te_object.h"
#include "te_output_filter.h"

int te_ts_filter_new(struct te_obj *demux, struct te_obj **new_filter);
bool te_ts_filter_is_security_formatted(struct te_obj *filter);
bool te_ts_filter_is_dlna_formatted(struct te_obj *filter);
#endif

