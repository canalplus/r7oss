/***********************************************************************
 *
 * File: display/ip/displaytiming/stmc32_4fs660.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMC32_4FS660_H
#define _STMC32_4FS660_H

#include "stmfsynth.h"

class CSTmC32_4FS660: public CSTmFSynth
{
public:
  CSTmC32_4FS660(void);
  ~CSTmC32_4FS660(void);

protected:
  uint32_t m_VCO_ndiv;

  bool  SolveFsynthEqn(uint32_t ulFrequency, stm_clock_fsynth_timing_t *timing) const;

private:
  CSTmC32_4FS660(const CSTmC32_4FS660&);
  CSTmC32_4FS660& operator=(const CSTmC32_4FS660&);
};


#endif // _STMC32_4FS660_H
