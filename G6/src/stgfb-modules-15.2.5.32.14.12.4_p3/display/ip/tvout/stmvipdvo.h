/***********************************************************************
 *
 * File: display/ip/tvout/stmvipdvo.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_VIP_DVO_H
#define _STM_VIP_DVO_H

#include <stm_display.h>

#include <display/ip/displaytiming/stmvtg.h>

#include "stmvip.h"


typedef enum tvo_vip_dvo_sync_id_e
{
    TVO_VIP_DVO_SYNC_DVO_IDX
  , TVO_VIP_DVO_SYNC_PAD_HSYNC_IDX
  , TVO_VIP_DVO_SYNC_PAD_VSYNC_IDX
  , TVO_VIP_DVO_SYNC_PAD_BNOT_IDX
  , TVO_VIP_DVO_SYNC_VGA_HSYNC_IDX
  , TVO_VIP_DVO_SYNC_VGA_VSYNC_IDX
  , TVO_VIP_DVO_SYNC_NBR
} tvo_vip_dvo_sync_id_t;

#define TVO_VIP_DVO_SYNC_TYPE_DVO               (1L << TVO_VIP_DVO_SYNC_DVO_IDX)
#define TVO_VIP_DVO_SYNC_TYPE_PAD_HSYNC         (1L << TVO_VIP_DVO_SYNC_PAD_HSYNC_IDX)
#define TVO_VIP_DVO_SYNC_TYPE_PAD_VSYNC         (1L << TVO_VIP_DVO_SYNC_PAD_VSYNC_IDX)
#define TVO_VIP_DVO_SYNC_TYPE_PAD_BNOT          (1L << TVO_VIP_DVO_SYNC_PAD_BNOT_IDX)
#define TVO_VIP_DVO_SYNC_TYPE_VGA_HSYNC         (1L << TVO_VIP_DVO_SYNC_VGA_HSYNC_IDX)
#define TVO_VIP_DVO_SYNC_TYPE_VGA_VSYNC         (1L << TVO_VIP_DVO_SYNC_VGA_VSYNC_IDX)

typedef enum stm_display_sync_delay_e
{
    STM_SYNC_DELAY_NONE
  , STM_SYNC_DELAY_ONE_CLOCK
} stm_display_sync_delay_t;


class CSTmVIPDVO: public CSTmVIP
{
public:
  CSTmVIPDVO(CDisplayDevice                 *pDev,
             uint32_t                        ulVIPRegs,
             uint32_t                        ulSyncType,
             const stm_display_sync_id_t    *sync_sel_map,
             const stm_display_sync_delay_t *sync_delay_map,
             uint32_t                        caps = 0);

  virtual ~CSTmVIPDVO(void);

  void SelectSync(const tvo_vip_sync_source_t);

protected:

private:
  const stm_display_sync_delay_t   *m_sync_delay_map;
  uint32_t                          m_ulDvoSyncType;

  CSTmVIPDVO(const CSTmVIP&);
  CSTmVIPDVO& operator=(const CSTmVIP&);
};


#endif //_STM_VIP_DVO_H
