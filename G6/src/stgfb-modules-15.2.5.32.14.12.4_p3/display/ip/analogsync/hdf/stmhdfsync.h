/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/stmhdfsync.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_HDF_SYNC_H
#define _STM_HDF_SYNC_H

#include <stm_display.h>

#include <display/ip/analogsync/stmanalogsync.h>
#include <display/ip/analogsync/hdf/fw_gen/fw_gen.h>

class CDisplayDevice;

class CSTmHDFSync: public CSTmAnalogSync
{
public:
    CSTmHDFSync ( const char *prefix);

    void LoadFirmware(void);

    bool GetHWConfiguration ( const uint32_t uFormat,
                              HDFParams_t   *pHDFParams);

private:
    CSTmHDFSync(const CSTmHDFSync&);
    CSTmHDFSync& operator=(const CSTmHDFSync&);
};

#endif /* _STM_HDF_SYNC_H   */
