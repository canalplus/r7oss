/***********************************************************************
 *
 * File: display/ip/stmvtac.cpp
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

#include "stmvtac.h"

// VTAC TX
#define VTAC_TX_CONFIG                          (0x00000000)
#define VTAC_TX_FIFO_USAGE                      (0x00000030)
#define VTAC_TX_STA_CLR                         (0x00000040)
#define VTAC_TX_STA                             (0x00000044)
#define VTAC_TX_ITS                             (0x00000048)
#define VTAC_TX_ITS_BCLR                        (0x0000004C)
#define VTAC_TX_ITS_BSET                        (0x00000050)
#define VTAC_TX_ITM                             (0x00000054)
#define VTAC_TX_ITM_BCLR                        (0x00000058)
#define VTAC_TX_ITM_BSET                        (0x0000005C)
#define VTAC_TX_DFV0                            (0x00000060)
#define VTAC_TX_DEBUG_PAD_CTRL                  (0x00000080)

// VTAC RX
#define VTAC_RX_CONFIG                          (0x00000000)
#define VTAC_RX_FIFO_CONFIG                     (0x00000004)
#define VTAC_RX_FIRST_ODD_PARITY_ERROR          (0x00000010)
#define VTAC_RX_FIRST_EVEN_PARITY_ERROR         (0x00000014)
#define VTAC_RX_TOTAL_PARITY_ERROR              (0x00000018)
#define VTAC_RX_FIFO_USAGE                      (0x00000030)
#define VTAC_RX_STA_CLR                         (0x00000040)
#define VTAC_RX_STA                             (0x00000044)
#define VTAC_RX_ITS                             (0x00000048)
#define VTAC_RX_ITS_BCLR                        (0x0000004C)
#define VTAC_RX_ITS_BSET                        (0x00000050)
#define VTAC_RX_ITM                             (0x00000054)
#define VTAC_RX_ITM_BCLR                        (0x00000058)
#define VTAC_RX_ITM_BSET                        (0x0000005C)
#define VTAC_RX_DFV0                            (0x00000060)
#define VTAC_RX_DEBUG_PAD_CTRL                  (0x00000080)

#define VTAC_ENABLE_BIT                         (1L << 0)
#define VTAC_SW_RST_BIT                         (1L << 1)
#define VTAC_FIFO_MAIN_CONFIG_VAL               (0x00000004)

/* VTAC Modes parameters */
typedef struct VTAC_ModeParams_s
{
    stm_display_vtac_mode_t      Mode;
    uint32_t                     Pix_freq;
    uint32_t                     Tx_freq;
    uint32_t                     Vsync;
    uint32_t                     Hsync;
    uint32_t                     VidIn_Width;
    uint32_t                     Total_bits;
    uint32_t                     Phyt_Width;
    uint32_t                     Phyts_Per_Pixel;
} VTAC_ModeParams_t;

/* VTAC Tx modes */
static const VTAC_ModeParams_t VTAC_ModeParams[]=
{
    { STM_VTAC_MODE_MAIN_RGB_14_445500 ,   148500,   445500,   1,   1,   0x3,   48,   0x2,   0x0C }           /* Mode  2 */
  , { STM_VTAC_MODE_MAIN_RGB_12_445500 ,   148500,   445500,   1,   1,   0x2,   42,   0x2,   0x0C }           /* Mode  3 */
  , { STM_VTAC_MODE_MAIN_RGB_12_371250 ,   148500,   371250,   1,   1,   0x2,   40,   0x2,   0x0A }           /* Mode  4 */
  , { STM_VTAC_MODE_MAIN_RGB_10_371250 ,   148500,   371250,   1,   1,   0x1,   36,   0x2,   0x0A }           /* Mode  5 */
  , { STM_VTAC_MODE_MAIN_RGB_10_297000 ,   148000,   297000,   1,   1,   0x1,   32,   0x2,   0x08 }           /* Mode  6 */
  , { STM_VTAC_MODE_DVO_12_445500      ,   148500,   445500,   1,   1,   0x2,   48,   0x2,   0x0C }           /* Mode  7 */
  , { STM_VTAC_MODE_DVO_10_445500      ,   148500,   445500,   1,   1,   0x1,   42,   0x2,   0x0C }           /* Mode  8 */
  , { STM_VTAC_MODE_DVO_10_371250      ,   148500,   371250,   1,   1,   0x1,   40,   0x2,   0x0A }           /* Mode  9 */
  , { STM_VTAC_MODE_DVO_8_371250       ,   148500,   371250,   1,   1,   0x0,   36,   0x2,   0x0A }           /* Mode 10 */
  , { STM_VTAC_MODE_AUX_10_121500      ,    13500,   121500,   1,   1,   0x1,   36,   0x0,   0x24 }           /* Mode 11 */
  , { STM_VTAC_MODE_AUX_10_114750      ,    13500,   114750,   1,   1,   0x1,   34,   0x0,   0x22 }           /* Mode 12 */
  , { STM_VTAC_MODE_AUX_10_108000      ,    13500,   108000,   1,   1,   0x1,   32,   0x0,   0x20 }           /* Mode 13 */
  , { STM_VTAC_MODE_AUX_8_101250       ,    13500,   101250,   1,   1,   0x0,   30,   0x0,   0x1E }           /* Mode 14 */
  , { STM_VTAC_MODE_AUX_8_94500        ,    13500,    94500,   1,   1,   0x0,   28,   0x0,   0x1C }           /* Mode 15 */
  , { STM_VTAC_MODE_AUX_8_87750        ,    13500,    87750,   1,   1,   0x0,   26,   0x0,   0x1A }           /* Mode 16 */
};

CSTmVTAC::CSTmVTAC (CDisplayDevice       *pDev,
                    stm_display_vtac_id_t ID,
                    uint32_t              syscfg_video_base,
                    uint32_t              vtac_tx_offset,
                    uint32_t              vtac_rx_offset)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_ID                  = ID;
  m_pDevRegs            = (uint32_t*)pDev->GetCtrlRegisterBase();
  m_uSYSCFGVideoOffset  = syscfg_video_base;
  m_uTXOffset           = vtac_tx_offset;
  m_uRXOffset           = vtac_rx_offset;

  m_VTAC_params.even_parity_en  = true;
  m_VTAC_params.odd_parity_en   = true;
  m_VTAC_params.alpha_en        = false;
  m_VTAC_params.gfx_en          = false;
  m_VTAC_params.vid_en          = false;

  m_CurrentModeIndex = STM_VTAC_MODE_UNKNOWN_MODE;
  m_PendingModeIndex = STM_VTAC_MODE_UNKNOWN_MODE;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmVTAC::~CSTmVTAC (void) {}

bool CSTmVTAC::ConfigVTACPHYParams(void)
{
  return true;
}

bool CSTmVTAC::SetVTACParams (const stm_display_vtac_params_t VTAC_Params)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "%d-%d-%d-%d-%d",VTAC_Params.even_parity_en, VTAC_Params.odd_parity_en, VTAC_Params.alpha_en, VTAC_Params.gfx_en, VTAC_Params.vid_en );

  m_VTAC_params.even_parity_en  = VTAC_Params.even_parity_en;
  m_VTAC_params.odd_parity_en   = VTAC_Params.odd_parity_en;
  m_VTAC_params.alpha_en        = VTAC_Params.alpha_en;
  m_VTAC_params.gfx_en          = VTAC_Params.gfx_en;
  m_VTAC_params.vid_en          = VTAC_Params.vid_en;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmVTAC::GetVTACParams (stm_display_vtac_params_t * const pVTAC_Params)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pVTAC_Params)
    return false;

  *pVTAC_Params  = m_VTAC_params;

  TRC( TRC_ID_MAIN_INFO, "%d-%d-%d-%d-%d",pVTAC_Params->even_parity_en, pVTAC_Params->odd_parity_en, pVTAC_Params->alpha_en, pVTAC_Params->gfx_en, pVTAC_Params->vid_en );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

bool CSTmVTAC::Start (const stm_display_vtac_mode_t Mode)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_PendingModeIndex = Mode;

  if(!ConfigVTACPHYParams())
    return false;

  if(!ProgramVTACSettings())
    return false;

  m_CurrentModeIndex = m_PendingModeIndex;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}

void CSTmVTAC::Stop (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CSTmVTAC::ProgramVTACSettings (void)
{
  uint32_t val;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  WriteRXReg(VTAC_RX_CONFIG, 0 );
  WriteRXReg(VTAC_RX_FIFO_CONFIG, 0);
  WriteTXReg(VTAC_TX_CONFIG, 0 );

  val  = (VTAC_ENABLE_BIT | VTAC_SW_RST_BIT);

  val |= (  (m_VTAC_params.even_parity_en << 13) | (m_VTAC_params.odd_parity_en << 12)
          | (m_VTAC_params.gfx_en         << 10) | (m_VTAC_params.vid_en        <<  9)
          | (m_VTAC_params.alpha_en       <<  8) );

  val |= (  (VTAC_ModeParams[m_PendingModeIndex].Phyts_Per_Pixel << 23)
          | (VTAC_ModeParams[m_PendingModeIndex].Phyt_Width      << 16)
          | (VTAC_ModeParams[m_PendingModeIndex].VidIn_Width     <<  4) );


  WriteRXReg(VTAC_RX_CONFIG, val );
  WriteRXReg(VTAC_RX_FIFO_CONFIG, VTAC_FIFO_MAIN_CONFIG_VAL);
  WriteTXReg(VTAC_TX_CONFIG, val );

  TRC( TRC_ID_MAIN_INFO, "CSTmVTAC::ProgramVTACSettings: %p = 0x%08x", ABSOLUTE_ADDRESS(m_uRXOffset, VTAC_RX_CONFIG),      ReadRXReg(VTAC_RX_CONFIG) );
  TRC( TRC_ID_MAIN_INFO, "CSTmVTAC::ProgramVTACSettings: %p = 0x%08x", ABSOLUTE_ADDRESS(m_uRXOffset, VTAC_RX_FIFO_CONFIG), ReadRXReg(VTAC_RX_FIFO_CONFIG) );
  TRC( TRC_ID_MAIN_INFO, "CSTmVTAC::ProgramVTACSettings: %p = 0x%08x", ABSOLUTE_ADDRESS(m_uTXOffset, VTAC_TX_CONFIG),      ReadTXReg(VTAC_TX_CONFIG) );


  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
