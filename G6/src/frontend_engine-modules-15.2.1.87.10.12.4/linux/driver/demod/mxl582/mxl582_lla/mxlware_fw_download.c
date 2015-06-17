
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

#ifndef __KERNEL__
#include <stdio.h>
#endif

#include "maxlineardatatypes.h"
#include "mxlware_hydra_oem_defines.h"
#include "maxlineardebug.h"
#include "mxlware_hydra_oem_drv.h"
#include "mxlware_fw_download.h"
#include "mxlware_hydra_registers.h"


static UINT32 MxL_Ctrl_GetBigEndian(UINT8 numOfBits, const UINT8 buf[])
{
  UINT32 retValue = 0;

  switch(numOfBits)
  {
    case 24:
      retValue = (((UINT32)buf[0])<<16) | (((UINT32)buf[1])<<8) | buf[2];
      break;

    case 32:
      retValue = (((UINT32)buf[0])<<24) |
                  (((UINT32)buf[1])<<16) |
                  (((UINT32)buf[2])<<8) | buf[3];
      break;

    default:
      break;
  }

  return retValue;
}

static void MxL_Ctrl_FlipDataInDword(UINT16 size, UINT8* dataPtr)
{
  UINT16 i;

  for (i = 0; i < (size & (UINT16)(~3)); i += 4)
  {
    dataPtr[i + 0] ^= dataPtr[i + 3];
    dataPtr[i + 3] ^= dataPtr[i + 0];
    dataPtr[i + 0] ^= dataPtr[i + 3];

    dataPtr[i + 1] ^= dataPtr[i + 2];
    dataPtr[i + 2] ^= dataPtr[i + 1];
    dataPtr[i + 1] ^= dataPtr[i + 2];
  }

  switch (size & 3)
  {
    case 0: case 1: /* do nothing */ break;
    case 2:
      dataPtr[size + 1] ^= dataPtr[i + 2];
      dataPtr[size + 2] ^= dataPtr[i + 1];
      dataPtr[size + 1] ^= dataPtr[i + 2];
      break;

    case 3:
      dataPtr[size + 1] ^= dataPtr[i + 3];
      dataPtr[size + 3] ^= dataPtr[i + 1];
      dataPtr[size + 1] ^= dataPtr[i + 3];
      break;
  }
}

static MXL_STATUS_E MxL_Ctrl_WriteFWSegement(UINT8 devId,
                                               UINT32 MemAddr,
                                               UINT32 totalSize,
                                               UINT8 *dataPtr,
                                               /*@null@*/ MXL_CALLBACK_FN_T fwCallbackFn)
{
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT32 dataCount = 0;
  UINT32 size = 0;
  UINT32 origSize = 0;
  UINT8* wBufPtr = NULL;
  UINT32 blockSize = ((MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH-(MXL_HYDRA_I2C_HDR_SIZE + MXL_HYDRA_REG_SIZE_IN_BYTES))/4)*4;
  UINT8 wMsgBuffer[MXL_HYDRA_OEM_MAX_BLOCK_WRITE_LENGTH-(MXL_HYDRA_I2C_HDR_SIZE + MXL_HYDRA_REG_SIZE_IN_BYTES)];
  UINT32 fwCBFreq = 0;
 
  //MXL_HYDRA_PRINT("%s: blockSize = %d;\n", __FUNCTION__, blockSize);

  do
  {
    origSize = (((UINT32)(dataCount + blockSize)) > totalSize) ? (totalSize - dataCount) : blockSize;

    size = origSize;
    wBufPtr = dataPtr;
    
    //MXL_HYDRA_PRINT("%s: origSize = %d; dataCount = %d\n", 
    //                                        __FUNCTION__, origSize, dataCount);
    
    // if data to send is multiple of 4 i.e size should be always 32-bit aligned
    if (origSize & 3)
    {
      size = (origSize + 4) & (UINT16)(~3);
      wBufPtr = &wMsgBuffer[0];
      MXLWARE_OSAL_MEMSET((void *)wBufPtr, 0x00, size);
      MXLWARE_OSAL_MEMCPY((void *)wBufPtr, (void *)dataPtr, origSize);
    }

    // each 4 bytes: LSB->MSB
    MxL_Ctrl_FlipDataInDword((UINT16)size, wBufPtr);

    // Send fw segment data to device
    mxlStatus = MxLWare_HYDRA_WriteFirmwareBlock(devId, MemAddr, size, wBufPtr);

    if (MXL_SUCCESS != mxlStatus) 
      break;

    dataCount += size;
    MemAddr   += size;
    dataPtr   += size;

    if (fwCallbackFn)
    {
      fwCBFreq += origSize;

      if (fwCBFreq >= (UINT32)(7 * blockSize))
      {
        if (2 == fwCallbackFn(devId, (UINT32)fwCBFreq, NULL))
        {
          MXL_HYDRA_PRINT("%s: Firmware download status indicator callback function is not initilized", __FUNCTION__);
          mxlStatus = MXL_NOT_INITIALIZED;
          break;
        }

        fwCBFreq = 0;
      }
    }
  } while (dataCount < totalSize);

  return mxlStatus;
}

MXL_STATUS_E MxLWare_FW_Download(UINT8 devId, UINT32 mbinBufferSize, UINT8 *mbinBufferPtr, MXL_CALLBACK_FN_T fwCallbackFn)
{
  MXL_STATUS_E mxlStatus = MXL_FAILURE;
  UINT32 index = 0;
  UINT32 segLength = 0;
  UINT32 segAddress = 0;
  MBIN_FILE_T* mbinPtr  = (MBIN_FILE_T*)mbinBufferPtr;
  MBIN_SEGMENT_T *segmentPtr;

#ifdef __MXL_SCORPIUS__
  MXL_BOOL_E xcpuFwFlag = MXL_FALSE;
#endif

#ifdef _MXL_BRING_UP__
  UINT32 data = 0;
#endif
  mbinBufferSize = mbinBufferSize;

  MXL_HYDRA_PRINT("%s: Header ID = %d, Segments number = %d \n", __FUNCTION__, mbinPtr->header.id, mbinPtr->header.numSegments);

  // Verify firmware header
  if (mbinPtr->header.id == MBIN_FILE_HEADER_ID)
  {
	  //set this to 0 before downloading fw. FW will write is back FW_DL_SIGN_ADDR after boot up
	  mxlStatus |= MxLWare_HYDRA_WriteRegister(devId, FW_DL_SIGN_ADDR, 0); 

  segmentPtr = (MBIN_SEGMENT_T *) (&mbinPtr->data[0]);

  for (index = 0; index < mbinPtr->header.numSegments; index++)
  {
    // Validate segment header ID
    if ( segmentPtr->header.id != MBIN_SEGMENT_HEADER_ID)
    {
      MXL_HYDRA_PRINT("%s: Invalid segment header ID (%c)\n", __FUNCTION__, segmentPtr->header.id);
		  mxlStatus = MXL_NOT_SUPPORTED; // If the segment format is wrong, then exit with MXL_FAILURE 
		  break;
    }

    // Get segment data length and start address
    segLength  = MxL_Ctrl_GetBigEndian(24, &(segmentPtr->header.len24[0]));
    segAddress = MxL_Ctrl_GetBigEndian(32, &(segmentPtr->header.address[0]));

#ifdef _MXL_BRING_UP__
		mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_CPU_RESET_REG, &data);
    MXL_HYDRA_PRINT("%s: Before Segment = %d SegAddr = 0x%X\n", __FUNCTION__, index, segAddress);
#endif
		MXL_HYDRA_PRINT("%s: Segment = %d SegAddr = 0x%X SegLength = %d\n", __FUNCTION__, index, segAddress, segLength);

#ifdef __MXL_SCORPIUS__
    if ((((segAddress & 0x90760000) == 0x90760000) || ((segAddress & 0x90740000) == 0x90740000)) && (xcpuFwFlag == MXL_FALSE))
    {
      // MCPU softreset
      // MxLWare_Hydra_UpdateByMnemonic(devId, 0x8003003C, 0 , 1, 1);
      mxlStatus = SET_REG_FIELD_DATA(devId, PRCM_PRCM_CPU_SOFT_RST_N, 1);

      MxLWare_HYDRA_OEM_SleepInMs(1000);

      // XCPU into reset
      MxLWare_HYDRA_WriteRegister(devId, 0x90720000, 0);

      MxLWare_HYDRA_OEM_SleepInMs(10);

      xcpuFwFlag = MXL_TRUE;
    }
#endif
    // Send segment
    mxlStatus = MxL_Ctrl_WriteFWSegement(devId, segAddress, segLength, (UINT8*)segmentPtr->data, fwCallbackFn);

#ifdef _MXL_BRING_UP__
		mxlStatus |= MxLWare_HYDRA_ReadRegister(devId, HYDRA_CPU_RESET_REG, &data);
#endif

    if (MXL_SUCCESS != mxlStatus)
    {
		  break;
    }

    // Next segment
    segmentPtr = (MBIN_SEGMENT_T*)&(segmentPtr->data[((segLength + 3)/4)*4]);
	  }
  }
  else
  {
    MXL_HYDRA_PRINT("%s: Invalid file header ID (%c)\n", __FUNCTION__, mbinPtr->header.id);
    mxlStatus = MXL_NOT_SUPPORTED; // the mbin file formate not supported 
  }

  return mxlStatus;
}
