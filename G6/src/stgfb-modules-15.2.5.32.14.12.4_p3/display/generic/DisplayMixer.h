/***********************************************************************
 *
 * File: display/generic/DisplayMixer.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _DISPLAY_MIXER_H
#define _DISPLAY_MIXER_H

#include "DisplayPlane.h"

/*! \file DisplayMixer.h
 *  \brief This is a pure virtual interface to a hardware mixer
 *
 *  Actually there is only one class that subclasses this, CGammaMixer, but
 *  providing the interface makes it easier to implement some of the
 *  more abstract output implementation classes without having to refer to the
 *  hardware specific code. I suppose at some point in the future we could have
 *  another hardware implementation.
 */
class CDisplayDevice;
class CDisplayPlane;

typedef enum
{
  /*
   * To be interpreted as an or'd bit field, hence the explicit values
   */
  STM_MA_NONE     = 0,
  STM_MA_GRAPHICS = 1,
  STM_MA_VIDEO    = 2,
  STM_MA_ALL      = 3
} stm_mixer_activation_t;


class CDisplayMixer
{
public:
  CDisplayMixer(void) {}
  virtual ~CDisplayMixer() {};

  virtual bool Start(const stm_display_mode_t *) = 0;
  virtual void Stop(void) = 0;

  virtual void Suspend(void) = 0;
  virtual void Resume(void) = 0;

  virtual bool PlaneValid(const CDisplayPlane *) const = 0;

  virtual uint32_t GetCapabilities(void) const = 0;
  virtual uint32_t SetControl(stm_output_control_t, uint32_t newVal) = 0;
  virtual uint32_t GetControl(stm_output_control_t, uint32_t *val) const = 0;

  virtual bool EnablePlane(const CDisplayPlane *, stm_mixer_activation_t act = STM_MA_NONE) = 0;
  virtual bool DisablePlane(const CDisplayPlane *) = 0;

  virtual bool HasEnabledPlanes(void) const = 0;

  virtual bool SetPlaneDepth(const CDisplayPlane *, int depth, bool activate) = 0;
  virtual bool GetPlaneDepth(const CDisplayPlane *, int *depth) const = 0;

  virtual bool UpdateFromOutput(COutput* pOutput, stm_plane_update_reason_t update_reason) = 0;

private:
  CDisplayMixer(const CDisplayMixer&);
  CDisplayMixer& operator=(const CDisplayMixer&);
};

#endif /* _DISPLAY_MIXER_H */
