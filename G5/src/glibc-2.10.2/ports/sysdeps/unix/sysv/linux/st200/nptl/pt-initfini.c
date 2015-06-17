/* Special .init and .fini section support for ST200. LinuxThreads version.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

/* This file is compiled into assembly code which is then munged by a sed
   script into two files: crti.s and crtn.s.

   * crti.s puts a function prologue at the beginning of the
   .init and .fini sections and defines global symbols for
   those addresses, so they can be called as functions.

   * crtn.s puts the corresponding function epilogues
   in the .init and .fini sections. */

__asm__ ("\n\n"
"#include \"defs.h\"                                                        \n"
"/*define SHARED*/                                                          \n"
"#define sp $r12                                                            \n"
"#define gp $r14                                                            \n"
"                                                                           \n"
"/*@HEADER_ENDS*/                                                           \n"
"                                                                           \n"
"/*@TESTS_BEGIN*/                                                           \n"
"                                                                           \n"
"/*@TESTS_END*/                                                             \n"
"                                                                           \n"
"/*@_init_PROLOG_BEGINS*/                                                   \n"
"       .section .init, \"ax\", @progbits                                   \n"
"	.align 32                                                           \n"
"	.global _init                                                       \n"
"_init:                                                                     \n"
"       add     sp    = sp, -32                                             \n"
"       stw     -16[sp] = $r63                                              \n"
"       ;;                                                                  \n"
"       stw     20[sp] = gp                                                 \n"
"       call    $r63 = 0f                                                   \n"
"       ;;                                                                  \n"
"0:                                                                         \n"
"       add     gp = $r63, @neggprel(0b)                                    \n"
"       call    $r63 = __pthread_initialize_minimal                         \n"
"       ;;                                                                  \n"
"/*@_init_PROLOG_ENDS*/                                                     \n"
"                                                                           \n"
"/*@_init_EPILOG_BEGINS*/                                                   \n"
"       .section .init, \"ax\", @progbits                                   \n"
"	.align 4                                                            \n"
"       ldw     $r63 = 16[sp]                                               \n"
"       ;;                                                                  \n"
"       ldw     gp = 20[sp]                                                 \n"
"       add     sp     = sp, 32                                             \n"
"       ;;                                                                  \n"
"       return  $r63                                                        \n"
"       ;;                                                                  \n"
"/*@_init_EPILOG_ENDS*/                                                     \n"
"                                                                           \n"
"/*@_fini_PROLOG_BEGINS*/                                                   \n"
"	.section .fini, \"ax\", @progbits                                   \n"
"	.align 32                                                           \n"
"	.global _fini                                                       \n"
"_fini:                                                                     \n"
"       add     sp    = sp, -32                                             \n"
"       stw     -16[sp] = $r63                                              \n"
"       ;;                                                                  \n"
"       stw     20[sp] = gp                                                 \n"
"       call    $r63 = 0f                                                   \n"
"       ;;                                                                  \n"
"0:                                                                         \n"
"       add     gp = $r63, @neggprel(0b)                                    \n"
"       ;;                                                                  \n"
"/*@_fini_PROLOG_ENDS*/                                                     \n"
"	call    $r63 = i_am_not_a_leaf                                      \n"
"	;;                                                                  \n"
"                                                                           \n"
"/*@_fini_EPILOG_BEGINS*/                                                   \n"
"	.section .fini, \"ax\", @progbits                                   \n"
"	.align 4                                                            \n"
"       ldw     $r63 = 16[sp]                                               \n"
"       ;;                                                                  \n"
"       ldw     gp     = 20[sp]                                             \n"
"       add     sp     = sp, 32                                             \n"
"       ;;                                                                  \n"
"       return  $r63                                                        \n"
"       ;;                                                                  \n"
"                                                                           \n"
"/*@_fini_EPILOG_ENDS*/                                                     \n"
"                                                                           \n"
"/*@TRAILER_BEGINS*/                                                        \n"
"       .weak   __gmon_start__                                              \n"
);

