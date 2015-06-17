/***********************************************************************
 *
 * File: soc/sti7200/sti7200hdmi.cpp
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
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

#include "sti7200reg.h"
#include "sti7200device.h"
#include "sti7200cut2localmainoutput.h"
#include "sti7200hdmi.h"

/*
 * DLL Rejection PLL setup
 */
#define SYS_CFG3_PLL_S_HDMI_PDIV_DEFAULT (2L   << SYS_CFG3_PLL_S_HDMI_PDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_NDIV_DEFAULT (0x64 << SYS_CFG3_PLL_S_HDMI_NDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_DEFAULT (0x32 << SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)
#define SYS_CFG3_PLL_S_HDMI_MDIV_2XREF   (0x19 << SYS_CFG3_PLL_S_HDMI_MDIV_SHIFT)

/*
 * Some offsets into the audio hardware so we can determine the HDMI audio
 * setup without making a dependency between the display and audio drivers.
 */
#define AUD_FSYNB_CFG   0x100
#define AUD_FSYNB2_MD   0x130
#define AUD_FSYNB2_PE   0x134
#define AUD_FSYNB2_SDIV 0x138
#define AUD_FSYNB2_EN   0x13C

#define AUD_FSYN_CFG_RST     (1L<<0)
#define AUD_FSYN_CFG_FS2_NSB (1L<<12)
#define AUD_FSYN_CFG_NPDA    (1L<<14)

#define MD_48KHZ_128FS    0xF3
#define PE_48KHZ_128FS    0x3C00
#define SDIV_48KHZ_128FS  0x5

#define AUD_HDMI_CTRL     0x204

#define AUD_HDMI_CTRL_DEBUG_VIDEO_NPCM (1L<<0)
#define AUD_HDMI_CTRL_VIDEO_SPDIF_NPCM (1L<<1)


////////////////////////////////////////////////////////////////////////////////
//

CSTi7200HDMI::CSTi7200HDMI(
    CDisplayDevice                 *pDev,
    ULONG                           ulHDMIBase,
    ULONG                           ulHDFormatterBase,
    stm_hdf_hdmi_hardware_version_t hdfRevision,
    COutput                        *pMainOutput,
    CSTmSDVTG                      *pVTG): CSTmHDFormatterHDMI(pDev,
                                                               STM_HDMI_HW_V_1_4,
                                                               hdfRevision,
                                                               ulHDMIBase,
                                                               ulHDFormatterBase,
                                                               ulHDMIBase+STi7200_HDMI_PCMPLAYER_OFFSET,
                                                               ulHDMIBase+STi7200_HDMI_SPDIF_OFFSET,
                                                               pMainOutput,
                                                               pVTG)
{
  ULONG val;

  DEBUGF2(2,("CSTi7200HDMI::CSTi7200HDMI pDev = %p  main output = %p\n",pDev,pMainOutput));

  m_ulSysCfgOffset = STi7200_SYSCFG_BASE;
  m_ulAudioOffset  = STi7200_AUDCFG_BASE;

  /*
   * Start with HDMI Serializer powered down.
   */
  val = ReadSysCfgReg(SYS_CFG21) | SYS_CFG21_HDMI_POFF;
  WriteSysCfgReg(SYS_CFG21, val);

  /*
   * If we do not yet have a HDMI audio clock running, we must start it
   * with a default frequency, otherwise the SW reset of the HDMI block
   * does not work.
   */
  val = ReadAUDReg(AUD_FSYNB2_SDIV);
  if(val == 0)
  {
    WriteAUDReg(AUD_FSYNB2_EN,    0);
    WriteAUDReg(AUD_FSYNB2_SDIV, SDIV_48KHZ_128FS);
    WriteAUDReg(AUD_FSYNB2_MD,   MD_48KHZ_128FS);
    WriteAUDReg(AUD_FSYNB2_PE,   PE_48KHZ_128FS);
    WriteAUDReg(AUD_FSYNB2_EN,   1);
  }

  val = ReadAUDReg(AUD_FSYNB_CFG);
  if((val & AUD_FSYN_CFG_FS2_NSB) == 0)
  {
    DEBUGF2(2,("CSTi7200HDMI::CSTi7200HDMI - starting default audio clock\n"));
    /*
     * Bring Fsynth2 out of digital standby
     */
    val |= AUD_FSYN_CFG_FS2_NSB;
    /*
     * Bring out of power down and reset if needed
     */
    if((val & AUD_FSYN_CFG_NPDA) == 0)
      val |= AUD_FSYN_CFG_NPDA | AUD_FSYN_CFG_RST;

    WriteAUDReg(AUD_FSYNB_CFG,val);
    g_pIOS->StallExecution(10);
    WriteAUDReg(AUD_FSYNB_CFG,val & ~AUD_FSYN_CFG_RST);
  }

  DEBUGF2(2,("CSTi7200HDMI::CSTi7200HDMI - out\n"));
}


CSTi7200HDMI::~CSTi7200HDMI() {}


bool CSTi7200HDMI::Create(void)
{
  DENTRY();

  /*
   * Try and create an FDMA driver InfoFrame manager before we call the
   * base class Create().
   */
  m_pIFrameManager = new CSTmDMAIFrames(m_pDisplayDevice, m_ulHDMIOffset, STi7200_REGISTER_BASE);
  if(!m_pIFrameManager || !m_pIFrameManager->Create(this,m_pMasterOutput))
  {
    DEBUGF2(2,("Unable to create a DMA based Info Frame manager\n"));
    /*
     * Reset m_pIFrameManager so the base class will create the CPU driven
     * version instead.
     */
    delete m_pIFrameManager;
    m_pIFrameManager = 0;
  }

  return CSTmHDFormatterHDMI::Create();
}


void CSTi7200HDMI::SetupRejectionPLL(const stm_mode_line_t *mode,
                                     ULONG                  tvStandard)
{
  ULONG val;

  /*
   * The main HDMI clocks have been setup when the main output video mode
   * was started. However there is an extra PLL, fed by the main output
   * FSynth which needs to be enabled. This unfortunately lives in the sys
   * config registers, not the clockgen B registers. The startup sequence is
   * quite complex....
   *
   * First Power down the BCH PLL
   */
  val  = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("CSTi7200HDMI::SetupRejectionPLL - waiting for BCH powerdown\n"));

  while((ReadSysCfgReg(SYS_STA2) & SYS_STA2_HDMI_PLL_LOCK) != 0);

  DEBUGF2(2,("CSTi7200HDMI::SetupRejectionPLL - got BCH powerdown\n"));

  /*
   * Power up the BCH PLL
   */
  val = ReadSysCfgReg(SYS_CFG3) & ~(SYS_CFG3_PLL_S_HDMI_PDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_NDIV_MASK |
                                    SYS_CFG3_PLL_S_HDMI_MDIV_MASK);

  val |= (SYS_CFG3_PLL_S_HDMI_PDIV_DEFAULT |
          SYS_CFG3_PLL_S_HDMI_NDIV_DEFAULT |
          SYS_CFG3_PLL_S_HDMI_MDIV_DEFAULT |
          SYS_CFG3_PLL_S_HDMI_EN);

  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("CSTi7200HDMI::SetupRejectionPLL - waiting for rejection PLL Lock\n"));

  while((ReadSysCfgReg(SYS_STA2) & SYS_STA2_HDMI_PLL_LOCK) == 0);

  DEBUGF2(2,("CSTi7200HDMI::SetupRejectionPLL - got rejection PLL Lock\n"));

  /* The HW documentation says we should bring the serializer out of reset
     here and now, but if we do that at this point, something seems to be
     going wrong sometimes, resulting in signals which can not be displayed by
     the connected sink. Relatively easy to observe when switching between VGA
     and 720p50 a couple of times.
     Experimentation has shown that moving the code to bring the serializer
     out of reset into the PostConfiguration() call solves these issues with
     no adverse effect. */
}


bool CSTi7200HDMI::PreConfiguration(const stm_mode_line_t *mode,
                                    ULONG                  tvStandard)
{
  ULONG phy,preemp;

  DENTRY();

  /*
   * Power up HDMI serializer
   */
  WriteSysCfgReg(SYS_CFG21, ReadSysCfgReg(SYS_CFG21) & ~SYS_CFG21_HDMI_POFF);

  /*
   * Setup PHY parameters for mode
   */
  phy = ReadSysCfgReg(SYS_CFG47) & ~SYS_CFG47_HDMIPHY_BUFFER_SPEED_MASK;
  preemp = 0;

  if(mode->TimingParams.ulPixelClock > 74250000)
  {
    /* 1080p 50/60Hz, 2880x480p or 2880x576p note we will only get here on Cut2 */
    DEBUGF2(2,("%s: PHY to 1.6Gb/s\n",__PRETTY_FUNCTION__));
    phy |= SYS_CFG47_HDMIPHY_BUFFER_SPEED_16;

    /*
     * 1080p requires pre-emphasis to be enabled.
     */
    if(mode->TimingParams.ulPixelClock > 148000000)
    {
      DEBUGF2(2,("%s: Pre-emphasis enabled\n", __PRETTY_FUNCTION__));
      preemp = (3 << SYS_CFG19_HDMIPHY_PREEMP_STR_SHIFT) | SYS_CFG19_HDMIPHY_PREEMP_ON;
    }
  }
  else if(mode->TimingParams.ulPixelClock > 27027000)
  {
    /* 720p, 1080i, 1440x480p, 1440x576p, 2880x480i, 2880x576i */
    DEBUGF2(2,("%s: PHY to 800Mb/s\n",__PRETTY_FUNCTION__));
    phy |= SYS_CFG47_HDMIPHY_BUFFER_SPEED_8;
  }
  else
  {
    DEBUGF2(2,("%s: PHY to 400Mb/s\n",__PRETTY_FUNCTION__));
    phy |= SYS_CFG47_HDMIPHY_BUFFER_SPEED_4;
  }

  WriteSysCfgReg(SYS_CFG47, phy);

  /*
   * For Cut2 we need to set the pre-emphasis config register, we can tell
   * this from the revision of the HDF/HDMI.
   */
  if(m_Revision != STM_HDF_HDMI_REV1)
    WriteSysCfgReg(SYS_CFG19, preemp);

  SetupRejectionPLL(mode,tvStandard);

  DEXIT();

  return true;
}


bool CSTi7200HDMI::PostConfiguration(const stm_mode_line_t *mode,
                                     ULONG                  tvStandard)
{
  DEBUGF2(2,(FENTRY "\n", __PRETTY_FUNCTION__));

  /* Bring the serializer out of reset. See comment above at end of
     SetupRejectionPLL() about why this is done here. */
  ULONG val = ReadSysCfgReg(SYS_CFG3) | SYS_CFG3_S_HDMI_RST_N;
  WriteSysCfgReg(SYS_CFG3, val);

  DEBUGF2(2,("%s: waiting for HDMI DLL Lock\n", __PRETTY_FUNCTION__));

  while((ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_DLL_LCK) == 0);

  DEBUGF2(2,("%s: got HDMI DLL Lock\n", __PRETTY_FUNCTION__));


  bool result = CSTmHDFormatterHDMI::PostConfiguration (mode, tvStandard);

  DEBUGF2(2,(FEXIT "\n", __PRETTY_FUNCTION__));

  return result;
}


bool CSTi7200HDMI::Stop(void)
{
  ULONG val;

  DEBUGF2(2,("CSTi7200HDMI::Stop\n"));

  CSTmHDMI::Stop();

  val = ReadSysCfgReg(SYS_CFG3);
  val &= ~(SYS_CFG3_PLL_S_HDMI_EN | SYS_CFG3_S_HDMI_RST_N);
  WriteSysCfgReg(SYS_CFG3, val);

  val = ReadSysCfgReg(SYS_CFG21) | SYS_CFG21_HDMI_POFF;
  WriteSysCfgReg(SYS_CFG21, val);

  DEBUGF2(2,("CSTi7200HDMI::Stop - out\n"));

  return true;
}


int CSTi7200HDMI::GetAudioFrequency(void)
{
  ULONG sdiv;
  long  md; /* Note this is signed */
  ULONG pe;

  /*
   * The same FSynth drives either the HDMI PCM or SPDIF player
   */
  sdiv = ReadAUDReg(AUD_FSYNB2_SDIV);
  md   = ReadAUDReg(AUD_FSYNB2_MD);
  pe   = ReadAUDReg(AUD_FSYNB2_PE);

  return stm_fsynth_frequency(STM_CLOCK_REF_30MHZ, sdiv, md, pe);
}


///////////////////////////////////////////////////////////////////////////////
//

CSTi7200HDMICut2::CSTi7200HDMICut2(
    CDisplayDevice              *pDev,
    CSTi7200Cut2LocalMainOutput *pMainOutput,
    CSTmSDVTG                   *pVTG): CSTi7200HDMI(pDev, STi7200C2_HDMI_BASE, STi7200C2_LOCAL_FORMATTER_BASE, STM_HDF_HDMI_REV2, pMainOutput, pVTG)
{
  /*
   * We can support hardware CTS generation for input audio clock frequencies
   * above 128*Fs, for the moment just support 256*Fs which matches the usual
   * analogue DAC master clock.
   */
  m_maxAudioClockDivide = 2;
  m_ulTVOutOffset = STi7200C2_LOCAL_DISPLAY_BASE+STi7200_MAIN_TVOUT_OFFSET;
}

CSTi7200HDMICut2::~CSTi7200HDMICut2(void) {}

#define TVOUT_CLK_SEL           0x004

#define TVOUT_CLK_BYPASS           (0L)
#define TVOUT_CLK_DIV_2            (1L)
#define TVOUT_CLK_MASK             (3L)

#define TVOUT_MAIN_CLK_SEL_TMDS(x) ((x)<<2)

bool CSTi7200HDMICut2::PreConfiguration(const stm_mode_line_t *mode,
                                        ULONG                  tvStandard)
{
  DENTRY();

  /*
   * The main output will have set the TMDS clock to the pixel clock for
   * HD/ED modes and 2x the pixel clock for SD modes. However we now have to
   * deal with 4x pixel repeats for SD modes and 2x or 4x repeats for ED modes
   * as well, to allow higher bitrate audio transmission.
   */
  if(((tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X) && mode->ModeParams.ScanType == SCAN_P) ||
     ((tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X) && mode->ModeParams.ScanType == SCAN_I))
  {
    ULONG clk = ReadTVOutReg(TVOUT_CLK_SEL) & ~TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_MASK);

    clk |= TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_DIV_2);
    WriteTVOutReg(TVOUT_CLK_SEL,clk);
    DEBUGF2(2,("%s: setting TMDS clock divide to 2 (54MHz)\n", __PRETTY_FUNCTION__));
  }
  else if((tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X) && mode->ModeParams.ScanType == SCAN_P)
  {
    ULONG clk = ReadTVOutReg(TVOUT_CLK_SEL) & ~TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_MASK);

    clk |= TVOUT_MAIN_CLK_SEL_TMDS(TVOUT_CLK_BYPASS);
    WriteTVOutReg(TVOUT_CLK_SEL,clk);
    DEBUGF2(2,("%s: setting TMDS clock divide to bypass (108MHz)\n", __PRETTY_FUNCTION__));
  }

  return CSTi7200HDMI::PreConfiguration(mode,tvStandard);
}

