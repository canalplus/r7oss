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

Source file name : te_section_filter.h

Defines base section filter
******************************************************************************/

#ifndef __TE_SECTION_FILTER_H
#define __TE_SECTION_FILTER_H

#include "te_object.h"

int attach_pid_to_sf(struct te_obj *pid_filter_p, struct te_obj *sec_filter_p);
void te_section_flush(struct te_obj *filter);
int te_section_filter_new(struct te_obj *demux, struct te_obj **new_filter);

void te_section_filter_disconnect(struct te_obj *out, struct te_obj *in);

#endif

