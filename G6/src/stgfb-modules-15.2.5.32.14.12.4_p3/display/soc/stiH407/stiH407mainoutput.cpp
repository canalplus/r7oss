/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407mainoutput.cpp
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
#include <display/ip/misr/stmmisrmaintvout.h>



#include "stiH407reg.h"
#include "stiH407device.h"
#include "stiH407mainoutput.h"
#include "stiH407mixer.h"

#define VTG_DENC_SYNC_ID  m_sync_sel_map[TVO_VIP_SYNC_DENC_IDX]
#define VTG_HDF_SYNC_ID   m_sync_sel_map[TVO_VIP_SYNC_HDF_IDX]
#define VTG_PADS_SYNC_ID  m_sync_sel_map[TVO_VIP_SYNC_TYPE_SDDCS]

//////////////////////////////////////////////////////////////////////////////
//
// HD, SD Progressive and SD Interlaced Output on main HD Dacs
//
CSTiH407MainOutput::CSTiH407MainOutput(
  CDisplayDevice               *pDev,
  CSTmVTG                      *pVTG,
  CSTmTVOutDENC                *pDENC,
  CGammaMixer                  *pMixer,
  CSTmFSynth                   *pFSynth,
  CSTmHDFormatter              *pHDFormatter,
  CSTmClockLLA                 *pClkDivider,
  CSTmVIP                      *pHDFVIP,
  CSTmVIP                      *pDENCVIP,
  const stm_display_sync_id_t  *syncMap): CSTmMainTVOutput("analog_hdout0",
                                                           STiH407_OUTPUT_IDX_MAIN,
                                                           pDev,
                                                           pVTG,
                                                           pDENC,
                                                           pMixer,
                                                           pHDFormatter)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pFSynth         = pFSynth;
  m_pClkDivider     = pClkDivider;
  m_pHDFVIP         = pHDFVIP;
  m_pDENCVIP        = pDENCVIP;

  m_uDENCSyncOffset = DENC_DELAY;

  m_sync_sel_map    = syncMap;

  if((m_pMisrMain = new CSTmMisrMainTVOut(pDev, STiH407_TVO_MAIN_PF_CTRL, STiH407_TVO_HD_OUT_CTRL))==0)
  {
    TRC( TRC_ID_ERROR, "failed to create m_pMisrMain" );
  }

  DisableDACs();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTiH407MainOutput::~CSTiH407MainOutput()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  delete m_pMisrMain;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

///////////////////////////////////////////////////////////////////////////////
// Main MISR Data capture implementation
//
void CSTiH407MainOutput::SetMisrData(const stm_time64_t LastVTGEvtTime, uint32_t  LastVTGEvt)
{
  const stm_display_mode_t   *pCurrentMode;
  TRC( TRC_ID_UNCLASSIFIED, "Configuring MAIN MISR capture" );
  if(m_pMisrMain == NULL)
  {
    TRC( TRC_ID_ERROR, "m_pMisrMain is NULL" );
    return;
  }
  pCurrentMode=GetCurrentDisplayMode();
  if(!pCurrentMode)
  {
    TRC( TRC_ID_UNCLASSIFIED, "MISR Capture start requested, but Display is not active, pCurrentMode NULL" );
    return;
  }

  m_pMisrMain->ReadMisrSigns(LastVTGEvtTime, LastVTGEvt);
}

void CSTiH407MainOutput::UpdateMisrCtrl(void)
{
  const stm_display_mode_t *pCurrentMode;

  if(m_pMisrMain == NULL)
  {
    TRC( TRC_ID_ERROR, "m_pMisrMain is NULL" );
    return;
  }

  pCurrentMode  = GetCurrentDisplayMode();
  if (pCurrentMode)
  {
    m_pMisrMain->UpdateMisrControlValue(pCurrentMode);
  }
}

TuningResults CSTiH407MainOutput::SetTuning( uint16_t service,
                                             void    *inputList,
                                             uint32_t inputListSize,
                                             void    *outputList,
                                             uint32_t outputListSize)
{
    TuningResults res = TUNING_INVALID_PARAMETER;
    tuning_service_type_t ServiceType = (tuning_service_type_t)service;

    TRC( TRC_ID_UNCLASSIFIED, "ServiceType %x", ServiceType );

    if(m_pMisrMain == NULL)
    {
      TRC( TRC_ID_ERROR, "m_pMisrMain is NULL" );
      return TUNING_SERVICE_NOT_SUPPORTED;
    }

    switch(ServiceType)
    {
      case MISR_CAPABILITY:
      {
        res = m_pMisrMain->GetMisrCapability(outputList, outputListSize);
        break;
      }
      case MISR_SET_CONTROL:
      {
        /*Get Current Mode for Scan Type and Vport Params*/
        const stm_display_mode_t *pCurrentMode;
        pCurrentMode  = GetCurrentDisplayMode();
        if(pCurrentMode)
        {
          res = m_pMisrMain->SetMisrControlValue(service, inputList, pCurrentMode);
        }
        else
        {
          res = TUNING_SERVICE_NOT_SUPPORTED;
        }
        break;
      }
      case MISR_COLLECT:
      {
        res = m_pMisrMain->CollectMisrValues(outputList);
        break;
      }
      case MISR_STOP:
      {
        m_pMisrMain->ResetMisrState();
        res = TUNING_OK;
        break;
      }
      default:
        break;
    }
    return res;
}

void CSTiH407MainOutput::HidePlane(const CDisplayPlane *plane)
{
  TRC( TRC_ID_MAIN_INFO, "CSTiH407MainOutput::HidePlane \"%s\" ID:%u", plane->GetName(),plane->GetID() );

  switch(plane->GetID())
  {
    case STiH407_PLANE_IDX_GDP1:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP1 clock set to SD disp clock" );
      m_pClkDivider->SetParent(STM_CLK_PIX_GDP1, STM_CLK_SRC_SD);
      break;
    }
    case STiH407_PLANE_IDX_GDP2:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP2 clock set to SD disp clock" );
      m_pClkDivider->SetParent(STM_CLK_PIX_GDP2, STM_CLK_SRC_SD);
      break;
    }
    case STiH407_PLANE_IDX_GDP3:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP3 clock set to SD disp clock" );
      m_pClkDivider->SetParent(STM_CLK_PIX_GDP3, STM_CLK_SRC_SD);
      break;
    }
    case STiH407_PLANE_IDX_GDP4:
    {
      TRC( TRC_ID_MAIN_INFO, "GDP4 clock set to SD disp clock" );
      m_pClkDivider->SetParent(STM_CLK_PIX_GDP4, STM_CLK_SRC_SD);
      break;
    }
    default:
      break;
  }

  CSTmMainTVOutput::HidePlane(plane);

}

bool CSTiH407MainOutput::ShowPlane(const CDisplayPlane *plane)
{
  TRC( TRC_ID_MAIN_INFO, "CSTiH407MainOutput::ShowPlane \"%s\" ID:%u", plane->GetName(),plane->GetID() );

  stm_clk_divider_output_divide_t div;

  if(!m_pClkDivider->getDivide(STM_CLK_PIX_MAIN,&div))
  {
    TRC( TRC_ID_ERROR, "Cannot get main display clock divider" );
    return false;
  }

  if(!CSTmMainTVOutput::ShowPlane(plane))
    return false;

  switch(plane->GetID())
  {
    case STiH407_PLANE_IDX_GDP1:
    {
      m_pClkDivider->Enable(STM_CLK_PIX_GDP1, STM_CLK_SRC_HD, div);
      TRC( TRC_ID_MAIN_INFO, "GDP1 clock set to HD disp clock" );
      break;
    }
    case STiH407_PLANE_IDX_GDP2:
    {
      m_pClkDivider->Enable(STM_CLK_PIX_GDP2, STM_CLK_SRC_HD, div);
      TRC( TRC_ID_MAIN_INFO, "GDP2 clock set to HD disp clock" );
      break;
    }
    case STiH407_PLANE_IDX_GDP3:
    {
      m_pClkDivider->Enable(STM_CLK_PIX_GDP3, STM_CLK_SRC_HD, div);
      TRC( TRC_ID_MAIN_INFO, "GDP3 clock set to HD disp clock" );
      break;
    }
    case STiH407_PLANE_IDX_GDP4:
    {
      m_pClkDivider->Enable(STM_CLK_PIX_GDP4, STM_CLK_SRC_HD, div);
      TRC( TRC_ID_MAIN_INFO, "GDP4 clock set to HD disp clock" );
      break;
    }
    case STiH407_PLANE_IDX_VID_PIP:
    {
      m_pClkDivider->Enable(STM_CLK_PIX_PIP, STM_CLK_SRC_HD, div);
      TRC( TRC_ID_MAIN_INFO, "PIP clock set to HD disp clock" );
      break;
    }
    default:
      break;
  }

  return true;
}


///////////////////////////////////////////////////////////////////////////////
// Main output configuration
//

bool CSTiH407MainOutput::ConfigureOutput(const uint32_t format)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Input to TVOut is signed RGB
   */
  WriteReg(STiH407_TVO_MAIN_IN_VID_FORMAT,STiH407_TVO_TVO_IN_FMT_SIGNED | STiH407_TVO_SYNC_EXT);

  if(format & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * The HDF VIP feeds 10bit DACs, so the color depth should be 30bit. The
     * DENC takes a fixed input format from its VIP, which is 24bit YUV.
     */
    uint32_t hdfformat = (format & ~STM_VIDEO_OUT_DEPTH_MASK) | STM_VIDEO_OUT_30BIT;

    /*
     * NOTE: This assumes that when using the DENC, we always want to use
     *       the DENC's formatted output on the HDDACs. If dual SCART is required
     *       in the future then we would have to add some additional setup here.
     */
    if(m_bUsingDENC)
    {
      TRC( TRC_ID_MAIN_INFO, "Configuring DENC Output" );
      m_pHDFVIP->SelectSync(TVO_VIP_MAIN_SYNCS);
      m_pHDFVIP->SetColorChannelOrder(TVO_VIP_CB_B, TVO_VIP_Y_G, TVO_VIP_CR_R);
      m_pHDFVIP->SetInputParams(TVO_VIP_DENC123, hdfformat, STM_SIGNAL_FULL_RANGE);
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "Configuring HD Output" );
      m_pHDFVIP->SetColorChannelOrder(TVO_VIP_CR_R, TVO_VIP_Y_G, TVO_VIP_CB_B);
      m_pHDFVIP->SetInputParams(TVO_VIP_MAIN_VIDEO, hdfformat, m_signalRange);
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


void CSTiH407MainOutput::ConfigureDisplayClocks(const stm_display_mode_t *mode)
{
  uint32_t tvStandard = mode->mode_params.output_standards;

  if(tvStandard & STM_OUTPUT_STD_NTG5)
  {
    m_pFSynth->SetDivider(2);
    m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_2);
  }
  else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
    m_pFSynth->SetDivider(4);
    m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_4);
  }
  else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    m_pFSynth->SetDivider(8);
    m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_8);

    /*
     * Only switch the DENC and SDDACs output clock if we are using the DENC.
     *
     * We should not need to touch CLK_PIX_AUX at all for the Main->DENC case.
     * However due to a limitation of current hardware which makes the DENC clock
     * hardwired to the CLK_PIX_AUX clock we're also setting its source to Main
     * when moving the DENC to Main mixer too !!
     *
     * This will be fixed for coming HW.
     */
    if(m_bUsingDENC)
    {
      m_pClkDivider->Enable(STM_CLK_PIX_AUX   , STM_CLK_SRC_HD, STM_CLK_DIV_8);
      m_pClkDivider->Enable(STM_CLK_DENC      , STM_CLK_SRC_HD, STM_CLK_DIV_4);
      m_pClkDivider->Enable(STM_CLK_OUT_SDDACS, STM_CLK_SRC_HD, STM_CLK_DIV_1);
    }
  }
  else
  {
    if(mode->mode_timing.pixel_clock_freq > 148500000)
    {
      /*
       * 1080p 100/120Hz, 4Kx2K
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 1" );
      m_pFSynth->SetDivider(1);
      m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_1);
    }
    else if(mode->mode_timing.pixel_clock_freq > 74250000)
    {
      /*
       * 1080p 50/60Hz or high refresh XGA
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 2" );
      m_pFSynth->SetDivider(2);
      m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_2);
    }
    else
    {
      /*
       * 1080p 25/30Hz, 1080i, 720p
       */
      TRC( TRC_ID_MAIN_INFO, "set fsynth divide to 4" );
      m_pFSynth->SetDivider(4);
      m_pClkDivider->Enable(STM_CLK_PIX_MAIN, STM_CLK_SRC_HD, STM_CLK_DIV_4);
    }
  }
}


void CSTiH407MainOutput::SetMainClockToHDFormatter(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
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
    ASSERTF(m_pClkDivider->getDivide(STM_CLK_PIX_MAIN,&div),("Failed to get pixel clock divider\n"));
    TRC( TRC_ID_MAIN_INFO, "Setting Main pix clock divider (0x%x) as HD Formatter pixel clock divide", div );
  }

  m_pClkDivider->Enable(STM_CLK_PIX_HDDACS, STM_CLK_SRC_HD, div);

  if(tvStandard & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
    m_pClkDivider->Enable(STM_CLK_OUT_HDDACS, STM_CLK_SRC_HD, STM_CLK_DIV_2);
  else
    m_pClkDivider->Enable(STM_CLK_OUT_HDDACS, STM_CLK_SRC_HD, STM_CLK_DIV_1);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407MainOutput::DisableClocks(void)
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


bool CSTiH407MainOutput::SetVTGSyncs(const stm_display_mode_t *mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_sync_sel_map)
  {
    TRC( TRC_ID_ERROR, "- failed : Undefined Syncs mapping !!" );
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
      /* validation team recommends to add -1 shift for HD modes on H407 SoC, refer to Bug 46963 */
      int yuv_hsync_delay = AWG_DELAY_HD - 1;
      /* INFO: following should be reverified if above assignement makes yuv_hsync_delay positive */
      int yuv_vsync_h_delay = mode->mode_timing.pixels_per_line + yuv_hsync_delay;
      if(mode->mode_params.scan_type == STM_INTERLACED_SCAN)
      {
        yuv_vsync_h_delay = (mode->mode_timing.pixels_per_line/2) + yuv_hsync_delay;
        yuv_hsync_delay += mode->mode_timing.pixels_per_line;
        /* End of INFO */
        m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_INVERTED);
      }
      else
      {
        /* 720P50 has 2 pixels horizental shift to the right */
        if(mode->mode_timing.pixels_per_line == 1980)
        {
          yuv_hsync_delay += 2;
          yuv_vsync_h_delay += 2;
        }
        m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
      }
      m_pVTG->SetSyncType(VTG_HDF_SYNC_ID, STVTG_SYNC_POSITIVE);
      m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, yuv_hsync_delay);
      m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, yuv_vsync_h_delay);  
  }
  else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
  {
    m_pVTG->SetSyncType   (VTG_HDF_SYNC_ID,  STVTG_SYNC_POSITIVE);
    m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
    m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, AWG_DELAY_SD);
    m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, AWG_DELAY_SD);

    if(m_bUsingDENC)
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
         * have a clear explanation for the main to DENC delay in the first place.
         */
        denc_sync_shift -= 2;
      }
      m_pVTG->SetSyncType   (VTG_DENC_SYNC_ID, STVTG_SYNC_TOP_NOT_BOT);
      m_pVTG->SetBnottopType(VTG_DENC_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
      m_pVTG->SetHSyncOffset(VTG_DENC_SYNC_ID,  denc_sync_shift);
      m_pVTG->SetVSyncHOffset(VTG_DENC_SYNC_ID, denc_sync_shift);

      /*
       * Set the DENC VIP here as the input format is fixed, to make sure the
       * syncs, clocks and data from the main path are routed to the DENC before
       * we start it.
       *
       * Note: we do not enable clipping in the DENC VIP because the clipping is
       *       controlled as part of the DENC's signal formatting.
       *
       */
      TRC( TRC_ID_MAIN_INFO, "Setting DENC VIP to Main path" );
      m_pDENCVIP->SetInputParams(TVO_VIP_MAIN_VIDEO,
                                 (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_24BIT),
                                 STM_SIGNAL_FULL_RANGE);

    }
  }
  else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
  {
      /* INFO: following should be reverified if above assignement makes yuv_hsync_delay positive */
      int yuv_hsync_delay = AWG_DELAY_ED - 1;
      int yuv_vsync_h_delay = yuv_hsync_delay;
      if(mode->mode_timing.lines_per_frame == 525)
      {
        yuv_vsync_h_delay = -(mode->mode_timing.pixels_per_line + yuv_hsync_delay);
      }
      else if(mode->mode_timing.lines_per_frame == 625)
      {
        yuv_hsync_delay -= 1;
      }
      /* End of INFO */
      m_pVTG->SetSyncType   (VTG_HDF_SYNC_ID, STVTG_SYNC_POSITIVE);
      m_pVTG->SetHSyncOffset(VTG_HDF_SYNC_ID, yuv_hsync_delay);
      m_pVTG->SetVSyncHOffset(VTG_HDF_SYNC_ID, yuv_vsync_h_delay );
      m_pVTG->SetBnottopType(VTG_HDF_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
  }
  else
  {
    TRC( TRC_ID_ERROR, "Unsupported Output Standard" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


uint32_t CSTiH407MainOutput::SetControl(stm_output_control_t ctrl, uint32_t val)
{
  TRC( TRC_ID_UNCLASSIFIED, "ctrl = %d val = %u", ctrl, val );
  switch(ctrl)
  {
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      /*
       * Deliberately skip the MainTVOutput implementation as the HDFormatter
       * does not control the clipping, the VIP does.
       */
      if(CSTmMasterOutput::SetControl(ctrl,val) != STM_OUT_OK)
        return STM_OUT_INVALID_VALUE;

      if(m_bIsStarted && (m_pHDFormatter->GetOwner() == this))
        ConfigureOutput(m_ulOutputFormat);

      break;
    }
    default:
      return CSTmMainTVOutput::SetControl(ctrl,val);
  }

  return STM_OUT_OK;
}


void CSTiH407MainOutput::EnableDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Do nothing when DACs are powered down
   */
  if(m_bDacPowerDown)
  {
    TRCOUT( TRC_ID_MAIN_INFO, "Video DACs are powered down !!" );
    return;
  }

  uint32_t val = ReadSysCfgReg(SYS_CFG5072);

  if (m_pHDFormatter->GetOwner() == this)
  {
    TRC( TRC_ID_MAIN_INFO, "Enabling HD DACs" );
    val &= ~SYS_CFG5072_DAC_HZUVW;
  }
  else if (m_pHDFormatter->GetOwner() == 0)
  {
    /*
     * Only tri-state the HD DAC setup if the aux pipeline is not
     * using the HD formatter for SCART.
     */
    TRC( TRC_ID_MAIN_INFO, "Tri-Stating HD DACs" );
    val |=  SYS_CFG5072_DAC_HZUVW;
  }

  if(m_bUsingDENC)
  {
    if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
    {
      TRC( TRC_ID_MAIN_INFO, "Enabling CVBS DAC" );
      val &= ~SYS_CFG5072_DAC_HZX;
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "Tri-Stating CVBS DAC" );
      val |=  SYS_CFG5072_DAC_HZX;
    }

    /*
     * Note: No S-Video DACs on H407
     */
  }

  WriteSysCfgReg(SYS_CFG5072,val);

  if(!m_bDacHdPowerDisabled)
  {
    TRC( TRC_ID_MAIN_INFO, "Powering Up HD DACs" );
    WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, ~TVO_DAC_POFF_DACS);
  }

  if(m_bUsingDENC)
  {
    TRC( TRC_ID_MAIN_INFO, "Powering Up SD DACs with video from DENC456 output" );
    WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, ~TVO_DAC_POFF_DACS);
    WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_DENC_OUT_SD_SHIFT, TVO_DAC_DENC_OUT_SD_WIDTH, TVO_DAC_DENC_OUT_SD_DAC456);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407MainOutput::DisableDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   *  Power down HD DACs if SD pipeline is not doing SCART
   */

  if ((m_pHDFormatter->GetOwner() == this)||(m_pHDFormatter->GetOwner() == 0))
  {
    TRC( TRC_ID_MAIN_INFO, "Powering Down HD DACs" );
    WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);
  }

  if(m_bUsingDENC)
  {
    /*
     * Power down SD DACs
     */
    TRC( TRC_ID_MAIN_INFO, "Powering Down SD DACs" );
    WriteBitField(TVO_SD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407MainOutput::PowerDownHDDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407MainOutput::PowerDownDACs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteBitField(TVO_HD_DAC_CONFIG, TVO_DAC_POFF_DACS_SHIFT, TVO_DAC_POFF_DACS_WIDTH, TVO_DAC_POFF_DACS);

  COutput::PowerDownDACs();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
