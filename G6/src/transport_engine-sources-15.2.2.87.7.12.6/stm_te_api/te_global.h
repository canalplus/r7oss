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

Source file name : te_global.h

Declares the te global data structure and functions that operate on it
******************************************************************************/
#ifndef __TE_GLOBAL_H
#define __TE_GLOBAL_H

#include <stm_te_dbg.h>
#include <te_object.h>
#include <linux/list.h>
#include <linux/mutex.h>

struct te_global {
	struct te_obj_class demux_class;
	struct te_obj_class tsmux_class;
	struct te_obj_class in_filter_class;
	struct te_obj_class out_filter_class;
	struct te_obj_class tsg_filter_class;
	struct te_obj_class tsg_index_filter_class;
	struct list_head demuxes;
	struct list_head tsmuxes;
	struct mutex lock;

	/* TODO: These lists of filters should not be in the global data - they
	 * should be managed at the demux level */
	struct list_head index_filters;
	struct list_head shared_sinks;
};

extern struct te_global te_global;

int te_global_init(void);
int te_global_term(void);

#endif
