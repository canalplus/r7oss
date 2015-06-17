/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stv6111_install.h
Author :			HS

Header for STV6111 installation functions

Date		Modification				Name
----		------------				--------
4 - June - 13	Created					 HS

************************************************************************/
#ifndef _STV6111_INSTALL_H
#define _STV6111_INSTALL_H

int stm_fe_stv6111_attach(struct stm_fe_demod_s *demod_priv);
int stm_fe_stv6111_detach(STCHIP_Handle_t tuner_h);

#endif /* _STV6111_INSTALL_H */
