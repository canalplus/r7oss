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

Source file name : diseqc_caps.h
Author :           Rahul.V

Header for setting the capabilities of diseqc devices

Date        Modification                                    Name
----        ------------                                    --------
04-Jul-11   Created                                         Rahul.V

************************************************************************/
#ifndef _DISEQC_CAPS_H
#define _DISEQC_CAPS_H

stm_fe_diseqc_caps_t diseqc_stv0903_caps = {
	.version = (STM_FE_DISEQC_VER_1_2 | STM_FE_DISEQC_VER_2_0)
};

#endif /* _DISEQC_CAPS_H */
