/*******************************************************************************
 *
 * File: Gamma/VDPFilter.h
 * Copyright (c) 2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
 ******************************************************************************/

#ifndef _VDP_FILTER_H
#define _VDP_FILTER_H

class VDPFilter
{
  public:
    VDPFilter(void);
    ~VDPFilter(void);

    bool Create(void);

    const ULONG *SelectHorizontalLumaFilter(ULONG scaleFactor,   ULONG phase) __attribute__((const));
    const ULONG *SelectVerticalLumaFilter(ULONG scaleFactor,     ULONG phase) __attribute__((const));
    const ULONG *SelectHorizontalChromaFilter(ULONG scaleFactor, ULONG phase) __attribute__((const));
    const ULONG *SelectVerticalChromaFilter(ULONG scaleFactor,   ULONG phase) __attribute__((const));

    const ULONG *SelectVC1VerticalLumaFilter(ULONG scaleFactor,     ULONG phase, ULONG vc1type) __attribute__((const));
    const ULONG *SelectVC1VerticalChromaFilter(ULONG scaleFactor,   ULONG phase, ULONG vc1type) __attribute__((const));

    void  DumpHorizontalFilter(const ULONG *coefs);
    void  DumpVerticalFilter(const ULONG *coefs);
    void  DumpFilters(void);

  private:
    static int      useCount;
    static DMA_Area vc1RangeMapping;

};

#endif // _VDP_FILTER_H
