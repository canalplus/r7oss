/***********************************************************************
 *
 * File: soc/stb7100/stb7100device.cpp
 * Copyright (c) 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <STMCommon/stmvirtplane.h>
#include <STMCommon/stmfsynth.h>

#include <Gamma/GenericGammaDefs.h>
#include <Gamma/GammaCompositorGDP.h>
#include <Gamma/GammaCompositorVideoPlane.h>
#include <Gamma/GammaCompositorDISP.h>
#include <Gamma/GammaBlitter.h>
#include <Gamma/GDPBDispOutput.h>
#include <Gamma/GammaCompositorNULL.h>

#include "stb7100reg.h"
#include "stb7100denc.h"
#include "stb7100mixer.h"
#include "stb7100vtg.h"
#include "stb7100hddisp.h"
#include "stb7100analogueouts.h"
#include "stb7100dvo.h"
#include "stb7100hdmi.h"
#include "stb7100AWGAnalog.h"
#include "stb7100device.h"
#include "stb710xgdp.h"
#include "stb710xcursor.h"
#include "stb7109dei.h"
#include "stb7109bdisp.h"


/*
 * Although this device originally only supported STb7100, it now also
 * supports 7109Cut2, which is very similar. Now it also supports 7109Cut3
 * which has some additional extensions in functionality but is still close
 * enough not to justify adding a whole new chip directory (we think).
 */


//////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSTb7100Device::CSTb7100Device(void): CGenericGammaDevice()
{
  DEBUGF2(2,("CSTb7100Device::CSTb7100Device\n"));

  // Memory map device registers
  m_pGammaReg = (ULONG*)g_pIOS->MapMemory(STb7100_REGISTER_BASE, STb7100_REG_ADDR_SIZE);
  ASSERTF(m_pGammaReg,("CSTb7100Device::CSTb7100Device 'unable to map device registers'\n"));

  m_pVTG1       = 0;
  m_pVTG2       = 0;
  m_pDENC       = 0;
  m_pMixer1     = 0;
  m_pMixer2     = 0;
  m_pHDFSynth   = 0;
  m_pSDFSynth   = 0;
  m_pVideoPlug1 = 0;
  m_pVideoPlug2 = 0;
  m_pAWGAnalog  = 0;
  m_pBDisp      = 0;

  m_pGDPBDispOutput = 0;

  /*
   * Global setup of the display clock registers.
   */
  WriteDevReg(STb7100_CLKGEN_BASE + CKGB_LCK, CKGB_LCK_UNLOCK);
  WriteDevReg(STb7100_CLKGEN_BASE + CKGB_CLK_REF_SEL, CKGB_REF_SEL_INTERNAL);

  ULONG chipid = ReadDevReg(STb7100_SYSCFG_BASE);

  m_bIs7109     = (((chipid>>12)&0x3ff) == 0x02c);
  m_chipVersion = (chipid>>28)+1;

  if(!m_bIs7109)
  {
    WriteDevReg(0x01243000, 0x5);
    WriteDevReg(0x01243004, 0x4);
    WriteDevReg(0x01243008, 0x2);
    WriteDevReg(0x0124300C, 0x1);
    WriteDevReg(0x01243010, 0x3);
  }
  else
  {
    WriteDevReg(0x01243000, 0xf);
    WriteDevReg(0x01243004, 0x2);
    WriteDevReg(0x01243008, 0x1);
    WriteDevReg(0x0124300C, 0x3);
  }

  WriteDevReg(0x01216200, 0x1);
  WriteDevReg(0x01216204, 0x5);
  WriteDevReg(0x01216208, 0x3);
  WriteDevReg(0x0121620C, 0x2);
  WriteDevReg(0x01216210, 0x4);


  DEBUGF2(2,("CSTb7100Device::CSTb7100Device this is a STb%s Cut %d.x\n",m_bIs7109?"7109":"7100",m_chipVersion));

  DEBUGF2(2,("CSTb7100Device::CSTb7100Device out\n"));
}


CSTb7100Device::~CSTb7100Device()
{
  DEBUGF2(2,("CSTb7100Device::~CSTb7100Device\n"));
  delete m_pVTG1;
  delete m_pVTG2;
  delete m_pDENC;
  delete m_pMixer1;
  delete m_pMixer2;
  delete m_pHDFSynth;
  delete m_pSDFSynth;
  delete m_pVideoPlug1;
  delete m_pVideoPlug2;
  delete m_pAWGAnalog;

  if(m_pBDisp)
  {
    /*
     * Override the base class destruction of registered graphics accelerators,
     * we need to leave this to the final destruction of the BDisp class.
     */
    for(unsigned int i=0;i<N_ELEMENTS(m_graphicsAccelerators);++i)
      m_graphicsAccelerators[i] = 0;
    delete m_pBDisp;
  }
  /* otherwise, we have a simple GammaBlitter */

  DEBUGF2(2,("CSTb7100Device::~CSTb7100Device out\n"));
}


bool CSTb7100Device::Create()
{
  DEBUGF2(2,("CSTb7100Device::Create IN\n"));
  stm_plane_id_t sharedPlane;

  if(m_bIs7109 && (m_chipVersion <2))
  {
    DEBUGF2(1,("CSTb7100Device::Create - STb7109 Cut1.0 Not Supported\n"));
    return false;
  }

  if((m_pHDFSynth = new CSTmFSynthType1(this, STb7100_CLKGEN_BASE+CKGB_FS0_MD1)) == 0)
  {
    DEBUGF(("CSTi7111Device::Create - failed to create FSynthHD\n"));
    return false;
  }

  m_pHDFSynth->SetClockReference(STM_CLOCK_REF_30MHZ,0);

  if((m_pSDFSynth = new CSTmFSynthType1(this, STb7100_CLKGEN_BASE+CKGB_FS1_MD1)) == 0)
  {
    DEBUGF(("CSTi7111Device::Create - failed to create FSynthSD\n"));
    return false;
  }

  m_pSDFSynth->SetClockReference(STM_CLOCK_REF_30MHZ,0);

  if(!m_pHDFSynth || !m_pSDFSynth)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create Frequency Synthesizers\n"));
    return false;
  }


  if((m_pVTG1 = new CSTb7100VTG1(this, STb7100_VOS_BASE, m_pHDFSynth)) == 0)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create VTG1\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::Create - m_pVTG1 = %p\n", m_pVTG1));

  if((m_pVTG2 = new CSTb7100VTG2(this, STb7100_VOS_BASE, m_pSDFSynth, m_pVTG1)) == 0)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create VTG2\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::Create - m_pVTG2 = %p\n", m_pVTG2));

  m_pDENC = new CSTb7100DENC(this, STb7100_DENC_BASE);
  if(!m_pDENC || !m_pDENC->Create())
  {
    DEBUGF(("CSTb7100Device::Create - failed to create DENC\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::Create - m_pDENC = %p\n", m_pDENC));

  if((m_pAWGAnalog = new CSTb7100AWGAnalog(this)) == 0)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create analogue sync AWG\n"));
    return false;
  }

  if(m_bIs7109 && (m_chipVersion >2))
  {
    m_pMixer1   = new CSTb7109Cut3MainMixer(this);
    m_pMixer2   = new CSTb7109Cut3AuxMixer(this);
    sharedPlane = OUTPUT_GDP3;
  }
  else
  {
    /*
     * 7100 and 7109 Cut2.0 have the same mixer configuration
     */
    m_pMixer1   = new CSTb7100MainMixer(this);
    m_pMixer2   = new CSTb7100AuxMixer(this);
    sharedPlane = OUTPUT_GDP2;
  }

  if(!m_pMixer1 || !m_pMixer2)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create mixers\n"));
    return false;
  }

  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STb7100_COMPOSITOR_BASE;
  bool useMPR = false;

  /*
   * On 7109Cut3 we need to use the video plug color conversion matrix
   * programming explicitly for 601 or 709 YUV->RGB conversion. On other
   * chips we use internal matricies selected by the plug's control register.
   */
  if(m_bIs7109 && (m_chipVersion >2))
    useMPR = true;

  m_pVideoPlug1 = new CGammaVideoPlug(ulCompositorBaseAddr+STb7100_VID1_OFFSET, true, useMPR);
  m_pVideoPlug2 = new CGammaVideoPlug(ulCompositorBaseAddr+STb7100_VID2_OFFSET, true, useMPR);
  if(!m_pVideoPlug1 || !m_pVideoPlug2)
  {
    DEBUGF(("CSTb7100Device::Create - failed to create video plugs\n"));
    return false;
  }

  if(!CreatePlanes())
    return false;

  if(!CreateOutputs(sharedPlane))
    return false;

  if(!CreateGraphics())
    return false;


  /*
   * Call the base class create now all the device specific bits are
   * in place.
   */
  if(!CGenericGammaDevice::Create())
  {
    DEBUGF(("CSTb7100Device::Create - base class Create failed\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::Create OUT\n"));

  return true;
}


bool CSTb7100Device::CreateOutputs(stm_plane_id_t sharedPlane)
{
  CSTb7100MainOutput *pMainOut = new CSTb7100MainOutput(this, m_pVTG1,
                                                              m_pVTG2,
                                                              m_pDENC,
                                                              m_pMixer1,
                                                              m_pHDFSynth,
                                                              m_pSDFSynth,
                                                              m_pAWGAnalog,
                                                              sharedPlane);

  if(!pMainOut)
  {
    DEBUGF(("CSTb7100Device::CreateOutputs - failed to create HD/Main output\n"));
    return false;
  }

  /*
   * On 7109Cut3 and above we use a 108MHz reference clock for SD/ED modes
   * instead of a 54MHz clock. See comments in the analogue output
   * implementation for more information.
   */
  if(m_bIs7109 && (m_chipVersion >2))
    pMainOut->Enable108MHzClockForSD();

  m_pOutputs[0] = pMainOut;
  DEBUGF2(2,("CSTb7100Device::CreateOutputs - HD/Main output = %p\n",m_pOutputs[0]));

  m_pOutputs[1] = new CSTb7100AuxOutput(this, m_pVTG2, m_pDENC, m_pMixer2, m_pSDFSynth, sharedPlane);
  if(!m_pOutputs[1])
  {
    DEBUGF(("CSTb7100Device::CreateOutputs - failed to create SD/Aux output\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::CreateOutputs - SD/Aux output = %p\n",m_pOutputs[1]));

  if(m_bIs7109 && (m_chipVersion >2))
    m_pOutputs[2] = new CSTb7109Cut3DVO(this, pMainOut);
  else
    m_pOutputs[2] = new CSTb7100DVO(this, pMainOut);

  if(!m_pOutputs[2])
  {
    DEBUGF(("CSTb7100Device::CreateOutputs - failed to create Digital output\n"));
    return false;
  }

  DEBUGF2(2,("CSTb7100Device::CreateOutputs - DVO output = %p\n",m_pOutputs[2]));

  /*
   * HDMI is available on 7109 >= Cut2.0 and 7100 >= Cut3.0
   */
  CSTmHDMI *pHDMI;
  if(m_bIs7109 && (m_chipVersion >= 3))
  {
    /*
     * 7109Cut3 and beyond have there own specialisation
     */
    DEBUGF2(2,("CSTb7100Device::CreateOutputs - HDMI available\n"));

    pHDMI = new CSTb7109Cut3HDMI(this, pMainOut, m_pVTG1);
    if(!pHDMI)
    {
      DEBUGF(("CSTb7100Device::CreateOutputs - failed to allocate HDMI output\n"));
      return false;
    }

    if(!pHDMI->Create())
    {
      DEBUGF(("CSTb7100Device::CreateOutputs - failed to create HDMI output\n"));
      delete pHDMI;
      return false;
    }

    m_pOutputs[3] = pHDMI;

    DEBUGF2(2,("CSTb7100Device::CreateOutputs - HDMI output = %p\n",m_pOutputs[3]));

    m_pGDPBDispOutput = new CGDPBDispOutput(this, pMainOut,
                                            m_pGammaReg+(STb7109_BDISP_BASE>>2),
                                            STb7100_REGISTER_BASE+STb7109_BDISP_BASE,
                                            BDISP_CQ1_BASE,1,1);

    if(m_pGDPBDispOutput == 0)
    {
      DERROR("failed to create BDisp virtual plane output");
      return false;
    }

    if(!m_pGDPBDispOutput->Create())
    {
      DERROR("failed to create BDisp virtual plane output");
      return false;
    }

    m_pOutputs[4] = m_pGDPBDispOutput;

    m_nOutputs = 5;
  }
  else if((m_bIs7109 && m_chipVersion == 2) || (!m_bIs7109 && m_chipVersion >=3))
  {
    /*
     * 7109Cut2 and 7100 >= Cut3 share the same implementation
     */
    DEBUGF2(2,("CSTb7100Device::CreateOutputs - HDMI available\n"));

    pHDMI = new CSTb7100HDMI(this, pMainOut, m_pVTG1);
    if(!pHDMI)
    {
      DEBUGF(("CSTb7100Device::CreateOutputs - failed to allocate HDMI output\n"));
      return false;
    }

    if(!pHDMI->Create())
    {
      DEBUGF(("CSTb7100Device::CreateOutputs - failed to create HDMI output\n"));
      delete pHDMI;
      return false;
    }

    m_pOutputs[3] = pHDMI;

    DEBUGF2(2,("CSTb7100Device::CreateOutputs - HDMI output = %p\n",m_pOutputs[3]));
    m_nOutputs = 4;
  }
  else
  {
    DEBUGF2(2,("CSTb7100Device::CreateOutputs - HDMI not supported on pre Cut3.0 7100 chips\n"));
    m_pOutputs[3] = 0;
    m_nOutputs    = 3;
  }

  // Tell the main output about the digital outputs that are slaved to its VTG
  pMainOut->SetSlaveOutputs(m_pOutputs[2], m_pOutputs[3]);

  return true;
}


bool CSTb7100Device::CreateGraphics()
{
  if(!m_bIs7109)
  {
    CGammaBlitter *blitter;

    blitter = new CGammaBlitter(m_pGammaReg+(STb7100_BLITTER_BASE>>2));
    if(!blitter)
    {
      DEBUGF2(1,("CSTb7100Device::CreateGraphics - failed to allocate blitter\n"));
      return false;
    }

    if(!blitter->Create())
    {
      DEBUGF2(1,("CSTb7100Device::CreateGraphics - failed to create blitter\n"));
      delete blitter;
      return false;
    }

    m_graphicsAccelerators[0] = blitter;
    m_numAccelerators = 1;
  }
  else
  {
    /*
     * Graphics
     */
    m_pBDisp = new CSTb7109BDisp (m_pGammaReg + (STb7109_BDISP_BASE >> 2));
    if(!m_pBDisp)
    {
      DEBUGF2(1,("CSTb7100Device::CreateGraphics() - failed to allocate BDisp object\n"));
      return false;
    }

    if(!m_pBDisp->Create())
    {
      DEBUGF2(1,("CSTb7100Device::CreateGraphics - failed to create BDisp\n"));
      return false;
    }

    m_graphicsAccelerators[0] = m_pBDisp->GetAQ(1);
    if(!m_graphicsAccelerators[0])
    {
      DEBUGF(("CSTb7100Device::CreateGraphics() - failed to get AQ\n"));
      return false;
    }

    m_numAccelerators = 1;
  }

  return true;
}


bool CSTb7100Device::CreatePlanes()
{
  bool res;
  if(m_bIs7109)
  {
    if(m_chipVersion == 2)
      res = Create7109Cut2Planes();
    else
      res = Create7109Cut3Planes();
  }
  else
  {
    res = Create7100Planes();
  }

  return res;
}


bool CSTb7100Device::Create7100Planes()
{
  CSTb710xGDP           *gdp;
  CGammaCompositorDISP  *disp;
  CSTb7100Cursor        *cursor;
  CGammaCompositorPlane *null;

  DEBUGF2(2,("CSTb7100Device::Create7100Planes\n"));

  m_numPlanes = 6;

  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STb7100_COMPOSITOR_BASE;

  /*
   * create the hardware planes for this device, we use GDPLite becuase the
   * GDP on the 7100 only support horizontal upscaling which is pretty useless
   * on its own.
   */
  gdp = new CSTb710xGDP(OUTPUT_GDP1, ulCompositorBaseAddr+STb7100_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7100Planes - failed to create GDP1\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTb710xGDP(OUTPUT_GDP2, ulCompositorBaseAddr+STb7100_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7100Planes - failed to create GDP2\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[1] = gdp;

  disp = new CSTb7100HDDisplay(OUTPUT_VID1,
                               m_pVideoPlug1,
                               ((ULONG)m_pGammaReg)+STb7100_HD_DISPLAY_BASE);

  if(!disp || !disp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7100Planes - failed to create VID1\n"));
    delete disp;
    return false;
  }

  m_hwPlanes[2] = disp;

  disp = new CSTb7100VideoDisplay(OUTPUT_VID2,
                                  m_pVideoPlug2,
                                  ((ULONG)m_pGammaReg)+STb7100_SD_DISPLAY_BASE);

  if(!disp || !disp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7100Planes - failed to create VID2\n"));
    delete disp;
    return false;
  }

  m_hwPlanes[3] = disp;

  cursor = new CSTb7100Cursor(ulCompositorBaseAddr+STb7100_CUR_OFFSET);
  if(!cursor || !cursor->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7100Planes - failed to create CUR\n"));
    delete cursor;
    return false;
  }
  m_hwPlanes[4] = cursor;

  null = new CGammaCompositorNULL(OUTPUT_NULL);
  if(!null || !null->Create())
  {
    DERROR("failed to create NULL\n");
    delete null;
    return false;
  }
  m_hwPlanes[5] = null;

  return true;
}


bool CSTb7100Device::Create7109Cut2Planes()
{
  CSTb710xGDP *gdp;
  CGammaCompositorVideoPlane *video;
  CGammaCompositorPlane *null;

  DEBUGF2(2,("CSTb7100Device::Create7109Cut2Planes\n"));

  m_numPlanes = 5;

  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STb7100_COMPOSITOR_BASE;

  /*
   * The GDPs and Aux video plane on 7109 Cut2 are the same as 7100. However
   * the main video is compeltely new and needs a different driver. For the
   * moment we are ignoring it.
   */
  gdp = new CSTb710xGDP(OUTPUT_GDP1, ulCompositorBaseAddr+STb7100_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut2Planes - failed to create GDP1\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTb710xGDP(OUTPUT_GDP2, ulCompositorBaseAddr+STb7100_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut2Planes - failed to create GDP2\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[1] = gdp;

  video = new CSTb7109DEI(OUTPUT_VID1,
                          m_pVideoPlug1,
                          ((ULONG)m_pGammaReg)+STb7100_HD_DISPLAY_BASE);
  if(!video || !video->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut2Planes - failed to create VID1\n"));
    delete video;
    return false;
  }

  m_hwPlanes[2] = video;

  video = new CSTb7100VideoDisplay(OUTPUT_VID2,
                                  m_pVideoPlug2,
                                  ((ULONG)m_pGammaReg)+STb7100_SD_DISPLAY_BASE);
  if(!video || !video->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut2Planes - failed to create VID2\n"));
    delete video;
    return false;
  }

  m_hwPlanes[3] = video;

  null = new CGammaCompositorNULL(OUTPUT_NULL);
  if(!null || !null->Create())
  {
    DERROR("failed to create NULL\n");
    delete null;
    return false;
  }
  m_hwPlanes[4] = null;

  return true;
}


bool CSTb7100Device::Create7109Cut3Planes()
{
  CSTb7109Cut3GDP *gdp;
  CGammaCompositorVideoPlane *video;
  CSTb7109Cut3Cursor *cursor;
  CGammaCompositorPlane *null;

  DEBUGF2(2,("CSTb7100Device::Create7109Cut3Planes\n"));

  m_numPlanes = 7;

  ULONG ulCompositorBaseAddr = ((ULONG)m_pGammaReg) + STb7100_COMPOSITOR_BASE;

  /*
   * 7109 Cut3 is different again. There is an extra GDP on the main pipeline
   * and the GDPs have both horizontal and vertical upscaling and support CLUT
   * graphics formats. There is a complication that the extra plane is now GDP2
   * and GDP3 is now the plane that can be displayed on either the main or aux
   * pipelines (previously GDP2 on 7100 and 7109Cut2).
   */
  gdp = new CSTb7109Cut3GDP(OUTPUT_GDP1, ulCompositorBaseAddr+STb7100_GDP1_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create GDP1\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[0] = gdp;

  gdp = new CSTb7109Cut3GDP(OUTPUT_GDP2, ulCompositorBaseAddr+STb7100_GDP2_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create GDP2\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[1] = gdp;

  gdp = new CSTb7109Cut3GDP(OUTPUT_GDP3, ulCompositorBaseAddr+STb7109_GDP3_OFFSET);
  if(!gdp || !gdp->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create GDP3\n"));
    delete gdp;
    return false;
  }
  m_hwPlanes[2] = gdp;

  video = new CSTb7109DEI(OUTPUT_VID1,
                          m_pVideoPlug1,
                          ((ULONG)m_pGammaReg)+STb7100_HD_DISPLAY_BASE);
  if(!video || !video->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create VID1\n"));
    delete video;
    return false;
  }

  m_hwPlanes[3] = video;

  video = new CSTb7100VideoDisplay(OUTPUT_VID2,
                                   m_pVideoPlug2,
                                   ((ULONG)m_pGammaReg)+STb7100_SD_DISPLAY_BASE);
  if(!video || !video->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create VID2\n"));
    delete video;
    return false;
  }

  m_hwPlanes[4] = video;

  cursor = new CSTb7109Cut3Cursor(ulCompositorBaseAddr+STb7100_CUR_OFFSET);
  if(!cursor || !cursor->Create())
  {
    DEBUGF2(1,("CSTb7100Device::Create7109Cut3Planes - failed to create CUR\n"));
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

  return true;
}


CDisplayPlane* CSTb7100Device::GetPlane(stm_plane_id_t planeID) const
{
  CDisplayPlane *pPlane;

  if(m_pGDPBDispOutput && (pPlane = m_pGDPBDispOutput->GetVirtualPlane(planeID)) != 0)
    return pPlane;

  return CGenericGammaDevice::GetPlane(planeID);
}


void CSTb7100Device::UpdateDisplay(COutput *pOutput)
{
  /*
   * Generic HW processing
   */
  CGenericGammaDevice::UpdateDisplay(pOutput);

  /*
   * Virtual plane output processing related to the main output
   */
  if(m_pGDPBDispOutput && (m_pGDPBDispOutput->GetTimingID() == pOutput->GetTimingID()))
    m_pGDPBDispOutput->UpdateHW();
}


/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 * When this is called only g_pIOS will have been initialised.
 */
CDisplayDevice* AnonymousCreateDevice(unsigned deviceid)
{
  if(deviceid == 0)
    return new CSTb7100Device();

  return 0;
}
