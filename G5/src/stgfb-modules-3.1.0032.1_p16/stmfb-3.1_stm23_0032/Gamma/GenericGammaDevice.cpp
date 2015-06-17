/***********************************************************************
 *
 * File: Gamma/GenericGammaDevice.cpp
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/GAL.h>
#include <Generic/Output.h>

#include "GenericGammaDevice.h"
#include "GammaCompositorPlane.h"
#include "GammaBlitter.h"

CGenericGammaDevice::CGenericGammaDevice(void): CDisplayDevice()
{
  DEBUGF2(2,("CGenericGammaDevice::CGenericGammaDevice in\n"));

  m_pGammaReg       = 0;
  m_numPlanes       = 0;
  m_numAccelerators = 0;

  for(unsigned i=0;i<CGAMMADEVICE_MAX_PLANES;i++)
  {
    m_hwPlanes[i] = 0;
  }

  for(unsigned i=0;i<N_ELEMENTS(m_graphicsAccelerators);i++)
  {
    m_graphicsAccelerators[i] = 0;
  }

  DEBUGF2(2,("CGenericGammaDevice::CGenericGammaDevice out\n"));
}


CGenericGammaDevice::~CGenericGammaDevice()
{
  DEBUGF2(2,("CGenericGammaDevice::~CGenericGammaDevice()\n"));

  for(unsigned i=0;i<m_numPlanes;i++)
  {
    delete m_hwPlanes[i];
  }

  DEBUGF2(2,("CGenericGammaDevice::~CGenericGammaDevice() deleted all planes\n"));

  for(unsigned i=0;i<m_numAccelerators;i++)
  {
    delete m_graphicsAccelerators[i];
  }

  DEBUGF2(2,("CGenericGammaDevice::~CGenericGammaDevice() deleted graphics engines\n"));

  for (int i = 0; i < CDISPLAYDEVICE_MAX_OUTPUTS; i++)
  {
    delete m_pOutputs[i];
    m_pOutputs[i] = 0L;
  }

  m_nOutputs = 0;

  DEBUGF2(2,("CGenericGammaDevice::~CGenericGammaDevice() deleted outputs\n"));

  g_pIOS->UnMapMemory(m_pGammaReg);

  DEBUGF2(2,("CGenericGammaDevice::~CGenericGammaDevice() out\n"));
}


bool CGenericGammaDevice::Create()
{
  // Call base class first
  if(!CDisplayDevice::Create())
  {
    DEBUGF2(2,("CGenericGammaDevice::Create - base class Create failed.\n"));
    return false;
  }

  return true;
}


CDisplayPlane* CGenericGammaDevice::GetPlane(stm_plane_id_t planenum) const
{
  for(int i=0;i<(int)m_numPlanes;i++)
  {
    if(m_hwPlanes[i]->GetID() == planenum)
    {
      return m_hwPlanes[i];
    }
  }
  return 0;
}


CGAL *CGenericGammaDevice::GetGAL(ULONG id) const
{
  if(id < (ULONG)m_numAccelerators)
    return m_graphicsAccelerators[id];

  return 0;
}


void CGenericGammaDevice::UpdateDisplay(COutput *pOutput)
{
  /*
   * Single implementation for devices with single or multiple VTGs; all planes
   * that are connected to outputs with the same timing ID as the pOutput
   * parameter are updated, regardless of which actual output they are being
   * displayed on.
   *
   * This caters for main/vcr style systems timed off the same VTG
   * such as 8K and 5528 or dual (or greater) pipeline systems such as the
   * STb7100. The key is that UpdateDisplay will be called by the OS interrupt/
   * tasklet handling _once_ per VTG interrupt for each VTG in the system,
   * _not_ each output. This allows planes to be connected to multiple
   * outputs (with the same timingID) and only be updated (correctly) once
   * per VTG interrupt.
   *
   * The interrupt "sense" is at the end of the field, so the
   * bottom field vsync indicates that the top field is now
   * in the process of being displayed.
   */
  bool   isTopFieldOnDisplay = (pOutput->GetCurrentVTGVSync() == STM_TOP_FIELD);
  TIME64 vsyncTime = pOutput->GetCurrentVSyncTime();

  const stm_mode_line_t *pCurrentMode = pOutput->GetCurrentDisplayMode();

  /*
   * If there is no valid display mode, we must have got a rouge VTG
   * interrupt from the hardware, ignore it.
   */
  if(!pCurrentMode)
    return;

  bool isDisplayInterlaced = (pCurrentMode->ModeParams.ScanType == SCAN_I);

  pOutput->UpdateHW();

  for(unsigned i=0;i<m_numPlanes;i++)
  {
    if(m_hwPlanes[i]->isActive() && (m_hwPlanes[i]->GetTimingID() == pOutput->GetTimingID()))
    {
      m_hwPlanes[i]->UpdateHW(isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);
    }
  }
}


void CGenericGammaDevice::UnlockPlanes(COutput *pOutput)
{
  /*
   * Like UpdateDisplay this unlocks (and flushed) all active planes that are
   * using the timing generator associated with the output, not just connected
   * to the output.
   */
  for(unsigned i=0;i<m_numPlanes;i++)
  {
    if(m_hwPlanes[i]->isActive() && (m_hwPlanes[i]->GetTimingID() == pOutput->GetTimingID()))
    {
      m_hwPlanes[i]->Unlock(0);
    }
  }
}
