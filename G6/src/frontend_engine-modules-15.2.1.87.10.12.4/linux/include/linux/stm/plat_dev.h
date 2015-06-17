/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : plat_dev.h
Author :           Rahul.V

Generic and common file for all st platform devices

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _PLAT_DEV_H
#define _PLAT_DEV_H

struct stm_platdev_data {
	/* pointer to run time driver data, allocated by driver code */
	void *driver_data;
	/* Version number of configuration data */
	uint32_t ver;
	/* pointer to configuration data, allocated by setup */
	void *config_data;
};

#endif /* _PLAT_DEV_H */
