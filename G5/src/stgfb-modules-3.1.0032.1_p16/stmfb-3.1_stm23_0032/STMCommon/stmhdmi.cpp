/***********************************************************************
 *
 * File: STMCommon/stmhdmi.cpp
 * Copyright (c) 2005-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>

#include <Generic/DisplayDevice.h>
#include <Generic/Output.h>

#include "stmviewport.h"
#include "stmvtg.h"
#include "stmhdmi.h"
#include "stmhdmiregs.h"
#include "stmiframemanager.h"

#if defined(HDMI_HOTPLUG_PIO_INVERT) && (HDMI_HOTPLUG_PIO_INVERT == 1)
	#define HPD_CHK_EQ      !=
	#define HPD_CHK_NE      ==
#else
	#define HPD_CHK_EQ      ==
	#define HPD_CHK_NE      !=
#endif

CSTmHDMI::CSTmHDMI(CDisplayDevice             *pDev,
                   stm_hdmi_hardware_version_t hwver,
                   ULONG                       ulHDMIOffset,
                   COutput                    *pMasterOutput,
                   CSTmVTG                    *pVTG): COutput(pDev, (ULONG)pVTG)
{
  DEBUGF2(2,("CSTmHDMI::CSTmHDMI pDev = %p\n",pDev));

  m_HWVersion      = hwver;

  m_pDevRegs       = (ULONG*)pDev->GetCtrlRegisterBase();
  m_ulHDMIOffset   = ulHDMIOffset;
  m_pMasterOutput  = pMasterOutput;
  m_pVTG           = pVTG;
  m_pIFrameManager = 0;
  m_ulIFrameManagerIntMask = 0;

  DEBUGF2(2,("CSTmHDMI::CSTmHDMI m_pDevRegs = %p m_ulHDMIOffset = %#08lx\n",m_pDevRegs,m_ulHDMIOffset));

  m_statusLock = 0;

  m_ulOutputFormat = 0;

  m_bSWResetCompleted   = false;
  m_ulLastCapturedPixel = 0;

  m_bUseHotplugInt      = true;

  m_bAVMute             = false;
  m_bESS                = false;

  m_bSinkSupportsDeepcolour = false;
  m_ulDeepcolourGCP         = STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN;

  m_pPHYConfig = 0;

  m_ulAudioOutputType = STM_HDMI_AUDIO_TYPE_NORMAL;

  g_pIOS->ZeroMemory(&m_audioState, sizeof(stm_hdmi_audio_state_t));
  /*
   * By default only support an audio clock of 128*Fs as CTS
   * audio clock regeneration is limited to this in original versions
   * of the hardware. This can be overridden by hardware specific classes
   */
  m_maxAudioClockDivide = 1;

  WriteHDMIReg(STM_HDMI_CFG, 0);
  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  /*
   * Set the default channel data to be a dark red, so its
   * very obvious. We don't set this in ::Start since a
   * user process may alter its value and expect it to be
   * persistent through hotplug.
   */
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL0_DATA, 0);
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL1_DATA, 0);
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL2_DATA, 0x60);

  DEXIT();
}


CSTmHDMI::~CSTmHDMI()
{
  DENTRY();

  WriteHDMIReg(STM_HDMI_CFG, 0);
  WriteHDMIReg(STM_HDMI_INT_EN,  0);
  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  delete m_pIFrameManager;

  DEBUGF2(2,("CSTmHDMI::~CSTmHDMI about to delete status lock\n"));

  if(m_statusLock)
    g_pIOS->DeleteResourceLock(m_statusLock);

  DEXIT();
}


bool CSTmHDMI::Create(void)
{
  DENTRY();

  m_statusLock = g_pIOS->CreateResourceLock();
  if(m_statusLock == 0)
  {
    DEBUGF2(2,("CSTmHDMI::Create failed to create status lock\n"));
    return false;
  }

  if(!m_pIFrameManager)
  {
    /*
     * If a subclass has not already created an InfoFrame manager then
     * create the default CPU driven one.
     */
    m_pIFrameManager = new CSTmCPUIFrames(m_pDisplayDevice,m_ulHDMIOffset);
    if(!m_pIFrameManager || !m_pIFrameManager->Create(this,this))
    {
      DERROR("Unable to create an Info Frame manager\n");
      return false;
    }
  }

  m_ulIFrameManagerIntMask = m_pIFrameManager->GetIFrameCompleteHDMIInterruptMask();

  if(m_bUseHotplugInt)
  {
    /*
     * Poll current status, as if the dipslay is connected first tme around
     * it will not trigger a hotplug interrupt.
     */
    ULONG hotplugstate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_HOT_PLUG);
    if(hotplugstate HPD_CHK_NE 0)
    {
      m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
      DEBUGF2(2,("CSTmHDMI::Create out initial hotplug detected\n"));
    }
    else
    {
      m_displayStatus = STM_DISPLAY_DISCONNECTED;
    }

    /*
     * Initialise the hot plug interrupt.
     */
    WriteHDMIReg(STM_HDMI_INT_EN, (STM_HDMI_INT_GLOBAL    |
                                   STM_HDMI_INT_HOT_PLUG));
  }

  /*
   * Set the default output format, which may be subclassed to program
   * hardware. This needs to be done after the status lock has been created
   * and will set m_ulOutputFormat for us.
   */
  SetOutputFormat(STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_RGB);

  InvalidateAudioPackets();

  /*
   * Configure a default audio clock regeneration parameter as some sinks are
   * unhappy if they cannot regenerate a valid audio clock, even if no valid
   * audio is being transmitted.
   */
  WriteHDMIReg(STM_HDMI_AUDN, 6144);

  DEXIT();

  return true;
}


/*
 * Helper step methods for Start()
 */
void CSTmHDMI::ProgramActiveArea(const stm_mode_line_t *const pModeLine, ULONG tvStandard)
{
  DENTRY();

  /*
   * Note: the HDMI horizontal start/stop pixel numbering appears to start
   * from 1 and is different to the programming for the compositor viewports.
   * The horizontal delay to active video that this produces has been verified
   * against the CEA-861 standards using a Quantum Data 882.
   */
  ULONG div = 1;
  if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
  {
    DEBUGF2(2,("%s: 2X Pixel Repeat\n",__PRETTY_FUNCTION__));
    div = 2;
  }
  else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
  {
    DEBUGF2(2,("%s: 4X Pixel Repeat\n",__PRETTY_FUNCTION__));
    div = 4;
  }

  ULONG xpo = (pModeLine->ModeParams.ActiveAreaXStart/div) + 1;
  ULONG xps = (pModeLine->ModeParams.ActiveAreaXStart/div) + (pModeLine->ModeParams.ActiveAreaWidth/div);

  DEBUGF2(2,("%s: XMIN = %lu\n", __PRETTY_FUNCTION__, xpo));
  DEBUGF2(2,("%s: XMAX = %lu\n", __PRETTY_FUNCTION__, xps));

  WriteHDMIReg(STM_HDMI_ACTIVE_VID_XMIN, xpo);
  WriteHDMIReg(STM_HDMI_ACTIVE_VID_XMAX, xps);

  ULONG ypo;
  ULONG yps;

  if(pModeLine->ModeParams.ScanType == SCAN_I)
  {
    ypo = (pModeLine->ModeParams.FullVBIHeight/2)+1;
    yps = (pModeLine->ModeParams.FullVBIHeight+pModeLine->ModeParams.ActiveAreaHeight)/2;
  }
  else
  {
    ypo = pModeLine->ModeParams.FullVBIHeight+1;
    yps = pModeLine->ModeParams.FullVBIHeight+pModeLine->ModeParams.ActiveAreaHeight;
  }

  WriteHDMIReg(STM_HDMI_ACTIVE_VID_YMIN, ypo);
  WriteHDMIReg(STM_HDMI_ACTIVE_VID_YMAX, yps);

  DEBUGF2(2,("%s: YMIN = %lu\n", __PRETTY_FUNCTION__, ypo));
  DEBUGF2(2,("%s: YMAX = %lu\n", __PRETTY_FUNCTION__, yps));
  DEXIT();
}


void CSTmHDMI::ProgramV1Config(ULONG &cfg, const stm_mode_line_t *const pModeLine, ULONG tvStandard)
{
  DENTRY();

  if(!pModeLine->TimingParams.HSyncPolarity)
  {
    DEBUGF2(2,("%s: Sync Negative\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_CFG_SYNC_POL_NEG;
  }

  if((pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_HD_MASK) == 0)
  {
    if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
    {
      if(pModeLine->ModeParams.ScanType == SCAN_I)
        cfg |= STM_HDMI_CFG_DLL_4X_TMDS;
      else
        cfg |= STM_HDMI_CFG_DLL_2X_TMDS;
    }
    else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
    {
      if(pModeLine->ModeParams.ScanType == SCAN_I)
        cfg |= STM_HDMI_CFG_DLL_2X_TMDS;
      else
        cfg |= STM_HDMI_CFG_DLL_1X_TMDS;
    }
    else
    {
      cfg |= STM_HDMI_CFG_DLL_4X_TMDS;
    }
  }
  else
  {
    if(pModeLine->TimingParams.ulPixelClock == 148500000 ||
       pModeLine->TimingParams.ulPixelClock == 148351648)
    {
      cfg |= STM_HDMI_CFG_DLL_1X_TMDS;
    }
    else
    {
      cfg |= STM_HDMI_CFG_DLL_2X_TMDS;
    }
  }

  DEXIT();
}


void CSTmHDMI::ProgramV29Config(ULONG &cfg, const stm_mode_line_t *const pModeLine, ULONG tvStandard)
{
  DENTRY();

  cfg |= (STM_HDMI_V29_CFG_SINK_TERM_DETECT_EN |
          STM_HDMI_V29_VID_FIFO_OVERRUN_CLR    |
          STM_HDMI_V29_VID_FIFO_UNDERRUN_CLR);

  if(!pModeLine->TimingParams.HSyncPolarity)
  {
    DEBUGF2(2,("%s: H Sync Negative\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_V29_CFG_H_SYNC_POL_NEG;
  }

  if(!pModeLine->TimingParams.VSyncPolarity)
  {
    DEBUGF2(2,("%s: V Sync Negative\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_V29_CFG_V_SYNC_POL_NEG;
  }

  if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_2X)
  {
    DEBUGF2(2,("%s: 2X Pixel Repeat\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_V29_VID_PIX_REP_2;
  }
  else if(tvStandard & STM_OUTPUT_STD_TMDS_PIXELREP_4X)
  {
    DEBUGF2(2,("%s: 4X Pixel Repeat\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_V29_VID_PIX_REP_4;
  }

  if(m_bSinkSupportsDeepcolour)
  {
    DEBUGF2(2,("%s: Configure Deepcolor\n",__PRETTY_FUNCTION__));
    cfg |= STM_HDMI_V29_SINK_SUPPORTS_DEEP_COLOR;
  }

  DEXIT();
}


struct pllmode_s
{
  ULONG min;
  ULONG max;
  ULONG mode;
};

struct pllmode_s pllmodes[] = {
    { 25174800,  25200000,  STM_HDMI_SRZ_PLL_CFG_MODE_25_2_MHZ  },
    { 27000000,  27027000,  STM_HDMI_SRZ_PLL_CFG_MODE_27_MHZ    },
    { 54000000,  54054000,  STM_HDMI_SRZ_PLL_CFG_MODE_54_MHZ    },
    { 72000000,  74250000,  STM_HDMI_SRZ_PLL_CFG_MODE_74_25_MHZ },
    { 108000000, 108108000, STM_HDMI_SRZ_PLL_CFG_MODE_108_MHZ   },
    { 148351648, 148500000, STM_HDMI_SRZ_PLL_CFG_MODE_148_5_MHZ }
};


bool CSTmHDMI::StartV29PHY(const stm_mode_line_t *const pModeLine, ULONG tvStandard)
{
  DENTRY();

  /*
   * We have arranged in the chip specific PreConfiguration for the input
   * clock to the PHY PLL to be the normal TMDS clock for 24bit colour, taking
   * into account pixel repetition. We have been passed a modeline where the
   * pixelclock parameter is in fact that TMDS clock in the case of the
   * pixel doubled/quadded modes.
   */
  ULONG ckpxpll = pModeLine->TimingParams.ulPixelClock;
  ULONG tmdsck;
  ULONG freqvco;
  ULONG pllctrl = 0;
  unsigned i;

  switch(m_ulOutputFormat & STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK)
  {
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT:
      tmdsck = (ckpxpll * 5) / 4; // 1.25x
      pllctrl = (5 << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT) | STM_HDMI_SRZ_PLL_CFG_SEL_12P5;
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_30BIT;
      DEBUGF2(2,("%s: Selected 30bit colour\n",__PRETTY_FUNCTION__));
      break;
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT:
      tmdsck = (ckpxpll * 3) / 2; // 1.5x
      pllctrl = 3 << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_36BIT;
      DEBUGF2(2,("%s: Selected 36bit colour\n",__PRETTY_FUNCTION__));
      break;
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT:
      tmdsck = ckpxpll * 2;
      pllctrl = 4 << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_48BIT;
      DEBUGF2(2,("%s: Selected 48bit colour\n",__PRETTY_FUNCTION__));
      break;
    default:
    case STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT:
      tmdsck = ckpxpll;
      pllctrl = 2 << STM_HDMI_SRZ_PLL_CFG_NDIV_SHIFT;
      if(m_bSinkSupportsDeepcolour)
        m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_24BIT;
      else
        m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN;

      DEBUGF2(2,("%s: Selected 24bit colour\n",__PRETTY_FUNCTION__));
      break;
  }

  if(!m_bSinkSupportsDeepcolour && (m_ulDeepcolourGCP != STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN))
  {
    DEBUGF2(2,("%s: Sink does not support deepcolour\n",__PRETTY_FUNCTION__));
    return false;
  }

  DEBUGF2(2,("%s: Setting serializer VCO for 10 * %luHz\n",__PRETTY_FUNCTION__, tmdsck));

  freqvco = tmdsck * 10;
  if(freqvco <= 425000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_425MHZ);
  else if (freqvco <= 850000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_850MHZ);
  else if (freqvco <= 1700000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_1700MHZ);
  else if (freqvco <= 2250000000UL)
    pllctrl |= STM_HDMI_SRZ_PLL_CFG_VCOR(STM_HDMI_SRZ_PLL_CFG_VCOR_2250MHZ);
  else
  {
    DEBUGF2(2,("%s: PHY serializer clock out of range\n",__PRETTY_FUNCTION__));
    return false;
  }

  /*
   * Setup the PLL mode parameter based on the ckpxpll
   */
  for(i=0;i<N_ELEMENTS(pllmodes);i++)
  {
    if(ckpxpll >= pllmodes[i].min && ckpxpll <= pllmodes[i].max)
      pllctrl |= STM_HDMI_SRZ_PLL_CFG_MODE(pllmodes[i].mode);
  }

  /*
   * Configure and power up the PHY PLL
   */
  DEBUGF2(2,("%s: pllctrl = 0x%08lx\n", __PRETTY_FUNCTION__, pllctrl));
  WriteHDMIReg(STM_HDMI_SRZ_PLL_CFG, pllctrl);

  while((ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_DLL_LCK) == 0);

  DEBUGF2(2,("%s: got PHY PLL Lock\n", __PRETTY_FUNCTION__));

  /*
   * To configure the source termination and pre-emphasis appropriately for
   * different high speed TMDS clock frequencies a phy configuration
   * table must be provided, tailored to the SoC and board combination.
   */
  if(m_pPHYConfig != 0)
  {
    i = 0;
    while(!((m_pPHYConfig[i].minTMDS == 0) && (m_pPHYConfig[i].maxTMDS == 0)))
    {
      if((m_pPHYConfig[i].minTMDS <= tmdsck) && (m_pPHYConfig[i].maxTMDS >= tmdsck))
      {
        WriteHDMIReg(STM_HDMI_SRZ_TAP_1, m_pPHYConfig[i].config[0]);
        WriteHDMIReg(STM_HDMI_SRZ_TAP_2, m_pPHYConfig[i].config[1]);
        WriteHDMIReg(STM_HDMI_SRZ_TAP_3, m_pPHYConfig[i].config[2]);
        WriteHDMIReg(STM_HDMI_SRZ_CTRL, ((m_pPHYConfig[i].config[3] | STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN) & ~STM_HDMI_SRZ_CTRL_POWER_DOWN));
        DEBUGF2(2,("%s: Setting serializer config 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
                   __PRETTY_FUNCTION__,
                   m_pPHYConfig[i].config[0],
                   m_pPHYConfig[i].config[1],
                   m_pPHYConfig[i].config[2],
                   m_pPHYConfig[i].config[3]));
        DEXIT();
        return true;
      }
      i++;
    }
  }

  /*
   * Default, power up the serializer with no pre-emphasis or source termination
   */
  WriteHDMIReg(STM_HDMI_SRZ_TAP_1, 0);
  WriteHDMIReg(STM_HDMI_SRZ_TAP_2, 0);
  WriteHDMIReg(STM_HDMI_SRZ_TAP_3, 0);
  WriteHDMIReg(STM_HDMI_SRZ_CTRL,  STM_HDMI_SRZ_CTRL_EXTERNAL_DATA_EN);

  DEXIT();
  return true;
}


void CSTmHDMI::StopV29PHY(void)
{
  DENTRY();

  WriteHDMIReg(STM_HDMI_SRZ_CTRL, STM_HDMI_SRZ_CTRL_POWER_DOWN);

  WriteHDMIReg(STM_HDMI_SRZ_PLL_CFG, STM_HDMI_SRZ_PLL_CFG_POWER_DOWN);
  while((ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_DLL_LCK) != 0);

  DEXIT();
}


bool CSTmHDMI::PostV29PhyConfiguration(const stm_mode_line_t*, ULONG tvStandard)
{
  /*
   * Default, do nothing, provided SoCs that do not have this do not have to
   * implement an empty function. Code should never get here.
   */
  DERROR("You haven't implemented a post configuration for the deepcolour PHY, why not?\n");
  return true;
}


void CSTmHDMI::EnableInterrupts(void)
{
  DENTRY();

  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  ULONG intenables = (STM_HDMI_INT_GLOBAL             |
                      STM_HDMI_INT_SW_RST             |
                      STM_HDMI_INT_NEW_FRAME          |
                      STM_HDMI_INT_GENCTRL_PKT        |
                      STM_HDMI_INT_PIX_CAP            |
                      STM_HDMI_INT_SPDIF_FIFO_OVERRUN);

  intenables |= m_ulIFrameManagerIntMask;

  if(m_bUseHotplugInt)
    intenables |= STM_HDMI_INT_HOT_PLUG;

#if 0
  /*
   * It looks like the PHY signal has been wired up directly to the interrupt,
   * so it cannot be cleared. Awaiting clarification from validation.
   */
  if(m_HWVersion == STM_HDMI_HW_V_2_9)
    intenables |= STM_HDMI_INT_SINK_TERM_PRESENT;
#endif

  DEBUGF2(2,("%s: int enables = 0x%08lx\n", __PRETTY_FUNCTION__, intenables));

  WriteHDMIReg(STM_HDMI_INT_EN, intenables);

  DEXIT();
}


bool CSTmHDMI::Start(const stm_mode_line_t* pModeLine, ULONG tvStandard)
{
  DENTRY();

  if(m_pCurrentMode)
  {
    if((pModeLine == m_pCurrentMode) && (tvStandard == m_ulTVStandard))
    {
      /*
       * If the requested mode is already running, just fix up the display
       * status if it has got out of sync. This can happen if a short
       * (out of spec) hotplug pulse has happened that triggered two
       * HPD interrupts (one low, one high), but was too fast for any higher
       * level hotplug handler to respond to both events.
       */
      g_pIOS->LockResource(m_statusLock);

      if(m_displayStatus == STM_DISPLAY_NEEDS_RESTART)
        m_displayStatus = STM_DISPLAY_CONNECTED;

      g_pIOS->UnlockResource(m_statusLock);
      return true;
    }

    DEBUGF2(1,("%s: mode already set %d @ %p\n",__PRETTY_FUNCTION__,pModeLine->Mode,pModeLine));
    return false;
  }

  DEBUGF2(2,("%s: mode %d @ %p\n",__PRETTY_FUNCTION__,pModeLine->Mode,pModeLine));

  /*
   * The HDMI is slaved off the timings of a main output, which must have
   * already been started.
   */
  if(m_pMasterOutput->GetCurrentDisplayMode() == 0)
  {
    DEBUGF2(1,("%s: master is not started\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_CEA861C) == 0 ||
     (tvStandard & STM_OUTPUT_STD_CEA861C) == 0)
  {
    DEBUGF2(2,("%s: cannot start a non CEA861C mode on HDMI\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((tvStandard & (STM_OUTPUT_STD_TMDS_PIXELREP_2X | STM_OUTPUT_STD_TMDS_PIXELREP_4X)) &&
     (m_ulOutputFormat & STM_VIDEO_OUT_DVI))
  {
    DEBUGF2(2,("%s: cannot pixel repeat in DVI mode\n",__PRETTY_FUNCTION__));
    return false;
  }

  if((pModeLine->ModeParams.OutputStandards & tvStandard) != tvStandard)
  {
    DEBUGF2(2,("%s: Unsupported TVStandard flag\n",__PRETTY_FUNCTION__));
    return false;
  }

  g_pIOS->LockResource(m_statusLock);
  if(m_displayStatus != STM_DISPLAY_NEEDS_RESTART)
  {
    DEBUGF2(2,("CSTmHDMI::Start - Display is disconnected\n"));
    g_pIOS->UnlockResource(m_statusLock);
    return false;
  }

  g_pIOS->UnlockResource(m_statusLock);

  /*
   * Do the necessary SoC specific configuration before changing the HDMI
   * configuration registers, this usually involves clock magic.
   */
  this->PreConfiguration(pModeLine, tvStandard);

  ULONG cfg = STM_HDMI_CFG_EN;

  if(m_ulOutputFormat & STM_VIDEO_OUT_HDMI)
  {
    DEBUGF2(2,("CSTmHDMI::Start - Enabling HDMI Mode\n"));
    cfg |= STM_HDMI_CFG_HDMI_NOT_DVI;
    cfg |= STM_HDMI_CFG_ESS_NOT_OESS;
  }
  else
  {
    DEBUGF2(2,("CSTmHDMI::Start - Enabling DVI Mode\n"));
    /*
     * In DVI mode ESS is used when HDCP 1.1 advanced features are enabled
     */
    if(m_bESS)
    {
      DEBUGF2(2,("CSTmHDMI::Start - Enabling Enhanced ESS\n"));
      cfg |= STM_HDMI_CFG_ESS_NOT_OESS;
    }
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_422)
  {
    DEBUGF2(2,("CSTmHDMI::Start - 4:2:2 Output\n"));
    cfg |= STM_HDMI_CFG_422_EN;
  }

  cfg |= ReadHDMIReg(STM_HDMI_CFG) & STM_HDMI_CFG_CP_EN;

  if(m_HWVersion == STM_HDMI_HW_V_2_9)
  {
    ProgramV29Config(cfg, pModeLine, tvStandard);
    StartV29PHY(pModeLine, tvStandard);
    ProgramActiveArea(pModeLine, tvStandard);
    /*
     * If we are enabling deepcolour, set up the GCP transmission
     * for the first active frame after we enable the hardware.
     */
    if(m_ulDeepcolourGCP != 0)
      SendGeneralControlPacket();

    /*
     * Now do SOC specific post phy configuration.
     */
    if(!this->PostV29PhyConfiguration(pModeLine, tvStandard))
    {
      this->Stop();
      return false;
    }

  }
  else
  {
    if((m_ulOutputFormat & STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK) != STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT)
    {
      DERROR("Deepcolour not supported on this device");
      return false;
    }

    ProgramV1Config(cfg, pModeLine, tvStandard);

    /*
     * We now think this is in fact spurious and that the behaviour behind
     * the registers was never implemented in hardware. Note that the registers
     * have gone away completely in the V2.9 cell.
     */
    if(pModeLine->ModeParams.FrameRate < 50000)
    {
      /*
       * For 1080p@24/25/30Hz we need the control period maximum delay
       * to be 1 vsync so the delay time does not exceed 50ms.
       */
      DEBUGF2(2,("CSTmHDMI::Start - Setting extended control period delay to 1 vsync\n"));
      WriteHDMIReg(STM_HDMI_EXTS_MAX_DLY, 1);
    }
    else
    {
      WriteHDMIReg(STM_HDMI_EXTS_MAX_DLY, STM_HDMI_EXTS_MAX_DLY_DEFAULT);
    }

    WriteHDMIReg(STM_HDMI_EXTS_MIN, STM_HDMI_EXTS_MIN_DEFAULT);

    /*
     * In this version of the cell the active area counters are driven from
     * the TMDS clock not the pixel clock, so we do not adjust for pixel
     * repeated modes hence the zeroing of the TV standard.
     */
    ProgramActiveArea(pModeLine, 0);
  }


  EnableInterrupts();

  /*
   * Write final configuration register state.
   */
  WriteHDMIReg(STM_HDMI_CFG, cfg);

  SWReset();

  /*
   * Now do SOC specific post configuration, usually involving more clock
   * magic, after this all HDMI/HDCP clocks must be configured and running!
   */
  if(!this->PostConfiguration(pModeLine, tvStandard))
  {
    this->Stop();
    return false;
  }

  if(!m_pIFrameManager->Start(pModeLine,tvStandard))
  {
    this->Stop();
    return false;
  }

  COutput::Start(pModeLine, tvStandard);

  g_pIOS->LockResource(m_statusLock);
  if(m_displayStatus == STM_DISPLAY_NEEDS_RESTART)
  {
    /*
     * The display is still connected so change the state to "connected"
     */
    m_displayStatus = STM_DISPLAY_CONNECTED;
    g_pIOS->UnlockResource(m_statusLock);
    DEBUGF2(2,("CSTmHDMI::Start - Display is now connected\n"));
  }
  else
  {
    /*
     * The user pulled the display cable out, or turned the TV off, while we
     * were starting up, so stop it all again.
     */
    g_pIOS->UnlockResource(m_statusLock);
    DEBUGF2(2,("CSTmHDMI::Start - Display has been disconnected, stopping again!\n"));
    this->Stop();
    return false;
  }

  DEXIT();
  return true;
}


bool CSTmHDMI::Stop(void)
{
  DENTRY();

  /*
   * Switch back to just the hotplug interrupt for devices where it is connected
   */
  if(m_bUseHotplugInt)
    WriteHDMIReg(STM_HDMI_INT_EN,(STM_HDMI_INT_GLOBAL | STM_HDMI_INT_HOT_PLUG));
  else
    WriteHDMIReg(STM_HDMI_INT_EN,0);

  if(m_HWVersion == STM_HDMI_HW_V_2_9)
    StopV29PHY();

  ULONG reg = ReadHDMIReg(STM_HDMI_CFG) & ~STM_HDMI_CFG_EN;
  WriteHDMIReg(STM_HDMI_CFG,reg);

  InvalidateAudioPackets();
  g_pIOS->ZeroMemory(&m_audioState,sizeof(stm_hdmi_audio_state_t));

  m_pIFrameManager->Stop();
  COutput::Stop();

  g_pIOS->LockResource(m_statusLock);

  if(m_displayStatus == STM_DISPLAY_CONNECTED)
    m_displayStatus = STM_DISPLAY_NEEDS_RESTART;

  g_pIOS->UnlockResource(m_statusLock);

  DEXIT();
  return true;
}


void CSTmHDMI::SWReset(void)
{
  ULONG cfg,sta;
  DENTRY();

  /*
   * Note that for the reset to work the audio SPDIF clock to the HDMI
   * must be running.
   */
  m_bSWResetCompleted = false;

  cfg = ReadHDMIReg(STM_HDMI_CFG) | STM_HDMI_CFG_SW_RST_EN;
  WriteHDMIReg(STM_HDMI_CFG,cfg);

  do
  {
    sta = ReadHDMIReg(STM_HDMI_STA);
  } while (((sta & STM_HDMI_STA_SW_RST) == 0) && !m_bSWResetCompleted);

  cfg &= ~STM_HDMI_CFG_SW_RST_EN;
  WriteHDMIReg(STM_HDMI_CFG,cfg);

  DEXIT();
}


void CSTmHDMI::UpdateFrame(void)
{
  if(m_pCurrentMode)
  {
    /*
     * If we are in a Deepcolour mode then we have to keep sending a GCP
     * every frame.
     */
    if(m_ulDeepcolourGCP != 0)
      SendGeneralControlPacket();

    /*
     * First do IFrame management, which will process the audio configuration
     * queue which may change information required by UpdateAudioState().
     *
     */
    m_LastVSyncTime = g_pIOS->GetSystemTime();
    m_pIFrameManager->UpdateFrame();
    m_pIFrameManager->SendFirstInfoFrame();

    UpdateAudioState();
  }
}


void CSTmHDMI::UpdateAudioState(void)
{
  if(m_audioState.status == STM_HDMI_AUDIO_DISABLED)
    return;

  stm_hdmi_audio_state_t state = {STM_HDMI_AUDIO_STOPPED};

  GetAudioHWState(&state);

  if (state.status != STM_HDMI_AUDIO_STOPPED)
  {
    if( state.clock_divide < 1 || state.clock_divide > m_maxAudioClockDivide)
    {
      /*
       * The audio clock setup was unusable by HDMI so disable audio.
       */
      DEBUGF2(1,("CSTi7200HDMI::UpdateAudioState unsupported audio clk divider = %lu\n",state.clock_divide));
      state.status = STM_HDMI_AUDIO_STOPPED;
    }
    else
    {
      state.fsynth_frequency = GetAudioFrequency();
    }
  }


  switch(m_audioState.status)
  {
    case STM_HDMI_AUDIO_RUNNING:
    {
      if(UNLIKELY(state.status == STM_HDMI_AUDIO_STOPPED))
      {
        InvalidateAudioPackets();
        m_audioState.status = STM_HDMI_AUDIO_STOPPED;
        DEBUGF2(3,("%s audio stopped on %s\n",
                   __PRETTY_FUNCTION__,
                   (m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)?"SPDIF":"I2S"));

        m_pIFrameManager->SetAudioValid(false);
      }
      else
      {
        if(m_audioState.clock_divide != state.clock_divide ||
           m_audioState.fsynth_frequency != state.fsynth_frequency)
        {
          SetAudioCTS(state);
        }
      }
      break;
    }
    case STM_HDMI_AUDIO_STOPPED:
    {
      if(UNLIKELY(state.status == STM_HDMI_AUDIO_RUNNING))
      {
        ULONG cfg = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~(STM_HDMI_AUD_CFG_DATATYPE_MASK|STM_HDMI_AUD_CFG_DST_DOUBLE_RATE);

        switch(m_ulAudioOutputType)
        {
          case STM_HDMI_AUDIO_TYPE_DST_DOUBLE:
            DEBUGF2(3,("%s: Selecting Double Rate DST\n",__PRETTY_FUNCTION__));
            cfg |= STM_HDMI_AUD_CFG_DST_DOUBLE_RATE;
            // Deliberate fallthrough
          case STM_HDMI_AUDIO_TYPE_DST:
            DEBUGF2(3,("%s: Selecting DST\n",__PRETTY_FUNCTION__));
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_DST;
            cfg &= ~STM_HDMI_AUD_CFG_DST_SAMPLE_INVALID;
            break;
          case STM_HDMI_AUDIO_TYPE_ONEBIT:
            DEBUGF2(3,("%s: Selecting 1-Bit Audio\n",__PRETTY_FUNCTION__));
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_ONEBIT;
            cfg &= ~STM_HDMI_AUD_CFG_ONEBIT_ALL_INVALID;
            break;
          case STM_HDMI_AUDIO_TYPE_HBR:
            DEBUGF2(3,("%s: Selecting HBR Audio\n",__PRETTY_FUNCTION__));
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_HBR;
            break;
          default:
            DEBUGF2(3,("%s: Selecting Normal Audio\n",__PRETTY_FUNCTION__));
            break;
        }

        WriteHDMIReg(STM_HDMI_AUDIO_CFG,cfg);

        m_audioState.N = 0;
        SetAudioCTS(state);

        WriteHDMIReg(STM_HDMI_SAMPLE_FLAT_MASK, 0x0);
        m_audioState.status = STM_HDMI_AUDIO_RUNNING;

        /*
         * Enable the sending of audio info frames, this will take effect
         * on the next HDMI new frame interrupt along with all of the hardware
         * programming we have just made.
         */
        m_pIFrameManager->SetAudioValid(true);

        DEBUGF2(3,("%s: audio running on %s, clkdiv = %lu\n",
                   __PRETTY_FUNCTION__,
                   (m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)?"SPDIF":"I2S",
                   state.clock_divide));
      }
      break;
    }
    default:
      DEBUGF2(1,("%s: Corrupt audio state\n",__PRETTY_FUNCTION__));
      break;
  }
}


bool CSTmHDMI::HandleInterrupts()
{
  ULONG its = ReadHDMIReg(STM_HDMI_INT_STA);

  WriteHDMIReg(STM_HDMI_INT_CLR, its);

  (void)ReadHDMIReg(STM_HDMI_STA); // sync bus

  if(its & STM_HDMI_INT_SW_RST)
  {
    m_bSWResetCompleted = true;
  }

  if(its & STM_HDMI_INT_PIX_CAP)
  {
    m_ulLastCapturedPixel = ReadHDMIReg(STM_HDMI_CHL0_CAP_DATA) & 0xff; // Only bottom 8 bits valid
  }

  if(its & STM_HDMI_INT_HOT_PLUG)
  {
    ULONG hotplugstate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_HOT_PLUG);

    if(m_displayStatus == STM_DISPLAY_DISCONNECTED)
    {
      /*
       * If the device has just been plugged in, flag that we now need to
       * start the output.
       */
      if(hotplugstate HPD_CHK_NE 0)
        m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
    }
    else
    {
      /*
       * We may either be waiting for the output to be started, or already started,
       * so _only_ change the state if the device has now been disconnected.
       */
      if(hotplugstate HPD_CHK_EQ 0)
        m_displayStatus = STM_DISPLAY_DISCONNECTED;
    }
  }
  else if(its & STM_HDMI_INT_SINK_TERM_PRESENT)
  {
    /*
     * If we got a change in sink termination, but no hotplug event, we initiate
     * a restart. Think about a HDMI switch that always asserts hotplug
     * regardless of the switch position, and doesn't pulse hotplug when the
     * switch's input is changed (they do exist!) So we may have been put into
     * a disconnected state, even though hotplug is asserted, after a failed
     * attempt to start the link (EDID read failed) so we need to force another
     * attempt to start. Or we may be connected and the switch had been switched
     * to another input (which we cannot detect because of the way the signal
     * has been integrated with the interrupt). Now the switch is switched back
     * to us, which we do detect, but we do not know if the sink has changed
     * its EDID or if the sink has been physically changed, so we have to force
     * a restart which will include a re-read of the EDID.
     */
    m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
  }

  if(its & STM_HDMI_INT_NEW_FRAME)
  {
    DEBUGF2(5,("HDMI New Frame Int\n"));
    UpdateFrame();
  }

  if(its & m_ulIFrameManagerIntMask)
  {
    DEBUGF2(5,("HDMI Info Frame Complete Int\n"));
    m_pIFrameManager->ProcessInfoFrameComplete(its & m_ulIFrameManagerIntMask);
  }

  if(its & STM_HDMI_INT_GENCTRL_PKT)
  {
    DEBUGF2(5,("HDMI General Control Packet Int\n"));
  }

  if(its & STM_HDMI_INT_SPDIF_FIFO_OVERRUN)
  {
    DEBUGF2(3,("HDMI SPDIF Overrun\n"));

    /*
     * Clear overrun status
     */
    ULONG val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) | STM_HDMI_AUD_CFG_FIFO_OVERRUN_CLR;
    WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);
  }

  return true;
}


// HDMI is a slave output and doesn't explicitly manage plane composition.
bool CSTmHDMI::CanShowPlane(stm_plane_id_t planeID) { return false; }
bool CSTmHDMI::ShowPlane   (stm_plane_id_t planeID) { return false; }
void CSTmHDMI::HidePlane   (stm_plane_id_t planeID) {}
bool CSTmHDMI::SetPlaneDepth(stm_plane_id_t planeID, int depth, bool activate) { return false; }
bool CSTmHDMI::GetPlaneDepth(stm_plane_id_t planeID, int *depth) const { return false; }


ULONG CSTmHDMI::SupportedControls(void) const
{
  ULONG caps = STM_CTRL_CAPS_VIDEO_OUT_SELECT | STM_CTRL_CAPS_SIGNAL_RANGE;

  if(m_ulOutputFormat & STM_VIDEO_OUT_HDMI)
  {
    caps |= STM_CTRL_CAPS_AVMUTE;
  }

  return caps;
}


void CSTmHDMI::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch (ctrl)
  {
    case STM_CTRL_SIGNAL_RANGE:
    {
      if(ulNewVal > STM_SIGNAL_VIDEO_RANGE)
        break;

      /*
       * Default implementation which just changes the AVI frame quantization
       * range, but doesn't enforce this in the hardware. Chips that can
       * actually change the clipping behaviour in the digital output stage
       * will override this to change the hardware as well.
       */
      m_signalRange = (stm_display_signal_range_t)ulNewVal;
      m_pIFrameManager->ForceAVIUpdate();
      break;
    }
    case STM_CTRL_HDMI_SINK_SUPPORTS_DEEPCOLOUR:
    {
      m_bSinkSupportsDeepcolour = (ulNewVal != 0);
      break;
    }
    case STM_CTRL_HDMI_PHY_CONF_TABLE:
    {
      m_pPHYConfig = (stm_display_hdmi_phy_config_t *)ulNewVal;
      break;
    }
    case STM_CTRL_HDMI_CEA_MODE_SELECT:
    {
      if(ulNewVal > STM_HDMI_CEA_MODE_16_9)
        break;

      m_pIFrameManager->SetCEAModeSelection((stm_hdmi_cea_mode_selection_t)ulNewVal);
      break;
    }
    case STM_CTRL_HDMI_AVI_QUANTIZATION:
    {
      if(ulNewVal > STM_HDMI_AVI_QUANTIZATION_BOTH)
        break;

      m_pIFrameManager->SetQuantization((stm_hdmi_avi_quantization_t)ulNewVal);
      break;
    }
    case STM_CTRL_HDMI_OVERSCAN_MODE:
    {
      if(ulNewVal > HDMI_AVI_INFOFRAME_UNDERSCAN)
        break;

      m_pIFrameManager->SetOverscanMode(ulNewVal);
      break;
    }
    case STM_CTRL_HDMI_CONTENT_TYPE:
    {
      m_pIFrameManager->SetAVIContentType(ulNewVal);
      break;
    }
    case STM_CTRL_AVMUTE:
    {
      g_pIOS->LockResource(m_statusLock);
      m_bAVMute = (ulNewVal != 0);
      SendGeneralControlPacket();
      g_pIOS->UnlockResource(m_statusLock);
      break;
    }
    case STM_CTRL_HDMI_POSTAUTH:
    {
      this->PostAuth((bool) ulNewVal);
      break;
    }
    case STM_CTRL_HDCP_ADVANCED:
    {
      /*
       * We need to keep track of HDCP mode to set the correct control
       * signaling in DVI mode.
       */
      m_bESS = (ulNewVal != 0);

      if(m_pCurrentMode && (m_ulOutputFormat & STM_VIDEO_OUT_DVI))
      {
        ULONG hdmicfg = ReadHDMIReg(STM_HDMI_CFG);

        if(m_bESS)
        {
          DEBUGF2(2,("CSTmHDMI::SetControl - DVI Extended ESS Signalling\n"));
          hdmicfg |= STM_HDMI_CFG_ESS_NOT_OESS;
        }
        else
        {
          DEBUGF2(2,("CSTmHDMI::SetControl - DVI Original ESS Signalling\n"));
      	  hdmicfg &= ~STM_HDMI_CFG_ESS_NOT_OESS;
        }

        WriteHDMIReg(STM_HDMI_CFG, hdmicfg);
      }

      break;
    }
    case STM_CTRL_VIDEO_OUT_SELECT:
    {
      SetOutputFormat(ulNewVal);
      break;
    }
    case STM_CTRL_YCBCR_COLORSPACE:
    {
      if(ulNewVal > STM_YCBCR_COLORSPACE_709)
        return;

      m_pIFrameManager->SetColorspaceMode(static_cast<stm_ycbcr_colorspace_t>(ulNewVal));
      break;
    }
    default:
    {
      DEBUGF2(2,("CSTmHDMI::SetControl Attempt to modify unexpected control %d\n",ctrl));
      break;
    }
  }
}


ULONG CSTmHDMI::GetControl(stm_output_control_t ctrl) const
{
  switch (ctrl)
  {
    case STM_CTRL_SIGNAL_RANGE:
    {
      return (ULONG)m_signalRange;
    }
    case STM_CTRL_HDMI_SINK_SUPPORTS_DEEPCOLOUR:
    {
      return m_bSinkSupportsDeepcolour?1:0;
    }
    case STM_CTRL_HDMI_PHY_CONF_TABLE:
    {
      return (ULONG)m_pPHYConfig;
    }
    case STM_CTRL_HDMI_CEA_MODE_SELECT:
    {
      return (ULONG)m_pIFrameManager->GetCEAModeSelection();
    }
    case STM_CTRL_HDMI_AVI_QUANTIZATION:
    {
      return (ULONG)m_pIFrameManager->GetQuantization();
    }
    case STM_CTRL_HDMI_OVERSCAN_MODE:
    {
      return m_pIFrameManager->GetOverscanMode();
    }
    case STM_CTRL_HDMI_CONTENT_TYPE:
    {
      return m_pIFrameManager->GetAVIContentType();
    }
    case STM_CTRL_AVMUTE:
    {
      return m_bAVMute?1:0;
    }
    case STM_CTRL_HDMI_PREAUTH:
    {
      return (ULONG) this->PreAuth();
    }
    case STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT:
    {
      return m_bUseHotplugInt?1:0;
    }
    case STM_CTRL_VIDEO_OUT_SELECT:
    {
      return m_ulOutputFormat;
    }
    case STM_CTRL_HDMI_AUDIO_OUT_SELECT:
    {
      return (ULONG)m_ulAudioOutputType;
    }
    case STM_CTRL_YCBCR_COLORSPACE:
    {
      return (ULONG)m_pIFrameManager->GetColorspaceMode();
    }
    default:
    {
      DEBUGF2(2,("CSTmHDMI::GetControl Attempt to access unexpected control %d\n",ctrl));
    }
  }

  return 0;
}


bool CSTmHDMI::SetOutputFormat(ULONG format)
{
  ULONG device_format = format & (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_HDMI);
  ULONG colour_format = format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422);
  ULONG colour_depth  = format & STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK;

  DEBUGF2(2,(FENTRY ": format = 0x%08lx\n",__PRETTY_FUNCTION__, format));

  if((colour_format & STM_VIDEO_OUT_422) &&
     (colour_depth != STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT))
  {
    DEBUGF2(2,("%s: Deepcolor not possible in 422\n",__PRETTY_FUNCTION__));
    return false;
  }

  switch (device_format)
  {
    case STM_VIDEO_OUT_DVI:
    case STM_VIDEO_OUT_HDMI:
    {
      if((m_ulOutputFormat & (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_HDMI)) != device_format)
      {
        DEBUGF2(2,("CSTmHDMI::SetOutputFormat - Switching HDMI/DVI Mode\n"));
        /*
         * Stopping the output will force the higher level HDMI to restart
         * it, assuming the TV is still connected, taking into account the
         * fact the new output format may not be compatible with the current
         * display mode.
         */
        this->Stop();
      }
      break;
    }
    default:
      DEBUGF2(2,("CSTmHDMI::SetOutputFormat - Unknown output mode\n"));
      return false;
  }

  if((m_ulOutputFormat & STM_VIDEO_OUT_HDMI_COLOURDEPTH_MASK) != colour_depth)
  {
    /*
     * Similar tactics to switching between HDMI and DVI. In this case we need
     * to change so much, including the TMDS clock and the PHY and serializer
     * configuration, that it is easier to start from scratch. Plus this catches
     * the case where changing the colour depth pushes the TMDS clock out of
     * range for the hardware.
     */
    this->Stop();
  }

  g_pIOS->LockResource(m_statusLock);

  if(m_pCurrentMode &&
     ((m_ulOutputFormat & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422)) != colour_format))
  {
    DEBUGF2(2,("CSTmHDMI::SetOutputFormat - Colour format changed, updating AVI frame\n"));
    m_pIFrameManager->ForceAVIUpdate();

    ULONG cfg = ReadHDMIReg(STM_HDMI_CFG);

    if(colour_format == STM_VIDEO_OUT_422)
    {
      DEBUGF2(2,("CSTmHDMI::SetOutputFormat - YUV 4:2:2\n"));
      cfg |= STM_HDMI_CFG_422_EN;
    }
    else
    {
      DEBUGF2(2,("CSTmHDMI::SetOutputFormat - YUV 4:4:4 / RGB\n"));
      cfg &= ~STM_HDMI_CFG_422_EN;
    }

    WriteHDMIReg(STM_HDMI_CFG,cfg);

  }

  /*
   * Deliberately update m_ulOutputFormat under the lock so its update is
   * atomic with InfoFrame processing.
   */
  m_ulOutputFormat = format;

  g_pIOS->UnlockResource(m_statusLock);

  DEXIT();

  return true;
}


ULONG CSTmHDMI::GetCapabilities(void) const
{
  if(m_HWVersion == STM_HDMI_HW_V_2_9)
    return (STM_OUTPUT_CAPS_TMDS | STM_OUTPUT_CAPS_TMDS_DEEPCOLOUR);
  else
    return STM_OUTPUT_CAPS_TMDS;
}


void CSTmHDMI::SendGeneralControlPacket(void)
{
  ULONG genpktctl = (m_ulDeepcolourGCP | STM_HDMI_GENCTRL_PKT_EN);

  if(m_bAVMute)
  {
    DEBUGF2(3,("CSTmHDMI::SendGeneralControlPacket - Enabling AVMute\n"));
    genpktctl |= STM_HDMI_GENCTRL_PKT_AVMUTE;
  }
  else
  {
    DEBUGF2(3,("CSTmHDMI::SendGeneralControlPacket - Disable AVMute\n"));
  }
  g_pIOS->CallBios (m_bAVMute ? 33 : 34, 2, 1, STM_HDMI_GENCTRL_PKT_AVMUTE);

  WriteHDMIReg(STM_HDMI_GEN_CTRL_PKT_CFG, genpktctl);
}


static ULONG iframe_frequencies[] = {0,32000,44100,48000,88200,96000,176400,192000};

/*
 * Helper for SetAudioCTS to do rounded integer division.
 */
static inline ULONG rounded_div(ULONG numerator, ULONG denominator)
{
  return (numerator + (denominator / 2)) / denominator;
}


void CSTmHDMI::SetAudioCTS(const stm_hdmi_audio_state_t &state)
{
  ULONG div;
  ULONG Fs;
  ULONG QuantizedFs;

  if(UNLIKELY(m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_DST ||
              m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_DST_DOUBLE))
  {
    /*
     * For DST audio the Fs used in CTS calculation is taken from the
     * sample frequency in the InfoFrame, even in double data rate mode.
     * Obviously the InfoFrame data must have been correctly set before
     * HDMI audio processing starts.
     */
    Fs = iframe_frequencies[m_pIFrameManager->GetAudioFrequencyIndex()];
  }
  else
  {
    Fs = ::rounded_div(state.fsynth_frequency, (128*state.clock_divide));
    if(UNLIKELY((m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR) &&
                (m_ulInputSource & STM_AV_SOURCE_SPDIF_INPUT)))
    {
      /*
       * For HBR audio we need to use the contents frame rate, which when
       * being fed from the S/PDIF player is a quarter of the audio clock.
       */
      Fs /= 4;
    }
  }

  /*
   * Create a nicely quantized version of Fs.
   *
   * 44100 has bit 2 set (meaning we need to use a decimal divisor to avoid
   * departing from the nominal).
   */
  QuantizedFs = 100 * ::rounded_div(Fs,100);

  /*
   * This sets up the HDMI audio clock regeneration parameter "N". Ideally
   * we want a value which allows a hardware generated _integer_ CTS value
   * to be picked such that the following relationship holds:
   *
   * (1) 128*Fs = pixclock * N / CTS
   *
   * We are assuming that we have non-coherent audio and video clocks, because
   * the HDMI spec uses the term coherent clocks without defining a precise
   * meaning and because in our systems the audio and video clocks are
   * individually tunable so they probably aren't coherent anyway.
   * Therefore we use the general relationships in the HDMI spec:
   *
   * (2). 128*Fs/1500 <= N <= 128*Fs/300
   * (3). recommended that N is as close to 128*Fs/1000 as possible
   *
   * Finding the best fit N for (1) is expensive and this is going to be called
   * from the vsync handling, probably in interrupt context. So, we have picked
   * two simple dividers that when used with nominal sampling frequencies will
   * give recommended values for most video pixel clocks. Other combinations
   * may result in jitter in the sink's regenerated clock, at this time it
   * isn't clear if this will be a problem.
   */

  switch(QuantizedFs)
  {
    case 44100:
    case 88200:
    case 176400:
      div = 900;
      break;
    default:
      div = 1000;
  }

  ULONG n = ::rounded_div((QuantizedFs*128), div);

  if(n != m_audioState.N)
  {
    WriteHDMIReg(STM_HDMI_AUDN, (unsigned long)n);
    m_audioState.N = n;
    DEBUGF2(3,("CSTmHDMI::SetAudioCTS Pixclock = %lu Freq = %lu new N = %lu\n",m_pCurrentMode->TimingParams.ulPixelClock,Fs,(unsigned long)n));
  }

  /*
   * Update the clock divides in case the oversampling clock as changed
   */
  ULONG tmp = ReadHDMIReg(STM_HDMI_AUDIO_CFG);
  tmp &= ~(STM_HDMI_AUD_CFG_SPDIF_CLK_MASK |
           STM_HDMI_AUD_CFG_CTS_CLK_DIV_MASK);

  tmp |= (((state.clock_divide-1)<<STM_HDMI_AUD_CFG_SPDIF_CLK_SHIFT)   |
          ((state.clock_divide-1)<<STM_HDMI_AUD_CFG_CTS_CLK_DIV_SHIFT));

  WriteHDMIReg(STM_HDMI_AUDIO_CFG, tmp);

  m_audioState.clock_divide = state.clock_divide;
  m_audioState.fsynth_frequency = state.fsynth_frequency;
}


void CSTmHDMI::InvalidateAudioPackets(void)
{
  ULONG tmp = ReadHDMIReg(STM_HDMI_AUDIO_CFG);
  tmp |= ( STM_HDMI_AUD_CFG_DST_SAMPLE_INVALID |
           STM_HDMI_AUD_CFG_ONEBIT_ALL_INVALID);
  WriteHDMIReg(STM_HDMI_AUDIO_CFG, tmp);

  WriteHDMIReg(STM_HDMI_SAMPLE_FLAT_MASK, STM_HDMI_SAMPLE_FLAT_ALL);
}


stm_meta_data_result_t CSTmHDMI::QueueMetadata(stm_meta_data_t *m)
{
  return m_pIFrameManager->QueueMetadata(m);
}


void CSTmHDMI::FlushMetadata(stm_meta_data_type_t type)
{
  m_pIFrameManager->FlushMetadata(type);
}

