/***********************************************************************
 *
 * File: soc/sti7106/sti7106device.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include "sti7106reg.h"
#include "sti7106device.h"
#include "sti7106hdmi.h"
#include "sti7106mixer.h"
#include "sti7106bdisp.h"

#include <soc/sti7111/sti7111vdp.h>
#include <soc/sti7111/sti7111dei.h>

CSTi7106Device::CSTi7106Device (void): CSTi7105Device ()
{
  DEBUGF2 (2, (FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  m_sharedVideoPlaneID = OUTPUT_VID1;
  m_sharedGDPPlaneID = OUTPUT_GDP1;

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}


CSTi7106Device::~CSTi7106Device (void)
{
  DEBUGF2 (2, (FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}


bool CSTi7106Device::CreateMixers(void)
{
  DENTRY();

  if((m_pMainMixer = new CSTi7106MainMixer(this,STi7111_COMPOSITOR_BASE+STi7111_MIXER1_OFFSET)) == 0)
  {
    DERROR("failed to create main mixer\n");
    return false;
  }

  if((m_pAuxMixer = new CSTi7106AuxMixer(this,STi7111_COMPOSITOR_BASE+STi7111_MIXER2_OFFSET)) == 0)
  {
    DERROR("failed to create aux mixer\n");
    return false;
  }

  DEXIT();
  return true;
}


bool CSTi7106Device::CreateVideoPlanes(CGammaCompositorPlane **vid1,CGammaCompositorPlane **vid2)
{
  CDEIVideoPipe *disp;

  DENTRY();

  disp = new CSTi7111DEI(OUTPUT_VID2, m_pVideoPlug2, ((ULONG)m_pGammaReg) + STi7111_HD_BASE);
  if(!disp || !disp->Create())
  {
    DERROR("failed to create VID1\n");
    delete disp;
    return false;
  }
  *vid1 = disp;

  disp = new CSTi7111VDP(OUTPUT_VID1, m_pVideoPlug1, ((ULONG)m_pGammaReg) + STi7111_SD_BASE);
  if(!disp || !disp->Create())
  {
    DERROR("failed to create VID2\n");
    delete disp;
    return false;
  }
  *vid2 = disp;

  DEXIT();
  return true;

}

bool CSTi7106Device::CreateHDMIOutput(void)
{
  CSTmHDMI *pHDMI;

  if((pHDMI = new CSTi7106HDMI(this, m_pOutputs[STi7111_OUTPUT_IDX_VDP0_MAIN], m_pVTG1)) == 0)
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

bool CSTi7106Device::CreateGraphics(void)
{
  m_pBDisp = new CSTi7106BDisp(m_pGammaReg + (STi7111_BLITTER_BASE>>2));
  if(!m_pBDisp)
  {
    DERROR("failed to allocate BDisp object\n");
    return false;
  }

  if(!m_pBDisp->Create())
  {
    DERROR("failed to create BDisp\n");
    return false;
  }

  m_graphicsAccelerators[STi7111_BLITTER_IDX_KERNEL] = m_pBDisp->GetAQ(STi7111_BLITTER_AQ_KERNEL);
  m_graphicsAccelerators[STi7111_BLITTER_IDX_VDP0_MAIN] = m_pBDisp->GetAQ(STi7111_BLITTER_AQ_VDP0_MAIN);
  m_graphicsAccelerators[STi7111_BLITTER_IDX_VDP1_MAIN] = m_pBDisp->GetAQ(STi7111_BLITTER_AQ_VDP1_MAIN);
  if(!m_graphicsAccelerators[STi7111_BLITTER_IDX_KERNEL]
     || !m_graphicsAccelerators[STi7111_BLITTER_IDX_VDP0_MAIN]
     || !m_graphicsAccelerators[STi7111_BLITTER_IDX_VDP1_MAIN])
  {
    DERROR("failed to get three AQs\n");
    return false;
  }

  m_numAccelerators = 3;
  return true;
}


/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 * When this is called only g_pIOS will have been initialised.
 */
CDisplayDevice *
AnonymousCreateDevice (unsigned deviceid)
{
  if (deviceid == 0)
    return new CSTi7106Device ();

  return 0;
}
