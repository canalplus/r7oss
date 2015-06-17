
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
#include "mxlware_hydra_phyctrl.h"

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgSerdesOpMode
 *
 * @param[in]   devId           Device ID
 * @param[in]   serdesOpMode
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * OEM Drv/Lib (DLL) modules should call this function to read a block
 * device registers or a block memory from the device using I2C device interface.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgSerdesOpMode(UINT8 devId, MXL_HYDRA_SERDES_OP_MODE_E serdesOpMode)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("serdesOpMode=%d\n", serdesOpMode););

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
 * @brief MxLWare_HYDRA_API_CfgSerdesInterface
 *
 * @param[in]   devId           Device ID
 * @param[in]   serdesFillerPayloadSize
 * @param[in]   serdesStatsReportCtrl
 *
 * @author Mahee
 *
 * @date 06/12/2012 Initial release
 *
 * OEM Drv/Lib (DLL) modules should call this function to read a block
 * device registers or a block memory from the device using I2C device interface.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgSerdesInterface(UINT8 devId, UINT8 serdesFillerPayloadSize, MXL_BOOL_E serdesStatsReportCtrl)
{
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  MXL_STATUS_E status;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("serdesFillerPayloadSize=%d, serdesStatsReportCtrl=%d\n", serdesFillerPayloadSize, serdesStatsReportCtrl););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {

  }

  MXLEXITAPISTR(devId, status);
  return status;
}
