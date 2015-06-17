/*
 * File     : elf_aux.h
 * Summary  : Defines the structure using in ELF auxiliary data (PT_ST_INFO)
 *            segments/sections.
 *
 * Copyright (c) 2009-2012 STMicroelectronics Limited.  All rights reserved.
 *
 * The ELF files can contain ST extension sections which are checked by the PBL
 * and other software (OS, libs, etc.).  These section can contain:
 *
 *     CRC checksums      : A CRC value for a segment (or part of a segment)
 *
 * These sections must be aligned to 64 byte boundary.
 *
 * A special segment type (PT_ST_INFO == 0x50000000) is used so that the ELF
 * loaders only need to scan the program headers to find this information -
 * section headers are not required.
 * Also, any program header requiring any additional information should have the
 * PF_AUX bit set in its p_flags field.
 */

#ifndef _ELF_AUX_H
#define _ELF_AUX_H

#ifdef __cplusplus
extern "C" {
#endif

struct elf_st_aux {
	uint8_t		reserved0[256];
	uint32_t	flags;		/* See ST_ELF_AUX_* defines */
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	crcvalue;	/* Checksum (CRC32 or Adler32) */
	uint32_t	reserved3;
	uint32_t	filesz;		/* True p_filesz of subsegment data */
	uint8_t		reserved4[32];
	uint8_t		reserved5[32];
	uint32_t	timestamp;	/* Timestamp */
} __attribute__((packed));

/* flags */
#define ST_ELF_AUX_UNDEFINED	0x00000000 /* No flag state */
#define ST_ELF_AUX_CRC32	0x00000001 /* CRC32 checksum is present */
#define ST_ELF_AUX_ADLER32	0x00000002 /* Adler32 checksum is present */
#define ST_ELF_AUX_PLAIN	0x00000020 /* Plain data; no extra processing */
#define ST_ELF_AUX_COMPRESSED	0x00000040 /* Data is compressed */
#define ST_ELF_AUX_CRC32MED	0x00000400 /* Check CRC32 from media */
#define ST_ELF_AUX_ADLER32MED	0x00000800 /* Check Adler32 from media */
#define ST_ELF_AUX_TIMESTAMPED	0x00001000 /* Aux seg contains a timestamp */

#ifdef __cplusplus
}
#endif

#endif /* _ELF_AUX_H */
