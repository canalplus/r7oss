/***********************************************************************
 *
 * File: display/ip/tvout/stmpreformatter.cpp
 * Copyright (c) 2011 STMicroelectronics Limited.
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

#include "stmpreformatter.h"

#define TVO_CSC_MAT_OFFSET             0x00
#define TVO_ADAPTIVE_DCMTN_FLTR_CFG    0x20
#define TVO_IN_VID_FORMAT              0x30

#define M_601_YCBCR_TO_RGB      m_pPFConversionMat->ycbcr_to_rgb[0]
#define M_709_YCBCR_TO_RGB      m_pPFConversionMat->ycbcr_to_rgb[1]
#define M_601_RGB_TO_YCBCR      m_pPFConversionMat->rgb_to_ycbcr[0]
#define M_709_RGB_TO_YCBCR      m_pPFConversionMat->rgb_to_ycbcr[1]

#define PF_ADAPTIVE_FILTER      0x00
#define PF_LINEAR_FILTER        0x01
#define PF_SAMPLE_DROP          0x03

CSTmPreformatter::CSTmPreformatter(CDisplayDevice               *pDev,
                                   uint32_t                      ulPFRegs,
                                   const stm_vout_pf_mat_coef_t *ulPFConversionMat)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pDevRegs         = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_ulPFRegOffset    = ulPFRegs;
  m_pPFConversionMat = ulPFConversionMat;

  m_bUseAdaptiveDecimationFilter = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmPreformatter::~CSTmPreformatter() {}


bool CSTmPreformatter::SetColorSpaceConversion(stm_ycbcr_colorspace_t colorspaceMode, stm_vout_pf_mat_direction_t direction)
{
  int input=0;

  TRC( TRC_ID_MAIN_INFO, "- In" );

  if(colorspaceMode == STM_YCBCR_COLORSPACE_709)
    input = 1;

  for(uint32_t i=0; i<8; i++)
  {
    switch(direction)
    {
      case VOUT_PF_CONV_YUV_TO_RGB:
        WriteReg((TVO_CSC_MAT_OFFSET + 4*i), m_pPFConversionMat->ycbcr_to_rgb[input][i]);
        TRC( TRC_ID_MAIN_INFO, "- WriteReg - Add=0x%08X, val=0x%08X.", (TVO_CSC_MAT_OFFSET + 4*i), m_pPFConversionMat->ycbcr_to_rgb[input][i] );
        break;
      case VOUT_PF_CONV_RGB_TO_YUV:
        WriteReg((TVO_CSC_MAT_OFFSET + 4*i), m_pPFConversionMat->rgb_to_ycbcr[input][i]);
        TRC( TRC_ID_MAIN_INFO, "- WriteReg - Add=0x%08X, val=0x%08X.", (TVO_CSC_MAT_OFFSET + 4*i), m_pPFConversionMat->rgb_to_ycbcr[input][i] );
        break;
      default:
        TRC( TRC_ID_ERROR, "Invalid pre-formatter conversion request" );
        return false;
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

bool CSTmPreformatter::SetAdaptiveDecimationFilter(stm_vout_pf_adaptive_decimation_filter_t filter)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  if(!m_bUseAdaptiveDecimationFilter)
  {
    return false;
  }
  switch(filter)
  {
    case VOUT_PF_ADAPTIVE_FILTER:
      WriteReg(TVO_ADAPTIVE_DCMTN_FLTR_CFG, VOUT_PF_ADAPTIVE_FILTER);
      TRC( TRC_ID_MAIN_INFO, "- WriteReg - Add=0x%08X, val=0x%08X.", TVO_ADAPTIVE_DCMTN_FLTR_CFG, PF_ADAPTIVE_FILTER );
      break;
    case VOUT_PF_LINEAR_FILTER:
      WriteReg(TVO_ADAPTIVE_DCMTN_FLTR_CFG, VOUT_PF_LINEAR_FILTER);
      TRC( TRC_ID_MAIN_INFO, "- WriteReg - Add=0x%08X, val=0x%08X.", TVO_ADAPTIVE_DCMTN_FLTR_CFG, PF_LINEAR_FILTER );
      break;
    case VOUT_PF_SAMPLE_DROP:
      WriteReg(TVO_ADAPTIVE_DCMTN_FLTR_CFG, VOUT_PF_SAMPLE_DROP);
      TRC( TRC_ID_MAIN_INFO, "- WriteReg - Add=0x%08X, val=0x%08X.", TVO_ADAPTIVE_DCMTN_FLTR_CFG, PF_SAMPLE_DROP );
      break;
    default:
      TRC( TRC_ID_ERROR, "Invalid pre-formatter adaptive decimation filter configuration" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
