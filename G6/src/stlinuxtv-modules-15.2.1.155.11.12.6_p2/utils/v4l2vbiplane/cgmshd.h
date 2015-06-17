/***********************************************************************
 *
 * File: linux/tests/v4l2vbiplane/cgmshd.h
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/


#ifndef __CGMSHD_H
#define __CGMSHD_H


/******************************************************************************/
/* Modules includes                                                           */
/******************************************************************************/


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************/
/* Public constants                                                           */
/******************************************************************************/

/******************************************************************************/
/* Public types                                                               */
/******************************************************************************/

typedef enum cgms_type_e
{
    CGMS_TYPE_A
  , CGMS_TYPE_B
} cgms_type_t;

typedef enum cgms_standard_e
{
    CGMS_HD_480P60000
  , CGMS_HD_720P60000
  , CGMS_HD_1080I60000
  , CGMS_TYPE_B_HD_480P60000
  , CGMS_TYPE_B_HD_720P60000
  , CGMS_TYPE_B_HD_1080I60000
} cgms_standard_t;


/******************************************************************************/
/* Public macros                                                              */
/******************************************************************************/

/******************************************************************************/
/* Public functions prototypes                                                */
/******************************************************************************/

void CGMSHD_DrawWaveform( cgms_standard_t Standard
                        , unsigned char  *Data_p
                        , int BytesPerLine
                        , unsigned long  *BufferData_p
                        );

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __CGMSHD_H */
