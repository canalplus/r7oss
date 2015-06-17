/*
 * Example using the STM custom cipher via cryptodev
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#include <crypto/stm_cipher.h>

#include <linux/dvb/stm-keyrules/default.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int debug = 0;

/*
 * Array of test cases
 *
 * Each test case includes the stm cipher config (clear key) the data to
 * encrypt and the expected result
 */
static struct {
	char name[32];
	struct stm_cipher_config_key cfg;
	uint8_t *plaintext;
	uint8_t *ciphertext;
	uint8_t *iv;
	uint32_t size;
} test_case[] = {
	{
		.name = "AES ECB residue clear",
		.cfg = {
			.command = STM_CIPHER_SET_CONFIG_KEY,
			.config = {
				.profile = "\0",
				.key_rule_id = RULE_CLEAR_CPCW16,
				.iv_rule_id = 0,
				.alg = STM_ALG_AES128,
				.chaining = STM_CHAINING_ECB,
			},
			.key = {
				.size = 16,
				.data = {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				},
			},
		},
		.plaintext = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xbe, 0xef,
		},
		.ciphertext = (uint8_t[]) {
			0xc6, 0xa1, 0x3b, 0x37, 0x87, 0x8f, 0x5b, 0x82,
			0x6f, 0x4f, 0x81, 0x62, 0xa1, 0xc8, 0xd8, 0x79,
			0xbe, 0xef,
		},
		.size = 18,
	},
	{
		.name = "AES CBC residue clear",
		.cfg = {
			.command = STM_CIPHER_SET_CONFIG_KEY,
			.config = {
				.profile = "\0",
				.key_rule_id = RULE_CLEAR_CPCW16,
				.iv_rule_id = RULE_CLEAR_IV16,
				.alg = STM_ALG_AES128,
				.chaining = STM_CHAINING_CBC,
			},
			.key = {
				.size = 16,
				.data = {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				},
			},
		},
		.plaintext = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xf0, 0xf1, 0xf2,
		},
		.ciphertext = (uint8_t[]) {
			0x66, 0xa7, 0xc7, 0xe8, 0x34, 0x52, 0x31, 0x48,
			0x97, 0x51, 0xde, 0x07, 0x33, 0x16, 0xad, 0xad,
			0x6e, 0x61, 0x99, 0xba, 0x56, 0xd5, 0x8c, 0x52,
			0x0b, 0x6e, 0x65, 0x16, 0xf1, 0xca, 0x81, 0xaa,
			0xf0, 0xf1, 0xf2,
		},
		.size = 35,
		.iv = (uint8_t[]) {
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
			0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
		},
	},
	{
		.name = "TDES ECB residue clear",
		.cfg = {
			.command = STM_CIPHER_SET_CONFIG_KEY,
			.config = {
				.profile = "\0",
				.key_rule_id = RULE_CLEAR_CPCW16,
				.iv_rule_id = 0,
				.alg = STM_ALG_3DES,
				.chaining = STM_CHAINING_ECB,
			},
			.key = {
				.size = 16,
				.data = {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				},
			},
		},
		.plaintext = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xde, 0xad, 0xbe, 0xef,
		},
		.ciphertext = (uint8_t[]) {
			0xdd, 0xad, 0xa1, 0x61, 0xe8, 0xd7, 0x96, 0x73,
			0xdd, 0xad, 0xa1, 0x61, 0xe8, 0xd7, 0x96, 0x73,
			0xdd, 0xad, 0xa1, 0x61, 0xe8, 0xd7, 0x96, 0x73,
			0xdd, 0xad, 0xa1, 0x61, 0xe8, 0xd7, 0x96, 0x73,
			0xdd, 0xad, 0xa1, 0x61, 0xe8, 0xd7, 0x96, 0x73,
			0xde, 0xad, 0xbe, 0xef,
		},
		.size = 44,
	},
	{
		.name = "TDES CBC residue clear",
		.cfg = {
			.command = STM_CIPHER_SET_CONFIG_KEY,
			.config = {
				.profile = "\0",
				.key_rule_id = RULE_CLEAR_CPCW16,
				.iv_rule_id = RULE_CLEAR_IV8,
				.alg = STM_ALG_3DES,
				.chaining = STM_CHAINING_CBC,
			},
			.key = {
				.size = 16,
				.data = {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				},
			},
		},
		.plaintext = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xca, 0xfe
		},
		.ciphertext = (uint8_t[]) {
			0xd7, 0x35, 0x7e, 0xb3, 0x3f, 0x30, 0xb6, 0x82,
			0x1a, 0xd4, 0xe5, 0x9f, 0xee, 0x8b, 0x14, 0x79,
			0xf2, 0xdd, 0x7a, 0xb1, 0xa5, 0xfd, 0xbd, 0xaf,
			0x92, 0x2f, 0xf2, 0x87, 0xe4, 0xc3, 0x00, 0xab, 
			0xca, 0xfe
		},
		.size = 34,
		.iv = (uint8_t[]) {
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		},
	},
	{
		.name = "AES CTR",
		.cfg = {
			.command = STM_CIPHER_SET_CONFIG_KEY,
			.config = {
				.profile = "\0",
				.key_rule_id = RULE_CLEAR_CPCW16,
				.iv_rule_id = RULE_CLEAR_IV16,
				.alg = STM_ALG_AES128,
				.chaining = STM_CHAINING_CTR,
			},
			.key = {
				.size = 16,
				.data = {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				},
			},
		},
		.plaintext = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x11,
		},
		.ciphertext = (uint8_t[]) {
			0xc6, 0xa1, 0x3b, 0x37, 0x87, 0x8f, 0x5b, 0x82,
			0x6f, 0x4f, 0x81, 0x62, 0xa1, 0xc8, 0xd8, 0x79,
			0x73, 0x46, 0x13, 0x95, 0x95, 0xc0, 0xb4, 0x1e,
			0x49, 0x7b, 0xbd, 0xe3, 0x65, 0xf4, 0x2d, 0x0a,
			0x49, 0xd6, 0x87, 0x53, 0x99, 0x9b, 0xa6, 0x8c,
			0xf2,
		},
		.size = 41,
		.iv = (uint8_t[]) {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
	},
};

static void print_buffer(char *label, uint8_t *buf, uint32_t len)
{
	uint32_t i;
	fprintf(stderr, "%s\n", label);
	for (i = 0; i < len; i++)
		fprintf(stderr, "%.2x%s", buf[i], (i + 1) % 16 ? "" : "\n");
	if (len % 16)
		fprintf(stderr,"\n");
}

static int stm_cipher_test(int cfd)
{
	int i;
	struct session_op sess = {0};
	struct crypt_op cryp = {0};
	int result = 0;
	struct stm_cipher_config_key *cfg;

	uint8_t *result_ciphertext;
	uint8_t *result_plaintext;

	for (i = 0; i < ARRAY_SIZE(test_case); i++) {

		fprintf(stderr, "================================\n");
		fprintf(stderr, "Test %d: %s\n", i, test_case[i].name);

		cfg = &test_case[i].cfg;

		/* Allocate buffers for test */
		result_ciphertext = malloc(test_case[i].size);
		result_plaintext = malloc(test_case[i].size);
		if (!result_ciphertext || !result_plaintext) {
			fprintf(stderr, "Test buffer allocation failed\n");
			result = 1;
			break;
		}

		/* Get session for STM cipher*/
		sess.cipher = CRYPTO_STM_CUSTOM;
		sess.keylen = sizeof(struct stm_cipher_config_key);
		sess.key = (__u8 *)cfg;
		if (ioctl(cfd, CIOCGSESSION, &sess)) {
			perror("ioctl(CIOCGSESSION)");
			result = 1;
			break;
		}
		if (debug) {
			printf("Created new cryptodev session: %u\n", sess);
			printf("\tCE session: %p\n", cfg->ce_session_hdl);
			printf("\tCE transform: %p\n", cfg->ce_transform_hdl);
		}

		/* Encrypt plaintext -> ciphertext */
		cryp.ses = sess.ses;
		cryp.len = test_case[i].size;
		cryp.src = test_case[i].plaintext;
		cryp.dst = result_ciphertext;
		cryp.iv = test_case[i].iv;
		cryp.op = COP_ENCRYPT;
		if (ioctl(cfd, CIOCCRYPT, &cryp)) {
			perror("ioctl(CIOCCRYPT) -- encrypt");
			result = 1;
			break;
		}

		/* Decrypt ciphertext -> plaintext */
		cryp.ses = sess.ses;
		cryp.len = test_case[i].size;
		cryp.src = result_ciphertext;
		cryp.dst = result_plaintext;
		cryp.iv = NULL;
		cryp.op = COP_DECRYPT;
		if (ioctl(cfd, CIOCCRYPT, &cryp)) {
			perror("ioctl(CIOCCRYPT) -- decrypt");
			result = 1;
			break;
		}

		/* Close session */
		if (ioctl(cfd, CIOCFSESSION,  &sess.ses)) {
			perror("ioctl(CIOCFSESSION)");
			result = 1;
			break;
		}

		if (debug) {
			/* Print buffers */
			print_buffer("Plaintext", test_case[i].plaintext,
					test_case[i].size);
			print_buffer("Ciphertext", result_ciphertext,
					test_case[i].size);
			print_buffer("Decrypted plaintext", result_plaintext,
				test_case[i].size);
		}

		/* Compare results */
		if (0 != memcmp(result_ciphertext, test_case[i].ciphertext,
					test_case[i].size)) {
			fprintf(stderr, "Ciphertext doesn't match result\n");
			print_buffer("Expected ciphertext",
					test_case[i].ciphertext,
					test_case[i].size);
			print_buffer("Actual ciphertext", result_ciphertext,
				test_case[i].size);
			result = 1;
			break;
		}
		if (0 != memcmp(result_plaintext, test_case[i].plaintext,
					test_case[i].size)) {
			fprintf(stderr, "Plaintext doesn't match result\n");
			print_buffer("Expected plaintext",
					test_case[i].plaintext,
					test_case[i].size);
			print_buffer("Decrypted plaintext", result_plaintext,
					test_case[i].size);
			result = 1;
			break;
		}

		fprintf(stderr, "================================\n\n");

		free(result_ciphertext);
		free(result_plaintext);
		result_ciphertext = NULL;
		result_plaintext = NULL;
	}

	if (result_ciphertext)
		free(result_ciphertext);
	if (result_plaintext)
		free(result_plaintext);

	return result;
}

int main(int argc, char** argv)
{
	int fd = -1, cfd = -1;
	int result = 0;

	if (argc > 1)
		debug = 1;

	/* Open the crypto device */
	fd = open("/dev/crypto", O_RDWR, 0);
	if (fd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	/* Clone file descriptor */
	if (ioctl(fd, CRIOGET, &cfd)) {
		perror("ioctl(CRIOGET)");
		return 1;
	}

	/* Run the tests */
	result = stm_cipher_test(cfd);

	/* Close cloned descriptor */
	if (close(cfd)) {
		perror("close(cfd)");
		return 1;
	}

	/* Close the original descriptor */
	if (close(fd)) {
		perror("close(fd)");
		return 1;
	}

	return result;
}

