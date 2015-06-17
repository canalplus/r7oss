/***********************************************************************
 *
 * File: include/stmdisplayclocks.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_CLOCKS_H
#define _STM_DISPLAY_CLOCKS_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum
{
  STM_CLOCK_REF_27MHZ,
  STM_CLOCK_REF_30MHZ
} stm_clock_ref_frequency_t;

/*
 * FSynth configuration, the equation is:
 *
 * <pre>
 *                                  32768*Fpll
 * #1: Fout = ------------------------------------------------------
 *                            md                        (md + 1)
 *            (sdiv*((pe*(1 + --)) - ((pe - 32768)*(1 + --------))))
 *                            32                           32
 * </pre>
 *
 * Where Fpll is 240MHz for a 30MHz input reference crystal and
 * 216MHz for a 27MHz input reference crystal.
 *
 * This simplifies to:
 *
 * <pre>
 *                       1048576*Fpll
 * #2: Fout = ----------------------------------
 *            (sdiv*(1081344 - pe + (32768*md)))
 * </pre>
 */
static inline unsigned long stm_fsynth_frequency(stm_clock_ref_frequency_t refclk,
                                                 unsigned long             sdiv,
                                                 signed long               md,
                                                 unsigned long             pe)
{
  md = md - 32; // convert from hardware to algebraic form
  sdiv = 2 << sdiv; // convert from hardware to algebraic form

  { /* New block to avoid C compiler warnings */
    unsigned long Fpll = (refclk==STM_CLOCK_REF_27MHZ)?216000000UL:240000000UL;

    /* Use our 64bit types to avoid C++ compiler warnings */
    ULONGLONG p = 1048576ll * Fpll;
    LONGLONG  q = 32768 * md;
    LONGLONG  r = 1081344 - pe;
    ULONGLONG s = r + q;
    ULONGLONG t = sdiv * s;
    ULONGLONG u = p / t;

    return (unsigned long) u;
  }
}


typedef struct
{
  ULONG fout; /* Target output frequency                          */
  ULONG sdiv; /* 0x0 = /2, 0x1 = /4, 0x2 = /8, 0x3 = /16 etc..    */
  ULONG md;   /* 5bit signed, valid range -16 (0x10) to -1 (0x1F) */
  ULONG pe;   /* 0 - 2^15-1                                       */
} stm_clock_fsynth_timing_t;


#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_CLOCKS_H */
