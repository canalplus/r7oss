/***********************************************************************
 *
 * File: soc/sti7200/sti7200cut2localdevice.cpp
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
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
#include <STMCommon/stmbdispregs.h>
#include <STMCommon/stmvirtplane.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>
#include <HDTVOutFormatter/stmtvoutteletext.h>

#include <Gamma/GDPBDispOutput.h>
#include <Gamma/GammaCompositorNULL.h>

#include "sti7200reg.h"
#include "sti7200cut2localdevice.h"
#include "sti7200cut2localauxoutput.h"
#include "sti7200cut2localmainoutput.h"
#include "sti7200hdmi.h"
#include "sti7200mixer.h"
#include "sti7200bdisp.h"
#include "sti7200gdp.h"
#include "sti7200cursor.h"
#include "sti7200flexvp.h"
#include "sti7200vdp.h"
#include "sti7200hdfdvo.h"

//////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTi7200Cut2LocalDevice::CSTi7200Cut2LocalDevice(void): CSTi7200Device()

{
  DENTRY();

  m_pFSynthHD0      = 0;
  m_pFSynthSD       = 0;
  m_pFSynth_FVP_166 = 0;
  m_pFSynth_IC_177  = 0;
  m_pFSynth_PIPE    = 0;
  m_pDENC           = 0;
  m_pVTG1           = 0;
  m_pVTG2           = 0;
  m_pMainMixer      = 0;
  m_pAuxMixer       = 0;
  m_pVideoPlug1     = 0;
  m_pVideoPlug2     = 0;
  m_pAWGAnalog      = 0;
  m_pGDPBDispOutput = 0;
  m_pTeletext       = 0;

  DEXIT();
}


CSTi7200Cut2LocalDevice::~CSTi7200Cut2LocalDevice()
{
  DENTRY();

  delete m_pDENC;
  delete m_pVTG1;
  delete m_pVTG2;
  delete m_pMainMixer;
  delete m_pAuxMixer;
  delete m_pVideoPlug1;
  delete m_pVideoPlug2;
  delete m_pFSynthHD0;
  delete m_pFSynthSD;
  delete m_pFSynth_FVP_166;
  delete m_pFSynth_IC_177;
  delete m_pFSynth_PIPE;
  delete m_pAWGAnalog;
  delete m_pTeletext;

  DEXIT();
}


bool CSTi7200Cut2LocalDevice::Create()
{
  CGammaCompositorGDP  *gdp;
  CSTi7200Cursor *cursor;
  CSTi7200FlexVP *main;
  CDEIVideoPipe  *aux;
  CGammaCompositorPlane *null;

  DENTRY();

  if(!CSTi7200Device::Create())
    return false;

  if((m_pFSynthHD0 = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS0_CLK1_CFG)) == 0)
  {
    DEBUGF(("%s - failed to create FSynthHD0\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pFSynthHD0->SetClockReference(STM_CLOCK_REF_30MHZ,0);

  if((m_pFSynthSD = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS1_CLK1_CFG)) == 0)
  {
    DEBUGF(("%s - failed to create FSynthSD\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pFSynthSD->SetClockReference(STM_CLOCK_REF_30MHZ,0);

  if((m_pFSynth_IC_177 = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS1_CLK4_CFG)) == 0)
  {
    DEBUGF(("%s - failed to create FSynth_IC_177\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * Ensure that some FSynth driven system clocks are running at the right
   * speed on Cut2. This should be done by the OS SoC setup, but isn't at the
   * moment.
   */
  m_pFSynth_IC_177->SetClockReference(STM_CLOCK_REF_30MHZ,0);
  m_pFSynth_IC_177->Start(185000000);

  if((m_pFSynth_FVP_166 = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS2_CLK3_CFG)) == 0)
  {
    DEBUGF(("%s - failed to create FSynth_FVP_166\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pFSynth_FVP_166->SetClockReference(STM_CLOCK_REF_30MHZ,0);
  m_pFSynth_FVP_166->Start(200000000);

  if((m_pFSynth_PIPE = new CSTmFSynthType2(this, STi7200_CLKGEN_BASE+CLKB_FS0_CLK3_CFG)) == 0)
  {
    DEBUGF(("%s - failed to create FSynth_PIPE\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pFSynth_PIPE->SetClockReference(STM_CLOCK_REF_30MHZ,0);
  m_pFSynth_PIPE->Start(150000000);

  /*
   * VTG appears to be clocked at 1xpixelclock on 7200
   */
  if((m_pVTG1 = new CSTmSDVTG(this, STi7200C2_LOCAL_DISPLAY_BASE+STi7200_VTG1_OFFSET, m_pFSynthHD0, false, STVTG_SYNC_POSITIVE)) == 0)
  {
    DEBUGF(("%s - failed to create VTG1\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((m_pVTG2 = new CSTmSDVTG(this, STi7200C2_LOCAL_DISPLAY_BASE+STi7200_VTG2_OFFSET, m_pFSynthSD, false, STVTG_SYNC_POSITIVE)) == 0)
  {
    DEBUGF(("%s - failed to create VTG2\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pTeletext = new CSTmTVOutTeletext(this, (STi7200C2_LOCAL_DISPLAY_BASE+STi7200_LOCAL_DENC_OFFSET),
                                            STi7200C2_LOCAL_AUX_TVOUT_BASE,
                                            STi7200_REGISTER_BASE,
                                            SDTP_DENC1_TELETEXT);
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


  m_pDENC = new CSTmTVOutDENC(this, (STi7200C2_LOCAL_DISPLAY_BASE+STi7200_LOCAL_DENC_OFFSET), m_pTeletext);
  if(!m_pDENC || !m_pDENC->Create())
  {
    DEBUGF(("%s - failed to create DENC\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((m_pMainMixer = new CSTi7200LocalMainMixer(this)) == 0)
  {
    DEBUGF(("%s - failed to create main mixer\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((m_pAuxMixer = new CSTi7200LocalAuxMixer(this)) == 0)
  {
    DEBUGF(("%s - failed to create aux mixer\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((m_pAWGAnalog = new CSTmHDFormatterAWG(this, STi7200C2_LOCAL_FORMATTER_BASE, STi7200C2_LOCAL_FORMATTER_AWG_SIZE)) == 0)
  {
    DEBUGF(("%s - failed to create analogue sync AWG\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * Planes
   */
  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STi7200_LOCAL_COMPOSITOR_BASE;

  gdp = new CSTi7200Cut2LocalGDP(OUTPUT_GDP1, ulCompositorBaseAddr+STi7200_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("%s - failed to create GDP1\n",__PRETTY_FUNCTION__));
    delete gdp;
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTi7200Cut2LocalGDP(OUTPUT_GDP2, ulCompositorBaseAddr+STi7200_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("%s - failed to create GDP2\n",__PRETTY_FUNCTION__));
    delete gdp;
    return false;
  }
  m_hwPlanes[1] = gdp;

  gdp = new CSTi7200Cut2LocalGDP(OUTPUT_GDP3, ulCompositorBaseAddr+STi7200_GDP3_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("%s - failed to create GDP3\n",__PRETTY_FUNCTION__));
    delete gdp;
    return false;
  }
  m_hwPlanes[2] = gdp;

  gdp = new CSTi7200Cut2LocalGDP(OUTPUT_GDP4, ulCompositorBaseAddr+STi7200_GDP4_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF(("%s - failed to create GDP4\n",__PRETTY_FUNCTION__));
    delete gdp;
    return false;
  }
  m_hwPlanes[3] = gdp;


  m_pVideoPlug1 = new CGammaVideoPlug(ulCompositorBaseAddr+STi7200_VID1_OFFSET, true, true);
  if(!m_pVideoPlug1)
  {
    DEBUGF(("%s - failed to create video plugs\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pVideoPlug2 = new CGammaVideoPlug(ulCompositorBaseAddr+STi7200_VID2_OFFSET, true, true);
  if(!m_pVideoPlug2)
  {
    DEBUGF(("%s - failed to create video plugs\n",__PRETTY_FUNCTION__));
    return false;
  }

  main = new CSTi7200FlexVP(OUTPUT_VID1, m_pVideoPlug1, ((ULONG)m_pGammaReg) + STi7200_FLEXVP_BASE);
  if(!main || !main->Create())
  {
    DEBUGF(("%s - failed to create VID1\n",__PRETTY_FUNCTION__));
    delete main;
    return false;
  }
  m_hwPlanes[4] = main;

  aux = new CSTi7200VDP(OUTPUT_VID2, m_pVideoPlug2, ((ULONG)m_pGammaReg) + STi7200_LOCAL_DEI_BASE);
  if(!aux || !aux->Create())
  {
    DEBUGF(("%s - failed to create VID2\n",__PRETTY_FUNCTION__));
    delete aux;
    return false;
  }
  m_hwPlanes[5] = aux;

  cursor = new CSTi7200Cursor(ulCompositorBaseAddr+STi7200_CUR_OFFSET);
  if(!cursor || !cursor->Create())
  {
    DEBUGF(("%s - failed to create CUR\n",__PRETTY_FUNCTION__));
    delete cursor;
    return false;
  }
  m_hwPlanes[6] = cursor;

  null = new CGammaCompositorNULL(OUTPUT_NULL);
  if(!null || !null->Create())
  {
    DERROR("failed to create NULL\n");
    delete null;
    return false;
  }
  m_hwPlanes[7] = null;

  m_numPlanes = 8;


  /*
   * Outputs
   */
  CSTi7200Cut2LocalMainOutput *pMainOutput;
  if((pMainOutput = new CSTi7200Cut2LocalMainOutput(this, m_pVTG1,m_pVTG2, m_pDENC, m_pMainMixer, m_pFSynthHD0, m_pAWGAnalog)) == 0)
  {
    DEBUGF(("%s - failed to create main output\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pOutputs[STi7200_OUTPUT_IDX_VDP0_MAIN] = pMainOutput;

  if((m_pOutputs[STi7200_OUTPUT_IDX_VDP1_MAIN] = new CSTi7200Cut2LocalAuxOutput(this, m_pVTG2, m_pDENC, m_pAuxMixer, m_pFSynthSD, m_pAWGAnalog, pMainOutput)) == 0)
  {
    DEBUGF(("%s - failed to create aux output\n",__PRETTY_FUNCTION__));
    return false;
  }

  CSTmHDMI *pHDMI;

  if((pHDMI = new CSTi7200HDMICut2(this, pMainOutput, m_pVTG1)) == 0)
  {
    DEBUGF(("%s - failed to allocate HDMI output\n",__PRETTY_FUNCTION__));
    return false;
  }

  if(!pHDMI->Create())
  {
    DEBUGF(("%s - failed to create HDMI output\n",__PRETTY_FUNCTION__));
    delete pHDMI;
    return false;
  }

  m_pOutputs[STi7200_OUTPUT_IDX_VDP0_HDMI] = pHDMI;

  CSTi7200HDFDVO *pFDVO;

  if((pFDVO = new CSTi7200HDFDVO(this, pMainOutput, STi7200C2_LOCAL_DISPLAY_BASE, STi7200C2_LOCAL_FORMATTER_BASE)) == 0)
  {
    DEBUGF(("%s - failed to create FDVO output\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_pOutputs[STi7200_OUTPUT_IDX_VDP0_DVO0] = pFDVO;

  pMainOutput->SetSlaveOutputs(pFDVO,pHDMI);

  m_pGDPBDispOutput = new CGDPBDispOutput(this, pMainOutput,
                                          m_pGammaReg + (STi7200_BLITTER_BASE>>2),
                                          STi7200_REGISTER_BASE+STi7200_BLITTER_BASE,
                                          BDISP_CQ1_BASE,1,2);

  if(m_pGDPBDispOutput == 0)
  {
    DERROR("failed to allocate BDisp virtual plane output");
    return false;
  }

  if(!m_pGDPBDispOutput->Create())
  {
    DERROR("failed to create BDisp virtual plane output");
    return false;
  }

  m_pOutputs[STi7200_OUTPUT_IDX_VDP0_GDP] = m_pGDPBDispOutput;
  m_nOutputs = 5;

  /*
   * Graphics
   */
  m_graphicsAccelerators[STi7200_BLITTER_IDX_KERNEL] = m_pBDisp->GetAQ(STi7200_BLITTER_AQ_KERNEL);
  m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP0_MAIN] = m_pBDisp->GetAQ(STi7200_BLITTER_AQ_VDP0_MAIN);
  m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP1_MAIN] = m_pBDisp->GetAQ(STi7200c2_BLITTER_AQ_VDP1_MAIN);
  if(!m_graphicsAccelerators[STi7200_BLITTER_IDX_KERNEL]
     || !m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP0_MAIN]
     || !m_graphicsAccelerators[STi7200c2_BLITTER_IDX_VDP1_MAIN])
  {
    DEBUGF(("%s - failed to get three AQs\n",__PRETTY_FUNCTION__));
    return false;
  }

  m_numAccelerators = 3;

  if(!CGenericGammaDevice::Create())
  {
    DEBUGF(("%s - base class Create failed\n",__PRETTY_FUNCTION__));
    return false;
  }

  DEXIT();

  return true;
}


CDisplayPlane* CSTi7200Cut2LocalDevice::GetPlane(stm_plane_id_t planeID) const
{
  CDisplayPlane *pPlane;

  if((pPlane = m_pGDPBDispOutput->GetVirtualPlane(planeID)) != 0)
    return pPlane;

  return CGenericGammaDevice::GetPlane(planeID);
}


void CSTi7200Cut2LocalDevice::UpdateDisplay(COutput *pOutput)
{
  /*
   * Generic HW processing
   */
  CGenericGammaDevice::UpdateDisplay(pOutput);

  /*
   * Virtual plane output processing related to the main output
   */
  if(m_pGDPBDispOutput->GetTimingID() == pOutput->GetTimingID())
    m_pGDPBDispOutput->UpdateHW();
}
