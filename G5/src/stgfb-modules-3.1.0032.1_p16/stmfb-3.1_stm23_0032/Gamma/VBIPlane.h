/***********************************************************************
 *
 * File: stgfb/Gamma/VBIPlane.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _VBI_PLANE_H
#define _VBI_PLANE_H

#include "GammaCompositorGDP.h"

class CVBIPlane: public CGammaCompositorGDP
{
  public:
    CVBIPlane(CGammaCompositorGDP *linktogdp, ULONG PKZVal);

    virtual stm_plane_caps_t GetCapabilities(void) const;

    virtual bool QueueBuffer(const stm_display_buffer_t * const pBuffer,
                             const void                 * const user);
protected:
    virtual bool setOutputViewport(GENERIC_GDP_LLU_NODE          &topNode,
                                   GENERIC_GDP_LLU_NODE          &botNode,
                                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    virtual void createDummyNode(GENERIC_GDP_LLU_NODE *);

  private:
    CVBIPlane(const CVBIPlane&);
    CVBIPlane& operator=(const CVBIPlane&);
};

#endif // _VBI_PLANE_H
