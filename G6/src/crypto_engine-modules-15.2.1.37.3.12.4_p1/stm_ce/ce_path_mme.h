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

Source file name : ce_path_mme.h

Declares tango HAL MME host functions
************************************************************************/

#ifndef __CE_PATH_MME_H
#define __CE_PATH_MME_H

#include "ce_path.h"
#include "mme.h"

/** Initialise/ terminate Multicom MME Transformer. */
int ce_path_mme_init_transformer(MME_TransformerHandle_t *mme_transfomer);
int ce_path_mme_term_transformer(MME_TransformerHandle_t mme_transformer);

/* CE PATH MME functions*/
int ce_path_mme_init(MME_TransformerHandle_t mme_transformer);
int ce_path_mme_term(MME_TransformerHandle_t mme_transformer);
int ce_path_mme_send_data(MME_TransformerHandle_t mme_transformer,
		uint32_t id, const uint8_t *data, uint32_t size,
		uint32_t ro, uint32_t rw);
int ce_path_mme_get_version(MME_TransformerHandle_t mme_transformer,
		stm_ce_version_t *versions, uint32_t max_versions,
		uint32_t *n_versions);

int ce_path_mme_session_new(struct ce_session *session);
int ce_path_mme_session_del(struct ce_session *session);
int ce_path_mme_session_set_rule_data(struct ce_session *session,
		const uint32_t rule_id,
		const stm_ce_buffer_t *protected_keys,
		uint32_t num_buffers);
int ce_path_mme_direct_access(struct ce_session *session,
		struct ce_transform *transform,	void *args);
int ce_path_mme_fuse_command(struct ce_session *session,
		struct ce_transform *transform,	void *args);
int ce_path_mme_transform_new(struct ce_transform *transform,
		stm_ce_transform_type_t type, uint32_t n_stages,
		uint32_t *path_id);
int ce_path_mme_transform_del(struct ce_transform *transform);
int ce_path_mme_transform_set_key(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *key,
		const uint32_t key_num,
		const int key_not_iv);
int ce_path_mme_transform_set_key_Ex(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *key,
		const uint32_t key_num,
		const int key_not_iv,
		const stm_ce_key_operation_t keyop);
int ce_path_mme_transform_set_iv(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_buffer_t *iv,
		const uint32_t key_num);
int ce_path_mme_transform_set_config(struct ce_transform *transform,
		uint32_t stage,
		const stm_ce_transform_config_t *config);
int ce_path_mme_transform_dma(struct ce_transform *transform,
		const stm_ce_buffer_t *src, const uint32_t n_src,
		const stm_ce_buffer_t *dst, const uint32_t n_dst);
int ce_path_mme_session_power_manage(struct ce_session *session,
		stm_ce_power_state_profile_t power_state_profile,
		const int power_down);
#endif
