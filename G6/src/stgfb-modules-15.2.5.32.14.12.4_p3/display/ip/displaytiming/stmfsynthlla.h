/***********************************************************************
 *
 * File: display/ip/displaytiming/stmfsynthlla.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_CLK_FSYNTH_H
#define _STM_CLK_FSYNTH_H

#include "stmfsynth.h"

typedef enum {
  STM_CLK_FS_CH_0,
  STM_CLK_FS_CH_1,
  STM_CLK_FS_CH_2,
  STM_CLK_FS_CH_3,
  STM_CLK_FS_CH_4,
  STM_CLK_FS_CH_5,
  STM_CLK_FS_CH_6,
  STM_CLK_FS_CH_7,
  STM_CLK_FS_CH_8,

  STM_CLK_FS_HD = STM_CLK_FS_CH_0,
  STM_CLK_FS_SD

} stm_clk_fsynth_channel_t;

class CSTmFSynthLLA: public CSTmFSynth
{
public:
  CSTmFSynthLLA(struct vibe_clk *clk_fs);
  virtual ~CSTmFSynthLLA(void);

  bool Start(uint32_t ulFrequency);
  void Stop(void);
  bool SetAdjustment(int ppm);

protected:
  struct vibe_clk *m_clk_fs;

  bool SolveFsynthEqn(uint32_t Fout, stm_clock_fsynth_timing_t *timing) const;
  void ProgramClock(void) {}

private:
  CSTmFSynthLLA(const CSTmFSynthLLA&);
  CSTmFSynthLLA& operator=(const CSTmFSynthLLA&);
};

#endif // _STM_CLK_FSYNTH_H
