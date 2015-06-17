/***********************************************************************
 *
 * File: display/ip/analogsync/stmanalogsync.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_ANALOG_SYNC_H
#define _STM_ANALOG_SYNC_H

#include <display/ip/analogsync/stmanalogsynctypes.h>

class CDisplayDevice;
class CSTmFwLoader;

class CSTmAnalogSync
{
public:
    CSTmAnalogSync ( uint32_t        firmwareEntrySize,
                     const char     *firmwarePrefix);

    virtual ~CSTmAnalogSync (void);

    virtual bool Create(void);
    virtual void LoadFirmware(void);

    bool SetCalibrationValues   ( const stm_display_mode_t &mode );
    bool SetCalibrationValues   ( const stm_display_mode_t &mode,
                                  const stm_display_analog_factors_t *CalibrationValues);
    bool GetCalibrationValues   ( stm_display_analog_factors_t *CalibrationValues);

protected:
    const char                     *m_pFwPrefix;
    CSTmFwLoader                   *m_pFwLoader;
    const char                     *m_pFWData;
    uint32_t                        m_FWEntrySize;
    uint16_t                        m_NbFWEntries;
    bool                            m_bIsFWLoaded;

    bool                            m_bUserDefinedValues;
    stm_display_analog_factors_t    m_defaultCalibrationValues;
    stm_analog_sync_setup_t         m_CurrentCalibrationSettings;
    const stm_analog_sync_setup_t  *m_pCalibrationValues;

    void SetDefaultScale(uint32_t dacVoltageMV);

private:
    CSTmAnalogSync(const CSTmAnalogSync&);
    CSTmAnalogSync& operator=(const CSTmAnalogSync&);
};

#endif /* _STM_ANALOG_SYNC_H */
