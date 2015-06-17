/**
 * Minus: a component library for the construction of operating systems.
 *        Compliant with the Fractal/Cecilia component model.
 *
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

/*
 *------------------------------------------------------------------------------
 * lowLoader.c
 *  implementation of the loader.api.lowLoader interface (initially for ST200)
 *------------------------------------------------------------------------------
 */

#include "elf.h"
#include "elfLoader.h"
#include "elfCopier.h"
#include "rt_host_def.h"
#include "hades_memory_map.h"
#include "p2012_config_mem.h"

int host_validate(char *file)
{
  int i;
  Elf64_Ehdr *elfHeader = (Elf64_Ehdr *) file;
  Elf64_Phdr *elfProgHeaders;
  //printf("ELF", "Validate elf file\n");

  /*
  if (elfHeader->e_machine != EM_ST200) {
    printf("Invalid target architecture\n");
    return -1;
  }
  */

  // TODO check core architecture and ABI

  // Checks that first loadable segment is at vaddr=0
  elfProgHeaders = (Elf64_Phdr *) &file[elfHeader->e_phoff];
  for (i=0; i<elfHeader->e_phnum; i++) {
    Elf64_Phdr *elfProgHeader = &elfProgHeaders[i];
    if (elfProgHeader->p_type != PT_LOAD) {
      continue;
    }
    /*
    if (elfProgHeader->p_vaddr != 0) {
      printf("First loadable segment is not at vaddr=0\n");
      return -2;
    }
    */
    break;
  }

  //printf("ELF", "Elf file validated\n");
  return 0;
}

//Code is kept as still compatible with static fw loading (will be re-enabled)
#if 0
// JLX: not used
char *host_loadSegments(char *file)
{
  unsigned int topMemory = 1; // set to 1 to ensure alignment
  unsigned char *progMemory;
  unsigned char *baseAddress = 0;
  int i, align;
  Elf64_Ehdr *elfHeader = (Elf64_Ehdr *) file;
  Elf64_Phdr *elfProgHeaders = (Elf64_Phdr *) &file[elfHeader->e_phoff];

  //printf("ELF", "Load segments\n");

  // compute program memory size
  for (i=0; i<elfHeader->e_phnum; i++) {
    Elf64_Phdr *segment = &elfProgHeaders[i];
    // If segment stay in memory
    if (segment->p_type != PT_LOAD) {
      continue;
    }

    if (topMemory < segment->p_vaddr) {
      topMemory = segment->p_vaddr;
    }

    align = segment->p_align;
    if (align > 1) {
      // If necessary, compute alignment
      topMemory = (topMemory + align - 1) & ~(align - 1);
    }
    topMemory += segment->p_memsz;
  }

  // allocate program memory
#if defined(ELF_DEBUG)
  pr_info("STHORM: Allocate program memory (size=%d=0x%X)\n", topMemory, topMemory);
#endif
  progMemory = malloc(topMemory);

  // compute base address
  for (i=0; i<elfHeader->e_phnum; i++) {
    Elf64_Phdr *segment = &elfProgHeaders[i];
    if (segment->p_type != PT_LOAD) continue;
    align = segment->p_align;
    if (align > 1) {
      // If necessary, compute alignment
      baseAddress = (unsigned char *) ((((unsigned int) progMemory) + align - 1)
                                       & ~(align - 1));
    } else {
      baseAddress = progMemory;
    }
    break;
  }

#if defined(ELF_DEBUG)
  printf("ELF", "Base load address 0x%08x\n", baseAddress);
#endif
  // copy program segments
  for (i=0; i<elfHeader->e_phnum; i++) {
    Elf64_Phdr *segment = &elfProgHeaders[i];
    unsigned char *segmentAddr;
    if (segment->p_type != PT_LOAD) {
      continue;
    }

    segmentAddr = baseAddress + segment->p_paddr;

#if defined(ELF_DEBUG)
    // sanity checks : validate alignment
    if ((((unsigned int) segmentAddr) & (segment->p_align - 1)) != 0) {
      printf("Wrong alignment for segment %d : paddr=0x%08x; memaddr=0x%08x; align=0x%x\n",
             i,
             (unsigned int) segment->p_paddr,
             (unsigned int) segmentAddr,
             segment->p_align);
      return 0;
    }
#endif

    // copy data from file
    memcpy(segmentAddr, &file[segment->p_offset], segment->p_filesz);
    // if necessary, add padding zero
    if (segment->p_filesz < segment->p_memsz) {
      memset(segmentAddr + segment->p_filesz, 0,
             segment->p_memsz - segment->p_filesz);
    }
  }

  return (char *) baseAddress;
}

static void update_range(int* seen, Elf64_Addr address, Elf64_Addr* lowest, Elf64_Addr* highest, Elf64_Xword size)
{
    if (! (*seen))
    {
        *lowest = address;
        *highest = address + size;
        *seen = 1;
    }
    else
    {
        if (address + size > *highest)
            *highest = address + size;
        if (address < *lowest)
            *lowest = address;
    }
}
#endif

void host_loadExecSegments(char *file, HostAddress* block)
{
  int i;
  Elf64_Ehdr *elfHeader = (Elf64_Ehdr *) file;
  Elf64_Phdr *elfProgHeaders = (Elf64_Phdr *) &file[elfHeader->e_phoff];

//Code is kept as still compatible with static fw loading (will be re-enabled)
#if 0
  Elf64_Addr lowest_text_addr = 0;
  Elf64_Addr highest_text_addr = 0;
  Elf64_Addr lowest_data_addr = 0;
  Elf64_Addr highest_data_addr = 0;
  int        seen_text_addr = 0;
  int        seen_data_addr = 0;


  printf("ELF", "Loading exec segments\n");

  // compute text & data locations and size
  for (i=0; i<elfHeader->e_phnum; i++) {
      Elf64_Phdr *segment = &elfProgHeaders[i];
      // If segment stay in memory
      if (segment->p_type != PT_LOAD) {
          continue;
      }

      if (segment->p_filesz == 0) {
          continue;
      }

      pr_info("STHORM: found segment in fabric firmware: base %#llx, length %d\n", segment->p_paddr, segment->p_memsz);

      if (segment->p_paddr >= HADES_L3_BASE)
          update_range(&seen_text_addr, segment->p_paddr, &lowest_text_addr, &highest_text_addr, segment->p_memsz);
      else
          update_range(&seen_data_addr, segment->p_paddr, &lowest_data_addr, &highest_data_addr, segment->p_memsz);
  }

  // Check text segments
  if (STHORM_BOOT_ADDR_L3 != lowest_text_addr)
  {
      pr_info("STHORM: fabric boot address %#lx is not at the base of the firmware ELF (%#llx-> %#llx) !\n", STHORM_BOOT_ADDR_L3, lowest_text_addr, highest_text_addr);
      return;
  }
  if (highest_text_addr - lowest_text_addr > HADES_BASE_FIRMWARE_SIZE)
  {
      pr_info("STHORM: fabric text segments in firmware ELF too big (%#llx > %#lx) !\n", highest_text_addr - lowest_text_addr, HADES_BASE_FIRMWARE_SIZE);
      return;
  }

  // Check data segments
  if (lowest_data_addr < P2012_CONF_FC_BASE)
  {
      pr_info("STHORM: the start address of the data segments in firmware ELF is %llx, before the fabric Ldata space %lx\n", lowest_data_addr, P2012_CONF_FC_BASE);
      return;
  }

  if (highest_data_addr > P2012_CONF_FC_BASE + P2012_CONF_FC_SIZE)
  {
      pr_info("STHORM: the end address of the data segments in firmware ELF is %llx, after the and of the fabric Ldata space %lx\n", highest_data_addr, P2012_CONF_FC_BASE + P2012_CONF_FC_SIZE);
      return;
  }
  pr_info("STHORM: fabric firmware segments: text [%llx->%llx], data[%llx->%llx]\n", lowest_text_addr, highest_text_addr, lowest_data_addr, highest_data_addr);
#endif

  // Use reserved BPA2 "Hades_firmware", first 32 MB, for the text segments
  if (!sthorm_map(block, (void*)HOST_BASE_FIRMWARE_ADDRESS, HADES_BASE_FIRMWARE_SIZE))
  {
      pr_err("Error: STHORM: unable to remap physical %p to virtual\n", HOST_FIRMWARE_ADDRESS);
      return;
  }
  //pr_info("STHORM: sthorm_map block: %p HOST_BASE_FIRMWARE_ADDRESS:%p HADES_BASE_FIRMWARE_SIZE:%d\n",
  //block, (void*)HOST_BASE_FIRMWARE_ADDRESS, HADES_BASE_FIRMWARE_SIZE);

  // transfer the text and data segments
  for (i=0; i<elfHeader->e_phnum; i++) {
    Elf64_Phdr *segment = &elfProgHeaders[i];
    // If segment stay in memory
    if (segment->p_type != PT_LOAD) {
      continue;
    }

    if (segment->p_filesz == 0) {
      continue;
    }

    // copy data from file
    if (segment->p_paddr >= HADES_L3_BASE)
      elfCopyText(segment->p_filesz, segment->p_memsz, block, segment->p_paddr - STHORM_BOOT_ADDR_L3, &file[segment->p_offset]);
    else
      elfCopyData(segment->p_filesz, segment->p_memsz, segment->p_paddr, &file[segment->p_offset]);
   }

  // Flush text block
  sthorm_flush(block);

  //printf("ELF", "File decoded\n");
}

int host_reloc(int type, char *reloc_addr,
            char *symbol_addr, int symbol_size)
{
/*
  switch(type) {
    case R_ST200_16:
#if defined(ELF_DEBUG)
      printf("Perform a R_ST200_16 relocation at 0x%08x, S+A=0x%04x\n",
             (unsigned int) reloc_addr,
             (unsigned short) symbol_addr);
#endif
      * ((unsigned short *) reloc_addr) = (unsigned short) ((unsigned int)symbol_addr & 0x0000ffff);
      break;

    case R_ST200_64:
    case R_ST200_JMP_SLOT:
    case R_ST200_REL64:
#if defined(ELF_DEBUG)
      printf("Perform a R_ST200_64/R_ST200_JMP_SLOT/R_ST200_REL64 relocation at 0x%08x, S+A=0x%08x\n",
             (unsigned int) reloc_addr,
             (unsigned int) symbol_addr);
#endif
      * ((unsigned int *) reloc_addr) = (unsigned int) symbol_addr;
      break;

    case R_ST200_COPY:
#if defined(ELF_DEBUG)
      printf("Perform a R_ST200_COPY relocation at 0x%08x, S=0x%08x, size=%d\n",
             (unsigned int) reloc_addr,
             (unsigned int) symbol_addr,
             symbol_size);
#endif
      memcpy(reloc_addr, symbol_addr, symbol_size);
      break;

    default:
      printf("Invalid relocation type %d\n", type);
      return -1;
  }
#if defined(ELF_DEBUG)
  printf("Relocation done\n");
#endif
*/
  return 0;
}

void host_end(char *P_addr)
{
/*
  unsigned int addr, i;

  printf("ELF", "[st200 LowLoader] Flush data cache\n");

  // Now that all the code is loaded, flush the caches
  // DCACHE : flush the 256 sets. Each set has 4 lines of 32 bytes
  addr = ((unsigned int)P_addr) & ~((1<<5)-1);
  for (i=0; i<256; i++)
    {
      asm volatile("prgset 0[%0]" : : "r" (addr));
      addr += 32;
    }

  // Ensure all the data are written into memory
  asm volatile("sync" : : );
  // ICACHE : just flush the whole cache
  asm volatile("prgins" : : );
  // Flush instruction buffer
  asm volatile("syncins" : : );

  flush_data_cache();
  printf("ELF", "Flush instr cache\n");
  flush_instruction_cache();
  printf("[st200 LowLoader] caches flushed\n");
*/
}
