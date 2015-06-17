/* STM custom crypto cipher configuration
 *
 * Provides wrappers for setting cipher configuration and key ladders on ST
 * custom cipher
 */

#ifndef __STM_CUSTOM_CIPHER_H
#define __STM_CUSTOM_CIPHER_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define STM_CIPHER_MAX_PROFILE_LEN 16
#define STM_CIPHER_MAX_LADDER_KEYS 6
#define STM_CIPHER_MAX_KEY_SIZE 16

enum stm_cipher_alg {
	STM_ALG_AES128,
	STM_ALG_3DES,
	STM_ALG_ARC4,
};

enum stm_cipher_chaining {
	STM_CHAINING_ECB,
	STM_CHAINING_CBC,
	STM_CHAINING_CTR,
	STM_CHAINING_OFB,
	STM_CHAINING_RCBC,
};

enum stm_cipher_residue {
	STM_RESIDUE_NONE,
	STM_RESIDUE_DVS042,
	STM_RESIDUE_CTS,
	STM_RESIDUE_SA_CTS,
	STM_RESIDUE_PLAIN_LR,
};

enum stm_cipher_command {
	STM_CIPHER_SET_CONFIG_KEY,
	STM_CIPHER_SET_CONFIG_LADDER,
};

struct stm_cipher_config {
	/** Session profile to use for cipher.
	 *
	 * Note to select the "NULL"/default session, set this to a
	 * zero-length string (i.e. profile[0] = '\0')
	 */
	char profile[STM_CIPHER_MAX_PROFILE_LEN];

	/** Rule identifier used for key decryption */
	uint32_t key_rule_id;

	/** Rule identifier used for IV decryption */
	uint32_t iv_rule_id;

	/* Block cipher configuration */
	uint32_t alg;
	uint32_t chaining;
	uint32_t packet_size;
	uint32_t msc;
	uint32_t residue;
};

/* Values for skip_set_iv */
enum stm_cipher_iv_skip_mode {
	STM_CIPHER_ALWAYS_SET_IV = 0,
	STM_CIPHER_NEVER_SET_IV,
	STM_CIPHER_SET_IV_ONCE
};

struct stm_cipher_crypto_options {
	bool skip_set_config;
	bool skip_set_key;
	uint32_t skip_set_iv;
};

struct stm_cipher_config_key {
	/** Cipher command
	 * Must be set to STM_CIPHER_SET_CONFIG_KEY for this struct
	 *
	 * Must be the first element in the struct
	 */
	uint32_t command;

	/** Cipher config */
	struct stm_cipher_config config;

	/** Returned opaque ce handles */
	void *ce_session_hdl;
	void *ce_transform_hdl;

	/** Cipher key data */
	struct {
		uint8_t size;
		uint8_t data[STM_CIPHER_MAX_KEY_SIZE];
	} key;

	struct stm_cipher_crypto_options stm_cryp_opts;
};

struct stm_cipher_config_ladder {
	/** Cipher command
	 * Must be set to STM_CIPHER_SET_CONFIG_LADDER for this struct
	 *
	 * Must be the first element in the struct
	 */
	uint32_t command;

	/** Cipher config */
	struct stm_cipher_config config;

	/** Returned opaque ce handles */
	void *ce_session_hdl;
	void *ce_transform_hdl;

	/** Key and IV data */
	struct stm_ladder {

		/** Number of key data buffers in keys field */
		uint8_t n_bufs;

		/** Array of buffers containing key/iv data
		 * The first protecting key is at bufs[0] and the leaf/content
		 * key is at bufs[n_bufs - 1] */
		struct {
			uint8_t size;
			uint8_t data[STM_CIPHER_MAX_KEY_SIZE];
		} buf[STM_CIPHER_MAX_LADDER_KEYS];
	} key, iv;

	struct stm_cipher_crypto_options stm_cryp_opts;
};

struct __stm_cipher_cmd {
	union {
		struct stm_cipher_config_key key;
		struct stm_cipher_config_ladder ladder_key;
	} u;
};

#endif
