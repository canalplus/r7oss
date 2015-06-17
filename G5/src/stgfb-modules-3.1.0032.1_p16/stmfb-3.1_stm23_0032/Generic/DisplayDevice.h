/***********************************************************************
 * 
 * File: Generic/DisplayDevice.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef DISPLAYDEVICE_H
#define DISPLAYDEVICE_H

#include "stmdisplayplane.h"

// Maximum number of outputs to cover Dual TV, Main+Aux (VCR) + DVO
#define CDISPLAYDEVICE_MAX_OUTPUTS 5

class CGAL;

class CDisplayPlane;
class COutput;

class CDisplayDevice
{
public:
  CDisplayDevice();
  virtual ~CDisplayDevice();

  // Create Device specific resources after initial mappings have
  // been completed.
  virtual bool Create();

  // Access to outputs
  ULONG    GetNumOutputs(void)    const { return m_nOutputs; }
  COutput* GetOutput(ULONG index) const { return (index<m_nOutputs)?m_pOutputs[index]:0; }
  
  // Control registers information
  virtual void* GetCtrlRegisterBase() const  = 0;

  // Functions to get a graphics accelerator associated with this device
  virtual CGAL* GetGAL(ULONG id) const = 0;

  // Request to use a device specific plane
  virtual CDisplayPlane* GetPlane(stm_plane_id_t num) const = 0;

  /*
   * Called after VSYNC interrupt to update the display for the specified output.
   * This is done on CDisplayDevice, rather than directly on COutput so that
   * the correct processing can be done on multi-pipeline, multi-output devices.
   */
  virtual void UpdateDisplay(COutput *) = 0;

  virtual void UnlockPlanes(COutput *) = 0;

protected:
  COutput*  m_pOutputs[CDISPLAYDEVICE_MAX_OUTPUTS]; // Available device outputs
  ULONG     m_nOutputs;                             // Number of available outputs
  
  volatile ULONG m_CurrentVTGSync;
}; /* CDisplayDevice */


/*
 * There will be one device specific implementation of this function in each
 * valid driver configuration. This is how you get hold of the device object
 */
extern CDisplayDevice* AnonymousCreateDevice(unsigned deviceid);

#endif /* DISPLAYDEVICE_H */
