/*******************************************************************************
 *
 * File: Gamma/GammaDisplayFilter.h
 * Copyright (c) 2002 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
 ******************************************************************************/

#ifndef _GAMMA_DISPLAY_FILTER_H
#define _GAMMA_DISPLAY_FILTER_H

class GammaDisplayFilter
{
  public:
    static const ULONG *SelectHorizontalFilter(ULONG scaleFactor) __attribute__((const));
    static const ULONG *SelectVerticalFilter(ULONG scaleFactor) __attribute__((const));
    static void         DumpHorizontalFilter(const ULONG *coefs);
    static void         DumpVerticalFilter(const ULONG *coefs);
    static void         DumpFilters(void);

  private:
    GammaDisplayFilter(void);
    ~GammaDisplayFilter(void);
};

#endif // _GAMMA_DISPLAY_FILTER_H
