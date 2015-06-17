/***********************************************************************
 *
 * File: display/ip/sync/stmawg.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>

#include <vibe_debug.h>
#include "stmawg.h"


CSTmAwg::CSTmAwg (CDisplayDevice *pDev, uint32_t ram_offset)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  m_pAWGRam = (uint32_t*)pDev->GetCtrlRegisterBase()+(ram_offset >> 2);
  m_pRamCode = 0;
  m_uRamSize = 0;
}


CSTmAwg::~CSTmAwg (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );
}


bool CSTmAwg::Start (const stm_display_mode_t * const pMode, const uint32_t uFormat)
{
  TRC( TRC_ID_UNCLASSIFIED, "for %d", pMode->mode_id );

  if (UNLIKELY ( !GenerateRamCode (pMode, uFormat)
              && !UploadRamCode ()))
    return false;

  return true;
}


bool CSTmAwg::Stop (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  return true;
}


bool CSTmAwg::UploadRamCode (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "uploading AWG timing (%u bytes)", m_uRamSize *4 );

  if (UNLIKELY (m_uRamSize > AWG_MAX_SIZE))
  {
    TRC( TRC_ID_MAIN_INFO, "size (%u) > AWG_MAX_SIZE (%d)", m_uRamSize, AWG_MAX_SIZE );
    return false;
  }

  /* write AWG firmware */
  for (uint8_t i = 0; i < m_uRamSize; i++)
  {
    WriteAwgRam (i, m_pRamCode[i]);
  }

#ifdef DEBUG
  /* dump AWG firmware */
  for (uint8_t i = 0; i < AWG_MAX_SIZE; ++i)
  {
    uint32_t reg = ReadAwgRam (i);
    TRC( TRC_ID_UNCLASSIFIED, "read RAM(%.2d): %.8x", i, reg );
  }
#endif

  return true;
}

