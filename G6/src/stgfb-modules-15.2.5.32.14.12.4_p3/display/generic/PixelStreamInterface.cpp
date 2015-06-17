/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display_device_priv.h>

#include "PixelStreamInterface.h"

CPixelStreamInterface::CPixelStreamInterface ( uint32_t interfaceID, CDisplaySource * pSource )
    : CSourceInterface(interfaceID, pSource)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Create Pixel Stream Interface %p with Id = %d", this, interfaceID );

  vibe_os_zero_memory( &m_pixel_stream_params, sizeof( m_pixel_stream_params ));

  TRC( TRC_ID_MAIN_INFO, "Created Pixel Stream Interface source = %p", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CPixelStreamInterface::~CPixelStreamInterface(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Destroying CPixelStreamInterface %p", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CPixelStreamInterface::Create(void)
{
  CSourceInterface * pSI = (CSourceInterface *)this;
  CDisplaySource   * pDS = GetParentSource();

  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Creating CPixelStreamInterface %p",this );

  if (!(pDS->RegisterInterface(pSI)))
  {
    TRC( TRC_ID_ERROR, "failed to register interface %p within source %p", pSI, pDS );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

int CPixelStreamInterface::GetParams(stm_display_pixel_stream_params_t* param) const
{
  if(param == NULL)
  {
    return -EINVAL;
  }

  *param = m_pixel_stream_params;

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// C Display source pixel stream interface
//

extern "C" {

int stm_display_source_pixelstream_release(stm_display_source_pixelstream_h p)
{
  CPixelStreamInterface * pSP = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(p, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSP = p->handle;

  if(stm_display_device_is_suspended(p->parent_dev))
    return -EAGAIN;

  if(STKPI_LOCK(p->parent_dev) != 0)
    return -EINTR;

  TRC( TRC_ID_API_SOURCE_PIXEL_STREAM, "pixel stream : %p", p );

  /*
   * Do release activities if necessary in the derived class
   */

  res = pSP->Release(p);

  stm_coredisplay_magic_clear(p);

  STKPI_UNLOCK(p->parent_dev);

  /*
   * Remove Source Interface instance from stm_registry database.
   */
  if(stm_registry_remove_object(p) < 0)
    TRC( TRC_ID_ERROR, "failed to remove pixel stream (%p) instance from the registry",p );

  delete p;

  return 0;
}

int stm_display_source_pixelstream_set_input_params(stm_display_source_pixelstream_h p,
                                                    const stm_display_source_pixelstream_params_t * params)
{
  CPixelStreamInterface * pSP = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(p, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  if(!CHECK_ADDRESS(params))
    return -EFAULT;

  pSP = p->handle;

  if(stm_display_device_is_suspended(p->parent_dev))
    return -EAGAIN;

  TRC( TRC_ID_UNCLASSIFIED, "pixel stream: %p, params: %p", p, params );

  if(STKPI_LOCK(p->parent_dev) != 0)
    return -EINTR;

  TRC( TRC_ID_API_SOURCE_PIXEL_STREAM, "pixel stream : %p", p );
  res = pSP->SetInputParams(p, params);

  STKPI_UNLOCK(p->parent_dev);

  return 0;
}

int stm_display_source_pixelstream_set_signal_status(stm_display_source_pixelstream_h p,
                                                     stm_display_source_pixelstream_signal_status_t status)
{
  CPixelStreamInterface * pSP = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(p, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSP = p->handle;

  if(stm_display_device_is_suspended(p->parent_dev))
    return -EAGAIN;

  TRC( TRC_ID_UNCLASSIFIED, "pixel stream: %p, status: %d", p, status );

  if(STKPI_LOCK(p->parent_dev) != 0)
    return -EINTR;

  TRC( TRC_ID_API_SOURCE_PIXEL_STREAM, "pixel stream : %p", p );

  switch(status)
  {
  case  PIXELSTREAM_SOURCE_STATUS_STABLE:
    res = pSP->SetInterfaceStatus(STM_SRC_INTERFACE_STABLE);
    break;
  case  PIXELSTREAM_SOURCE_STATUS_UNSTABLE:
    res = pSP->SetInterfaceStatus(STM_SRC_INTERFACE_UNSTABLE);
    break;
  default:
    /* do nathing */
    break;
  }
  STKPI_UNLOCK(p->parent_dev);

  return 0;
}

} // extern "C"
