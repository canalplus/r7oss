/***********************************************************************
 *
 * File: soc/sti7108/sti7108hdmi.cpp
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
#include <STMCommon/stmhdmiregs.h>
#include <STMCommon/stmdmaiframes.h>
#include <STMCommon/stmv29iframes.h>

#include "sti7108reg.h"
#include "sti7108device.h"
#include "sti7108mainoutput.h"
#include "sti7108hdmi.h"


#define MD_48KHZ_256FS    0xF3
#define PE_48KHZ_256FS    0x3C00
#define SDIV_48KHZ_256FS  0x4

////////////////////////////////////////////////////////////////////////////////
//

CSTi7108HDMI::CSTi7108HDMI(
    CDisplayDevice      *pDev,
    COutput             *pMainOutput,
    CSTmSDVTG           *pVTG,
    CSTmTVOutClkDivider *pClkDivider): CSTmHDFormatterHDMI(pDev,
                                                   STM_HDMI_HW_V_2_9,
                                                   STM_HDF_HDMI_REV3,
                                                   STi7108_HDMI_BASE,
                                                   STi7108_HD_FORMATTER_BASE,
                                                   STi7108_HDMI_PCMPLAYER_BASE,
                                                   STi7108_HDMI_SPDIF_BASE,
                                                   pMainOutput,
                                                   pVTG)
{
  DEBUGF2 (2, (FENTRY " @ %p: pDev = %p  main output = %p\n", __PRETTY_FUNCTION__, this, pDev, pMainOutput));

  m_pClkDivider = pClkDivider;

  m_ulSysCfg0Offset = STi7108_SYSCFG_BANK0_BASE;
  m_ulSysCfg3Offset = STi7108_SYSCFG_BANK3_BASE;
  m_ulSysCfg4Offset = STi7108_SYSCFG_BANK4_BASE;

  /*
   * We can support hardware CTS generation for input audio clock frequencies
   * above 128*Fs, for the moment just support 256*Fs which matches the usual
   * analogue DAC master clock.
   */
  m_maxAudioClockDivide = 2;

  StartAudioClocks();

  DEBUGF2 (2, (FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));
}


CSTi7108HDMI::~CSTi7108HDMI()
{
}


bool CSTi7108HDMI::Create(void)
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

  if(!CSTmHDFormatterHDMI::Create())
    return false;

  /*
   * We need to put the PHY back into reset now we have a lock created
   * (by default the reset controller brings it out of reset on chip reset).
   *
   * TODO: Think about how we make this SMP safe against other drivers.
   */
  g_pIOS->LockResource(m_statusLock);
  ULONG val = ReadSysCfg0Reg(SYS_CFG12_B0) & ~SYS_CFG12_B0_HDMI_PHY_N_RST;
  WriteSysCfg0Reg(SYS_CFG12_B0,val);
  g_pIOS->UnlockResource(m_statusLock);

  DEXIT();
  return true;
}


void CSTi7108HDMI::StartAudioClocks(void)
{
  DENTRY();
  /*
   * If we do not yet have a HDMI audio clock running, we must start it
   * with a default frequency, otherwise the SW reset of the HDMI block
   * does not work.
   */
  ULONG val = ReadSysCfg4Reg(SYS_CFG24_B4);
  DEBUGF2(2,("%s aud fsyn cfg = 0x%lx\n",__PRETTY_FUNCTION__,val));
  /*
   * Bring out of power down and reset if needed
   */
  if((val & SYS_CFG24_B4_N_RST) == 0)
  {
    /*
     * Trash the whole setup if the clocks are in reset, the chip reset values
     * just confuse the issue.
     *
     * Note the the reset bit is inverted compared with previous chips, sigh.
     */
    val = SYS_CFG24_B4_NPDA;

    WriteSysCfg4Reg(SYS_CFG24_B4,val);
    g_pIOS->StallExecution(10);
    val |= SYS_CFG24_B4_N_RST;
    WriteSysCfg4Reg(SYS_CFG24_B4,val);
  }

  if((val & SYS_CFG24_B4_FS0_EN) == 0 ||
     ReadSysCfg4Reg(SYS_CFG_B4_FS0_SDIV) == 0)
  {
    /*
     * Bring Fsynth0 out of digital standby
     */
    val |= SYS_CFG24_B4_FS0_NSB | SYS_CFG24_B4_FS0_SEL | SYS_CFG24_B4_FS0_EN;
    WriteSysCfg4Reg(SYS_CFG24_B4,val);

    WriteSysCfg4Reg(SYS_CFG_B4_FS0_EN,   0);
    WriteSysCfg4Reg(SYS_CFG_B4_FS0_SDIV, SDIV_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS0_MD,   MD_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS0_PE,   PE_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS0_EN,   1);
  }

  if((val & SYS_CFG24_B4_FS2_EN) == 0 ||
     ReadSysCfg4Reg(SYS_CFG_B4_FS2_SDIV) == 0)
  {
    /*
     * Bring Fsynth2 out of digital standby
     */
    val |= SYS_CFG24_B4_FS2_NSB | SYS_CFG24_B4_FS2_SEL | SYS_CFG24_B4_FS2_EN;
    WriteSysCfg4Reg(SYS_CFG24_B4,val);

    WriteSysCfg4Reg(SYS_CFG_B4_FS2_EN,   0);
    WriteSysCfg4Reg(SYS_CFG_B4_FS2_SDIV, SDIV_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS2_MD,   MD_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS2_PE,   PE_48KHZ_256FS);
    WriteSysCfg4Reg(SYS_CFG_B4_FS2_EN,   1);
  }
}


bool CSTi7108HDMI::SetupRejectionPLL(const stm_mode_line_t          *mode,
                                     ULONG                           tvStandard,
                                     stm_clk_divider_output_divide_t divide)
{
  ULONG val;

  /*
   * The old BCH/rejection PLL is now reused to provide the CLKPXPLL clock
   * input to the new PHY PLL that generates the serializer clock (TMDS*10) and
   * the TMDS clock which is now fed back into the HDMI formatter instead of
   * the TMDS clock line from ClockGenB.
   *
   * Note we do not lock access to SYS_CFG4 as no other driver has cause to
   * touch it.
   */
  val  = ReadSysCfg3Reg(SYS_CFG4);
  val &= ~SYS_CFG4_PLL_S_HDMI_EN;
  WriteSysCfg3Reg(SYS_CFG4, val);

  DEBUGF2(2,("%s: waiting for BCH powerdown\n", __PRETTY_FUNCTION__));

  while((ReadSysCfg3Reg(SYS_STA5) & SYS_STA5_HDMI_PLL_LOCK) != 0);

  DEBUGF2(2,("%s: got BCH powerdown\n", __PRETTY_FUNCTION__));

  /*
   * Power up the BCH PLL
   */
  val = ReadSysCfg3Reg(SYS_CFG4) & ~(SYS_CFG4_PLL_S_HDMI_PDIV_MASK |
                                     SYS_CFG4_PLL_S_HDMI_NDIV_MASK |
                                     SYS_CFG4_PLL_S_HDMI_MDIV_MASK);

  /*
   * PLLout = (Fin*Mdiv) / ((2 * Ndiv) / 2^Pdiv)
   */
  ULONG mdiv = 32;
  ULONG ndiv = 64;
  ULONG pdiv;

  switch(divide)
  {
    case STM_CLK_DIV_1:
      DEBUGF2(2,("%s: PLL out = fsynth\n", __PRETTY_FUNCTION__));
      pdiv = 2;
      break;
    case STM_CLK_DIV_2:
      DEBUGF2(2,("%s: PLL out = fsynth/2\n", __PRETTY_FUNCTION__));
      pdiv = 3;
      break;
    case STM_CLK_DIV_4:
      DEBUGF2(2,("%s: PLL out = fsynth/4\n", __PRETTY_FUNCTION__));
      pdiv = 4;
      break;
    default:
      DEBUGF2(1,("%s: unsupported TMDS clock divide (0x%x)\n", __PRETTY_FUNCTION__,divide));
      return false;
  }

  val |= ((pdiv << SYS_CFG4_PLL_S_HDMI_PDIV_SHIFT) |
          (ndiv << SYS_CFG4_PLL_S_HDMI_NDIV_SHIFT) |
          (mdiv << SYS_CFG4_PLL_S_HDMI_MDIV_SHIFT) |
          SYS_CFG4_PLL_S_HDMI_EN);

  WriteSysCfg3Reg(SYS_CFG4, val);

  DEBUGF2(2,("%s: waiting for rejection PLL Lock\n", __PRETTY_FUNCTION__));

  while((ReadSysCfg3Reg(SYS_STA5) & SYS_STA5_HDMI_PLL_LOCK) == 0);

  DEBUGF2(2,("%s: got rejection PLL Lock\n", __PRETTY_FUNCTION__));

  return true;
}


bool CSTi7108HDMI::PreConfiguration(const stm_mode_line_t *mode,
                                    ULONG                  tvStandard)
{
  DEBUGF2(2,(FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  stm_clk_divider_output_divide_t divide;
  m_pClkDivider->getDivide(STM_CLK_DISP_HD,&divide);

  switch(divide)
  {
    case STM_CLK_DIV_1:
    case STM_CLK_DIV_2:
      /*
       * This is the HD case
       */
      DEBUGF2(2,("%s: setting TMDS PLL divide = DISP_HD divide\n", __PRETTY_FUNCTION__));
      break;
    case STM_CLK_DIV_4:
      /*
       * This is the ED case, i.e. 480p, 576p and VGA
       */
      if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
      {
        divide = STM_CLK_DIV_2;
        DEBUGF2(2,("%s: setting TMDS PLL divide to 2 (54MHz)\n", __PRETTY_FUNCTION__));
      }
      else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
      {
        divide = STM_CLK_DIV_1;
        DEBUGF2(2,("%s: setting TMDS PLL divide to bypass (108MHz)\n", __PRETTY_FUNCTION__));
      }
      else
      {
        DEBUGF2(2,("%s: setting TMDS PLL divide to 4 (27MHz)\n", __PRETTY_FUNCTION__));
      }
      break;
    case STM_CLK_DIV_8:
      /*
       * This is the SD case, i.e. 480i, 576i
       */
      if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
      {
        divide = STM_CLK_DIV_4;
        DEBUGF2(2,("%s: setting TMDS PLL divide to 4 (27MHz)\n", __PRETTY_FUNCTION__));
      }
      else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
      {
        divide = STM_CLK_DIV_2;
        DEBUGF2(2,("%s: setting TMDS PLL divide to 2 (54MHz)\n", __PRETTY_FUNCTION__));
      }
      else
      {
        DERROR("Need pixel repetition, 13.5MHz clock not valid on HDMI\n");
        return false;
      }
      break;
  }

  if(!SetupRejectionPLL(mode, tvStandard, divide))
    return false;

  DEBUGF2(2,(FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));

  return true;
}


bool CSTi7108HDMI::PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard)
{
  /*
   * Remove the serializer reset.
   */
  g_pIOS->LockResource(m_statusLock);

  ULONG val = ReadSysCfg0Reg(SYS_CFG12_B0) | SYS_CFG12_B0_HDMI_PHY_N_RST;
  WriteSysCfg0Reg(SYS_CFG12_B0,val);

  g_pIOS->UnlockResource(m_statusLock);

  return true;
}


bool CSTi7108HDMI::Stop(void)
{
  DEBUGF2(2,(FENTRY " @ %p\n", __PRETTY_FUNCTION__, this));

  CSTmHDMI::Stop();

  g_pIOS->LockResource(m_statusLock);

  ULONG val = ReadSysCfg0Reg(SYS_CFG12_B0) & ~SYS_CFG12_B0_HDMI_PHY_N_RST;
  WriteSysCfg0Reg(SYS_CFG12_B0,val);

  val = ReadSysCfg3Reg(SYS_CFG4) & ~SYS_CFG4_PLL_S_HDMI_EN;
  WriteSysCfg3Reg(SYS_CFG4, val);

  g_pIOS->UnlockResource(m_statusLock);

  DEBUGF2(2,(FEXIT " @ %p\n", __PRETTY_FUNCTION__, this));

  return true;
}


int CSTi7108HDMI::GetAudioFrequency(void)
{
  ULONG sdiv;
  long  md; /* Note this is signed */
  ULONG pe;

  /*
   * TODO: how do we read these safely in an SMP system?
   */
  if(m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)
  {
    sdiv = ReadSysCfg4Reg(SYS_CFG_B4_FS2_SDIV);
    md   = ReadSysCfg4Reg(SYS_CFG_B4_FS2_MD);
    pe   = ReadSysCfg4Reg(SYS_CFG_B4_FS2_PE);
  }
  else
  {
    sdiv = ReadSysCfg4Reg(SYS_CFG_B4_FS0_SDIV);
    md   = ReadSysCfg4Reg(SYS_CFG_B4_FS0_MD);
    pe   = ReadSysCfg4Reg(SYS_CFG_B4_FS0_PE);
  }

  return stm_fsynth_frequency(STM_CLOCK_REF_30MHZ, sdiv, md, pe);
}
