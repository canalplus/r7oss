#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include "ubifs.h"

#define AES_BLOCK_ALIGN		16

/* 128 bit key size in bytes for UBIFS */
#define UBIFS_CRYPTO_KEYSIZE 16
/* AES in counter mode is the encryption algorithm */
#define UBIFS_CRYPTO_ALGORITHM "cbc(aes)"

#define UBIFS_CRYPTO_DBG(x)

/* Padding field helpers */
#define UBIFS_CRYPTO_PAD_ENCRYPTED_MASK		0x80
#define UBIFS_CRYPTO_PAD_ALIGNMENT_MASK		(AES_BLOCK_ALIGN-1)

#define UBIFS_CRYPTO_PAD_GET_ALIGNMENT(x) \
		((x) & UBIFS_CRYPTO_PAD_ALIGNMENT_MASK)
#define UBIFS_CRYPTO_PAD_SET_ALIGNMENT(x,y) \
		({x = (x & ~UBIFS_CRYPTO_PAD_ALIGNMENT_MASK) | ((y) & UBIFS_CRYPTO_PAD_ALIGNMENT_MASK);})
#define UBIFS_CRYPTO_PAD_GET_ENCRYPTED_FLAG(x) \
		((x) & UBIFS_CRYPTO_PAD_ENCRYPTED_MASK)
#define UBIFS_CRYPTO_PAD_SET_ENCRYPTED_FLAG(x) \
		({ x |= UBIFS_CRYPTO_PAD_ENCRYPTED_MASK; })

/**
 * aes_crypt - encrypt / decrypt data.
 * @mode: 0 - ecryption disabled, 1 - enabled, 2 - strict
 * @enc: 0 to decrypt, != 0 to encrypt
 * @str: the data to crypt
 * @len: length of the data. may be increased for block alignment (see warning below)
 * @key: the cryptographic key to use to crypt the data
 * @iv: initial vector for CBC
 * @pad: number of extra bytes added for block alignment (value goes out if enc=1, in otherwise)
 *
 * This function applies AES-CBC encryption to the data.
 * Returns zero in case of success and a negative error code in case of failure.
 *
 * WARNING: The operation is done in-place, so @str mutates!
 * WARNING: str's real length must be at least *len + (AES_BLOCK_ALIGN - 1) to deal with AES block padding
 */
int ubifs_crypt(int mode, int enc, void *buf, int *len, uint8_t *key, uint8_t *uiv, uint8_t *pad)
{
	struct crypto_blkcipher *tfm;
	struct blkcipher_desc desc;
	struct scatterlist sg;
	int err;
	uint8_t iv[16];

	if (!buf || !len || !key || !uiv || !pad || (mode < 0 || mode >= UBIFS_ENCRYPTION_CNT))
		return -EINVAL;

//	printk("%s: mode: %d, enc: %d, len: %d, pad: %u\n", __func__, mode, enc, (len ? *len : -1), (pad ? *pad : 0xff));

	if (mode == UBIFS_ENCRYPTION_OFF)
		return 0;

	memcpy(iv, uiv, sizeof(iv));

	/* adjust size for AES block alignment (may be 16 or 32 depending on DMA constraints) */
	if (enc) {
		if (*len % AES_BLOCK_ALIGN) {
			UBIFS_CRYPTO_PAD_SET_ALIGNMENT(*pad, AES_BLOCK_ALIGN - (*len % AES_BLOCK_ALIGN));
			UBIFS_CRYPTO_DBG(printk("len adjustment : %d -> %d\n", *len, *len+UBIFS_CRYPTO_PAD_GET_ALIGNMENT(*pad));)
			*len +=  UBIFS_CRYPTO_PAD_GET_ALIGNMENT(*pad);
		}
		UBIFS_CRYPTO_PAD_SET_ENCRYPTED_FLAG(*pad);
	} else {
		/* special case when data is not encrypted and used strict encryption mode */
		if (!UBIFS_CRYPTO_PAD_GET_ENCRYPTED_FLAG(*pad)) {
			if (mode == UBIFS_ENCRYPTION_STRICT) {
				ubifs_err("discarding non-encrypted data in strict mode");
				return -EINVAL;
			} else
				return 0;
		}
	}

	tfm = crypto_alloc_blkcipher(UBIFS_CRYPTO_ALGORITHM, 0, 0);
	if (IS_ERR(tfm)) {
		err = PTR_ERR(tfm);
		ubifs_err("failed to load transform for aes, error %d", err);
		return err;
	}

	err = crypto_blkcipher_setkey(tfm, key, UBIFS_CRYPTO_KEYSIZE);
	if (err) {
		ubifs_err("cannot set the AES key, flags %#x, error %d",
		crypto_blkcipher_get_flags(tfm), err);
		return err;
	}

	memset(&sg, 0, sizeof(sg));

//	printk("buf = %p, len = %d\n", buf, *len);

	sg_set_buf(&sg, buf, *len);

	desc.info = iv;
	desc.tfm = tfm;
	desc.flags = 0;
	if (enc)
		err = crypto_blkcipher_encrypt_iv(&desc, &sg, &sg, *len);
	else
		err = crypto_blkcipher_decrypt_iv(&desc, &sg, &sg, *len);
	crypto_free_blkcipher(tfm);

	if (err)
		ubifs_err("AES %scryption has failed, error %d", (enc ? "en" : "de"), err);

	/* strip post-padding on decryption */
	if (!enc)
		*len -=  UBIFS_CRYPTO_PAD_GET_ALIGNMENT(*pad);

	return err;
}

void ubifs_crypt_truncate(int mode, int *len, uint8_t *pad)
{
	if (mode == UBIFS_ENCRYPTION_OFF || !UBIFS_CRYPTO_PAD_GET_ENCRYPTED_FLAG(*pad))
		return;

	if (*len % AES_BLOCK_ALIGN) {
		UBIFS_CRYPTO_PAD_SET_ALIGNMENT(*pad, AES_BLOCK_ALIGN - (*len % AES_BLOCK_ALIGN));
		UBIFS_CRYPTO_DBG(printk("trunc: len adjustment : %d -> %d\n", *len, *len+ UBIFS_CRYPTO_PAD_GET_ALIGNMENT(*pad));)
		*len +=  UBIFS_CRYPTO_PAD_GET_ALIGNMENT(*pad);
	} else {
		UBIFS_CRYPTO_PAD_SET_ALIGNMENT(*pad, 0);
	}
}
