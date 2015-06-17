/******************************************************************************
Copyright (C) 2012, 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : te_work.h

Defines buffer work queue operations
******************************************************************************/

#ifndef __TE_BUFFER_WORK
#define __TE_BUFFER_WORK

#include "te_object.h"
#include "te_hal_obj.h"

struct te_buffer_work_queue;

int te_bwq_create(struct te_buffer_work_queue **bwq,
			struct te_hal_obj *signal,
			struct workqueue_struct *q);

int te_bwq_destroy(struct te_buffer_work_queue *bwq);

typedef void (*TE_BWQ_WORK)(struct te_obj *, struct te_hal_obj *);

int te_bwq_register(struct te_buffer_work_queue *bwq,
			struct te_obj *obj,
			struct te_hal_obj *buffer,
			TE_BWQ_WORK func);

int te_bwq_unregister(struct te_buffer_work_queue *bwq, struct te_hal_obj *buffer);


#endif
