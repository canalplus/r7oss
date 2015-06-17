/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407hdmiphy.cpp
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

#include "stiH407reg.h"
#include "stiH407device.h"
#include "stiH407hdmiphy.h"

////////////////////////////////////////////////////////////////////////////////
//

CSTiH407HDMIPhy::CSTiH407HDMIPhy(CDisplayDevice *pDev): CSTmHDMITx3G4_C28_Phy(pDev,
                                                                              STiH407_HDMI_BASE)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p: pDev = %p", this, pDev );

  m_uClockGenA12 = STiH407_CLKGEN_A_12;

  TRCOUT( TRC_ID_MAIN_INFO, "%p", this );
}


CSTiH407HDMIPhy::~CSTiH407HDMIPhy()
{
}


bool CSTiH407HDMIPhy::SetupRejectionPLL(const stm_display_mode_t *mode)
{
  return true;
}


void CSTiH407HDMIPhy::DisableRejectionPLL()
{
}


bool CSTiH407HDMIPhy::Start(const stm_display_mode_t *mode, uint32_t outputFormat)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p", this );

  bool ret = false;
  if(SetupRejectionPLL(mode))
  {
    if(!(ret = CSTmHDMITx3G4_C28_Phy::Start(mode, outputFormat)))
      DisableRejectionPLL();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "%p", this );

  return ret;
}


void CSTiH407HDMIPhy::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p", this );

  CSTmHDMITx3G4_C28_Phy::Stop();

  DisableRejectionPLL();

  TRCOUT( TRC_ID_MAIN_INFO, "%p", this );
}
