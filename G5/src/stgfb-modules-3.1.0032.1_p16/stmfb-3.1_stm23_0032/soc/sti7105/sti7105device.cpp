/***********************************************************************
 *
 * File: soc/sti7105/sti7105device.cpp
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <soc/sti7111/sti7111hdmi.h>

#include "sti7105reg.h"
#include "sti7105device.h"


CSTi7105Device::CSTi7105Device (void): CSTi7111Device ()
{
  DEBUGF2 (2, (FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}


CSTi7105Device::~CSTi7105Device (void)
{
  DEBUGF2 (2, (FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}



bool CSTi7105Device::CreateOutputs(void)
{
  if(!CSTi7111Device::CreateOutputs())
    return false;

  /*
   * TODO: Create DVO1 output
   */
  return true;
}


bool CSTi7105Device::CreateHDMIOutput(void)
{
  CSTmHDMI *pHDMI;

  /*
   * Yes all this trouble just to change the clockgenC input mux setup.
   */
  if((pHDMI = new CSTi7111HDMI(this, m_pOutputs[STi7111_OUTPUT_IDX_VDP0_MAIN], m_pVTG1, AUD_FSYN_CFG_SYSA_IN)) == 0)
  {
    DERROR("failed to allocate HDMI output\n");
    return false;
  }

  if(!pHDMI->Create())
  {
    DERROR("failed to create HDMI output\n");
    delete pHDMI;
    return false;
  }

  m_pOutputs[STi7111_OUTPUT_IDX_VDP0_HDMI] = pHDMI;
  return true;
}
