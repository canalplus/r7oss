/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2011-2014 STMicroelectronics - All Rights Reserved
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

#include "QueueBufferInterface.h"

CQueueBufferInterface::CQueueBufferInterface ( uint32_t interfaceID, CDisplaySource * pSource )
    : CSourceInterface(interfaceID, pSource)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Create Queue Buffer Interface %p with Id = %d", this, interfaceID );

  m_pDisplayDevice = pSource->GetParentDevice();

  m_lock            = 0;

  vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));

  TRC( TRC_ID_MAIN_INFO, "Created Queue Buffer Interface  source = %p", this );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CQueueBufferInterface::~CQueueBufferInterface(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Destroying QueueBufferInterface %p", this );
  vibe_os_delete_resource_lock(m_lock);

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CQueueBufferInterface::Create(void)
{
  CSourceInterface * pSI = (CSourceInterface *)this;
  CDisplaySource   * pDS = GetParentSource();

  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Creating QueueBufferInterface %p",this );

  m_lock = vibe_os_create_resource_lock();
  if (!m_lock)
  {
    TRC( TRC_ID_ERROR, "failed to allocate resource lock" );
    return false;
  }

  if (!(pDS->RegisterInterface(pSI)))
  {
    TRC( TRC_ID_ERROR, "failed to register interface %p within source %p", pSI, pDS );
    return false;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

uint32_t CQueueBufferInterface::SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val)
{
  // Default, return failure for an unsupported control
  return STM_SRC_NO_CTRL;
}

uint32_t CQueueBufferInterface::GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const
{
  // Default, return failure for an unsupported control
  return STM_SRC_NO_CTRL;
}

TuningResults CQueueBufferInterface::SetTuning( uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize)
{
    TuningResults res = TUNING_SERVICE_NOT_SUPPORTED;
    tuning_service_type_t  serviceType = (tuning_service_type_t)service;

    switch(serviceType)
    {
        case RESET_STATISTICS:
        {
            vibe_os_zero_memory(&m_Statistics, sizeof(m_Statistics));
            res = TUNING_OK;
            break;
        }
        default:
            break;
    }
    return res;
}

bool CQueueBufferInterface::RegisterStatistics(void)
{
  char Tag[STM_REGISTRY_MAX_TAG_SIZE];

  vibe_os_zero_memory(&Tag, STM_REGISTRY_MAX_TAG_SIZE);
  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_PicturesOwned");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicOwned, sizeof(m_Statistics.PicOwned))!= 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_interfaceID );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_interfaceID );
  }

  vibe_os_zero_memory(&Tag, STM_REGISTRY_MAX_TAG_SIZE);
  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_PicturesReleased");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicReleased, sizeof(m_Statistics.PicReleased))!= 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_interfaceID );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_interfaceID );
  }

  vibe_os_zero_memory(&Tag, STM_REGISTRY_MAX_TAG_SIZE);
  vibe_os_snprintf (Tag, sizeof(Tag), "Stat_PicturesQueued");
  if(stm_registry_add_attribute((stm_object_h)this, Tag, STM_REGISTRY_UINT32, &m_Statistics.PicQueued, sizeof(m_Statistics.PicQueued))!= 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' attribute (%d)", Tag, m_interfaceID );
    return false;
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", Tag, m_interfaceID );
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
// C Display source interface
//

extern "C" {

int stm_display_source_queue_lock(stm_display_source_queue_h queue, bool force)
{
  CQueueBufferInterface * pSQ = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSQ = queue->handle;

  if(stm_display_device_is_suspended(queue->parent_dev))
    return -EAGAIN;

  if(STKPI_LOCK(queue->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE_QUEUE, "New owner : 0x%p", queue);
  res = pSQ->LockUse(queue, force) ? 0:-ENOLCK;

  STKPI_UNLOCK(queue->parent_dev);

  return res;
}


int stm_display_source_queue_unlock(stm_display_source_queue_h queue)
{
  CQueueBufferInterface * pSQ = NULL;

  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSQ = queue->handle;

  if(stm_display_device_is_suspended(queue->parent_dev))
    return -EAGAIN;

  if(STKPI_LOCK(queue->parent_dev) != 0)
    return -EINTR;

  pSQ->Unlock(queue);
  TRC(TRC_ID_API_SOURCE_QUEUE, "Queue unlocked by 0x%p", queue);

  STKPI_UNLOCK(queue->parent_dev);

  return 0;
}


int stm_display_source_queue_buffer(stm_display_source_queue_h queue, const stm_display_buffer_t *pBuffer)
{
  CQueueBufferInterface * pSQ = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSQ = queue->handle;

  if(stm_display_device_is_suspended(queue->parent_dev))
    return -EAGAIN;

  if(!CHECK_ADDRESS(pBuffer))
    return -EFAULT;

  if(STKPI_LOCK(queue->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_QUEUE_BUFFER, "hdl 0x%p on SrcQueue %d", queue, pSQ->GetID() );

  TRC(TRC_ID_QUEUE_BUFFER, "src flags : %x, x : %d, y : %d, width : %u, height : %u, presentation_time : %lld, PTS : %lld",
      pBuffer->src.flags,
      pBuffer->src.visible_area.x,
      pBuffer->src.visible_area.y,
      pBuffer->src.visible_area.width,
      pBuffer->src.visible_area.height,
      pBuffer->info.presentation_time,
      pBuffer->info.PTS);
  res = pSQ->QueueBuffer(pBuffer, queue)?0:-ENOLCK;

  STKPI_UNLOCK(queue->parent_dev);

  return res;
}


int stm_display_source_queue_flush(stm_display_source_queue_h queue, bool flush_buffers_on_display)
{
  CQueueBufferInterface * pSQ = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSQ = queue->handle;

  if(stm_display_device_is_suspended(queue->parent_dev))
    return -EAGAIN;

  if(STKPI_LOCK(queue->parent_dev) != 0)
  {
    TRC(TRC_ID_MAIN_INFO, "!!!WARNING!!! Flush command interrupted!!!");
    return -EINTR;
  }

  TRC(TRC_ID_API_SOURCE_QUEUE, "hdl 0x%p on SrcQueue %d. flush_buffers_on_display : %d", queue, pSQ->GetID(), flush_buffers_on_display);
  res = pSQ->Flush(flush_buffers_on_display, queue, true)?0:-ENOLCK;

  STKPI_UNLOCK(queue->parent_dev);

  return res;
}


int stm_display_source_queue_get_pixel_formats(stm_display_source_queue_h queue, stm_pixel_format_t *formats, uint32_t max_formats)
{
  CQueueBufferInterface * pSQ = NULL;
  int n, i;
  const stm_pixel_format_t *buffer_formats;

  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  pSQ = queue->handle;

  if(!CHECK_ADDRESS(formats))
    return -EFAULT;

  if(STKPI_LOCK(queue->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE_QUEUE, "Queue 0x%p", queue);
  n = pSQ->GetBufferFormats(&buffer_formats);

  // copy the formats in the specified array
  n = MIN((uint32_t)n, max_formats);
  for(i=0; i<n; i++)
    formats[n]=buffer_formats[n];

  STKPI_UNLOCK(queue->parent_dev);

  return n;
}

int stm_display_source_queue_release(stm_display_source_queue_h queue)
{
  if(!stm_coredisplay_is_handle_valid(queue, VALID_SRC_INTERFACE_HANDLE))
    return -EINVAL;

  if(stm_display_device_is_suspended(queue->parent_dev))
    return -EAGAIN;

  stm_coredisplay_magic_clear(queue);

  if(STKPI_LOCK(queue->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE_QUEUE, "Queue 0x%p", queue);

  STKPI_UNLOCK(queue->parent_dev);

  /*
   * Remove Source Interface instance from stm_registry database.
   */
  if(stm_registry_remove_object(queue) < 0)
    TRC( TRC_ID_ERROR, "failed to remove queue instance from the registry" );

  delete queue;

  return 0;
}

} // extern "C"
