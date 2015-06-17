/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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

Source file name : d0mxl214.h
Author :           Manu

Header for mxl214 LLA wrapper

Date        Modification                                    Name
----        ------------                                    --------
1-July-14   Created                                           MS

************************************************************************/
#ifndef _D0MXL214_H
#define _D0MXL214_H


int mxl214_init(struct stm_fe_demod_s *priv);
int mxl214_tune(struct stm_fe_demod_s *priv, bool *lock);
int mxl214_scan(struct stm_fe_demod_s *priv, bool *lock);
int mxl214_status(struct stm_fe_demod_s *priv, bool *locked);
int mxl214_tracking(struct stm_fe_demod_s *priv);
int mxl214_unlock(struct stm_fe_demod_s *priv);
int stm_fe_mxl214_attach(struct stm_fe_demod_s *demod_priv);
int stm_fe_mxl214_detach(struct stm_fe_demod_s *demod_priv);
int mxl214_term(struct stm_fe_demod_s *demod_priv);
int mxl214_abort(struct stm_fe_demod_s *priv, bool abort);
int mxl214_standby(struct stm_fe_demod_s *priv, bool standby);
int mxl214_restore(struct stm_fe_demod_s *priv);





#endif /* _D0MXL214_H */
