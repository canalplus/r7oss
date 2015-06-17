/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : debugfs.h
Author :           Deepak G

Header file for Debugfs Support for FE

Date        Modification                                    Name
----        ------------                                    --------
23-Nov-13   Created                                         Deepak G

************************************************************************/
#ifndef _DEBUGFS_H
#define _DEBUGFS_H
#ifdef CONFIG_DEBUG_FS
int stmfe_ts_conf_debugfs_create(struct stm_fe_demod_s *priv);
int stmfe_demod_debugfs_create(struct stm_fe_demod_s *priv);
int stmfe_debugfs_create(void);
int  stmfe_debugfs_remove(void);
#else
#define stmfe_ts_conf_debugfs_create(a) NULL
#define stmfe_demod_debugfs_create(a) NULL
#define stmfe_debugfs_create() NULL
#define  stmfe_debugfs_remove() NULL
#endif
#endif /* _DEBUGFS_H */
