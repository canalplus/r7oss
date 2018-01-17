/*
 * Always ON (AON) register interface between bootloader and Linux
 *
 * Copyright © 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __BRCMSTB_AON_DEFS_H__
#define __BRCMSTB_AON_DEFS_H__

#include <linux/compiler.h>

/* Magic number in upper 16-bits */
#define BRCMSTB_S3_MAGIC_MASK                   0xffff0000
#define BRCMSTB_S3_MAGIC_SHORT                  0x5AFE0000

enum {
	/* Restore random key for AES memory verification (off = fixed key) */
	S3_FLAG_LOAD_RANDKEY		= (1 << 0),

	/* Scratch buffer page table is present */
	S3_FLAG_SCRATCH_BUFFER_TABLE	= (1 << 1),
};

#define BRCMSTB_HASH_LEN		(128 / 8) /* 128-bit hash */

#define AON_REG_MAGIC_FLAGS			0x00
#define AON_REG_CONTROL_LOW			0x04
#define AON_REG_CONTROL_HIGH			0x08
#define AON_REG_S3_HASH				0x0c /* hash of S3 params */
#define AON_REG_CONTROL_HASH_LEN		0x1c
#define AON_REG_PANIC				0x20

#define BRCMSTB_S3_MAGIC		0x5AFEB007
#define BRCMSTB_PANIC_MAGIC		0x512E115E
#define BOOTLOADER_SCRATCH_SIZE		64
#define IMAGE_DESCRIPTORS_BUFSIZE	(2 * 1024)
/*
 * Store up to 64 4KB page entries; this number is flexible, as long as
 * brcmstb_bootloader_scratch_table::num_entries is adjusted accordingly
 */
#define BRCMSTB_SCRATCH_BUF_SIZE	(256 * 1024)

struct brcmstb_bootloader_scratch_table {
	/* System page size, in KB; likely 4 (i.e., 4KB) */
	uint32_t page_size;
	/*
	 * Number of page entries in this table. Provided for flexibility, but
	 * should be BRCMSTB_SCRATCH_BUF_SIZE / PAGE_SIZE
	 */
	uint16_t num_entries;
	uint16_t reserved;
	struct {
		uint32_t upper;
		uint32_t lower;
	} entries[];
} __packed;

struct brcmstb_s3_params {
	/* scratch memory for bootloader */
	uint8_t scratch[BOOTLOADER_SCRATCH_SIZE];

	uint32_t magic; /* BRCMSTB_S3_MAGIC */
	uint64_t reentry; /* PA */

	/* descriptors */
	uint32_t hash[BRCMSTB_HASH_LEN / 4];

	/*
	 * If 0, then ignore this parameter (there is only one set of
	 *   descriptors)
	 *
	 * If non-0, then a second set of descriptors is stored at:
	 *
	 *   descriptors + desc_offset_2
	 *
	 * The MAC result of both descriptors is XOR'd and stored in @hash
	 */
	uint32_t desc_offset_2;

	/*
	 * (Physical) address of a brcmstb_bootloader_scratch_table, for
	 * providing a large DRAM buffer to the bootloader
	 */
	uint64_t buffer_table;

	uint32_t spare[70];

	uint8_t descriptors[IMAGE_DESCRIPTORS_BUFSIZE];
} __packed;

#endif /* __BRCMSTB_AON_DEFS_H__ */
