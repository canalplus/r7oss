/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : pm.h
Author :           Ankur

Header for pm.c

Date        Modification                                    Name
----        ------------                                    --------
22-Mar-12   Created                                         Ankur

************************************************************************/
#ifndef _PM_H
#define _PM_H

#include <linux/pm_runtime.h>
#include <linux/suspend.h>
int32_t stmfe_power_mode(struct stm_fe_demod_s *priv, bool mode);
int32_t demod_suspend(struct stm_fe_demod_s *priv);
int32_t demod_resume(struct stm_fe_demod_s *priv);
int32_t demod_restore(struct stm_fe_demod_s *priv);
#endif /* _PM_H */
