/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2000-2014 STMicroelectronics - All Rights Reserved
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

#include "Output.h"
#include "DisplayPlane.h"
#include "DisplaySource.h"
#include "DisplayDevice.h"
#include <display_device_priv.h>

#define STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX 2
/* 0 means spinlock used  */
/* 1 means semaphore used */
/* 2 means mutex used     */

#define MAX_STKPI_DURATION_IN_US    3000


/*
 * CoreDisplay registry types.
 * We don't manage SourceInterface Objects here. This is managed by
 * the DisplaySource object where the interface is begin registered.
 */
static unsigned stm_display_device_type;  /* common device type */
static unsigned stm_display_output_type;
static unsigned stm_display_plane_type;
static unsigned stm_display_source_type;

static struct {
    const char *tag;
    stm_object_h type;
} stm_display_types[] = {
  {"stm_display_device", &stm_display_device_type},
  {"stm_display_output", &stm_display_output_type},
  {"stm_display_plane",  &stm_display_plane_type },
  {"stm_display_source", &stm_display_source_type},
};

CDisplayDevice::CDisplayDevice(uint32_t id, int nOutputs, int nPlanes, int nSources)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p (id=%u, nOutputs=%d nPlanes=%d)", this, id, nOutputs, nPlanes );

  m_ID                 = id;
  m_nNumTuningServices = 0;
  m_pTuningCaps        = 0;
  m_CurrentVTGSync     = 0;
  m_pOutputs           = 0;
  m_numOutputs         = nOutputs;
  m_pPlanes            = 0;
  m_numPlanes          = nPlanes;
  m_pSources           = 0;
  m_numSources         = nSources;
  m_pReg               = 0;
  m_bIsSuspended       = false;
  m_bIsFrozen          = false;
  m_vsyncLock          = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CDisplayDevice::~CDisplayDevice()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * We have to defer destroying outputs to the subclasses,
   * as their destructors may need to access device registers
   * and we cannot guarantee that those registers are still mapped
   * in the memory space at this point.
   */
  delete[] m_pOutputs;
  delete[] m_pPlanes;
  delete[] m_pSources;

  /*
   * Remove all remaining CoreDisplay's objects from registry database
   */
  RegistryTerm();

  switch ( STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX )
  {
    case 0 :
      vibe_os_delete_resource_lock(m_vsyncLock);
      break;
    case 1 :
      vibe_os_delete_semaphore(m_vsyncLock);
      break;
    case 2 :
      vibe_os_delete_mutex(m_vsyncLock);
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CDisplayDevice::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  ASSERTF((m_pOutputs == 0 && m_pPlanes == 0 && m_pSources == 0),("CDisplayDevice::Create() has already been called"));

  /*
   * It might be odd to have a device without outputs, but there may be a
   * use for such a thing so don't prevent it.
   */
  if(m_numOutputs > 0)
  {
    if(!(m_pOutputs = new COutput *[m_numOutputs]))
      return false;

    for (uint32_t i = 0; i < m_numOutputs; i++) m_pOutputs[i] = 0;
  }

  if(m_numPlanes > 0)
  {
    if(!(m_pPlanes = new CDisplayPlane *[m_numPlanes]))
      return false;

    for (uint32_t i = 0; i < m_numPlanes; i++) m_pPlanes[i] = 0;
  }

  if(m_numSources > 0)
  {
    if(!(m_pSources = new CDisplaySource *[m_numSources]))
      return false;

    for (uint32_t i = 0; i < m_numSources; i++) m_pSources[i] = 0;
  }

  /*
   * Add object types in the stm_registry database.
   */
  RegistryInit();

  switch ( STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX )
  {
    case 0 :
      m_vsyncLock = vibe_os_create_resource_lock();
      break;
    case 1 :
      /* Create semaphore for protecting sources and planes shared datas from stkpi and threaded_irq context */
      m_vsyncLock = vibe_os_create_semaphore(1);      /* Create semaphore counter at 1, so not taken */
      break;
    case 2 :
      /* Create mutex for protecting sources and planes shared datas from stkpi and threaded_irq context */
      m_vsyncLock = vibe_os_create_mutex();
      break;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


int CDisplayDevice::VSyncLock(void) const
{
    int res = 0;

    switch ( STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX )
    {
        case 0 :
            vibe_os_lock_resource(m_vsyncLock);
            break;
        case 1 :
            res = vibe_os_down_semaphore(m_vsyncLock);

            if (res != 0)
            {
                TRC(TRC_ID_MAIN_INFO, "WARNING! VSyncLock was interrupted (%d)", res);

            }
            break;
        case 2 :
            res = vibe_os_lock_mutex(m_vsyncLock);

            if (res != 0)
            {
                TRC(TRC_ID_MAIN_INFO, "WARNING! VSyncLock was interrupted (%d)", res);

            }
            break;
    }

    return ( res );
}

void CDisplayDevice::VSyncUnlock(void) const
{
    switch ( STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX )
    {
        case 0 :
            vibe_os_unlock_resource(m_vsyncLock);
            break;
        case 1 :
            vibe_os_up_semaphore(m_vsyncLock);
            break;
        case 2 :
            vibe_os_unlock_mutex(m_vsyncLock);
            break;
    }
}

void CDisplayDevice::DebugCheckVSyncLockTaken(const char *function_name) const
{
    // Check if m_vsyncLock was taken in the specific function calling it.
    // It is a security check for debug activity.
    // Normally, we should check that this thread is owning the m_vsyncLock but
    // as we haven't this feature, we try to get the m_vsyncLock to see if a thread
    // has already taken it.

    if ( vibe_os_in_interrupt() != 0 )
    {
        TRC(TRC_ID_ERROR, "Error: %s called from interrupt context", function_name);
        return;
    }

    switch ( STM_DISPLAY_USE_SPINLOCK_ELSE_SEMAPHORE_OR_MUTEX )
    {
        case 1 :
            if ( vibe_os_down_trylock_semaphore(m_vsyncLock) == 0)
            {
                // m_vsyncLock was available and taken by vibe_os_down_trylock_semaphore()
                TRC(TRC_ID_ERROR, "Error: %s has not taken m_vsyncLock previously", function_name);
                vibe_os_up_semaphore(m_vsyncLock);
            }
            break;
        case 2 :
            if ( vibe_os_is_locked_mutex(m_vsyncLock) == 0 )
            {
                // m_vsyncLock is not locked
                TRC(TRC_ID_ERROR, "Error: %s has not taken m_vsyncLock previously", function_name);
            }
            break;
    }
}

void CDisplayDevice::RegistryInit()
{
  char     DeviceTag[STM_REGISTRY_MAX_TAG_SIZE];
  unsigned i=0;
  int res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Add Display type into stm_registry database. This will be
   * used as parent object for all created display devices
   * objects.
   *
   * We should not break execution if the device type was previously
   * added to the registry. This will ensure the adding of second device
   * objects in the registry using same object type
   */
  res = stm_registry_add_object(STM_REGISTRY_TYPES,
                  stm_display_types[i].tag,
                  stm_display_types[i].type);
  if (0 == res)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%p)",  stm_display_types[i].tag, stm_display_types[i].type );
  }

  /*
   * Add real Display object into stm_registry database. This will be
   * used in stm_display_open_device() as parent object instead using fake
   * object.
   */
  vibe_os_snprintf (DeviceTag, sizeof(DeviceTag), "%s%d",
                    stm_display_types[i].tag, m_ID);
  res = stm_registry_add_object(stm_display_types[i].type,
                    DeviceTag,
                    (stm_object_h)this);
  if (0 == res)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%p)", DeviceTag, this );

    /* CoreDisplay Object is the Root Object */
    for (i=1; i<N_ELEMENTS(stm_display_types); i++)
    {
      res = stm_registry_add_object((stm_object_h)this,
                        stm_display_types[i].tag,
                        stm_display_types[i].type);
      if (0 != res)
        TRC( TRC_ID_ERROR, "Cannot register '%s' type (%d)",  stm_display_types[i].tag, res );
      else
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%d)",  stm_display_types[i].tag, res );
    }
  }
  else
    TRC( TRC_ID_ERROR, "Cannot register '%s' type (%d)",  stm_display_types[i].tag, res );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CDisplayDevice::RegistryTerm()
{
  int i = N_ELEMENTS(stm_display_types)-1;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Remove object types from the stm_registry database.
   * Don't remove the display type object until we move this object
   * otherwise we will fail to clean up the registry.
   */
  do
  {
    int res = stm_registry_remove_object(stm_display_types[i].type);
    if (0 != res)
    {
      TRC( TRC_ID_ERROR, "Cannot unregister '%s' type (%d)",  stm_display_types[i].tag, res );
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "Unregistered '%s' type (%d)",  stm_display_types[i].tag, res );
    }
    i--;
  } while ( i > 0 );

  /*
   * Remove this object from the stm_registry database.
   */
  if(stm_registry_remove_object((stm_object_h)this) < 0)
    TRC( TRC_ID_ERROR, "failed to remove display object = %p from the registry",this );
  else
    TRC( TRC_ID_MAIN_INFO, "Remove display object = %p from the registry",this );

  /*
   * Now remove the display type object from the stm_registry database.
   */
  if(stm_registry_remove_object(stm_display_types[i].type) < 0)
    TRC( TRC_ID_ERROR, "failed to remove display type = %p from the registry",stm_display_types[i].type );
  else
    TRC( TRC_ID_MAIN_INFO, "Remove display type = %p from the registry",stm_display_types[i].type );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

int CDisplayDevice::FindOutputsWithCapabilities(
                               stm_display_output_capabilities_t  caps_match,
                               stm_display_output_capabilities_t  caps_mask,
                               uint32_t*                          id,
                               uint32_t                           max_ids)
{
  int             nb_output_found=0;
  uint32_t        output_id;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  for(output_id=0; output_id<m_numOutputs; output_id++ ) {

    if(!m_pOutputs[output_id])
      continue;

    if( (m_pOutputs[output_id]->GetCapabilities() & ((uint32_t)caps_mask)) == ((uint32_t)caps_match) ) {

      if(max_ids==0) {

        // Count all output having this specific capabilities
        nb_output_found++;

      } else {

        // Fill output "id" table and count number of output having this specific capabilities until max_ids reached
        id[nb_output_found]=output_id;
        nb_output_found++;
        if((uint32_t)(nb_output_found) == max_ids) break;

      }

    }

  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return nb_output_found;
}

bool CDisplayDevice::AddOutput(COutput *pOutput)
{
  char OutputTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pOutput)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add output \"%s\" ID:%u (%p) to device ID:%u (%p)",pOutput->GetName(),pOutput->GetID(),pOutput,GetID(),this );

  if(pOutput->GetID() >= m_numOutputs)
  {
    TRC( TRC_ID_ERROR, "Output ID (%u) out of valid range for device",pOutput->GetID() );
    goto error;
  }

  if(m_pOutputs[pOutput->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Output ID (%u) already registered with device",pOutput->GetID() );
    goto error;
  }

  if(!pOutput->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create output before adding to device" );
    goto error;
  }


  m_pOutputs[pOutput->GetID()] = pOutput;

  /*
   * Add the real Plane object to the registry.
   */
  vibe_os_snprintf (OutputTag, sizeof(OutputTag), "%s",
                    pOutput->GetName());

  if (stm_registry_add_object(&stm_display_output_type,
                  OutputTag,
                (stm_object_h)pOutput) != 0)
    TRC( TRC_ID_ERROR, "Cannot register '%s' object (%d)",  OutputTag, pOutput->GetID() );
  else
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)",  OutputTag, pOutput->GetID() );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pOutput;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return false;
}

int CDisplayDevice::FindPlanesWithCapabilities(
                               stm_plane_capabilities_t  caps_match,
                               stm_plane_capabilities_t  caps_mask,
                               uint32_t*                 id,
                               uint32_t                  max_ids)
{
  int             nb_plane_found=0;
  uint32_t        plane_id;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  for(plane_id=0; plane_id<m_numPlanes; plane_id++ ) {

    if(!m_pPlanes[plane_id])
      continue;

    if( (m_pPlanes[plane_id]->GetCapabilities() & caps_mask) == caps_match ) {

      if(max_ids==0) {

        // Count all plane having this specific capabilities
        nb_plane_found++;

      } else {

        // Fill plane "id" table and count number of plane having this specific capabilities until max_ids reached
        id[nb_plane_found]=plane_id;
        nb_plane_found++;
        if((uint32_t)(nb_plane_found) == max_ids) break;

      }

    }

  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return nb_plane_found;
}

bool CDisplayDevice::AddPlane(CDisplayPlane *pPlane)
{
  char PlaneTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pPlane)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add plane \"%s\" ID:%u (%p) to device ID:%u (%p)",pPlane->GetName(),pPlane->GetID(),pPlane,GetID(),this );

  if(pPlane->GetID() >= m_numPlanes)
  {
    TRC( TRC_ID_ERROR, "Plane ID (%u) out of valid range for device",pPlane->GetID() );
    goto error;
  }

  if(m_pPlanes[pPlane->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Plane ID (%u) already registered with device",pPlane->GetID() );
    goto error;
  }

  if(!pPlane->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create plane before adding to device" );
    goto error;
  }

  m_pPlanes[pPlane->GetID()] = pPlane;

  /*
   * Add the real Plane object to the registry.
   */
  vibe_os_snprintf (PlaneTag, sizeof(PlaneTag), "%s",
                    pPlane->GetName());

  if (stm_registry_add_object(&stm_display_plane_type, PlaneTag, (stm_object_h)pPlane) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' object (%d)", PlaneTag, pPlane->GetID() );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", PlaneTag, pPlane->GetID() );
    /*
     * Register the attribute to collect statistics on the plane
     */
    pPlane->RegisterStatistics();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pPlane;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return false;
}

bool CDisplayDevice::AddSource(CDisplaySource *pSource)
{
  char SourceTag[STM_REGISTRY_MAX_TAG_SIZE];
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pSource)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add Source \"%s\" ID:%u (%p) to device ID:%u (%p)",pSource->GetName(),pSource->GetID(),pSource,GetID(),this );

  if(pSource->GetID() >= m_numSources)
  {
    TRC( TRC_ID_ERROR, "Source ID (%u) out of valid range for device",pSource->GetID() );
    goto error;
  }

  if(m_pSources[pSource->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Source ID (%u) already registered with device",pSource->GetID() );
    goto error;
  }

  if(!pSource->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create Source before adding to device" );
    goto error;
  }

  m_pSources[pSource->GetID()] = pSource;

  /*
   * Add the real Source object to the registry.
   */
  vibe_os_snprintf (SourceTag, sizeof(SourceTag), "%s",
                       pSource->GetName());

  if (stm_registry_add_object(&stm_display_source_type, SourceTag, (stm_object_h)pSource) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' object (%d)", SourceTag, pSource->GetID() );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)", SourceTag, pSource->GetID() );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pSource;
  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return false;
}

void CDisplayDevice::RemoveSources(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  for(unsigned i=0;i<m_numSources;i++)
  {
    if(m_pSources[i])
    {
      /*
       * Remember that a SourceInterface was registred as a child of this object
       * into the stm_registry database. In order to successfully clean up
       * the stm_registry database before removing coredisplay module we should
       * remove the SourceInterface object from the database before removing
       * this object. The removal of SourceInterface object will be done in the
       * CDisplaySource destructor.
       */
      delete m_pSources[i];

      /*
       * Now we can remove object from the registry before exiting.
       */
      if(stm_registry_remove_object(m_pSources[i]) < 0)
        TRC( TRC_ID_ERROR, "failed to remove source object = %p from the registry",m_pSources[i] );

      m_pSources[i] = 0L;
    }
  }
  TRC( TRC_ID_MAIN_INFO, "CDisplayDevice::RemoveSources() deleted all sources" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CDisplayDevice::RemovePlanes(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );
  for(unsigned i=0;i<m_numPlanes;i++)
  {
    /*
     * Remove object from the registry before exiting.
     */
    if(!m_pPlanes[i])
      continue;
    if(stm_registry_remove_object(m_pPlanes[i]) < 0)
      TRC( TRC_ID_ERROR, "failed to remove plane object = %p from the registry",m_pPlanes[i] );
    delete m_pPlanes[i];
    m_pPlanes[i] = 0L;
  }
  TRC( TRC_ID_MAIN_INFO, "CDisplayDevice::RemovePlanes() deleted all planes" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CDisplayDevice::RemoveOutputs(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * First, call all the output CleanUp routines while all of the output
   * objects still exist.
   */
  for (uint32_t i = 0; i < m_numOutputs; i++)
  {
    if(m_pOutputs[i]) m_pOutputs[i]->CleanUp();
  }

  /*
   * Now delete everything.
   */
  for (uint32_t i = 0; i < m_numOutputs; i++)
  {
    /*
     * Remove object from the registry before exiting.
     */
    if(!m_pOutputs[i])
      continue;
    if(stm_registry_remove_object(m_pOutputs[i]) < 0)
      TRC( TRC_ID_ERROR, "failed to remove output object = %p from the registry",m_pOutputs[i] );
    delete m_pOutputs[i];
    m_pOutputs[i] = 0L;
  }
  TRC( TRC_ID_MAIN_INFO, "CDisplayDevice::RemoveOutputs() deleted all Outputs" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


TuningResults CDisplayDevice::SetTuning(uint16_t service,
                                        void *inputList,
                                        uint32_t inputListSize,
                                        void *outputList,
                                        uint32_t outputListSize)
{
    TuningResults         res = TUNING_SERVICE_NOT_SUPPORTED;
    SetTuningInputData_t* input = 0;
    CDisplayPlane*        pPlaneHandle  = 0;
    COutput             * pOutputHandle = NULL;
    CDisplaySource*       pSourceHandle = 0;

    input  = (SetTuningInputData_t*)inputList;

    if(!input)
    {
        TRC(TRC_ID_ERROR, "Error! Invalid input list!");
        return res;
    }

    if(m_bIsSuspended)
    {
        TRC(TRC_ID_MAIN_INFO, "Service not available (driver suspended)");
        return TUNING_SERVICE_NOT_AVAILABLE;
    }

    switch(service)
    {
        case CRC_COLLECT:
        case CRC_CAPABILITY:
        {
            /* to get CRC on video planes */
            pPlaneHandle = GetPlane(input->PlaneId);
            TRC( TRC_ID_UNCLASSIFIED, "pPlaneHandle=%p", pPlaneHandle );
            if(pPlaneHandle)
            {
                res = pPlaneHandle->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            }
            else
            {
                TRC(TRC_ID_ERROR, "Invalid pPlaneHandle=0x%p", pPlaneHandle);
                res = TUNING_INVALID_PARAMETER;
            }
            break;
        }
        case MDTP_CRC_COLLECT:
        case MDTP_CRC_SET_CONTROL:
        case MDTP_CRC_STOP:
        case MDTP_CRC_CAPABILITY:
        {
            /* to get CRC on sources */
            pSourceHandle = GetSource(input->SourceId);
            TRC( TRC_ID_UNCLASSIFIED, "pSourceHandle=%p", pSourceHandle );
            if(pSourceHandle)
            {
                res = pSourceHandle->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            }
            else
            {
                TRC(TRC_ID_ERROR, "Invalid pSourceHandle=0x%p", pSourceHandle);
                res = TUNING_INVALID_PARAMETER;
            }
            break;
        }
        case MISR_COLLECT:
        case MISR_SET_CONTROL:
        case MISR_CAPABILITY:
        case MISR_STOP:
        {
            pOutputHandle = GetOutput(input->OutputId);
            TRC( TRC_ID_UNCLASSIFIED, "pOutputHandle=%p", pOutputHandle );
            if(pOutputHandle)
            {
                res = pOutputHandle->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            }
            else
            {
                TRC(TRC_ID_ERROR, "Invalid pOutputHandle=0x%p", pOutputHandle);
                res = TUNING_INVALID_PARAMETER;
            }
            break;
        }
        case RESET_STATISTICS:
        {
            pPlaneHandle = GetPlane(input->PlaneId);
            if(pPlaneHandle)
            {
                res = pPlaneHandle->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            }

            pSourceHandle = GetSource(input->SourceId);
            if(pSourceHandle)
            {
                res = pSourceHandle->SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            }
            if(res != TUNING_OK)
            {
                TRC(TRC_ID_ERROR, "Error! RESET_STATISTICS failed (%d)", res);
            }
            break;
        }
        default:
            TRC(TRC_ID_ERROR, "Error! Unkown service! (%d)", service);
            break;
    }

    return res;
}


void CDisplayDevice::UpdateSource(uint32_t timingID)
{
  /*
   * Don't allow any coming updates while suspending the device or if a
   * suspend is already in progress.
   */
  if(m_bIsSuspended)
    return;

  for(unsigned i=0;i<m_numSources;i++)
  {
    if(!m_pSources[i])
      continue;

    if(m_pSources[i]->GetTimingID() == timingID)
    {
        m_pSources[i]->SourceVSyncUpdateHW();
    }
  }
}


void CDisplayDevice::OutputVSyncIrqUpdateHw(uint32_t timingID)
{
  /*
   * Single implementation for devices with single or multiple VTGs; all planes
   * that are connected to outputs with the same timing ID are updated,
   * regardless of which actual output they are being displayed on.
   *
   */
  const stm_display_mode_t *pCurrentMode = 0;
  stm_time64_t           vsyncTime = 0ULL;
  bool                   isTopFieldOnDisplay = false;
  bool                   isDisplayInterlaced = false;

  /*
   * Don't allow any coming updates while suspending the device or if a
   * suspend is already in progress.
   */
  if(m_bIsSuspended)
    return;

  TRCBL(TRC_ID_VSYNC);

  /*
   * Go through all outputs and update them. There may be circumstances where
   * more than one output actually reports the same timing ID.
   */
  for(unsigned i=0;i<m_numOutputs;i++)
  {
    if(!m_pOutputs[i])
      continue;

    if(m_pOutputs[i]->GetTimingID() == timingID)
    {
      if(!pCurrentMode)
      {
        /*
         * It doesn't matter which output we find first, use it to find out
         * about the display mode and timing generator state.
         */
        pCurrentMode = m_pOutputs[i]->GetCurrentDisplayMode();

        /*
         * If there is no valid display mode, we must have got a rogue VTG
         * interrupt from the hardware, ignore it and do not do anything else.
         */
        if(!pCurrentMode)
          return;

        isDisplayInterlaced = (pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN);
        isTopFieldOnDisplay = (m_pOutputs[i]->GetCurrentVTGEvent() & STM_TIMING_EVENT_TOP_FIELD) != 0;
        vsyncTime           = m_pOutputs[i]->GetCurrentVTGEventTime();
      }

      m_pOutputs[i]->UpdateHW();
    }
  }
}

  /*
   * Same as OutputVSyncIrqUpdateHw() but in thread context.
   */
void CDisplayDevice::OutputVSyncThreadedIrqUpdateHw(uint32_t timingID)
{

  const stm_display_mode_t *pCurrentMode = 0;
  stm_time64_t           vsyncTime = 0ULL;
  bool                   isTopFieldOnDisplay = false;
  bool                   isDisplayInterlaced = false;

  if(m_bIsSuspended)
    return;

  /*
   * Go through all outputs just to get the vsync time.
   * The outputs are updated in IRQ.
   */
  for(unsigned i=0;i<m_numOutputs;i++)
  {
    if(!m_pOutputs[i])
      continue;

    if(m_pOutputs[i]->GetTimingID() == timingID)
    {
      if(!pCurrentMode)
      {
        /*
         * It doesn't matter which output we find first, use it to find out
         * about the display mode and timing generator state.
         */
        pCurrentMode = m_pOutputs[i]->GetCurrentDisplayMode();

        /*
         * If there is no valid display mode, we must have got a rogue VTG
         * interrupt from the hardware, ignore it and do not do anything else.
         */
        if(!pCurrentMode)
          return;

        isDisplayInterlaced = (pCurrentMode->mode_params.scan_type == STM_INTERLACED_SCAN);
        isTopFieldOnDisplay = (m_pOutputs[i]->GetCurrentVTGEvent() & STM_TIMING_EVENT_TOP_FIELD) != 0;
        vsyncTime           = m_pOutputs[i]->GetCurrentVTGEventTime();
      }
    }
  }

  if ( VSyncLock() != 0 )
    return;

  for(unsigned i=0;i<m_numPlanes;i++)
  {
    if(m_pPlanes[i] && (m_pPlanes[i]->GetTimingID() == timingID))
    {
      m_pPlanes[i]->OutputVSyncThreadedIrqUpdateHW(isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);
    }
  }

  for(unsigned i=0;i<m_numSources;i++)
  {
    if(m_pSources[i] && (m_pSources[i]->GetTimingID() == timingID))
    {
      m_pSources[i]->OutputVSyncThreadedIrqUpdateHW(isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime);
    }
  }

  VSyncUnlock();

}


int CDisplayDevice::Freeze(void)
{
  int             res=0;
  uint32_t        id;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsFrozen)
    return res;

  /*
   * Set the device state to frozen to avoid any coming update while
   * we are switch off the display device.
   *
   * Sources, Planes and Outputs will continue processing previous
   * update as they are not yet frozen at this point.
   */
  m_bIsFrozen = true;
  m_bIsSuspended = true;

  for(id=0; id<m_numSources; id++ )
  {
    if(m_pSources[id])
      m_pSources[id]->Freeze();
  }

  for(id=0; id<m_numPlanes; id++ )
  {
    if(m_pPlanes[id])
      m_pPlanes[id]->Freeze();
  }

  for(id=0; id<m_numOutputs; id++ )
  {
    if(m_pOutputs[id])
      m_pOutputs[id]->Suspend();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


int CDisplayDevice::Suspend(void)
{
  int             res=0;
  uint32_t        id;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return res;

  /*
   * Set the device state to suspended to avoid any coming update while
   * we are switch off the display device.
   *
   * Sources, Planes and Outputs will continue processing previous
   * update as they are not yet suspended at this point.
   */
  m_bIsSuspended = true;

  for(id=0; id<m_numSources; id++ )
  {
    if(m_pSources[id])
      m_pSources[id]->Suspend();
  }

  for(id=0; id<m_numPlanes; id++ )
  {
    if(m_pPlanes[id])
      m_pPlanes[id]->Suspend();
  }

  for(id=0; id<m_numOutputs; id++ )
  {
    if(m_pOutputs[id])
      m_pOutputs[id]->Suspend();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


int CDisplayDevice::Resume(void)
{
  int             res=0;
  uint32_t        id;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return res;

  for(id=0; id<m_numOutputs; id++)
  {
    if(m_pOutputs[id])
      m_pOutputs[id]->Resume();
  }

  for(id=0; id<m_numPlanes; id++)
  {
    if(m_pPlanes[id])
      m_pPlanes[id]->Resume();
  }

  for(id=0; id<m_numSources; id++)
  {
    if(m_pSources[id])
      m_pSources[id]->Resume();
  }

  m_bIsSuspended = false;
  m_bIsFrozen = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


void CDisplayDevice::PowerDownVideoDacs(void)
{
  uint32_t        id;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return;

  for(id=0; id<m_numOutputs; id++)
  {
    if(m_pOutputs[id])
      m_pOutputs[id]->PowerDownDACs();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


//////////////////////////////////////////////////////////////////////////////
// C Device interface

extern "C" {

struct private_display_device_s
{
  struct stm_display_device_s public_dev;
  int use_count;
};

static unsigned maxDevices = 2;
static int numDevices = 0;
static struct private_display_device_s theDevices[2] = {{{0}},{{0}}};

/*********************************************************************/
/** C Private Device interfaces                                      */
/*********************************************************************/
int stm_display_device_register_pm_runtime_hooks(stm_display_device_h dev, int (*get)(const uint32_t id), int (*put)(const uint32_t id))
{
  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(!CHECK_ADDRESS(get) || !CHECK_ADDRESS(put))
    return -EFAULT;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  dev->pm.runtime_get = get;
  dev->pm.runtime_put = put;

  STKPI_UNLOCK(dev);

  return 0;
}

int stm_display_device_get_power_state(stm_display_device_h dev, uint32_t *pm_state)
{
  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  /* pm_state pointer should be valid */
  if(!CHECK_ADDRESS(pm_state))
    return -EFAULT;

  if(!dev->pm.spinLock)
  {
    TRC(TRC_ID_ERROR, "Device with invalid spinLock!");
    return -EINVAL;
  }

  /* This function can be called in ISR context so we cannot take "dev->mutex_lock".
     The protection of "dev->pm.state" is done thanks to a dedicated spinLock ("dev->pm.spinLock")
  */
  vibe_os_lock_resource(dev->pm.spinLock);
  *pm_state = dev->pm.state;
  vibe_os_unlock_resource(dev->pm.spinLock);

  TRC(TRC_ID_API_DEVICE, "dev : %u, pm_state : %u", dev->handle->GetID(), *pm_state);

  return 0;
}

bool stm_display_device_is_suspended(stm_display_device_h dev)
{
  uint32_t pm_state = DEVICE_ACTIVE;
  bool is_suspended = false;

  if(stm_display_device_get_power_state(dev, &pm_state) < 0)
  {
    TRC(TRC_ID_ERROR, "Failed to get the power_state!");
    return true;
  }

  if((pm_state == DEVICE_SUSPENDED) || (pm_state == DEVICE_FROZEN))
  {
    is_suspended = true;
    // In the current implementation, IS_DEVICE_SUSPENDED() is only called by STKPI function
    // to check that the device is not suspended. If it is suspended, it means that STKPI functions are
    // called when the device is suspended.
    TRC(TRC_ID_MAIN_INFO, "DE_ERROR: STKPI fct called while device suspended!");
  }

  TRC(TRC_ID_API_DEVICE, "dev : %u, is_suspended : %d", dev->handle->GetID(), is_suspended);

  return is_suspended;
}


int stm_display_device_power_down_video_dacs(stm_display_device_h dev)
{
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  pDev->PowerDownVideoDacs();

  STKPI_UNLOCK(dev);

  return 0;
}

int stm_display_device_stkpi_lock(stm_display_device_h dev, const char *fct_name)
{
  CDisplayDevice* pDev = NULL;
  int             res;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  pDev = dev->handle;

  res = vibe_os_lock_mutex(dev->mutex_lock);

  /* lock_time should be collected AFTER the lock */
  dev->lock_time = vibe_os_get_system_time();

  return res;
}

void stm_display_device_stkpi_unlock(stm_display_device_h dev, const char *fct_name)
{
  CDisplayDevice* pDev = NULL;
  vibe_time64_t   lock_duration_in_us;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return;

  pDev = dev->handle;

  /* unlock_time should be collected BEFORE the unlock */
  dev->unlock_time = vibe_os_get_system_time();

  lock_duration_in_us = dev->unlock_time - dev->lock_time;
  TRC(TRC_ID_STKPI_LOCK_DURATION, "%s STKPI lock duration : %llu us", fct_name, lock_duration_in_us);

  if(lock_duration_in_us > MAX_STKPI_DURATION_IN_US)
  {
    TRC(TRC_ID_MAIN_INFO, "WARNING! %s locking the STKPI for %llu us!", fct_name, lock_duration_in_us);
  }

  vibe_os_unlock_mutex(dev->mutex_lock);
}


/*********************************************************************/
/** C Public Device interfaces                                       */
/*********************************************************************/

int stm_display_device_find_outputs_with_capabilities(stm_display_device_h dev,
                                                     stm_display_output_capabilities_t  caps_match,
                                                     stm_display_output_capabilities_t  caps_mask,
                                                     uint32_t*                          id,
                                                     uint32_t                           max_ids)
{
  CDisplayDevice* pDev = NULL;
  int nb_output_found;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  // We allow this function to be called even when the driver is suspended (so IS_DEVICE_SUSPENDED() is not called)

  pDev = dev->handle;

  if(max_ids > 0) {

    //id pointer should be valid
    if(!CHECK_ADDRESS(id))
      return -EFAULT;
  }

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  nb_output_found=pDev->FindOutputsWithCapabilities(caps_match, caps_mask, id, max_ids);

  STKPI_UNLOCK(dev);

  return (nb_output_found);
}

int stm_display_device_open_output(stm_display_device_h dev, uint32_t outputID, stm_display_output_h *output)
{
  CDisplayDevice* pDev = NULL;
  struct stm_display_output_s *public_output = 0;
  COutput *real_output;
  char OutputTag[STM_REGISTRY_MAX_TAG_SIZE];
  int error_code = 0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  if(!CHECK_ADDRESS(output))
  {
    error_code = -EFAULT;
    goto exit;
  }

  TRC(TRC_ID_API_DEVICE, "dev : %u, outputID : %u", pDev->GetID(), outputID);
  real_output = pDev->GetOutput(outputID);
  if(!real_output)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Failed to get a valid output object from the device" );
    error_code = -ENODEV;
    goto exit;
  }

  public_output = new struct stm_display_output_s;
  if(!public_output)
  {
    TRC( TRC_ID_ERROR, "Failed to allocate output handle structure" );
    error_code = -ENOMEM;
    goto exit;
  }

  public_output->handle     = real_output;
  public_output->parent_dev = dev;

  stm_coredisplay_magic_set(public_output, VALID_OUTPUT_HANDLE);

  /*
   * Add the Output instance to the registry.
   */
  vibe_os_snprintf (OutputTag, sizeof(OutputTag), "%s-%p",
                    real_output->GetName(), public_output);

  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                  (stm_object_h)real_output,
                                  OutputTag,
                                  (stm_object_h)public_output) != 0)
    TRC( TRC_ID_ERROR, "Cannot register output instance (%p)", public_output );

  TRC(TRC_ID_API_DEVICE, "output : %s", public_output->handle->GetName());
exit:
  STKPI_UNLOCK(dev);

  *output=public_output;
  return error_code;
}


int stm_display_device_find_planes_with_capabilities(stm_display_device_h dev,
                                                     stm_plane_capabilities_t caps_match,
                                                     stm_plane_capabilities_t caps_mask,
                                                     uint32_t* id,
                                                     uint32_t  max_ids)
{
  CDisplayDevice* pDev = NULL;
  int nb_plane_found;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  // We allow this function to be called even when the driver is suspended (so IS_DEVICE_SUSPENDED() is not called)

  pDev = dev->handle;

  if(max_ids > 0) {

    //id pointer should be valid
    if(!CHECK_ADDRESS(id))
      return -EFAULT;
  }

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  nb_plane_found=pDev->FindPlanesWithCapabilities(caps_match, caps_mask, id, max_ids);

  STKPI_UNLOCK(dev);

  return (nb_plane_found);
}

int stm_display_device_open_plane(stm_display_device_h dev, uint32_t planeID, stm_display_plane_h *plane)
{
  CDisplayDevice* pDev = NULL;
  struct stm_display_plane_s *public_plane = 0;
  CDisplayPlane *real_plane;
  char PlaneTag[STM_REGISTRY_MAX_TAG_SIZE];
  int error_code = 0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  if(!CHECK_ADDRESS(plane))
  {
    error_code = -EFAULT;
    goto exit;
  }

  TRC(TRC_ID_API_DEVICE, "dev : %u, planeID : %u", pDev->GetID(), planeID);
  real_plane = pDev->GetPlane(planeID);
  if(!real_plane)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Failed to get a valid plane object from the device" );
    error_code = -ENODEV;
    goto exit;
  }

  public_plane = new struct stm_display_plane_s;
  if(!public_plane)
  {
    TRC( TRC_ID_ERROR, "Failed to allocate plane handle structure" );
    error_code = -ENOMEM;
    goto exit;
  }

  public_plane->handle     = real_plane;
  public_plane->parent_dev = dev;

  stm_coredisplay_magic_set(public_plane, VALID_PLANE_HANDLE);

  /*
   * Add the Plane instance to the registry.
   */
  vibe_os_snprintf(PlaneTag, sizeof(PlaneTag), "%s-%p",
                   real_plane->GetName(), public_plane);

  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                  (stm_object_h)real_plane,
                                  PlaneTag,
                                  (stm_object_h)public_plane) != 0)
    TRC( TRC_ID_ERROR, "Cannot register plane instance (%p)", public_plane );

  TRC(TRC_ID_API_DEVICE, "plane : %s", public_plane->handle->GetName());
exit:
  STKPI_UNLOCK(dev);

  *plane = public_plane;

  return error_code;
}

int stm_display_device_open_source(stm_display_device_h dev, uint32_t sourceID, stm_display_source_h *source)
{
  CDisplayDevice       * pDev = NULL;
  stm_display_source_h   public_source = 0;
  CDisplaySource       * real_source;
  char                   SourceTag[STM_REGISTRY_MAX_TAG_SIZE];
  int error_code = 0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  if(!CHECK_ADDRESS(source))
  {
    error_code = -EFAULT;
    goto exit;
  }

  TRC(TRC_ID_API_DEVICE, "dev : %u, sourceID : %u", pDev->GetID(), sourceID);
  real_source = pDev->GetSource(sourceID);
  if(!real_source)
  {
    TRC( TRC_ID_ERROR, "Failed to get a valid source object from the device" );
    error_code = -ENODEV;
    goto exit;
  }

  public_source = new struct stm_display_source_s;
  if(!public_source)
  {
    TRC( TRC_ID_ERROR, "Failed to allocate source handle structure" );
    error_code = -ENOMEM;
    goto exit;
  }

  public_source->handle     = real_source;
  public_source->parent_dev = dev;

  stm_coredisplay_magic_set(public_source, VALID_SOURCE_HANDLE);

  /*
   * Add the Source instance to the registry.
   */
  vibe_os_snprintf (SourceTag, sizeof(SourceTag), "%s-%p",
                    real_source->GetName(), public_source);

  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                  (stm_object_h)real_source,
                                  SourceTag,
                                  (stm_object_h)public_source) != 0)
    TRC( TRC_ID_ERROR, "Cannot register source instance (%p)", public_source );

  TRC(TRC_ID_API_DEVICE, "source : %s", public_source->handle->GetName());
exit:
  STKPI_UNLOCK(dev);

  *source=public_source;
  return error_code;
}


int stm_display_device_get_number_of_tuning_services(stm_display_device_h dev, uint16_t *nservices)
{
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  *nservices = pDev->GetNumTuningServices();

  STKPI_UNLOCK(dev);

  return 0;
}


int stm_display_device_get_tuning_caps(stm_display_device_h dev, stm_device_tuning_caps_t *return_caps)
{
  CDisplayDevice* pDev = NULL;
  const stm_device_tuning_caps_t *dev_caps;
  int services;
  int ret = -1;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());

  if((services = pDev->GetNumTuningServices()) == 0)
    goto exit;

  if((dev_caps = pDev->GetTuningCaps()) == 0)
    goto exit;

  for(int i=0;i<services;i++)
    return_caps[i] = dev_caps[i];

  ret = 0;
exit:
  STKPI_UNLOCK(dev);

  return ret;
}


int stm_display_device_set_tuning(stm_display_device_h dev,
                                  uint16_t             service,
                                  void                *input_list,
                                  uint32_t             input_list_size,
                                  void                *output_list,
                                  uint32_t             output_list_size)
{
  TuningResults res = TUNING_INVALID_PARAMETER;
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  if(IS_DEVICE_SUSPENDED(dev))
  {
    //For the moment we only print an error (this is done by IS_DEVICE_SUSPENDED)
    //return -EAGAIN;
  }

  pDev = dev->handle;

  if(!CHECK_ADDRESS(input_list) && (input_list_size != 0))
    return -EFAULT;

  if(!CHECK_ADDRESS(output_list) && (output_list_size != 0))
    return -EFAULT;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  res = pDev->SetTuning(service, input_list, input_list_size, output_list, output_list_size);

  switch(res)
  {
    case TUNING_INVALID_PARAMETER:
      return -ERANGE;
    case TUNING_SERVICE_NOT_SUPPORTED:
      return -ENOTSUP;
    case TUNING_SERVICE_NOT_AVAILABLE:
      return -EAGAIN;
    case TUNING_EINTR:
      return -EINTR;
    case TUNING_NO_DATA_AVAILABLE:
      return -ENODATA;
    case TUNING_OK:
      break;
  }

  return 0;
}


void stm_display_device_update_vsync_irq(stm_display_device_h dev, uint32_t timing_id)
{
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return;

  // We allow this function to be called even when the driver is suspended (so IS_DEVICE_SUSPENDED() is not called)

  pDev = dev->handle;
  TRC(TRC_ID_API_DEVICE, "dev : %u, timing_id : %u", pDev->GetID(), timing_id);

  pDev->OutputVSyncIrqUpdateHw(timing_id);
}

void stm_display_device_update_vsync_threaded_irq(stm_display_device_h dev, uint32_t timing_id)
{
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return;

  // We allow this function to be called even when the driver is suspended (so IS_DEVICE_SUSPENDED() is not called)

  pDev = dev->handle;
  TRC(TRC_ID_API_DEVICE, "dev : %u, timing_id : %u", pDev->GetID(), timing_id);

  pDev->OutputVSyncThreadedIrqUpdateHw(timing_id);
}

void stm_display_device_source_update(stm_display_device_h dev, uint32_t timing_id)
{
  CDisplayDevice* pDev = NULL;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return;

  pDev = dev->handle;
  TRC(TRC_ID_API_DEVICE, "dev : %u, timing_id : %u", pDev->GetID(), timing_id);

  pDev->UpdateSource(timing_id);
}

void stm_display_device_destroy(stm_display_device_h dev)
{
  struct private_display_device_s *private_dev = (struct private_display_device_s*)dev;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!private_dev)
  {
    goto exit;
  }

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
  {
    goto exit;
  }

  if(STKPI_LOCK(dev) != 0)
  {
    goto exit;
  }

  TRC(TRC_ID_API_DEVICE, "dev : %u", dev->handle->GetID());

  TRC( TRC_ID_MAIN_INFO, "Destroying device %d", dev->handle->GetID());

  while(private_dev->use_count > 0)
  {
    /* This display device has some opened handle, not yet closed. */
    /* So force the closing of them. */
    TRC(TRC_ID_ERROR, "dev : %u force closing device handle before destroying it", dev->handle->GetID());

    STKPI_UNLOCK(dev);

    stm_display_device_close(dev);

    if(STKPI_LOCK(dev) != 0)
    {
      goto exit;
    }
  }

  stm_coredisplay_magic_clear(dev);

  /*
   * Remove object instance from the registry before exiting.
   */
  if(stm_registry_remove_object(dev) < 0)
  {
    TRC( TRC_ID_ERROR, "failed to remove display object = %p from the registry",dev );
  }

  delete dev->handle;
  dev->handle = NULL;

  numDevices--;

  vibe_os_delete_mutex(dev->mutex_lock);
  dev->mutex_lock = 0;

  dev->pm.runtime_get = 0;
  dev->pm.runtime_put = 0;
  vibe_os_delete_resource_lock(dev->pm.spinLock);
  dev->pm.spinLock = 0;

exit:
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void stm_display_device_close(stm_display_device_h dev)
{
  struct private_display_device_s *private_dev = (struct private_display_device_s*)dev;
  int        (*runtime_put)(const uint32_t id) = NULL;
  uint32_t    pid                              = 0;

  if(!private_dev)
    return;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return;

  TRCIN(TRC_ID_MAIN_INFO, "");

  if(STKPI_LOCK(dev) != 0)
  {
    goto exit;
  }

  if(private_dev->use_count <= 0)
  {
    /* Too many close of device */
    TRC(TRC_ID_ERROR, "Too many calls for closing device, not enough opening done.");

    STKPI_UNLOCK(dev);
    goto exit;
  }

  private_dev->use_count--;

  TRC(TRC_ID_API_DEVICE, "dev : %u, use_count : %d", dev->handle->GetID(), private_dev->use_count);

  runtime_put = dev->pm.runtime_put;
  pid         = dev->pm.pid;

  STKPI_UNLOCK(dev);

  /* DPM : Put device */
  if(runtime_put)
  {
    dev->pm.runtime_put(pid);
  }

exit:
  TRCOUT(TRC_ID_MAIN_INFO, "");
}


int stm_display_device_create(uint32_t id, stm_display_device_h *device)
{
  char            DeviceTag[STM_REGISTRY_MAX_TAG_SIZE];
  CDisplayDevice *pDev = NULL;
  int             res;

  if(id>=maxDevices)
    return -ENODEV;

  if(!CHECK_ADDRESS(device))
    return -EFAULT;

  TRCIN(TRC_ID_MAIN_INFO, "");

  theDevices[id].public_dev.mutex_lock = vibe_os_create_mutex();
  if(theDevices[id].public_dev.mutex_lock == 0)
  {
    TRC( TRC_ID_ERROR, "create device's mutex_lock failed" );
    goto error_exit;
  }

  /* Prevent STKPI calls while we create the device */
  res = vibe_os_lock_mutex(theDevices[id].public_dev.mutex_lock);
  if(res != 0)
  {
    TRC( TRC_ID_ERROR, "failed to lock the STKPI mutex for dev %d", id );
    goto error_exit;
  }

  if((pDev = AnonymousCreateDevice(id)) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "create device object %d failed", id );
    goto error_exit;
  }

  if(!pDev->Create())
  {
    TRC( TRC_ID_ERROR, "failed to complete device %d creation", id );
    goto error_exit;
  }

  theDevices[id].public_dev.handle          = pDev;
  theDevices[id].public_dev.pm.pid          = id;

  theDevices[id].public_dev.pm.spinLock = vibe_os_create_resource_lock();
  if(theDevices[id].public_dev.pm.spinLock == 0)
  {
    TRC( TRC_ID_ERROR, "Failed to create spinLock" );
    goto error_exit;
  }

  vibe_os_lock_resource(theDevices[id].public_dev.pm.spinLock);
  theDevices[id].public_dev.pm.state        = DEVICE_ACTIVE;
  vibe_os_unlock_resource(theDevices[id].public_dev.pm.spinLock);

  theDevices[id].public_dev.pm.runtime_get  = 0;
  theDevices[id].public_dev.pm.runtime_put  = 0;

  stm_coredisplay_magic_set(&theDevices[id].public_dev, VALID_DEVICE_HANDLE);

  numDevices++;

  theDevices[id].use_count = 0;

  *device=(&theDevices[id].public_dev);

  TRC(TRC_ID_API_DEVICE, "dev : %u, id : %u", pDev->GetID(), id);

  /*
   * Add the Device instance to the registry.
   * Create one single instance per device as we are providing the
   * same public device handle.
   */
  vibe_os_snprintf (DeviceTag, sizeof(DeviceTag), "display_device%d-%p",
                      id, (*device));
  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                    (stm_object_h)(theDevices[id].public_dev.handle),
                                  DeviceTag,
                                    (stm_object_h)(*device)) != 0)
  {
    TRC( TRC_ID_ERROR, "Cannot register %s device instance (%p)", DeviceTag, (*device) );
  }

  vibe_os_unlock_mutex(theDevices[id].public_dev.mutex_lock);

  TRCOUT(TRC_ID_MAIN_INFO, "");

  return 0;

error_exit:
  delete pDev;

  if(theDevices[id].public_dev.mutex_lock)
  {
    vibe_os_delete_mutex(theDevices[id].public_dev.mutex_lock);
    theDevices[id].public_dev.mutex_lock = 0;
  }

  if(theDevices[id].public_dev.pm.spinLock)
  {
    vibe_os_delete_resource_lock(theDevices[id].public_dev.pm.spinLock);
    theDevices[id].public_dev.pm.spinLock = 0;
  }

  TRCOUT(TRC_ID_MAIN_INFO, "");

  return -ENODEV;
}


int stm_display_open_device(uint32_t id, stm_display_device_h *device)
{
  int      (*runtime_get)(const uint32_t id) = NULL;
  uint32_t  pid                              = 0;

  if(id>=maxDevices)
    return -ENODEV;

  if(!CHECK_ADDRESS(device))
    return -EFAULT;

  if(theDevices[id].public_dev.mutex_lock == 0)
  {
      return -ENODEV;
  }

  TRCIN(TRC_ID_MAIN_INFO, "");

  if(vibe_os_lock_mutex(theDevices[id].public_dev.mutex_lock) != 0)
  {
    TRCOUT(TRC_ID_MAIN_INFO, "");
    return -EINTR;
  }

  theDevices[id].use_count++;

  *device=(&theDevices[id].public_dev);

  TRC(TRC_ID_API_DEVICE, "dev : %u, id : %u, use_count : %d",
      (*device)->handle->GetID(), id, theDevices[id].use_count);

  runtime_get = theDevices[id].public_dev.pm.runtime_get;
  pid         = theDevices[id].public_dev.pm.pid;

  vibe_os_unlock_mutex(theDevices[id].public_dev.mutex_lock);

  /* DPM : Get device */
  if(runtime_get)
  {
    runtime_get(pid);
  }

  TRCOUT(TRC_ID_MAIN_INFO, "");

  return 0;
}


int stm_display_device_freeze(stm_display_device_h dev)
{
  CDisplayDevice* pDev = NULL;
  int res=0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  TRC(TRC_ID_MAIN_INFO, "");

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  res=pDev->Freeze();
  if(res == 0)
  {
    vibe_os_lock_resource(dev->pm.spinLock);
    dev->pm.state = DEVICE_FROZEN;
    vibe_os_unlock_resource(dev->pm.spinLock);
  }

  STKPI_UNLOCK(dev);

  return res;
}


int stm_display_device_suspend(stm_display_device_h dev)
{
  CDisplayDevice* pDev = NULL;
  int res=0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  TRC(TRC_ID_MAIN_INFO, "");

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  res=pDev->Suspend();
  if(res == 0)
  {
    vibe_os_lock_resource(dev->pm.spinLock);
    dev->pm.state = DEVICE_SUSPENDED;
    vibe_os_unlock_resource(dev->pm.spinLock);
  }

  STKPI_UNLOCK(dev);

  return res;
}

int stm_display_device_resume(stm_display_device_h dev)
{
  CDisplayDevice* pDev = NULL;
  int res=0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  TRC(TRC_ID_MAIN_INFO, "");

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());
  res=pDev->Resume();
  if(res == 0)
  {
    vibe_os_lock_resource(dev->pm.spinLock);
    dev->pm.state = DEVICE_ACTIVE;
    vibe_os_unlock_resource(dev->pm.spinLock);
  }

  STKPI_UNLOCK(dev);

  return res;
}

int stm_display_device_shutting_down(stm_display_device_h dev)
{
  CDisplayDevice* pDev = NULL;
  int res=0;

  if(!stm_coredisplay_is_handle_valid(dev, VALID_DEVICE_HANDLE))
    return -EINVAL;

  TRC(TRC_ID_MAIN_INFO, "");

  pDev = dev->handle;

  if(STKPI_LOCK(dev) != 0)
    return -EINTR;

  TRC(TRC_ID_API_DEVICE, "dev : %u", pDev->GetID());

  vibe_os_lock_resource(dev->pm.spinLock);
  dev->pm.state = DEVICE_SHUTTING_DOWN;
  vibe_os_unlock_resource(dev->pm.spinLock);

  STKPI_UNLOCK(dev);

  return res;
}

} // extern "C"
