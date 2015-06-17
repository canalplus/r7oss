/***********************************************************************
 *
 * File: Gamma/GammaCompositorNULL.h
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _GAMMA_COMPOSITOR_NULL_H
#define _GAMMA_COMPOSITOR_NULL_H

#include "GammaCompositorPlane.h"

class CGammaCompositorNULL: public CGammaCompositorPlane
{
  public:

    CGammaCompositorNULL(stm_plane_id_t NULLid);
    ~CGammaCompositorNULL();

    virtual bool Create(void);

    virtual bool QueueBuffer(const stm_display_buffer_t * const pBuffer,
                             const void                 * const user);

    virtual void UpdateHW (bool isDisplayInterlaced,
                           bool isTopFieldOnDisplay,
                           const TIME64 &vsyncTime);

    virtual bool SetControl(stm_plane_ctrl_t control, ULONG value);
    virtual bool GetControl(stm_plane_ctrl_t control, ULONG *value) const;

    void EnableHW(void);

  protected:

  private:
    CGammaCompositorNULL(const CGammaCompositorNULL&);
    CGammaCompositorNULL& operator=(const CGammaCompositorNULL&);
};


#endif // _GAMMA_COMPOSITOR_NULL_H
