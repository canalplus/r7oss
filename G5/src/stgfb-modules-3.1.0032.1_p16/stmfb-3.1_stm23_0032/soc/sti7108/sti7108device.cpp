/***********************************************************************
 *
 * File: soc/sti7108/sti7108device.cpp
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

#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmbdispregs.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmtvoutteletext.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include <Gamma/GDPBDispOutput.h>
#include <Gamma/GammaCompositorNULL.h>

#include "sti7108reg.h"
#include "sti7108device.h"
#include "sti7108auxoutput.h"
#include "sti7108mainoutput.h"
#include "sti7108mixer.h"
#include "sti7108bdisp.h"
#include "sti7108gdp.h"
#include "sti7108cursor.h"
#include "sti7108vdp.h"
#include "sti7108dvo.h"
#include "sti7108hdmi.h"
#include "sti7108clkdivider.h"

CSTi7108Device::CSTi7108Device(void): CGenericGammaDevice()

{
  DENTRY();

  m_pGammaReg = (ULONG*)g_pIOS->MapMemory(STi7108_REGISTER_BASE, STi7108_REG_ADDR_SIZE);
  ASSERTF(m_pGammaReg,("CSTi7108Device::CSTi7108Device 'unable to map device registers'\n"));

  /*
   * Global setup of the display clock registers.
   */
  WriteDevReg(STi7108_CLKGEN_BASE + CKGB_LCK, CKGB_LCK_UNLOCK);
  WriteDevReg(STi7108_CLKGEN_BASE + CKGB_CLK_REF_SEL, CKGB_REF_SEL_SYS_CLKIN);
  WriteDevReg(STi7108_CLKGEN_BASE + CKGB_DISPLAY_CFG, CKGB_CFG_PIX_SD_FS1(CKGB_CFG_BYPASS));
  WriteDevReg(STi7108_CLKGEN_BASE + CKGB_CLK_SRC, CKGB_SRC_PIXSD_FS1_NOT_FS0);

  m_pFSynthHD       = 0;
  m_pFSynthSD       = 0;
  m_pDENC           = 0;
  m_pVTG1           = 0;
  m_pVTG2           = 0;
  m_pMainMixer      = 0;
  m_pAuxMixer       = 0;
  m_pVideoPlug1     = 0;
  m_pVideoPlug2     = 0;
  m_pAWGAnalog      = 0;
  m_pBDisp          = 0;
  m_pTeletext       = 0;
  m_pClkDivider     = 0;

  DEXIT();
}


CSTi7108Device::~CSTi7108Device()
{
  DENTRY();

  delete m_pDENC;
  delete m_pVTG1;
  delete m_pVTG2;
  delete m_pMainMixer;
  delete m_pAuxMixer;
  delete m_pVideoPlug1;
  delete m_pVideoPlug2;
  delete m_pFSynthHD;
  delete m_pFSynthSD;
  delete m_pAWGAnalog;
  delete m_pTeletext;
  delete m_pClkDivider;

  /*
   * Override the base class destruction of registered graphics accelerators,
   * we need to leave this to the destruction of the BDisp class.
   */
  for(unsigned int i=0;i<N_ELEMENTS(m_graphicsAccelerators);i++)
    m_graphicsAccelerators[i] = 0;
  delete m_pBDisp;

  DEXIT();
}


bool CSTi7108Device::Create()
{
  DENTRY();

  if(!CreateInfrastructure())
    return false;

  if(!CreatePlanes())
    return false;

  if(!CreateOutputs())
    return false;

  if(!CreateGraphics())
    return false;

  if(!CGenericGammaDevice::Create())
  {
    DERROR("CSTi7108Device::Create - base class Create failed\n");
    return false;
  }

  DEXIT();

  return true;
}


bool CSTi7108Device::CreateInfrastructure(void)
{
  if((m_pFSynthHD = new CSTmFSynthType1(this, STi7108_CLKGEN_BASE+CKGB_FS0_MD1)) == 0)
  {
    DERROR("failed to create FSynthHD\n");
    return false;
  }

  m_pFSynthHD->SetClockReference(STM_CLOCK_REF_30MHZ,0);
  m_pFSynthHD->SetDivider(1);
  m_pFSynthHD->Start(108000000);

  if((m_pFSynthSD = new CSTmFSynthType1(this, STi7108_CLKGEN_BASE+CKGB_FS1_MD1)) == 0)
  {
    DERROR("failed to create FSynthSD\n");
    return false;
  }

  m_pFSynthSD->SetClockReference(STM_CLOCK_REF_30MHZ,0);
  m_pFSynthSD->SetDivider(1);
  m_pFSynthSD->Start(108000000);

  if((m_pClkDivider = new CSTi7108ClkDivider(this)) == 0)
  {
    DERROR("failed to create ClkDivider\n");
    return false;
  }

  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_0, STM_CLK_DIV_1);
  m_pClkDivider->Enable(STM_CLK_DISP_HD, STM_CLK_SRC_0, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_PIX_SD, STM_CLK_SRC_1, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_DISP_SD, STM_CLK_SRC_1, STM_CLK_DIV_8);
  m_pClkDivider->Enable(STM_CLK_656, STM_CLK_SRC_0, STM_CLK_DIV_2);

  if((m_pVTG1 = new CSTmSDVTG(this, STi7108_VTG1_BASE, m_pFSynthHD, false, STVTG_SYNC_POSITIVE)) == 0)
  {
    DERROR("failed to create VTG1\n");
    return false;
  }

  if((m_pVTG2 = new CSTmSDVTG(this, STi7108_VTG2_BASE, m_pFSynthSD, false, STVTG_SYNC_POSITIVE)) == 0)
  {
    DERROR("failed to create VTG2\n");
    return false;
  }

  m_pTeletext = new CSTmTVOutTeletext(this, STi7108_DENC_BASE,
                                            STi7108_AUX_TVOUT_BASE,
                                            STi7108_REGISTER_BASE,
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

  m_pDENC = new CSTmTVOutDENC(this, STi7108_DENC_BASE, m_pTeletext);
  if(!m_pDENC || !m_pDENC->Create())
  {
    DERROR("failed to create DENC\n");
    return false;
  }

  if((m_pMainMixer = new CSTi7108MainMixer(this,STi7108_COMPOSITOR_BASE+STi7108_MIXER1_OFFSET)) == 0)
  {
    DERROR("failed to create main mixer\n");
    return false;
  }

  if((m_pAuxMixer = new CSTi7108AuxMixer(this,STi7108_COMPOSITOR_BASE+STi7108_MIXER2_OFFSET)) == 0)
  {
    DERROR("failed to create aux mixer\n");
    return false;
  }

  if((m_pAWGAnalog = new CSTmHDFormatterAWG(this,STi7108_HD_FORMATTER_BASE,STi7108_HD_FORMATTER_AWG_SIZE)) == 0)
  {
    DERROR("failed to create analogue sync AWG\n");
    return false;
  }

  return true;
}


bool CSTi7108Device::CreatePlanes(void)
{
  CGammaCompositorGDP    *gdp;
  CGammaCompositorCursor *cursor;
  CDEIVideoPipe          *disp;
  CGammaCompositorPlane  *null;

  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STi7108_COMPOSITOR_BASE;

  gdp = new CSTi7108GDP(OUTPUT_GDP1, ulCompositorBaseAddr+STi7108_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DERROR("failed to create GDP1\n");
    delete gdp;
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTi7108GDP(OUTPUT_GDP2, ulCompositorBaseAddr+STi7108_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DERROR("failed to create GDP2\n");
    delete gdp;
    return false;
  }
  m_hwPlanes[1] = gdp;

  gdp = new CSTi7108GDP(OUTPUT_GDP3, ulCompositorBaseAddr+STi7108_GDP3_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DERROR("failed to create GDP3\n");
    delete gdp;
    return false;
  }
  m_hwPlanes[2] = gdp;

  gdp = new CSTi7108GDP(OUTPUT_GDP4, ulCompositorBaseAddr+STi7108_GDP4_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DERROR("failed to create GDP4\n");
    delete gdp;
    return false;
  }
  m_hwPlanes[3] = gdp;

  m_pVideoPlug1 = new CGammaVideoPlug(ulCompositorBaseAddr+STi7108_VID1_OFFSET, true, true);
  if(!m_pVideoPlug1)
  {
    DERROR("failed to create video plugs\n");
    return false;
  }

  m_pVideoPlug2 = new CGammaVideoPlug(ulCompositorBaseAddr+STi7108_VID2_OFFSET, true, true);
  if(!m_pVideoPlug2)
  {
    DERROR("failed to create video plugs\n");
    return false;
  }

  /*
   * TODO: Implement and add the HQVDP video pipeline
   */

  disp = new CSTi7108VDP(OUTPUT_VID1, m_pVideoPlug1, ((ULONG)m_pGammaReg) + STi7108_SD_BASE);
  if(!disp || !disp->Create())
  {
    DERROR("failed to create VID1\n");
    delete disp;
    return false;
  }
  m_hwPlanes[4] = disp;

  cursor = new CSTi7108Cursor(ulCompositorBaseAddr+STi7108_CUR_OFFSET);
  if(!cursor || !cursor->Create())
  {
    DERROR("failed to create CUR\n");
    delete cursor;
    return false;
  }
  m_hwPlanes[5] = cursor;

  null = new CGammaCompositorNULL(OUTPUT_NULL);
  if(!null || !null->Create())
  {
    DERROR("failed to create NULL\n");
    delete null;
    return false;
  }
  m_hwPlanes[6] = null;

  m_numPlanes = 7;
  return true;
}


bool CSTi7108Device::CreateOutputs(void)
{
  CSTi7108MainOutput *pMainOutput;
  if((pMainOutput = new CSTi7108MainOutput(this, m_pVTG1,m_pVTG2, m_pDENC, m_pMainMixer, m_pFSynthHD, m_pAWGAnalog, m_pClkDivider)) == 0)
  {
    DERROR("failed to create main output\n");
    return false;
  }
  m_pOutputs[STi7108_OUTPUT_IDX_VDP0_MAIN] = pMainOutput;

  if((m_pOutputs[STi7108_OUTPUT_IDX_VDP1_MAIN] = new CSTi7108AuxOutput(this, m_pVTG2, m_pDENC, m_pAuxMixer, m_pFSynthSD, m_pAWGAnalog, pMainOutput, m_pClkDivider)) == 0)
  {
    DERROR("failed to create aux output\n");
    return false;
  }

  if(!this->CreateHDMIOutput())
    return false;

  CSTi7108DVO *pFDVO = new CSTi7108DVO (this, pMainOutput, m_pClkDivider, STi7108_FLEXDVO_BASE, STi7108_HD_FORMATTER_BASE);
  if(!pFDVO)
  {
    DERROR("failed to create FDVO output\n");
    return false;
  }
  m_pOutputs[STi7108_OUTPUT_IDX_DVO0] = pFDVO;

  pMainOutput->SetSlaveOutputs(m_pOutputs[STi7108_OUTPUT_IDX_DVO0],m_pOutputs[STi7108_OUTPUT_IDX_VDP0_HDMI]);

  m_nOutputs = 4;

  return true;
}


bool CSTi7108Device::CreateHDMIOutput(void)
{
  CSTmHDMI *pHDMI = 0;
  if((pHDMI = new CSTi7108HDMI(this, m_pOutputs[STi7108_OUTPUT_IDX_VDP0_MAIN], m_pVTG1, m_pClkDivider)) == 0)
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


  m_pOutputs[STi7108_OUTPUT_IDX_VDP0_HDMI] = pHDMI;
  return true;
}


bool CSTi7108Device::CreateGraphics(void)
{
  m_pBDisp = new CSTi7108BDisp(m_pGammaReg + (STi7108_BLITTER_BASE>>2));
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

  m_graphicsAccelerators[STi7108_BLITTER_IDX_KERNEL] = m_pBDisp->GetAQ(STi7108_BLITTER_AQ_KERNEL);
  m_graphicsAccelerators[STi7108_BLITTER_IDX_VDP0_MAIN] = m_pBDisp->GetAQ(STi7108_BLITTER_AQ_VDP0_MAIN);
  m_graphicsAccelerators[STi7108_BLITTER_IDX_VDP1_MAIN] = m_pBDisp->GetAQ(STi7108_BLITTER_AQ_VDP1_MAIN);
  if(!m_graphicsAccelerators[STi7108_BLITTER_IDX_KERNEL]
     || !m_graphicsAccelerators[STi7108_BLITTER_IDX_VDP0_MAIN]
     || !m_graphicsAccelerators[STi7108_BLITTER_IDX_VDP1_MAIN])
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
CDisplayDevice* AnonymousCreateDevice(unsigned deviceid)
{
  if(deviceid == 0)
    return new CSTi7108Device();

  return 0;
}
