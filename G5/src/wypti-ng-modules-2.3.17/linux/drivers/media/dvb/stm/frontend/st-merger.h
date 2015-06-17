/*
 * STM TSMerger driver
 *
 * Copyright (c) STMicroelectronics 2006
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 */

#ifndef _ST_MERGER_H
#define _ST_MERGER_H

#include <linux/stm/stm-frontend.h>

struct stm_tsm_handle *stm_tsm_init ( int pti_id, 
				      struct plat_frontend_config *config );

void stm_tsm_poll ( struct stm_tsm_handle* );

void stm_tsm_reset_channel ( struct stm_tsm_handle*, int n, int status );

int  stm_tsm_inject_user_data( struct stm_tsm_handle *handle, 
			       int pti_id,
			       int stream_id,
			       const char __user *data, 
			       off_t size);

int  stm_tsm_inject_kernel_data( struct stm_tsm_handle *handle, 
				 int pti_id,
				 int stream_id,
				 char *data, 
				 off_t size );

int  stm_tsm_inject_scatterlist( struct stm_tsm_handle *handle,
			         int pti_id,
			         int stream_id,
			         struct scatterlist *sg,
			         int size);

void  stm_tsm_enable_channel( struct stm_tsm_handle *handle, int n);
void  stm_tsm_disable_channel( struct stm_tsm_handle *handle, int n);
void  stm_tsm_flip_channel_input_polarity( struct stm_tsm_handle *handle, int n);
#endif
