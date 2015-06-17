/***********************************************************************
 *
 * File: display/generic/DisplayDevice.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef DISPLAYDEVICE_H
#define DISPLAYDEVICE_H

#include <vibe_os.h>
#include "stm_display_plane.h"
#include "stm_display_test_tuning.h"

#define VALID_DEVICE_HANDLE	       0x01230123

class CDisplayDevice;
class CDisplaySource;
class CDisplayPlane;
class COutput;


enum TuningResults {
  TUNING_OK,
  TUNING_SERVICE_NOT_SUPPORTED,
  TUNING_SERVICE_NOT_AVAILABLE,
  TUNING_INVALID_PARAMETER,
  TUNING_EINTR,
  TUNING_NO_DATA_AVAILABLE
};


enum DevicePowerState {
  DEVICE_ACTIVE,
  DEVICE_ACTIVE_STANDBY,
  DEVICE_SUSPENDED,
  DEVICE_FROZEN,
  DEVICE_SHUTTING_DOWN
};

struct stm_display_device_pm_s
{
  uint32_t pid;
  /* device power state */
  uint32_t state;
  void *   spinLock;    /* SpinLock dedicated exclusively to the protection of pm.state */

  int (*runtime_get)(const uint32_t pid);
  int (*runtime_put)(const uint32_t pid);
};

struct stm_display_device_s
{
  CDisplayDevice * handle;
  void           * mutex_lock;
  uint32_t         magic;

  vibe_time64_t    lock_time;
  vibe_time64_t    unlock_time;

  /* device power struct */
  struct  stm_display_device_pm_s pm;
};


class CDisplayDevice
{
public:
  CDisplayDevice(uint32_t id, int nOutputs, int nPlanes, int nSources);
  virtual ~CDisplayDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  virtual bool Create();

  uint32_t    GetID(void)            const { return m_ID; }

  // Access to outputs
  uint32_t GetNumberOfOutputs(void)  const { return m_numOutputs; }
  COutput* GetOutput(uint32_t index) const { return (index<m_numOutputs)?m_pOutputs[index]:0; }

  int FindOutputsWithCapabilities(
                               stm_display_output_capabilities_t  caps_match,
                               stm_display_output_capabilities_t  caps_mask,
                               uint32_t*                          id,
                               uint32_t                           max_ids);
  int FindPlanesWithCapabilities(
                               stm_plane_capabilities_t  caps_match,
                               stm_plane_capabilities_t  caps_mask,
                               uint32_t*                 id,
                               uint32_t                  max_ids);

  // Control registers information
  void* GetCtrlRegisterBase() const { return m_pReg; }

  // Request to use a device specific plane
  uint32_t        GetNumberOfPlanes(void)  const { return m_numPlanes; }
  CDisplayPlane*  GetPlane(uint32_t index) const { return (index<m_numPlanes)?m_pPlanes[index]:0; }

  // Access to sources
  uint32_t        GetNumberOfSources(void)  const { return m_numSources; }
  CDisplaySource* GetSource(uint32_t index) const { return (index<m_numSources)?m_pSources[index]:0; }

  uint16_t GetNumTuningServices(void) const { return m_nNumTuningServices; }
  const stm_device_tuning_caps_t *GetTuningCaps(void) const { return m_pTuningCaps; }


  virtual TuningResults SetTuning(uint16_t service,
                                  void *inputList,
                                  uint32_t inputListSize,
                                  void *outputList,
                                  uint32_t outputListSize);

  /*
   * Called after VSYNC interrupt to update the display for the specified output.
   * This is done on CDisplayDevice so that the correct processing can be done
   * for all hardware clocked by a specific timing generator.
   */
  void OutputVSyncIrqUpdateHw(uint32_t timingID);
  void OutputVSyncThreadedIrqUpdateHw(uint32_t timingID);

  void UpdateSource(uint32_t timingID);

  /*
   * Power Management stuff.
   */
  void        PowerDownVideoDacs(void);
  virtual int Freeze(void);
  virtual int Suspend(void);
  virtual int Resume(void);

  int         VSyncLock(void) const;
  void        VSyncUnlock(void) const;
  void        DebugCheckVSyncLockTaken(const char *function_name) const;

protected:

  uint32_t               m_ID;
  COutput              **m_pOutputs;    // Available device outputs
  uint32_t               m_numOutputs;  // Number of outputs
  CDisplayPlane        **m_pPlanes;     // Available Planes
  uint32_t               m_numPlanes;   // Number of Planes
  CDisplaySource       **m_pSources;    // Available Sources
  uint32_t               m_numSources;  // Number of Sources

  uint16_t                  m_nNumTuningServices;
  stm_device_tuning_caps_t *m_pTuningCaps;

  volatile uint32_t m_CurrentVTGSync;

  // IO mapped pointer to the start of the register block
  uint32_t* m_pReg;

  // Power Managment state
  bool               m_bIsSuspended; // Has the HW been powered off
  bool               m_bIsFrozen;

  void              *m_vsyncLock;   // semaphore for protecting sources and planes shared datas from stkpi and threaded_irq context

  void WriteDevReg(uint32_t reg, uint32_t val) { vibe_os_write_register(m_pReg, reg, val); }
  uint32_t ReadDevReg(uint32_t reg) { return vibe_os_read_register(m_pReg, reg); }

  /*
   * Helper for device creation code to add an output to the device's list of
   * outputs. However it does a little more than that, calling the output's
   * create function and if there is any failure destroying the object for you.
   *
   * This means you can use the form if(!AddOutput(new MyOutputClass(...))) {}
   */
  bool AddOutput(COutput *);
  bool AddPlane(CDisplayPlane *);
  bool AddSource(CDisplaySource *);

  void RemoveSources(void);
  void RemovePlanes(void);
  void RemoveOutputs(void);

  /*
   * Helper for stm_registry management code to add coredisplay objects to the
   * registry's database.
   */
  void RegistryInit(void);
  void RegistryTerm(void);

}; /* CDisplayDevice */


/*
 * There will be one device specific implementation of this function in each
 * valid driver configuration. This is how you get hold of the device object
 */
extern CDisplayDevice* AnonymousCreateDevice(unsigned deviceid);

#endif /* DISPLAYDEVICE_H */
