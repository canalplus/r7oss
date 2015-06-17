/***********************************************************************
 *
 * File: Gamma/DEIVideoPipeV2.h
 * Copyright (c) 2008-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _DEI_VIDEO_PIPE_V2_H
#define _DEI_VIDEO_PIPE_V2_H

#include <STMCommon/stmiqi.h>

#include "DEIVideoPipe.h"

/*
 * This class represents a second revision of the DEI video pipeline that
 * has a changed control register layout and has a second generation IQI
 * block after the re-scale.
 */
struct DEIVideoPipeV2Setup
{
  struct DEISetup dei_setup; /* must be first! */
  struct IQISetup iqi_setup;
};


class CDEIVideoPipeV2: public CDEIVideoPipe
{
public:
  CDEIVideoPipeV2 (stm_plane_id_t        GDPid,
                   CGammaVideoPlug      *plug,
                   ULONG                 dei_base);

  virtual ~CDEIVideoPipeV2 (void);

  bool Create (void);

  bool QueuePreparedNode (const stm_display_buffer_t    * const pFrame,
                          const GAMMA_QUEUE_BUFFER_INFO &qbi,
                          struct DEIVideoPipeV2Setup    *node);

  bool QueueBuffer (const stm_display_buffer_t * const pFrame,
                    const void                 * const user);

  void UpdateHW (bool          isDisplayInterlaced,
                 bool          isTopFieldOnDisplay,
                 const TIME64 &vsyncTime);

  stm_plane_caps_t GetCapabilities (void) const;

  bool SetControl (stm_plane_ctrl_t control, ULONG  value);
  bool GetControl (stm_plane_ctrl_t control, ULONG *value) const;

private:
  CSTmIQI *m_IQI;

  CDEIVideoPipeV2(const CDEIVideoPipeV2&);
  CDEIVideoPipeV2& operator=(const CDEIVideoPipeV2&);

};


#endif /* _DEI_VIDEO_PIPE_V2_H */
