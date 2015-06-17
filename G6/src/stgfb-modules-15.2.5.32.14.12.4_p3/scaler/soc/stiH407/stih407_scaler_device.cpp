/***********************************************************************
 *
 * File: scaler/soc/stiH407/stih407_scaler_device.cpp
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

#include "stih407_scaler_device.h"
#include "blitter_scaler.h"

CStiH407ScalerDevice::CStiH407ScalerDevice(void): CScalerDevice(STiH407_SCALER_COUNT)
{
}

CStiH407ScalerDevice::~CStiH407ScalerDevice()
{
  RemoveScalers();
}

bool CStiH407ScalerDevice::Create(void)
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

bool CStiH407ScalerDevice::CreateScalers(void)
{
  uint32_t scaler_id  = 0;

  /* On STih407, we should use the blitter scaler */
  if(!AddScaler(new CBlitterScaler(scaler_id++, STiH407_BLITTER_USED_FOR_SCALING, this) ) )
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
  return new CStiH407ScalerDevice();
}

