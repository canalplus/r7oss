/***********************************************************************
 *
 * File: display/ip/tvout/stmc8fdvo_v2_1.cpp
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

#include <display/generic/DisplayDevice.h>
#include "stmc8fdvo_v2_1.h"

#define FDVO_DOF_CFG                   (0x4)

#define FDVO_DOF_CHROMA_FILTER_SHIFT   (20)
#define FDVO_DOF_CHROMA_FILTER_MASK    (3L<<FDVO_DOF_CHROMA_FILTER_SHIFT)


CSTmC8FDVO_V2_1::CSTmC8FDVO_V2_1(
    const char     *name,
    uint32_t        id,
    CDisplayDevice *pDev,
    CSTmVIP        *pDVOVIP,
    uint32_t        uFDVOOffset,
    COutput        *pMasterOutput): CSTmFDVO(id, pDev, uFDVOOffset, pMasterOutput, name)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  m_pVIP = pDVOVIP;

  m_u422ChromaFilter = STM_CHROMA_FILTER_DISABLED;

  /*
   * Default digital configuration
   */
  m_pVIP->SetColorChannelOrder(TVO_VIP_CR_R, TVO_VIP_Y_G, TVO_VIP_CB_B);

  m_signalRange = STM_SIGNAL_FILTER_SAV_EAV;

  m_pVIP->SetInputParams((m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?TVO_VIP_MAIN_VIDEO:TVO_VIP_AUX_VIDEO, m_ulOutputFormat, m_signalRange);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmC8FDVO_V2_1::~CSTmC8FDVO_V2_1() {}


OutputResults CSTmC8FDVO_V2_1::Start(const stm_display_mode_t* pModeLine)
{
  if (!SetVTGSyncs(pModeLine))
    return STM_OUT_INVALID_VALUE;

  return CSTmFDVO::Start(pModeLine);
}


uint32_t CSTmC8FDVO_V2_1::SetControl(stm_output_control_t ctrl, uint32_t newVal)
{
  switch(ctrl)
  {
    case OUTPUT_CTRL_422_CHROMA_FILTER:
    {
      if (newVal>STM_CHROMA_FILTER_GFX_ONLY)
        return STM_OUT_INVALID_VALUE;
      m_u422ChromaFilter = newVal;
      /*
       * Just call SetOutputFormat again with the current format and mode
       * to update the hardware filter state, if the output is running.
       */
      SetOutputFormat(m_ulOutputFormat,GetCurrentDisplayMode());
      break;
    }
    case OUTPUT_CTRL_CLIP_SIGNAL_RANGE:
    {
      if(newVal> STM_SIGNAL_VIDEO_RANGE)
        return STM_OUT_INVALID_VALUE;

      m_signalRange = (stm_display_signal_range_t)newVal;
      m_pVIP->SetInputParams((m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?TVO_VIP_MAIN_VIDEO:TVO_VIP_AUX_VIDEO, m_ulOutputFormat, m_signalRange);
      break;
    }
    default:
      return CSTmFDVO::SetControl(ctrl, newVal);
  }

  return STM_OUT_OK;
}


bool CSTmC8FDVO_V2_1::SetOutputFormat(uint32_t format,const stm_display_mode_t* pModeLine)
{
  uint32_t cfg;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CSTmFDVO::SetOutputFormat(format,pModeLine))
    return false;

  m_pVIP->SetInputParams((m_VideoSource==STM_VIDEO_SOURCE_MAIN_COMPOSITOR)?TVO_VIP_MAIN_VIDEO:TVO_VIP_AUX_VIDEO, format, m_signalRange);

  cfg = ReadFDVOReg(FDVO_DOF_CFG)&(~FDVO_DOF_CHROMA_FILTER_MASK);

  TRC( TRC_ID_MAIN_INFO, "422 Chroma filter" );
  cfg |= ((m_u422ChromaFilter << FDVO_DOF_CHROMA_FILTER_SHIFT)&FDVO_DOF_CHROMA_FILTER_MASK);

  WriteFDVOReg(FDVO_DOF_CFG, cfg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}
