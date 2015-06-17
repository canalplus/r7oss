/***********************************************************************
 *
 * File: display/ip/gdp/VBIPlane.h
 * Copyright (c) 2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _VBI_PLANE_H
#define _VBI_PLANE_H

#include "GdpPlane.h"

struct ScalingContext {
    stm_rect_t     SrcVisibleArea;
    stm_rational_t SrcPixelAspectRatio;
    bool           IsSrcInterlaced;

    stm_rect_t     DisplayVisibleArea;
    stm_rational_t DisplayPixelAspectRatio;
    bool           IsDisplayInterlaced;

    /* Info concerning the source 3D mode */
    stm_display_3d_format_t source_3D_format;

    /* Info concerning the display 3D mode */
    uint32_t       display_3D_mode_flags;
};

class CVBIPlane: public CGdpPlane
{
  public:
    CVBIPlane(const char          *name,
              uint32_t             id,
              const stm_plane_capabilities_t caps,
              CGdpPlane*linktogdp);

    virtual void PresentDisplayNode(CDisplayNode *pPrevNode,
                                    CDisplayNode *pCurrNode,
                                    CDisplayNode *pNextNode,
                                    bool isPictureRepeated,
                                    bool isInterlaced,
                                    bool isTopFieldOnDisplay,
                                    const stm_time64_t &vsyncTime);

protected:
    virtual bool GetScalingContext(const InputFrameInfo &frame, ScalingContext &scalingContext) const;
    virtual bool PrepareIOWindows(CDisplayNode * pNodeToDisplay, CDisplayInfo *pDisplayInfo);

    virtual bool setOutputViewport(GENERIC_GDP_LLU_NODE          &topNode,
                                   GENERIC_GDP_LLU_NODE          &botNode);

    virtual void createDummyNode(GENERIC_GDP_LLU_NODE *);

    virtual bool GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range);

  private:
    bool m_bIsTypeB;
    CVBIPlane(const CVBIPlane&);
    CVBIPlane& operator=(const CVBIPlane&);
};

#endif // _VBI_PLANE_H
