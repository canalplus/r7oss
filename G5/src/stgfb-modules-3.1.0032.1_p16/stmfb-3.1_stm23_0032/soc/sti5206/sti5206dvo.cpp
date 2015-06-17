/***********************************************************************
 *
 * File: soc/sti5206/sti5206dvo.cpp
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

#include "sti5206dvo.h"
#include "sti5206reg.h"

CSTi5206DVO::CSTi5206DVO(CDisplayDevice *pDev,
                         COutput        *pMasterOutput,
                         ULONG           ulDVOBase,
                         ULONG           ulHDFormatterBase): CSTmHDFDVO(pDev, ulDVOBase, ulHDFormatterBase, pMasterOutput)
{
  DENTRY();

  /*
   * 5206 only has an 8bit DVO output
   */
  m_ulOutputFormat = STM_VIDEO_OUT_ITUR656;

  DEXIT();
}


bool CSTi5206DVO::SetOutputFormat(ULONG format,const stm_mode_line_t* pModeLine)
{
  DENTRY();

  /*
   * 5206 only has an 8bit DVO interface
   */
  if(format != STM_VIDEO_OUT_ITUR656)
    return false;

  return CSTmHDFDVO::SetOutputFormat(format,pModeLine);
}
