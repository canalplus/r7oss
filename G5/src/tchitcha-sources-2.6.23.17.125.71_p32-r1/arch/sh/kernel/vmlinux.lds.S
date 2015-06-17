/* $Id: vmlinux.lds.S,v 1.8 2003/05/16 17:18:14 lethal Exp $
 * ld script to make SuperH Linux kernel
 * Written by Niibe Yutaka
 */
#include <asm/thread_info.h>
#include <asm/cache.h>
#include <asm-generic/vmlinux.lds.h>

#ifdef CONFIG_CPU_LITTLE_ENDIAN
OUTPUT_FORMAT("elf32-sh-linux", "elf32-sh-linux", "elf32-sh-linux")
#else
OUTPUT_FORMAT("elf32-shbig-linux", "elf32-shbig-linux", "elf32-shbig-linux")
#endif
OUTPUT_ARCH(sh)
ENTRY(_start)
SECTIONS
{
#ifdef CONFIG_32BIT
  . = CONFIG_PAGE_OFFSET + CONFIG_BOOT_LINK_OFFSET + CONFIG_ZERO_PAGE_OFFSET;
#else
  . = CONFIG_PAGE_OFFSET + CONFIG_MEMORY_START + CONFIG_ZERO_PAGE_OFFSET;
#endif
  _text = .;			/* Text and read-only data */
  text = .;			/* Text and read-only data */
  .empty_zero_page : {
	*(.empty_zero_page)
	} = 0
  .text : {
  	*(.text.head)
	TEXT_TEXT
	SCHED_TEXT
	LOCK_TEXT
#if defined(CONFIG_KPROBES)	/* KPROBE-SH */
	KPROBES_TEXT
#endif	/* KPROBE-SH */

	. = ALIGN(L1_CACHE_BYTES);
	__text_lpm_start = .;
	*(.text.lpm);
	. = ALIGN(L1_CACHE_BYTES);
	__text_lpm_end = .;

	*(.fixup)
	*(.gnu.warning)
	. = ALIGN(16);		/* Exception table align */
	} = 0x0009

  __start___ex_table = .;
  __ex_table : {
	*(__ex_table)
	LONG(0);
	. = ALIGN(64);
	}
  __stop___ex_table = .;

#ifdef CONFIG_RT73_WIRELESS_FRMW_BUILTIN
  . = ALIGN(PAGE_SIZE);

  __initrt73frmw_start = .;
  .init.wireless : { *(.init.wireless) }
  __initrt73frmw_end = .;
#endif
  _etext = .;			/* End of text section */

  RODATA 

  NOTES

  BUG_TABLE

  . = ALIGN(PAGE_SIZE);
  /* Code which muct be executed uncached AND the associated data */
  _start_uncached = .;
  .uncached.text : { *(.uncached) }
  .uncached.data : { *(.uncached.data) }
  _end_uncached = .;

  .data : {			/* Data */
	_sdata = .;
	DATA_DATA

 	 /* Align the initial ramdisk image (INITRD) on page boundaries. */
 	 . = ALIGN(PAGE_SIZE);
 	 __rd_start = .;
 	 *(.initrd)
 	 . = ALIGN(PAGE_SIZE);
 	 __rd_end = .;

	CONSTRUCTORS
	}

  . = ALIGN(PAGE_SIZE);
  .data.page_aligned : { *(.data.page_aligned) }
  __nosave_begin = .;
  .data_nosave : { *(.data.nosave) }
  . = ALIGN(PAGE_SIZE);
  __nosave_end = .;

  PERCPU(PAGE_SIZE)

  . = ALIGN(L1_CACHE_BYTES);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  _edata = .;			/* End of data section */

  . = ALIGN(THREAD_SIZE);		/* init_task */
  .data.init_task : { *(.data.init_task) }

  . = ALIGN(PAGE_SIZE);		/* Init code and data */
  __init_begin = .;
  _sinittext = .;
  .init.text : { 
	*(.init.text)
	LONG(0);
	. = ALIGN(64);
  }
  _einittext = .;
  .init.data : { *(.init.data) }
  . = ALIGN(16);
  __setup_start = .;
  .init.setup : { *(.init.setup) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : {
	INITCALLS
  }
  __initcall_end = .;
  __con_initcall_start = .;
  .con_initcall.init : { *(.con_initcall.init) }
  __con_initcall_end = .;
  SECURITY_INIT

  /* .exit.text is discarded at runtime, not link time, to deal with
     references from .rodata */
  .exit.text : { *(.exit.text) }
  .exit.data : { *(.exit.data) }

#ifdef CONFIG_BLK_DEV_INITRD
  . = ALIGN(PAGE_SIZE);

  __initramfs_start = .;
  .init.ramfs : { *(.init.ramfs) }
  __initramfs_end = .;
#endif

#ifdef CONFIG_DEBUG_MEMLEAK
 . = ALIGN(4);
  __memleak_offsets_start = .;
  .init.memleak : { *(.init.memleak_offsets) }
  __memleak_offsets_end = .;
#endif

 . = ALIGN(4);
  __machvec_start = .;
  .machvec.init : { *(.machvec.init) }
  __machvec_end = .;

  . = ALIGN(PAGE_SIZE);
  .bss : {
	__init_end = .;
	__bss_start = .;		/* BSS */
  	*(.bss.page_aligned)
  	*(.bss)
	. = ALIGN(4);
	_ebss = .;			/* uClinux MTD sucks */
	_end = . ;
  }

  /* When something in the kernel is NOT compiled as a module, the
   * module cleanup code and data are put into these segments.  Both
   * can then be thrown away, as cleanup code is never called unless
   * it's a module.
   */
  /DISCARD/ : {
	*(.exitcall.exit)
	}

  STABS_DEBUG

  DWARF_DEBUG
}
