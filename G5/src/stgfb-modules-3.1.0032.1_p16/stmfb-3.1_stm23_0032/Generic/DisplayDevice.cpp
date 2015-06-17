/***********************************************************************
 *
 * File: Generic/DisplayDevice.cpp
 * Copyright (c) 2000, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include "IOS.h"
#include "IDebug.h"
#include "GAL.h"
#include "Output.h"
#include "DisplayPlane.h"
#include "DisplayDevice.h"

CDisplayDevice::CDisplayDevice(void)
{
  int i;

  m_CurrentVTGSync = 0;

  for (i = 0; i < CDISPLAYDEVICE_MAX_OUTPUTS; i++)
  {
    m_pOutputs[i] = 0L;
  }
}


CDisplayDevice::~CDisplayDevice()
{
  DEBUGF2(2,("CDisplayDevice::~CDisplayDevice\n"));
  /*
   * We have to deferr destoying outputs to the subclasses,
   * as their destructors may need to access device registers
   * and we cannot guarantee that those registers are still mapped
   * in the memory space at this point.
   */
  DEBUGF2(2,("CDisplayDevice::~CDisplayDevice out\n"));
}


bool CDisplayDevice::Create()
{
  return true;
}


//////////////////////////////////////////////////////////////////////////////
// C Device interface

extern "C" {

struct private_display_device_s
{
  stm_display_device_t public_dev;
  int use_count;
};

static unsigned maxDevices = 2;
static int numDevices = 0;
static struct private_display_device_s theDevices[2] = {{{0}},{{0}}};

/*
 * We need a cache of locks for blitter instances, because they might be
 * shared between multiple display pipeline devices. This means we cannot use
 * the device lock to provide thread safe access to the blitter. In any case
 * using the device lock isn't a great idea, as syncing the blitter (which can
 * block) may needlessly prevent buffers being queued on display planes.
 *
 * The number of blitters is based on a fully implemented BDispII block which
 * can have 2CQs and 4AQs.
 */
static unsigned maxBlitters = 6;
static ULONG blitterLocks[6] = {0};

static int get_num_outputs(stm_display_device_t *dev)
{
  CDisplayDevice* pDev = (CDisplayDevice*)(dev->handle);
  int n;

  if(g_pIOS->DownSemaphore(dev->lock) != 0)
    return -1;

  n = pDev->GetNumOutputs();

  g_pIOS->UpSemaphore(dev->lock);

  return n;
}

extern stm_display_output_ops_t stm_display_output_ops;

static struct stm_display_output_s *get_output(stm_display_device_t *dev, ULONG outputID)
{
  CDisplayDevice* pDev = (CDisplayDevice*)(dev->handle);
  struct stm_display_output_s *public_output = 0;
  COutput *real_output;

  if(g_pIOS->DownSemaphore(dev->lock) != 0)
    return 0;

  real_output = pDev->GetOutput(outputID);
  if(!real_output)
    goto exit;

  public_output = new struct stm_display_output_s;
  if(!public_output)
    goto exit;

  public_output->handle = (ULONG)real_output;
  public_output->owner  = dev;
  public_output->lock   = dev->lock;
  public_output->ops    = &stm_display_output_ops;

exit:
  g_pIOS->UpSemaphore(dev->lock);

  return public_output;
}


extern stm_display_plane_ops_t stm_display_plane_ops;

static struct stm_display_plane_s *get_plane(stm_display_device_t *dev, ULONG planeID)
{
  CDisplayDevice* pDev = (CDisplayDevice*)(dev->handle);
  struct stm_display_plane_s *public_plane = 0;
  CDisplayPlane *real_plane;

  if(g_pIOS->DownSemaphore(dev->lock) != 0)
    return 0;

  real_plane = pDev->GetPlane((stm_plane_id_t)planeID);
  if(!real_plane)
    goto exit;

  public_plane = new struct stm_display_plane_s;
  if(!public_plane)
    goto exit;

  public_plane->handle = (ULONG)real_plane;
  public_plane->owner  = dev;
  public_plane->lock   = dev->lock;
  public_plane->ops    = &stm_display_plane_ops;

exit:
  g_pIOS->UpSemaphore(dev->lock);

  return public_plane;
}


extern const stm_display_blitter_ops_t stm_display_blitter_ops;

static struct stm_display_blitter_s *get_blitter(stm_display_device_t *dev, ULONG blitterID)
{
  CDisplayDevice* pDev = (CDisplayDevice*)(dev->handle);
  struct stm_display_blitter_s *public_blitter = 0;
  CGAL *real_blitter;

  if(blitterID >= maxBlitters)
    return 0;

  if(g_pIOS->DownSemaphore(dev->lock) != 0)
    return 0;

  real_blitter = pDev->GetGAL(blitterID);
  if(!real_blitter)
    goto exit;

  public_blitter = new struct stm_display_blitter_s;
  if(!public_blitter)
    goto exit;

  public_blitter->handle = (ULONG)real_blitter;
  public_blitter->owner  = dev;
  public_blitter->lock   = blitterLocks[blitterID];
  public_blitter->ops    = &stm_display_blitter_ops;

exit:
  g_pIOS->UpSemaphore(dev->lock);

  return public_blitter;
}


static void update_display(stm_display_device_t *dev, struct stm_display_output_s *output)
{
  CDisplayDevice* pDev = (CDisplayDevice*)(dev->handle);

  pDev->UpdateDisplay((COutput *)(output->handle));
}


static void display_device_release(stm_display_device_t *dev)
{
  struct private_display_device_s *private_dev = (struct private_display_device_s*)dev;

  g_pIOS->DownSemaphore(dev->lock);

  ASSERTF((private_dev->use_count > 0),("%s: Too many release calls\n",__FUNCTION__));

  private_dev->use_count--;

  if(private_dev->use_count > 0)
  {
    g_pIOS->UpSemaphore(dev->lock);
    return;
  }

  delete (CDisplayDevice *)dev->handle;
  dev->handle = 0;

  numDevices--;

  g_pIOS->DeleteSemaphore(dev->lock);
  dev->lock = 0;

  if(numDevices == 0)
  {
    for(unsigned int i=0;i<maxBlitters;i++)
      g_pIOS->DeleteSemaphore(blitterLocks[i]);

    g_pIDebug->Release();
    g_pIOS->Release();
  }
}


static const stm_display_device_ops_t ops = {
  GetNumOutputs : get_num_outputs,
  GetOutput     : get_output,
  GetPlane      : get_plane,
  GetBlitter    : get_blitter,
  Update        : update_display,
  Release       : display_device_release
};


static int device_initialize(unsigned id)
{
  CDisplayDevice *pDev = 0;

  if(numDevices == 0)
  {
    if(g_pIOS->Init()<0)
      return -1;

    if(g_pIDebug->Init()<0)
      return -1;

    for(unsigned int i=0;i<maxBlitters;i++)
    {
      blitterLocks[i] = g_pIOS->CreateSemaphore(1);
      if(blitterLocks[i] == 0)
      {
        DEBUGF2(1,("DevfbInit: create blitter lock semaphore failed\n"));
        goto error_exit;
      }
    }
  }

  theDevices[id].public_dev.lock = g_pIOS->CreateSemaphore(0); // Create mutex already held

  if(theDevices[id].public_dev.lock == 0)
  {
    DEBUGF2(1,("DevfbInit: create device lock semaphore failed\n"));
    goto error_exit;
  }

  if((pDev = AnonymousCreateDevice(id)) == 0)
  {
    DEBUGF2(1,("DevfbInit: create device object failed\n"));
    goto error_exit;
  }

  if(!pDev->Create())
  {
    DEBUGF2(1,("DevfbInit: failed to complete device creation\n"));
    goto error_exit;
  }

  theDevices[id].public_dev.ops = &ops;
  theDevices[id].public_dev.handle = (ULONG)pDev;
  numDevices++;

  return 0;

error_exit:
  delete pDev;

  if(theDevices[id].public_dev.lock)
  {
    g_pIOS->DeleteSemaphore(theDevices[id].public_dev.lock);
    theDevices[id].public_dev.lock = 0;
  }

  if(numDevices == 0)
  {
    for(unsigned int i=0;i<maxBlitters;i++)
    {
      if(blitterLocks[i])
      {
        g_pIOS->DeleteSemaphore(blitterLocks[i]);
        blitterLocks[i] = 0;
      }
    }

    g_pIDebug->Release();
    g_pIOS->Release();
  }

  return -1;
}


stm_display_device_t *stm_display_get_device(unsigned id)
{
  if(id>=maxDevices)
    return 0;

  if(theDevices[id].public_dev.lock == 0)
  {
    if(device_initialize(id) < 0)
      return 0;
  }
  else
  {
    if(g_pIOS->DownSemaphore(theDevices[id].public_dev.lock) != 0)
      return 0;
  }

  theDevices[id].use_count++;

  g_pIOS->UpSemaphore(theDevices[id].public_dev.lock);

  return &theDevices[id].public_dev;
}


////////////////////////////////////////////////////////////////
// C interface for the blitter.
//

static int blit(stm_display_blitter_t         *blitter,
                const stm_blitter_operation_t *op,
                const stm_rect_t              *srcrect,
                const stm_rect_t              *dstrect)
{
  int ret=0;
  CGAL*	pGAL = (CGAL*)(blitter->handle);
  bool needsresize = false;
  bool subbyteformat = false;

  if(g_pIOS->DownSemaphore(blitter->lock) != 0)
    return -1;

  if (((srcrect->bottom - srcrect->top) != (dstrect->bottom - dstrect->top)) ||
      ((srcrect->right - srcrect->left) != (dstrect->right - dstrect->left)))
  {
    needsresize = true;
  }

  if ((op->srcSurface.format == SURF_CLUT1) ||
      (op->srcSurface.format == SURF_CLUT2) ||
      (op->srcSurface.format == SURF_CLUT4) ||
      (op->srcSurface.format == SURF_A1))
  {
    subbyteformat = true;
  }

  if(op->ulFlags == 0 && !subbyteformat && !needsresize && (op->srcSurface.format == op->dstSurface.format))
  {
    stm_point_t srcLocation = { srcrect->left, srcrect->top };

    if (!pGAL->CopyRect(*op, *dstrect, srcLocation))
    {
      DEBUGF2(1,("pGAL->CopyRect failed\n"));
      ret = -1;
    }
  }
  else
  {
    if (!pGAL->CopyRectComplex(*op, *dstrect, *srcrect))
    {
      DEBUGF2(1,("pGAL->CopyRectComplex failed\n"));
      ret = -1;
    }
  }

  g_pIOS->UpSemaphore(blitter->lock);

  return ret;
}


static int draw_rect(stm_display_blitter_t         *blitter,
                     const stm_blitter_operation_t *op,
                     const stm_rect_t              *srcrect)
{
  int ret=0;
  CGAL*	pGAL = (CGAL*)(blitter->handle);

  if(g_pIOS->DownSemaphore(blitter->lock) != 0)
    return -1;

  if(!pGAL->DrawRect(*op, *srcrect))
    ret = -1;

  g_pIOS->UpSemaphore(blitter->lock);

  return ret;
}


static int fill_rect(stm_display_blitter_t         *blitter,
                     const stm_blitter_operation_t *op,
                     const stm_rect_t              *srcrect)
{
  int ret=0;
  CGAL*	pGAL = (CGAL*)(blitter->handle);

  if(g_pIOS->DownSemaphore(blitter->lock) != 0)
    return -1;

  if(!pGAL->FillRect(*op, *srcrect))
    ret= -1;

  g_pIOS->UpSemaphore(blitter->lock);

  return ret;
}


static STMFBBDispSharedAreaPriv *get_shared_area(stm_display_blitter_t *blitter)
{
  CGAL*	pGAL = (CGAL*)(blitter->handle);
  STMFBBDispSharedAreaPriv *shared;

  if(g_pIOS->DownSemaphore(blitter->lock) != 0)
    return 0;

  shared = pGAL->GetSharedArea();

  g_pIOS->UpSemaphore(blitter->lock);

  return shared;
}


static unsigned long get_blt_load(stm_display_blitter_t *blitter)
{
  CGAL*	pGAL = (CGAL*)(blitter->handle);

  return pGAL->GetBlitLoad();
}


static int sync_blitter(stm_display_blitter_t *blitter,
                        int                    wait_next_only)
{
  int ret;
  CGAL*	pGAL = (CGAL*)(blitter->handle);

  if(g_pIOS->DownSemaphore(blitter->lock) != 0)
    return -1;

  ret = pGAL->SyncChip(wait_next_only ? true : false);

  g_pIOS->UpSemaphore(blitter->lock);

  return ret;
}


static int handle_blitter_interrupt(stm_display_blitter_t *blitter)
{
  CGAL*	pGAL = (CGAL*)(blitter->handle);

  return pGAL->HandleBlitterInterrupt();
}


static void release_blitter(stm_display_blitter_t *blitter)
{
  delete blitter;
}


const stm_display_blitter_ops_t stm_display_blitter_ops = {
  Blit            : blit,
  DrawRect        : draw_rect,
  FillRect        : fill_rect,
  Sync            : sync_blitter,
  GetBlitLoad     : get_blt_load,
  GetSharedArea   : get_shared_area,
  HandleInterrupt : handle_blitter_interrupt,
  Release         : release_blitter
};


} // extern "C"
