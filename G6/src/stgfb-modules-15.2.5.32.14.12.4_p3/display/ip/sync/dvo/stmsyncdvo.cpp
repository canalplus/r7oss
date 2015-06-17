/***********************************************************************
 *
 * File: display/ip/sync/dvo/stmsyncdvo.cpp
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

#include "stmsyncdvo.h"

static const int AWG_CTRL_REG = 0x0;
static const int AWG_RAM_REG  = 0x100;

#define AWG_CTRL_EN            (1L<<0)
#define AWG_FRAME_BASED_SYNC   (1L<<2)

CSTmSyncDvo::CSTmSyncDvo ( CDisplayDevice* pDev
                         , uint32_t ulRegOffset ): CSTmSync ( pDev, ulRegOffset)
                                                 , m_AWGDVO ( pDev, (ulRegOffset+AWG_RAM_REG))
{
  Disable();
  m_sAwgCodeParams.bAllowEmbeddedSync = false;
  m_sAwgCodeParams.bEnableData = false;
  m_sAwgCodeParams.OutputClkDealy = 0;
}


CSTmSyncDvo::~CSTmSyncDvo () {}

bool CSTmSyncDvo::Start ( const stm_display_mode_t * const pMode
                        , const uint32_t                   uFormat)
{
  uint32_t Value;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  Disable();

  m_AWGDVO.Start(pMode, uFormat, m_sAwgCodeParams);
  if(m_sAwgCodeParams.bAllowEmbeddedSync)
  {
    Value = ReadSyncReg(AWG_CTRL_REG);
    if(pMode->mode_params.scan_type == STM_INTERLACED_SCAN)
      /* Enable AWG for interlaced modes*/
      Value &= ~AWG_FRAME_BASED_SYNC;
    else
      /* Enable AWG for progressive modes*/
      Value |= AWG_FRAME_BASED_SYNC;
    WriteSyncReg(AWG_CTRL_REG, Value);
  }

  Enable();

  m_ulOutputFormat = uFormat;
  m_CurrentMode = *pMode;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

bool CSTmSyncDvo::Start ( const stm_display_mode_t * const pMode
                         , const uint32_t                   uFormat
                         , stm_awg_dvo_parameters_t         sAwgCodeParams)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_sAwgCodeParams = sAwgCodeParams;
  Start(pMode, uFormat);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

bool CSTmSyncDvo::Stop  (void)
{
  return CSTmSync::Stop();
}


void CSTmSyncDvo::Enable (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsDisabled = false;

  /* enable AWG, using previous state */
  uint32_t awg = ReadSyncReg (AWG_CTRL_REG) | AWG_CTRL_EN;
  TRC( TRC_ID_MAIN_INFO, "AWG CTRL REG = 0x%x",awg );
  WriteSyncReg (AWG_CTRL_REG, awg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmSyncDvo::Disable (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsDisabled = true;
  /* disable AWG, but leave state */
  uint32_t awg = ReadSyncReg (AWG_CTRL_REG) & ~(AWG_CTRL_EN);
  WriteSyncReg (AWG_CTRL_REG, awg);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
