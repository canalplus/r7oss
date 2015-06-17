
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


#include "mxlware_hydra_commonapi.h"
#include "mxlware_hydra_commands.h"
#include "mxlware_hydra_phyctrl.h"
#include "mxlware_hydra_registers.h"
#include "mxlware_hydra_oem_drv.h"
#include "mxlware_fw_download.h"
//#include"globaldefs.h" //ot support WAIT_N_MS


/* MxLWare Driver version for Hydra device */
static const UINT8 MxLWare_HYDRA_Version[] = {2, 1, 1, 7, 100};

static MXL_BOOL_E MxL_Ctrl_IsFirmwareAlive(UINT8 devId)
{
  UINT8 mxlStatus = MXL_SUCCESS;
  UINT32 fwHeartBeat = 0;
  UINT32 regData = 0;
  MXL_BOOL_E fwActiveFlag = MXL_FALSE;

  mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_HEAR_BEAT, &fwHeartBeat); 
  MxLWare_HYDRA_OEM_SleepInMs(10);
  mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_HEAR_BEAT, &regData); 

  if ((fwHeartBeat) && (regData) && (!mxlStatus))
  {
    if ((regData - fwHeartBeat) == 0) fwActiveFlag = MXL_FALSE;
    else fwActiveFlag = MXL_TRUE;
  }

  return fwActiveFlag;
}

static MXL_BOOL_E MxL_Ctrl_ValidateSku(MXL_HYDRA_CONTEXT_T * devHandlePtr)
{
  MXL_BOOL_E skuOK = MXL_TRUE;
  MXL_STATUS_E status;
  UINT32 regData = 0;
    
  status = GET_REG_FIELD_DATA(devHandlePtr->devId, PAD_MUX_BOND_OPTION, &regData);
  if (status == MXL_SUCCESS)
  {
    switch(regData)
    {
      case MXL_HYDRA_SKU_ID_581:
        skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_581)?MXL_TRUE:MXL_FALSE;
        MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL581 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
      break;
      case MXL_HYDRA_SKU_ID_584:
        skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_584)?MXL_TRUE:MXL_FALSE;
        MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL584 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
      break;
      case MXL_HYDRA_SKU_ID_585:
        skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_585)?MXL_TRUE:MXL_FALSE;
        MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL585 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
      break;
      case MXL_HYDRA_SKU_ID_544:
        skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_544)?MXL_TRUE:MXL_FALSE;
        MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL544 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
      break;
      case MXL_HYDRA_SKU_ID_582:
        skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_582)?MXL_TRUE:MXL_FALSE;
        MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL582 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
      break;

      case MXL_HYDRA_SKU_ID_561:
      {
        
        UINT32 skuData;            
        
        status = GET_REG_FIELD_DATA(devHandlePtr->devId, PRCM_PRCM_CHIP_ID, &regData);

        if (regData == 0x560)
        {
          skuOK = (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_568)?MXL_TRUE:MXL_FALSE;
          MXLDBG1(MXL_HYDRA_PRINT("SKU selection MxL568 %s\n", (skuOK == MXL_TRUE)?"correct":"incorrect"););
        }
        else
        {
          status = SET_REG_FIELD_DATA(devHandlePtr->devId,PAD_MUX_DIGIO_01_PINMUX_SEL, 0x5)
          status |= GET_REG_FIELD_DATA(devHandlePtr->devId, PAD_MUX_GPIO_01_SYNC_IN, &skuData)

		      if (status == MXL_SUCCESS)
		      {
			      skuOK = (((devHandlePtr->deviceType == MXL_HYDRA_DEVICE_561) && (skuData == 0)) ||
					           ((devHandlePtr->deviceType == MXL_HYDRA_DEVICE_541) && (skuData == 1)))?MXL_TRUE:MXL_FALSE;
		      }
        }

      }
      break;
      
      default:
        skuOK = MXL_FALSE; 
        break;
    }
  } else skuOK = MXL_FALSE;

  return skuOK;
}
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevXtal
 *
 * @param[in]   devId        Device ID
 * @param[in]   xtalParamPtr
 * @param[in]   enableClkOut
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *       11/15/2012 - Update XTAL config code based on new peograming seq.
 *                    Move XTAL cal to FW.
 *
 * This API should be used to configure device XTAL configuration including
 * XTAL frequency, XTAL Capacitance and also control CLK_OUT feature.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevXtal(UINT8 devId, MXL_HYDRA_DEV_XTAL_T * xtalParamPtr, MXL_BOOL_E enableClkOut)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 status = (UINT8)MXL_SUCCESS;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("xtalFreqId=%d, xtalCap=%d, clkOut=%s\n",
                                            (xtalParamPtr) ? xtalParamPtr->xtalFreq : 0,
                                            (xtalParamPtr) ? xtalParamPtr->xtalCap : 0,
                                            (enableClkOut == MXL_ENABLE) ? "on" : "off" ););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {

    // if clock out feature is not available and user sends enable clock out
    if ((devHandlePtr->features.clkOutAvailable == MXL_FALSE) && (enableClkOut == MXL_ENABLE))
    {
      status = MXL_NOT_SUPPORTED;
      MXLERR(MXL_HYDRA_PRINT("[%s] Clkout is not supported in this device\n", __FUNCTION__););
    }
    else
    {
      // Program Clock out
      // AFE_REG_D2A_XTAL_CAL_TSTEP = 1
      status |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_XTAL_EN_CLKOUT_1P8, enableClkOut);

      if ((xtalParamPtr) && (xtalParamPtr->xtalCap >= MXL_HYDRA_CAP_MIN) && (xtalParamPtr->xtalCap <= MXL_HYDRA_CAP_MAX))
      {
        // program XTAL frequency
        if (xtalParamPtr->xtalFreq == MXL_HYDRA_XTAL_24_MHz)
          status |= MxLWare_HYDRA_WriteRegister(devId, HYDRA_CRYSTAL_SETTING, 0);
        else
          status |= MxLWare_HYDRA_WriteRegister(devId, HYDRA_CRYSTAL_SETTING, 1);

        // Update device context for XTAL value
        devHandlePtr->xtalFreq = xtalParamPtr->xtalFreq;

        // program XTAL cap value
        status |= MxLWare_HYDRA_WriteRegister(devId, HYDRA_CRYSTAL_CAP, xtalParamPtr->xtalCap);

      }
      else
      {
        status |= MXL_INVALID_PARAMETER;
        MXLERR(MXL_HYDRA_PRINT("[%s] xtalCap should be in range <%d;%d>\n", __FUNCTION__, MXL_HYDRA_CAP_MIN, MXL_HYDRA_CAP_MAX););
      }

    }
  }

  MXLEXITAPISTR(devId, status);

  return (MXL_STATUS_E)status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevFWDownload
 *
 * @param[in]   devId        Device ID
 * @param[in]   mbinBufferSize
 * @param[in]   mbinBufferPtr
 * @param[in]   fwCallbackFn
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release 
 * @date 07/16/2012 Update API to new programming flow change
 * 
 * This API downloads Hydra firmware stored in buffer (mbinBufferPtr) to the 
 * device through I2C interface. If fwCallbackFn is defined (not NULL), it 
 * will be called after each segment is downloaded.  This callback routine 
 * can be used to monitor firmware download progress.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevFWDownload(UINT8 devId, UINT32 mbinBufferSize, UINT8 *mbinBufferPtr, MXL_CALLBACK_FN_T fwCallbackFn)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT32 regData = 0;
  MXL_HYDRA_SKU_COMMAND_T devSkuCfg;
  UINT8 cmdSize = sizeof(MXL_HYDRA_SKU_COMMAND_T);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
      
  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("mbinBufferSize=%d, mbinBufferPtr=%p, callback %s\n", mbinBufferSize, mbinBufferPtr, (fwCallbackFn)?"defined":"not defined"););

  // Get MxLWare driver context
  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    // validate input parameters
    if ((mbinBufferSize) && (mbinBufferPtr))
    {

      // Keep CPU under reset
      //mxlStatus = MxLWare_Hydra_UpdateByMnemonic(devId, PRCM_PRCM_CPU_SOFT_RST_N, 0);
      mxlStatus |= SET_REG_FIELD_DATA(devId, PRCM_PRCM_CPU_SOFT_RST_N, 0);
      if (MXL_SUCCESS == mxlStatus)
      {
        // Delay of 100 ms
        MxLWare_HYDRA_OEM_SleepInMs(1);

        // Reset TX FIFO's
        mxlStatus = MxLWare_HYDRA_WriteRegister(devId, HYDRA_RESET_TRANSPORT_FIFO_REG, HYDRA_RESET_TRANSPORT_FIFO_DATA);
        if (MXL_SUCCESS == mxlStatus)
        {
          // Reset baseband, widebands, SerDes & ADC
          mxlStatus = MxLWare_HYDRA_WriteRegister(devId, HYDRA_RESET_BBAND_REG, HYDRA_RESET_BBAND_DATA);
          if (MXL_SUCCESS == mxlStatus)
          {
            // Reset XBAR
            mxlStatus = MxLWare_HYDRA_WriteRegister(devId, HYDRA_RESET_XBAR_REG, HYDRA_RESET_XBAR_DATA);
            if (MXL_SUCCESS == mxlStatus)
            {
              // Disable clock to Baseband, Wideband, SerDes, Alias ext & Transport modules
              mxlStatus = MxLWare_HYDRA_WriteRegister(devId, HYDRA_MODULES_CLK_2_REG, HYDRA_DISABLE_CLK_2);
              if (MXL_SUCCESS == mxlStatus)
              {
                // Clear Software & Host interrupt status - (Clear on read)
                mxlStatus = MxLWare_HYDRA_ReadRegister(devId, HYDRA_PRCM_ROOT_CLK_REG, &regData);
                if (MXL_SUCCESS == mxlStatus)
                {
                  // Verify Firmware header
                  mxlStatus = MxLWare_FW_Download(devId, mbinBufferSize, mbinBufferPtr, fwCallbackFn);
                  if (MXL_SUCCESS == mxlStatus)
                  {

#ifdef __MXL_SCORPIUS__

                    if (devHandlePtr->deviceType == MXL_HYDRA_DEVICE_568)
                    {
                      MxLWare_HYDRA_OEM_SleepInMs(10);

                      // bring XCPU out of reset
                      MxLWare_HYDRA_WriteRegister(devId, 0x90720000, 1);
                      MxLWare_HYDRA_OEM_SleepInMs(500);

                      // Enable XCPU UART message processing in MCPU
                      MxLWare_HYDRA_WriteRegister(devId, 0x9076B510, 1);

                    }

#else

                    // Bring CPU out of reset
                    //mxlStatus = MxLWare_Hydra_UpdateByMnemonic(devId, PRCM_PRCM_CPU_SOFT_RST_N, 1);
                    mxlStatus = SET_REG_FIELD_DATA(devId, PRCM_PRCM_CPU_SOFT_RST_N, 1);
                    //Wait till FW Boots
                    MxLWare_HYDRA_OEM_SleepInMs(150);                    

#endif
                    
                    // Check if Firmware is alive
                    if (MXL_TRUE == MxL_Ctrl_IsFirmwareAlive(devId)) 
                    {
                      devSkuCfg.skuType = devHandlePtr->skuType;

                      // buid and send command to configure interrupt settings
                      BUILD_HYDRA_CMD(MXL_HYDRA_DEV_CFG_SKU_CMD, MXL_CMD_WRITE, cmdSize, &devSkuCfg, cmdBuff);
                      mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

                    }
                    else 
                      mxlStatus = MXL_FAILURE;

                  }

                }
              }
            }
          }
        }
      }
    } else mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevInterrupt
 *
 * @param[in]   devId        Device ID
 * @param[in]   intrCfg      Device Interrupt configuration
 * @param[in]   intrMask     Interrupt mask
 *
 * @author Mahee
 *
 * @date 06/12/2012
 *
 * This API should be used to enable or disable specific interrupt
 * sources of the device along with type of INTR required.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevInterrupt(UINT8 devId, MXL_HYDRA_INTR_CFG_T intrCfg, UINT32 intrMask)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 status = 0;
  MXL_INTR_CFG_T devIntrCfg;
  UINT8 cmdSize = sizeof(MXL_INTR_CFG_T);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("intrMask = %x\n", intrMask););

  status |= MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    devIntrCfg.intrMask = intrMask;
    devIntrCfg.intrType = (UINT32)intrCfg.intrType;
    devIntrCfg.intrDurationInNanoSecs = intrCfg.intrDurationInNanoSecs;

    // buid and send command to configure interrupt settings
    BUILD_HYDRA_CMD(MXL_HYDRA_DEMOD_INTR_TYPE_CMD, MXL_CMD_WRITE, cmdSize, &devIntrCfg, cmdBuff);
    status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

    if (status == MXL_SUCCESS)
    {
      devHandlePtr->intrMask = intrMask;
    }

  }

  MXLEXITAPISTR(devId, status);
  return (MXL_STATUS_E)status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgClearDevtInterrupt
 *
 * @param[in]   devId        Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should be used to clear device interrupt.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgClearDevtInterrupt(UINT8 devId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    // buid and send command to clear interrupt settings
    cmdBuff[0] = MXL_HYDRA_PLID_CMD_WRITE;
    cmdBuff[1] = (UINT8)(sizeof(UINT32));
    cmdBuff[2] = (UINT8)0;
    cmdBuff[3] = (UINT8)MXL_HYDRA_DEV_INTR_CLEAR_CMD;                                                                      
    cmdBuff[4] = 0x00;
    cmdBuff[5] = 0x00;

    status = MxLWare_HYDRA_SendCommand(devId, (6 * sizeof(UINT8)), &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, status);
  return (MXL_STATUS_E)status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDevInterruptStatus
 *
 * @param[in]   devId        Device ID
 * @param[out]  intrStatusPtr
 * @param[out]  intrMaskPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should be used to retrieve interrupt status information from
 * the device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDevInterruptStatus(UINT8 devId, UINT32 * intrStatusPtr, UINT32 * intrMaskPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status = MXL_INVALID_PARAMETER;
  UINT32 regData = 0;
  UINT32 tempRegData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT(" "););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    if ((intrStatusPtr) && (intrMaskPtr))
    {
      // read device interrupt status
      *intrStatusPtr = 0;
      *intrMaskPtr = 0;
      status = MxLWare_HYDRA_ReadRegister(devId, HYDRA_INTR_MASK_REG, &regData);
      //This is done to clear the interrupt status and re-read the status
      status = MxLWare_HYDRA_ReadRegister(devId, HYDRA_INTR_STATUS_REG, &tempRegData);
      regData |= tempRegData;

      if (status == MXL_SUCCESS) 
      {
        *intrStatusPtr = regData;
        *intrMaskPtr = devHandlePtr->intrMask;

        status = GET_REG_FIELD_DATA(devId, WDT_WD_INT_STATUS, &regData);
        if (status == MXL_SUCCESS) 
        {
          if (regData == 1)
          {
            status = SET_REG_FIELD_DATA(devId, FSK_TX_FTM_OE, 0);
            status |= SET_REG_FIELD_DATA(devId, FSK_TX_FTM_TX_EN, 0);
            status |= SET_REG_FIELD_DATA(devId, FSK_TX_FTM_TX_INT_SRC_SEL, 0);
            status |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_FSK_TERM_INT_EN, 1);
            status |= SET_REG_FIELD_DATA(devId, AFE_REG_D2A_FSK_RESETB_1P8, 1);
            status |= MxLWare_HYDRA_WriteRegister(devId, 0x3FFFEB5C, 0x0); 
            *intrStatusPtr |= (1 << 29);
          }
          else if (regData == 0)
          {
            *intrStatusPtr &=  ~(1 << 29);
          }
        }
      }

    }
    else
      status = MXL_INVALID_PARAMETER;

  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDevVersionInfo
 *
 * @param[in]   devId        Device ID
 * @param[out]  versionInfoPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This function returns Hydra device identification data and version info.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDevVersionInfo(UINT8 devId, MXL_HYDRA_VER_INFO_T *versionInfoPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus = 0;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("MxLWare_HYDRA_API_ReqDevVersionInfo ++"););

  mxlStatus |= MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (versionInfoPtr)
    {
      versionInfoPtr->chipId = devHandlePtr->deviceType;
      versionInfoPtr->chipVer = devHandlePtr->chipVersion;

      // MxLware Version
      versionInfoPtr->mxlWareVer[0] = MxLWare_HYDRA_Version[0];
      versionInfoPtr->mxlWareVer[1] = MxLWare_HYDRA_Version[1];
      versionInfoPtr->mxlWareVer[2] = MxLWare_HYDRA_Version[2];
      versionInfoPtr->mxlWareVer[3] = MxLWare_HYDRA_Version[3];
      versionInfoPtr->mxlWareVer[4] = MxLWare_HYDRA_Version[4];

      // Firmware status - Check if heart beat is active
      versionInfoPtr->firmwareDownloaded = MxL_Ctrl_IsFirmwareAlive(devId);

      if (versionInfoPtr->firmwareDownloaded == MXL_TRUE)
      {
        // Read Firmware version
        mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_FIRMWARE_VERSION, &regData); 
        versionInfoPtr->firmwareVer[0] = GET_BYTE(regData,3);
        versionInfoPtr->firmwareVer[1] = GET_BYTE(regData,2);
        versionInfoPtr->firmwareVer[2] = GET_BYTE(regData,1);
        versionInfoPtr->firmwareVer[3] = GET_BYTE(regData,0);

        // Read Firmware RC version
        mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_FW_RC_VERSION, &regData); 
        versionInfoPtr->firmwareVer[4] = GET_BYTE(regData,0); 
        
#if 1
        // Read Firmware Patch version
        mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_FIRMWARE_PATCH_VERSION, &regData); 
        versionInfoPtr->fwPatchVer[0] = 0; // GET_BYTE(regData,0);
        versionInfoPtr->fwPatchVer[1] = 0; // GET_BYTE(regData,1);
        versionInfoPtr->fwPatchVer[2] = 0; // GET_BYTE(regData,2);
        versionInfoPtr->fwPatchVer[3] = 0; // GET_BYTE(regData,3);
        versionInfoPtr->fwPatchVer[4] = 0; 
#endif
      }
    } 
    else 
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLENTERAPI(MXL_HYDRA_PRINT("MxLWare_HYDRA_API_ReqDevVersionInfo --"););

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevPowerMode
 *
 * @param[in]   devId        Device ID
 * @param[out]  pwrMode
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API can be called to configure Hydra device into a required power
 * mode. Please refer to corresponding device’s datasheet for power
 * consumption values.

 * Power Mode	Description
 * MXL_HYDRA_PWR_MODE_ACTIVE
 *       All tuners and demods are active. Hydra is normal working mode.
 * MXL_HYDRA_PWR_MODE_STANDBY
 *       Power saving mode where all tuners and demods are inactive.
 * MXL_HYDRA_PWR_MODE_LP_TUNER
 *       Only low power tuner (network tuner) will be active.
 * MXL_HYDRA_PWR_MODE_WAKE_ON_RF
 *       Device is configured to a low power sleep mode and can be woken
 *       up by RF input signal.
 * MXL_HYDRA_PWR_MODE_WAKE_ON_BROADCAST
 *       Device is configured to a low power single tuner mode and can
 *       be woken up by a magic packet broadcast from satellite.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevPowerMode(UINT8 devId, MXL_HYDRA_PWR_MODE_E pwrMode)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 status = 0;

  MxL_HYDRA_POWER_MODE_CMD devPowerCfg;
  UINT8 cmdSize = sizeof(devPowerCfg);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("pwrMode=%d\n", pwrMode););

  status |= MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    devPowerCfg.powerMode = pwrMode;

    // buid and send command to configure device power mode
    BUILD_HYDRA_CMD(MXL_HYDRA_DEV_CFG_POWER_MODE_CMD, MXL_CMD_WRITE, cmdSize, &devPowerCfg, cmdBuff);
    status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

  }

  MXLEXITAPISTR(devId, status);
  return (MXL_STATUS_E)status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevWakeOnRF
 *
 * @param[in]   devId        Device ID
 * @param[in]   tunerId
 * @param[in]   wakeTimeInSeconds
 * @param[in]   rssiThreshold
 *
 * @author Mahee
 *
 * @date 02/21/2013 Initial release
 *
 * This API configures wake on RF fetaure of HYDRA.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevWakeOnRF(UINT8 devId, MXL_HYDRA_TUNER_ID_E tunerId, UINT32 wakeTimeInSeconds, SINT32 rssiThreshold)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 status = 0;

  MXL_HYDRA_RF_WAKEUP_CFG_T rfWakeCfg;
  UINT8 cmdSize = sizeof(rfWakeCfg);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("tunerId=%d, wakeTimeInSeconds=%d rssiThreshold=%d\n", tunerId, wakeTimeInSeconds, rssiThreshold););

  status |= MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    rfWakeCfg.tunerCount = 1;
    rfWakeCfg.params.tunerIndex = tunerId;
    rfWakeCfg.params.timeIntervalInSeconds = wakeTimeInSeconds;
    rfWakeCfg.params.rssiThreshold = rssiThreshold;

    // buid and send command to configure wake on RF feature
    BUILD_HYDRA_CMD(MXL_HYDRA_DEV_RF_WAKE_UP_CMD, MXL_CMD_WRITE, cmdSize, &rfWakeCfg, cmdBuff);
    status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, status);
  return (MXL_STATUS_E)status;

}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevWakeOnPidParams
 *
 * @param[in]   devId        Device ID
 * @param[in]   enable
 * @param[in]   wakeOnPidPtr
 * @param[in]   pidSearchTimeInMsecs
 * @param[in]   demodSleepTimeInMsecs
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API configures PIDs for use with wake on broadcast feature of the device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevWakeOnPidParams(UINT8 devId, MXL_BOOL_E enable,
                              MXL_HYDRA_WAKE_ON_PID_T *wakeOnPidPtr,
                              UINT32 pidSearchTimeInMsecs, UINT32 demodSleepTimeInMsecs)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;
  MXL_HYDRA_BROADCAST_WAKEUP_CFG_T broadcastWakeCfg;
  UINT8 cmdSize = sizeof(broadcastWakeCfg);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT16 i;
  UINT16 temp;


  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("enable=%d, pidSearchTimeInMsecs=%d, demodSleepTimeInMsecs=%d\n", enable, pidSearchTimeInMsecs, demodSleepTimeInMsecs););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    if (wakeOnPidPtr)
    {
	  MXLWARE_OSAL_MEMSET(broadcastWakeCfg.wakeOnPid.wakeUpPid, 0, 16);
	  memcpy(&broadcastWakeCfg.wakeOnPid, wakeOnPidPtr, sizeof(MXL_HYDRA_WAKE_ON_PID_T));
	  broadcastWakeCfg.enable = enable;
      broadcastWakeCfg.pidSearchTimeInMsecs = pidSearchTimeInMsecs;
	  broadcastWakeCfg.demodSleepTimeInMsecs = demodSleepTimeInMsecs;

		MXL_HYDRA_PRINT("PID List: Before\n");
		for(i=0;i<broadcastWakeCfg.wakeOnPid.numPids;i++)
		{
			MXL_HYDRA_PRINT("%d\n", broadcastWakeCfg.wakeOnPid.wakeUpPid[i]);
		}

		// convert data to componsate for I2C data swapping in FW layer
		for(i=0;i<broadcastWakeCfg.wakeOnPid.numPids;i=i+2)
		{
			/*wakeOnPidPtr.wakeUpPid[i] ^= wakeOnPidPtr.wakeUpPid[i+1];
			wakeOnPidPtr.wakeUpPid[i+1] ^= wakeOnPidPtr.wakeUpPid[i];
			wakeOnPidPtr.wakeUpPid[i] ^= wakeOnPidPtr.wakeUpPid[i+1];*/

			temp = broadcastWakeCfg.wakeOnPid.wakeUpPid[i];
			broadcastWakeCfg.wakeOnPid.wakeUpPid[i] = broadcastWakeCfg.wakeOnPid.wakeUpPid[i+1];
			broadcastWakeCfg.wakeOnPid.wakeUpPid[i+1] = temp;
		}

      // buid and send command to configure wake on RF feature
      BUILD_HYDRA_CMD(MXL_HYDRA_DEV_BROADCAST_WAKE_UP_CMD, MXL_CMD_WRITE, cmdSize, &broadcastWakeCfg, cmdBuff);
      status |= MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
    }
    else
      status = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevStartBroadcastWakeUp
 *
 * @param[in]   devId        Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API will wakeup Hydra device to search for matching pid from TS
 * when a device is configured in host driven
 * â€œMXL_HYDRA_PWR_MODE_WAKE_ON_BROADCASTâ€ power mode.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevStartBroadcastWakeUp(UINT8 devId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT(" "););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {

  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevOverwriteDefault
 *
 * @param[in]   devId        Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API configures Hydra devices control registers to their updated
 * default values. This API will initialize all modules and their
 * related sub-modules
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDevOverwriteDefault(UINT8 devId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status = MXL_SUCCESS;
  
  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT(" "););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
	if(MxL_Ctrl_ValidateSku(devHandlePtr) == MXL_TRUE)
	{
		// Enable AFE
		status |= SET_REG_FIELD_DATA(devId, PRCM_AFE_REG_CLOCK_ENABLE, 1);  

		//status |= MxLWare_Hydra_UpdateByMnemonic(devId, PRCM_PRCM_AFE_REG_SOFT_RST_N, 1);  
		if (status == MXL_SUCCESS)
		{
		  status |= SET_REG_FIELD_DATA(devId, PRCM_PRCM_AFE_REG_SOFT_RST_N, 1);  
		  if (status == MXL_SUCCESS)
		  {
			  UINT32 chipVer;
			  status |= GET_REG_FIELD_DATA(devId, PRCM_CHIP_VERSION, &chipVer);
			  if (status == MXL_SUCCESS)
			  {
			    devHandlePtr->chipVersion = (chipVer == 2)?2:1; 
			    MXLDBG1(MXL_HYDRA_PRINT("[hydra] chip version: %d\n", devHandlePtr->chipVersion););
			  } 
        else 
          devHandlePtr->chipVersion = 0;
		  }
		}
	}
	else
		status = MXL_FAILURE;
  }

  MXLEXITAPISTR(devId, status);
  return (MXL_STATUS_E)status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevDiseqcFskOpMode
 *
 * @param[in]   devId           Device ID
 * @param[in]   devAuxCtrlMode
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release 
 * 
 * This API should be used to enable either DiSEqC or FSK (SWM).
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E  MxLWare_HYDRA_API_CfgDevDiseqcFskOpMode(UINT8 devId, MXL_HYDRA_AUX_CTRL_MODE_E devAuxCtrlMode)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("devAuxCtrlMode=%d\n", devAuxCtrlMode););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    // update device context
    if (devAuxCtrlMode == MXL_HYDRA_AUX_CTRL_MODE_FSK)
    {
      devHandlePtr->features.FSKEnabled = MXL_TRUE;
      devHandlePtr->features.DiSEqCEnabled = MXL_FALSE;
    }
    else
    {
      devHandlePtr->features.FSKEnabled = MXL_FALSE;
      devHandlePtr->features.DiSEqCEnabled = MXL_TRUE;
    }

  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevGPIODirection
 *
 * @param[in]   devId           Device ID
 * @param[in]   gpioId
 * @param[in]   gpioDir
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should be used to configure device GPIO pin’s direction
 * (input or output).
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E  MxLWare_HYDRA_API_CfgDevGPIODirection(UINT8 devId, UINT8 gpioId, MXL_HYDRA_GPIO_DIR_E gpioDir)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("gpioId=%d, gpioDir=%d\n", gpioId, gpioDir););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {

  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDevGPOData
 *
 * @param[in]   devId           Device ID
 * @param[in]   gpioId
 * @param[in]   gpioData
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API writes data to GPIO pin when the pin is used as output.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E  MxLWare_HYDRA_API_CfgDevGPOData(UINT8 devId, UINT8 gpioId, MXL_HYDRA_GPIO_DATA_E gpioData)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("gpioId=%d, gpioData=%d\n", gpioId, gpioData););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {

  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDevGPIData
 *
 * @param[in]   devId           Device ID
 * @param[in]   gpioId
 * @param[out]   gpioDataPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should be used to read state of GPIO pin when the pin is
 * used as input.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E  MxLWare_HYDRA_API_ReqDevGPIData(UINT8 devId, UINT8 gpioId, MXL_HYDRA_GPIO_DATA_E *gpioDataPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("gpioId=%d\n", gpioId););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    if (gpioDataPtr)
    {

    }
    else
      status = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDevTemperature
 *
 * @param[in]   devId           Device ID
 * @param[out]  devTemperatureInCelsiusPtr - Current device temperature.
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should be used to retrieve current device temperature.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E  MxLWare_HYDRA_API_ReqDevTemperature(UINT8 devId, SINT16 *devTemperatureInCelsiusPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("devId=%d\n", devId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (devTemperatureInCelsiusPtr)
    {
      mxlStatus = MxLWare_HYDRA_ReadRegister(devId, HYDRA_TEMPARATURE, &regData);

      *devTemperatureInCelsiusPtr = (SINT16)(regData & 0xFFFF);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}
