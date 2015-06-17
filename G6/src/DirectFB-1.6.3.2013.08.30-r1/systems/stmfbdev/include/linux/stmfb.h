/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfb.h
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STMFB_H
#define _STMFB_H

/*
 * Surface definitions for usermode, in the kernel driver they are
 * already defined internally as part of the generic framework.
 */
#if !defined(__KERNEL__)
#include <sys/time.h>
typedef enum
{
    SURF_NULL_PAD,
    SURF_RGB565 ,
    SURF_RGB888 ,
    SURF_ARGB8565,
    SURF_ARGB8888,
    SURF_ARGB1555,
    SURF_ARGB4444,
    SURF_CRYCB888,   /* Note the order of the components */
    SURF_YCBCR422R,
    SURF_YCBCR422MB,
    SURF_YCBCR420MB,
    SURF_ACRYCB8888, /* Note the order, not compatible with DirectFB's AYUV */
    SURF_CLUT1,
    SURF_CLUT2,
    SURF_CLUT4,
    SURF_CLUT8,
    SURF_ACLUT44,
    SURF_ACLUT88,
    SURF_A1,
    SURF_A8,
    SURF_BGRA8888, /* Bigendian ARGB */
    SURF_YUYV,     /* 422R with luma and chroma byteswapped              */
    SURF_YUV420,   /* Planar YUV with 1/2 horizontal and vertical chroma */
                   /* in three separate buffers Y,Cb then Cr             */
    SURF_YVU420,   /* Planar YUV with 1/2 horizontal and vertical chroma */
                   /* in three separate buffers Y,Cr then Cb             */
    SURF_YUV422P,  /* Planar YUV with 1/2 horizontal chroma              */
                   /* in three separate buffers Y,Cb then Cr             */

    SURF_RLD_BD,   /* RLE Decoding controlled by setting source format   */
    SURF_RLD_H2,
    SURF_RLD_H8,
    SURF_CLUT8_ARGB4444_ENTRIES, /* For cursor plane support             */
    SURF_YCbCr420R2B, /* fourCC: NV12: 12 bit YCbCr (8 bit Y plane followed by
                                                     one 16 bit quarter size
                                                     Cb|Cr [7:0|7:0] plane) */
    SURF_YCbCr422R2B, /* fourCC: NV16: 16 bit YCbCr (8 bit Y plane followed by
                                                     one 16 bit half width
                                                     Cb|Cr [7:0|7:0] plane) */

    SURF_END,
    SURF_COUNT = SURF_END
}stm_pixel_format_t;

typedef stm_pixel_format_t SURF_FMT; /*!< For backwards compatibility */

#endif /* !__KERNEL__ */

typedef enum
{
    STMFBGP_FRAMEBUFFER,

    STMFBGP_GFX_FIRST,
    STMFBGP_GFX0 = STMFBGP_GFX_FIRST,
    STMFBGP_GFX1,
    STMFBGP_GFX2,
    STMFBGP_GFX3,
    STMFBGP_GFX4,
    STMFBGP_GFX_LAST = STMFBGP_GFX4

} STMFB_GFXMEMORY_PARTITION;


/*
 * Defined for STMFBIO_CGMS_ANALOG (use as bitfield)
 */
#define STMFBIO_CGMS_ANALOG_COPYRIGHT_ASSERTED 1 /* copyright bit */
#define STMFBIO_CGMS_ANALOG_COPYING_RESTRICTED 2 /* generation bit */


#define STMFBIO_COLOURKEY_FLAGS_ENABLE	0x00000001
#define STMFBIO_COLOURKEY_FLAGS_INVERT	0x00000002

typedef enum {
  STMFBIO_FF_OFF,
  STMFBIO_FF_SIMPLE,
  STMFBIO_FF_ADAPTIVE
} stmfbio_ff_state;


#define STMFBIO_VAR_CAPS_NONE           (0)
#define STMFBIO_VAR_CAPS_COLOURKEY      (1L<<1)
#define STMFBIO_VAR_CAPS_FLICKER_FILTER (1L<<2)
#define STMFBIO_VAR_CAPS_PREMULTIPLIED  (1L<<3)
#define STMFBIO_VAR_CAPS_OPACITY        (1L<<4)
#define STMFBIO_VAR_CAPS_GAIN           (1L<<5)
#define STMFBIO_VAR_CAPS_BRIGHTNESS     (1L<<6)
#define STMFBIO_VAR_CAPS_SATURATION     (1L<<7)
#define STMFBIO_VAR_CAPS_CONTRAST       (1L<<8)
#define STMFBIO_VAR_CAPS_TINT           (1L<<9)
#define STMFBIO_VAR_CAPS_ALPHA_RAMP     (1L<<10)
#define STMFBIO_VAR_CAPS_ZPOSITION      (1L<<11)
#define STBFBIO_VAR_CAPS_RESCALE_COLOUR_TO_VIDEO_RANGE (1L<<12)

enum stmfbio_output_id {
	STMFBIO_OUTPUTID_INVALID,

	STMFBIO_OUTPUTID_MAIN,
};


typedef enum _stmfbio_activate {
  STMFBIO_ACTIVATE_IMMEDIATE      = 0x00000000,
  STMFBIO_ACTIVATE_ON_NEXT_CHANGE = 0x00000001,
  STMFBIO_ACTIVATE_TEST           = 0x00000002,
  STMFBIO_ACTIVATE_MASK           = 0x0000000f,

  STMFBIO_ACTIVATE_VBL            = 0x00000010,
} stmfbio_activate;


enum stmfbio_output_standard {
	STMFBIO_STD_UNKNOWN,
	/* */
	STMFBIO_STD_PAL_BDGHI    = 0x4000000000000000LLU,
	STMFBIO_STD_SECAM        = 0x2000000000000000LLU,

	/* analogue standards are a mess - the following values
           coincidentially coincide with the v4l2 defines... */
	STMFBIO_STD_PAL_M        = 0x0000000000000100LLU,
	STMFBIO_STD_PAL_N        = 0x0000000000000200LLU,
	STMFBIO_STD_PAL_Nc       = 0x0000000000000400LLU,
	STMFBIO_STD_PAL_60       = 0x0000000000000800LLU,

	STMFBIO_STD_NTSC_M       = 0x0000000000001000LLU,
	STMFBIO_STD_NTSC_M_JP    = 0x0000000000002000LLU,
	STMFBIO_STD_NTSC_443     = 0x0000000000004000LLU,

	/* */
	STMFBIO_STD_VGA_60       = 0x0000000100000000LLU,
	STMFBIO_STD_VGA_59_94    = 0x0000000200000000LLU,
	STMFBIO_STD_480P_60      = 0x0000000400000000LLU,
	STMFBIO_STD_480P_59_94   = 0x0000000800000000LLU,
	STMFBIO_STD_576P_50      = 0x0000001000000000LLU,
	STMFBIO_STD_1080P_60     = 0x0000002000000000LLU,
	STMFBIO_STD_1080P_59_94  = 0x0000004000000000LLU,
	STMFBIO_STD_1080P_50     = 0x0000008000000000LLU,
	STMFBIO_STD_1080P_23_976 = 0x0000010000000000LLU,
	STMFBIO_STD_1080P_24     = 0x0000020000000000LLU,
	STMFBIO_STD_1080P_25     = 0x0000040000000000LLU,
	STMFBIO_STD_1080P_29_97  = 0x0000080000000000LLU,
	STMFBIO_STD_1080P_30     = 0x0000100000000000LLU,
	STMFBIO_STD_1080I_60     = 0x0000200000000000LLU,
	STMFBIO_STD_1080I_59_94  = 0x0000400000000000LLU,
	STMFBIO_STD_1080I_50     = 0x0000800000000000LLU,
	STMFBIO_STD_720P_60      = 0x0001000000000000LLU,
	STMFBIO_STD_720P_59_94   = 0x0002000000000000LLU,
	STMFBIO_STD_720P_50      = 0x0004000000000000LLU,

	STMFBIO_STD_QFHD3660     = 0x0010000000000000LLU,
	STMFBIO_STD_QFHD3650     = 0x0020000000000000LLU,
	STMFBIO_STD_WQFHD5660    = 0x0040000000000000LLU,
	STMFBIO_STD_WQFHD5650    = 0x0080000000000000LLU,
	STMFBIO_STD_QFHD5660     = 0x0100000000000000LLU,
	STMFBIO_STD_QFHD5650     = 0x0200000000000000LLU,
	STMFBIO_STD_QFHD1830     = 0x0400000000000000LLU,
	STMFBIO_STD_QFHD1825     = 0x0800000000000000LLU,

    STMFBIO_STD_XGA_60       = 0x1000000000000000LLU,
    STMFBIO_STD_XGA_75       = 0x2000000000000000LLU,
    STMFBIO_STD_XGA_85       = 0x4000000000000000LLU,
};


#define STMFBIO_STD_NTSC      (STMFBIO_STD_NTSC_M \
                               | STMFBIO_STD_NTSC_M_JP)

#define STMFBIO_STD_PAL       (STMFBIO_STD_PAL_BDGHI)

#define STMFBIO_STD_525_60    (STMFBIO_STD_PAL_M    \
                               | STMFBIO_STD_PAL_60 \
                               | STMFBIO_STD_NTSC   \
                               | STMFBIO_STD_NTSC_443)
#define STMFBIO_STD_625_50    (STMFBIO_STD_PAL      \
                               | STMFBIO_STD_PAL_N  \
                               | STMFBIO_STD_PAL_Nc \
                               | STMFBIO_STD_SECAM)

#define STMFBIO_STD_SMPTE293M (STMFBIO_STD_480P_60      \
                               | STMFBIO_STD_480P_59_94 \
                               | STMFBIO_STD_576P_50)

#define STMFBIO_STD_SMPTE274M (STMFBIO_STD_1080P_60       \
                               | STMFBIO_STD_1080P_59_94  \
                               | STMFBIO_STD_1080P_50     \
                               | STMFBIO_STD_1080P_23_976 \
                               | STMFBIO_STD_1080P_24     \
                               | STMFBIO_STD_1080P_25     \
                               | STMFBIO_STD_1080P_29_97  \
                               | STMFBIO_STD_1080P_30     \
                               | STMFBIO_STD_1080I_60     \
                               | STMFBIO_STD_1080I_59_94  \
                               | STMFBIO_STD_1080I_50)

#define STMFBIO_STD_SMPTE296M (STMFBIO_STD_720P_60      \
                               | STMFBIO_STD_720P_59_94 \
                               | STMFBIO_STD_720P_50)

#define STMFBIO_STD_SD        (STMFBIO_STD_525_60 \
                               | STMFBIO_STD_625_50)

#define STMFBIO_STD_ED        (STMFBIO_STD_SMPTE293M)

#define STMFBIO_STD_HD        (STMFBIO_STD_SMPTE274M \
                               | STMFBIO_STD_SMPTE296M)

#define STMFBIO_STD_VESA      (STMFBIO_STD_VGA_60 \
                               | STMFBIO_STD_VGA_59_94 \
                               | STMFBIO_STD_XGA_60 \
                               | STMFBIO_STD_XGA_75 \
                               | STMFBIO_STD_XGA_85)

#define STMFBIO_STD_CEA861    (STMFBIO_STD_525_60      \
                               | STMFBIO_STD_625_50    \
                               | STMFBIO_STD_VGA_60    \
                               | STMFBIO_STD_VGA_59_94 \
                               | STMFBIO_STD_SMPTE293M \
                               | STMFBIO_STD_SMPTE274M \
                               | STMFBIO_STD_SMPTE296M)

#define STMFBIO_STD_NTG5      (STMFBIO_STD_QFHD3660    \
                               | STMFBIO_STD_QFHD3650  \
                               | STMFBIO_STD_WQFHD5660 \
                               | STMFBIO_STD_WQFHD5650 \
                               | STMFBIO_STD_QFHD5660  \
                               | STMFBIO_STD_QFHD5650  \
                               | STMFBIO_STD_QFHD1830  \
                               | STMFBIO_STD_QFHD1825)

#define STMFBIO_STD_INTERLACED (STMFBIO_STD_525_60        \
                                | STMFBIO_STD_625_50      \
                                | STMFBIO_STD_1080I_60    \
                                | STMFBIO_STD_1080I_59_94 \
                                | STMFBIO_STD_1080I_50)

#define STMFBIO_STD_PROGRESSIVE (STMFBIO_STD_VGA_60         \
                                 | STMFBIO_STD_VGA_59_94    \
                                 | STMFBIO_STD_SMPTE293M    \
                                 | STMFBIO_STD_1080P_60     \
                                 | STMFBIO_STD_1080P_59_94  \
                                 | STMFBIO_STD_1080P_50     \
                                 | STMFBIO_STD_1080P_23_976 \
                                 | STMFBIO_STD_1080P_24     \
                                 | STMFBIO_STD_1080P_25     \
                                 | STMFBIO_STD_1080P_29_97  \
                                 | STMFBIO_STD_1080P_30     \
                                 | STMFBIO_STD_SMPTE296M    \
                                 | STMFBIO_STD_NTG5)

struct stmfbio_outputinfo
{
	enum stmfbio_output_id outputid;
	enum _stmfbio_activate activate;
	enum stmfbio_output_standard standard;
};

struct stmfbio_outputstandards
{
	enum stmfbio_output_id outputid;
	enum stmfbio_output_standard all_standards;
};

struct stmfbio_plane_dimension {
	__u32 w;
	__u32 h;
};

struct stmfbio_plane_rect {
	__u32 x;
	__u32 y;
	struct stmfbio_plane_dimension dim;
};

struct stmfbio_plane_config {
	unsigned long                  baseaddr;
	/* FIXME: the source API is incomplete! Should be updated to support a
           real source 'viewport' inside a source surface. */
	struct stmfbio_plane_dimension source;  /* resolution */
	struct stmfbio_plane_rect      dest;
	stm_pixel_format_t            format;
	__u32                          pitch; /* desired pitch */

	/* private */
	__u32 bitdepth; /* will be updated on successful return */
};

struct stmfbio_planeinfo
{
	__u32 layerid; /* must be 0 (for now) */
	enum _stmfbio_activate activate;
	struct stmfbio_plane_config config;
};

struct stmfbio_plane_pan
{
	__u32 layerid; /* must be 0 (for now) */
	enum _stmfbio_activate activate;
	unsigned long baseaddr;
};

struct stmfbio_var_screeninfo_ex
{
  /*
   * Display layer to operate on, 0 is always the layer showing the framebuffer.
   * No other layers have to be defined and the IOCTL will return ENODEV
   * if given an invalid layerid.
   */
  __u32 layerid;
  __u32 caps;                    /* Valid entries in structure               */
  __u32 failed;                  /* Indicates entries that failed during set */
  stmfbio_activate activate;

  /*
   * STMFBIO_VAR_CAPS_COLOURKEY
   */
  __u32 min_colour_key;
  __u32 max_colour_key;
  __u32 colourKeyFlags;

  stmfbio_ff_state ff_state;          /* STMFBIO_VAR_CAPS_FLICKER_FILTER      */

  __u8 premultiplied_alpha;           /* STMFBIO_VAR_CAPS_PREMULTIPLIED       */
  __u8 rescale_colour_to_video_range; /* STBFBIO_VAR_CAPS_RESCALE_COLOUR_...  */

  __u8 opacity;                       /* STMFBIO_VAR_CAPS_OPACITY             */
  __u8 gain;                          /* STMFBIO_VAR_CAPS_GAIN                */
  __u8 brightness;                    /* STMFBIO_VAR_CAPS_BRIGHTNESS          */
  __u8 saturation;                    /* STMFBIO_VAR_CAPS_SATURATION          */
  __u8 contrast;                      /* STMFBIO_VAR_CAPS_CONTRAST            */
  __u8 tint;                          /* STMFBIO_VAR_CAPS_HUE                 */
  __u8 alpha_ramp[2];                 /* STMFBIO_CAR_CAPS_ALPHA_RAMP          */

  __u32 z_position;                   /* STMFBIO_VAR_CAPS_ZPOSITION           */
};

struct stmfbio_vps
{
  __u8 vps_enable;
  __u8 vps_data[6];
};

/*
 * Note: The standards are a bitfield in order to match the internal driver
 *       representation. Do not modify!
 */
#define STMFBIO_OUTPUT_STD_PAL_BDGHI       (1L<<0)
#define STMFBIO_OUTPUT_STD_PAL_M           (1L<<1)
#define STMFBIO_OUTPUT_STD_PAL_N           (1L<<2)
#define STMFBIO_OUTPUT_STD_PAL_Nc          (1L<<3)
#define STMFBIO_OUTPUT_STD_NTSC_M          (1L<<4)
#define STMFBIO_OUTPUT_STD_NTSC_J          (1L<<5)
#define STMFBIO_OUTPUT_STD_NTSC_443        (1L<<6)
#define STMFBIO_OUTPUT_STD_SECAM           (1L<<7)
#define STMFBIO_OUTPUT_STD_PAL_60          (1L<<8)

#define STMFBIO_OUTPUT_ANALOGUE_RGB        (1L<<0)
#define STMFBIO_OUTPUT_ANALOGUE_YPrPb      (1L<<1)
#define STMFBIO_OUTPUT_ANALOGUE_YC         (1L<<2)
#define STMFBIO_OUTPUT_ANALOGUE_CVBS       (1L<<3)
#define STMFBIO_OUTPUT_ANALOGUE_MASK       (0xf)

#define STMFBIO_OUTPUT_ANALOGUE_CLIP_VIDEORANGE (0)
#define STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE  (1L<<8)

#define STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_AUTO (0)
#define STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_601  (1L<<9)
#define STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_709  (2L<<9)
#define STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_MASK (3L<<9)


#define STMFBIO_OUTPUT_DVO_ENABLED            (0)
#define STMFBIO_OUTPUT_DVO_DISABLED           (1L<<0)
#define STMFBIO_OUTPUT_DVO_YUV_444_16BIT      (0)
#define STMFBIO_OUTPUT_DVO_YUV_444_24BIT      (1L<<1)
#define STMFBIO_OUTPUT_DVO_YUV_422_16BIT      (2L<<1)
#define STMFBIO_OUTPUT_DVO_ITUR656            (3L<<1)
#define STMFBIO_OUTPUT_DVO_RGB_24BIT          (4L<<1)
#define STMFBIO_OUTPUT_DVO_MODE_MASK          (7L<<1)

#define STMFBIO_OUTPUT_DVO_CLIP_VIDEORANGE        (0)
#define STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE         (1L<<8)
#define STMFBIO_OUTPUT_DVO_CHROMA_FILTER_DISABLED (0)
#define STMFBIO_OUTPUT_DVO_CHROMA_FILTER_ENABLED  (1L<<9)

#define STMFBIO_OUTPUT_HDMI_ENABLED           (0)
#define STMFBIO_OUTPUT_HDMI_DISABLED          (1L<<0)
#define STMFBIO_OUTPUT_HDMI_RGB               (0)
#define STMFBIO_OUTPUT_HDMI_YUV               (1L<<1)
#define STMFBIO_OUTPUT_HDMI_444               (0)
#define STMFBIO_OUTPUT_HDMI_422               (1L<<2)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_SHIFT (3)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_24BIT (0)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_30BIT (1L<<STMFBIO_OUTPUT_HDMI_COLOURDEPTH_SHIFT)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_36BIT (2L<<STMFBIO_OUTPUT_HDMI_COLOURDEPTH_SHIFT)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_48BIT (3L<<STMFBIO_OUTPUT_HDMI_COLOURDEPTH_SHIFT)
#define STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK  (3L<<STMFBIO_OUTPUT_HDMI_COLOURDEPTH_SHIFT)

#define STMFBIO_OUTPUT_HDMI_CLIP_VIDEORANGE   (0)
#define STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE    (1L<<8)

#define STMFBIO_OUTPUT_CAPS_NONE             (0)
#define STMFBIO_OUTPUT_CAPS_SDTV_ENCODING    (1L<<0)
#define STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG  (1L<<1)
#define STMFBIO_OUTPUT_CAPS_DVO_CONFIG       (1L<<2)
#define STMFBIO_OUTPUT_CAPS_HDMI_CONFIG      (1L<<3)
#define STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND (1L<<4)
#define STMFBIO_OUTPUT_CAPS_BRIGHTNESS       (1L<<5)
#define STMFBIO_OUTPUT_CAPS_SATURATION       (1L<<6)
#define STMFBIO_OUTPUT_CAPS_CONTRAST         (1L<<7)
#define STMFBIO_OUTPUT_CAPS_HUE              (1L<<8)
#define STMFBIO_OUTPUT_CAPS_PSI_MASK         (STMFBIO_OUTPUT_CAPS_BRIGHTNESS | \
                                              STMFBIO_OUTPUT_CAPS_SATURATION | \
                                              STMFBIO_OUTPUT_CAPS_CONTRAST   | \
                                              STMFBIO_OUTPUT_CAPS_HUE)

struct stmfbio_output_configuration
{
  enum stmfbio_output_id outputid;
  __u32 caps;
  __u32 failed;
  stmfbio_activate activate;

  __u32 sdtv_encoding;
  __u32 analogue_config;
  __u32 dvo_config;
  __u32 hdmi_config;
  __u32 mixer_background;
  __u8  brightness;
  __u8  saturation;
  __u8  contrast;
  __u8  hue;

};


/*
 * Picture configuration allows the application to set information used to
 * construct Line21/23 WSS information in NTSC/PAL analogue output and to
 * construct the AVI InfoFrame in HDMI output where available. For a particular
 * display pipeline, accessed by its associated framebuffer IOCTLs, changing
 * the picture information will effect both analogue and HDMI outputs if they
 * are enabled. Note that VBI information is not supported on YPrPb component
 * analogue output in ED or HD display modes, only in SD (PAL/NTSC).
 *
 * The definitions below match values defined by internal types in
 * <include/stmmetadata.h> and should not be changed.
 */

/*
 * The picture aspect ratio indicates the intended aspect ratio
 * of the full content of a 720x480 or 720x576 picture being
 * displayed on an SD/ED display, i.e. it is indicating the pixel aspect ratio
 * of the image. It does not necessarily indicate (but in all probability is the
 * same as) the aspect ratio of the display.
 *
 * HD pictures always have a pixel aspect ratio of 1:1, hence the picture
 * aspect ratio should always match the display size ratio i.e. 16:9.
 */
#define STMFBIO_PIC_PICTURE_ASPECT_UNKNOWN    (0)
#define STMFBIO_PIC_PICTURE_ASPECT_4_3        (1)
#define STMFBIO_PIC_PICTURE_ASPECT_16_9       (2)

/*
 * The video aspect indicates the actual aspect ratio of video contained in
 * the picture on the display.
 */
#define STMFBIO_PIC_VIDEO_ASPECT_UNKNOWN      (0)
#define STMFBIO_PIC_VIDEO_ASPECT_4_3          (1)
#define STMFBIO_PIC_VIDEO_ASPECT_16_9         (2)
#define STMFBIO_PIC_VIDEO_ASPECT_14_9         (3)
#define STMFBIO_PIC_VIDEO_ASPECT_GT_16_9      (4)

/*
 * When the picture aspect ratio and video aspect ratio do not match then
 * the letterbox style indicates how the video is positioned inside the picture.
 * See CEA-861 Annex H, or standards for analogue video VBI signals
 * for details.
 */
#define STMFBIO_PIC_LETTERBOX_NONE            (0)
#define STMFBIO_PIC_LETTERBOX_CENTER          (1)
#define STMFBIO_PIC_LETTERBOX_TOP             (2)
#define STMFBIO_PIC_LETTERBOX_SAP_14_9        (3)
#define STMFBIO_PIC_LETTERBOX_SAP_4_3         (4)

#define STMFBIO_PIC_RESCALE_NONE              (0)
#define STMFBIO_PIC_RESCALE_HORIZONTAL        (1)
#define STMFBIO_PIC_RESCALE_VERTICAL          (2)
#define STMFBIO_PIC_RESCALE_BOTH              (3)

#define STMFBIO_PIC_BAR_DISABLE               (0)
#define STMFBIO_PIC_BAR_ENABLE                (1)

#define STMFBIO_PICTURE_FLAGS_PICUTRE_ASPECT   (1L<<0)
#define STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT     (1L<<1)
#define STMFBIO_PICTURE_FLAGS_LETTERBOX        (1L<<2)
#define STMFBIO_PICTURE_FLAGS_RESCALE_INFO     (1L<<3)
#define STMFBIO_PICTURE_FLAGS_BAR_INFO         (1L<<4)
#define STMFBIO_PICTURE_FLAGS_BAR_INFO_ENABLE  (1L<<5)

struct stmfbio_picture_configuration
{
  /* Which fields in the structure should be changed */
  __u32 flags;

  /*
   *  {0,0} = immediate, otherwise this should be a time in the
   *  future based on clock_gettime(CLOCK_MONOTONIC,...), not gettimeofday()
   */
  struct timeval timestamp;

  __u16  picture_aspect;
  __u16  video_aspect;
  __u16  letterbox_style;

  /* Indicate if the displayed image (usually video) has been rescaled */
  __u16  picture_rescale;

  /*
   * Black bar information, in pixels/lines from the start of the active video
   * area. The enable allows bar information to be switched on/off without
   * changing the actual bar data.
   */
  __u16 bar_enable;
  __u16 top_bar_end;
  __u16 bottom_bar_start;
  __u16 left_bar_end;
  __u16 right_bar_start;
};


struct stmfbio_auxmem2
{
  __u32 index;
  __u32 physical;
  __u32 size;
};


typedef enum stmfbio_3d_mode
{
  /* No 3D i.e. 2D */
  STMFBIO_3D_NONE,
  /* Sub-sampled 3D formats in a normal 2D frame */
  STMFBIO_3D_SBS_HALF,
  STMFBIO_3D_TOP_BOTTOM,
  /* Full frame 3D formats, double clocked */
  STMFBIO_3D_FRAME_PACKED,
  STMFBIO_3D_FIELD_ALTERNATIVE,
  /* TV panel specific 3D modes */
  STMFBIO_3D_FRAME_SEQUENTIAL,
  STMFBIO_3D_LL_RR,
  STMFBIO_LINE_ALTERNATIVE
} stmfbio_3d_mode;


enum stmfbio_3d_framebuffer_type
{
  STMFBIO_3D_FB_MONO,    /* Single 2D framebuffer (or packed 3D SbS Half/TopBottom) */
  STMFBIO_3D_FB_STEREO   /* Stereo 3D framebuffer for framepacked/sequential style modes  */
};


struct stmfbio_3d_configuration
{
  enum stmfbio_output_id           outputid;
  enum _stmfbio_activate           activate;
  enum stmfbio_3d_mode             mode;
  enum stmfbio_3d_framebuffer_type framebuffer_type;
  __s8                             framebuffer_depth;
};


/*
 * non-standard ioctls to control the FB plane, although
 * these can be used directly they are really provided for the DirectFB
 * driver
 */
#define STMFBIO_GET_OUTPUTSTANDARDS     _IOWR('B', 0x10, struct stmfbio_outputstandards)
#define STMFBIO_GET_OUTPUTINFO          _IOWR('B', 0x11, struct stmfbio_outputinfo)
#define STMFBIO_SET_OUTPUTINFO          _IOWR('B', 0x12, struct stmfbio_outputinfo)
#define STMFBIO_GET_PLANEMODE           _IOWR('B', 0x11, struct stmfbio_planeinfo)
#define STMFBIO_SET_PLANEMODE           _IOW ('B', 0x12, struct stmfbio_planeinfo)
#define STMFBIO_PAN_PLANE               _IOW ('B', 0x10, struct stmfbio_planeinfo)

#define STMFBIO_GET_VAR_SCREENINFO_EX   _IOR ('B', 0x12, struct stmfbio_var_screeninfo_ex)
#define STMFBIO_SET_VAR_SCREENINFO_EX   _IOWR('B', 0x13, struct stmfbio_var_screeninfo_ex)
#define STMFBIO_GET_OUTPUT_CONFIG       _IOR ('B', 0x14, struct stmfbio_output_configuration)
#define STMFBIO_SET_OUTPUT_CONFIG       _IOWR('B', 0x15, struct stmfbio_output_configuration)
#define STMFBIO_GET_3D_CONFIG           _IOWR('B', 0x20, struct stmfbio_3d_configuration)
#define STMFBIO_SET_3D_CONFIG           _IOW ('B', 0x21, struct stmfbio_3d_configuration)

#define STMFBIO_SET_PICTURE_CONFIG      _IOWR('B', 0x16, struct stmfbio_picture_configuration)
#define STMFBIO_GET_AUXMEMORY2          _IOWR('B', 0x17, struct stmfbio_auxmem2)

#define STMFBIO_SET_DAC_HD_POWER        _IO  ('B', 0xd)
#define STMFBIO_SET_CGMS_ANALOG         _IO  ('B', 0xe)
#define STMFBIO_SET_DAC_HD_FILTER       _IO  ('B', 0xf)
#define STMFBIO_SET_VPS_ANALOG          _IOW ('B', 0x1f, struct stmfbio_vps)

/*
 * Implement the matrox FB extension for VSync synchronisation, again for
 * DirectFB.
 */
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC       _IOW('F', 0x20, u_int32_t)
#endif

/*
 * Implement panel configuration
 */

#define PANEL_IO

#ifdef PANEL_IO
#define STMFBIO_SET_PANEL_CONFIG        _IOWR('B', 0x18, struct stmfbio_panel_config)

typedef enum 
{
  STMFB_PANEL_DITHER_METHOD_NONE,
  STMFB_PANEL_DITHER_METHOD_TRUNC,
  STMFB_PANEL_DITHER_METHOD_ROUND,
  STMFB_PANEL_DITHER_METHOD_RANDOM,
  STMFB_PANEL_DITHER_METHOD_SPATIAL,
}stmfbio_display_panel_dither_mode;

typedef enum
{
  STMFB_PANEL_FREERUN,
  STMFB_PANEL_FIXED_FRAME_LOCK,
  STMFB_PANEL_DYNAMIC_FRAME_LOCK,
  STMFB_PANEL_LOCK_LOAD,
}stmfbio_display_panel_lock_method;

typedef enum stmfbio_display_panel_lvds_bus_width_select_e
{
  STMFBIO_PANEL_LVDS_SINGLE,
  STMFBIO_PANEL_LVDS_DUAL,
  STMFBIO_PANEL_LVDS_QUAD,
} stmfbio_display_panel_lvds_bus_width_select_t;


typedef enum stmfbio_display_panel_lvds_bus_swap_e
{
  STMFBIO_PANEL_LVDS_SWAP_AB    = 1,
  STMFBIO_PANEL_LVDS_SWAP_CD    = 2,
  STMFBIO_PANEL_LVDS_SWAP_AB_CD = 3
} stmfbio_display_panel_lvds_bus_swap_t;


typedef enum stmfbio_display_panel_lvds_pair_swap_e
{
  STMFBIO_PANEL_LVDS_SWAP_PAIR0_PN = (1L<<0),
  STMFBIO_PANEL_LVDS_SWAP_PAIR1_PN = (1L<<1),
  STMFBIO_PANEL_LVDS_SWAP_PAIR2_PN = (1L<<2),
  STMFBIO_PANEL_LVDS_SWAP_PAIR3_PN = (1L<<3),
  STMFBIO_PANEL_LVDS_SWAP_PAIR4_PN = (1L<<4),
  STMFBIO_PANEL_LVDS_SWAP_PAIR5_PN = (1L<<5),
  STMFBIO_PANEL_LVDS_SWAP_CLOCK_PN = (1L<<6),
} stmfbio_display_panel_lvds_pair_swap_t;


typedef struct stmfbio_display_panel_lvds_ch_swap_config_s
{
  stmfbio_display_panel_lvds_bus_swap_t  swap_options;
  stmfbio_display_panel_lvds_pair_swap_t pair_swap_ch_a;
  stmfbio_display_panel_lvds_pair_swap_t pair_swap_ch_b;
  stmfbio_display_panel_lvds_pair_swap_t pair_swap_ch_c;
  stmfbio_display_panel_lvds_pair_swap_t pair_swap_ch_d;
}stmfbio_display_panel_lvds_ch_swap_config_t;


typedef struct stmfbio_display_panel_lvds_signal_pol_config_s
{
  __u8 hs_pol;  /*!< hsync polarity              */
  __u8 vs_pol;  /*!< vsync polarity              */
  __u8 de_pol;  /*!< data enable polarity        */
  __u8 odd_pol; /*!< odd not even field polarity */
} stmfbio_display_panel_lvds_signal_pol_config_t;


typedef enum stmfbio_display_panel_lvds_map_e
{
  STMFBIO_PANEL_LVDS_MAP_STANDARD1,
  STMFBIO_PANEL_LVDS_MAP_STANDARD2,
  STMFBIO_PANEL_LVDS_MAP_CUSTOM
} stmfbio_display_panel_lvds_map_t;


typedef enum stmfbio_display_lvds_common_mode_voltage_e
{
  STMFBIO_PANEL_LVDS_1_25V, /*!< 1.25v */
  STMFBIO_PANEL_LVDS_1_10V, /*!< 1.10v */
  STMFBIO_PANEL_LVDS_1_35V  /*!< 1.35v */
}stmfbio_display_lvds_common_mode_voltage_t;


typedef enum stmfbio_display_lvds_bias_current_e
{
  STMFBIO_PANEL_LVDS_0_5_MA, /*!< 0.5 mA */
  STMFBIO_PANEL_LVDS_1_0_MA, /*!< 1.0 mA */
  STMFBIO_PANEL_LVDS_1_5_MA, /*!< 1.5 mA */
  STMFBIO_PANEL_LVDS_2_0_MA, /*!< 2.0 mA */
  STMFBIO_PANEL_LVDS_2_5_MA, /*!< 2.5 mA */
  STMFBIO_PANEL_LVDS_3_0_MA, /*!< 3.0 mA */
  STMFBIO_PANEL_LVDS_3_5_MA, /*!< 3.5 mA */
  STMFBIO_PANEL_LVDS_4_0_MA  /*!< 4.0 mA */
}stmfbio_display_lvds_bias_current_t;


typedef struct stmfbio_display_panel_lvds_bias_control_s
{
  stmfbio_display_lvds_common_mode_voltage_t  common_mode_voltage;
  stmfbio_display_lvds_bias_current_t         bias_current;
} stmfbio_display_panel_lvds_bias_control_t;


typedef struct stmfbio_display_panel_lvds_config_s
{
  stmfbio_display_panel_lvds_bus_width_select_t   lvds_bus_width_sel;
  stmfbio_display_panel_lvds_ch_swap_config_t     swap_options;
  stmfbio_display_panel_lvds_signal_pol_config_t  signal_polarity;
  stmfbio_display_panel_lvds_map_t                lvds_signal_map;
  __u32                                    custom_lvds_map[8];
  __u16                                    clock_data_adj;
  stmfbio_display_panel_lvds_bias_control_t       bias_control_value;
} stmfbio_display_panel_lvds_config_t;

struct stmfbio_panel_config
{
  __u32 lookup_table1_p;
  __u32 lookup_table2_p;
  __u32 linear_color_remap_table_p;
  __u32 vertical_refresh_rate;              /*!< Vertical refresh rate in mHz */
  __u32 active_area_width;       /*!< */
  __u32 active_area_height;      /*!< */
  __u32 active_area_start_pixel; /*!< Pixels are counted from 0 */
  __u32 active_area_start_line;  /*!< Lines are counted from 1, starting
                                             with the first vsync blanking line */
  __u32 pixels_per_line;    /*!< Total number of pixels per line
                                             including the blanking area */
  __u32 lines_per_frame;    /*!< Total number of lines per progressive
                                             frame or pair of interlaced fields
                                             including the blanking area */
  __u32 pixel_clock_freq;   /*!< In Hz, e.g. 27000000 (27MHz) */
  __u32 hsync_width;        /*!< */
  __u32 vsync_width;        /*!< */
  stmfbio_display_panel_dither_mode dither;
  stmfbio_display_panel_lock_method lock_method;
  __u16 pwr_to_de_delay_during_power_on;
  __u16 de_to_bklt_on_delay_during_power_on;
  __u16 bklt_to_de_off_delay_during_power_off;
  __u16 de_to_pwr_delay_during_power_off;
  __u8 enable_lut1;
  __u8 enable_lut2;
  __u8 afr_enable;
  __u8 is_half_display_clock;
  __u8 hsync_polarity;     /*!< 0: Negative 1: Positive */
  __u8 vsync_polarity;     /*!< 0: Negative 1: Positive */
//  stmfbio_display_panel_lvds_config_t lvds_config;
};
#endif

#endif /* _STMFB_H */
