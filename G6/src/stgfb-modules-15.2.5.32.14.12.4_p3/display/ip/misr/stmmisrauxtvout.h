/***********************************************************************
 *
 * File: display/ip/misr/stmmisrauxtvout.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _STM_MISR_AUX_TVOUT_H
#define _STM_MISR_AUX_TVOUT_H

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/ip/stmmisrviewport.h>
#include "stmmisrtvout.h"

class CDisplayDevice;

/*
 * for all MISRs present in AUX pipeline
 */
class CSTmMisrAuxTVOut: public CSTmMisrTVOut
{
public:
  CSTmMisrAuxTVOut(CDisplayDevice *pDev, uint32_t MisrPFCtrlReg, uint32_t MisrOutputCtrlReg);
  virtual ~CSTmMisrAuxTVOut(void);

  void ReadMisrSigns(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt);
  TuningResults SetMisrControlValue(uint16_t service, void *inputList, const stm_display_mode_t *pCurrentMode);
  void UpdateMisrControlValue(const stm_display_mode_t *pCurrentMode);
  TuningResults GetMisrCapability(void *outputList, uint32_t outputListSize);

private:
  CSTmMisrAuxTVOut(const CSTmMisrAuxTVOut&);
  CSTmMisrAuxTVOut& operator=(const CSTmMisrAuxTVOut&);
};

#endif //_STM_MISR_AUX_TVOUT_H

