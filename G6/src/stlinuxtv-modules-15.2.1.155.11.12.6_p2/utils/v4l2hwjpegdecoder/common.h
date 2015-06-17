/*
 * common.h, common definition for DFB and MMAP jpeg decoding applications
 *
 * Copyright (C) 2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */


#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>

#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#define false   0
#define true    1
#endif
#endif

#define FAIL    false
#define SUCCESS true

#define V4L2_HWJPEG_DRIVER_NAME     "JPEG Decoder"
#define V4L2_DISPLAY_DRIVER_NAME    "Planes"
#define V4L2_DISPLAY_CARD_NAME      "STMicroelectronics"



#define DEVICE_TREE_PATH            "/sys/bus/platform/devices/"
#define LOG_FILE                    "/tmp/crc32c-list.dump"
#define PERF_BENCH_FILE             "/tmp/perf-bench.dump"

/* Local definitions for GAM header files */
/* -------------------------------------- */
#define STM_ENDIANESS_LITTLE       0x0
#define STM_ENDIANESS_BIG          0x1
#define STM_GAMFILE_HEADER_SIZE    0x6
#define STM_GAMFILE_CK_HEADER_SIZE 0x8
#define STM_GAMFILE_SIGNATURE      0x444F
#define STM_422FILE_SIGNATURE      0x422F
#define STM_420FILE_SIGNATURE      0x420F
#define STM_GAMFILE_PROPERTIES_LSB 0x01
#define STM_GAMFILE_PROPERTIES     0x00
#define STM_GAMFILE_PROPERTIES_FULL_ALPHA    0xff /* only for alpha formats */
#define STM_GAMFILE_PROPERTIES_SQUARE_PIXEL  0x01 /* for macroblock */
#define STM_GAMFILE_PROPERTIES_PAL16_9       0x03 /* for macroblock */
#define STM_GAMFILE_PROPERTIES_PAL4_3        0x08 /* for macroblock */
#define STM_GAMFILE_PROPERTIES_NTSC16_9      0x06 /* for macroblock */
#define STM_GAMFILE_PROPERTIES_NTSC4_3       0x0c /* for macroblock */
#define STM_GAMFILE_PROPERTIES_BYTESWAP      0x10 /* for macroblock */

#define STM_422FILE_PROPERTIES    (STM_GAMFILE_PROPERTIES_SQUARE_PIXEL \
                                       | STM_GAMFILE_PROPERTIES_BYTESWAP)
#define STM_420FILE_PROPERTIES    (STM_GAMFILE_PROPERTIES_PAL16_9 \
                                       | STM_GAMFILE_PROPERTIES_BYTESWAP)

#define unlikely(x)     __builtin_expect(!!(x),0)
#define likely(x)       __builtin_expect(!!(x),1)

#define NUM_FORMATS sizeof(fmttable) / sizeof(struct fmt)

#define V4L2_PIX_FMT_NV16M   v4l2_fourcc('N', 'M', '1', '6') /* 16  Y/CbCr 4:2:2  */

extern bool verbose_enabled, crc_enabled, dump_enabled, custom_filename;

typedef struct GamPictureHeader_s
{
    unsigned short     HeaderSize;
    unsigned short     Signature;
    unsigned short     Type;
    unsigned short     Properties;
    unsigned int       PictureWidth;     /* With 4:2:0 R2B this is a OR between Pitch and Width */
    unsigned int       PictureHeight;
    unsigned int       LumaSize;
    unsigned int       ChromaSize;
} GamPictureHeader_t;

/* Known bitmap types */
enum gamfile_type
{
    STM_GAMFILE_NV12        = 0x00B0,
    STM_GAMFILE_NV12M       = 0x00FF,
    STM_GAMFILE_NV16        = 0x00B1,
    STM_GAMFILE_NV16M       = 0x00FE,
    STM_GAMFILE_NV24        = 0x00FD,
};

struct fmt
{
    unsigned bppsize;
    unsigned int fourcc;
    struct {
        unsigned num;
        unsigned denom;
    } sampling;
    unsigned chromasampling;
    char* desc;
    char* shortdesc;
};

static struct fmt fmttable[] =
{
    [0] = {
        .bppsize = 0,
        .fourcc = 0,
        .sampling = {0, 0},
        .desc = 0,
        .shortdesc = 0
    },
    [1] = {
        .bppsize = 12,
        .fourcc = V4L2_PIX_FMT_NV12,
        .sampling = {1, 2},
        .desc = "Non-contiguous two planes YCbCr 4:2:0",
        .shortdesc = "420"
    },
    [2] = {
        .bppsize = 16,
        .fourcc = V4L2_PIX_FMT_NV16,
        .sampling = {1, 1},
        .desc = "Non-contiguous two planes YCbCr 4:2:2",
        .shortdesc = "422"
    },
    [3] = {
        .bppsize = 24,
        .fourcc = V4L2_PIX_FMT_NV24,
        .sampling = {2, 1},
        .desc = "Non-contiguous two planes YCbCr 4:4:4",
        .shortdesc = "444"
    }
};

/*************************************************************************
 * Function    : gamma_types
 * Description : Get the STM GAM type from the V4L2 format
 *************************************************************************/
static unsigned int gamma_types(unsigned int fmt)
{
    unsigned int gam_type=0;
    switch (fmt) {
        case V4L2_PIX_FMT_NV12:
            gam_type = STM_GAMFILE_NV12;
            break;

        case V4L2_PIX_FMT_NV12M:
            gam_type = STM_GAMFILE_NV12M;
            break;

        case V4L2_PIX_FMT_NV16:
            gam_type = STM_GAMFILE_NV16;
            break;

        case V4L2_PIX_FMT_NV16M:
            gam_type = STM_GAMFILE_NV16M;
            break;

        case V4L2_PIX_FMT_NV24:
            gam_type = STM_GAMFILE_NV24;
            break;

        default:
            gam_type = STM_GAMFILE_NV12;
            break;
    }

    return gam_type;
}

/*************************************************************************
 * Function    : getfmt
 * Description : Get the format from the fourcc code
 *************************************************************************/
static struct fmt* getfmt( unsigned int fourcc)
{
    int i;
    for (i = 0; i < NUM_FORMATS; i++) {
        if (fmttable[i].fourcc == fourcc) {
            return &fmttable[i];
        }
    }
    return NULL;
}

void get_supported_formats( int videofd);
void stream_off( int videofd);
bool dump_crc( char* fileName, int videofd,
               struct v4l2_format srcFormat,
               struct v4l2_format dstFormat);
bool dump_GAM ( char* fileName,
                char* destFileName,
                void* dstBuffer,
                struct v4l2_format dstFormat);
void verbose(const char* format, ...);
void log_fill_rate( unsigned long long displayTime,
                    int src_width,
                    int src_height,
                    int dst_width,
                    int dst_height,
                    unsigned int pixelformat);
#endif
