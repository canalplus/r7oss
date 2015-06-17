/***********************************************************************
 * 
 * File: include/stmdisplaydevice.h
 * Copyright (c) 2006 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_DISPLAY_DEVICE_H
#define STM_DISPLAY_DEVICE_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct stm_display_device_s stm_display_device_t;

typedef struct
{
  int                           (*GetNumOutputs)(stm_display_device_t *);
  struct stm_display_output_s  *(*GetOutput)    (stm_display_device_t *, ULONG outputID);
  struct stm_display_plane_s   *(*GetPlane)     (stm_display_device_t *, ULONG planeID);
  struct stm_display_blitter_s *(*GetBlitter)   (stm_display_device_t *, ULONG blitterID);  

  void (*Update)(stm_display_device_t *, struct stm_display_output_s *);

  void (*Release)(stm_display_device_t *);

} stm_display_device_ops_t;


struct stm_display_device_s
{
  ULONG handle;
  ULONG lock;

  const stm_display_device_ops_t *ops;
};

/****************************************************************************
 * C Interface to the display driver core
 */

/* 
 * stm_display_device_t *stm_display_get_device(unsigned id)
 * 
 * Return a device instance, initializing the driver core if not already
 * done so. The first call for a particular device, which requires full
 * initialization is not completely thread safe, and this should be taken
 * into account by the user of the API.
 * 
 * Returns NULL on any error.
 *  
 */ 
extern stm_display_device_t *stm_display_get_device(unsigned id);

/*
 * int stm_display_get_number_of_outputs(stm_display_device_t *d)
 * 
 * A convenience function to return the number of outputs available on
 * the specified device. 
 * 
 * Returns -1 if the device lock cannot be obtained. Linux users should check
 * for signal delivery in this case.
 * 
 * Note: this interface may be removed, as outputs can be enumerated using
 * stm_display_get_output, increasing the id until the function returns NULL.
 * 
 */
static inline int stm_display_get_number_of_outputs(stm_display_device_t *d)
{
  return d->ops->GetNumOutputs(d);
}

/*
 * struct stm_display_output_s *stm_display_get_output(stm_display_device_t *d,
 *                                                     ULONG outputID)
 * 
 * Get an output instance for the given output identifier.
 * 
 * Returns NULL if the device lock cannot be obtained or the output identifier
 * does not exist on the given device.
 * 
 */
static inline struct stm_display_output_s *stm_display_get_output(stm_display_device_t *d, ULONG outputID)
{
  return d->ops->GetOutput(d,outputID);
}

/*
 * struct stm_display_plane_s *stm_display_get_plane(stm_display_device_t *d,
 *                                                   ULONG planeID)
 * 
 * Get a plane instance for the given plane identifier.
 *
 * Returns NULL if the device lock cannot be obtained or the plane identifier
 * does not exist on the given device.
 * 
 */
static inline struct stm_display_plane_s *stm_display_get_plane(stm_display_device_t *d, ULONG planeID)
{
  return d->ops->GetPlane(d,planeID);
}

/*
 * struct stm_display_blitter_s *stm_display_get_blitter(stm_display_device_t *d,
 *                                                       ULONG blitterID)
 * 
 * Get a blitter instance for the given blitter identifier.
 * 
 * Returns NULL if the device lock cannot be obtained or the blitter identifier
 * does not exist on the given device.
 * 
 */
static inline struct stm_display_blitter_s *stm_display_get_blitter(stm_display_device_t *d, ULONG blitterID)
{
  return d->ops->GetBlitter(d,blitterID);
}

/*
 * void stm_display_update(stm_display_device_t *d,
 *                         struct stm_display_output_s *o)
 * 
 * Update all hardware programming on the given device that is associated to
 * the given output instance, by virtue of sharing the output's video timing
 * generator, for the next video frame/field. This should only be called as a
 * result of a VSync interrupt for that timing generator.
 * 
 * Note that this call can result in memory allocated from the OS heap being
 * released. If that is illegal in interrupt context (e.g. under OS21) then
 * this must be called from task context instead.
 * 
 */
static inline void stm_display_update(stm_display_device_t *d, struct stm_display_output_s *o)
{
  d->ops->Update(d,o);
}

/*
 * void stm_display_release_device(stm_display_device_t *d)
 * 
 * Release the given device instance. This may cause the underlying driver
 * implementation to shutdown and release all its resources when the last
 * device instance is released.
 * 
 * The display device instance pointer is invalid after this call completes.
 */
static inline void stm_display_release_device(stm_display_device_t *d)
{
  if(d) {d->ops->Release(d);}
}


#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_DEVICE_H */
