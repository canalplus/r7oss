/***********************************************************************
 *
 * File: display/ip/sync/stmawg.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMAWG_H
#define _STMAWG_H

#include <stm_display_output.h>

#include <vibe_os.h>
#include <vibe_debug.h>


#define AWG_MAX_SIZE 64

class CDisplayDevice;

class CSTmAwg
{
public:
  CSTmAwg (CDisplayDevice* pDev, uint32_t ram_offset);
  virtual ~CSTmAwg (void);

  bool Start (const stm_display_mode_t * const pMode, const uint32_t uFormat);
  bool Stop  (void);

protected:
  // IO mapped pointer to the start of the Gamma register block
  uint32_t* m_pAWGRam;

  virtual bool GenerateRamCode (const stm_display_mode_t * const pMode, const uint32_t uFormat)   = 0;
  bool UploadRamCode (void);

  uint32_t* m_pRamCode;
  uint8_t   m_uRamSize;

  void  WriteAwgRam (uint32_t reg, uint32_t value)
  {
    TRC( TRC_ID_UNCLASSIFIED, "w %p <- %.8x", m_pAWGRam + (reg),value );
    vibe_os_write_register (m_pAWGRam, (reg<<2), value);
  }

  uint32_t ReadAwgRam (uint32_t reg)
  {
    TRC( TRC_ID_UNCLASSIFIED, "r %p -> %.8x", m_pAWGRam + (reg),vibe_os_read_register (m_pAWGRam, reg)  );
    return vibe_os_read_register (m_pAWGRam, (reg<<2));
  }

};

#endif /* _STMAWG_H */
