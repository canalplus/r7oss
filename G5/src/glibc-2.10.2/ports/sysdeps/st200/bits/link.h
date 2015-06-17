/* Copyright (C) 2009 Free Software Foundation, Inc.
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

#ifndef	_LINK_H
# error "Never include <bits/link.h> directly; use <link.h> instead."
#endif


/* Registers for entry into PLT on st200.  */
typedef struct La_st200_regs
{
  uint32_t lr_r15; /* 0:  argument result pointer */
  uint32_t lr_r16; /* 4:  argument register */
  uint32_t lr_r17; /* 8:  argument register */
  uint32_t lr_r18; /* 12: argument register */
  uint32_t lr_r19; /* 16: argument register */
  uint32_t lr_r20; /* 20: argument register */
  uint32_t lr_r21; /* 24: argument register */
  uint32_t lr_r22; /* 28: argument register */
  uint32_t lr_r23; /* 32: argument register */
  uint32_t lr_r63; /* 36: return address */
  uint32_t lr_r12;
} La_st200_regs;

/* Return values for calls from PLT on st200.  */
typedef struct La_st200_retval
{
  uint32_t lrv_r16; /* result register */
  uint32_t lrv_r17; /* result register */
  uint32_t lrv_r18; /* result register */
  uint32_t lrv_r19; /* result register */
  uint32_t lrv_r20; /* result register */
  uint32_t lrv_r21; /* result register */
  uint32_t lrv_r22; /* result register */
  uint32_t lrv_r23; /* result register */
} La_st200_retval;


__BEGIN_DECLS

extern Elf32_Addr la_st200_gnu_pltenter (Elf32_Sym *__sym, unsigned int __ndx,
				         uintptr_t *__refcook,
				         uintptr_t *__defcook,
				         La_st200_regs *__regs,
				         unsigned int *__flags,
				         const char *__symname,
				         long int *__framesizep);
extern unsigned int la_st200_gnu_pltexit (Elf32_Sym *__sym, unsigned int __ndx,
					  uintptr_t *__refcook,
					  uintptr_t *__defcook,
					  const La_st200_regs *__inregs,
					  La_st200_retval *__outregs,
					  const char *symname);

__END_DECLS
