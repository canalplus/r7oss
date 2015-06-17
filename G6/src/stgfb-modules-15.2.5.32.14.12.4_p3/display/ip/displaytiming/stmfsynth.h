/***********************************************************************
 *
 * File: display/ip/displaytiming/stmfsynth.h
 * Copyright (c) 2005-2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFSYNTH_H
#define _STMFSYNTH_H

typedef struct
{
  uint32_t fout; /* Target output frequency                          */
  uint32_t sdiv; /* 0x0 = /2, 0x1 = /4, 0x2 = /8, 0x3 = /16 etc..    */
  uint32_t md;   /* 5bit signed, valid range -16 (0x10) to -1 (0x1F) */
  uint32_t pe;   /* 0 - 2^15-1                                       */
} stm_clock_fsynth_timing_t;

class CDisplayDevice;

#define MAX_SLAVED_FSYNTH       3

class CSTmFSynth
{
public:
  CSTmFSynth(void);

  virtual ~CSTmFSynth(void);

  virtual bool Start(uint32_t ulFrequency);
  virtual void Stop(void);

  void SetClockReference(stm_clock_ref_frequency_t  refClock, int32_t error_ppm);

  void SetDivider(int value);

  virtual bool SetAdjustment(int ppm);
  int  GetAdjustment(void) { return m_adjustment; }

  bool SetSlave(bool IsSlave);
  bool RegisterSlavedFSynth(CSTmFSynth *pFSynth);
  void UnRegisterSlavedFSynth(CSTmFSynth *pFSynth);

protected:
  bool         m_IsSlave;
  CSTmFSynth **m_pSlavedFSynth;

  /*
   * The expected post fsynth divider value; the Fsynth frequency will
   * multiplied up by this.
   */
  int          m_divider;
  int          m_adjustment;

  /*
   * Reference clock information, 27 or 30MHz and an error in parts per million
   */
  stm_clock_ref_frequency_t  m_refClock;
  int32_t                    m_refError;

  uint32_t                   m_NominalOutputFrequency;
  stm_clock_fsynth_timing_t  m_CurrentTiming;

  /*
   * Specific to a particular FSynth type
   */
  virtual bool  SolveFsynthEqn(uint32_t ulFrequency, stm_clock_fsynth_timing_t *timing) const = 0;
  virtual void  ProgramClock(void)      = 0;

  uint32_t AdjustFrequency(uint32_t ulFrequency, int32_t adjustment);

private:
  CSTmFSynth(const CSTmFSynth&);
  CSTmFSynth& operator=(const CSTmFSynth&);
};

#endif // _STMFSYNTH_H
