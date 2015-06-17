/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV utilities

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

The STLinuxTV utilities may alternatively be licensed under a proprietary
license from ST.

Source file name : common.h

Common function used by both audio & video encoders

************************************************************************/

#include "v4lEncodeTestApp.h"

#ifndef __COMMON_H__
#define __COMMON_H__

typedef struct ControlNode_s {
	unsigned int ControlID;
	unsigned int ControlValue;
	char ControlName[256];
	struct ControlNode_s *NextControl;

} ControlNode_t;

ControlNode_t *InitControlNode(char *name, int id, int value);
void AddControlNode(ControlNode_t **head, ControlNode_t **end,
		    ControlNode_t *new);
int FrameWriter(drv_context_t *Context, unsigned int frame_no,
		unsigned long crc, void *data, unsigned int size);
int alloc_v4l2_buffers(drv_context_t *ContextPt, unsigned int type);
int stream_on(drv_context_t *ContextPt, unsigned int type);

#endif
