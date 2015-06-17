/***********************************************************************
 *
 * File: scaler/generic/scaler_device.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_debug.h>
#include <vibe_os.h>
#include <stm_scaler.h>

#include "scaler_device.h"
#include "scaler_handle.h"

/*
 * Scaler registry types.
 */
const char  *stm_scaler_tag        = {"stm_scaler"};
unsigned     stm_scaler_type;

CScalerDevice::CScalerDevice(int32_t nScalers)
{
  m_numScalers = nScalers;
  m_pScalers = 0;
}

bool CScalerDevice::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Create the table for storing CScaler object references
  if(m_numScalers > 0)
  {
    if(!(m_pScalers = new CScaler *[m_numScalers]))
      return false;

    for (uint32_t i = 0; i < m_numScalers; i++) m_pScalers[i] = 0;
  }

  /*
   * Add object types in the stm_registry database.
   */
  RegistryInit();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;
}


CScalerDevice::~CScalerDevice()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  delete[] m_pScalers;

  /*
   * Remove all remaining Scaler's objects from registry database
   */
  RegistryTerm();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


void CScalerDevice::RegistryInit(void)
{
  int res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Add stm_scaler type into stm_registry database. This will be
   * used as parent object for all scaler identifier and their instances.
   */
  res = stm_registry_add_object(STM_REGISTRY_TYPES,
                                stm_scaler_tag,
                                (stm_object_h)&stm_scaler_type);
  if (res == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%p) to the registry",  stm_scaler_tag, &stm_scaler_type );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Failed to register '%s' type (%p) to the registry",  stm_scaler_tag, &stm_scaler_type );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CScalerDevice::RegistryTerm(void)
{
  int res;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Remove object types from the stm_registry database.
   * In opposite order that they were added.
   */
  res = stm_registry_remove_object((stm_object_h)&stm_scaler_type);
  if (res == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Unregister '%s' type (%d) from the registry",  stm_scaler_tag, res );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Failed to unregister '%s' type (%d) from the registry",  stm_scaler_tag, res );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CScalerDevice::AddScaler(CScaler *pScaler)
{
  char ScalerTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pScaler)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add scaler id %u (%p)",pScaler->GetID(), pScaler );

  if(pScaler->GetID() >= m_numScalers)
  {
    TRC( TRC_ID_ERROR, "Scaler ID (%u) out of valid range for device",pScaler->GetID() );
    goto error;
  }

  if(m_pScalers[pScaler->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Scaler ID (%u) already registered with device",pScaler->GetID() );
    goto error;
  }

  if(!pScaler->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create scaler before adding to device" );
    goto error;
  }

  m_pScalers[pScaler->GetID()] = pScaler;

  /*
   * Add stm_scaler(id) object into stm_registry database.
   */
  vibe_os_snprintf(ScalerTag, sizeof(ScalerTag), "%s%d",
                   stm_scaler_tag, pScaler->GetID());

  if(stm_registry_add_object((stm_object_h)&stm_scaler_type,
                             ScalerTag,
                             (stm_object_h)pScaler) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%p) to the registry", ScalerTag, pScaler );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Failed to register '%s' object (%p) to the registry", ScalerTag, pScaler );
    goto error;
  }

  if(!pScaler->RegisterStatistics())
  {
      TRC( TRC_ID_ERROR, "Failed to register attribut to '%s' object (%p)", ScalerTag, pScaler );
      goto error;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pScaler;
  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return false;
}

void CScalerDevice::RemoveScalers(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Remove the Scaler objects
  for(uint32_t i=0;i<m_numScalers;i++)
  {
    /*
     * Remove stm_scaler(id) object from the registry before exiting.
     */
    if(!m_pScalers[i])
      continue;

    if(stm_registry_remove_object(m_pScalers[i]) == 0)
    {
      TRC( TRC_ID_MAIN_INFO, "Unregistered '%s%d' object (%p) from the registry", stm_scaler_tag, i, m_pScalers[i] );
    }
    else
    {
      TRC( TRC_ID_ERROR, "Failed to unregister '%s%d' object (%p) from the registry", stm_scaler_tag, i, m_pScalers[i] );
    }

    delete m_pScalers[i];
    m_pScalers[i] = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

//////////////////////////////////////////////////////////////////////////////
// C Device interface

extern "C" {

struct private_scaler_device_s
{
  struct stm_scaler_device_s  pub_dev;
  int                         use_counter;
};

static struct private_scaler_device_s theScalerDevice = {{0, 0}, 0};


static int scaler_device_initialize(void)
{
  CScalerDevice *pDev = 0;

  theScalerDevice.pub_dev.lock = vibe_os_create_semaphore(0); // Create mutex already held

  if(theScalerDevice.pub_dev.lock == 0)
  {
    TRC( TRC_ID_ERROR, "Failed to create scaler device lock semaphore" );
    goto error_exit;
  }

  if((pDev = AnonymousCreateScalerDevice()) == 0)
  {
    TRC( TRC_ID_ERROR, "Failed to create scaler device object" );
    goto error_exit;
  }

  if(!pDev->Create())
  {
    TRC( TRC_ID_ERROR, "failed to complete scaler device creation" );
    goto error_exit;
  }

  theScalerDevice.pub_dev.handle = (CScalerDevice*)pDev;

  return 0;

error_exit:
  delete pDev;

  if(theScalerDevice.pub_dev.lock)
  {
    vibe_os_delete_semaphore(theScalerDevice.pub_dev.lock);
    theScalerDevice.pub_dev.lock = 0;
  }

  return -1;
}

int stm_scaler_device_open(void)
{
  if(theScalerDevice.pub_dev.lock == 0)
  {
    if(scaler_device_initialize() < 0)
      return -ENODEV;
  }
  else
  {
    if(vibe_os_down_semaphore(theScalerDevice.pub_dev.lock) != 0)
      return -EINTR;
  }

  theScalerDevice.use_counter++;

  vibe_os_up_semaphore(theScalerDevice.pub_dev.lock);

  return 0;
}

void stm_scaler_device_close(void)
{
  if(theScalerDevice.pub_dev.lock != 0)
  {
    if (vibe_os_down_semaphore(theScalerDevice.pub_dev.lock) != 0)
      return;

    theScalerDevice.use_counter--;

    if(theScalerDevice.use_counter>0)
    {
      vibe_os_up_semaphore(theScalerDevice.pub_dev.lock);
      return;
    }

    delete theScalerDevice.pub_dev.handle;

    vibe_os_delete_semaphore(theScalerDevice.pub_dev.lock);
    theScalerDevice.pub_dev.lock=0;

  }
  return;
}

int stm_scaler_device_open_scaler(uint32_t scalerId, stm_scaler_h *scaler_handle, const stm_scaler_config_t *scaler_config)
{
    CScalerDevice    *pDev;
    CScaler          *scaler_object;
    stm_scaler_h      new_scaler_handle = 0;
    char              ScalerTag[STM_REGISTRY_MAX_TAG_SIZE];
    int               res = 0;
    scaler_error_t    error_code;

    if(!CHECK_ADDRESS(scaler_handle))
        return -EFAULT;

    if(!CHECK_ADDRESS(scaler_config))
      return -EFAULT;

    if(vibe_os_down_semaphore(theScalerDevice.pub_dev.lock) != 0)
        return -EINTR;

    pDev = theScalerDevice.pub_dev.handle;

    // Retrieve scaler object
    scaler_object = pDev->GetScaler(scalerId);
    if(!scaler_object)
    {
        TRC( TRC_ID_ERROR, "Failed to get this scaler id=(%d) from the device", scalerId );
        res = -ENODEV;
        goto exit;
    }

    // Allocate new scaler handle
    new_scaler_handle = new struct stm_scaler_s;
    if(!new_scaler_handle)
    {
        TRC( TRC_ID_ERROR, "Failed to allocate scaler handle structure" );
        res = -ENOMEM;
        goto exit;
    }

    // Fill new scaler handle
    new_scaler_handle->lock                            = theScalerDevice.pub_dev.lock;
    new_scaler_handle->scaling_ctx_node.m_scalerObject = scaler_object;

    // Call the scaler to initialize the underneath layers
    error_code = scaler_object->Open(&(new_scaler_handle->scaling_ctx_node), scaler_config);
    if(error_code != ERR_NONE)
    {
        res = -EFAULT;
        goto exit_and_delete;
    }

    /* Add the Scaler instance to the registry */
    vibe_os_snprintf(ScalerTag, sizeof(ScalerTag), "%s%d-%p",
                     stm_scaler_tag, scaler_object->GetID(), new_scaler_handle);

    if(stm_registry_add_instance(STM_REGISTRY_INSTANCES,
                                 (stm_object_h)&stm_scaler_type,
                                 ScalerTag,
                                 (stm_object_h)new_scaler_handle) == 0)
    {
        TRC( TRC_ID_MAIN_INFO, "Registered '%s' instance (%p) to the registry", ScalerTag, new_scaler_handle );
    }
    else
    {
        TRC( TRC_ID_ERROR, "Failed to register '%s' instance (%p) to the registry", ScalerTag, new_scaler_handle );
        res = -EFAULT;
        goto exit_and_close;
    }
    goto exit;

exit_and_close:
    if(scaler_object->Close(&(new_scaler_handle->scaling_ctx_node)))
    {
        TRC( TRC_ID_UNCLASSIFIED, "Failed to close scaler handle" );
    }

exit_and_delete:
    delete(new_scaler_handle);
    new_scaler_handle = 0;

exit:
    vibe_os_up_semaphore(theScalerDevice.pub_dev.lock);

    *scaler_handle = new_scaler_handle;

    return res;
}

int stm_scaler_device_close_scaler(stm_scaler_h scaler_handle)
{
    int             res = 0;
    CScaler        *scaler_object;
    scaler_error_t  error_code;
    void          * lock;

    // Check scaler handle validity
    if(!stm_scaler_handle_valid(scaler_handle))
    {
        return -EINVAL;
    }

    if(vibe_os_down_semaphore(scaler_handle->lock) != 0)
        return -EINTR;

    scaler_object = (CScaler *)(scaler_handle->scaling_ctx_node.m_scalerObject);

    error_code = scaler_object->Close(&(scaler_handle->scaling_ctx_node));
    switch(error_code)
    {
        case ERR_NONE:
            res = 0;
            break;
        case ERR_INTERRUPTED:
            return -EINTR;
            break;
        default:
            res = -EFAULT;
    }

    // Retrieve lock because scaler_handle could be deleted just after.
    lock = scaler_handle->lock;

    // Remove scaler handle instance from the registry
    if(stm_registry_remove_object(scaler_handle) == 0)
    {
        TRC( TRC_ID_MAIN_INFO, "Unregistered instance (%p) from the registry", scaler_handle );
        delete(scaler_handle);
    }
    else
    {
        TRC( TRC_ID_ERROR, "Failed to unregister instance (%p) from the registry", scaler_handle );
        res = -EFAULT;
    }

    vibe_os_up_semaphore(lock);

    return res;

}

bool stm_scaler_handle_valid(const stm_scaler_h scaler_handle)
{
  stm_object_h object_type;

  if(stm_registry_get_object_type((stm_object_h)scaler_handle, &object_type) !=0 )
  {
    TRC( TRC_ID_ERROR, "Handle %p not known in the registry!", scaler_handle );
    return false;
  }

  if(object_type != (stm_object_h)&stm_scaler_type)
  {
    TRC( TRC_ID_ERROR, "%p is NOT a scaler handle!", scaler_handle );
    return false;
  }

  return true;
}

} // extern "C"
