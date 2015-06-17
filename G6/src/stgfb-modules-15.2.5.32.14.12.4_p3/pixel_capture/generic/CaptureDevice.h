/***********************************************************************
 *
 * File: pixel_capture/generic/CaptureDevice.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _PIXEL_CAPTURE_DEVICE_H__
#define _PIXEL_CAPTURE_DEVICE_H__

#include <stm_display_types.h>

#include <vibe_os.h>
#include <vibe_debug.h>

#include <stm_pixel_capture.h>
#include <capture_device_priv.h>

class CCapture;


struct stm_pixel_capture_pm_s
{
  uint32_t id;
  uint32_t state;
  int (*runtime_get)(const uint32_t type, const uint32_t id);
  int (*runtime_put)(const uint32_t type, const uint32_t id);
};

struct stm_pixel_capture_s
{
  uint32_t                     handle;
  uint32_t                     type;
  void                       * lock;
  struct stm_pixel_capture_s * owner;

  /* device power struct */
  struct stm_pixel_capture_pm_s pm;
};

enum DevicePowerState {
  CAPTURE_DEVICE_ACTIVE,
  CAPTURE_DEVICE_ACTIVE_STANDBY,
  CAPTURE_DEVICE_SUSPENDED,
  CAPTURE_DEVICE_FROZEN
};

class CPixelCaptureDevice
{
public:
  CPixelCaptureDevice(stm_pixel_capture_device_type_t type);
  virtual ~CPixelCaptureDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  virtual bool Create(uint32_t base_address,uint32_t size, stm_pixel_capture_hw_features_t hw_features);

  // Access to Captures instances
  uint32_t  GetNumberOfCaptures(void)  const { return m_numCaptures; }
  CCapture* GetCapture(uint32_t index) const { return (index<m_numCaptures)?m_pCaptures[index]:0; }

  /*
   * Called after VSYNC interrupt to update the capture for the specified input.
   * This is done on CCapture so that the correct processing can be done
   * for all hardware clocked by a specific timing generator.
   */
  void UpdateCaptureDevice(uint32_t timingID, stm_pixel_capture_time_t vsyncTime, uint32_t timingevent);

  /*
   * Power Management stuff.
   */
  virtual int Freeze(void);
  virtual int Suspend(void);
  virtual int Resume(void);

protected:

  CCapture             **m_pCaptures;    // Available capture instances
  uint32_t               m_numCaptures;  // Number of Captures

  stm_pixel_capture_device_type_t m_Type;             // Capture device type (FVDP-CAP or COMPO-CAP)

  stm_pixel_capture_time_t        m_LastVTGEventTime;

  /*
   * Helper for capture creation code to add an input to the device's list of
   * inputs.
   */
  bool AddCapture(CCapture *);
  void RemoveCaptures(void);

  /*
   * Helper for stm_registry management code to add capture objects to the
   * registry's database.
   */
  void RegistryInit(void);
  void RegistryTerm(void);

  virtual bool CreateCaptures(uint32_t base_address,uint32_t size,stm_pixel_capture_hw_features_t hw_features);

  // Power Managment state
  bool               m_bIsSuspended; // Has the HW been powered off
  bool               m_bIsFrozen;
}; /* CPixelCaptureDevice */


/*
 * There will be one device specific implementation of this function in each
 * valid driver configuration. This is how you get hold of the device object
 */
extern CPixelCaptureDevice* AnonymousCreateCaptureDevice(stm_pixel_capture_device_type_t type);

#endif /* _PIXEL_CAPTURE_DEVICE_H__ */
