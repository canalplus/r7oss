/***********************************************************************
 *
 * File: pixel_capture/generic/CaptureDevice.cpp
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include "Capture.h"
#include "CaptureDevice.h"

#include "stm_pixel_capture.h"

#include <pixel_capture/ip/gamma/stmgammacapture.h>
#include <pixel_capture/ip/fvdp/stmscalercapture.h>
#include <pixel_capture/ip/dvp/stmdvpcapture.h>

/*
 * Pixel Capture devices internal counter.
 * This is used to know about the first device initialization.
 */
static int numDevices = 0;

/*
 * Pixel Capture Device registry types.
 * This is common capture device type and tag. We only register this
 * type at the first device initialization.
 * The Device type will be removed from the registry database when
 * destroying the last existing device.
 */
const char  *stm_capture_device_tag        = {"stm_capture_device"};
unsigned     stm_capture_device_type;

/*
 * Capture objects registry types.
 * This is for different capture device types. For each capture type
 * we are registring a specific object type in the registry.
 * Those  capture types will be removed from the registry when the
 * CaptureDevice is no more used (no Captures attached to the device).
 */
static unsigned stm_compo_capture_type;
static unsigned stm_fvdp_capture_type;
static unsigned stm_dvp_capture_type;

static struct {
    const char *tag;
    stm_object_h type;
} stm_pixel_capture_types[] = {
  {"stm_compo_capture", &stm_compo_capture_type},
  {"stm_fvdp_capture", &stm_fvdp_capture_type},
  {"stm_dvp_capture", &stm_dvp_capture_type},
};

CPixelCaptureDevice::CPixelCaptureDevice(stm_pixel_capture_device_type_t type)
{
  TRCIN( TRC_ID_MAIN_INFO, "%p (Type=%d)", this, type );

  m_pCaptures           = 0;
  m_numCaptures         = 1;
  m_LastVTGEventTime    = (stm_pixel_capture_time_t)0;
  m_Type                = type;

  m_bIsSuspended = false;
  m_bIsFrozen    = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


CPixelCaptureDevice::~CPixelCaptureDevice()
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  RemoveCaptures();

  /*
   * Remove all remaining pixel CaptureDevice's objects from registry
   * database.
   */
  RegistryTerm();

  delete [] m_pCaptures;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CPixelCaptureDevice::Create(uint32_t base_address, uint32_t size, stm_pixel_capture_hw_features_t hw_features)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  ASSERTF((m_pCaptures == 0),("CPixelCaptureDevice::Create() has already been called"));

  if(m_numCaptures > 0)
  {
    if(!(m_pCaptures = new CCapture *[m_numCaptures]))
      return false;

    for (uint32_t i = 0; i < m_numCaptures; i++) m_pCaptures[i] = 0;
  }

  /*
   * Add object types in the stm_registry database.
   */
  RegistryInit();

  /*
   * Create is done okay, call for create capture
   */
  CreateCaptures(base_address, size, hw_features);

  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return true;
}

bool CPixelCaptureDevice::CreateCaptures(uint32_t base_address, uint32_t size, stm_pixel_capture_hw_features_t hw_features)
{
  bool     retval=true;
  uint32_t capture_id=0;

  switch(m_Type)
  {
    case STM_PIXEL_CAPTURE_COMPO:
      {
        /*
         * Only one Compositor Capture pipeline available on all supported chips
         */
        CGammaCompositorCAP *pCompoCapture = new CGammaCompositorCAP("COMPO-CAPTURE_0", capture_id++,
                                                      base_address, size,
                                                      this, STM_PIXEL_DISPLAY);
        if(!pCompoCapture || !pCompoCapture->Create() || !AddCapture(pCompoCapture))
        {
          TRC( TRC_ID_ERROR, "failed to create compositor capture object" );
          retval=false;
        }
      }
      break;

    case STM_PIXEL_CAPTURE_DVP:
      {
        /*
         * Only one DVP Capture pipeline available on all supported chips
         */
         CDvpCAP *pDVPCapture = new CDvpCAP("DVP-CAPTURE_0", capture_id++,
                                                       base_address, size,
                                                       this, STM_PIXEL_DISPLAY,
                                                       hw_features);
         if(!pDVPCapture || !pDVPCapture->Create() || !AddCapture(pDVPCapture))
         {
           TRC( TRC_ID_ERROR, "failed to create DVP capture object" );
           retval=false;
         }
      }
      break;

    case STM_PIXEL_CAPTURE_FVDP:
      {
        /*
         * Only one FVDP Capture pipeline available on all supported chips
         */
        CFvdpScalerCAP *pScalerCapture = new CFvdpScalerCAP("FVDP-CAPTURE_0", capture_id++,
                                                            this, STM_PIXEL_CAPTURE);
        if(!AddCapture(pScalerCapture))
        {
          TRC( TRC_ID_ERROR, "failed to create scaler capture object" );
          retval=false;
        }
      }
      break;

    default:
	/* Should never happen */
	BUG_ON(TRUE);
  }

  return retval;
}

void CPixelCaptureDevice::RegistryInit()
{
  char     DeviceTag[STM_REGISTRY_MAX_TAG_SIZE];
  int      res=0;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Add Pixel Capture type into stm_registry database. This will be
   * used as parent object for all created display devices
   * objects.
   *
   * We should not break execution if the device type was previously
   * added to the registry. This will ensure the adding of second device
   * objects in the registry using same object type
   *
   * Types are created only for the first initialization.
   */
  if(numDevices == 0)
  {
    res = stm_registry_add_object(STM_REGISTRY_TYPES,
                    stm_capture_device_tag,
                    (stm_object_h)&stm_capture_device_type);
    if (0 == res)
    {
      TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%p)",  stm_capture_device_tag, &stm_capture_device_type );
    }
  }

  /*
   * Add real Capture Device object into stm_registry database. This will be
   * used in stm_pixel_capture_open() as parent object instead using fake
   * object.
   */
  vibe_os_snprintf (DeviceTag, sizeof(DeviceTag), "%s%d",
                    stm_capture_device_tag, (int)m_Type);
  res = stm_registry_add_object((stm_object_h)&stm_capture_device_type,
                    DeviceTag,
                    (stm_object_h)this);
  if (0 == res)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%p)", DeviceTag, this );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' type (%d)",  DeviceTag, res );
  }

  /*
   * Types are created only for the first initialization.
   */
  vibe_os_snprintf (DeviceTag, sizeof(DeviceTag), "%s%d",
                    stm_pixel_capture_types[m_Type].tag, numDevices);
  res = stm_registry_add_object((stm_object_h)this,
                    DeviceTag,
                    stm_pixel_capture_types[m_Type].type);
  if (0 != res)
  {
    TRC( TRC_ID_ERROR, "Cannot register '%s' type (%d)",  DeviceTag, res );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' type (%d)",  DeviceTag, res );
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

void CPixelCaptureDevice::RegistryTerm()
{
  int res = 0;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Remove object types from the stm_registry database.
   * Don't remove the capture device type object until we move this
   * object otherwise we will fail to clean up the registry.
   */
  if ((res = stm_registry_remove_object(stm_pixel_capture_types[m_Type].type)) < 0)
  {
    TRC( TRC_ID_ERROR, "Cannot unregister '%s' type (%d)",  stm_pixel_capture_types[m_Type].tag, res );
  }
  else
  {
    TRC( TRC_ID_MAIN_INFO, "Unregistered '%s' type (%d)",  stm_pixel_capture_types[m_Type].tag, res );
  }

  /*
   * Remove this object from the stm_registry database.
   */
  if(stm_registry_remove_object((stm_object_h)this) < 0)
    TRC( TRC_ID_ERROR, "failed to remove capture object = %p from the registry",this );
  else
    TRC( TRC_ID_MAIN_INFO, "Remove capture object = %p from the registry",this );

  /*
   * Now remove the capture type object from the stm_registry database.
   */
  if(numDevices == 0)
  {
    if(stm_registry_remove_object((stm_object_h)&stm_capture_device_type) < 0)
      TRC( TRC_ID_ERROR, "failed to remove capture type = %p from the registry",&stm_capture_device_type );
    else
      TRC( TRC_ID_MAIN_INFO, "Remove capture type = %p from the registry",&stm_capture_device_type );
  }
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


bool CPixelCaptureDevice::AddCapture(CCapture *pCapture)
{
  char CaptureTag[STM_REGISTRY_MAX_TAG_SIZE];

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!pCapture)
    return false;

  TRC( TRC_ID_MAIN_INFO, "Trying to add Capture \"%s\" ID:%u (%p) to device (%p)",pCapture->GetName(),pCapture->GetID(),pCapture,this );

  if(pCapture->GetID() >= m_numCaptures)
  {
    TRC( TRC_ID_ERROR, "Output ID (%u) out of valid range for device",pCapture->GetID() );
    goto error;
  }

  if(m_pCaptures[pCapture->GetID()] != 0)
  {
    TRC( TRC_ID_ERROR, "Capture ID (%u) already registered with device",pCapture->GetID() );
    goto error;
  }

  if(!pCapture->Create())
  {
    TRC( TRC_ID_ERROR, "Failed to create intput before adding to device" );
    goto error;
  }

  m_pCaptures[pCapture->GetID()] = pCapture;

  /*
   * Add the real Plane object to the registry.
   */
  vibe_os_snprintf (CaptureTag, sizeof(CaptureTag), "%s",
                    pCapture->GetName());

  if (stm_registry_add_object(stm_pixel_capture_types[m_Type].type,
                  CaptureTag,
                (stm_object_h)pCapture) != 0)
    TRC( TRC_ID_ERROR, "Cannot register '%s' object (%d)",  CaptureTag, pCapture->GetID() );
  else
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' object (%d)",  CaptureTag, pCapture->GetID() );

  pCapture->RegisterStatistics();

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return true;

error:
  delete pCapture;
  TRCOUT( TRC_ID_MAIN_INFO, "" );

  return false;
}


void CPixelCaptureDevice::RemoveCaptures(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  /*
   * Now delete everything.
   */
  for (uint32_t i = 0; i < m_numCaptures; i++)
  {
    /*
     * Remove object from the registry before exiting.
     */
    if(!m_pCaptures[i])
      continue;

    if(stm_registry_remove_object(m_pCaptures[i]) < 0)
    {
      TRC( TRC_ID_ERROR, "failed to remove output object = %p from the registry",m_pCaptures[i] );
    }

    delete m_pCaptures[i];
    m_pCaptures[i] = 0L;
  }
  TRC( TRC_ID_MAIN_INFO, "CPixelCaptureDevice::RemoveCaptures() deleted all Captures" );

  TRCOUT( TRC_ID_MAIN_INFO, "" );
}


int CPixelCaptureDevice::Freeze(void)
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

  for(id=0; id<m_numCaptures; id++ )
  {
    if(m_pCaptures[id])
      m_pCaptures[id]->Freeze();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


int CPixelCaptureDevice::Suspend(void)
{
  int             res=0;
  uint32_t        id;

  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(m_bIsSuspended)
    return res;

  /*
   * Set the capture device state to suspended to avoid any coming update while
   * we are switch off the capture device.
   *
   * Captures will continue processing previous
   * update as they are not yet suspended at this point.
   */
  m_bIsSuspended = true;

  for(id=0; id<m_numCaptures; id++ )
  {
    if(m_pCaptures[id])
      m_pCaptures[id]->Suspend();
  }

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


int CPixelCaptureDevice::Resume(void)
{
  int             res=0;
  uint32_t        id;
  TRCIN( TRC_ID_MAIN_INFO, "" );

  if(!m_bIsSuspended)
    return res;

  for(id=0; id<m_numCaptures; id++)
  {
    if(m_pCaptures[id])
      m_pCaptures[id]->Resume();
  }

  m_bIsSuspended = false;
  m_bIsFrozen = false;

  TRCOUT( TRC_ID_MAIN_INFO, "" );
  return res;
}


void CPixelCaptureDevice::UpdateCaptureDevice(uint32_t timingID, stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  TRC( TRC_ID_UNCLASSIFIED, "Update capture = %p - timingID = %d - vsyncTime = %llu", this, timingID, vsyncTime );

  /*
   * Go through all captures and update them. There may be circumstances where
   * more than one capture actually reports the same timing ID.
   */
  for(unsigned i=0;i<m_numCaptures;i++)
  {
    if(m_pCaptures[i] && m_pCaptures[i]->hasBuffersToRelease() && (m_pCaptures[i]->GetTimingID() == timingID))
    {
      m_LastVTGEventTime = vsyncTime;
      if(m_pCaptures[i]->IsAttached())
      {
        m_pCaptures[i]->ProcessNewCaptureBuffer(vsyncTime, timingevent);
      }
      else
      {
        m_pCaptures[i]->CaptureUpdateHW(vsyncTime, timingevent);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// C Device interface

#if defined(__cplusplus)
extern "C" {
#endif

struct private_pixel_capture_s
{
  struct stm_pixel_capture_s public_dev;
  int use_count;
};

static struct private_pixel_capture_s theDevices[3] = {{{0}},{{0}},{{0}}};

/*********************************************************************/
/** C Private Device interfaces                                      */
/*********************************************************************/
int stm_pixel_capture_device_register_pm_runtime_hooks(stm_pixel_capture_device_type_t type, uint32_t instance, int (*get)(const uint32_t type, const uint32_t id), int (*put)(const uint32_t type, const uint32_t id))
{
  if(!CHECK_ADDRESS(get) || !CHECK_ADDRESS(put))
    return -EFAULT;

  if(theDevices[type].public_dev.lock == 0)
  {
    /* Parent Capture Device object is not yet created */
    TRC( TRC_ID_ERROR, "Pixel Capture Device not yet created!!" );
    return -ENODEV;
  }
  else
  {
    if(vibe_os_down_semaphore(theDevices[type].public_dev.lock) != 0)
      return -EINTR;
  }

  /* Setup PM data */
  theDevices[type].public_dev.pm.id           = instance;
  theDevices[type].public_dev.pm.state        = CAPTURE_DEVICE_ACTIVE;
  theDevices[type].public_dev.pm.runtime_get  = get;
  theDevices[type].public_dev.pm.runtime_put  = put;

  vibe_os_up_semaphore(theDevices[type].public_dev.lock);

  return 0;
}


int stm_pixel_capture_device_get_power_state(stm_pixel_capture_h pixel_capture, uint32_t *pm_state)
{
  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Pixel Capture Device handle!!" );
    return -ENODEV;
  }

  /* pm_state pointer should be valid */
  if(!CHECK_ADDRESS(pm_state))
    return -EFAULT;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );
  *pm_state = pixel_capture->pm.state;

  vibe_os_up_semaphore(pixel_capture->lock);

  return 0;
}


bool stm_pixel_capture_device_is_suspended(stm_pixel_capture_h pixel_capture)
{
  uint32_t pm_state = CAPTURE_DEVICE_ACTIVE;
  bool is_suspended = false;

  if(stm_pixel_capture_device_get_power_state(pixel_capture, &pm_state) < 0)
    return true;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );

  if(pm_state != CAPTURE_DEVICE_ACTIVE)
    is_suspended = true;

  if(is_suspended)
    TRC( TRC_ID_UNCLASSIFIED, "display device %p is not active (pm_state = %d) !!!", pixel_capture, pm_state );

  return is_suspended;
}


int stm_pixel_capture_device_init(stm_pixel_capture_device_type_t type, uint32_t base_address, uint32_t size, stm_pixel_capture_hw_features_t hw_features)
{
  CPixelCaptureDevice *pCaptureDevice = 0;
  struct private_pixel_capture_s *dev = &theDevices[type];

  dev->public_dev.lock = vibe_os_create_semaphore(0); // Create mutex already held

  if(dev->public_dev.lock == 0)
  {
    TRC( TRC_ID_ERROR, "create pixel_capture lock semaphore failed" );
    goto error_exit;
  }

  if((pCaptureDevice = new CPixelCaptureDevice(type)) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "create capture device object failed (type = %d)", (unsigned int)type );
    goto error_exit;
  }

  if(!pCaptureDevice->Create(base_address, size, hw_features))
  {
    TRC( TRC_ID_ERROR, "failed to complete capture device object creation (type = %d)", (unsigned int)type );
    goto error_exit;
  }

  dev->public_dev.handle = (uint32_t)pCaptureDevice;

  /* Initialise PM state */
  dev->public_dev.pm.id           = 0;
  dev->public_dev.pm.state        = CAPTURE_DEVICE_ACTIVE;
  dev->public_dev.pm.runtime_get  = 0;
  dev->public_dev.pm.runtime_put  = 0;

  numDevices++;

  vibe_os_up_semaphore(theDevices[type].public_dev.lock);

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture device : %p", pCaptureDevice );
  return 0;

error_exit:
  delete pCaptureDevice;

  if(dev->public_dev.lock)
  {
    vibe_os_delete_semaphore(dev->public_dev.lock);
    dev->public_dev.lock = 0;
  }

  TRC( TRC_ID_API_PIXEL_CAPTURE, "failed to init pixel_capture device" );
  return -1;
}


/* would be part of vibe_os module */
bool stm_pixel_capture_is_handle_valid(const stm_pixel_capture_h pixel_capture,
                            stm_object_h type)
{
  stm_object_h object_type;

  if(!pixel_capture)
    return false;

  if(stm_registry_get_object_type((stm_object_h)pixel_capture, &object_type) !=0 )
  {
    TRC( TRC_ID_ERROR, "Handle %p not known in the registry!", pixel_capture );
    return false;
  }

  /*
   * Depending on the implementation the user would use different object
   * type then Get passed object type from the handle if no valid type is begin
   * passed.
   *
   * The user should provide the object type for a good handle checking.
   */
  if(!type) type = (stm_object_h)pixel_capture->handle;

  if(object_type != type)
  {
    TRC( TRC_ID_ERROR, "%p is NOT a pixel_capture handle!", pixel_capture );
    return false;
  }

  return true;
}


int stm_pixel_capture_device_open(stm_pixel_capture_device_type_t type, const uint32_t instance,
                                  stm_pixel_capture_h *pixel_capture)
{
  CPixelCaptureDevice  *pCaptureDevice;
  CCapture             *pCapture;            /* Real (and internal) capture handle */
  stm_pixel_capture_h   capture_handle = 0;  /* Public capture handle */
  char                  CaptureTag[STM_REGISTRY_MAX_TAG_SIZE];
  int                   error_code = 0;

  /*
   * Check for supported Capture types.
   */
  if((type != STM_PIXEL_CAPTURE_FVDP) && (type != STM_PIXEL_CAPTURE_COMPO) && (type != STM_PIXEL_CAPTURE_DVP))
    return -ENOTSUP;

  if(!CHECK_ADDRESS(pixel_capture))
    return -EFAULT;

  if(theDevices[type].public_dev.lock == 0)
  {
    /* Parent Capture Device object is not yet created */
    return -ENODEV;
  }
  else
  {
    if(vibe_os_down_semaphore(theDevices[type].public_dev.lock) != 0)
      return -EINTR;
  }
  pCaptureDevice = (CPixelCaptureDevice *)theDevices[type].public_dev.handle;

  theDevices[type].use_count++;

  pCapture = pCaptureDevice->GetCapture(instance);
  if(!pCapture)
  {
    TRC( TRC_ID_UNCLASSIFIED, "Failed to get a valid capture from the device" );
    error_code = -ENODEV;
    goto exit_error;
  }

  capture_handle = new struct stm_pixel_capture_s;
  if(!capture_handle)
  {
    TRC( TRC_ID_ERROR, "Failed to allocate capture handle structure" );
    error_code = -ENOMEM;
    goto exit_error;
  }

  capture_handle->handle   = (uint32_t)pCapture;
  capture_handle->type     = type;
  capture_handle->owner    = capture_handle;
  capture_handle->lock     = theDevices[type].public_dev.lock;

  capture_handle->pm.id           = theDevices[type].public_dev.pm.id;
  capture_handle->pm.state        = theDevices[type].public_dev.pm.state;
  capture_handle->pm.runtime_get  = theDevices[type].public_dev.pm.runtime_get;
  capture_handle->pm.runtime_put  = theDevices[type].public_dev.pm.runtime_put;

  /* Add the Capture instance to the registry */
  vibe_os_snprintf(CaptureTag, sizeof(CaptureTag), "%s-%p",
                   pCapture->GetName(), capture_handle);

  if(stm_registry_add_instance (STM_REGISTRY_INSTANCES,
                                (stm_object_h)capture_handle->handle,
                                CaptureTag,
                                (stm_object_h)capture_handle) == 0)
  {
    TRC( TRC_ID_MAIN_INFO, "Registered '%s' instance (%p) to the registry", CaptureTag, capture_handle );
  }
  else
  {
    TRC( TRC_ID_ERROR, "Failed to register '%s' instance (%p) to the registry", CaptureTag, capture_handle );
  }

  vibe_os_up_semaphore(theDevices[type].public_dev.lock);

  *pixel_capture = capture_handle;

  if(theDevices[type].use_count > 1)
  {
    /* DPM : Get device */
    if(capture_handle->pm.runtime_get)
    {
      error_code = capture_handle->pm.runtime_get(capture_handle->type, capture_handle->pm.id);
      if(error_code<0)
        goto exit_error;
    }
  }

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", pCapture->GetName() );
  return 0;

exit_error:
  delete capture_handle;
  delete pCapture;
  delete pCaptureDevice;

  if(theDevices[type].public_dev.lock)
  {
    vibe_os_delete_semaphore(theDevices[type].public_dev.lock);
    theDevices[type].public_dev.lock = 0;
    numDevices--;
  }

  TRC( TRC_ID_API_PIXEL_CAPTURE, "failed to open pixel_capture device" );
  return error_code;
}


int stm_pixel_capture_device_close(const stm_pixel_capture_h pixel_capture)
{
  struct private_pixel_capture_s *capture_dev = (struct private_pixel_capture_s *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
    return -ENODEV;

  capture_dev = &theDevices[pixel_capture->type];

  if (vibe_os_down_semaphore(pixel_capture->lock)!=0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );

  /*
   * Remove object instance from the registry before exiting.
   */
  if(stm_registry_remove_object(pixel_capture) < 0)
    TRC( TRC_ID_ERROR, "failed to remove display object = %p from the registry",pixel_capture );

  /* decrement the capture counter */
  capture_dev->use_count--;

  if(capture_dev->use_count > 0)
  {
    if(capture_dev->use_count > 1)
    {
      /* DPM : Put device */
      if(pixel_capture->pm.runtime_put)
        pixel_capture->pm.runtime_put(pixel_capture->type, pixel_capture->pm.id);
    }
    TRC( TRC_ID_MAIN_INFO, "Still opened instance on this device %p",pixel_capture );
    vibe_os_up_semaphore(pixel_capture->lock);
    goto free_handle;
  }

  /*
   * Decrement devices number before going on object destruction.
   * Remember that the capture device registry type will be removed only
   * once there is no more devices (numDevices = 0).
   */
  numDevices--;

  /* try to delete the Capture Device object if it is no more used */
  delete (CPixelCaptureDevice *)capture_dev->public_dev.handle;
  vibe_os_delete_semaphore(capture_dev->public_dev.lock);
  capture_dev->public_dev.lock = 0;

free_handle:
  /*
   * Release the structure's memory.
   */
  delete pixel_capture;

  return 0;
}


int stm_pixel_capture_device_update(const stm_pixel_capture_h pixel_capture, uint32_t timing_id,
                          stm_pixel_capture_time_t vsyncTime, uint32_t timingevent)
{
  struct private_pixel_capture_s *capture_dev = (struct private_pixel_capture_s *)NULL;
  CPixelCaptureDevice *pCPixelCaptureDevice = (CPixelCaptureDevice *)NULL;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Pixel Capture Device handle!!" );
    return -ENODEV;
  }

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );
  capture_dev = &theDevices[pixel_capture->type];

  /* don't lock the device as we could be running in interrupt context */

  pCPixelCaptureDevice = (CPixelCaptureDevice *)capture_dev->public_dev.handle;

  pCPixelCaptureDevice->UpdateCaptureDevice(timing_id, vsyncTime, timingevent);

  return 0;
}


int stm_pixel_capture_device_freeze(stm_pixel_capture_h pixel_capture)
{
  struct private_pixel_capture_s *capture_dev = (struct private_pixel_capture_s *)NULL;
  CPixelCaptureDevice *pCPixelCaptureDevice = (CPixelCaptureDevice *)NULL;
  int res=0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Pixel Capture Device handle!!" );
    return -ENODEV;
  }

  capture_dev = &theDevices[pixel_capture->type];

  pCPixelCaptureDevice = (CPixelCaptureDevice *)capture_dev->public_dev.handle;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );

  res=pCPixelCaptureDevice->Freeze();
  if(res == 0)
    pixel_capture->pm.state = CAPTURE_DEVICE_FROZEN;

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_device_suspend(stm_pixel_capture_h pixel_capture)
{
  struct private_pixel_capture_s *capture_dev = (struct private_pixel_capture_s *)NULL;
  CPixelCaptureDevice *pCPixelCaptureDevice = (CPixelCaptureDevice *)NULL;
  int res=0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Pixel Capture Device handle!!" );
    return -ENODEV;
  }

  capture_dev = &theDevices[pixel_capture->type];

  pCPixelCaptureDevice = (CPixelCaptureDevice *)capture_dev->public_dev.handle;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );

  res=pCPixelCaptureDevice->Suspend();
  if(res == 0)
    pixel_capture->pm.state = CAPTURE_DEVICE_SUSPENDED;

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}


int stm_pixel_capture_device_resume(const stm_pixel_capture_h pixel_capture)
{
  struct private_pixel_capture_s *capture_dev = (struct private_pixel_capture_s *)NULL;
  CPixelCaptureDevice *pCPixelCaptureDevice = (CPixelCaptureDevice *)NULL;
  int res=0;

  if(!stm_pixel_capture_is_handle_valid(pixel_capture, 0))
  {
    TRC( TRC_ID_ERROR, "Invalid Pixel Capture Device handle!!" );
    return -ENODEV;
  }

  capture_dev = &theDevices[pixel_capture->type];

  pCPixelCaptureDevice = (CPixelCaptureDevice *)capture_dev->public_dev.handle;

  if(vibe_os_down_semaphore(pixel_capture->lock) != 0)
    return -EINTR;

  TRC( TRC_ID_API_PIXEL_CAPTURE, "pixel_capture : %s", ((CCapture *)pixel_capture->handle)->GetName() );

  res=pCPixelCaptureDevice->Resume();
  if(res == 0)
    pixel_capture->pm.state = CAPTURE_DEVICE_ACTIVE;

  vibe_os_up_semaphore(pixel_capture->lock);

  return res;
}

#if defined(__cplusplus)
} // extern "C"
#endif
