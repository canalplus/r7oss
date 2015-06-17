/***********************************************************************
 *
 * File: HDTVOutFormatter/stmhdfdvo.cpp
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

#include <Generic/DisplayDevice.h>

#include "stmhdfdvo.h"

/*
 * This represents a FlexDVO block, fed by a HDFormatter block.
 */
static const int HDFMT_DIG2_CFG    = 0x180;

#define HDFMT_CFG_INPUT_YCBCR      (0L)
#define HDFMT_CFG_INPUT_RGB        (1L)
#define HDFMT_CFG_INPUT_AUX        (2L)
#define HDFMT_CFG_INPUT_MASK       (3L)
#define HDFMT_CFG_RCTRL_G          (0L)
#define HDFMT_CFG_RCTRL_B          (1L)
#define HDFMT_CFG_RCTRL_R          (2L)
#define HDFMT_CFG_RCTRL_MASK       (3L)
#define HDFMT_CFG_REORDER_RSHIFT   (2L)
#define HDFMT_CFG_REORDER_GSHIFT   (4L)
#define HDFMT_CFG_REORDER_BSHIFT   (6L)
#define HDFMT_CFG_CLIP_EN          (1L<<8)
#define HDFMT_CFG_CLIP_GB_N_CRCB   (1L<<9)
#define HDFMT_CFG_CLIP_SAV_EAV     (1L<<10)



CSTmHDFDVO::CSTmHDFDVO(CDisplayDevice *pDev,
                   ULONG           ulFDVOOffset,
                   ULONG           ulHDFOffset,
                   COutput        *pMasterOutput): CSTmFDVO(pDev,ulFDVOOffset,pMasterOutput)
{
  DENTRY();

  m_pHDFRegs      = (ULONG*)pDev->GetCtrlRegisterBase() + (ulHDFOffset>>2);

  /*
   * Default digital configuration
   */
  ULONG val = (HDFMT_CFG_RCTRL_R << HDFMT_CFG_REORDER_RSHIFT) |
              (HDFMT_CFG_RCTRL_G << HDFMT_CFG_REORDER_GSHIFT) |
              (HDFMT_CFG_RCTRL_B << HDFMT_CFG_REORDER_BSHIFT) |
               HDFMT_CFG_CLIP_SAV_EAV;

  WriteHDFmtReg(HDFMT_DIG2_CFG, val);

  m_signalRange = STM_SIGNAL_FILTER_SAV_EAV;

  DEXIT();
}


CSTmHDFDVO::~CSTmHDFDVO() {}


void CSTmHDFDVO::SetControl(stm_output_control_t ctrl, ULONG ulNewVal)
{
  switch(ctrl)
  {
    case STM_CTRL_SIGNAL_RANGE:
    {
      if(ulNewVal> STM_SIGNAL_VIDEO_RANGE)
        break;

      m_signalRange = (stm_display_signal_range_t)ulNewVal;

      ULONG val = ReadHDFmtReg(HDFMT_DIG2_CFG) & ~(HDFMT_CFG_CLIP_SAV_EAV |
                                                   HDFMT_CFG_CLIP_EN);

      switch(m_signalRange)
      {
        case STM_SIGNAL_FILTER_SAV_EAV:
          DEBUGF2(2,("CSTmHDFDVO::SetControl - Filter SAV/EAV\n"));
          val |= HDFMT_CFG_CLIP_SAV_EAV;
          break;
        case STM_SIGNAL_VIDEO_RANGE:
          DEBUGF2(2,("CSTmHDFDVO::SetControl - Clip input to video range\n"));
          val |= HDFMT_CFG_CLIP_EN;
          break;
        default:
          DEBUGF2(2,("CSTmHDFDVO::SetControl - Full signal range\n"));
          break;
      }

      WriteHDFmtReg(HDFMT_DIG2_CFG, val);

      break;
    }
    default:
      CSTmFDVO::SetControl(ctrl, ulNewVal);
      break;
  }
}


bool CSTmHDFDVO::SetOutputFormat(ULONG format,const stm_mode_line_t* pModeLine)
{
  DENTRY();

  ULONG hdfconfig = ReadHDFmtReg(HDFMT_DIG2_CFG) & ~HDFMT_CFG_INPUT_MASK;

  switch(format)
  {
    case STM_VIDEO_OUT_RGB:
      DEBUGF2(2,("%s: 24bit RGB\n",__PRETTY_FUNCTION__));
      hdfconfig |= HDFMT_CFG_INPUT_RGB;
      break;
    default:
      break;
  }

  if(pModeLine)
  {
    WriteHDFmtReg(HDFMT_DIG2_CFG, hdfconfig);
  }

  DEXIT();

  return CSTmFDVO::SetOutputFormat(format,pModeLine);
}
