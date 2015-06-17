/******************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

Transport Engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Transport Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : te_interface.h

Declares interfaces exported by transport_engine to other STKPI components
******************************************************************************/

#ifndef __TE_INTERFACE_H
#define __TE_INTERFACE_H

/* Interface defines */
#include <stm_data_interface.h>
#include <stm_te_if_tsinhw.h>
#include <stm_te_if_ce.h>
#include <stm_te_if_tsmux.h>
#include <stm_se.h>

/* Declare interfaces defined in te_interface.c */
extern stm_data_interface_push_sink_t stm_te_data_push_interface;
extern stm_data_interface_pull_src_t stm_te_pull_byte_interface;
extern stm_data_interface_pull_src_t stm_te_pull_pcr_interface;
extern stm_data_interface_pull_src_t stm_te_pull_section_interface;
extern stm_data_interface_pull_src_t stm_te_pull_ts_index_interface;
extern stm_te_pid_interface_t te_in_filter_pid_interface;
extern stm_fe_te_sink_interface_t fe_te_sink_interface;
extern stm_ip_te_sink_interface_t ip_te_sink_interface;
extern stm_te_async_data_interface_sink_t stm_te_tsg_data_interface;

#endif
