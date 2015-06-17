/************************************************************************
 * Copyright (C) 2013, MathEmbedded. All Rights Reserved.
 *
 * This is a V4L2 driver which configures the capture pipeline in order
 * to capture the specified output point in the format specified
 *
 * As a V4L2 driver it accepts standard V4L2 commands
 * 
************************************************************************/

/******************************
 * INCLUDES
 ******************************/
#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/dmx.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "stm_v4l2.h"
#include "stmvout.h"

#include <include/stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <stmdisplayplane.h>
#include <stmdisplayblitter.h>

#include "dvb_cap_export.h"
#include "dvb_module.h"
#include "backend.h"
#include "dvb_v4l2.h"
#include "dvb_v4l2_export.h"
#include "stm_v4l2.h"
#include "stmdisplayoutput.h"
#include "dvb_video.h"
#include "dvb_cap_video.h"

/*******************************
 * EXTERNAL FUNCTION DEFINITIONS
 *******************************/

extern unsigned long GetPhysicalContiguous(unsigned long ptr,size_t size);

/******************************
 * GLOBAL VARIABLES
 ******************************/
unsigned long __iomem *cap_regs_base = NULL; /* Base address for Capture registers */

/******************************
 * PRIVATE STRUCTURES
 ******************************/

/* Interface to user */
struct stvin_description {
    char                name[32];
    int                 deviceId;
    int                 virtualId;
    int                 inuse;
    struct capture_v4l2 *priv;
};

/* Driver identification */
static struct stvin_description g_vinDevice[] = {
    {"CAPDVB_0",0,0,0,NULL},
    {"CAPDVB_1",0,0,0,NULL},
    {"CAPDVB_2",0,0,0,NULL}
};

/* Definition of pixel format */
typedef struct {
  SURF_FMT pixel_format;         /* Pixel format */
  unsigned long bits:5;          /* Corresponding bits to set */
  unsigned long bytes_per_pixel; /* Number of bits per pixel */
} cap_supported_pixel_format_t;

/* Supported pixel formats */
static cap_supported_pixel_format_t stmfb_v4l2_pixel_format[] =
{
    { SURF_RGB565, 0x0, 0x2 },
    { SURF_RGB888, 0x1, 0x3 },
    { SURF_ARGB8565, 0x4, 0x3 },
    { SURF_ARGB8888, 0x5, 0x4 },
    { SURF_ARGB1555, 0x6, 0x2 },
    { SURF_ARGB4444, 0x7, 0x2 },
    { SURF_CRYCB888, 0x10, 0x3 },
    { SURF_YCBCR422R, 0x12, 0x2 },
    { SURF_ACRYCB8888, 0x1e, 0x4 },
};

/* Definition of capture point */
typedef struct {
    cap_v4l2_capture_source_t source; /* Capture point */
    unsigned int bits:4;                /* Corresponding bits to set */
} cap_supported_capture_source_t;

/* Supported capture points */
static cap_supported_capture_source_t stmfb_v4l2_source_mapping[] =
{
    { CAP_PT_VID1, 0x1 },          /* Internal VID1 output inARGB8888 format */
    { CAP_PT_VID2, 0x2 },          /* Internal VID2 output in ARGB8888 format */
    { CAP_PT_EXT_YCBCR888, 0x3 },  /* External AYCbCr port in AYCbCr8888 format */
    { CAP_PT_MIX2_YCBCR, 0xa },    /* MIX2 YCbCr output */
    { CAP_PT_MIX2_RGB, 0xb },      /* MIX2 RGB output */
    { CAP_PT_MIX1_YCBCR, 0xe },    /* MIX1 YCbCr output */
    { CAP_PT_MIX1_RGB, 0xf }       /* MIX1 RGB output */
};

/* Definition of capture colorimetry */
typedef struct {
    enum v4l2_colorspace colorspace; /* Colorimetry */
    unsigned long bits:1;            /* Corresponding bits to set */
} cap_supported_colorspace_t;

static cap_supported_colorspace_t stmfb_v4l2_colorspace[] =
{
    { V4L2_COLORSPACE_SMPTE170M, 0x0 }, /* ITU-R 601 -- broadcast NTSC/PAL */
    { V4L2_COLORSPACE_REC709, 0x1 }     /* HD and modern captures. */
};

/******************************
 * SUPPORTED VIDEO MODES
 ******************************/

/* 
 * Capture Interface version of ModeParamsTable in the stmfb driver
 */
static cap_v4l2_video_mode_line_t CapModeParamsTable[] =
{
  /* 
   * SD/ED modes 
   */
  { CAP_720_480_I60000, { 60000, SCAN_I, 720, 480, 119, 36 }, { false, false } },
  { CAP_720_480_P60000, { 60000, SCAN_P, 720, 480, 122, 36 }, { false, false } },
  { CAP_720_480_I59940, { 59940, SCAN_I, 720, 480, 119, 36 }, { false, false } },  
  { CAP_720_480_P59940, { 59940, SCAN_P, 720, 480, 122, 36 }, { false, false } },
  { CAP_640_480_I59940, { 59940, SCAN_I, 640, 480, 118, 38 }, { false, false } },  
  { CAP_720_576_I50000, { 50000, SCAN_I, 720, 576, 132, 44 }, { false, false } },
  { CAP_720_576_P50000, { 50000, SCAN_P, 720, 576, 132, 44 }, { false, false } },
  { CAP_768_576_I50000, { 50000, SCAN_I, 768, 576, 155, 44 }, { false, false } },

  /*
   * Modes used in VCR trick modes
   */
  { CAP_720_240_P59940, { 59940, SCAN_P, 720, 240, 119, 18 }, { false, false } },
  { CAP_720_240_P60000, { 60000, SCAN_P, 720, 240, 119, 18 }, { false, false } },
  { CAP_720_288_P50000, { 50000, SCAN_P, 720, 288, 132, 22 }, { false, false } },

  /*
   * 1080p modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { CAP_1920_1080_P60000, { 60000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P59940, { 59940, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P50000, { 50000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P30000, { 30000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P29970, { 29970, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P25000, { 25000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P24000, { 24000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { CAP_1920_1080_P23976, { 23976, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },

  /*
   * 1080i modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { CAP_1920_1080_I60000, { 60000, SCAN_I, 1920, 1080, 192, 40 }, { true, true } },
  { CAP_1920_1080_I59940, { 59940, SCAN_I, 1920, 1080, 192, 40 }, { true, true } },
  { CAP_1920_1080_I50000_274M, { 50000, SCAN_I, 1920, 1080, 192, 40 }, { true, true} },
  /* Australian 1080i. */
  { CAP_1920_1080_I50000_AS4933, { 50000, SCAN_I, 1920, 1080, 352, 124 }, { true, true } },

  /*
   * 720p modes, SMPTE 296M (analogue) and CEA-861-C (HDMI)
   */
  { CAP_1280_720_P60000, { 60000, SCAN_P, 1280, 720, 260, 25 }, { true, true } },
  { CAP_1280_720_P59940, { 59940, SCAN_P, 1280, 720, 260, 25 }, { true, true } },
  { CAP_1280_720_P50000, { 50000, SCAN_P, 1280, 720, 260, 25 }, { true, true } },

  /*
   * A 1280x1152@50Hz Australian analogue HD mode.
   */
  { CAP_1280_1152_I50000, { 50000, SCAN_I, 1280, 1152, 235, 178 }, { true, true } },

  /*
   * good old VGA, the default fallback mode for HDMI displays, note that
   * the same video code is used for 4x3 and 16x9 displays and it is up to
   * the display to determine how to present it (see CEA-861-C). Also note
   * that CEA specifies the pixel aspect ratio is 1:1 .
   */
  { CAP_640_480_P59940, { 59940, SCAN_P, 640, 480, 144, 35 }, { false, false } },
  { CAP_640_480_P60000, { 60000, SCAN_P, 640, 480, 144, 35 }, { false, false } }
};

/****************************************************
 * Horizontal sample rate * converter filter constants
 ******************************************************/

typedef struct ResizingFilter_s
{
    unsigned int     ForScalingFactorLessThan; // 0x100 is x 1, 0x200 means down scaling, halving the dimension
    unsigned char    Coefficients[40];        // 40 for horizontal, 24 for vertical
} ResizingFilter_t;

#define ScalingFactor(N)        ((1000 * 256)/N)// Scaling factor in 1000s
#define WORDS_PER_VERTICAL_FILTER    6
#define WORDS_PER_HORIZONTAL_FILTER    10

static ResizingFilter_t VerticalResizingFilters[] =
{
    { ScalingFactor(850), /* Calculated with resize = x1.00 */
        { 0x00, 0x40, 0x00,
          0x09, 0x3d, 0xfa,
          0x13, 0x37, 0xf6,
          0x1d, 0x2f, 0xf4,
          0x26, 0x26, 0xf4,
          0x2f, 0x1b, 0xf6,
          0x36, 0x11, 0xf9,
          0x3b, 0x08, 0xfd } },

    { ScalingFactor(800),
        { 0x02, 0x3c, 0x02,
          0x0b, 0x38, 0xfd,
          0x14, 0x34, 0xf8,
          0x1d, 0x2e, 0xf5,
          0x26, 0x26, 0xf4,
          0x2e, 0x1d, 0xf5,
          0x35, 0x13, 0xf8,
          0x3b, 0x0a, 0xfb } },       
   
    { ScalingFactor(600), /* Calculated with resize = x0.70 */
        { 0x09, 0x2e, 0x09,
          0x0f, 0x2e, 0x03,
          0x16, 0x2b, 0xff,
          0x1d, 0x28, 0xfb,
          0x25, 0x23, 0xf8,
          0x2c, 0x1e, 0xf6,
          0x33, 0x18, 0xf5,
          0x39, 0x11, 0xf6 } },

    { ScalingFactor(375), /* Calculated with resize = x0.50 */
        { 0x10, 0x20, 0x10,
          0x13, 0x20, 0x0d,
          0x17, 0x1f, 0x0a,
          0x1a, 0x1f, 0x07,
          0x1e, 0x1e, 0x04,
          0x22, 0x1d, 0x01,
          0x26, 0x1b, 0xff,
          0x2b, 0x19, 0xfc } },

    { ScalingFactor(175), /* Calculated with resize = x0.25 */
        { 0x14, 0x18, 0x14,
          0x15, 0x18, 0x13,
          0x16, 0x17, 0x13,
          0x17, 0x17, 0x12,
          0x17, 0x18, 0x11,
          0x18, 0x18, 0x10,
          0x19, 0x18, 0x0f,
          0x1a, 0x17, 0x0f } },

    { 0xffffffff, /* calculated with resize = x0.10 */
        { 0x15, 0x16, 0x15,
          0x15, 0x16, 0x15,
          0x15, 0x16, 0x15,
          0x16, 0x15, 0x15,
          0x16, 0x15, 0x15,
          0x16, 0x15, 0x15,
          0x16, 0x16, 0x14,
          0x16, 0x16, 0x14 } }
};

static ResizingFilter_t HorizontalResizingFilters[] =
{
    { ScalingFactor(850), /* calculated with resize = x1.00 */
        { 0x00, 0x00, 0x40, 0x00, 0x00,
          0xfd, 0x09, 0x3c, 0xfa, 0x04,
          0xf9, 0x13, 0x39, 0xf5, 0x06,
          0xf5, 0x1f, 0x31, 0xf3, 0x08,
          0xf3, 0x2a, 0x28, 0xf3, 0x08,
          0xf3, 0x34, 0x1d, 0xf5, 0x07,
          0xf5, 0x3b, 0x12, 0xf9, 0x05,
          0xfa, 0x3f, 0x07, 0xfd, 0x03 } },

    { ScalingFactor(600), /* calculated with resize = x0.70 */
        { 0xf7, 0x0b, 0x3c, 0x0b, 0xf7,
          0xf5, 0x13, 0x3a, 0x04, 0xfa,
          0xf4, 0x1c, 0x34, 0xfe, 0xfe,
          0xf5, 0x23, 0x2e, 0xfa, 0x00,
          0xf7, 0x29, 0x26, 0xf7, 0x03,
          0xfa, 0x2d, 0x1e, 0xf6, 0x05,
          0xff, 0x30, 0x15, 0xf6, 0x06,
          0x03, 0x31, 0x0f, 0xf7, 0x06 } },

    { ScalingFactor(375), /* calculated with resize = x0.50 */
        { 0xfb, 0x13, 0x24, 0x13, 0xfb,
          0xfd, 0x17, 0x23, 0x0f, 0xfa,
          0xff, 0x1a, 0x23, 0x0b, 0xf9,
          0x01, 0x1d, 0x21, 0x08, 0xf9,
          0x04, 0x20, 0x1f, 0x04, 0xf9,
          0x07, 0x22, 0x1c, 0x01, 0xfa,
          0x0b, 0x23, 0x18, 0xff, 0xfb,
          0x0f, 0x24, 0x14, 0xfd, 0xfc } },

    { ScalingFactor(175), /* calculated with resize = x0.25 */
        { 0x09, 0x0f, 0x10, 0x0f, 0x09,
          0x0a, 0x0f, 0x11, 0x0e, 0x08,
          0x0a, 0x10, 0x11, 0x0e, 0x07,
          0x0b, 0x10, 0x12, 0x0d, 0x06,
          0x0c, 0x11, 0x12, 0x0c, 0x05,
          0x0d, 0x12, 0x11, 0x0c, 0x04,
          0x0e, 0x12, 0x11, 0x0b, 0x04,
          0x0f, 0x13, 0x11, 0x0a, 0x03 } },

    { 0xffffffff, /* calculated with resize = x0.10 */
        { 0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
          0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
          0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
          0x0d, 0x0d, 0x0d, 0x0d, 0x0c,
          0x0d, 0x0d, 0x0d, 0x0d, 0x0c,
          0x0d, 0x0d, 0x0e, 0x0d, 0x0b,
          0x0d, 0x0e, 0x0d, 0x0d, 0x0b,
          0x0d, 0x0e, 0x0d, 0x0d, 0x0b } }
};

/* Variable storage per device */
struct capture_v4l2 {
    int                            index;             /* Device index number */
    void __iomem                   *virt_address;     /* Memory buffer virtual address */
    unsigned long                  physical_address;  /* Memory buffer physical address */
    unsigned long                  capture_width;     /* Main pipe width in pixels */
    unsigned long                  capture_height ;   /* Main pipe height in pixels */
    unsigned long                  capture_interlaced;/* Is main pipe display interlaced (1) or 
                                                         non interlaced (0) */
    unsigned long                  display_width;     /* Aux pipe width in pixels */
    unsigned long                  display_height;    /* Aux pipe height in pixels */
    cap_supported_capture_source_t *capture_source;   /* Capture point */
    cap_supported_colorspace_t *colorspace;           /* Capture colorimetry */ 
    cap_supported_pixel_format_t   *pixel_format;     /* Capture pixel format */
    cap_v4l2_video_mode_line_t     *video_mode;       /* The main display parameters */
};

/******************************
 * INTERNAL FUNCTIONS
 ******************************/

/*
 * Locate the specified capture point
 *
 * @param source The capture point to find in the table
 * @return A pointer to the capture point parameters, NULL on error
 */
static cap_supported_capture_source_t *find_capture_source(
                                             cap_v4l2_capture_source_t source )
{
    unsigned int n;
 
    for( n=0; n < sizeof( stmfb_v4l2_source_mapping )/
        sizeof( cap_supported_capture_source_t ); ++n )
    {
        if ( stmfb_v4l2_source_mapping[n].source == source )
            return &stmfb_v4l2_source_mapping[n];
    }

    printk( KERN_ERR "Failed to find capture source\n" );
    return NULL;
}

/*
 * Locate the specified pixel format
 *
 * @param source The pixel format to find in the table
 * @return A pointer to the pixel format parameters, NULL on error
 */
static cap_supported_pixel_format_t *find_pixel_format(
                                              enum v4l2_field pixel_format )
{
    unsigned int n;

    for ( n=0 ; n < sizeof( stmfb_v4l2_pixel_format )/
        sizeof( cap_supported_pixel_format_t ); ++n )
    {
        if ( stmfb_v4l2_pixel_format[n].pixel_format == pixel_format )
            return &stmfb_v4l2_pixel_format[n];
    }

    printk( KERN_ERR "Failed to find pixel format\n" );

    return NULL;
}

/*
 * Locate the specified colorspace format
 *
 * @param source The colorspace format to find in the table
 * @return A pointer to the colorspace format parameters, NULL on error
 */
static cap_supported_colorspace_t *find_colorspace_format(
                                             enum v4l2_colorspace colorspace )
{
    unsigned int n;
 
    for( n=0; n < sizeof( stmfb_v4l2_colorspace )/
        sizeof( cap_supported_colorspace_t ); ++n )
    {
        if ( stmfb_v4l2_colorspace[n].colorspace == colorspace )
            return &stmfb_v4l2_colorspace[n];
    }

    printk( KERN_ERR "Failed to find colorspace\n" );
    return NULL;
}

/*
 * Locate the specified HDMI display mode
 *
 * @param height The main display height in pixels
 * @param width The main display width in pixels
 * @param is_display_interlaced The main display interlacing (1=yes, 0=no).
 * @return A pointer to the colorspace format parameters, NULL on error
 */
static cap_v4l2_video_mode_line_t *find_video_mode( unsigned long height,
                                                    unsigned long width,
                                                    unsigned long is_display_interlaced )
{
    unsigned int n;
    stm_scan_type_t scan_type = !is_display_interlaced ? SCAN_P : SCAN_I;

    for (n=0;n<sizeof(CapModeParamsTable)/sizeof(cap_v4l2_video_mode_line_t);++n)
    {
        if (CapModeParamsTable[n].ModeParams.ActiveAreaWidth == width &&
            CapModeParamsTable[n].ModeParams.ActiveAreaHeight == height &&
            CapModeParamsTable[n].ModeParams.ScanType  == scan_type )
            return &CapModeParamsTable[n];
    }

    printk(KERN_ERR "Failed to display parameters\n");
    return NULL;
}

/*
 * Get the filter coefficients
 *
 * @param table The filter table to search
 * @param filter_increment The filter increment to find coefficients for
 * @return A pointer to the coefficients, NULL on error
 */
static unsigned long *get_filter_coefficients( const ResizingFilter_t *table,
                                               unsigned long filter_increment )
{
    unsigned index=0;

    filter_increment &= 0xFFF;
   
    while( filter_increment > table[index].ForScalingFactorLessThan )
    {
        ++index;
    }

    return ( unsigned long * )table[index].Coefficients;
} 

/*
 * Get the filter increment value
 *
 * @param src The source size
 * @param display The output size
 * @return Filter increment
 */
static unsigned long get_rate_converter_increment( unsigned long src,
                                                   unsigned long display)
{
    unsigned long filter_increment=0;

    display = max( display, src/8 ); /* limit any down scaling to 1/8th */
    if ( display != src )
        filter_increment = ( 1 << 24 ) | ( ( ( src -1 ) * 256 ) / ( display-1 ) );

    return filter_increment;
}

/*
 * Start the capture pipeline with the parameters provided
 *
 * @param ldvb Parameters
 */
static void CapturePipeStart( struct capture_v4l2 *ldvb )
{
     unsigned long *c;
     unsigned int n;
     unsigned long gam_cap_ctl;
     const unsigned long gdp_plane_base_address_mask = 0xFFFFF000;

     const unsigned long gam_cap_hsrc = get_rate_converter_increment(
         ldvb->capture_width, ldvb->display_width );
     const unsigned long gam_cap_vsrc = get_rate_converter_increment(
         ldvb->capture_height, ldvb->display_height );
     const unsigned long capture_height_offset = ldvb->video_mode ?
         ldvb->video_mode->ModeParams.FullVBIHeight : 0;
     const unsigned long capture_source =
         ldvb->capture_source ? ldvb->capture_source->bits : 0;
     const unsigned long colorspace =
         ldvb->colorspace ? ldvb->colorspace->bits : 0;         
     const unsigned long pixel_format =
         ldvb->pixel_format ? ldvb->pixel_format->bits : 0;

     const unsigned bytes_per_pixel =
         ldvb->pixel_format ? ldvb->pixel_format->bytes_per_pixel : 2;
     const unsigned bytes_per_line =
         bytes_per_pixel * ( ldvb->display_width );

     const unsigned long gam_cap_cmw = ldvb->display_width |
         ( ( ldvb->display_height / ( ldvb->capture_interlaced ? 2 : 1 ) ) << 16 );
     const unsigned long gam_cap_pmp =
         bytes_per_line * ( ldvb->capture_interlaced ? 2 : 1 );

     const unsigned long gam_cap_cwo = capture_height_offset << 16;
     const unsigned long gam_cap_cws = ( ( ldvb->capture_width-1 ) << 0 ) |
         ( ( ldvb->capture_height+capture_height_offset-1) << 16 );
     
     const unsigned long bot_field_enable = ldvb->capture_interlaced ? 1 : 0;

     iowrite32( ldvb->physical_address & gdp_plane_base_address_mask,
         &cap_regs_base[GAM_CAP_VTP] );
     iowrite32( (ldvb->physical_address & gdp_plane_base_address_mask)
         +bytes_per_line, &cap_regs_base[GAM_CAP_VBP] );
     iowrite32( gam_cap_hsrc, &cap_regs_base[GAM_CAP_HSRC] );
     iowrite32( gam_cap_vsrc, &cap_regs_base[GAM_CAP_VSRC] );
     iowrite32( gam_cap_pmp, &cap_regs_base[GAM_CAP_PMP] );
     iowrite32( gam_cap_cmw, &cap_regs_base[GAM_CAP_CMW] );
     iowrite32( gam_cap_cwo, &cap_regs_base[GAM_CAP_CWO] );
     iowrite32( gam_cap_cws, &cap_regs_base[GAM_CAP_CWS] );

     c = get_filter_coefficients( VerticalResizingFilters, gam_cap_vsrc );
     for( n=0; n < WORDS_PER_VERTICAL_FILTER; ++n )
         iowrite32( c[n], &cap_regs_base[GAM_CAP_VFC(n)] );

     c = get_filter_coefficients( HorizontalResizingFilters, gam_cap_hsrc );
     for( n=0; n < WORDS_PER_HORIZONTAL_FILTER; ++n )
         iowrite32( c[n], &cap_regs_base[GAM_CAP_HFC(n)] );

     gam_cap_ctl =
         ( 0 << 27 )                      |/* Chroma sign unchanged */
         ( 0 << 26 )                      |/* Colour space converter */
         ( colorspace << 25 )             |/* 601/709 */
         ( 0 << 24 )                      |/* No colour space conversion */
         ( 0 << 23 )                      |/* LE bitmap */
         ( pixel_format << 16 )           |/* Pixel format */
         (( gam_cap_hsrc ? 1 : 0) << 10 ) |/* Horizontal resizing */
         (( gam_cap_vsrc ? 1 : 0) << 9 )  |/* Vertical downsizing */
         ( 1 << 8 )                       |/* Capture mode enabled */
         ( 0 << 7 )                       |/* Continuous capture mode */
         ( 1 << 6 )                       |/* Top field captured */
         ( bot_field_enable << 5 )        |/* Bottom field captured */
         ( 0 << 4 )                       |/* VTG 0 */
         capture_source;                   /* Source */  
     wmb();
     iowrite32( gam_cap_ctl, &cap_regs_base[GAM_CAP_CTL] );
}

/*
 * Process commands received (via the V4L2 driver) from user space
 *
 * @param handle The V4L2 handle for this driver
 * @param driver The driver handle
 * @param device The device number
 * @param type The driver type, in this case V4L2_BUF_TYPE_VIDEO_CAPTURE
 * @param cmd The user space command
 * @param arg The user space arguments
 * @return 0 on success, non-zero on failure 
 */
static int capture_ioctl( struct stm_v4l2_handles *handle,
                          struct stm_v4l2_driver *driver, 
                          int device, 
                          enum _stm_v4l2_driver_type type, 
                          struct file *file, 
                          unsigned int cmd, 
                          void *arg )
{
    struct capture_v4l2 *ldvb = handle->v4l2type[type].handle;


    switch( cmd )
    {
        case VIDIOC_ENUMINPUT:
        {
            struct v4l2_input *input = ( struct v4l2_input * )arg;
            int index = input->index - driver->index_offset[ device ];

            /* check consistency of index */
            if ( ( index < 0 ) || ( index >= ARRAY_SIZE ( g_vinDevice ) ) )
                goto err_inval;

            strcpy( input->name, g_vinDevice[index].name );

            break;
        }

        case VIDIOC_S_INPUT:
        {
            int id = ( *(int *) arg ) - driver->index_offset[ device ];

            /* Note: an invalid value is not ERROR unless the Registration
               Driver Interface tells so. */
            if ( ( id < 0 ) || ( id >= ARRAY_SIZE ( g_vinDevice ) ) )
                goto err_inval;

            /* check if resource already in use */
            if ( g_vinDevice[ id ].inuse )
            {
                DVB_ERROR("Device already in use \n");
                goto err_inval;
            }

            /* allocate handle for driver registration */
            handle->v4l2type[ type ].handle =
                kmalloc(sizeof( struct capture_v4l2 ),GFP_KERNEL );
            if ( !handle->v4l2type[ type ].handle )
            {
                DVB_ERROR("kmalloc failed\n");
                return -ENODEV;
            }

            ldvb = handle->v4l2type[ type ].handle;
            memset( ldvb, 0, sizeof( struct capture_v4l2 ) );

            ldvb->index = id;

            g_vinDevice[ id ].inuse = 1;
            g_vinDevice[ id ].priv = ldvb;
            break;
        }

        case VIDIOC_G_INPUT:
        {
            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            *( (int *)arg ) = ( ldvb->index + driver->index_offset[ device ] );

            break;
        }

        case VIDIOC_S_FMT:
        {
            struct v4l2_format *fmt = ( struct v4l2_format * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !fmt )
                goto err_inval;

            if ( fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;

            ldvb->capture_width = fmt->fmt.pix.width;
            ldvb->capture_height = fmt->fmt.pix.height;
            ldvb->capture_interlaced =
                fmt->fmt.pix.field == V4L2_FIELD_INTERLACED ? 1 : 0;
            ldvb->capture_source = find_capture_source( fmt->fmt.pix.priv );
            ldvb->pixel_format = find_pixel_format( fmt->fmt.pix.pixelformat ); 
            ldvb->colorspace = find_colorspace_format( fmt->fmt.pix.colorspace );
            ldvb->video_mode = find_video_mode( ldvb->capture_height,
                ldvb->capture_width, ldvb->capture_interlaced );
            break;
        }

        case VIDIOC_G_FMT:
        {
            struct v4l2_format *fmt = ( struct v4l2_format * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !fmt )
                goto err_inval;

            if ( fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;

            fmt->fmt.pix.field = ldvb->capture_interlaced == 1 ?
                V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
            fmt->fmt.pix.priv = ldvb->capture_source->source;
            fmt->fmt.pix.pixelformat = ldvb->pixel_format->pixel_format; 
            fmt->fmt.pix.colorspace = ldvb->colorspace->colorspace;
            break;
        }

        case VIDIOC_S_CROP:
        {
            struct v4l2_crop *crop = ( struct v4l2_crop * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !crop )
                goto err_inval;

            if ( crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;
 
            ldvb->display_width = crop->c.width;
            ldvb->display_height = crop->c.height;
            break;
        }

        case VIDIOC_G_CROP:
        {
            struct v4l2_crop *crop = ( struct v4l2_crop * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !crop  )
                goto err_inval;

            if ( crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;
 
            crop->c.width = ldvb->display_width;
            crop->c.height = ldvb->display_height;
            break;
        }

        case VIDIOC_QBUF:
        {
            struct v4l2_buffer *buf = ( struct v4l2_buffer * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !buf )
                goto err_inval;

            if ( buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;

            if ( buf->memory != V4L2_MEMORY_USERPTR )
                goto err_inval;

            if ( !buf->m.userptr )
                return -EIO;

            ldvb->virt_address = ( void __iomem * )buf->m.userptr;
            ldvb->physical_address =
                GetPhysicalContiguous( buf->m.userptr, buf->length );

            if ( !ldvb->physical_address )
                return -EIO;
            break;
        }

        case VIDIOC_QUERYBUF:
        {
            struct v4l2_buffer *buf = ( struct v4l2_buffer * )arg;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            if ( !buf )
                goto err_inval;

            if ( buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE )
                goto err_inval;

            if ( buf->memory != V4L2_MEMORY_USERPTR )
                goto err_inval;

            buf->m.userptr = ( unsigned long )ldvb->virt_address;
            break;
        }

        case VIDIOC_STREAMON:
            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            CapturePipeStart( ldvb );

            break;

        case VIDIOC_STREAMOFF:
        {
            unsigned long gam_cap_ctl;

            if ( !ldvb ) {
                DVB_ERROR( "driver handle NULL, call VIDIOC_S_INPUT first.\n" );
                return -ENODEV;
            }

            gam_cap_ctl = ioread32( &cap_regs_base[ GAM_CAP_CTL ] );
            gam_cap_ctl &= ~(1ul << 8);
            iowrite32( gam_cap_ctl, &cap_regs_base[ GAM_CAP_CTL ] );
            break;
        }

        default:
            return -ENODEV;
        }

        return 0;

err_inval:
        return -EINVAL;
}

/******************************
 * DRIVER FUNCTIONS
 ******************************/

static struct page* capture_vm_nopage( struct vm_area_struct *vma,
                                       unsigned long vaddr,
                                       int *type )
{
    struct page *page;
    void *page_addr;
    unsigned long page_frame;

    if ( vaddr > vma->vm_end )
        return NOPAGE_SIGBUS;

    /*
     * Note that this assumes an identity mapping between the page offset and
     * the pfn of the physical address to be mapped. This will get more complex
     * when the 32bit SH4 address space becomes available.
     */
    page_addr = ( void* )( ( vaddr - vma->vm_start ) + ( vma->vm_pgoff << PAGE_SHIFT ) );

    page_frame = ( ( unsigned long )page_addr >> PAGE_SHIFT );

    if( !pfn_valid( page_frame ) )
        return NOPAGE_SIGBUS;

    page = virt_to_page( __va( page_addr ) );

    get_page( page );

    if ( type )
        *type = VM_FAULT_MINOR;
    return page;
}

static void capture_vm_open( struct vm_area_struct *vma )
{
}

static void capture_vm_close( struct vm_area_struct *vma )
{
}

static struct vm_operations_struct capture_vm_ops_memory =
{
    .open     = capture_vm_open,
    .close    = capture_vm_close,
    .nopage   = capture_vm_nopage,
};

static int capture_mmap( struct stm_v4l2_handles *handle,
                         enum _stm_v4l2_driver_type type,
                         struct file *file,
                         struct vm_area_struct*  vma )
{
    return -EINVAL;
}

static int capture_close( struct stm_v4l2_handles *handle,
                          enum _stm_v4l2_driver_type type,
                          struct file *file )
{
    struct capture_v4l2 *ldvb = handle->v4l2type[ type ].handle;

    if ( !ldvb )
        return 0;

    g_vinDevice[ ldvb->index ].inuse = 0;

    handle->v4l2type[ type ].handle = NULL;
    kfree( ldvb );

    return 0;
}

static struct stm_v4l2_driver cap_video_capture = {
        .type          = STM_V4L2_VIDEO_INPUT,
        .control_range = {
                { V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST },
                { V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST },
                { V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST } },
        .n_indexes     = { ARRAY_SIZE ( g_vinDevice ), ARRAY_SIZE ( g_vinDevice ), ARRAY_SIZE ( g_vinDevice) },
        .ioctl         = capture_ioctl,
        .close         = capture_close,
        .poll          = NULL,
        .mmap          = capture_mmap,
        .name          = "Compositor Capture Device",
};

static int capture_probe( struct device *dev )
{
    unsigned int base;
    unsigned int size;
    struct platform_device *cap_device_data = to_platform_device( dev );
    struct resource *res = platform_get_resource( cap_device_data, IORESOURCE_MEM, 0 );

    stm_v4l2_register_driver( &cap_video_capture );

    if ( ( !cap_device_data ) || ( !cap_device_data->name ) ) {
        DVB_ERROR("Device probe failed.\n" );
        return -ENODEV;
    }

    base = res->start;
    size = res->end -base + 1;
    if ( !request_region(res->start, size, "cap" ) ) {
        dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
            ( unsigned long )res->start, ( unsigned long )res->end );
        return -EBUSY;
    }

    cap_regs_base = ioremap_nocache( base, size );
    if ( !cap_regs_base )
    {
        DVB_ERROR( "Failed to map IOs\n" );
        release_region( base, size );
        return -ENXIO;
    }

    DVB_DEBUG( "Capture pipeline initialised\n" );

    return 0;
}

static int capture_remove( struct device *dev )
{
    struct platform_device *cap_device_data = to_platform_device( dev );
    struct resource *res = platform_get_resource( cap_device_data, IORESOURCE_MEM, 0 );

    iounmap( cap_regs_base );
    cap_regs_base = NULL;

    release_region( res->start, res->end -res->start + 1 );

    stm_v4l2_unregister_driver( &cap_video_capture );

    DVB_DEBUG( "Capture pipeline unregistered\n" );

    return 0;
}

static struct device_driver cap_driver = {
    .name = "cap",
    .bus = &platform_bus_type,
    .probe = capture_probe,
    .remove = capture_remove,
};

__init int cap_init( void )
{
    return driver_register( &cap_driver );
}

void __exit cap_exit( void )
{
    driver_unregister( &cap_driver );
}

