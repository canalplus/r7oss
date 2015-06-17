/******************************************************************************
* COPYRIGHT (C) STMicroelectronics 2004.
*
* c8fvp3_aligned_viewport.c: $Revision: 806 $
*
* Last modification done by $Author: heni_arab $ on $Date:: 2009-11-27 #$
*
*  Aligned horizontal and vertical viewport .
*  Initially written by Heni Arab.
*
******************************************************************************/
#include "c8fvp3_aligned_viewport.h"

void hor_viewport_alignement(gvh_bool_t OutputSbsFlag,
                             gvh_u16_t InputViewportX,
                             gvh_u16_t InputViewportRightX,
                             gvh_u16_t InputViewportWidth,
                             gvh_u16_t *AlignedViewportX,
                             gvh_u16_t *AlignedViewportRightX,
                             gvh_u16_t *AlignedViewportWidth)
{
#ifdef FW_STXP70
  printf_on_debug("\tCU:: %s \n", __FUNCTION__);
#endif

  *AlignedViewportX      = InputViewportX & 0xFFFFFFFE;
  if (AlignedViewportRightX != 0) {
    *AlignedViewportRightX = OutputSbsFlag?InputViewportRightX:(InputViewportRightX & 0xFFFFFFFE);
  }
  *AlignedViewportWidth  = InputViewportWidth + (InputViewportX & 0x1);
  *AlignedViewportWidth += OutputSbsFlag?0:(*AlignedViewportWidth & 0x1);
}
    
void ver_viewport_alignement(gvh_bool_t ChromaSampling420Not422,
                             gvh_bool_t ProgressiveFlag,
                             gvh_bool_t OutputTabFlag ,
                             gvh_u16_t InputViewportY,
                             gvh_u16_t InputViewportRightY,
                             gvh_u16_t InputViewportHeight,
                             gvh_u16_t *AlignedViewportY,
                             gvh_u16_t *AlignedViewportRightY,
                             gvh_u16_t *AlignedViewportHeight,
                             gvh_u16_t *AlignedViewportRightHeight)
{
#ifdef FW_STXP70
  printf_on_debug("\tCU:: %s \n", __FUNCTION__);
#endif

  if(ChromaSampling420Not422 && (!ProgressiveFlag)) {
    *AlignedViewportY      = InputViewportY & 0xFFFFFFFC;
    if (AlignedViewportRightY != 0) {
      *AlignedViewportRightY = OutputTabFlag?InputViewportRightY:(InputViewportRightY & 0xFFFFFFFC);
    } 
    *AlignedViewportHeight = InputViewportHeight  + (InputViewportY - (*AlignedViewportY));
    if (OutputTabFlag) {
      *AlignedViewportRightHeight = InputViewportHeight + (((*AlignedViewportHeight + 3) >> 2) << 2) - (*AlignedViewportHeight);
    } else {
      *AlignedViewportHeight = ((*AlignedViewportHeight + 3) >> 2) << 2;
      *AlignedViewportRightHeight = 0;
    }
  }
  else {
    *AlignedViewportY      = InputViewportY & 0xFFFFFFFE;
    if (AlignedViewportRightY != 0) {
      *AlignedViewportRightY = OutputTabFlag?InputViewportRightY:(InputViewportRightY & 0xFFFFFFFE);
    }
    *AlignedViewportHeight = InputViewportHeight + (InputViewportY & 0x1);
    *AlignedViewportHeight = OutputTabFlag?(*AlignedViewportHeight):(((*AlignedViewportHeight + 1) >> 1) << 1);
    *AlignedViewportRightHeight = OutputTabFlag?(*AlignedViewportHeight):0;
  }
}

