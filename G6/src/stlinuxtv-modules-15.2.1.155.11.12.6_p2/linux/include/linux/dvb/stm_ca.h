/*
 * Declares STM extensions to the Linux DVB CA API
 *
 * These extensions allow for configuration of STM CA devices via the
 * LinuxDVB-CA CA_SEND_MSG ioctl command.
 */

#ifndef __STM_CA_H
#define __STM_CA_H

#include <linux/limits.h>
#include <linux/dvb/ca.h>

/* Constants for fixed-size structures */
#define DVB_CA_PROFILE_SIZE 32
#define DVB_CA_PROFILE_MAX_LEN (DVB_CA_PROFILE_SIZE - 1)
#define DVB_CA_MAX_LADDER_KEYS 6
#define DVB_CA_MAX_KEY_DATA 32

#define DVB_CA_MAX_FUSE_NAME 40
#define DVB_CA_MAX_FUSE_DATA  4


enum {
	DVB_CA_POLARITY_NONE = -1,
	/* Matches existing LinuxDVB-CA API */
	/* In LDVB-CA 0 is used for even polarity, 1 for odd polarity.
	 * those value should not be changed in order to keep backcompat */
	DVB_CA_POLARITY_EVEN = 0,
	DVB_CA_POLARITY_ODD = 1,
};

typedef enum {
	DVB_CA_CIPHER_NONE,
	DVB_CA_CIPHER_DVB_CSA,
	DVB_CA_CIPHER_DVB_CSA3,
	DVB_CA_CIPHER_AES,
	DVB_CA_CIPHER_TDES,
	DVB_CA_CIPHER_MULTI2,
	DVB_CA_CIPHER_RESERVED0
} dvb_ca_cipher_t;

typedef enum {
	DVB_CA_CHAINING_NONE,
	DVB_CA_CHAINING_CBC,
	DVB_CA_CHAINING_CTR,
	DVB_CA_CHAINING_OFB,
	DVB_CA_CHAINING_RCBC,
	DVB_CA_CHAINING_DVB_LSA_CBC,
	DVB_CA_CHAINING_DVB_LSA_RCBC,
} dvb_ca_chaining_t;

typedef enum {
	DVB_CA_RESIDUE_NONE,
	DVB_CA_RESIDUE_DVS042,
	DVB_CA_RESIDUE_CTS,
	DVB_CA_RESIDUE_SA_CTS,
	DVB_CA_RESIDUE_IPTV,
	DVB_CA_RESIDUE_PLAIN_LR,
} dvb_ca_residue_t;

#define DVB_CA_MSC_AUTO UINT_MAX

/* CA commands for stm ca devices */
enum {
	/* Set the CA profile name for this device. This will cause profile
	 * data to be loaded, which defines the set of available key
	 * rules/ladders.
	 * This command should be called immediately after opening the CA
	 * device, otherwise the platform's default profile will be assume */
	DVB_CA_SET_PROFILE,

	/* Resets the CA device, clearing all profile and key data */
	DVB_CA_RESET,

	/* Sets protected key ladder data. The data will be specific to
	 * the ladders available in the selected CA profile */
	DVB_CA_SET_LADDER_DATA,

	/* Sets the descrambling configuration for as single index on a CA
	 * device */
	DVB_CA_SET_DSC_CONFIG,

	/* Sets the descrambling key for as single index on a CA device */
	DVB_CA_SET_DSC_KEY,

	/* Sets the descrambling IV or initial counter value for a single
	 * index on a CA device */
	DVB_CA_SET_DSC_IV,

	/* Sets the re-scrambling configuration for as single index on a CA
	 * device */
	DVB_CA_SET_SCR_CONFIG,

	/* Sets the re-scrambling key for as single index on a CA device */
	DVB_CA_SET_SCR_KEY,

	/* Sets the re-scrambling IV or initial counter value for a single
	 * index on a CA device */
	DVB_CA_SET_SCR_IV,

	/* Sets the de-scrambling configuration for block-based encryption */
	DVB_CA_SET_RAW_DSC_CONFIG,

	/* Sets the scrambling configuration for block-based encryption */
	DVB_CA_SET_RAW_SCR_CONFIG,

	/* Get CA information for this index */
	DVB_CA_GET_INFO,

	/* Operation on Fuse */
	DVB_CA_FUSE_OP,
};

/* Represents a single key buffer */
typedef struct {
	unsigned char size;
	unsigned int polarity;
	unsigned char data[DVB_CA_MAX_KEY_DATA];
} dvb_ca_key_t;

/* Used to send key ladder data with DVB_CA_SET_LADDER_DATA command */
typedef struct {
	/* Identifier of ladder to set */
	int id;
	/* Number of keys in array keys */
	unsigned char n_keys;
	/* Array of key data to set */
	dvb_ca_key_t keys[DVB_CA_MAX_LADDER_KEYS];
} dvb_ca_ladder_t;

/* Used to send cipher configuration data with command DVB_CA_SET_DSC_CONFIG
 * and DVB_CA_SET_SCR_CONFIG */
typedef struct {
	dvb_ca_cipher_t cipher;
	dvb_ca_chaining_t chaining;
	dvb_ca_residue_t residue;
	unsigned int key_ladder_id;
	unsigned int iv_ladder_id;
	unsigned int msc;
	unsigned int dma_transfer_packet_size;
} dvb_ca_config_t;

/* Used to get CA index info with command DVB_CA_GET_INFO */
typedef struct {
	/* Opaque handle for underlying CE session */
	void *ce_session_hdl;

	/* Opaque handle for CE DSC transform */
	void *ce_dsc_transform_hdl;

	/* Opaque handle for CE RAW DSC transform */
	void *ce_raw_dsc_transform_hdl;

	/* Opaque handle for CE RAW SCR transform */
	void *ce_raw_scr_transform_hdl;
} dvb_ca_info_t;


/* possible operation on fuse */
typedef enum {
	DVB_CA_FUSE_READ = 0,
	DVB_CA_FUSE_WRITE,
	DVB_CA_FUSE_WRITE_AND_LOCK,
	DVB_CA_FUSE_LOCK,
} dvb_ca_fuse_op_t;

/* Used to read/write fuse with DVB_CA_FUSE_OP  */
typedef struct {
	dvb_ca_fuse_op_t  op;
	/* Fuse name we want to read/write*/
	char name[DVB_CA_MAX_FUSE_NAME];
	/* Value */
	unsigned int value[DVB_CA_MAX_FUSE_DATA];
} dvb_ca_fuse_t;

/* STM ca message structure */
typedef struct {
	/* Fields for binary compatibility with LDVB-CA message struct */
	unsigned int index;
	/* Should be set to CA_STM for use with this CA api */
	unsigned int type;
#define CA_STM 0x53544D00
	unsigned int length;

	/* STM CA command */
	unsigned int command;
	/* Command parameters */
	union {
		char profile[DVB_CA_PROFILE_SIZE];
		dvb_ca_ladder_t ladder;
		dvb_ca_config_t config;
		dvb_ca_key_t key;
		dvb_ca_info_t info;
		dvb_ca_fuse_t fuse;
	} u;
} dvb_ca_msg_t;

/* Union combining stm ca message struct with standard LDVB-CA message */
typedef union {
	struct ca_msg msg;
	dvb_ca_msg_t dvb_ca_msg;
} dvb_ca_msg_u;

#endif /* __STM_CA_H */
