/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#include "encoder_test.h"
#include "common.h"

#include <linux/delay.h>

// For Jiffies/
#include <linux/sched.h>

// For checking returned buffers
#include <linux/crc32.h>

#include <linux/delay.h>

#include "stm_memsrc.h"
#include "stm_memsink.h"

#include "video_buffer_traffic320yuv.h"
// video_buffer_traffic320yuv defines the following:
#define VIDEO_WIDTH       TRAFFIC_VIDEO_WIDTH
#define VIDEO_HEIGHT      TRAFFIC_VIDEO_HEIGHT
#define VIDEO_BUFFER_SIZE TRAFFIC_VIDEO_BUFFER_SIZE
#define VIDEO_SURFACE_FORMAT TRAFFIC_VIDEO_SURFACE_FORMAT

// Raster formats = bytes per pixel - all planar formats = 1
// Planar formats = total plane size as 2xfactor of main plane - all raster format = 2
// Width Alignment (bytes)
// Height Alignment (bytes) - obtained from SE DecodedBufferManager

static const int LUT[14][4] =
{
    {1, 2, 1,  1}, //    SURFACE_FORMAT_UNKNOWN,
    {1, 2, 1,  1}, //    SURFACE_FORMAT_MARKER_FRAME,
    {1, 2, 1,  1}, //    SURFACE_FORMAT_AUDIO,
    {1, 3, 16, 32},//    SURFACE_FORMAT_VIDEO_420_MACROBLOCK,
    {1, 3, 16, 32},//    SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK,
    {2, 2, 4,  1}, //    SURFACE_FORMAT_VIDEO_422_RASTER,
    {1, 3, 2,  16}, //    SURFACE_FORMAT_VIDEO_420_PLANAR,
    {1, 3, 2,  16}, //    SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED,
    {1, 4, 2,  16}, //    SURFACE_FORMAT_VIDEO_422_PLANAR,
    {4, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_8888_ARGB,
    {3, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_888_RGB,
    {2, 2, 2,  32}, //    SURFACE_FORMAT_VIDEO_565_RGB,
    {2, 2, 4,  32}, //    SURFACE_FORMAT_VIDEO_422_YUYV,
    {1, 3, 4,  32}, //    SURFACE_FORMAT_VIDEO_420_RASTER2B - Alignment may not be true in practice if input buffer not from decoder
};

// Some locals

#define SD_WIDTH         720
#define VIDEO_FRAME_RATE_NUM (25)
#define VIDEO_FRAME_RATE_DEN (1)
#define TIME_PER_FRAME ((1000*VIDEO_FRAME_RATE_DEN)/VIDEO_FRAME_RATE_NUM)

// Determine the output frame size....
#define VIDEO_PITCH     (((VIDEO_WIDTH*LUT[VIDEO_SURFACE_FORMAT][0] + LUT[VIDEO_SURFACE_FORMAT][2]-1)/LUT[VIDEO_SURFACE_FORMAT][2])*LUT[VIDEO_SURFACE_FORMAT][2])
#define VIDEO_BUFFER_HEIGHT     (((VIDEO_HEIGHT + LUT[VIDEO_SURFACE_FORMAT][3]-1)/LUT[VIDEO_SURFACE_FORMAT][3])*LUT[VIDEO_SURFACE_FORMAT][3])
#define VIDEO_FRAME_SIZE    ((VIDEO_PITCH*VIDEO_BUFFER_HEIGHT*LUT[VIDEO_SURFACE_FORMAT][1])/2)
#define TEST_BUFFER_SIZE    (1920*1088*3/2) //min(VIDEO_FRAME_SIZE, VIDEO_BUFFER_SIZE)
#define VERTICAL_ALIGNMENT  (LUT[VIDEO_SURFACE_FORMAT][3])

#define COMPRESSED_BUFFER_SIZE  1920*1080
#define VIDEO_BITRATE_MODE STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR
//#define VIDEO_BITRATE_MODE -1 //Fix qp
#define VIDEO_BITRATE 4000000
#define VIDEO_CPB_BUFFER_SIZE (VIDEO_BITRATE*2)

//#define DUMP_INPUT_BUFFER
//#define DUMP_OUTPUT_BUFFER

#define VIDEO_PREPROC_IN_MIN_WIDTH       32
#define VIDEO_PREPROC_IN_MIN_HEIGHT      32
#define VIDEO_PREPROC_IN_MAX_WIDTH     1920
#define VIDEO_PREPROC_IN_MAX_HEIGHT    1088
#define VIDEO_PREPROC_OUT_MIN_WIDTH     120 // fvdp-enc limit
#define VIDEO_PREPROC_OUT_MIN_HEIGHT     36 // fvdp-enc limit
#define VIDEO_PREPROC_OUT_MAX_WIDTH    1920
#define VIDEO_PREPROC_OUT_MAX_HEIGHT   1088
#define VIDEO_PREPROC_MIN_FRAMERATE       1
#define VIDEO_PREPROC_MAX_FRAMERATE      60
#define VIDEO_PREPROC_4_BY_3_WIDTH      160
#define VIDEO_PREPROC_4_BY_3_HEIGHT     120
#define VIDEO_PREPROC_16_BY_9_WIDTH     320
#define VIDEO_PREPROC_16_BY_9_HEIGHT    180
#define VIDEO_PREPROC_221_1_WIDTH       221
#define VIDEO_PREPROC_221_1_HEIGHT      100

typedef enum buffer_argument_s
{
    RAW_VIDEO_FRAME_NONE       = (0),
    RAW_VIDEO_FRAME_VIRTUAL    = (1 << 0),
    RAW_VIDEO_FRAME_PHYSICAL   = (1 << 1),
    RAW_VIDEO_FRAME_SIZE       = (1 << 2),
    RAW_VIDEO_FRAME_ALIGNMENT  = (1 << 2),
    RAW_VIDEO_FRAME_NULL       = (1 << 2),
} buffer_argument_t;

// METADATA VALUE ONLY
static const int32_t GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY[][2] =
{
    {1, 1},
    {0, 1},
    {1, 0},
    {0, 0},
};

// With unspecified aspect ratio
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[][2] =
{
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT}, // MIN
    {VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT}, // MAX
    {1920, 1080},                                        // SUPPORTED
    {1920, 1088},                                        // SUPPORTED
    {1280, 720},                                         // SUPPORTED
};

// With unspecified aspect ratio
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[][2] =
{
    {VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT}, // MIN WIDTH
    {VIDEO_PREPROC_IN_MAX_WIDTH + 1, VIDEO_PREPROC_IN_MAX_HEIGHT}, // MAX WIDTH
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT - 1}, // MIN HEIGHT
    {VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT + 1}, // MAX HEIGHT
    {4096, 2048},                                            // UNSUPPORTED
    {VIDEO_WIDTH, 0},                                        // INVALID
    {0, VIDEO_HEIGHT},                                       // INVALID
    {0, 0},                                                  // INVALID
};

static const int32_t GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[][2] =
{
    {10, 1},
    {40, 1},
//    {60000, 1001}, // test for coder
//    {30000, 1001}, // test for coder
//    {24000, 1001}, // test for coder
};

static const int32_t BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[][2] =
{
    {0, 1},
    {1, 0},
};

static const int32_t GOOD_VIDEO_PREPROC_COLORSPACE_ONLY[] =
{
    STM_SE_COLORSPACE_UNSPECIFIED,
    STM_SE_COLORSPACE_SMPTE170M,
    STM_SE_COLORSPACE_SMPTE240M,
    STM_SE_COLORSPACE_BT709,
    STM_SE_COLORSPACE_BT470_SYSTEM_M,
    STM_SE_COLORSPACE_BT470_SYSTEM_BG,
    STM_SE_COLORSPACE_SRGB,
};

static const int32_t BAD_VIDEO_PREPROC_COLORSPACE_ONLY[] =
{
    STM_SE_COLORSPACE_UNSPECIFIED - 1, // MIN
    STM_SE_COLORSPACE_SRGB + 1,        // MAX
};

static const int32_t GOOD_VIDEO_PREPROC_MEDIA_ONLY[] =
{
    STM_SE_MEDIA_VIDEO,
};

static const int32_t BAD_VIDEO_PREPROC_MEDIA_ONLY[] =
{
    STM_SE_MEDIA_AUDIO - 1, // MIN
    STM_SE_MEDIA_ANY + 1,   // MAX
    STM_SE_MEDIA_AUDIO,
    STM_SE_MEDIA_ANY,
};

static const int32_t GOOD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[] =
{
    STM_SE_PICTURE_TYPE_UNKNOWN,
    STM_SE_PICTURE_TYPE_I,
    STM_SE_PICTURE_TYPE_P,
    STM_SE_PICTURE_TYPE_B,
};

static const int32_t BAD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[] =
{
    STM_SE_PICTURE_TYPE_UNKNOWN - 1, // MIN
    STM_SE_PICTURE_TYPE_B + 1,       // MAX
};

static const int32_t GOOD_VIDEO_PREPROC_SCAN_TYPE_ONLY[] =
{
    STM_SE_SCAN_TYPE_PROGRESSIVE,
    STM_SE_SCAN_TYPE_INTERLACED,
};

static const int32_t BAD_VIDEO_PREPROC_SCAN_TYPE_ONLY[] =
{
    STM_SE_SCAN_TYPE_PROGRESSIVE - 1, // MIN
    STM_SE_SCAN_TYPE_INTERLACED + 1,  // MAX
};

// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[] =
{
    SURFACE_FORMAT_VIDEO_422_RASTER,
    SURFACE_FORMAT_VIDEO_420_PLANAR,
    SURFACE_FORMAT_VIDEO_420_RASTER2B,
};

// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[] =
{
    SURFACE_FORMAT_UNKNOWN - 1,            // MIN
    SURFACE_FORMAT_VIDEO_420_RASTER2B + 1, // MAX
    SURFACE_FORMAT_UNKNOWN,
    SURFACE_FORMAT_MARKER_FRAME,
    SURFACE_FORMAT_AUDIO,
    SURFACE_FORMAT_VIDEO_420_MACROBLOCK,
    SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK,
    SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED,
    SURFACE_FORMAT_VIDEO_422_PLANAR,
    SURFACE_FORMAT_VIDEO_8888_ARGB,
    SURFACE_FORMAT_VIDEO_888_RGB,
    SURFACE_FORMAT_VIDEO_565_RGB,
    SURFACE_FORMAT_VIDEO_422_YUYV,
};

static const int32_t GOOD_VIDEO_PREPROC_DISCONTINUITY_ONLY[] =
{
    STM_SE_DISCONTINUITY_CONTINUOUS,         // MIN
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST, // MAX
    STM_SE_DISCONTINUITY_DISCONTINUOUS,      // SUPPORTED
    STM_SE_DISCONTINUITY_EOS,                // SUPPORTED
};

static const int32_t BAD_VIDEO_PREPROC_DISCONTINUITY_ONLY[] =
{
    STM_SE_DISCONTINUITY_CONTINUOUS - 1,     // MIN
    STM_SE_DISCONTINUITY_FADEIN + 1,         // MAX
    STM_SE_DISCONTINUITY_OPEN_GOP_REQUEST,   // UNSUPPORTED
    STM_SE_DISCONTINUITY_FRAME_SKIPPED,      // UNSUPPORTED
    STM_SE_DISCONTINUITY_MUTE,               // UNSUPPORTED
    STM_SE_DISCONTINUITY_FADEOUT,            // UNSUPPORTED
    STM_SE_DISCONTINUITY_FADEIN,             // UNSUPPORTED
};

// With pixel aspect ratio 1/1
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[] =
{
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_221_1,

    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_221_1,
};

// With pixel aspect ratio 1/1
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[][2] =
{
    {VIDEO_PREPROC_4_BY_3_WIDTH, VIDEO_PREPROC_4_BY_3_HEIGHT},
    {VIDEO_PREPROC_16_BY_9_WIDTH, VIDEO_PREPROC_16_BY_9_HEIGHT},
    {VIDEO_PREPROC_221_1_WIDTH, VIDEO_PREPROC_221_1_HEIGHT},

    {VIDEO_PREPROC_4_BY_3_WIDTH + 1, VIDEO_PREPROC_4_BY_3_HEIGHT},
    {VIDEO_PREPROC_4_BY_3_WIDTH, VIDEO_PREPROC_4_BY_3_HEIGHT + 1},
    {VIDEO_PREPROC_4_BY_3_WIDTH - 1, VIDEO_PREPROC_4_BY_3_HEIGHT},
    {VIDEO_PREPROC_4_BY_3_WIDTH, VIDEO_PREPROC_4_BY_3_HEIGHT - 1},
    {VIDEO_PREPROC_16_BY_9_WIDTH + 1, VIDEO_PREPROC_16_BY_9_HEIGHT},
    {VIDEO_PREPROC_16_BY_9_WIDTH, VIDEO_PREPROC_16_BY_9_HEIGHT + 1},
    {VIDEO_PREPROC_16_BY_9_WIDTH - 1, VIDEO_PREPROC_16_BY_9_HEIGHT},
    {VIDEO_PREPROC_16_BY_9_WIDTH, VIDEO_PREPROC_16_BY_9_HEIGHT - 1},
    {VIDEO_PREPROC_221_1_WIDTH + 1, VIDEO_PREPROC_221_1_HEIGHT},
    {VIDEO_PREPROC_221_1_WIDTH, VIDEO_PREPROC_221_1_HEIGHT + 1},
    {VIDEO_PREPROC_221_1_WIDTH - 1, VIDEO_PREPROC_221_1_HEIGHT},
    {VIDEO_PREPROC_221_1_WIDTH, VIDEO_PREPROC_221_1_HEIGHT - 1},
};

// With pixel aspect ratio 1/1
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[] =
{
    STM_SE_ASPECT_RATIO_4_BY_3 - 1,      // MIN
    STM_SE_ASPECT_RATIO_UNSPECIFIED + 1, // MAX
};

// With pixel aspect ratio 1/1
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[][2] =
{
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
};

static const int32_t GOOD_VIDEO_PREPROC_DISCONTINUITY_VS_BUFFER[] =
{
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST,
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST,
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST,
    STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST,
    STM_SE_DISCONTINUITY_EOS,
    STM_SE_DISCONTINUITY_EOS,
    STM_SE_DISCONTINUITY_EOS,
    STM_SE_DISCONTINUITY_EOS,
    STM_SE_DISCONTINUITY_EOS,
};

static const int32_t GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[][4] =
{
    {RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},
};

static const int32_t BAD_VIDEO_PREPROC_DISCONTINUITY_VS_BUFFER[] =
{
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_CONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
    STM_SE_DISCONTINUITY_DISCONTINUOUS,
};

static const int32_t BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[][4] =
{
    {RAW_VIDEO_FRAME_SIZE, 0, 0, 0},                                                      // SIZE NULL
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},                            // SIZE NULL
    {RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0},                           // SIZE NULL
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_PHYSICAL | RAW_VIDEO_FRAME_SIZE, 0, 0, 0}, // SIZE NULL
    {RAW_VIDEO_FRAME_VIRTUAL, 1, 0, 0},                                                   // ALIGNMENT
    {RAW_VIDEO_FRAME_PHYSICAL, 0, 1, 0},                                                  // ALIGNMENT
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_PHYSICAL, 1, 1, 0},                        // ALIGNMENT
    {RAW_VIDEO_FRAME_VIRTUAL, 0, 0, 0},                                                   // BUFFER NULL
    {RAW_VIDEO_FRAME_PHYSICAL, 0, 0, 0},                                                  // BUFFER NULL
    {RAW_VIDEO_FRAME_VIRTUAL | RAW_VIDEO_FRAME_PHYSICAL, 0, 0, 0},                        // BUFFER NULL
};

// METADATA VS CONTROL VALUE
static const int32_t GOOD_VIDEO_PREPROC_DISPLAY_VS_STORAGE_ASPECT_RATIO[] =
{
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE,
};

// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[] =
{
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_4_BY_3,
    STM_SE_ASPECT_RATIO_16_BY_9,
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_UNSPECIFIED,
};

static const int32_t BAD_VIDEO_PREPROC_DISPLAY_VS_STORAGE_ASPECT_RATIO[] =
{
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3,
    STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9,
};

// With correct input resolution
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[] =
{
    STM_SE_ASPECT_RATIO_221_1,
    STM_SE_ASPECT_RATIO_221_1,
};

static const int32_t GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING_VS_WINDOW_OF_INTEREST[] =
{
    STM_SE_CTRL_VALUE_DISAPPLY,
    STM_SE_CTRL_VALUE_DISAPPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
};

// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[][4] =
{
    {0, 0, 0, 0},                      // SUPPORTED
    {0, 0, VIDEO_WIDTH, VIDEO_HEIGHT}, // WARNING
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH - VIDEO_PREPROC_IN_MIN_WIDTH, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, VIDEO_PREPROC_IN_MAX_HEIGHT - VIDEO_PREPROC_IN_MIN_HEIGHT, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH - VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT - VIDEO_PREPROC_IN_MIN_HEIGHT, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
};

static const int32_t BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING_VS_WINDOW_OF_INTEREST[] =
{
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
    STM_SE_CTRL_VALUE_APPLY,
};

// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[][4] =
{
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT}, // MIN WIDTH
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH + 1, VIDEO_PREPROC_IN_MAX_HEIGHT}, // MAX WIDTH
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT - 1}, // MIN HEIGHT
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT + 1}, // MAX HEIGHT
    {VIDEO_PREPROC_IN_MAX_WIDTH - VIDEO_PREPROC_IN_MIN_WIDTH + 1, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, VIDEO_PREPROC_IN_MAX_HEIGHT - VIDEO_PREPROC_IN_MIN_HEIGHT + 1, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH - VIDEO_PREPROC_IN_MIN_WIDTH + 1, VIDEO_PREPROC_IN_MAX_HEIGHT - VIDEO_PREPROC_IN_MIN_HEIGHT, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH - VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT - VIDEO_PREPROC_IN_MIN_HEIGHT + 1, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, 0, 4096, 2048},                                            // UNSUPPORTED
    {0, 0, VIDEO_WIDTH, 0},                                        // INVALID
    {0, 0, 0, VIDEO_HEIGHT},                                       // INVALID
    {0, 0, 0, 0},                                                  // INVALID
};

static const int32_t GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[][2] =
{
    {VIDEO_PREPROC_OUT_MAX_WIDTH, VIDEO_PREPROC_OUT_MAX_HEIGHT},
    {VIDEO_PREPROC_OUT_MAX_WIDTH, VIDEO_PREPROC_OUT_MAX_HEIGHT},
    {VIDEO_PREPROC_OUT_MIN_WIDTH, VIDEO_PREPROC_OUT_MIN_HEIGHT},
    {VIDEO_PREPROC_OUT_MIN_WIDTH, VIDEO_PREPROC_OUT_MIN_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
};

// With cropping disable
// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[][2] =
{
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {1920, 1080},
    {1920, 1088},
    {1280, 720},
};

// With cropping enable
// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[][4] =
{
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT},
    {0, 0, 1920, 1080},
    {0, 0, 1920, 1088},
    {0, 0, 1280, 720},
};

static const int32_t BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[][2] =
{
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
    {VIDEO_WIDTH, VIDEO_HEIGHT},
};

// With cropping disable
// With unspecified aspect ratio
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[][2] =
{
    {VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT - 1},
    {VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT - 1},
    {VIDEO_PREPROC_IN_MAX_WIDTH + 1, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT + 1},
    {VIDEO_PREPROC_IN_MIN_WIDTH + 1, VIDEO_PREPROC_IN_MAX_HEIGHT + 1},
};

// With cropping enable
// With unspecified aspect ratio
// With correct input resolution
// With correct pitch
static const int32_t BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[][4] =
{
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MIN_HEIGHT - 1},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH - 1, VIDEO_PREPROC_IN_MIN_HEIGHT - 1},
    {0, 0, VIDEO_PREPROC_IN_MAX_WIDTH + 1, VIDEO_PREPROC_IN_MIN_HEIGHT},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH, VIDEO_PREPROC_IN_MAX_HEIGHT + 1},
    {0, 0, VIDEO_PREPROC_IN_MIN_WIDTH + 1, VIDEO_PREPROC_IN_MAX_HEIGHT + 1},
};

static const int32_t GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[][2] =
{
    {40, 1},
    {10, 1},
    {30, 1},
    {30, 1},
    {30, 1},
};

static const int32_t GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[][2] =
{
    {10, 1},
    {40, 1},
    {60000, 1001},
    {30000, 1001},
    {24000, 1001},
};

static const int32_t BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[][2] =
{
    {30, 1},
    {30, 1},
    {41, 1},
//    {10, 1},
};

static const int32_t BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[][2] =
{
    {0, 1},
    {1, 0},
    {10, 1},
//    {41, 1},
};

static int GetBitPerPixel(surface_format_t SurfaceFormat)
{
    int bitPerPixel = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        bitPerPixel = 16;
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
        bitPerPixel = 12;
        break;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        bitPerPixel = 16;
        break;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        bitPerPixel = 12;
        break;
    default:
        return 12;
    }

    return bitPerPixel;
}

static int GetWidth(stm_se_aspect_ratio_t AspectRatio)
{
    int width = 0;

    switch (AspectRatio)
    {
    case STM_SE_ASPECT_RATIO_4_BY_3:
        width = VIDEO_PREPROC_4_BY_3_WIDTH;
        break;
    case STM_SE_ASPECT_RATIO_16_BY_9:
        width = VIDEO_PREPROC_16_BY_9_WIDTH;
        break;
    case STM_SE_ASPECT_RATIO_221_1:
        width = VIDEO_PREPROC_221_1_WIDTH;
        break;
    case STM_SE_ASPECT_RATIO_UNSPECIFIED:
        width = VIDEO_PREPROC_4_BY_3_WIDTH;
        break;
    default:
        return VIDEO_PREPROC_4_BY_3_WIDTH;
    }

    return width;
}

static int GetHeight(stm_se_aspect_ratio_t AspectRatio)
{
    int height = 0;

    switch (AspectRatio)
    {
    case STM_SE_ASPECT_RATIO_4_BY_3:
        height = VIDEO_PREPROC_4_BY_3_HEIGHT;
        break;
    case STM_SE_ASPECT_RATIO_16_BY_9:
        height = VIDEO_PREPROC_16_BY_9_HEIGHT;
        break;
    case STM_SE_ASPECT_RATIO_221_1:
        height = VIDEO_PREPROC_221_1_HEIGHT;
        break;
    case STM_SE_ASPECT_RATIO_UNSPECIFIED:
        height = VIDEO_PREPROC_4_BY_3_HEIGHT;
        break;
    default:
        return VIDEO_PREPROC_4_BY_3_HEIGHT;
    }

    return height;
}

/* Return: -1: fail; 0: pass 1: EOS */
static int PullCompressedEncode(pp_descriptor *memsinkDescriptor)
{
    int retval = 0;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    u32 crc = 0;
    int loop = 0;

    //Wait for encoded frame to be available for memsink
    do
    {
        loop ++;
        memsinkDescriptor->m_availableLen = 0;
        retval = stm_memsink_test_for_data(memsinkDescriptor->obj, &memsinkDescriptor->m_availableLen);

        if (retval && (-EAGAIN != retval))
        {
            pr_err("%s stm_memsink_test_for_data failed (%d)\n", __FUNCTION__, retval);
            return -1;
        }

        mdelay(1); //in ms

        if (loop == 100)
        {
            pr_err("PullCompressedEncode : Nothing available\n");
            return -1;
        }
    }
    while (-EAGAIN == retval);

    //
    // setup memsink_pull_buffer
    //
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)memsinkDescriptor->m_Buffer;
    p_memsink_pull_buffer->physical_address = NULL;
    p_memsink_pull_buffer->virtual_address  = &memsinkDescriptor->m_Buffer      [sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length    = COMPRESSED_BUFFER_SIZE           - sizeof(stm_se_capture_buffer_t);
    retval = stm_memsink_pull_data(memsinkDescriptor->obj,
                                   p_memsink_pull_buffer,
                                   p_memsink_pull_buffer->buffer_length,
                                   &memsinkDescriptor->m_availableLen);

    if (retval != 0)
    {
        pr_err("PullCompressedEncode : stm_memsink_pull_data fails (%d)\n", retval);
        return -1;
    }

    // Check for EOS
    if (p_memsink_pull_buffer->u.compressed.discontinuity & STM_SE_DISCONTINUITY_EOS)
    {
        pr_info("%s EOS Detected! discontinuity 0x%x\n", __FUNCTION__, p_memsink_pull_buffer->u.compressed.discontinuity);
        return 1;
    }

#ifdef DUMP_OUTPUT_BUFFER
    print_hex_dump(KERN_INFO, "OUTPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   p_memsink_pull_buffer->virtual_address, min((unsigned) 64, memsinkDescriptor->m_availableLen), true);
#endif
    crc = crc32_be(crc, p_memsink_pull_buffer->virtual_address, memsinkDescriptor->m_availableLen);
    pr_info("CRC Value of this frame pull is 0x%08x\n", crc);
    return 0;
}

static int init_stream(EncoderContext *pContext)
{
    int result = 0;

    // Create a MemSink
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&gVideoTest[0].Sink.obj);
    if (result < 0)
    {
        pr_err("%s: Failed to create a new memsink\n", __FUNCTION__);
        return result;
    }

    // Create Stream
    result = stm_se_encode_stream_new("EncodeStream0", pContext->encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &pContext->video_stream[0]);
    if (result < 0)
    {
        pr_err("%s: Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __FUNCTION__);
        goto stream_new_fail;
    }

    // Attach the memsink
    result = stm_se_encode_stream_attach(pContext->video_stream[0], gVideoTest[0].Sink.obj);
    if (result < 0)
    {
        pr_err("%s: Failed to attach encode stream to memsink for video\n", __FUNCTION__);
        goto stream_attach_fail;
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE Control
    result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE, VIDEO_BITRATE_MODE);
    if (result < 0)
    {
        pr_err("%s: Expected pass on testing set valid control\n", __FUNCTION__);
        goto set_control_fail;
    }

    // STM_SE_CTRL_ENCODE_STREAM_BITRATE Control
    result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_ENCODE_STREAM_BITRATE, VIDEO_BITRATE);
    if (result < 0)
    {
        pr_err("%s: Expected pass on testing set valid control\n", __FUNCTION__);
        goto set_control_fail;
    }

    // STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE Control
    result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE, VIDEO_CPB_BUFFER_SIZE);
    if (result < 0)
    {
        pr_err("%s: Expected pass on testing set valid control\n", __FUNCTION__);
        goto set_control_fail;
    }

    return 0;

set_control_fail:
    if (stm_se_encode_stream_detach(pContext->video_stream[0], gVideoTest[0].Sink.obj)) {};
stream_attach_fail:
    if (stm_se_encode_stream_delete(pContext->video_stream[0])) {};
stream_new_fail:
    if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj)) {};

    return result;
}

static int deinit_stream(EncoderContext *pContext)
{
    int result = 0;

    result = stm_se_encode_stream_inject_discontinuity(pContext->video_stream[0], STM_SE_DISCONTINUITY_EOS);
    if (result < 0)
    {
        pr_err("%s: Failed to inject discontinuity EOS\n", __FUNCTION__);
        goto fail_inject_discontinuity;
    }

    result = PullCompressedEncode(&gVideoTest[0].Sink);
    if (result < 0)
    {
        pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
        goto fail_inject_discontinuity;
    }

    result = stm_se_encode_stream_detach(pContext->video_stream[0], gVideoTest[0].Sink.obj);
    if (result < 0)
    {
        pr_err("%s: Failed to detach encode stream from memsink\n", __FUNCTION__);
        goto fail_stream_detach;
    }

    // Remove Stream
    result = stm_se_encode_stream_delete(pContext->video_stream[0]);
    if (result < 0)
    {
        pr_err("%s: Failed to delete encode stream %d\n", __FUNCTION__, result);
        goto fail_stream_delete;
    }

    // Remove the memsink
    result = stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj);
    if (result < 0)
    {
        pr_err("%s: Failed to delete memsink\n", __FUNCTION__);
        return result;
    }

    return 0;

fail_inject_discontinuity:
    if (stm_se_encode_stream_detach(pContext->video_stream[0], gVideoTest[0].Sink.obj)) {};
fail_stream_detach:
    if (stm_se_encode_stream_delete(pContext->video_stream[0])) {};
fail_stream_delete:
    if (stm_memsink_delete((stm_memsink_h)gVideoTest[0].Sink.obj)) {};

    return result;
}

// These tests are expected to fail ... returning success is a failure!
static int inject_only_fail(EncoderContext *pContext)
{
    int i = 0;
    int result = 0;
    int failures = 0;
    bpa_buffer original_raw_video_frame;
    bpa_buffer raw_video_frame;
    bpa_buffer compressed_video_frame;
    stm_se_uncompressed_frame_metadata_t original_descriptor;
    stm_se_uncompressed_frame_metadata_t descriptor;

    pr_info("%s: Testing Failure Cases - You could expect errors here\n", __func__);

    // Allocate Memory Buffer
    result = AllocateBuffer(TEST_BUFFER_SIZE, &original_raw_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        goto allocation_fail;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    // Fill buffer
    memset(original_raw_video_frame.virtual, 0x80, original_raw_video_frame.size);
#ifdef DUMP_INPUT_BUFFER
    print_hex_dump(KERN_INFO, "INPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   original_raw_video_frame.virtual, min((unsigned) 64, original_raw_video_frame.size), true);
#endif

    memset(&original_descriptor, 0, sizeof(descriptor));
    original_descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    original_descriptor.video.frame_rate.framerate_num                        = VIDEO_FRAME_RATE_NUM;
    original_descriptor.video.frame_rate.framerate_den                        = VIDEO_FRAME_RATE_DEN;
    original_descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    original_descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    original_descriptor.video.video_parameters.colorspace                     = (VIDEO_WIDTH > SD_WIDTH ? STM_SE_COLORSPACE_SMPTE170M : STM_SE_COLORSPACE_SMPTE240M);
    original_descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    original_descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    original_descriptor.video.pitch                                           = VIDEO_PITCH;
    original_descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    original_descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    original_descriptor.video.surface_format                                  = VIDEO_SURFACE_FORMAT;
    original_descriptor.video.vertical_alignment                              = VERTICAL_ALIGNMENT;
    original_descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    original_descriptor.native_time   = jiffies / 10;
    original_descriptor.system_time   = jiffies;
    original_descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY) / sizeof(BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[i][0];
        descriptor.video.video_parameters.height = BAD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }


    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY) / sizeof(BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.frame_rate.framerate_num = BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[i][0];
        descriptor.video.frame_rate.framerate_den = BAD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[i][1];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_COLORSPACE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_COLORSPACE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.colorspace = BAD_VIDEO_PREPROC_COLORSPACE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_MEDIA_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_MEDIA_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.media = BAD_VIDEO_PREPROC_MEDIA_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_PICTURE_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.picture_type = BAD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_SCAN_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_SCAN_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.scan_type = BAD_VIDEO_PREPROC_SCAN_TYPE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.surface_format = BAD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[i];
        descriptor.video.pitch = descriptor.video.video_parameters.width * GetBitPerPixel(descriptor.video.surface_format) / 8;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_DISCONTINUITY_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_DISCONTINUITY_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.discontinuity = BAD_VIDEO_PREPROC_DISCONTINUITY_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.aspect_ratio = BAD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[i];
        descriptor.video.video_parameters.width = BAD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[i][0];
        descriptor.video.video_parameters.height = BAD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY) / sizeof(BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.discontinuity = BAD_VIDEO_PREPROC_DISCONTINUITY_VS_BUFFER[i];

        raw_video_frame = original_raw_video_frame;
        if (BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_VIRTUAL)
        {
            raw_video_frame.virtual = (void *)BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][1];
        }

        if (BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_PHYSICAL)
        {
            raw_video_frame.physical = BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][2];
        }

        if (BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_SIZE)
        {
            raw_video_frame.size = BAD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][3];
        }

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], raw_video_frame.virtual, raw_video_frame.physical, raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    // Free BPA Memory
    result = FreeBuffer(&compressed_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&original_raw_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    pr_info("%s: Failure testing complete (%d)\n", __func__, failures);
    return failures;

init_stream_fail:
deinit_stream_fail:
    if (FreeBuffer(&compressed_video_frame)) {};
allocation_fail:
    if (FreeBuffer(&original_raw_video_frame)) {};

    return result;
}

// These tests are expected to pass ... returning success is a good thing
static int inject_only_pass(EncoderContext *pContext)
{
    int i = 0;
    int failures = 0;
    int result = 0;
    bpa_buffer original_raw_video_frame;
    bpa_buffer raw_video_frame;
    bpa_buffer compressed_video_frame;
    stm_se_uncompressed_frame_metadata_t original_descriptor;
    stm_se_uncompressed_frame_metadata_t descriptor;

    pr_info("%s: Testing Pass Cases - You should not expect errors here\n", __func__);

    // Allocate Memory Buffer
    result = AllocateBuffer(TEST_BUFFER_SIZE, &original_raw_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        goto allocation_fail;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    // Fill buffer
    memset(original_raw_video_frame.virtual, 0x80, original_raw_video_frame.size);
#ifdef DUMP_INPUT_BUFFER
    print_hex_dump(KERN_INFO, "INPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   original_raw_video_frame.virtual, min((unsigned) 64, original_raw_video_frame.size), true);
#endif

    memset(&original_descriptor, 0, sizeof(descriptor));
    original_descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    original_descriptor.video.frame_rate.framerate_num                        = VIDEO_FRAME_RATE_NUM;
    original_descriptor.video.frame_rate.framerate_den                        = VIDEO_FRAME_RATE_DEN;
    original_descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    original_descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    original_descriptor.video.video_parameters.colorspace                     = (VIDEO_WIDTH > SD_WIDTH ? STM_SE_COLORSPACE_SMPTE170M : STM_SE_COLORSPACE_SMPTE240M);
    original_descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    original_descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    original_descriptor.video.pitch                                           = VIDEO_PITCH;
    original_descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    original_descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    original_descriptor.video.surface_format                                  = VIDEO_SURFACE_FORMAT;
    original_descriptor.video.vertical_alignment                              = VERTICAL_ALIGNMENT;
    original_descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    original_descriptor.native_time   = jiffies / 10;
    original_descriptor.system_time   = jiffies;
    original_descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY) / sizeof(GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.pixel_aspect_ratio_numerator = GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY[i][0];
        descriptor.video.video_parameters.pixel_aspect_ratio_denominator = GOOD_VIDEO_PREPROC_PIXEL_ASPECT_RATIO_ONLY[i][1];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY) / sizeof(GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[i][0];
        descriptor.video.video_parameters.height = GOOD_VIDEO_PREPROC_INPUT_RESOLUTION_ONLY[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY) / sizeof(GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.frame_rate.framerate_num = GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[i][0];
        descriptor.video.frame_rate.framerate_den = GOOD_VIDEO_PREPROC_INPUT_FRAMERATE_ONLY[i][1];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_COLORSPACE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_COLORSPACE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.colorspace = GOOD_VIDEO_PREPROC_COLORSPACE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_MEDIA_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_MEDIA_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.media = GOOD_VIDEO_PREPROC_MEDIA_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_PICTURE_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.picture_type = GOOD_VIDEO_PREPROC_PICTURE_TYPE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_SCAN_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_SCAN_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.scan_type = GOOD_VIDEO_PREPROC_SCAN_TYPE_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }


    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.surface_format = GOOD_VIDEO_PREPROC_SURFACE_FORMAT_TYPE_ONLY[i];
        descriptor.video.pitch = descriptor.video.video_parameters.width * GetBitPerPixel(descriptor.video.surface_format) / 8;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_DISCONTINUITY_ONLY) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_DISCONTINUITY_ONLY[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.discontinuity = GOOD_VIDEO_PREPROC_DISCONTINUITY_ONLY[i];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (descriptor.discontinuity & STM_SE_DISCONTINUITY_EOS)
        {
            result = PullCompressedEncode(&gVideoTest[0].Sink);
            if (result < 0)
            {
                pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
                goto free_buffer;
            }
        }
        else
        {
            pr_info("PullCompressedEncode is not needed for %u (%s)\n", descriptor.discontinuity, StringifyDiscontinuity(descriptor.discontinuity));
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[%d]\n", i);

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.aspect_ratio = GOOD_VIDEO_PREPROC_STORAGE_ASPECT_RATIO_VS_RESOLUTION[i];
        descriptor.video.video_parameters.width = GOOD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[i][0];
        descriptor.video.video_parameters.height = GOOD_VIDEO_PREPROC_RESOLUTION_VS_STORAGE_ASPECT_RATIO[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto free_buffer;
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY) / sizeof(GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[%d]\n", i);
        descriptor = original_descriptor;

        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        descriptor.discontinuity = GOOD_VIDEO_PREPROC_DISCONTINUITY_VS_BUFFER[i];

        raw_video_frame = original_raw_video_frame;
        if (GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_VIRTUAL)
        {
            raw_video_frame.virtual = (void *)GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][1];
        }

        if (GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_PHYSICAL)
        {
            raw_video_frame.physical = GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][2];
        }

        if (GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][0] & RAW_VIDEO_FRAME_SIZE)
        {
            raw_video_frame.size = GOOD_VIDEO_PREPROC_BUFFER_VS_DISCONTINUITY[i][3];
        }

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], raw_video_frame.virtual, raw_video_frame.physical, raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        if (descriptor.discontinuity & STM_SE_DISCONTINUITY_EOS)
        {
            result = PullCompressedEncode(&gVideoTest[0].Sink);
            if (result < 0)
            {
                pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
                goto free_buffer;
            }
        }
        else
        {
            pr_info("PullCompressedEncode is not needed for %u (%s)\n", descriptor.discontinuity, StringifyDiscontinuity(descriptor.discontinuity));
        }

        if (PullCompressedEncode(&gVideoTest[0].Sink) != -1)
        {
            pr_err("Pull data is not empty\n");
            result = -1;
            goto free_buffer;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    // Free BPA Memory
    result = FreeBuffer(&compressed_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&original_raw_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    pr_info("%s: Pass testing complete (%d)\n", __func__, failures);
    return failures;

free_buffer:
    if (deinit_stream(pContext)) {};
init_stream_fail:
deinit_stream_fail:
    if (FreeBuffer(&compressed_video_frame)) {};
allocation_fail:
    if (FreeBuffer(&original_raw_video_frame)) {};

    return result;
}

// These tests are expected to fail ... returning success is a failure!
static int inject_fail(EncoderContext *pContext)
{
    int i = 0;
    int result = 0;
    int failures = 0;
    bpa_buffer original_raw_video_frame;
    bpa_buffer compressed_video_frame;
    stm_se_uncompressed_frame_metadata_t original_descriptor;
    stm_se_uncompressed_frame_metadata_t descriptor;
    stm_se_picture_resolution_t Resolution;
    stm_se_framerate_t          Framerate;

    pr_info("%s: Testing Failure Cases - You could expect errors here\n", __func__);

    // Allocate Memory Buffer
    result = AllocateBuffer(TEST_BUFFER_SIZE, &original_raw_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        goto allocation_fail;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    // Fill buffer
    memset(original_raw_video_frame.virtual, 0x80, original_raw_video_frame.size);
#ifdef DUMP_INPUT_BUFFER
    print_hex_dump(KERN_INFO, "INPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   original_raw_video_frame.virtual, min((unsigned) 64, original_raw_video_frame.size), true);
#endif

    memset(&original_descriptor, 0, sizeof(descriptor));
    original_descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    original_descriptor.video.frame_rate.framerate_num                        = VIDEO_FRAME_RATE_NUM;
    original_descriptor.video.frame_rate.framerate_den                        = VIDEO_FRAME_RATE_DEN;
    original_descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    original_descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    original_descriptor.video.video_parameters.colorspace                     = (VIDEO_WIDTH > SD_WIDTH ? STM_SE_COLORSPACE_SMPTE170M : STM_SE_COLORSPACE_SMPTE240M);
    original_descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    original_descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    original_descriptor.video.pitch                                           = VIDEO_PITCH;
    original_descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    original_descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    original_descriptor.video.surface_format                                  = VIDEO_SURFACE_FORMAT;
    original_descriptor.video.vertical_alignment                              = VERTICAL_ALIGNMENT;
    original_descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    original_descriptor.native_time   = jiffies / 10;
    original_descriptor.system_time   = jiffies;
    original_descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO) / sizeof(int32_t)) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, BAD_VIDEO_PREPROC_DISPLAY_VS_STORAGE_ASPECT_RATIO[i]);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.aspect_ratio = BAD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[i];
        descriptor.video.video_parameters.width = GetWidth(descriptor.video.video_parameters.aspect_ratio);
        descriptor.video.video_parameters.height = GetHeight(descriptor.video.video_parameters.aspect_ratio);
        descriptor.video.pitch = descriptor.video.video_parameters.width * GetBitPerPixel(descriptor.video.surface_format) / 8;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION) / sizeof(BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        Resolution.width = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][0];
        Resolution.height = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &Resolution);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[i][0];
        descriptor.video.video_parameters.height = BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING) / sizeof(BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, BAD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING_VS_WINDOW_OF_INTEREST[i]);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = VIDEO_PREPROC_IN_MAX_WIDTH;
        descriptor.video.video_parameters.height = VIDEO_PREPROC_IN_MAX_HEIGHT;
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
        descriptor.video.window_of_interest.x = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][0];
        descriptor.video.window_of_interest.y = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][1];
        descriptor.video.window_of_interest.width = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][2];
        descriptor.video.window_of_interest.height = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][3];
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION) / sizeof(BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, STM_SE_CTRL_VALUE_APPLY);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        Resolution.width = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][0];
        Resolution.height = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &Resolution);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = VIDEO_PREPROC_IN_MAX_WIDTH;
        descriptor.video.video_parameters.height = VIDEO_PREPROC_IN_MAX_HEIGHT;
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
        descriptor.video.window_of_interest.x = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][0];
        descriptor.video.window_of_interest.y = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][1];
        descriptor.video.window_of_interest.width = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][2];
        descriptor.video.window_of_interest.height = BAD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][3];
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE) / sizeof(BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[0])) ; i++)
    {
        pr_info("BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        Framerate.framerate_num = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[i][0];
        Framerate.framerate_den = BAD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &Framerate);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.frame_rate.framerate_num = BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[i][0];
        descriptor.video.frame_rate.framerate_den = BAD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[i][1];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result >= 0)
        {
            pr_err("Error: %s Expected failure on testing set invalid inject frame\n", __func__);
            failures++;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    // Free BPA Memory
    result = FreeBuffer(&compressed_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&original_raw_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    pr_info("%s: Failure testing complete (%d)\n", __func__, failures);
    return failures;

set_control_fail:
    if (deinit_stream(pContext)) {};
init_stream_fail:
deinit_stream_fail:
    if (FreeBuffer(&compressed_video_frame)) {};
allocation_fail:
    if (FreeBuffer(&original_raw_video_frame)) {};

    return result;
}

// These tests are expected to pass ... returning success is a good thing
static int inject_pass(EncoderContext *pContext)
{
    int i = 0;
    int failures = 0;
    int result = 0;
    bpa_buffer original_raw_video_frame;
    bpa_buffer compressed_video_frame;
    stm_se_uncompressed_frame_metadata_t original_descriptor;
    stm_se_uncompressed_frame_metadata_t descriptor;
    stm_se_picture_resolution_t Resolution;
    stm_se_framerate_t          Framerate;

    pr_info("%s: Testing Pass Cases - You should not expect errors here\n", __func__);

    // Allocate Memory Buffer
    result = AllocateBuffer(TEST_BUFFER_SIZE, &original_raw_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the sequence (%d)\n", __func__, TEST_BUFFER_SIZE, result);
        return result;
    }

    result = AllocateBuffer(COMPRESSED_BUFFER_SIZE, &compressed_video_frame);
    if (result < 0)
    {
        pr_err("%s: Failed to allocate a physical buffer of size %d bytes to store the stream (%d)\n", __func__, COMPRESSED_BUFFER_SIZE, result);
        goto allocation_fail;
    }

    memset(compressed_video_frame.virtual, 0, COMPRESSED_BUFFER_SIZE);
    gVideoTest[0].Sink.m_Buffer = (unsigned char *)compressed_video_frame.virtual;
    gVideoTest[0].Sink.m_BufferLen = compressed_video_frame.size;

    // Fill buffer
    memset(original_raw_video_frame.virtual, 0x80, original_raw_video_frame.size);
#ifdef DUMP_INPUT_BUFFER
    print_hex_dump(KERN_INFO, "INPUT BUFFER DUMP IN HEXADECIMAL: ", DUMP_PREFIX_OFFSET, 16, 1,
                   original_raw_video_frame.virtual, min((unsigned) 64, original_raw_video_frame.size), true);
#endif

    memset(&original_descriptor, 0, sizeof(descriptor));
    original_descriptor.media = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    original_descriptor.video.frame_rate.framerate_num                        = VIDEO_FRAME_RATE_NUM;
    original_descriptor.video.frame_rate.framerate_den                        = VIDEO_FRAME_RATE_DEN;
    original_descriptor.video.video_parameters.width                          = VIDEO_WIDTH;
    original_descriptor.video.video_parameters.height                         = VIDEO_HEIGHT;
    original_descriptor.video.video_parameters.colorspace                     = (VIDEO_WIDTH > SD_WIDTH ? STM_SE_COLORSPACE_SMPTE170M : STM_SE_COLORSPACE_SMPTE240M);
    original_descriptor.video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_4_BY_3;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_numerator   = 1;
    original_descriptor.video.video_parameters.pixel_aspect_ratio_denominator = 1;
    original_descriptor.video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    original_descriptor.video.pitch                                           = VIDEO_PITCH;
    original_descriptor.video.window_of_interest                              = (stm_se_picture_rectangle_t) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    original_descriptor.video.picture_type                                    = STM_SE_PICTURE_TYPE_UNKNOWN;
    original_descriptor.video.surface_format                                  = VIDEO_SURFACE_FORMAT;
    original_descriptor.video.vertical_alignment                              = VERTICAL_ALIGNMENT;
    original_descriptor.native_time_format = TIME_FORMAT_PTS;
    // Fake Data that will change
    original_descriptor.native_time   = jiffies / 10;
    original_descriptor.system_time   = jiffies;
    original_descriptor.discontinuity = STM_SE_DISCONTINUITY_CONTINUOUS;

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO) / sizeof(int32_t)) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO, GOOD_VIDEO_PREPROC_DISPLAY_VS_STORAGE_ASPECT_RATIO[i]);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.aspect_ratio = GOOD_VIDEO_PREPROC_STORAGE_VS_DISPLAY_ASPECT_RATIO[i];
        descriptor.video.video_parameters.width = GetWidth(descriptor.video.video_parameters.aspect_ratio);
        descriptor.video.video_parameters.height = GetHeight(descriptor.video.video_parameters.aspect_ratio);
        descriptor.video.pitch = descriptor.video.video_parameters.width * GetBitPerPixel(descriptor.video.surface_format) / 8;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto pull_fail;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION) / sizeof(GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        Resolution.width = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][0];
        Resolution.height = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &Resolution);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[i][0];
        descriptor.video.video_parameters.height = GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_RESOLUTION[i][1];
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto pull_fail;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING) / sizeof(GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, GOOD_VIDEO_PREPROC_INPUT_WINDOW_CROPPING_VS_WINDOW_OF_INTEREST[i]);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = VIDEO_PREPROC_IN_MAX_WIDTH;
        descriptor.video.video_parameters.height = VIDEO_PREPROC_IN_MAX_HEIGHT;
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
        descriptor.video.window_of_interest.x = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][0];
        descriptor.video.window_of_interest.y = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][1];
        descriptor.video.window_of_interest.width = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][2];
        descriptor.video.window_of_interest.height = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_INPUT_WINDOW_CROPPING[i][3];
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto pull_fail;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION) / sizeof(GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        result = stm_se_encode_stream_set_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING, STM_SE_CTRL_VALUE_APPLY);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid control\n", __func__);
            goto set_control_fail;
        }

        Resolution.width = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][0];
        Resolution.height = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_RESOLUTION[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION, &Resolution);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.video_parameters.width = VIDEO_PREPROC_IN_MAX_WIDTH;
        descriptor.video.video_parameters.height = VIDEO_PREPROC_IN_MAX_HEIGHT;
        descriptor.video.pitch = descriptor.video.video_parameters.width;
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
        descriptor.video.window_of_interest.x = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][0];
        descriptor.video.window_of_interest.y = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][1];
        descriptor.video.window_of_interest.width = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][2];
        descriptor.video.window_of_interest.height = GOOD_VIDEO_PREPROC_WINDOW_OF_INTEREST_VS_OUTPUT_RESOLUTION[i][3];
        descriptor.video.video_parameters.aspect_ratio = STM_SE_ASPECT_RATIO_UNSPECIFIED;

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        result = PullCompressedEncode(&gVideoTest[0].Sink);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing pull compressed frame\n", __func__);
            goto pull_fail;
        }

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    for (i = 0 ; i < (sizeof(GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE) / sizeof(GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[0])) ; i++)
    {
        pr_info("GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[%d]\n", i);
        result = init_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on init_stream\n", __func__);
            goto init_stream_fail;
        }

        Framerate.framerate_num = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[i][0];
        Framerate.framerate_den = GOOD_VIDEO_PREPROC_OUTPUT_VS_INPUT_FRAMERATE[i][1];
        result = stm_se_encode_stream_set_compound_control(pContext->video_stream[0], STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE, &Framerate);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing set valid compound control\n", __func__);
            goto set_control_fail;
        }

        descriptor = original_descriptor;
        descriptor.video.frame_rate.framerate_num = GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[i][0];
        descriptor.video.frame_rate.framerate_den = GOOD_VIDEO_PREPROC_INPUT_VS_OUTPUT_FRAMERATE[i][1];

        result = stm_se_encode_stream_inject_frame(pContext->video_stream[0], original_raw_video_frame.virtual, original_raw_video_frame.physical, original_raw_video_frame.size, descriptor);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on testing valid inject frame\n", __func__);
            failures++;
        }

        do
        {
            pr_info("Pull data\n");
        }
        while (PullCompressedEncode(&gVideoTest[0].Sink) == 0);

        result = deinit_stream(pContext);
        if (result < 0)
        {
            pr_err("Error: %s Expected pass on deinit_stream\n", __func__);
            goto deinit_stream_fail;
        }
    }

    // Free BPA Memory
    result = FreeBuffer(&compressed_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free stream buffer (%d)\n", __func__, result);
    }

    gVideoTest[0].Sink.m_Buffer = NULL;
    gVideoTest[0].Sink.m_BufferLen = 0;

    result = FreeBuffer(&original_raw_video_frame);
    if (result < 0)
    {
        pr_err("Error: %s Failed to free sequence buffer (%d)\n", __func__, result);
    }

    pr_info("%s: Pass testing complete (%d)\n", __func__, failures);
    return failures;

pull_fail:
set_control_fail:
    if (deinit_stream(pContext)) {};
init_stream_fail:
deinit_stream_fail:
    if (FreeBuffer(&compressed_video_frame)) {};
allocation_fail:
    if (FreeBuffer(&original_raw_video_frame)) {};

    return result;
}

static int video_inject_frame_test(EncoderTestArg *arg)
{
    EncoderContext context = {0};
    EncoderContext *pContext = &context;

    int result = 0;
    int failures = 0;

    memset(&gVideoTest, 0, sizeof(gVideoTest));

    // Create an Encode Object
    result = stm_se_encode_new("Encode0", &pContext->encode);
    if (result < 0)
    {
        pr_err("%s: Failed to create a new Encode\n", __FUNCTION__);
        return result;
    }

    // Perform metadata testing
    result = inject_only_fail(pContext);
    if (result < 0)
    {
        pr_err("Error: %s Expected pass on inject_only_fail\n", __func__);
        goto inject_fail;
    }
    failures += result;

    result = inject_only_pass(pContext);
    if (result < 0)
    {
        pr_err("Error: %s Expected pass on inject_only_pass\n", __func__);
        goto inject_fail;
    }
    failures += result;

    // Perform control and metadata testing
    result = inject_fail(pContext);
    if (result < 0)
    {
        pr_err("Error: %s Expected pass on inject_fail\n", __func__);
        goto inject_fail;
    }
    failures += result;

    result = inject_pass(pContext);
    if (result < 0)
    {
        pr_err("Error: %s Expected pass on inject_pass\n", __func__);
        goto inject_fail;
    }
    failures += result;

    result = stm_se_encode_delete(pContext->encode);
    if (result < 0)
    {
        pr_err("%s: stm_se_encode_delete returned %d\n", __FUNCTION__, result);
        return result;
    }

    return failures;

inject_fail:
    if (stm_se_encode_delete(pContext->encode)) {};

    return result;
}

/*** Only 40 Chars will be displayed ***/
ENCODER_TEST(video_inject_frame_test, "Test Video Inject Frame", BASIC_TEST);
