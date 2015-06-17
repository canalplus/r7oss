/***********************************************************************
 *
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include <stm_vxi.h>
#include <vxi/ip/vxi.h>
#include "stiH416vxiDevice.h"
#include "stiH416reg.h"

CSTiH416vxiDevice::CSTiH416vxiDevice(stm_vxi_platform_data_t *pData):CVxiDevice(STiH416_VXI_INSTANCES_COUNT)

{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  m_platformData = *pData;
  m_pReg = (uint32_t*)vibe_os_map_memory(pData->soc.reg, pData->soc.size);
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CSTiH416vxiDevice::~CSTiH416vxiDevice()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  RemoveVxis();

  vibe_os_unmap_memory(m_pReg);
  m_pReg = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CSTiH416vxiDevice::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!CVxiDevice::Create())
  {
    TRC( TRC_ID_ERROR, "Base class Create failed" );
    return false;
  }

  if(!CreateVxis())
  {
    TRC( TRC_ID_ERROR, "Vxi class Create failed" );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}


bool CSTiH416vxiDevice::CreateVxis()
{
  if(!AddVxi(new CVxi("VXI-0", 0, this, &m_platformData)))
  {
    TRC( TRC_ID_ERROR, "Failed to add vxi" );
    return false;
  }

  return true;

}

/*
 * This is the top level of device creation.
 * There should be exactly one of these per kernel module.
 */
CVxiDevice* AnonymousCreateVxiDevice(stm_vxi_platform_data_t *data)
{
    return new CSTiH416vxiDevice(data);
}
