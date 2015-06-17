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

Source file name : tsclient.h
Author :           Shobhit

Header for Remote PCPD LLA

Date        Modification                                    Name
----        ------------                                    --------
21-Jan-13   Created                                         Shobhit

************************************************************************/
#ifndef _TSSERVER_H
#define _TSSERVER_H

#define MAX_MSG_SIZE			256
#define MAX_CMD_PARAM			10
#define MAX_STRING_SIZE			60

void store_data_le32(u_int8_t *msg, u_int32_t value);
unsigned int load_data_le32(u_int8_t *msg);

int transport_ts_setup(struct socket **sockt, struct stm_fe_demod_s *priv);
int stm_fe_tsserver_attach(struct stm_fe_demod_s *priv);
int tsclient_init_ts(struct stm_fe_demod_s *priv, char *demux_name);
int tsclient_pid_start(stm_object_h demod_object, stm_object_h demux_object,
		       uint32_t pid);
int tsclient_pid_stop(stm_object_h demod_object, stm_object_h demux_object,
		       uint32_t pid);
int tsclient_term_ts(struct stm_fe_demod_s *priv, char *demux_name);
int tsserver_init(struct stm_fe_demod_s *priv);
int tsserver_tune(struct stm_fe_demod_s *priv, bool *lock);
int tsserver_unlock(struct stm_fe_demod_s *priv);
int tsserver_abort(struct stm_fe_demod_s *priv, bool lock);
int tsserver_status(struct stm_fe_demod_s *priv, bool *lock);
int tsserver_tracking(struct stm_fe_demod_s *priv);
void parse_command(char **str, int num, int *list);

#endif /* _TSSERVER_H */
