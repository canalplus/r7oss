
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
#include "mxlware_hydra_commands.h"


/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ResetFsk
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 09/20/2012 Initial release
 *
 * The API is used to reset FSK block.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ResetFsk(UINT8 devId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("Device ID=%d\n", devId););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    cmdBuff[0] = MXL_HYDRA_PLID_CMD_WRITE;
    cmdBuff[1] = (UINT8)(sizeof(UINT32));
    cmdBuff[2] = (UINT8)0;
    cmdBuff[3] = (UINT8)MXL_HYDRA_FSK_RESET_CMD;
    cmdBuff[4] = 0x00;
    cmdBuff[5] = 0x00;

    // send FSK reset command to the device
    status = MxLWare_HYDRA_SendCommand(devId, (6 * sizeof(UINT8)), &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, status);
  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDiseqcOpMode
 *
 * @param[in]   devId           Device ID
 * @param[in]   diseqcId
 * @param[in]   opMode
 * @param[in]   version
 * @param[in]   carrierFreqInHz
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * The API is used to configure DiSEqC interface in Hydra. There are 3 DiSEqC 1.x
 * interfaces and 1 DiSEqC 2.x interface. In case of DiSEqC 2.x interface, host
 * has to configure the appropriate DiSEqC ID based on device configuration.
 * Hydra SOC supports three different frequencies of 22 KHz, 33 KHz & 44 KHz
 * for communicating with DiSEqC slave.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDiseqcOpMode(UINT8 devId,
                                               MXL_HYDRA_DISEQC_ID_E diseqcId,
                                               MXL_HYDRA_DISEQC_OPMODE_E opMode,
                                               MXL_HYDRA_DISEQC_VER_E version,
                                               MXL_HYDRA_DISEQC_CARRIER_FREQ_E carrierFreqInHz)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXL58x_DSQ_OP_MODE_T diseqcMsg;
  UINT8 cmdSize = sizeof(diseqcMsg);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("diseqcId=%d, opMode=%d, version=%d, carrierFreqInHz=%d\n", diseqcId, opMode, version, carrierFreqInHz););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    diseqcMsg.diseqcId = diseqcId;
    diseqcMsg.centerFreq = carrierFreqInHz;
    diseqcMsg.version = version;
    diseqcMsg.opMode = opMode;

    BUILD_HYDRA_CMD(MXL_HYDRA_DISEQC_CFG_MSG_CMD, MXL_CMD_WRITE, cmdSize, &diseqcMsg, cmdBuff);

    status = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
    
  }

  MXLEXITAPISTR(devId, status);
  return status;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_CfgDiseqcOpMode);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDiseqcContinuousToneCtrl
 *
 * @param[in]   devId           Device ID
 * @param[in]   diseqcId
 * @param[in]   continuousToneCtrl
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * The API is used to configure DiSEqC interface in Hydra. There are 3 DiSEqC 1.x
 * interfaces and 1 DiSEqC 2.x interface. In case of DiSEqC 2.x interface, host
 * has to configure the appropriate DiSEqC ID based on device configuration.
 * Hydra SOC supports three different frequencies of 22 KHz, 33 KHz & 44 KHz
 * for communicating with DiSEqC slave.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDiseqcContinuousToneCtrl(UINT8 devId,
                                                            MXL_HYDRA_DISEQC_ID_E diseqcId,
                                                            MXL_BOOL_E continuousToneCtrl)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXL_HYDRA_DISEQC_CFG_CONT_TONE_T diseqcContinuousTone;
  UINT8 cmdSize = sizeof(diseqcContinuousTone);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("diseqcId=%d, continuousToneCtrl=%d\n", diseqcId, continuousToneCtrl););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    diseqcContinuousTone.diseqcId = diseqcId;
    diseqcContinuousTone.contToneFlag = continuousToneCtrl;

    BUILD_HYDRA_CMD(MXL_HYDRA_DISEQC_CONT_TONE_CFG, MXL_CMD_WRITE, cmdSize, &diseqcContinuousTone, cmdBuff);

    status = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
    
  }

  MXLEXITAPISTR(devId, status);
  return status;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_CfgDiseqcContinuousToneCtrl);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDiseqcStatus
 *
 * @param[in]   devId           Device ID
 * @param[in]   diseqcId
 * @param[out]  statusPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to retrieves DiSEqC module’s status.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDiseqcStatus(UINT8 devId, MXL_HYDRA_DISEQC_ID_E diseqcId, UINT32 *statusPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("diseqcId = %d\n", diseqcId););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    if (statusPtr)
    {
      // read diseqc status
      status = MxLWare_HYDRA_ReadRegister(devId,
                                          (DISEQ0_STATUS_REG + (diseqcId * (sizeof(UINT32)))),
                                          &regData);

      if (status == MXL_SUCCESS) 
	  {
	    *statusPtr = regData;
	    // reset diseqc status
	    status = MxLWare_HYDRA_WriteRegister(devId,
		     								(DISEQ0_STATUS_REG + (diseqcId * (sizeof(UINT32)))),
			    							0);

	  }
      else
	  {
	    *statusPtr = 0;
	  }

    } 
    else
      status = MXL_INVALID_PARAMETER;
  }

  MXLEXITAPISTR(devId, status);
  return status;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_ReqDiseqcStatus);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqDiseqcRead
 *
 * @param[in]   devId           Device ID
 * @param[in]   diseqcId
 * @param[out]  diseqcMsgPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This function is used to read DiSEqC response message from the
 * external DiSEqC device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqDiseqcRead(UINT8 devId, MXL_HYDRA_DISEQC_ID_E diseqcId, MXL_HYDRA_DISEQC_RX_MSG_T *diseqcMsgPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 mxlStatus = (UINT8)MXL_SUCCESS;
  UINT8 regData[DISEQC_RX_BLOCK_SIZE];
  UINT32 rxMsgStatus = 0;
  MXL_BOOL_E isDataReady = MXL_FALSE;
  UINT64 startTime = 0;
  UINT64 currTime = 0;
  UINT8 i = 0;
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("diseqcId=%d", diseqcId););

  mxlStatus |= MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {

    if (diseqcMsgPtr)
    {
      // set diseqc msg avil to 0
      mxlStatus |= MxLWare_HYDRA_WriteRegister(devId,
                                               DISEQC_RX_MSG_AVIAL_ADDR,
                                               0);

      // send command to read Rx data
      cmdBuff[0] = MXL_HYDRA_PLID_CMD_WRITE;
      cmdBuff[1] = (UINT8)(sizeof(UINT32));                                            
      cmdBuff[2] = (UINT8)0;                                                                       
      cmdBuff[3] = (UINT8)MXL_HYDRA_DISEQC_COPY_MSG_TO_MAILBOX;                                                                      
      cmdBuff[4] = 0x00;                                                                              
      cmdBuff[5] = 0x00;                                                                             

      // send FSK reset command to the device
      mxlStatus |= MxLWare_HYDRA_SendCommand(devId, (6 * sizeof(UINT8)), &cmdBuff[0]);

      // Get current timestamp
      MxLWare_HYDRA_OEM_GetCurrTimeInMs(&startTime);

      // check if Rx data is ready
      do
      {
         mxlStatus |= MxLWare_HYDRA_ReadRegister(devId,
                                                  DISEQC_RX_MSG_AVIAL_ADDR,
                                                  &rxMsgStatus);

         if ((rxMsgStatus & 0x1)) isDataReady = MXL_TRUE;

         MxLWare_HYDRA_OEM_GetCurrTimeInMs(&currTime);

      } while ((isDataReady == MXL_FALSE) && ((currTime - startTime) < 200 /*  timeout of 200 ms */) );


      if (isDataReady == MXL_TRUE)
      {
        // read RX message
        mxlStatus |= MxLWare_HYDRA_ReadRegisterBlock(devId,
                                                     DISEQC_RX_BLOCK_ADDR,
                                                     DISEQC_RX_BLOCK_SIZE,
                                                     &regData[0]);

        diseqcMsgPtr->nbyte = regData[0];

        MXL_HYDRA_PRINT("DiSEqC Data : \n");
        for (i = 0; i < 32; i++)
        {
          diseqcMsgPtr->bufMsg[i] = regData[i+4];
          MXL_HYDRA_PRINT("0x%0X ", diseqcMsgPtr->bufMsg[i]);
        }

      }
      else
      {
        diseqcMsgPtr->nbyte = 0;
        mxlStatus |= MXL_FAILURE;
      }

      // set diseqc msg avil to 0
      mxlStatus |= MxLWare_HYDRA_WriteRegister(devId,
                                               DISEQC_RX_MSG_AVIAL_ADDR,
                                               0);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;

  }

  MXLEXITAPISTR(devId, mxlStatus);
  return (MXL_STATUS_E)mxlStatus;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_ReqDiseqcRead);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDiseqcWrite
 *
 * @param[in]   devId           Device ID
 * @param[in]   diseqcId
 * @param[in]   diseqcMsgPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to write DiSEqC message to an external LNB device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDiseqcWrite(UINT8 devId, MXL_HYDRA_DISEQC_TX_MSG_T *diseqcMsgPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus;
  UINT8 cmdSize = sizeof(MXL_HYDRA_DISEQC_TX_MSG_T);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT32 i  = 0;

  MXLENTERAPISTR(devId);

  if (diseqcMsgPtr)
    MXLENTERAPI(MXL_HYDRA_PRINT("diseqcId=%d %d Bytes", diseqcMsgPtr->diseqcId, diseqcMsgPtr->nbyte););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if ((mxlStatus == MXL_SUCCESS) && (diseqcMsgPtr))
  {
    if (diseqcMsgPtr->diseqcId <= MXL_HYDRA_DISEQC_ID_3) 
    {
      for (i = 0; i < diseqcMsgPtr->nbyte; i++)
      {
        MXL_HYDRA_PRINT("0x%X", diseqcMsgPtr->bufMsg[i]);
      }

      BUILD_HYDRA_CMD(MXL_HYDRA_DISEQC_MSG_CMD, MXL_CMD_WRITE, cmdSize, diseqcMsgPtr, cmdBuff);
      mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

    }
    else
      mxlStatus = MXL_INVALID_PARAMETER;

  }
  else 
    mxlStatus = MXL_INVALID_PARAMETER;

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_CfgDiseqcWrite);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgFskOpMode
 *
 * @param[in]   devId           Device ID
 * @param[in]   fskCfgType
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to configure FSK controller to FTM
 * or other specific modes supported by the device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgFskOpMode(UINT8 devId, MXL_HYDRA_FSK_OP_MODE_E fskCfgType)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT8 cmdSize = sizeof(MXL_HYDRA_FSK_OP_MODE_E);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT32 cmdData = (UINT32)fskCfgType;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("fskCfgType=%d", fskCfgType););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    BUILD_HYDRA_CMD(MXL_HYDRA_FSK_SET_OP_MODE_CMD, MXL_CMD_WRITE, cmdSize, &cmdData, cmdBuff);
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
  }

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}
EXPORT_SYMBOL(MxLWare_HYDRA_API_CfgFskOpMode);
/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_ReqFskMsgRead
 *
 * @param[in]   devId           Device ID
 * @param[out]  msgPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to read message from FSK/SWM controller.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_ReqFskMsgRead(UINT8 devId, MXL_HYDRA_FSK_MSG_T *msgPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT(" "););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    if (msgPtr)
    {
      // Read data from the device
      if (mxlStatus == MXL_SUCCESS)
      {
        mxlStatus = MxLWare_HYDRA_CheckForCmdResponse(devId, devHandlePtr, MXL_HYDRA_FSK_MSG_CMD);

        if (devHandlePtr->cmdData.dataSize)
        {
          MXLWARE_OSAL_MEMCPY(msgPtr, &devHandlePtr->cmdData.data[4], sizeof(MXL_HYDRA_FSK_MSG_T));

#if !MXL_ENABLE_BIG_ENDIAN
          // only for LE platform
          MxL_CovertDataForEndianness(!(MXL_ENABLE_BIG_ENDIAN), sizeof(UINT32), (UINT8 *)&(msgPtr->msgValidFlag));
          MxL_CovertDataForEndianness(!(MXL_ENABLE_BIG_ENDIAN), sizeof(UINT32), (UINT8 *)&(msgPtr->msgLength));
#endif
          // convert data to componsate for I2C data swapping in FW layer
          MxL_CovertDataForEndianness(1, (sizeof (UINT8) * MXL_HYDRA_FSK_MESG_MAX_LENGTH), (UINT8 *)(&msgPtr->msgBuff[0]));                                           
          MXL_HYDRA_PRINT("\r\nResp Len %d", msgPtr->msgLength);
          MXL_HYDRA_PRINT("\r\n0x%02X 0x%02X 0x%02X 0x%02X", msgPtr->msgBuff[0], 
                                                 msgPtr->msgBuff[1], 
                                                 msgPtr->msgBuff[2], 
                                                 msgPtr->msgBuff[3]);
          
        }
        else
        {
          msgPtr->msgValidFlag = 0;
        }

      }

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
 * @brief MxLWare_HYDRA_API_CfgFskMsgWrite
 *
 * @param[in]   devId           Device ID
 * @param[in]   msgPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API will write a message to FSK/SWM controller. This message
 * can be a FSK/SWM network message or FSK/SWM controller configuration
 * message.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgFskMsgWrite(UINT8 devId, MXL_HYDRA_FSK_MSG_T *msgPtr)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT8 cmdSize = sizeof(MXL_HYDRA_FSK_MSG_T);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT32 reqType;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("devId = %d\n", devId););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if ((mxlStatus == MXL_SUCCESS) && (msgPtr))
  {
    // BUILD_HYDRA_CMD(MXL_HYDRA_FSK_MSG_CMD, MXL_CMD_WRITE, cmdSize, msgPtr, cmdBuff);  
    // build command manually instead of macro
    reqType = MXL_CMD_WRITE;
    cmdBuff[0] = ((reqType == MXL_CMD_WRITE) ? MXL_HYDRA_PLID_CMD_WRITE : MXL_HYDRA_PLID_CMD_READ);
    cmdBuff[1] = (cmdSize > 254)? 0xFF : (UINT8)(cmdSize + 4);                                             
    cmdBuff[2] = (UINT8)0;                                                                       
    cmdBuff[3] = (UINT8)MXL_HYDRA_FSK_MSG_CMD;                                                                      
    cmdBuff[4] = 0x00;
    cmdBuff[5] = 0x00;   

    //cmdSize += 6;
    
    cmdBuff[6] = GET_BYTE(msgPtr->msgValidFlag, 0);     // reg addr byte
    cmdBuff[7] = GET_BYTE(msgPtr->msgValidFlag, 1);     // reg addr byte
    cmdBuff[8] = GET_BYTE(msgPtr->msgValidFlag, 2);     // reg addr byte
    cmdBuff[9] = GET_BYTE(msgPtr->msgValidFlag, 3);     // reg addr byte

    cmdBuff[10] = GET_BYTE(msgPtr->msgLength, 0);     // reg addr byte
    cmdBuff[11] = GET_BYTE(msgPtr->msgLength, 1);     // reg addr byte
    cmdBuff[12] = GET_BYTE(msgPtr->msgLength, 2);     // reg addr byte
    cmdBuff[13] = GET_BYTE(msgPtr->msgLength, 3);     // reg addr byte

    MXLWARE_OSAL_MEMCPY((void *)&cmdBuff[14], (const void *)&(msgPtr->msgBuff[0]), msgPtr->msgLength);
    MXL_HYDRA_PRINT("\r\n Wrt Len %d", cmdBuff[10]);
    MXL_HYDRA_PRINT("\r\n0x%02X 0x%02X 0x%02X 0x%02X", cmdBuff[14], cmdBuff[15], cmdBuff[16], cmdBuff[17]);    

    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);

  }
  else 
    mxlStatus = MXL_INVALID_PARAMETER;

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgFskFreq
 *
 * @param[in]   devId           Device ID
 * @param[in]   fskFreqMode     FSK Frequency Mode
 * @param[in]   freq            Frequency to be configured.
 *
 * @author Sateesh
 *
 * @date 12/12/2013 Initial release 
 * 
 * This function will allow the host to set the FSK frequency mode 
 * into “normal” (2.3MHz) mode or “manual” where the output frequency 
 * is specified by the “freq” parameter.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_API_CfgFskFreq(UINT8 devId, MXL_HYDRA_FSK_CFG_FREQ_MODE_E fskFreqMode, UINT32 freq)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  MXL58x_CFG_FSK_FREQ_T fskFreqCmd;
  UINT8 cmdSize = sizeof(MXL58x_CFG_FSK_FREQ_T);
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("fskFreqMode=%d\n", fskFreqMode););
  MXLENTERAPI(MXL_HYDRA_PRINT("freq=%dHz\n", freq););

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if ((mxlStatus == MXL_SUCCESS) && \
	  ((fskFreqMode == MXL_HYDRA_FSK_CFG_FREQ_MODE_NORMAL) || (fskFreqMode == MXL_HYDRA_FSK_CFG_FREQ_MODE_MANUAL)) && \
	  ((freq >= 600) && (freq <= 9700000)))
  {
	mxlStatus = MxLWare_HYDRA_API_CfgFskOpMode(devId, MXL_HYDRA_FSK_CFG_TYPE_39KPBS);
	if(mxlStatus == MXL_SUCCESS)
	{
		// build Demod Frequency Offset Search command
		fskFreqCmd.fskFreqMode = fskFreqMode;
		fskFreqCmd.freq = freq;
		BUILD_HYDRA_CMD(MXL_HYDRA_FSK_CFG_FSK_FREQ_CMD, MXL_CMD_WRITE, cmdSize, &fskFreqCmd, cmdBuff);

		// send command to device
		mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
	}
  }
  else 
    mxlStatus = MXL_INVALID_PARAMETER;

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_PowerDownFsk
 *
 * @param[in]   devId           Device ID
 * 
 * @author Sateesh
 *
 * @date 01/08/2013 Initial release 
 * 
 * The API is used to disable and power down FSK block.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/
MXL_STATUS_E MxLWare_HYDRA_API_PowerDownFsk(UINT8 devId)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT8 cmdSize = 0;
  UINT8 cmdBuff[MXL_HYDRA_OEM_MAX_CMD_BUFF_LEN];
  UINT32 dummy = 0;

  MXLENTERAPISTR(devId);

  mxlStatus = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (mxlStatus == MXL_SUCCESS)
  {
    BUILD_HYDRA_CMD(MXL_HYDRA_FSK_POWER_DOWN_CMD, MXL_CMD_WRITE, cmdSize, &dummy, cmdBuff);
    mxlStatus = MxLWare_HYDRA_SendCommand(devId, cmdSize + MXL_HYDRA_CMD_HEADER_SIZE, &cmdBuff[0]);
  }
  else 
    mxlStatus = MXL_INVALID_PARAMETER;

  MXLEXITAPISTR(devId, mxlStatus);
  return mxlStatus;
}
