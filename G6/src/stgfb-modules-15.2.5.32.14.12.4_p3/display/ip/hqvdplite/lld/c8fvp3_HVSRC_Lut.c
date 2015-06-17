/********************************************************************************
 * COPYRIGHT (C) STMicroelectronics 2007.
 *
 * File name   : c8fvp3_HQR_verif_hal_api.h $Revision: 1420 $
 *
 * Last modification done by $Author: arabh $ on $Date:: 2008-03-18 $
 * 
 ********************************************************************************/

/*!
 * \file c8fvp3_HVSRC_Lut.c
 * \brief Specifics functions that initialize the Lut of HVSRC
 *
 * These functions initialize the values of Luts for the HVSRC filters
 */

#include "c8fvp3_HVSRC_Lut.h"
#include "c8fvp3_HVSRC_LutRef.h"

void  HVSRC_get_lut(gvh_u8_t StrengthY,
                    gvh_u16_t SizeInY,
                    gvh_u16_t SizeOutY,
                    gvh_u8_t StrengthC,
                    gvh_u16_t SizeInC,
                    gvh_u16_t SizeOutC,
                    gvh_u32_t *LutY,
                    gvh_u32_t *LutC,
                    gvh_u8_t *shiftY,
                    gvh_u8_t *shiftC) {

  gvh_u32_t *myLutY = 0;
  gvh_u32_t *myLutC = 0;
  gvh_u32_t i;

  gvh_u32_t LutThreshold[2][4]= { { 8192,6654,4915,4096} , { 8192,6144,5407,4587} };
  
  gvh_u32_t LutThresholdIdxY=(StrengthY==HVSRC_LUT_Y_LEGACY)?0:1;
  gvh_u32_t LutThresholdIdxC=(StrengthC==HVSRC_LUT_C_LEGACY)?0:1;
  
  gvh_u32_t ZoomY = 8192 * SizeOutY/SizeInY;
  gvh_u32_t ZoomC = 8192 * SizeOutC/SizeInC;
 
  *shiftY=0;
  *shiftC=0;
 
  if ( ZoomY > LutThreshold[LutThresholdIdxY][0]) {
    myLutY = (gvh_u32_t*) LUT_A_Luma[StrengthY];
    *shiftY = LUTshift_A_Luma[StrengthY];
  } else if (ZoomY == LutThreshold[LutThresholdIdxY][0] ) {
    myLutY = (gvh_u32_t*) LUT_B;
    *shiftY = LUT_B_SHIFT;
  } else if (ZoomY < LutThreshold[LutThresholdIdxY][3] ) {
    myLutY = (gvh_u32_t*) LUT_F_Luma[StrengthY];
    *shiftY = LUTshift_F_Luma[StrengthY];
  } else if (ZoomY < LutThreshold[LutThresholdIdxY][2] ) {
    myLutY = (gvh_u32_t*) LUT_E_Luma[StrengthY];
    *shiftY = LUTshift_E_Luma[StrengthY];
  } else if (ZoomY < LutThreshold[LutThresholdIdxY][1] ) {
    myLutY = (gvh_u32_t*) LUT_D_Luma[StrengthY];   
    *shiftY = LUTshift_D_Luma[StrengthY];
  } else { /*if (ZoomY < LutThreshold[LutThresholdIdxY][0] ) {*/
    myLutY = (gvh_u32_t*) LUT_C_Luma[StrengthY];      
    *shiftY = LUTshift_C_Luma[StrengthY];
  }

  if ( StrengthC != HVSRC_LUT_C_LEGACY) {
    StrengthC = HVSRC_LUT_C_OPT;
  }

  if ( ZoomC > LutThreshold[LutThresholdIdxC][0]) {
    if (ZoomC == 16384 && ZoomY == 8192) {
      myLutC = (gvh_u32_t*) LUT_Chromax2;
      *shiftC = LUT_CHROMAX2_SHIFT;
    } else {
      myLutC = (gvh_u32_t*) LUT_A_Chroma[StrengthC];
      *shiftC = LUTshift_A_Chroma[StrengthC];
    }
  } else if (ZoomC == LutThreshold[LutThresholdIdxC][0] ) {
    myLutC = (gvh_u32_t*) LUT_B;
    *shiftC = LUT_B_SHIFT;
  } else if (ZoomC < LutThreshold[LutThresholdIdxC][3] ) {
    myLutC = (gvh_u32_t*) LUT_F_Chroma[StrengthC];
    *shiftC = LUTshift_F_Chroma[StrengthC];
  } else if (ZoomC < LutThreshold[LutThresholdIdxC][2] ) {
    myLutC = (gvh_u32_t*) LUT_E_Chroma[StrengthC];
    *shiftC = LUTshift_E_Chroma[StrengthC];
  } else if (ZoomC < LutThreshold[LutThresholdIdxC][1] ) {
    myLutC = (gvh_u32_t*) LUT_D_Chroma[StrengthC];   
    *shiftC = LUTshift_D_Chroma[StrengthC];
  } else { /*if (ZoomC < LutThreshold[LutThresholdIdxC][0] ) {*/
    myLutC = (gvh_u32_t*) LUT_C_Chroma[StrengthC];      
    *shiftC = LUTshift_C_Chroma[StrengthC];
  }

  for (i=0;i<NBREGCOEF;i++) {
    /*
      global_verif_hal.print_msg("*** INFO  : GET No myLutY[0]", myLutY[i], global_verif_hal.verbose_level);
      global_verif_hal.print_msg("*** INFO  : GET No myLutC[0]", myLutC[i], global_verif_hal.verbose_level);
    */
    LutY[i] = myLutY[i];
    LutC[i] = myLutC[i];
  }

}

void HVSRC_getFilterCoefHV(gvh_u16_t ApiViewportX,
                           gvh_u16_t ApiWidthIn,
                           gvh_u16_t ApiWidthOut,
                           gvh_u16_t ApiHeightIn,
                           gvh_u16_t ApiHeightOut,
                           gvh_u8_t StrengthYV,
                           gvh_u8_t StrengthCV,
                           gvh_u8_t StrengthYH,
                           gvh_u8_t StrengthCH,
                           gvh_bool_t InputIs444,
                           gvh_bool_t OutputIs444,
                           gvh_bool_t OuputIsSbs,
                           gvh_bool_t OuputIsTab,
                           gvh_u32_t *LutYH,
                           gvh_u32_t *LutCH,
                           gvh_u32_t *LutYV,
                           gvh_u32_t *LutCV,
                           gvh_u8_t *shiftYH,
                           gvh_u8_t *shiftCH,
                           gvh_u8_t *shiftYV,
                           gvh_u8_t *shiftCV) {

  gvh_u16_t SizeInC, SizeOutC, SizeInY, SizeOutY;
  gvh_u16_t aligned_viewport_X;


    hor_viewport_alignement((OuputIsSbs || OuputIsTab)?1:0,
                            ApiViewportX,
                            0,
                            ApiWidthIn,
                            &aligned_viewport_X,
                            0,
                            &SizeInY);


  SizeOutY = ApiWidthOut;
  SizeInY  = (OuputIsSbs && InputIs444)?ApiWidthIn:ApiWidthIn & ~1;
  SizeInC  = InputIs444?SizeInY*2:SizeInY;
  SizeOutC = OutputIs444?SizeOutY*2:SizeOutY;
  


  HVSRC_get_lut(StrengthYH,
                SizeInY,
                SizeOutY,
                StrengthCH,
                SizeInC,
                SizeOutC,
                LutYH,
                LutCH,
                shiftYH,
                shiftCH);

  SizeInY = ApiHeightIn;
  SizeOutY = ApiHeightOut;
  SizeInC  = SizeInY;
  SizeOutC = SizeOutY;

  HVSRC_get_lut(StrengthYV,
                SizeInY,
                SizeOutY,
                StrengthCV,
                SizeInC,
                SizeOutC,
                LutYV,
                LutCV,
                shiftYV,
                shiftCV);
}

