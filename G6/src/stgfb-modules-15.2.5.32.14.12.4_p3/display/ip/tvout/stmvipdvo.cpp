/***********************************************************************
 *
 * File: display/ip/tvout/stmvipdvo.cpp
 * Copyright (c) 2013 STMicroelectronics Limited.
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

#include "stmvipdvo.h"

#define TVO_VIP_DVO_CTL_OFFSET                      0x00                    /* VIP Configuration Register */


#define TVO_VIP_DVO_SYNC_SEL_OFFSET                 0x18
#define TVO_VIP_DVO_VGA_SYNC_SEL_OFFSET             0x1C
#define TVO_VIP_DVO_SYNC_SEL_WIDTH                  5
#define TVO_VIP_DVO_SYNC_DELAY_WIDTH                1
#define TVO_VIP_DVO_SYNC_DVO_SHIFT                  0
#define TVO_VIP_DVO_SYNC_PAD_HSYNC_SHIFT            8
#define TVO_VIP_DVO_SYNC_PAD_VSYNC_SHIFT            16
#define TVO_VIP_DVO_SYNC_PAD_BNOT_SHIFT             24
#define TVO_VIP_DVO_SYNC_VGA_HSYNC_SHIFT            32
#define TVO_VIP_DVO_SYNC_VGA_VSYNC_SHIFT            40
#define TVO_VIP_DVO_SYNC_SEL_OFFSET_MAIN            0
#define TVO_VIP_DVO_SYNC_SEL_OFFSET_AUX             (2 << 3)


int tvo_vip_dvo_sync_shift [TVO_VIP_DVO_SYNC_NBR] =
{
    TVO_VIP_DVO_SYNC_DVO_SHIFT              /* TVO_VIP_DVO_SYNC_DVO_IDX */
  , TVO_VIP_DVO_SYNC_PAD_HSYNC_SHIFT        /* TVO_VIP_DVO_SYNC_PAD_HSYNC_IDX */
  , TVO_VIP_DVO_SYNC_PAD_VSYNC_SHIFT        /* TVO_VIP_DVO_SYNC_PAD_VSYNC_IDX */
  , TVO_VIP_DVO_SYNC_PAD_BNOT_SHIFT         /* TVO_VIP_DVO_SYNC_PAD_BNOT_IDX */
  , TVO_VIP_DVO_SYNC_VGA_HSYNC_SHIFT        /* TVO_VIP_DVO_SYNC_VGA_HSYNC_IDX */
  , TVO_VIP_DVO_SYNC_VGA_VSYNC_SHIFT        /* TVO_VIP_DVO_SYNC_VGA_VSYNC_IDX */
};


CSTmVIPDVO::CSTmVIPDVO(CDisplayDevice                 *pDev,
                       uint32_t                        ulVIPRegs,
                       uint32_t                        ulSyncType,
                       const stm_display_sync_id_t    *sync_sel_map,
                       const stm_display_sync_delay_t *sync_delay_map,
                       uint32_t                        capabilities): CSTmVIP(pDev, ulVIPRegs, ulSyncType&TVO_VIP_SYNC_DVO_IDX, sync_sel_map, capabilities)
{
  TRC( TRC_ID_MAIN_INFO, "CSTmVIPDVO::CSTmVIPDVO: pDev=%p - ulVIPRegs=0x%x", pDev, ulVIPRegs );
  m_ulDvoSyncType  = ulSyncType;
  m_sync_delay_map = sync_delay_map;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmVIPDVO::~CSTmVIPDVO() {}



void CSTmVIPDVO::SelectSync(const tvo_vip_sync_source_t sync_source)
{
  TRC( TRC_ID_MAIN_INFO, "- Input is %s", ((sync_source == TVO_VIP_MAIN_SYNCS)? "Main":"Aux") );

  int      idx = 0;
  uint32_t SyncSel[2];
  uint32_t SyncSelMainOrAux;

  SyncSel[0] = ReadReg(TVO_VIP_DVO_SYNC_SEL_OFFSET);
  SyncSel[1] = ReadReg(TVO_VIP_DVO_VGA_SYNC_SEL_OFFSET);

  SyncSelMainOrAux = (sync_source == TVO_VIP_MAIN_SYNCS)?TVO_VIP_DVO_SYNC_SEL_OFFSET_MAIN:TVO_VIP_DVO_SYNC_SEL_OFFSET_AUX;

  for(idx=0; idx < TVO_VIP_DVO_SYNC_NBR; idx++)
  {
    if(m_ulDvoSyncType & (1 << idx))
    {
      SetBitField(SyncSel[((tvo_vip_dvo_sync_shift[idx]>=32)?1:0)], tvo_vip_dvo_sync_shift[idx], TVO_VIP_DVO_SYNC_SEL_WIDTH, (SyncSelMainOrAux | m_sync_sel_map[idx]));
      if (idx>0) /* No delay available for dvo data */
        SetBitField(SyncSel[((tvo_vip_dvo_sync_shift[idx]>=32)?1:0)], (tvo_vip_dvo_sync_shift[idx]+TVO_VIP_DVO_SYNC_SEL_WIDTH), TVO_VIP_DVO_SYNC_DELAY_WIDTH, m_sync_delay_map[idx]);
    }
  }

  TRC( TRC_ID_MAIN_INFO, "- SyncSelVGA = 0x%x, SyncSel = 0x%x", SyncSel[1], SyncSel[0] );

  WriteReg(TVO_VIP_DVO_SYNC_SEL_OFFSET, SyncSel[0]);
  WriteReg(TVO_VIP_DVO_VGA_SYNC_SEL_OFFSET, SyncSel[1]);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}
