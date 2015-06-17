/***********************************************************************
 *
 * File: display/ip/analogsync/hdf/stmhdfsync.cpp
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

#include "stmhdfsync.h"

#if defined (CONFIG_DEBUG_HDF_SYNC)
#define DEBUG_HDF_SYNC true
#else
#define DEBUG_HDF_SYNC false
#endif

CSTmHDFSync::CSTmHDFSync (const char *prefix) : CSTmAnalogSync(sizeof(stm_analog_sync_setup_t), prefix)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_pCalibrationValues = 0;
  SetDefaultScale(1400);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CSTmHDFSync::LoadFirmware(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  CSTmAnalogSync::LoadFirmware();

  m_pCalibrationValues = reinterpret_cast<const stm_analog_sync_setup_t *>(m_pFWData);

  if(DEBUG_HDF_SYNC && m_bIsFWLoaded)
  {
    TRC( TRC_ID_MAIN_INFO, "===============================================" );
    TRC( TRC_ID_MAIN_INFO, "Number of Supported Modes = %d.", m_NbFWEntries );
    TRC( TRC_ID_MAIN_INFO, "===============================================" );
    for(uint32_t ModeIndex=0; ModeIndex < m_NbFWEntries; ModeIndex++)
    {

        TRC( TRC_ID_MAIN_INFO, "ModeIndex           = %d.", ModeIndex );
        TRC( TRC_ID_MAIN_INFO, "TimingMode          = 0x%08X", m_pCalibrationValues[ModeIndex].TimingMode );
        TRC( TRC_ID_MAIN_INFO, "Scaling_factor_Cb   = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Scaling_factor_Cb );
        TRC( TRC_ID_MAIN_INFO, "Scaling_factor_Y    = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Scaling_factor_Y );
        TRC( TRC_ID_MAIN_INFO, "Scaling_factor_Cr   = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Scaling_factor_Cr );
        TRC( TRC_ID_MAIN_INFO, "Offset_factor_Cb    = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Offset_factor_Cb );
        TRC( TRC_ID_MAIN_INFO, "Offset_factor_Y     = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Offset_factor_Y );
        TRC( TRC_ID_MAIN_INFO, "Offset_factor_Cr    = %d", m_pCalibrationValues[ModeIndex].AnalogFactors.Offset_factor_Cr );
        TRC( TRC_ID_MAIN_INFO, "-----------------------------------------------" );
    }
    TRC( TRC_ID_MAIN_INFO, "===============================================" );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTmHDFSync::GetHWConfiguration ( const uint32_t uFormat,
                                       HDFParams_t   *pHDFParams)

{
    TRCIN( TRC_ID_MAIN_INFO, "" );

    vibe_os_zero_memory(pHDFParams, sizeof(HDFParams_t));

    /* call Firmware and retrieve the AWG and HDF update in case of HD and Denc dac mult in case of SD. */
    int RetOk;
    if((uFormat & STM_VIDEO_OUT_RGB) == STM_VIDEO_OUT_RGB)
        RetOk = Ana_GenerateAwgCodeRGB(&m_CurrentCalibrationSettings , pHDFParams);
    else
        RetOk = Ana_GenerateAwgCodeYUV(&m_CurrentCalibrationSettings , pHDFParams);

    if(RetOk == 0)
    {
      if (DEBUG_HDF_SYNC)
    {
        TRC( TRC_ID_MAIN_INFO, "ANA_SCALE_CTRL_DAC_Y  = 0x%.8x", pHDFParams->ANA_SCALE_CTRL_DAC_Y );
        TRC( TRC_ID_MAIN_INFO, "ANA_SCALE_CTRL_DAC_Cb = 0x%.8x", pHDFParams->ANA_SCALE_CTRL_DAC_Cb );
        TRC( TRC_ID_MAIN_INFO, "ANA_SCALE_CTRL_DAC_Cr = 0x%.8x", pHDFParams->ANA_SCALE_CTRL_DAC_Cr );
    }
    }
    else
      TRC( TRC_ID_MAIN_INFO, "Failed to get HDF sync parameters (%d)",RetOk );

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return (RetOk == 0);
}
