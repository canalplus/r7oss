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

Source file name : te_tsg_sec_filter.h

Defines non-program section insertion filter types
******************************************************************************/

#ifndef __TE_TSG_SEC_FILTER_H
#define __TE_TSG_SEC_FILTER_H

#include <linux/completion.h>

struct te_tsg_sec_filter {
	struct te_tsg_filter tsg;
	uint8_t *table;
	size_t table_len;
	uint32_t rep_ivl; /* milliseconds */

	bool submitted;
	struct completion done;
	/* strelay index */
	unsigned int push_sec_relayfs_index;
};

int te_tsg_sec_filter_new(struct te_obj *tsmux, struct te_obj **new_filter);

#endif
