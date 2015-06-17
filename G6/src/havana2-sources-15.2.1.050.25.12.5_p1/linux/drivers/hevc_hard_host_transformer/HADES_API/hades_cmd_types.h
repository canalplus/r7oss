/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine Library.

Streaming Engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Streaming Engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : hades_cmd_types.h

Description: Types used to send commands to the HADES decoder

Date        Modification                                    Name
----        ------------                                    --------
16-Oct-14   Copied from Hades model                         Swadesh

************************************************************************/
#ifndef _HADES_CMD_TYPES_H_
#define _HADES_CMD_TYPES_H_

/* definition of ping_hades command */

/* ping_hades mme_cmd */
typedef struct {
	uint32_t void_param;
} ping_hades_param_t;

/* ping_hades return */
typedef struct {
	uint8_t hades_model_sha1[64];
	uint8_t hades_model_tag[64];
	uint8_t hevc_dec_ipm_sha1[64];
	uint8_t hevc_dec_ipm_tag[64];
	uint8_t pedf_home[256];
	uint8_t pedf_link_mode[64];
	uint8_t fw_build_string[128];
	uint8_t fw_tag_version[8];
} fw_version_info_t;

/* hades commands */
typedef enum {
	HEVC,
	HADES_FW_INFO
} command_type_t;

/* hades command type, common to all codecs */
typedef struct {
	command_type_t command_type;
	union {
		hevcdecpix_transform_param_t hevc_mme_cmd;
		ping_hades_param_t fw_info;
	} command;
} hades_cmd_t;

/* hades status type, common to all codecs */
typedef struct {
	command_type_t command_type;
	union {
		hevcdecpix_status_t hevc_mme_status;
		fw_version_info_t fw_info;
	} status;
} hades_status_t;


#endif // _HADES_CMD_TYPES_H_

