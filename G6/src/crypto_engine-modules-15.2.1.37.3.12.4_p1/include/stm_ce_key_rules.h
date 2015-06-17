/**
 * @brief Declares stm_ce key rules structure
 *
 * @author Jonathan Brett
 * @data 14th July 2011
 **/

#ifndef _STM_CE_KEY_RULES_H
#define _STM_CE_KEY_RULES_H

/* Profile specific key-rules, provided at build environement creation */
#include "key-rules.h"

#define STM_CE_KEY_RULES_VERSION 0x00000002
#define STM_CE_KEY_RULES_MAX_NAME 16
#define STM_CE_KEY_RULES_MAX_DERIVATIONS 16
#define STM_CE_KEY_RULES_MAX_RULES 45
#define STM_CE_KEY_RULES_NONE 0
#define STM_CE_KEY_CLASS_NONE (-1)

/**
 * @brief Enumerates stm_ce HALs. Used to specify which HAL a set of rules is
 * applicable to.
 **/
enum stm_ce_hal_e {
	STM_CE_HAL_TANGO,
	STM_CE_HAL_LAST,
};

/**
 * @brief Struct to hold a single key derivation
 **/
typedef struct stm_ce_derivation_s {
	/** HAL Protecting key class **/
	int hal_protecting_key_class;

	/** HAL Source key class **/
	int hal_source_key_class;

	/** HAL key class being derived **/
	int hal_key_class;
	/** Size of the encrypted key **/
	unsigned int encrypted_key_size;

	/** HAL derivation cipher **/
	unsigned int hal_cipher;
} stm_ce_derivation_t;

/**
 * @brief Struct to hold a single key rule
 **/
typedef struct stm_ce_rule_s {
	/* Key rule ID */
	unsigned int id;
	/* Key rule name */
	char name[STM_CE_KEY_RULES_MAX_NAME];

	/* HAL leaf key class */
	int hal_leaf_key_class;
	/* HAL leaf key size */
	int hal_leaf_key_size;
	/* HAL leaf key number */
	int hal_leaf_key_num;

	/* Number of derivations */
	int num_derivations;
	/* Array of derivations */
	struct stm_ce_derivation_s
		derivations[STM_CE_KEY_RULES_MAX_DERIVATIONS];
} stm_ce_rule_t;

/**
 * @brief Structure to hold a collection of key rules
 **/
typedef struct stm_ce_rule_set_s {
	/** Struct version **/
	unsigned int struct_version;

	/** Key rules name **/
	char name[STM_CE_KEY_RULES_MAX_NAME];
	/** Key rules version **/
	unsigned int version;

	/** HAL **/
	int hal;

	/** Number of key rules **/
	unsigned int num_rules;
	/** Array of key rules containing "num_rules" elements */
	struct stm_ce_rule_s rules[STM_CE_KEY_RULES_MAX_RULES];
} stm_ce_rule_set_t;

#endif
