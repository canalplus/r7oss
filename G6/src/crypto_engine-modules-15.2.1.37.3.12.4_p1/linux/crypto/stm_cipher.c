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

Source file name : stm_cipher.c

Implements the stm_cipher adaptation layer, which translates Linux crypto
framework calls to the stm custom cipher into STKPI operations on stm_ce
************************************************************************/
#include <linux/kernel.h>
#include <crypto/algapi.h>
#include "crypto_adaptation.h"
#include "crypto/stm_cipher.h"


static int ce_config_get_ivsize(stm_ce_transform_config_t *ce_cfg)
{
	if (STM_CE_CHAINING_NONE == ce_cfg->chaining)
		return 0;

	if (STM_CE_CIPHER_TDES == ce_cfg->cipher)
		return 8;
	else
		return 16;
}

static int ce_config_from_stm_cipher(stm_ce_transform_config_t *ce_cfg,
		struct stm_cipher_config *st_cfg)
{
	switch (st_cfg->alg) {
	case STM_ALG_AES128:
		ce_cfg->cipher = STM_CE_CIPHER_AES;
		break;
	case STM_ALG_3DES:
		ce_cfg->cipher = STM_CE_CIPHER_TDES;
		break;
	case STM_ALG_ARC4:
		ce_cfg->cipher = STM_CE_CIPHER_ARC4;
		break;
	default:
		return -EINVAL;
	}

	switch (st_cfg->chaining) {
	case STM_CHAINING_ECB:
		ce_cfg->chaining = STM_CE_CHAINING_NONE;
		break;
	case STM_CHAINING_CBC:
		ce_cfg->chaining = STM_CE_CHAINING_CBC;
		break;
	case STM_CHAINING_CTR:
		ce_cfg->chaining = STM_CE_CHAINING_CTR;
		break;
	case STM_CHAINING_OFB:
		ce_cfg->chaining = STM_CE_CHAINING_OFB;
		break;
	case STM_CHAINING_RCBC:
		ce_cfg->chaining = STM_CE_CHAINING_RCBC;
		break;
	default:
		return -EINVAL;
	}

	switch (st_cfg->residue) {
	case STM_RESIDUE_NONE:
		ce_cfg->residue = STM_CE_RESIDUE_NONE;
		break;
	case STM_RESIDUE_DVS042:
		ce_cfg->residue = STM_CE_RESIDUE_DVS042;
		break;
	case STM_RESIDUE_CTS:
		ce_cfg->residue = STM_CE_RESIDUE_CTS;
		break;
	case STM_RESIDUE_SA_CTS:
		ce_cfg->residue = STM_CE_RESIDUE_SA_CTS;
		break;
	case STM_RESIDUE_PLAIN_LR:
		ce_cfg->residue = STM_CE_RESIDUE_PLAIN_LR;
		break;
	default:
		return -EINVAL;
	}

	ce_cfg->packet_size = st_cfg->packet_size;
	ce_cfg->msc = st_cfg->msc;
	ce_cfg->key_rule_id = st_cfg->key_rule_id;
	ce_cfg->iv_rule_id = st_cfg->iv_rule_id;

	return 0;
}

int stm_cipher_init(struct crypto_tfm *tfm)
{
	struct ce_alg *ce_alg = container_of(tfm->__crt_alg, struct ce_alg,
			crypto_alg);
	struct ce_ctx *ctx = crypto_tfm_ctx(tfm);

	/* We cannot create the transform object until its session
	 * and configuration is known, which is provided by the setkey
	 * function.
	 * All we can do at this point is initialise the context to sensible
	 * default values
	 */
	memset(ctx, 0, sizeof(*ctx));
	memcpy(&ctx->ce_config, &ce_alg->ce_config_base,
			sizeof(stm_ce_transform_config_t));
	return 0;
}

void stm_cipher_exit(struct crypto_tfm *tfm)
{
	struct ce_ctx *ctx = crypto_tfm_ctx(tfm);

	if (ctx->ce_transform)
		stm_ce_transform_delete(ctx->ce_transform);
	if (ctx->ce_session)
		ce_put_session(ctx->ce_session);

	kfree(ctx->ce_key.data);
}

static int stm_cipher_setconfig(struct crypto_tfm *tfm, struct ce_ctx *ctx,
		struct stm_cipher_config *cfg)
{
	int err;

	/* If the requested session profile doesn't match the existing session,
	 * we need to get a new session witht the correct profile */
	if (!ctx->ce_session || 0 != strncmp(cfg->profile,
				ctx->ce_session->name,
				STM_CIPHER_MAX_PROFILE_LEN)) {
		if (ctx->ce_transform) {
			stm_ce_transform_delete(ctx->ce_transform);
			ctx->ce_transform = NULL;
		}
		if (ctx->ce_session) {
			ce_put_session(ctx->ce_session);
			ctx->ce_session = NULL;
		}

		err = ce_get_session(cfg->profile, &ctx->ce_session);
		if (err)
			return err;
	}

	/* Update the stored cipher parameters in ce_config if requested */
	err = ce_config_from_stm_cipher(&ctx->ce_config, cfg);
	if (err)
		return err;

	/* Update the crypto tfm iv size */
	tfm->crt_ablkcipher.ivsize = ce_config_get_ivsize(&ctx->ce_config);

	/* Create the transform (if none exists) */
	if (!ctx->ce_transform) {
		err = stm_ce_transform_new(ctx->ce_session->ce_hdl,
				STM_CE_TRANSFORM_TYPE_MEMORY,
				1, &ctx->ce_transform);
		if (err) {
			pr_err("Failed to stm_ce transform (%d)\n", err);
			return err;
		}
	}

	/* Update the the transform config */
	return ce_transform_update(ctx);
}

static int stm_cipher_set_key_single(struct ce_ctx *ctx,
		struct stm_cipher_config_key *key)
{
	int err;

	/* Allocate space for the key data. Note kfree(NULL) is safe */
	if (ctx->ce_key.size < key->key.size)
		kfree(ctx->ce_key.data);

	ctx->ce_key.data = kmalloc(key->key.size, GFP_KERNEL);
	if (!ctx->ce_key.data)
		return -ENOMEM;

	memcpy(ctx->ce_key.data, key->key.data, key->key.size);
	ctx->ce_key.size = key->key.size;

	err = stm_ce_transform_set_key(ctx->ce_transform, 0, &ctx->ce_key, 0);
	if (err)
		pr_warning("Failed to set stm_ce transform key (%d)\n", err);

	return err;
}

static int stm_cipher_set_ladder_key(struct ce_ctx *ctx,
		struct stm_cipher_config_ladder *ladder)
{
	stm_ce_buffer_t ce_bufs[STM_CIPHER_MAX_LADDER_KEYS - 1];
	int i;
	int err;

	if (ladder->key.n_bufs > ARRAY_SIZE(ce_bufs) ||
			ladder->iv.n_bufs > ARRAY_SIZE(ce_bufs)) {
		pr_err("Too many ladder keys (%d)\n", ladder->key.n_bufs);
		return -EINVAL;
	}

	for (i = 0; i < ladder->key.n_bufs; i++) {
		ce_bufs[i].data = ladder->key.buf[i].data;
		ce_bufs[i].size = ladder->key.buf[i].size;
	}

	/* Buffers 0..(n_bufs - 2) are protecting keys */
	err = stm_ce_session_set_rule_data(ctx->ce_session->ce_hdl,
			ctx->ce_config.key_rule_id,
			ce_bufs);
	if (err) {
		pr_err("Error setting rule data for rule %d (%d)\n",
				ctx->ce_config.key_rule_id, err);
		return err;
	}

	/* Allocate space for the key data. Note kfree(NULL) is safe */
	if (ctx->ce_key.size < ce_bufs[ladder->key.n_bufs - 1].size)
		kfree(ctx->ce_key.data);

	ctx->ce_key.data = kmalloc(ce_bufs[ladder->key.n_bufs - 1].size,
		GFP_KERNEL);
	if (!ctx->ce_key.data)
		return -ENOMEM;

	memcpy(ctx->ce_key.data, ce_bufs[ladder->key.n_bufs - 1].data,
		ce_bufs[ladder->key.n_bufs - 1].size);
	ctx->ce_key.size = ce_bufs[ladder->key.n_bufs - 1].size;
	/* Buffer (n_buf - 1) contains the content key */
	err = stm_ce_transform_set_key(ctx->ce_transform, 0,
			&ce_bufs[ladder->key.n_bufs - 1], 0);
	if (err) {
		pr_err("Error setting transform key (%d)\n", err);
		return err;
	}
	/* If there is no IV, just return */
	if ((0 == ladder->iv.n_bufs) || (0 == ctx->ce_config.iv_rule_id))
		return err;

	for (i = 0; i < ladder->iv.n_bufs; i++) {
		ce_bufs[i].data = ladder->iv.buf[i].data;
		ce_bufs[i].size = ladder->iv.buf[i].size;
	}

	if (ladder->iv.n_bufs > 1) {
	/* Buffers 0..(n_bufs - 2) are protecting keys */
	err = stm_ce_session_set_rule_data(ctx->ce_session->ce_hdl,
			ctx->ce_config.iv_rule_id,
			ce_bufs);
	if (err) {
		pr_err("Error setting rule data for rule %d (%d)\n",
					ctx->ce_config.key_rule_id, err);
		return err;
		}
	}

	/* Buffer (n_buf - 1) contains the content IV */
	err = stm_ce_transform_set_iv(ctx->ce_transform, 0,
			&ce_bufs[ladder->iv.n_bufs - 1], 0);
	if (err)
		pr_err("Error setting transform iv (%d)\n", err);

	return err;
}

int stm_cipher_set_key(struct crypto_ablkcipher *cipher,
		const u8 *key, unsigned int keylen)
{
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(cipher);
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	int err = 0;
	struct stm_cipher_config_key *cfg_key =
		(struct stm_cipher_config_key *)key;
	struct stm_cipher_config_ladder *cfg_ladder =
		(struct stm_cipher_config_ladder *)key;

	struct stm_cipher_crypto_options *stm_cryp_opts = NULL;

	if (!key || keylen < sizeof(cfg_key->command)) {
		pr_err("Invalid stm cipher struct\n");
		return -EINVAL;
	}


	switch (cfg_key->command) {
	case STM_CIPHER_SET_CONFIG_KEY:
		if (keylen == sizeof(struct stm_cipher_config_key)) {
			stm_cryp_opts = &cfg_key->stm_cryp_opts;
			err = stm_cipher_setconfig(tfm, ctx, &cfg_key->config);
			if (!err) {
				cfg_ladder->ce_session_hdl =
					ctx->ce_session->ce_hdl;
				cfg_ladder->ce_transform_hdl =
					ctx->ce_transform;
				if (!stm_cryp_opts->skip_set_key)
					err = stm_cipher_set_key_single(ctx,
							cfg_key);
			}
			if (0)
				err = stm_cipher_set_key_single(ctx,
						cfg_key);
		} else {
			pr_err("Bad cipher key size\n");
			err = -EINVAL;
		}
		break;

	case STM_CIPHER_SET_CONFIG_LADDER:
		if (keylen == sizeof(struct stm_cipher_config_ladder)) {
			stm_cryp_opts = &cfg_ladder->stm_cryp_opts;
			err = stm_cipher_setconfig(tfm, ctx,
					&cfg_ladder->config);
			if (!err) {
				cfg_ladder->ce_session_hdl =
					ctx->ce_session->ce_hdl;
				cfg_ladder->ce_transform_hdl =
					ctx->ce_transform;
				if (!stm_cryp_opts->skip_set_key) {
					err = stm_cipher_set_ladder_key(ctx,
							cfg_ladder);
				}
			}
			/* Bug 29629 WA */
#if 1
			if (0)
#else
			if (!err)
#endif
				err = stm_cipher_set_ladder_key(ctx,
						cfg_ladder);
		} else {
			pr_err("Bad cipher ladder size\n");
			err = -EINVAL;
		}
		break;

	default:
		pr_err("Bad stm cipher command: 0x%x\n", cfg_key->command);
		err = -EINVAL;
	}
	if (NULL != stm_cryp_opts) {
		ctx->stm_cryp_opts.skip_set_config =
				stm_cryp_opts->skip_set_config;
		ctx->stm_cryp_opts.skip_set_iv =
				stm_cryp_opts->skip_set_iv;
	}


	return err;
}
