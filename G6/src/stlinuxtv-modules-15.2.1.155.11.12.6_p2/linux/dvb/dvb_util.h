/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_util.c

Some FE related helpers functions

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#define POST_FIX "-STM_FE"
#define ATTACH_STM_FE  (1 << 0)
#define DEVICE_MATCH   (1 << 1)

#define FE_ADAP_NO_OPTIONS_DEFAULT   -1
#define FE_ADAP_FORCE_STM_FE_NATIVE  1
#define FE_ADAP_FORCE_LDVB_NATIVE    0
#define STM_FE			0
#define KERNEL_FE		1

extern int ctrl_via_stm_fe;
static inline int set_conf(char *ip_str, char *name, int *config)
{
	int len = strlen(ip_str) - strlen(POST_FIX);
	int ret = 0;

	if (name)
		strncmp(ip_str, name, strlen(name)) ?
		    (ret = 0) : (ret |= DEVICE_MATCH);

	if (ctrl_via_stm_fe == FE_ADAP_FORCE_STM_FE_NATIVE)
		ret |= (ATTACH_STM_FE | DEVICE_MATCH);
	else if (len > 0)
		if ((!strcmp((ip_str + len), POST_FIX)) &&
		    (ctrl_via_stm_fe != FE_ADAP_FORCE_LDVB_NATIVE))
			ret |= (ATTACH_STM_FE | DEVICE_MATCH);

	*config |= ret;
	return ret;
}
