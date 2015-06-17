/***********************************************************************
 *
 * File: STMCommon/stmfsynth.cpp
 * Copyright (c) 2005, 2007, 2008 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>

#include "stmfsynth.h"


CSTmFSynth::CSTmFSynth(CDisplayDevice *pDev, ULONG ulRegOffset)
{
  DEBUGF2(2,("CSTmFSynth::CSTmFSynth pDev = %p\n",pDev));

  m_pDevRegs = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulFSynthOffset = ulRegOffset;

  m_refClock    = STM_CLOCK_REF_30MHZ;
  m_refError    = 0;
  m_adjustment  = 0;
  m_divider     = 1;

  m_CurrentTiming.fout = 0;
}


CSTmFSynth::~CSTmFSynth() {}


/*
 * The following fsynth equation solving is derived from the ST ALSA driver in
 * STLinux2.3. Thanks to Dan Thompson for providing this.
 */
static inline ULONGLONG div64_64(ULONGLONG p,ULONGLONG q)
{
  return p/q;
}

/* Return the number of set bits in x. */
static unsigned int population(unsigned int x)
{
  /* This is the traditional branch-less algorithm for population count */
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0f0f0f0f;
  x = x + (x << 8);
  x = x + (x << 16);

  return x >> 24;
}

/* Return the index of the most significant set in x.
 * The results are 'undefined' is x is 0 (0xffffffff as it happens
 * but this is a mere side effect of the algorithm. */
static unsigned int most_significant_set_bit(unsigned int x)
{
  /* propagate the MSSB right until all bits smaller than MSSB are set */
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);

  /* now count the number of set bits [clz is population(~x)] */
  return population(x) - 1;
}


/* Solve the frequency synthesiser equations to provide a specified output
 * frequency.
 *
 * The approach taken to solve the equation is to solve for sdiv assuming
 * maximal values for md and one greater than maximal pe (-16 and 32768
 * respectively) before rounding down. Once sdiv is selected we can
 * solve for md by assuming maximal pe and rounding down. With these
 * values pe can trivially be calculated.
 *
 * The function is implemented entirely with integer calculations making
 * it suitable for use within the Linux kernel.
 *
 * The magic numbers within the function are derived from the Fsynth equation
 * which is as follows:
 *
 * <pre>
 *                                  32768*Fpll
 * #1: Fout = ------------------------------------------------------
 *                            md                        (md + 1)
 *            (sdiv*((pe*(1 + --)) - ((pe - 32768)*(1 + --------))))
 *                            32                           32
 * </pre>
 *
 * Where:
 *
 *  - Fpll and Fout are frequencies in Hz
 *  - sdiv is power of 2 between 1 and 8
 *  - md is an integer between -1 and -16
 *  - pe is an integer between 0 and 32767
 *
 * This simplifies to:
 *
 * <pre>
 *                       1048576*Fpll
 * #2: Fout = ----------------------------------
 *            (sdiv*(1081344 - pe + (32768*md)))
 * </pre>
 *
 * Rearranging:
 *
 * <pre>
 *                 1048576*Fpll
 * #3: predivide = ------------ = (sdiv*(1081344 - pe + (32768*md)))
 *                     Fout
 * </pre>
 *
 * If solve for sdiv and let pe = 32768 and md = -16 we get:
 *
 * <pre>
 *                     predivide            predivide
 * #4: sdiv = --------------------------- = ---------
 *            (1081344 - pe + (32768*md))     524288
 * </pre>
 *
 * Returning to eqn. #3, solving for md and let pe = 32768 we get:
 *
 * <pre>
 *           predivide                    predivide
 *          (--------- - 1081344 + pe)   (--------- - 1048576)
 *             sdiv                         sdiv
 * #5: md = -------------------------- = ---------------------
 *                    32768                      32768

 * </pre>
 *
 * Finally we return to #3 and rearrange for pe:
 *
 * <pre>
 *              predivide
 * #6: pe = -1*(--------- - 1081344 - (32768*md))
 *                sdiv
 * </pre>
 *
 */
bool CSTmFSynth::SolveFsynthEqn(ULONG Fout,stm_clock_fsynth_timing_t *timing) const
{
  ULONGLONG p, q;
  unsigned int predivide;
  int preshift; /* always +ve but used in subtraction */
  unsigned int sdiv;
  int md;
  unsigned int pe = 1 << 14;
  unsigned long Fpll = (m_refClock==STM_CLOCK_REF_27MHZ)?216000000UL:240000000UL;

  /* pre-divide the frequencies */
  p = 1048576ull * Fpll;          /* <<20? */
  q = Fout;

  predivide = (unsigned int)div64_64(p, q);

  /* determine an appropriate value for the output divider using eqn. #4
   * with md = -16 and pe = 32768 (and round down) */
  sdiv = predivide / 524288;
  if (sdiv > 1) {
    /* sdiv = fls(sdiv) - 1; // this doesn't work
     * for some unknown reason */
    sdiv = most_significant_set_bit(sdiv);
  } else
    sdiv = 1;

  /* pre-shift a common sub-expression of later calculations */
  preshift = predivide >> sdiv;

  /* determine an appropriate value for the coarse selection using eqn. #5
   * with pe = 32768 (and round down which for signed values means away
   * from zero) */
  md = ((preshift - 1048576) / 32768) - 1;        /* >>15? */

  /* calculate a value for pe that meets the output target */
  pe = -1 * (preshift - 1081344 - (32768 * md));  /* <<15? */

  /* finally give sdiv its true hardware form */
  sdiv--;

  /* special case for 58593.75Hz and harmonics...
   * can't quite seem to get the rounding right */
  if (md == -17 && pe == 0) {
    md = -16;
    pe = 32767;
  }

  /* update the outgoing arguments */
  timing->fout = Fout;
  timing->sdiv = sdiv;
  timing->md   = md;
  timing->pe   = pe;

  DEBUGF2(2,("%s: Fout == %lu SDIV == %u, MD == %02x, PE == 0x%x\n", __PRETTY_FUNCTION__, Fout, sdiv, md, pe));

  /* return true if all variables meet their contraints */
  return (sdiv <= 7 && -16 <= md && md <= -1 && pe <= 32767);
}


ULONG CSTmFSynth::AdjustFrequency(ULONG ulFrequency, LONG adjustment)
{
  LONG delta;

  adjustment += m_refError;

  /*             a
   * F = f + --------- * f = f + d
   *          1000000
   *
   *         a
   * d = --------- * f
   *      1000000
   *
   * where:
   *   f - nominal frequency
   *   a - adjustment in ppm (parts per milion)
   *   F - frequency to be set in synthesizer
   *   d - delta (difference) between f and F
   */
  if (adjustment < 0) {
          /* div64_64 operates on unsigned values... */
          delta = -1;
          adjustment = -adjustment;
  } else {
          delta = 1;
  }

  /* 500000 ppm is 0.5, which is used to round up values */
  delta *= (int)div64_64((ULONGLONG)ulFrequency * (ULONGLONG)adjustment + 500000ULL, 1000000ULL);

  return ulFrequency + delta;
}


bool CSTmFSynth::Start(ULONG ulFrequency)
{
  stm_clock_fsynth_timing_t timing;

  ULONG f = AdjustFrequency(ulFrequency*m_divider, 0);
  if(!SolveFsynthEqn(f, &timing))
  {
    DEBUGF2(1,("%s: Unable to solve fsynth equation for %luHz\n",__PRETTY_FUNCTION__,f));
    return false;
  }

  m_NominalOutputFrequency = ulFrequency;
  m_CurrentTiming          = timing;

  DEBUGF2(2,("%s: setting frequency %luHz for divider %d\n",__PRETTY_FUNCTION__,ulFrequency,m_divider));
  ProgramClock();

  return true;
}


void CSTmFSynth::Stop()
{
  m_adjustment         = 0;
  m_CurrentTiming.fout = 0;
}


bool CSTmFSynth::SetAdjustment(int ppm)
{
  if(m_CurrentTiming.fout != 0)
  {
    stm_clock_fsynth_timing_t timing;

    if(ppm< -500 || ppm > 500)
      return false;
      
    ULONG f = AdjustFrequency(m_NominalOutputFrequency*m_divider,ppm);
    if(!SolveFsynthEqn(f, &timing))
    {
      DEBUGF2(1,("%s: Unable to solve fsynth equation for %luHz\n",__PRETTY_FUNCTION__,f));
      return false;
    }

    m_adjustment    = ppm;
    m_CurrentTiming = timing;
    ProgramClock();
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
//
#define FSYNTH_TYPE1_MD1   0x0
#define FSYNTH_TYPE1_PE1   0x4
#define FSYNTH_TYPE1_EN1   0x8
#define FSYNTH_TYPE1_SDIV1 0xC

void CSTmFSynthType1::ProgramClock(void)
{
  WriteFSynthReg(FSYNTH_TYPE1_EN1,   0);
  WriteFSynthReg(FSYNTH_TYPE1_SDIV1, m_CurrentTiming.sdiv);
  WriteFSynthReg(FSYNTH_TYPE1_MD1,   m_CurrentTiming.md);
  WriteFSynthReg(FSYNTH_TYPE1_PE1,   m_CurrentTiming.pe);
  WriteFSynthReg(FSYNTH_TYPE1_EN1, 1);
}

////////////////////////////////////////////////////////////////////////////////
//
#define FSYNTH_TYPE2_CFG              0

#define FSYNTH_TYPE2_CFG_PE_SHIFT     0
#define FSYNTH_TYPE2_CFG_PE_MASK      (0xffff << FSYNTH_TYPE2_CFG_PE_SHIFT)
#define FSYNTH_TYPE2_CFG_MD_SHIFT     16
#define FSYNTH_TYPE2_CFG_MD_MASK      (0x1f << FSYNTH_TYPE2_CFG_MD_SHIFT)
#define FSYNTH_TYPE2_CFG_SDIV_SHIFT   22
#define FSYNTH_TYPE2_CFG_SDIV_MASK    (0x7 << FSYNTH_TYPE2_CFG_SDIV_SHIFT)
#define FSYNTH_TYPE2_CFG_EN_PRG       (1L << 21)
#define FSYNTH_TYPE2_CFG_SELCLKOUT    (1L << 25)
#define FSYNTH_TYPE2_CFG_SDIV3        (1L << 27) 

void CSTmFSynthType2::ProgramClock(void)
{
  ULONG tmp = ReadFSynthReg(0) & ~(FSYNTH_TYPE2_CFG_PE_MASK |
                                   FSYNTH_TYPE2_CFG_MD_MASK |
                                   FSYNTH_TYPE2_CFG_EN_PRG  |
                                   FSYNTH_TYPE2_CFG_SDIV_MASK);

  tmp |= m_CurrentTiming.sdiv << FSYNTH_TYPE2_CFG_SDIV_SHIFT;
  tmp |= (m_CurrentTiming.md & 0x1f) << FSYNTH_TYPE2_CFG_MD_SHIFT;
  tmp |= (m_CurrentTiming.pe << FSYNTH_TYPE2_CFG_PE_SHIFT);
  tmp |= FSYNTH_TYPE2_CFG_SELCLKOUT;

  WriteFSynthReg(FSYNTH_TYPE2_CFG, tmp);
  WriteFSynthReg(FSYNTH_TYPE2_CFG, tmp | FSYNTH_TYPE2_CFG_EN_PRG);
}
