/************************************************************************
 * Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

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

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stat.h

Common part to register the statistics class

Date        Modification                                    Name
----        ------------                                    --------

************************************************************************/

#ifndef __STAT_H__
#define __STAT_H__

#include <linux/device.h>

void stlinuxtv_stat_init_device(struct device *Dev);
int stlinuxtv_stat_register_class(void);
void stlinuxtv_stat_unregister_class(void);

#endif
