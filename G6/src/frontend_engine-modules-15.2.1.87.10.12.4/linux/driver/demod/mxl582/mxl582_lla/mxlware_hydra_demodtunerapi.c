
/*
* Copyright (c) 2011-2013 MaxLinear, Inc. All rights reserved
*
* License type: GPLv2
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*
* This program may alternatively be licensed under a proprietary license from
* MaxLinear, Inc.
*
* See terms and conditions defined in file 'LICENSE.txt', which is part of this
* source code package.
*/


#include "mxlware_hydra_oem_drv.h"
#include "mxlware_hydra_commonapi.h"
#include "mxlware_hydra_phyctrl.h"
#include "mxlware_hydra_registers.h"
#include "mxlware_hydra_commands.h"

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgTunerEnable
 *
 * @param[in]   devId        Device ID
 * @param[in]   tunerId
 * @param[in]   enable
 *
 * @author Mahee
 *
 * @date 11/14/2012 Initial release
 *
 * This API should be used to enable or disable tuner.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgTunerEnable(UINT8 devId, MXL_HYDRA_TUNER_ID_E tunerId, MXL_BOOL_E enable)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;
  MxL_HYDRA_TUNER_CMD ctrlTunerCmd;
  UINT8 cmdSize = sizeof(ctrlTunerCmd);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  
  MXLENTERAPI(MXL_HYDRA_PRINT("tunerId = %d %d\n", tunerId, devHandlePtr->deviceType));
  if (mxlStatus == MXL_SUCCESS)
  {
    if (tunerId < devHandlePtr->features.tunersCnt)
    {
      ctrlTunerCmd.tunerId = tunerId;
      ctrlTunerCmd.enable = enable;
      BUILD_HYDRA_CMD(MXL_HYDRA_TUNER_ACTIVATE_CMD, MXL_CMD_WRITE, cmdSize, &ctrlTunerCmd, cmdBuff);

      // send command to device
      mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
    } else mxlStatus = MXL_INVALID_PARAMETER;      

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodAbortTune
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId   
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release 
 * 
 * This API should be used to abort tune.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodAbortTune(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;
  MXL_HYDRA_DEMOD_ABORT_TUNE_T abortTuneCmd;
  UINT8 cmdSize = sizeof(abortTuneCmd);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    // build abort tune command
    abortTuneCmd.demodId = demodId;
    BUILD_HYDRA_CMD(MXL_HYDRA_ABORT_TUNE_CMD, MXL_CMD_WRITE, cmdSize, &abortTuneCmd, cmdBuff);

    // send command to device
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodDisable
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to disable Hydra Demod module.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodDisable(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("DemodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    // Abort tune before disabling demod
    mxlStatus = MxLWare_HYDRA_API_CfgDemodAbortTune(devId, demodId);
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodChanTune
 *
 * @param[in]   devId        Device ID
 * @param[in]   tunerId      Tuner ID
 * @param[in]   demodId    Demod ID
 * @param[in]   chanTuneParamsPtr   Channel tune parameters
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used when tuning for a channel.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodChanTune(UINT8 devId, MXL_HYDRA_TUNER_ID_E tunerId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_TUNE_PARAMS_T * chanTuneParamsPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus = (UINT8)MXL_SUCCESS;
  MXL_HYDRA_DEMOD_PARAM_T demodChanCfg;
  UINT8 cmdSize = sizeof(demodChanCfg);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  // Enable DSS input register address
  MXL_REG_FIELD_T xpt_enable_dss_input[MXL_HYDRA_DEMOD_MAX] = {
                                                          {XPT_INP_MODE_DSS0}, 
                                                          {XPT_INP_MODE_DSS1},
                                                          {XPT_INP_MODE_DSS2}, 
                                                          {XPT_INP_MODE_DSS3},
                                                          {XPT_INP_MODE_DSS4}, 
                                                          {XPT_INP_MODE_DSS5},
                                                          {XPT_INP_MODE_DSS6}, 
                                                          {XPT_INP_MODE_DSS7} };

  // Enable DVB input register
  MXL_REG_FIELD_T xpt_enable_dvb_input[MXL_HYDRA_DEMOD_MAX] = {
                                                               {XPT_ENABLE_INPUT0},
                                                               {XPT_ENABLE_INPUT1},
                                                               {XPT_ENABLE_INPUT2},
                                                               {XPT_ENABLE_INPUT3},
                                                               {XPT_ENABLE_INPUT4},
                                                               {XPT_ENABLE_INPUT5},
                                                               {XPT_ENABLE_INPUT6},
                                                               {XPT_ENABLE_INPUT7} };

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
 // printk("%s status. =%d\n",__func__,mxlStatus);
  if (mxlStatus == MXL_SUCCESS)
  {

    if (chanTuneParamsPtr)
    {
      // Validate input parameters
      switch (chanTuneParamsPtr->standardMask)
      {

        case MXL_HYDRA_DSS:
          demodChanCfg.fecCodeRate = chanTuneParamsPtr->params.paramsDSS.fec;
          if (chanTuneParamsPtr->params.paramsDSS.rollOff > MXL_HYDRA_ROLLOFF_0_35)
          {
            mxlStatus |= MXL_INVALID_PARAMETER;
          }
          break;

        case MXL_HYDRA_DVBS:

          // Validate rolloff
          if (chanTuneParamsPtr->params.paramsS.rollOff > MXL_HYDRA_ROLLOFF_0_35)
          {
            mxlStatus |= MXL_INVALID_PARAMETER;
          }

          // Validate modulation
		  if ((chanTuneParamsPtr->params.paramsS.modulation != MXL_HYDRA_MOD_QPSK) && \
				(chanTuneParamsPtr->params.paramsS.modulation != MXL_HYDRA_MOD_AUTO))
          {
            mxlStatus |= MXL_INVALID_PARAMETER;
          }
		  else if (chanTuneParamsPtr->params.paramsS.modulation == MXL_HYDRA_MOD_AUTO) 
          {
            chanTuneParamsPtr->params.paramsS.modulation = MXL_HYDRA_MOD_QPSK;
          }
          break;

        case MXL_HYDRA_DVBS2:
          // Validate rolloff
          if (chanTuneParamsPtr->params.paramsS2.rollOff > MXL_HYDRA_ROLLOFF_0_35)
          {
            mxlStatus |= MXL_INVALID_PARAMETER;
          }

          // Validate modulation
          if (chanTuneParamsPtr->params.paramsS2.modulation > MXL_HYDRA_MOD_8PSK)
          {
            mxlStatus |= MXL_INVALID_PARAMETER;
          }

          break;

        default:
          mxlStatus |= MXL_INVALID_PARAMETER;

      }

    }
    else
      mxlStatus |= MXL_INVALID_PARAMETER;


    if ((mxlStatus == MXL_SUCCESS) && (chanTuneParamsPtr))
    {
      // Abort tune first
      //MxLWare_HYDRA_API_CfgDemodAbortTune(devId, demodId);

      // Configure channel settings & perform chan tune
      demodChanCfg.tunerIndex = (UINT32)tunerId;
      demodChanCfg.demodIndex = (UINT32)demodId;
      demodChanCfg.frequencyInHz = chanTuneParamsPtr->frequencyInHz;
      demodChanCfg.symbolRateInHz = (chanTuneParamsPtr->symbolRateKSps * 1000);

      if ((chanTuneParamsPtr->freqSearchRangeKHz%1000))
        chanTuneParamsPtr->freqSearchRangeKHz = (chanTuneParamsPtr->freqSearchRangeKHz/1000) + 1;
      else
        chanTuneParamsPtr->freqSearchRangeKHz = (chanTuneParamsPtr->freqSearchRangeKHz)/1000;

      demodChanCfg.maxCarrierOffsetInMHz = (UINT32)(chanTuneParamsPtr->freqSearchRangeKHz);
      demodChanCfg.spectrumInversion = (UINT32)chanTuneParamsPtr->spectrumInfo;
      demodChanCfg.standard = (UINT32)chanTuneParamsPtr->standardMask;

      // update device context
      devHandlePtr->bcastStd[demodId] = chanTuneParamsPtr->standardMask;

      MXL_HYDRA_PRINT(" tunerIndex : %d\n", demodChanCfg.tunerIndex);
      MXL_HYDRA_PRINT(" demodIndex : %d\n", demodChanCfg.demodIndex);
      MXL_HYDRA_PRINT(" frequencyInHz : %d\n", demodChanCfg.frequencyInHz);
      MXL_HYDRA_PRINT(" symbolRateInHz : %d\n", demodChanCfg.symbolRateInHz);
      MXL_HYDRA_PRINT(" maxCarrierOffsetInMHz : %d\n", demodChanCfg.maxCarrierOffsetInMHz);
      MXL_HYDRA_PRINT(" spectrumInversion : %d\n", demodChanCfg.spectrumInversion);


      // get standard specific tunning parameters
      switch (chanTuneParamsPtr->standardMask)
      {
        case MXL_HYDRA_DSS:
          demodChanCfg.fecCodeRate = chanTuneParamsPtr->params.paramsDSS.fec;
		  demodChanCfg.rollOff = chanTuneParamsPtr->params.paramsDSS.rollOff; 
          MXL_HYDRA_PRINT(" fecCodeRate : %d\n", demodChanCfg.fecCodeRate);
		  MXL_HYDRA_PRINT(" rollOff : %d\n", demodChanCfg.rollOff);  
          break;

        case MXL_HYDRA_DVBS:
          demodChanCfg.rollOff = chanTuneParamsPtr->params.paramsS.rollOff;
          demodChanCfg.modulationScheme = chanTuneParamsPtr->params.paramsS.modulation;
          demodChanCfg.fecCodeRate = chanTuneParamsPtr->params.paramsS.fec;

          MXL_HYDRA_PRINT(" fecCodeRate : %d\n", demodChanCfg.fecCodeRate);
          MXL_HYDRA_PRINT(" modulationScheme : %d\n", demodChanCfg.modulationScheme);
          MXL_HYDRA_PRINT(" rollOff : %d\n", demodChanCfg.rollOff);
          break;

        case MXL_HYDRA_DVBS2:
          demodChanCfg.rollOff = chanTuneParamsPtr->params.paramsS2.rollOff;
          demodChanCfg.modulationScheme = chanTuneParamsPtr->params.paramsS2.modulation;
          demodChanCfg.fecCodeRate = chanTuneParamsPtr->params.paramsS2.fec;
          demodChanCfg.pilots = chanTuneParamsPtr->params.paramsS2.pilots;

          MXL_HYDRA_PRINT(" fecCodeRate : %d\n", demodChanCfg.fecCodeRate);
          MXL_HYDRA_PRINT(" modulationScheme : %d\n", demodChanCfg.modulationScheme);
          MXL_HYDRA_PRINT(" rollOff : %d\n", demodChanCfg.rollOff);

          break;

        default:
          mxlStatus |= MXL_INVALID_PARAMETER;

      }
      demodId = (MXL_HYDRA_DEMOD_ID_E)MxL_Ctrl_GetTsID(devHandlePtr, demodId);

      // Enable DVB input to TS block
      mxlStatus |= MxLWare_Hydra_UpdateByMnemonic(devId,
                                                xpt_enable_dvb_input[demodId].regAddr,
                                                xpt_enable_dvb_input[demodId].lsbPos,
                                                xpt_enable_dvb_input[demodId].numOfBits,
                                                MXL_TRUE);

      // Enable or Disable TS block input type based on broadcasting standard
      if (chanTuneParamsPtr->standardMask == MXL_HYDRA_DSS)
      {
        // Enable DSS input to TS block
        mxlStatus |= MxLWare_Hydra_UpdateByMnemonic(devId,
                                                xpt_enable_dss_input[demodId].regAddr,
                                                xpt_enable_dss_input[demodId].lsbPos,
                                                xpt_enable_dss_input[demodId].numOfBits,
                                                MXL_TRUE);
      }
      else
      {
        // Disable DSS input to TS block
        mxlStatus |= MxLWare_Hydra_UpdateByMnemonic(devId,
                                                xpt_enable_dss_input[demodId].regAddr,
                                                xpt_enable_dss_input[demodId].lsbPos,
                                                xpt_enable_dss_input[demodId].numOfBits,
                                                MXL_FALSE);
      }

      if (mxlStatus == MXL_SUCCESS)
      {
        // send command to the device
        BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_SET_PARAM_CMD, MXL_CMD_WRITE, cmdSize, &demodChanCfg, cmdBuff);
        mxlStatus |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
      }

    }

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodConstellationSource
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId   Demod ID
 * @param[in]   source      Constellation source
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to set source of constellation data retrieved
 * by API function MxLWare_HYDRA_ReqDemodConstellationData.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodConstellationSource(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_CONSTELLATION_SRC_E source)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus;
  MXL_HYDRA_DEMOD_IQ_SRC_T iqSrc;
  UINT8 cmdSize = sizeof(iqSrc);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d, source=%d\n", demodId, source););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    // build FW command
    iqSrc.demodId = demodId;
    iqSrc.sourceOfIQ = source;

    BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_SET_IQ_SOURCE_CMD, MXL_CMD_WRITE, cmdSize, &iqSrc, cmdBuff);

    // send command to device
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

#ifdef MXL_ENABLE_SPECTRUM_RB
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqTunerPowerSpectrum
 *
 * @param[in]   devId        Device ID
 * @param[in]   tunerId   
 * @param[in]   demodId   
 * @param[in]   freqStartInKHz   
 * @param[in]   numOfFreqSteps 
 * @param[out]  powerBufPtr 
 * @param[out]  spectrumReadbackStatusPtr 
 *
 * @author Mahee
 * 
 * @date 06/12/2012 Initial release 
 * 
 * This API retrieves power spectrum data in specified frequency range 
 * for a specified tuner.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqTunerPowerSpectrum(UINT8 devId, 
                                                      MXL_HYDRA_TUNER_ID_E tunerId, 
                                                      MXL_HYDRA_DEMOD_ID_E demodId, 
                                                      UINT32 freqStartInKHz, 
                                                      UINT32 numOfFreqSteps, 
                                                      SINT16 * powerBufPtr,
                                                      MXL_HYDRA_SPECTRUM_ERROR_CODE_E *spectrumReadbackStatusPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 status = (MXL_STATUS_E)MXL_SUCCESS;
  UINT32 regData = 0;
  UINT64 startTime = 0;
  UINT64 currTime = 0;
  MXL_BOOL_E spectrumReadyFlag = MXL_FALSE;
  UINT32 spectrumDataAddr[2];
  SINT16 spectrumData[MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH/sizeof(SINT16)];
  UINT32 startFreq = 0;
  UINT32 endFreq = 0;
  UINT32 totalSize = 0;
  UINT32 tmpNumOfSteps = 0;
  UINT32 index = 0;
  UINT32 numOfBinsInBytes = 0;

  UINT16 maxNumOfSteps[] = { MAX_STEP_SIZE_24_XTAL_102_05_KHZ,
                             MAX_STEP_SIZE_24_XTAL_204_10_KHZ,
                             MAX_STEP_SIZE_24_XTAL_306_15_KHZ,
                             MAX_STEP_SIZE_24_XTAL_408_20_KHZ,
                             MAX_STEP_SIZE_27_XTAL_102_05_KHZ,
                             MAX_STEP_SIZE_27_XTAL_204_10_KHZ,
                             MAX_STEP_SIZE_27_XTAL_306_15_KHZ,
                             MAX_STEP_SIZE_27_XTAL_408_20_KHZ};

  UINT32 freqStep[] = { 102050, 204100, 306150, 408200,
                         102050, 204100, 306150, 408200};

  MXL_HYDRA_SPECTRUM_REQ_T pwrSpectrum;
  MXL_HYDRA_SPECTRUM_REQ_T minAgc;

  UINT8 cmdSize = sizeof(pwrSpectrum);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("tunerId=%d, freqStartInKHz=%d, freqStepInHz=%d, numOfFreqSteps=%d\n", tunerId, freqStartInKHz, freqStep[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ], numOfFreqSteps););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    // validate input parameter
    // validate spectrum step size - 24 XTAL is supported for now 01/03/2013
    if ((powerBufPtr) && (spectrumReadbackStatusPtr) && (freqStartInKHz >= MXL_HYDRA_SPECTRUM_MIN_FREQ_KHZ) \
		&& (freqStartInKHz*1000UL + numOfFreqSteps*freqStep[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ] <= MXL_HYDRA_SPECTRUM_MAX_FREQ_KHZ*1000UL) \
		&& (maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ]*sizeof(SINT16) <= MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH))
      {
		  // send command to calculate min AGC 
		  minAgc.demodIndex = demodId;
		  minAgc.tunerIndex = tunerId;
		  minAgc.startingFreqInkHz = freqStartInKHz;
		  minAgc.stepSizeInKHz = MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ;
		  minAgc.totalSteps = numOfFreqSteps;
      
		  // build command
		  BUILD_HYDRA_CMD(MXL_HYDRA_TUNER_SPECTRUM_MIN_GAIN_CMD, MXL_CMD_WRITE, cmdSize, &minAgc, cmdBuff);
		  // send command to device
      
		  status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

      MxLWare_HYDRA_OEM_GetCurrTimeInMs(&startTime);

      do
      {
        // check status for min agc command -DMD0_SPECTRUM_MIN_GAIN_STATUS
        status |= MxLWare_HYDRA_ReadRegister(devId,
                          (DMD0_SPECTRUM_MIN_GAIN_STATUS + HYDRA_DMD_STATUS_OFFSET(demodId)),
                          &regData);

        MXL_HYDRA_PRINT("Spectrum Status RegAddr = 0x%08X, Data = 0x%08X", (DMD0_SPECTRUM_MIN_GAIN_STATUS + HYDRA_DMD_STATUS_OFFSET(demodId)), regData);

        MxLWare_HYDRA_OEM_GetCurrTimeInMs(&currTime);

      } while (((currTime - startTime) < 20000 /*  timeout of 20 secs */) && ((regData & 0xFFFFFFFF) == 0x0));

      if (((regData & 0xFFFF) == 0x1) || ((regData & 0xFFFF) == 0x2))
        status = MXL_INVALID_PARAMETER;
		  else if (((regData & 0xFFFF) == 0x3) || ((regData & 0xFFFF) == 0x4) || ((regData & 0xFFFF) == 0x5))
        status = MXL_FAILURE;

      *spectrumReadbackStatusPtr = (MXL_HYDRA_SPECTRUM_ERROR_CODE_E)(regData & 0xFFFF);

      // if regData <31:16> = 1 then spectrm agc data is ready
      if ((status == MXL_SUCCESS) && ((regData & 0xFFFF0000) == 0x10000))
      {
        // Calculate end frequency
			endFreq = (freqStartInKHz * 1000) + (freqStep[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ] * numOfFreqSteps); 
        tmpNumOfSteps = numOfFreqSteps;

        // loop entire spectrum
			for (startFreq = (freqStartInKHz * 1000);  startFreq < endFreq; startFreq += (freqStep[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ] * maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ]))
        {
          tmpNumOfSteps -= totalSize;
          index += totalSize;

			  // if num of steps is not less than max allowed then step size should be valid max num of steps
			  if (tmpNumOfSteps < maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ]) totalSize = tmpNumOfSteps;
			  else totalSize = maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ];

          if (status == MXL_SUCCESS)
          {
            // Delay of 50 msecs
            MxLWare_HYDRA_OEM_SleepInMs(50);

            // build FW command
            pwrSpectrum.demodIndex = demodId;
            pwrSpectrum.tunerIndex = tunerId;
				pwrSpectrum.stepSizeInKHz = MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ;
            pwrSpectrum.totalSteps = totalSize;
            pwrSpectrum.startingFreqInkHz = (UINT32)(startFreq/1000);

				MXL_HYDRA_PRINT("tunerId = %d, demodId = %d freqStartInKHz = %d, numOfFreqSteps = %d\n", 
											  tunerId, demodId, 
											  pwrSpectrum.startingFreqInkHz, 
											  pwrSpectrum.totalSteps);

            // build command
				BUILD_HYDRA_CMD(MXL_HYDRA_TUNER_SPECTRUM_REQ_CMD, MXL_CMD_WRITE, cmdSize, &pwrSpectrum, cmdBuff);

				// send command to device
				status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
          }

          // if command is sent successfully to device
          if (status == MXL_SUCCESS)
          {
            // Get current timestamp
            MxLWare_HYDRA_OEM_GetCurrTimeInMs(&startTime);

            do
            {
              // Read status of the spectrum readback command
              status |= MxLWare_HYDRA_ReadRegister(devId,
                                      (HYDRA_TUNER_SPECTRUM_STATUS_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)),
                                      &regData);

              MXL_HYDRA_PRINT("Spectrum Status RegAddr = 0x%08X, Data = 0x%08X", (HYDRA_TUNER_SPECTRUM_STATUS_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)), regData);

              spectrumReadyFlag = MXL_FALSE;

              if (((regData & 0xFFFF) == 0x1) || ((regData & 0xFFFF) == 0x2))
              {
                status = MXL_INVALID_PARAMETER;
              }
				  else if (((regData & 0xFFFF) == 0x3) || ((regData & 0xFFFF) == 0x4) || ((regData & 0xFFFF) == 0x5))
              {
                status = MXL_FAILURE;
              }
              else
                spectrumReadyFlag = MXL_TRUE;

              *spectrumReadbackStatusPtr = (MXL_HYDRA_SPECTRUM_ERROR_CODE_E)(regData & 0xFFFF);

              // if regData <31:16> = 1 then spectrm data is ready
              if ((status == MXL_SUCCESS) && ((regData & 0xFFFF0000) == 0x10000) && (spectrumReadyFlag == MXL_TRUE))
              {

                // Get number of bins to read data from and readback data buffer address
                status |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                          (HYDRA_TUNER_SPECTRUM_BIN_SIZE_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)),
                                          (2 * 4), // 2 * 4 bytes - 2 32-bit registers
                                          (UINT8 *)&spectrumDataAddr[0]);

                MXL_HYDRA_PRINT("RegAddr = 0x%08X, Data = 0x%08X Data = 0x%08X\n",
                                      (HYDRA_TUNER_SPECTRUM_BIN_SIZE_OFFSET + HYDRA_TUNER_STATUS_OFFSET(tunerId)),
                                      spectrumDataAddr[0], // Num of bins to read
                                      spectrumDataAddr[1]); // Address

                // # bins should be greater than zero
                if (spectrumDataAddr[1])
                {
                  // validate num of bins - if the bin size is greater than MAX then limit the bin
					  // size to MAX
					  if (spectrumDataAddr[0] > maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ])
						spectrumDataAddr[0] = maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ];
                
                  // Calculate num of bins in bytes
                  numOfBinsInBytes = (spectrumDataAddr[0] * 2);

                  // Num of Bytes to read should be multiple of 4
                  if (numOfBinsInBytes % 4) numOfBinsInBytes += (numOfBinsInBytes % 4);

                  // Read Spectrum data
                  status |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                                      (spectrumDataAddr[1]),  // starting address
                                                      (numOfBinsInBytes), // # of bins * 2 bytes
                                                      (UINT8 *)&spectrumData[0]);

                  MXL_HYDRA_PRINT("Spectrum Data - Start\n");

                  // each spectrum value is 16 bits so the loop increment is +2
                  for (regData = 0; regData < totalSize; regData += 2)
                  {
                    powerBufPtr[index+regData] = spectrumData[regData+1];
                    MXL_HYDRA_PRINT("%d    %d", (regData), powerBufPtr[index+regData]);

                    // check is the last reacod should be read or not
                    // need this check because we read number of bytes as a factor of 4
                    // due to 32-bit register read. We always round number of bytes to read to the next highest factor of 4.
                    if ((regData+1) < totalSize)
                    {
                      powerBufPtr[index+(regData+1)] = spectrumData[regData];
                      MXL_HYDRA_PRINT("%d    %d", (regData+1), powerBufPtr[index+(regData+1)]);
                    }

                  }

                  MXL_HYDRA_PRINT("Spectrum Data - Done\n");

                }
                else
                {
                  spectrumReadyFlag = MXL_FALSE;
                }

              }
              else
              {
                spectrumReadyFlag = MXL_FALSE;
              }

              MxLWare_HYDRA_OEM_GetCurrTimeInMs(&currTime);

              // loop with timeout value of 500 ms & spectrum data is not ready
            } while (((currTime - startTime) < 500 /*  timeout of 500 ms */) && (spectrumReadyFlag == MXL_FALSE));

          }

        }
      }
      
	  }
      else 
      {
		  if (maxNumOfSteps[MXL_HYDRA_STEP_SIZE_24_XTAL_408_20KHZ]*sizeof(SINT16) > MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH)
		  {
		    MXL_HYDRA_PRINT("Cannot be executed due to memory limitation. Increase MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH if possible\n");
		  }
		  else
		  {
			MXL_HYDRA_PRINT(" Invalid Parameter!");
		  }
		  status = MXL_INVALID_PARAMETER;
      }

  }
  else
  {
    MXL_HYDRA_PRINT("Status = %d", status);
  }

  MXLEXITAPISTR(devId, status);

  return (MXL_STATUS_E)status;
}

#endif //MXL_ENABLE_SPECTRUM_RB
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodConstellationData
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  iSamplePtr
 * @param[out]  qSamplePtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to retrieve constellation data from given demodulator.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodConstellationData(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, SINT16 *iSamplePtr, SINT16 *qSamplePtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if ((mxlStatus == MXL_SUCCESS) &&((iSamplePtr) && (qSamplePtr)))
  {

    // Lock demod status
    mxlStatus = HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

    mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, 
                                           (HYDRA_DMD_DISPLAY_I_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                           &regData);

    *iSamplePtr = (SINT16)((regData >> 16) & 0xFFFF);
    *qSamplePtr = (SINT16)(regData & 0xFFFF);

    // Unlock demod status
    mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

  }
  else
    mxlStatus = MXL_INVALID_PARAMETER;

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodLockStatus
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  demodLockPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used retrieve demod lock information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodLockStatus(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_DEMOD_LOCK_T *demodLockPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (demodLockPtr)
    {
      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

      // read demod lock status
      mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, 
                                             (HYDRA_DMD_LOCK_STATUS_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                             &regData);
      // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

      demodLockPtr->fecLock = (regData == 1) ? MXL_TRUE : MXL_FALSE;

      if (demodLockPtr->fecLock == MXL_TRUE)  MXL_HYDRA_PRINT("Demod Lock");
      else MXL_HYDRA_PRINT("Demod UNLOCK");

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodSNR
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  snrPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used retrieve demod SNR (Signal to Noise Ratio)
 * value in 100x dB.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodSNR(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, SINT16 *snrPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (snrPtr)
    {

    // Lock demod status
    mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

    // read SNR from device
    mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, 
                                           (HYDRA_DMD_SNR_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                           &regData);
    // Unlock demod status
    mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

    *snrPtr = (SINT16)(regData & 0xFFFF);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodChanParams
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  demodTuneParamsPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used retrieve demod channel signal information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodChanParams(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_TUNE_PARAMS_T *demodTuneParamsPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus;

  UINT32 regData[MXL_DEMOD_CHAN_PARAMS_BUFF_SIZE];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (demodTuneParamsPtr)
    {
      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

      // read demod channel parameters
      mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                                  (HYDRA_DMD_STANDARD_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                                  (MXL_DEMOD_CHAN_PARAMS_BUFF_SIZE * 4), // 25 * 4 bytes
                                                  (UINT8 *)&regData[0]);

      demodTuneParamsPtr->standardMask = (MXL_HYDRA_BCAST_STD_E )regData[DMD_STANDARD_ADDR];
      demodTuneParamsPtr->spectrumInfo = (MXL_HYDRA_SPECTRUM_E )regData[DMD_SPECTRUM_INVERSION_ADDR];
      demodTuneParamsPtr->symbolRateKSps = (regData[DMD_SYMBOL_RATE_ADDR]/1000);
      demodTuneParamsPtr->freqSearchRangeKHz = (regData[DMD_FREQ_SEARCH_RANGE_IN_KHZ_ADDR]);

      switch(demodTuneParamsPtr->standardMask)
      {
        case MXL_HYDRA_DSS:
          demodTuneParamsPtr->params.paramsDSS.fec = (MXL_HYDRA_FEC_E)regData[DMD_FEC_CODE_RATE_ADDR];
          break;

        case MXL_HYDRA_DVBS:
          demodTuneParamsPtr->params.paramsS.fec = (MXL_HYDRA_FEC_E)regData[DMD_FEC_CODE_RATE_ADDR];
          demodTuneParamsPtr->params.paramsS.modulation = (MXL_HYDRA_MODULATION_E )regData[DMD_MODULATION_SCHEME_ADDR];
          demodTuneParamsPtr->params.paramsS.rollOff = (MXL_HYDRA_ROLLOFF_E)regData[DMD_SPECTRUM_ROLL_OFF_ADDR];
          break;

        case MXL_HYDRA_DVBS2:
          demodTuneParamsPtr->params.paramsS2.fec = (MXL_HYDRA_FEC_E)regData[DMD_FEC_CODE_RATE_ADDR];
          demodTuneParamsPtr->params.paramsS2.modulation = (MXL_HYDRA_MODULATION_E )regData[DMD_MODULATION_SCHEME_ADDR];
          demodTuneParamsPtr->params.paramsS2.pilots = (MXL_HYDRA_PILOTS_E )regData[DMD_DVBS2_PILOT_ON_OFF_ADDR];
          demodTuneParamsPtr->params.paramsS2.rollOff = (MXL_HYDRA_ROLLOFF_E)regData[DMD_SPECTRUM_ROLL_OFF_ADDR];
          break;
      }

      // read demod channel parameters
      mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                                  (HYDRA_DMD_STATUS_CENTER_FREQ_IN_KHZ_ADDR + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                                  (4), // 4 bytes
                                                  (UINT8 *)&regData[0]);

      demodTuneParamsPtr->frequencyInHz = (regData[0] * 1000);

      // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodRxPowerLevel
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  inputPwrLevelPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to read RF input power in 10x dBm units of a
 * specific demod.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodRxPowerLevel(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, SINT32 *inputPwrLevelPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (inputPwrLevelPtr)
    {
      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

      // lock tuner status
      mxlStatus |= MxLWare_HYDRA_ReadRegister(devId,
                                        (HYDRA_DMD_STATUS_INPUT_POWER_ADDR + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                        &regData);

      // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

      *inputPwrLevelPtr = (SINT16)(regData & 0xFFFF);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodErrorCounters
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  demodStatusPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to retrieve Demod signal information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodErrorCounters(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_DEMOD_STATUS_T *demodStatusPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus;
  UINT32 regData[8];
  UINT32 bcastStd = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {

    if (demodStatusPtr)
    {

      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

      // read demod error stats
      mxlStatus |= MxLWare_HYDRA_ReadRegister(devId,
                                        (HYDRA_DMD_STANDARD_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                        &bcastStd);

      demodStatusPtr->standardMask = (MXL_HYDRA_BCAST_STD_E )bcastStd;

      mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                                  (HYDRA_DMD_DVBS2_CRC_ERRORS_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                                  (7 * sizeof(UINT32)),
                                                  (UINT8 *)&regData[0]);

      MXL_HYDRA_PRINT("Standard  = %d\n", demodStatusPtr->standardMask);

      switch(demodStatusPtr->standardMask)
      {
        case MXL_HYDRA_DSS:
          demodStatusPtr->u.demodStatus_DSS.corrRsErrors = regData[3]; // uncorrected RS errors
          demodStatusPtr->u.demodStatus_DSS.rsErrors = regData[4]; // uncorrected RS errors
          demodStatusPtr->u.demodStatus_DSS.berCount = regData[5];
          demodStatusPtr->u.demodStatus_DSS.berWindow = regData[6];
          mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId, 
                                                  (HYDRA_DMD_DVBS_1ST_CORR_RS_ERRORS_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                                  (4 * sizeof(UINT32)),
                                                  (UINT8 *)&regData[0]);
		  demodStatusPtr->u.demodStatus_DSS.corrRsErrors1 = regData[0];
          demodStatusPtr->u.demodStatus_DSS.rsErrors1 = regData[1];
          demodStatusPtr->u.demodStatus_DSS.berCount_Iter1 = regData[2];
          demodStatusPtr->u.demodStatus_DSS.berWindow_Iter1 = regData[3];
          break;

        case MXL_HYDRA_DVBS:
          demodStatusPtr->u.demodStatus_DVBS.corrRsErrors = regData[3]; // uncorrected RS errors
          demodStatusPtr->u.demodStatus_DVBS.rsErrors = regData[4]; // uncorrected RS errors
          demodStatusPtr->u.demodStatus_DVBS.berCount = regData[5];
          demodStatusPtr->u.demodStatus_DVBS.berWindow = regData[6];

          // for DVB-S read 1st iter BER values
          mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId, 
                                                  (HYDRA_DMD_DVBS_1ST_CORR_RS_ERRORS_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                                  (4 * sizeof(UINT32)),
                                                  (UINT8 *)&regData[0]);

		  demodStatusPtr->u.demodStatus_DVBS.corrRsErrors1 = regData[0];
          demodStatusPtr->u.demodStatus_DVBS.rsErrors1 = regData[1];
          demodStatusPtr->u.demodStatus_DVBS.berCount_Iter1 = regData[2];
          demodStatusPtr->u.demodStatus_DVBS.berWindow_Iter1 = regData[3];

          break;

        case MXL_HYDRA_DVBS2:
          demodStatusPtr->u.demodStatus_DVBS2.crcErrors = regData[0];
          demodStatusPtr->u.demodStatus_DVBS2.packetErrorCount = regData[1];
          demodStatusPtr->u.demodStatus_DVBS2.totalPackets = regData[2]; 

          break;
      }

      // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodScaledBER
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId   
 * @param[out]  
 *
 * @author Sateesh
 *
 * @date 06/12/2012 Initial release 
 * 
 * This API shall be used to retrieve Demod signal information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodScaledBER(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_DEMOD_SCALED_BER_T *scaledBer)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus;
  UINT32 regData[2];
  UINT32 bcastStd = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {

    if (scaledBer)
    {
      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

	  // read bcast standard
      mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, 
                                        (HYDRA_DMD_STANDARD_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                        &bcastStd);

	  if (( bcastStd == MXL_HYDRA_DSS) || ( bcastStd == MXL_HYDRA_DVBS))
	  {
		  mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId, 
													  (DMD0_STATUS_DVBS_1ST_SCALED_BER_COUNT_ADDR + HYDRA_DMD_STATUS_OFFSET(demodId)),
													  (2 * sizeof(UINT32)),
													  (UINT8 *)&regData[0]);

		  scaledBer->scaledBerIter1 = regData[0];
		  scaledBer->scaledBerIter2 = regData[1];
	  }

	  // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);
        
    } 
    else 
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqAdcRssiPower
 *
 * @param[in]   devId			Device ID
 * @param[in]   tunerId			Tuner ID
 * @param[out]  adcRssiPwr      ADC RSSI Power   
 *
 * @author Sateesh
 *
 * @date 10/21/2013 Initial release 
 * 
 * This API should be used to read the ADC RSSI power.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqAdcRssiPower(UINT8 devId, MXL_HYDRA_TUNER_ID_E tunerId, SINT32 *adcRssiPwr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus;
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (adcRssiPwr)
    {
	  // Write some random pattern to 0x3FFFEB6C register
      mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, POWER_FROM_ADCRSSI_READBACK, 0x5A5A5A5A);

	  BUILD_HYDRA_CMD(MXL_HYDRA_DEV_REQ_PWR_FROM_ADCRSSI_CMD, MXL_CMD_WRITE, sizeof(MXL_HYDRA_TUNER_ID_E), &tunerId, cmdBuff);

	  // send command to device
      mxlStatus |= MxLWare_HYDRA_SendCommand(devId, sizeof(MXL_HYDRA_TUNER_ID_E) + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

	  MxLWare_HYDRA_OEM_SleepInMs(50);

	  mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, POWER_FROM_ADCRSSI_READBACK, (UINT32 *) adcRssiPwr);

	  while (( *adcRssiPwr == 0x5A5A5A5A) && (mxlStatus == MXL_SUCCESS))
	  {
		  MxLWare_HYDRA_OEM_SleepInMs(50);
		  mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, POWER_FROM_ADCRSSI_READBACK, (UINT32 *) adcRssiPwr);
	  }
	  //if(mxlStatus == MXL_SUCCESS)
		//MXL_HYDRA_PRINT("ADC RSSI POWER %.3f\n", ((float)*adcRssiPwr)/(float)1000.0);
    } 
    else 
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgClearDemodErrorCounters
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to retrieve Demod signal information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgClearDemodErrorCounters(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;
  UINT32 demodIndex = (UINT32)demodId;
  UINT8 cmdSize = sizeof(UINT32);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {

    BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_RESET_FEC_COUNTER_CMD, MXL_CMD_WRITE, cmdSize, &demodIndex, cmdBuff);
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDemodSignalOffsetInfo
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[out]  demodSigOffsetInfoPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to retrieve carrier offset & symbol offset
 * frequency information.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDemodSignalOffsetInfo(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_DEMOD_SIG_OFFSET_INFO_T *demodSigOffsetInfoPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus = MXL_SUCCESS;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (demodSigOffsetInfoPtr)
    {

      // Lock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_LOCK(devId,demodId);

      // read demod signal offset info
      mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, 
                                            (HYDRA_DMD_FREQ_OFFSET_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                            &regData);

      demodSigOffsetInfoPtr->carrierOffsetInHz = (SINT32)regData;

       // read demod signal offset info
       mxlStatus |= MxLWare_HYDRA_ReadRegister(devId,
                                            (HYDRA_DMD_STR_FREQ_OFFSET_ADDR_OFFSET + HYDRA_DMD_STATUS_OFFSET(demodId)),
                                            &regData);

      demodSigOffsetInfoPtr->symbolOffsetInSymbol = (SINT32)regData;
      MXL_HYDRA_PRINT("symbolOffsetInSymbol = %d\n", demodSigOffsetInfoPtr->symbolOffsetInSymbol);

      // Unlock demod status
      mxlStatus |= HYDRA_DEMOD_STATUS_UNLOCK(devId,demodId);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodScrambleCode
 *
 * @param[in]   devId        Device ID
 * @param[in]   demodId
 * @param[in]   demodScrambleInfoPtr
 *
 * @author Mahee
 *
 * @date 09/21/2012 Initial release
 *
 * This API shall be used when tuning for a channel.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodScrambleCode (UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_DEMOD_SCRAMBLE_INFO_T *demodScrambleInfoPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus;
  MXL_HYDRA_DEMOD_SCRAMBLE_CODE_T hydraScrambleCode;

  UINT8 cmdSize = sizeof(hydraScrambleCode);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT8 reqType;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (demodScrambleInfoPtr)
    {
      // send demod scramble code to the device
      hydraScrambleCode.demodIndex = (UINT32)demodId;
      MXLWARE_OSAL_MEMCPY(hydraScrambleCode.scrambleSequence, demodScrambleInfoPtr->scrambleSequence, (MXL_DEMOD_SCRAMBLE_SEQ_LEN * sizeof(UINT8)));
      hydraScrambleCode.scrambleCode = demodScrambleInfoPtr->scrambleCode;

      // build command manually instead of macro
      reqType = MXL_CMD_WRITE;
      cmdBuff[0] = ((reqType == MXL_CMD_WRITE) ? MXL_HYDRA_PLID_CMD_WRITE : MXL_HYDRA_PLID_CMD_READ);
      cmdBuff[1] = (cmdSize > 254)? 0xFF : (UINT8)(cmdSize + 4);
      cmdBuff[2] = (UINT8)0;
      cmdBuff[3] = (UINT8)MXL_HYDRA_DEMOD_SCRAMBLE_CODE_CMD;
      cmdBuff[4] = 0x00;
      cmdBuff[5] = 0x00;
      cmdSize += 6;

      cmdBuff[6] = GET_BYTE(hydraScrambleCode.demodIndex, 0);     // reg addr byte
      cmdBuff[7] = GET_BYTE(hydraScrambleCode.demodIndex, 1);     // reg addr byte
      cmdBuff[8] = GET_BYTE(hydraScrambleCode.demodIndex, 2);     // reg addr byte
      cmdBuff[9] = GET_BYTE(hydraScrambleCode.demodIndex, 3);     // reg addr byte

      MXLWARE_OSAL_MEMCPY((void *)&cmdBuff[10], (const void *)&(demodScrambleInfoPtr->scrambleSequence[0]), (MXL_DEMOD_SCRAMBLE_SEQ_LEN * sizeof(UINT8)));

      cmdBuff[22] = GET_BYTE(hydraScrambleCode.scrambleCode, 0);   // reg addr byte
      cmdBuff[23] = GET_BYTE(hydraScrambleCode.scrambleCode, 1);   // reg addr byte
      cmdBuff[24] = GET_BYTE(hydraScrambleCode.scrambleCode, 2);   // reg addr byte
      cmdBuff[25] = GET_BYTE(hydraScrambleCode.scrambleCode, 3);   // reg addr byte

      mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize, &cmdBuff[0]);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDemodSearchFreqOffset
 *
 * @param[in]   devId       - Device ID
 * @param[in] demodId		- Demod Id
 * @param[in] searchFreqType- Demod Frequency Offset Search Range
 *
 * @author Sateesh
 *
 * @date 07/02/2013 Initial release 
 * 
 * This API should be used to configure Demod Frequency Offset Search  
 * Range demod Equalizers.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_API_CfgDemodSearchFreqOffset(UINT8 devId, MXL_HYDRA_DEMOD_ID_E demodId, MXL_HYDRA_SEARCH_FREQ_OFFSET_TYPE_E searchFreqType)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;
  MXL58x_CFG_FREQ_OFF_SEARCH_RANGE_T searchFreqRangeCmd;
  UINT8 cmdSize = sizeof(searchFreqRangeCmd);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("demodId=%d\n", demodId););
  MXLENTERAPI(MXL_HYDRA_PRINT("searchFreqType=%d\n", searchFreqType););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    // build Demod Frequency Offset Search command
    searchFreqRangeCmd.demodIndex = demodId;
    searchFreqRangeCmd.searchType = searchFreqType;
    BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_FREQ_OFFSET_SEARCH_RANGE_CMD, MXL_CMD_WRITE, cmdSize, &searchFreqRangeCmd, cmdBuff);

    // send command to device
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}


/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_GainStepControl
 *
 * @param[in] devId         - Device ID
 * @param[in] tunerId		- Tuner Id
 * @param[in] gainState     - Gain state (0-8)
 *
 * @author Sateesh
 *
 * @date 07/02/2013 Initial release 
 * 
 * This API should be used to freeze the AGC and control gain.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_API_GainStepControl(UINT8 devId, MXL_HYDRA_TUNER_ID_E tunerId, UINT8 gainState)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_INVALID_PARAMETER;
  UINT32 lnaBo1P8 = 0;
  UINT32 rf1Bo1P8 = 0;
  UINT32 data = 0;

  UINT8 rficState = 2*gainState;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("tunerId=%d\n", tunerId););
  MXLENTERAPI(MXL_HYDRA_PRINT("state=%d\n", gainState););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
	  if ((gainState <= 8) && (tunerId < devHandlePtr->features.tunersCnt))
	  {
		  // Freeze AGC in Firmware
		  mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, DBG_ENABLE_DISABLE_AGC, 0x3);

	      mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, WB_DFE0_DFE_FB_AGC_BASEADDR + tunerId * 0x100000, 0);
      
		  if (mxlStatus == MXL_SUCCESS)
		  {
				// d2a_rffe_lna_bo_1p8
				lnaBo1P8 = (rficState < 6) ? rficState : 6;

				// d2a_rffe_rf1_bo_1p8
				rf1Bo1P8 = (rficState < 6) ? 0 : (rficState-6);

				// d2a_tX_rffe_rf1_bo_1p8 <3> && d2a_tX_rffe_lna_bo_1p8<2>
				data = (((rf1Bo1P8 >> 3) & 0x1) << 1) | ((lnaBo1P8 >> 2) & 0x1);
				mxlStatus = MxLWare_HYDRA_WriteRegister(devId, AFE_REG_D2A_TA_RFFE_LNA_BO_1P8_BASEADDR, ((data & 0x3) << (tunerId << 1)));

				data = 0;
				mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, WB_DFE0_DFE_FB_RF1_BASEADDR + tunerId * 0x100000, &data);

				if (mxlStatus == MXL_SUCCESS)
				{
					  data = (data >> 10) << 10;
					  data |= (((lnaBo1P8 & 0x3) << 8) | (0xBUL << 0x4) | (rf1Bo1P8 & 0x7));
					  mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, WB_DFE0_DFE_FB_RF1_BASEADDR + tunerId * 0x100000, data);

					  mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, WB_DFE0_DFE_FB_AGC_BASEADDR + tunerId * 0x100000, 1);

					  // d2a_rffe_rf1_cap_1p8
					  switch (tunerId)
					  {
						  case MXL_HYDRA_TUNER_ID_0:
							  mxlStatus |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_TA_RFFE_RF1_CAP_1P8, 0);  	
							  break;
						  case MXL_HYDRA_TUNER_ID_1:
							  mxlStatus |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_TB_RFFE_RF1_CAP_1P8, 0);  	
							  break;
						  case MXL_HYDRA_TUNER_ID_2:
							  mxlStatus |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_TC_RFFE_RF1_CAP_1P8, 0);  	
							  break;
						  case MXL_HYDRA_TUNER_ID_3:
							  mxlStatus |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_TD_RFFE_RF1_CAP_1P8, 0);  	
							  break;
						  default:
							  break;
					  }
				}
			}
	  }
	  else
	  {
         mxlStatus = MXL_INVALID_PARAMETER;
	  }
  }
 
  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}
