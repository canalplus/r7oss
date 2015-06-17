
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
#include "mxlware_hydra_oem_defines.h"


/*#include "chip.h"
#include "globaldefs.h"
#include "stos.h"
*/
#ifdef __MXL_OEM_DRIVER__
#include "mxl_hydra_i2cwrappers.h"
#endif


/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_DeviceReset
 *
 * @param[in]   devId           Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API performs a hardware reset on Hydra SOC identified by devId.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_OEM_DeviceReset(UINT8 devId)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;

  devId = devId;

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_SleepInMs
 *
 * @param[in]   delayTimeInMs
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * his API will implement delay in milliseconds specified by delayTimeInMs.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

void MxLWare_HYDRA_OEM_SleepInMs(UINT16 delayTimeInMs)
{


  STCHIP_Handle_t hChip = NULL;
  hChip=(STCHIP_Handle_t )MxL_HYDRA_OEM_DataPtr[0];//devId is 0


 ChipWaitOrAbort(hChip,(uint32_t)delayTimeInMs);


#ifdef __MXL_OEM_DRIVER__
  Sleep(delayTimeInMs);
#endif

}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_GetCurrTimeInMs
 *
 * @param[out]   msecsPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API should return system time milliseconds.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

void MxLWare_HYDRA_OEM_GetCurrTimeInMs(UINT64 *msecsPtr)
{

  UINT32 get_jiffies;


  get_jiffies =  stm_fe_time_now();
  *msecsPtr = (UINT64)stm_fe_jiffies_to_msecs(get_jiffies);

#ifdef __MXL_OEM_DRIVER__
  *msecsPtr = GetTickCount();
#endif

}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_I2cWrite
 *
 * @param[in]   devId           Device ID
 * @param[in]   dataLen
 * @param[in]   buffPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API performs an I2C block write by writing data payload to Hydra device.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_OEM_I2cWrite(UINT8 devId, UINT16 dataLen, UINT8 *buffPtr)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  /*oem_data_t *oemDataPtr = (oem_data_t *)0;

  oemDataPtr = (oem_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  buffPtr = buffPtr;
  dataLen = dataLen;
  oemDataPtr = oemDataPtr;
*/
	STCHIP_Handle_t hChip = NULL;
	fe_i2c_msg msg[1];
	uint32_t msgindex = 0,retry_count=0;
	int ret =-1;
	uint32_t msg_cnt;
	/*U32 i=0;*/
	hChip=(STCHIP_Handle_t )MxL_HYDRA_OEM_DataPtr[devId];

	/* Message to write the actual data onto the device */
	Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, dataLen,
	buffPtr, WRITING, TRUE);


	do {
		ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
		if (ret != msgindex && retry_count == 2) {
			for(msg_cnt = 0; msg_cnt < msgindex; msg_cnt++)
				printk(KERN_ERR
					"%s:WR msg[%d] hChip->dev_i2c =0x%x addr=0x%x len=0x%d Err= %d\n"
					, __func__, msg_cnt,(uint32_t)hChip->dev_i2c ,msg[msg_cnt].addr,
					msg[msg_cnt].len, ret);
		hChip->Error = CHIPERR_I2C_NO_ACK;
		}
		retry_count++;
	} while ((ret != msgindex) && (retry_count <= 2));

		  mxlStatus=hChip->Error;


	 /* printk("\nMxLWare_HYDRA_OEM_I2cWrite hChip->Error=0x%x  i2c=0x%x handle=0x%x\n",
		hChip->Error, hChip->I2cAddr,hChip->dev_i2c);
	  for(i=0;i<dataLen;i++)
	  {
		printk("\n buffPtr[0x%x]=0x%x\n", i, buffPtr[i]);
	  }*/
	 /*printk("%s 0x%-4x@%s flag=%d 0x%X 0x%x 0x%x 0x%x", __FUNCTION__,hChip->I2cAddr,
			   hChip->dev_i2c,msg[0].flags ,msg[0].buf[0],msg[0].buf[1],msg[0].buf[2],msg[0].buf[3]);
		*/

#ifdef __MXL_OEM_DRIVER__

  mxlStatus = MxL_HYDRA_I2C_WriteData(oemDataPtr->drvIndex, oemDataPtr->i2cAddress, dataLen, buffPtr);

#endif

  return mxlStatus;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_OEM_I2cRead
 *
 * @param[in]   devId           Device ID
 * @param[in]   dataLen
 * @param[out]  buffPtr
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * This API shall be used to perform I2C block read transaction.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_OEM_I2cRead(UINT8 devId, UINT16 dataLen, UINT8 *buffPtr)
{
  MXL_STATUS_E mxlStatus = MXL_SUCCESS;
  /*oem_data_t *oemDataPtr = (oem_data_t *)0;

  oemDataPtr = (oem_data_t *)MxL_HYDRA_OEM_DataPtr[devId];
  buffPtr = buffPtr;
  dataLen = dataLen;
  oemDataPtr = oemDataPtr;
*/
	  STCHIP_Handle_t hChip = NULL;
	  fe_i2c_msg msg[1];
	  uint32_t msg_cnt;
	  uint32_t msgindex = 0,retry_count=0/* ,i=0*/;
	  int ret =-1;
      BOOL STOP_After_SubAddress = TRUE;

	 hChip=(STCHIP_Handle_t )MxL_HYDRA_OEM_DataPtr[devId];

	/*address write has been already called before oem_i2cread*/
	/* Prepare message for reading from device */
	Fill_CustomMsg(msg, (hChip->I2cAddr),&msgindex, dataLen, buffPtr, READING, STOP_After_SubAddress);

	do {
		ret = i2c_transfer(hChip->dev_i2c, &msg[0], msgindex);
		if (ret != msgindex && retry_count == 2) {
			for(msg_cnt = 0; msg_cnt < msgindex; msg_cnt++)
				printk(KERN_ERR
					"%s:RD msg[%d] hChip->dev_i2c =0x%x addr=0x%x len=0x%d Err= %d\n"
					, __func__, msg_cnt,(uint32_t)hChip->dev_i2c ,msg[msg_cnt].addr,
					msg[msg_cnt].len, ret);
		hChip->Error = CHIPERR_I2C_NO_ACK;
	}
	retry_count++;
	} while ((ret != msgindex) && (retry_count <= 2));

	mxlStatus=hChip->Error;

	/*printk("\nMxLWare_HYDRA_OEM_I2cRead hChip->Error=0x%x	i2c=0x%x handle=0x%x\n",
		   hChip->Error, hChip->I2cAddr,hChip->dev_i2c);
		 for(i=0;i<dataLen;i++)
		 {
			   printk("\n buffPtr[0x%x]=0x%x\n",i,buffPtr[i]);
		 }*/
	 /*  printk("%s 0x%-4x@%s flag=%d 0x%X 0x%x 0x%x 0x%x\n", __FUNCTION__,hChip->I2cAddr,
			   hChip->dev_i2c,msg[0].flags ,msg[0].buf[0],msg[0].buf[1],msg[0].buf[2],msg[0].buf[3]);
	   */

#ifdef __MXL_OEM_DRIVER__
    mxlStatus = MxL_HYDRA_I2C_ReadData(oemDataPtr->drvIndex, oemDataPtr->i2cAddress, dataLen, buffPtr);
#endif

  return mxlStatus;
}


void Fill_CustomMsg(fe_i2c_msg * msg, uint32_t addr, uint32_t * MsgIndex,
	     uint32_t BufferLen, uint8_t * Buffer_p, context_t Context,
	     bool UseStop)
{
#ifdef CONFIG_ARCH_STI
	msg[*MsgIndex].addr = addr;
	msg[*MsgIndex].flags = Context;
	msg[*MsgIndex].buf = Buffer_p;
	msg[*MsgIndex].len = BufferLen;
	(*MsgIndex)++;
#else
	msg[*MsgIndex].addr = addr;
	msg[*MsgIndex].flags = Context;
	if (!UseStop) {
		msg[*MsgIndex].flags = Context;
		/*msg[*MsgIndex].flags |= 0;-I2C_M_NOSTOP undeclard error?*/
	} else {
		msg[*MsgIndex].flags |= I2C_M_NOREPSTART;
		/* STI2C sets this false
		*for last message;not done here */
	}
	msg[*MsgIndex].buf = Buffer_p;
	msg[*MsgIndex].len = BufferLen;
	(*MsgIndex)++;
#endif
}
