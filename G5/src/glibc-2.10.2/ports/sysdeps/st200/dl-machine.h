/* Machine-dependent ELF dynamic relocation inline functions.  ST200 version.
   Copyright (C) 1995-1997, 2000-2002, 2003, 2009 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef dl_machine_h
#define dl_machine_h 1

#define ELF_MACHINE_NAME "ST200"

#include <assert.h>
#include <tls.h>

#define _ELF_LX_ABI_BIT	(0)                             /* 1st bit position in byte */
#define ELF_LX_ABI_MASK		(0x7<<_ELF_LX_ABI_BIT)           /* mask */
#define ELF_LX_ABI_NO		(0x0<<_ELF_LX_ABI_BIT)
#define ELF_LX_ABI_PIC		(0x3<<_ELF_LX_ABI_BIT)

#undef VALID_ELF_ABIVERSION
#define VALID_ELF_ABIVERSION(ver) \
  (((ver) & ELF_LX_ABI_MASK) == ELF_LX_ABI_NO || \
   ((ver) & ELF_LX_ABI_MASK) == ELF_LX_ABI_PIC)
#undef VALID_ELF_OSABI
#define VALID_ELF_OSABI(osabi) \
  ((osabi) == ELFOSABI_SYSV)
#undef VALID_ELF_HEADER
#define VALID_ELF_HEADER(hdr,exp,size) \
  memcmp (hdr,exp,size-2) == 0 \
  && VALID_ELF_OSABI (hdr[EI_OSABI]) \
  && VALID_ELF_ABIVERSION (hdr[EI_ABIVERSION])

/* Translate a processor specific dynamic tag to the index
   in l_info array.  */
#define DT_ST200(x) (DT_ST200_##x - DT_LOPROC + DT_NUM)

extern Elf32_Addr __st200_make_fptr (struct link_map *, const Elf32_Sym *,
				     Elf32_Addr);

/* An FDESC is a function descriptor.  */

struct st200_fdesc
  {
    Elf32_Addr ip;	/* code entry point */
    Elf32_Addr gp;	/* global pointer */
  };

/* Return nonzero iff ELF header is compatible with the running host.  */
static inline int __attribute__((unused))
elf_machine_matches_host (const Elf64_Ehdr *ehdr)
{
  return ehdr->e_machine == EM_LX;
}


/* Return the link-time address of _DYNAMIC.  */
static inline Elf32_Addr
elf_machine_dynamic (void)
{
  Elf32_Addr p;

  __asm__ (
	"	ldw %0 = @gotoff(_DYNAMIC)[$r14]\n"
	: "=r" (p));

  return p;
}


/* Return the run-time load address of the shared object.  */
static inline Elf32_Addr
elf_machine_load_address (void)
{
  Elf32_Addr ip;

  __asm__ (
           "      add  %0 = $r14, @gprel(_DYNAMIC)\n"
	   : "=r" (ip)
          );

  return ip - elf_machine_dynamic ();
}

/* Set up the loaded object described by L so its unrelocated PLT
   entries will jump to the on-demand fixup code in dl-runtime.c.  */

static inline int
elf_machine_runtime_setup (struct link_map *l, int lazy, int profile)
{
  extern void _dl_runtime_resolve (void);
  extern void _dl_runtime_profile (void);

  if (l->l_info[DT_JMPREL] && lazy)
    {
      register Elf32_Addr gp __asm__ ("r14");
      Elf32_Addr *got, doit;

       /* The GOT entries for functions in the PLT have not yet been filled
	 in.  Their initial contents will arrange when called to load an
	 offset into the .rela.plt section and _GLOBAL_OFFSET_TABLE_[1],
	 and then jump to _GLOBAL_OFFSET_TABLE[2].  */
      got = (Elf32_Addr *) D_PTR (l, l_info[DT_PLTGOT]);
      /* If a library is prelinked but we have to relocate anyway,
	 we have to be able to undo the prelinking of .got.plt.
	 The prelinker saved us here address of .plt + 0x20 (the
	 address of the first plt min entry).  */
      if (got[1])
	{
	  l->l_mach.plt = got[1] + l->l_addr;
	  l->l_mach.gotplt = (Elf32_Addr) &got[3];
	}

      /* Identify this shared object.  */
      got[0] = (Elf32_Addr) l;

      /* This function will be called to perform the relocation.  */
      if (!profile)
	{
	  doit = (Elf32_Addr) &_dl_runtime_resolve;
	}
      else
	{
	  doit = (Elf32_Addr) &_dl_runtime_profile;
	  /* Say that we really want profiling and the timers are started.  */
	  if (GLRO(dl_profile) != NULL
	      && _dl_name_match_p (GLRO(dl_profile), l))
	    GL(dl_profile_map) = l;
	}

      got[1] = doit;
      got[2] = gp;
    }

  return lazy;
}

/* Initial entry point code for the dynamic linker.
   The C function `_dl_start' is the real entry point;
   its return value is the user program's entry point.  */

/* When we arrive at _start, the program stack looks like this:
        argc            argument counter (integer)
        argv[0]         program name (pointer)
        argv[1...N]     program args (pointers)
        argv[argc-1]    end of args (integer)
	NULL
        env[0...N]      environment variables (pointers)
        NULL

	When we are done here, we want
	a1=argc
	a2=argv[0]
	a3=argv[argc+1]
*/

#ifndef RTLD_START_SPECIAL_INIT
#define RTLD_START_SPECIAL_INIT /* nothing */
#endif

#define USER_ENTRY_ADDR \
"       mov     $r0.63 = $r0.1                                             \n"
#define DL_FINI_ADDR \
"       ldw     $r0.16 = @gotoff(_dl_fini)[$r0.14]                         \n"\
"       ;;                                                                 \n"


#define RTLD_START __asm__ (						      \
".text                                                                     \n"\
"       .align 8                                                           \n"\
"	.global _start						           \n"\
"	.proc   						           \n"\
"_start:								   \n"\
"       add     $r0.12 = $r0.12, -32                                       \n"\
"       # Set up gp                                                        \n"\
"       call    $r0.63 = 0f                                                \n"\
"       ;;                                                                 \n"\
"       # We calculate gp using                                            \n"\
"       # gp = true_address(label 0) - gprel(label 0).                     \n"\
"       # Note that gprel(label 0) is a data - text calculation, so this   \n"\
"       # prevents us from moving the data seg relative to the text seg.   \n"\
"0:     add     $r0.14 = $r0.63, @neggprel(0b)                             \n"\
"       ;;                                                                 \n"\
"       # Call _dl_start(&argc)                                            \n"\
"       add     $r0.16 = $r0.12, 32                                        \n"\
"       call    $r0.63 = _dl_start                                         \n"\
"       ;;                                                                 \n"\
"       .endp                                                              \n"\
"       # Fall through                                                     \n"\
"       .global _dl_start_user                                             \n"\
"       .proc                                                              \n"\
"_dl_start_user:                                                           \n"\
"       # Save pointer to user entry point fdsc in $r0.1.                  \n"\
"       mov     $r0.1  = $r0.16                                            \n"\
"       # Store the highest stack address in __libc_stack_end.             \n"\
"       ldw     $r0.8  = @gotoff(__libc_stack_end)[$r0.14]                 \n"\
"       ;;                                                                 \n"\
"       add     $r0.9  = $r0.12, 32                                        \n"\
"       ;;                                                                 \n"\
"       stw     0[$r0.8] = $r0.9                                           \n"\
"       ;;                                                                 \n"\
"       # See if we were run as a command with the executable file name as \n"\
"       # an extra leading argument.                                       \n"\
"       ldw     $r0.8  = @gprel(_dl_skip_args)[$r0.14]                     \n"\
"       ;;                                                                 \n"\
"       # Get the original argument count.                                 \n"\
"       ldw     $r0.9  = 32[$r0.12]                                        \n"\
"       ;;                                                                 \n"\
"       add     $r0.10 = $r0.12, 36  # r10 = original argv on stack        \n"\
"       ;;                                                                 \n"\
"       # Here $r0.8 is the value of _dl_skip_args                         \n"\
"       # $r0.9 is the original argc                                       \n"\
"       # We need to                                                       \n"\
"       #    1. Overwrite argc on stack with (argc - _dl_skip_args)        \n"\
"       #    2. Move argv down on stack, to overwrite skipped args         \n"\
"       #       (cannot just adjust sp since we must keep sp aligned)      \n"\
"       #    3. Move envp down on stack, to keep it next to argv           \n"\
"       #    4. Update _dl_argv                                            \n"\
"       # Subtract _dl_skip_args from original argument count.             \n"\
"       sub     $r0.17 = $r0.9,  $r0.8                                     \n"\
"       sh2add  $r0.11 = $r0.8,  $r0.10  # r11 = argv we want to move      \n"\
"       ;;                                                                 \n"\
"       # Step 1: overwrite argc on stack                                  \n"\
"       stw     32[$r0.12] = $r0.17                                        \n"\
"       ;;                                                                 \n"\
"       ldw     $r0.9  = @gotoff(_dl_argv)[$r0.14]  # r9 = &_dl_argv       \n"\
"       ;;                                                                 \n"\
"       # Step 2: loop to move argv down the stack                         \n"\
"       # Note that we enter this loop even when _dl_skip_args is zero -   \n"\
"       # in that case we copy argv over itself.                           \n"\
"1:     ldw     $r0.16 = 0[$r0.11]                                         \n"\
"       ;;                                                                 \n"\
"       add     $r0.11 = $r0.11, 4                                         \n"\
"       ;;                                                                 \n"\
"       cmpne   $b0.0  = $r0.16, 0                                         \n"\
"       ;;                                                                 \n"\
"       stw     0[$r0.10] = $r0.16                                         \n"\
"       ;;                                                                 \n"\
"       add     $r0.10 = $r0.10, 4                                         \n"\
"       ;;                                                                 \n"\
"       br      $b0.0, 1b                                                  \n"\
"       ;;                                                                 \n"\
"       ldw     $r0.18 = 0[$r0.9]              # r18 = _dl_argv            \n"\
"       shl     $r0.8  = $r0.8, 2              # r8  = 4 * _dl_skip_args   \n"\
"       mov     $r0.19 = $r0.10                # r19 = updated &envp       \n"\
"       ;;                                                                 \n"\
"       # Step 3: loop to move envp down the stack                         \n"\
"1:     ldw     $r0.16 = 0[$r0.11]                                         \n"\
"       ;;                                                                 \n"\
"       add     $r0.11 = $r0.11, 4                                         \n"\
"       ;;                                                                 \n"\
"       cmpne   $b0.0  = $r0.16, 0                                         \n"\
"       ;;                                                                 \n"\
"       stw     0[$r0.10] = $r0.16                                         \n"\
"       ;;                                                                 \n"\
"       add     $r0.10 = $r0.10, 4                                         \n"\
"       ;;                                                                 \n"\
"       br      $b0.0, 1b                                                  \n"\
"       ;;                                                                 \n"\
"       sub  $r0.18 = $r0.18, $r0.8  # r18 = _dl_argv - (4 * _dl_skip_args)\n"\
"       ;;                                                                 \n"\
"       # Step 4: overwrite _dl_argv                                       \n"\
"       stw     0[$r0.9] = $r0.18                                          \n"\
"       ;;                                                                 \n"\
RTLD_START_SPECIAL_INIT                                                       \
"       # Now call _dl_init(_rtld_local, argc, argv, envp).                \n"\
"       ldw     $r0.16 = @gprel(_rtld_local)[$r0.14]                       \n"\
"       ;;                                                                 \n"\
"       # r17 already contains argc.                                       \n"\
"       add     $r0.18 = $r0.12, 36                                        \n"\
"       # r19 already contains &envp                                       \n"\
"       call    $r0.63 = _dl_init_internal                                 \n"\
"       ;;                                                                 \n"\
"       # Now call (*user_entry)(_dl_fini).                                \n"\
DL_FINI_ADDR                                                                  \
USER_ENTRY_ADDR                                                               \
"       add     $r0.12 = $r0.12, 32                                        \n"\
"       ;;                                                                 \n"\
"       goto    $r0.63                                                     \n"\
"       ;;                                                                 \n"\
"       .endp                                                              \n"\
"       .align 8                                                           \n"\
".previous                                                                 \n"\
);

/* ELF_RTYPE_CLASS_PLT iff TYPE describes relocation of a PLT entry or TLS
   variable, so undefined references should not be allowed to define the
   value.
   ELF_RTYPE_CLASS_NOCOPY iff TYPE should not be allowed to resolve to one
   of the main executable's symbols, as for a COPY reloc. */
#if !defined RTLD_BOOTSTRAP || USE___THREAD
#define elf_machine_type_class(type) \
  (((((type) == R_ST200_IPLT)           \
      | ((type) == R_ST200_JMP_SLOT)    \
      | ((type) == R_ST200_DTPMOD32)    \
      | ((type) == R_ST200_DTPREL32)    \
      | ((type) == R_ST200_TPREL32)) * ELF_RTYPE_CLASS_PLT)\
   | (((type) == R_ST200_COPY) * ELF_RTYPE_CLASS_COPY))
#else
#define elf_machine_type_class(type) \
  (((((type) == R_ST200_IPLT) | ((type) == R_ST200_JMP_SLOT)) * ELF_RTYPE_CLASS_PLT)\
   | (((type) == R_ST200_COPY) * ELF_RTYPE_CLASS_COPY))
#endif

/* A reloc type used for ld.so cmdline arg lookups to reject PLT entries.  */
#define ELF_MACHINE_JMP_SLOT	 (R_ST200_JMP_SLOT)

/* Rela is always used.  */
#define ELF_MACHINE_NO_REL 1

/* Fixup a PLT entry to bounce directly to the function at VALUE.  */
static inline Elf32_Addr __attribute__ ((always_inline))
elf_machine_fixup_plt (struct link_map *l, lookup_t t,
		       const Elf32_Rela *reloc,
		       Elf32_Addr *reloc_addr, Elf32_Addr value)
{
  /* l is the link_map for the caller, t is the link_map for the object
   * being called */
  /* got has already been relocated in elf_get_dynamic_info() */
  *reloc_addr = value;
  return value;
}

/* Return the final value of a plt relocation.  */
static inline Elf32_Addr
elf_machine_plt_value (struct link_map *map, const Elf32_Rela *reloc,
		       Elf32_Addr value)
{
  /* No need to handle rel vs rela since ST200 is rela only */
  return value + reloc->r_addend;
}

#endif /* !dl_machine_h */

/* Names of the architecture-specific auditing callback functions.  */
#define ARCH_LA_PLTENTER st200_gnu_pltenter
#define ARCH_LA_PLTEXIT st200_gnu_pltexit

#ifdef RESOLVE_MAP

#define R_ST200_TYPE(R)	 ((R) & -8)
#define R_ST200_FORMAT(R) ((R) & 7)

/* Perform the relocation specified by RELOC and SYM (which is fully
   resolved).  MAP is the object containing the reloc.  */
auto inline void
__attribute ((always_inline))
elf_machine_rela (struct link_map *map,
		  const Elf32_Rela *reloc,
		  const Elf32_Sym *sym,
		  const struct r_found_version *version,
		  void *const reloc_addr_arg)
{
  Elf32_Addr *const reloc_addr = reloc_addr_arg;
  const unsigned long int r_type = ELF32_R_TYPE (reloc->r_info);
  Elf32_Addr value;

#define COPY_UNALIGNED_WORD(swp, twp, align) \
  { \
    void *__s = (swp), *__t = (twp); \
    unsigned char *__s1 = __s, *__t1 = __t; \
    unsigned short *__s2 = __s, *__t2 = __t; \
    unsigned long *__s4 = __s, *__t4 = __t; \
    switch ((align)) \
    { \
    case 0: \
      *__t4 = *__s4; \
      break; \
    case 2: \
      *__t2++ = *__s2++; \
      *__t2 = *__s2; \
      break; \
    default: \
      *__t1++ = *__s1++; \
      *__t1++ = *__s1++; \
      *__t1++ = *__s1++; \
      *__t1 = *__s1; \
      break; \
    } \
  }

#if !defined RTLD_BOOTSTRAP && !defined HAVE_Z_COMBRELOC && !defined SHARED
  /* This is defined in rtld.c, but nowhere in the static libc.a; make the
     reference weak so static programs can still link.  This declaration
     cannot be done when compiling rtld.c (i.e.  #ifdef RTLD_BOOTSTRAP)
     because rtld.c contains the common defn for _dl_rtld_map, which is
     incompatible with a weak decl in the same file.  */
  weak_extern (_dl_rtld_map);
#endif

  /* We cannot use a switch here because we may not be able to locate the
     switch jump table until we've self-relocated (depends how the compiler
     code generates a switch).  */

#if !defined RTLD_BOOTSTRAP || !defined HAVE_Z_COMBRELOC
  if (__builtin_expect (r_type == R_ST200_REL32, 0))
    {
      value = reloc->r_addend;
# if !defined RTLD_BOOTSTRAP && !defined HAVE_Z_COMBRELOC
      /* Already done in dynamic linker.  */
      if (map != &GL(dl_rtld_map))
# endif
        value += map->l_addr;
    }
  else
#endif
    if (__builtin_expect (r_type == R_ST200_NONE, 0))
      return;
  else
    {
      const Elf32_Sym *const refsym = sym;
      struct link_map *sym_map = RESOLVE_MAP (&sym, version, r_type);

      /* RESOLVE_MAP() will return NULL if it fail to locate the symbol.  */

      value = sym_map == NULL ? 0 : sym_map->l_addr + sym->st_value;
      value += reloc->r_addend;

      if (r_type == R_ST200_32)
	;/* No adjustment.  */
      else if (r_type == ELF_MACHINE_JMP_SLOT)
	{
	  elf_machine_fixup_plt (NULL, sym_map, reloc, reloc_addr, value);
	  return;
	}
      else if (r_type == R_ST200_32_PCREL)
	value -= (Elf32_Addr) reloc_addr;
      else if (r_type == R_ST200_COPY)
	{
	  if (sym == NULL)
	    /* This can happen in trace mode if an object could not be	\
	       found. */
	    return;
	  if (sym->st_size > refsym->st_size
	      || (sym->st_size < refsym->st_size && GLRO(dl_verbose)))
	    {
	      const char *strtab;

	      strtab = (const char *) D_PTR (map, l_info[DT_STRTAB]);
	      _dl_error_printf ("\
%s: Symbol `%s' has different size in shared object, consider re-linking\n",
				rtld_progname ?: "<program name unknown>",
				strtab + refsym->st_name);
	    }
	  memcpy (reloc_addr, (void *) value, MIN (sym->st_size,
						   refsym->st_size));
	  return;
	}
#if !defined RTLD_BOOTSTRAP || defined USE___THREAD
      else if (r_type == R_ST200_DTPMOD32)
# ifdef RTLD_BOOTSTRAP
	/* During startup the dynamic linker is always index 1.  */
	value = 1;
# else
        /* Get the information from the link map returned by the
	   resolv function.  */
      value = (sym_map ? sym_map->l_tls_modid : 0);
# endif
      else if (r_type == R_ST200_DTPREL32)
	value -= (sym_map ? sym_map->l_addr : 0);
      else if (r_type == R_ST200_TPREL32)
	{
          if (sym_map)
            {
# ifndef RTLD_BOOTSTRAP
              CHECK_STATIC_TLS (map, sym_map);
# endif
              value += sym_map->l_tls_offset - sym_map->l_addr;
            }
	}
#endif
      else
	_dl_reloc_bad_type (map, r_type, 0);
    }

  COPY_UNALIGNED_WORD (&value, reloc_addr, (int) reloc_addr & 3);
}

/* Let do-rel.h know that on ST200 if l_addr is 0, all RELATIVE relocs
   can be skipped.  */
#define ELF_MACHINE_REL_RELATIVE 1

auto inline void
__attribute ((always_inline))
elf_machine_rela_relative (Elf32_Addr l_addr, const Elf32_Rela *reloc,
			   void *const reloc_addr_arg)
{
  Elf32_Addr value;

  assert (reloc->r_info == R_ST200_REL32);
  value = l_addr + reloc->r_addend;
  COPY_UNALIGNED_WORD (&value, reloc_addr_arg, (int) reloc_addr_arg & 3);

#undef COPY_UNALIGNED_WORD
}

/* Perform a RELATIVE reloc on the .got entry that transfers to the .plt.  */
auto inline void
__attribute ((always_inline))
elf_machine_lazy_rel (struct link_map *map,
		      Elf32_Addr l_addr, const Elf32_Rela *reloc)
{
  Elf32_Addr *const reloc_addr = (void *) (l_addr + reloc->r_offset);
  const unsigned long int r_type = ELF32_R_TYPE (reloc->r_info);

  if (__builtin_expect (r_type == ELF_MACHINE_JMP_SLOT, 1))
    {
      if (__builtin_expect (map->l_mach.plt, 0) == 0)
	reloc_addr[0] += l_addr;
      else
	reloc_addr[0] = (map->l_mach.plt
		       + (((Elf32_Addr) reloc_addr) - map->l_mach.gotplt) * 4);
    }
  else if (r_type == R_ST200_NONE)
    return;
  else
    _dl_reloc_bad_type (map, r_type, 1);
}

#endif /* RESOLVE_MAP */
