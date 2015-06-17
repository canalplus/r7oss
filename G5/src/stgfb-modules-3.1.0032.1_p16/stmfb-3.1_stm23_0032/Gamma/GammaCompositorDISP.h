/***********************************************************************
 *
 * File: Gamma/GammaCompositorDISP.h
 * Copyright (c) 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef _GAMMA_COMPOSITOR_DISP_H
#define _GAMMA_COMPOSITOR_DISP_H

#include "GammaCompositorVideoPlane.h"

struct DISPFieldSetup
{
    ULONG LUMA_BA; // This can be different on each field for 422R
    ULONG LUMA_XY;
    ULONG CHR_XY;
    ULONG LUMA_VSRC;
    ULONG CHR_VSRC;
    ULONG LUMA_SIZE;
    ULONG CHR_SIZE;
};


struct GammaDISPSetup
{
    // Video display processor registers
    ULONG CTL;
    ULONG MA_CTL;

    ULONG CHR_BA;
    ULONG PMP;
    ULONG TARGET_SIZE;

    ULONG LUMA_HSRC;
    ULONG CHR_HSRC;

    ULONG PDELTA;
    ULONG NLZZD; // As output is always 4:4:4 this is used for Luma and Chroma

    // Specific setup for each field
    DISPFieldSetup topField;
    DISPFieldSetup botField;
    DISPFieldSetup altTopField;
    DISPFieldSetup altBotField;

    VideoPlugSetup vidPlugSetup;

    // Filter coefficients
    const ULONG *hfluma;
    const ULONG *hfchroma;
    const ULONG *vfluma;
    const ULONG *vfchroma;
};


class CGammaCompositorDISP: public CGammaCompositorVideoPlane
{
public:
    CGammaCompositorDISP(stm_plane_id_t GDPid, CGammaVideoPlug *, ULONG dispbaseAddr);

    virtual bool QueueBuffer(const stm_display_buffer_t * const pFrame,
                             const void                 * const user);
    virtual void UpdateHW(bool isDisplayInterlaced,
                          bool isTopFieldOnDisplay,
                          const TIME64 &vsyncTime);

protected:
    virtual void DisableHW(void);

private:
    void selectScalingFilters(GammaDISPSetup                &displayNode,
                              const GAMMA_QUEUE_BUFFER_INFO &qbinfo);

    void setup422RHorizontalScaling(GammaDISPSetup          &displayNode,
                                    const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void setupMBHorizontalScaling(GammaDISPSetup            &displayNode,
                                  const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;

    void calculateVerticalFilterSetup(DISPFieldSetup          &field,
                                const GAMMA_QUEUE_BUFFER_INFO &qbinfo,
                                      int                     firstSampleOffset,
                                      bool                    doLumaFilter,
                                      bool                    isBottomField) const;

    void setupProgressiveVerticalScaling(GammaDISPSetup          &displayNode,
                                   const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;


    void setupDeInterlacedVerticalScaling(GammaDISPSetup          &displayNode,
                                    const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;


    void setupInterlacedVerticalScaling(GammaDISPSetup          &displayNode,
                                  const GAMMA_QUEUE_BUFFER_INFO &qbinfo) const;


    void setMemoryAddressing(GammaDISPSetup                   &displayNode,
                             const stm_display_buffer_t       *const pFrame,
                             const GAMMA_QUEUE_BUFFER_INFO    &qbinfo) const;

    DISPFieldSetup *SelectFieldSetup(bool            isTopFieldOnDisplay,
                                     stm_plane_node &displayNode);

    void WriteCommonSetup(GammaDISPSetup *setup);
    void WriteFieldSetup (DISPFieldSetup *field);

};

#endif // _GAMMA_COMPOSITOR_DISP_H
