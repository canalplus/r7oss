/* PLT trampolines.  st200 version.
   Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <sysdep.h>

/*
   This code is used in dl-runtime.c to call the `_dl_fixup' function
   and then redirect to the address it returns. `_dl_fixup()' takes two
   arguments, however _dl_profile_fixup() takes five.
*/

ENTRY(_dl_runtime_resolve)
       add  $r12     = $r12, -48 /* We use this plus the 16 byte area in callers frame */
       stw  (48-48)[$r12] = $r23 /* Uses pre-adjustment SP value */
       ;;
       stw  16[$r12] = $r15
       ;;
       stw  20[$r12] = $r16
       ;;
       stw  24[$r12] = $r17
       ;;
       stw  28[$r12] = $r18
       ;;
       stw  32[$r12] = $r19
       ;;
       stw  36[$r12] = $r20
       ;;
       stw  40[$r12] = $r21
       ;;
       stw  44[$r12] = $r22
       ;;
       stw  52[$r12] = $r63
       ;;
 /* I do not know why we need to save r14 here, if we did not save it we
    could also reduce the stack frame size from 48 to 40. */
       stw  56[$r12] = $r14
       mov  $r16     = $r10
       mov  $r17     = $r9
       call $r63     = _dl_fixup
       ;;
       mov  $r63     = $r16
       ldw  $r8      = 52[$r12]
       ;;
       ldw  $r15     = 16[$r12]
       ;;
       ldw  $r16     = 20[$r12]
       ;;
       ldw  $r17     = 24[$r12]
       ;;
       ldw  $r18     = 28[$r12]
       ;;
       ldw  $r19     = 32[$r12]
       ;;
       ldw  $r20     = 36[$r12]
       ;;
       ldw  $r21     = 40[$r12]
       ;;
       ldw  $r22     = 44[$r12]
       ;;
       ldw  $r23     = 48[$r12]
       ;;
       ldw  $r14     = 56[$r12]
       goto $r63
       mov  $r63     = $r8
       add  $r12     = $r12, +48
       ;;
END(_dl_runtime_resolve)

/* The fourth argument to _dl_profile_fixup and the third one to
   _dl_call_pltexit are a pointer to
   La_st200_regs:
      +0:	 4byte r15
      +4:	 4byte r16
      +8:	 4byte r17
      +12:	 4byte r18
      +16:	 4byte r19
      +20:	 4byte r20
      +24:	 4byte r21
      +28:	 4byte r22
      +32:	 4byte r23
      +36:	 4byte r63
      +40:	 4byte r12

   The fifth argument to _dl_profile_fixup is a pointer to long int.
   The fourth argument to _dl_call_pltexit is a pointer to
   La_st200_retval:
      +0:	4byte r16
      +4:	4byte r17
      +8:	4byte r18
      +12:	4byte r19
      +16:	4byte r20
      +20:	4byte r21
      +24:	4byte r22
      +28:	4byte r23

  Since stack has to be 8 byte aligned, the stack allocation is in
  8byte increment. Before calling _dl_profile_fixup, the stack will
  look like

  sp+80:      caller stack args
  sp+72:      saved reloc_arg
  sp+68:      saved link_map
  sp+64:      saved $r2
  sp+60:      new_frame_size (passed by ref. to _dl_profile_fixup)
  sp+16:      La_st200_regs (44 bytes)
  sp:         scratch (16 bytes)

  Note the caller's sp is at sp+64, so [sp+64:sp+79] overlays the
  caller's scratch area.
*/
#define PLTENTER_FRAME_SIZE 64
#define PLTENTER_LA_ST200_REGS 16
#define PLTENTER_NEW_FRAME_SIZE 60
#define PLTENTER_SAVED_R2 64
#define PLTENTER_SAVED_LINK_MAP 68
#define PLTENTER_SAVED_RELOC_ARG 72
#define PLTENTER_CALLER_STACK_ARGS 80
/*
  The arguments to _dl_profile_fixup are:
r16:	link_map *
r17:	reloc_arg
r18:	retaddr
r19:	pointer to La_st200_regs
r20:	pointer to new_frame_size

 Before calling _dl_call_pltexit, we extend the stack frame to also
 include an Ls_st200_retval, i.e. the stack will look like

sp+112:	caller stack args
sp+104: saved reloc_arg
sp+100: saved link_map
sp+96:	saved $r2
sp+92:	new_frame_size
sp+48:	La_st200_regs (44 bytes)
sp+16:	La_st200_retval (32 bytes)
sp:	scratch (16 bytes)

  Note the caller's sp is at sp+96, so [sp+96:sp+111] overlays the
  caller's scratch area.
*/
#define SIZEOF_LA_ST200_RETVAL 32
#define PLTEXIT_EXTRA_FRAME SIZEOF_LA_ST200_RETVAL /* Must be multiple of 8 */
#define PLTEXIT_FRAME_SIZE (PLTENTER_FRAME_SIZE + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_LA_ST200_RETVAL 16
#define PLTEXIT_LA_ST200_REGS (PLTENTER_LA_ST200_REGS + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_NEW_FRAME_SIZE (PLTENTER_NEW_FRAME_SIZE + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_SAVED_R2 (PLTENTER_SAVED_R2 + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_SAVED_LINK_MAP (PLTENTER_SAVED_LINK_MAP + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_SAVED_RELOC_ARG (PLTENTER_SAVED_RELOC_ARG + PLTEXIT_EXTRA_FRAME)
#define PLTEXIT_CALLER_STACK_ARGS (PLTENTER_CALLER_STACK_ARGS + PLTEXIT_EXTRA_FRAME)
/*
 The arguments to _dl_call_pltexit are:
r16:	link_map *
r17:	reloc_arg
r18:	pointer to La_st200_regs
r19:	pointer to La_st200_retval
 */

#ifndef PROF
ENTRY(_dl_runtime_profile)
       add  $r12     = $r12, -PLTENTER_FRAME_SIZE
       stw  (PLTENTER_SAVED_R2-PLTENTER_FRAME_SIZE)[$r12]  = $r2 /* use pre-adjusted sp value */
       ;;
       add  $r2      = $r12, PLTENTER_FRAME_SIZE  /* save caller_sp value in $r2 */
       stw  PLTENTER_SAVED_LINK_MAP[$r12] = $r10
       ;;
       stw  PLTENTER_SAVED_RELOC_ARG[$r12] = $r9
       ;;
       stw  (PLTENTER_LA_ST200_REGS+0)[$r12] = $r15
       ;;
       stw  (PLTENTER_LA_ST200_REGS+4)[$r12] = $r16
       ;;
       stw  (PLTENTER_LA_ST200_REGS+8)[$r12] = $r17
       ;;
       stw  (PLTENTER_LA_ST200_REGS+12)[$r12] = $r18
       ;;
       stw  (PLTENTER_LA_ST200_REGS+16)[$r12] = $r19
       ;;
       stw  (PLTENTER_LA_ST200_REGS+20)[$r12] = $r20
       ;;
       stw  (PLTENTER_LA_ST200_REGS+24)[$r12] = $r21
       ;;
       stw  (PLTENTER_LA_ST200_REGS+28)[$r12] = $r22
       ;;
       stw  (PLTENTER_LA_ST200_REGS+32)[$r12] = $r23
       ;;
       stw  (PLTENTER_LA_ST200_REGS+36)[$r12] = $r63
       mov  $r16     = $r10
       mov  $r17     = $r9
       mov  $r18     = $r63
       ;;
       stw  (PLTENTER_LA_ST200_REGS+40)[$r12] = $r2     /* save caller_sp */
       add  $r19     = $r12, PLTENTER_LA_ST200_REGS
       add  $r20     = $r12, PLTENTER_NEW_FRAME_SIZE
       call $r63     = _dl_profile_fixup
       ;;
       mov  $r63     = $r16      /* Address to call */
       ldw $r10      = PLTENTER_NEW_FRAME_SIZE[$r12]   /* new_frame_size */
       ;;
       ldw  $r15     = (PLTENTER_LA_ST200_REGS+0)[$r12]
       ;;
       ldw  $r16     = (PLTENTER_LA_ST200_REGS+4)[$r12]
       ;;
       ldw  $r17     = (PLTENTER_LA_ST200_REGS+8)[$r12]
       cmpge $b0     = $r10, 0
       cmpeq $b1     = $r10, 0
       ;;
       ldw  $r18     = (PLTENTER_LA_ST200_REGS+12)[$r12]
       ;;
       ldw  $r19     = (PLTENTER_LA_ST200_REGS+16)[$r12]
       ;;
       ldw  $r20     = (PLTENTER_LA_ST200_REGS+20)[$r12]
       ;;
       ldw  $r21     = (PLTENTER_LA_ST200_REGS+24)[$r12]
       ;;
       ldw  $r22     = (PLTENTER_LA_ST200_REGS+28)[$r12]
       ;;
       ldw  $r23     = (PLTENTER_LA_ST200_REGS+32)[$r12]
       br   $b0,     .Lneed_pltexit
       ;;
       /* No need to call pltexit, just call the function */
       ldw $r24 = (PLTENTER_LA_ST200_REGS+36-PLTENTER_FRAME_SIZE)[$r2]
       mov $r12 = $r2
       ;;
       ldw $r2  = (PLTENTER_SAVED_R2-PLTENTER_FRAME_SIZE)[$r2]
       goto $r63
       mov  $r63 = $r24
       ;;
.Lneed_pltexit:
       br   $b1,     .Lno_new_frame /* Must call pltexit, but no need to copy caller frame args */
       ;;
       /* _dl_profile_fixup requested a new frame size.
	  We need to keep the current stack, but we need to call the
	  resolved function with r10 bytes of stack arguments.
	  But the stack arguments are on the stack at sp+64.
	  So we need to copy the callers stack arguments to the bottom
	  of the stack, i.e.
	     r10 = (r10 + 7) & 8  (round up to multiple of 8)
	     old_sp = sp
	     sp -= r10
	     memcpy (sp + 16, old_sp+64, r10)
	*/
       add  $r10     = $r10, 7
       ;;
       and  $r10     = $r10, 8
       add  $r24     = $r12, 16
       ;;
       add  $r11     = $r12, PLTENTER_CALLER_STACK_ARGS   /* src: ptr to caller stack frame args */
       sub  $r12     = $r12, $r10 /* extend frame to include copy of caller args */
       sub  $r24     = $r12, $r10 /* dst: ptr to copy of caller stack frame args */
       ;;
      /* Copy caller stack frame args from their original position to new
	 position at top of stack */
.Lcopy:
       ldw  $r1      = 0[$r11]
       add  $r11     = $r11, 4
       cmpgt $b0     = $r10, 4
       ;;
       stw  0[$r24]  = $r1
       add  $r24      = $r24, 4
       add  $r10      = $r10, -4
       br   $b0, .Lcopy
       ;;
.Lno_new_frame:
       /* Call the resolved function */
       call $r63     = $r63
       ;;
       /* Now delete the extra frame allocated, and allocate
	  stack for _dl_call_pltexit. */
       add  $r12     = $r2, -PLTEXIT_FRAME_SIZE
       ;;
       /* Store all possible result registers in La_st200_retval */
       stw (PLTEXIT_LA_ST200_RETVAL+0)[$r12] = $r16
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+4)[$r12] = $r17
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+8)[$r12] = $r18
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+12)[$r12] = $r19
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+16)[$r12] = $r20
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+20)[$r12] = $r21
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+24)[$r12] = $r22
       ;;
       stw (PLTEXIT_LA_ST200_RETVAL+28)[$r12] = $r23
       ;;
       ldw  $r16 = PLTEXIT_SAVED_LINK_MAP[$r12]
       ;;
       ldw  $r17 = PLTEXIT_SAVED_RELOC_ARG[$r12]
       ;;
       add  $r18     = $r12, PLTEXIT_LA_ST200_REGS
       add  $r19     = $r12, PLTEXIT_LA_ST200_RETVAL
       call $r63    = _dl_call_pltexit
       ;;
       /* Reload from La_st200_retval */
       ldw $r16 = (PLTEXIT_LA_ST200_RETVAL+0)[$r12]
       ;;
       ldw $r17 = (PLTEXIT_LA_ST200_RETVAL+4)[$r12]
       ;;
       ldw $r18 = (PLTEXIT_LA_ST200_RETVAL+8)[$r12]
       ;;
       ldw $r19 = (PLTEXIT_LA_ST200_RETVAL+12)[$r12]
       ;;
       ldw $r20 = (PLTEXIT_LA_ST200_RETVAL+16)[$r12]
       ;;
       ldw $r21 = (PLTEXIT_LA_ST200_RETVAL+20)[$r12]
       ;;
       ldw $r22 = (PLTEXIT_LA_ST200_RETVAL+24)[$r12]
       ;;
       ldw $r23 = (PLTEXIT_LA_ST200_RETVAL+28)[$r12]
       ;;
       /* Here there are two possible values for SP, but
	  we know $r2 is the caller_sp, so restore using that. */
       ldw $r63 = (PLTENTER_LA_ST200_REGS+36-PLTENTER_FRAME_SIZE)[$r2]
       ;;
       mov $r12 = $r2
       ldw $r2  = (PLTENTER_SAVED_R2-PLTENTER_FRAME_SIZE)[$r2]
       goto $r63
       ;;
END(_dl_runtime_profile)
#endif
