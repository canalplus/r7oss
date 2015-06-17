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

#include "DisplayDevice.h"
#include "DisplaySource.h"
#include "DisplayPlane.h"
#include "SourceInterface.h"
#include "QueueBufferInterface.h"
#include "PixelStreamInterface.h"
#include <display_device_priv.h>

CDisplaySource::CDisplaySource(const char                   *name,
                               uint32_t                      sourceID,
                               const CDisplayDevice         *pDev,
                               uint32_t                      maxNumConnectedPlanes)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "Create Source %s with Id = %d", name, sourceID );

  m_name            = name;
  m_sourceID        = sourceID;
  m_pDisplayDevice  = pDev;
  m_maxNumConnectedPlanes = maxNumConnectedPlanes;

  m_pConnectedPlanes    = 0;
  m_owner               = 0;
  m_numConnectedPlanes  = 0;

  m_LastSync            = STM_TIMING_EVENT_NONE;
  m_LastVSyncTime       = (stm_time64_t)0;
  m_Status              = 0;
  m_capabilities = CAPS_NONE;
  m_pSourceInterface = 0;

  m_hasADeinterlacer     = false;
  m_isUsedByVideoPlanes  = false;

  TRC( TRC_ID_MAIN_INFO, "Source = %p named %s created", this, name );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CDisplaySource::~CDisplaySource(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete[] m_pConnectedPlanes;

  if(m_pSourceInterface)
  {
    /*
     * Remove Source Interface object from stm_registry database.
     */
    if(stm_registry_remove_object(m_pSourceInterface) < 0)
      TRC( TRC_ID_ERROR, "failed to remove queue (%p) object from the registry",m_pSourceInterface );
    delete (CSourceInterface *)m_pSourceInterface;
  }

  TRC( TRC_ID_MAIN_INFO, "Source %s Destroyed", m_name );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CDisplaySource::Create(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRC( TRC_ID_MAIN_INFO, "source %p created",this );

  m_pConnectedPlanes = new CDisplayPlane*[m_maxNumConnectedPlanes];
  if(!m_pConnectedPlanes)
  {
    TRC( TRC_ID_ERROR, "failed to allocate planes array" );
    return false;
  }

  // Initialize all CDisplayPlane* pointer to NULL value.
  vibe_os_zero_memory(m_pConnectedPlanes, m_maxNumConnectedPlanes * sizeof(CDisplayPlane*));

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

bool CDisplaySource::CanConnectToPlane(CDisplayPlane *pPlane)
{
  bool IsConnectionPossible = false;

  // Check if this source can be connected to this plane. The source is generic
  // so this is in fact the interface that matters
  if(m_pSourceInterface)
  {
    IsConnectionPossible =  m_pSourceInterface->IsConnectionPossible(pPlane);
  }

  return(IsConnectionPossible);
}

bool CDisplaySource::ConnectPlane(CDisplayPlane *pPlane)
{
  uint32_t        numId = 0;

  // Catch an empty slot to connect the source to the plane
  for(numId = 0; numId<m_maxNumConnectedPlanes; numId++)
  {
    if ( m_pConnectedPlanes[numId] == 0 )
      break;
  }

  // Check that m_pConnectedPlanes[] will not overflow
  if ( (m_numConnectedPlanes >= m_maxNumConnectedPlanes) || (numId >= m_maxNumConnectedPlanes) )
  {
      TRC( TRC_ID_ERROR, "Source %d can not be connected to plane %d max m_numConnectedPlanes=%d reached", GetID(), pPlane->GetID(), m_numConnectedPlanes  );
      return false;
  }

  //if this is the first plane to be connected
  if (m_numConnectedPlanes == 0)
  {
    if(m_pSourceInterface)
    {
      TRC( TRC_ID_MAIN_INFO, "First plane to be connected to source %s: We can update its interface status", GetName() );

      if( pPlane->GetConnectedOutputID(NULL, 0) != 0 )
      {
        // An output is connected to this plane
        // So save timingid and output information into the source interface
        m_pSourceInterface->UpdateOutputInfo(pPlane);
      }

      // then set status of the hardware which is controlled by this interface.
      // This is telling the interface to start IT if it does have one.
      if(!m_pSourceInterface->SetInterfaceStatus(STM_SRC_INTERFACE_IN_USE))
      {
        TRC( TRC_ID_ERROR, "Interface %p status can not be updated", m_pSourceInterface );
        return false;
      }
    }
  }

  // Store that this plane is connected to this source
  m_pConnectedPlanes[numId] = pPlane;
  m_numConnectedPlanes++;
  SetStatus(STM_STATUS_SOURCE_CONNECTED, true);

  // If at least one plane, connected to this source, has deinterlacer,
  // the source should save this info for providing previous, current and next pictures.
  if( pPlane->hasADeinterlacer() )
  {
    m_hasADeinterlacer = true;
  }

  if( pPlane->isVideoPlane() )
  {
    m_isUsedByVideoPlanes = true;
  }

  TRC( TRC_ID_MAIN_INFO, "Source %s connected to plane %s", GetName(), pPlane->GetName()  );

  return true;
}

bool CDisplaySource::DisconnectPlane(CDisplayPlane *pPlane)
{
  uint32_t        numId = 0;

  // Catch the plane ID from connected plane list in Source
  for(numId = 0; numId<m_maxNumConnectedPlanes; numId++)
  {
    if ( m_pConnectedPlanes[numId] == pPlane )
      break;
  }

  if (numId == m_maxNumConnectedPlanes)
  {
    TRC( TRC_ID_ERROR, "plane %s was not owned by source %s", pPlane->GetName(), GetName() );
    return false;
  }

  TRC( TRC_ID_MAIN_INFO, "Source %s disconnected from plane %s", GetName() , pPlane->GetName() );
  m_pConnectedPlanes[numId] = 0;
  m_numConnectedPlanes--;

  // If it remains at least one plane with a deinterlacer, connected to this source,
  // the source should save this info for providing previous, current and next pictures.
  UpdateHasDeinterlacer();

  if (m_numConnectedPlanes == 0)
  {
    if (m_pSourceInterface)
    {
      TRC( TRC_ID_MAIN_INFO, "No more plane connected to source %u: We can update its interface status", GetID()  );

      // When a source is no more connected to a plane, don't clear its TimingID. It could be a standalone source
      // like in case of swap. The TimingID will be updated by a new connection plane-source.

      // This was the last plane connected to this Source Interface so we can update the interface status
      if(!m_pSourceInterface->SetInterfaceStatus(STM_SRC_INTERFACE_NOT_USED))
      {
        TRC( TRC_ID_ERROR, "Failed to update the source interface status" );
        return false;
      }
    }
    m_isUsedByVideoPlanes  = false;
    SetStatus(STM_STATUS_SOURCE_CONNECTED, false);
  }

  return true;
}

void CDisplaySource::UpdateHasDeinterlacer(void)
{
  uint32_t        numId = 0;
  CDisplayPlane*  pDP   = 0;

  if ( m_numConnectedPlanes > 0 )
  {
    // Parse all connected planes to see if one has deinterlacer
    for(numId = 0; (numId<m_maxNumConnectedPlanes); numId++)
    {
      pDP  = m_pConnectedPlanes[numId];
      if (pDP && pDP->hasADeinterlacer() )
      {
        // Found one plane with deinterlacer
        m_hasADeinterlacer = true;
        return;
      }
    }
  }

  // No plane with deinterlacer found
  m_hasADeinterlacer = false;
}


int CDisplaySource::GetConnectedPlaneID(uint32_t *id, uint32_t max_ids)
{
  uint32_t        numId = 0;

  TRC( TRC_ID_UNCLASSIFIED, "CDisplaySource::GetConnectedPlaneID source = %u id = %p max_ids = %d", GetID() ,id ,max_ids );

  if (id == 0)
    return m_numConnectedPlanes;

  for(uint32_t i=0; (i<m_maxNumConnectedPlanes) && (numId<max_ids) ; i++)
  {
      CDisplayPlane* pDP  = m_pConnectedPlanes[i];
      if (pDP)
      {
        id[numId++] = pDP->GetID();
      }
  }

  return numId;
}


int CDisplaySource::GetConnectedPlaneCaps(stm_plane_capabilities_t *caps, uint32_t max_caps)
{
  uint32_t        numCaps = 0;

  TRC( TRC_ID_MAIN_INFO, "CDisplaySource::GetConnectedPlaneCaps source = %u max_caps = %u caps = %p", GetID(), max_caps, caps );

  for(uint32_t i=0; (i<m_maxNumConnectedPlanes) && (numCaps<max_caps); i++)
  {
      CDisplayPlane* pDP  = m_pConnectedPlanes[i];
      if (pDP)
        caps[numCaps++] = pDP->GetCapabilities();
  }

  return numCaps;
}


CSourceInterface* CDisplaySource::GetInterface(const stm_display_source_interfaces_t interface)
{
  CSourceInterface *pInterface = 0;

  TRC( TRC_ID_UNCLASSIFIED, "CDisplaySource::GetInterface sourceID = %u interface = %d", GetID() ,interface );

  if( (m_pSourceInterface) && (interface == m_pSourceInterface->GetInterfaceType() ) )
  {
    // This source has the right kind of interface
    pInterface = m_pSourceInterface;
  }

  return pInterface;
}

uint32_t CDisplaySource::GetTimingID(void) const
{
  if(m_pSourceInterface)
  {
    return m_pSourceInterface->GetTimingID();
  }
  else
  {
    return 0;
  }
}


int32_t CDisplaySource::GetSourceInterfaceHwId(void)
{
  if(m_pSourceInterface)
  {
    return m_pSourceInterface->GetHwId();
  }
  else
  {
    return 0;
  }
}


void CDisplaySource::SourceVSyncUpdateHW (void)
{
  bool                isInterlaced = false;
  bool                isTopVSync   = (GetCurrentVSync() == STM_TIMING_EVENT_TOP_FIELD);
  const stm_time64_t  vsyncTime    = GetCurrentVSyncTime();

  // Pass the information to the Interface (which will pass it to the connected planes)
  if(m_pSourceInterface && (m_numConnectedPlanes>0))
  {
    m_pSourceInterface->SourceVSyncUpdateHW(isInterlaced, isTopVSync, vsyncTime);
  }
}


void CDisplaySource::OutputVSyncThreadedIrqUpdateHW (bool isdisplayInterlaced,
                                                     bool isTopFieldOnDisplay,
                                                     const stm_time64_t &vsyncTime)
{
  // Pass the information to the Interface
  if ( m_pSourceInterface )
  {
    m_pSourceInterface->OutputVSyncThreadedIrqUpdateHW(isdisplayInterlaced, isTopFieldOnDisplay, vsyncTime);
  }
}


TuningResults CDisplaySource::SetTuning(uint16_t service, void *inputList, uint32_t inputListSize, void *outputList, uint32_t outputListSize)
{
  TuningResults res = TUNING_SERVICE_NOT_SUPPORTED;
  TRC( TRC_ID_UNCLASSIFIED, "CDisplaySource::SetTuning() m_pSourceInterface = %p", m_pSourceInterface );

  if ( (m_pSourceInterface) && (m_pSourceInterface->GetInterfaceType() == STM_SOURCE_QUEUE_IFACE) )
  {
    CQueueBufferInterface *pQueueBufferInterface = (CQueueBufferInterface *) m_pSourceInterface;

    TRC( TRC_ID_UNCLASSIFIED, "pQueueBufferInterface = %p", pQueueBufferInterface );
    res = pQueueBufferInterface->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
  }

  return res;
}

bool CDisplaySource::RegisterInterface(CSourceInterface *pInterface)
{
  char                          InterfaceTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRC( TRC_ID_MAIN_INFO, "CDisplaySource::RegisterInterface Source = %u Interface = %p", GetID() ,pInterface );

  if(!pInterface)
    return false;

  m_pSourceInterface = pInterface;

  /*
   * Add real Source Interface object into stm_registry database. This
   * will be used in stm_display_source_get_interface() as parent object.
   */
  vibe_os_snprintf (InterfaceTag, sizeof(InterfaceTag), "stm_source_interface%d",
                    m_pSourceInterface->GetID());
  if (stm_registry_add_object((stm_object_h)this, InterfaceTag,
                    (stm_object_h)m_pSourceInterface) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' interface id = (%d)",  InterfaceTag, m_pSourceInterface->GetID() );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' interface id = (%d)",  InterfaceTag,  m_pSourceInterface->GetID() );

    if (m_pSourceInterface->GetInterfaceType() == STM_SOURCE_QUEUE_IFACE)
    {
      CQueueBufferInterface *pQueueBufferInterface = (CQueueBufferInterface *) m_pSourceInterface;

      // Register the attributes to collect statistics on the source
      pQueueBufferInterface->RegisterStatistics();
    }
  }

  return true;
}


bool CDisplaySource::HandleInterrupts(void)
{
  stm_display_timing_events_t   sync;
  stm_time64_t  now;

  if(m_pSourceInterface)
  {
    sync = m_pSourceInterface->GetInterruptStatus();

    //ASSERTF((sync != STM_TIMING_EVENT_NONE) && ((sync & STM_TIMING_EVENT_LINE) == 0),("CDisplaySource::HandleInterrupts bad interrupt status\n"));

    m_LastSync = sync;

    now = vibe_os_get_system_time();

    if(m_LastVSyncTime != 0)
    {
      stm_time64_t timediff = now - m_LastVSyncTime;

      if(timediff < 0LL)
      {
        TRC( TRC_ID_ERROR, "backwards time detected, last time = %lld now = %lld", m_LastVSyncTime,now );
      }
    }

    m_LastVSyncTime = now;
  }

  return true;
}


uint32_t CDisplaySource::SetControl(stm_display_source_ctrl_t ctrl, uint32_t ctrl_val)
{
  uint32_t ret = STM_SRC_NO_CTRL;

  // Default, forward the call to the SourceInterface class

  if(m_pSourceInterface)
    ret = m_pSourceInterface->SetControl(ctrl, ctrl_val);

  return ret;
}

uint32_t CDisplaySource::GetControl(stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) const
{
  uint32_t ret = STM_SRC_NO_CTRL;

  // Default, forward the call to the SourceInterface class

  if(m_pSourceInterface)
    ret = m_pSourceInterface->GetControl(ctrl, ctrl_val);

  return ret;
}

int CDisplaySource::GetCapabilities(stm_display_source_caps_t *caps)
{
  // Default, return failure for an unsupported control
  return -ENOTSUP;
}

void CDisplaySource::SetStatus(stm_display_status_t statusFlag, bool set)
{
    if(set)
        m_Status |= statusFlag;
    else
        m_Status &= ~statusFlag;
}

void CDisplaySource::Freeze(void)
{
  if(m_pSourceInterface)
    m_pSourceInterface->Freeze();
}

void CDisplaySource::Suspend(void)
{
  if(m_pSourceInterface)
    m_pSourceInterface->Suspend();
}

void CDisplaySource::Resume(void)
{
  if(m_pSourceInterface)
    m_pSourceInterface->Resume();
}

////////////////////////////////////////////////////////////////////////////////
// C Display source interface
//

extern "C" {

int stm_display_source_get_name(stm_display_source_h source, const char **name)
{
  CDisplaySource * pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(name))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  *name = pDS->GetName();

  STKPI_UNLOCK(source->parent_dev);

  return 0;
}


int stm_display_source_get_timing_identifier(stm_display_source_h source, uint32_t *timing_id)
{
  CDisplaySource * pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(timing_id))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  *timing_id = pDS->GetTimingID();

  STKPI_UNLOCK(source->parent_dev);

  return (*timing_id == 0)?-ENOTSUP:0;
}

int stm_display_source_get_device_id(stm_display_source_h source, uint32_t *id)
{
  CDisplaySource * pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(id))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  *id = pDS->GetParentDevice()->GetID();

  STKPI_UNLOCK(source->parent_dev);

  return 0;
}


int stm_display_source_get_connected_plane_id(stm_display_source_h source, uint32_t *id, uint32_t max_ids)
{
  CDisplaySource * pDS = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  res = pDS->GetConnectedPlaneID(id, max_ids);
  TRC(TRC_ID_API_SOURCE, "source : %s, max_ids : %u, connected planes : %d", pDS->GetName(), max_ids, res);

  if ( id ) {
    uint32_t *p = (uint32_t *)id;
    for ( int i = 0; i < res; i++, p++ ) {
      TRC(TRC_ID_API_SOURCE, "plane id : %d", *p );
    }
  }

  STKPI_UNLOCK(source->parent_dev);
  return res;
}


int stm_display_source_get_connected_plane_caps(stm_display_source_h source, stm_plane_capabilities_t *caps, uint32_t max_caps)
{
  CDisplaySource * pDS = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(caps))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  res = pDS->GetConnectedPlaneCaps(caps,max_caps)?0:-1;

  STKPI_UNLOCK(source->parent_dev);

  return res;
}

int stm_display_source_get_interface(stm_display_source_h source, stm_display_source_interface_params_t iface_params, void **iface_handle)
{
  CDisplaySource                  * pDS = NULL;
  stm_display_source_queue_h        public_interface_queue = 0;
  stm_display_source_pixelstream_h  public_interface_pixelstream = 0;
  stm_display_pixel_stream_params_t pixel_stream_params;
  void                      *public_interface = 0;
  CSourceInterface *         real_interface;
  char                       InterfaceTag[STM_REGISTRY_MAX_TAG_SIZE];
  int                        ret = 0;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(iface_handle))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  real_interface = pDS->GetInterface(iface_params.interface_type);
  if(!real_interface)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Failed to get a valid source interface object (type = %d) from the source %p (id = %d)", iface_params.interface_type, source, pDS->GetID() );
    ret = -ENOTSUP;
    goto exit;
  }

  if(iface_params.interface_type == STM_SOURCE_PIXELSTREAM_IFACE)
  {
    CPixelStreamInterface *pPixelstreamInterface = (CPixelStreamInterface *)real_interface;

    if(iface_params.interface_params.pixel_stream_params == NULL)
    {
      TRC( TRC_ID_ERROR, "Failed to get a valid source interface object from the pixel stream source type - null" );
      ret = -ENOTSUP;
      goto exit;
    }

    /*
     * Check for expected source type and instance number to obtain the proper pixel stream source
     */
    pPixelstreamInterface->GetParams(&pixel_stream_params);

    if((iface_params.interface_params.pixel_stream_params->source_type != pixel_stream_params.source_type) ||
       (iface_params.interface_params.pixel_stream_params->instance_number != pixel_stream_params.instance_number))
    {
      TRC( TRC_ID_ERROR, "Failed to get a valid source interface object from the pixel stream source type/instance" );
      ret = -ENOTSUP;
      goto exit;
    }

    public_interface_pixelstream = new struct stm_display_source_pixelstream_s;
    if(!public_interface_pixelstream)
    {
      TRC( TRC_ID_ERROR, "Failed to allocate pixel stream interface handle structure" );
      ret = -ENOMEM;
      goto exit;
    }

    public_interface_pixelstream->handle     = pPixelstreamInterface;
    public_interface_pixelstream->parent_dev = source->parent_dev;
    public_interface_pixelstream->owner      = source;

    stm_coredisplay_magic_set(public_interface_pixelstream, VALID_SRC_INTERFACE_HANDLE);
    public_interface = public_interface_pixelstream;
  }
  else
  {
    public_interface_queue = new struct stm_display_source_queue_s;
    if(!public_interface_queue)
    {
      TRC( TRC_ID_ERROR, "Failed to allocate queue interface handle structure" );
      ret = -ENOMEM;
      goto exit;
    }

    public_interface_queue->handle     = (CQueueBufferInterface *)real_interface;
    public_interface_queue->parent_dev = source->parent_dev;

    stm_coredisplay_magic_set(public_interface_queue, VALID_SRC_INTERFACE_HANDLE);
    public_interface = public_interface_queue;
  }

  /*
   * Add the Source Interface instance to the registry.
   */
  vibe_os_snprintf (InterfaceTag, sizeof(InterfaceTag), "source_interface%d-%p",
                    ((class CSourceInterface *)real_interface)->GetID(), public_interface);
  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                  (stm_object_h)real_interface,
                                  InterfaceTag,
                                  (stm_object_h)public_interface) != 0)
    TRC( TRC_ID_ERROR, "Cannot register %s instance (%p)", InterfaceTag, public_interface );

exit:
  STKPI_UNLOCK(source->parent_dev);

  *iface_handle = public_interface;
  TRC(TRC_ID_API_SOURCE, "iface_handle : 0x%p", *iface_handle);

  return ret;
}

void stm_display_source_close(stm_display_source_h source)
{
  if(source)
  {
    TRC(TRC_ID_API_SOURCE, "source : %s", source->handle->GetName());
    stm_coredisplay_magic_clear(source);

    /*
     * Remove object instance from the registry before exiting.
     */
    if(stm_registry_remove_object(source) < 0)
      TRC( TRC_ID_ERROR, "failed to remove source instance from the registry" );

    delete source;
  }
}

int stm_display_source_get_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val)
{
  CDisplaySource * pDS = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(ctrl_val))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());

  if(pDS->GetControl(ctrl, ctrl_val) == STM_SRC_NO_CTRL)
    ret = -ENOTSUP;

  STKPI_UNLOCK(source->parent_dev);

  return ret;
}

int stm_display_source_set_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t new_val)
{
  CDisplaySource * pDS = NULL;
  int ret = 0;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());

  if(pDS->SetControl(ctrl, new_val) == STM_SRC_NO_CTRL)
    ret = -ENOTSUP;

  STKPI_UNLOCK(source->parent_dev);

  return ret;
}

void stm_display_source_handle_interrupts(stm_display_source_h source)
{

  if(stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
  {
    CDisplaySource* pDS  = source->handle;
    TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());

    pDS->HandleInterrupts();
  }
}

void stm_display_source_get_last_timing_event(stm_display_source_h source, stm_display_timing_events_t *field, stm_time64_t *interval)
{
  CDisplaySource * pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return;

  pDS  = source->handle;

  *field    = pDS->GetCurrentVSync();
  *interval = pDS->GetCurrentVSyncTime();
  TRC(TRC_ID_API_SOURCE, "source : %s, field : 0x%04x, vsynctime : %lld", pDS->GetName(), *field, *interval);

#if 0
    if((*field != STM_TIMING_EVENT_NONE) && (*field != STM_TIMING_EVENT_LINE))
        TRC( TRC_ID_MAIN_INFO, "source = %p field = %p interval = %d",source,*field,*interval );
#endif
}

int stm_display_source_get_capabilities(stm_display_source_h source, stm_display_source_caps_t *caps)
{
  CDisplaySource * pDS = NULL;
  int res;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(caps))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_SOURCE, "source : %s", pDS->GetName());
  res = pDS->GetCapabilities(caps);

  STKPI_UNLOCK(source->parent_dev);

  return res;
}


int stm_display_source_get_status(stm_display_source_h source, uint32_t * status)
{
  CDisplaySource * pDS = NULL;

  if(!stm_coredisplay_is_handle_valid(source, VALID_SOURCE_HANDLE))
    return -EINVAL;

  pDS = source->handle;

  if(!CHECK_ADDRESS(status))
    return -EFAULT;

  if(STKPI_LOCK(source->parent_dev) != 0)
    return -EINTR;

  *status = pDS->GetStatus();
  TRC(TRC_ID_API_SOURCE, "source : %s, status = %d", pDS->GetName(), *status);

  STKPI_UNLOCK(source->parent_dev);

  return 0;
}

} // extern "C"
