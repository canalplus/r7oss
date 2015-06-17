/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_FILTERTAB_H
#define FVDP_FILTERTAB_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */
#define VF_TAB_SIZE 132     /* Vertical Filters: 132 (33*4) bytes */
#define HF_TAB_SIZE 576     /* Horizontal Filters: 576 (36*16) bytes  */
#define HQVF_TAB_SIZE 1056  /* HQ scaler Vertical Filters: 1056 (132*8) bytes */
#define HQHF_TAB_SIZE 576   /* HQ scaler Horizontal Filters: 576 (36*16) bytes  */

#define NUM_VERT_SCALE_RANGES      14
#define NUM_HOR_SCALE_RANGES       17
#define NUM_RGB_HOR_SCALE_RANGES   21
#define NUM_RGB_VER_SCALE_RANGES   18

typedef enum
{
    FILTER_FE,
    FILTER_BE,
    FILTER_PIP,
    FILTER_AUX,
    FILTER_ENC,
    FILTER_NUM_CORES
} FILTER_Core_t;

typedef enum
{
    TBL_FORM_VF,   /* Vertical Filters: 136 (34*4) bytes */
    TBL_FORM_HF,   /* Horizontal Filters: 576 (36*16) bytes  */
    TBL_FORM_HQVF, /* HQ Vertical Filters: 1056 (132*8) bytes */
    TBL_FORM_HQHF, /* HQ Horizontal Filters: 576 (36*16) bytes */
    TBL_FORM_DF,   /* Deinterlacer (Vert) Filters: 1056 (132*8) bytes */
    TBL_FORM_SIZE
} ScalerTableFormat_t;


typedef enum
{
    FLTTBL_VF_Y,
    FLTTBL_VF_UV,
    FLTTBL_HF_Y,
    FLTTBL_HF_UV,
    FLTTBL_VF_YPK,
    FLTTBL_VF_UVPK,
    FLTTBL_HF_YPK,
    FLTTBL_HF_UVPK,
    FLTTBL_SIZE
} ScalerFilterTable_t;

typedef enum
{
    VF_LPF_Y_7_Orig,
    VF_LPF_Y_7_Algo4_Custom,
    VF_LPF_Y_7_Algo5_Custom,
} VShrinkFilterSelect_t;

typedef struct
{
    void const * pUV;
    void const * pY;
    void const * pUVpk;
    void const * pYpk;
} FilterTbls4P_t;

typedef struct
{
    const uint32_t * pBounders;
    const uint8_t BoundersNum;
    const FilterTbls4P_t* pFilterTable;
} FilterTblsSet_t;

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
extern FVDP_Result_t FilterTableLoad(FILTER_Core_t FilterCore, ScalerTableFormat_t TabType, uint32_t Scale, bool IsRGB, bool IsProcessingRGB);
extern FVDP_Result_t FilterTable_GetBounderIndex(ScalerTableFormat_t TabType, uint32_t Scale, bool IsRGB, uint16_t *Index, bool IsProcessingRGB);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_FILTERTAB_H */

