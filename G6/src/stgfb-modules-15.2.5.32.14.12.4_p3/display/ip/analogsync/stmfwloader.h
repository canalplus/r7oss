/***********************************************************************
 *
 * File: display/ip/analogsync/stmfwloader.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFWLOADER_H
#define _STMFWLOADER_H

#include <stm_display_output.h>

#include <vibe_os.h>

class CSTmFwLoader
{
public:
  CSTmFwLoader (uint16_t entry_size, const char *prefix);
  virtual ~CSTmFwLoader (void);

  bool CacheFirmwares (const char * &pDest, uint16_t &pNbEntries);

protected:
  bool        m_bIsFWLoaded;
  const char *m_FWPrefix;
  uint8_t     m_FWEntrySize;
  const  STMFirmware *m_FWbundle;

private:
  CSTmFwLoader(const CSTmFwLoader&);
  CSTmFwLoader& operator=(const CSTmFwLoader&);
};

#endif /* _STMFWLOADER_H */
