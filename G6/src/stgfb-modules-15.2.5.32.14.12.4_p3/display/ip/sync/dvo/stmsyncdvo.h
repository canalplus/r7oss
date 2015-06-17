/***********************************************************************
 *
 * File: display/ip/sync/dvo/stmsyncdvo.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMSYNC_DVO_H
#define _STMSYNC_DVO_H

#include <stm_display_output.h>
#include <display/ip/sync/dvo/stmawgdvo.h>
#include <display/ip/sync/stmsync.h>

#include <vibe_os.h>

class CDisplayDevice;
class CSTmAwgDvo;



class CSTmSyncDvo: public CSTmSync
{
public:
  CSTmSyncDvo (CDisplayDevice* pDev, uint32_t ulRegOffset);
  virtual ~CSTmSyncDvo (void);

  bool Start ( const stm_display_mode_t * const pMode
             , const uint32_t                   uFormat);
  bool Start ( const stm_display_mode_t * const pMode
             , const uint32_t                   uFormat
             , stm_awg_dvo_parameters_t         AwgCodeParams);
  bool Stop  (void);

  virtual void Disable (void);
  virtual void Enable  (void);

protected:
  CSTmAwgDvo               m_AWGDVO;
  stm_awg_dvo_parameters_t m_sAwgCodeParams;

  void     WriteReg (uint32_t reg, uint32_t value)  { vibe_os_write_register (m_pSyncCtrl, reg, value); }
  uint32_t ReadReg  (uint32_t reg)           { return vibe_os_read_register  (m_pSyncCtrl, reg); }

};

#endif /* _STMSYNC_DVO_H */
