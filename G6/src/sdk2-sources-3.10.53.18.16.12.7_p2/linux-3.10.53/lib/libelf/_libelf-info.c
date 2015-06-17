/*
 * File     : lib/libelf/_libelf-info.c
 * Synopsis : Utility library for handling ELF files (info functions)
 * Author   : Carl Shaw <carl.shaw@st.com>
 * Author   : Giuseppe Condorelli <giuseppe.condorelli@st.com>
 * Contrib  : Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 * Copyright (c) 2008, 2011, 2012 STMicroelectronics Limited.
 *
 */

#include <linux/module.h>

/* Useful for debug, this function shows elf header informations */
void ELFW(printHeaderInfo)(const struct ELFW(info) *elfinfo)
{
	char *typestr[] = {"NONE", "REL", "EXEC", "DYN", "CORE"};
	char *osstr[] = {"NONE", "HPUX", "NETBSD", "Linux", "unknown",
			"unknown", "Solaris", "AIX", "IRIX", "FreeBSD",
			 "TRU64", "Modesto", "OpenBSD", "OpenVMS", "NSK"};

	if ((elfinfo->header)->e_type <= 4)
		printk(KERN_INFO"type       : %s\n",
				typestr[(elfinfo->header)->e_type]);
	else
		printk(KERN_INFO"type       : %u\n", (elfinfo->header)->e_type);
	printk(KERN_INFO"machine    : %u\n", (elfinfo->header)->e_machine);
#if __LIBELF_WORDSIZE == 32
	printk(KERN_INFO"entry      : 0x%08x\n", (elfinfo->header)->e_entry);
#else
	printk(KERN_INFO"entry      : 0x%08llx\n", (elfinfo->header)->e_entry);
#endif
	printk(KERN_INFO"flags      : %u\n", (elfinfo->header)->e_flags);
	printk(KERN_INFO"sections   : %u\n", elfinfo->numsections);
	printk(KERN_INFO"segments   : %u\n", elfinfo->numpheaders);
	printk(KERN_INFO"format     : %s %s\n",
		((elfinfo->header)->e_ident[EI_CLASS] == ELFCLASS32) ?
						"32 bit" : "64 bit",
		((elfinfo->header)->e_ident[EI_DATA] == ELFDATA2LSB) ?
					"little endian" : "big endian");

	if ((elfinfo->header)->e_ident[EI_OSABI] < 15)
		printk(KERN_INFO"OS	: %s\n",
			osstr[(elfinfo->header)->e_ident[EI_OSABI]]);
	else if ((elfinfo->header)->e_ident[EI_OSABI] == 97)
		printk(KERN_INFO"OS         : ARM\n");
	else if ((elfinfo->header)->e_ident[EI_OSABI] == 255)
		printk(KERN_INFO"OS         : STANDALONE\n");
}
EXPORT_SYMBOL(ELFW(printHeaderInfo));

/* Useful for debug, this function shows elf section informations */
void ELFW(printSectionInfo)(const struct ELFW(info) *elfinfo)
{
	uint32_t 	i, n;
	char 		*str, *type = NULL;
	ElfW(Shdr)	*sec;
	char		flags[10];
	struct typess	elftypes[] = {ELF_TYPES};

	printk(KERN_INFO"Number of sections: %u\n", elfinfo->numsections);
	for (i = 0; i < elfinfo->numsections; i++) {
		sec = ELFW(getSectionByIndex)(elfinfo, i);
		if (*str == '\0')
			str = "<no name>";

		str = elfinfo->strtab + sec->sh_name;

		for (n = 0; elftypes[n].name != NULL; n++) {
			if (elftypes[n].val == sec->sh_type)
				type = elftypes[n].name;
		}

		if (!type)
			type = "UNKNOWN";

		n = 0;
		if (sec->sh_flags & SHF_WRITE)
			flags[n++] = 'W';
		if (sec->sh_flags & SHF_ALLOC)
			flags[n++] = 'A';
		if (sec->sh_flags & SHF_EXECINSTR)
			flags[n++] = 'X';
		if (sec->sh_flags & SHF_MERGE)
			flags[n++] = 'M';
		if (sec->sh_flags & SHF_STRINGS)
			flags[n++] = 'S';
#if defined(CONFIG_ST_ELF_EXTENSIONS)
		if (sec->sh_flags & SHF_COMPRESSED)
			flags[n++] = 'Z';
#endif /* defined(CONFIG_ST_ELF_EXTENSIONS) */
		flags[n] = '\0';

		printk(KERN_INFO"[%02u] %s %s %s\n", i, str, type, flags);
	}
}
EXPORT_SYMBOL(ELFW(printSectionInfo));
