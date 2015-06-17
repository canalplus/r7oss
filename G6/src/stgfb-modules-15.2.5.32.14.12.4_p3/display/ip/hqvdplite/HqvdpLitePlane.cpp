/***********************************************************************
 *
 * File: display/ip/HqvdpLitePlane.cpp
 * Copyright (c) 2013 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <stm_display.h>

#include <vibe_os.h>
#include <vibe_debug.h>
#include <display/generic/Output.h>


#include <display/ip/hqvdplite/HqvdpLitePlane.h>
#include <display/ip/hqvdplite/lld/HQVDP_api.h>
#include <display/ip/hqvdplite/lld/hqvdp_mailbox_macros.h>

/* Debug option to force a spatial deinterlacing */
/* #define DEBUG_FORCE_SPATIAL_DEINTERLACING */


typedef enum
{
    FILTERSET_LEGACY=0,
    FILTERSET_SHARP,
    FILTERSET_MEDIUM,
    FILTERSET_SMOOTH,
    FILTERSET_LAST
}  ZoomFilterSet_t;

#define MAX_SCALING_FACTOR  8

#define HQVDPLITE_STR64_RDPLUG_OFFSET         (0x0e0000)
#define HQVDPLITE_STR64_WRPLUG_OFFSET         (0x0e2000)
#define HQVDPLITE_MBX_OFFSET                  (0x0e4000)
#define HQVDPLITE_PMEM_OFFSET                 (0x040000)
#define HQVDPLITE_DMEM_OFFSET                 (0x000000)
#define HQVDPLITE_STR_READ_REGS_BASEADDR      (0x0b8000)

#define HQVDPLITE_XP70_MBOX_SIZE              (0x00000FFF)
#define HQVDPLITE_XP70_IMEM_SIZE              (65536)
#define HQVDPLITE_XP70_DMEM_SIZE              (16384)


#define DCACHE_LINE_SIZE    (32)

/* defines used by checking HW limitation */
#define MAX_HQVDPLITE_FIRWMARE_CYCLES         1000
#define HQVDPLITE_DEFAULT_CLOCK_FREQUENCY     400

/* For a given use case, the STBusDataRate can be up to 40% higher than the STBusDataRate for the reference use case.
   Which correspond to 140%.
*/
#define BW_RATIO      140

#define FULLHD_WIDTH  1920
#define FULLHD_HEIGHT 1080
#define UHD_WIDTH     3840
#define UHD_HEIGHT    2160


static const stm_pixel_format_t g_surfaceFormats[] = {
    SURF_YCBCR420MB,
    SURF_YCbCr420R2B,
    SURF_YCbCr422R2B,
    SURF_YCbCr422R2B10,
    SURF_CRYCB888,
    SURF_CRYCB101010,
    SURF_ACRYCB8888,
};

static const stm_plane_feature_t g_hqvdplitePlaneFeatures[] = {
    PLANE_FEAT_VIDEO_SCALING,
    PLANE_FEAT_VIDEO_SCALING,
    PLANE_FEAT_VIDEO_MADI,
    PLANE_FEAT_WINDOW_MODE,
    PLANE_FEAT_SRC_COLOR_KEY,
    PLANE_FEAT_TRANSPARENCY,
    PLANE_FEAT_GLOBAL_COLOR,
    PLANE_FEAT_VIDEO_FMD,
    PLANE_FEAT_VIDEO_IQI_PEAKING,
    PLANE_FEAT_VIDEO_IQI_LE,
    PLANE_FEAT_VIDEO_IQI_CTI
};

/* Size of STBUS (in Bytes) */
#define STBUS_NBR_OF_BYTES_PER_WORD     8
#define DEFAULT_MIN_SPEC_BETWEEN_REQS   0

#define XP70_RESET_DONE                         0x01

#define XP70_STATE_BOOTING                      0x01
#define XP70_STATE_READY                        0x01
#define XP70_STATE_PROCESSING                   0x04

/****************************************** Parameters API */
/* TOP CONFIG*/
#define HQVDPLITE_CONFIG_PROGRESSIVE                /*0x01*/ TOP_CONFIG_PROGRESSIVE_MASK   /*& (1 << TOP_CONFIG_PROGRESSIVE_SHIFT)*/
#define HQVDPLITE_CONFIG_TOP_NOT_BOT                /*0x02*/ TOP_CONFIG_TOP_NOT_BOT_MASK   /*& (1 << TOP_CONFIG_TOP_NOT_BOT_SHIFT)*/
#define HQVDPLITE_CONFIG_IT_EOP                     /*0x04*/ TOP_CONFIG_IT_ENABLE_EOP_MASK /*& (1 << TOP_CONFIG_IT_ENABLE_EOP_SHIFT)*/
#define HQVDPLITE_CONFIG_PASS_THRU                  /*0x08*/ TOP_CONFIG_PASS_THRU_MASK     /*& (1 << TOP_CONFIG_PASS_THRU_SHIFT)*/

#define HQVDPLITE_CONFIG_NB_OF_FIELD_3              /*0x00*/ TOP_CONFIG_NB_OF_FIELD_MASK   & (TOP_CONFIG_NB_OF_FIELD_3 << TOP_CONFIG_NB_OF_FIELD_SHIFT)
#define HQVDPLITE_CONFIG_NB_OF_FIELD_5              /*0x20*/ TOP_CONFIG_NB_OF_FIELD_MASK   & (TOP_CONFIG_NB_OF_FIELD_5 << TOP_CONFIG_NB_OF_FIELD_SHIFT)

/* TOP MEM FORMAT */
#define HQVDPLITE_INPUT_FORMAT_420_RASTER_DUAL_BUFFER    0x00
#define HQVDPLITE_INPUT_FORMAT_422_RASTER_DUAL_BUFFER    0x01
#define HQVDPLITE_INPUT_FORMAT_422_RASTER_SINGLE_BUFFER  0x05
#define HQVDPLITE_INPUT_FORMAT_444_RSB_WITH_ALPHA        0x06
#define HQVDPLITE_INPUT_FORMAT_444_RASTER_SINGLE_BUFFER  0x07
#define HQVDPLITE_INPUT_FORMAT_420_MB_DUAL_BUFFER        0x08
#define HQVDPLITE_MEM_FORMAT_420RASTER_DUAL_BUFFER       TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_420_RASTER_DUAL_BUFFER   << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_422RASTER_SINGLE_BUFFER     TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_422_RASTER_SINGLE_BUFFER << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_420MACROBLOCK               TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_420_MB_DUAL_BUFFER       << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_444RASTER_SINGLE_BUFFER     TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_444_RASTER_SINGLE_BUFFER << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_444_RSB_WITH_ALPHA          TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_444_RSB_WITH_ALPHA       << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_422RASTER_DUAL_BUFFER       TOP_MEM_FORMAT_INPUT_FORMAT_MASK   & (HQVDPLITE_INPUT_FORMAT_422_RASTER_DUAL_BUFFER   << TOP_MEM_FORMAT_INPUT_FORMAT_SHIFT)

#define HQVDPLITE_INPUT_PIX10BIT_NOT_8BIT                0x1
#define HQVDPLITE_MEM_FORMAT_PIX10BIT_NOT_8BIT           TOP_MEM_FORMAT_PIX10BIT_NOT_8BIT_MASK & (HQVDPLITE_INPUT_PIX10BIT_NOT_8BIT << TOP_MEM_FORMAT_PIX10BIT_NOT_8BIT_SHIFT)

#define HQVDPLITE_OUTPUT_FORMAT_420_RASTER_DUAL_BUFFER   0x0
#define HQVDPLITE_OUTPUT_FORMAT_422_RASTER_SINGLE_BUFFER 0x5
#define HQVDPLITE_OUTPUT_FORMAT_444_RASTER_SINGLE_BUFFER 0x6
#define HQVDPLITE_MEM_FORMAT_OUTPUTFORMAT_420            TOP_MEM_FORMAT_OUTPUT_FORMAT_MASK & (HQVDPLITE_OUTPUT_FORMAT_420_RASTER_DUAL_BUFFER   <<TOP_MEM_FORMAT_OUTPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_OUTPUTFORMAT_422            TOP_MEM_FORMAT_OUTPUT_FORMAT_MASK & (HQVDPLITE_OUTPUT_FORMAT_422_RASTER_SINGLE_BUFFER <<TOP_MEM_FORMAT_OUTPUT_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_OUTPUTFORMAT_444            TOP_MEM_FORMAT_OUTPUT_FORMAT_MASK & (HQVDPLITE_OUTPUT_FORMAT_444_RASTER_SINGLE_BUFFER <<TOP_MEM_FORMAT_OUTPUT_FORMAT_SHIFT)

#define HQVDPLITE_OUTPUT_ALPHA                           0x80
#define HQVDPLITE_MEM_FORMAT_OUTPUTALPHA                 TOP_MEM_FORMAT_OUTPUT_ALPHA_MASK & (HQVDPLITE_OUTPUT_ALPHA << TOP_MEM_FORMAT_OUTPUT_ALPHA_SHIFT)

#define HQVDPLITE_MEM_FORMAT_MEM_TO_TV                   0x1
#define HQVDPLITE_MEM_FORMAT_MEM_TO_MEM                  0x0
#define HQVDPLITE_MEM_FORMAT_MEMTOTV                     TOP_MEM_FORMAT_MEM_TO_TV_MASK & (HQVDPLITE_MEM_FORMAT_MEM_TO_TV << TOP_MEM_FORMAT_MEM_TO_TV_SHIFT)
#define HQVDPLITE_MEM_FORMAT_MEMTOMEM                    TOP_MEM_FORMAT_MEM_TO_MEM_MASK & (~HQVDPLITE_MEM_FORMAT_MEM_TO_TV << TOP_MEM_FORMAT_MEM_TO_MEM_SHIFT)

#define HQVDPLITE_FLAT_3D                                0x1
#define HQVDPLITE_FLAT_3D_NONE                           0x0
#define HQVDPLITE_MEM_FORMAT_FLAT3D                      TOP_MEM_FORMAT_FLAT3D_MASK & (HQVDPLITE_FLAT_3D << TOP_MEM_FORMAT_FLAT3D_SHIFT)
#define HQVDPLITE_MEM_FORMAT_FLAT3D_NONE                 TOP_MEM_FORMAT_FLAT3D_MASK & (HQVDPLITE_FLAT_3D_NONE << TOP_MEM_FORMAT_FLAT3D_SHIFT)

#define HQVDPLITE_VSYNC_LEFT                             0x0
#define HQVDPLITE_VSYNC_RIGHT                            0x1
#define HQVDPLITE_MEM_FORMAT_RIGHT                       TOP_MEM_FORMAT_RIGHTNOTLEFT_MASK & (HQVDPLITE_VSYNC_RIGHT << TOP_MEM_FORMAT_RIGHTNOTLEFT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_LEFT                        TOP_MEM_FORMAT_RIGHTNOTLEFT_MASK & (HQVDPLITE_VSYNC_LEFT << TOP_MEM_FORMAT_RIGHTNOTLEFT_SHIFT)

#define HQVDPLITE_MEM_FORMAT_OUTPUT_2D                   0x0
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3D_SBS               0x1
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3D_TAB               0x2
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3D_FP                0x3
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_2D          TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_OUTPUT_2D << TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_SBS         TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_OUTPUT_3D_SBS << TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_TAB         TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_OUTPUT_3D_TAB << TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_FP          TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_OUTPUT_3D_FP << TOP_MEM_FORMAT_OUTPUT_3D_FORMAT_SHIFT)

#define HQVDPLITE_MEM_FORMAT_PIX8TO10_Y                  0x2
#define HQVDPLITE_MEM_FORMAT_PIX8TO10LSB_Y               TOP_MEM_FORMAT_PIX8TO10_LSB_Y_MASK & (HQVDPLITE_MEM_FORMAT_PIX8TO10_Y << TOP_MEM_FORMAT_PIX8TO10_LSB_Y_SHIFT)

#define HQVDPLITE_MEM_FORMAT_PIX8TO10_C                  0x0
#define HQVDPLITE_MEM_FORMAT_PIX8TO10LSB_C               TOP_MEM_FORMAT_PIX8TO10_LSB_C_MASK & (HQVDPLITE_MEM_FORMAT_PIX8TO10_C << TOP_MEM_FORMAT_PIX8TO10_LSB_C_SHIFT)

#define HQVDPLITE_MEM_FORMAT_INPUT_2D                    0x0
#define HQVDPLITE_MEM_FORMAT_INPUT_3D_SBS                0x1
#define HQVDPLITE_MEM_FORMAT_INPUT_3D_TAB                0x2
#define HQVDPLITE_MEM_FORMAT_INPUT_3D_FP                 0x3
#define HQVDPLITE_MEM_FORMAT_INPUT_DOLBY_BL              0x4
#define HQVDPLITE_MEM_FORMAT_INPUT_DOLBY_BL_EL           0x5
#define HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_2D           TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_INPUT_2D << TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_SBS          TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_INPUT_3D_SBS << TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_TAB          TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_INPUT_3D_TAB << TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT)
#define HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_FP           TOP_MEM_FORMAT_INPUT_3D_FORMAT_MASK & (HQVDPLITE_MEM_FORMAT_INPUT_3D_FP << TOP_MEM_FORMAT_INPUT_3D_FORMAT_SHIFT)

#define HQVDPLITE_LUMA_MBSRCPITCH_OFFSET                 TOP_LUMA_SRC_PITCH_MBSRCPITCH_SHIFT
#define HQVDPLITE_CHROMA_MBSRCPITCH_OFFSET               TOP_CHROMA_SRC_PITCH_MBSRCPITCH_SHIFT

#define HQVDPLITE_INPUT_PICTURE_SIZE_WIDTH_MASK          TOP_INPUT_FRAME_SIZE_WIDTH_MASK
#define HQVDPLITE_INPUT_PICTURE_SIZE_WIDTH_SHIFT         TOP_INPUT_FRAME_SIZE_WIDTH_SHIFT
#define HQVDPLITE_INPUT_PICTURE_SIZE_HEIGHT_MASK         TOP_INPUT_FRAME_SIZE_HEIGHT_MASK
#define HQVDPLITE_INPUT_PICTURE_SIZE_HEIGHT_SHIFT        TOP_INPUT_FRAME_SIZE_HEIGHT_SHIFT

#define HQVDPLITE_VIEWPORT_ORIG_X_MASK                   TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_MASK
#define HQVDPLITE_VIEWPORT_ORIG_X_SHIFT                  TOP_INPUT_VIEWPORT_ORI_VIEWPORTX_SHIFT
#define HQVDPLITE_VIEWPORT_ORIG_Y_MASK                   TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_MASK
#define HQVDPLITE_VIEWPORT_ORIG_Y_SHIFT                  TOP_INPUT_VIEWPORT_ORI_VIEWPORTY_SHIFT

#define HQVDPLITE_VIEWPORT_SIZE_WIDTH_MASK               TOP_INPUT_VIEWPORT_SIZE_WIDTH_MASK
#define HQVDPLITE_VIEWPORT_SIZE_WIDTH_SHIFT              TOP_INPUT_VIEWPORT_SIZE_WIDTH_SHIFT
#define HQVDPLITE_VIEWPORT_SIZE_HEIGHT_MASK              TOP_INPUT_VIEWPORT_SIZE_HEIGHT_MASK
#define HQVDPLITE_VIEWPORT_SIZE_HEIGHT_SHIFT             TOP_INPUT_VIEWPORT_SIZE_HEIGHT_SHIFT

#define HQVDPLITE_CRC_DEFAULT_RESET_VALUE                TOP_CRC_RESET_CTRL_MASK    /* Reset all CRC (Bits 0-20) */

/* VC1 PARAM CTRL */
#define HQVDPLITE_VC1_CONFIG_ENABLE_MASK  0x00000001

/* DEI */
/* NEXT_NOT_PREV: */
#define HQVDPLITE_DEI_CTL_NEXT_NOT_PREV_MASK          0x1
#define HQVDPLITE_DEI_CTL_NEXT_NOT_PREV_EMP           28
#define HQVDPLITE_DEI_CTL_NEXT_NOT_PREV_USE_PREV      0
#define HQVDPLITE_DEI_CTL_NEXT_NOT_PREV_USE_NEXT      1
/* DIR_REC_EN: */
#define HQVDPLITE_DEI_CTL_DIR_REC_EN_MASK             0x01
#define HQVDPLITE_DEI_CTL_DIR_REC_EN_EMP              11
#define HQVDPLITE_DEI_CTL_DIR_RECURSIVITY_OFF         0
#define HQVDPLITE_DEI_CTL_DIR_RECURSIVITY_ON          1
/* Diag ModeD: Detail Detector */
#define HQVDPLITE_DEI_CTL_DD_MODE_MASK                0x01
#define HQVDPLITE_DEI_CTL_DD_MODE_EMP                 9
#define HQVDPLITE_DEI_CTL_DD_MODE_DIAG7               0
#define HQVDPLITE_DEI_CTL_DD_MODE_DIAG9               1
/* KMOV_FACTOR : Defines the motion factor. */
#define HQVDPLITE_DEI_CTL_KMOV_FACTOR_MASK            0x0F
#define HQVDPLITE_DEI_CTL_KMOV_FACTOR_EMP             20
#define HQVDPLITE_DEI_CTL_KMOV_14_16                  14
#define HQVDPLITE_DEI_CTL_KMOV_12_16                  12
#define HQVDPLITE_DEI_CTL_KMOV_10_16                  10
#define HQVDPLITE_DEI_CTL_KMOV_8_16                   8
/*T_DETAIL : */
#define HQVDPLITE_DEI_CTL_T_DETAIL_MASK               0x0F
#define HQVDPLITE_DEI_CTL_T_DETAIL_EMP                16
/* KCOR : Correction factor used in the fader.Unsigned fractional value,   */
#define HQVDPLITE_DEI_CTL_KCOR_MASK                   0x0F
#define HQVDPLITE_DEI_CTL_KCOR_EMP                    12
/* Reduce Motion */
#define HQVDPLITE_DEI_CTL_REDUCE_MOTION_MASK          0x1
#define HQVDPLITE_DEI_CTL_REDUCE_MOTION_EMP           24
#define HQVDPLITE_DEI_CTL_8_BITS_MOTION               0
#define HQVDPLITE_DEI_CTL_4_BITS_MOTION               1
/* MD: Motion Detector */
#define HQVDPLITE_DEI_CTL_MD_MODE_MASK                CSDI_CONFIG_MD_MODE_MASK
#define HQVDPLITE_DEI_CTL_MD_MODE_EMP                 CSDI_CONFIG_MD_MODE_SHIFT
#define HQVDPLITE_DEI_CTL_MD_MODE_OFF                 0x00
#define HQVDPLITE_DEI_CTL_MD_MODE_INIT                0x01
#define HQVDPLITE_DEI_CTL_MD_MODE_LOW                 0x02
#define HQVDPLITE_DEI_CTL_MD_MODE_FULL                0x03
#define HQVDPLITE_DEI_CTL_MD_MODE_NO_RECURSIVE        0x04
/* CHROMA_INTERP : Defines the interpolation algorithm used for chroma */
#define HQVDPLITE_DEI_CTL_CHROMA_INTERP_MASK          CSDI_CONFIG_CHROMA_INTERP_MASK
#define HQVDPLITE_DEI_CTL_CHROMA_INTERP_EMP           CSDI_CONFIG_CHROMA_INTERP_SHIFT
#define HQVDPLITE_DEI_CTL_CHROMA_VI                   0x00
#define HQVDPLITE_DEI_CTL_CHROMA_DI                   0x01
#define HQVDPLITE_DEI_CTL_CHROMA_MEDIAN               0x02
#define HQVDPLITE_DEI_CTL_CHROMA_3D                   0x03
/* LUMA_INTERP : Defines the interpolation algorithm used for luma */
#define HQVDPLITE_DEI_CTL_LUMA_INTERP_MASK            CSDI_CONFIG_LUMA_INTERP_MASK
#define HQVDPLITE_DEI_CTL_LUMA_INTERP_EMP             CSDI_CONFIG_LUMA_INTERP_SHIFT
#define HQVDPLITE_DEI_CTL_LUMA_VI                     0x00
#define HQVDPLITE_DEI_CTL_LUMA_DI                     0x01
#define HQVDPLITE_DEI_CTL_LUMA_MEDIAN                 0x02
#define HQVDPLITE_DEI_CTL_LUMA_3D                     0x03
/* MAIN_MODE : Selects a group of configurations for the deinterlacer */
#define HQVDPLITE_DEI_CTL_MAIN_MODE_MASK              CSDI_CONFIG_MAIN_MODE_MASK
#define HQVDPLITE_DEI_CTL_MAIN_MODE_EMP               CSDI_CONFIG_MAIN_MODE_SHIFT
#define HQVDPLITE_DEI_CTL_MAIN_MODE_BYPASS_ON         0x00
#define HQVDPLITE_DEI_CTL_MAIN_MODE_FIELD_MERGING     0x01
#define HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING     0x02
/* Median or DMDI mode */
#define HQVDPLITE_DEI_CTL_MEDIAN_DMDI_MASK            0x1
#define HQVDPLITE_DEI_CTL_DMDI                        0x01
#define HQVDPLITE_DEI_CTL_MEDIAN_DMDI_EMP             10

#define HQVDPLITE_DEI_3D_PARAMS_MASK                  ((HQVDPLITE_DEI_CTL_DD_MODE_MASK      << HQVDPLITE_DEI_CTL_DD_MODE_EMP)       | \
                                                       (HQVDPLITE_DEI_CTL_DIR_REC_EN_MASK   << HQVDPLITE_DEI_CTL_DIR_REC_EN_EMP)    | \
                                                       (HQVDPLITE_DEI_CTL_KCOR_MASK         << HQVDPLITE_DEI_CTL_KCOR_EMP)          | \
                                                       (HQVDPLITE_DEI_CTL_T_DETAIL_MASK     << HQVDPLITE_DEI_CTL_T_DETAIL_EMP)      | \
                                                       (HQVDPLITE_DEI_CTL_KMOV_FACTOR_MASK  << HQVDPLITE_DEI_CTL_KMOV_FACTOR_EMP))

/* 3D DEI recommended values */
#define HQVDPLITE_DEI_CTL_BEST_KCOR_VALUE             12  /* recommended value for KCOR */
#define HQVDPLITE_DEI_CTL_BEST_DETAIL_VALUE           15  /* T_DETAIL has been increased in order to reduce the noise sensibility */
#define HQVDPLITE_DEI_CTL_BEST_KMOV_VALUE             HQVDPLITE_DEI_CTL_KMOV_12_16

#define HQVDPLITE_CSDI_CONFIG2_INFLEXION_MOTION       0x1
#define HQVDPLITE_CSDI_CONFIG2_FIR_LENGTH             0x2
#define HQVDPLITE_CSDI_CONFIG2_MOTION_FIR             0x1
#define HQVDPLITE_CSDI_CONFIG2_MOTION_CLOSING         0x1
#define HQVDPLITE_CSDI_CONFIG2_DETAILS                0x0
#define HQVDPLITE_CSDI_CONFIG2_CHROMA_MOTION          0x1
#define HQVDPLITE_CSDI_CONFIG2_ADAPTIVE_FADING        0x1

#define HQVDPLITE_CSDI_DCDI_CONFIG_ALPHA_LIMIT              0x20
#define HQVDPLITE_CSDI_DCDI_CONFIG_MEDIAN_MODE              0x3
#define HQVDPLITE_CSDI_DCDI_CONFIG_WIN_CLAMP_SIZE           0x8
#define HQVDPLITE_CSDI_DCDI_CONFIG_MEDIAN_BLEND_ANGLE_DEP   0x1
#define HQVDPLITE_CSDI_DCDI_CONFIG_ALPHA_MEDIAN_EN          0x1


/* HVSRC FILTERING_CTRL */


/* HVSRC PARAM_CTRL */
#define V_AUTOKOVS_DEFAULT                  0x0  /* Disable Auto Control as it doesn't work well */
#define V_AUTOKOVS_SHIFT                    HVSRC_PARAM_CTRL_V_AUTOKOVS_SHIFT
#define V_AUTOKOVS_MASK                     HVSRC_PARAM_CTRL_V_AUTOKOVS_MASK
#define H_AUTOKOVS_DEFAULT                  0x0  /* Disable Auto Control as it doesn't work well */
#define H_AUTOKOVS_SHIFT                    HVSRC_PARAM_CTRL_H_AUTOKOVS_SHIFT
#define H_AUTOKOVS_MASK                     HVSRC_PARAM_CTRL_H_AUTOKOVS_MASK

#define YH_KOVS_DISABLE                     0x8  /* Overshoot disabled in downscaling (8=off) */
#define YH_KOVS_MAX                         0x0  /* Overshoot enabled in upscaling (0=max)*/
#define YH_KOVS_SHIFT                       HVSRC_PARAM_CTRL_YH_KOVS_SHIFT
#define YH_KOVS_MASK                        HVSRC_PARAM_CTRL_YH_KOVS_MASK
#define YV_KOVS_DISABLE                     0x8
#define YV_KOVS_MAX                         0x0
#define YV_KOVS_SHIFT                       HVSRC_PARAM_CTRL_YV_KOVS_SHIFT
#define YV_KOVS_MASK                        HVSRC_PARAM_CTRL_YV_KOVS_MASK
#define CH_KOVS_DISABLE                     0x8
#define CH_KOVS_MAX                         0x0
#define CH_KOVS_SHIFT                       HVSRC_PARAM_CTRL_CH_KOVS_SHIFT
#define CH_KOVS_MASK                        HVSRC_PARAM_CTRL_CH_KOVS_MASK
#define CV_KOVS_DISABLE                     0x8
#define CV_KOVS_MAX                         0x0
#define CV_KOVS_SHIFT                       HVSRC_PARAM_CTRL_CV_KOVS_SHIFT
#define CV_KOVS_MASK                        HVSRC_PARAM_CTRL_CV_KOVS_MASK

#define H_ACON_DEFAULT                      0x1  /* Antiringing enabled in upscaling */
#define H_ACON_DISABLE                      0x0  /* Antiringing disabled in downscaling */
#define H_ACON_SHIFT                        HVSRC_PARAM_CTRL_H_ACON_SHIFT
#define H_ACON_MASK                         HVSRC_PARAM_CTRL_H_ACON_MASK
#define V_ACON_DEFAULT                      0x1
#define V_ACON_DISABLE                      0x0
#define V_ACON_SHIFT                        HVSRC_PARAM_CTRL_V_ACON_SHIFT
#define V_ACON_MASK                         HVSRC_PARAM_CTRL_V_ACON_MASK


/* IQI */
#define IQI_CONFIG_DISABLE_DEFAULT          0x00000001
#define IQI_PK_CONFIG_DEFAULT               0x00000000
#define IQI_PK_LUT0                         0x00000000
#define IQI_PK_GAIN_DEFAULT                 0x00000404
#define IQI_PK_CORING_LEVEL_DEFAULT         0x00000000
#define IQI_LTI_CONFIG_DEFAULT              0x00000000
#define IQI_CTI_CONFIG_DEFAULT              0x000600C4
#define IQI_LE_CONFIG_DEFAULT               0x00000000
#define IQI_LE_LUT_CONFIG_DEFAULT           0x00000000
#define PROG_TO_INT_BYPASS_ENABLE           0x00000001
#define PROG_TO_INT_PRESENT_TOP_FIELD       0x00000002

#define IQI_PK_CONFIG_ENABLE                0x00000004
#define IQI_CTI_CONFIG_ENABLE               0x00000010
#define IQI_LE_CONFIG_ENABLE                0x00000030
#define IQI_CSC_CONFIG_ENABLE               0x00000070
#define IQI_PK_CONFIG_DEMO                  0x00020000
#define IQI_CTI_CONFIG_DEMO                 0x00100000
#define IQI_LE_CONFIG_DEMO                  0x00010000




#define DEBUG_ASSIGN(backup, src)    do{ (backup) = (src); }while(0)

/* bit field Setting */
static inline void _SetBitField(uint32_t & value, const uint32_t shift, const uint32_t mask, const uint32_t bit_field_value)
{
    value &= ~mask;
    value |= ((bit_field_value << shift) & mask);
}
#define SetBitField(value, field, bit_field_value) \
    _SetBitField(value, field ## _SHIFT, field ## _MASK, bit_field_value)


static inline void SetDeiMainMode(uint32_t & dei_ctl,  uint32_t  mode)
{
    dei_ctl &= ~HQVDPLITE_DEI_CTL_MAIN_MODE_MASK;     /* Clear the bits in the mask */
    dei_ctl |= ((mode << HQVDPLITE_DEI_CTL_MAIN_MODE_EMP) & HQVDPLITE_DEI_CTL_MAIN_MODE_MASK);
}

static inline void SetDeiMotionState(uint32_t & dei_ctl,  stm_hqvdplite_HQVDPLITE_DEI_MOTION_state motionState)
{
    /* stm_hqvdplite_HQVDPLITE_DEI_MOTION_state values are the same as HQVDPLITE MD mode configuration parameters */
    TRC( TRC_ID_UNCLASSIFIED, "ms = %d", motionState );
    dei_ctl &= ~HQVDPLITE_DEI_CTL_MD_MODE_MASK;     /* Clear the bits in the mask */
    dei_ctl |= (((uint32_t)motionState << HQVDPLITE_DEI_CTL_MD_MODE_EMP)  & HQVDPLITE_DEI_CTL_MD_MODE_MASK);
}

static inline void SetDeiInterpolation(uint32_t & dei_ctl,  uint32_t  lumaMode, uint32_t  chromaMode)
{
    SetBitField(dei_ctl, CSDI_CONFIG_LUMA_INTERP, lumaMode);
    SetBitField(dei_ctl, CSDI_CONFIG_CHROMA_INTERP, chromaMode);
}

CHqvdpLitePlane::CHqvdpLitePlane (const char                     *name,
                                  uint32_t                        id,
                                  const CDisplayDevice           *pDev,
                                  const stm_plane_capabilities_t  caps,
                                  CVideoPlug                     *pVideoPlug,
                                  uint32_t                        hqvdpLiteBaseAddress): CDisplayPlane (name, id, pDev, caps)
{
    TRCIN( TRC_ID_MAIN_INFO, ", self: %p, id: %u", this, id );

    vibe_os_zero_memory( &m_CmdPrepared, sizeof( m_CmdPrepared ));
    m_ulDEIPixelBufferLength = 0;
    m_ulDEIPixelBufferHeight = 0;
    vibe_os_zero_memory( &m_HQVdpLiteCrcData, sizeof( m_HQVdpLiteCrcData ));
    m_bUseFMD = false;
    vibe_os_zero_memory( &m_SharedArea1, sizeof( m_SharedArea1 ));
    vibe_os_zero_memory( &m_SharedArea2, sizeof( m_SharedArea2 ));
    m_SharedAreaPreparedForNextVSync_p = 0;
    m_SharedAreaUsedByFirmware_p = 0;
    vibe_os_zero_memory( &m_pPreviousFromFirmware0, sizeof( m_pPreviousFromFirmware0 ));

    m_pSurfaceFormats = g_surfaceFormats;
    m_nFormats        = N_ELEMENTS (g_surfaceFormats);

    m_pFeatures       = g_hqvdplitePlaneFeatures;
    m_nFeatures       = N_ELEMENTS (g_hqvdplitePlaneFeatures);

    m_baseAddress     = (uint32_t *)((uint8_t *)pDev->GetCtrlRegisterBase() + hqvdpLiteBaseAddress);
    m_mbxBaseAddress  = (uint32_t *)((uint8_t *)pDev->GetCtrlRegisterBase() + hqvdpLiteBaseAddress + HQVDPLITE_MBX_OFFSET);
    m_videoPlug       = pVideoPlug;

    m_MotionState        = HQVDPLITE_DEI_MOTION_INIT;

    m_bHasProgressive2InterlacedHW = true;

    m_newCmdSendToFirmware      = false;
    m_bReportPixelFIFOUnderflow = true;

    vibe_os_zero_memory (&m_MotionBuffersArea, sizeof (DMA_Area));
    m_MotionBuffersSize = 0;

    m_PreviousMotionBufferPhysAddr = 0;
    m_CurrentMotionBufferPhysAddr  = 0;
    m_NextMotionBufferPhysAddr     = 0;

    m_hqvdpLiteDisplayInfo.Reset();
    ResetEveryUseCases();

    /*to do later, just init here */

    m_FilterSetLuma   = HVSRC_LUT_Y_MEDIUM;
    m_FilterSetChroma = HVSRC_LUT_C_OPT;

    m_isIQIChanged = false;
    m_hqvdpClockFreqInMHz = 0;
    m_iqiConfig = 0;
    m_iqiDefaultColorState = false;
    m_iqiDefaultColorRGB = INIT_DEFAULT_COLOR;
    m_iqiDefaultColorYCbCr = 0;
    m_hasADeinterlacer = true;

    vibe_os_zero_memory( &m_peaking_setup, sizeof( m_peaking_setup ));
    vibe_os_zero_memory( &m_le_setup, sizeof( m_le_setup ));

    TRCOUT( TRC_ID_UNCLASSIFIED, ", self: %p", this );
}

CHqvdpLitePlane::~CHqvdpLitePlane(void)
{
    int r = 0;
    TRCIN( TRC_ID_MAIN_INFO, "" );

    r = vibe_os_revoke_access(m_MotionBuffersArea.pMemory,
                             m_MotionBuffersSize,
                             VIBE_ACCESS_TID_DISPLAY,
                             VIBE_ACCESS_O_WRITE);
    if (r<0)
    {
      TRC(TRC_ID_SDP, "vibe_os_revoke_access returns error %d", r);
    }

    r = vibe_os_destroy_region(m_MotionBuffersArea.pMemory,
                               m_MotionBuffersSize);
    if (r<0)
    {
        TRC(TRC_ID_SDP, "vibe_os_destroy_region returns error %d", r);
    }
    vibe_os_free_dma_area (&m_MotionBuffersArea);

    XP70Term();

    TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CHqvdpLitePlane::Create (void)
{
    unsigned int motion_buffer_size = 0;
    int r = 0;
    TRCIN( TRC_ID_MAIN_INFO, "" );

    if (!CDisplayPlane::Create ())
        return false;

    m_fixedpointONE = 1<<13;              /* n.13 fixed point for DISP rescaling */
    m_ulMaxHSrcInc  = 4*m_fixedpointONE;  /* downscale 4x */
    m_ulMinHSrcInc  = m_fixedpointONE/32; /* upscale 32x */
    m_ulMaxVSrcInc  = 4*m_fixedpointONE;
    m_ulMinVSrcInc  = m_fixedpointONE/32;

    m_bUseFMD    = false;

    m_ulDEIPixelBufferLength = 1920;
    m_ulDEIPixelBufferHeight = 1088;

    SetScalingCapabilities(&m_rescale_caps);

#define MOTION_BUFFER_ALIGN     16

    /* Each motion buffer has the size of a luma FIELD (= half of a frame)
       and contains information about luma variation in each part of the picture */
    motion_buffer_size = _ALIGN_UP(m_ulDEIPixelBufferLength, STBUS_NBR_OF_BYTES_PER_WORD) * m_ulDEIPixelBufferHeight / 2;
    motion_buffer_size = _ALIGN_UP(motion_buffer_size, MOTION_BUFFER_ALIGN); /* to start the next buffer at address multiple of 16 bytes */
    m_MotionBuffersSize = motion_buffer_size * HQVDPLITE_DEI_MOTION_BUFFERS;

    vibe_os_allocate_dma_area(&m_MotionBuffersArea,
                              m_MotionBuffersSize,
                              MOTION_BUFFER_ALIGN,
                              SDAAF_SECURE);
    if (!m_MotionBuffersArea.pMemory)
    {
        TRC( TRC_ID_ERROR, "Could not allocate DMA memory for Motion Buffer");
        return false;
    }

    m_PreviousMotionBufferPhysAddr = m_MotionBuffersArea.ulPhysical;
    m_CurrentMotionBufferPhysAddr  = m_PreviousMotionBufferPhysAddr + motion_buffer_size;
    m_NextMotionBufferPhysAddr     = m_CurrentMotionBufferPhysAddr + motion_buffer_size;

    r = vibe_os_create_region(m_MotionBuffersArea.pMemory,
                              m_MotionBuffersSize);
    if (r<0)
    {
        TRC(TRC_ID_ERROR, "vibe_os_create_region returns error %d", r);
        return false;
    }

    r = vibe_os_grant_access(m_MotionBuffersArea.pMemory,
                             m_MotionBuffersSize,
                             VIBE_ACCESS_TID_DISPLAY,
                             VIBE_ACCESS_O_WRITE);
    if (r<0)
    {
      TRC(TRC_ID_ERROR, "vibe_os_grant_access returns error %d", r);
      return false;
    }

    vibe_os_allocate_dma_area(&m_pPreviousFromFirmware0, sizeof(HQVDPLITE_CMD_t), DCACHE_LINE_SIZE, SDAAF_NONE);
    if (!m_pPreviousFromFirmware0.pMemory)
    {
        TRC( TRC_ID_ERROR, "Error during AllocateDMAArea for m_pPreviousFromFirmware0 !" );
        return false;
    }
    vibe_os_zero_memory(m_pPreviousFromFirmware0.pData, m_pPreviousFromFirmware0.ulDataSize);

    XP70Init();

    {
        struct vibe_clk hqvdp_clock = { 0, "CLK_MAIN_DISP", 0,0,0,0 };
        m_hqvdpClockFreqInMHz = vibe_os_clk_get_rate(&hqvdp_clock) / 1000000;
        if(!m_hqvdpClockFreqInMHz)
            m_hqvdpClockFreqInMHz = HQVDPLITE_DEFAULT_CLOCK_FREQUENCY;
        TRC( TRC_ID_MAIN_INFO, "HQVDP clock rate is %d MHz!", m_hqvdpClockFreqInMHz );
    }


    m_iqiPeaking.Create();
    m_iqiLe.Create();
    m_iqiCti.Create();


    m_iqiConfig = IQI_CONFIG_DISABLE_DEFAULT;
    m_iqiDefaultColorRGB = INIT_DEFAULT_COLOR;
    m_iqiDefaultColorYCbCr = 0;
    m_iqiDefaultColorState = false;

    TRCOUT( TRC_ID_MAIN_INFO, "" );
    return true;
}

DisplayPlaneResults CHqvdpLitePlane::SetCompoundControl(stm_display_plane_control_t ctrl, void * newVal)
{
    DisplayPlaneResults result = STM_PLANE_NOT_SUPPORTED;

    result = CDisplayPlane::SetCompoundControl(ctrl, newVal);
    if(result == STM_PLANE_OK)
    {
        switch(ctrl)
        {
            case PLANE_CTRL_INPUT_WINDOW_VALUE:
            {
                if ( m_pDisplayDevice->VSyncLock() != 0 )
                    return STM_PLANE_EINTR;
                m_MotionState = HQVDPLITE_DEI_MOTION_OFF;
                m_pDisplayDevice->VSyncUnlock();
                break;
            }
            default:
                break;
        }
    }
    // Default, return failure for an unsupported control
    return result;
}

bool CHqvdpLitePlane::GetCompoundControlRange(stm_display_plane_control_t selector, stm_compound_control_range_t *range)
{

    if(selector == PLANE_CTRL_INPUT_WINDOW_VALUE)
    {
        range->min_val.x = 0;
        range->min_val.y = 0;
        range->min_val.width = 48;
        range->min_val.height = 16;

        range->max_val.x = 0;
        range->max_val.y = 0;
        range->max_val.width = 4096;
        range->max_val.height = 2160;

        range->default_val.x = 0;
        range->default_val.y = 0;
        range->default_val.width = 0;
        range->default_val.height = 0;

        range->step.x = 1;
        range->step.y = 1;
        range->step.width = 1;
        range->step.height = 1;
    }
    else
    {
        range->min_val.x = 0;
        range->min_val.y = 0;
        range->min_val.width = 48;
        range->min_val.height = 16;

        range->max_val.x = 0;
        range->max_val.y = 0;
        range->max_val.width = 4096;
        range->max_val.height = 2160;

        range->default_val.x = 0;
        range->default_val.y = 0;
        range->default_val.width = 0;
        range->default_val.height = 0;

        range->step.x = 1;
        range->step.y = 1;
        range->step.width = 1;
        range->step.height = 1;
    }
    return true;
}

DisplayPlaneResults CHqvdpLitePlane::SetControl (stm_display_plane_control_t control, uint32_t value)
{
    DisplayPlaneResults retval = STM_PLANE_NOT_SUPPORTED;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    switch (control)
    {
        case PLANE_CTRL_INPUT_WINDOW_MODE:
        {
            retval = CDisplayPlane::SetControl (control, value);
            if(retval == STM_PLANE_OK)
            {
                if ( m_pDisplayDevice->VSyncLock() != 0)
                    return STM_PLANE_EINTR;
                m_MotionState = HQVDPLITE_DEI_MOTION_OFF;
                m_pDisplayDevice->VSyncUnlock();
            }
            break;
        }

        case PLANE_CTRL_HIDE_MODE_POLICY:
        {
            switch (static_cast<stm_plane_ctrl_hide_mode_policy_t>(value))
            {
                case PLANE_HIDE_BY_MASKING:
                case PLANE_HIDE_BY_DISABLING:
                {
                    if ( m_pDisplayDevice->VSyncLock() != 0)
                        return STM_PLANE_EINTR;
                    m_eHideMode = static_cast<stm_plane_ctrl_hide_mode_policy_t>(value);
                    m_pDisplayDevice->VSyncUnlock();
                    TRC( TRC_ID_MAIN_INFO, "New m_eHideMode=%u",  m_eHideMode );
                    retval = STM_PLANE_OK;
                    break;
                }
                default:
                {
                    retval = STM_PLANE_INVALID_VALUE;
                    break;
                }
            }
            break;
        }
        case PLANE_CTRL_VIDEO_MADI_STATE:
        {
            switch (static_cast<stm_plane_control_state_t>(value))
            {
                case CONTROL_ON:
                {
                    retval = STM_PLANE_OK;
                    break;
                }

                /* For HQVDPLITE DEI should always be on so off setting is not supported */
                case CONTROL_OFF:
                case CONTROL_ONLY_LEFT_ON:
                case CONTROL_ONLY_RIGHT_ON:
                {
                    retval = STM_PLANE_NOT_SUPPORTED;
                    break;
                }

                default:
                {
                    retval = STM_PLANE_INVALID_VALUE;
                    break;
                }
            }
            break;
        }
        case PLANE_CTRL_COLOR_FILL_MODE:
        {
          switch (static_cast<stm_plane_mode_t>(value))
          {
            case MANUAL_MODE:
            {
                retval = STM_PLANE_OK;
                break;
            }
            case AUTO_MODE:
            {
                retval = STM_PLANE_NOT_SUPPORTED;
                break;
            }
            default:
            {
                retval = STM_PLANE_INVALID_VALUE;
                break;
            }
          }
          break;
        }
        case PLANE_CTRL_COLOR_FILL_STATE:
        {
            uint32_t tmp_colorspace = 0;
            stm_ycbcr_colorspace_t colorspace = STM_YCBCR_COLORSPACE_709;
            if (value)
            {
                m_iqiDefaultColorState = true;
            } else {
                m_iqiDefaultColorState = false;
            }
            if(m_pOutput)
            {
                m_pOutput->GetControl(OUTPUT_CTRL_YCBCR_COLORSPACE, &tmp_colorspace);
                colorspace = static_cast<stm_ycbcr_colorspace_t>(tmp_colorspace);
            }
            m_iqiDC.ColorFill(m_iqiDefaultColorState, colorspace, m_iqiDefaultColorRGB, &m_iqiDefaultColorYCbCr);
            retval = STM_PLANE_OK;
            break;
        }
        case PLANE_CTRL_COLOR_FILL_VALUE:
        {
            uint32_t tmp_colorspace = 0;
            stm_ycbcr_colorspace_t colorspace = STM_YCBCR_COLORSPACE_709;
            m_iqiDefaultColorRGB = value;
            if(m_pOutput)
            {
                m_pOutput->GetControl(OUTPUT_CTRL_YCBCR_COLORSPACE, &tmp_colorspace);
                colorspace = static_cast<stm_ycbcr_colorspace_t>(tmp_colorspace);
            }
            m_iqiDC.ColorFill(m_iqiDefaultColorState, colorspace, m_iqiDefaultColorRGB, &m_iqiDefaultColorYCbCr);

            retval = STM_PLANE_OK;
            break;
        }
        case PLANE_CTRL_VIDEO_IQI_PEAKING_STATE:
        {
            m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
            switch (static_cast<stm_plane_control_state_t>(value))

              {
                case CONTROL_ON:
                    {
                        m_iqiConfig |= IQI_CONFIG_PK_ENABLE_MASK ;
                        if( m_iqiPeaking.SetPeakingPreset( PCIQIC_ST_DEFAULT)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }

                    case CONTROL_OFF:

                    {
                        m_iqiConfig &= ~IQI_CONFIG_PK_ENABLE_MASK ;
                        if( m_iqiPeaking.SetPeakingPreset(PCIQIC_BYPASS)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_SMOOTH:
                    {
                        m_iqiConfig |= IQI_CONFIG_PK_ENABLE_MASK ;
                        if( m_iqiPeaking.SetPeakingPreset( PCIQIC_ST_SOFT)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        }
                        else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_MEDIUM:
                    {
                        m_iqiConfig |= IQI_CONFIG_PK_ENABLE_MASK ;
                        if( m_iqiPeaking.SetPeakingPreset( PCIQIC_ST_MEDIUM)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_SHARP:
                    {
                        m_iqiConfig |= IQI_CONFIG_PK_ENABLE_MASK ;
                        if( m_iqiPeaking.SetPeakingPreset( PCIQIC_ST_STRONG)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    default:
                    {
                        retval = STM_PLANE_NOT_SUPPORTED;
                        break;
                    }
            }
            break;
        }
        case PLANE_CTRL_VIDEO_IQI_CTI_STATE:
        {
            TRC( TRC_ID_HQVDPLITE, "SetControl - PLANE_CTRL_VIDEO_IQI_CTI_STATE");
            m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
            switch (static_cast<stm_plane_control_state_t>(value))
              {
                    case CONTROL_OFF:
                    {
                        m_iqiConfig &= ~IQI_CONFIG_CTI_ENABLE_MASK ;
                        TRC( TRC_ID_MAIN_INFO, "## CTI preset BYPASS");
                        if( m_iqiCti.SetCtiPreset(PCIQIC_BYPASS)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_ON:
                    case CONTROL_SMOOTH:
                    {
                        m_iqiConfig |= IQI_CONFIG_CTI_ENABLE_MASK ;
                        TRC( TRC_ID_MAIN_INFO, "## CTI preset ON / SMOOTH / SOFT");
                        if( m_iqiCti.SetCtiPreset( PCIQIC_ST_SOFT)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }


                    case CONTROL_MEDIUM:
                    {
                        m_iqiConfig |= IQI_CONFIG_CTI_ENABLE_MASK ;
                        TRC( TRC_ID_HQVDPLITE, "## CTI preset MEDIUM");
                        if( m_iqiCti.SetCtiPreset( PCIQIC_ST_MEDIUM)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_SHARP:
                    {
                        m_iqiConfig |= IQI_CONFIG_CTI_ENABLE_MASK ;
                        TRC( TRC_ID_HQVDPLITE, "## CTI preset STRONG");
                        if( m_iqiCti.SetCtiPreset( PCIQIC_ST_STRONG)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;

                        break;
                    }
                    default:
                    {
                        retval = STM_PLANE_NOT_SUPPORTED;
                        break;
                    }
            }
            break;
        }
       case PLANE_CTRL_VIDEO_IQI_LE_STATE:
        {
            m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
            switch (static_cast<stm_plane_control_state_t>(value))
              {
                case CONTROL_ON:
                    {
                        m_iqiConfig |= IQI_CONFIG_LE_ENABLE_MASK ;
                        if( m_iqiLe.SetLePreset( PCIQIC_ST_DEFAULT)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }

                    case CONTROL_OFF:

                    {
                        m_iqiConfig &= ~IQI_CONFIG_LE_ENABLE_MASK ;
                        if( m_iqiLe.SetLePreset(PCIQIC_BYPASS)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_SMOOTH:
                    {
                        m_iqiConfig |= IQI_CONFIG_LE_ENABLE_MASK ;
                        if( m_iqiLe.SetLePreset( PCIQIC_ST_SOFT)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        }
                        else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_MEDIUM:
                    {
                        m_iqiConfig |= IQI_CONFIG_LE_ENABLE_MASK ;
                        if( m_iqiLe.SetLePreset( PCIQIC_ST_MEDIUM)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;
                        break;
                    }
                    case CONTROL_SHARP:
                    {
                        m_iqiConfig |= IQI_CONFIG_LE_ENABLE_MASK ;
                        if( m_iqiLe.SetLePreset( PCIQIC_ST_STRONG)) {
                            m_isIQIChanged = true;
                            retval = STM_PLANE_OK;
                        } else retval =  STM_PLANE_INVALID_VALUE;

                        break;
                    }
                    default:
                    {
                        retval = STM_PLANE_NOT_SUPPORTED;
                        break;
                    }
            }
            break;
        }
        default:
        {
            retval = CDisplayPlane::SetControl (control, value);
            break;
        }
    }
    return retval;
}


DisplayPlaneResults CHqvdpLitePlane::GetControl (stm_display_plane_control_t  control, uint32_t *value) const
{
    DisplayPlaneResults retval = STM_PLANE_OK;
    enum PlaneCtrlIQIConfiguration tmp;

    TRCIN( TRC_ID_UNCLASSIFIED, "" );


    switch (control)
    {
        case PLANE_CTRL_HIDE_MODE_POLICY:
        {
            *value = m_eHideMode;
            retval = STM_PLANE_OK;
            break;
        }
        case PLANE_CTRL_COLOR_FILL_MODE:
        {
          *value = MANUAL_MODE;
          retval = STM_PLANE_OK;
          break;
        }
        case PLANE_CTRL_COLOR_FILL_STATE:
        {
          *value = m_iqiDefaultColorState;
          retval = STM_PLANE_OK;
          break;
        }
        case PLANE_CTRL_COLOR_FILL_VALUE:
        {
          *value = m_iqiDefaultColorRGB;
          retval = STM_PLANE_OK;
          break;
        }
        case PLANE_CTRL_VIDEO_IQI_PEAKING_STATE:
        {
            m_iqiPeaking.GetPeakingPreset(&tmp);
            *value = (uint32_t)tmp;
            retval = STM_PLANE_OK;
            break;

        }
        case PLANE_CTRL_VIDEO_IQI_CTI_STATE:
        {
            m_iqiCti.GetCtiPreset(&tmp);
            *value = (uint32_t)tmp;
            retval = STM_PLANE_OK;
            break;
        }
        case PLANE_CTRL_VIDEO_IQI_LE_STATE:
        {
            m_iqiLe.GetLePreset(&tmp);
            *value = (uint32_t)tmp;
            retval = STM_PLANE_OK;
            break;
        }
        default:
        {
            retval = CDisplayPlane::GetControl (control, value);
            break;
        }
    }
    return retval;
}

DisplayPlaneResults CHqvdpLitePlane::SetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * newVal)
{
    DisplayPlaneResults retval=STM_PLANE_OK;

    switch (ctrl)
    {
        case PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA:
        {
            stm_iqi_peaking_conf_t * data = ( stm_iqi_peaking_conf_t *)GET_PAYLOAD_POINTER(newVal);


            if( m_iqiPeaking.SetPeakingParams(data)) {
                TRC( TRC_ID_MAIN_INFO,"valid peaking params! ");
                m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
                m_iqiConfig |= IQI_CONFIG_PK_ENABLE_MASK ;
                m_isIQIChanged = true;
                }
            else retval =  STM_PLANE_INVALID_VALUE;
            break;
        }
        case PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA:
        {
            stm_iqi_le_conf_t * data = ( stm_iqi_le_conf_t *)GET_PAYLOAD_POINTER(newVal);
            if( m_iqiLe.SetLeParams(data)) {
                m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
                m_iqiConfig |= IQI_CONFIG_LE_ENABLE_MASK ;
                TRC( TRC_ID_MAIN_INFO,"valid le params! ");
                m_isIQIChanged = true;
                }

            else retval =  STM_PLANE_INVALID_VALUE;

            break;
        }
        case PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA:
        {
            stm_iqi_cti_conf_t * data = ( stm_iqi_cti_conf_t *)GET_PAYLOAD_POINTER(newVal);
            if( m_iqiCti.SetCtiParams(data)) {
                m_iqiConfig &= ~IQI_CONFIG_DISABLE_DEFAULT;
                m_iqiConfig |= IQI_CONFIG_CTI_ENABLE_MASK ;
                TRC( TRC_ID_MAIN_INFO,"valid peaking params! ");
                m_isIQIChanged = true;
                }
            else retval =  STM_PLANE_INVALID_VALUE;
            break;
        }
        default:
        {
            retval = STM_PLANE_NOT_SUPPORTED;
            break;
        }
    }
    return retval;
}


DisplayPlaneResults CHqvdpLitePlane::GetTuningDataControl(stm_display_plane_tuning_data_control_t ctrl, stm_tuning_data_t * currentVal)
{
    DisplayPlaneResults retval = STM_PLANE_OK;


    switch (ctrl)
    {

        case PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA:
        {
            stm_iqi_peaking_conf_t * data = ( stm_iqi_peaking_conf_t *)GET_PAYLOAD_POINTER(currentVal);
            m_iqiPeaking.GetPeakingParams(data);

            TRC( TRC_ID_HQVDPLITE,"actual peaking values: ");
            TRC( TRC_ID_HQVDPLITE,"- undershoot: %d", data->undershoot);
            TRC( TRC_ID_HQVDPLITE,"- overshoot: %d", data->overshoot);
            TRC( TRC_ID_HQVDPLITE,"- coring_mode: %d", data->coring_mode);
            TRC( TRC_ID_HQVDPLITE,"- coring_level: %d", data->coring_level);
            TRC( TRC_ID_HQVDPLITE,"- vertical_peaking: %d", data->vertical_peaking);
            TRC( TRC_ID_HQVDPLITE,"- clipping_mode: %d", data->clipping_mode);
            TRC( TRC_ID_HQVDPLITE,"- bandpass_filter_centerfreq: %d", data->bandpass_filter_centerfreq);
            TRC( TRC_ID_HQVDPLITE,"- highpass_filter_cutofffreq: %d", data->highpass_filter_cutofffreq);
            TRC( TRC_ID_HQVDPLITE,"- bandpassgain: %d", data->bandpassgain);
            TRC( TRC_ID_HQVDPLITE,"- highpassgain: %d", data->highpassgain);
            TRC( TRC_ID_HQVDPLITE,"- ver_gain: %d", data->ver_gain);



            break;
        }
        case PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA:
        {
            stm_iqi_le_conf_t * data = ( stm_iqi_le_conf_t *)GET_PAYLOAD_POINTER(currentVal);
            m_iqiLe.GetLeParams(data);

            break;
        }
        case PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA:
        {
            stm_iqi_cti_conf_t * data = ( stm_iqi_cti_conf_t *)GET_PAYLOAD_POINTER(currentVal);
            m_iqiCti.GetCtiParams(data);
            TRC( TRC_ID_HQVDPLITE, "actual cti values: ");
            TRC( TRC_ID_HQVDPLITE, "- strength1: %d", data->strength1);
            TRC( TRC_ID_HQVDPLITE, "- strength2: %d", data->strength2);

            break;
        }
        default:
        {
            retval = STM_PLANE_NOT_SUPPORTED;
            break;
        }
    }
    return retval;
}

bool CHqvdpLitePlane::GetTuningDataRevision(stm_display_plane_tuning_data_control_t ctrl, uint32_t * revision)
{
    bool result = false;

    if(!revision)
        return false;

    switch(ctrl)
    {
        case PLANE_CTRL_VIDEO_FMD_TUNING_DATA:
        case PLANE_CTRL_VIDEO_MADI_TUNING_DATA:
        case PLANE_CTRL_VIDEO_IQI_PEAKING_TUNING_DATA:
        case PLANE_CTRL_VIDEO_IQI_LE_TUNING_DATA:
        case PLANE_CTRL_VIDEO_IQI_CTI_TUNING_DATA:

        {
            *revision = 1;
            result=true;
            break;
        }
        default:
        {
            *revision = 0;
            result = false;
            break;
        }
    }
    return result;
}


bool CHqvdpLitePlane::IsFeatureApplicable( stm_plane_feature_t feature, bool *applicable) const
{
    bool is_supported = false;
    switch(feature)
    {
        case PLANE_FEAT_VIDEO_MADI:
        {
            is_supported = true;
            if(applicable)
                *applicable = true;

            break;
        }
        case PLANE_FEAT_VIDEO_FMD:
        {
            is_supported = m_bUseFMD;
            if(applicable)
                *applicable = is_supported;
            break;
        }
        default:
        {
            is_supported =  CDisplayPlane::IsFeatureApplicable(feature, applicable);
            break;
        }
    }
    return is_supported;
}



void CHqvdpLitePlane::PresentDisplayNode(CDisplayNode       *pPrevNode,
                                         CDisplayNode       *pCurrNode,
                                         CDisplayNode       *pNextNode,
                                         bool                isPictureRepeated,
                                         bool                isDisplayInterlaced,
                                         bool                isTopFieldOnDisplay,
                                         const stm_time64_t &vsyncTime)
{
    stm_display_use_cases_t     currentUseCase;
    CHqvdpLiteDisplayInfo      *pDisplayInfo = 0;

    // Check that VSyncLock is already taken before accessing to shared variables
    DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

    if(!pCurrNode)
    {
        TRC( TRC_ID_ERROR, "pCurrNode is a NULL pointer!" );
        return;
    }

    TRC( TRC_ID_HQVDPLITE, "isPictureRepeated : %u, isDisplayInterlaced : %u, isTopFieldOnDisplay : %u, vsyncTime : %lld", isPictureRepeated, isDisplayInterlaced, isTopFieldOnDisplay, vsyncTime );
    TRC( TRC_ID_PICT_QUEUE_RELEASE, "%d%c (%p)",  pCurrNode->m_pictureId, pCurrNode->m_srcPictureTypeChar, pCurrNode );

    if (!m_outputInfo.isOutputStarted)
    {
        TRC( TRC_ID_MAIN_INFO, "Output not started" );
        goto nothing_to_display;
    }

    /* get the currrent use case (this cannot fail) */
    currentUseCase = GetCurrentUseCase(pCurrNode->m_srcPictureType, isDisplayInterlaced, isTopFieldOnDisplay);

    /*
     * check whether the context is changed
     * if yes, reset and recompute everything
     */
    if(IsContextChanged(pCurrNode, isPictureRepeated))
    {
        TRC( TRC_ID_HQVDPLITE,"### Display context has changed");

        /* Reset all saved Cmds */
        ResetEveryUseCases();

        /* Recompute the DisplayInfo */
        if (!FillHqvdpLiteDisplayInfo(pCurrNode, &m_hqvdpLiteDisplayInfo, isDisplayInterlaced))
        {
            TRC( TRC_ID_ERROR, "Failed to create the HqvdpLite DisplayInfo!" );
            goto nothing_to_display;
        }
    }

    /* Prepare IQI as a separate command */
    {
        HQVDPLITE_CMD_t      *pFieldSetup = &m_savedCmd[currentUseCase].fieldSetup;
        HQVDPLITE_IQI_Params_t   *pIQIParams;
        pIQIParams = &pFieldSetup->Iqi;
        if (!PrepareCmdIQIParams (pCurrNode, &m_hqvdpLiteDisplayInfo, isTopFieldOnDisplay, isDisplayInterlaced, pIQIParams))
        {
            TRC( TRC_ID_ERROR, "Error ! Unable to prepare IQI params for command ");
            goto nothing_to_display;
        }
    }

    /* Exit if IO Windows are not valid */
    if(!m_hqvdpLiteDisplayInfo.m_areIOWindowsValid)
    {
        TRC( TRC_ID_ERROR, "IO Windows not valid" );
        goto nothing_to_display;
    }

    /* The DisplayInfo are now valid */
    pDisplayInfo = &m_hqvdpLiteDisplayInfo;

    /* Check whether a valid Cmd is available for this use case. If not, compute it */
    if(!m_savedCmd[currentUseCase].isValid)
    {
        TRC( TRC_ID_HQVDPLITE, "No valid Cmd available for this use case: Recompute the Cmd");

        /* prepare all parameters for each command */
        if(!PrepareCmd(pCurrNode, pDisplayInfo, isTopFieldOnDisplay, &m_savedCmd[currentUseCase].fieldSetup))
        {
            TRC( TRC_ID_ERROR, "Failed to prepare the Cmd!" );
            goto nothing_to_display;
        }
        m_savedCmd[currentUseCase].isValid = true;
    }

    /* Make a copy of the command that is going to be used... */
    m_CmdPrepared = m_savedCmd[currentUseCase].fieldSetup;

    /*
     * At this stage, we are sure that we have:
     *   - A valid command in "m_CmdPrepared"
     *   - Some valid DisplayInfo pointed by "pDisplayInfo"
     */

    /* Customize the fields changing all the time (pictureBaseAddresses, CSDI config...) */
    if (!SetDynamicDisplayInfo(pPrevNode, pCurrNode, pNextNode, pDisplayInfo, &m_CmdPrepared, isPictureRepeated))
    {
        TRC( TRC_ID_ERROR, "Failed to prepare the DisplayInfo changing all the time!" );
        goto nothing_to_display;
    }
    /* Send the command to the firmware */
    SendCmd(pDisplayInfo, &m_CmdPrepared);

    UpdatePictureDisplayedStatistics(pCurrNode, isPictureRepeated, isDisplayInterlaced, isTopFieldOnDisplay);

    return;

nothing_to_display:
    NothingToDisplay();
}

void CHqvdpLitePlane::Resume(void)
{
  TRCIN( TRC_ID_MAIN_INFO, "" );

  InitializeHQVDPLITE();
  CDisplayPlane::Resume();
  TRCOUT( TRC_ID_MAIN_INFO, "" );
}

bool CHqvdpLitePlane::FillHqvdpLiteDisplayInfo(CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo, bool isDisplayInterlaced)
{
    uint32_t transparency = STM_PLANE_TRANSPARENCY_OPAQUE;
    stm_viewport_t  viewport;

    /* Reset the DisplayInfo before recomputing them */
    pDisplayInfo->Reset();

    // Fill the information about the output display
    if(!FillDisplayInfo(pCurrNode, pDisplayInfo))
    {
        TRC( TRC_ID_ERROR, "FillDisplayInfo failed !!" );
        goto reset_and_exit;
    }

    if(!PrepareIOWindows(pCurrNode, pDisplayInfo))
    {
        TRC( TRC_ID_ERROR, "PrepareIOWindows() failed !!" );
        goto reset_and_exit;
    }

    /* set display hw information */
    SetHWDisplayInfos(pCurrNode, pDisplayInfo);

    if(!pDisplayInfo->m_areIOWindowsValid)
    {
        goto reset_and_exit;
    }

    if (m_TransparencyState==CONTROL_ON)
    {
        transparency = m_Transparency;
    }
    else
    {
        if (pCurrNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CONST_ALPHA )
        {
            transparency = pCurrNode->m_bufferDesc.src.ulConstAlpha;
        }
    }

    /* Calculate the destination Viewport
       NB: The viewport line numbering is always frame based, even on an interlaced display
    */
    viewport.startPixel = STCalculateViewportPixel(&m_outputInfo.currentMode, pDisplayInfo->m_dstFrameRect.x);
    viewport.stopPixel  = STCalculateViewportPixel(&m_outputInfo.currentMode, pDisplayInfo->m_dstFrameRect.x + pDisplayInfo->m_dstFrameRect.width - 1);
    viewport.startLine  = STCalculateViewportLine(&m_outputInfo.currentMode,  pDisplayInfo->m_dstFrameRect.y);
    viewport.stopLine   = STCalculateViewportLine(&m_outputInfo.currentMode,  pDisplayInfo->m_dstFrameRect.y + pDisplayInfo->m_dstFrameRect.height - 1);

    TRC(TRC_ID_MAIN_INFO, "Destination Viewport: VPO (%d, %d) VPS (%d, %d)",
                viewport.startPixel,
                viewport.startLine,
                viewport.stopPixel,
                viewport.stopLine);

    if (!m_videoPlug->CreatePlugSetup(pDisplayInfo->m_videoPlugSetup,
                                     &pCurrNode->m_bufferDesc,
                                      viewport,
                                      pDisplayInfo->m_selectedPicture.colorFmt,
                                      transparency))
    {

        TRC( TRC_ID_ERROR, "Failed to create the PlugSetup!" );
        goto reset_and_exit;
    }

    return true;

reset_and_exit:
    pDisplayInfo->Reset();
    return false;
}

bool CHqvdpLitePlane::SetDynamicDisplayInfo(CDisplayNode            *pPrevNode,
                                            CDisplayNode            *pCurrNode,
                                            CDisplayNode            *pNextNode,
                                            CHqvdpLiteDisplayInfo   *pDisplayInfo,
                                            HQVDPLITE_CMD_t         *pFieldSetup,
                                            bool                     isPictureRepeated)
{
    /* Set luma and chroma buffer address in Top cmd */
    if(!SetPictureBaseAddresses(pCurrNode,
                                pDisplayInfo,
                                &pFieldSetup->Top.CurrentLuma,
                                &pFieldSetup->Top.CurrentChroma))
    {

        TRC( TRC_ID_ERROR, "Failed to set luma and chroma addresses!" );

        return false;
    }

    /* Set CSDI params */
    if(!SetCsdiParams(pPrevNode, pCurrNode, pNextNode, pDisplayInfo, pFieldSetup, isPictureRepeated))
    {

        TRC( TRC_ID_ERROR, "Failed to set the CSDI params!" );
        return false;
    }
    return true;
}

/*******************************************************************************
Name        : PrepareCmd
Description : Prepare the command to send to HQVDPLITE.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmd (CDisplayNode          *pCurrNode,
                                  CHqvdpLiteDisplayInfo *pDisplayInfo,
                                  bool                   isTopFieldOnDisplay,
                                  HQVDPLITE_CMD_t       *pFieldSetup)
{
    TRCIN( TRC_ID_UNCLASSIFIED, "" );

    if (PrepareCmdTopParams (pCurrNode, pDisplayInfo, &(pFieldSetup->Top)) == false)
    {
        TRC( TRC_ID_ERROR, "Error ! Unable to prepare Top params for command " );
        return false;
    }

    if (PrepareCmdVc1Params(&(pFieldSetup->Vc1re)) == false)
    {
        TRC( TRC_ID_ERROR, "Error ! Unable to prepare VC1 params for command " );
        return false;
    }

    if (PrepareCmdFMDParams (&(pFieldSetup->Fmd)) == false)
    {
        TRC( TRC_ID_ERROR, "Error ! Unable to prepare FMD params for command " );
        return false;
    }

    if (PrepareCmdCSDIParams (pCurrNode, pDisplayInfo, pFieldSetup) == false)
    {
        TRC( TRC_ID_ERROR, "Error ! Unable to prepare CSDI params for command " );
        return false;
    }

    if (PrepareCmdHVSRCParams(pCurrNode, pDisplayInfo, pFieldSetup) == false)
    {
        TRC( TRC_ID_ERROR, "Error ! Unable to prepare HVSRC params for command " );
        return false;
    }

    TRCOUT( TRC_ID_UNCLASSIFIED, "" );
    return true;
}


void CHqvdpLitePlane::SetHWDisplayInfos(CDisplayNode *pCurrNode, CHqvdpLiteDisplayInfo *pDisplayInfo)
{
    if(!m_bHasProgressive2InterlacedHW)
        return;

    /* set field info */
    if(pDisplayInfo->m_isSrcInterlaced)
    {
        /*
         * store m_bHasProgressive2InterlacedHW in displayInfo.isHWDeinterlacing
         * because m_bHasProgressive2InterlacedHW could be overwritten by each
         * derive classes of DisplayPlane class
         */
        if(m_bHasProgressive2InterlacedHW)
        {
            TRC( TRC_ID_UNCLASSIFIED, "enable HW de-interlacing");
            pDisplayInfo->m_isHWDeinterlacing = true;
        }
    }


    /* set interpolateFields */
    if(m_outputInfo.isDisplayInterlaced)
    {
       if(m_bHasProgressive2InterlacedHW)
       {
            TRC( TRC_ID_UNCLASSIFIED, "using P2I");
            pDisplayInfo->m_isUsingProgressive2InterlacedHW = true;
        }
    }
}


bool CHqvdpLitePlane::AdjustIOWindowsForHWConstraints(CDisplayNode *pCurrNode, CDisplayInfo *pDisplayInfo) const
{

    /* HQVDP output width and height should be even */
    pDisplayInfo->m_dstFrameRect.height &= ~(0x1);
    pDisplayInfo->m_dstFrameRect.width  &= ~(0x1);


    return true;
}


bool CHqvdpLitePlane::IsScalingPossible(CDisplayNode   *pCurrNode,
                                        CDisplayInfo   *pGenericDisplayInfo)
{
    CHqvdpLiteDisplayInfo *pDisplayInfo = (CHqvdpLiteDisplayInfo *)pGenericDisplayInfo;
    bool isSrcInterlaced = pDisplayInfo->m_isSrcInterlaced;


    TRCBL(TRC_ID_MAIN_INFO);
    TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Primary picture");
    pDisplayInfo->m_isSecondaryPictureSelected = false;
    FillSelectedPictureDisplayInfo(pCurrNode, pDisplayInfo);

    if(IsScalingPossibleByHw(pCurrNode, pDisplayInfo, isSrcInterlaced))
    {
        // Scaling possible with the Primary picture
        return true;
    }


    /* Is decimation available? */
    if( (pCurrNode->m_bufferDesc.src.horizontal_decimation_factor != STM_NO_DECIMATION) ||
        (pCurrNode->m_bufferDesc.src.vertical_decimation_factor   != STM_NO_DECIMATION) )
    {
        TRCBL(TRC_ID_MAIN_INFO);
        TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Secondary picture");
        pDisplayInfo->m_isSecondaryPictureSelected = true;
        FillSelectedPictureDisplayInfo(pCurrNode, pDisplayInfo);

        if(IsScalingPossibleByHw(pCurrNode, pDisplayInfo, isSrcInterlaced))
        {
            // Scaling possible with the Secondary picture (aka Decimated)
            return true;
        }


        // In case of Progressive source a bigger dowscaling can be achieved by forcing the source to be treated as Interlaced
        if(!pDisplayInfo->m_isSrcInterlaced)
        {
            TRCBL(TRC_ID_MAIN_INFO);
            TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Secondary picture forced as Interlaced");
            isSrcInterlaced = true;

            if(IsScalingPossibleByHw(pCurrNode, pDisplayInfo, isSrcInterlaced))
            {
                // Scaling possible thanks to this trick. The source is now considered as Interlaced
                pDisplayInfo->m_isSrcInterlaced = true;
                TRC( TRC_ID_MAIN_INFO, "Src now considered as Interlaced" );
                return true;
            }
        }
    }
    else
    {
        // No decimation available

        // In case of Progressive source a bigger dowscaling can be achieved by forcing the source to be treated as Interlaced
        if(!pDisplayInfo->m_isSrcInterlaced)
        {
            TRCBL(TRC_ID_MAIN_INFO);
            TRC( TRC_ID_MAIN_INFO, "## Checking if scaling possible with Primary picture forced as Interlaced");
            isSrcInterlaced = true;

            if(IsScalingPossibleByHw(pCurrNode, pDisplayInfo, isSrcInterlaced))
            {
                // Scaling possible thanks to this trick. The source is now considered as Interlaced
                pDisplayInfo->m_isSrcInterlaced = true;
                TRC( TRC_ID_MAIN_INFO, "Src now considered as Interlaced" );
                return true;
            }

        }
    }


    TRC( TRC_ID_ERROR, "!!! Scaling NOT possible !!!" );
    return false;
}


bool CHqvdpLitePlane::IsScalingPossibleByHw(CDisplayNode           *pCurrNode,
                                            CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                            bool                    isSrcInterlaced)
{
    uint32_t vhsrc_input_width;
    uint32_t vhsrc_input_height;
    uint32_t vhsrc_output_width;
    uint32_t vhsrc_output_height;

    /*
        This function will check if the scaling from (src_width, src_frame_height) to (dst_width, dst_frame_height)
        is possible according to 3 criteria:
         1) The horizontal and vertical zoom factor should not exceed "MAX_SCALING_FACTOR"
         2) The requested Zoom should not exceed the Limit Zoom Out
         3) The STBus traffic should not exceed a threshold
    */

    TRC( TRC_ID_MAIN_INFO, "Trying: IsSrcInterlaced = %d, src_width = %d, src_frame_height = %d, dst_width = %d, dst_frame_height=%d",
                isSrcInterlaced,
                pDisplayInfo->m_selectedPicture.srcFrameRect.width,
                pDisplayInfo->m_selectedPicture.srcFrameRect.height,
                pDisplayInfo->m_dstFrameRect.width,
                pDisplayInfo->m_dstFrameRect.height);

    // With current code:
    // - The DEI is always ON with Interlaced inputs so the VHSRC is always receiving a Progressive picture (so vhsrc_input_height  = src_frame_height)
    // - The P2I is always ON with Interlaced output so the VHSRC is always producing a Progressive picture (so vhsrc_output_height = dst_frame_height)
    vhsrc_input_width   = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
    vhsrc_input_height  = pDisplayInfo->m_selectedPicture.srcFrameRect.height;
    vhsrc_output_width  = pDisplayInfo->m_dstFrameRect.width;
    vhsrc_output_height = pDisplayInfo->m_dstFrameRect.height;


    if(!IsZoomFactorOk(vhsrc_input_width,
                       vhsrc_input_height,
                       vhsrc_output_width,
                       vhsrc_output_height))
    {
        TRC(TRC_ID_MAIN_INFO, "ZoomFactor NOK!");
        return false;
    }

    if(isSrcInterlaced)
    {
        TRC(TRC_ID_MAIN_INFO, "First, trying if scaling possible if 3D DEI mode");

        if(IsSTBusDataRateOk(pCurrNode, pDisplayInfo, isSrcInterlaced, DEI_MODE_3D) &&
           IsHwProcessingTimeOk(vhsrc_input_width, vhsrc_input_height, vhsrc_output_width, vhsrc_output_height, DEI_MODE_3D) )
        {
            // All the conditions are OK. This scaling is possible and the maxDeiMode is DEI_MODE_3D
            pDisplayInfo->m_is3DDeinterlacingPossible = true;
            return true;
        }

        TRC(TRC_ID_MAIN_INFO, "Scaling not possible with 3D DEI mode. Now trying DI mode");

        // Scaling NOT OK with 3D mode. Check if it is OK with DI mode
        if(IsSTBusDataRateOk(pCurrNode, pDisplayInfo, isSrcInterlaced, DEI_MODE_DI) &&
           IsHwProcessingTimeOk(vhsrc_input_width, vhsrc_input_height, vhsrc_output_width, vhsrc_output_height, DEI_MODE_DI) )
        {
            // All the conditions are OK. This scaling is possible and the maxDeiMode is DEI_MODE_DI
            pDisplayInfo->m_is3DDeinterlacingPossible = false;
            return true;
        }

        TRC(TRC_ID_MAIN_INFO, "STBusDataRate or HwProcessingTime NOK! (even with DI mode)");
    }
    else
    {
        if(IsSTBusDataRateOk(pCurrNode, pDisplayInfo, isSrcInterlaced, DEI_MODE_OFF) &&
           IsHwProcessingTimeOk(vhsrc_input_width, vhsrc_input_height, vhsrc_output_width, vhsrc_output_height, DEI_MODE_OFF) )
        {
            // All the conditions are OK. This scaling is possible and the maxDeiMode is DEI_MODE_OFF
            pDisplayInfo->m_is3DDeinterlacingPossible = false;
            return true;
        }
        TRC(TRC_ID_MAIN_INFO, "STBusDataRate or HwProcessingTime NOK!");
    }

    return false;
}


bool CHqvdpLitePlane::IsZoomFactorOk(uint32_t vhsrc_input_width,
                                     uint32_t vhsrc_input_height,
                                     uint32_t vhsrc_output_width,
                                     uint32_t vhsrc_output_height)
{
    if( (vhsrc_input_width  <= vhsrc_output_width  * MAX_SCALING_FACTOR) &&
        (vhsrc_input_height <= vhsrc_output_height * MAX_SCALING_FACTOR) )
    {
        return true;
    }
    else
    {
        return false;
    }
}

// This function checks that the HW processing time to perform this scaling is achievable
bool CHqvdpLitePlane::IsHwProcessingTimeOk(uint32_t vhsrc_input_width,
                                           uint32_t vhsrc_input_height,
                                           uint32_t vhsrc_output_width,
                                           uint32_t vhsrc_output_height,
                                           stm_display_dei_mode_t deiMode)
{
    uint32_t outputPixelClockInHz;
    uint32_t maxHWCycles, maxNbrOfCycles;
    uint32_t Tx;
    uint64_t hqvdpProcessingTimeInNs, outputVideoLineDurationInNs;

    /******************************************************
        The downscale constraint is:
        HQVDP_processing_time < Output_video_line_duration
    */

    /******************************************************
      Calculation of HQVDP_processing_time

      HQVDP processing time = NbLine * maxNbrOfCycles / HqvdpClockFrequencyInMHz

      Where:
          Nbline is the number of input lines required to produce one output line
                = Picture_Height_at_the_input_of_the_VHSRC / Picture_Height_at_the_output_of_the_VHSRC
          maxNbrOfCycles = Max number of cycles taken by the firmware and HW for the processing of a line

      => HQVDP processing time = (Picture_Height_at_the_input_of_the_VHSRC  * maxNbrOfCycles) /
                                 (Picture_Height_at_the_output_of_the_VHSRC * HqvdpClockFrequencyInMHz)
    */

    // maxHWCycles is the max between InputWidth and OutputWidth
    maxHWCycles = MAX(vhsrc_input_width, vhsrc_output_width);

    // maxNbrOfCycles is the max between the nbr of HW processing cycles and FW processing cycles (= MAX_HQVDPLITE_FIRWMARE_CYCLES)
    // Consequently, maxNbrOfCycles can never be lower than MAX_HQVDPLITE_FIRWMARE_CYCLES
    maxNbrOfCycles = MAX(maxHWCycles, MAX_HQVDPLITE_FIRWMARE_CYCLES);
    TRC(TRC_ID_UNCLASSIFIED, "maxNbrOfCycles = %d", maxNbrOfCycles);

    // OverHeadFactor is a correcting factor depending on DEI/FMD activity (mainly). It may vary from one HW implementation to the other.
    // - If more than 2 fields are read by the HQVDP, the OverHeadFactor is 20%
    // - Otherwise, the OverHeadFactor is 5%
    switch(deiMode)
    {
        case DEI_MODE_OFF:
        case DEI_MODE_VI:
        case DEI_MODE_DI:
            // Modes using only 2 fields (1 luma field + 1 chroma field). OverHeadFactor is 5%
            maxNbrOfCycles = (maxNbrOfCycles * 105) / 100;
            break;

        default:
            // OverHeadFactor is 20%
            maxNbrOfCycles = (maxNbrOfCycles * 120) / 100;
            break;
    }
    TRC(TRC_ID_UNCLASSIFIED, "maxNbrOfCycles with overhead = %d", maxNbrOfCycles);

    // Multiplied by 1000 to get a result in Ns
    hqvdpProcessingTimeInNs = vibe_os_div64(1000 * ((uint64_t) vhsrc_input_height) * ((uint64_t) maxNbrOfCycles), ((uint64_t) vhsrc_output_height) * ((uint64_t) m_hqvdpClockFreqInMHz) );

    /******************************************************
     Calculation of Output_video_line_duration (from VHSRC point of view)

     Output video line duration = Tx / outputPixelClockInHz

     where Tx = Total Output Horizontal Resolution (including blanking)
    */

    Tx = m_outputInfo.currentMode.mode_timing.pixels_per_line;
    TRC(TRC_ID_UNCLASSIFIED, "Tx = %d", Tx);

    if(m_outputInfo.isDisplayInterlaced)
    {
        // When output is Interlaced, the P2I will be used so, if the output pixel clock is N, the VHSRC will in fact work at the frequency 2*N
        outputPixelClockInHz = 2 * m_outputInfo.currentMode.mode_timing.pixel_clock_freq;
    }
    else
    {
        outputPixelClockInHz = m_outputInfo.currentMode.mode_timing.pixel_clock_freq;
    }

    // Multiplied by 1000000000 to get a result in Ns
    outputVideoLineDurationInNs = vibe_os_div64(1000000000 * ((uint64_t) Tx), ((uint64_t) outputPixelClockInHz) );

    /******************************************************/

    TRC(TRC_ID_MAIN_INFO, "hqvdpProcessingTimeInNs     = %lld", hqvdpProcessingTimeInNs);
    TRC(TRC_ID_MAIN_INFO, "outputVideoLineDurationInNs = %lld", outputVideoLineDurationInNs);

    if(hqvdpProcessingTimeInNs < outputVideoLineDurationInNs)
    {
        // Scaling possible
        return true;
    }
    else
    {
        // Scaling NOT possible
        return false;
    }
}


// See description in CHqvdpLitePlane::ComputeSTBusDataRate()
bool CHqvdpLitePlane::GetNbrBytesPerPixel(CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                          bool                    isSrcInterlaced,
                                          stm_display_dei_mode_t  deiMode,
                                          stm_rational_t         *pNbrBytesPerPixel)
{
    if(isSrcInterlaced)
    {
        switch(deiMode)
        {
            case DEI_MODE_OFF:
            case DEI_MODE_VI:
            case DEI_MODE_DI:
            {
                if(pDisplayInfo->m_selectedPicture.isSrc422)
                {
                    pNbrBytesPerPixel->numerator = 2;
                    pNbrBytesPerPixel->denominator = 1;
                }
                else if(pDisplayInfo->m_selectedPicture.isSrc420)
                {
                    pNbrBytesPerPixel->numerator = 3;
                    pNbrBytesPerPixel->denominator = 2;
                }
                else
                {
                    // 4:4:4 source
                    TRC(TRC_ID_ERROR, "4:4:4 Interlaced source is not supported!");
                    return false;
                }
                break;
            }
            case DEI_MODE_3D:
            default:
            {
                if(pDisplayInfo->m_selectedPicture.isSrc422)
                {
                    pNbrBytesPerPixel->numerator = 9;
                    pNbrBytesPerPixel->denominator = 1;
                }
                else if(pDisplayInfo->m_selectedPicture.isSrc420)
                {
                    pNbrBytesPerPixel->numerator = 15;
                    pNbrBytesPerPixel->denominator = 2;
                }
                else
                {
                    // 4:4:4 source
                    TRC(TRC_ID_ERROR, "4:4:4 Interlaced source is not supported!");
                    return false;
                }
                break;
            }
        }
    }
    else
    {
        /* Progressive source: DEI Bypassed */
        if(pDisplayInfo->m_selectedPicture.isSrc422)
        {
            pNbrBytesPerPixel->numerator = 2;
            pNbrBytesPerPixel->denominator = 1;
        }
        else if(pDisplayInfo->m_selectedPicture.isSrc420)
        {
            pNbrBytesPerPixel->numerator = 3;
            pNbrBytesPerPixel->denominator = 2;
        }
        else
        {
            // 4:4:4 source
            pNbrBytesPerPixel->numerator = 3;
            pNbrBytesPerPixel->denominator = 1;
        }
    }
    return true;
}


bool CHqvdpLitePlane::ComputeSTBusDataRate(CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                           bool                    isSrcInterlaced,
                                           uint32_t                src_width,
                                           uint32_t                src_frame_height,
                                           uint32_t                dst_width,
                                           uint32_t                dst_frame_height,
                                           stm_display_dei_mode_t  deiMode,
                                           bool                    isSrcOn10bits,
                                           uint32_t                verticalRefreshRate,
                                           uint64_t               *pDataRateInMBperS)
{
    stm_rational_t nbrBytesPerPixel;
    uint64_t num, den;
    uint32_t OutputTotalHeight;
    uint32_t src_height;

    if(!GetNbrBytesPerPixel(pDisplayInfo, isSrcInterlaced, deiMode, &nbrBytesPerPixel))
    {
        TRC(TRC_ID_ERROR, "Failed to get the nbr of Bytes per Pixel!");
        return false;
    }

    src_height = (isSrcInterlaced ? src_frame_height / 2 : src_frame_height);

    // If the output is Interlaced, no need to calculate the "field" size for OutputHeight and OutputTotalHeight
    // because the result will be the same when doing "OutputHeight / OutputTotalHeight"
    OutputTotalHeight = m_outputInfo.currentMode.mode_timing.lines_per_frame;

    /*
        STBus data rate estimation:

        STBus data rate = (Size of data read-written in memory) / (time available to perform the operation)

        Size of data read-written in memory = (src_width * src_height) * NbrBytesPerPixel

            * With DEI Spatial or Bypassed, for each pixel, only the Current Field is read by the HW:

                                        4:4:4 input         4:2:2 input         4:2:0 input
                Curr Field              3 Bytes             2 Bytes             1.5 Bytes (1 Byte of Luma and 0.5 Byte of Chroma)
                TOTAL NbrBytesPerPixel  3 Bytes             2 Bytes             1.5 Bytes

            * With 3D DEI, for each pixel to process, the HW has to read:

                                        4:4:4 input         4:2:2 input         4:2:0 input
                Curr Field              NA                  2 Bytes             1.5 Bytes
                Prev Field              NA                  2 Bytes             1.5 Bytes
                Next Field              NA                  2 Bytes             1.5 Bytes
                3 Motion Buffers        NA                  3 Bytes             3 Bytes
                TOTAL NbrBytesPerPixel  NA                  9 Bytes             7.5 Bytes

        time available to perform the operation = OutputHeight / (VSyncFreq * OutputTotalHeight)

        So we get:
        STBus data rate = NbrBytesPerPixel * (src_width * src_height) / (OutputHeight / (VSyncFreq * OutputTotalHeight))
                        = NbrBytesPerPixel * (src_width * src_height) * (VSyncFreq * OutputTotalHeight) / OutputHeight

        In the info about the current VTG mode, we have the "vertical_refresh_rate".
        "vertical_refresh_rate" is in mHz so it should be divided by 1000 to get the VSyncFreq.

        STBus data rate = NbrBytesPerPixel * (src_width * src_height) * (vertical_refresh_rate * OutputTotalHeight) / (1000 * OutputHeight)

        This will give a data rate in Bytes/s.
        Divide it by 10^6 to get a result in MB/s

        As a conclusion, the formula to use is:
        STBus data rate = NbrBytesPerPixel * (src_width * src_height) * (vertical_refresh_rate * OutputTotalHeight) / (1000 * 1000000 * OutputHeight)
    */

    num = ((uint64_t) nbrBytesPerPixel.numerator) * ((uint64_t) src_width) * ((uint64_t) src_height) * ((uint64_t) verticalRefreshRate) * ((uint64_t) OutputTotalHeight);
    den = ((uint64_t) nbrBytesPerPixel.denominator) * 1000 * 1000000 * ((uint64_t) dst_frame_height);

    *pDataRateInMBperS = vibe_os_div64(num, den);

    if(isSrcOn10bits)
    {
        // The above calculation are for a 8 bits source. In case of 10 bits source, the STBusDataRateInMBperS is increased by 10/8
        *pDataRateInMBperS = vibe_os_div64(*pDataRateInMBperS * 10, 8);
    }

    return true;
}


// This function checks that the number of MB/s required on the STBus by the Display IP to perform this scaling is achievable
bool CHqvdpLitePlane::IsSTBusDataRateOk(CDisplayNode           *pCurrNode,
                                        CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                        bool                    isSrcInterlaced,
                                        stm_display_dei_mode_t  deiMode)
{
    uint64_t dataRateReferenceInMBperS;
    uint64_t dataRateCurrentUseCaseInMBperS;
    uint64_t threshold;
    uint32_t fullOutputWidth;
    uint32_t fullOutputHeight;  // In Frame coordinates


    fullOutputWidth  = m_outputInfo.currentMode.mode_params.active_area_width;
    fullOutputHeight = m_outputInfo.currentMode.mode_params.active_area_height;

    // Compute the STBus Data Rate for a reference use case:
      // Only when the display source is UHD and display output is UHD(UHD->UHD use case), the reference is the data rate of UHD picture.
      // The rest cases, the reference is the data rate of FullHD picture
    if((pDisplayInfo->m_primarySrcFrameRect.height > 1088) &&(fullOutputHeight > 1080))
    {
        if(!ComputeSTBusDataRate(pDisplayInfo,
                                 pDisplayInfo->m_isSrcInterlaced,
                                 UHD_WIDTH,
                                 UHD_HEIGHT,
                                 fullOutputWidth,
                                 fullOutputHeight,
                                 DEI_MODE_3D,                      // This field is ignored if the source is Progressive
                                 false,
                                 30000,
                                 &dataRateReferenceInMBperS))
        {
            TRC(TRC_ID_ERROR, "Failed to compute dataRateReferenceInMBperS!");
        }
    }
    else
    {
        // This is the case of a 1920x1080 source (on 8 bits) displayed on full screen output (with 3D DEI mode if source is Interlaced).
        // NB: "pDisplayInfo->m_isSrcInterlaced" should be used instead of isSrcInterlaced to get the real ScanType of the source

        if (!ComputeSTBusDataRate(pDisplayInfo,
                                  pDisplayInfo->m_isSrcInterlaced,
                                  FULLHD_WIDTH,
                                  FULLHD_HEIGHT,
                                  fullOutputWidth,
                                  fullOutputHeight,
                                  DEI_MODE_3D,                      // This field is ignored if the source is Progressive
                                  false,
                                  60000,
                                  &dataRateReferenceInMBperS))
        {
            TRC(TRC_ID_ERROR, "Failed to compute dataRateReferenceInMBperS!");
        }
    }

    // Compute the STBus Data Rate for the current use case
    if (!ComputeSTBusDataRate(pDisplayInfo,
                              isSrcInterlaced,
                              pDisplayInfo->m_selectedPicture.srcFrameRect.width,
                              pDisplayInfo->m_selectedPicture.srcFrameRect.height,
                              pDisplayInfo->m_dstFrameRect.width,
                              pDisplayInfo->m_dstFrameRect.height,
                              deiMode,
                              pDisplayInfo->m_selectedPicture.isSrcOn10bits,
                              m_outputInfo.currentMode.mode_params.vertical_refresh_rate,
                              &dataRateCurrentUseCaseInMBperS))
    {
        TRC(TRC_ID_ERROR, "Failed to compute dataRateCurrentUseCaseInMBperS!");
    }


    // data rate can be BW_RATIO % above the data rate for the full screen use case
    threshold = vibe_os_div64(dataRateReferenceInMBperS * BW_RATIO, 100);

    TRC(TRC_ID_MAIN_INFO, "DataRate Reference = %llu MB/s", dataRateReferenceInMBperS);
    TRC(TRC_ID_MAIN_INFO, "DataRate Current Use Case = %llu MB/s", dataRateCurrentUseCaseInMBperS);
    TRC(TRC_ID_MAIN_INFO, "Threshold = %llu MB/s", threshold);

    if(dataRateCurrentUseCaseInMBperS <= threshold)
    {
        return true;
    }
    else
    {

        return false;
    }
}

void CHqvdpLitePlane::ResetEveryUseCases(void)
{
    /* Reset the command corresponding to each use case */
    vibe_os_zero_memory (&m_savedCmd, (MAX_USE_CASES * sizeof (hqvdpLiteCmd_t)));
}


/*******************************************************************************
Name        : SetPictureBaseAddresses
Description : Set Luma/Chroma Base Addresses of a picture.
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
bool CHqvdpLitePlane::SetPictureBaseAddresses (CDisplayNode             *pCurrNode,
                                               CHqvdpLiteDisplayInfo    *pDisplayInfo,
                                               uint32_t                 *LumaBaseAddress_p,
                                               uint32_t                 *ChromaBaseAddress_p)
{
    stm_buffer_src_picture_desc_t *pSrcPicture = 0;

    if(!pCurrNode)
        return false;

    if (pDisplayInfo->m_isSecondaryPictureSelected)
        pSrcPicture = &(pCurrNode->m_bufferDesc.src.secondary_picture);
    else
        pSrcPicture = &(pCurrNode->m_bufferDesc.src.primary_picture);

    BufferNodeType srcPictureType = pCurrNode->m_srcPictureType;

    if(!pSrcPicture)
        return false;

    /* calculate PictureBaseAddress base on the color format */
    switch (pSrcPicture->color_fmt)
    {
        case SURF_YCBCR422R:
        case SURF_CRYCB888:
        case SURF_CRYCB101010:
        case SURF_ACRYCB8888:
        {
            /* Raster Single Buffer Formats */
            if (!pDisplayInfo->m_isSrcInterlaced)
            {
                *LumaBaseAddress_p = (uint32_t) pSrcPicture->video_buffer_addr;
            }
            else
            {
                if(srcPictureType == GNODE_PROGRESSIVE || srcPictureType == GNODE_TOP_FIELD)
                {
                    *LumaBaseAddress_p = (uint32_t)  pSrcPicture->video_buffer_addr;
                }
                else
                {
                    *LumaBaseAddress_p = (uint32_t) (pSrcPicture->video_buffer_addr + pSrcPicture->pitch);
                }
            }
            *ChromaBaseAddress_p = 0;
            break;
        }

        case SURF_YCbCr420R2B:
        case SURF_YCbCr422R2B:
        case SURF_YCbCr422R2B10:
        {
            /* YcbCr 4:2:0 Raster Double Buffer */
            if(srcPictureType == GNODE_PROGRESSIVE || srcPictureType == GNODE_TOP_FIELD)
            {
                *LumaBaseAddress_p   = (uint32_t)  pSrcPicture->video_buffer_addr;
                *ChromaBaseAddress_p = (uint32_t) (pSrcPicture->video_buffer_addr + pSrcPicture->chroma_buffer_offset);
            }
            else
            {
                /* In case of Bottom field presented, this the driver that should skip a line in the base address */
                *LumaBaseAddress_p   = (uint32_t)  pSrcPicture->video_buffer_addr + pSrcPicture->pitch;
                *ChromaBaseAddress_p = (uint32_t) (pSrcPicture->video_buffer_addr + pSrcPicture->chroma_buffer_offset) + pSrcPicture->pitch;
            }
            break;
        }
        case SURF_YCBCR420MB:
        default:
        {
            /* Macroblock Format (usual case) */
            *LumaBaseAddress_p   = (uint32_t)  pSrcPicture->video_buffer_addr;
            *ChromaBaseAddress_p = (uint32_t) (pSrcPicture->video_buffer_addr + pSrcPicture->chroma_buffer_offset);
            break;
        }
    }
    return true;
}

void CHqvdpLitePlane::RotateMotionBuffers (void)
{
    uint32_t motionBufferAvailable;

    motionBufferAvailable = m_PreviousMotionBufferPhysAddr;
    m_PreviousMotionBufferPhysAddr  = m_CurrentMotionBufferPhysAddr;
    m_CurrentMotionBufferPhysAddr   = m_NextMotionBufferPhysAddr;
    m_NextMotionBufferPhysAddr      = motionBufferAvailable;
}

void CHqvdpLitePlane::UpdateMotionState(void)
{
    switch(m_MotionState)
    {
        case HQVDPLITE_DEI_MOTION_OFF:
        {
            m_MotionState = HQVDPLITE_DEI_MOTION_INIT;
            break;
        }
        case HQVDPLITE_DEI_MOTION_INIT:
        {
            m_MotionState = HQVDPLITE_DEI_MOTION_LOW_CONF;
            break;
        }
        case HQVDPLITE_DEI_MOTION_LOW_CONF:
        {
            m_MotionState = HQVDPLITE_DEI_MOTION_FULL_CONF;
            break;
        }
        case HQVDPLITE_DEI_MOTION_FULL_CONF:
        {
            // Motion state cannot go higher
            break;
        }
    }
}

void CHqvdpLitePlane::ProcessLastVsyncStatus(const stm_time64_t &vsyncTime, CDisplayNode *pNodeDisplayed)
{
    HQVDPLITE_CMD_t *cmdWithPreviousStatus_p;

    if (!pNodeDisplayed)
    {
        /* pNodeDisplayed is null so no picture was displayed during the previous VSync period and there is no result to collect */
        return;
    }

    if (m_planeVisibily == STM_PLANE_DISABLED)
    {
        /* The plane is currently disabled at Mixel level so no processing is done and there is not status to collect */
        return;
    }

    /* Exit if IO Windows was not valid */
    if(!m_hqvdpLiteDisplayInfo.m_areIOWindowsValid)
        return;

    m_bReportPixelFIFOUnderflow = true;

    cmdWithPreviousStatus_p = (HQVDPLITE_CMD_t*)(m_pPreviousFromFirmware0.pData);

    vibe_os_zero_memory(cmdWithPreviousStatus_p, sizeof(HQVDPLITE_CMD_t));

    vibe_os_memcpy_from_dma_area (m_SharedAreaUsedByFirmware_p, 0, cmdWithPreviousStatus_p, sizeof(HQVDPLITE_CMD_t));

    /* Save the HQVDP CRC data collected at last VSync */
    vibe_os_zero_memory(&m_HQVdpLiteCrcData, sizeof(Crc_t));
    m_HQVdpLiteCrcData.LastVsyncTime  = vsyncTime;
    m_HQVdpLiteCrcData.PictureID      = pNodeDisplayed->m_pictureId;
    m_HQVdpLiteCrcData.PTS            = pNodeDisplayed->m_bufferDesc.info.PTS;
    m_HQVdpLiteCrcData.Status         = true;

    m_Statistics.CurDispPicPTS = pNodeDisplayed->m_bufferDesc.info.PTS;

    switch(pNodeDisplayed->m_srcPictureType)
    {
        case GNODE_BOTTOM_FIELD:
        {
            m_HQVdpLiteCrcData.PictureType = 'B';
            break;
        }
        case GNODE_TOP_FIELD:
        {
            m_HQVdpLiteCrcData.PictureType = 'T';
            break;
        }
        case GNODE_PROGRESSIVE:
        {
            m_HQVdpLiteCrcData.PictureType = 'F';
            break;
        }
        default:
        {
            m_HQVdpLiteCrcData.PictureType = 'U';
            break;
        }
    }

    TRC( TRC_ID_UNCLASSIFIED, "PictureId: %d    PictureType: %c    PTS=%llx",  m_HQVdpLiteCrcData.PictureID, m_HQVdpLiteCrcData.PictureType, m_HQVdpLiteCrcData.PTS );

    /* HQVDP_TOPStatus_Params_t */
    m_HQVdpLiteCrcData.CrcValue[0] = cmdWithPreviousStatus_p->TopStatus.InputYCrc;
    m_HQVdpLiteCrcData.CrcValue[1] = cmdWithPreviousStatus_p->TopStatus.InputUvCrc;
    /* HQVDP_FMDStatus_Params_t */
    m_HQVdpLiteCrcData.CrcValue[2] = cmdWithPreviousStatus_p->FmdStatus.NextYFmdCrc;
    m_HQVdpLiteCrcData.CrcValue[3] = cmdWithPreviousStatus_p->FmdStatus.NextNextYFmdCrc;
    m_HQVdpLiteCrcData.CrcValue[4] = cmdWithPreviousStatus_p->FmdStatus.NextNextNextYFmdCrc;
    /* HQVDP_CSDIStatus_Params_t */
    m_HQVdpLiteCrcData.CrcValue[5] = cmdWithPreviousStatus_p->CsdiStatus.PrevYCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[6] = cmdWithPreviousStatus_p->CsdiStatus.CurYCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[7] = cmdWithPreviousStatus_p->CsdiStatus.NextYCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[8] = cmdWithPreviousStatus_p->CsdiStatus.PrevUvCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[9] = cmdWithPreviousStatus_p->CsdiStatus.CurUvCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[10]= cmdWithPreviousStatus_p->CsdiStatus.NextUvCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[11]= cmdWithPreviousStatus_p->CsdiStatus.YCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[12]= cmdWithPreviousStatus_p->CsdiStatus.UvCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[13]= cmdWithPreviousStatus_p->CsdiStatus.UvCupCrc;
    m_HQVdpLiteCrcData.CrcValue[14]= cmdWithPreviousStatus_p->CsdiStatus.MotCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[15]= cmdWithPreviousStatus_p->CsdiStatus.MotCurCsdiCrc;
    m_HQVdpLiteCrcData.CrcValue[16]= cmdWithPreviousStatus_p->CsdiStatus.MotPrevCsdiCrc;
    /* HQVDP_HQRStatus_Params_t */
    m_HQVdpLiteCrcData.CrcValue[17]= cmdWithPreviousStatus_p->HvsrcStatus.YHvsrcCrc;
    m_HQVdpLiteCrcData.CrcValue[18]= cmdWithPreviousStatus_p->HvsrcStatus.UHvsrcCrc;
    m_HQVdpLiteCrcData.CrcValue[19]= cmdWithPreviousStatus_p->HvsrcStatus.VHvsrcCrc;
    /* HQVDP_IQIStatus_Params_t */
    m_HQVdpLiteCrcData.CrcValue[20]= cmdWithPreviousStatus_p->IqiStatus.PxfItStatus;
    m_HQVdpLiteCrcData.CrcValue[21]= cmdWithPreviousStatus_p->IqiStatus.YIqiCrc;
    m_HQVdpLiteCrcData.CrcValue[22]= cmdWithPreviousStatus_p->IqiStatus.UIqiCrc;
    m_HQVdpLiteCrcData.CrcValue[23]= cmdWithPreviousStatus_p->IqiStatus.VIqiCrc;

    /* HQVDP_TOPStatus_Params_t */

    TRC( TRC_ID_UNCLASSIFIED, "TopStatus.InputYCrc           = 0x%x", cmdWithPreviousStatus_p->TopStatus.InputYCrc );
    TRC( TRC_ID_UNCLASSIFIED, "TopStatus.InputUvCrc          = 0x%x", cmdWithPreviousStatus_p->TopStatus.InputUvCrc );
    /* HQVDP_FMDStatus_Params_t */
    TRC( TRC_ID_UNCLASSIFIED, "FmdStatus.NextYFmdCrc         = 0x%x", cmdWithPreviousStatus_p->FmdStatus.NextYFmdCrc );
    TRC( TRC_ID_UNCLASSIFIED, "FmdStatus.NextNextYFmdCrc     = 0x%x", cmdWithPreviousStatus_p->FmdStatus.NextNextYFmdCrc );
    TRC( TRC_ID_UNCLASSIFIED, "FmdStatus.NextNextNextYFmdCrc = 0x%x", cmdWithPreviousStatus_p->FmdStatus.NextNextNextYFmdCrc );
    /* HQVDP_CSDIStatus_Params_t */
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.PrevYCsdiCrc       = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.PrevYCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.CurYCsdiCrc        = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.CurYCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.NextYCsdiCrc       = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.NextYCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.PrevUvCsdiCrc      = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.PrevUvCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.CurUvCsdiCrc       = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.CurUvCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.NextUvCsdiCrc      = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.NextUvCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.YCsdiCrc           = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.YCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.UvCsdiCrc          = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.UvCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.UvCupCrc           = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.UvCupCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.MotCsdiCrc         = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.MotCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.MotCurCsdiCrc      = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.MotCurCsdiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "CsdiStatus.MotPrevCsdiCrc     = 0x%x", cmdWithPreviousStatus_p->CsdiStatus.MotPrevCsdiCrc );
    /* HQVDP_HQRStatus_Params_t */
    TRC( TRC_ID_UNCLASSIFIED, "HvsrcStatus.YHvsrcCrc         = 0x%x", cmdWithPreviousStatus_p->HvsrcStatus.YHvsrcCrc );
    TRC( TRC_ID_UNCLASSIFIED, "HvsrcStatus.UHvsrcCrc         = 0x%x", cmdWithPreviousStatus_p->HvsrcStatus.UHvsrcCrc );
    TRC( TRC_ID_UNCLASSIFIED, "HvsrcStatus.VHvsrcCrc         = 0x%x", cmdWithPreviousStatus_p->HvsrcStatus.VHvsrcCrc );
    /* HQVDP_IQIStatus_Params_t */
    TRC( TRC_ID_UNCLASSIFIED, "IqiStatus.PxfItStatus         = 0x%x", cmdWithPreviousStatus_p->IqiStatus.PxfItStatus );
    TRC( TRC_ID_UNCLASSIFIED, "IqiStatus.YIqiCrc             = 0x%x", cmdWithPreviousStatus_p->IqiStatus.YIqiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "IqiStatus.UIqiCrc             = 0x%x", cmdWithPreviousStatus_p->IqiStatus.UIqiCrc );
    TRC( TRC_ID_UNCLASSIFIED, "IqiStatus.VIqiCrc             = 0x%x", cmdWithPreviousStatus_p->IqiStatus.VIqiCrc );


    /* Now check the STATUS Flags */

    /* In normal condition IT_FIFO_EMPTY should be equal to '0' at the end of a processing */
    if(cmdWithPreviousStatus_p->IqiStatus.PxfItStatus & IQI_STATUS_PXF_IT_STATUS_IT_FIFO_EMPTY_MASK)
    {
        pNodeDisplayed->m_bufferDesc.info.stats.status |= STM_STATUS_BUF_HW_ERROR;
        if(m_bReportPixelFIFOUnderflow)
        {
            /*
             * Reporting this once is sufficient as the likely cause of the
             * condition is persistent, so once we are in this state we never
             * get out of it.
             */

            TRC( TRC_ID_ERROR, "### HQVDP: Pixel FIFO underflow!!! ###" );

            m_bReportPixelFIFOUnderflow = false;
        }
    }

    /* In normal condition END_PROCESSING should be equal to '1' at the end of a processing */
    if((cmdWithPreviousStatus_p->IqiStatus.PxfItStatus & IQI_STATUS_PXF_IT_STATUS_END_PROCESSING_MASK) == 0)
    {
        pNodeDisplayed->m_bufferDesc.info.stats.status |= STM_STATUS_BUF_HW_ERROR;
        if(m_bReportPixelFIFOUnderflow)
        {

            TRC( TRC_ID_ERROR, "### HQVDP: Cmd not processed correctly!!! ###" );

        }
        m_bReportPixelFIFOUnderflow = false;
    }
}


// "m_vsyncLock" should be taken when NothingToDisplay() is called
bool CHqvdpLitePlane::NothingToDisplay()
{
    bool res;

    // Check that VSyncLock is already taken before accessing to shared variables
    DEBUG_CHECK_VSYNC_LOCK_PROTECTION(m_pDisplayDevice);

    if(m_isPlaneActive)
    {
        TRC(TRC_ID_MAIN_INFO, "%s: Nothing to display. Null cmd sent to HQVDP Fw", GetName());
        m_isPlaneActive = false;
    }

    // Perform the stop actions specific to HQVDP plane
    SendNullCmd();

    // Perform the generic stop actions
    res = CDisplayPlane::NothingToDisplay();

    return res;
}


void CHqvdpLitePlane::SwitchFieldSetup()
{
    DMA_Area * tmpPtr;

    /* The Prepared command becomes the used cmd. The used cmd becomes available to prepare another cmd */
    tmpPtr = m_SharedAreaUsedByFirmware_p;
    m_SharedAreaUsedByFirmware_p = m_SharedAreaPreparedForNextVSync_p;
    m_SharedAreaPreparedForNextVSync_p = tmpPtr;
}

void CHqvdpLitePlane::SendCmd(CHqvdpLiteDisplayInfo *pDisplayInfo,
                              HQVDPLITE_CMD_t   *pFieldSetup)
{
    if(pDisplayInfo->m_areIOWindowsValid)
    {
        m_videoPlug->WritePlugSetup(pDisplayInfo->m_videoPlugSetup, isVisible());
        EnableHW();
        m_isPlaneActive = true;
        WriteFieldSetup(pFieldSetup);
    }
    else
    {
        TRC( TRC_ID_ERROR, "Invalid IO Windows" );
        NothingToDisplay();
    }
}


void CHqvdpLitePlane::SendNullCmd()
{
    c8fvp3_SetCmdAddress((uint32_t)m_baseAddress, 0x0);
}

void CHqvdpLitePlane::WriteFieldSetup (const HQVDPLITE_CMD_t * const field)
{
    /* A new VSync has occured: Perform a flip-flop of the buffers used */
    SwitchFieldSetup();

#ifdef CONFIG_STM_VIRTUAL_PLATFORM
    /* Generate SW Vsync to update CURRENT_CMD (Only for VSOC use)*/
    WriteMbxReg(mailbox_SOFT_VSYNC_OFFSET, 0x103);
    WriteMbxReg(mailbox_SOFT_VSYNC_OFFSET, 0x3);
#endif
    /* Copy data to shared memory area and flush it */
    vibe_os_memcpy_to_dma_area (m_SharedAreaPreparedForNextVSync_p, 0, field, sizeof(HQVDPLITE_CMD_t));

    c8fvp3_SetCmdAddress((uint32_t)m_baseAddress, (uint32_t)m_SharedAreaPreparedForNextVSync_p->ulPhysical);

    TRC(TRC_ID_HQVDPLITE, "mailbox_NEXT_CMD_OFFSET = 0x%x", ReadMbxReg(mailbox_NEXT_CMD_OFFSET)  );
    TRC(TRC_ID_HQVDPLITE, "mailbox_CURRENT_CMD_OFFSET = 0x%x", ReadMbxReg(mailbox_CURRENT_CMD_OFFSET)  );

    TRC( TRC_ID_HQVDPLITE, "New Cmd sent to XP70" );

}

/*******************************************************************************
Name        : PrepareCmdTopParams
Description : Prepare the Top part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdTopParams (CDisplayNode           *pCurrNode,
                                           CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                           HQVDPLITE_TOP_Params_t *pTopParams)
{
    uint32_t   SourcePictureHorizontalSize, SourcePictureVerticalSize, SourcePicturePitch;
    uint32_t   Orig_X, Orig_Y;
    uint32_t   ViewportHeight, ViewportWidth;


    /* Clear the content of the command params */
    vibe_os_zero_memory(pTopParams, sizeof(HQVDPLITE_TOP_Params_t));

    /**************************/
    /* Top parameter : CONFIG */
    /**************************/
    /* Top parameter : CONFIG  : Progressive bit */
    if (!pDisplayInfo->m_isSrcInterlaced)
    {
        pTopParams->Config |= HQVDPLITE_CONFIG_PROGRESSIVE;
    }
    else
    {
        pTopParams->Config &= ~HQVDPLITE_CONFIG_PROGRESSIVE;
        /* Top parameter : CONFIG  : Top or Bottom bit */
        /* Select Field */
        if(pCurrNode->m_srcPictureType == GNODE_PROGRESSIVE || pCurrNode->m_srcPictureType == GNODE_TOP_FIELD)
        {
            pTopParams->Config |= HQVDPLITE_CONFIG_TOP_NOT_BOT;
        }
        else
        {
            pTopParams->Config &= ~HQVDPLITE_CONFIG_TOP_NOT_BOT;
        }
    }

    /* Top parameter : CONFIG  : IT EOP bit */
    pTopParams->Config &= ~HQVDPLITE_CONFIG_IT_EOP;

    /* Top parameter : CONFIG  : Pass Thru bit */
    pTopParams->Config &= ~HQVDPLITE_CONFIG_PASS_THRU;

    /* Top parameter : CONFIG  : Nb of field bits = 3 now (could be changed to 5) */
    pTopParams->Config |= HQVDPLITE_CONFIG_NB_OF_FIELD_3;

    /******************************/
    /* Top parameter : MEM_FORMAT */
    /******************************/
    pTopParams->MemFormat = 0;
    /* Top parameter : MEM_FORMAT  : Raster or macroblock Input Format */
    switch (pDisplayInfo->m_selectedPicture.colorFmt)
    {
        case SURF_YCBCR420MB:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_420MACROBLOCK;
            break;

        case SURF_YCBCR422R:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_422RASTER_SINGLE_BUFFER;
            break;

        case SURF_YUYV:
            /* pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_422RASTER_SINGLE_BUFFER;*/
            TRC( TRC_ID_ERROR, "Error! Not supported BitmapType on HQVDPLite!" );
            break;

        case SURF_YCbCr420R2B:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_420RASTER_DUAL_BUFFER;
            break;

        case SURF_YCbCr422R2B:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_422RASTER_DUAL_BUFFER;
            break;

        case SURF_YCbCr422R2B10:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_422RASTER_DUAL_BUFFER;
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_PIX10BIT_NOT_8BIT;
            break;

        case SURF_CRYCB888:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_444RASTER_SINGLE_BUFFER;
            break;

        case SURF_CRYCB101010:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_444RASTER_SINGLE_BUFFER;
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_PIX10BIT_NOT_8BIT;
            break;

        case SURF_ACRYCB8888:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_444_RSB_WITH_ALPHA;
            break;

        case SURF_YUV420:
        case SURF_YVU420:
        case SURF_YUV422P:
        default:
            TRC( TRC_ID_ERROR, "Error! Not supported BitmapType!" );
            return false;
    }
    /* Top parameter : MEM_FORMAT  : Output format is set Raster 444 (usefull only in MEM to MEM configuration) */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUTFORMAT_444;

    /* Top parameter : MEM_FORMAT  : Output  Alpha is usefull only in MEM to MEM configuration */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUTALPHA;

    /* Top parameter : MEM_FORMAT  : Setting Mem to TV (always TRUE) */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_MEMTOTV;

    /* Top parameter : MEM_FORMAT  : Setting Mem to Mem (always FALSE) */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_MEMTOMEM;

    /* Top parameter : MEM_FORMAT  : Pix8to10 lsb bits (10 by default)Luma */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_PIX8TO10LSB_Y;

    /* Top parameter : MEM_FORMAT  : Pix8to10 lsb bits (0 by default) Chroma */
    pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_PIX8TO10LSB_C;


    /* Top parameter : MEM_FORMAT  : setting for output 3D format based on VTG mode set */
    switch (m_outputInfo.currentMode.mode_params.flags)
    {
        case STM_MODE_FLAGS_NONE:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_2D;
            break;
        case STM_MODE_FLAGS_3D_FRAME_PACKED:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_FP;
            break;
        case STM_MODE_FLAGS_3D_SBS_HALF:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_SBS;
            break;
        case STM_MODE_FLAGS_3D_TOP_BOTTOM:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_TAB;
            break;
        default:/* Unsupported formats */
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_OUTPUT_3DFORMAT_2D;
            break;
    }


    /* Top parameter : MEM_FORMAT  :  Input 3D format (taken from stream info, unless it is forced by appli) */
    switch(pCurrNode->m_bufferDesc.src.config_3D.formats)
    {
        case FORMAT_3D_NONE:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_2D;
            break;
        case FORMAT_3D_SBS_HALF:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_SBS;
            break;
        case FORMAT_3D_STACKED_HALF:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_TAB;
            break;
        case FORMAT_3D_FRAME_SEQ:
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_FP;
            break;
        default: /* Unsupported formats */
            pTopParams->MemFormat |= HQVDPLITE_MEM_FORMAT_INPUT_3DFORMAT_2D;
            break;
    }
    /******************************************************************/
    /* Top parameter : CURRENT_LUMA and CURRENT_CHROMA source address */
    /*                 (will be set by PresentDisplayNode() )         */
    /******************************************************************/
    pTopParams->CurrentLuma   = 0;
    pTopParams->CurrentChroma = 0;

    /***************************************************************************************************/
    /* Top parameter : CURRENT_RIGHT_LUMA and CURRENT_RIGHT_CHROMA source address for 3D right support */
    /*                 (will be set by PresentDisplayNode() )                                          */
    /***************************************************************************************************/
    pTopParams->CurrentRightLuma   = 0;
    pTopParams->CurrentRightChroma = 0;

    /*******************************/
    /* Top parameter : OUTPUT_DATA */
    /*******************************/
    /* pTopParams->OutputData; */

    /*******************************************************/
    /* Top parameter : LUMA_SRC_PITCH and CHROMA_SRC_PITCH */
    /*******************************************************/
    SourcePictureHorizontalSize = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
    SourcePictureVerticalSize   = pDisplayInfo->m_selectedPicture.srcFrameRect.height;
    SourcePicturePitch          = pDisplayInfo->m_selectedPicture.pitch;

    /* Adapting Source picture size to stream 3D characteristics for 3D support */
    switch(pCurrNode->m_bufferDesc.src.config_3D.formats)
    {
        case FORMAT_3D_FRAME_SEQ:
            SourcePictureHorizontalSize = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
            SourcePictureVerticalSize   = pDisplayInfo->m_selectedPicture.srcFrameRect.height;
            break;
        case FORMAT_3D_SBS_HALF:
            SourcePictureHorizontalSize = pDisplayInfo->m_selectedPicture.srcFrameRect.width/2;
            SourcePictureVerticalSize   = pDisplayInfo->m_selectedPicture.srcFrameRect.height;
            break;
        case FORMAT_3D_STACKED_HALF:
            SourcePictureHorizontalSize = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
            SourcePictureVerticalSize   = pDisplayInfo->m_selectedPicture.srcFrameRect.height/2;
            break;
        default: /* Unsupported formats */
            SourcePictureHorizontalSize = pDisplayInfo->m_selectedPicture.srcFrameRect.width;
            SourcePictureVerticalSize   = pDisplayInfo->m_selectedPicture.srcFrameRect.height;
            break;
    }

    if (pDisplayInfo->m_isSrcInterlaced)
    {
        SourcePictureVerticalSize /= 2;
        SourcePicturePitch *= 2;
    }

    switch (pDisplayInfo->m_selectedPicture.colorFmt)
    {
        case SURF_YCBCR422R:
        case SURF_CRYCB888:
        case SURF_CRYCB101010:
        case SURF_ACRYCB8888:
        {
            pTopParams->LumaSrcPitch = SourcePicturePitch;
            /* ChromaSrcPitch is not needed in that use case because luma/chroma are together in a single buffer */
            pTopParams->ChromaSrcPitch = SourcePicturePitch;
            break;
        }
        case SURF_YCbCr420R2B:
        case SURF_YCbCr422R2B:
        case SURF_YCbCr422R2B10:
        {
            /* 4:2:0 Raster Dual Buffer */
            pTopParams->LumaSrcPitch = SourcePicturePitch;
            pTopParams->ChromaSrcPitch = SourcePicturePitch;
            break;
        }
        case SURF_YCBCR420MB:
        {
            /* Macroblock format */
            if (!pDisplayInfo->m_isSrcInterlaced)
            {   /* 1 line for progressive macroblock source picture */
                pTopParams->LumaSrcPitch   = 1 << HQVDPLITE_LUMA_MBSRCPITCH_OFFSET;
                pTopParams->ChromaSrcPitch = 1 << HQVDPLITE_CHROMA_MBSRCPITCH_OFFSET;
            }
            else
            {   /* 2 lines for interlaced macroblock source picture */
                pTopParams->LumaSrcPitch   = 2 << HQVDPLITE_LUMA_MBSRCPITCH_OFFSET;
                pTopParams->ChromaSrcPitch = 2 << HQVDPLITE_CHROMA_MBSRCPITCH_OFFSET;
            }

            /* For a MB format, pitch has no real meaning but some codecs are specifying rounded width in it */
            /* In this case we need to replace the horizontal size by the pitch for a correct display        */
            if(SourcePicturePitch > (pDisplayInfo->m_isSrcInterlaced ? 2*SourcePictureHorizontalSize : SourcePictureHorizontalSize))
            {
                SourcePictureHorizontalSize=SourcePicturePitch;
            }
            break;
        }
        default:
        {
            TRC( TRC_ID_ERROR, "Error! Not supported BitmapType!" );
            break;
        }
    }

    /***********************************/
    /* Top parameter : RIGHT VIEW PITCH */
    /***********************************/
    /*  getting 3D Right view pitch from left view */
    pTopParams->LumaRightSrcPitch = pTopParams->LumaSrcPitch ;
    pTopParams->ChromaRightSrcPitch = pTopParams->ChromaSrcPitch ;

    /***********************************/
    /* Top parameter : PROCESSED_PITCH */
    /***********************************/
    /* Only used when the IP is operating Mem to Mem so it is not used */
    pTopParams->LumaProcessedPitch = SourcePicturePitch;
    /* Chroma pitch is usefull only when output format is 420 R2B in this case, ChromaPitch can be equal to LumaPitch */
    pTopParams->ChromaProcessedPitch = SourcePicturePitch;


    /**************************************/
    /* Top parameter : INPUT_FRAME_SIZE */
    /**************************************/
    pTopParams->InputFrameSize = ((SourcePictureHorizontalSize << HQVDPLITE_INPUT_PICTURE_SIZE_WIDTH_SHIFT)  & HQVDPLITE_INPUT_PICTURE_SIZE_WIDTH_MASK)
                                |((SourcePictureVerticalSize   << HQVDPLITE_INPUT_PICTURE_SIZE_HEIGHT_SHIFT) & HQVDPLITE_INPUT_PICTURE_SIZE_HEIGHT_MASK);

    /**************************************/
    /* Top parameter : INPUT_VIEWPORT_ORI */
    /**************************************/
    Orig_Y =   (pDisplayInfo->m_selectedPicture.srcFrameRect.y / 16);
    Orig_X = ( (pDisplayInfo->m_selectedPicture.srcFrameRect.x / 16) << HQVDPLITE_VIEWPORT_ORIG_X_SHIFT) & HQVDPLITE_VIEWPORT_ORIG_X_MASK;
    pTopParams->InputViewportOri  = ((Orig_Y << HQVDPLITE_VIEWPORT_ORIG_Y_SHIFT) & HQVDPLITE_VIEWPORT_ORIG_Y_MASK) | (Orig_X );

    /********************************************/
    /* Top parameter : INPUT_VIEWPORT_ORI_RIGHT */
    /********************************************/
    /* Copying exact same parameters as Left view offset. Offset is aware about  3D type */
    Orig_Y =   (pDisplayInfo->m_selectedPicture.srcFrameRect.y / 16);
    Orig_X = ( (pDisplayInfo->m_selectedPicture.srcFrameRect.x / 16) << HQVDPLITE_VIEWPORT_ORIG_X_SHIFT) & HQVDPLITE_VIEWPORT_ORIG_X_MASK;
    pTopParams->InputViewportOriRight  = ((Orig_Y << HQVDPLITE_VIEWPORT_ORIG_Y_SHIFT) & HQVDPLITE_VIEWPORT_ORIG_Y_MASK) | (Orig_X );


    /***************************************/
    /* Top parameter : INPUT_VIEWPORT_SIZE */
    /***************************************/
    switch(pCurrNode->m_bufferDesc.src.config_3D.formats)
    {
        case(FORMAT_3D_NONE) :
        case(FORMAT_3D_FRAME_SEQ) :
        {
            ViewportHeight = (pDisplayInfo->m_selectedPicture.srcFrameRect.height << HQVDPLITE_VIEWPORT_SIZE_HEIGHT_SHIFT) & HQVDPLITE_VIEWPORT_SIZE_HEIGHT_MASK;
            ViewportWidth  = (pDisplayInfo->m_selectedPicture.srcFrameRect.width  << HQVDPLITE_VIEWPORT_SIZE_WIDTH_SHIFT ) & HQVDPLITE_VIEWPORT_SIZE_WIDTH_MASK;
            break;
        }
        case(FORMAT_3D_SBS_HALF) :
        {
            ViewportHeight = (pDisplayInfo->m_selectedPicture.srcFrameRect.height << HQVDPLITE_VIEWPORT_SIZE_HEIGHT_SHIFT) & HQVDPLITE_VIEWPORT_SIZE_HEIGHT_MASK;
            ViewportWidth  = (pDisplayInfo->m_selectedPicture.srcFrameRect.width/2  << HQVDPLITE_VIEWPORT_SIZE_WIDTH_SHIFT ) & HQVDPLITE_VIEWPORT_SIZE_WIDTH_MASK;
            break;
        }
        case(FORMAT_3D_STACKED_HALF) :
        {
            ViewportHeight = (pDisplayInfo->m_selectedPicture.srcFrameRect.height/2 << HQVDPLITE_VIEWPORT_SIZE_HEIGHT_SHIFT) & HQVDPLITE_VIEWPORT_SIZE_HEIGHT_MASK;
            ViewportWidth  = (pDisplayInfo->m_selectedPicture.srcFrameRect.width  << HQVDPLITE_VIEWPORT_SIZE_WIDTH_SHIFT ) & HQVDPLITE_VIEWPORT_SIZE_WIDTH_MASK;
            break;
        }
        default : /* Unsupported formats */
        {
            ViewportHeight = (pDisplayInfo->m_selectedPicture.srcFrameRect.height << HQVDPLITE_VIEWPORT_SIZE_HEIGHT_SHIFT) & HQVDPLITE_VIEWPORT_SIZE_HEIGHT_MASK;
            ViewportWidth  = (pDisplayInfo->m_selectedPicture.srcFrameRect.width  << HQVDPLITE_VIEWPORT_SIZE_WIDTH_SHIFT ) & HQVDPLITE_VIEWPORT_SIZE_WIDTH_MASK;
            break;
        }
    }
    pTopParams->InputViewportSize = ViewportHeight | ViewportWidth;

    /****************************/
    /* Top parameter : CRC_CTRL */
    /****************************/
    pTopParams->CrcResetCtrl |= HQVDPLITE_CRC_DEFAULT_RESET_VALUE;

    return (true);
} /* End of hqvdplite_PrepareCmdTopParam */


/*******************************************************************************
Name        : PrepareCmdVc1Params
Description : Prepare the VC1 part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdVc1Params (HQVDPLITE_VC1RE_Params_t * pVC1ReParams)
{
    /* Clear the content of the command params */
    vibe_os_zero_memory(pVC1ReParams, sizeof(HQVDPLITE_VC1RE_Params_t));

    /******************/
    /* VC1 parameters */
    /******************/

    /* Not implemented yet
    pVC1ReParams->CtrlPrvCsdi = 0;
    pVC1ReParams->CtrlPrvCsdi &= ~HQVDPLITE_VC1_CONFIG_ENABLE_MASK;

    pVC1ReParams->CtrlCurCsdi = 0;
    pVC1ReParams->CtrlCurCsdi &= ~HQVDPLITE_VC1_CONFIG_ENABLE_MASK;

    pVC1ReParams->CtrlNxtCsdi = 0;
    pVC1ReParams->CtrlNxtCsdi &= ~HQVDPLITE_VC1_CONFIG_ENABLE_MASK;

    pVC1ReParams->CtrlCurFmd = 0;
    pVC1ReParams->CtrlCurFmd &= ~HQVDPLITE_VC1_CONFIG_ENABLE_MASK;

    pVC1ReParams->CtrlNxtFmd = 0;
    pVC1ReParams->CtrlNxtFmd &= ~HQVDPLITE_VC1_CONFIG_ENABLE_MASK;
    */

    return true;
}

/*******************************************************************************
Name        : PrepareCmdFMDParams
Description : Prepare the FMD part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdFMDParams (HQVDPLITE_FMD_Params_t * pFMDParams)
{
    /* Clear the content of the command params */
    vibe_os_zero_memory(pFMDParams, sizeof(HQVDPLITE_FMD_Params_t));

    /******************/
    /* FMD parameters */
    /******************/

    /* Not implemented yet */

    return true;
}

/*******************************************************************************
Name        : PrepareCmdCSDIParams
Description : Prepare the CSDI part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdCSDIParams (CDisplayNode            *pCurrNode,
                                            CHqvdpLiteDisplayInfo   *pDisplayInfo,
                                            HQVDPLITE_CMD_t         *pFieldSetup)
{
    /* Clear the content of the command params */
    vibe_os_zero_memory(&pFieldSetup->Csdi, sizeof(HQVDPLITE_CSDI_Params_t));

    /*****************/
    /* CSDi : CONFIG */
    /*****************/
    pFieldSetup->Csdi.Config = 0;

    pFieldSetup->Csdi.Config2 =
        (( HQVDPLITE_CSDI_CONFIG2_INFLEXION_MOTION  << CSDI_CONFIG2_INFLEXION_MOTION_SHIFT) & CSDI_CONFIG2_INFLEXION_MOTION_MASK)   |
        (( HQVDPLITE_CSDI_CONFIG2_FIR_LENGTH        << CSDI_CONFIG2_FIR_LENGTH_SHIFT)       & CSDI_CONFIG2_FIR_LENGTH_MASK)         |
        (( HQVDPLITE_CSDI_CONFIG2_MOTION_FIR        << CSDI_CONFIG2_MOTION_FIR_SHIFT)       & CSDI_CONFIG2_MOTION_FIR_MASK)         |
        (( HQVDPLITE_CSDI_CONFIG2_MOTION_CLOSING    << CSDI_CONFIG2_MOTION_CLOSING_SHIFT)   & CSDI_CONFIG2_MOTION_CLOSING_MASK)     |
        (( HQVDPLITE_CSDI_CONFIG2_DETAILS           << CSDI_CONFIG2_DETAILS_SHIFT)          & CSDI_CONFIG2_DETAILS_MASK)            |
        (( HQVDPLITE_CSDI_CONFIG2_CHROMA_MOTION     << CSDI_CONFIG2_CHROMA_MOTION_SHIFT)    & CSDI_CONFIG2_CHROMA_MOTION_MASK)      |
        (( HQVDPLITE_CSDI_CONFIG2_ADAPTIVE_FADING   << CSDI_CONFIG2_ADAPTIVE_FADING_SHIFT)  & CSDI_CONFIG2_ADAPTIVE_FADING_MASK);

    pFieldSetup->Csdi.DcdiConfig =
        (( HQVDPLITE_CSDI_DCDI_CONFIG_ALPHA_LIMIT            << CSDI_DCDI_CONFIG_ALPHA_LIMIT_SHIFT)            & CSDI_DCDI_CONFIG_ALPHA_LIMIT_MASK) |
        (( HQVDPLITE_CSDI_DCDI_CONFIG_MEDIAN_MODE            << CSDI_DCDI_CONFIG_MEDIAN_MODE_SHIFT)            & CSDI_DCDI_CONFIG_MEDIAN_MODE_MASK) |
        (( HQVDPLITE_CSDI_DCDI_CONFIG_WIN_CLAMP_SIZE         << CSDI_DCDI_CONFIG_WIN_CLAMP_SIZE_SHIFT)         & CSDI_DCDI_CONFIG_WIN_CLAMP_SIZE_MASK) |
        (( HQVDPLITE_CSDI_DCDI_CONFIG_MEDIAN_BLEND_ANGLE_DEP << CSDI_DCDI_CONFIG_MEDIAN_BLEND_ANGLE_DEP_SHIFT) & CSDI_DCDI_CONFIG_MEDIAN_BLEND_ANGLE_DEP_MASK) |
        (( HQVDPLITE_CSDI_DCDI_CONFIG_ALPHA_MEDIAN_EN        << CSDI_DCDI_CONFIG_ALPHA_MEDIAN_EN_SHIFT)        & CSDI_DCDI_CONFIG_ALPHA_MEDIAN_EN_MASK);

     /* Use 8 bits motion buffers */
     pFieldSetup->Csdi.Config &= ~(HQVDPLITE_DEI_CTL_4_BITS_MOTION << HQVDPLITE_DEI_CTL_REDUCE_MOTION_EMP);

    return true;
}

/*******************************************************************************
Name        : SelectDeiMode
Description :- Are Prev, and Next nodes available?  If not => DEI_MODE_DIRECTIONAL
            :- Are polarities ok?                   If not => DEI_MODE_DIRECTIONAL
            :- Are characteristics changed?         If not => DEI_MODE_DIRECTIONAL
            :- Then, DEI_MODE_3D
Returns     : return DEI_MODE_xx
*******************************************************************************/
stm_display_dei_mode_t CHqvdpLitePlane::SelectDEIMode(CHqvdpLiteDisplayInfo *pDisplayInfo,
                                                      CDisplayNode *pPrevNode,
                                                      CDisplayNode *pCurNode,
                                                      CDisplayNode *pNextNode)
{
    BufferNodeType nextNodePictType;

    if(!pDisplayInfo->m_isSrcInterlaced)
    {
        // Progressive source: DEI Off
        return DEI_MODE_OFF;
    }

#ifdef DEBUG_FORCE_SPATIAL_DEINTERLACING
    return DEI_MODE_DI;
#endif

    if(!pDisplayInfo->m_is3DDeinterlacingPossible)
    {
        /* STBus DataRate calculation have shown that we can't do more than DI mode */
        TRC(TRC_ID_HQVDPLITE_DEI, "3D DEI mode not possible due to STBus DataRate");
        return DEI_MODE_DI;
    }

    if(pDisplayInfo->m_isSecondaryPictureSelected)
    {
        /* When Decimated picture is used, Directional Deinterlacing is recommended to minimize the STBus traffic */
        TRC(TRC_ID_HQVDPLITE_DEI, "Secondary picture selected: Use DI mode");
        return DEI_MODE_DI;
    }

    if(!pPrevNode || !pCurNode || !pNextNode)
    {
        /* A field is missing so temporal deinterlacing is not possible */
        TRC(TRC_ID_HQVDPLITE_DEI, "A field is missing: Use DI mode");
        return DEI_MODE_DI;
    }

    if(pCurNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY)
    {
        /* The source characteristics have changed so temporal deinterlacing is not possible */
        TRC(TRC_ID_HQVDPLITE_DEI, "Cur Field characteristics have changed: Use DI mode");
        return DEI_MODE_DI;
    }

    if(pPrevNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY)
    {
        /* The source characteristics have changed so temporal deinterlacing is not possible */
        TRC(TRC_ID_HQVDPLITE_DEI, "Prev Field characteristics have changed: Use DI mode");
        return DEI_MODE_DI;
    }

    if(pNextNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY)
    {
        /* The source characteristics have changed so temporal deinterlacing is not possible */
        TRC(TRC_ID_HQVDPLITE_DEI, "Next Field characteristics have changed: Use DI mode");
        return DEI_MODE_DI;
    }

    if(pCurNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY)
    {
        TRC(TRC_ID_HQVDPLITE_DEI, "Cur Field has a temporal discontinuity compare to Prev Field: Use DI mode");
        return DEI_MODE_DI;
    }

    if(pNextNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_TEMPORAL_DISCONTINUITY)
    {
        TRC(TRC_ID_HQVDPLITE_DEI, "Next Field has a temporal discontinuity compare to Cur Field: Use DI mode");
        return DEI_MODE_DI;
    }

    // NB: No need to check temporal discontinuity on pPrevNode because it tells if there is a discontinuity
    //     between PreviousNode and Pre-PreviousNode. So it doesn't matter here.

    nextNodePictType = pNextNode->m_srcPictureType;

    /* check if the source fields have alternate polarities (TBT or BTB) */
    if ( (pPrevNode->m_srcPictureType == GNODE_TOP_FIELD)
       &&(pCurNode->m_srcPictureType  == GNODE_BOTTOM_FIELD)
       &&(nextNodePictType            == GNODE_TOP_FIELD) )
    {
        return DEI_MODE_3D;
    }
    else if ( (pPrevNode->m_srcPictureType   == GNODE_BOTTOM_FIELD)
              &&(pCurNode->m_srcPictureType  == GNODE_TOP_FIELD)
              &&(nextNodePictType            == GNODE_BOTTOM_FIELD) )
    {
        return DEI_MODE_3D;
    }
    else
    {
        /* The 3 fields don't have alternate polarities so temporal deinterlacing is not possible */
        TRC(TRC_ID_HQVDPLITE_DEI, "The 3 fields don't have alternate polarities: Use DI mode");
        return DEI_MODE_DI;
    }
}

void CHqvdpLitePlane::SetNextDEIField(CDisplayNode           *pNextNode,
                                      CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                      HQVDPLITE_CMD_t        *pFieldSetup,
                                      bool                    isSecondaryPicture)
{
    pFieldSetup->Csdi.NextLuma = 0;
    pFieldSetup->Csdi.NextChroma = 0;

    if(!pNextNode)
        return;

    if(!(pNextNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY))
    {
        SetPictureBaseAddresses (pNextNode,
                                 pDisplayInfo,
                                 &pFieldSetup->Csdi.NextLuma,
                                 &pFieldSetup->Csdi.NextChroma);
    }
}

void CHqvdpLitePlane::SetPreviousDEIField(CDisplayNode          *pPrevNode,
                                          CHqvdpLiteDisplayInfo *pDisplayInfo,
                                          HQVDPLITE_CMD_t       *pFieldSetup,
                                          bool                   isSecondaryPicture)
{
    pFieldSetup->Csdi.PrevLuma = 0;
    pFieldSetup->Csdi.PrevChroma = 0;

    if(!pPrevNode)
        return;

    if(!(pPrevNode->m_bufferDesc.src.flags & STM_BUFFER_SRC_CHARACTERISTIC_DISCONTINUITY))
    {
        SetPictureBaseAddresses (pPrevNode,
                                 pDisplayInfo,
                                 &pFieldSetup->Csdi.PrevLuma,
                                 &pFieldSetup->Csdi.PrevChroma);
    }
    else
    {
        pFieldSetup->Csdi.PrevLuma   = pFieldSetup->Csdi.NextLuma;
        pFieldSetup->Csdi.PrevChroma = pFieldSetup->Csdi.NextChroma;
    }
}

void CHqvdpLitePlane::Set3DDEIConfiguration(uint32_t *DeiCtlRegisterValue)
{
     uint32_t Configuration = 0;

     Configuration = (HQVDPLITE_DEI_CTL_DD_MODE_DIAG7 << HQVDPLITE_DEI_CTL_DD_MODE_EMP)          |
                     (HQVDPLITE_DEI_CTL_DIR_RECURSIVITY_ON << HQVDPLITE_DEI_CTL_DIR_REC_EN_EMP)  |
                     (HQVDPLITE_DEI_CTL_BEST_KCOR_VALUE << HQVDPLITE_DEI_CTL_KCOR_EMP)           |
                     (HQVDPLITE_DEI_CTL_BEST_DETAIL_VALUE << HQVDPLITE_DEI_CTL_T_DETAIL_EMP)     |
                     (HQVDPLITE_DEI_CTL_BEST_KMOV_VALUE << HQVDPLITE_DEI_CTL_KMOV_FACTOR_EMP);

    /* Ensure that we change only the bits corresponding to the 3DMode */
     Configuration &= HQVDPLITE_DEI_3D_PARAMS_MASK;

     /* Clear the bits about the 3D mode in DeiCtlRegisterValue */
     *DeiCtlRegisterValue &= ~HQVDPLITE_DEI_3D_PARAMS_MASK;

     /* Write the new 3D mode*/
     *DeiCtlRegisterValue |= Configuration;
}


bool CHqvdpLitePlane::SetCsdiParams (CDisplayNode          *pPrevNode,
                                     CDisplayNode          *pCurrNode,
                                     CDisplayNode          *pNextNode,
                                     CHqvdpLiteDisplayInfo *pDisplayInfo,
                                     HQVDPLITE_CMD_t       *pFieldSetup,
                                     bool                   isPictureRepeated)
{
    uint32_t               csdi_config;
    stm_display_dei_mode_t selectedDeiMode;

    TRCBL(TRC_ID_HQVDPLITE_DEI);

    if(!pCurrNode || !pFieldSetup)
    {
        TRC(TRC_ID_ERROR, "Invalid param!");
        return false;
    }

    csdi_config = pFieldSetup->Csdi.Config;

    // Debugging trace: display previous, current and next node information
    if( IS_TRC_ID_ENABLED(TRC_ID_HQVDPLITE_DEI) )
    {
        char prevNodeText[25];
        char nextNodeText[25];

        if ( pPrevNode )
        {
            vibe_os_snprintf(prevNodeText, sizeof(prevNodeText), "%d%c %s",
                pPrevNode->m_pictureId,
                pPrevNode->m_srcPictureTypeChar,
                pPrevNode->m_isSkippedForFRC?"(SkippedFRC)":"");
        }
        else
        {
            vibe_os_snprintf(prevNodeText, sizeof(prevNodeText), "(null)");
        }

        if ( pNextNode )
        {
            vibe_os_snprintf(nextNodeText, sizeof(nextNodeText), "%d%c %s",
                pNextNode->m_pictureId,
                pNextNode->m_srcPictureTypeChar,
                pNextNode->m_isSkippedForFRC?"(SkippedFRC)":"");
        }
        else
        {
            vibe_os_snprintf(nextNodeText, sizeof(nextNodeText), "(null)");
        }

        TRC(TRC_ID_HQVDPLITE_DEI, "Plane %s P: %s C: %d%c N: %s",
                GetName(),
                prevNodeText,
                pCurrNode->m_pictureId,
                pCurrNode->m_srcPictureTypeChar,
                nextNodeText);
    }

    /*** Select the DEI mode to use ***/
    selectedDeiMode = SelectDEIMode(pDisplayInfo, pPrevNode, pCurrNode, pNextNode);

    if((pDisplayInfo->m_isSrcInterlaced) && (selectedDeiMode == DEI_MODE_OFF))
    {
        TRC(TRC_ID_ERROR, "Error! DEI should NOT be OFF with Interlaced sources!");
    }

    /*
     * rotate motion state if DEI mode is DEI_MODE_3D
     * reset motion state to MOTION_OFF if DEI mode is DEI_MODE_DI
     */
    switch(selectedDeiMode)
    {
        case DEI_MODE_OFF:
        {
            TRC(TRC_ID_HQVDPLITE_DEI, "DEI_MODE_OFF");
            m_MotionState = HQVDPLITE_DEI_MOTION_OFF;
            SetDeiMainMode (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_BYPASS_ON);
            break;
        }

        case DEI_MODE_DI:
        {
            TRC(TRC_ID_HQVDPLITE_DEI, "DEI_MODE_DI");
            m_MotionState = HQVDPLITE_DEI_MOTION_OFF;
            SetDeiMainMode     (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING);
            SetDeiInterpolation(csdi_config, HQVDPLITE_DEI_CTL_LUMA_DI, HQVDPLITE_DEI_CTL_CHROMA_DI);
            break;
        }

        case DEI_MODE_3D:
        {
            TRC(TRC_ID_HQVDPLITE_DEI, "DEI_MODE_3D");

            // If the same picture is repeated:
            // - Motion Buffers should not be rotated
            // - Motion State should be not updated
            if (!isPictureRepeated)
            {
                RotateMotionBuffers();
                UpdateMotionState();
            }
            else
            {
                TRC(TRC_ID_HQVDPLITE_DEI, "Pict repeated: Motion state unchanged");
            }

            // Update csdi_config according to the new motion state
            switch(m_MotionState)
            {
                case HQVDPLITE_DEI_MOTION_OFF:
                {
                    TRC(TRC_ID_HQVDPLITE_DEI, "MOTION_OFF");
                    SetDeiMainMode     (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING);
                    SetDeiInterpolation(csdi_config, HQVDPLITE_DEI_CTL_LUMA_DI, HQVDPLITE_DEI_CTL_CHROMA_DI);
                    break;
                }
                case HQVDPLITE_DEI_MOTION_INIT:
                {
                    TRC(TRC_ID_HQVDPLITE_DEI, "MOTION_INIT");
                    SetDeiMainMode     (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING);
                    SetDeiInterpolation(csdi_config, HQVDPLITE_DEI_CTL_LUMA_DI, HQVDPLITE_DEI_CTL_CHROMA_DI);
                    break;
                }
                case HQVDPLITE_DEI_MOTION_LOW_CONF:
                {
                    TRC(TRC_ID_HQVDPLITE_DEI, "MOTION_LOW");
                    SetDeiMainMode       (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING);
                    SetDeiInterpolation  (csdi_config, HQVDPLITE_DEI_CTL_LUMA_3D, HQVDPLITE_DEI_CTL_CHROMA_3D);
                    Set3DDEIConfiguration(&csdi_config);
                    break;
                }
                case HQVDPLITE_DEI_MOTION_FULL_CONF:
                {
                    TRC(TRC_ID_HQVDPLITE_DEI, "MOTION_FULL");
                    //m_MotionState is unchanged
                    SetDeiMainMode       (csdi_config, HQVDPLITE_DEI_CTL_MAIN_MODE_DEINTERLACING);
                    SetDeiInterpolation  (csdi_config, HQVDPLITE_DEI_CTL_LUMA_3D, HQVDPLITE_DEI_CTL_CHROMA_3D);
                    Set3DDEIConfiguration(&csdi_config);
                    break;
                }
            }
            break;
        }

        case DEI_MODE_VI:
        case DEI_MODE_MEDIAN:
        default:
            TRC(TRC_ID_ERROR, "Mode %d not implemented!", selectedDeiMode);
            break;
    }

    /* Write the Base Address of the Previous and Next Fields in the command */
    SetNextDEIField    (pNextNode, pDisplayInfo, pFieldSetup, pDisplayInfo->m_isSecondaryPictureSelected);
    SetPreviousDEIField(pPrevNode, pDisplayInfo, pFieldSetup, pDisplayInfo->m_isSecondaryPictureSelected);

    /* Set the current motion state */
    SetDeiMotionState(csdi_config, m_MotionState);

    /* Set Motion Buffers */
    pFieldSetup->Csdi.PrevMotion = m_PreviousMotionBufferPhysAddr;
    pFieldSetup->Csdi.CurMotion  = m_CurrentMotionBufferPhysAddr;
    pFieldSetup->Csdi.NextMotion = m_NextMotionBufferPhysAddr;

    /* Set the Csdi config */
    pFieldSetup->Csdi.Config = csdi_config;

    TRC(TRC_ID_HQVDPLITE_DEI, "csdi_config: %#.8x", pFieldSetup->Csdi.Config);

    TRC(TRC_ID_HQVDPLITE_DEI, "PY: %#.8x CY: %#.8x NY: %#.8x",
                 pFieldSetup->Csdi.PrevLuma,
                 pFieldSetup->Top.CurrentLuma,
                 pFieldSetup->Csdi.NextLuma);

    TRC(TRC_ID_HQVDPLITE_DEI, "PC: %#.8x CC: %#.8x NC: %#.8x",
                 pFieldSetup->Csdi.PrevChroma,
                 pFieldSetup->Top.CurrentChroma,
                 pFieldSetup->Csdi.NextChroma);

    TRC(TRC_ID_HQVDPLITE_DEI, "PM: %#.8x CM: %#.8x NM: %#.8x",
                 pFieldSetup->Csdi.PrevMotion,
                 pFieldSetup->Csdi.CurMotion,
                 pFieldSetup->Csdi.NextMotion);

    return true;
}

/*******************************************************************************
Name        : PrepareCmdHVSRCParams
Description : Prepare the HVSRC part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdHVSRCParams(CDisplayNode           *pCurrNode,
                                            CHqvdpLiteDisplayInfo  *pDisplayInfo,
                                            HQVDPLITE_CMD_t        *pFieldSetup)
{
    stm_rect_t  InputRectangle;     /* InputRectangle  in Frame notation*/
    stm_rect_t  OutputRectangle;    /* OutputRectangle in Frame notation*/
    int32_t     YHInitPhase = 0;
    int32_t     CHInitPhase = 0;
    int32_t     YVInitPhase = 0;
    int32_t     CVInitPhase = 0;
    uint32_t    ChromaInputWidth = 0;
    uint32_t    ChromaInputHeight = 0;

    HQVDPLITE_HVSRC_Params_t *pHVSRCParams = &pFieldSetup->Hvsrc;

    InputRectangle  = pDisplayInfo->m_selectedPicture.srcFrameRect;
    OutputRectangle = pDisplayInfo->m_dstFrameRect;

    /* Clear the content of the command params */
    vibe_os_zero_memory(pHVSRCParams, sizeof(HQVDPLITE_HVSRC_Params_t));




    /* Adapting output rectangle size with respect to output mode (SbS or TaB)*/
    switch(m_outputInfo.currentMode.mode_params.flags)
    {
        case STM_MODE_FLAGS_3D_SBS_HALF:
        {
            OutputRectangle.width = OutputRectangle.width/2;
            break;
        }
        case STM_MODE_FLAGS_3D_TOP_BOTTOM:
        {
            OutputRectangle.height = OutputRectangle.height/2;
            break;
        }
        default:
            break;
    }


    /****************************/
    /* HVSRC : HOR_PANORAMIC_CTRL */
    /****************************/
    pHVSRCParams->HorPanoramicCtrl = 0;

    /******************************/
    /* HVSRC  : OUTPUT_PICTURE_SIZE */
    /******************************/
    pHVSRCParams->OutputPictureSize = 0;

    /* HVSRC output should ALWAYS be a Frame */
    pHVSRCParams->OutputPictureSize = (OutputRectangle.height <<16) + OutputRectangle.width;

    /****************************/
    /* HVSRC  : INIT_HORIZONTAL */
    /****************************/
    pHVSRCParams->InitHorizontal = ((CHInitPhase << HVSRC_INIT_HORIZONTAL_CHROMA_PHASE_SHIFT_SHIFT) & HVSRC_INIT_HORIZONTAL_CHROMA_PHASE_SHIFT_MASK)
                                  |((YHInitPhase << HVSRC_INIT_HORIZONTAL_LUMA_INIT_PHASE_SHIFT) & HVSRC_INIT_HORIZONTAL_LUMA_INIT_PHASE_MASK);

    /**************************/
    /* HVSRC  : INIT_VERTICAL */
    /**************************/
    pHVSRCParams->InitVertical   = ((CVInitPhase << HVSRC_INIT_VERTICAL_CHROMA_PHASE_SHIFT_SHIFT) & HVSRC_INIT_VERTICAL_CHROMA_PHASE_SHIFT_MASK)
                                  |((YVInitPhase << HVSRC_INIT_VERTICAL_LUMA_INIT_PHASE_SHIFT) & HVSRC_INIT_VERTICAL_LUMA_INIT_PHASE_MASK);

    /***********************/
    /* HVSRC  : PARAM_CTRL */
    /***********************/

    /* Disable Auto Control all the time because it doesn't work as expected */
    SetBitField(pHVSRCParams->ParamCtrl, V_AUTOKOVS, V_AUTOKOVS_DEFAULT);
    SetBitField(pHVSRCParams->ParamCtrl, H_AUTOKOVS, H_AUTOKOVS_DEFAULT);

    /*
     * Luma Settings
     */

    /* Disable Luma Horizontal Overshoting when there is no horizontal scaling */
    if (OutputRectangle.width == InputRectangle.width)
    {
        SetBitField(pHVSRCParams->ParamCtrl, H_ACON, H_ACON_DEFAULT);
        SetBitField(pHVSRCParams->ParamCtrl, YH_KOVS, YH_KOVS_DISABLE);
    }
    /* Disable Horizontal Antiringing and Luma Horizontal Overshoting in width downscaling */
    else if (OutputRectangle.width < InputRectangle.width)
    {
        SetBitField(pHVSRCParams->ParamCtrl, H_ACON, H_ACON_DISABLE);
        SetBitField(pHVSRCParams->ParamCtrl, YH_KOVS, YH_KOVS_DISABLE);
    }
    /* Enable Horizontal Antiringing and Luma Horizontal Overshoting in width upscaling */
    else
    {
        SetBitField(pHVSRCParams->ParamCtrl, H_ACON, H_ACON_DEFAULT);
        SetBitField(pHVSRCParams->ParamCtrl, YH_KOVS, YH_KOVS_MAX);
    }

    /* Disable Luma Vertical OverShooting when there is no vertical Scaling */
    if(OutputRectangle.height == InputRectangle.height)
    {
        SetBitField(pHVSRCParams->ParamCtrl, V_ACON, V_ACON_DEFAULT);
        SetBitField(pHVSRCParams->ParamCtrl, YV_KOVS, YV_KOVS_DISABLE);
    }
    /* Disable Vertical Antiringing and Luma Vertical OverShooting in height downscaling */
    else if(OutputRectangle.height < InputRectangle.height)
    {
        SetBitField(pHVSRCParams->ParamCtrl, V_ACON, V_ACON_DISABLE);
        SetBitField(pHVSRCParams->ParamCtrl, YV_KOVS, YV_KOVS_DISABLE);
    }
    /* Enable Vertical Antiringing and Luma Vertical OverShooting in height upscaling */
    else
    {
        SetBitField(pHVSRCParams->ParamCtrl, V_ACON, V_ACON_DEFAULT);
        SetBitField(pHVSRCParams->ParamCtrl, YV_KOVS, YV_KOVS_MAX);
    }


    /*
     * Chroma Settings
     */

    /* Calculate input chroma width base on the color format */
    if(pDisplayInfo->m_selectedPicture.isSrc420)
    {
        ChromaInputWidth = InputRectangle.width / 2;
        ChromaInputHeight = InputRectangle.height / 2;
    }
    else if(pDisplayInfo->m_selectedPicture.isSrc422)
    {

        ChromaInputWidth = InputRectangle.width / 2;
        ChromaInputHeight = InputRectangle.height;
    }
    else
    {
        ChromaInputWidth = InputRectangle.width;
        ChromaInputHeight = InputRectangle.height;
    }

    /* Disable Chroma Horizonal Overshooting when there is no Horizontal Scaling */
    if(OutputRectangle.width == InputRectangle.width)
    {
        SetBitField(pHVSRCParams->ParamCtrl, CH_KOVS, CH_KOVS_DISABLE);
    }
    /* Set Chroma Horizonal Overshooting based on the color format */
    else
    {
        /* Disable Chroma Horizonal Overshooting in width downscaling base on the color format */
        if(OutputRectangle.width < ChromaInputWidth)
        {
            SetBitField(pHVSRCParams->ParamCtrl, CH_KOVS, CH_KOVS_DISABLE);
        }
        /* Enable Chroma Horizonal Overshooting in width upscaling base on the color format */
        else
        {
            SetBitField(pHVSRCParams->ParamCtrl, CH_KOVS, CH_KOVS_MAX);
        }
    }

    /* Disable Chroma Vertical OverShooting when there is no Vertical Scaling */
    if(OutputRectangle.height == InputRectangle.height)
    {
        SetBitField(pHVSRCParams->ParamCtrl, CH_KOVS, CH_KOVS_DISABLE);
    }
    /* Set Chroma Vertical OverShooting based on the color format */
    else
    {
        /* Disable Chroma Vertical OverShooting in height downscaling base on the color format */
        if(OutputRectangle.height < ChromaInputHeight)
        {
            SetBitField(pHVSRCParams->ParamCtrl, CV_KOVS, CV_KOVS_DISABLE);
        }
        /* Enable Chroma Vertical OverShooting in height upscaling base on the color format */
        else
        {
            SetBitField(pHVSRCParams->ParamCtrl, CV_KOVS, CV_KOVS_MAX);
        }
    }

    /* Set the hvsrc lookup table which is read from lld */
    c8fvp3_SetHvsrcLut(pFieldSetup, (uint8_t)m_FilterSetLuma, (uint8_t)m_FilterSetChroma,
                       (uint8_t)m_FilterSetLuma, (uint8_t)m_FilterSetChroma);
    return true;
}


/*******************************************************************************
Name        : PrepareCmdIQIParams
Description : Prepare the IQI part of the HQVDPLITE command.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::PrepareCmdIQIParams (CDisplayNode             *pCurrNode,
                                           CHqvdpLiteDisplayInfo    *pDisplayInfo,
                                           bool                      isTopFieldOnDisplay,
                                           bool                      isDisplayInterlaced,
                                           HQVDPLITE_IQI_Params_t   *pIQIParams)
{
    /* Clear the content of the command params */
    vibe_os_zero_memory(pIQIParams, sizeof(HQVDPLITE_IQI_Params_t));

    pIQIParams->ConBri = 0x100;
    pIQIParams->SatGain = 0x100;

    pIQIParams->DefaultColor = m_iqiDefaultColorYCbCr;

    pIQIParams->PxfConf = 0;

    pIQIParams->Config = m_iqiConfig;

    if (!pDisplayInfo->m_isUsingProgressive2InterlacedHW)
    {
        pIQIParams->PxfConf = PROG_TO_INT_BYPASS_ENABLE;
    }
    else
    {
        /* Interlaced output: P2I should be ON */
        pIQIParams->PxfConf &= ~PROG_TO_INT_BYPASS_ENABLE;
        /* the polarity */
        if(isTopFieldOnDisplay)
            pIQIParams->PxfConf &=~PROG_TO_INT_PRESENT_TOP_FIELD;
        else
            pIQIParams->PxfConf |= PROG_TO_INT_PRESENT_TOP_FIELD;
    }

    if((pIQIParams->Config != IQI_CONFIG_DISABLE_DEFAULT) /*&& (m_isIQIChanged == true)*/) {

       if ( pIQIParams->Config & IQI_CONFIG_PK_ENABLE_MASK ) {
           m_iqiPeaking.CalculateSetup (pDisplayInfo, isDisplayInterlaced, &m_peaking_setup);
           m_iqiPeaking.CalculateParams(&m_peaking_setup, pIQIParams);

           if(m_isIQIChanged == true) {
                TRC( TRC_ID_HQVDPLITE, "PK Config register content : 0x%x", pIQIParams->PkConfig);
                TRC( TRC_ID_HQVDPLITE, "PK Coeff0Coeff1 register content : 0x%x", pIQIParams->Coeff0Coeff1);
                TRC( TRC_ID_HQVDPLITE, "PK Coeff2Coeff3 register content : 0x%x", pIQIParams->Coeff2Coeff3);
                TRC( TRC_ID_HQVDPLITE, "PK Coeff4 register content : 0x%x", pIQIParams->Coeff4);
                TRC( TRC_ID_HQVDPLITE, "PK PkLut register content : 0x%x", pIQIParams->PkLut);
                TRC( TRC_ID_HQVDPLITE, "PK PkGain register content : 0x%x", pIQIParams->PkGain);
                TRC( TRC_ID_HQVDPLITE, "PK PkCoringLevel register content : 0x%x", pIQIParams->PkCoringLevel);
          }

       }
       if ( pIQIParams->Config & IQI_CONFIG_LE_ENABLE_MASK ) {
           m_iqiLe.CalculateSetup (pCurrNode, &m_le_setup);
           m_iqiLe.CalculateParams(&m_le_setup, pIQIParams);
           if (m_le_setup.le.enable_csc) pIQIParams->Config |= IQI_CONFIG_CSC_ENABLE_MASK;
           if(m_isIQIChanged == true) {

              TRC( TRC_ID_HQVDPLITE,"LE Config register content : 0x%x", pIQIParams->LeConfig);

              for(int j=0; j<64; j++)
                TRC( TRC_ID_HQVDPLITE,"LE LUT register content : 0x%x", pIQIParams->LeLut[j]);

            }


       }
       if ( pIQIParams->Config & IQI_CONFIG_CTI_ENABLE_MASK ) {
           m_iqiCti.CalculateParams(pIQIParams);
       }

       if(m_isIQIChanged == true) TRC( TRC_ID_HQVDPLITE," IQI Config register content : 0x%x", pIQIParams->Config);
       m_isIQIChanged = false;
    }
    return true;

}

/*******************************************************************************
Name        : IsIQIContextChanged
Description : Check whether there is new IQI configuration
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::IsIQIContextChanged (void)
{
    return m_isIQIChanged;
}


TuningResults CHqvdpLitePlane::SetTuning(uint16_t service, void *inputList, uint32_t inputListSize,
                                         void *outputList, uint32_t outputListSize)

{
    TuningResults res = TUNING_INVALID_PARAMETER;
    tuning_service_type_t  ServiceType = (tuning_service_type_t)service;
    char *string = (char*)NULL;
    uint32_t lenthOfString = 0;

    TRC( TRC_ID_UNCLASSIFIED, "Start!" );

    switch(ServiceType)
    {
        case CRC_CAPABILITY:
        {
            if(outputList != 0)
            {
                string = (char *)outputList;
                lenthOfString = vibe_os_snprintf(string, outputListSize, "HQVDP,InputY,InputUv,NextYFmd,NextNextYFmd,NextNextNextYFmd,PrevYCsdi,CurYCsdi,NextYCsdi,PrevUvCsdi,CurUvCsdi,NextUvCsdi,YCsdi,UvCsdi,UvCup,MotCsdi,MotCurCsdi,MotPrevCsdi,YHvr,UHvr,VHvr,PxfItStatus,YIqi,UIqi,VIqi");
                outputListSize = lenthOfString;
                res = TUNING_OK;
            }
            else
            {
                TRC( TRC_ID_ERROR, "Invalid outputList!");
            }
            break;
        }
        case CRC_COLLECT:
        {

            TRC( TRC_ID_UNCLASSIFIED, "CRC_COLLECT()" );

            SetTuningOutputData_t *output = (SetTuningOutputData_t *)outputList;
            if(output != 0)
            {
                if ( m_pDisplayDevice->VSyncLock() != 0)
                    return TUNING_EINTR;

                if(m_HQVdpLiteCrcData.Status)
                {
                    output->Data.Crc = m_HQVdpLiteCrcData;
                    TRC( TRC_ID_UNCLASSIFIED, "VSync %lld PTS : %lld", output->Data.Crc.LastVsyncTime, output->Data.Crc.PTS);

                    /* clear m_HQVdpLiteCrcData next Vsync */
                    vibe_os_zero_memory(&m_HQVdpLiteCrcData, sizeof(m_HQVdpLiteCrcData));
                    res = TUNING_OK;
                }
                else
                {
                    res = TUNING_NO_DATA_AVAILABLE;
                }
                m_pDisplayDevice->VSyncUnlock();
            }
            else
            {
                TRC( TRC_ID_ERROR, "Invalid output!");
            }
            break;
        }
        default:
        {
            res = CDisplayPlane::SetTuning(service, inputList, inputListSize, outputList, outputListSize);
            break;
        }
    }
    return res;
}
/*******************************************************************************
Name        : XP70Init
Description :
Parameters  :
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
bool CHqvdpLitePlane::XP70Init( )
{
    bool IsSuccessfull=false;

    /* Init xp70 */
    IsSuccessfull = InitializeHQVDPLITE();
    if (!IsSuccessfull)
    {
        TRC( TRC_ID_ERROR, "Error during InitializeHQVDPLITE" );
        return (false);
    }

    IsSuccessfull = AllocateSharedDataStructure();
    if (!IsSuccessfull)
    {
        TRC( TRC_ID_ERROR, "Error during Allocation of shared data" );
    }

    return (IsSuccessfull);
}

/*******************************************************************************
Name        : InitializeHQVDPLITE
Description : initialize the HQVDPLITE.
Parameters  : init params.
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::InitializeHQVDPLITE()
{
    bool        IsSuccessfull=false;
#ifndef CONFIG_STM_VIRTUAL_PLATFORM
    uint32_t    XP70ResetDone = 0;
#endif

    /* soft reset */
    WriteMbxReg(mailbox_SW_RESET_CTRL_OFFSET, 0xFFFFFFFF);
    WriteMbxReg(mailbox_SW_RESET_CTRL_OFFSET, 0x00000000);

/* HCE workaround (from SDK1) */
#ifndef CONFIG_STM_VIRTUAL_PLATFORM
    /* Wait until XP70 ReadyForDownload */
    do
    {
        XP70ResetDone = (ReadMbxReg(  mailbox_STARTUP_CTRL_1_OFFSET)) & XP70_RESET_DONE;
    }
    while (XP70ResetDone == 0);
#endif

    /* Load RD and WR plug with microcode */
    IsSuccessfull = LoadPlugs();
    if (IsSuccessfull != true)
    {
        TRC( TRC_ID_ERROR, "Error while loading Plugs!" );
        return (false);
    }

    IsSuccessfull = LoadFirmwareFromHeaderFile();
    if (IsSuccessfull != true)
    {
        TRC( TRC_ID_ERROR, "Error while loading Firmware!" );
        return (false);
    }

    c8fvp3_BootIp((uint32_t)m_baseAddress);

    /* workaround hang up VSOC */

    TRC( TRC_ID_MAIN_INFO, "****** BootIp *****" );

    while( c8fvp3_IsBootIpDone ((uint32_t)m_baseAddress) == FALSE);

    /* Test if Bilbo is ready to receive first command */
    while( c8fvp3_IsReady ((uint32_t)m_baseAddress) == FALSE);

    /* Send a NULL Cmd to the firmware */
    c8fvp3_SetCmdAddress((uint32_t)m_baseAddress, 0x0);

    /* Enable the xp70 to start on hard Vsync */
    c8fvp3_SetVsyncMode((uint32_t)m_baseAddress, mailbox_SOFT_VSYNC_VSYNC_MODE_HW);
    return true;
}

/*******************************************************************************
Name        : AllocateSharedDataStructure
Description : Allocate the data structures used to communicate with the firmware. Two structures are needed
              because a flip flop should be done between a structured used by the firmware and a
              structure prepared for next VSync.
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::AllocateSharedDataStructure()
{
    /* allocation of data shared with firmware (must be allocated in NON CACHED memory) */

    /*******************/
    /*   SharedArea1   */
    /*******************/

    vibe_os_allocate_dma_area(&m_SharedArea1, sizeof(HQVDPLITE_CMD_t), DCACHE_LINE_SIZE, SDAAF_NONE);
    if (!m_SharedArea1.pMemory)
    {
        TRC( TRC_ID_ERROR, "Error during AllocateDMAArea for SharedArea1 !" );
        return (false);
    }
    vibe_os_zero_memory(m_SharedArea1.pData, m_SharedArea1.ulDataSize);

    /*******************/
    /*   SharedArea2   */
    /*******************/
    vibe_os_allocate_dma_area(&m_SharedArea2, sizeof(HQVDPLITE_CMD_t), DCACHE_LINE_SIZE, SDAAF_NONE);
    if (!m_SharedArea2.pMemory)
    {
        TRC( TRC_ID_ERROR, "Error during AllocateDMAArea for SharedArea2 !" );
        return (false);
    }
    vibe_os_zero_memory(m_SharedArea2.pData, m_SharedArea2.ulDataSize);

    /* allocation successful */
    m_SharedAreaPreparedForNextVSync_p =&m_SharedArea1;
    m_SharedAreaUsedByFirmware_p =&m_SharedArea2;

    return (true);
}

/*******************************************************************************
Name        : LoadPlugs
Description : Load RD and WR plug with microcode
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::LoadPlugs()
{
    uint32_t     *ReadPlugBaseAddress, *WritePlugBaseAddress;
    /* Init Plugs */
    ReadPlugBaseAddress = (uint32_t *)((uint8_t *)m_baseAddress + HQVDPLITE_STR64_RDPLUG_OFFSET);
    WritePlugBaseAddress = (uint32_t *)((uint8_t *)m_baseAddress + HQVDPLITE_STR64_WRPLUG_OFFSET);

    /* ucode Plugs */
    c8fvp3_config_ucodeplug((uint32_t )ReadPlugBaseAddress, HQVDP_UcodeRdPlug, HQVDP_UCODERDPLUG_SIZE);
    c8fvp3_config_ucodeplug((uint32_t )WritePlugBaseAddress,HQVDP_UcodeWrPlug,HQVDP_UCODEWRPLUG_SIZE);
    SetPlugsConfiguration(ReadPlugBaseAddress, WritePlugBaseAddress);
    return (true);
}

/*******************************************************************************
Name        : LoadFirmwareFromFile
Description :
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::LoadFirmwareFromHeaderFile()
{
    DMA_Area IMEM;
    DMA_Area DMEM;
    DMA_Area* IMEM_p = 0;
    DMA_Area* DMEM_p = 0;

    bool        IsSuccessfull=false;
    bool        loadFwThroughRegWriteNotStreamer = 1; /*0: used for VSOC, 1: used for SOC */

    if (loadFwThroughRegWriteNotStreamer == 1)
    {
        /* upload firmware from array */
        if (HQVDPLITE_PMEM_SIZE <= 0x10000)
        {
            /*** Load IMEM ***/
            c8fvp3_download_code((uint32_t)m_baseAddress, loadFwThroughRegWriteNotStreamer, (uint32_t)(HQVDPLITE_pmem), HQVDPLITE_PMEM_OFFSET, HQVDPLITE_XP70_IMEM_SIZE);

            /*** Load PMEM ***/
            c8fvp3_download_code((uint32_t)m_baseAddress, loadFwThroughRegWriteNotStreamer, (uint32_t)(HQVDPLITE_dmem), HQVDPLITE_DMEM_OFFSET, HQVDPLITE_XP70_DMEM_SIZE);

            IsSuccessfull=true;

            TRC( TRC_ID_MAIN_INFO, "Firmware uploaded successfully" );
        }
        else
        {
            TRC( TRC_ID_ERROR, "ERROR !!!! BILBO CODE SIZE TOO LARGE: Max is 0xC000" );

            IsSuccessfull=false;
        }
    }
    else
    {
        vibe_os_allocate_dma_area(&IMEM, HQVDPLITE_XP70_IMEM_SIZE, DCACHE_LINE_SIZE, SDAAF_VIDEO_MEMORY);
        IMEM_p = &IMEM;
        if (!IMEM_p->pMemory)
        {
            TRC( TRC_ID_ERROR, "Error during AllocateDMAArea for IMEM !" );
            return (false);
        }
        vibe_os_zero_memory(IMEM_p->pData, IMEM_p->ulDataSize);

        vibe_os_memcpy_to_dma_area (IMEM_p, 0, HQVDPLITE_pmem, HQVDPLITE_XP70_IMEM_SIZE);
        vibe_os_flush_dma_area(IMEM_p, 0, HQVDPLITE_XP70_IMEM_SIZE);

        vibe_os_allocate_dma_area(&DMEM, HQVDPLITE_XP70_DMEM_SIZE, DCACHE_LINE_SIZE, SDAAF_VIDEO_MEMORY);
        DMEM_p = &DMEM;
        if (!DMEM_p->pMemory)
        {
            TRC( TRC_ID_ERROR, "Error during AllocateDMAArea for DMEM !" );
            vibe_os_free_dma_area(&IMEM);
            return (false);
        }
        vibe_os_zero_memory(DMEM_p->pData, DMEM_p->ulDataSize);

        vibe_os_memcpy_to_dma_area (DMEM_p, 0, HQVDPLITE_dmem, HQVDPLITE_XP70_DMEM_SIZE);
        vibe_os_flush_dma_area(DMEM_p, 0, HQVDPLITE_XP70_DMEM_SIZE);

        /* upload firmware from array */
        if (HQVDPLITE_PMEM_SIZE <= 0x10000)
        {
            /*** Load IMEM ***/
            c8fvp3_download_code((uint32_t)m_baseAddress, loadFwThroughRegWriteNotStreamer, IMEM_p->ulPhysical, HQVDPLITE_PMEM_OFFSET, HQVDPLITE_XP70_IMEM_SIZE);
            /*** Load PMEM ***/
            c8fvp3_download_code((uint32_t)m_baseAddress, loadFwThroughRegWriteNotStreamer, DMEM_p->ulPhysical, HQVDPLITE_DMEM_OFFSET, HQVDPLITE_XP70_DMEM_SIZE);

            IsSuccessfull=true;

            TRC( TRC_ID_MAIN_INFO, "Firmware uploaded successfully" );
          }
        else
          {
            TRC( TRC_ID_ERROR, "ERROR !!!! BILBO CODE SIZE TOO LARGE: Max is 0xC000" );

            IsSuccessfull=false;
        }
        vibe_os_free_dma_area(&IMEM);
        vibe_os_free_dma_area(&DMEM);
    }

    return (IsSuccessfull);
}


/*******************************************************************************
Name        : SetPlugsConfiguration
Description : Set Plugs configuration. The same configuration works for Raster and Macroblock sources
Parameters  :
Assumptions :
Limitations :
Returns     : true if successful, false otherwise
*******************************************************************************/
bool CHqvdpLitePlane::SetPlugsConfiguration(uint32_t *ReadPlugBaseAddress, uint32_t *WritePlugBaseAddress)
{
    /* configure STBus RD PLUG registers  */
    c8fvp3_config_regplug((uint32_t) ReadPlugBaseAddress, 0x02,0x03,0x06,0x01,0x00,DEFAULT_MIN_SPEC_BETWEEN_REQS,0x01);
    /* configure STBus WR PLUG registers */
    c8fvp3_config_regplug((uint32_t) WritePlugBaseAddress,0x02,0x03,0x06,0x01,0x00,DEFAULT_MIN_SPEC_BETWEEN_REQS,0x01) ;
    return (true);
}

/*******************************************************************************
Name        : XP70Term
Description :
Parameters  :
Assumptions :
Limitations :
Returns     : nothing
*******************************************************************************/
void CHqvdpLitePlane::XP70Term()
{
    DeallocateSharedDataStructure();
    vibe_os_free_dma_area(&m_pPreviousFromFirmware0);
}

/*******************************************************************************
Name        : DeallocateSharedDataStructure
Description : Deallocate the data structures used to communicate with the firmware.
Parameters  :
Assumptions :
Limitations :
Returns     : nothing
*******************************************************************************/
void CHqvdpLitePlane::DeallocateSharedDataStructure()
{
    vibe_os_free_dma_area(&m_SharedArea1);
    vibe_os_free_dma_area(&m_SharedArea2);
}


