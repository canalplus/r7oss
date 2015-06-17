/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2007 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : Common Decoder API definitions
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: decapicommon.h,v $
--  $Date: 2010/12/13 13:04:01 $
--  $Revision: 1.24 $
--
------------------------------------------------------------------------------*/
#ifndef __DECAPICOMMON_H__
#define __DECAPICOMMON_H__


#define MPEG4_NOT_SUPPORTED             (unsigned int)(0x00)
#define MPEG4_SIMPLE_PROFILE            (unsigned int)(0x01)
#define MPEG4_ADVANCED_SIMPLE_PROFILE   (unsigned int)(0x02)
#define MPEG4_CUSTOM_NOT_SUPPORTED      (unsigned int)(0x00)
#define MPEG4_CUSTOM_FEATURE_1          (unsigned int)(0x01)
#define H264_NOT_SUPPORTED              (unsigned int)(0x00)
#define H264_BASELINE_PROFILE           (unsigned int)(0x01)
#define H264_MAIN_PROFILE               (unsigned int)(0x02)
#define H264_HIGH_PROFILE               (unsigned int)(0x03)
#define VC1_NOT_SUPPORTED               (unsigned int)(0x00)
#define VC1_SIMPLE_PROFILE              (unsigned int)(0x01)
#define VC1_MAIN_PROFILE                (unsigned int)(0x02)
#define VC1_ADVANCED_PROFILE            (unsigned int)(0x03)
#define MPEG2_NOT_SUPPORTED             (unsigned int)(0x00)
#define MPEG2_MAIN_PROFILE              (unsigned int)(0x01)
#define JPEG_NOT_SUPPORTED              (unsigned int)(0x00)
#define JPEG_BASELINE                   (unsigned int)(0x01)
#define JPEG_PROGRESSIVE                (unsigned int)(0x02)
#define PP_NOT_SUPPORTED                (unsigned int)(0x00)
#define PP_SUPPORTED                    (unsigned int)(0x01)
#define PP_TILED_4X4                    (unsigned int)(0x20000000)
#define PP_DITHERING                    (unsigned int)(0x10000000)
#define PP_SCALING                      (unsigned int)(0x0C000000)
#define PP_DEINTERLACING                (unsigned int)(0x02000000)
#define PP_ALPHA_BLENDING               (unsigned int)(0x01000000)
#define PP_OUTP_ENDIAN                  (unsigned int)(0x00040000)
#define PP_TILED_INPUT                  (unsigned int)(0x0000C000)
#define PP_PIX_ACC_OUTPUT               (unsigned int)(0x40000000)
#define PP_ABLEND_CROP                  (unsigned int)(0x80000000)
#define SORENSON_SPARK_NOT_SUPPORTED    (unsigned int)(0x00)
#define SORENSON_SPARK_SUPPORTED        (unsigned int)(0x01)
#define VP6_NOT_SUPPORTED               (unsigned int)(0x00)
#define VP6_SUPPORTED                   (unsigned int)(0x01)
#define VP7_NOT_SUPPORTED               (unsigned int)(0x00)
#define VP7_SUPPORTED                   (unsigned int)(0x01)
#define VP8_NOT_SUPPORTED               (unsigned int)(0x00)
#define VP8_SUPPORTED                   (unsigned int)(0x01)
#define REF_BUF_NOT_SUPPORTED           (unsigned int)(0x00)
#define REF_BUF_SUPPORTED               (unsigned int)(0x01)
#define REF_BUF_INTERLACED              (unsigned int)(0x02)
#define REF_BUF_DOUBLE                  (unsigned int)(0x04)
#define TILED_NOT_SUPPORTED             (unsigned int)(0x00)
#define TILED_8x4_SUPPORTED             (unsigned int)(0x01)
#define AVS_NOT_SUPPORTED               (unsigned int)(0x00)
#define AVS_SUPPORTED                   (unsigned int)(0x01)
#define JPEG_EXT_NOT_SUPPORTED          (unsigned int)(0x00)
#define JPEG_EXT_SUPPORTED              (unsigned int)(0x01)
#define RV_NOT_SUPPORTED                (unsigned int)(0x00)
#define RV_SUPPORTED                    (unsigned int)(0x01)
#define MVC_NOT_SUPPORTED               (unsigned int)(0x00)
#define MVC_SUPPORTED                   (unsigned int)(0x01)
#define WEBP_NOT_SUPPORTED              (unsigned int)(0x00)
#define WEBP_SUPPORTED                  (unsigned int)(0x01)

#define H264_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define H264_FUSE_ENABLED                         (unsigned int)(0x01)
#define MPEG4_NOT_SUPPORTED_FUSE                 (unsigned int)(0x00)
#define MPEG4_FUSE_ENABLED                         (unsigned int)(0x01)
#define MPEG2_NOT_SUPPORTED_FUSE                 (unsigned int)(0x00)
#define MPEG2_FUSE_ENABLED                         (unsigned int)(0x01)
#define SORENSON_SPARK_NOT_SUPPORTED_FUSE         (unsigned int)(0x00)
#define SORENSON_SPARK_ENABLED                     (unsigned int)(0x01)
#define JPEG_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define JPEG_FUSE_ENABLED                         (unsigned int)(0x01)
#define VP6_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define VP6_FUSE_ENABLED                         (unsigned int)(0x01)
#define VP7_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define VP7_FUSE_ENABLED                         (unsigned int)(0x01)
#define VP8_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define VP8_FUSE_ENABLED                         (unsigned int)(0x01)
#define VC1_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define VC1_FUSE_ENABLED                         (unsigned int)(0x01)
#define JPEG_PROGRESSIVE_NOT_SUPPORTED_FUSE         (unsigned int)(0x00)
#define JPEG_PROGRESSIVE_FUSE_ENABLED             (unsigned int)(0x01)
#define REF_BUF_NOT_SUPPORTED_FUSE                 (unsigned int)(0x00)
#define REF_BUF_FUSE_ENABLED                     (unsigned int)(0x01)
#define AVS_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define AVS_FUSE_ENABLED                         (unsigned int)(0x01)
#define RV_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define RV_FUSE_ENABLED                             (unsigned int)(0x01)
#define MVC_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define MVC_FUSE_ENABLED                         (unsigned int)(0x01)

#define PP_NOT_SUPPORTED_FUSE                     (unsigned int)(0x00)
#define PP_FUSE_ENABLED                             (unsigned int)(0x01)
#define PP_FUSE_DEINTERLACING_ENABLED            (unsigned int)(0x40000000)
#define PP_FUSE_ALPHA_BLENDING_ENABLED           (unsigned int)(0x20000000)
#define MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED       (unsigned int)(0x00008000)
#define MAX_PP_OUT_WIDHT_1280_FUSE_ENABLED       (unsigned int)(0x00004000)
#define MAX_PP_OUT_WIDHT_720_FUSE_ENABLED        (unsigned int)(0x00002000)
#define MAX_PP_OUT_WIDHT_352_FUSE_ENABLED        (unsigned int)(0x00001000)

/* Macro to copy support flags and picture max width from DWL HW config
 * to Decoder HW config */
#define SET_DEC_BUILD_SUPPORT(decHwCfg, dwlHwCfg) \
    decHwCfg.maxDecPicWidth = dwlHwCfg.maxDecPicWidth; \
    decHwCfg.maxPpOutPicWidth = dwlHwCfg.maxPpOutPicWidth; \
    decHwCfg.h264Support = dwlHwCfg.h264Support; \
    decHwCfg.jpegSupport = dwlHwCfg.jpegSupport; \
    decHwCfg.jpegESupport = dwlHwCfg.jpegESupport; \
    decHwCfg.mpeg2Support = dwlHwCfg.mpeg2Support; \
    decHwCfg.mpeg4Support = dwlHwCfg.mpeg4Support; \
    decHwCfg.vc1Support = dwlHwCfg.vc1Support; \
    decHwCfg.ppSupport = dwlHwCfg.ppSupport; \
    decHwCfg.ppConfig = dwlHwCfg.ppConfig; \
    decHwCfg.sorensonSparkSupport = dwlHwCfg.sorensonSparkSupport; \
    decHwCfg.vp6Support = dwlHwCfg.vp6Support; \
    decHwCfg.vp7Support = dwlHwCfg.vp7Support; \
    decHwCfg.vp8Support = dwlHwCfg.vp8Support; \
    decHwCfg.refBufSupport = dwlHwCfg.refBufSupport; \
    decHwCfg.tiledModeSupport = dwlHwCfg.tiledModeSupport; \
    decHwCfg.avsSupport = dwlHwCfg.avsSupport; \
    decHwCfg.rvSupport = dwlHwCfg.rvSupport; \
    decHwCfg.customMpeg4Support = dwlHwCfg.customMpeg4Support; \
    decHwCfg.mvcSupport = dwlHwCfg.mvcSupport; \
    decHwCfg.webpSupport = dwlHwCfg.webpSupport;

typedef struct DecHwConfig_ {
	unsigned int mpeg4Support;        /* one of the MPEG4 values defined above */
	unsigned int customMpeg4Support;  /* one of the MPEG4 custom values defined above */
	unsigned int h264Support;         /* one of the H264 values defined above */
	unsigned int vc1Support;          /* one of the VC1 values defined above */
	unsigned int mpeg2Support;        /* one of the MPEG2 values defined above */
	unsigned int jpegSupport;         /* one of the JPEG values defined above */
	unsigned int jpegProgSupport;     /* one of the Progressive JPEG values defined above */
	unsigned int maxDecPicWidth;      /* maximum picture width in decoder */
	unsigned int ppSupport;           /* PP_SUPPORTED or PP_NOT_SUPPORTED */
	unsigned int ppConfig;            /* Bitwise list of PP function */
	unsigned int maxPpOutPicWidth;    /* maximum post-processor output picture width */
	unsigned int sorensonSparkSupport;   /* one of the SORENSON_SPARK values defined above */
	unsigned int refBufSupport;       /* one of the REF_BUF values defined above */
	unsigned int tiledModeSupport; /* one of the TILED values defined above */
	unsigned int vp6Support;          /* one of the VP6 values defined above */
	unsigned int vp7Support;          /* one of the VP7 values defined above */
	unsigned int vp8Support;          /* one of the VP8 values defined above */
	unsigned int avsSupport;          /* one of the AVS values defined above */
	unsigned int jpegESupport;        /* one of the JPEG EXT values defined above */
	unsigned int rvSupport;           /* one of the HUKKA values defined above */
	unsigned int mvcSupport;          /* one of the MVC values defined above */
	unsigned int webpSupport;         /* one of the WEBP values defined above */
} DecHwConfig;

/* Reference picture format types */
typedef enum {
	DEC_REF_FRM_RASTER_SCAN          = 0,
	DEC_REF_FRM_TILED_DEFAULT        = 1
} DecRefFrmFormat;

/* Output picture format types */
typedef enum {
	DEC_OUT_FRM_RASTER_SCAN          = 0,
	DEC_OUT_FRM_TILED_8X4            = 1
} DecOutFrmFormat;

#endif /* __DECAPICOMMON_H__ */
