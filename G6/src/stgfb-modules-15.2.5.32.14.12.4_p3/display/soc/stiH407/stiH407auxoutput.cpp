/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407auxoutput.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/DisplayPlane.h>

#include <display/ip/displaytiming/stmclocklla.h>
#include <display/ip/displaytiming/stmfsynth.h>
#include <display/ip/displaytiming/stmvtg.h>

#include <display/ip/tvout/stmtvoutdenc.h>
#include <display/ip/tvout/stmvip.h>
#include <display/ip/hdf/stmhdf.h>

#include <display/ip/stmviewport.h>
#include <display/ip/misr/stmmisrauxtvout.h>

#include "stiH407reg.h"
#include "stiH407device.h"
#include "stiH407auxoutput.h"
#include "stiH407mixer.h"

#define VTG_HDF_SYNC_ID  m_sync_sel_map[TVO_VIP_SYNC_HDF_IDX]  /* 3 */
#define VTG_DENC_SYNC_ID m_sync_sel_map[TVO_VIP_SYNC_DENC_IDX] /* 1 */

CSTiH407AuxOutput::CSTiH407AuxOutput(
  CDisplayDevice               *pDev,
  CSTmVTG                      *pVTG,
  CSTmTVOutDENC                *pDENC,
  CGammaMixer                  *pMixer,
  CSTmFSynth                   *pFSynth,
  CSTmHDFormatter              *pHDFormatter,
  CSTmClockLLA                 *pClkDivider,
  CSTmVIP                      *pHDFVIP,
  CSTmVIP                      *pDENCVIP,
  const stm_display_sync_id_t  *syncMap): CSTmAuxTVOutput( "analog_sdout0",
                                                         STiH407_OUTPUT_IDX_AUX,
                                                         pDev,
                                                         pVTG,
                                                         pDENC,
                                                         pMixer,
                                                         pHDFormatter)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pFSynth        = pFSynth;
  m_pClkDivider    = pClkDivider;
  m_pHDFVIP        = pHDFVIP;
  m_pDENCVIP       = pDENCVIP;

  m_sync_sel_map   = syncMap;

  /* validation team recommends to add -1 shift for SD modes on H407 SoC, refer to Bug 46963 */
  m_uDENCSyncOffset      = DENC_DELAY - 1;
  m_uExternalSyncShift   = 0;
  m_bInvertExternalSyncs = false;

  m_VideoSource = STM_VIDEO_SOURCE_AUX_COMPOSITOR;

  /* Set default Output format : No Y/C on this platform */
  m_ulOutputFormat = STM_VIDEO_OUT_CVBS;

  m_maxDACVoltage  = 1400; // in mV
  m_DACSaturation  = 1023;
  RecalculateDACSetup();

  if((m_pMisrAux = new CSTmMisrAuxTVOut(pDev, STiH407_TVO_AUX_PF_CTRL, STiH407_TVO_SD_OUT_CTRL)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create m_pMisrAux" );
  }

  DisableDACs();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTiH407AuxOutput::~CSTiH407AuxOutput()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  delete m_pMisrAux;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH407AuxOutput::ShowPlane(const CDisplayPlane *plane)
{
  TRC( TRC_ID_MAIN_INFO, "plane \"%s\" ID:%u", plane->GetName(), plane->GetID() );

  stm_clk_divider_output_divide_t div;
  ASSERTF(m_pClkDivider->getDivide(STM_CLK_PIX_AUX,&div),("Failed to get pixel clock divider\n"));

  if(!CSTmAuxTVOutput::ShowPlane(plane))
    return false;

  switch(plane->GetID())
  {
    case STiH407_PLANE_IDX_GDP1:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP1 clock set to SD disp clock" );
      m_pClkDivider->Enable(STM_CLK_PIX_GDP1, STM_CLK_SRC_SD, div);
      break;
    }
    case STiH407_PLANE_IDX_GDP2:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP2 clock set to SD disp clock" );
      m_pClkDivider->Enable(STM_CLK_PIX_GDP2, STM_CLK_SRC_SD, div);
      break;
    }
    /*
     *  Note: The current mixer setup doesn't enable GDP3 on the aux mixer,
     *        but we can put the code in anyway in case someone changes their
     *        mind on which GDP is linked to the Main VBI plane for ED/HD CMGS-A
     */
    case STiH407_PLANE_IDX_GDP3:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP3 clock set to SD disp clock" );
      m_pClkDivider->Enable(STM_CLK_PIX_GDP3, STM_CLK_SRC_SD, div);
      break;
    }
    case STiH407_PLANE_IDX_GDP4:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP4 clock set to SD disp clock" );
      m_pClkDivider->Enable(STM_CLK_PIX_GDP4, STM_CLK_SRC_SD, div);
      break;
    }
    case STiH407_PLANE_IDX_VID_PIP:
    {
      TRC( TRC_ID_MAIN_INFO, "VID1 clock set to SD disp clock" );
      m_pClkDivider->Enable(STM_CLK_PIX_PIP, STM_CLK_SRC_SD, div);
      break;
    }
    default:
      break;
  }

  return true;
}

void CSTiH407AuxOutput::SetMisrData(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt)
{
  const stm_display_mode_t   *pCurrentMode;
  TRC( TRC_ID_UNCLASSIFIED, "Configuring AUX MISR capture" );
  if(m_pMisrAux == NULL)
  {
    TRC( TRC_ID_ERROR, "m_pMisrAux is NULL" );
    return;
  }

  pCurrentMode=GetCurrentDisplayMode();
  if(!pCurrentMode)
  {
    TRC( TRC_ID_UNCLASSIFIED, "MISR Capture start requested, but Display is not active, pCurrentMode NULL" );
    return;
  }
  m_pMisrAux->ReadMisrSigns(LastVTGEvtTime, LastVTGEvt);
}

void CSTiH407AuxOutput::UpdateMisrCtrl(void)
{
  const stm_display_mode_t *pCurrentMode;

  if(m_pMisrAux == NULL)
  {
    TRC( TRC_ID_ERROR, "m_pMisrAux is NULL" );
    return;
  }

  pCurrentMode  = GetCurrentDisplayMode();
  if (pCurrentMode)
  {
    m_pMisrAux->UpdateMisrControlValue(pCurrentMode);
  }
}


TuningResults CSTiH407AuxOutput::SetTuning( uint16_t service,
                                            void    *inputList,
                                            uint32_t inputListSize,
                                            void    *outputList,
                                            uint32_t outputListSize)
{
    TuningResults res = TUNING_INVALID_PARAMETER;
    tuning_service_type_t ServiceType = (tuning_service_type_t)service;

    TRC( TRC_ID_UNCLASSIFIED, "ServiceType %x", ServiceType );
    if(m_pMisrAux == NULL)
    {
      TRC( TRC_ID_ERROR, "m_pMisrAux is NULL" );
      return TUNING_SERVICE_NOT_SUPPORTED;
    }

    switch(ServiceType)
    {
      case MISR_CAPABILITY:
      {
        res = m_pMisrAux->GetMisrCapability(outputList, outputListSize);
        break;
      }
      case MISR_SET_CONTROL:
      {
        /*Get Current Mode for Scan Type and Vport Params*/
        const stm_display_mode_t *pCurrentMode;
        pCurrentMode  = GetCurrentDisplayMode();
        if(pCurrentMode)
        {
          res = m_pMisrAux->SetMisrControlValue(service, inputList, pCurrentMode);
        }
        else
        {
          res = TUNING_SERVICE_NOT_SUPPORTED;
        }
        break;
      }
      case MISR_COLLECT:
      {
        res = m_pMisrAux->CollectMisrValues(outputList);
        break;
      }
      case MISR_STOP:
      {
        m_pMisrAux->ResetMisrState();
        res = TUNING_OK;
        break;
      }
      default:
        break;
    }
    return res;
}



///////////////////////////////////////////////////////////////////////////////
// Aux output configuration
//

bool CSTiH407AuxOutput::ConfigureOutput(const uint32_t format)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /*
     * Input to TVOut is signed RGB
     */
    WriteReg(STiH407_TVO_AUX_IN_VID_FORMAT,STiH407_TVO_TVO_IN_FMT_SIGNED | STiH407_TVO_SYNC_EXT);

    /*
     * The HDF VIP feeds 10bit DACs, so the color depth should be 30bit.
     *
     * Note: we do not enable clipping in the HDF VIP if it is in use.
     *       This is because the clipping is controlled as part of the DENC's
     *       signal formatting. Control of the HDF VIP clipping would be needed
     *       if we supported direct processing of the Aux video path, by the
     *       HDFormatter, without going through the DENC.
     */
    uint32_t hdfformat = (format & ~STM_VIDEO_OUT_DEPTH_MASK) | STM_VIDEO_OUT_30BIT;

    if(format & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
    {
      /*
       * TODO: Check this channel ordering is correct for H407.
       */
      if (m_bUsingDENC)
      {
        m_pHDFVIP->SelectSync(TVO_VIP_AUX_SYNCS);
        m_pHDFVIP->SetColorChannelOrder(TVO_VIP_CB_B, TVO_VIP_Y_G, TVO_VIP_CR_R);
        m_pHDFVIP->SetInputParams(TVO_VIP_DENC123, hdfformat, STM_SIGNAL_FULL_RANGE);
      }
      else
      {
        m_pHDFVIP->SetColorChannelOrder(TVO_VIP_CR_R, TVO_VIP_Y_G, TVO_VIP_CB_B);
        m_pHDFVIP->SetInputParams(TVO_VIP_AUX_VIDEO, hdfformat, STM_SIGNAL_FULL_RANGE);
      }
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;
}


void CSTiH407AuxOutput::ConfigureDisplayClocks(const stm_display_mode_t *mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t tvStandard = mode->mode_params.output_standards;

  if(tvStandard & STM_OUTPUT_STD_NTG5)
  {
    m_pFSynth->SetDivider(2);
    m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_2);
  }
  else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
    m_pFSynth->SetDivider(4);
    m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_4);
  }
  else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    m_pFSynth->SetDivider(8);
    m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_8);

    if((mode->mode_id == STM_TIMING_MODE_576I50000_13500)||(mode->mode_id == STM_TIMING_MODE_576I50000_14750))
    {
      /* specific adjustment for PAL mode on Aux output*/
      if(!m_pFSynth->SetAdjustment(-2))
        TRC( TRC_ID_ERROR, "Cannot set adjustment for the PAL mode on Aux output!!" );
      else
        TRC( TRC_ID_MAIN_INFO, "Set fsynth adjustment to -2");
    }

    /*
     * Only switch the DENC and SDDACs output clock if we are using the DENC.
     *
     * We should not need to touch CLK_PIX_AUX at all for the Main->DENC case.
     */
    if(m_bUsingDENC)
    {
      m_pClkDivider->Enable(STM_CLK_DENC      , STM_CLK_SRC_SD, STM_CLK_DIV_4);
      m_pClkDivider->Enable(STM_CLK_OUT_SDDACS, STM_CLK_SRC_SD, STM_CLK_DIV_1);
    }
  }
  else
  {
    if(mode->mode_timing.pixel_clock_freq > 148500000)
    {
      /*
       * 1080p 100/120Hz, 4Kx2K
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 1");
      m_pFSynth->SetDivider(1);
      m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_1);
    }
    else if(mode->mode_timing.pixel_clock_freq > 74250000)
    {
      /*
       * 1080p 50/60Hz or high refresh XGA
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 2");
      m_pFSynth->SetDivider(2);
      m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_2);
    }
    else
    {
      /*
       * 1080p 25/30Hz, 1080i, 720p
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 4");
      m_pFSynth->SetDivider(4);
      m_pClkDivider->Enable(STM_CLK_PIX_AUX, STM_CLK_SRC_SD, STM_CLK_DIV_4);
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407AuxOutput::SetAuxClockToHDFormatter(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  /*
   * Switch the HD DAC clock to the SD clock with no divide
   */
  stm_clk_divider_output_divide_t div;
  uint32_t tvStandard = m_CurrentOutputMode.mode_params.output_standards;

  if(m_bUsingDENC)
  {
    /*
     * When we are routing DENC output to the HD Formatter the HD pixel clock
     * needs to be the same as the DENC clock (27MHz) not the SD pixel clock
     * (13.5MHz).
     */
    ASSERTF(m_pClkDivider->getDivide(STM_CLK_DENC,&div),("Failed to get DENC clock divider\n"));
    TRC( TRC_ID_MAIN_INFO, "Setting DENC clock divider (0x%x) as HD Formatter pixel clock divide", div );
  }
  else
  {
    ASSERTF(m_pClkDivider->getDivide(STM_CLK_PIX_AUX,&div),("Failed to get pixel clock divider\n"));
    TRC( TRC_ID_MAIN_INFO, "Setting Aux pix clock divider (0x%x) as HD Formatter pixel clock divide", div );
  }

  m_pClkDivider->Enable(STM_CLK_PIX_HDDACS, STM_CLK_SRC_SD, div);

  if(tvStandard & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
    m_pClkDivider->Enable(STM_CLK_OUT_HDDACS, STM_CLK_SRC_SD, STM_CLK_DIV_2);
  else
    m_pClkDivider->Enable(STM_CLK_OUT_HDDACS, STM_CLK_SRC_SD, STM_CLK_DIV_1);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407AuxOutput::DisableClocks(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_pHDFormatter->IsStarted())
  {
    TRC( TRC_ID_MAIN_INFO, "Disabling HD Clocks" );
    m_pClkDivider->Disable(STM_CLK_OUT_HDDACS);
    m_pClkDivider->Disable(STM_CLK_PIX_HDDACS);
  }

  if(!m_pDENC->IsStarted())
  {
    TRC( TRC_ID_MAIN_INFO, "Disabling SD Clocks" );
    m_pClkDivider->Disable(STM_CLK_OUT_SDDACS);
    m_pClkDivider->Disable(STM_CLK_DENC);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH407AuxOutput::SetVTGSyncs(const stm_display_mode_t *mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );


  if(!m_sync_sel_map)
  {
    TRCOUT( TRC_ID_ERROR, "Undefined Syncs mapping !!");
    return false;
  }

  uint32_t tvStandard = mode->mode_params.output_standards;

  if((tvStandard == STM_OUTPUT_STD_NTG5) || (tvStandard == STM_OUTPUT_STD_HDMI_LLC_EXT) || (tvStandard == STM_OUTPUT_STD_CEA861))
  {
    /*
     * Digital only formats, just set the AWG syncs to a default
     */
    m_pVTG->SetSyncType   (VTG_HDF_SYNC_ID,  STVTG_SYNC_POSITIVE);
    m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
    m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, 0);
    m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, 0);
  }
  else if(tvStandard & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
  {
    m_pVTG->SetSyncType   (VTG_HDF_SYNC_ID,  STVTG_SYNC_POSITIVE);
    if(mode->mode_params.scan_type == STM_INTERLACED_SCAN)
      m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_INVERTED);
    else
      m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);

    m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, AWG_DELAY_HD);
    m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, AWG_DELAY_HD);
  }
  else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    int denc_sync_shift = m_uDENCSyncOffset;
    if(mode->mode_timing.lines_per_frame == 525)
    {
      /*
       * Account for the difference in the size of the front porch between the
       * analog and digital 525line mode definitions. The mode line we specify
       * is the digital one. This was expected to be 3 pixels but that produces
       * a clear de-centering of the image. This could be down to the rise time
       * to the center of the analog sync pulse or more likely the fact we do not
       * have a clear explanation for the DENC offset value in the first place.
       */
      denc_sync_shift -= 2;
    }

    m_pVTG->SetSyncType(VTG_DENC_SYNC_ID, STVTG_SYNC_TOP_NOT_BOT);
    m_pVTG->SetBnottopType(VTG_DENC_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
    m_pVTG->SetHSyncOffset(VTG_DENC_SYNC_ID, denc_sync_shift);
    m_pVTG->SetVSyncHOffset(VTG_DENC_SYNC_ID, denc_sync_shift);

    if(m_bUsingDENC)
    {
      /*
       * Set the DENC VIP sync, clocks and data routing for the Aux
       * path, it may have previously been switched to the main path, before we
       * actually start the DENC again.
       *
       * The DENC takes a fixed input format from its VIP, which is 24bit YUV.
       *
       * Note: we do not enable clipping in the DENC VIP because the clipping is
       *       controlled as part of the DENC's signal formatting.
       */
       m_pDENCVIP->SetInputParams(TVO_VIP_AUX_VIDEO,
                                  (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_24BIT),
                                  STM_SIGNAL_FULL_RANGE);
    }

    if(m_pHDFormatter->GetOwner() == this)
    {
      m_pVTG->SetSyncType(VTG_HDF_SYNC_ID, STVTG_SYNC_TOP_NOT_BOT);
      m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
      m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, AWG_DELAY_SD);
      m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, AWG_DELAY_SD);
    }
  }
  else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
    m_pVTG->SetSyncType   (VTG_HDF_SYNC_ID,  STVTG_SYNC_POSITIVE);
    m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, AWG_DELAY_ED);
    m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, AWG_DELAY_ED);
    m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
  }
  else
  {
    TRCOUT( TRC_ID_ERROR, "Unsupported Output Standard");
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTiH407AuxOutput::EnableDACs(void)
{
  uint32_t val;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Do nothing when DACs are powered down
   */
  if(m_bDacPowerDown)
  {
    TRCOUT( TRC_ID_MAIN_INFO, "Video DACs are powered down !!" );
    return;
  }

  val = ReadSysCfgReg(SYS_CFG5072);

  if (m_pHDFormatter->GetOwner() == this)
  {
    /*
     * We can blindly enable the HD DACs if we need them, it doesn't
     * matter if they were already in use by the main pipeline.
     */
    TRC( TRC_ID_MAIN_INFO, "Enabling HD DACs" );
    val &= ~SYS_CFG5072_DAC_HZUVW;

    TRC( TRC_ID_MAIN_INFO, "Powering Up HD DACs" );
    WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, ~TVO_DAC_POFF_DACS);
  }
  else if (m_pHDFormatter->GetOwner() == 0)
  {
    /*
     * Only tri-state the HD DAC setup if the main pipeline is not
     * using the HD formatter.
     */
    TRC( TRC_ID_MAIN_INFO, "Tri-Stating HD DACs" );
    val |=  SYS_CFG5072_DAC_HZUVW;
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
  {
    TRC( TRC_ID_MAIN_INFO, "Enabling CVBS DAC" );
    val &= ~SYS_CFG5072_DAC_HZX;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Tri-Stating CVBS DAC" );
    val |= SYS_CFG5072_DAC_HZX;
  }

  /*
   * Note: No S-Video DACs
   */
  WriteSysCfgReg(SYS_CFG5072,val);

  TRC( TRC_ID_MAIN_INFO, "Powering Up SD DACs" );
  WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, ~TVO_DAC_POFF_DACS);
  WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_DENC_OUT_SD_SHIFT, TVO_DAC_DENC_OUT_SD_WIDTH, TVO_DAC_DENC_OUT_SD_DAC456);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407AuxOutput::DisableDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Power down HD DACs only if this pipeline using them for SCART output
   */
  stm_clk_divider_output_source_t clksrc;

  if ((m_pHDFormatter->GetOwner() == this)||(m_pHDFormatter->GetOwner() == 0))
  {
    TRC( TRC_ID_MAIN_INFO, "Powering Down HD DACs" );
    WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);
  }

  /*
   * Power down SD DACs only if the HD pipeline is not currently using them.
   */
  m_pClkDivider->getSource(STM_CLK_OUT_SDDACS,&clksrc);

  if(clksrc == STM_CLK_SRC_SD)
  {
    TRC( TRC_ID_MAIN_INFO, "Powering Down SD DACs" );
    WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407AuxOutput::PowerDownDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);

  COutput::PowerDownDACs();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
