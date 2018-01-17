/*
 * Copyright © 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#ifndef _BRCMSTB_MEMORY_API_H
#define _BRCMSTB_MEMORY_API_H

/*
 * Memory API that must be supplied for Broadcom STB middleware.
 *
 * **********************
 * READ ME BEFORE EDITING
 * **********************
 *
 * If you update this file, make sure to bump BRCMSTB_H_VERSION
 * in brcmstb.h if there is an API change!
 */

#define MAX_BRCMSTB_RANGE           8
#define MAX_BRCMSTB_MEMC            3
#define MAX_BRCMSTB_RESERVED_RANGE  16

struct brcmstb_range {
	u64 addr;
	u64 size;  /* 0 means no region */
};

struct brcmstb_memory {
	struct {
		struct brcmstb_range range[MAX_BRCMSTB_RANGE];
		int count;
	} memc[MAX_BRCMSTB_MEMC];
	/* fixed map space */
	struct {
		struct brcmstb_range range[MAX_BRCMSTB_RANGE];
		int count;
	} lowmem;
	/* bmem carveout regions */
	struct {
		struct brcmstb_range range[MAX_BRCMSTB_RANGE];
		int count;
	} bmem;
	/* CMA carveout regions */
	struct {
		struct brcmstb_range range[MAX_BRCMSTB_RANGE];
		int count;
	} cma;
	/* regions that nexus cannot recommend for bmem or CMA */
	struct {
		struct brcmstb_range range[MAX_BRCMSTB_RESERVED_RANGE];
		int count;
	} reserved;
};

int brcmstb_memory_get(struct brcmstb_memory *mem);
int brcmstb_memory_phys_addr_to_memc(phys_addr_t pa);
void *brcmstb_memory_kva_map(struct page *page, int num_pges, pgprot_t pgprot);
void *brcmstb_memory_kva_map_phys(phys_addr_t phys, size_t size, bool cached);
int brcmstb_memory_kva_unmap(const void *kva);

/* Below functions are for calling during initialization and may need stubs */

int __init brcmstb_memory_get_default_reserve(int bank_nr,
		phys_addr_t *pstart, phys_addr_t *psize);

#ifdef CONFIG_BRCMSTB_MEMORY_API
void __init brcmstb_memory_reserve(void);
#else
static inline void brcmstb_memory_reserve(void) {};
#endif

/* The following relate to the default reservation scheme */

enum brcmstb_reserve_type {
	BRCMSTB_RESERVE_BMEM,
	BRCMSTB_RESERVE_CMA,
};

/* Determines what type of memory reservation will be used w/o CLI params */
extern const enum brcmstb_reserve_type brcmstb_default_reserve;
/* Should be set to true by any CLI option that overrides default reserve */
extern bool brcmstb_memory_override_defaults;

#endif  /* _BRCMSTB_MEMORY_API_H */
