/*******************************************************************************
 *
 * File: Gamma/VDPFilter.cpp
 * Copyright (c) 2005-2007,2008 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 * 
 ******************************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>


#include "STRefVDPFilters.h"

#include "VDPFilter.h"

static int v_coeff_size = VFC_NB_COEFS*sizeof(ULONG);

int      VDPFilter::useCount=0;
DMA_Area VDPFilter::vc1RangeMapping;


VDPFilter::VDPFilter(void)
{
  if(useCount == 0)
  {
    g_pIOS->ZeroMemory(&vc1RangeMapping, sizeof(DMA_Area));
  }

}


VDPFilter::~VDPFilter(void)
{
  DENTRY();

  if(useCount == 0)
    return;

  useCount--;

  if(useCount == 0)
  {
    DEBUGF2(2,("%s: Freeing VC1 range mapping tables\n",__PRETTY_FUNCTION__));
    g_pIOS->FreeDMAArea(&vc1RangeMapping);
  }

  DEXIT();
}


bool VDPFilter::Create(void)
{
  DENTRY();

  useCount++;
  if(useCount == 1)
  {
    g_pIOS->AllocateDMAArea(&vc1RangeMapping, v_coeff_size*11*8, 0, SDAAF_NONE);

    if(vc1RangeMapping.pMemory == 0)
      return false;

    for(int R=0;R<8;R++)
    {
      long set_offset = v_coeff_size*11*R;
      
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset,                 &VerticalCoefficientsA,            v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size,    &VerticalCoefficientsB,            v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*2,  &VerticalCoefficientsLumaC,        v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*3,  &VerticalCoefficientsLumaD,        v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*4,  &VerticalCoefficientsLumaE,        v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*5,  &VerticalCoefficientsLumaF,        v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*6,  &VerticalCoefficientsChromaZoomX2, v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*7,  &VerticalCoefficientsChromaC,      v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*8,  &VerticalCoefficientsChromaD,      v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*9,  &VerticalCoefficientsChromaE,      v_coeff_size);
      g_pIOS->MemcpyToDMAArea(&vc1RangeMapping, set_offset+v_coeff_size*10, &VerticalCoefficientsChromaF,      v_coeff_size);

      for(int i=0;i<11;i++)
      {
        ULONG *coeffs = (ULONG *)(vc1RangeMapping.pData + set_offset + (v_coeff_size*i));
        ULONG div = (coeffs[20] >> 25) & 0x3;
        LONG offset_0_4,offset_1_3,offset_2;

        offset_0_4 = coeffs[21] & 0x1ff;
        if(coeffs[21]&0x200) offset_0_4 |= 0xfffffe00;

        offset_1_3 = (coeffs[21]>>11) & 0x1ff;
        if(coeffs[21]&0x00100000) offset_1_3 |= 0xfffffe00;

        offset_2 = (coeffs[21] >>22) & 0x1ff;
        if(coeffs[21]&0x80000000) offset_2 |= 0xfffffe00;


        if(div == 0)
        {
          /*
           * Scale up offset by R factor. This only works if the offsets in
           * the filters are small and don't go out of range when multiplied
           * by two. Thankfully all the filters with a div == 0 meet this
           * requirement, except for the filter for no scaling, hence the
           * adjustment on the offset for the centre filter tap (2).
           */
          offset_0_4 = (offset_0_4*(R*64+576))/512;
          offset_1_3 = (offset_1_3*(R*64+576))/512;
          offset_2   = (offset_2*(R*64+576))/512;
          if(offset_2>511)
            offset_2  = 511; /* Make sure we don't overflow in the hardware */

          /*
           * Use the shift to multiply the coefficients by two
           */
          coeffs[20] |= 0x01000000;
          coeffs[21]  = (offset_0_4 & 0x3ff) | ((offset_1_3 & 0x3ff) << 11) | ((offset_2 & 0x3ff) << 22) | 0x00200400;
        }
        else
        {
          /*
           * We can use the divide to implicitly multiply both the coefficients
           * and offset by 2
           */
          coeffs[20] &= ~(0x3 << 25);
          coeffs[20] |= (div-1) << 25;
          /*
           * So we now scale the offsets by 1/2 the R factor. This means they can't go out of range.
           */ 
          offset_0_4 = (offset_0_4*(R*64+576))/1024;
          offset_1_3 = (offset_1_3*(R*64+576))/1024;
          offset_2   = (offset_2*(R*64+576))/1024;;

          coeffs[21]  = (offset_0_4 & 0x3ff) | ((offset_1_3 & 0x3ff) << 11) | ((offset_2 & 0x3ff) << 22);
        }
        
        /*
         * Now scale down all the coefficients by 1/2 the R factor
         */
        for(int j=0;j<21;j++)
        {
          LONG k1,k2,k3,k4;
          
          k1 = (signed char)(coeffs[j]       & 0xff);
          k2 = (signed char)((coeffs[j]>>8)  & 0xff);
          k3 = (signed char)((coeffs[j]>>16) & 0xff);
          k4 = (signed char)((coeffs[j]>>24) & 0xff);

          k1 = (k1*(R*64+576))/1024;
          k2 = (k2*(R*64+576))/1024;
          k3 = (k3*(R*64+576))/1024;
          if(j != 20)
            k4 = (k4*(R*64+576))/1024;
          
          coeffs[j] = ((k4&0xff)<<24) | ((k3&0xff)<<16) | ((k2&0xff)<<8) | (k1&0xff);
        }
      }
    }
  }

  DEXIT();
  return true;
}


const ULONG *VDPFilter::SelectHorizontalLumaFilter(ULONG scaleFactor,ULONG phase)
{
  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                      // < 0.5x
    return (ULONG*)HorizontalCoefficientsLumaF;
  else if (scaleFactor > 0x3555)                 // >= 0.5x, < 0.6x
    return (ULONG*)HorizontalCoefficientsLumaE;
  else if (scaleFactor > 0x2800)                 // >= 0.6x, < 0.8x
    return (ULONG*)HorizontalCoefficientsLumaD;
  else if (scaleFactor > 0x2000)                 // >= 0.8x, < 1x
    return (ULONG*)HorizontalCoefficientsLumaC;
  else if (scaleFactor == 0x2000 && phase == 0)  // No scaling or subpixel pan/scan
    return (ULONG*)HorizontalCoefficientsB;
  else                                    
   return (ULONG*)HorizontalCoefficientsA;       // zoom in or subpixel pan/scan at 1x
}


const ULONG *VDPFilter::SelectHorizontalChromaFilter(ULONG scaleFactor,ULONG phase)
{
  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                      // < 0.5x
    return (ULONG*)HorizontalCoefficientsChromaF;
  else if (scaleFactor > 0x3555)                 // >= 0.5x, < 0.6x
    return (ULONG*)HorizontalCoefficientsChromaE;
  else if (scaleFactor > 0x2800)                 // >= 0.6x, < 0.8x
    return (ULONG*)HorizontalCoefficientsChromaD;
  else if (scaleFactor > 0x2000)                 // >= 0.8x, < 1x
    return (ULONG*)HorizontalCoefficientsChromaC;
  else if (scaleFactor == 0x2000 && phase == 0)  // No scaling or subpixel pan/scan
    return (ULONG*)HorizontalCoefficientsB;
  else if (scaleFactor == 0x1000)                // 2x chroma
    return (ULONG*)HorizontalCoefficientsChromaZoomX2;
  else                                    
   return (ULONG*)HorizontalCoefficientsA;       // zoom in or subpixel pan/scan at 1x
}


const ULONG *VDPFilter::SelectVerticalLumaFilter(ULONG scaleFactor,ULONG phase)
{
  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                          // < 0.5x
    return (ULONG*)VerticalCoefficientsLumaF;
  else if (scaleFactor > 0x3555)                     // >= 0.5x, < 0.6x
    return (ULONG*)VerticalCoefficientsLumaE;
  else if (scaleFactor > 0x2800)                     // >= 0.6x, < 0.8x
    return (ULONG*)VerticalCoefficientsLumaD;
  else if (scaleFactor > 0x2000)                     // >= 0.8x, < 1x
    return (ULONG*)VerticalCoefficientsLumaC;
  else if (scaleFactor == 0x2000 && phase == 0x1000) // No scaling or subpixel pan/scan
    return (ULONG*)VerticalCoefficientsB;
  else
    return (ULONG*)VerticalCoefficientsA;            // zoom in or subpixel pan/scan at 1x
}


const ULONG *VDPFilter::SelectVerticalChromaFilter(ULONG scaleFactor,ULONG phase)
{
  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                          // < 0.5x
    return (ULONG*)VerticalCoefficientsChromaF;
  else if (scaleFactor > 0x3555)                     // >= 0.5x, < 0.6x
    return (ULONG*)VerticalCoefficientsChromaE;
  else if (scaleFactor > 0x2800)                     // >= 0.6x, < 0.8x
    return (ULONG*)VerticalCoefficientsChromaD;
  else if (scaleFactor > 0x2000)                     // >= 0.8x, < 1x
    return (ULONG*)VerticalCoefficientsChromaC;
  else if (scaleFactor == 0x2000 && phase == 0x1000) // No scaling or subpixel pan/scan
    return (ULONG*)VerticalCoefficientsB;
  else if (scaleFactor == 0x1000)                    // 2x chroma
    return (ULONG*)VerticalCoefficientsChromaZoomX2;
  else
    return (ULONG*)VerticalCoefficientsA;            // zoom in or subpixel pan/scan at 1x
}


enum { FILTER_A,FILTER_B, LUMA_C,LUMA_D,LUMA_E,LUMA_F,CHROMA_2X,CHROMA_C,CHROMA_D,CHROMA_E,CHROMA_F };


const ULONG *VDPFilter::SelectVC1VerticalLumaFilter(ULONG scaleFactor,ULONG phase, ULONG vc1type)
{
  char *pData;

  if(vc1type > STM_PLANE_SRC_VC1_RANGE_MAP_MAX)
    return SelectVerticalLumaFilter(scaleFactor,phase);

  pData = vc1RangeMapping.pData + (v_coeff_size*11*vc1type);

  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                          // < 0.5x
    return (ULONG *)(pData + (v_coeff_size*LUMA_F));
  else if (scaleFactor > 0x3555)                     // >= 0.5x, < 0.6x
    return (ULONG*)(pData + (v_coeff_size*LUMA_E));
  else if (scaleFactor > 0x2800)                     // >= 0.6x, < 0.8x
    return (ULONG*)(pData + (v_coeff_size*LUMA_D));
  else if (scaleFactor > 0x2000)                     // >= 0.8x, < 1x
    return (ULONG*)(pData + (v_coeff_size*LUMA_C));
  else if (scaleFactor == 0x2000 && phase == 0x1000) // No scaling or subpixel pan/scan
    return (ULONG*)(pData + (v_coeff_size*FILTER_B));
  else
    return (ULONG*)(pData + (v_coeff_size*FILTER_A)); // zoom in or subpixel pan/scan at 1x
}


const ULONG *VDPFilter::SelectVC1VerticalChromaFilter(ULONG scaleFactor,ULONG phase, ULONG vc1type)
{
  char *pData;

  if(vc1type > STM_PLANE_SRC_VC1_RANGE_MAP_MAX)
    return SelectVerticalChromaFilter(scaleFactor,phase);

  pData = vc1RangeMapping.pData + (v_coeff_size*11*vc1type);

  // This logic has been derived from the STLayer reference driver
  if (scaleFactor > 0x4000)                          // < 0.5x
    return (ULONG*)(pData + (v_coeff_size*CHROMA_F));
  else if (scaleFactor > 0x3555)                     // >= 0.5x, < 0.6x
    return (ULONG*)(pData + (v_coeff_size*CHROMA_E));
  else if (scaleFactor > 0x2800)                     // >= 0.6x, < 0.8x
    return (ULONG*)(pData + (v_coeff_size*CHROMA_D));
  else if (scaleFactor > 0x2000)                     // >= 0.8x, < 1x
    return (ULONG*)(pData + (v_coeff_size*CHROMA_C));
  else if (scaleFactor == 0x2000 && phase == 0x1000) // No scaling or subpixel pan/scan
    return (ULONG*)(pData + (v_coeff_size*FILTER_B));
  else if (scaleFactor == 0x1000)                    // 2x chroma
    return (ULONG*)(pData + (v_coeff_size*CHROMA_2X));
  else
    return (ULONG*)(pData + (v_coeff_size*FILTER_A));// zoom in or subpixel pan/scan at 1x
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


void VDPFilter::DumpHorizontalFilter(const ULONG *coefs)
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

  g_pIDebug->IDebugPrintf("coeffs: %p\n",coefs);
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


void VDPFilter::DumpVerticalFilter(const ULONG *coefs)
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

  g_pIDebug->IDebugPrintf("coeffs: %p\n",coefs);
  g_pIDebug->IDebugPrintf("shifts: %d\t%d\t%d\n",shift_0_4,shift_1_3,shift_2);
  g_pIDebug->IDebugPrintf("offset: %d\t%d\t%d\n",offset_0_4,offset_1_3,offset_2);
  g_pIDebug->IDebugPrintf("div: %d\n",div);

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


void VDPFilter::DumpFilters(void)
{
  DEBUGF2(2,("VDPFilter Horizontal Luma Coeffs\n"));
  DEBUGF2(2,("================================\n"));
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsLumaF);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsLumaE);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsLumaD);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsLumaC);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsB);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsA);

  DEBUGF2(2,("VDPFilter Horizontal Chroma Coeffs\n"));
  DEBUGF2(2,("==================================\n"));
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsChromaF);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsChromaE);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsChromaD);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsChromaC);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsB);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsA);
  DumpHorizontalFilter((ULONG*)HorizontalCoefficientsChromaZoomX2);

  DEBUGF2(2,("VDPFilter Vertical Luma Coeffs\n"));
  DEBUGF2(2,("==============================\n"));
  DumpVerticalFilter((ULONG*)VerticalCoefficientsLumaF);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsLumaE);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsLumaD);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsLumaC);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsB);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsA);

  DEBUGF2(2,("VDPFilter Vertical Chroma Coeffs\n"));
  DEBUGF2(2,("================================\n"));
  DumpVerticalFilter((ULONG*)VerticalCoefficientsChromaF);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsChromaE);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsChromaD);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsChromaC);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsB);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsA);
  DumpVerticalFilter((ULONG*)VerticalCoefficientsChromaZoomX2);
}
