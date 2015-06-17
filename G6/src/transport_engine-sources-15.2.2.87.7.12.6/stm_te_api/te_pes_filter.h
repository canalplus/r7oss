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

Source file name : te_pes_filter.h

Defines PES filter specific operations
******************************************************************************/

#ifndef __TE_PES_FILTER_H
#define __TE_PES_FILTER_H

#include "te_object.h"
#include "te_output_filter.h"

int te_pes_filter_new(struct te_obj *demux, struct te_obj **new_filter);
uint8_t te_pes_filter_get_stream_id(struct te_obj *filter);

#endif

