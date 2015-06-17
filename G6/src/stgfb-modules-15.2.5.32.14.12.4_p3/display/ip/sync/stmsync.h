/***********************************************************************
 *
 * File: display/ip/sync/stmsync.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMSYNC_H
#define _STMSYNC_H

#include <stm_display_output.h>

#include <vibe_os.h>

class CDisplayDevice;

class CSTmSync
{
public:
  CSTmSync (CDisplayDevice* pDev, uint32_t ulRegOffset);
  virtual ~CSTmSync (void);

  virtual bool Start ( const stm_display_mode_t * const pMode
                     , const uint32_t                   uFormat) = 0;
  bool Stop  (void);

  virtual void Disable (void)     = 0;
  virtual void Enable  (void)     = 0;

  virtual void Suspend (void);
  virtual void Resume  (void);

protected:
  uint32_t* m_pSyncCtrl;
  bool      m_bIsSuspended;
  bool      m_bIsDisabled;

  uint32_t  m_ulOutputFormat;
  stm_display_mode_t m_CurrentMode;

  void  WriteSyncReg (uint32_t reg, uint32_t value)
  {
    vibe_os_write_register (m_pSyncCtrl, reg, value);
  }

  uint32_t ReadSyncReg (uint32_t reg)
  {
    return vibe_os_read_register (m_pSyncCtrl, reg);
  }


};

#endif /* _STMSYNC_H */
