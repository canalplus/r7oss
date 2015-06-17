/***********************************************************************
 *
 * File: display/ip/analogsync/stmanalogsync.cpp
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

#include <display/ip/analogsync/stmfwloader.h>

#include "stmanalogsync.h"

CSTmAnalogSync::CSTmAnalogSync (uint32_t        firmwareEntrySize,
                                const char     *firmwarePrefix)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pFwPrefix       = firmwarePrefix;
  m_pFwLoader       = 0;
  m_pFWData         = 0;
  m_FWEntrySize     = firmwareEntrySize;
  m_NbFWEntries     = 0;
  m_bIsFWLoaded     = false;

  m_bUserDefinedValues = false;

  vibe_os_zero_memory(&m_defaultCalibrationValues, sizeof(stm_display_analog_factors_t));

  m_CurrentCalibrationSettings.TimingMode = STM_TIMING_MODE_RESERVED;
  m_CurrentCalibrationSettings.AnalogFactors = m_defaultCalibrationValues;
  m_pCalibrationValues = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTmAnalogSync::~CSTmAnalogSync ()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete m_pFwLoader;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmAnalogSync::Create(void)
{
  bool ret = true;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_pFwPrefix && m_FWEntrySize)
  {
    TRC( TRC_ID_MAIN_INFO, "Creating Fw loader for \"%s\" entry size %d", m_pFwPrefix, m_FWEntrySize );
    m_pFwLoader = new CSTmFwLoader (m_FWEntrySize, m_pFwPrefix);
    if(m_pFwLoader)
    {
      this->LoadFirmware();
    }
    else
    {
      TRC( TRC_ID_ERROR, "Failed to create firmware loader" );
      ret = false;
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return ret;
}


void CSTmAnalogSync::LoadFirmware(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Clear references to any previously loaded firmware data
   */
  m_bIsFWLoaded = false;
  m_pFWData = 0;

  if(m_pFwLoader->CacheFirmwares(m_pFWData, m_NbFWEntries))
  {
    if((m_NbFWEntries == 0) || (m_pFWData == 0))
    {
      TRC( TRC_ID_ERROR, "Unable to load firmware for \"%s\"", m_pFwPrefix );
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "Loaded %d entries from \"%s\" firmware", m_NbFWEntries, m_pFwPrefix );
      m_bIsFWLoaded = true;
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmAnalogSync::SetDefaultScale(uint32_t dacVoltageMV)
{
  /*
   * The validation code is based around an idealized DAC with a voltage
   * swing of 1400mV, where 100% scale is represented by the value 1024.
   */
  uint32_t defaultScale = (dacVoltageMV * 1024) / 1400;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "default scale = %u",defaultScale );

  m_defaultCalibrationValues.Offset_factor_Cb  = 0;
  m_defaultCalibrationValues.Offset_factor_Cr  = 0;
  m_defaultCalibrationValues.Offset_factor_Y   = 0;
  m_defaultCalibrationValues.Scaling_factor_Cb = defaultScale;
  m_defaultCalibrationValues.Scaling_factor_Cr = defaultScale;
  m_defaultCalibrationValues.Scaling_factor_Y  = defaultScale;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmAnalogSync::SetCalibrationValues ( const stm_display_mode_t &mode )
{
    bool     Supported = false;
    uint32_t ModeIndex = 0;
    TRCIN( TRC_ID_MAIN_INFO, "" );

    /*
     * Protect User defined values for the same Mode :
     *   This can happend when calling SetOutputFormat method with same mode
     */
    if(m_bUserDefinedValues && m_CurrentCalibrationSettings.TimingMode == mode.mode_id)
    {
      TRC( TRC_ID_MAIN_INFO, "Using user defined calibration values" );
      return false;
    }

    if(m_bIsFWLoaded)
    {
      for( ModeIndex = 0; ((ModeIndex < m_NbFWEntries) && !Supported); ModeIndex++)
      {
        if(m_pCalibrationValues[ModeIndex].TimingMode == mode.mode_id)
        {
            m_CurrentCalibrationSettings = m_pCalibrationValues[ModeIndex];
            Supported = true;
        }
      }
    }

    if(!Supported)
    {
      /*
       * Fallback to the default calibration setup.
       */
      TRC( TRC_ID_MAIN_INFO, "Using default calibration values" );
      m_CurrentCalibrationSettings.TimingMode = mode.mode_id;
      m_CurrentCalibrationSettings.AnalogFactors = m_defaultCalibrationValues;

      /*
       * Set this Mode as Supported to keep reporting
       * only the user defined protection error
       */
       Supported = true;
    }

    m_bUserDefinedValues = false;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return Supported;
}


bool CSTmAnalogSync::SetCalibrationValues ( const stm_display_mode_t &mode,
                                            const stm_display_analog_factors_t *CalibrationValues)

{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_CurrentCalibrationSettings.TimingMode == STM_TIMING_MODE_RESERVED)
  {
    TRC( TRC_ID_ERROR, "Invalid Timing Mode !!" );
    return false;
  }

  if(m_CurrentCalibrationSettings.TimingMode != mode.mode_id)
    TRC( TRC_ID_MAIN_INFO, "This may not work as Timing Modes are differents !!" );

  m_CurrentCalibrationSettings.TimingMode = mode.mode_id;
  m_CurrentCalibrationSettings.AnalogFactors = *CalibrationValues;
  m_bUserDefinedValues = true;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


bool CSTmAnalogSync::GetCalibrationValues ( stm_display_analog_factors_t *CalibrationValues )
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_CurrentCalibrationSettings.TimingMode == STM_TIMING_MODE_RESERVED)
    return false;

 *CalibrationValues = m_CurrentCalibrationSettings.AnalogFactors;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}
