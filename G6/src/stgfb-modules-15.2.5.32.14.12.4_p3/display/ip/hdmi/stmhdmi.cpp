/***********************************************************************
 *
 * File: display/ip/hdmi/stmhdmi.cpp
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>

#include <display/ip/displaytiming/stmvtg.h>
#include <display/ip/stmviewport.h>

#include "stmhdmi.h"
#include "stmhdmiphy.h"
#include "stmhdmiregs.h"
#include "stmiframemanager.h"

CSTmHDMI::CSTmHDMI(const char                  *name,
                   uint32_t                     id,
                   CDisplayDevice              *pDev,
                   stm_hdmi_hardware_features_t hwfeatures,
                   uint32_t                     uHDMIOffset,
                   COutput                     *pMasterOutput): COutput(name, id, pDev, pMasterOutput->GetTimingID())
{
  TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::CSTmHDMI pDev = %p", pDev );

  m_HWFeatures     = hwfeatures;

  m_ulCapabilities |= (OUTPUT_CAPS_HDMI | OUTPUT_CAPS_FORCED_COLOR );

  if((m_HWFeatures & STM_HDMI_DEEP_COLOR) == STM_HDMI_DEEP_COLOR)
    m_ulCapabilities |= OUTPUT_CAPS_HDMI_DEEPCOLOR;

  m_pDevRegs       = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_uHDMIOffset    = uHDMIOffset;
  m_pMasterOutput  = pMasterOutput;
  m_pPHY           = 0;
  m_pIFrameManager = 0;
  m_ulIFrameManagerIntMask = 0;

  TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::CSTmHDMI m_pDevRegs = %p m_uHDMIOffset = %#08x", m_pDevRegs,m_uHDMIOffset );

  m_statusLock = 0;

  m_ulOutputFormat = 0;

  m_bSWResetCompleted   = false;
  m_ulLastCapturedPixel = 0;

  m_bUseHotplugInt      = true;

  m_bAVMute             = false;

  m_bSinkSupportsDeepcolour = false;
  m_ulDeepcolourGCP         = STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN;

  m_uCurrentTMDSClockFreq = 0;

  /*
   * By default N will be set by the driver and the hardware will dynamically
   * calculate CTS.
   */
  m_bUseFixedNandCTS = false;

  m_ulAudioOutputType = STM_HDMI_AUDIO_TYPE_NORMAL;

  vibe_os_zero_memory(&m_audioState, sizeof(stm_hdmi_audio_state_t));
  /*
   * By default only support an audio clock of 128*Fs as CTS
   * audio clock regeneration is limited to this in original versions
   * of the hardware. This can be overridden by hardware specific classes
   */
  m_maxAudioClockDivide = 1;

  /*
   * Set the default audio clock crystal reference to 30MHz
   */
  m_audioClockReference = STM_CLOCK_REF_30MHZ;

  /*
   * Set decimition for 422 conversion
   */
  m_hdmi_decimation_bypass=false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmHDMI::~CSTmHDMI()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete m_pIFrameManager;
  delete m_pPHY;

  TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::~CSTmHDMI about to delete status lock" );

  if(m_statusLock)
    vibe_os_delete_resource_lock(m_statusLock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDMI::PowerOnSetup(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteHDMIReg(STM_HDMI_CFG, 0);
  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  /*
   * Ensure the PHY is completely powered down and the PADs are in the
   * correct state to meet the leakage current requirements of the HDMI
   * spec.
   */
  m_pPHY->Stop();

  if(m_bUseHotplugInt)
  {
    /*
     * Poll current status, as if the display is connected first time around
     * it will not trigger a hotplug interrupt.
     */
    uint32_t hotplugstate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_HOT_PLUG);
    if(hotplugstate != 0)
    {
      m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Create out initial hotplug detected" );
    }
    else
    {
      m_displayStatus = STM_DISPLAY_DISCONNECTED;
    }

    /*
     * Initialize the hot plug and Rx sense (if needed) interrupts.
     */
    if((m_HWFeatures & STM_HDMI_RX_SENSE)==STM_HDMI_RX_SENSE)
      WriteHDMIReg(STM_HDMI_INT_EN, (STM_HDMI_INT_GLOBAL    |
                                     STM_HDMI_INT_HOT_PLUG  |
                                     STM_HDMI_INT_SINK_TERM_PRESENT));
    else
      WriteHDMIReg(STM_HDMI_INT_EN, (STM_HDMI_INT_GLOBAL    |
                                     STM_HDMI_INT_HOT_PLUG ));
  }

  /*
   * Set the default channel data to be a dark red, so its
   * very obvious. We don't set this in ::Start since a
   * user process may alter its value and expect it to be
   * persistent through hotplug.
   */
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL0_DATA, 0);
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL1_DATA, 0);
  WriteHDMIReg(STM_HDMI_DEFAULT_CHL2_DATA, 0x60);

  InvalidateAudioPackets();

  /*
   * Configure a default audio clock regeneration parameter as some sinks are
   * unhappy if they cannot regenerate a valid audio clock, even if no valid
   * audio is being transmitted.
   */
  WriteHDMIReg(STM_HDMI_AUDN, 6144);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDMI::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_pPHY)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Create No PHY Object provided by subclass, cannot continue" );
    return false;
  }

  if(!COutput::Create())
    return false;

  if(m_pMasterOutput && !m_pMasterOutput->RegisterSlavedOutput(this))
    return false;

  m_statusLock = vibe_os_create_resource_lock();
  if(m_statusLock == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Create failed to create status lock" );
    return false;
  }

  if(!m_pIFrameManager)
  {
    /*
     * If a subclass has not already created an InfoFrame manager then
     * create the default CPU driven one.
     */
    m_pIFrameManager = new CSTmCPUIFrames(m_pDisplayDevice,m_uHDMIOffset);
    if(!m_pIFrameManager || !m_pIFrameManager->Create(this,m_pMasterOutput))
    {
      TRC( TRC_ID_ERROR, "Unable to create an Info Frame manager" );
      return false;
    }
  }

  m_ulIFrameManagerIntMask = m_pIFrameManager->GetIFrameCompleteHDMIInterruptMask();

  /*
   * Call this now we have the resource lock created
   */
  PowerOnSetup();

  /*
   * Set the default output format, which may be sub-classed to program
   * hardware. This needs to be done after the status lock has been created
   * and will set m_ulOutputFormat for us.
   *
   * Note: Must be called after PowerOnSetup() so any subclass power on
   *       setup is done first.
   */
  SetOutputFormat(STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


void CSTmHDMI::CleanUp(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  Stop();

  WriteHDMIReg(STM_HDMI_CFG, 0);
  WriteHDMIReg(STM_HDMI_INT_EN,  0);
  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  if(m_pMasterOutput)
    m_pMasterOutput->UnRegisterSlavedOutput(this);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


/*
 * Helper step methods for Start()
 */
void CSTmHDMI::ProgramActiveArea(const stm_display_mode_t *const pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Note: the HDMI horizontal start/stop pixel numbering appears to start
   * from 1 and is different to the programming for the compositor viewports.
   * The horizontal delay to active video that this produces has been verified
   * against the CEA-861 standards using a Quantum Data 882.
   */
  uint32_t div = 1;
  if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_2X)
  {
    TRC( TRC_ID_MAIN_INFO, "2X Pixel Repeat" );
    div = 2;
  }
  else if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_4X)
  {
    TRC( TRC_ID_MAIN_INFO, "4X Pixel Repeat" );
    div = 4;
  }

  uint32_t xpo = (pModeLine->mode_params.active_area_start_pixel/div) + 1;
  uint32_t xps = (pModeLine->mode_params.active_area_start_pixel/div) + (pModeLine->mode_params.active_area_width/div);

  TRC( TRC_ID_MAIN_INFO, "XMIN = %u", xpo );
  TRC( TRC_ID_MAIN_INFO, "XMAX = %u", xps );

  WriteHDMIReg(STM_HDMI_ACTIVE_VID_XMIN, xpo);
  WriteHDMIReg(STM_HDMI_ACTIVE_VID_XMAX, xps);

  uint32_t ypo;
  uint32_t yps;

  /*
   * Unlike the compositor viewports in interlaced modes the HDMI IP counts
   * field lines not pairs of lines.
   */
  ypo = pModeLine->mode_params.active_area_start_line;

  /*
   * The viewport stop line is inclusive, that is this line will be part
   * of the active video.
   */
  if(pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)
  {
    yps = pModeLine->mode_params.active_area_start_line+(pModeLine->mode_params.active_area_height/2)-1;
  }
  else
  {
    yps = pModeLine->mode_params.active_area_start_line+pModeLine->mode_params.active_area_height-1;
  }

  WriteHDMIReg(STM_HDMI_ACTIVE_VID_YMIN, ypo);
  WriteHDMIReg(STM_HDMI_ACTIVE_VID_YMAX, yps);

  TRC( TRC_ID_MAIN_INFO, "YMIN = %u", ypo );
  TRC( TRC_ID_MAIN_INFO, "YMAX = %u", yps );
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDMI::ProgramV1Config(uint32_t &cfg, const stm_display_mode_t *const pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pModeLine->mode_timing.hsync_polarity)
  {
    TRC( TRC_ID_MAIN_INFO, "Sync Negative" );
    cfg |= STM_HDMI_CFG_SYNC_POL_NEG;
  }

  if((pModeLine->mode_params.output_standards & STM_OUTPUT_STD_CEA861) &&
     (((pModeLine->mode_timing.pixel_clock_freq <= 108108000) &&
       (pModeLine->mode_params.flags & (STM_MODE_FLAGS_HDMI_PIXELREP_2X | STM_MODE_FLAGS_HDMI_PIXELREP_4X))) ||
      (pModeLine->mode_timing.pixel_clock_freq <= 27027000)))
  {
    TRC( TRC_ID_MAIN_INFO, "Setting DLL for CEA SD/ED/VGA modes" );
    if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_2X)
    {
      if(pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)
        cfg |= STM_HDMI_CFG_DLL_4X_TMDS;
      else
        cfg |= STM_HDMI_CFG_DLL_2X_TMDS;
    }
    else if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_4X)
    {
      if(pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)
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
    TRC( TRC_ID_MAIN_INFO, "Setting DLL for CEA HD, VESA XGA and NTG5 modes" );
    if(pModeLine->mode_timing.pixel_clock_freq > 74250000)
    {
      TRC( TRC_ID_MAIN_INFO, "HD/XGA DLL 1X" );
      cfg |= STM_HDMI_CFG_DLL_1X_TMDS;
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "HD/XGA DLL 2X" );
      cfg |= STM_HDMI_CFG_DLL_2X_TMDS;
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDMI::ProgramV29Config(uint32_t &cfg, const stm_display_mode_t *const pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  cfg |= (STM_HDMI_V29_CFG_SINK_TERM_DETECT_EN |
          STM_HDMI_V29_VID_FIFO_OVERRUN_CLR    |
          STM_HDMI_V29_VID_FIFO_UNDERRUN_CLR);

  if(!pModeLine->mode_timing.hsync_polarity)
  {
    TRC( TRC_ID_MAIN_INFO, "H Sync Negative" );
    cfg |= STM_HDMI_V29_CFG_H_SYNC_POL_NEG;
  }

  if(!pModeLine->mode_timing.vsync_polarity)
  {
    TRC( TRC_ID_MAIN_INFO, "V Sync Negative" );
    cfg |= STM_HDMI_V29_CFG_V_SYNC_POL_NEG;
  }

  if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_2X)
  {
    TRC( TRC_ID_MAIN_INFO, "2X Pixel Repeat" );
    cfg |= STM_HDMI_V29_VID_PIX_REP_2;
  }
  else if(pModeLine->mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_4X)
  {
    TRC( TRC_ID_MAIN_INFO, "4X Pixel Repeat" );
    cfg |= STM_HDMI_V29_VID_PIX_REP_4;
  }

  if(m_bSinkSupportsDeepcolour)
  {
    TRC( TRC_ID_MAIN_INFO, "Configure Deepcolor" );
    cfg |= STM_HDMI_V29_SINK_SUPPORTS_DEEP_COLOR;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDMI::SetDeepcolorGCPState(const stm_display_mode_t *const pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  switch(m_ulOutputFormat & STM_VIDEO_OUT_DEPTH_MASK)
  {
    case STM_VIDEO_OUT_30BIT:
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_30BIT;
      m_uCurrentTMDSClockFreq = (pModeLine->mode_timing.pixel_clock_freq *5) / 4;
      TRC( TRC_ID_MAIN_INFO, "Selected 30bit colour" );
      break;
    case STM_VIDEO_OUT_36BIT:
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_36BIT;
      m_uCurrentTMDSClockFreq = (pModeLine->mode_timing.pixel_clock_freq *2) / 3;
      TRC( TRC_ID_MAIN_INFO, "Selected 36bit colour" );
      break;
    case STM_VIDEO_OUT_48BIT:
      m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_48BIT;
      m_uCurrentTMDSClockFreq = (pModeLine->mode_timing.pixel_clock_freq *2);
      TRC( TRC_ID_MAIN_INFO, "Selected 48bit colour" );
      break;
    default:
    case STM_VIDEO_OUT_24BIT:
      if(m_bSinkSupportsDeepcolour)
        m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_24BIT;
      else
        m_ulDeepcolourGCP = STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN;

      m_uCurrentTMDSClockFreq = pModeLine->mode_timing.pixel_clock_freq;

      TRC( TRC_ID_MAIN_INFO, "Selected 24bit colour" );
      break;
  }

  if(((m_ulCapabilities & OUTPUT_CAPS_HDMI_DEEPCOLOR) == 0) && (m_ulDeepcolourGCP != STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN))
  {
    TRC( TRC_ID_MAIN_INFO, "Hardware does not support deepcolour" );
    return false;
  }

  if(!m_bSinkSupportsDeepcolour && (m_ulDeepcolourGCP != STM_HDMI_GENCTRL_PKT_COLORDEPTH_UNKNOWN))
  {
    TRC( TRC_ID_MAIN_INFO, "Sink does not support deepcolour" );
    return false;
  }

  /*
   * If we are enabling deepcolor, set up the GCP transmission
   * for the first active frame after we enable the hardware.
   */
  if(m_ulDeepcolourGCP != 0)
    SendGeneralControlPacket();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDMI::EnableInterrupts(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteHDMIReg(STM_HDMI_INT_CLR, 0xffffffff);

  uint32_t intenables = (STM_HDMI_INT_GLOBAL             |
                         STM_HDMI_INT_SW_RST             |
                         STM_HDMI_INT_NEW_FRAME          |
                         STM_HDMI_INT_GENCTRL_PKT        |
                         STM_HDMI_INT_PIX_CAP            |
                         STM_HDMI_INT_PLL_LCK            |
                         STM_HDMI_INT_SPDIF_FIFO_OVERRUN);

  /*
   * Add HPD and RX sense IRQs filter (For Rx sense only when this fonctionality is present)
   */
  uint32_t intfilterenabled = (STM_HDMI_IT_PRESCALER | STM_HDMI_NOISE_SUPP_WIDTH);

  intenables |= m_ulIFrameManagerIntMask;

  if(m_bUseHotplugInt)
  {
    intenables |= STM_HDMI_INT_HOT_PLUG;
    intfilterenabled |= STM_HDMI_HPD_FILTER_EN;
  }

  /*
   * It looks like the PHY signal has been wired up directly to the interrupt,
   * so it cannot be cleared. Awaiting clarification from validation.
   */
  if((m_HWFeatures & STM_HDMI_RX_SENSE)==STM_HDMI_RX_SENSE)
  {
    intenables |= STM_HDMI_INT_SINK_TERM_PRESENT;
    intfilterenabled |= STM_HDMI_SINK_TERM_FILTER_EN;
  }


  TRC( TRC_ID_MAIN_INFO, "int enables = 0x%08x", intenables );
  TRC( TRC_ID_MAIN_INFO, "int filter enables = 0x%08x", intfilterenabled );

  WriteHDMIReg(STM_HDMI_INT_EN, intenables);
  WriteHDMIReg(STM_HDMI_INT_FILTER, intfilterenabled);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDMI::IsPixelRepetitionCompatible(const stm_display_mode_t &baseMode,
                                           const stm_display_mode_t &repeatedMode,
                                           int repeat) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * TODO: Implement this!
   */
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmHDMI::IsModeHDMICompatible(const stm_display_mode_t &newMode) const
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  const stm_display_mode_t *master_mode;
  /*
   * The HDMI is slaved off the timings of a main output, which must have
   * already been started.
   */
  ASSERTF(m_pMasterOutput,("HDMI master output has become invalid"));

  if((master_mode = m_pMasterOutput->GetCurrentDisplayMode()) == 0)
  {
    TRC( TRC_ID_ERROR, "master is not started" );
    return false;
  }

  if((newMode.mode_params.output_standards & (STM_OUTPUT_STD_CEA861    |
                                              STM_OUTPUT_STD_VESA_MASK |
                                              STM_OUTPUT_STD_NTG5      |
                                              STM_OUTPUT_STD_HDMI_LLC_EXT)) == 0)
  {
    TRC( TRC_ID_ERROR, "cannot start standard 0x%08x on TMDS interface", newMode.mode_params.output_standards );
    return false;
  }

  uint32_t pixelrep_flags = newMode.mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_MASK;

  if(pixelrep_flags != 0)
  {
    if(m_ulOutputFormat & STM_VIDEO_OUT_DVI)
    {
      TRC( TRC_ID_ERROR, "cannot pixel repeat in DVI mode" );
      return false;
    }

    int repeat = 1;

    switch(pixelrep_flags)
    {
      case STM_MODE_FLAGS_HDMI_PIXELREP_2X:
        repeat = 2;
        break;
      case STM_MODE_FLAGS_HDMI_PIXELREP_4X:
        repeat = 4;
        break;
      default:
        TRC( TRC_ID_ERROR, "invalid pixel repeat flags" );
        return false;
    }

    if(!IsPixelRepetitionCompatible(*master_mode, newMode, repeat))
      return false;

    /*
     * IsPixelRepetitionCompatible does not check the pixel clock as it is
     * also used by UpdateOutputMode where the modes checked may have
     * slightly different clocks. But in this case the repeated pixel clock
     * must be an exact multiple of the master's clock.
     */
    if((master_mode->mode_timing.pixel_clock_freq * repeat) != newMode.mode_timing.pixel_clock_freq)
    {
      TRC( TRC_ID_ERROR, "repeated pixel clock frequency does not match master's clock" );
      return false;
    }
  }
  else
  {
    /*
     * NOTE: For the moment only allow HDMI modes that have both the same timings
     *       _and_ the same active video area. There may be a case in the
     *       future which could require us to need different active areas, in
     *       which case the test would be changed to just AreModeTimingsIdentical
     */
    if(!AreModesIdentical(*master_mode, newMode))
    {
      TRC( TRC_ID_ERROR, "HDMI mode and master timing modes are not the same" );
      return false;
    }
  }

  uint32_t flags_3d = newMode.mode_params.flags & STM_MODE_FLAGS_3D_MASK;
  switch(flags_3d)
  {
    case STM_MODE_FLAGS_NONE:
    case STM_MODE_FLAGS_3D_SBS_HALF:
    case STM_MODE_FLAGS_3D_TOP_BOTTOM:
      break; // OK
    case STM_MODE_FLAGS_3D_FRAME_PACKED:
    case STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE:
      /*
       * TODO: Add HDMI hardware support for these
       */
      TRC( TRC_ID_ERROR, "3D mode not supported 0x%x",flags_3d );
      return false;
    default:
      /*
       * TV panel 3D modes (not supported by the HDMI hardware) or invalid flags
       */
      TRC( TRC_ID_ERROR, "3D mode not supported 0x%x",flags_3d );
      return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


const stm_display_mode_t* CSTmHDMI::SupportedMode(const stm_display_mode_t *mode) const
{
  if(mode->mode_params.output_standards & STM_OUTPUT_STD_NTG5)
  {
    TRC(TRC_ID_MAIN_INFO,"Looking for valid NTG5 mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);
    return mode;
  }

  if(mode->mode_params.output_standards & STM_OUTPUT_STD_HDMI_LLC_EXT)
  {
    TRC(TRC_ID_MAIN_INFO,"Looking for valid 4K2K progressive mode");
    if(mode->mode_timing.pixel_clock_freq < 296703297 ||
       mode->mode_timing.pixel_clock_freq > 297000000)
    {
      TRCOUT(TRC_ID_MAIN_INFO,"pixel clock out of range , pixclock = %u", mode->mode_timing.pixel_clock_freq);
      return 0;
    }
    return mode;
  }

  if(mode->mode_params.output_standards & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
  {
    TRC(TRC_ID_MAIN_INFO,"Looking for valid HD/XGA mode, pixclock = %u", mode->mode_timing.pixel_clock_freq);

    if(mode->mode_params.output_standards & STM_OUTPUT_STD_AS4933)
    {
      TRCOUT(TRC_ID_MAIN_INFO,"No support for AS4933 yet");
      return 0;
    }

    if(mode->mode_timing.pixel_clock_freq < 65000000 ||
       mode->mode_timing.pixel_clock_freq > 148500000)
    {
      TRCOUT(TRC_ID_MAIN_INFO,"pixel clock out of range");
      return 0;
    }

    return mode;
  }

  /*
   * We support SD interlaced modes based on a 13.5Mhz pixel clock
   * and progressive modes at 27Mhz or 27.027Mhz. We also report support
   * for SD interlaced modes over HDMI where the clock is doubled and
   * pixels are repeated. Finally we support VGA (640x480p@59.94Hz and 60Hz)
   * for digital displays (25.18MHz pixclock)
   */
  if(mode->mode_params.output_standards & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
    TRC(TRC_ID_MAIN_INFO,"Looking for valid ED progressive mode");
    if(mode->mode_timing.pixel_clock_freq < 25174800 ||
       mode->mode_timing.pixel_clock_freq > 27027000)
    {
      TRCOUT(TRC_ID_MAIN_INFO,"pixel clock out of range");
      return 0;
    }

    return mode;
  }

  if(mode->mode_params.output_standards == STM_OUTPUT_STD_CEA861)
  {
    /*
     * Support all CEA specific pixel double/quad modes SD and ED for Dolby TrueHD audio
     */
    TRCOUT(TRC_ID_MAIN_INFO,"Looking for valid SD/ED HDMI mode");
    return mode;
  }

  if(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
  {
    TRC(TRC_ID_MAIN_INFO,"Looking for valid SD interlaced mode");
    if(mode->mode_timing.pixel_clock_freq < 13500000 ||
       mode->mode_timing.pixel_clock_freq > 13513500)
    {
      TRCOUT(TRC_ID_MAIN_INFO,"pixel clock out of range , pixclock = %u", mode->mode_timing.pixel_clock_freq);
      return 0;
    }
    return mode;
  }

  TRCOUT(TRC_ID_MAIN_INFO,"no matching mode found");

  return 0;
}


OutputResults CSTmHDMI::Start(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return STM_OUT_BUSY;

  if(m_bIsStarted)
  {
    if((m_CurrentOutputMode.mode_id != STM_TIMING_MODE_RESERVED) &&
      AreModesIdentical(*pModeLine, m_CurrentOutputMode) &&
     (pModeLine->mode_params.flags == m_CurrentOutputMode.mode_params.flags))
    {
      TRC( TRC_ID_MAIN_INFO, "mode are identical" );
      if(m_displayStatus == STM_DISPLAY_NEEDS_RESTART)
        m_displayStatus = STM_DISPLAY_CONNECTED;
      return STM_OUT_OK;
    }
  }

  TRC( TRC_ID_MAIN_INFO, "mode %d @ %p",pModeLine->mode_id,pModeLine );

  if(!IsModeHDMICompatible(*pModeLine))
    return STM_OUT_INVALID_VALUE;

  if(m_displayStatus == STM_DISPLAY_CONNECTED)
    m_displayStatus = STM_DISPLAY_NEEDS_RESTART;

  vibe_os_lock_resource(m_statusLock);
  if(m_displayStatus != STM_DISPLAY_NEEDS_RESTART)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Display is disconnected" );
    vibe_os_unlock_resource(m_statusLock);
    return STM_OUT_INVALID_VALUE;
  }

  vibe_os_unlock_resource(m_statusLock);

  /*
   * Do the necessary SoC specific configuration before changing the HDMI
   * configuration registers, this usually involves clock magic.
   */
  this->PreConfiguration(pModeLine);

  EnableInterrupts();

  uint32_t cfg = STM_HDMI_CFG_EN;

  if(m_ulOutputFormat & STM_VIDEO_OUT_HDMI)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Enabling HDMI Mode" );
    cfg |= STM_HDMI_CFG_HDMI_NOT_DVI;
    cfg |= STM_HDMI_CFG_ESS_NOT_OESS;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Enabling DVI Mode" );
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_422)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - 4:2:2 Output" );
    cfg |= STM_HDMI_CFG_422_EN;
  }

  if (m_hdmi_decimation_bypass)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Hdmi desimation for 4:2:2 Output bypassed" );
    cfg |= STM_HDMI_V29_422_DECIMATION_BYPASS;
  }

  cfg |= ReadHDMIReg(STM_HDMI_CFG) & STM_HDMI_CFG_CP_EN;

  if(!m_pPHY->Start(pModeLine, m_ulOutputFormat))
    return STM_OUT_INVALID_VALUE;

  if(!SetDeepcolorGCPState(pModeLine))
    goto error_exit;

  if((m_HWFeatures & STM_HDMI_DEEP_COLOR)==STM_HDMI_DEEP_COLOR)
  {
    ProgramV29Config(cfg, pModeLine);
    ProgramActiveArea(pModeLine);
  }
  else
  {
    ProgramV1Config(cfg, pModeLine);

    /*
     * We now think this is in fact spurious and that the behaviour behind
     * the registers was never implemented in hardware. Note that the registers
     * have gone away completely in the V2.9 cell.
     */
    if(pModeLine->mode_params.vertical_refresh_rate < 50000)
    {
      /*
       * For 1080p@24/25/30Hz we need the control period maximum delay
       * to be 1 vsync so the delay time does not exceed 50ms.
       */
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Setting extended control period delay to 1 vsync" );
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
     * repeated modes hence the zeroing of the flags.
     */
    stm_display_mode_t tmpmode = *pModeLine;
    tmpmode.mode_params.flags &= ~(STM_MODE_FLAGS_HDMI_PIXELREP_2X | STM_MODE_FLAGS_HDMI_PIXELREP_4X);
    ProgramActiveArea(&tmpmode);
  }

  /*
   * Write final configuration register state.
   */
  WriteHDMIReg(STM_HDMI_CFG, cfg);
  TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - HDMI CFG 0x%08x", ReadHDMIReg(STM_HDMI_CFG) );

  /*
   * Now do SOC specific post configuration, usually involving more clock
   * magic, after this all HDMI/HDCP clocks must be configured and running!
   */
  if(!this->PostConfiguration(pModeLine))
    goto error_exit;

  SWReset();

  if(!m_pIFrameManager->Start(pModeLine))
    goto error_exit;

  COutput::Start(pModeLine);

  if(!this->SetOutputFormat(m_ulOutputFormat))
    goto error_exit;

  vibe_os_lock_resource(m_statusLock);
  if(m_displayStatus == STM_DISPLAY_NEEDS_RESTART)
  {
    /*
     * The display is still connected so change the state to "connected"
     */
    uint32_t rxsensestate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_SINK_TERM);
    if((m_HWFeatures & STM_HDMI_RX_SENSE)==STM_HDMI_RX_SENSE)
    {
      if(rxsensestate != 0)
        m_displayStatus = STM_DISPLAY_CONNECTED;
      else
        m_displayStatus = STM_DISPLAY_INACTIVE;
    }
    else
    {
      m_displayStatus = STM_DISPLAY_CONNECTED;
    }

    vibe_os_unlock_resource(m_statusLock);
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Display is now connected" );
  }
  else
  {
    /*
     * The user pulled the display cable out, or turned the TV off, while we
     * were starting up, so stop it all again.
     */
    vibe_os_unlock_resource(m_statusLock);
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::Start - Display has been disconnected, stopping again!" );
    goto error_exit;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return STM_OUT_OK;

error_exit:
  this->Stop();
  return STM_OUT_INVALID_VALUE;
}


bool CSTmHDMI::DisableHW(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return false;

  /*
   * Switch back to just the hotplug interrupt for devices where it is connected
   */
  if(m_bUseHotplugInt)
  {
    if((m_HWFeatures & STM_HDMI_RX_SENSE)==STM_HDMI_RX_SENSE)
      WriteHDMIReg(STM_HDMI_INT_EN,(STM_HDMI_INT_GLOBAL | STM_HDMI_INT_HOT_PLUG |STM_HDMI_INT_SINK_TERM_PRESENT));
    else
      WriteHDMIReg(STM_HDMI_INT_EN,(STM_HDMI_INT_GLOBAL | STM_HDMI_INT_HOT_PLUG ));
  }
  else
  {
    WriteHDMIReg(STM_HDMI_INT_EN,0);
  }

  m_pIFrameManager->Stop();

  vibe_os_lock_resource(m_statusLock);
  {
    uint32_t reg = ReadHDMIReg(STM_HDMI_CFG) & ~STM_HDMI_CFG_EN;
    WriteHDMIReg(STM_HDMI_CFG,reg);

    InvalidateAudioPackets();

    m_pPHY->Stop();

    vibe_os_zero_memory(&m_audioState,sizeof(stm_hdmi_audio_state_t));
    m_uCurrentTMDSClockFreq = 0;

    DisableClocks();
  }
  vibe_os_unlock_resource(m_statusLock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmHDMI::Stop(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if (!DisableHW())
    return false;

  vibe_os_lock_resource(m_statusLock);
  {
    /*
     * Reset m_CurrentMode under lock.
     */
    COutput::Stop();

    if(m_displayStatus == STM_DISPLAY_CONNECTED)
      m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
  }
  vibe_os_unlock_resource(m_statusLock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTmHDMI::SWReset(void)
{
  uint32_t cfg,sta,timeout;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Note that for the reset to work the audio SPDIF clock to the HDMI
   * must be running.
   */
  m_bSWResetCompleted = false;

  cfg = ReadHDMIReg(STM_HDMI_CFG) | STM_HDMI_CFG_SW_RST_EN;
  WriteHDMIReg(STM_HDMI_CFG,cfg);

  timeout = 0;
  do
  {
    sta = ReadHDMIReg(STM_HDMI_STA);
    /*
     * We really shouldn't have to do this.
     */
    if(++timeout == 100000)
    {
      TRC( TRC_ID_MAIN_INFO, "Warning Softreset timeout" );
      break;
    }
  } while (((sta & STM_HDMI_STA_SW_RST) == 0) && !m_bSWResetCompleted);

  cfg &= ~STM_HDMI_CFG_SW_RST_EN;
  WriteHDMIReg(STM_HDMI_CFG,cfg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDMI::UpdateOutputMode(const stm_display_mode_t &newmode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t new_pixel_clock;
  bool requires_restart = false;

  if(!m_bIsStarted)
  {
    TRC( TRC_ID_MAIN_INFO, "unable to update output mode" );
    vibe_os_lock_resource(m_statusLock);
    {
      if(m_displayStatus == STM_DISPLAY_CONNECTED)
        m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
    }
    vibe_os_unlock_resource(m_statusLock);
    return;
  }

  if(m_CurrentOutputMode.mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_MASK)
  {
    /*
     * The tricky case we have to reconcile the updated mode with the pixel
     * repeated version.
     */
    int repeat = (m_CurrentOutputMode.mode_params.flags & STM_MODE_FLAGS_HDMI_PIXELREP_2X)?2:4;
    new_pixel_clock = newmode.mode_timing.pixel_clock_freq * repeat;
    TRC( TRC_ID_MAIN_INFO, "Pixel repeated mode change: repeat = %d new pixel clock = %u",repeat,new_pixel_clock );
    /*
     * Not implemented today to do later on.
     * bypass and do Force restart in context of pixel repeated format switch
     */
    //if(!IsPixelRepetitionCompatible(newmode, m_CurrentOutputMode, repeat))
    {
      TRC( TRC_ID_MAIN_INFO, "Incompatible modes, stopping HDMI output" );
      requires_restart = true;
    }
  }
  else
  {
    new_pixel_clock = newmode.mode_timing.pixel_clock_freq;

    TRC( TRC_ID_MAIN_INFO, "Standard mode change: new pixel clock = %u",new_pixel_clock );

    /*
     * Sanity check the new mode, if we don't like it stop the output and let
     * the upper level manager deal with it with a restart.
     */
    if(!AreModesCompatible(m_CurrentOutputMode, newmode))
    {
      TRC( TRC_ID_MAIN_INFO, "Incompatible modes, stopping HDMI output" );
      requires_restart = true;
    }

    if((m_CurrentOutputMode.mode_params.hdmi_vic_codes[0] != newmode.mode_params.hdmi_vic_codes[0]) ||
       (m_CurrentOutputMode.mode_params.hdmi_vic_codes[1] != newmode.mode_params.hdmi_vic_codes[1]))
    {
      TRC( TRC_ID_MAIN_INFO, "VIC codes do not match, stopping HDMI output" );
      requires_restart = true;
    }
  }

  /*
   * Handle a change in 3D flags.
   * Force restart in context of 3d mode switch
   */
  uint32_t new_3d_flags = (newmode.mode_params.flags & STM_MODE_FLAGS_3D_MASK);
  if((m_CurrentOutputMode.mode_params.flags & STM_MODE_FLAGS_3D_MASK) != new_3d_flags)
  {
      TRC( TRC_ID_MAIN_INFO, "3D mode switched, stopping HDMI output" );
      requires_restart = true;
  }

  if(requires_restart)
  {
    TRC( TRC_ID_MAIN_INFO, "HDMI output requires restart, m_displayStatus = %d ", m_displayStatus);
    vibe_os_lock_resource(m_statusLock);
    {
      /*
       * Display requires restart, Set connection to NEEDS_RESTART in order to allow HDMI manager to wake up
       * and check format against edid indication
       */
      if(m_displayStatus == STM_DISPLAY_CONNECTED)
      {
        m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
        TRC( TRC_ID_MAIN_INFO, "HDMI STM_DISPLAY_NEEDS_RESTART " );
      }
    }
    vibe_os_unlock_resource(m_statusLock);
    return;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDMI::UpdateFrame(void)
{
  vibe_os_lock_resource(m_statusLock);
  if(m_bIsStarted)
  {
    /*
     * If we are in a Deepcolour mode then we have to keep sending a GCP
     * every frame.
     */
    if(m_ulDeepcolourGCP != 0)
      SendGeneralControlPacket();

    vibe_os_unlock_resource(m_statusLock);
    /*
     * First do IFrame management, which will process the audio configuration
     * queue which may change information required by UpdateAudioState().
     *
     */
    m_LastVTGEventTime = vibe_os_get_system_time();
    m_pIFrameManager->UpdateFrame();
    m_pIFrameManager->SendFirstInfoFrame();

    UpdateAudioState();
  }
  else
  {
    vibe_os_unlock_resource(m_statusLock);
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
    if( state.clock_divide < 1 || state.clock_cts_divide > m_maxAudioClockDivide)
    {
      /*
       * The audio clock setup was unusable by HDMI so disable audio.
       */
      TRC( TRC_ID_ERROR, "CSTmHDMI::UpdateAudioState unsupported audio clk divider = %u", state.clock_cts_divide );
      state.status = STM_HDMI_AUDIO_STOPPED;
    }
    else
    {
      state.fsynth_frequency = GetAudioFrequency();
    }
  }

  /*
   * This is a pretty gross lock
   */
  vibe_os_lock_resource(m_statusLock);

  switch(m_audioState.status)
  {
    case STM_HDMI_AUDIO_RUNNING:
    {
      if(UNLIKELY(state.status == STM_HDMI_AUDIO_STOPPED))
      {
        InvalidateAudioPackets();
        m_audioState.status = STM_HDMI_AUDIO_STOPPED;
        TRC( TRC_ID_UNCLASSIFIED, "audio stopped on %s", (m_AudioSource == STM_AUDIO_SOURCE_SPDIF)?"SPDIF":"I2S" );

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
        uint32_t cfg = ReadHDMIReg(STM_HDMI_AUDIO_CFG) & ~(STM_HDMI_AUD_CFG_DATATYPE_MASK|STM_HDMI_AUD_CFG_DST_DOUBLE_RATE);

        switch(m_ulAudioOutputType)
        {
          case STM_HDMI_AUDIO_TYPE_DST_DOUBLE:
            TRC( TRC_ID_UNCLASSIFIED, "Selecting Double Rate DST" );
            cfg |= STM_HDMI_AUD_CFG_DST_DOUBLE_RATE;
            // Deliberate fallthrough
          case STM_HDMI_AUDIO_TYPE_DST:
            TRC( TRC_ID_UNCLASSIFIED, "Selecting DST" );
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_DST;
            cfg &= ~STM_HDMI_AUD_CFG_DST_SAMPLE_INVALID;
            break;
          case STM_HDMI_AUDIO_TYPE_ONEBIT:
            TRC( TRC_ID_UNCLASSIFIED, "Selecting 1-Bit Audio" );
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_ONEBIT;
            cfg &= ~STM_HDMI_AUD_CFG_ONEBIT_ALL_INVALID;
            break;
          case STM_HDMI_AUDIO_TYPE_HBR:
            TRC( TRC_ID_UNCLASSIFIED, "Selecting HBR Audio" );
            cfg |= STM_HDMI_AUD_CFG_DATATYPE_HBR;
            break;
          default:
            TRC( TRC_ID_UNCLASSIFIED, "Selecting Normal Audio" );
            break;
        }

        cfg = cfg & (~STM_HDMI_AUD_CFG_DISABLE_AUDIO_TRANSMISSION);
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

        TRC( TRC_ID_UNCLASSIFIED, "audio running on %s, clkdiv = %u", (m_AudioSource == STM_AUDIO_SOURCE_SPDIF)?"SPDIF":"I2S", state.clock_divide );
      }
      break;
    }
    case STM_HDMI_AUDIO_DISABLED:
      break;
    default:
      TRC( TRC_ID_ERROR, "Corrupt audio state" );
      break;
  }

  vibe_os_unlock_resource(m_statusLock);
}


bool CSTmHDMI::HandleInterrupts()
{
  uint32_t its = ReadHDMIReg(STM_HDMI_INT_STA);
  uint32_t rxsensestate;

  WriteHDMIReg(STM_HDMI_INT_CLR, its);

  (void)ReadHDMIReg(STM_HDMI_STA); // sync bus

  if(its & STM_HDMI_INT_HOT_PLUG)
  {
    uint32_t hotplugstate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_HOT_PLUG);
    vibe_os_lock_resource(m_statusLock);

    if(m_displayStatus == STM_DISPLAY_DISCONNECTED)
    {
      /*
       * If the device has just been plugged in, flag that we now need to
       * start the output.
       */
      if(hotplugstate != 0)
      {
        m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
      }
    }
    else
    {
      /*
       * We may either be waiting for the output to be started, or already started,
       * so _only_ change the state if the device has now been disconnected.
       */
      if(hotplugstate == 0)
      {
        uint32_t reg = ReadHDMIReg(STM_HDMI_CFG) & ~STM_HDMI_CFG_EN;
        WriteHDMIReg(STM_HDMI_CFG,reg);
        m_pIFrameManager->Stop();
        m_displayStatus = STM_DISPLAY_DISCONNECTED;
      }
    }

    vibe_os_unlock_resource(m_statusLock);
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

    rxsensestate = (ReadHDMIReg(STM_HDMI_STA) & STM_HDMI_STA_SINK_TERM);

    vibe_os_lock_resource(m_statusLock);

    if(((m_HWFeatures & STM_HDMI_RX_SENSE)==STM_HDMI_RX_SENSE)&& (m_displayStatus!= STM_DISPLAY_DISCONNECTED))
    {
      if(rxsensestate != 0)
        m_displayStatus = STM_DISPLAY_NEEDS_RESTART;
      else
        m_displayStatus = STM_DISPLAY_INACTIVE;
    }
    vibe_os_unlock_resource(m_statusLock);
  }

  if(m_bIsSuspended)
    return true;

  if(its & STM_HDMI_INT_SW_RST)
  {
    m_bSWResetCompleted = true;
  }

  if(its & STM_HDMI_INT_PIX_CAP)
  {
    m_ulLastCapturedPixel = ReadHDMIReg(STM_HDMI_CHL0_CAP_DATA) & 0xff; // Only bottom 8 bits valid
  }

  if(its & STM_HDMI_INT_NEW_FRAME)
  {
    TRC( TRC_ID_UNCLASSIFIED, "HDMI New Frame Int" );
    UpdateFrame();
  }

  if(its & m_ulIFrameManagerIntMask)
  {
    TRC( TRC_ID_UNCLASSIFIED, "HDMI Info Frame Complete Int" );
    m_pIFrameManager->ProcessInfoFrameComplete(its & m_ulIFrameManagerIntMask);
  }

  if(its & STM_HDMI_INT_GENCTRL_PKT)
  {
    TRC( TRC_ID_UNCLASSIFIED, "HDMI General Control Packet Int" );
  }

  if(its & STM_HDMI_INT_SPDIF_FIFO_OVERRUN)
  {
    TRC( TRC_ID_UNCLASSIFIED, "HDMI SPDIF Overrun" );

    /*
     * Clear overrun status
     */
    uint32_t val = ReadHDMIReg(STM_HDMI_AUDIO_CFG) | STM_HDMI_AUD_CFG_FIFO_OVERRUN_CLR;
    WriteHDMIReg(STM_HDMI_AUDIO_CFG, val);
  }

  return true;
}


uint32_t CSTmHDMI::SetCompoundControl(stm_output_control_t ctrl, void *newVal)
{
  switch (ctrl)
  {
    case OUTPUT_CTRL_HDMI_PHY_CONF_TABLE:
    {
      m_pPHY->SetPhyConfig((stm_display_hdmi_phy_config_t *)newVal);
      break;
    }
    case OUTPUT_CTRL_DISPLAY_ASPECT_RATIO:
    {
      stm_rational_t *ar = (stm_rational_t *)newVal;
      stm_avi_vic_selection_t vic_selection = STM_AVI_VIC_FOLLOW_PICTURE_ASPECT_RATIO;
      if((ar->numerator == 16) && (ar->denominator == 9))
      {
        vic_selection = STM_AVI_VIC_16_9;
      }
      else if((ar->numerator == 4) && (ar->denominator == 3))
      {
        vic_selection = STM_AVI_VIC_4_3;
      }
      m_pIFrameManager->SetControl(OUTPUT_CTRL_AVI_VIC_SELECT, vic_selection);
      break;
    }
    default:
      TRC( TRC_ID_ERROR, "CSTmHDMI::SetCompoundControl Attempt to modify unexpected control %d", ctrl );
      return STM_OUT_NO_CTRL;
  }

  return STM_OUT_OK;
}


uint32_t CSTmHDMI::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch (ctrl)
  {
    case OUTPUT_CTRL_AVI_VIC_SELECT:
    case OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT:
    case OUTPUT_CTRL_AVI_QUANTIZATION_MODE:
    case OUTPUT_CTRL_AVI_SCAN_INFO:
    case OUTPUT_CTRL_AVI_CONTENT_TYPE:
    case OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO:
    {
      return m_pIFrameManager->SetControl(ctrl, newVal);
    }
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      if(newVal > STM_SIGNAL_VIDEO_RANGE)
        return STM_OUT_INVALID_VALUE;

      m_signalRange = static_cast<stm_display_signal_range_t>(newVal);

      SetSignalRangeClipping();
      (void)m_pIFrameManager->SetControl(ctrl, newVal); // Deliberately ignoring the return value from this
      break;
    }
    case OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR:
    {
      if((m_ulCapabilities & OUTPUT_CAPS_HDMI_DEEPCOLOR) == 0)
        return STM_OUT_NO_CTRL;

      m_bSinkSupportsDeepcolour = (newVal != 0);
      break;
    }
    case OUTPUT_CTRL_ENABLE_HOTPLUG_INTERRUPT:
    {
      m_bUseHotplugInt = (newVal != 0);
      break;
    }
    case OUTPUT_CTRL_HDMI_AVMUTE:
    {
      vibe_os_lock_resource(m_statusLock);
      m_bAVMute = (newVal != 0);
      SendGeneralControlPacket();
      vibe_os_unlock_resource(m_statusLock);
      break;
    }
    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
    {
      if(!SetOutputFormat(newVal))
        return STM_OUT_INVALID_VALUE;

      break;
    }
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
    {
      if(newVal > STM_AUDIO_SOURCE_8CH_I2S)
        return STM_OUT_INVALID_VALUE;

      if(!SetAudioSource(static_cast<stm_display_output_audio_source_t>(newVal)))
        return STM_OUT_INVALID_VALUE;

      break;
    }
    case OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT:
    {
      if(newVal > STM_HDMI_AUDIO_TYPE_HBR)
        return STM_OUT_INVALID_VALUE;

      /*
       * Default implementation for when there are no SoC restrictions to
       * the supported audio formats.
       *
       * Don't do anything if the value isn't changing
       */
      if(m_ulAudioOutputType == static_cast<stm_hdmi_audio_output_type_t>(newVal))
        return STM_OUT_OK;

      TRC( TRC_ID_MAIN_INFO, "change audio output type to %u",newVal );

      /*
       * We are being told there is a change to the audio stream, so stop the
       * current audio processing and get the audio update interrupt handling
       * to sort it out.
       */
      vibe_os_lock_resource(m_statusLock);

      InvalidateAudioPackets();

      m_ulAudioOutputType = static_cast<stm_hdmi_audio_output_type_t>(newVal);
      m_audioState.status = STM_HDMI_AUDIO_STOPPED;

      vibe_os_unlock_resource(m_statusLock);
      break;
    }
    default:
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetControl Attempt to modify unexpected control %d", ctrl );
      return STM_OUT_NO_CTRL;
    }
  }

  return STM_OUT_OK;
}


uint32_t CSTmHDMI::GetControl(stm_output_control_t ctrl, uint32_t *val) const
{
  switch (ctrl)
  {
    case OUTPUT_CTRL_VIDEO_SOURCE_SELECT:
    {
      *val = (uint32_t)m_VideoSource;
      break;
    }
    case OUTPUT_CTRL_AUDIO_SOURCE_SELECT:
    {
      *val = (uint32_t)m_AudioSource;
      break;
    }
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      *val = (uint32_t)m_signalRange;
      break;
    }
    case OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR:
    {
      if((m_ulCapabilities & OUTPUT_CAPS_HDMI_DEEPCOLOR) == 0)
        return STM_OUT_NO_CTRL;

      *val = m_bSinkSupportsDeepcolour?1:0;
      break;
    }
    case OUTPUT_CTRL_AVI_VIC_SELECT:
    case OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT:
    case OUTPUT_CTRL_AVI_QUANTIZATION_MODE:
    case OUTPUT_CTRL_AVI_SCAN_INFO:
    case OUTPUT_CTRL_AVI_CONTENT_TYPE:
    case OUTPUT_CTRL_AVI_EXTENDED_COLORIMETRY_INFO:
    {
      if(!m_pIFrameManager)
      {
        *val = 0;
        return STM_OUT_NO_CTRL;
      }

      return m_pIFrameManager->GetControl(ctrl,val);
    }
    case OUTPUT_CTRL_HDMI_AVMUTE:
    {
      *val = m_bAVMute?1:0;
      break;
    }
    case OUTPUT_CTRL_ENABLE_HOTPLUG_INTERRUPT:
    {
      *val = m_bUseHotplugInt?1:0;
      break;
    }
    case OUTPUT_CTRL_VIDEO_OUT_SELECT:
    {
      *val = m_ulOutputFormat;
      break;
    }
    case OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT:
    {
      *val = (uint32_t)m_ulAudioOutputType;
      break;
    }
    default:
      return COutput::GetControl(ctrl,val);

  }

  return STM_OUT_OK;
}


bool CSTmHDMI::SetOutputFormat(uint32_t format)
{
  uint32_t device_format = format & (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_HDMI);
  uint32_t colour_format = format & (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_444);
  uint32_t colour_depth  = format & STM_VIDEO_OUT_DEPTH_MASK;

  TRCIN( TRC_ID_MAIN_INFO, "format = 0x%08x", format );

  if (format == 0) /*disable HDMI output*/
  {
     TRC( TRC_ID_MAIN_INFO, "HDMI interface disabled with format=0" );
     DisableHW();

     vibe_os_lock_resource(m_statusLock);
     {
       m_CurrentOutputMode.mode_id = STM_TIMING_MODE_RESERVED;
       vibe_os_zero_memory(&m_CurrentOutputMode.mode_timing, sizeof(stm_display_mode_timing_t));
       /*
        * Display is connected with TMDS stopped since format=0, set connecttion status to "connected"
        */
       if (m_displayStatus == STM_DISPLAY_NEEDS_RESTART)
         m_displayStatus = STM_DISPLAY_CONNECTED;
     }
     vibe_os_unlock_resource(m_statusLock);

     return true;
  }
  if(format & (STM_VIDEO_OUT_YC | STM_VIDEO_OUT_CVBS | STM_VIDEO_OUT_ITUR656))
  {
    TRC( TRC_ID_ERROR, "Invalid format flags specified for HDMI output" );
    return false;
  }

  if((colour_format & STM_VIDEO_OUT_RGB) &&
     ((colour_format & ~STM_VIDEO_OUT_RGB) != 0))
  {
    TRC( TRC_ID_ERROR, "RGB and YUV cannot be specified together" );
    return false;
  }

  if((colour_format & (STM_VIDEO_OUT_422 | STM_VIDEO_OUT_444)) == (STM_VIDEO_OUT_422 | STM_VIDEO_OUT_444))
  {
    TRC( TRC_ID_ERROR, "444 and 422 cannot be specified together" );
    return false;
  }

  if((colour_format & (STM_VIDEO_OUT_422 | STM_VIDEO_OUT_444)) != 0)
    colour_format |= STM_VIDEO_OUT_YUV; // Ensure YUV is set if 444 or 422 is specified

  if((colour_format & ~STM_VIDEO_OUT_YUV) == 0)
    colour_format |= STM_VIDEO_OUT_444; // Default YUV to 444 if not explicitly specified

  switch(colour_depth)
  {
    case STM_VIDEO_OUT_16BIT:
    case STM_VIDEO_OUT_18BIT:
    case STM_VIDEO_OUT_20BIT:
      TRC( TRC_ID_ERROR, "< 24bit color not supported on HDMI/DVI" );
      return false;
    case STM_VIDEO_OUT_30BIT:
    case STM_VIDEO_OUT_36BIT:
    case STM_VIDEO_OUT_48BIT:
      if((m_ulCapabilities & OUTPUT_CAPS_HDMI_DEEPCOLOR) == 0)
      {
        TRC( TRC_ID_ERROR, "> 24bit color not supported on this HW" );
        return false;
      }
      if(device_format != STM_VIDEO_OUT_HDMI)
      {
        TRC( TRC_ID_ERROR, "> 24bit color only supported in HDMI mode" );
        return false;
      }
      if(colour_format == STM_VIDEO_OUT_422)
      {
        TRC( TRC_ID_ERROR, "> 24bit color not possible in 422 mode" );
        return false;
      }
    default:
      break;
  }

  switch (device_format)
  {
    case STM_VIDEO_OUT_DVI:
    case STM_VIDEO_OUT_HDMI:
    {
      if((m_ulOutputFormat & (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_HDMI)) != device_format)
      {
        TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetOutputFormat - Switching HDMI/DVI Mode" );
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
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetOutputFormat - Unknown output mode" );
      return false;
  }

  if((m_ulOutputFormat & STM_VIDEO_OUT_DEPTH_MASK) != colour_depth)
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

  vibe_os_lock_resource(m_statusLock);

  if(m_bIsStarted)
  {
    TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetOutputFormat - Colour format changed, updating AVI frame" );
    m_pIFrameManager->ForceAVIUpdate();

    uint32_t cfg = ReadHDMIReg(STM_HDMI_CFG);

    if(colour_format & STM_VIDEO_OUT_422)
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetOutputFormat - YUV 4:2:2" );
      cfg |= STM_HDMI_CFG_422_EN;

     /*
      * This is a work around only applicable for pixel repeated video modes :VIC =6,7,10 and 11
      * In case of HDMI  YCbCr 422 output , Cb and Cr chroma sampling are swapped on V2.9 SoCs.
      */

      if (((m_HWFeatures & STM_HDMI_DEEP_COLOR)==STM_HDMI_DEEP_COLOR) &&
      ((m_CurrentOutputMode.mode_params.hdmi_vic_codes[0]==6) || (m_CurrentOutputMode.mode_params.hdmi_vic_codes[1]==7)||
      (m_CurrentOutputMode.mode_params.hdmi_vic_codes[0]==10) || (m_CurrentOutputMode.mode_params.hdmi_vic_codes[1]==11)))
      {
        cfg |= STM_HDMI_V29_422_DROP_CB_N_CR;
      }
      else
      {
        cfg &= ~STM_HDMI_V29_422_DROP_CB_N_CR;
      }
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "CSTmHDMI::SetOutputFormat - YUV 4:4:4 / RGB" );
      cfg &= ~STM_HDMI_CFG_422_EN;
      cfg &= ~STM_HDMI_V29_422_DROP_CB_N_CR;
    }

    WriteHDMIReg(STM_HDMI_CFG,cfg);
  }

  /*
   * Deliberately update m_ulOutputFormat under the lock so its update is
   * atomic with InfoFrame processing.
   */
  m_ulOutputFormat = device_format | colour_format | colour_depth;

  vibe_os_unlock_resource(m_statusLock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


void CSTmHDMI::SetSignalRangeClipping(void)
{
  return; // Default NULL implementation, to be overriden by SoCs
}


bool CSTmHDMI::SetAudioSource(stm_display_output_audio_source_t source)
{
  return false;
}


void CSTmHDMI::SendGeneralControlPacket(void)
{
  uint32_t genpktctl = (m_ulDeepcolourGCP | STM_HDMI_GENCTRL_PKT_EN);

  if(m_bAVMute)
  {
    TRC( TRC_ID_UNCLASSIFIED, "CSTmHDMI::SendGeneralControlPacket - Enabling AVMute" );
    genpktctl |= STM_HDMI_GENCTRL_PKT_AVMUTE;
  }
  else
  {
    TRC( TRC_ID_UNCLASSIFIED, "CSTmHDMI::SendGeneralControlPacket - Disable AVMute" );
  }
  vibe_os_call_bios (m_bAVMute ? 33 : 34, 2, 1, STM_HDMI_GENCTRL_PKT_AVMUTE);

  WriteHDMIReg(STM_HDMI_GEN_CTRL_PKT_CFG, genpktctl);
}


static const uint32_t iframe_frequencies[] = {0,32000,44100,48000,88200,96000,176400,192000};

/*
 * Helper for SetAudioCTS to do rounded integer division.
 */
static inline uint32_t rounded_div(uint32_t numerator, uint32_t denominator)
{
  return (numerator + (denominator / 2)) / denominator;
}


void CSTmHDMI::SetAudioCTS(const stm_hdmi_audio_state_t &state)
{
  uint32_t div;
  uint32_t Fs;
  uint32_t QuantizedFs;

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
    Fs = ::rounded_div(state.fsynth_frequency, (128*state.clock_cts_divide));
    if(UNLIKELY((m_ulAudioOutputType == STM_HDMI_AUDIO_TYPE_HBR) &&
                (m_AudioSource == STM_AUDIO_SOURCE_SPDIF)))
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

  uint32_t n = ::rounded_div((QuantizedFs*128), div);

  if(n != m_audioState.N)
  {
    WriteHDMIReg(STM_HDMI_AUDN, (unsigned long)n);
    m_audioState.N = n;
    TRC( TRC_ID_UNCLASSIFIED, "CSTmHDMI::SetAudioCTS Pixclock = %u Freq = %u new N = %u", GetCurrentDisplayMode()->mode_timing.pixel_clock_freq,Fs,n );
    if(m_bUseFixedNandCTS)
    {
      /*
       * This is a quick hack for fixed CTS, it will not match the recommended
       * values for N and CTS for /1.001 based video pixel clocks in the spec.
       * Note that the calculation must use the TMDS clock not the pixel clock
       * which takes into account deepcolor.
       */
      uint32_t cts = vibe_os_div64((uint64_t)m_uCurrentTMDSClockFreq*n,(128*QuantizedFs));
      WriteHDMIReg(STM_HDMI_AUD_CTS, cts);
      TRC( TRC_ID_UNCLASSIFIED, "CSTmHDMI::SetAudioCTS TMDS clock = %u CTS = %u", m_uCurrentTMDSClockFreq, cts );
    }
  }

  /*
   * Update the clock divides in case the oversampling clock as changed
   */
  uint32_t tmp = ReadHDMIReg(STM_HDMI_AUDIO_CFG);
  tmp &= ~(STM_HDMI_AUD_CFG_SPDIF_CLK_MASK |
           STM_HDMI_AUD_CFG_CTS_CLK_DIV_MASK);

  tmp |= (((state.clock_divide-1)<<STM_HDMI_AUD_CFG_SPDIF_CLK_SHIFT)   |
          ((state.clock_cts_divide-1)<<STM_HDMI_AUD_CFG_CTS_CLK_DIV_SHIFT));

  if(m_bUseFixedNandCTS)
    tmp |= STM_HDMI_AUD_CFG_CTS_CAL_BYPASS;

  WriteHDMIReg(STM_HDMI_AUDIO_CFG, tmp);

  m_audioState.clock_divide = state.clock_divide;
  m_audioState.clock_cts_divide = state.clock_cts_divide;
  m_audioState.fsynth_frequency = state.fsynth_frequency;
}


void CSTmHDMI::InvalidateAudioPackets(void)
{
  uint32_t tmp = ReadHDMIReg(STM_HDMI_AUDIO_CFG);
  tmp |= ( STM_HDMI_AUD_CFG_DST_SAMPLE_INVALID |
           STM_HDMI_AUD_CFG_ONEBIT_ALL_INVALID |
           STM_HDMI_AUD_CFG_DISABLE_AUDIO_TRANSMISSION);
  WriteHDMIReg(STM_HDMI_AUDIO_CFG, tmp);

  WriteHDMIReg(STM_HDMI_SAMPLE_FLAT_MASK, STM_HDMI_SAMPLE_FLAT_ALL);
}


stm_display_metadata_result_t CSTmHDMI::QueueMetadata(stm_display_metadata_t *m)
{
  return m_pIFrameManager->QueueMetadata(m);
}


void CSTmHDMI::FlushMetadata(stm_display_metadata_type_t type)
{
  m_pIFrameManager->FlushMetadata(type);
}


void CSTmHDMI::SetClockReference(stm_clock_ref_frequency_t ref, int error_ppm)
{
  m_audioClockReference = ref;
}


void CSTmHDMI::Suspend(void)
{
  if(!m_bIsSuspended)
  {
    /*
     * Calling Stop() for HDMI output will immediately power down the
     * serializer and the PHY which is what we want to happen and put
     * the low level state machine back into a "needs restart" state;
     * which the upper level should notice when it resumes.
     *
     * We have to keep m_CurrentOutputMode unchanged as it will be used
     * while resuming HDMI output.
     */
    this->Stop();
    m_bIsSuspended = true;
  }
}


void CSTmHDMI::Resume(void)
{
  if(m_bIsSuspended)
  {
    /*
     * This should not call Start(), we just re-instate the default hardware
     * configuration as if we had be powered on (in case the hardware was in
     * reset or powered down during the suspend) then reset the output format
     * and audio selection which may cause subclasses to reconfigure hardware
     * glue registers that may have been lost.
     *
     * The upper level HDMI management we restart HDMI after checking hotplug
     * and any connected display's EDID.
     */

    PowerOnSetup();

    SetOutputFormat(m_ulOutputFormat);
    SetSignalRangeClipping();
    /*
     * To force the reset of the audio source selection and SoC glue registers
     * we need to make SetAudioSource() think there was previously no audio
     * selected, otherwise it will not do anything. The implementations
     * optimize the code path if the audio source is not changing to avoid
     * audio artifacts of a running system. So we have to defeat that logic.
     */

    stm_display_output_audio_source_t current_audio_source = m_AudioSource;
    m_AudioSource = STM_AUDIO_SOURCE_NONE;
    SetAudioSource(current_audio_source);

    m_bIsSuspended = false;
  }
}
