/***********************************************************************
 *
 * File: include/stmdisplayplane.h
 * Copyright (c) 2000, 2004, 2005, 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_DISPLAY_PLANE_H
#define _STM_DISPLAY_PLANE_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum
{
  /*
   * ST hardware planes, the defines deliberately match the plane enable bit in
   * the hardware compositor mixer control.
   */
  OUTPUT_NONE   = 0,
  OUTPUT_BKG    = (1L<<0),
  OUTPUT_VID1   = (1L<<1),
  OUTPUT_VID2   = (1L<<2),
  OUTPUT_GDP1   = (1L<<3),
  OUTPUT_GDP2   = (1L<<4),
  OUTPUT_GDP3   = (1L<<5),
  OUTPUT_GDP4   = (1L<<6),
  OUTPUT_CUR    = (1L<<9),
  OUTPUT_VBI    = (1L<<13),  // A plane linked to a GDP for VBI waveforms
  OUTPUT_NULL   = (1L<<14),  // A Null plane for virtual framebuffer support
  OUTPUT_ALP    = (1L<<15),
  /*
   * Virtual planes, using a blit compositor to mix them together
   */
  OUTPUT_VIRT1  = (1L<<16),
  OUTPUT_VIRT2  = (1L<<17),
  OUTPUT_VIRT3  = (1L<<18),
  OUTPUT_VIRT4  = (1L<<19),
  OUTPUT_VIRT5  = (1L<<20),
  OUTPUT_VIRT6  = (1L<<21),
  OUTPUT_VIRT7  = (1L<<22),
  OUTPUT_VIRT8  = (1L<<23),
  OUTPUT_VIRT9  = (1L<<24),
  OUTPUT_VIRT10 = (1L<<25),
  OUTPUT_VIRT11 = (1L<<26),
  OUTPUT_VIRT12 = (1L<<27),
  OUTPUT_VIRT13 = (1L<<28),
  OUTPUT_VIRT14 = (1L<<29),
  OUTPUT_VIRT15 = (1L<<30)
} stm_plane_id_t;


typedef enum
{
  SURF_NULL_PAD,
  SURF_RGB565 ,
  SURF_RGB888 ,
  SURF_ARGB8565,
  SURF_ARGB8888,
  SURF_ARGB1555,
  SURF_ARGB4444,
  SURF_CRYCB888,
  SURF_YCBCR422R,
  SURF_YCBCR422MB,
  SURF_YCBCR420MB,
  SURF_ACRYCB8888,
  SURF_CLUT1,
  SURF_CLUT2,
  SURF_CLUT4,
  SURF_CLUT8,
  SURF_ACLUT44,
  SURF_ACLUT88,
  SURF_A1,
  SURF_A8,
  SURF_BGRA8888, /* Bigendian ARGB                                     */
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
  SURF_END
}SURF_FMT;


#define PLANE_CAPS_NONE                   (0)
#define PLANE_CAPS_RESIZE                 (1L<<0)
#define PLANE_CAPS_SRC_COLOR_KEY          (1L<<1)
#define PLANE_CAPS_DST_COLOR_KEY          (1L<<2)
#define PLANE_CAPS_GLOBAL_ALPHA           (1L<<3)
#define PLANE_CAPS_DEINTERLACE            (1L<<4)
#define PLANE_CAPS_SUBPIXEL_POSITION      (1L<<5)
#define PLANE_CAPS_FLICKER_FILTER         (1L<<6)
#define PLANE_CAPS_PREMULTIPLED_ALPHA     (1L<<7)
#define PLANE_CAPS_RESCALE_TO_VIDEO_RANGE (1L<<8)
#define PLANE_CAPS_VBI_POSITIONING        (1L<<9)
#define PLANE_CAPS_DIRECT_BUFFER_UPDATES  (1L<<10)

/*
 * Global status of the plane
 */
#define STM_PLANE_STATUS_CONNECTED        (1L<<0)
#define STM_PLANE_STATUS_LOCKED           (1L<<1)
#define STM_PLANE_STATUS_ACTIVE           (1L<<2)
#define STM_PLANE_STATUS_VISIBLE          (1L<<3)
#define STM_PLANE_STATUS_PAUSED           (1L<<4)
#define STM_PLANE_STATUS_QUEUE_FULL       (1L<<5)
#define STM_PLANE_STATUS_MASK             (0xffff)
/*
 * Buffer specific status, returned only in a buffer completed callback
 */
#define STM_PLANE_STATUS_BUF_HW_ERROR     (1L<<29)
#define STM_PLANE_STATUS_BUF_DISPLAYED    (1L<<30)
#define STM_PLANE_STATUS_BUF_CRC_VALID    (1L<<31)


#define PLANE_CTRL_CAPS_NONE            (0)
#define PLANE_CTRL_CAPS_PSI_CONTROLS    (1L<<0)
#define PLANE_CTRL_CAPS_GAIN            (1L<<1)
#define PLANE_CTRL_CAPS_ALPHA_RAMP      (1L<<2)
#define PLANE_CTRL_CAPS_IQI             (1L<<3)
#define PLANE_CTRL_CAPS_IQI_CE          (1L<<4)
#define PLANE_CTRL_CAPS_FILMGRAIN       (1L<<5)
#define PLANE_CTRL_CAPS_TNR             (1L<<6)
#define PLANE_CTRL_CAPS_SCREEN_XY       (1L<<7)
#define PLANE_CTRL_CAPS_DEINTERLACE     (1L<<8)
#define PLANE_CTRL_CAPS_BUFFER_ADDRESS  (1L<<9)
#define PLANE_CTRL_CAPS_FMD             (1L<<10)

enum PlaneCtrlIQIConfiguration {
  PCIQIC_FIRST,                 /* this is the default config used by the driver, i.e. IQI disabled */

  PCIQIC_BYPASS = PCIQIC_FIRST,

  PCIQIC_ST_DEFAULT,
  PCIQIC_ST_SOFT = PCIQIC_ST_DEFAULT,
  PCIQIC_ST_MEDIUM,
  PCIQIC_ST_STRONG,

  PCIQIC_COUNT
};

enum PlaneCtrlIQIEnables {
  PCIQIE_NONE                   = 0x00000000,
  PCIQIE_ENABLESINXCOMPENSATION = 0x00000001,
  PCIQIE_ENABLE_PEAKING         = 0x00000002,
  PCIQIE_ENABLE_LTI             = 0x00000004,
  PCIQIE_ENABLE_CTI             = 0x00000008,
  PCIQIE_ENABLE_LE              = 0x00000010,

  PCIQIE_ALL                    = 0x0000001f
};

enum PlaneCtrlxVPConfiguration {
  PCxVPC_FIRST,                 /* this is the default config used by the driver, i.e. xVP disabled */

  PCxVPC_BYPASS = PCxVPC_FIRST,
  PCxVPC_FILMGRAIN,
  PCxVPC_TNR,
  PCxVPC_TNR_BYPASS,
  PCxVPC_TNR_NLEOVER,           /* override the TNR's NLE */

  PCxVPC_COUNT
};

enum PlaneCtrlDEIConfiguration {
  PCDEIC_FIRST,                 /* this is the default config used by the driver, i.e. DEI-3D enabled */

  PCDEIC_3DMOTION = PCDEIC_FIRST,
  PCDEIC_DISABLED,
  PCDEIC_MEDIAN,

  PCDEIC_COUNT
};

enum StmColorKeyConfigFlags {
  SCKCF_NONE   = 0x00000000,

  SCKCF_ENABLE = 0x00000001,
  SCKCF_FORMAT = 0x00000002,
  SCKCF_R_INFO = 0x00000004,
  SCKCF_G_INFO = 0x00000008,
  SCKCF_B_INFO = 0x00000010,
  SCKCF_MINVAL = 0x00000020,
  SCKCF_MAXVAL = 0x00000040,

  SCKCF_ALL    = 0x0000007f
};

enum StmColorKeyConfigValueFormat {
  SCKCVF_RGB   = 0x00,
  SCKCVF_CrYCb = 0x01
};

enum StmColorKeyColorComponentMode {
  SCKCCM_DISABLED = 0x00,
  SCKCCM_ENABLED  = 0x01,
  SCKCCM_INVERSE  = (0x02 | SCKCCM_ENABLED)
};

typedef struct StmColorKeyConfig {
  enum StmColorKeyConfigFlags flags; /* specify which of the following
                                        fields are present */

  UCHAR enable;
  enum StmColorKeyConfigValueFormat format;

  enum StmColorKeyColorComponentMode r_info;
  enum StmColorKeyColorComponentMode g_info;
  enum StmColorKeyColorComponentMode b_info;

  ULONG minval;
  ULONG maxval;
} stm_color_key_config_t;

enum StmPlaneCtrlHideMode {
  SPCHM_MIXER_PULLSDATA, /* mixer will still pull data, just not blend it */
  SPCHM_MIXER_DISABLE    /* mixer will completely ignore the data */
};

typedef enum
{
  PLANE_CTRL_PSI_BRIGHTNESS = 1,/* 0-255 default 128 :PLANE_CTRL_CAPS_PSI_CONTROLS   */
  PLANE_CTRL_PSI_SATURATION,    /* 0-255 default 128 :PLANE_CTRL_CAPS_PSI_CONTROLS   */
  PLANE_CTRL_PSI_CONTRAST,      /* 0-255 default 128 :PLANE_CTRL_CAPS_PSI_CONTROLS   */
  PLANE_CTRL_PSI_TINT,          /* 0-255 default 128 :PLANE_CTRL_CAPS_PSI_CONTROLS   */
  PLANE_CTRL_GAIN,              /* 0-255 default 255 :PLANE_CTRL_CAPS_GAIN           */
  PLANE_CTRL_ALPHA_RAMP,        /* Format is 0xXXXXbbaa where:
                                 * XX = dont care,
                                 * aa = alpha0 range 0-255, default 0
                                 * bb = alpha1 range 0-255, default 255
                                 * The two alpha values are packed so they can
                                 * be updated on the hardware simultaneously to
                                 * avoid possible visual artifacts.
                                 */
  PLANE_CTRL_IQI_FIRST,
  PLANE_CTRL_IQI_CONFIG = PLANE_CTRL_IQI_FIRST,
                                /* enum PlaneCtrlIQIConfiguration */
  PLANE_CTRL_IQI_DEMO,          /* IQI demo on/off, default: off  */

  PLANE_CTRL_IQI_ENABLES,       /* enum PlaneCtrlIQIEnables */
  PLANE_CTRL_IQI_PEAKING,       /* IQI peaking */
  PLANE_CTRL_IQI_LTI,           /* IQI Luma Transient Improvement */
  PLANE_CTRL_IQI_CTI,           /* IQI Chroma Transient Imrpovement */
  PLANE_CTRL_IQI_LE,            /* IQI Luma Enhancer */
  PLANE_CTRL_IQI_CE,            /* IQI Chrominance Enhancer */
  PLANE_CTRL_IQI_LAST = PLANE_CTRL_IQI_CE,

  PLANE_CTRL_SCREEN_XY,         /* "Immediate" screen position of plane (for cursors) */
                                /* format is (signed short)y<<16 | (signed short)x    */

  PLANE_CTRL_XVP_FIRST,
  PLANE_CTRL_XVP_CONFIG = PLANE_CTRL_XVP_FIRST,
                                  /* enum PlaneCtrlxVPConfiguration */
  PLANE_CTRL_XVP_TNRNLE_OVERRIDE, /* 0 ... 255 */
  PLANE_CTRL_XVP_TNR_TOPBOT,      /*  */
  PLANE_CTRL_XVP_LAST = PLANE_CTRL_XVP_TNR_TOPBOT,

  PLANE_CTRL_DEI_FIRST,
  PLANE_CTRL_DEI_FMD_ENABLE = PLANE_CTRL_DEI_FIRST, /* 1: enable, 0: disable */
  PLANE_CTRL_DEI_CTRLREG,
  PLANE_CTRL_DEI_MODE,          /* enum PlaneCtrlDEIConfiguration */
  PLANE_CTRL_DEI_FMD_THRESHS,
  PLANE_CTRL_DEI_LAST = PLANE_CTRL_DEI_FMD_THRESHS,

  PLANE_CTRL_COLOR_KEY,         /* pointer to struct StmfbColorKeyConfig
                                   'immediate' activation, unless a queued
                                   buffer has something set, in which case it
                                   overrides this configuration. */
  PLANE_CTRL_BUFFER_ADDRESS,    /* physical buffer address when buffers are
                                   marked with STM_PLANE_SRC_DIRECT_BUFFER_ADDR */

  PLANE_CTRL_HIDE_MODE,		/* when plane hidden should the mixer still pull the data?
                                   enum StmPlaneCtrlHideMode only valid for
                                   video planes with DEI */

} stm_plane_ctrl_t;


typedef struct
{
  ULONG ulCaps;      // Main capabilities, or'd PLANE_CAPS_xxxx
  ULONG ulControls;  // Supported controls, or'd PLANE_CTRL_CAPS_xxxx
  ULONG ulMinFields; // Minimum number of fields/frames that a plane can be displayed for
  ULONG ulMinWidth;
  ULONG ulMinHeight;
  ULONG ulMaxWidth;
  ULONG ulMaxHeight;

  /*
   * The following bound the scaling capabilities of the plane, however this
   * does not reflect any limitations due to memory bandwidth.
   */
  stm_rational_t minHorizontalRescale;
  stm_rational_t maxHorizontalRescale;
  stm_rational_t minVerticalRescale;
  stm_rational_t maxVerticalRescale;

} stm_plane_caps_t;


typedef struct
{
  long  x, y;
  long  width, height;
} stm_plane_crop_t;


#define STM_PLANE_SRC_INTERLACED          (1L<<0)  /* SRC content is interleaved interlaced fields.             */
#define STM_PLANE_SRC_BOTTOM_FIELD_FIRST  (1L<<1)  /* Display the bottom field of interlaced content            */
                                                   /* before the top field,the default is top field first.      */
#define STM_PLANE_SRC_REPEAT_FIRST_FIELD  (1L<<2)  /* Repeat the first field on interlaced content, i.e. the    */
                                                   /* buffer is on display for at least 3 fields.               */
#define STM_PLANE_SRC_TOP_FIELD_ONLY      (1L<<3)  /* Display the top field only.                               */
#define STM_PLANE_SRC_BOTTOM_FIELD_ONLY   (1L<<4)  /* Display the bottom field only.                            */
#define STM_PLANE_SRC_INTERPOLATE_FIELDS  (1L<<5)  /* Interpolate fields of interlaced content for trick mode   */
                                                   /* display of a buffer for multiple display frames.          */
#define STM_PLANE_SRC_COLORSPACE_709      (1L<<6)  /* Src content is in ITU-R BT.709 colorspace,                */
                                                   /* the default is ITU-R BT.601. This is only relevent to YUV */
                                                   /* SRC pixel formats.                                        */
#define STM_PLANE_SRC_PREMULTIPLIED_ALPHA (1L<<7)  /* The source colour has been premultiplied by the per pixel */
                                                   /* alpha channel, only relevent with ARGB formats.           */
#define STM_PLANE_SRC_LIMITED_RANGE_ALPHA (1L<<8)  /* The alpha channel is in the range 0-128 not 0-255         */
#define STM_PLANE_SRC_CONST_ALPHA         (1L<<10) /* Enable constant alpha.                                    */
#define STM_PLANE_SRC_XY_IN_16THS         (1L<<11) /* x,y crop coordinates are specified in 16ths of pixel/line */
#define STM_PLANE_SRC_XY_IN_32NDS         (1L<<12) /* x,y crop coordinates are specified in 32nds of pixel/line */

#define STM_PLANE_SRC_VC1_POSTPROCESS_LUMA   (1L<<13) /* Enable VC1 luma post processing, type set in ulPostProcessLumaType   */
#define STM_PLANE_SRC_VC1_POSTPROCESS_CHROMA (1L<<14) /* Enable VC1 chroma post processing, type set in ulPostProcessChromaType */

typedef struct
{
  ULONG     ulVideoBufferAddr;       /* Hardware bus address of video buffer     */
  ULONG     ulVideoBufferSize;       /* Size of video buffer                     */
  ULONG     chromaBufferOffset;      /* Start of chroma in non interleaved YUV   */
  SURF_FMT  ulColorFmt;              /* Pixel format                             */
  ULONG     ulPixelDepth;            /* In bits per pixel                        */
  ULONG     ulStride;                /* Length of a raster or luma line in bytes */

  ULONG     ulTotalLines;            /* The maximum number of video lines in     */
                                     /* this buffer, ulTotalLines*ulStride is    */
                                     /* the number of bytes in the buffer for    */
                                     /* raster formats and the size of the luma  */
                                     /* part of the buffer for MB and Planar     */
                                     /* formats.                                 */

  stm_plane_crop_t Rect;             /* Source rectangle to display              */
  stm_rational_t   PixelAspectRatio; /* Source pixel aspect ratio                */
  ULONG     ulLinearCenterPercentage;/* Region to preserve source aspect ratio   */
                                     /* when converting to 16:9 display output.  */
  ULONG     ulConstAlpha;            /* Range 0-255                              */

  stm_color_key_config_t ColorKey;   /* color key configuration               */

  ULONG     ulClutBusAddress;        /* The hardware bus address of the CLUT     */
                                     /* associated with this image. This should  */
                                     /* contain 256 entries in packed 32bit      */
                                     /* ARGB colour format, but where the alpha  */
                                     /* range is limited to 0-128 (note not 127!)*/

  ULONG     ulPostProcessLumaType;   /* VC1 range mapping R, or range reduction define */
  ULONG     ulPostProcessChromaType; /* VC1 range mapping R, or range reduction define */
#define STM_PLANE_SRC_VC1_RANGE_MAP_MIN   0x0
#define STM_PLANE_SRC_VC1_RANGE_MAP_MAX   0x7
#define STM_PLANE_SRC_VC1_RANGE_REDUCTION 0x7

  ULONG     ulFlags;                 /* STM_PLANE_SRC_INTERLACED etc. */
} stm_buffer_src_t;


#define STM_PLANE_DST_COLOR_KEY               (1L<<0) /* Enable dest color keying                            */
#define STM_PLANE_DST_COLOR_KEY_INV           (1L<<1) /* Dst colour key matches if outside of specifed range */
#define STM_PLANE_DST_FLICKER_FILTER          (1L<<2) /* Enable flicker filtering if available               */
#define STM_PLANE_DST_ADAPTIVE_FF             (1L<<3) /* Enable adaptive flicker filtering if available      */
#define STM_PLANE_DST_RESCALE_TO_VIDEO_RANGE  (1L<<4) /* Rescale the full digital RGB range to the typical   */
                                                      /* video output range 16-235 (8bit values)             */
#define STM_PLANE_DST_OVERWITE                (1L<<5) /* Do not blend the src with the destination           */
                                                      /* overwrite the destination instead. This is only,    */
                                                      /* an option on outputs employing a blitter composition*/
                                                      /* based mixer.                                        */
#define STM_PLANE_DST_USE_SCREEN_XY           (1L<<6) /* For cursors, ignore dst.Rect.[x|y]                  */
#define STM_PLANE_DST_CONVERT_TO_16_9_DISPLAY (1L<<7) /* Use a non-linear zoom to convert from source pixel  */
                                                      /* aspect ratio to the destination pixel aspect ratio  */
                                                      /* assuming it is a 16:9 display.                      */

typedef struct
{
  stm_plane_crop_t  Rect;        /* Destination rectangle on screen */

  ULONG      ulFlags;            /* STM_PLANE_DST_COLOR_KEY etc. */

  ULONG      ulColorKeyMin;      /* RGB format                      */
  ULONG      ulColorKeyMax;      /* RGB format                      */

} stm_buffer_dst_t;


typedef struct
{
  TIME64 vsyncTime;
  ULONG  ulStatus;               /* Plane status flags at this timestamp: STM_PLANE_STATUS_BUF_HW_ERROR etc. */
  ULONG  ulYCRC[2];              /* only valid on main display pipeline  */
  ULONG  ulUVCRC[2];             /* and when DEI is in bypass mode!      */
} stm_buffer_presentation_stats_t;


#define STM_PLANE_PRESENTATION_PERSISTENT         (1L<<0)
#define STM_PLANE_PRESENTATION_GRAPHICS           (1L<<1)
#define STM_PLANE_PRESENTATION_DIRECT_BUFFER_ADDR (1L<<2)  /* Use a direct base address for the buffer data, ignoring ulVideoBufferAddr */

typedef struct
{
  TIME64    presentationTime;                    /* Required presentation time, relative to the CPU wall clock    */
  int       nfields;                             /* The number of fields/frames the buffer will be on screen for  */
  ULONG     ulFlags;                             /* STM_PLANE_PRESENTATION_PERSISTENT etc. */
  void    (*pDisplayCallback)  (void*, TIME64);  /* Callback on first field/frame buffer is displayed             */
  void    (*pCompletedCallback)(void*, const stm_buffer_presentation_stats_t *);
                                                 /* Callback when buffer is removed from display                  */
  void     *pUserData;                           /* User defined value passed into both callback routines         */
  stm_buffer_presentation_stats_t stats;
} stm_buffer_presentation_t;


typedef struct
{
  stm_buffer_src_t          src;
  stm_buffer_dst_t          dst;
  stm_buffer_presentation_t info;
} stm_display_buffer_t;


struct stm_display_output_s;
typedef struct stm_display_plane_s stm_display_plane_t;

typedef struct
{
  int (*ConnectToOutput)     (stm_display_plane_t *, struct stm_display_output_s *);
  int (*DisconnectFromOutput)(stm_display_plane_t *, struct stm_display_output_s *);

  int (*Lock)                (stm_display_plane_t *);
  int (*Unlock)              (stm_display_plane_t *);

  int (*QueueBuffer)         (stm_display_plane_t *, stm_display_buffer_t *);

  int (*Pause)               (stm_display_plane_t *, int bFlushQueue);
  int (*Resume)              (stm_display_plane_t *);
  int (*Flush)               (stm_display_plane_t *);

  int (*Hide)                (stm_display_plane_t *);
  int (*Show)                (stm_display_plane_t *);

  int (*GetCapabilities)     (stm_display_plane_t *, stm_plane_caps_t *);
  int (*GetImageFormats)     (stm_display_plane_t *, const SURF_FMT **);

  int (*SetControl)          (stm_display_plane_t *, stm_plane_ctrl_t, ULONG);
  int (*GetControl)          (stm_display_plane_t *, stm_plane_ctrl_t, ULONG *);

  int (*SetDepth)            (stm_display_plane_t *, struct stm_display_output_s *, int depth, int activate);
  int (*GetDepth)            (stm_display_plane_t *, struct stm_display_output_s *, int *depth);

  void (*GetStatus)          (stm_display_plane_t *, ULONG *);

  int (*Release)             (stm_display_plane_t *);

} stm_display_plane_ops_t;


struct stm_display_plane_s
{
  ULONG handle;
  ULONG lock;

  struct stm_display_device_s   *owner;
  const stm_display_plane_ops_t *ops;
};


/*****************************************************************************
 * C interface to display planes
 *
 */

/*
 * int stm_display_plane_connect_to_output(stm_display_plane_t *p,
 *                                         struct stm_display_output_s *o)
 *
 * Connect the plane to the given output. If successful buffers queued on
 * the plane will be processed and displayed on display being generated by
 * the output.
 *
 * Returns -1 if the device lock cannot be obtained or the plane is not
 * supported by the output, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_connect_to_output(stm_display_plane_t *p, struct stm_display_output_s *o)
{
  return p->ops->ConnectToOutput(p,o);
}

/*
 * int stm_display_plane_disconnect_from_output(stm_display_plane_t *p,
 *                                              struct stm_display_output_s *o)
 *
 *
 * Disconnect the plane from the output, it will no longer contribute to the
 * output's display and all hardware processing on the plane will be stopped.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_disconnect_from_output(stm_display_plane_t *p, struct stm_display_output_s *o)
{
  return p->ops->DisconnectFromOutput(p,o);
}


/*
 * int stm_display_plane_lock(stm_display_plane_t *p)
 *
 * Obtain exclusive use of the underlying plane's buffer queue for this
 * plane instance.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_lock(stm_display_plane_t *p)
{
  return p->ops->Lock(p);
}


/*
 * int stm_display_plane_unlock(stm_display_plane_t *p)
 *
 * Release this plane instance's exclusive use of the underlying plane's
 * buffer queue.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_unlock(stm_display_plane_t *p)
{
  return p->ops->Unlock(p);
}


/*
 * int stm_display_plane_queue_buffer(stm_display_plane_t *p,
 *                                    stm_display_buffer_t *b)
 *
 * Queue a buffer for display on the plane.
 *
 * Returns -1 if the device lock cannot be obtained, this plane instance
 * doesn't have exclusive access to the buffer queue or the queue operation
 * fails for any reason. Otherwise it returns 0.
 *
 */
static inline int stm_display_plane_queue_buffer(stm_display_plane_t *p, stm_display_buffer_t *b)
{
  return p->ops->QueueBuffer(p,b);
}

/*
 * int stm_display_plane_pause(stm_display_plane_t *p, int bFlushQueue)
 *
 * Pause buffer queue processing. The buffer currently being processed by
 * the hardware will continue to be on the display, to produce a still image.
 * If the bFlushQueue parameter is true then the buffer queue is flushed,
 * causing any buffer completed callbacks to fire. However caution is required,
 * as it is permissible for the implementation to split the processing of a
 * single buffer into multiple queue entries. This means that the completion
 * callback for the buffer being processed by the hardware may be called by
 * the flush. Therefore it is not recommended that the contents of flushed
 * buffers be changed while paused. The intended use of this facility is to
 * be able to requeue buffers with different parameters to avoid large
 * user perceptible latencies, e.g. switching from slow motion to normal
 * playback.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_pause(stm_display_plane_t *p, int bFlushQueue)
{
  return p->ops->Pause(p,bFlushQueue);
}

/*
 * int stm_display_plane_resume(stm_display_plane_t *p)
 *
 * Restarts buffer queue processing after a call to stm_display_plane_pause.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_resume(stm_display_plane_t *p)
{
  return p->ops->Resume(p);
}

/*
 * int stm_display_plane_flush(stm_display_plane_t *p)
 *
 * Removes all buffers from the plane's queue and buffers in the process
 * of being processed by the hardware. The plane will no longer contribute
 * anything to the final display output.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_flush(stm_display_plane_t *p)
{
  return p->ops->Flush(p);
}


/*
 * int stm_display_plane_hide(stm_display_plane_t *p)
 *
 * If the plane is currently active then hide it from display for all outputs
 * it is connected to, but do not stop processing buffers (including any
 * processing required for de-interlacing or film mode detection).
 *
 * Note: if the plane is flushed and then made active again by queueing
 * new buffers, the plane will automatically be visible on the outputs
 * again. The plane's current visibility is available via
 * stm_display_plane_get_status().
 *
 * Returns -1 if the device lock cannot be obtained or if the plane was not
 * currently active, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_hide(stm_display_plane_t *p)
{
  return p->ops->Hide(p);
}


/*
 * int stm_display_plane_show(stm_display_plane_t *p)
 *
 * If the plane is currently active then make sure it is visible on the display
 * for all outputs it is connected to.
 *
 * Returns -1 if the device lock cannot be obtained or if the plane was not
 * currently active, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_show(stm_display_plane_t *p)
{
  return p->ops->Show(p);
}


/*
 * int stm_display_plane_get_capabilities(stm_display_plane_t *p,
 *                                        stm_plane_caps_t *c)
 *
 * Populates the plane capabilities structure pointed to by "c". The
 * contents of the structure may vary when the plane is connected
 * to a running output, depending on the display mode.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_get_capabilities(stm_display_plane_t *p, stm_plane_caps_t *c)
{
  return p->ops->GetCapabilities(p,c);
}


/*
 * int stm_display_plane_get_image_formats(stm_display_plane_t *p,
 *                                         const SURF_FMT **f)
 *
 * Gets a pointer to a list of supported image formats and places it in "f".
 * The list itself is owned by the plane and musst not be modified.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns the
 * number of formats in the list.
 *
 */
static inline int stm_display_plane_get_image_formats(stm_display_plane_t *p, const SURF_FMT **f)
{
  return p->ops->GetImageFormats(p,f);
}


/*
 * int stm_display_plane_set_control(stm_display_plane_t *p,
 *                                   stm_plane_ctrl_t c,
 *                                   ULONG v)
 *
 * Change the value of the plane control specified to the value given.
 *
 * Returns -1 if the device lock cannot be obtained, the control is not
 * supported or the value set is invalid, otherwise it returns 0.
 *
 */
static inline int stm_display_plane_set_control(stm_display_plane_t *p, stm_plane_ctrl_t c, ULONG v)
{
  return p->ops->SetControl(p, c, v);
}


/*
 * int stm_display_plane_get_control(stm_display_plane_t *p,
 *                                   stm_plane_ctrl_t c,
 *                                   ULONG *v)
 *
 * Get the value of the plane control specified.
 *
 * Returns -1 if the device lock cannot be obtained or the control is not
 * supported, otherwise it returns 0 and the control's value is copied to the
 * address provided.
 *
 */
static inline int stm_display_plane_get_control(stm_display_plane_t *p, stm_plane_ctrl_t c, ULONG *v)
{
  return p->ops->GetControl(p, c, v);
}


/*
 * int stm_display_plane_set_depth(stm_display_plane_t *p,
 *                                 struct stm_display_output_s *o,
 *                                 int depth,
 *                                 int activate)
 *
 * Set the plane's depth in the plane stack of the specified output. If a plane
 * can be attached to multiple outputs then the position can be different on
 * each output. If the activate parameter is 0 then the change is only tested
 * for validity, the actual plane order is not changed.
 *
 * Returns -1 if the device lock cannot be obtained, the plane is not usable
 * on the given output or the specified depth is invalid, otherwise it
 * returns 0.
 *
 */
static inline int stm_display_plane_set_depth(stm_display_plane_t *p, struct stm_display_output_s *o, int depth, int activate)
{
  return p->ops->SetDepth(p, o, depth, activate);
}


/*
 * int stm_display_plane_get_depth(stm_display_plane_t *p,
 *                                 struct stm_display_output_s *o,
 *                                 int *depth)
 *
 * Get the plane's depth in the plane stack of the specified output. If a plane
 * can be attached to multiple outputs then the position can be different on
 * each output.
 *
 * Returns -1 if the device lock cannot be obtained. It returns %false if
 * paramters are wrong, e.g. the plane is unknown. Otherwise it returns %true
 * and the depth value is copied to the address provided.
 *
 */
static inline int stm_display_plane_get_depth(stm_display_plane_t *p, struct stm_display_output_s *o, int *d)
{
  return p->ops->GetDepth(p, o, d);
}


/*
 * void stm_display_plane_get_status(stm_display_plane_t *p, ULONG *s)
 *
 * Get the current plane status flags.
 *
 * This call cannot fail, does not take the device lock and may be called
 * from interrupt context.
 */
static inline void stm_display_plane_get_status(stm_display_plane_t *p, ULONG *s)
{
  p->ops->GetStatus(p, s);
}


/*
 * int stm_display_plane_release(stm_display_plane_t *p)
 *
 * Release the given plane instance. If this instance has locked the underlying
 * plane's buffer queue, then this lock is released and the queue flushed.
 *
 * Returns -1 if the device lock cannot be obtained, otherwise it returns 0.
 *
 * The instance pointer is invalid after this call completes sucessfully.
 */
static inline int stm_display_plane_release(stm_display_plane_t *p)
{
  return p->ops->Release(p);
}


#if defined(__cplusplus)
}
#endif

#endif /* _STM_DISPLAY_PLANE_H */
