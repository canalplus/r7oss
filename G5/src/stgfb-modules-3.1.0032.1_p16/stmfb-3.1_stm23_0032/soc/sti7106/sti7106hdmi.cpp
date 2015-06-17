/***********************************************************************
 *
 * File: soc/sti7106/sti7106hdmi.cpp
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
#include <STMCommon/stmv29iframes.h>

#include "sti7106reg.h"
#include "sti7106hdmi.h"

#include <HDTVOutFormatter/stmtvoutreg.h>

////////////////////////////////////////////////////////////////////////////////
//
// Note: although we are reusing the 7111 class because the SoC infrastructure
// is very similar (particularly audio), we have to override both the HDMI
// version and HDFormatter revision.

CSTi7106HDMI::CSTi7106HDMI(CDisplayDevice *pDev,
                           COutput        *pMainOutput,
                           CSTmSDVTG      *pVTG): CSTi7111HDMI(pDev, pMainOutput, pVTG, AUD_FSYN_CFG_SYSB_IN, STM_HDMI_HW_V_2_9, STM_HDF_HDMI_REV3)
{
  DEBUGF2 (2, (FENTRY " @ %p: pDev = %p  main output = %p\n", __PRETTY_FUNCTION__, this, pDev, pMainOutput));

  /*
   * Override 7111 SYS_CFG2 as it has changed considerably.
   *
   * Firstly mask out all the reserved bits
   */
  ULONG val = ReadSysCfgReg(SYS_CFG2) & 0x9C000000;
  /*
   * Now set the TMDS clock input to the PHY clock output and use the
   * rejection PLL.
   */
  val &= ~SYS_CFG2_HDMI_TMDS_CLKB_N_PHY;
  val |= SYS_CFG2_HDMI_REJECTION_PLL_N_BYPASS;

  WriteSysCfgReg(SYS_CFG2, val);

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}


CSTi7106HDMI::~CSTi7106HDMI()
{
  DENTRY();

  DEXIT();
}


bool CSTi7106HDMI::Create(void)
{
  DENTRY();

  m_pIFrameManager = new CSTmV29IFrames(m_pDisplayDevice,m_ulHDMIOffset);
  if(!m_pIFrameManager || !m_pIFrameManager->Create(this,m_pMasterOutput))
  {
    DEBUGF2(2,("Unable to create HDMI v2.9 Info Frame manager\n"));
    delete m_pIFrameManager;
    m_pIFrameManager = 0;
    return false;
  }

  /*
   * Bypass the 7111 Create as we do not want the FDMA driven IFrames
   */
  return CSTmHDFormatterHDMI::Create();
}


bool CSTi7106HDMI::SetInputSource(ULONG src)
{
  DENTRY();

  /*
   * 7106 does correctly support 8ch PCM over HDMI, so bypass the 7111 code
   */
  if(!CSTmHDFormatterHDMI::SetInputSource(src))
    return false;

  DEXIT();
  return true;
}


bool CSTi7106HDMI::SetupRejectionPLL(const stm_mode_line_t *mode,
                                     ULONG                  tvStandard,
                                     ULONG                  divide)
{
  ULONG val;

  /*
   * The old BCH/rejection PLL is now reused to provide the CLKPXPLL clock
   * input to the new PHY PLL that generates the serializer clock (TMDS*10) and
   * the TMDS clock which is now fed back into the HDMI formatter instead of
   * the TMDS clock line from ClockGenB.
   *
   */
  val  = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("%s: waiting for Rejection PLL powerdown\n", __PRETTY_FUNCTION__));

  while((ReadSysCfgReg(SYS_STA9) & SYS_STA9_HDMI_PLL_LOCK) != 0);

  DEBUGF2(2,("%s: got Rejection PLL powerdown\n", __PRETTY_FUNCTION__));

  /*
   * Power up the Rejection PLL
   */
  val = ReadSysCfgReg(SYS_CFG3) & ~(SYS_CFG3_PLL_S_HDMI_PDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_NDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_MDIV_MASK);

  /*
   * PLLout = (Fin*Mdiv) / ((2 * Ndiv) / 2^Pdiv)
   */
  ULONG mdiv = 32;
  ULONG ndiv = 64;
  ULONG pdiv;

  switch(divide)
  {
    case TVOUT_CLK_BYPASS:
      DEBUGF2(2,("%s: PLL out = fsynth\n", __PRETTY_FUNCTION__));
      pdiv = 2;
      break;
    case TVOUT_CLK_DIV_2:
      DEBUGF2(2,("%s: PLL out = fsynth/2\n", __PRETTY_FUNCTION__));
      pdiv = 3;
      break;
    case TVOUT_CLK_DIV_4:
      DEBUGF2(2,("%s: PLL out = fsynth/4\n", __PRETTY_FUNCTION__));
      pdiv = 4;
      break;
    default:
      DEBUGF2(1,("%s: unsupported TMDS clock divide (0x%lx)\n", __PRETTY_FUNCTION__,divide));
      return false;
  }

  val |= ((pdiv << SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT) |
          (ndiv << SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT) |
          (mdiv << SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT) |
          SYS_CFG3_PLL_S_HDMI_EN);

  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("%s: waiting for rejection PLL Lock\n", __PRETTY_FUNCTION__));

  while((ReadSysCfgReg(SYS_STA9) & SYS_STA9_HDMI_PLL_LOCK) == 0);

  DEBUGF2(2,("%s: got rejection PLL Lock\n", __PRETTY_FUNCTION__));

  return true;
}


bool CSTi7106HDMI::PreConfiguration(const stm_mode_line_t *mode,
                                    ULONG                  tvStandard)
{
  DEBUGF2(2,(FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  /*
   * We are re-using the 7111/7105 ClockGenB and HDFormatter clock setup,
   * but now the TMDS clock is going to come from HDMI PHY PLL not ClockGenB
   * in order to be able to generate the deepcolor TMDS clock multiples. So
   * we do not want to divide the TMDS clock in the HDFormatter any more, it
   * will always be correct. However we do need to know the divide set by the
   * master output clock setup so we can replicate it in the RejectionPLL setup
   * for the input clock to the HDMI PHY's TMDS clock generation.
   */
  ULONG divide = (ReadTVOutReg(TVOUT_CLK_SEL) & TVOUT_MAIN_CLK_SEL_PIX(TVOUT_CLK_MASK)) >> TVOUT_MAIN_CLK_PIX_SHIFT;

  switch (divide)
  {
    case TVOUT_CLK_DIV_8:
    {
      /*
       * If the pixel clock is /8 then we must be in an SD interlaced mode with
       * a base clock of 108MHz.
       */
      if(mode->ModeParams.ScanType != SCAN_I)
      {
        DERROR("Unexpected: Pixel clock is /8 but mode is progressive!!!\n");
        return false;
      }

      if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
      {
        divide = TVOUT_CLK_DIV_2;
        DEBUGF2(2,("%s: setting Rejection PLL divide to 2 (54MHz)\n", __PRETTY_FUNCTION__));
      }
      else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
      {
        divide = TVOUT_CLK_DIV_4;
        DEBUGF2(2,("%s: setting Rejection PLL divide to 4 (27MHz)\n", __PRETTY_FUNCTION__));
      }
      else
      {
        DERROR("Need pixel repetition, 13.5MHz clock not valid on HDMI\n");
        return false;
      }
      break;
    }
    case TVOUT_CLK_DIV_4:
    {
      /*
       * If the pixel clock is /4 then we are in ED or VGA
       */
      if(mode->ModeParams.ScanType != SCAN_P)
      {
        DERROR("Unexpected: Pixel clock is /4 but mode is interlaced!!!\n");
        return false;
      }

      if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
      {
        divide = TVOUT_CLK_DIV_2;
        DEBUGF2(2,("%s: setting Rejection PLL divide to 2 (54MHz)\n", __PRETTY_FUNCTION__));
      }
      else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
      {
        divide = TVOUT_CLK_BYPASS;
        DEBUGF2(2,("%s: setting Rejection PLL divide to bypass (108MHz)\n", __PRETTY_FUNCTION__));
      }
      else
      {
        DEBUGF2(2,("%s: setting Rejection PLL divide to 4 (27MHz)\n", __PRETTY_FUNCTION__));
      }
      break;
    }
    case TVOUT_CLK_DIV_2:
      DEBUGF2(2,("%s: setting Rejection PLL divide to 2 (74.25MHz)\n", __PRETTY_FUNCTION__));
      break;
    case TVOUT_CLK_BYPASS:
      DEBUGF2(2,("%s: setting Rejection PLL divide to bypass (148MHz)\n", __PRETTY_FUNCTION__));
      break;
    default:
      break;
  }

  ULONG clk = ReadTVOutReg(TVOUT_CLK_SEL) & ~TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_MASK);
  clk |= TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_BYPASS);
  WriteTVOutReg(TVOUT_CLK_SEL,clk);


  if(!SetupRejectionPLL(mode, tvStandard, divide))
    return false;

  DEBUGF2(2,(FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));

  return true;
}


bool CSTi7106HDMI::PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard)
{
  /*
   * Remove the serializer reset, without this we do not get any life from the
   * 7106 HDMI cell, you cannot bring it out of reset. But if this is the PHY
   * reset then that shouldn't happen, it shouldn't effect the TMDS clock
   * generation for instance. Hmmmm. However it means that this must be
   * called before we try and soft reset the digital part of the HDMI cell.
   */
  ULONG val = ReadSysCfgReg(SYS_CFG3) | SYS_CFG3_S_HDMI_RST_N;
  WriteSysCfgReg(SYS_CFG3, val);

  return true;
}


bool CSTi7106HDMI::Stop(void)
{
  ULONG val;

  DEBUGF2(2,(FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  CSTmHDMI::Stop();

  val = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,(FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));

  return true;
}
