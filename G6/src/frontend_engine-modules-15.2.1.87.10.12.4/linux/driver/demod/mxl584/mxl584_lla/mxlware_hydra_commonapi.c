

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
#include "mxlware_hydra_registers.h"
#include "mxlware_hydra_oem_drv.h"




/* OEM Data pointer array */
void * MxL_HYDRA_OEM_DataPtr[MXL_HYDRA_MAX_NUM_DEVICES];

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDrvInit
 *
 * @param[in]   devId        Device ID
 * @param[in]   oemDataPtr   pointer to OEM data structure if required
 * @param[in]   deviceType   Hydra SKU device type
 *
 * @author Mahee
 *
 * @date 06/12/2012
 *
 * This API must be called before any other API's are called for a given
 * devId. Otherwise, a called API function must return MXL_HYDRA_NOT_INITIALIZED
 * without performing any further operation.
 *
 * @retval MXL_SUCCESS            - OK
 * @retval MXL_FAILURE            - Failure
 * @retval MXL_ALREADY_INITIALIZED  - MxLWare is already initilized
 * @retval MXL_INVALID_PARAMETER  - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDrvInit(UINT8 devId, void * oemDataPtr, MXL_HYDRA_DEVICE_E deviceType)
{
  MXL_STATUS_E status = MXL_SUCCESS;
  MXL_HYDRA_CONTEXT_T * devHandlePtr;
  UINT8 i = 0;
  UINT8 j = 0;
  UINT32 regData = 0;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("oemDataPtr = %p, deviceType = %d\n", oemDataPtr, deviceType););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_NOT_INITIALIZED)
  {
    // update initilized flag
    devHandlePtr->initialized = MXL_TRUE;
    devHandlePtr->deviceType = deviceType;

    devHandlePtr->intrMask = 0;

    // store oem data pointer
    MxL_HYDRA_OEM_DataPtr[devId] = (void *)oemDataPtr;

    // update device features like, number of demods, tuners available for the given device type
    switch(devHandlePtr->deviceType)
    {
      case MXL_HYDRA_DEVICE_581:
        devHandlePtr->features.clkOutAvailable = MXL_TRUE;
        devHandlePtr->features.demodsCnt = 8;
        devHandlePtr->features.tunersCnt = 1;
        break;

      case MXL_HYDRA_DEVICE_585:
        devHandlePtr->features.clkOutAvailable = MXL_FALSE;
        devHandlePtr->features.demodsCnt = 8;
        devHandlePtr->features.tunersCnt = 4;
        break;

      case MXL_HYDRA_DEVICE_544:
        devHandlePtr->features.clkOutAvailable = MXL_FALSE;
        devHandlePtr->features.demodsCnt = 4;
        devHandlePtr->features.tunersCnt = 4;
        break;

      case MXL_HYDRA_DEVICE_561:
        devHandlePtr->features.clkOutAvailable = MXL_FALSE;
        devHandlePtr->features.demodsCnt = 6;
        devHandlePtr->features.tunersCnt = 4;
        break;

      case MXL_HYDRA_DEVICE_TEST:
      case MXL_HYDRA_DEVICE_584:
      default:
        devHandlePtr->features.clkOutAvailable = MXL_FALSE;
        devHandlePtr->features.demodsCnt = 8;
        devHandlePtr->features.tunersCnt = 4;
        break;
    }

    //MxLWare_Hydra_ReadByMnemonic(devId, PAD_MUX_BOND_OPTION, &regData);
    GET_REG_FIELD_DATA(PAD_MUX_BOND_OPTION, &regData);

    // default XTAL value is 24MHz
    devHandlePtr->xtalFreq = MXL_HYDRA_XTAL_24_MHz;

    // PID filter bank to use while programming PID filters
    devHandlePtr->pidFilterBank = MXL_HYDRA_PID_BANK_A;

    // by default TS muxing is disabled i.e 1 to 1 mux
    devHandlePtr->tsMuxModeSlice0 = MXL_HYDRA_TS_MUX_DISABLE;
    devHandlePtr->tsMuxModeSlice1 = MXL_HYDRA_TS_MUX_DISABLE;
    devHandlePtr->sharedPidFilterSize = MXL_HYDRA_SHARED_PID_FILT_SIZE_DEFAULT;

    // assign default broadcast standard as DVB-S
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_0]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_1]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_2]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_3]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_4]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_5]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_6]  = MXL_HYDRA_DVBS;
    devHandlePtr->bcastStd[MXL_HYDRA_DEMOD_ID_7]  = MXL_HYDRA_DVBS;

    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_0]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_1]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_2]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_3]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_4]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_5]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_6]  = MXL_FALSE;
    devHandlePtr->dss2dvbEnacapMode[MXL_HYDRA_DEMOD_ID_7]  = MXL_FALSE;

    // Init pid filters
    for (i = 0; i < MXL_HYDRA_DEMOD_MAX; i++)
    {
      MXLWARE_OSAL_MEMSET(&devHandlePtr->fixedPidFilt[i][0], 0, (MXL_HYDRA_TS_FIXED_PID_FILT_SIZE * sizeof(MXL_HYDRA_TS_PID_T)));
      MXLWARE_OSAL_MEMSET(&devHandlePtr->regPidFilt[i][0], 0, (MXL_HYDRA_SHARED_PID_FILT_SIZE_DEFAULT * sizeof(MXL_HYDRA_TS_PID_T)));

      if (i < MXL_HYDRA_DEMOD_ID_4)
      {
        // update register address for slice 0
        for (j = 0; j < MXL_HYDRA_SHARED_PID_FILT_SIZE_DEFAULT; j++)
        {
          // register address - (based Addr) + (demodID offset) + (pid filter item's offset)
          if (j < MXL_HYDRA_TS_FIXED_PID_FILT_SIZE)
            devHandlePtr->fixedPidFilt[i][j].regAddr = XPT_KNOWN_PID_BASEADDR + ((i % MXL_HYDRA_DEMOD_ID_4) * 0x80) + (j * 4);

          devHandlePtr->regPidFilt[i][j].regAddr = XPT_PID_BASEADDR + ((i % MXL_HYDRA_DEMOD_ID_4) * 0x200) + (j * 8);
        }
      }
      else
      {
        // update register address for slice 1
        for (j = 0; j < MXL_HYDRA_SHARED_PID_FILT_SIZE_DEFAULT; j++)
        {
          // register address - (based Addr) + (demodID offset) + (pid filter item's offset)
          if (j < MXL_HYDRA_TS_FIXED_PID_FILT_SIZE)
            devHandlePtr->fixedPidFilt[i][j].regAddr = XPT_KNOWN_PID_BASEADDR1 + ((i % MXL_HYDRA_DEMOD_ID_4) * 0x80) + (j * 4);

          devHandlePtr->regPidFilt[i][j].regAddr = XPT_PID_BASEADDR1 + ((i % MXL_HYDRA_DEMOD_ID_4) * 0x200) + (j * 8);
        }
      }
    }

    status = MXL_SUCCESS;
    MXLDBG1(MXL_HYDRA_PRINT("[%s] Device %s initialized\n", __FUNCTION__, MxLWare_HYDRA_DBG_GetDeviceName(deviceType)););
  }
  else if (status == MXL_SUCCESS)
  {
    status = MXL_ALREADY_INITIALIZED;
    MXLERR(MXL_HYDRA_PRINT("[%s] The device%d is already initialized\n", __FUNCTION__, devId););
  }

  MXLEXITAPISTR(devId, status);

  return status;
}

/**
 ************************************************************************
 *
 * @brief MxLWare_HYDRA_API_CfgDrvUnInit
 *
 * @param[in]   devId        Device ID
 *
 * @author Mahee
 *
 * @date 06/12/2012
 *
 * This API must be called while closing MxLWare driver interface.
 *
 * @retval MXL_SUCCESS              - OK
 * @retval MXL_FAILURE              - Failure
 * @retval MXL_INVALID_PARAMETER    - Invalid parameter is passed
 *
 ************************************************************************/

MXL_STATUS_E MxLWare_HYDRA_API_CfgDrvUnInit(UINT8 devId)
{
  MXL_STATUS_E status = MXL_SUCCESS;
  MXL_HYDRA_CONTEXT_T * devHandlePtr;

  MXLENTERAPISTR(devId);
  MXLENTERAPI(MXL_HYDRA_PRINT("devId = %d\n", devId););

  status = MxLWare_HYDRA_Ctrl_GetDeviceContext(devId, &devHandlePtr);
  if (status == MXL_SUCCESS)
  {
    // update initilized flag
    devHandlePtr->initialized = MXL_FALSE;

    devHandlePtr->intrMask = 0;

    // store oem data pointer
    MxL_HYDRA_OEM_DataPtr[devId] = (void *)0;

    // update device features like, number of demods, tuners available for the given device type
    devHandlePtr->features.demodsCnt = 0;
    devHandlePtr->features.tunersCnt = 0;

    status = MXL_SUCCESS;

  }
  else if (status == MXL_NOT_INITIALIZED)
  {
    status = MXL_NOT_INITIALIZED;
    MXLERR(MXL_HYDRA_PRINT("[%s] The device%d is already initialized\n", __FUNCTION__, devId););
  }

  MXLEXITAPISTR(devId, status);

  return status;
}
