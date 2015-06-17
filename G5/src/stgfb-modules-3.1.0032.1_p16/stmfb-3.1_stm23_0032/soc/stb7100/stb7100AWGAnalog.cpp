/***********************************************************************
 *
 * File: soc/stb7100/stb7100AWGAnalog.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IDebug.h>
#include <Generic/DisplayDevice.h>

#include "stb7100reg.h"

#include "stb7100AWGAnalog.h"


#define AWG_RAM_SIZE         46



CSTb7100AWGAnalog::CSTb7100AWGAnalog (CDisplayDevice* pDev):
                                      CAWG (pDev,
                                            STb7100_AWG_BASE + AWG_RAM_REG,
                                            AWG_RAM_SIZE)
{
  DEBUGF2 (2, ("%s\n", __PRETTY_FUNCTION__));
  m_pAWGReg = (ULONG *) pDev->GetCtrlRegisterBase () + (STb7100_AWG_BASE >> 2); 
}


CSTb7100AWGAnalog::~CSTb7100AWGAnalog ()
{
  DEBUGF2 (2, ("%s\n", __PRETTY_FUNCTION__));
}


bool CSTb7100AWGAnalog::Enable (stm_awg_mode_t mode)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  /* enable AWG and set up sync behavior */
  ULONG awg = 0;

  if (mode & STM_AWG_MODE_FILTER_HD)
    awg |= 0x2;
  else if (mode & STM_AWG_MODE_FILTER_ED)
    awg |= 0x1;
  else if (mode & STM_AWG_MODE_FILTER_SD)
    awg |= 0x0;
  else
    awg |= 0x3;

  if (mode & STM_AWG_MODE_FIELDFRAME)
    awg |= AWG_CTRL_VSYNC_FIELD_NOT_FRAME;

  if (mode & STM_AWG_MODE_HSYNC_IGNORE)
    awg |= AWG_CTRL_HSYNC_NO_FORCE_IP_INCREMENT;

  if(!m_bIsDisabled)
    awg |= AWG_CTRL_ENABLE;

  DEBUGF2 (2, ("AWG_CTRL_0: %#.8lx\n", awg));
  WriteReg (AWG_CTRL_0, awg);

  return true;
}


void CSTb7100AWGAnalog::Enable (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_bIsDisabled = false;

  if(m_bIsFWLoaded)
  {
    /* enable AWG, using previous state */
    ULONG awg = ReadReg (AWG_CTRL_0) | AWG_CTRL_ENABLE;
    DEBUGF2 (2, ("AWG_CTRL_REG: %.8lx\n", awg));
    WriteReg (AWG_CTRL_0, awg);
  }
}


void CSTb7100AWGAnalog::Disable (void)
{
  DEBUGF2 (3, ("%s\n", __PRETTY_FUNCTION__));

  m_bIsDisabled = true;

  /* disable AWG, but leave state */
  ULONG awg = ReadReg (AWG_CTRL_0) & ~(AWG_CTRL_ENABLE);
  DEBUGF2 (2, ("AWG_CTRL_REG: %.8lx\n", awg));
  WriteReg (AWG_CTRL_0, awg);
}
