/***********************************************************************
 *
 * File: display/ip/analogsync/stmfwloader.cpp
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stm_display.h>
#include <vibe_debug.h>
#include <display/generic/DisplayDevice.h>
#include <display/generic/Output.h>

#include "stmfwloader.h"

typedef struct FirmwareEntry_s
{
    uint8_t  fw_version;
    uint8_t  fw_entry_size;
    uint16_t fw_entries_count;
    uint32_t fw_crc;
    char const * pData;
} FirmwareEntry_t;

static const uint8_t    FIRMWARE_MIN_DATA_SIZE  = ((2 * sizeof(uint8_t)) + sizeof(uint16_t) + sizeof(uint32_t));
static const uint8_t    FIRMWARE_NAME_MAX       = 30;  // same as in linux/firmware.h
static const uint32_t   FIRMWARE_BUNDLE_CRC     = 0x78563412;

static const uint8_t    FIRMWARE_BUNDLE_VERSION = 2;
/* layout of firmware bundle file (always BIG endian):
      uint8_t  version
      uint8_t  size of a fw entry
      uint16_t number of entries
      uint32_t crc
      char *   Data;
*/

CSTmFwLoader::CSTmFwLoader (uint16_t entry_size, const char *prefix)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_bIsFWLoaded  = false;

  m_FWPrefix     = prefix;
  m_FWEntrySize  = entry_size;

  m_FWbundle = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmFwLoader::~CSTmFwLoader (void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if (LIKELY (m_FWbundle))
    vibe_os_release_firmware (m_FWbundle);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmFwLoader::CacheFirmwares (const char * &pDest, uint16_t &pNbEntries)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    char name[FIRMWARE_NAME_MAX];
    const FirmwareEntry_t *FirmwareEntry_p;

    /*
     * If this firmware was previously cached, release it before trying to
     * reload and cache it again.
     */
    if(m_bIsFWLoaded)
    {
        vibe_os_release_firmware (m_FWbundle);
        m_bIsFWLoaded = false;
        m_FWbundle = 0;
    }

    pDest = 0;

    if (vibe_os_snprintf (name, sizeof (name),
                        "%s.fw", m_FWPrefix) < 0)
        return false;

    TRC( TRC_ID_MAIN_INFO, "caching '%s'", name );

    if (LIKELY (vibe_os_request_firmware (&m_FWbundle, name)))
        return false;

    /* do some basic error checks of the firmware data here */
    ASSERTF (m_FWbundle, ("m_FWBundle == NULL?!\n"));
    ASSERTF (m_FWbundle->pData, ("m_FWbundle->pData == NULL?!\n"));
    ASSERTF (m_FWbundle->ulDataSize > FIRMWARE_MIN_DATA_SIZE, ("m_FWBundle->ulDataSize <= %d?!\n",FIRMWARE_MIN_DATA_SIZE));

    FirmwareEntry_p = (FirmwareEntry_t *)m_FWbundle->pData;

    if (UNLIKELY (FirmwareEntry_p->fw_version != FIRMWARE_BUNDLE_VERSION))
    {
        TRC( TRC_ID_ERROR, "Invalid FW Version!!" );
        goto error;
    }
    if (UNLIKELY (FirmwareEntry_p->fw_entry_size != m_FWEntrySize))
    {
        TRC( TRC_ID_ERROR, "Invalid FW Entry size!!" );
        goto error;
    }
    if (UNLIKELY (FirmwareEntry_p->fw_crc != FIRMWARE_BUNDLE_CRC))
    {
        TRC( TRC_ID_ERROR, "Bad FW CRC!!" );
        goto error;
    }

    if (UNLIKELY (FirmwareEntry_p->pData == NULL))
    {
        TRC( TRC_ID_ERROR, "Bad FW Data!!" );
        goto error;
    }

    pDest = (char *)(&m_FWbundle->pData[FIRMWARE_MIN_DATA_SIZE]);
    pNbEntries = FirmwareEntry_p->fw_entries_count;

    m_bIsFWLoaded = true;

    TRC( TRC_ID_MAIN_INFO, "FW Data Loaded at @%p (%d entries found)", pDest, pNbEntries );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;

error:
    vibe_os_release_firmware (m_FWbundle);
    m_bIsFWLoaded = false;
    m_FWbundle = 0;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return false;
}
