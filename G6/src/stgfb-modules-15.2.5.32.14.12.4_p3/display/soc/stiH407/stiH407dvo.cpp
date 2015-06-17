/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407dvo.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>
#include <vibe_os.h>
#include <vibe_debug.h>

#include <display/ip/tvout/stmvipdvo.h>

#include <display/ip/displaytiming/stmclocklla.h>
#include <display/ip/displaytiming/stmvtg.h>

#include "stiH407device.h"
#include "stiH407dvo.h"
#include "stiH407reg.h"

#define VTG_DVO_SYNC_ID   m_pTVOSyncMap[TVO_VIP_DVO_SYNC_DVO_IDX]

CSTiH407DVO::CSTiH407DVO(
  CDisplayDevice              *pDev,
  COutput                     *pMasterOutput,
  CSTmVTG                     *pVTG,
  CSTmClockLLA                *pClkDivider,
  CSTmVIP                     *pDVOVIP,
  const stm_display_sync_id_t *pSyncMap,
  uint32_t                     uDVOBase): CSTmC8FDVO_V2_1( "fdvo0",
                                                  STiH407_OUTPUT_IDX_DVO,
                                                  pDev,
                                                  pDVOVIP,
                                                  uDVOBase,
                                                  pMasterOutput)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pClkDivider = pClkDivider;
  m_pVTG        = pVTG;
  m_pTVOSyncMap = pSyncMap;

  m_VideoSource = (STiH407_OUTPUT_IDX_MAIN == m_pMasterOutput->GetID())?STM_VIDEO_SOURCE_MAIN_COMPOSITOR:STM_VIDEO_SOURCE_AUX_COMPOSITOR;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CSTiH407DVO::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if (!CSTmC8FDVO_V2_1::Create())
    return false;

  /*
   * TODO: Setup the SAS VTG sync outputs to the FlexDVO formatter.
   */

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

void CSTiH407DVO::DisableClocks(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Disabling DVO Clocks" );
  m_pClkDivider->Disable(STM_CLK_PIX_DVO);
  m_pClkDivider->Disable(STM_CLK_OUT_DVO);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CSTiH407DVO::SetVTGSyncs(const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  uint32_t tvStandard = pModeLine->mode_params.output_standards;
  int dvo_sync_shift = 0;

  if(!m_pTVOSyncMap)
  {
    TRC( TRC_ID_ERROR, "- failed : Undefined Syncs mapping !!" );
    return false;
  }

  if(m_sAwgCodeParams.bAllowEmbeddedSync)
  {
    /* delay caused by AWG firmware is equal to HSync front porch */
    int trailingPixels = pModeLine->mode_timing.pixels_per_line - (pModeLine->mode_params.active_area_start_pixel + pModeLine->mode_params.active_area_width);

    if(tvStandard & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
    {
      dvo_sync_shift = TVOUT_DVO_EMB_DELAY_HD - trailingPixels;
    }
    else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
    {
      dvo_sync_shift = TVOUT_DVO_EMB_DELAY_ED - trailingPixels;
    }
    else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
    {
      dvo_sync_shift = TVOUT_DVO_EMB_DELAY_SD - trailingPixels;
    }
    m_pVTG->SetSyncType(VTG_DVO_SYNC_ID, STVTG_SYNC_POSITIVE);
  }
  else
  {
    if(tvStandard & (STM_OUTPUT_STD_HD_MASK | STM_OUTPUT_STD_XGA))
    {
      dvo_sync_shift = TVOUT_DVO_EXT_DELAY_HD;
    }
    else if(tvStandard & (STM_OUTPUT_STD_ED_MASK | STM_OUTPUT_STD_VGA))
    {
      dvo_sync_shift = TVOUT_DVO_EXT_DELAY_ED;
    }
    else if(tvStandard & STM_OUTPUT_STD_SD_MASK)
    {
      dvo_sync_shift = TVOUT_DVO_EXT_DELAY_SD;
    }
    m_pVTG->SetSyncType(VTG_DVO_SYNC_ID, STVTG_SYNC_TIMING_MODE);
  }

  if(pModeLine->mode_params.scan_type == STM_INTERLACED_SCAN)
    m_pVTG->SetBnottopType(VTG_DVO_SYNC_ID, STVTG_SYNC_BNOTTOP_INVERTED);
  else
    m_pVTG->SetBnottopType(VTG_DVO_SYNC_ID, STVTG_SYNC_BNOTTOP_NOT_INVERTED);
  m_pVTG->SetVSyncHOffset(VTG_DVO_SYNC_ID, dvo_sync_shift);
  m_pVTG->SetHSyncOffset(VTG_DVO_SYNC_ID, dvo_sync_shift);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTiH407DVO::SetOutputFormat(uint32_t format,const stm_display_mode_t* pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(format == 0) /* Disable Output */
  {
    this->Stop();
    return true;
  }

  if(pModeLine)
  {
    /*
     * If the output is already running, work out if we can setup the
     * required clock divides for the new format before setting it.
     */
    stm_clk_divider_output_source_t  source = (m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?STM_CLK_SRC_HD:STM_CLK_SRC_SD;
    stm_clk_divider_output_name_t      name = (m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?STM_CLK_PIX_MAIN:STM_CLK_PIX_AUX;
    stm_clk_divider_output_divide_t  divide;
    stm_clk_divider_output_divide_t fdvodiv;

    m_pClkDivider->getDivide(name,&divide);

    switch(format)
    {
      case (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT):
      case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_16BIT):
      case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_24BIT):
      {
        /*
         * DVO clock same as pixel clock
         */
        TRC( TRC_ID_MAIN_INFO, "FDVO clock == PIX clock" );
        fdvodiv = divide;
        break;
      }
      case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_16BIT):
      case (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_ITUR656):
      {
        switch(divide)
        {
          case STM_CLK_DIV_2:
            fdvodiv = STM_CLK_DIV_1;
            break;
          case STM_CLK_DIV_4:
            fdvodiv = STM_CLK_DIV_2;
            break;
          case STM_CLK_DIV_8:
            fdvodiv = STM_CLK_DIV_4;
            break;
          case STM_CLK_DIV_1:
          default:
            /*
             * DVO clock 2x pixel clock, not possible if pixel clock == reference clock
             */
            TRC( TRC_ID_ERROR, "Cannot set 2xPIX clock" );
            return false;
        }

        TRC( TRC_ID_MAIN_INFO, "FDVO clock == 2xPIX clock" );
        break;
      }
      default:
        TRC( TRC_ID_ERROR, "Invalid format requested" );
        return false;
    }

    if(!CSTmC8FDVO_V2_1::SetOutputFormat(format,pModeLine))
      return false;

    m_pClkDivider->Enable(STM_CLK_PIX_DVO, source, fdvodiv);
    m_pClkDivider->Enable(STM_CLK_OUT_DVO, source, fdvodiv);
  }
  else
  {
    /*
     * Simple case where the output is stopped.
     */
    if(!CSTmC8FDVO_V2_1::SetOutputFormat(format,pModeLine))
      return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
