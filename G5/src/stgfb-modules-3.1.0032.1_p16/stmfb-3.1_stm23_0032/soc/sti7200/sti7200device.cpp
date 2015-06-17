/***********************************************************************
 *
 * File: soc/sti7200/sti7200device.cpp
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7200reg.h"
#include "sti7200device.h"
#include "sti7200bdisp.h"

CSTi7200BDisp *CSTi7200Device::m_pBDisp = 0;
ULONG *CSTi7200Device::m_pBDispRegMapping = 0;
int CSTi7200Device::m_nBDispUseCount = 0;

//////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTi7200Device::CSTi7200Device(void): CGenericGammaDevice()

{
  DEBUGF2(2,("CSTi7200Device::CSTi7200Device in\n"));

  // Memory map the device registers.
  m_pGammaReg = (ULONG*)g_pIOS->MapMemory(STi7200_REGISTER_BASE, STi7200_REG_ADDR_SIZE);
  ASSERTF(m_pGammaReg,("CSTi7200Device::CSTi7200Device 'unable to map device registers'\n"));


  /*
   * On reset all of the clockgenB frequency synths are bypassed with the
   * SYSBCLKIN (30MHz) clock. Why????? Default everything to something
   * sensible by clearing the bottom 13 bits. Note that this gets done twice,
   * once for each created device, so we need to make sure we do not destroy
   * any setup in the upper bits.
   */
  ULONG val = ReadDevReg(STi7200_CLKGEN_BASE+CLKB_OUT_MUX_CFG) & ~0x1fff;
  WriteDevReg(STi7200_CLKGEN_BASE+CLKB_OUT_MUX_CFG, val);

  DEBUGF2(2,("CSTi7200Device::CSTi7200Device out\n"));
}


CSTi7200Device::~CSTi7200Device()
{
  DEBUGF2(2,("CSTi7200Device::~CSTi7200Device()\n"));

  /*
   * Override the base class destruction of registered graphics accelerators,
   * we need to leave this to the final destruction of the BDisp class.
   */
  for(unsigned i=0;i<N_ELEMENTS(m_graphicsAccelerators);i++)
  {
    m_graphicsAccelerators[i] = 0;
  }

  if(m_pBDisp)
  {
    m_nBDispUseCount--;
    if(m_nBDispUseCount == 0)
    {
      DEBUGF2(2,("CSTi7200Device::~CSTi7200Device() deleting singleton bdisp instance\n"));
      delete m_pBDisp;
      m_pBDisp = 0;
      g_pIOS->UnMapMemory(m_pBDispRegMapping);
      m_pBDispRegMapping = 0;
    }
  }

  DEBUGF2(2,("CSTi7200Device::~CSTi7200Device() out\n"));
}


bool CSTi7200Device::Create()
{ 
  DEBUGF2(2,("CSTi7200Device::Create out\n"));
  if(m_pBDisp == 0)
  {
    m_nBDispUseCount = 0;
    /*
     * As we have a shared object here, between multiple devices, we cannot
     * use the current device's register mapping because it might become invalid
     * while the BDisp is in use by another device. So we create a private
     * mapping of the register space for the BDisp class.
     */
    m_pBDispRegMapping = (ULONG*)g_pIOS->MapMemory(STi7200_REGISTER_BASE, STi7200_REG_ADDR_SIZE);
    
    m_pBDisp = new CSTi7200BDisp(m_pBDispRegMapping+(STi7200_BLITTER_BASE>>2));
    if(!m_pBDisp)
    {
      DEBUGF2(1,("CSTi7200Device::Create - failed to allocate BDisp object\n"));
      g_pIOS->UnMapMemory(m_pBDispRegMapping);
      m_pBDispRegMapping = 0;
      return false;
    }
    
    if(!m_pBDisp->Create())
    {
      DEBUGF2(1,("CSTi7200Device::Create - failed to create BDisp\n"));
      return false;
    }

  }
  
  m_nBDispUseCount++;
  
  return true;
}
