/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_adaptation Library.

Crypto_adaptation is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_adaptation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_adaptation; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_adaptation Library may alternatively be licensed under a proprietary
license from ST.

Source file name : crypto_adaptation.h

Contains crypto_adaptation internal definitions
************************************************************************/
#ifndef __CRYPTO_ADAPTATION_H
#define __CRYPTO_ADAPTATION_H

#include <linux/crypto.h>
#include <linux/list.h>
#include "stm_ce.h"
#include "crypto/stm_cipher.h"

#define CE_CRYPTO_CRA_PRIORITY 400

struct ce_alg_template {
	char name[CRYPTO_MAX_ALG_NAME];
	unsigned int blocksize;
	unsigned int keysize;
	unsigned int ivsize;
	stm_ce_cipher_t ce_cipher;
	stm_ce_chaining_t ce_chaining;
	stm_ce_residue_t ce_residue;
	uint32_t ce_key_rule_id;
	uint32_t ce_iv_rule_id;

	/* Optional overrides for crypto alg functions */
	int (*init)(struct crypto_tfm *);
	void (*exit)(struct crypto_tfm *);
	int (*setkey)(struct crypto_ablkcipher *, const u8 *,
			unsigned int keylen);
};

struct ce_alg {
	struct list_head list;
	struct crypto_alg crypto_alg;
	stm_ce_transform_config_t ce_config_base;
};

struct ce_session {
	struct list_head lh;
	struct kref ref;
	char name[STM_CIPHER_MAX_PROFILE_LEN];
	stm_ce_handle_t ce_hdl;
};

struct ce_ctx {
	struct ce_session *ce_session;
	stm_ce_handle_t ce_transform;
	stm_ce_transform_config_t ce_config;
	stm_ce_buffer_t ce_key;
	unsigned int max_keylen;
	struct stm_cipher_crypto_options stm_cryp_opts;
};

int ce_crypto_register(void);
int ce_crypto_unregister(void);

/* CE transform update helper */
int ce_transform_update(struct ce_ctx *ctx);

/* CE session helper functions */
int ce_get_session(char *name, struct ce_session **ret_session);
void ce_put_session(struct ce_session *sess);
void ce_destroy_all_sessions(void);

/* stm custom cipher prototypes */
int stm_cipher_init(struct crypto_tfm *tfm);
void stm_cipher_exit(struct crypto_tfm *tfm);
int stm_cipher_set_key(struct crypto_ablkcipher *cipher,
		const u8 *key, unsigned int keylen);
#endif
