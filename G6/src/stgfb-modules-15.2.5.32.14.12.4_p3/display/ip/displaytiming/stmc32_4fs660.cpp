/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc32_4fs660.cpp
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>

#include "stmc32_4fs660.h"


CSTmC32_4FS660::CSTmC32_4FS660(void): CSTmFSynth()
{
  m_VCO_ndiv = 0;
}


CSTmC32_4FS660::~CSTmC32_4FS660()
{
}


/*
 * This is a variation of the FS660 equation solver taken from the ST Linux
 * clock framework code, drivers/stm/clock-common.c
 *
 * The difference is it is not limited to an accuracy of 20KHz, which is
 * simply not accurate enough for video related clocks. So we have to
 * go to 64bit maths; also to make the equations simpler we use 2^20 as our
 * value of "1" in the fixed point maths instead of 10000.
 */
bool CSTmC32_4FS660::SolveFsynthEqn(uint32_t Fout,stm_clock_fsynth_timing_t *timing) const
{
  int si;
  const uint32_t p20 = 1048576; /* 2 power 20 */
  uint64_t p;                   /* pe value */
  uint32_t m;                   /* md value */
  uint32_t divisor;             /* bottom term of the Fout calculation */
  uint64_t new_freq;            /* Needs to be 64bit to hold intermediate calculation */
  uint32_t deviation = Fout;
  uint32_t new_deviation;

  uint64_t Fvco = (m_refClock==STM_CLOCK_REF_27MHZ)?27000000UL:30000000UL;

  Fvco *= (m_VCO_ndiv + 16);

  TRCIN( TRC_ID_MAIN_INFO, "Requested Fout == %u Fvco == %llu", Fout, Fvco );

  for (si = 0; (si < 9) && deviation; si++) {
          for (m = 0; (m < 32) && deviation; m++) {
                  p = vibe_os_div64((Fvco * p20)+(Fout/2), Fout);
                  p >>= si; /* Note this assumes that nsdiv is 1, so we can avoid a real divide */
                  p = p - p20 - (m * (p20 / 32));
                  /*
                   * Because we used 2^20 to represent 1 in our fixed point
                   * maths, we don't have to multiply p by 2^20, it already
                   * is.
                   */
                  if (p > 32767) continue;

                  divisor = (p20 + (m * (p20 / 32)) + p);
                  new_freq = vibe_os_div64((Fvco * p20)+(divisor/2), divisor);
                  new_freq >>= si;  /* Note this assumes that nsdiv is 1, so we can avoid a real divide */

                  if (new_freq < Fout)
                          new_deviation = Fout - new_freq;
                  else
                          new_deviation = new_freq - Fout;
                  if (!new_deviation || (new_deviation < deviation)) {
                          timing->fout = new_freq;
                          timing->pe = p;
                          timing->md = m;
                          timing->sdiv = si;
                          deviation = new_deviation;
                  }
          }
  }


  TRCOUT( TRC_ID_UNCLASSIFIED, "Fout == %u SDIV == %u, MD == %02x, PE == 0x%x", timing->fout, timing->sdiv, timing->md, timing->pe );

  /* return true if all variables meet their contraints */
  return (timing->sdiv <= 8 && timing->md < 32 && timing->pe <= 32767);
}
