/***********************************************************************
 *
 * File: STMCommon/stmv29iframes.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_V29_IFRAMES_H
#define _STM_V29_IFRAMES_H

#include "stmiframemanager.h"

/*
 * This is a HDMI infoframe implementation for >=V2.9 HDMI hardware, sorry
 * about the uninspired class name.
 */
class CSTmV29IFrames: public CSTmIFrameManager
{
public:
  CSTmV29IFrames(CDisplayDevice *,ULONG ulHDMIOffset);
  virtual ~CSTmV29IFrames(void);

  bool Stop(void);
  void UpdateFrame(void);

  bool Create(CSTmHDMI *parent, COutput *master);

  ULONG GetIFrameCompleteHDMIInterruptMask(void);

protected:
  void WriteInfoFrame(int transmissionSlot,stm_hdmi_info_frame_t *);
  void EnableTransmissionSlot(int transmissionSlot);
  void DisableTransmissionSlot(int transmissionSlot);
  void SendFirstInfoFrame(void);
  void ProcessInfoFrameComplete(ULONG interruptStatus);

private:
  int   m_nSlots;
  ULONG m_ulConfig;

  void WriteInfoFrameHelper(int transmissionSlot,stm_hdmi_info_frame_t *frame);

  CSTmV29IFrames(const CSTmV29IFrames&);
  CSTmV29IFrames& operator=(const CSTmV29IFrames&);
};

#endif //_STM_V29_IFRAMES_H
