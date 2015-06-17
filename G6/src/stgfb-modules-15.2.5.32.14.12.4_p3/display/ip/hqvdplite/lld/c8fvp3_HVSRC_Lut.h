/********************************************************************************
 * COPYRIGHT (C) STMicroelectronics 2007.
 *
 * File name   : c8fvp3_HQR_verif_hal_api.h $Revision: 1420 $
 *
 * Last modification done by $Author: arabh $ on $Date:: 2008-03-18 $
 * 
 ********************************************************************************/

/*!
 * \file c8fvp3_HVSRC_Lut.h
 * \brief Specifics functions that initialize the Lut of HVSRC
 *
 * These functions initialize the values of Luts for the HVSRC filters
 */

#ifndef _C8FVP3_HVSRC_LUT_H_
#define _C8FVP3_HVSRC_LUT_H_

#include "c8fvp3_stddefs.h"
#include "c8fvp3_aligned_viewport.h"

/* Luma strength */
#define HVSRC_LUT_Y_LEGACY (0)
#define HVSRC_LUT_Y_SMOOTH (1)
#define HVSRC_LUT_Y_MEDIUM (2)
#define HVSRC_LUT_Y_SHARP  (3)

/* Chroma strength */
#define HVSRC_LUT_C_LEGACY (0)
#define HVSRC_LUT_C_OPT    (1)

#define HORI_SHIFT_Y_SHIFT                        (0x00000000)
#define HORI_SHIFT_C_SHIFT                        (0x00000010)
#define VERT_SHIFT_Y_SHIFT                        (0x00000000)
#define VERT_SHIFT_C_SHIFT                        (0x00000010)

/*!
 * \brief Do initialization of the HVSRC LUT
 *
 * \param[in] ApiViewportX  Viewport X origin
 * \param[in] ApiWidthIn    Input viewport width
 * \param[in] ApiWidthOut   Output viewport width
 * \param[in] ApiHeightIn   Input viewport height
 * \param[in] ApiHeightOut  Output viewport height
 * \param[in] StrengthYV    luma strength of the vertical LUT legacy/smooth/medium/sharp
 * \param[in] StrengthCV    chroma strength of the vertical LUT legacy/smooth/medium/sharp
 * \param[in] StrengthYH    luma strength of the horizontal LUT legacy/smooth/medium/sharp
 * \param[in] StrengthCH    chroma strength of the horizontal LUT legacy/smooth/medium/sharp
 * \param[in] InputIs444    Input chroma sampling
 * \param[in] OutputIs444   Outputchroma sampling
 * \param[in] OuputIsSbs    Output 3D format is SbS
 * \param[in] OuputIsTab    Output 3D format is TaB
 * \param[out] LutYH        pointer to luma horizontal lut
 * \param[out] LutCH        pointer to chroma horizontal lut
 * \param[out] LutYV        pointer to luma vertical lut
 * \param[out] LutCV        pointer to chroma vertical lut
 * \param[out] shiftYH      sum of horizontal coef luma
 * \param[out] shiftCH      sum of horizontal coef chroma
 * \param[out] shiftYV      sum of vertical coef luma
 * \param[out] shiftCV      sum of vertical coef chroma
 */
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
                           gvh_u8_t *shiftCV);
                           
/*!                        
 * \brief Do initialization of the HVSRC LUT
 *
 * \param[in] StrengthY      luma strength of the LUT legacy/smooth/medium/sharp
 * \param[in] SizeInY        luma width or height of the input
 * \param[in] SizeOutY       luma width or height of the output
 * \param[in] StrengthC      chroma strength of the LUT legacy/smooth/medium/sharp
 * \param[in] SizeInC        chroma width or height of the input
 * \param[in] SizeOutC       chroma width or height of the output
 * \param[out] LutY          pointer to luma   lut
 * \param[out] LutC          pointer to chroma lut
 * \param[out] shiftY        sum of luma coef
 * \param[out] shiftC        sum of chroma coef
 */
void HVSRC_get_lut(gvh_u8_t StrengthY,
                   gvh_u16_t SizeInY,
                   gvh_u16_t SizeOutY,
                   gvh_u8_t StrengthC,
                   gvh_u16_t SizeInC,
                   gvh_u16_t SizeOutC,
                   gvh_u32_t *LutY,
                   gvh_u32_t *LutC,
                   gvh_u8_t *shiftY,
                   gvh_u8_t *shiftC);


#endif /* _C8FVP3_HVSRC_LUT_H_ */
