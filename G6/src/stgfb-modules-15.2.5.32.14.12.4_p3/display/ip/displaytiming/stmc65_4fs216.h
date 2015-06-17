/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc65_4fs216.h
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMC65_4FS216_H
#define _STMC65_4FS216_H

#include "stmfsynth.h"

class CSTmC65_4FS216: public CSTmFSynth
{
public:
  CSTmC65_4FS216(CDisplayDevice*,uint32_t uRegOffset);
  ~CSTmC65_4FS216(void);

  /*
   * Helper to reverse engineer the output frequency of an fsynth of this
   * type from its programming. Used by the SoC HDMI classes to find out
   * the configured frequency of the audio FSynths.
   */
  static inline uint32_t CalculateFrequency(stm_clock_ref_frequency_t refclk,
                                            uint32_t                  sdiv,
                                            int32_t                   md, // Yes this is signed
                                            uint32_t                  pe);

protected:
  uint32_t*  m_pDevRegs;
  uint32_t   m_uFSynthOffset;
  
  uint32_t   m_InputPLLMult;

  bool  SolveFsynthEqn(uint32_t ulFrequency, stm_clock_fsynth_timing_t *timing) const;

  void WriteFSynthReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pDevRegs, (m_uFSynthOffset+reg), val); }
  uint32_t ReadFSynthReg(uint32_t reg) { return vibe_os_read_register(m_pDevRegs, (m_uFSynthOffset+reg)); }

private:
  CSTmC65_4FS216(const CSTmC65_4FS216&);
  CSTmC65_4FS216& operator=(const CSTmC65_4FS216&);
};


/*
 * For the FS216, the Fout equation is:
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
uint32_t CSTmC65_4FS216::CalculateFrequency(stm_clock_ref_frequency_t refclk,
                                            uint32_t                  sdiv,
                                            int32_t                   md, // Yes this is signed
                                            uint32_t                  pe)
{
  md = md - 32; // convert from hardware to algebraic form
  sdiv = 2 << sdiv; // convert from hardware to algebraic form

  { /* New block to avoid C compiler warnings */
    uint32_t Fpll = (refclk==STM_CLOCK_REF_27MHZ)?216000000UL:240000000UL;

    /* Use our 64bit types to avoid C++ compiler warnings */
    uint64_t p = 1048576ll * Fpll;
    int64_t  q = 32768 * md;
    int64_t  r = 1081344 - pe;
    uint64_t s = r + q;
    uint64_t t = sdiv * s;
    uint64_t u = vibe_os_div64(p, t);

    return (uint32_t) u;
  }
}



/*-----------------------------------------------------------------------------
 * Multi register controlled fsynth as found in ClockGenB on 710x and 7111
 * and ClockGenC audio clocks on all 7xxx series parts.
 */
class CSTmFSynthType1: public CSTmC65_4FS216
{
public:
  CSTmFSynthType1(CDisplayDevice *pDev, uint32_t base): CSTmC65_4FS216(pDev, base) {}
  ~CSTmFSynthType1(void) {}

protected:
  void ProgramClock(void);

private:
  CSTmFSynthType1(const CSTmFSynthType1&);
  CSTmFSynthType1& operator=(const CSTmFSynthType1&);
};


/*
 * Single register controlled fsynth as found in ClockGenB on 7200
 */
class CSTmFSynthType2: public CSTmC65_4FS216
{
public:
  CSTmFSynthType2(CDisplayDevice *pDev, uint32_t base): CSTmC65_4FS216(pDev, base) {}
  ~CSTmFSynthType2(void) {}

protected:
  void ProgramClock(void);

private:
  CSTmFSynthType2(const CSTmFSynthType2&);
  CSTmFSynthType2& operator=(const CSTmFSynthType2&);
};

#endif // _STMC65_4FS216_H
