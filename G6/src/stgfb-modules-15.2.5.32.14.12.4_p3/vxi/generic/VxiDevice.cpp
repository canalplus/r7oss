/***********************************************************************
 *
 * File: vxi/generic/VXIDevice.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "stm_registry.h"
#include "vxi/ip/vxi.h"
#include "VxiDevice.h"
#include "stm_vxi.h"

/*
 * Vxi registry types.
 */
const char  *stm_vxi_tag  = {"stm_vxi"};
unsigned     stm_vxi_type;
static int   numDevices = 0;

CVxiDevice::CVxiDevice(uint32_t nVxis)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  TRCIN( TRC_ID_MAIN_INFO, "%p (nVxis=%d)", this, nVxis );

  m_pVxis = 0;
  m_numVxis = nVxis;
  m_pReg = 0;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

CVxiDevice::~CVxiDevice()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Remove all remaining VXI objects from registry database.
   */
  RegistryTerm();

  if(m_pVxis)
    delete [] m_pVxis;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CVxiDevice::Create()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  ASSERTF((m_pVxis == 0),("CVxiDevice::Create() has already been called"));

  if(m_numVxis > 0)
  {
    if(!(m_pVxis = new CVxi *[m_numVxis]))
      return false;

    for (uint32_t i = 0; i < m_numVxis; i++) m_pVxis[i] = 0;
  }

  /*
   * Add object types in the stm_registry database.
   */
  RegistryInit();

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

bool CVxiDevice::AddVxi(CVxi *pVxi)
{
  char VxiTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pVxi)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add VXI id %u (%p)",pVxi->GetID(), pVxi );

  if(pVxi->GetID() >= m_numVxis)
  {
    TRC( TRC_ID_ERROR, "Vxi ID (%u) out of valid range for device",pVxi->GetID() );
    goto error;
  }

  if(m_pVxis[pVxi->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Vxi ID (%u) already registered with device",pVxi->GetID() );
    goto error;
  }

  if(!pVxi->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create vxi before adding to device" );
    goto error;
  }

  m_pVxis[pVxi->GetID()] = pVxi;

  /*
   * Add stm_vxi(id) object into stm_registry database.
   */
  vibe_os_snprintf(VxiTag, sizeof(VxiTag), "%s%d",
                   stm_vxi_tag, pVxi->GetID());

  if(stm_registry_add_object((stm_object_h)&stm_vxi_type,
                             VxiTag,
                             (stm_object_h)pVxi) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%p) to the registry", VxiTag, pVxi );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Failed to register '%s' object (%p) to the registry", VxiTag, pVxi );
    goto error;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pVxi;
  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return false;
}

void CVxiDevice::RegistryInit(void)
{
  int res = 0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Add stm_vxi type into stm_registry database. This will be
   * used as parent object for all vxi identifier and their instances.
   */
  if(numDevices == 0)
  {
    res = stm_registry_add_object(STM_REGISTRY_TYPES,
                                  stm_vxi_tag,
                                 (stm_object_h)&stm_vxi_type);
    if (res == 0)
    {
      TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%p) to the registry",  stm_vxi_tag, &stm_vxi_type );
    }
    else
    {
      TRC( TRC_ID_MAIN_INFO, "Failed to register '%s' type (%p) to the registry",  stm_vxi_tag, &stm_vxi_type );
    }
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CVxiDevice::RegistryTerm(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Remove object types from the stm_registry database.
   * In opposite order that they were added.
   */
  if(numDevices == 0)
  {
    if(stm_registry_remove_object((stm_object_h)&stm_vxi_type) ==0)
    {
      TRC( TRC_ID_MAIN_INFO, "Unregister '%s' type from the registry", stm_vxi_tag );
    }
    else
    {
      TRC( TRC_ID_ERROR, "Failed to unregister '%s' type from the registry", stm_vxi_tag );
    }
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CVxiDevice::RemoveVxis(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  // Remove the Vxi objects
  for(uint32_t i=0;i<m_numVxis;i++)
  {
    /*
     * Remove stm_vxi(id) object from the registry before exiting.
     */
    if(!m_pVxis[i])
      continue;

    if(stm_registry_remove_object(m_pVxis[i]) == 0)
    {
      TRC( TRC_ID_MAIN_INFO, "Unregistered '%s%d' object (%p) from the registry", stm_vxi_tag, i, m_pVxis[i] );
    }
    else
    {
      TRC( TRC_ID_ERROR, "Failed to unregister '%s%d' object (%p) from the registry", stm_vxi_tag, i, m_pVxis[i] );
    }

    delete m_pVxis[i];
    m_pVxis[i] = 0;
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

//////////////////////////////////////////////////////////////////////////////
// C Device interface

#if defined(__cplusplus)
extern "C" {
#endif

#include "linux/kernel/drivers/stm/vxi/platform.h"
static stm_vxi_platform_data_t *data = 0;

struct private_vxi_device_s
{
  struct stm_vxi_s  public_dev;
  int               use_count;
};

static unsigned maxDevices = 1;

 /*
  * The current implementation, it should be updated in case of more than
  * one divice.
  */
static struct private_vxi_device_s theVxiDevice = {{0,0}, 0};

bool stm_vxi_is_handle_valid(const stm_vxi_h vxiHandle)
{
  stm_object_h object_type;

  if(stm_registry_get_object_type((stm_object_h)vxiHandle, &object_type) !=0 )
  {
    TRC( TRC_ID_ERROR, "Handle %p not known in the registry!", vxiHandle );
    return false;
  }

  if(object_type != (stm_object_h)&stm_vxi_type)
  {
    TRC( TRC_ID_ERROR, "%p is NOT a vxi handle!", vxiHandle );
    return false;
  }

  return true;
}

static int vxi_device_initialize(uint32_t id)
{
  CVxiDevice *pVxiDevice = 0;
  unsigned int size = 0;
  theVxiDevice.public_dev.lock = vibe_os_create_semaphore(0); // Create mutex already held

  if(theVxiDevice.public_dev.lock == 0)
  {
    TRC( TRC_ID_ERROR, "create vxi device lock semaphore failed" );
    goto error_exit;
  }

  stmvxi_get_vxi_platform_data_size(&size);
  data = (stm_vxi_platform_data_t *)vibe_os_allocate_memory(size);
  stmvxi_get_platform_data(data);

  if((pVxiDevice = AnonymousCreateVxiDevice(data)) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "create vxi device object failed" );
    goto error_exit;
  }

  if(!pVxiDevice->Create())
  {
    TRC( TRC_ID_ERROR, "failed to complete vxi device creation" );
    goto error_exit;
  }

  theVxiDevice.public_dev.handle = (uint32_t)pVxiDevice;

  stm_coredisplay_magic_set(&theVxiDevice.public_dev, VALID_DEVICE_HANDLE);

  numDevices++;
  return 0;

error_exit:
  delete pVxiDevice;

  if(theVxiDevice.public_dev.lock)
  {
    vibe_os_delete_semaphore(theVxiDevice.public_dev.lock);
    theVxiDevice.public_dev.lock = 0;
  }

  return -1;
}

int stm_vxi_device_open(uint32_t id, stm_vxi_h *vxiHandle)
{
  char        VxiTag[STM_REGISTRY_MAX_TAG_SIZE];
  CVxiDevice *pDev = 0;
  CVxi       *pVxi = 0;
  int         error_code;
  stm_vxi_h   new_vxi_handle = 0;  /* Public vxi handle */

  if(id>=maxDevices)
    return -ENODEV;

  if(!CHECK_ADDRESS(vxiHandle))
    return -EFAULT;

  if(theVxiDevice.public_dev.lock == 0)
  {
    if(vxi_device_initialize(id) < 0)
      return -ENODEV;
  }
  else
  {
    if(vibe_os_down_semaphore(theVxiDevice.public_dev.lock) != 0)
      return -EINTR;
  }

  theVxiDevice.use_count++;

  pDev = (CVxiDevice *)theVxiDevice.public_dev.handle;

  pVxi = pDev->GetVxi(id);
  if(!pVxi)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Failed to get a valid vxi from the device" );
    error_code = -ENODEV;
    goto exit_error;
  }

  // Allocate new vxi handle
  new_vxi_handle = new struct stm_vxi_s;
  if(!new_vxi_handle)
  {
    TRC( TRC_ID_ERROR, "Failed to allocate vxi handle structure" );
    error_code = -ENOMEM;
    goto exit_error;
  }

  new_vxi_handle->handle   = (uint32_t)pVxi;
  new_vxi_handle->lock     = theVxiDevice.public_dev.lock;

  /* Add the vxi instance to the registry */
  vibe_os_snprintf(VxiTag, sizeof(VxiTag), "%s%d-%p",
                   stm_vxi_tag, pVxi->GetID(), new_vxi_handle);

  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                (stm_object_h)&stm_vxi_type,
                                VxiTag,
                                (stm_object_h)new_vxi_handle) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' instance (%p) to the registry", VxiTag, new_vxi_handle );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Failed to register '%s' instance (%p) to the registry", VxiTag, new_vxi_handle );
  }

  vibe_os_up_semaphore(theVxiDevice.public_dev.lock);

  *vxiHandle = new_vxi_handle;

  return 0;

exit_error:
    vibe_os_up_semaphore(theVxiDevice.public_dev.lock);

  return error_code;

}

int stm_vxi_device_close(stm_vxi_h vxiHandle)
{
  struct private_vxi_device_s *vxi_dev = (struct private_vxi_device_s*)vxiHandle;

  if(!stm_vxi_is_handle_valid(vxiHandle))
    return -ENODEV;

  vxi_dev = &theVxiDevice;

  if(vibe_os_down_semaphore(vxiHandle->lock) != 0)
    return -EINTR;

  /*
   * Remove object instance from the registry before exiting.
   */
  if(stm_registry_remove_object(vxiHandle) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Unregister instance (%p) from the registry", vxiHandle );
  }
  else
  {
    TRC( TRC_ID_ERROR, "failed to unregister vxi instance = %p from the registry",vxiHandle );
  }
  vxi_dev->use_count--;

  if(vxi_dev->use_count > 0)
  {
    vibe_os_up_semaphore(vxiHandle->lock);
    goto free_handle;
  }

  stm_coredisplay_magic_clear(vxiHandle);

  numDevices--;

  delete (CVxiDevice *)vxi_dev->public_dev.handle;
  vibe_os_delete_semaphore(vxi_dev->public_dev.lock);
  vxi_dev->public_dev.lock = 0;
  vibe_os_free_memory((void*)data);

free_handle:
  /*
   * Release the structure's memory.
   */
  delete vxiHandle;

  return 0;
}

#if defined(__cplusplus)
} // extern "C"
#endif
