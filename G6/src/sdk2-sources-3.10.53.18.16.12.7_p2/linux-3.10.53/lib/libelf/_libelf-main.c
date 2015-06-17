/*
 * File     : lib/libelf/_libelf-main.c
 * Synopsis : Utility library for handling ELF files
 * Author   : Carl Shaw <carl.shaw@st.com>
 * Author   : Giuseppe Condorelli <giuseppe.condorelli@st.com>
 * Contrib  : Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 * Copyright (c) 2008, 2011 STMicroelectronics Limited.
 *
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/slab.h>
#ifdef CONFIG_ST_ELF_EXTENSIONS
#include <linux/zlib.h>
#include <linux/zutil.h>
#endif /* CONFIG_ST_ELF_EXTENSIONS */

#define ELF_CHECK_FLAG(x)	({x ? x : ~SHF_NULL; })
#define ELF_CHECK_TYPE(x)	({x ? x : ~SHT_NULL; })

#define __elfclass_concat1(class, size)	class##size
#define __elfclass_concat(class, size)	__elfclass_concat1(class, size)
#define LIBELF_ELFCLASS	__elfclass_concat(ELFCLASS, __LIBELF_WORDSIZE)

/* Check elf file identity */
unsigned int ELFW(checkIdent)(ElfW(Ehdr) *hdr)
{
	return memcmp(hdr->e_ident, ELFMAG, SELFMAG);
}
EXPORT_SYMBOL(ELFW(checkIdent));

static inline int ELFW(valid_offset)(struct ELFW(info) *elfinfo, ElfW(Off) off,
	ElfW(Off) struct_size)
{
	return off + struct_size <= elfinfo->size;
}

/* Initialise in-memory ELF file */
struct ELFW(info) *ELFW(initFromMem)(uint8_t *elffile,
				uint32_t size, int mmapped)
{
	ElfW(Shdr)	*sec;
	struct ELFW(info) *elfinfo;
	int i;
	elfinfo = (struct ELFW(info) *)kmalloc(sizeof(struct ELFW(info)),
				GFP_KERNEL);

	if (elfinfo == NULL)
		return NULL;

	elfinfo->mmapped = mmapped;
	elfinfo->size = size;

	elfinfo->base = (uint8_t *)elffile;
	elfinfo->header = (ElfW(Ehdr) *)elffile;

	/* Check that image is really an ELF file */

	if (size < sizeof(ElfW(Ehdr)))
		goto fail;

	if (ELFW(checkIdent)((ElfW(Ehdr) *)elffile))
		goto fail;

	/* Make sure it is 32 or 64 bit, little endian and current version */
	if (elffile[EI_CLASS] != LIBELF_ELFCLASS ||
		elffile[EI_DATA] != ELFDATA2LSB ||
		elffile[EI_VERSION] != EV_CURRENT)
		goto fail;

	/* Make sure we are a data file, relocatable or executable file */
	if ((elfinfo->header)->e_type > 3) {
		printk(KERN_ERR"Invalid ELF object type\n");
		goto fail;
	}

	/* Check the structure sizes are what we expect */
	if ((elfinfo->header->e_ehsize != sizeof(ElfW(Ehdr))) ||
	    (elfinfo->header->e_phentsize != sizeof(ElfW(Phdr))) ||
	    (elfinfo->header->e_shentsize != sizeof(ElfW(Shdr))))
		goto fail;

	/* Get number of sections */

	if ((elfinfo->header)->e_shnum != 0) {
		/* Normal numbering */
		elfinfo->numsections = (elfinfo->header)->e_shnum;
	} else {
		/* Extended numbering */
		elfinfo->numsections = (elfinfo->secbase)->sh_size;
	}

	/* Get number of program headers */

	if ((elfinfo->header)->e_phnum != 0xffff) { /* PN_XNUM */
		/* Normal numbering */
		elfinfo->numpheaders = (elfinfo->header)->e_phnum;
	} else {
		/* Extended numbering */
		elfinfo->numpheaders = (elfinfo->secbase)->sh_info;
	}

	/* Validate header offsets and sizes */
	if (!ELFW(valid_offset)(elfinfo, elfinfo->header->e_shoff,
			      sizeof(ElfW(Shdr)) * elfinfo->numsections) ||
	    !ELFW(valid_offset)(elfinfo, elfinfo->header->e_phoff,
			      sizeof(ElfW(Phdr)) * elfinfo->numpheaders))
		goto fail;

	/* Cache commonly-used addresses and values */
	elfinfo->secbase = (ElfW(Shdr) *)(elfinfo->base +
				(elfinfo->header)->e_shoff);
	elfinfo->progbase = (ElfW(Phdr) *)(elfinfo->base +
				(elfinfo->header)->e_phoff);

	/* Validate section headers */
	for (i = 0; i < elfinfo->numsections; i++) {
		ElfW(Shdr) *shdr;
		shdr = &elfinfo->secbase[i];
		if (shdr->sh_type != SHT_NULL && shdr->sh_type != SHT_NOBITS &&
#ifdef CONFIG_ST_ELF_EXTENSIONS
		    !(shdr->sh_flags & SHF_COMPRESSED) &&
#endif /* CONFIG_ST_ELF_EXTENSIONS */
		    !ELFW(valid_offset)(elfinfo, shdr->sh_offset, shdr->sh_size)
		   )
			goto fail;
	}

	/* Validate program headers */
	for (i = 0; i < elfinfo->numpheaders; i++) {
		ElfW(Phdr) *phdr;
		phdr = &elfinfo->progbase[i];
		if (!ELFW(valid_offset)(elfinfo, phdr->p_offset,
				      phdr->p_filesz))
			goto fail;
		if (phdr->p_filesz > phdr->p_memsz)
			goto fail;
	}

	/* Get address of string table */

	if ((elfinfo->header)->e_shstrndx != SHN_HIRESERVE) {
		/* Normal numbering */
		elfinfo->strsecindex = (elfinfo->header)->e_shstrndx;
	} else {
		/* Extended numbering */
		elfinfo->strsecindex = (elfinfo->secbase)->sh_link;
	}

	if (elfinfo->strsecindex >= elfinfo->numsections)
		goto fail;

	sec = &elfinfo->secbase[elfinfo->strsecindex];
	elfinfo->strtab  = (char *)(elfinfo->base + sec->sh_offset);
	elfinfo->strtabsize = sec->sh_size;

	return elfinfo;

fail:
	kfree(elfinfo);
	return NULL;
}
EXPORT_SYMBOL(ELFW(initFromMem));

/* Free up memory-based resources */
uint32_t ELFW(free)(struct ELFW(info) *elfinfo)
{
	kfree((void *)elfinfo);

	return 0;
}
EXPORT_SYMBOL(ELFW(free));

ElfW(Shdr) *ELFW(getSectionByIndex)(const struct ELFW(info) *elfinfo,
				uint32_t index)
{
	return (ElfW(Shdr) *)((uint8_t *)(elfinfo->secbase) +
				((elfinfo->header)->e_shentsize * index));
}
EXPORT_SYMBOL(ELFW(getSectionByIndex));

/*
 * Search for section starting from its name. Also shflag and shtype are given
 * to restrict search for those sections matching them.
 * No flags check will be performed if SHF_NULL and SHT_NULL will be given.
 */
ElfW(Shdr) *ELFW(getSectionByNameCheck)(const struct ELFW(info) *elfinfo,
				 const char *secname,
				 uint32_t *index, int shflag, int shtype)
{
	uint32_t 	i;
	char 		*str;
	ElfW(Shdr)	*sec;

	if (index)
		*index = 0;

	for (i = 0; i < elfinfo->numsections; i++) {
		if ((elfinfo->secbase[i].sh_flags & ELF_CHECK_FLAG(shflag)) &&
		(elfinfo->secbase[i].sh_type & ELF_CHECK_TYPE(shtype))) {
			sec = ELFW(getSectionByIndex)(elfinfo, i);
			str = elfinfo->strtab + sec->sh_name;
			if (strcmp(secname, str) == 0) {
				if (index)
					*index = i;
				return sec;
			}
		}
	}

	return NULL;
}
EXPORT_SYMBOL(ELFW(getSectionByNameCheck));

unsigned long ELFW(findBaseAddrCheck)(ElfW(Ehdr) *hdr, ElfW(Shdr) *sechdrs,
				unsigned long *base, int shflag, int shtype)
{
	unsigned int i;
	int prev_index = 0;

	for (i = 1; i < hdr->e_shnum; i++)
		if ((sechdrs[i].sh_flags & ELF_CHECK_FLAG(shflag))
			&& (sechdrs[i].sh_type & ELF_CHECK_TYPE(shtype))) {
			if (sechdrs[i].sh_addr < *base)
				*base = sechdrs[i].sh_addr;
			prev_index = i;
		}
	return prev_index;
}
EXPORT_SYMBOL(ELFW(findBaseAddrCheck));

/*
 * Check if the given section is present in the elf file. This function
 * also returns the index where section was found, if it was.
 */
int ELFW(searchSectionType)(const struct ELFW(info) *elfinfo, const char *name,
				int *index)
{
	uint32_t	i, n;
	ElfW(Shdr)	*sec;
	struct typess   elftypes[] = {ELF_TYPES};

	for (i = 0; i < elfinfo->numsections; i++) {
		sec = ELFW(getSectionByIndex)(elfinfo, i);
		for (n = 0; elftypes[n].name != NULL; n++)
			if ((strcmp(elftypes[n].name, name) == 0) &&
				(elftypes[n].val == sec->sh_type)) {
					if (index != NULL)
						*index = i;
					return 0;
			}
	}
	return 1;
}
EXPORT_SYMBOL(ELFW(searchSectionType));
