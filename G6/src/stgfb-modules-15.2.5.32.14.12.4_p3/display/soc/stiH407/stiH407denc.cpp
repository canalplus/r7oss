/***********************************************************************
 *
 * File: display/soc/stiH407/sti407denc.cpp
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

#include "stiH407denc.h"

/*
 * Note: Currently the only known difference between the DENC on H407 and
 *       recent SoCs is that the DENC outputs that are normally connected to
 *       the DACs for S-Video don't go anywhere as the DACs do not exist on
 *       this SoC.
 *
 *       We may find there are more differences later.
 */
CSTiH407DENC::CSTiH407DENC(CDisplayDevice* pDev,
                           uint32_t baseAddr,
                           CSTmTVOutTeletext *pTeletext): CSTmTVOutDENC(pDev, baseAddr, pTeletext)
{

}


bool CSTiH407DENC::SetMainOutputFormat(uint32_t format)
{
  /*
   * STiH407 does not have Y/C (S-Video) DACs so do not allow the format to be
   * selected on the DENC.
   */
  if((format & STM_VIDEO_OUT_YC) != 0)
    return false;

  return CSTmTVOutDENC::SetMainOutputFormat(format);
}

