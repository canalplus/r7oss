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

Source file name : stm_ce.h

Declares STKPI Crypto Engine (CE) API constants, types and functions
************************************************************************/

#ifndef __STM_CE_H
#define __STM_CE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm_common.h"

/* stm_ce types */
typedef struct stm_ce_handle_s *stm_ce_handle_t;

typedef struct stm_ce_buffer_s {
	uint8_t *data;
	size_t size;
} stm_ce_buffer_t;

typedef enum stm_ce_direction_e {
	STM_CE_DIRECTION_ENCRYPT = 0,
	STM_CE_DIRECTION_DECRYPT,
	STM_CE_DIRECTION_LAST,
} stm_ce_direction_t;

typedef enum stm_ce_cipher_e {
	STM_CE_CIPHER_NONE = 0,
	STM_CE_CIPHER_PROPRIATORY0,
	STM_CE_CIPHER_AES,
	STM_CE_CIPHER_TDES,
	STM_CE_CIPHER_DVB_CSA2,
	STM_CE_CIPHER_DVB_CSA3,
	STM_CE_CIPHER_MULTI2,
	STM_CE_CIPHER_ARC4,
	STM_CE_CIPHER_RESERVED0,
	STM_CE_CIPHER_CA3,
	STM_CE_CIPHER_LAST,
} stm_ce_cipher_t;

typedef enum stm_ce_hash_e {
	STM_CE_HASH_SHA1 = 0,
	STM_CE_HASH_SHA224,
	STM_CE_HASH_SHA256,
	STM_CE_HASH_MD5,
	STM_CE_HASH_LAST,
} stm_ce_hash_t;

typedef enum stm_ce_chaining_e {
	STM_CE_CHAINING_NONE = 0,
	STM_CE_CHAINING_CBC,
	STM_CE_CHAINING_CTR,
	STM_CE_CHAINING_OFB,
	STM_CE_CHAINING_RCBC,
	STM_CE_CHAINING_DVB_LSA_CBC,
	STM_CE_CHAINING_DVB_LSA_RCBC,
	STM_CE_CHAINING_LAST,
} stm_ce_chaining_t;

typedef enum stm_ce_residue_e {
	STM_CE_RESIDUE_NONE = 0,
	STM_CE_RESIDUE_DVS042,
	STM_CE_RESIDUE_CTS,
	STM_CE_RESIDUE_SA_CTS,
	STM_CE_RESIDUE_IPTV,
	STM_CE_RESIDUE_PLAIN_LR,
	STM_CE_RESIDUE_LAST,
} stm_ce_residue_t;

typedef struct stm_ce_version_s {
	char *name;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
} stm_ce_version_t;

typedef struct stm_ce_caps_s {
	stm_ce_version_t *version;
	uint32_t num_versions;
} stm_ce_caps_t;

/* stm_ce functions */
int stm_ce_init(void);

int stm_ce_term(void);

int stm_ce_get_capabilities(stm_ce_caps_t *caps);

/* stm_ce_session types */
typedef enum stm_ce_session_ctrl_e {
	STM_CE_SESSION_CTRL_LAST,
} stm_ce_session_ctrl_t;

typedef enum stm_ce_session_cmpd_ctrl_e {
	STM_CE_SESSION_CMPD_CTRL_DIRECT_ACCESS = 7,
	STM_CE_SESSION_CMPD_CTRL_FUSE_OP,
	STM_CE_SESSION_CMPD_CTRL_LAST,
} stm_ce_session_cmpd_ctrl_t;

typedef struct stm_ce_session_caps_s {
	char *session_profile;
} stm_ce_session_caps_t;

/* stm_ce_session functions */
int stm_ce_session_new(const char *scheme, stm_ce_handle_t *session);

int stm_ce_session_delete(const stm_ce_handle_t session);

int stm_ce_session_get_control(const stm_ce_handle_t session,
	const stm_ce_session_ctrl_t selector,
	uint32_t *value);

int stm_ce_session_set_control(const stm_ce_handle_t session,
	const stm_ce_session_ctrl_t selector,
	const uint32_t value);

int stm_ce_session_set_compound_control(
	const stm_ce_handle_t session,
	const stm_ce_session_cmpd_ctrl_t selector,
	void *args);

int stm_ce_session_get_capabilities(const stm_ce_handle_t session,
	stm_ce_session_caps_t *caps);

int stm_ce_session_set_rule_data(stm_ce_handle_t session,
	const int32_t key_rule_id,
	const stm_ce_buffer_t *buffers);

/* stm_ce_transform constants */
#define STM_CE_MSC_AUTO UINT_MAX

/* stm_ce_transform types */
typedef enum stm_ce_transform_type_e {
	STM_CE_TRANSFORM_TYPE_STREAM = 0,
	STM_CE_TRANSFORM_TYPE_MEMORY,
	STM_CE_TRANSFORM_TYPE_MEMORY_CLEAR,
	STM_CE_TRANSFORM_TYPE_LAST,
} stm_ce_transform_type_t;

typedef enum stm_ce_transform_ctrl_e {
	STM_CE_TRANSFORM_DATA_OUT_0,
	STM_CE_TRANSFORM_DATA_OUT_1,
	STM_CE_TRANSFORM_CTRL_LAST
} stm_ce_transform_ctrl_t;

typedef enum stm_ce_transform_cmpd_ctrl_e {
	STM_CE_TRANSFORM_CMPD_CTRL_DIRECT_ACCESS = 7,
	STM_CE_TRANSFORM_CMPD_CTRL_LAST,
} stm_ce_transform_cmpd_ctrl_t;

typedef struct stm_ce_transform_caps_s {
	stm_ce_transform_type_t type;
	uint32_t stages;
} stm_ce_transform_caps_t;

typedef enum stm_ce_key_polarity_e {
	STM_CE_KEY_POLARITY_NONE = 1,
	STM_CE_KEY_POLARITY_EVEN = 1,
	STM_CE_KEY_POLARITY_ODD = 2,
	STM_CE_KEY_POLARITY_ALL = -1,
	STM_CE_KEY_POLARITY_EVEN_CLEAR = 17,
	STM_CE_KEY_POLARITY_ODD_CLEAR = 18,
} stm_ce_key_polarity_t;

typedef enum stm_ce_power_state_profile_e {
	STM_CE_POWER_STATE_ACTIVE_STANDBY_PROFILE = 0,
	STM_CE_POWER_STATE_ACTIVE_STANDBY_PURE_DYNAMIC = 1,
	STM_CE_POWER_STATE_HOST_PASSIVE_STANDBY = 2,
	STM_CE_POWER_STATE_CONTROLLER_PASSIVE_STANDBY = 3,
	STM_CE_POWER_STATE_LAST = 4,
} stm_ce_power_state_profile_t;

typedef enum stm_ce_key_operation_e {
	eSTM_CE_KEY_GENERIC_SET_KEY = 1,
	eSTM_CE_KEY_GENERIC_RESET_KEY = 2,
	eSTM_CE_KEY_LOAD_KEY_FM_BUS_ENC = 3,
	eSTM_CE_KEY_LOAD_KEY_FM_BUS_DEC = 4,
	eSTM_CE_KEY_RESET_BUS_SRC = 5,
	eSTM_CE_KEY_RESET_BUS_DEST = 6,
	eSTM_CE_KEY_LOAD_KEY_FM_BUS_NO_CONF_CHG_ENC = 7,
	eSTM_CE_KEY_LOAD_KEY_FM_BUS_NO_CONF_CHG_DEC = 8,
} stm_ce_key_operation_t;

typedef struct stm_ce_transform_config_s {
	uint32_t key_rule_id;
	uint32_t iv_rule_id;
	stm_ce_direction_t direction;
	stm_ce_cipher_t cipher;
	stm_ce_chaining_t chaining;
	stm_ce_residue_t residue;
	uint32_t msc;
	uint32_t packet_size;
} stm_ce_transform_config_t;

typedef struct stm_ce_transform_hash_s {
	stm_ce_hash_t algorithm;
} stm_ce_transform_hash_t;


/* stm_ce_transform functions */
int stm_ce_transform_new(const stm_ce_handle_t session,
	const stm_ce_transform_type_t type,
	const uint32_t n_stages,
	stm_ce_handle_t *transform);

int stm_ce_transform_delete(const stm_ce_handle_t transform);

int stm_ce_transform_get_control(const stm_ce_handle_t transform,
	const stm_ce_transform_ctrl_t selector,
	uint32_t *value);

int stm_ce_transform_set_control(const stm_ce_handle_t transform,
	const stm_ce_transform_ctrl_t selector,
	const uint32_t value);

int stm_ce_transform_set_compound_control(
	const stm_ce_handle_t transform,
	const stm_ce_transform_cmpd_ctrl_t selector,
	const void *value);

int stm_ce_transform_get_capabilities(const stm_ce_handle_t transform,
	stm_ce_transform_caps_t *caps);

int stm_ce_transform_set_config(const stm_ce_handle_t transform,
	const uint32_t stage,
	const stm_ce_transform_config_t *config);

int stm_ce_transform_set_key(const stm_ce_handle_t transform,
	const uint32_t stage,
	const stm_ce_buffer_t *key,
	const int32_t key_number);
int stm_ce_transform_set_key_Ex(const stm_ce_handle_t transform,
	const uint32_t stage,
	const stm_ce_buffer_t *key,
	const int32_t key_number,
	const stm_ce_key_operation_t keyop);

int stm_ce_transform_set_iv(const stm_ce_handle_t transform,
	const uint32_t stage,
	const stm_ce_buffer_t *iv,
	const int32_t key_number);

/* Memory transform additional functions */
int stm_ce_transform_attach(const stm_ce_handle_t transform,
	const stm_object_h target);

int stm_ce_transform_detach(const stm_ce_handle_t transform,
	const stm_object_h target);

int stm_ce_transform_dma(const stm_ce_handle_t transform,
	const stm_ce_buffer_t *src, const stm_ce_buffer_t *dst);

int stm_ce_transform_dma_sg(const stm_ce_handle_t transform,
	const stm_ce_buffer_t *src, const uint32_t n_src,
	const stm_ce_buffer_t *dst, const uint32_t n_dst);

int stm_ce_transform_set_hash_config(const stm_ce_handle_t transform,
	const stm_ce_transform_hash_t *config);

int stm_ce_transform_get_digest(const stm_ce_handle_t transform,
	const stm_ce_buffer_t *buf);


#ifdef __cplusplus
}
#endif

#endif
