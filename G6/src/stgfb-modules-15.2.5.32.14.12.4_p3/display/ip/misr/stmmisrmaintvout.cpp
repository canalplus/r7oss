/***********************************************************************
 *
 * File: display/ip/misr/stmmisrmaintvout.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <stm_display.h>
#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayPlane.h>
#include <display/generic/DisplayDevice.h>
#include <display/ip/displaytiming/stmvtg.h>
#include <display/ip/stmmisrviewport.h>

#include "stmmisrmaintvout.h"


//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced MISR Output on main HD Dacs
//
CSTmMisrMainTVOut::CSTmMisrMainTVOut(CDisplayDevice *pDev, uint32_t MisrPFCtrlReg, uint32_t MisrOutputCtrlReg): CSTmMisrTVOut(pDev, MisrPFCtrlReg, MisrOutputCtrlReg)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmMisrMainTVOut::~CSTmMisrMainTVOut() {}

void CSTmMisrMainTVOut::ReadMisrSigns(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt)
{
  if((m_MisrOutput.isMisrCaptureStarted)||(m_MisrPF.isMisrCaptureStarted))
  {
    m_MisrData.LastVsyncTime       = LastVTGEvtTime;
    m_MisrData.VTGEvt              = LastVTGEvt;

    if(m_MisrOutput.isMisrCaptureStarted)
      ReadMisrSign(&m_MisrOutput, m_MisrOutputCtrlReg);

    if(m_MisrPF.isMisrCaptureStarted)
      ReadMisrSign(&m_MisrPF, m_MisrPFCtrlReg);
  }
}

void CSTmMisrMainTVOut::UpdateMisrControlValue(const stm_display_mode_t *pCurrentMode)
{
  UpdateMisrCtrl(&m_MisrPF, pCurrentMode);
  UpdateMisrCtrl(&m_MisrOutput, pCurrentMode);
}

TuningResults CSTmMisrMainTVOut::SetMisrControlValue(uint16_t service, void *inputList, const stm_display_mode_t *pCurrentMode)
{
  /*This function will be updated after test module in enhanced to pass more parameters than just service*/
  SetTuningInputData_t *input = (SetTuningInputData_t *)inputList;
  static char misr_main_pf[] = "MISR_MAIN_PF";
  static char misr_hd_out[] = "MISR_HD_OUT";

  TRC( TRC_ID_UNCLASSIFIED, "for service: %x", service );

  if(pCurrentMode == NULL)
  {
    return TUNING_SERVICE_NOT_SUPPORTED;
  }

  if(vibe_os_memcmp((void*)input->ServiceStr, (void*) misr_main_pf, sizeof(misr_main_pf) ) == 0)
  {
    TRC( TRC_ID_UNCLASSIFIED, "service MISR_MAIN_PF" );
    return (SetMisrCtrl(&m_MisrPF, m_MisrPFCtrlReg, input, pCurrentMode));

  }

  if(vibe_os_memcmp((void*)input->ServiceStr, (void*) misr_hd_out, sizeof(misr_hd_out) ) == 0)
  {
    TRC( TRC_ID_UNCLASSIFIED, "service MISR_HD_OUT" );
    return (SetMisrCtrl(&m_MisrOutput, m_MisrOutputCtrlReg, input, pCurrentMode));
  }

  /*We could not find valid service so far, print error and return false*/
  TRC( TRC_ID_UNCLASSIFIED, "Invalid Service: %s", input->ServiceStr );
  return TUNING_INVALID_PARAMETER;
}

TuningResults CSTmMisrMainTVOut::GetMisrCapability(void *outputList, uint32_t outputListSize)
{
    char *string = (char *)outputList;
    uint32_t lengthOfString = outputListSize;
    if(string != NULL)
    {
        lengthOfString = vibe_os_snprintf(string, outputListSize, "MISR_MAIN_PF,MISR_HD_OUT");
    }
    return TUNING_OK;
}

