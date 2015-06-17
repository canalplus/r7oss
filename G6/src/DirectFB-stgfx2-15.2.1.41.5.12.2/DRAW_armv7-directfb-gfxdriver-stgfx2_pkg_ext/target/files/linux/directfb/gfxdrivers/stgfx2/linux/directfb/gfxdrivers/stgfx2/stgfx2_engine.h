#ifndef __STGFX2_ENGINE_H__
#define __STGFX2_ENGINE_H__

#include "config.h"

#if !HAVE_DIRECTFB__ENGINE

static inline void register_stgfx2 (struct _STGFX2DriverData *stdrv,
                                    struct _STGFX2DeviceData *stdev) { }

#else /* HAVE_DIRECTFB__ENGINE */

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <directfb.h>


void register_stgfx2 (struct _STGFX2DriverData *stdrv,
                      struct _STGFX2DeviceData *stdev);


#ifdef __cplusplus
}

#include <core/Renderer.h>
#include <core/Util.h>


namespace STM {

class STGFX2Engine;

class STGFX2Task : public DirectFB::SurfaceTask
{
public:
     STGFX2Task( STGFX2Engine *engine );

protected:
     DFBResult Push();

private:
     friend class STGFX2Engine;

     STGFX2Engine   *engine;
};


class STGFX2Engine : public DirectFB::Engine {
private:
   struct _STGFX2DriverData *stdrv;
   struct _STGFX2DeviceData *stdev;

public:
   STGFX2Engine (struct _STGFX2DriverData *stdrv,
                 struct _STGFX2DeviceData *stdev);


   DFBResult bind  (DirectFB::Renderer::Setup *setup);
   DFBResult check (DirectFB::Renderer::Setup *setup);

   DFBResult CheckState (CardState           *state,
                         DFBAccelerationMask  accel);
   DFBResult SetState (DirectFB::SurfaceTask  *task,
                       CardState              *state,
                       StateModificationFlags  modified,
                       DFBAccelerationMask     accel);

   DFBResult FillRectangles (DirectFB::SurfaceTask *task,
                             const DFBRectangle    *drects,
                             unsigned int           num_rects);

   DFBResult DrawRectangles (DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *drects,
                             unsigned int            num_rects);

   DFBResult Blit (DirectFB::SurfaceTask  *task,
                   const DFBRectangle     *srects,
                   const DFBPoint         *dpoints,
                   unsigned int            num);

   DFBResult Blit2 (DirectFB::SurfaceTask  *task,
                    const DFBRectangle     *srects,
                    const DFBPoint         *dpoints,
                    const DFBPoint         *s2points,
                    u32                     num);

   DFBResult StretchBlit (DirectFB::SurfaceTask  *task,
                          const DFBRectangle     *srects,
                          const DFBRectangle     *drects,
                          u32                     num);

private:
  friend class STGFX2Task;
};


}

#endif // __cplusplus


#endif /* HAVE_DIRECTFB__ENGINE */


#endif /* __STGFX2_ENGINE_H__ */
