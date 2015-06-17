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

#ifndef __MXL_HRCLS_OEM_DRV_H__
#define __MXL_HRCLS_OEM_DRV_H__

/*****************************************************************************************
    Include Header Files
    (No absolute paths - paths handled by make file)
*****************************************************************************************/
#include "stm_fe_os.h"
#include "i2c_wrapper.h"

#include "maxlineardatatypes.h"
#include "mxl_hrcls_oem_defines.h"

/*****************************************************************************************
    Macros
*****************************************************************************************/

/*****************************************************************************************
    User-Defined Types (Typedefs)
*****************************************************************************************/

/*****************************************************************************************
    Global Variable Declarations
*****************************************************************************************/
extern void * MxL_HRCLS_OEM_DataPtr[];
/*****************************************************************************************
    Prototypes
*****************************************************************************************/

MXL_STATUS_E MxLWare_HRCLS_OEM_Reset(UINT8 devId);
MXL_STATUS_E MxLWare_HRCLS_OEM_WriteRegister(UINT8 devId, UINT16 regAddr, UINT16 regData);
MXL_STATUS_E MxLWare_HRCLS_OEM_ReadRegister(UINT8 devId, UINT16 regAddr,/*@out@*/  UINT16 *dataPtr);
MXL_STATUS_E MxLWare_HRCLS_OEM_ReadBlock(UINT8 devId, UINT16 regAddr, UINT16 readSize, /*@out@*/ UINT8 *bufPtr);
MXL_STATUS_E MxLWare_HRCLS_OEM_ReadBlockExt(UINT8 devId, UINT16 cmdId, UINT16 offset, UINT16 readSize, /*@out@*/ UINT8* bufPtr);
MXL_STATUS_E MxLWare_HRCLS_OEM_WriteBlock(UINT8 devId, UINT16 regAddr, UINT16 bufSize, UINT8* bufPtr);
MXL_STATUS_E MxLWare_HRCLS_OEM_ReadBlockExt(UINT8 devId, UINT16 cmdId, UINT16 offset, UINT16 readSize, UINT8 *bufPtr);
MXL_STATUS_E MxLWare_HRCLS_OEM_LoadNVRAMFile(UINT8 devId, UINT8 *bufPtr, UINT32 bufLen);
MXL_STATUS_E MxLWare_HRCLS_OEM_SaveNVRAMFile(UINT8 devId, UINT8 *bufPtr, UINT32 bufLen);
void MxLWare_HRCLS_OEM_DelayUsec(UINT32 usec);
void MxLWare_HRCLS_OEM_GetCurrTimeInUsec(/*@out@*/ UINT64* usecPtr);

void Fill_CustomMsg(fe_i2c_msg * msg, uint32_t addr, uint32_t * MsgIndex,
	     uint32_t BufferLen, uint8_t * Buffer_p, context_t Context,
	     bool UseStop);
#endif /* __MXL_HRCLS_OEM_DRV_H__*/
