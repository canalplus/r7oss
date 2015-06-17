/*
 * common.c, common functions for DFB and MMAP jpeg decoding applications
 *
 * Copyright (C) 2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */



#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>

#include <fcntl.h>

#include <linux/videodev2.h>

#include "common.h"

#ifndef __cplusplus
#ifndef bool
#define bool    unsigned char
#define false   0
#define true    1
#endif
#endif

/*****************************************************************************
 * Function    : verbose
 * Description : A wrapper for printf to display debug information only when
 *               verbose is enabled
 *****************************************************************************/
void verbose(const char* format, ...)
{
    va_list args;

    if (verbose_enabled == true) {
        printf( "DEBUG :");
        va_start( args, format);
        vprintf( format, args);
        va_end( args);
        printf( "\n");
    }
}

/*****************************************************************************
 * Function    : get_supported_formats
 * Description : Display the Input and Output supported by the jpeg vid device
 *****************************************************************************/
void get_supported_formats( int videofd)
{
    int i, x;
    struct v4l2_fmtdesc fmtdesc;

    if (verbose_enabled) {
        /* Get supported Input Formats */
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        i = 0;
        do {
            fmtdesc.index = i;
            if ((x=ioctl(videofd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0) {
                verbose("input: %s\n", fmtdesc.description);
            }
            i++;
        } while ( x==0);

        /* Get supported Output Formats */
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        i = 0;
        do {
            fmtdesc.index = i;
            if ((x=ioctl(videofd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0) {
                verbose("output: %s\n", fmtdesc.description);
            }
            i++;
        } while ( x==0);
    }
}

/***********************************************************************
 * Function    : stream_off
 * Description : disable streaming on the jpeg video device and close it
 ***********************************************************************/
void stream_off( int videofd)
{
    int type;

    if (videofd > 0) {
        /* Deinitialization sequence */
        verbose("VIDIOC_STREAMOFF (src)\n");
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl(videofd, VIDIOC_STREAMOFF, &type) < 0) {
            printf("VIDIOC_STREAMOFF (src) failed!\n");
        }
        verbose("VIDIOC_STREAMOFF (dst)\n");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(videofd, VIDIOC_STREAMOFF, &type) < 0) {
            printf("VIDIOC_STREAMOFF (dst) failed!\n");
        }
        /* Close the video device*/
        close(videofd);
    }

    return;
}

/***********************************************************************
 * Function    : dump_crc
 * Description : Get the CRC of Luma and Chroma buffers from the driver
 *               and write them into the test report file
 ***********************************************************************/
bool dump_crc( char* fileName, int videofd,
               struct v4l2_format srcFormat,
               struct v4l2_format dstFormat)
{
    FILE *luma_file, *chroma_file, *crc_log_file;
    char luma_crc[128], chroma_crc[128];
    char buffer[256];

    char* luma_path = "/sys/kernel/debug/stm_c8jpg/luma_crc32c";
    char* chroma_path = "/sys/kernel/debug/stm_c8jpg/chroma_crc32c";

    struct fmt *outputfmt;

    outputfmt = getfmt(dstFormat.fmt.pix.pixelformat);
    if (!outputfmt) {
        printf("couldn't lookup output format!\n");
        stream_off(videofd);
        return FAIL;
    }

    verbose("output picture is %dx%d (pitch: %d) %s\n",
           dstFormat.fmt.pix.width,
           dstFormat.fmt.pix.height,
           dstFormat.fmt.pix.bytesperline,
           outputfmt->desc);

    if (crc_enabled) {
        verbose("Calculating CRC \n");

        luma_file = fopen(luma_path, "r");
        chroma_file = fopen(chroma_path, "r");

        if (luma_file && chroma_file) {
            int len;

            fgets(luma_crc, sizeof(luma_crc), luma_file);
            fgets(chroma_crc, sizeof(chroma_crc), chroma_file);

            len = strlen(luma_crc);
            luma_crc[len - 1] = 0;
            len = strlen(chroma_crc);
            chroma_crc[len - 1] = 0;

            /* Log crc values*/
            crc_log_file = fopen(LOG_FILE, "a+w");

            if (crc_log_file) {
                sprintf(buffer, "%s, %d, %d, %s, %d, %d, %s, %s\n",
                        basename(fileName),
                        srcFormat.fmt.pix.width,
                        srcFormat.fmt.pix.height,
                        ((dstFormat.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)? "4:2:0" :((dstFormat.fmt.pix.pixelformat == V4L2_PIX_FMT_NV16)? "4:2:2" : "4:4:4")),
                        dstFormat.fmt.pix.width,
                        dstFormat.fmt.pix.height,
                        luma_crc, chroma_crc);
                fputs(buffer, crc_log_file);
                fclose(crc_log_file);
            }

            fclose(luma_file);
            fclose(chroma_file);
        } else {
            printf("Error reading CRC filesystem ! \n");
        }
    }
    return SUCCESS;
}

/*************************************************************************
 * Function    : log_fill_rate
 * Description : Logs the fill rate of the driver
 *************************************************************************/
void log_fill_rate( unsigned long long displayTime,
                    int src_width,
                    int src_height,
                    int dst_width,
                    int dst_height,
                    unsigned int pixelformat)
{
    FILE *decode_time_file = NULL;
    FILE *perf_bench_log_file = NULL;
    char decodetime[128];
    char buffer[256];
    long long fillTime = 0;

    char* perf_file_path = "/sys/kernel/debug/stm_c8jpg/decode_time";

    decode_time_file = fopen(perf_file_path, "r");

    if (decode_time_file) {
        fgets(decodetime, sizeof(decodetime), decode_time_file);
        sscanf(decodetime, "%llu", &fillTime);
        verbose("decodetime = %llu usec\n", fillTime);
        fillTime += displayTime;

        perf_bench_log_file = fopen( PERF_BENCH_FILE, "a+w");

        if (perf_bench_log_file) {
            if (src_height != dst_height || src_width != dst_width) {
                sprintf(buffer, "%dx%d pix %s (resize to %dx%d) in %fs >> %f Mpixels/sec\n",
                        src_width,
                        src_height,
                        ((pixelformat == V4L2_PIX_FMT_NV12)? "4:2:0" :((pixelformat == V4L2_PIX_FMT_NV16)? "4:2:2" : "4:4:4")),
                        dst_width,
                        dst_height,
                        (float)fillTime/1000000.0f,
                        (src_width * src_height)/(float)fillTime);
            } else {
                sprintf(buffer, "%dx%d pix %s (no resize) in %fs >> %f Mpixels/sec\n",
                        src_width,
                        src_height,
                        ((pixelformat == V4L2_PIX_FMT_NV12)? "4:2:0" :((pixelformat == V4L2_PIX_FMT_NV16)? "4:2:2" : "4:4:4")),
                        (float)fillTime/1000000.0f,
                        (src_width * src_height)/(float)fillTime);
            }
            fputs( buffer, perf_bench_log_file);
            fclose(perf_bench_log_file);
        } else {
            printf("Unable to open %s file to log the fill rate !\n", PERF_BENCH_FILE);
        }
        fclose(decode_time_file);
    } else {
        printf("DECODE TIME filesystem not found ! Please enable it first.\n");
    }
}

/*************************************************************************
 * Function    : dump_GAM
 * Description : Dump the decoded jpeg to a GAM file
 *************************************************************************/
bool dump_GAM ( char* fileName,
                char* destFileName,
                void* dstBuffer,
                struct v4l2_format dstFormat)
{
    GamPictureHeader_t header_file;
    char filename[256];
    int gamfd = 0;
    struct fmt *outputfmt;

    if (dump_enabled) {
        outputfmt = getfmt(dstFormat.fmt.pix.pixelformat);
        if (!outputfmt) {
            printf("couldn't lookup output format!\n");
            return FAIL;
        }

        memset(&header_file, 0, sizeof (header_file));

        if (custom_filename == true) {
            sprintf(filename, "%s.gam", destFileName);
       } else {
           sprintf(filename, "dump-%04dx%04d-%s.gam",
               dstFormat.fmt.pix.width,
               dstFormat.fmt.pix.height,
               outputfmt->shortdesc);
       }
       if ((gamfd = open(filename, O_CREAT | O_WRONLY, 0664)) < 0) {
            printf("couldn't open file for dumping the output buffers!\n");
            return FAIL;
       } else {
            verbose("file %s open for dumping \n", filename);
       }

       if (gamfd > 0) {
           header_file.PictureWidth  = dstFormat.fmt.pix.width;
           header_file.PictureHeight = dstFormat.fmt.pix.height;
           header_file.HeaderSize    = STM_GAMFILE_HEADER_SIZE;
           header_file.Signature     = STM_GAMFILE_SIGNATURE;
           header_file.Properties    = STM_GAMFILE_PROPERTIES;
           header_file.Type          = gamma_types(dstFormat.fmt.pix.pixelformat);
           header_file.LumaSize      = dstFormat.fmt.pix.sizeimage;
           header_file.ChromaSize    = 0;

           /* Build the GAM header */
           switch ((enum gamfile_type) header_file.Type) {
               case STM_GAMFILE_NV12:
                   header_file.PictureWidth  = ((dstFormat.fmt.pix.bytesperline << 16) | dstFormat.fmt.pix.width);
                   header_file.LumaSize      = dstFormat.fmt.pix.sizeimage * 4;
                   header_file.LumaSize     /= 6;
                   header_file.ChromaSize    = header_file.LumaSize / 2;
                   header_file.Signature     = STM_420FILE_SIGNATURE;
                   header_file.Properties    = STM_420FILE_PROPERTIES;
                   break;

               case STM_GAMFILE_NV16:
                   header_file.LumaSize      = header_file.ChromaSize = dstFormat.fmt.pix.sizeimage / 2;
                   header_file.Signature     = STM_422FILE_SIGNATURE;
                   header_file.Properties    = STM_422FILE_PROPERTIES;
                   break;

               case STM_GAMFILE_NV24:
                   header_file.ChromaSize    = dstFormat.fmt.pix.sizeimage * 4;
                   header_file.ChromaSize   /= 6;
                   header_file.LumaSize      = header_file.ChromaSize / 2;
                   header_file.Signature     = STM_GAMFILE_SIGNATURE;
                   header_file.Properties    = STM_GAMFILE_PROPERTIES;
                   break;

               case STM_GAMFILE_NV12M:
               case STM_GAMFILE_NV16M:
               default:
                   printf("GAM format %.8x not supported! \n",
                       (enum gamfile_type) header_file.Type);
                   return FAIL;
                   break;

           }

           /* Write the GAM header to the file */
           verbose("Writing GAM header \n");
           write(gamfd, (void*) &header_file, sizeof(header_file));
            /* Write the GAM buffer to the file */
           verbose("luma/chroma size: %d\n", dstFormat.fmt.pix.sizeimage);
           write(gamfd, (void*)dstBuffer, dstFormat.fmt.pix.sizeimage);
           close(gamfd);
       }
    }

    return SUCCESS;
}
