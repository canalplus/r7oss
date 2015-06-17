/************************************************************************
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_ce_mme_share.h

Declares types and constants shared between the stm_ce tango HAL host code and
the slave MME code
************************************************************************/

#ifndef __STM_CE_MME_SHARE_H
#define __STM_CE_MME_SHARE_H

#include "stm_ce.h"
#include "stm_ce_key_rules.h"
#include "stm_ce_fuse.h"

/* Version of the STM_CE_INTERFACE
 * If two applications use the same version of this interface, they are
 * guaranteed to the binary compatible.
 * Please be careful when modifying this interface; unavoidable changes to the
 * resulting binary interface *MUST* result in a new interface version number
 * */
#define STM_CE_MME_INTERFACE_VERSION (2)

/* HAL-specific data items identifiers */
enum ce_mme_data_id_e {
	CE_MME_DATA_STTKD_KTD = 1,
};

/**
 * MME transformer command identifiers.
 *
 * The command is embedded in, struct stm_ce_transformer_cmd_params_s.
 *
 * Elements are explicitly numbered to make viewing the source whilst
 * debugging, easier.
 */
enum ce_mme_command_e {
	MME_CE_INIT = 0,
	MME_CE_TERM = 1,
	MME_CE_SESSION_NEW = 2,
	MME_CE_SESSION_DEL = 3,
	MME_CE_SESSION_SET_RULE_DATA = 4,
	MME_CE_TRANSFORM_NEW = 5,
	MME_CE_TRANSFORM_DEL = 6,
	MME_CE_TRANSFORM_SET_KEY = 7,
	MME_CE_TRANSFORM_SET_IV = 8,
	MME_CE_TRANSFORM_SET_CONFIG = 9,
	MME_CE_TRANSFORM_DMA = 10,
	MME_CE_SEND_DATA = 11,
	MME_CE_GET_VERSION = 12,
	MME_CE_DIRECT_ACCESS = 13,
	MME_CE_SESSION_POWER_STATE_SUSPEND = 14,
	MME_CE_SESSION_POWER_STATE_RESUME = 15,
	MME_CE_FUSE_COMMAND = 16,
	MME_CE_TRANSFORM_SET_KEY_EX = 17,
};

/**
 * Used to indicate status of completed MME commands
 * This abstraction is used, since standard errno values are not consistent
 * across all toolsets
 */
enum ce_mme_err_e {
	CE_MME_SUCCESS = 0,   /* Success */
	CE_MME_EINVAL = 1,   /* Bad parameter passed to HAL */
	CE_MME_ENOTSUP = 2,  /* Feature not supported by HAL */
	CE_MME_EIO = 3,      /* Low-level hardware error */
	CE_MME_ENOMEM = 4,   /* HAL out of memory */
};

/** ce sttkd slave MME transformer name */
#define STM_CE_MME_TRANSFORMER "CE_STTKD_MME_TRANSFORMER"

/** Marshal key data from indirect to contiguous storage. */
#define MME_MAX_KEY_LEN (32)
struct ce_mme_buffer {
	uint8_t data[MME_MAX_KEY_LEN];
	size_t size;
};

/** Version info constants */
#define CE_MME_MAX_VERSIONS 4
#define CE_MME_MAX_VERSION_NAME 32

/** Version information struct */
struct ce_mme_version_s {
	char name[CE_MME_MAX_VERSION_NAME];
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
};

/* Direct Access
 * Describes an outline da struct to provide a maximum data size for mme params.
 * The mme param is actually passed as a uint8_t[].
 */
#define MAX_DA_DATA 32
struct direct_access_mme_s {
	uint32_t interface;
	uint8_t data_in[MAX_DA_DATA];
	uint32_t length_in;
	uint32_t offset_in;
	uint8_t data_out[MAX_DA_DATA];
	uint32_t length_out;
	uint32_t offset_out;
};

/** Maximum length of error message */
#define CE_MME_MAX_ERRORTEXT 128

/** HAL error information struct */
struct ce_mme_error_s {
	uint32_t hal_code;
	char text[CE_MME_MAX_ERRORTEXT];
};

/** Fix the max number of keys at the maximum number of derivations in a key
 * rule */
#define MME_MAX_KEY_BUFS STM_CE_KEY_RULES_MAX_DERIVATIONS

/** Fix the maximum number of mme data buffers, which can be sent with a
 * command */
#define CE_MAX_MME_BUFFERS 1

/**
 * Multicom MME transformer command parameters.
 *
 * A single MME transform is used to pass command and data between the Host
 * and RT. All command parameters and results are passed via the bidirectional
 * buffer.
 */
struct ce_mme_params_s {
	/** Command ID. */
	enum ce_mme_command_e ce_mme_command;

	stm_ce_handle_t session;
	stm_ce_handle_t transform;
	stm_ce_transform_type_t transform_type;
	uint32_t stage;

	/** Error code returned by HAL. */
	int err;

	/** Reduce structure size via union. */
	union stm_ce_transform_cmd_params_u {

		/** Any of the three key types - protecting, key, IV. */
		struct keys_mme_s {
			uint32_t rule_id;
			struct ce_mme_buffer buf[MME_MAX_KEY_BUFS];
			uint32_t num_buffers;
			uint32_t key_num;
			stm_ce_key_operation_t keyop;
		} keys;

		/*Power state Profiles*/
		stm_ce_power_state_profile_t power_state_type;

		/* Transform stage configuration data */
		stm_ce_transform_config_t config;

		/* Returned Tango Path identifier (HAL-specific) */
		uint32_t path_id;

		/* Data buffer */
		struct send_data_mme_s {
			uint32_t id;
			uint32_t size;
		} send_data;

		struct dma_mme_s {
			uint32_t n_src_buffers;
			uint32_t n_dst_buffers;
		} dma;

		/* Returned error info (populated if err != 0) */
		struct ce_mme_error_s error;

		/* Returned version information */
		struct version_info_s {
			uint32_t n_versions;
			struct ce_mme_version_s v[CE_MME_MAX_VERSIONS];
		} version_info;

		/* Direct Access: Bidirectional data structure. */
		uint8_t da[sizeof(struct direct_access_mme_s)];

		/* Fuse Comamnd: Bidirectional data structure. */
		uint8_t fc[sizeof(struct stm_ce_fuse_op)];
	} u;
};

#endif
