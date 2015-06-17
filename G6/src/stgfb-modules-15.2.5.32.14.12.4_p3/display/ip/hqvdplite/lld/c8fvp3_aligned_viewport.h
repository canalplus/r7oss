/********************************************************************************
* c8fvp3_aligned_viewport.h: $Revision: 806 $
*
* Last modification done by $Author: heni arab $ on $Date:: 2007-03-30 #$
********************************************************************************/

#ifndef _C8FVP3_ALIGNED_VIEWPORT_H
#define _C8FVP3_ALIGNED_VIEWPORT_H

#include "c8fvp3_stddefs.h"

/*!
 * \brief Do initializations that shall be done one time per picture. Aligned
 *  horizontal viewport for a single picture
 *
 * Assumptions : This function is called after the command has been transfered.
 * \param[in] OutputSbsFlag          Define if output 3D format is Side by Side
 * \param[in] InputViewportX         Define the horizontal origin of the left side
 * \param[in] InputViewportRightX    Define the horizontal origin of the right side
 * \param[in] InputViewportWidth     Define the horizontal viewport size.
 * \param[out] AlignedViewportX      Define the horizontal aligned origin of the left side
 * \param[out] AlignedViewportRightX Define the horizontal aligned origin of the right side
 * \param[out] AlignedViewportWidth  Define the horizontal aligned viewport size
 */
extern void hor_viewport_alignement(gvh_bool_t OutputSbsFlag,
                                    gvh_u16_t InputViewportX,
                                    gvh_u16_t InputViewportRightX,
                                    gvh_u16_t InputViewportWidth,
                                    gvh_u16_t *AlignedViewportX,
                                    gvh_u16_t *AlignedViewportRightX,
                                    gvh_u16_t *AlignedViewportWidth);


/*!
 * \brief Do initializations that shall be done one time per picture. Aligned
 *  vertical viewport for a single picture
 *
 * Assumptions : This function is called after the command has been transfered.
 * \param[in] ChromaSampling420Not422      Define the chroma sampling         
 * \param[in] ProgressiveFlag              Define if input material is progressive 
 * \param[in] OutputTabFlag                Define if output 3D format is Top and Bottom
 * \param[in] InputViewportY               Define the vertical origin of the left side
 * \param[in] InputViewportRightY          Define the vertical origin of the right side
 * \param[in] InputViewportHeight          Define the vertical viewport size.
 * \param[out] AlignedViewportY            Define the vertical aligned origin of the left side
 * \param[out] AlignedViewportRightY       Define the vertical aligned origin of the right side
 * \param[out] AlignedViewportHeight       Define the vertical aligned viewport size of the left side
 * \param[out] AlignedViewportRightHeight  Define the vertical aligned viewport size of the right side
 */
extern void ver_viewport_alignement(gvh_bool_t ChromaSampling420Not422,
                                    gvh_bool_t ProgressiveFlag,
                                    gvh_bool_t OutputTabFlag,
                                    gvh_u16_t InputViewportY,
                                    gvh_u16_t InputViewportRightY,
                                    gvh_u16_t InputViewportHeight,
                                    gvh_u16_t *AlignedViewportY,
                                    gvh_u16_t *AlignedViewportRightY,
                                    gvh_u16_t *AlignedViewportHeight,
                                    gvh_u16_t *AlignedViewportRightHeight);


#endif
