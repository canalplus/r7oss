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

Source file name :diseqc_stv0900.h
Author :          Ankur

Low level function installation for diseqc

Date        Modification                                    Name
----        ------------                                    --------
21-May-12   Created                                         Ankur

************************************************************************/
#ifndef _DISEQC_STV0900_H
#define _DISEQC_STV0900_H

int stm_fe_diseqc_stv0900_attach(struct stm_fe_diseqc_s *priv);
int stm_fe_diseqc_stv0900_detach(struct stm_fe_diseqc_s *priv);

#endif /* _DISEQC_STV0900_H */
