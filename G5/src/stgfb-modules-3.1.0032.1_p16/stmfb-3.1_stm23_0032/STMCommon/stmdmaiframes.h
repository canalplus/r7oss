/***********************************************************************
 *
 * File: STMCommon/stmdmaiframes.h
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DMA_IFRAMES_H
#define _STM_DMA_IFRAMES_H

#include "stmiframemanager.h"

/*
 * A DMA driven IFrame manager for HDMI hardware with a single datapacket FIFO
 * whose registers are managed using a DMA pacing signal. We can reuse quite
 * a lot of the CPU driven IFrame manager for the same style hardware.
 */
class CSTmDMAIFrames: public CSTmCPUIFrames
{
public:
  CSTmDMAIFrames(CDisplayDevice *,ULONG ulHDMIOffset, ULONG ulPhysRegisterBase);
  virtual ~CSTmDMAIFrames(void);

  bool Create(CSTmHDMI *parent, COutput *master);

  ULONG GetIFrameCompleteHDMIInterruptMask(void);

protected:
  void SendFirstInfoFrame(void);

private:
  ULONG m_ulPhysicalRegisterBaseAddress;

  DMA_Channel *m_DMAChannel;
  void        *m_DMAHandle;

  CSTmDMAIFrames(const CSTmDMAIFrames&);
  CSTmDMAIFrames& operator=(const CSTmDMAIFrames&);
};

#endif //_STM_DMA_IFRAMES_H
