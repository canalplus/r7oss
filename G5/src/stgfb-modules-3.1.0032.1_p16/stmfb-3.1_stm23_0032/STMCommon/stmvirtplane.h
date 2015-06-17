/***********************************************************************
 *
 * File: STMCommon/stmvirtplane.h
 * Copyright (c) 2007 STMicroelectronics Limited.
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_VIRT_PLANE_H
#define _STM_VIRT_PLANE_H

#include "Generic/DisplayPlane.h"

class CSTmBDispOutput;

class CSTmVirtualPlane: public CDisplayPlane
{
public:
  CSTmVirtualPlane(stm_plane_id_t GDPid, CSTmBDispOutput *);
  ~CSTmVirtualPlane();

  bool QueueBuffer(const stm_display_buffer_t * const pBuffer,
                   const void                 * const user);

  stm_plane_caps_t GetCapabilities(void) const;
  int GetFormats(const SURF_FMT** pData) const;

  bool SetControl(stm_plane_ctrl_t control, ULONG value);
  bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

  DMA_Area *UpdateHW (bool   isDisplayInterlaced,
                      bool   isTopFieldOnDisplay,
                      TIME64 vsyncTime);

protected:
  CSTmBDispOutput *m_pBDispOutput;

  ULONG m_ulAlphaRamp;

private:
  stm_plane_caps_t m_capabilities;

  CSTmVirtualPlane(const CSTmVirtualPlane&);
  CSTmVirtualPlane& operator=(const CSTmVirtualPlane&);
};

#endif // _STM_VIRT_PLANE_H
