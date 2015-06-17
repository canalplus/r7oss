/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2remotedevice.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmtvoutteletext.h>
#include <Gamma/GammaCompositorNULL.h>

#include "sti7200reg.h"
#include "sti7200cut2remotedevice.h"
#include "sti7200remoteoutput.h"
#include "sti7200denc.h"
#include "sti7200mixer.h"
#include "sti7200bdisp.h"
#include "sti7200gdp.h"
#include "sti7200vdp.h"


//////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTi7200Cut2RemoteDevice::CSTi7200Cut2RemoteDevice(void): CSTi7200Device()

{
  m_pFSynth     = 0;
  m_pDENC       = 0;
  m_pVTG        = 0;
  m_pMixer      = 0;
  m_pVideoPlug  = 0;
  m_pTeletext   = 0;

  DEBUGF2(2,("CSTi7200Cut2RemoteDevice::CSTi7200Cut2RemoteDevice out\n"));
}


CSTi7200Cut2RemoteDevice::~CSTi7200Cut2RemoteDevice()
{
  delete m_pDENC;
  delete m_pVTG;
  delete m_pMixer;
  delete m_pVideoPlug;
  delete m_pFSynth;
  delete m_pTeletext;

}


bool CSTi7200Cut2RemoteDevice::Create()
{
  CGammaCompositorGDP   *gdp;
  CDEIVideoPipe         *disp;
  CGammaCompositorPlane *null;

  if(!CSTi7200Device::Create())
    return false;

  if((m_pFSynth = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS1_CLK2_CFG)) == 0)
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create FSynth\n"));
    return false;
  }

  m_pFSynth->SetClockReference(STM_CLOCK_REF_30MHZ,0);

  /*
   * VTG appears to be clocked at 1xpixelclock on 7200
   */
  if((m_pVTG = new CSTmSDVTG(this, STi7200C2_VTG3_BASE, m_pFSynth, false, STVTG_SYNC_POSITIVE)) == 0)
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create VTG\n"));
    return false;
  }

  m_pTeletext = new CSTmTVOutTeletext(this, STi7200C2_REMOTE_DENC_BASE,
                                            STi7200C2_REMOTE_TVOUT_BASE,
                                            STi7200_REGISTER_BASE,
                                            SDTP_DENC2_TELETEXT);
  /*
   * Slight difference for Teletext, if we fail to create it then clean up
   * and continue. We may not have been able to claim an FDMA channel and we
   * are not treating that as fatal.
   */
  if(m_pTeletext && !m_pTeletext->Create())
  {
    DERROR("failed to create Teletext");
    delete m_pTeletext;
    m_pTeletext = 0;
  }


  m_pDENC = new CSTi7200DENC(this, STi7200C2_REMOTE_DENC_BASE, m_pTeletext);
  if(!m_pDENC || !m_pDENC->Create())
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create DENC\n"));
    return false;
  }

  if((m_pMixer = new CSTi7200Cut2RemoteMixer(this)) == 0)
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create main mixer\n"));
    return false;
  }


  m_numPlanes = 4;
  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STi7200_REMOTE_COMPOSITOR_BASE;

  gdp = new CSTi7200RemoteGDP(OUTPUT_GDP1, ulCompositorBaseAddr+STi7200_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create GDP1\n"));
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTi7200RemoteGDP(OUTPUT_GDP2, ulCompositorBaseAddr+STi7200_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create GDP2\n"));
    return false;
  }
  m_hwPlanes[1] = gdp;

  m_pVideoPlug = new CGammaVideoPlug(ulCompositorBaseAddr+STi7200_VID1_OFFSET, true, true);
  if(!m_pVideoPlug)
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create video plugs\n"));
    return false;
  }

  disp = new CSTi7200VDP(OUTPUT_VID1,m_pVideoPlug, ((ULONG)m_pGammaReg) + STi7200_REMOTE_DEI_BASE);
  if(!disp || !disp->Create())
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create VID1\n"));
    return false;
  }
  m_hwPlanes[2] = disp;

  null = new CGammaCompositorNULL(OUTPUT_NULL);
  if(!null || !null->Create())
  {
    DERROR("failed to create NULL\n");
    delete null;
    return false;
  }
  m_hwPlanes[3] = null;

  CSTi7200Cut2RemoteOutput *pMain;

  if((pMain = new CSTi7200Cut2RemoteOutput(this, m_pVTG, m_pDENC, m_pMixer, m_pFSynth)) == 0)
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - failed to create primary output\n"));
    return false;
  }

  m_pOutputs[STi7200_OUTPUT_IDX_VDP2_MAIN] = pMain;

  m_nOutputs = 1;


  /*
   * Graphics
   */
  m_graphicsAccelerators[STi7200_BLITTER_IDX_KERNEL] = m_pBDisp->GetAQ(STi7200_BLITTER_AQ_KERNEL);
  m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP2_MAIN] = m_pBDisp->GetAQ(STi7200c2_BLITTER_AQ_VDP2_MAIN);
  if(!m_graphicsAccelerators[STi7200_BLITTER_IDX_KERNEL]
     || !m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP2_MAIN])
  {
    DEBUGF(("%s - failed to get two AQs\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_numAccelerators = 2;

  if(!CGenericGammaDevice::Create())
  {
    DEBUGF(("CSTi7200Cut2RemoteDevice::Create - base class Create failed\n"));
    return false;
  }

  DEBUGF2(2,("CSTi7200Cut2RemoteDevice::Create out\n"));

  return true;
}
