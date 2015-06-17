/*
 * File     : lib/libelf/_libelf-load.c
 * Synopsis : Physical address ELF loader
 * Author   : David Cook <david.cook@st.com>
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 */

#ifdef CONFIG_SUPERH
#include <asm/cacheflush.h>
#endif /* CONFIG_SUPERH */
#ifdef CONFIG_ARM
#include <asm/setup.h>
#endif /* CONFIG_ARM */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_ST_ELF_EXTENSIONS
#include <linux/crc32.h>
#include <linux/elf_aux.h>
#include <linux/zlib.h>
#include <linux/zutil.h>
#endif /* CONFIG_ST_ELF_EXTENSIONS */


/* Function which returns non-zero if the first range entirely contains the
 * second.
 * Args: start range 1 (inclusive)
 *	 end range 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 end range 2 (exclusive)
 */
static inline int rangeContainsA(uint32_t s1, uint32_t e1,
				 uint32_t s2, uint32_t e2)
{
	if (s2 >= s1 && e2 <= e1)
		return 1;
	return 0;
}


/* Function which returns non-zero if the first range entirely contains the
 * second.
 * Args: start range 1 (inclusive)
 *	 range length 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 range length 2 (exclusive)
 */
static inline int rangeContainsL(uint32_t s1, uint32_t l1,
				 uint32_t s2, uint32_t l2)
{
	if (s2 >= s1 && s2 + l2 <= s1 + l1)
		return 1;
	return 0;
}


#if defined(CONFIG_ST_ELF_EXTENSIONS) || defined(CONFIG_ARM)
/* Function which returns non-zero if the 2 ranges specified overlap each other.
 * Args: start range 1 (inclusive)
 *	 range length 1 (exclusive)
 *	 start range 2 (inclusive)
 *	 range length 2 (exclusive)
 */
static int overlapping(uint32_t s1, uint32_t l1, uint32_t s2, uint32_t l2)
{
	if (s1 + l1 <= s2)
		return 0;
	if (s2 + l2 <= s1)
		return 0;
	return 1;
}
#endif /* defined(CONFIG_ST_ELF_EXTENSIONS) || defined(CONFIG_ARM) */


/* Function which checks the physical destination addresses are within any
 * allowed ranges for this load (as specified in the loadParams), or if no
 * allowed ranges were specified, that all ranges to load are outside of kernel
 * memory.
 * Returns 0 for allowed, non-zero for not allowed.
 */
static int allowedRangeCheck(const ElfW(Phdr) *phdr, const uint32_t segNum,
			     const struct ELFW(LoadParams) *loadParams)
{
	if (loadParams && loadParams->numAllowedRanges) {
		int i, inLimits = 0;
		for (i = 0 ; i < loadParams->numAllowedRanges; i++) {
			struct ELFW(MemRange) *allowed =
					&loadParams->allowedRanges[i];
			if (rangeContainsA(allowed->base, allowed->top,
					   phdr->p_paddr,
					   phdr->p_paddr + phdr->p_memsz)) {
				inLimits = 1;
				break;
			}
		}
		if (!inLimits) {
			pr_err("libelf: Segment %d outside allowed limits\n",
			       segNum);
			return -EINVAL;
		}
	} else {
		/* If no limits were specified, ensure the segment
		 * is outside of kernel memory.
		 * We only check on some architectures.  We allow the load with
		 * a warning for architectures we have not checked on.
		 */
#ifdef CONFIG_SUPERH
		/* This was the check used by the old 'coprocessor' driver which
		 * has been removed (except it allowed the ST40 .empty_zero_page
		 * too and we don't bother...see coproc_check_area() in the
		 * STLinux 2.4 0211 kernel file drivers/stm/copro-st_socs.c to
		 * add that).
		 */
		unsigned long startPFN, endPFN;
		startPFN = PFN_DOWN(phdr->p_paddr);
		endPFN = PFN_DOWN(phdr->p_paddr + phdr->p_memsz);
		if (startPFN < max_low_pfn && endPFN > min_low_pfn) {
#elif defined(CONFIG_ARM)
		/* The ARM supports multiple physical memory ranges for the
		 * kernel.
		 */
		int i;
		int overlap = 0;
		for (i = 0; i < meminfo.nr_banks; i++) {
			if (overlapping(phdr->p_paddr, phdr->p_memsz,
					meminfo.bank[i].start,
					meminfo.bank[i].size)) {
				overlap = 1;
				break;
			}
		}
		if (overlap) {
#else /* Other architectures */
		static int warned;	/* Zero-initialised as static */
		if (!warned) {
			pr_warning("libelf: Not checking for kernel memory clash during ELF segment loading\n");
			warned++;
		}
		if (0) {
#endif /* Architectures */
			pr_err("libelf: Segment %d overlaps kernel memory\n",
			       segNum);
			return -EINVAL;
		}
	}
	return 0;
}


/* Function which returns an IO address from an existing IO mapping (as
 * provided in the loadParams) if the entire segment can be loaded via that
 * mapping.  If a mapping is found, the cached pointer will be set to true or
 * false.
 * Returns NULL if there is no existing mapping to use.
 */
static void __iomem *findIOMapping(const ElfW(Addr) pAddr,
				   const unsigned long size,
				   const struct ELFW(LoadParams) *loadParams,
				   bool *cached)
{
	int i;

	if (!loadParams || loadParams->numExistingMappings == 0)
		return NULL;

	for (i = 0; i < loadParams->numExistingMappings; i++) {
		struct ELFW(IORemapMapping) *exMap =
				&loadParams->existingMappings[i];
		if (rangeContainsL(exMap->physBase, exMap->size,
				   pAddr, size)) {
			if (cached)
				*cached = exMap->cached;
			return exMap->vIOBase + (pAddr - exMap->physBase);
		}
	}
	return NULL;
}


#if defined(CONFIG_ST_ELF_EXTENSIONS)
/* Find the PT_LOAD segment associated with a PT_ST_INFO (ST auxiliary)
 * segment.
 * Returns NULL if a matching loadable segment is not found.
 * If a loadable segment is found and segIndex is not NULL, the index of the
 * segment is put in segIndex.
 */
static ElfW(Phdr) *findLoadableSegForSTAuxSeg(const struct ELFW(info) *elfinfo,
					      const ElfW(Phdr) *stAux,
					      int *segIndex)
{
	ElfW(Phdr) *phdr = elfinfo->progbase;
	ElfW(Phdr) *loadableSeg = NULL;
	int i;
	/* Find the associated PT_LOAD segment */
	for (i = 0; i < elfinfo->header->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD && (phdr[i].p_flags & PF_AUX) &&
			overlapping(stAux->p_vaddr,
				    (stAux->p_filesz & ~0x10000000),
				    phdr[i].p_vaddr, phdr[i].p_memsz)) {
			/* We've found the matching loadable segment */
			loadableSeg = &phdr[i];
			if (segIndex)
				*segIndex = i;
			break;
		}
	}
	return loadableSeg;
}


/* Find a PT_ST_INFO (ST auxiliary) segment associated with a PT_LOAD segment.
 * Returns NULL if a matching aux segment is not found.
 * If an ST aux segment is found and segIndex is not NULL, the index of the
 * segment is put in segIndex.
 */
static ElfW(Phdr) *findSTAuxSegForLoadable(const struct ELFW(info) *elfinfo,
					   const ElfW(Phdr) *loadableSeg,
					   int *segIndex)
{
	ElfW(Phdr) *phdr = elfinfo->progbase;
	ElfW(Phdr) *stAux = NULL;
	int i;
	/* Find the associated PT_ST_INFO segment */
	for (i = 0; i < elfinfo->header->e_phnum; i++) {
		if (phdr[i].p_type == PT_ST_INFO &&
			overlapping(loadableSeg->p_vaddr, loadableSeg->p_memsz,
				    phdr[i].p_vaddr,
				    (phdr[i].p_filesz & ~0x10000000))) {
			/* We've found the matching ST aux segment */
			stAux = &phdr[i];
			if (segIndex)
				*segIndex = i;
			break;
		}
	}
	return stAux;
}


/* Perform a checksum if the relevant checksum flag is present in stAux.
 * Checks can be media checks (of the stored ELF data) or normal checks of the
 * loaded data.
 * If useIO is true, the data pointer is an ioremapped address and for most
 * architectures should be read out to a kernel buffer before passing to a
 * checksum function.
 * Returns 0 if the checksum is okay (or flag not present).
 * Returns non-zero if the checksum fails.
 */
static int checksum(struct elf_st_aux *stAux, void *dataIn, uint32_t size,
		    bool media, bool useIO)
{
	int		retVal = 0;
	uint32_t	flagMask;
	void		*data = NULL;
#ifdef CONFIG_SUPERH
	/* On the ST40 we can use ioremapped addresses as kernel virtual
	 * addresses directly, so can ignore the useIO parameter and this code
	 * will work much faster.
	 * On the ARM we could sneakily manipulate the ioremapped address to
	 * get a kernel virtual address, but it might not be portable so we
	 * don't.
	 */
	useIO = false;
#endif /* CONFIG_SUPERH */
	if (!useIO)
		data = dataIn;

	if (media)
		flagMask = (ST_ELF_AUX_CRC32MED | ST_ELF_AUX_ADLER32MED);
	else
		flagMask = (ST_ELF_AUX_CRC32 | ST_ELF_AUX_ADLER32);

	if (stAux->flags & flagMask &
	    (ST_ELF_AUX_CRC32 | ST_ELF_AUX_CRC32MED)) {
		/* XOR with ~0 at start and end to match zlib implementation */
		u32 crc = crc32(0, NULL, 0) ^ ~0;
		if (useIO) {
			data = kmalloc(size, GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			memcpy_fromio(data, dataIn, size);
		}
		crc = crc32(crc, data, size) ^ ~0;
		if (crc != stAux->crcvalue)
			retVal = 1;
	} else if (stAux->flags & flagMask &
		   (ST_ELF_AUX_ADLER32 | ST_ELF_AUX_ADLER32MED)) {
		u32 crc = zlib_adler32(0, NULL, 0);
		if (useIO) {
			data = kmalloc(size, GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			memcpy_fromio(data, dataIn, size);
		}
		crc = zlib_adler32(crc, data, size);
		if (crc != stAux->crcvalue)
			retVal = 1;
	}
	if (useIO && data)
		kfree(data);
	return retVal;
}


/* Performs pre-load processing associated with PT_ST_INFO segments.
 * Returns non-zero for a failure, or 0 if loading may continue.
 */
static int stInfoPreLoadProc(const struct ELFW(info) *elfinfo,
			     const int auxSegIndex)
{
	ElfW(Phdr)		*phdr = elfinfo->progbase;
	void			*elfBase = elfinfo->base;
	struct elf_st_aux	*stAux = elfBase + phdr[auxSegIndex].p_offset;
	int			lIndex = -1;

	if (stAux->flags & (ST_ELF_AUX_CRC32MED | ST_ELF_AUX_ADLER32MED)) {
		/* Find the associated PT_LOAD segment */
		ElfW(Phdr) *loadable = findLoadableSegForSTAuxSeg(elfinfo,
				&phdr[auxSegIndex], &lIndex);
		if (!loadable) {
			pr_warning("libelf: PT_ST_INFO segment %d has no associated PT_LOAD segment\n",
				   auxSegIndex);
			return 0;
		}

		/* Perform media checksum (if flag set) */
		if (checksum(stAux, elfBase + loadable->p_offset,
			     loadable->p_filesz, true, false)) {
			pr_err("libelf: Checksum of segment %d failed (media check)\n",
			       lIndex);
			return 1;
		}
	}
	return 0;
}


/* Performs load-time processing associated with PT_ST_INFO segments.
 * Returns non-zero for a failure, or 0 if loading may continue.
 */
static int stInfoLoadProc(const struct ELFW(info) *elfinfo,
			  const int loadSegIndex, void __iomem *ioAddr)
{
	ElfW(Phdr)		*phdr = elfinfo->progbase;
	void			*elfBase = elfinfo->base;

	/* Find the PT_ST_INFO segment */
	ElfW(Phdr) *auxSeg = findSTAuxSegForLoadable(elfinfo,
						     &phdr[loadSegIndex], NULL);
	if (auxSeg) {
		/* PT_ST_INFO segment found */
		struct elf_st_aux *stAux = elfBase + auxSeg->p_offset;
		if (stAux->flags & (ST_ELF_AUX_CRC32 | ST_ELF_AUX_ADLER32)) {
			/* Perform checksum */
			if (checksum(stAux, ioAddr, phdr[loadSegIndex].p_memsz,
				     false, true)) {
				pr_err("libelf: Checksum of segment %d failed\n",
				       loadSegIndex);
				return 1;
			}
		}
	} else {
		pr_warning("libelf: Segment %d marked as having auxiliary info, but PT_ST_INFO segment not found\n",
			   loadSegIndex);
	}
	return 0;
}
#endif /* CONFIG_ST_ELF_EXTENSIONS */


/* Perform an ELF load to physical addresses, returning non-zero on failure,
 * else 0.
 * loadParams specifies checks to perform and restrictions on the load.
 * For successful loads:
 *  - the ELF entry address is passed back via entryAddr
 */
int ELFW(physLoad)(const struct ELFW(info) *elfInfo,
		   const struct ELFW(LoadParams) *loadParams,
		   uint32_t *entryAddr)
{
	ElfW(Phdr)	*phdr = elfInfo->progbase;
	void		*elfBase = elfInfo->base;
	unsigned int	loadedSegNum = 0;
	int		i;

#if defined(CONFIG_ST_ELF_EXTENSIONS)
	/* Pre-scan for interesting ST ELF extension segments which need
	 * processing.
	 */
	for (i = 0; i < elfInfo->header->e_phnum; i++) {
		if (phdr[i].p_type == PT_ST_INFO) {
			if (stInfoPreLoadProc(elfInfo, i))
				return 1;
		}
	}
#endif /* CONFIG_ST_ELF_EXTENSIONS */

	/* Normal loading */
	for (i = 0; i < elfInfo->header->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD) {
			unsigned long memSize, copySize, setSize;
			void __iomem *ioDestAddr;
			void *virtSrcAddr;
			bool loadFailure = false;
			bool mustFlush = false;
			bool newIOMapping = true;
			int res;

			memSize = phdr[i].p_memsz;

			/* Some ST200 Micro Toolset ELF files have a strange 0
			 * size segment as a result of a .note section.
			 */
			if (memSize == 0)
				continue;

			/* Check load range limits (from loadParams) */
			res = allowedRangeCheck(&phdr[i], i, loadParams);
			if (res)
				return res;

#if defined(CONFIG_ST_ELF_EXTENSIONS)
			if (phdr[i].p_flags & PF_ZLIB) {
				/* Zlib inflate to a kernel buffer... */
				int zlibResult;
				virtSrcAddr = vmalloc(memSize);
				if (!virtSrcAddr)
					return -ENOMEM;
				zlibResult = zlib_inflate_blob_with_header(
					virtSrcAddr, memSize,
					elfBase + phdr[i].p_offset,
					phdr[i].p_filesz);
				if (zlibResult != memSize) {
					kfree(virtSrcAddr);
					pr_err("libelf: Segment %d inflation failed (result: %d)\n",
					       i, zlibResult);
					return -EINVAL;
				}
				copySize = memSize;
				setSize = 0;
			} else
#endif /* defined(CONFIG_ST_ELF_EXTENSIONS) */
			{
				virtSrcAddr = elfBase + phdr[i].p_offset;
				copySize = phdr[i].p_filesz;
				setSize = memSize - phdr[i].p_filesz;
			}

			/* Can we use an existing mapping as provided by
			 * loadParams?
			 */
			if (loadParams->numExistingMappings) {
				ioDestAddr = findIOMapping(phdr[i].p_paddr,
							   memSize,
							   loadParams,
							   &mustFlush);
				if (ioDestAddr)
					newIOMapping = false;
			}

			if (newIOMapping) {
				/* Note: On ARMv6 or higher multiple mappings
				 * with different attributes are not allowed
				 * so this isn't good enough if a mapping
				 * already exists and we just were not told -
				 * drivers calling us must have recorded the
				 * existing mappings in the load parameters so
				 * this path is not used!
				 */
				if (loadParams->useCached) {
#ifdef CONFIG_SUPERH
					ioDestAddr = ioremap_cache(
								phdr[i].p_paddr,
								memSize);
					mustFlush = true;
#else /* !CONFIG_SUPERH */
					/* Cannot use ioremap_cached() on many
					 * architectures as there is no API to
					 * flush, but we can use write-combining
					 * as the next best thing.
					 */
					ioDestAddr = ioremap_wc(phdr[i].p_paddr,
								memSize);
#endif /* CONFIG_SUPERH */
				} else
					ioDestAddr = ioremap(phdr[i].p_paddr,
							     memSize);

				if (ioDestAddr == NULL) {
					if (virtSrcAddr !=
					    elfBase + phdr[i].p_offset)
						vfree(virtSrcAddr);
					pr_err("libelf: Segment %d ioremap of 0x%08x failed\n",
					       i,
					       (unsigned int)phdr[i].p_paddr);
					return -ENOMEM;
				}
			}

			memcpy_toio(ioDestAddr, virtSrcAddr, copySize);
			if (setSize)
				memset_io(ioDestAddr + copySize, 0, setSize);

			if (loadParams && loadParams->ptLoadCallback)
				loadParams->ptLoadCallback(
					loadedSegNum, loadParams->privData,
					phdr[i].p_paddr, virtSrcAddr, copySize,
					memSize);

#if defined(CONFIG_ST_ELF_EXTENSIONS)
			if (virtSrcAddr != elfBase + phdr[i].p_offset)
				vfree(virtSrcAddr);

			/* Perform any load-time processing defined by ST ELF
			 * extension segments associated with this loadable
			 * segment.
			 */
			if ((phdr[i].p_flags & PF_AUX) &&
			    stInfoLoadProc(elfInfo, i, ioDestAddr)) {
				loadFailure = true;
			}
#endif /* defined(CONFIG_ST_ELF_EXTENSIONS) */

			/* Flush the cache if we used a cached mapping */
#ifdef CONFIG_SUPERH
			if (mustFlush)
				flush_ioremap_region(phdr[i].p_paddr,
						     ioDestAddr, 0,
						     phdr[i].p_memsz);
#endif /* Architectures */
			if (newIOMapping)
				iounmap(ioDestAddr);

			if (loadFailure)
				return 1;

			loadedSegNum++;
		}
	}

	if (entryAddr)
		*entryAddr = elfInfo->header->e_entry;
	return 0;
}
EXPORT_SYMBOL(ELFW(physLoad));
