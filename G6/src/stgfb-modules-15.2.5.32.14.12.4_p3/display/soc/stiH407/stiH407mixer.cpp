/***********************************************************************
 *
 * File: display/soc/stiH407/stiH407mixer.cpp
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

#include <display/ip/tvout/stmpreformatter.h>

#include "stiH407mixer.h"

/*
 * Values from TVOut functional spec, done like this to avoid having to convert
 * the 2's compliment negative numbers by hand.
 */

static const stm_vout_pf_mat_coef_t PreformatterConversionMat =
{
  /* YCbCr to RGB */
  {
    /* 601 YCbCr to RGB (Not defined) */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000
    , 0x00000000, 0x00000000, 0x00000000, 0x00000000
    }
    /* 709 YCbCr to RGB (Not defined) */
  , { 0x00000000, 0x00000000, 0x00000000, 0x00000000
    , 0x00000000, 0x00000000, 0x00000000, 0x00000000
    }
  },
  /* RGB to YCbCr */
  {
    /* 601 RGB to YCbCr */
    { 0xF927082E, 0x04C9FEAB, 0x01D30964, 0xFA95FD3D,
      0x0000082E, 0x00002000, 0x00002000, 0x00000000
    }
    /* 709 RGB to YCbCr */
  , { 0xF891082F, 0x0367FF40, 0x01280B71, 0xF9B1FE20,
      0x0000082F, 0x00002000, 0x00002000, 0x00000000
    }
  }
};


CSTiH407Mixer::CSTiH407Mixer (CDisplayDevice           *pDev,
                              uint32_t                  mixerOffset,
                              uint32_t                  pfOffset,
                        const stm_mixer_id_t           *planeMap,
                              int                       planeMapSize,
                              stm_mixer_id_t            ulVBILinkedPlane): CGammaMixer(pDev,mixerOffset, planeMap, ulVBILinkedPlane)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_uPFOffset = pfOffset; //Pre-formatter register offset

  /*
   * TODO: Move the valid configuration into the base class and do it like this
   *       for all SoCs
   */
  for(int i=0;i<planeMapSize;i++)
    m_validPlanes |= planeMap[i];

  m_validPlanes |= MIXER_ID_BKG;

  /*
   * H407 Mixer only outputs unsigned and signed RGB, conversion to YUV is
   * in the TVOut pre-formatter.
   */
  m_bHasHwYCbCrMatrix = false;
  m_pPreformatter = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH407Mixer::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if((m_pPreformatter = new CSTmPreformatter(m_pDev, m_uPFOffset, &PreformatterConversionMat)) == 0)
  {
    TRC( TRC_ID_ERROR, "failed to create Aux PreFormatter" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


CSTiH407Mixer::~CSTiH407Mixer(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete m_pPreformatter;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTiH407Mixer::UpdateColorspace(const stm_display_mode_t *pModeLine)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  stm_ycbcr_colorspace_t      colorspaceMode;

  if( (pModeLine) && (m_colorspaceMode == STM_YCBCR_COLORSPACE_AUTO_SELECT) )
      colorspaceMode = (pModeLine->mode_params.output_standards & STM_OUTPUT_STD_HD_MASK)?STM_YCBCR_COLORSPACE_709:STM_YCBCR_COLORSPACE_601;
  else
      colorspaceMode = m_colorspaceMode;

  m_pPreformatter->SetColorSpaceConversion(colorspaceMode, VOUT_PF_CONV_RGB_TO_YUV);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
