/**
 * Minus: a component library for the construction of operating systems.
 *        Compliant with the Fractal/Cecilia component model.
 *
 * This file is derived from the Kortex library (think.objectweb.org).
 *
 * Copyright (C) 2004 France Telecom R&D
 * Copyright (C) 2007 STMicroelectronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Contact: think@objectweb.org
 *
 * Authors: Matthieu Leclercq (STMicroelectronics)
 *
 */

#include "elf.h"
#include "elfLoader.h"

#define NULL (0)

// ----------------------------------------------------------------------------
// Implementation of the ELFLoader interface
// ----------------------------------------------------------------------------

void host_loadELF(unsigned char* file, HostAddress* block)
{
  // int i,j,k;

  // Reference in the mapped file
  Elf64_Ehdr* header;
  //Elf64_Shdr* sections;

  // Base load address

  // Verify the ELF type of the file
  if (file[EI_MAG0] != ELFMAG0 ||
      file[EI_MAG1] != ELFMAG1 ||
      file[EI_MAG2] != ELFMAG2 ||
      file[EI_MAG3] != ELFMAG3 ||
      file[EI_CLASS] != ELFCLASS64) {
	  pr_err("Error: This is not a ELF 64bit file\n");
	  return;
  }

  // TODO checks the endianness !

  header = (Elf64_Ehdr*)file;
  switch (header->e_type)
  {
    case ET_NONE :
      pr_info("STHORM: UNKNOWN Elf Type file\n");
      pr_err("Error: STHORM: Type not yet handled\n");
      return;
    case ET_REL :
      pr_info("STHORM: Relocatable Elf Type file\n");
      pr_err("Error: STHORM: Type not yet handled\n");
      return;
    case ET_EXEC :
      //printf("Executable Elf Type file\n");
      // Validate ELF file by lowLoader
      if (host_validate((char*)file)) {
      	pr_err("Error: STHORM: firmware ELF file is not suitable\n");
        return;
      }
      //sections = (Elf64_Shdr*)&file[header->e_shoff];
      // Load segments in memory
      // printf("[loader.loadELF]Load segments in memory\n");
      host_loadExecSegments((char*)file, block);
      break;
    case ET_DYN :
      pr_info("STHORM: Dynamic Elf Type file\n");
      pr_err("Error: STHORM: Type not yet handled\n");
      return;
    case ET_CORE :
      pr_info("STHORM: Core Elf Type file\n");
      pr_err("Error: STHORM: Type not yet handled\n");
      return;
    default:
      pr_err("Error: STHORM: UNKNOWN Elf Type file\n");
      pr_err("Error: STHORM: Type not yet handled\n");
      return;
  }

  pr_info("STHORM: firmware program segments loaded at %p (%d bytes)\n", block->physical, block->size);

#if 0
  // host_reloc() is stubbed, so there is no point in computing offsets

  // Binaries relocation
  for(i=1;i < header->e_shnum; i++) {
    Elf64_Shdr* relocSec;
    Elf64_Shdr* symSec;
    Elf64_Sym* symtab;
    char* strtab;

    void* relstart;
    int size;

    // Does this section is a relocation table
    if(sections[i].sh_type != SHT_RELA &&
       sections[i].sh_type != SHT_REL) continue;

    relocSec = &sections[i];
    // Does the section being relocated is in memory
    /*
    if(! (sections[relocSec->sh_info].sh_flags & SHF_ALLOC))
      continue;
    */

    // Look for informations on the symbol table
    symSec = &sections[relocSec->sh_link];
    symtab = (Elf64_Sym *)&file[symSec->sh_offset];
    strtab = (char*) &file[sections[symSec->sh_link].sh_offset];

    relstart = &file[relocSec->sh_offset];
    if(sections[i].sh_type == SHT_RELA)
      size = sizeof(Elf64_Rela);
    else
      size = sizeof(Elf64_Rel);
    for (j = 0, k = 0; k < relocSec->sh_size; k += size, j++) {
    // for(j=0; j < relocSec->sh_size / size; j++) {
      Elf64_Sym* symbol = NULL;
      unsigned char* reloc_addr;
      unsigned int symbol_addr = 0;
      Elf64_Word symbol_size = 0;
      Elf64_Rel* relocElem = (Elf64_Rel*)((unsigned int)relstart + k);

      reloc_addr = (unsigned char*) (long)(*memoryStart + relocElem->r_offset);

      if (ELF64_R_SYM(relocElem->r_info) != 0) {
        symbol = &symtab[ELF64_R_SYM(relocElem->r_info)];
        symbol_size = symbol->st_size;
        switch(symbol->st_shndx) {
        case SHN_UNDEF:            // Extern reference
        case SHN_COMMON:           // Extern reference defined locally
        case SHN_ABS:              // ATTENTION What should we do?
          symbol_addr = 0;
          //printf("SHN_UNDEF, SHN_ABS or SHN_COMMON %s\n",
          //        &strtab[symbol->st_name]);
          break;
        default:                   // Internal reference in a loaded section
          symbol_addr = (unsigned int) symbol->st_value;
          break;
        }
      }
      if(relocSec->sh_type == SHT_RELA) {
        symbol_addr +=  ((Elf64_Rela*) relocElem)->r_addend;
      }
      symbol_addr += *memoryStart;

#if defined(ELF_DEBUG)
      if (symbol == NULL) {
	pr_info("STHORM: relocation at 0x%08x to 0x%08x\n",
               reloc_addr,
               symbol_addr);
      } else {
	pr_info("STHORM: relocation of '%s' at 0x%08x to 0x%08x\n",
               &strtab[symbol->st_name],
               reloc_addr,
               symbol_addr);
      }
#endif
      if(host_reloc(
               ELF64_R_TYPE(relocElem->r_info),
               (char*)reloc_addr,
               (char*)symbol_addr,
               (int) symbol_size)) {
	pr_info("STHORM: Can't handle reloc type %li at %p\n",
                 (long)ELF64_R_TYPE(relocElem->r_info),
                 reloc_addr);
      }
    }
  }

  host_end((char *)*memoryStart);

#endif // 0 (host_reloc & host_end are stubbed anyway
}

#if 0
// JLX: not used
unsigned int host_getEntry(unsigned char* file)
{
  Elf64_Ehdr* header = (Elf64_Ehdr*)file;
  return header->e_entry;
}

void *host_getSymbol(unsigned char* file, const char *name, unsigned int memoryStart)
{
  Elf64_Ehdr* header = (Elf64_Ehdr*)file;
  Elf64_Shdr* sections = (Elf64_Shdr*)&file[header->e_shoff];
  int i;

  for(i=1;i < header->e_shnum; i++) {
    Elf64_Sym *symbols;
    char *strtab;
    int nbSymbol, j;

    // printf("Section name=%d type=%d\n", sections[i].sh_name, sections[i].sh_type);
    if (sections[i].sh_type == SHT_DYNSYM) {

      symbols = (Elf64_Sym *)&file[sections[i].sh_offset];
      nbSymbol = sections[i].sh_size / sizeof(Elf64_Sym);
      strtab = (char*) &file[sections[sections[i].sh_link].sh_offset];

      for (j=sections[i].sh_info; j<nbSymbol; j++) {
        if (strcmp(name, &strtab[symbols[j].st_name]) == 0) {
          return (void *) (long)(memoryStart + symbols[j].st_value);
        }
      }
    }

    if (sections[i].sh_type == SHT_SYMTAB) {

      symbols = (Elf64_Sym *)&file[sections[i].sh_offset];
      nbSymbol = sections[i].sh_size / sizeof(Elf64_Sym);
      strtab = (char*) &file[sections[sections[i].sh_link].sh_offset];

      //printf("nbSymbol=%d, strtab=%X-%X\n", nbSymbol, &strtab, strtab);
      for (j=0; j<nbSymbol; j++) {
        // printf("j=%d, name=%s-%s\n", j, name, &strtab[symbols[j].st_name]);
        if (strcmp(name, &strtab[symbols[j].st_name]) == 0) {
          // printf("Program segments loaded at 0x%08x\n", memoryStart);
          // printf("symbols[j].st_value = 0x%08x\n", symbols[j].st_value);
          return (void *) (long)(symbols[j].st_value);
        }
      }
    }
  }
  return 0;
}
#endif
