/*
* Copyright (c) 2011-2014 MaxLinear, Inc. All rights reserved
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

#ifndef S_SPLINT_S


/*#include <sys/time.h>
#include <unistd.h>
#include <string.h>*/

#include <linux/time.h>
/*#include <linux/unistd.h>
#include <linux/string.h>*/
#include "mxl_hrcls_common.h"
#include "i2c_wrapper.h"
#include "mxl_hrcls_oem_drv.h"
#include "gen_macros.h"
#include "stfe_utilities.h"
/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_HRCLS_OEM_Reset
--|
--| DESCRIPTION   : This function resets MxL_HRCLS through Reset Pin
--| PARAMETERS    : devId - MxL_HRCLS device id
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_Reset(UINT8 devId)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should toggle reset pin of MxL_HRCLS specified by I2C Slave Addr

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_HRCLS_OEM_WriteRegister
--|
--| DESCRIPTION   : This function does I2C write operation.
--| PARAMETERS    : devId - MxL_HRCLS device id
--|                 regAddr - Register address of MxL_HRCLS to write.
--|                 regData - Data to write to the specified address.
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_WriteRegister(UINT8 devId, UINT16 regAddr, UINT16 regData)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should implement I2C write protocol that complies with MxL_HRCLS I2C
  // format.

  // 16bit Register Write Protocol:
  // +------+-+-----+-+-+----------+-+----------+-+----------+-+----------+-+-+
  // |MASTER|S|SADDR|W| |regAddr(H)| |regAddr(L)| |regData(H)| |regData(L)| |P|
  // +------+-+-----+-+-+----------+-+----------+-+----------+-+----------+-+-+
  // |SLAVE |         |A|          |A|          |A|          |A|          |A| |
  // +------+---------+-+----------+-+----------+-+----------+-+----------+-+-+
  // Legends: SADDR (I2c slave address), S (Start condition), A (Ack), N(NACK), P(Stop condition)

  STCHIP_Handle_t hChip = NULL;
  fe_i2c_msg msg[1];
  unsigned char	 Buffer[10];
  uint32_t msgindex = 0,retry_count=0;
  int ret =-1;
  uint32_t msg_cnt;
  /*U32 i=0;*/
  hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[devId];

	Buffer[0] = MSB(regAddr);
	Buffer[1] = LSB(regAddr);
	Buffer[2] = MSB(regData);
	Buffer[3] = LSB(regData);
  /* Message to write the actual data onto the device */
	Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, 4, Buffer, WRITING,
		TRUE);
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

		  status = hChip->Error;

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_HRCLS_OEM_ReadRegister
--|
--| DESCRIPTION   : This function does I2C read operation.
--| PARAMETERS    : devId - MxL_HRCLS device id
--|                 regAddr - Register address of MxL_HRCLS to read.
--|                 dataPtr - Data container to return 16 data.
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_ReadRegister(UINT8 devId, UINT16 regAddr, UINT16 *dataPtr)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should implement I2C read protocol that complies with MxL_HRCLS I2C
  // format.

  // 16 Register Read Protocol:
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // |MASTER|S|SADDR|W| |0xFF| |0xFB| |regAddr(H)| |regAddr(L)| |P|
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // |SLAVE |         |A|    |A|    |A|          |A|          |A| |
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // +------+-+-----+-+-+-------+--+-------+--+-+
  // |MASTER|S|SADDR|R| |       |MA|       |MN|P|
  // +------+-+-----+-+-+-------+--+-------+--+-+
  // |SLAVE |         |A|Data(H)|  |Data(L)|  | |
  // +------+---------+-+-------+--+----------+-+
  // Legends: SADDR(I2c slave address), S(Start condition), MA(Master Ack), MN(Master NACK), P(Stop condition)

	unsigned char    SA_Buffer[10], RBuffer[2];
	fe_i2c_msg msg[3];
	uint32_t msg_cnt;
	uint32_t msgindex = 0,retry_count=0/* ,i=0*/;
	int ret =-1;
	STCHIP_Handle_t hChip = NULL;

	hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[devId];


	SA_Buffer[0] = 0xFF;
	SA_Buffer[1] = 0xFB;
	SA_Buffer[2] = MSB(regAddr);
	SA_Buffer[3] = LSB(regAddr);

	/* Prepare message for sub address write */
	Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, 4, SA_Buffer, WRITING,
		TRUE);

	/* Prepare message for reading from device */
	Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, 2, RBuffer, READING,
		TRUE);

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


  if (hChip->Error == CHIPERR_NO_ERROR)
	  {
		 *dataPtr =( (RBuffer[0]<<8) | (RBuffer[1]) );
	  }

  status = hChip->Error;

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_HRCLS_OEM_WriteBlock
--|
--| DESCRIPTION   : This function does I2C block write operation.
--| PARAMETERS    : devId - MxL_HRCLS device id
--|                 regAddr - Register address of MxL_HRCLS to start a block write.
--|                 bufSize - The number of bytes to write
--|                 bufPtr - Data bytes to write to the specified address.
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_WriteBlock(UINT8 devId, UINT16 regAddr, UINT16 bufSize, UINT8 *bufPtr)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should implement I2C block write protocol that complies with MxL_HRCLS I2C
  // format.

  // Block Write Protocol:
  // +------+-+-----+-+-+----------+-+----------+-+---------+-+---+-----------------+-+-+
  // |MASTER|S|SADDR|W| |regAddr(H)| |regAddr(L)| |bufPtr[0]| |   |bufPtr[Bufsize-1]| |P|
  // +------+-+-----+-+-+----------+-+----------+-+---------+-+...+-----------------+-+-+
  // |SLAVE |         |A|          |A|          |A|         |A|   |                 |A| |
  // +------+---------+-+----------+-+---- -----+-+---------+-+---+-----------------+-+-+
  // Legends: SADDR(I2c slave address), S(Start condition), A(Ack), P(Stop condition)
	unsigned char 	Buffer[2+bufSize];
	fe_i2c_msg msg[1];
	uint32_t msg_cnt;
	uint32_t msgindex = 0,retry_count=0/* ,i=0*/;
	int ret =-1;
	STCHIP_Handle_t hChip=NULL;

	hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[devId];



	Buffer[0] = MSB(regAddr);
	Buffer[1] = LSB(regAddr);
	memcpy(&Buffer[2], bufPtr, bufSize);
/*
	// Buffer[2] = bufPtr;
	// for(i=0;i<bufSize;i++)
	//{
		// Buffer[2+i] = *(bufPtr+i);
		//}
		*/
	 Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex,2+bufSize, Buffer, WRITING, TRUE);

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

  /*  if(hChip->Error!=CHIPERR_NO_ERROR)
	return MXL_FAILURE;                        */

    status = hChip->Error;

  return status;
}

/*------------------------------------------------------------------------------
--| FUNCTION NAME : MxLWare_HRCLS_OEM_ReadBlock
--|
--| DESCRIPTION   : This function does I2C block read operation.
--| PARAMETERS    : devId - MxL_HRCLS device id
--|                 regAddr - Register Address to start a block read
--|                 readSize - The number of bytes to read
--|                 bufPtr - Container to hold readback data
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_ReadBlock(UINT8 devId, UINT16 regAddr, UINT16 readSize, UINT8 *bufPtr)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should implement I2C block read protocol that complies with MxL_HRCLS I2C
  // format.

  // Block Read Protocol (n bytes of data):
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // |MASTER|S|SADDR|W| |0xFF| |0xFD| |regAddr(H)| |regAddr(L)| |P|
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // |SLAVE |         |A|    |A|    |A|          |A|          |A| |
  // +------+-+-----+-+-+----+-+----+-+----------+-+----------+-+-+
  // +------+-+-----+-+-------+-+-----+-+-----+-+-----+-+-+
  // |MASTER|S|SADDR|R|       |A|     |A|       |     |N|P|
  // +------+-+-----+-+-+-----+-+-----+-+  ...  +-----+-+-+
  // |SLAVE |         |A|DATA1| |DATA2| |       |DATAn|   |
  // +------+---------+-+-----+-+-----+-+-----+-+-----+---+
  // Legends: SADDR (I2c slave address), S (Start condition), A (Acknowledgement), N(NACK), P(Stop condition)

	unsigned char Buffer[readSize+4];
	fe_i2c_msg msg[2];
	uint32_t msg_cnt;
	uint32_t msgindex = 0,retry_count=0/* ,i=0*/;
	int ret =-1;
	STCHIP_Handle_t hChip = NULL;

  hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[devId];

  Buffer[0] = 0xFF;
  Buffer[1] = 0xFD;
  Buffer[2] = MSB(regAddr);
  Buffer[3] = LSB(regAddr);

  /* Prepare message for sub address write */
  Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, 4, Buffer, WRITING, TRUE);

  /* Prepare message for reading from device */
  Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, readSize, Buffer, READING, TRUE);

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


  if (hChip->Error == CHIPERR_NO_ERROR)
	  memcpy(bufPtr, Buffer, readSize);

  status=hChip->Error;

  return status;
}

MXL_STATUS_E MxLWare_HRCLS_OEM_ReadBlockExt(UINT8 devId, UINT16 cmdId, UINT16 offset, UINT16 readSize, UINT8 *bufPtr)
{
  MXL_STATUS_E status = MXL_SUCCESS;

  // !!! FIXME !!!
  // OEM should implement I2C block read protocol that complies with MxL_HRCLS I2C
  // format.

  // Block Read Protocol (n bytes of data):
  // +------+-+-----+-+-+----+-+----+-+--------+-+--------+-+----------+-+---------+-+-+
  // |MASTER|S|SADDR|W| |0xFF| |0xFD| |cmdId(H)| |cmdId(L)| |offset(H) | |offset(L)| |P|
  // +------+-+-----+-+-+----+-+----+-+--------+-+--------+-+----------+-+---------+-+-+
  // |SLAVE |         |A|    |A|    |A|        |A|        |A|          |A|         |A| |
  // +------+-+-----+-+-+----+-+----+-+--------+-+--------+-+----------+-+---------+-+-+
  // +------+-+-----+-+-------+-+-----+-+-----+-+-----+-+-+
  // |MASTER|S|SADDR|R|       |A|     |A|       |     |N|P|
  // +------+-+-----+-+-+-----+-+-----+-+  ...  +-----+-+-+
  // |SLAVE |         |A|DATA1| |DATA2| |       |DATAn|   |
  // +------+---------+-+-----+-+-----+-+-----+-+-----+---+
  // Legends: SADDR (I2c slave address), S (Start condition), A (Acknowledgement), N(NACK), P(Stop condition)

  unsigned char Buffer[readSize+6];
  fe_i2c_msg msg[2];
  uint32_t msg_cnt;
  uint32_t msgindex = 0,retry_count=0/* ,i=0*/;
  int ret =-1;
  STCHIP_Handle_t hChip = NULL;
  hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[devId];

  Buffer[0] = 0xFF;
  Buffer[1] = 0xFD;
  Buffer[2] = MSB(cmdId);
  Buffer[3] = LSB(cmdId);
  Buffer[4] = MSB(offset);
  Buffer[5] = LSB(offset);

  /* Prepare message for sub address write */
  Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, 4, Buffer, WRITING, TRUE);

  /* Prepare message for reading from device */
  Fill_CustomMsg(msg, hChip->I2cAddr, &msgindex, readSize, Buffer, READING, TRUE);

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


  if (hChip->Error == CHIPERR_NO_ERROR)
	  memcpy(bufPtr, Buffer, readSize);

  status = hChip->Error;

  return status;
}

/*------------------------------------------------------------------------------
--|
--| FUNCTION NAME : MxLWare_HRCLS_OEM_LoadNVRAMFile
--|
--| DESCRIPTION   : Load MxLNVRAMFile
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_LoadNVRAMFile(UINT8 devId, UINT8 *bufPtr, UINT32 bufLen)
{
  MXL_STATUS_E status = MXL_FAILURE;

  // !!! FIXME !!!
  // To be implemented for customer OEM platform
  return status;
}

/*------------------------------------------------------------------------------
--|
--| FUNCTION NAME : MxLWare_HRCLS_OEM_SaveNVRAMFile
--|
--| DESCRIPTION   : Save MxLNVRAMFile
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

MXL_STATUS_E MxLWare_HRCLS_OEM_SaveNVRAMFile(UINT8 devId, UINT8 *bufPtr, UINT32 bufLen)
{
  MXL_STATUS_E status = MXL_FAILURE;
  // !!! FIXME !!!
  // To be implemented for customer OEM platform

  return status;
}

/*------------------------------------------------------------------------------
--|
--| FUNCTION NAME : MxLWare_HRCLS_OEM_DelayUsec
--|
--| DESCRIPTION   : Delay in micro-seconds
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

void MxLWare_HRCLS_OEM_DelayUsec(UINT32 usec)
{
  // !!! FIXME !!!
  // To be implemented for customer OEM platform
 /* usleep(usec);*/
	STCHIP_Handle_t hChip = NULL;
	hChip=(STCHIP_Handle_t )MxL_HRCLS_OEM_DataPtr[0];//devId is 0

	/*ChipWaitOrAbort(hChip,(uint32_t)usec);*/
	udelay(usec);

}

/*------------------------------------------------------------------------------
--|
--| FUNCTION NAME : MxLWare_HRCLS_OEM_GetCurrTimeInUsec
--|
--| DESCRIPTION   : Get current time in micro-seconds
--|
--| RETURN VALUE  : MXL_SUCCESS or MXL_FAILURE
--|
--|---------------------------------------------------------------------------*/

void MxLWare_HRCLS_OEM_GetCurrTimeInUsec(UINT64* usecPtr)
{
  struct timeval currTime;

#define __WORDSIZE 32

#if __WORDSIZE == 32
  static unsigned long long sec_count = 0;
  unsigned long long mask, incrementValue;
#endif
  unsigned long long timeInUsec;

  MxL_HRCLS_DEBUG(" MxL_HRCLS_Ctrl_GetTimeTickInUsec ");
  /*gettimeofday(&currTime, NULL);*/
  do_gettimeofday(&currTime);

#if __WORDSIZE == 32
  incrementValue = ((unsigned long long) 1 << (sizeof(currTime.tv_sec) * 8));
  mask = incrementValue-1;

  if ((unsigned long long) currTime.tv_sec < (sec_count & mask))
  {
    sec_count += incrementValue;
  }
  sec_count &= ~(mask);
  sec_count |= (currTime.tv_sec & mask);
  timeInUsec = (unsigned long long)(sec_count * 1000 * 1000) + (unsigned long long) currTime.tv_usec;
#else
  timeInUsec = (unsigned long long)((unsigned long long) currTime.tv_sec * 1000 * 1000) + (unsigned long long)currTime.tv_usec;
//printf("sec:%x, usec:%lu, timeInUsec:%Lu\n", (unsigned int) currTime.tv_sec, (unsigned long) currTime.tv_usec, timeInUsec);
#endif
  *usecPtr= (UINT64) timeInUsec;

  // return MXL_SUCCESS;
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

#endif
