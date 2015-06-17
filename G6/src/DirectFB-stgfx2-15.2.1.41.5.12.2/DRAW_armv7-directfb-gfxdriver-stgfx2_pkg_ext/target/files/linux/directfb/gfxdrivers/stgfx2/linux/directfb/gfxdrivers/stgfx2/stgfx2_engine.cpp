/*
   ST Microelectronics BDispII driver - the DirectFB interface

   (c) Copyright 2013              STMicroelectronics Ltd.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <errno.h>

#include <directfb.h>

extern "C" {
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include "stm_gfxdriver.h"
#include "directfb_state_glue.h"
#include "bdisp2_directfb_glue.h"
}

#include "stgfx2_engine.h"


D_DEBUG_DOMAIN (STGFX2_Engine, "STGFX2/Engine", "STGFX2 Drawing Engine Engine");
D_DEBUG_DOMAIN (STGFX2_Task,   "STGFX2/Task",   "STGFX2 Drawing Engine Task");



extern "C" {
  void
  register_stgfx2 (struct _STGFX2DriverData *stdrv,
                   struct _STGFX2DeviceData *stdev)
  {
    if (dfb_config->task_manager)
      {
        STM::STGFX2Engine *engine = new STM::STGFX2Engine (stdrv, stdev);
        DirectFB::Renderer::RegisterEngine (engine);
      }
  }
}

namespace STM {

STGFX2Task::STGFX2Task (STGFX2Engine *engine)
  :
  SurfaceTask (CSAID_GPU),
  engine (engine )
{
  D_DEBUG_AT (STGFX2_Task, "STGFX2Task::%s( %p )\n", __func__, this);
}

DFBResult
STGFX2Task::Push ()
{
  D_DEBUG_AT (STGFX2_Task, "STGFX2Task::%s( %p )\n", __func__, this);

  bdisp_aq_EmitCommands (engine->stdrv, engine->stdev);

  /* fixme: we somehow need to call Done() when the job has finished. Since
     we have no way to do so via notification, we wait for idle instead. */
  bdisp_aq_EngineSync (engine->stdrv, engine->stdev);
  Done ();

  return DFB_OK;
}

STGFX2Engine::STGFX2Engine (struct _STGFX2DriverData *stdrv,
                            struct _STGFX2DeviceData *stdev)
  :
  stdrv( stdrv ),
  stdev( stdev )
{
  D_DEBUG_AT (STGFX2_Engine, "STGFX2Engine::%s()\n", __func__);

  caps.cores = 1;
  caps.clipping = DFXL_NONE;

  caps.render_options = (DFBSurfaceRenderOptions)(DSRO_SMOOTH_DOWNSCALE
                                                  | DSRO_SMOOTH_UPSCALE
                                                  | DSRO_MATRIX
                                                  | DSRO_ANTIALIAS // FIXME
                                                 );
  caps.max_scale_down_x = 63;
  caps.max_scale_down_y = 63;

  caps.max_operations = 1000; /* random number, but larger than node list */
}

DFBResult
STGFX2Engine::bind (DirectFB::Renderer::Setup *setup)
{
  D_DEBUG_AT (STGFX2_Engine, "STGFX2Engine::%s ()\n", __func__);

  for (unsigned int i = 0; i < setup->tiles; ++i)
    {
      setup->tasks[i] = new STGFX2Task (this);
    }

  return DFB_OK;
}

DFBResult
STGFX2Engine::check (DirectFB::Renderer::Setup *setup)
{
  D_DEBUG_AT( STGFX2_Engine, "STGFX2Engine::%s()\n", __func__ );

  for (unsigned int i = 0; i < setup->tiles; ++i)
    {
    }

  return DFB_OK;
}

DFBResult
STGFX2Engine::CheckState (CardState           *state,
                          DFBAccelerationMask  accel)
{
  DFBAccelerationMask state_accel = state->accel;
  DFBResult res = DFB_OK;

  D_DEBUG_AT (STGFX2_Engine, "STGFX2Engine::%s()\n", __func__);

  bdisp_aq_CheckState (stdrv, stdev, state, accel);

  if ((accel & DFXL_FILLRECTANGLE)
      && !(state->accel & DFXL_FILLRECTANGLE))
       res = DFB_UNSUPPORTED;
  if ((accel & DFXL_DRAWRECTANGLE)
      && !(state->accel & DFXL_DRAWRECTANGLE))
       res = DFB_UNSUPPORTED;
  if ((accel & DFXL_BLIT)
      && !(state->accel & DFXL_BLIT))
       res = DFB_UNSUPPORTED;
  if ((accel & DFXL_STRETCHBLIT)
      && !(state->accel & DFXL_STRETCHBLIT))
       res = DFB_UNSUPPORTED;
  if ((accel & DFXL_BLIT2)
      && !(state->accel & DFXL_BLIT2))
       res = DFB_UNSUPPORTED;

  state->accel = state_accel;

  return res;
}

DFBResult
STGFX2Engine::SetState (DirectFB::SurfaceTask  *task,
                        CardState              *state,
                        StateModificationFlags  modified,
                        DFBAccelerationMask     accel)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *)task;

  (void) modified;

  D_DEBUG_AT ( STGFX2_Engine,
               "STGFX2Engine::%s (%p, 0x%08x) <- modified 0x%08x\n",
               __func__, state, accel, modified);

  bdisp_aq_SetState (stdrv, stdev, NULL, state, accel);

  return DFB_OK;
}

DFBResult
STGFX2Engine::FillRectangles (DirectFB::SurfaceTask *task,
                              const DFBRectangle    *drects,
                              unsigned int           num_rects)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *) task;

  D_DEBUG_AT (STGFX2_Engine,
              "STGFX2Engine::%s (%d)\n", __func__, num_rects);

  for (unsigned int i = 0; i < num_rects; ++i)
    if (!bdisp_aq_FillRectangle (stdrv, stdev, (DFBRectangle *) &drects[i]))
      return DFB_UNSUPPORTED;

  return DFB_OK;
}

DFBResult
STGFX2Engine::DrawRectangles (DirectFB::SurfaceTask *task,
                              const DFBRectangle    *drects,
                              unsigned int           num_rects)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *) task;

  D_DEBUG_AT (STGFX2_Engine,
              "STGFX2Engine::%s (%d)\n", __func__, num_rects);

  for (unsigned int i = 0; i < num_rects; ++i)
    if (!bdisp_aq_DrawRectangle (stdrv, stdev, (DFBRectangle *) &drects[i]))
      return DFB_UNSUPPORTED;

  return DFB_OK;
}

DFBResult
STGFX2Engine::Blit (DirectFB::SurfaceTask *task,
                    const DFBRectangle    *srects,
                    const DFBPoint        *dpoints,
                    unsigned int           num)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *) task;

  D_DEBUG_AT (STGFX2_Engine,
              "STGFX2Engine::%s (%d)\n", __func__, num);

  for (unsigned int i = 0; i < num; ++i)
    if (!bdisp_aq_Blit (stdrv, stdev,
                        (DFBRectangle *) &srects[i],
                        dpoints[i].x, dpoints[i].y))
      return DFB_UNSUPPORTED;

  return DFB_OK;
}

DFBResult
STGFX2Engine::Blit2 (DirectFB::SurfaceTask *task,
                     const DFBRectangle    *srects,
                     const DFBPoint        *dpoints,
                     const DFBPoint        *s2points,
                     u32                    num)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *) task;

  D_DEBUG_AT (STGFX2_Engine,
              "STGFX2Engine::%s (%d)\n", __func__, num);

  for (unsigned int i = 0; i < num; ++i)
    if (!bdisp_aq_Blit2 (stdrv, stdev,
                         (DFBRectangle *) &srects[i],
                         dpoints[i].x, dpoints[i].y,
                         s2points[i].x, s2points[i].y))
      return DFB_UNSUPPORTED;

  return DFB_OK;
}

DFBResult
STGFX2Engine::StretchBlit (DirectFB::SurfaceTask *task,
                           const DFBRectangle    *srects,
                           const DFBRectangle    *drects,
                           u32                    num)
{
  __attribute__((unused)) STGFX2Task *mytask = (STGFX2Task *) task;

  D_DEBUG_AT (STGFX2_Engine,
              "STGFX2Engine::%s (%d)\n", __func__, num);

  for (unsigned int i = 0; i < num; ++i)
    if (!bdisp_aq_StretchBlit (stdrv, stdev,
                               (DFBRectangle *) &srects[i],
                               (DFBRectangle *) &drects[i]))
      return DFB_UNSUPPORTED;

  return DFB_OK;
}


}
