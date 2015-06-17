#include <linux/elf.h>
#include <uapi/linux/elf.h>

/*
 * We use this macro to refer to ELF types independent of the wordsize.
 * `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.
 */
#define ElfW(type)  _ElfW(Elf, __LIBELF_WORDSIZE, type)
#define _ElfW(e, w, t)    _ElfW_1(e, w, _##t)
#define _ElfW_1(e, w, t)  e##w##t


#define ELFW(func)  _ELFW(ELF, __LIBELF_WORDSIZE, func)
#define _ELFW(e, w, f)    _ELFW_1(e, w, _##f)
#define _ELFW_1(e, w, f)  e##w##f


#ifndef ELF_TYPES
/* Use ELF_TYPES as guard, as this file could be included twice if
 * both CONFIG_LIBELF_32 and CONFIG_LIBELF_64 are enabled */

struct typess {
	uint32_t	val;
	char 		*name;
};

#ifdef CONFIG_ST_ELF_EXTENSIONS
#define MORE_ELF_TYPES \
	{0x50000000, "ST_INFO"},
#else /* !CONFIG_ST_ELF_EXTENSIONS */
#define MORE_ELF_TYPES
#endif /* CONFIG_ST_ELF_EXTENSIONS */

#define ELF_TYPES	{0, "NULL"}, \
	{1, "PROGBITS"}, \
	{2, "SYMTAB"}, \
	{3, "STRTAB"}, \
	{4, "RELA"}, \
	{5, "HASH"}, \
	{6, "DYNAMIC"}, \
	{7, "NOTE"}, \
	{8, "NOBITS"}, \
	{9, "REL"}, \
	{10, "SHLIB"}, \
	{11, "DYNSYM"}, \
	{14, "INIT_ARRAY"}, \
	{15, "FINI_ARRAY"}, \
	{16, "PREINIT_ARRAY"}, \
	{17, "GROUP"}, \
	{18, "SYMTAB_SHNDX"}, \
	MORE_ELF_TYPES \
	{0x6ffffff6, "GNU_HASH"}, \
	{0x6ffffff7, "GNU_PRELINK_LIBLIST"}, \
	{0x6ffffff8, "CHECKSUM"}, \
	{0x6ffffffd, "GNU version definitions"}, \
	{0x6ffffffe, "GNU version needs"}, \
	{0x6fffffff, "GNU version symbol table"}, \
	{0xffffffff, NULL}


/* Default ELF load parameters */
#define ELF_LOADPARAMS_INIT	{NULL, NULL, 0, NULL, true, 0, NULL}
/* Macro to free some LoadParams */
#define ELF_LOADPARAMS_FREE(p)	do { \
					if ((p)->allowedRanges) \
						kfree((p)->allowedRanges); \
					if ((p)->existingMappings) \
						kfree((p)->existingMappings); \
				} while (0)

#endif /* ELF_TYPES */

struct ELFW(MemRange) {
	ElfW(Addr)	base;
	ElfW(Addr)	top;
};

struct ELFW(IORemapMapping) {
	ElfW(Addr)	physBase;
	void __iomem	*vIOBase;
	uint32_t	size;
	bool		cached;
};

/* Parameters for an ELF<xx>_physLoad() */
struct ELFW(LoadParams) {
	/* A callback called with the data of each PT_LOAD segment with data in
	 * it after it has been loaded.
	 */
	int (*ptLoadCallback)(unsigned int loadedSegNum, void *privData,
			      unsigned long pAddr, void *segData,
			      unsigned long dataSize, unsigned long fullSize);
	/* Private data passed to the callback function */
	void			*privData;
	/* The number of allowed load ranges in the following list */
	uint32_t		numAllowedRanges;
	struct ELFW(MemRange)	*allowedRanges;
	/* Attempt to use cached regions for new mappings if this architecture
	 * allows them.
	 */
	bool		useCached;
	/* A list of existing ioremap mappings which the driver would like us to
	 * use in preference to creating new mappings to perform the load.
	 */
	uint32_t		numExistingMappings;
	struct ELFW(IORemapMapping) *existingMappings;
};

struct ELFW(info) {
	uint8_t	*base;	/* Base address of ELF image in memory  */
	ElfW(Ehdr)	*header; /* Base address of ELF header in memory */
	uint32_t	size;	/* Total size of ELF data in bytes */
	uint32_t	mmapped;	/* Set to 1 if ELF file mmapped */
	ElfW(Shdr)	*secbase;	/* Section headers base address */
	ElfW(Phdr)	*progbase;	/* Program headers base address */
	char		*strtab;	/* String table for section headers */
	uint32_t	strtabsize;	/* Size of string table */
	uint32_t	strsecindex;	/* Section header index for strings */
	uint32_t	numsections;	/* Number of sections */
	uint32_t	numpheaders;	/* Number of program headers */
};

extern unsigned int ELFW(checkIdent)(ElfW(Ehdr) *);
extern struct ELFW(info) *ELFW(initFromMem)(uint8_t *, uint32_t, int);
extern uint32_t ELFW(free)(struct ELFW(info) *);
extern ElfW(Shdr) *ELFW(getSectionByIndex)(const struct ELFW(info) *, uint32_t);
extern ElfW(Shdr) *ELFW(getSectionByNameCheck)(const struct ELFW(info) *,
					const char *, uint32_t *, int, int);
extern void ELFW(printHeaderInfo)(const struct ELFW(info) *);
extern void ELFW(printSectionInfo)(const struct ELFW(info) *);
extern unsigned long ELFW(findBaseAddrCheck)(ElfW(Ehdr) *, ElfW(Shdr) *,
					unsigned long *, int, int);
extern int ELFW(searchSectionType)(const struct ELFW(info) *, const char *,
				int *);
extern unsigned long ELFW(checkPhMemSize)(const struct ELFW(info) *);
extern unsigned long ELFW(checkPhMinVaddr)(const struct ELFW(info) *);

static inline ElfW(Shdr) *ELFW(getSectionByName)(const struct ELFW(info) *info,
					const char *secname, uint32_t *index)
{
	return ELFW(getSectionByNameCheck)(info, secname, index,
						SHF_NULL, SHT_NULL);
}
static inline unsigned long ELFW(findBaseAddr)(ElfW(Ehdr) *hdr,
				ElfW(Shdr) *sechdrs, unsigned long *base)
{
	return ELFW(findBaseAddrCheck)(hdr, sechdrs, base, SHF_NULL, SHT_NULL);
}

/* Perform an ELF load to physical addresses, returning non-zero on failure,
 * else 0.
 * loadParams specifies checks to perform and restrictions on the load.
 * For successful loads:
 *  - the ELF entry address is passed back via entryAddr
 */
int ELFW(physLoad)(const struct ELFW(info) *elfinfo,
		   const struct ELFW(LoadParams) *loadParams,
		   uint32_t *entryAddr);
