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

Source file name : ce_crypto.c

Defines crypto_adaptation functions to integrate stm_ce DMA
functionality into the Linux crypto framework
************************************************************************/
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <linux/dma-mapping.h>

#include "crypto_adaptation.h"
#include "stm_ce_key_rules.h"
#include "stm_ce_sp/key-rules.h"
#include "crypto/stm_cipher.h"

/* List of instantiated ce_session objects */
LIST_HEAD(ce_session_list);
DEFINE_MUTEX(ce_session_list_lock);

/* List of all ce_alg objects */
LIST_HEAD(ce_alg_list);
DEFINE_MUTEX(ce_alg_list_lock);

/* Default session profile to use for crypto session objects */
#define CE_CRYPTO_DEFAULT_PROFILE "\0"

static inline int _sg_nents(struct scatterlist *sg)
{
	int nents = 0;
	while (sg) {
		nents++;
		sg = sg_next(sg);
	}
	return nents;
}

/* Set the config and key for a transform */
int ce_transform_update(struct ce_ctx *ctx)
{
	int err;

	err = stm_ce_transform_set_config(ctx->ce_transform, 0,
			&ctx->ce_config);
	if (err) {
		pr_err("Failed to configure transform\n");
		return err;
	}
	if (ctx->ce_key.size) {
		err = stm_ce_transform_set_key(ctx->ce_transform, 0,
				&ctx->ce_key, 0);
		if (err)
			pr_warning("Failed to set transform key\n");
	}
	return err;
}

int ce_get_session(char *name, struct ce_session **ret_session)
{
	int err = 0, len;
	struct ce_session *sess;
	bool sess_found = false;

	len = strlen(name);
	if (len > (STM_CIPHER_MAX_PROFILE_LEN - 1))
		return -EINVAL;

	mutex_lock(&ce_session_list_lock);

	/* Search for an existing session with matching profile name */
	list_for_each_entry(sess, &ce_session_list, lh) {
		if (strncmp(sess->name, name, sizeof(sess->name) == 0)) {
			sess_found = true;
			kref_get(&sess->ref);
			break;
		}
	}

	/* Create a new session for this profile */
	if (!sess_found) {
		sess = kzalloc(sizeof(*sess), GFP_KERNEL);
		if (!sess) {
			err = -ENOMEM;
		} else {
			/* Zero-length string implies NULL (default) session */
			if (0 == strnlen(name, sizeof(sess->name)))
				err = stm_ce_session_new(NULL, &sess->ce_hdl);
			else
				err = stm_ce_session_new(name, &sess->ce_hdl);

			if (!err) {
				strncpy(sess->name, name, len);
				kref_init(&sess->ref);
				list_add_tail(&sess->lh, &ce_session_list);
			}
		}
	}

	mutex_unlock(&ce_session_list_lock);

	if (!err)
		*ret_session = sess;
	else
		kzfree(sess);

	return err;
}

static void ce_session_release(struct kref *ref)
{
	struct ce_session *sess = container_of(ref, struct ce_session, ref);

	stm_ce_session_delete(sess->ce_hdl);
	list_del(&sess->lh);
	kfree(sess);
}

void ce_put_session(struct ce_session *sess)
{
	mutex_lock(&ce_session_list_lock);
	kref_put(&sess->ref, ce_session_release);
	mutex_unlock(&ce_session_list_lock);
}

void ce_destroy_all_sessions(void)
{
	struct ce_session *sess, *temp;
	mutex_lock(&ce_session_list_lock);

	/* This list should be empty at this point, so warn if there is
	 * anything in it */
	list_for_each_entry_safe(sess, temp, &ce_session_list, lh) {
		pr_warning("Orphan session (profile: %s). Forcing deletion\n",
				sess->name);
		ce_session_release(&sess->ref);
	}

	mutex_unlock(&ce_session_list_lock);
}

static int ce_crypto_cra_init(struct crypto_tfm *tfm)
{
	struct ce_alg *ce_alg = container_of(tfm->__crt_alg, struct ce_alg,
			crypto_alg);
	struct crypto_alg *crypto_alg = tfm->__crt_alg;
	struct ce_ctx *ctx = crypto_tfm_ctx(tfm);
	int err;

	/* Get reference to default session object */
	err = ce_get_session(CE_CRYPTO_DEFAULT_PROFILE, &ctx->ce_session);
	if (err) {
		pr_err("Failed to get default session object\n");
		goto error;
	}

	memcpy(&ctx->ce_config, &ce_alg->ce_config_base,
			sizeof(stm_ce_transform_config_t));

	err = stm_ce_transform_new(ctx->ce_session->ce_hdl,
			STM_CE_TRANSFORM_TYPE_MEMORY, 1, &ctx->ce_transform);
	if (err) {
		pr_err("Failed to create transform for %s\n",
				crypto_tfm_alg_name(tfm));
		goto error;
	}

	ctx->ce_key.size = 0;
	ctx->max_keylen = crypto_alg->cra_ablkcipher.max_keysize;
	ctx->ce_key.data = kmalloc(ctx->max_keylen, GFP_KERNEL);
	if (!ctx->ce_key.data) {
		pr_err("Failed to allocate key buffer\n");
		goto error;
	}

	/* Perform initial transform config */
	err = ce_transform_update(ctx);
	if (err)
		goto error;

	/* TODO: Setup STKPI data path
	 * We need stm_memsrc/memsink to support scatterlists in order to use
	 * them here. See bug 18916:
	 * https://bugzilla.stlinux.com/show_bug.cgi?id=18916 */
	return 0;
error:
	if (ctx->ce_transform)
		stm_ce_transform_delete(ctx->ce_transform);
	if (ctx->ce_session)
		ce_put_session(ctx->ce_session);

	return err;
}

static void ce_crypto_cra_exit(struct crypto_tfm *tfm)
{
	struct ce_ctx *ctx = crypto_tfm_ctx(tfm);
	int err;

	err = stm_ce_transform_delete(ctx->ce_transform);
	if (err)
		pr_warning("Failed to delete tfm for %s (%d)\n",
				crypto_tfm_alg_name(tfm), err);

	/* kfree(NULL) is safe */
	kfree(ctx->ce_key.data);
	ce_put_session(ctx->ce_session);
}

static int ce_crypto_setkey(struct crypto_ablkcipher *cipher, const u8 *key,
		unsigned int keylen)
{
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(cipher);
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(cipher);
	int err;

	if (keylen > ctx->max_keylen)
		return -EINVAL;

	memcpy(ctx->ce_key.data, key, keylen);
	ctx->ce_key.size = keylen;

	err = stm_ce_transform_set_key(ctx->ce_transform, 0, &ctx->ce_key, 0);
	if (err) {
		pr_warning("Failed to set transform key for %s\n",
				crypto_tfm_alg_name(tfm));
	}

	return err;
}

static int ce_crypto_start(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	int err = 0;
	stm_ce_buffer_t *ce_src, *ce_dst = NULL;
	int i = 0, nents_src = 0, nents_dst = 0;
	struct scatterlist *sg;
	size_t size;

	stm_ce_buffer_t ce_iv = {
		.data = req->info,
		.size = crypto_ablkcipher_ivsize(tfm),
	};

	pr_debug("%s enter\n", __func__);

	if (STM_CIPHER_NEVER_SET_IV != ctx->stm_cryp_opts.skip_set_iv)	{
		/* Set the IV */
		if (ctx->ce_config.chaining != STM_CE_CHAINING_NONE) {
			err = stm_ce_transform_set_iv(ctx->ce_transform, 0,
					&ce_iv, 0);
			if (err) {
				pr_err("Failed to set iv\n");
				goto error0;
			}
		}
	}

	/* Only set the IV the first time? */
	if (STM_CIPHER_SET_IV_ONCE == ctx->stm_cryp_opts.skip_set_iv)
		ctx->stm_cryp_opts.skip_set_iv = STM_CIPHER_NEVER_SET_IV;

	/* convert from sg to a ce list of physically-addressed buffer */
	nents_src = _sg_nents(req->src);

	err = dma_map_sg(NULL, req->src, nents_src,
			(req->src == req->dst) ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	if (!err) {
		pr_err("dma_map_sg() sg src error\n");
		goto error0;
	}

	size = nents_src * sizeof(stm_ce_buffer_t);

	ce_src = kzalloc(size, GFP_KERNEL);

	if (!ce_src) {
		pr_err("Failed to kzalloc ce_src\n");
		err = -ENOMEM;
		goto error1;
	}

	for_each_sg(req->src, sg, nents_src, i) {
		ce_src[i].size = sg_dma_len(sg);
		ce_src[i].data = (uint8_t *)sg_dma_address(sg);

		pr_debug("[%d] sg %p, nents = %d, sg->dma_address 0x%x sg->length 0x%x\n"
			 , i, sg, nents_src, sg->dma_address, sg->length);
	}

	if (req->src != req->dst) {
		nents_dst = _sg_nents(req->dst);

		size = nents_dst * sizeof(stm_ce_buffer_t);
		ce_dst = kzalloc(size, GFP_KERNEL);

		if (!ce_dst) {
			pr_err("Failed to kzalloc ce_dst\n");
			err = -ENOMEM;
			goto error2;
		}

		err = dma_map_sg(NULL, req->dst, nents_dst, DMA_FROM_DEVICE);

		if (!err) {
			pr_err("dma_map_sg() dst error\n");
			goto error3;
		}

		for_each_sg(req->dst, sg, nents_dst, i) {
			ce_dst[i].size = sg_dma_len(sg);
			ce_dst[i].data = (uint8_t *)sg_dma_address(sg);

			pr_debug("[%d] sg %p, nents = %d, sg->dma_address 0x%x sg->length 0x%x\n"
				 , i, sg, nents_dst, sg->dma_address, sg->length);
		}
	}

	/* Perform the DMA */
	err = stm_ce_transform_dma_sg(ctx->ce_transform, ce_src,
				nents_src,
				(req->src == req->dst) ? ce_src : ce_dst,
				(req->src == req->dst) ? nents_src : nents_dst);

	if (err) {
		pr_err("Error in dma\n");
		goto error4;
	}

	if (req->src != req->dst) {
		dma_sync_sg_for_cpu(NULL, req->dst,
				    nents_dst, DMA_FROM_DEVICE);
	}

error4:
	if (req->src != req->dst)
		dma_unmap_sg(NULL, req->dst, nents_dst, DMA_FROM_DEVICE);
error3:
	if (req->src != req->dst)
		kfree(ce_dst);
error2:
	kfree(ce_src);
error1:
	dma_unmap_sg(NULL, req->src, nents_src,
		    (req->src == req->dst) ? DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
error0:
	pr_debug("%s exit\n", __func__);
	return err;
}

static int ce_crypto_encrypt(struct ablkcipher_request *req)
{
	int err;

	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	/* Configure the transform for encryption */
	if (!ctx->stm_cryp_opts.skip_set_config) {
		if (STM_CE_DIRECTION_ENCRYPT != ctx->ce_config.direction) {
			ctx->ce_config.direction = STM_CE_DIRECTION_ENCRYPT;
			err = ce_transform_update(ctx);
			if (err)
				return err;
		}
	}
	return ce_crypto_start(req);
}

static int ce_crypto_decrypt(struct ablkcipher_request *req)
{
	int err;
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(tfm);


	if (!ctx->stm_cryp_opts.skip_set_config) {
		/* Configure the transform for decryption */
		if (STM_CE_DIRECTION_DECRYPT != ctx->ce_config.direction) {
			ctx->ce_config.direction = STM_CE_DIRECTION_DECRYPT;
			err = ce_transform_update(ctx);
			if (err)
				return err;
		}
	}
	return ce_crypto_start(req);
}

static struct ce_alg_template driver_algs[] = {
	{
		.name = "ecb(aes)",
		.blocksize = AES_BLOCK_SIZE,
		.keysize = AES_KEYSIZE_128,
		.ivsize = 0,
		.ce_cipher = STM_CE_CIPHER_AES,
		.ce_chaining = STM_CE_CHAINING_NONE,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = STM_CE_KEY_RULES_NONE,
	},
	{
		.name = "cbc(aes)",
		.blocksize = AES_BLOCK_SIZE,
		.keysize = AES_KEYSIZE_128,
		.ivsize = AES_BLOCK_SIZE,
		.ce_cipher = STM_CE_CIPHER_AES,
		.ce_chaining = STM_CE_CHAINING_NONE,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = STM_CE_KEY_RULES_NONE,
	},
	{
		.name = "ctr(aes)",
		.blocksize = AES_BLOCK_SIZE,
		.keysize = AES_KEYSIZE_128,
		.ivsize = AES_BLOCK_SIZE,
		.ce_cipher = STM_CE_CIPHER_AES,
		.ce_chaining = STM_CE_CHAINING_CTR,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = RULE_CLEAR_IV16,
	},
	{
		.name = "ecb(des3_ede)",
		.blocksize = DES_BLOCK_SIZE,
		.keysize = DES_KEY_SIZE * 2,
		.ivsize = 0,
		.ce_cipher = STM_CE_CIPHER_TDES,
		.ce_chaining = STM_CE_CHAINING_NONE,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = STM_CE_KEY_RULES_NONE,
	},
	{
		.name = "cbc(des3_ede)",
		.blocksize = DES_BLOCK_SIZE,
		.keysize = DES_KEY_SIZE * 2,
		.ivsize = DES_BLOCK_SIZE,
		.ce_cipher = STM_CE_CIPHER_TDES,
		.ce_chaining = STM_CE_CHAINING_CBC,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = RULE_CLEAR_IV8,
	},
	{
		.name = "ctr(des3_ede)",
		.blocksize = DES_BLOCK_SIZE,
		.keysize = DES_KEY_SIZE * 2,
		.ivsize = DES_BLOCK_SIZE,
		.ce_cipher = STM_CE_CIPHER_TDES,
		.ce_chaining = STM_CE_CHAINING_CTR,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = RULE_CLEAR_CPCW16,
		.ce_iv_rule_id = RULE_CLEAR_IV8,
	},
	{
		.name = "stm-custom",
		.blocksize = 1,
		.keysize = 0,
		.ivsize = 16,
		.ce_cipher = STM_CE_CIPHER_NONE,
		.ce_chaining = STM_CE_CHAINING_NONE,
		.ce_residue = STM_CE_RESIDUE_NONE,
		.ce_key_rule_id = STM_CE_KEY_RULES_NONE,
		.ce_iv_rule_id = STM_CE_KEY_RULES_NONE,
		.init = stm_cipher_init,
		.exit = stm_cipher_exit,
		.setkey = stm_cipher_set_key,
	}
};

static struct ce_alg *ce_alg_new(struct ce_alg_template *template)
{
	struct ce_alg *ce_alg;
	struct crypto_alg *alg;

	ce_alg = kzalloc(sizeof(struct ce_alg), GFP_KERNEL);
	if (!ce_alg) {
		pr_err("alg %s allocation failed\n", template->name);
		return ERR_PTR(-ENOMEM);
	}

	alg = &ce_alg->crypto_alg;

	snprintf(alg->cra_name, CRYPTO_MAX_ALG_NAME, "%s", template->name);
	snprintf(alg->cra_driver_name, CRYPTO_MAX_ALG_NAME, "%s-stm_ce",
			template->name);
	alg->cra_module = THIS_MODULE;
	alg->cra_priority = CE_CRYPTO_CRA_PRIORITY;
	alg->cra_blocksize = template->blocksize;
	alg->cra_alignmask = 0;
	alg->cra_ctxsize = sizeof(struct ce_ctx);

	alg->cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC;
	alg->cra_type = &crypto_ablkcipher_type;
	INIT_LIST_HEAD(&alg->cra_list);

	if (template->init)
		alg->cra_init = template->init;
	else
		alg->cra_init = ce_crypto_cra_init;
	if (template->exit)
		alg->cra_exit = template->exit;
	else
		alg->cra_exit = ce_crypto_cra_exit;
	if (template->setkey)
		alg->cra_ablkcipher.setkey = template->setkey;
	else
		alg->cra_ablkcipher.setkey = ce_crypto_setkey;
	alg->cra_ablkcipher.encrypt = ce_crypto_encrypt;
	alg->cra_ablkcipher.decrypt = ce_crypto_decrypt;


	alg->cra_ablkcipher.min_keysize = template->keysize;
	alg->cra_ablkcipher.max_keysize = template->keysize;
	alg->cra_ablkcipher.ivsize = template->ivsize;

	ce_alg->ce_config_base.cipher = template->ce_cipher;
	ce_alg->ce_config_base.chaining = template->ce_chaining;
	ce_alg->ce_config_base.residue = template->ce_residue;
	ce_alg->ce_config_base.key_rule_id = template->ce_key_rule_id;
	ce_alg->ce_config_base.iv_rule_id = template->ce_iv_rule_id;
	ce_alg->ce_config_base.direction = STM_CE_DIRECTION_DECRYPT;
	ce_alg->ce_config_base.msc = 0;

	return ce_alg;
}

int ce_crypto_register(void)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(driver_algs); i++) {
		struct ce_alg *ce_alg;

		ce_alg = ce_alg_new(&driver_algs[i]);
		if (IS_ERR(ce_alg)) {
			err = PTR_ERR(ce_alg);
			pr_warning("alg %s allocation failed\n",
					driver_algs[i].name);
			continue;
		}

		err = crypto_register_alg(&ce_alg->crypto_alg);
		if (err) {
			pr_warning("alg %s registration failed\n",
					ce_alg->crypto_alg.cra_name);
			kfree(ce_alg);
		} else {
			pr_info("Registered crypto alg %s\n",
					ce_alg->crypto_alg.cra_name);
			list_add_tail(&ce_alg->list, &ce_alg_list);
		}
	}
	return 0;
}

int ce_crypto_unregister(void)
{
	struct ce_alg *ce_alg, *n;

	/* Unregister and destroy all created ce_alg objects */
	list_for_each_entry_safe(ce_alg, n, &ce_alg_list, list) {
		crypto_unregister_alg(&ce_alg->crypto_alg);
		pr_info("Unregistered alg %s, driver=%s\n",
				ce_alg->crypto_alg.cra_name,
				ce_alg->crypto_alg.cra_driver_name);
		list_del(&ce_alg->list);
		kfree(ce_alg);
	}

	return 0;
}
