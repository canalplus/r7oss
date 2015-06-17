/***********************************************************************
 *
 * File: scaler/soc/stiH416/stih416_scaler_device.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include <stm_scaler.h>

#include "stih416_scaler_device.h"
#include "fvdp_scaler.h"

CStiH416ScalerDevice::CStiH416ScalerDevice(void): CScalerDevice(STiH416_SCALER_COUNT)
{
}

CStiH416ScalerDevice::~CStiH416ScalerDevice()
{
  RemoveScalers();
}

bool CStiH416ScalerDevice::Create(void)
{

  if(!CScalerDevice::Create())
  {
    TRC( TRC_ID_ERROR, "Base class Create failed" );
    return false;
  }

  if(!CreateScalers())
  {
    return false;
  }

  return(true);
}

bool CStiH416ScalerDevice::CreateScalers(void)
{
  uint32_t scaler_id = 0;

  /* On STih416, only the channel FVDP_CHANNEL_ENC is used by the scaler */
  if(!AddScaler(new CFvdpScaler(scaler_id++, this, FVDP_CHANNEL_ENC) ) )
  {
    TRC( TRC_ID_ERROR, "Failed to add scaler" );
    return false;
  }

  return true;
}

/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 * When this is called only vibe_os will have been initialised.
 */
CScalerDevice* AnonymousCreateScalerDevice(void)
{
  return new CStiH416ScalerDevice();
}

