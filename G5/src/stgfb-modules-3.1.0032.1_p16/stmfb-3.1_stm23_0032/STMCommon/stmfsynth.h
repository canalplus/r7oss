/***********************************************************************
 *
 * File: STMCommon/stmfsynth.h
 * Copyright (c) 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
\***********************************************************************/

#ifndef _STMFSYNTH_H
#define _STMFSYNTH_H 

class CDisplayDevice;


class CSTmFSynth
{
public:
  CSTmFSynth(CDisplayDevice*,ULONG ulRegOffset);

  virtual ~CSTmFSynth(void);

  virtual bool Start(ULONG ulFrequency);
  virtual void Stop(void);

  void SetClockReference(stm_clock_ref_frequency_t  refClock, LONG error_ppm)
  {
    m_refClock = refClock;
    m_refError = error_ppm;
  }

  void SetDivider(int value) { m_divider = value; }

  bool SetAdjustment(int ppm);
  int  GetAdjustment(void) { return m_adjustment; }

protected:
  ULONG*  m_pDevRegs;
  ULONG   m_ulFSynthOffset;
  int     m_adjustment;
  
  /*
   * Post fsynth divider
   */
  int     m_divider;

  /*
   * Reference clock information, 27 or 30MHz and an error in parts per million
   */
  stm_clock_ref_frequency_t  m_refClock;
  LONG                       m_refError;

  ULONG                      m_NominalOutputFrequency;
  stm_clock_fsynth_timing_t  m_CurrentTiming;

  bool  SolveFsynthEqn(ULONG ulFrequency, stm_clock_fsynth_timing_t *timing) const;
  ULONG AdjustFrequency(ULONG ulFrequency, LONG adjustment);

  virtual void ProgramClock(void)      = 0;

  void WriteFSynthReg(ULONG reg, ULONG val) { g_pIOS->WriteRegister(m_pDevRegs + ((m_ulFSynthOffset+reg)>>2), val); }
  ULONG ReadFSynthReg(ULONG reg) { return g_pIOS->ReadRegister(m_pDevRegs + ((m_ulFSynthOffset+reg)>>2)); }

private:
  CSTmFSynth(const CSTmFSynth&);
  CSTmFSynth& operator=(const CSTmFSynth&);
};


/*
 * Multi register controlled fsynth as found in ClockGenB on 710x and 7111
 * and ClockGenC audio clocks on all 7xxx series parts.
 */
class CSTmFSynthType1: public CSTmFSynth
{
public:
  CSTmFSynthType1(CDisplayDevice *pDev, ULONG base): CSTmFSynth(pDev, base) {}
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
class CSTmFSynthType2: public CSTmFSynth
{
public:
  CSTmFSynthType2(CDisplayDevice *pDev, ULONG base): CSTmFSynth(pDev, base) {}
  ~CSTmFSynthType2(void) {}

protected:
  void ProgramClock(void);

private:
  CSTmFSynthType2(const CSTmFSynthType2&);
  CSTmFSynthType2& operator=(const CSTmFSynthType2&);
};

#endif // _STMFSYNTH_H
