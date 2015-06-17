/*******************************************************************************
 *
 * File: Gamma/GammaDisplayFilter.cpp
 * Copyright (c) 2002 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
 ******************************************************************************/

#include <stmdisplay.h>

#include <Generic/IDebug.h>

#include "STRefDisplayFilters.h"
#include "DVDDisplayFilters.h"

#include "GammaDisplayFilter.h"

static const bool USE_STREFERENCE_FILTERS = true;

const ULONG *GammaDisplayFilter::SelectHorizontalFilter(ULONG scaleFactor)
{
    if(USE_STREFERENCE_FILTERS)
    {
        // This logic has been derived from the STLayer reference driver
        if (scaleFactor > 0x8000)
            return (ULONG*)HorizontalCoefficientsG;
        else if (scaleFactor > 0x6000) //zoom out 3 times or more (0.33 >= zoom-in factor)
            return (ULONG*)HorizontalCoefficientsF;
        else if (scaleFactor > 0x4000) //zoom out between 2 to 3 times (0.5 >= zoom-in factor > 0.33)
            return (ULONG*)HorizontalCoefficientsE;
        else if (scaleFactor > 0x3000) //zoom out between 1.45 and 2 times (0.69 >= zoom-in factor > 0.5)
            return (ULONG*)HorizontalCoefficientsD;
        else if (scaleFactor > 0x2000) //zoom out between 1 and 1.45 times (1 > zoom-in factor > 0.69)
            return (ULONG*)HorizontalCoefficientsC;
        else if (scaleFactor == 0x2000) // No scaling
            return (ULONG*)HorizontalCoefficientsB;
        else //if (scaleFactor < 0x2000) //zoom in
            return (ULONG*)HorizontalCoefficientsA; //zoom-in factor > 1
    }
    else
    {
        // This logic has been derived from the STCM driver for the STm8000
        if (scaleFactor >= 0x6000) //zoom out 3 times or more (0.33 >= zoom-in factor)
            return horizontalZoomOutCoeffs_025_03;
        else if (scaleFactor >= 0x4000) //zoom out between 2 to 3 times (0.5 >= zoom-in factor > 0.33)
            return horizontalZoomOutCoeffs_03_05;
        else if (scaleFactor >= 0x2e60) //zoom out between 1.45 and 2 times (0.69 >= zoom-in factor > 0.5)
            return horizontalZoomOutCoeffs_05_069;
        else if (scaleFactor > 0x2000) //zoom out between 1 and 1.45 times (1 > zoom-in factor > 0.69)
            return horizontalZoomOutCoeffs_069_1;
        else if (scaleFactor == 0x2000) // No scaling
            return horizontalZoomNoneCoeffs;
        else //if (scaleFactor < 0x2000) //zoom in
            return horizontalZoomInCoeffs; //zoom-in factor > 1
    }
}


const ULONG *GammaDisplayFilter::SelectVerticalFilter(ULONG scaleFactor)
{
    if(USE_STREFERENCE_FILTERS)
    {
        // This logic has been derived from the STLayer reference driver
        if (scaleFactor > 0x8000)
            return (ULONG*)VerticalCoefficientsG;
        else if (scaleFactor > 0x6000) //zoom out 3 times or more (0.33 >= zoom-in factor)
            return (ULONG*)VerticalCoefficientsF;
        else if (scaleFactor > 0x4000) //zoom out between 2 to 3 times (0.5 >= zoom-in factor > 0.33)
            return (ULONG*)VerticalCoefficientsE;
        else if (scaleFactor > 0x3000) //zoom out between 1.45 and 2 times (0.69 >= zoom-in factor > 0.5)
            return (ULONG*)VerticalCoefficientsD;
        else if (scaleFactor > 0x2000) //zoom out between 1 and 1.45 times (1 > zoom-in factor > 0.69)
            return (ULONG*)VerticalCoefficientsC;
        else if (scaleFactor == 0x2000) // No scaling
            return (ULONG*)VerticalCoefficientsB;
        else //if (scaleFactor < 0x2000) //zoom in
            return (ULONG*)VerticalCoefficientsA; //zoom-in factor > 1
    }
    else
    {
        // This logic has been derived from the STCM driver for the STm8000
        if (scaleFactor >= 0x6000) //zoom out 3 times or more (0.33 >= zoom-in factor)
            return verticalZoomOutCoeffs_025_03;
        else if (scaleFactor >= 0x4000) //zoom out between 2 to 3 times (0.5 >= zoom-in factor > 0.33)
            return verticalZoomOutCoeffs_03_05;
        else if (scaleFactor >= 0x2e60) //zoom out between 1.45 and 2 times (0.69 >= zoom-in factor > 0.5)
            return verticalZoomOutCoeffs_05_069;
        else if (scaleFactor > 0x2000) //zoom out between 1 and 1.45 times (1 > zoom-in factor > 0.5)
            return verticalZoomOutCoeffs_069_1;
        else if (scaleFactor == 0x2000) // No scaling
            return verticalZoomNoneCoeffs;
        else //if (scaleFactor < 0x2000) //zoom in
            return verticalZoomInCoeffs; //zoom-in factor > 1
    }
}


static inline int coeff(char k,int shift,int offset)
{
  return (((int)k)<<shift)+offset;
}


static inline void printcoefs(int *coefs, int n)
{
  int total = 0;
  for(int i=0;i<n;i++)
  {
    total += coefs[i];
    g_pIDebug->IDebugPrintf("%d\t",coefs[i]);
  }
  g_pIDebug->IDebugPrintf(": total = %d\n",total);
}


void GammaDisplayFilter::DumpHorizontalFilter(const ULONG *coefs)
{
#if defined(DEBUG) && (DEBUG_LEVEL > 2)
  int offset_0_7, offset_1_6, offset_2_5, offset_3_4;
  int shift_0_7, shift_1_6, shift_2_5, shift_3_4;
  int div;
  int t[8];

  offset_0_7 = coefs[33] & 0x1ff;
  if(coefs[33]&0x200) offset_0_7 |= 0xfffffe00;

  offset_1_6 = (coefs[33]>>16) & 0x1ff;
  if(coefs[33]&0x02000000) offset_1_6 |= 0xfffffe00;

  offset_2_5 = coefs[34] & 0x1ff;
  if(coefs[34]&0x200) offset_2_5 |= 0xfffffe00;

  offset_3_4 = (coefs[34]>>16) & 0x1ff;
  if(coefs[34]&0x02000000) offset_3_4 |= 0xfffffe00;

  div = 1<<(8+((coefs[33]>>11)&0x7));

  shift_0_7 = (coefs[33]&0x400)?1:0;
  shift_1_6 = (coefs[33]&0x04000000)?1:0;
  shift_2_5 = (coefs[34]&0x400)?1:0;
  shift_3_4 = (coefs[34]&0x04000000)?1:0;

  g_pIDebug->IDebugPrintf("shifts: %d\t%d\t%d\t%d\n",shift_0_7,shift_1_6,shift_2_5,shift_3_4);
  g_pIDebug->IDebugPrintf("offset: %d\t%d\t%d\t%d\n",offset_0_7,offset_1_6,offset_2_5,offset_3_4);
  g_pIDebug->IDebugPrintf("div   : %d\n",div);

  /*
   * Phase 0
   */
  t[0] = coeff((char)((coefs[32]>>24)&0xff),shift_0_7,offset_0_7);
  t[1] = coeff((char)((coefs[32]>>16)&0xff),shift_1_6,offset_1_6);
  t[2] = coeff((char)((coefs[32]>>8)&0xff) ,shift_2_5,offset_2_5);
  t[3] = coeff((char)((coefs[32]>>0)&0xff) ,shift_3_4,offset_3_4);
  t[4] = coeff((char)((coefs[24]>>0)&0xff) ,shift_3_4,offset_3_4);
  t[5] = coeff((char)((coefs[16]>>0)&0xff) ,shift_2_5,offset_2_5);
  t[6] = coeff((char)((coefs[8]>>0)&0xff)  ,shift_1_6,offset_1_6);
  t[7] = coeff((char)((coefs[0]>>0)&0xff)  ,shift_0_7,offset_0_7);

  printcoefs(t,8);

  /*
   * Phase 1
   */
  t[0] = coeff((char)((coefs[7]>>24)&0xff)  ,shift_0_7,offset_0_7);
  t[1] = coeff((char)((coefs[15]>>24)&0xff) ,shift_1_6,offset_1_6);
  t[2] = coeff((char)((coefs[23]>>24)&0xff) ,shift_2_5,offset_2_5);
  t[3] = coeff((char)((coefs[31]>>24)&0xff) ,shift_3_4,offset_3_4);
  t[4] = coeff((char)((coefs[24]>>8)&0xff)  ,shift_3_4,offset_3_4);
  t[5] = coeff((char)((coefs[16]>>8)&0xff)  ,shift_2_5,offset_2_5);
  t[6] = coeff((char)((coefs[8]>>8)&0xff)   ,shift_1_6,offset_1_6);
  t[7] = coeff((char)((coefs[0]>>8)&0xff)   ,shift_0_7,offset_0_7);

  printcoefs(t,8);

  /*
   * Phase 16
   */
  t[0] = coeff((char)((coefs[4]>>0)&0xff)  ,shift_0_7,offset_0_7);
  t[1] = coeff((char)((coefs[12]>>0)&0xff) ,shift_1_6,offset_1_6);
  t[2] = coeff((char)((coefs[20]>>0)&0xff) ,shift_2_5,offset_2_5);
  t[3] = coeff((char)((coefs[28]>>0)&0xff) ,shift_3_4,offset_3_4);
  t[4] = coeff((char)((coefs[28]>>0)&0xff) ,shift_3_4,offset_3_4);
  t[5] = coeff((char)((coefs[20]>>0)&0xff) ,shift_2_5,offset_2_5);
  t[6] = coeff((char)((coefs[12]>>0)&0xff) ,shift_1_6,offset_1_6);
  t[7] = coeff((char)((coefs[4]>>0)&0xff)  ,shift_0_7,offset_0_7);

  printcoefs(t,8);
#endif
}


void GammaDisplayFilter::DumpVerticalFilter(const ULONG *coefs)
{
#if defined(DEBUG) && (DEBUG_LEVEL > 2)
  int offset_0_4, offset_1_3, offset_2;
  int shift_0_4, shift_1_3, shift_2;
  int div;
  int t[5];

  offset_0_4 = coefs[21] & 0x1ff;
  if(coefs[21]&0x200) offset_0_4 |= 0xfffffe00;

  offset_1_3 = (coefs[21]>>11) & 0x1ff;
  if(coefs[21]&0x00100000) offset_1_3 |= 0xfffffe00;

  offset_2 = (coefs[21] >>22) & 0x1ff;
  if(coefs[21]&0x80000000) offset_2 |= 0xfffffe00;

  div = 1<<(6+((coefs[20]>>25)&0x3));

  shift_0_4 = (coefs[21]&0x400)?1:0;
  shift_1_3 = (coefs[21]&0x00200000)?1:0;
  shift_2   = (coefs[20]&0x01000000)?1:0;

  g_pIDebug->IDebugPrintf("shifts: %d\t%d\t%d\n",shift_0_4,shift_1_3,shift_2);
  g_pIDebug->IDebugPrintf("offset: %d\t%d\t%d\n",offset_0_4,offset_1_3,offset_2);
  g_pIDebug->IDebugPrintf("div   : %d\n",div);

  /*
   * Phase 0
   */
  t[0] = coeff((char)((coefs[20]>>16)&0xff),shift_0_4,offset_0_4);
  t[1] = coeff((char)((coefs[20]>>8)&0xff) ,shift_1_3,offset_1_3);
  t[2] = coeff((char)((coefs[16]>>0)&0xff) ,shift_2,offset_2);
  t[3] = coeff((char)((coefs[8]>>0)&0xff)  ,shift_1_3,offset_1_3);
  t[4] = coeff((char)((coefs[0]>>0)&0xff)  ,shift_0_4,offset_0_4);

  printcoefs(t,5);

  /*
   * Phase 1
   */
  t[0] = coeff((char)((coefs[7]>>24)&0xff)  ,shift_0_4,offset_0_4);
  t[1] = coeff((char)((coefs[15]>>24)&0xff) ,shift_1_3,offset_1_3);
  t[2] = coeff((char)((coefs[16]>>8)&0xff)  ,shift_2,offset_2);
  t[3] = coeff((char)((coefs[8]>>8)&0xff)   ,shift_1_3,offset_1_3);
  t[4] = coeff((char)((coefs[0]>>8)&0xff)   ,shift_0_4,offset_0_4);

  printcoefs(t,5);

  /*
   * Phase 16
   */
  t[0] = coeff((char)((coefs[4]>>0)&0xff)  ,shift_0_4,offset_0_4);
  t[1] = coeff((char)((coefs[12]>>0)&0xff) ,shift_1_3,offset_1_3);
  t[2] = coeff((char)((coefs[20]>>0)&0xff) ,shift_2,offset_2);
  t[3] = coeff((char)((coefs[12]>>0)&0xff) ,shift_1_3,offset_1_3);
  t[4] = coeff((char)((coefs[4]>>0)&0xff)  ,shift_0_4,offset_0_4);

  printcoefs(t,5);
#endif
}


void GammaDisplayFilter::DumpFilters(void)
{
  g_pIDebug->IDebugPrintf("GammaDisplayFilter Horizontal Coeffs\n");
  g_pIDebug->IDebugPrintf("====================================\n");

  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsG);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsF);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsE);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsD);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsC);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsB);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsA);

  g_pIDebug->IDebugPrintf("GammaDisplayFilter Vertical Coeffs\n");
  g_pIDebug->IDebugPrintf("==================================\n");

  DumpVerticalFilter((ULONG*)VerticalCoefficientsG);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsF);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsE);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsD);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsC);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsB);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsA);
}
