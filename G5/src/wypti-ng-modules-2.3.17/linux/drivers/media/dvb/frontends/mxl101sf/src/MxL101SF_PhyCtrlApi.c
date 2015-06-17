/*******************************************************************************
 *
 * FILE NAME          : MxL101SF_PhyCtrlApi.cpp
 * 
 * AUTHOR             : Brenndon Lee
 * DATE CREATED       : 1/22/2008
 *
 * DESCRIPTION        : This file contains control APIs that configure MxL101SF 
 *                      and read back the statistics through I2C interface.
 *                             
 *******************************************************************************
 *                Copyright (c) 2006, MaxLinear, Inc.
 ******************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "dvb_frontend.h"
#include "MxL101SF_PhyCtrlApi.h"
#include "MxL101SF_PhyDefs.h"
#include "MxL101SF_OEM_Drv.h"

const UINT8 MxLCtrlVersion[] = {6, 2, 3, 1};

UINT8  MxL_DeviceMode;         // Tuner or SOC Mode

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_SoftResetDevice
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 9/8/2008
--|
--| DESCRIPTION   : By writing any value into address 0xFF (AIC), all control 
--|                 registers are initialized to the default value.
--|                 AIC - Address Initiated Command
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_SoftResetDevice(void)
{
  return Ctrl_ProgramRegisters(MxL_PhySoftReset);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_GetChipInfo
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 2/7/2008
--|
--| DESCRIPTION   : This function returns MxL101SF Chip Id and version information.
--|                 Chip Id of MxL101SF has 0x61
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_GetChipInfo(PMXL_DEV_INFO_T DevInfoPtr)
{
  UINT8 status = MXL_FALSE;
  UINT8 readBack;

  status = Ctrl_ReadRegister(CHIP_ID_REG, &readBack);
  DevInfoPtr->DevId = readBack;

  status |= Ctrl_ReadRegister(TOP_CHIP_REV_ID_REG, &readBack);
  DevInfoPtr->DevVer = readBack;
  
  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_SetMxLDeviceMode
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/15/2008
--|
--| DESCRIPTION   : MxL101SF has a built-in RF Tuner in addtion to DVB-T BB block
--|                 In tuner only mode, Digial BB will be disabled.
--|                 MXL_TUNER_MODE : Tuner mode, MXL_SOC_MODE : SOC mode
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_SetMxLDeviceMode(PMXL_DEV_MODE_CFG_T DevModePtr)
{
  UINT8 status = MXL_FALSE;

  MxL_DeviceMode = DevModePtr->DeviceMode;

  // Set Device mode
  if (MxL_DeviceMode == MXL_SOC_MODE)
    status = Ctrl_ProgramRegisters(MxL_TunerDemodMode);
  else if (MxL_DeviceMode == MXL_TUNER_MODE)
    status = Ctrl_ProgramRegisters(MxL_TunerMode);
  
  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_TopMasterControl
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 4/17/2009
--|
--| DESCRIPTION   : Power up Tuner
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_TopMasterControl(PMXL_TOP_MASTER_CFG_T TopMasterCtrlPtr)
{
  UINT8 status = MXL_FALSE;

  if (TopMasterCtrlPtr->TopMasterEnable == MXL_DISABLE)
    status = Ctrl_ProgramRegisters(MxL_TopMasterDisable);
  else
    status = Ctrl_ProgramRegisters(MxL_TopMasterEnable);
  
  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_InitTunerDemod
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 7/30/2009
--|
--| DESCRIPTION   : Initializing Tuner and Demod block of MxL101SF
--|                 before tuning to a certain frequency.
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_InitTunerDemod(void)
{
  return Ctrl_ProgramRegisters(MxL_101OverwriteDefault);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_EnableMpegOutput
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 12/19/2008
--|
--| DESCRIPTION   : Enable TSIF for MPEG-2 data output
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_EnableMpegOutput(void)
{
  return Ctrl_ProgramRegisters(MxL_MpegDataOutToTSIF);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigMpegOut
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/15/2008
--|
--| DESCRIPTION   : For Transport stream output through ASI(MPEG TS) interface
--|                 the following parameters are needed to configure after or before
--|                 demod lock.
--|                  TS output mode : Seral or Parallel
--|                  CLK Phase : Normal or Inverted
--|                  MPEG Valid Polarity : Low or High
--|                  MPEG Sync Polarity  : Low or High
--|                  MPEG CLK Frequency  : 0 - 36.571429MHz
--|                                      : 1 - 2.285714MHz
--|                                      : 2 - 4.571429MHz
--|                                      : 3 - 6.857143MHz
--|                                      : 4 - 9.142857MHz
--|                                      : 5 - 13.714286MHz
--|                                      : 6 - 18.285714MHz
--|                                      : 7 - 27.428571MHz
--|                 * MxL101SF Feature
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigMpegOut(PMXL_MPEG_CFG_T MpegOutCfgPtr)
{
  UINT8 status = MXL_TRUE;
  UINT8 mode, tmp;

  mode = 0xC0; // MPEG Out

  // Configure MPEG Clock phase
  if (MpegOutCfgPtr->MpegClkPhase == MPEG_CLK_IN_PHASE) 
    mode &= ~V6_INVERTED_CLK_PHASE;
  else 
    mode |= V6_INVERTED_CLK_PHASE;;

  if (MpegOutCfgPtr->MpegClkFreq > MPEG_CLOCK_27_428571MHz) 
    MpegOutCfgPtr->MpegClkFreq = MPEG_CLOCK_27_428571MHz;
  
  mode |= (((UINT8)MpegOutCfgPtr->MpegClkFreq) << 2);

  status = Ctrl_WriteRegister(V6_MPEG_OUT_CLK_INV_REG, mode);

  // Configure data input mode, MPEG Valid polarity, MPEG Sync polarity
  // Get current configuration
  status |= Ctrl_ReadRegister(V6_MPEG_OUT_CTRL_REG, &mode);        
  mode &= 0xF0;

  // Data Input mode
  if (MpegOutCfgPtr->SerialOrPar == MPEG_DATA_PARALLEL) 
  {
    mode |= V6_MPEG_DATA_PARALLEL;
  }
  else 
  {
    mode |= V6_MPEG_DATA_SERIAL;

    // If serial interface is selected, configure MSB or LSB order in transmission
    status |= Ctrl_ReadRegister(V6_MPEG_INOUT_BIT_ORDER_CTRL_REG, &tmp);  

    if (MpegOutCfgPtr->LsbOrMsbFirst == MPEG_SERIAL_MSB_1ST) 
      tmp |= V6_MPEG_SER_MSB_FIRST;  
    else 
      tmp &= ~V6_MPEG_SER_MSB_FIRST; // LSB First

    status |= Ctrl_WriteRegister(V6_MPEG_INOUT_BIT_ORDER_CTRL_REG, tmp);
  }
  
  // MPEG Sync polarity
  if (MpegOutCfgPtr->MpegSyncPol == MPEG_CLK_IN_PHASE) 
    mode &= ~V6_INVERTED_MPEG_SYNC;
  else 
    mode |= V6_INVERTED_MPEG_SYNC;

  // MPEG Valid polarity
  if (MpegOutCfgPtr->MpegValidPol == MPEG_CLK_IN_PHASE) 
    mode &= ~V6_INVERTED_MPEG_VALID; 
  else 
    mode |= V6_INVERTED_MPEG_VALID;

  status |= Ctrl_WriteRegister(V6_MPEG_OUT_CTRL_REG, mode);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigDevPowerSavingMode
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 10/20/2009
--|
--| DESCRIPTION   : This function configures Standby mode and Sleep mode to
--|                 control power consumption.
--|
--| RETURN VALUE  : True or False
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigDevPowerSavingMode(PMXL_PWR_MODE_CFG_T PwrModePtr)
{
  UINT8 status = MXL_FALSE;
  
  switch (PwrModePtr->PowerMode)
  {
    case STANDBY_ON:
    case SLEEP_ON:
      status = Ctrl_ProgramRegisters(MxL_101SuspendMode);
      break;

    case STANDBY_OFF:
    case SLEEP_OFF:
      status = Ctrl_ProgramRegisters(MxL_101WakeUp);
      status |= MxL101SF_API_SoftResetDevice();   
      status |= MxL101SF_API_InitTunerDemod();
      break;

    default:
      break;
  }

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_TuneRF
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur - updated new topmastercontrol function name
--|                                 - Support for TPS Cell ID
--|
--| DATE CREATED  : 1/15/2008
--|                 10/23/2009
--|                 01/08/2010
--|
--| DESCRIPTION   : After power-on initialization, when channel frequency has 
--|                 changed, this function shall be called
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_TuneRF(PMXL_RF_TUNE_CFG_T TuneParamPtr)
{
  PREG_CTRL_INFO_T ctrlArrayPtr;
  UINT8 status = MXL_FALSE;
  UINT8 mxl_mode;
  UINT8 regData = 0;
  MXL_DEMOD_LOCK_STATUS_T cpLock;
  UINT32 cellIdStartTime;
  UINT32 StartTime;
  UINT32 cellIdEndTime;
  UINT16 TimeOut;
  UINT8 agcLockStatus;
  UINT16 i, j;
  
  // Stop Tune
  status = Ctrl_WriteRegister(START_TUNE_REG, 0);

  // Check Device mode
  status |= Ctrl_ReadRegister(MXL_MODE_REG, &mxl_mode);

  // Fill out registers for channel tune
  ctrlArrayPtr = Ctrl_PhyTune(TuneParamPtr->Frequency, TuneParamPtr->Bandwidth);
  
  if (!ctrlArrayPtr) return MXL_FALSE;
  
  // Program registers 
  status |= Ctrl_ProgramRegisters(ctrlArrayPtr);
  if ((mxl_mode & MXL_DEV_MODE_MASK) == MXL_SOC_MODE)
  {
    // Program registers to retrive TPS Cell ID information 
    if (TuneParamPtr->TpsCellIdRbCtrl == MXL_ENABLE)
      status |= Ctrl_ProgramRegisters(MxL_EnableCellId); 
  }
  status |= Ctrl_WriteRegister(START_TUNE_REG, 1);

  if ((mxl_mode & MXL_DEV_MODE_MASK) == MXL_SOC_MODE)
  {

    if (TuneParamPtr->TpsCellIdRbCtrl == MXL_ENABLE)
    {
      // Get Timestamp in milliseconds for CP lock
//      Ctrl_GetTime(&cellIdStartTime);

      // Get Timestamp for Tune RF function when TPS Cell ID feature is enabled
//      Ctrl_GetTime(&StartTime);
//      cellIdEndTime = cellIdStartTime;   
      TimeOut = 1024;  //Timeout in milliseconds for Tune RF function when TPS Cell Id is enabled

      status |= Ctrl_ReadRegister(V6_AGC_LOCK_REG, &regData);
      agcLockStatus = (regData & V6_AGC_LOCK_MASK) >> 5;

      // If AGC lock is present, Tune RF function timeout will increase to 2048 from 1024 milliseconds
      if (agcLockStatus)
        TimeOut = 2048; 
     
      do
      {
      	status |= MxLWare_API_GetDemodStatus(MXL_DEMOD_CP_LOCK_REQ, &cpLock);

	if (cpLock.Status == MXL_UNLOCKED)
	{
		if (j>=TimeOut)
			break;
		i=0;
	}

	i++;
	j++;
	msleep(1);

      }while (i < 350);

/*      // Check for continuous CP lock for 350 milliseconds
      while ((cellIdEndTime - cellIdStartTime) < 350)  
      {
        status |= MxLWare_API_GetDemodStatus(MXL_DEMOD_CP_LOCK_REQ, &cpLock);       
        if (cpLock.Status == MXL_UNLOCKED) 
        {
          // Exit from function, if there is no continuous CP lock for 350 Milliseconds & Tune RF function has timed out
          if ((cellIdEndTime - StartTime) >= TimeOut)  break;
          
          Ctrl_GetTime(&cellIdStartTime);
        }

        Ctrl_GetTime(&cellIdEndTime);
      }*/

      // Program registers for TPS Cell ID feature 
      status |= Ctrl_ProgramRegisters(MxL_DisableCellId); 
    }
  }

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetSNR
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur - Support for Interger calculation of SNR
--|
--| DATE CREATED  : 1/24/2008
--|                 1/12/2010
--|
--| DESCRIPTION   : This function returns SNR(Signal to Noise Ratio).
--|                 SNR is calculated as follows after reading 10bit register
--|                 Folting-point calculation:
--|                  SNR = 10 * SNR_REG_VALUE / 64 - 2.5  dB
--|                 Integer calculation: 
--|                  10000 x SNR = 1563 x SNR_REG_VALUE - 25000  dB 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

#if __FLOATING_POINT_STATISTICS__

MXL_STATUS MxL101SF_API_DemodGetSNR(PMXL_DEMOD_SNR_INFO_T SnrPtr)
{
  UINT8 status = MXL_FALSE;
  REAL32 snrCalc;
  UINT16 snrData;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_SNR_RB_LSB_REG, &rawData);
  snrData = rawData;

  status |= Ctrl_ReadRegister(V6_SNR_RB_MSB_REG, &rawData);
  snrData |= (rawData & 0x03) << 8;
  
  if (status == MXL_TRUE)
  {
    snrCalc =  CALCULATE_SNR(snrData);      
    SnrPtr->SNR = snrCalc;
  }
  
  return (MXL_STATUS)status;       
}

#else

MXL_STATUS MxL101SF_API_DemodGetSNR(PMXL_DEMOD_SNR_INFO_T SnrPtr)
{
  UINT8 status = MXL_FALSE;
  UINT32 snrCalc;
  UINT16 snrData;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_SNR_RB_LSB_REG, &rawData);
  snrData = rawData;

  status |= Ctrl_ReadRegister(V6_SNR_RB_MSB_REG, &rawData);
  snrData |= (rawData & 0x03) << 8;
  
  if (status == MXL_TRUE)
  {
    snrCalc =  CALCULATE_SNR(snrData);      
    SnrPtr->SNR = snrCalc;
  }
  
  return (MXL_STATUS)status;       
}

#endif

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetBER
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur - Support for Interger calculation of BER
--|
--| DATE CREATED  : 1/24/2008
--|                 1/12/2010
--|
--| DESCRIPTION   : This function returns BER(Bit Error Ratio).
--|                 Floating-point calculation
--|                  BER = (AVG_ERRORS x 4)/(N_ACCUMULATE x 64 x 188 x 8)
--|                 Integer calculation
--|                  1e-9 x BER = (AVG_ERRORS x 4 x 611)
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

#if __FLOATING_POINT_STATISTICS__

MXL_STATUS MxL101SF_API_DemodGetBER(PMXL_DEMOD_BER_INFO_T BerInfoPtr)
{
  UINT8 status;
  REAL32 berCalc;
  UINT32 n_accumulate;
  UINT16 avg_errors;
  UINT8 rawData;
  
  avg_errors = 0;
  status = Ctrl_ReadRegister(V6_RS_AVG_ERRORS_LSB_REG, &rawData);
  avg_errors = rawData;

  status |= Ctrl_ReadRegister(V6_RS_AVG_ERRORS_MSB_REG, &rawData);
  avg_errors |= (rawData << 8);

  // Get N_ACCUMULATE
  status |= Ctrl_ReadRegister(V6_N_ACCUMULATE_REG, &rawData);
  n_accumulate = rawData;

  berCalc = CALCULATE_BER(avg_errors, n_accumulate);

  if (status == MXL_TRUE)
  {
    BerInfoPtr->BER = berCalc;
  }

  return (MXL_STATUS)status;       
}

#else

MXL_STATUS MxL101SF_API_DemodGetBER(PMXL_DEMOD_BER_INFO_T BerInfoPtr)
{
  UINT8 status;
  UINT32 berCalc;
  UINT32 n_accumulate;
  UINT16 avg_errors;
  UINT8 rawData;
  
  avg_errors = 0;
  status = Ctrl_ReadRegister(V6_RS_AVG_ERRORS_LSB_REG, &rawData);
  avg_errors = rawData;

  status |= Ctrl_ReadRegister(V6_RS_AVG_ERRORS_MSB_REG, &rawData);
  avg_errors |= (rawData << 8);

  status |= Ctrl_ReadRegister(V6_N_ACCUMULATE_REG, &rawData);
  n_accumulate = rawData;

  berCalc = CALCULATE_BER(avg_errors, n_accumulate);

  if (status == MXL_TRUE)
  {
    BerInfoPtr->BER = berCalc;
  }

  return (MXL_STATUS)status;       
}

#endif

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetSyncLockStatus
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 5/6/2009
--|
--| DESCRIPTION   : This function return SYNC_LOCK Status 
--|                 if 1, Locked
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetSyncLockStatus(PMXL_DEMOD_LOCK_STATUS_T SyncLockPtr)
{
  MXL_STATUS status;
  UINT8 rawData;
  
  status = Ctrl_ReadRegister(V6_SYNC_LOCK_REG, &rawData);

  if (status == MXL_TRUE)
  { 
    if ((rawData & SYNC_LOCK_MASK) >> 4)
      SyncLockPtr->Status = MXL_LOCKED;
    else
      SyncLockPtr->Status = MXL_UNLOCKED;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetRsLockStatus
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 4/30/2008
--|
--| DESCRIPTION   : This function returns RS Lock status
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetRsLockStatus(PMXL_DEMOD_LOCK_STATUS_T RsLockPtr)
{
  MXL_STATUS status;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_RS_LOCK_DET_REG, &rawData);

  if (status == MXL_TRUE)
  {
    if ((rawData & RS_LOCK_DET_MASK) >> 3)
      RsLockPtr->Status = MXL_LOCKED;
    else
      RsLockPtr->Status = MXL_UNLOCKED;
  }
 
  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetCpLockStatus
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 4/30/2008
--|
--| DESCRIPTION   : This function returns CP Lock status
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetCpLockStatus(PMXL_DEMOD_LOCK_STATUS_T CpLockPtr)
{
  MXL_STATUS status;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_CP_LOCK_DET_REG, &rawData);

  if (status == MXL_TRUE)
  {
    if((rawData & V6_CP_LOCK_DET_MASK) >> 2)
      CpLockPtr->Status = MXL_LOCKED;
    else
      CpLockPtr->Status = MXL_UNLOCKED;
  }
 
  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodResetIrqStatus
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|                 9/09/2008
--|
--| DESCRIPTION   : This function clears IRQ status registers
--|                 Writing 0xFF to this register will clear
--|                 the previous status
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodResetIrqStatus(void)
{
  return Ctrl_ProgramRegisters(MxL_IrqClear);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsCodeRate
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function returns code rate of TPS parameters
--|                 bit<2:0> - 000:1/2, 001:2/3, 010:3/4, 011:5/6, 100:7/8
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsCodeRate(PMXL_DEMOD_TPS_INFO_T TpsCodeRatePtr)
{
  MXL_STATUS status;
  UINT8 rawData;
  
  status = Ctrl_ReadRegister(V6_CODE_RATE_TPS_REG, &rawData);

  if (status == MXL_TRUE)
    TpsCodeRatePtr->TpsInfo = rawData & V6_CODE_RATE_TPS_MASK;
  
  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsHierarchy
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function returns Hiearchy information of TPS parameters
--|                 bit<6:4> - 000:Non hierarchy, 001:1, 010:2, 011:4
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsHierarchy(PMXL_DEMOD_TPS_INFO_T TpsHierarchyPtr)
{
  MXL_STATUS status;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_TPS_HIERACHY_REG, &rawData);

  if (status == MXL_TRUE)
    TpsHierarchyPtr->TpsInfo = (rawData & V6_TPS_HIERARCHY_INFO_MASK) >> 6;
  
  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigStreamPriority
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 1/14/2010
--|
--| DESCRIPTION   : This API will config High priority or Low priority stream
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigStreamPriority(PMXL_DEMOD_TS_PRIORITY_CFG_T TsPriorityPtr)
{
  UINT8 status;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_TPS_HPORLP_REG, &rawData);

  if (status == MXL_TRUE)
  {
    if (TsPriorityPtr->StreamPriority == LP_STREAM)
      rawData = (rawData | 0x80);
    else
      rawData = (rawData & 0x7F);

    status |= Ctrl_WriteRegister(V6_TPS_HPORLP_REG, rawData);
  }
    
  return (MXL_STATUS)status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetHierarchicalAlphaValue
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 1/14/2010
--|
--| DESCRIPTION   : This function returns Hierarchical Alpha of TPS parameters
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetHierarchicalAlphaValue(PMXL_DEMOD_TPS_INFO_T TpsAlphaPtr)
{
  MXL_STATUS status;
  UINT8 rawData;

  status = Ctrl_ReadRegister(V6_TPS_ALPHA_REG, &rawData);

  if (status == MXL_TRUE)
    TpsAlphaPtr->TpsInfo = (rawData & V6_TPS_ALPHA_MASK);
  
  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsConstellation
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function return Constellation status bit information
--|                 Constellation, 00 : QPSK, 01 : 16QAM, 10:64QAM   
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsConstellation(PMXL_DEMOD_TPS_INFO_T TpsConstellationPtr)
{
  MXL_STATUS status;
  UINT8 tpsParams;
  
  status = Ctrl_ReadRegister(V6_MODORDER_TPS_REG, &tpsParams);
 
  if (status == MXL_TRUE)
    TpsConstellationPtr->TpsInfo = (tpsParams & V6_PARAM_CONSTELLATION_MASK) >> 4 ;
  
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsFftMode
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function return FFT Mode status bit information
--|                 FFT Mode, 00:2K, 01:8K, 10:4K
--| 
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsFftMode(PMXL_DEMOD_TPS_INFO_T TpsFftModePtr)
{
  MXL_STATUS status;
  UINT8 tpsParams;
  
  status = Ctrl_ReadRegister(V6_MODE_TPS_REG, &tpsParams);
 
  if (status == MXL_TRUE)
    TpsFftModePtr->TpsInfo = (tpsParams & V6_PARAM_FFT_MODE_MASK) >> 2 ;
  
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsGuardInterval
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 1/24/2008
--|
--| DESCRIPTION   : This function return GI status bit information
--|                 00:1/32, 01:1/16, 10:1/8, 11:1/4
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsGuardInterval(PMXL_DEMOD_TPS_INFO_T TpsGIPtr)
{
  MXL_STATUS status;
  UINT8 tpsParams;
  
  status = Ctrl_ReadRegister(V6_CP_TPS_REG, &tpsParams);
 
  if (status == MXL_TRUE)
    TpsGIPtr->TpsInfo = (tpsParams & V6_PARAM_GI_MASK) >> 4 ;
  
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetTpsLock
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 7/29/2008
--|
--| DESCRIPTION   : This function returns Tps lock status bit information.
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetTpsLock(PMXL_DEMOD_LOCK_STATUS_T TpsLockPtr)
{
  MXL_STATUS status;
  UINT8 tpsParams;
  
  status = Ctrl_ReadRegister(V6_TPS_LOCK_REG, &tpsParams);
 
  if (status == MXL_TRUE)
  {
    if((tpsParams & V6_PARAM_TPS_LOCK_MASK) >> 6)
      TpsLockPtr->Status = MXL_LOCKED;
    else
      TpsLockPtr->Status = MXL_UNLOCKED;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetPacketErrorCount
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 3/13/2008
--|
--| DESCRIPTION   : This function returns TS Packet error count.
--|                 
--|                  PER Count = FEC_PER_COUNT * (2 ** (FEC_PER_SCALE * 4))
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetPacketErrorCount(PMXL_DEMOD_PEC_INFO_T PecInfoPtr)
{
  UINT8 status;
  UINT32 fec_per_count, fec_per_scale;
  UINT8 rawData;
  
  // FEC_PER_COUNT Register
  status = Ctrl_ReadRegister(V6_FEC_PER_COUNT_REG, &rawData);
  fec_per_count = rawData;
 
  // FEC_PER_SCALE Register
  status |= Ctrl_ReadRegister(V6_FEC_PER_SCALE_REG, &rawData);
  
  rawData &= V6_FEC_PER_SCALE_MASK;
  rawData *= 4;
  
  fec_per_scale = 1 << rawData;
  
  fec_per_count *= fec_per_scale;
  
  PecInfoPtr->PEC = fec_per_count;
  
  return (MXL_STATUS)status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodResetPacketErrorCount
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 3/13/2008
--|
--| DESCRIPTION   : This function resets TS Packet error count.
--|                 
--|                 After setting 7th bit of V5_PER_COUNT_RESET_REG,
--|                 it should be reset to 0.
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodResetPacketErrorCount(void)
{
  return Ctrl_ProgramRegisters(MxL_ResetPerCount);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_TunerGetLockStatus
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 7/3/2008
--|
--| DESCRIPTION   : This function provides RF synthesizer lock status
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_TunerGetLockStatus(PMXL_TUNER_LOCK_STATUS_T TunerLockStatusPtr)
{
  MXL_STATUS status;
  UINT8 data;

  // Initialize
  TunerLockStatusPtr->RefSynthLock = MXL_UNLOCKED;
  TunerLockStatusPtr->RfSynthLock = MXL_UNLOCKED;
  
  status = Ctrl_ReadRegister(V6_RF_LOCK_STATUS_REG, &data);

  if (status == MXL_TRUE)
  {
    if ((data & 3) == 3)
      TunerLockStatusPtr->RefSynthLock = MXL_LOCKED;;  // Locked

    if ((data & 0xc) == 0xc)
      TunerLockStatusPtr->RfSynthLock = MXL_LOCKED;;  // Locked
  }

  return status;       
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_TunerSetIFOutputFreq
--| 
--| AUTHOR        : Brenndon Lee
--|
--| DATE CREATED  : 7/7/2008
--|
--| DESCRIPTION   : In Tuner only mode of MxL101SF, this function configures
--|                 IF output frequency
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_TunerSetIFOutputFreq(PMXL_TUNER_IF_FREQ_T IfFreqParamPtr)
{
  UINT8 status;
  UINT8 control;
  UINT16 IFFCW;

  // Set IF polarity  
  if (IfFreqParamPtr->IF_Polarity == TUNER_NORMAL_IF_SPECTRUM) 
    control = TUNER_NORMAL_IF_SPECTRUM;
  else 
    control = TUNER_INVERT_IF_SPECTRUM;

  if (IfFreqParamPtr->IF_Index < IF_OTHER_35MHZ_45MHZ)
  {
    control |= (UINT8)IfFreqParamPtr->IF_Index;
  }

  status = Ctrl_WriteRegister(V6_TUNER_IF_SEL_REG, control);

  IfFreqParamPtr->IF_Freq /= 1000000;
    
  // Do round
//  IfFreqParamPtr->IF_Freq += 0.5;

  if (IfFreqParamPtr->IF_Index == 0) 
  {
    control = 0x08;

    IFFCW = (UINT16)(IfFreqParamPtr->IF_Freq / (108 * 4096));
  }
  else if (IfFreqParamPtr->IF_Index == 15)
  {
    control = 0x08;

    IFFCW = (UINT16)(IfFreqParamPtr->IF_Freq / (216 * 4096));
  }
  else
  {
    control = 0;
    IFFCW = 0;
  }

  control |= (IFFCW >> 8);
  status |= Ctrl_WriteRegister(V6_TUNER_IF_FCW_BYP_REG, control);

  control = IFFCW & 0x00FF;
  status |= Ctrl_WriteRegister(V6_TUNER_IF_FCW_REG, control);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_TunerLoopThruControl
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : If loop through mode is enabled, RF signal from the antenna
--|                 is looped through to an external demodulator.
--|                  0 : Disable, 1: Enable, 
--|                 API is reorganised according to the MxL101SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_TunerLoopThruControl(MXL_BOOL EnableDisable)
{
  MXL_STATUS status = MXL_TRUE;
  UINT8 control;

  if (EnableDisable == MXL_DISABLE) control = 0; // Disable
  else control = 1;             // Enable

  status = Ctrl_WriteRegister(V6_TUNER_LOOP_THRU_CTRL_REG, control);

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_XtalSelect
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : Select XTAL frequency of CLK out 
--|                 4 : 24MHz, 8 : 28.8MHz, 7 : 27 MHz, 12 : 48 MHz
--|                 API is reorganised according to the MxL1x1SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_XtalSelect(MXL_XTAL_FREQ_E XtalFreq)
{
  UINT8 status = MXL_TRUE;
  
  status = Ctrl_WriteRegister(V6_DIG_CLK_FREQ_SEL_REG, (UINT8)XtalFreq);
  status |= Ctrl_WriteRegister(V6_DIG_RFREFSELECT_REG, ((UINT8)XtalFreq)|0xA0);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_XtalClkOutGain
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : If Xtal Clock out is enabled, then a valid clk out gain value
--|                 should be programmed to the chip.
--|                 API is reorganised according to the MxL1x1SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_XtalClkOutGain(MXL_XTAL_CLK_OUT_GAIN_E ClkOutGain)
{
  if (ClkOutGain > CLK_OUT_18_75dB) ClkOutGain = CLK_OUT_18_75dB;

  return Ctrl_WriteRegister(V6_XTAL_CLK_OUT_GAIN_REG, (UINT8)ClkOutGain);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_XtalClkOutControl
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : XTAL Clock out control
--|                  0 : Disable, 1: Enable,
--|                 API is reorganised according to the MxL1x1SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_XtalClkOutControl(MXL_BOOL EnableDisable)
{
  UINT8 status = MXL_TRUE;
  UINT8 control;

  status = Ctrl_ReadRegister(V6_DIG_XTAL_ENABLE_REG, &control);

  if (EnableDisable == MXL_DISABLE) control &= 0xDF; // Disable
  else control |= 0x20;             // Enable

  status |= Ctrl_WriteRegister(V6_DIG_XTAL_ENABLE_REG, control);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_XtalBiasControl
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : 0 : 200uA, 1 : 575 uA, ...
--|                 API is reorganised according to the MxL1x1SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_XtalBiasControl(MXL_XTAL_BIAS_E XtalBias)
{
  MXL_STATUS status = MXL_TRUE;
  UINT8 control;

  control = 0x10;          // Xtal Enable

  if (XtalBias > I894_2uA) XtalBias = I894_2uA;

  control |= (UINT8)XtalBias;

  status = Ctrl_WriteRegister(V6_DIG_XTAL_BIAS_REG, control);

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_XtalCapControl
--| 
--| AUTHOR        : Brenndon Lee
--|                 Mahendra Kondur
--|
--| DATE CREATED  : 7/7/2008
--|                 8/4/2009
--|
--| DESCRIPTION   : XTAL Clock Cap control
--|                 0 : 10pF, 1 : 1 pF, ...
--|                 API is reorganised according to the MxL1x1SF control structure 
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_XtalCapControl(UINT8 XtalCapacitor)
{
  MXL_STATUS status = MXL_TRUE;

  if ((XtalCapacitor < 26) || (XtalCapacitor == 0x3F)) 
    status = Ctrl_WriteRegister(V6_XTAL_CAP_REG, XtalCapacitor);

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigXtalSettings
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 8/4/2009
--|
--| DESCRIPTION   : This API shall configure XTAL settings for MxL101SF
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigXtalSettings(PMXL_XTAL_CFG_T XtalCfgPtr)
{
  UINT8 status;
  
  status = MxL101SF_API_TunerLoopThruControl(XtalCfgPtr->LoopThruEnable);
  status |= MxL101SF_API_XtalSelect(XtalCfgPtr->XtalFreq);
  status |= MxL101SF_API_XtalClkOutGain(XtalCfgPtr->XtalClkOutGain);
  status |= MxL101SF_API_XtalClkOutControl(XtalCfgPtr->XtalClkOutEnable);
  status |= MxL101SF_API_XtalBiasControl(XtalCfgPtr->XtalBiasCurrent);
  status |= MxL101SF_API_XtalCapControl(XtalCfgPtr->XtalCap);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_GetTunerSignalStrength
--| 
--| AUTHOR        : Mahendra
--|
--| DATE CREATED  : 10/20/2009
--|
--| DESCRIPTION   : This API shall give MxL101SF's RF Signal Strength in dBm unit
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_GetTunerSignalStrength(PMXL_SIGNAL_STATS_T SigQualityPtr)
{
  UINT8 status;
  UINT8 regData;
  UINT16 rxPwr;

  status = Ctrl_WriteRegister(0x00, 0x02);
  
  status |= Ctrl_ReadRegister(V6_DIG_RF_PWR_LSB_REG, &regData);
  rxPwr = regData;

  status |= Ctrl_ReadRegister(V6_DIG_RF_PWR_MSB_REG, &regData);
  rxPwr |= (regData & 0x07) << 8;

  SigQualityPtr->SignalStrength = (REAL32) (rxPwr / 8) - 110;

  status |= Ctrl_WriteRegister(0x00, 0x00);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigCableSettings
--| 
--| AUTHOR        : Mahendra
--|
--| DATE CREATED  : 11/05/2009
--|
--| DESCRIPTION   : This API shall configure DVBC settings on MxL101SF
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigCableSettings(void)
{
  return Ctrl_ProgramRegisters(MxL_CableSettings);
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_ConfigGPOPins
--| 
--| AUTHOR        : Mahendra
--|
--| DATE CREATED  : 10/02/2009
--|
--| DESCRIPTION   : API to confugure GPIO Pins of MxL101SF
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_ConfigGPOPins(PMXL_DEV_GPO_CFG_T GpoPinCfgPtr)
{
  UINT8 status = MXL_FALSE;
  UINT8 regData = 0;
  UINT8 gpoMask = 0;

  status = Ctrl_ReadRegister(V6_GPO_CTRL_REG, &regData); 

  switch(GpoPinCfgPtr->GpoPinId)
  {
    case MXL_GPO_0:
      gpoMask = V6_GPO_0_MASK;
      break;

    case MXL_GPO_1:
      gpoMask = V6_GPO_1_MASK;
      break;
    
    default:
      break;
  }

  if (GpoPinCfgPtr->GpoPinCtrl)
    regData |= gpoMask;
  else
    regData &= ~gpoMask;

  status |= Ctrl_WriteRegister(V6_GPO_CTRL_REG, regData);  
  
  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxL101SF_API_DemodGetCellId
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 11/19/2009
--|
--| DESCRIPTION   : This API returns TPS Cell ID information
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxL101SF_API_DemodGetCellId(PMXL_DEMOD_CELL_ID_INFO_T tpsCellId)
{
  UINT8 status;
  UINT8 regData;
  UINT16 CellId;

  status = Ctrl_WriteRegister(0x00, 0x02);
  
  status |= Ctrl_ReadRegister(V6_TPS_CELL_ID_LSB_REG, &regData);
  CellId = regData;

  status |= Ctrl_ReadRegister(V6_TPS_CELL_ID_MSB_REG, &regData);
  CellId |= regData << 8;

  tpsCellId->TpsCellId = CellId;

  status |= Ctrl_WriteRegister(0x00, 0x00);

  return (MXL_STATUS)status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_ConfigDevice
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 08/08/2009
--|
--| DESCRIPTION   : The general device configuration shall be handled 
--|                 through this API
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_ConfigDevice(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;

  switch (CmdType)
  {
    case MXL_DEV_SOFT_RESET_CFG:
      status = MxL101SF_API_SoftResetDevice();
      break;

    case MXL_DEV_OPERATIONAL_MODE_CFG:
      status = MxL101SF_API_SetMxLDeviceMode((PMXL_DEV_MODE_CFG_T)ParamPtr);
      break;

    case MXL_DEV_XTAL_SETTINGS_CFG:
      status = MxL101SF_API_ConfigXtalSettings((PMXL_XTAL_CFG_T)ParamPtr);
      break;

    case MXL_DEV_101SF_OVERWRITE_DEFAULTS_CFG:
      status = MxL101SF_API_InitTunerDemod();
      break;

    case MXL_DEV_101SF_POWER_MODE_CFG:
      status = MxL101SF_API_ConfigDevPowerSavingMode((PMXL_PWR_MODE_CFG_T)ParamPtr);
      break;

    case MXL_DEV_MPEG_OUT_CFG:
      status = MxL101SF_API_ConfigMpegOut((PMXL_MPEG_CFG_T)ParamPtr);
      break;

    case MXL_DEV_GPO_PINS_CFG:
      status = MxL101SF_API_ConfigGPOPins((PMXL_DEV_GPO_CFG_T)ParamPtr);
      break;

    case MXL_DEV_CABLE_CFG:
      status = MxL101SF_API_ConfigCableSettings();
      break;

    default:
      break;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_GetDeviceStatus
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 08/08/2009
--|
--| DESCRIPTION   : The general device inquiries shall be handled 
--|                 through this API
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_GetDeviceStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;

  switch (CmdType)
  {
    case MXL_DEV_ID_VERSION_REQ:
      status = MxL101SF_API_GetChipInfo((PMXL_DEV_INFO_T)ParamPtr);
      break;

    default:
      break;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_GetDemodStatus
--| 
--| AUTHOR        : Mahendra Kondur
--| 
--| DATE CREATED  : 08/08/2009
--|
--| DESCRIPTION   : The demod specific inquiries shall be handled 
--|                 through this API
--|                 - Support for MXL_DEMOD_TPS_HIERARCHICAL_ALPHA_REQ
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_GetDemodStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;

  switch (CmdType)
  {
    case MXL_DEMOD_SNR_REQ:
      status = MxL101SF_API_DemodGetSNR((PMXL_DEMOD_SNR_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_BER_REQ:
      status = MxL101SF_API_DemodGetBER((PMXL_DEMOD_BER_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_CODE_RATE_REQ:
      status = MxL101SF_API_DemodGetTpsCodeRate((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_HIERARCHY_REQ:
      status = MxL101SF_API_DemodGetTpsHierarchy((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;
      
    case MXL_DEMOD_TPS_CONSTELLATION_REQ:
      status = MxL101SF_API_DemodGetTpsConstellation((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_FFT_MODE_REQ:
      status = MxL101SF_API_DemodGetTpsFftMode((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;
  
    case MXL_DEMOD_TPS_HIERARCHICAL_ALPHA_REQ:
      status = MxL101SF_API_DemodGetHierarchicalAlphaValue((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_GUARD_INTERVAL_REQ:
      status = MxL101SF_API_DemodGetTpsGuardInterval((PMXL_DEMOD_TPS_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_LOCK_REQ:
      status = MxL101SF_API_DemodGetTpsLock((PMXL_DEMOD_LOCK_STATUS_T)ParamPtr);
      break;

    case MXL_DEMOD_TPS_CELL_ID_REQ:
      status = MxL101SF_API_DemodGetCellId((PMXL_DEMOD_CELL_ID_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_PACKET_ERROR_COUNT_REQ:
      status = MxL101SF_API_DemodGetPacketErrorCount((PMXL_DEMOD_PEC_INFO_T)ParamPtr);
      break;

    case MXL_DEMOD_SYNC_LOCK_REQ:
      status = MxL101SF_API_DemodGetSyncLockStatus((PMXL_DEMOD_LOCK_STATUS_T)ParamPtr);
      break;
      
    case MXL_DEMOD_RS_LOCK_REQ:
      status = MxL101SF_API_DemodGetRsLockStatus((PMXL_DEMOD_LOCK_STATUS_T)ParamPtr);
      break;

    case MXL_DEMOD_CP_LOCK_REQ:
      status = MxL101SF_API_DemodGetCpLockStatus((PMXL_DEMOD_LOCK_STATUS_T)ParamPtr);
      break;

    default:
      break;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_ConfigDemod
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 08/08/2009
--|                 01/14/2009
--|
--| DESCRIPTION   : The demod block specific configuration shall be handled 
--|                 through this API
--|                 - Support for MXL_DEMOD_STREAM_PRIORITY_CFG
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_ConfigDemod(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;

  switch (CmdType)
  {
    case MXL_DEMOD_RESET_IRQ_CFG:
      status = MxL101SF_API_DemodResetIrqStatus();
      break;

    case MXL_DEMOD_RESET_PEC_CFG:  
      status = MxL101SF_API_DemodResetPacketErrorCount();
      break;

    case MXL_DEMOD_TS_PRIORITY_CFG:
      status = MxL101SF_API_ConfigStreamPriority((PMXL_DEMOD_TS_PRIORITY_CFG_T)ParamPtr);
      break;

    default:
      break;
  }

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_ConfigTuner
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 08/08/2009
--|
--| DESCRIPTION   : The tuner block specific configuration shall be handled 
--|                 through this API
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_ConfigTuner(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;
  
  switch (CmdType)
  {
    case MXL_TUNER_TOP_MASTER_CFG:
      status = MxL101SF_API_TopMasterControl((PMXL_TOP_MASTER_CFG_T)ParamPtr);
      break;

    case MXL_TUNER_CHAN_TUNE_CFG:
      status = MxL101SF_API_TuneRF((PMXL_RF_TUNE_CFG_T)ParamPtr);
      break;

    case MXL_TUNER_IF_OUTPUT_FREQ_CFG: 
      status = MxL101SF_API_TunerSetIFOutputFreq((PMXL_TUNER_IF_FREQ_T)ParamPtr);
      break;

    default:
      break;
  }
    
  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_API_GetTunerStatus
--| 
--| AUTHOR        : Mahendra Kondur
--|
--| DATE CREATED  : 08/08/2009
--|
--| DESCRIPTION   : The tuner specific inquiries shall be handled 
--|                 through this API
--|
--| RETURN VALUE  : MXL_STATUS
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS MxLWare_API_GetTunerStatus(MXL_CMD_TYPE_E CmdType, void *ParamPtr)
{
  MXL_STATUS status = MXL_TRUE;

  switch (CmdType)
  {
    case MXL_TUNER_LOCK_STATUS_REQ:
      status = MxL101SF_API_TunerGetLockStatus((PMXL_TUNER_LOCK_STATUS_T)ParamPtr);
      break;

    case MXL_TUNER_SIGNAL_STRENGTH_REQ:
      status = MxL101SF_API_GetTunerSignalStrength((PMXL_SIGNAL_STATS_T)ParamPtr);
    default:
      break;
  }

  return status;
}

