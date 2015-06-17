/***********************************************************************
 *
 * File: display/ip/analogsync/denc/stmdencsync.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DENC_SYNC_H
#define _STM_DENC_SYNC_H

#include <stm_display.h>

#include <display/ip/analogsync/stmanalogsync.h>
#include <display/ip/analogsync/denc/fw_gen/fw_gen.h>

class CDisplayDevice;

class CSTmDENCSync: public CSTmAnalogSync
{
public:
    CSTmDENCSync ( const char *prefix);

    void LoadFirmware(void);

    bool GetHWConfiguration ( const uint32_t    uFormat,
                              DACMult_Config_t *pDENCDAC_Params);

private:
    CSTmDENCSync(const CSTmDENCSync&);
    CSTmDENCSync& operator=(const CSTmDENCSync&);
};

#endif /* _STM_DENC_SYNC_H   */
