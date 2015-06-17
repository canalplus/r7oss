/***********************************************************************
 *
 * File: display/ip/misr/stmmisrmaintvout.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_MISR_MAIN_TVOUT_H
#define _STM_MISR_MAIN_TVOUT_H

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/ip/stmmisrviewport.h>
#include "stmmisrtvout.h"

class CDisplayDevice;

/*
 * for all MISRs present in MAIN pipeline
 */
class CSTmMisrMainTVOut: public CSTmMisrTVOut
{
public:
  CSTmMisrMainTVOut(CDisplayDevice *pDev, uint32_t MisrPFCtrlReg, uint32_t MisrOutputCtrlReg);
  virtual ~CSTmMisrMainTVOut(void);

  void ReadMisrSigns(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt);
  TuningResults SetMisrControlValue(uint16_t service, void *inputList, const stm_display_mode_t *pCurrentMode);
  void UpdateMisrControlValue(const stm_display_mode_t *pCurrentMode);
  TuningResults GetMisrCapability(void *outputList, uint32_t outputListSize);

private:
  CSTmMisrMainTVOut(const CSTmMisrMainTVOut&);
  CSTmMisrMainTVOut& operator=(const CSTmMisrMainTVOut&);
};

#endif //_STM_MISR_MAIN_TVOUT_H
