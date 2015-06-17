/***********************************************************************
 *
 * File: display/ip/sync/dvo/stmawgdvo.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <display/generic/DisplayDevice.h>

#include <vibe_debug.h>
#include "stmawgdvo.h"


CSTmAwgDvo::CSTmAwgDvo ( CDisplayDevice *pDev
                       , uint32_t        ram_offset): CSTmAwg (pDev, ram_offset)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  m_sAwgCodeParams.bAllowEmbeddedSync = false;
  m_sAwgCodeParams.bEnableData        = false;
  m_sAwgCodeParams.OutputClkDealy     = 1;
}


CSTmAwgDvo::~CSTmAwgDvo (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );
}


bool CSTmAwgDvo::Start ( const stm_display_mode_t * const pMode
                       , const uint32_t                   uFormat
                       , stm_awg_dvo_parameters_t         AwgCodeParams)
{
  m_sAwgCodeParams = AwgCodeParams;

  TRC( TRC_ID_UNCLASSIFIED, "for %d", pMode->mode_id );

  return CSTmAwg::Start(pMode, uFormat);
}


bool CSTmAwgDvo::Stop (void)
{
  TRC( TRC_ID_UNCLASSIFIED, "" );

  return true;
}


bool CSTmAwgDvo::GenerateRamCode ( const stm_display_mode_t * const pMode
                                 , const uint32_t                   uFormat)
{
  return stm_dvo_awg_generate_code( pMode, uFormat, m_sAwgCodeParams, &m_uRamSize, &m_pRamCode);
}
