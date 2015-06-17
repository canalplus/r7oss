/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * hdmirx capture definitions
************************************************************************/
#ifndef __HDMIRX_CAPTURE_H__
#define __HDMIRX_CAPTURE_H__

struct hdmirx_context {
	char viddec_name[32];
	char auddec_name[32];
	char plane_name[32];

	enum v4l2_mbus_pixelcode dvp_mbus_code;

	struct media_device *media;

	struct media_entity *hdmirx_entity, *dvp_entity;
	struct media_entity *viddec_entity, *auddec_entity;
	struct media_entity *plane_entity;
};

void *keyboard_monitor(void *arg);

#endif
