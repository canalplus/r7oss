/*
 * v4l2-decode.c, a test application for decoding JPEGs using V4L2
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

#include <fcntl.h>

#include <linux/videodev2.h>

#include "linux/include/linux/stmvout.h"
#include "utils/v4l2_helper.h"

#include "common.h"

#include <unistd.h>
#include <fcntl.h>

/* Parameters of the app execution */
bool dump_enabled = false,
    display_enabled = false,
    crc_enabled = false,
    verbose_enabled = false,
    cropping_enabled = false,
    resize_enabled = false,
    custom_filename = false,
    hom_enabled = false,
    perf_bench_required = false;

/***********************************************************************
 * Function    : v4l2_clean_up_and_exit
 * Description : unmap allocated memory, close jpeg video device and exit
 ***********************************************************************/
void v4l2_clean_up_and_exit( int videofd,
                        void* srcBuffer,
                        int srcSize,
                        void* dstBuffer,
                        int dstSize)
{
    /* Close the video device*/
    if (videofd > 0) {
        close(videofd);
    }
    /* Unmap buffers from memory */
    if (srcBuffer) {
        munmap(srcBuffer, srcSize);
    }
    if (dstBuffer) {
        munmap(dstBuffer, dstSize);
    }
    verbose("Done.\n");

    exit (0);
}


/***************************************************************
 * Function    : v4l2_display_GAM
 * Description : Display a GAM buffer through 'Main-VID'
 ***************************************************************/
void v4l2_display_GAM( struct v4l2_format src_format, struct v4l2_format format, void* gamBuffer)
{
    int           v4lfd;
    const char   *plane_name = "Main-VID";
    int           progressive=1;
    int           iterations =500;
    int           delay=1;
    int           i=0;

    unsigned long long frameduration;

    struct v4l2_format         fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_crop           crop;
    v4l2_std_id                stdid = V4L2_STD_UNKNOWN;
    struct v4l2_standard       standard;
    struct v4l2_cropcap        cropcap;
    struct v4l2_buffer         buffer[20];

    int                        bufferNb = 3;
    struct timespec            currenttime;
    struct timeval             ptime;

    if (display_enabled) {

        /* Open the requested V4L2 device */
        if((v4lfd = v4l2_open_by_name(V4L2_DISPLAY_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR)) < 0) {
            perror("Unable to open video device");
            goto exit_disp;
        }

        v4l2_list_outputs (v4lfd);
        i = v4l2_set_output_by_name (v4lfd, plane_name);
        if (i == -1) {
            perror("Unable to set video output");
            goto exit_disp;
        }

        if (stdid == V4L2_STD_UNKNOWN) {
            /* if no standard was requested */
            if(LTV_CHECK (ioctl(v4lfd,VIDIOC_G_OUTPUT_STD,&stdid))<0) {
                goto exit_disp;
            }
            if (stdid == V4L2_STD_UNKNOWN) {
                /* and no standard is currently set on the display pipeline, we just
                    pick the first standard listed, whatever that might be. */
                standard.index = 0;
                if(LTV_CHECK (ioctl(v4lfd,VIDIOC_ENUM_OUTPUT_STD,&standard))<0) {
                    goto exit_disp;
                }
                stdid = standard.id;
            }
        }

        if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_OUTPUT_STD,&stdid))<0) {
            goto exit_disp;
        }
        standard.index = 0;
        standard.id    = V4L2_STD_UNKNOWN;
        do {
            if(LTV_CHECK (ioctl(v4lfd,VIDIOC_ENUM_OUTPUT_STD,&standard))<0) {
                goto exit_disp;
            }
            ++standard.index;
        } while((standard.id & stdid) != stdid);

        verbose("Current display standard is '%s'\n",standard.name);

        memset(&cropcap, 0, sizeof(cropcap));
        cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if(LTV_CHECK (ioctl(v4lfd,VIDIOC_CROPCAP,&cropcap))<0) {
            goto exit_disp;
        }
        frameduration = ((double)standard.frameperiod.numerator/(double)standard.frameperiod.denominator)*1000000.0;
        verbose("Frameduration = %lluus\n",frameduration);

        /* Set display sized buffer format */
        memcpy(&fmt, &format, sizeof(fmt));
        fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        fmt.fmt.pix.field       = V4L2_FIELD_ANY;

        /* set buffer format */
        if(LTV_CHECK (ioctl(v4lfd,VIDIOC_S_FMT,&fmt))<0) {
            goto exit_disp;
        }
        verbose("Selected V4L2 pixel format:\n");
        verbose("Colorspace            = %d\n",fmt.fmt.pix.colorspace);
        verbose("Width                 = %d\n",fmt.fmt.pix.width);
        verbose("Height                = %d\n",fmt.fmt.pix.height);
        verbose("BytesPerLine          = %d\n",fmt.fmt.pix.bytesperline);
        verbose("SizeImage             = %d\n",fmt.fmt.pix.sizeimage);
        verbose("Priv (Chroma Offset)  = %d\n",fmt.fmt.pix.priv);

        /* Request buffers to be allocated on the V4L2 device */
        memset(&reqbuf, 0, sizeof(reqbuf));
        reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        reqbuf.memory = V4L2_MEMORY_USERPTR;
        reqbuf.count  = bufferNb;
        reqbuf.reserved[0] = 0;
        reqbuf.reserved[1] = 0;

        /* request buffers */
        if(LTV_CHECK (ioctl (v4lfd, VIDIOC_REQBUFS, &reqbuf))<0) {
            goto exit_disp;
        }
        if(reqbuf.count < bufferNb) {
            perror("Unable to allocate all buffers");
            goto exit_disp;
        }

        /* Configure the display buffers . */
        for(i=0;i<bufferNb;i++) {
            memset(&buffer[i], 0, sizeof(struct v4l2_buffer));
            buffer[i].index = i;
            buffer[i].type  = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buffer[i].field = progressive?V4L2_FIELD_NONE:V4L2_FIELD_INTERLACED;
            buffer[i].m.userptr = (unsigned long)gamBuffer ;
            buffer[i].length = fmt.fmt.pix.sizeimage;
            buffer[i].memory = V4L2_MEMORY_USERPTR;
        }

        /* Set the Output crop to be the full resolution. */
        memset(&crop, 0, sizeof(struct v4l2_crop));
        crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        crop.c.left   = 0;
        crop.c.top    = 0;
        crop.c.width  = cropcap.bounds.width;
        crop.c.height = cropcap.bounds.height;
        verbose("Output Image Dimensions (%d,%d) (%d x %d)\n",
                crop.c.left,
                crop.c.top,
                crop.c.width,
                crop.c.height);

        /* set output crop */
        if (LTV_CHECK (ioctl(v4lfd,VIDIOC_S_CROP,&crop)) < 0) {
            goto exit_disp;
        }
        /* Set the buffer crop to be the full image. */
        memset(&crop, 0, sizeof(struct v4l2_crop));
        crop.type     = V4L2_BUF_TYPE_PRIVATE;
        crop.c.left   = 0;
        crop.c.top    = 0;
        crop.c.width  = format.fmt.pix.width;
        crop.c.height = format.fmt.pix.height;

        verbose("Source Image Dimensions (%d,%d) (%d x %d)\n",
                crop.c.left,
                crop.c.top,
                crop.c.width,
                crop.c.height);


        /* Work out the presentation time of the first buffer to be queued.
         * Note the use of the posix monotomic clock NOT the adjusted wall clock. */
        clock_gettime(CLOCK_MONOTONIC,&currenttime);

        verbose("Current time = %ld.%ld\n",currenttime.tv_sec,currenttime.tv_nsec/1000);
        ptime.tv_sec  = currenttime.tv_sec + delay;
        ptime.tv_usec = currenttime.tv_nsec / 1000;

        /* Queue all the buffers up for display. */
        for(i=0;i<bufferNb;i++) {
            buffer[i].timestamp = ptime;
            /* queue buffer */
            if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer[i])) < 0) {
                goto exit_disp;
            }
            ptime.tv_usec += frameduration;
            while(ptime.tv_usec >= 1000000L) {
                ptime.tv_sec++;
                ptime.tv_usec -= 1000000L;
            }
        }

        /* Start stream */
        verbose("Starting stream \n");

        if(LTV_CHECK (ioctl (v4lfd, VIDIOC_STREAMON, &buffer[0].type)) < 0) {
            goto exit_disp;
        }
        for(i=0;i<iterations;i++) {
            struct v4l2_buffer dqbuffer;
            long long time1;
            long long time2;
            long long timediff;

            /* To Dqueue a streaming memory buffer you must set the buffer type
            * AND and the memory type correctly, otherwise the API returns an error. */
            dqbuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            dqbuffer.memory = V4L2_MEMORY_MMAP;

            /* We didn't open the V4L2 device non-blocking so dequeueing a buffer will
            * sleep until a buffer becomes available. */
            #ifdef DEBUG
            printf("dequeueing buffer\n");
            #endif
            if(bufferNb < 3) {
                printf("Warning ! We use only %d buffer(s) so dequeueing will block forever\nPress CTRL+C to exit program !!!\n",
                        bufferNb);
            }
            if(LTV_CHECK (ioctl (v4lfd, VIDIOC_DQBUF, &dqbuffer)) < 0) {
                goto streamoff;
            }
            time1 = (buffer[dqbuffer.index].timestamp.tv_sec*1000000LL) + buffer[dqbuffer.index].timestamp.tv_usec;
            time2 = (dqbuffer.timestamp.tv_sec*1000000LL) + dqbuffer.timestamp.tv_usec;

            timediff = time2 - time1;
#ifdef DEBUG
            verbose ("Buffer%d: wanted %ld.%06ld, actual %ld.%06ld, diff = %lld us\n",
                dqbuffer.index, buffer[dqbuffer.index].timestamp.tv_sec,
                buffer[dqbuffer.index].timestamp.tv_usec,
                dqbuffer.timestamp.tv_sec, dqbuffer.timestamp.tv_usec, timediff);
#endif
            ptime.tv_usec += frameduration;
            if (unlikely(i == 0)) {
                /* after the first frame came back, we know when exactly the VSync is
                   happening, so adjust the presentation time - only once, though! */
                ptime.tv_usec += timediff;
                if (perf_bench_required == true) {
                    /* Log the Fill Rate */
                    log_fill_rate( frameduration + timediff,
                                   src_format.fmt.pix.width,
                                   src_format.fmt.pix.height,
                                   format.fmt.pix.width,
                                   format.fmt.pix.height,
                                   format.fmt.pix.pixelformat);
                }
            }
            while(ptime.tv_usec >= 1000000L) {
                ptime.tv_sec++;
                ptime.tv_usec -= 1000000L;
            }

            buffer[dqbuffer.index].timestamp = ptime;

            /* Queue buffer */
            if(LTV_CHECK (ioctl (v4lfd, VIDIOC_QBUF, &buffer[dqbuffer.index])) < 0) {
                goto streamoff;
            }
        }

    streamoff:
        verbose("Stream off \n");
        LTV_CHECK (ioctl(v4lfd, VIDIOC_STREAMOFF, &buffer[0].type));
    exit_disp:
        close(v4lfd);
    }
}

/************************************************************
 * Function    : usage
 * Description : Display the help of the application
 ************************************************************/
static void usage(void)
{
    fprintf(stderr,"Usage: v4l2-decode [options] file_name [options]\n");
    fprintf(stderr,"\t-d         - Dump the decoded picture in a GAM file\n");
    fprintf(stderr,"\t-n name    - If dump enabled, use this filename for the dumped GAM\n");
    fprintf(stderr,"\t-p         - disPlay the decoded picture (on Main-Vid plane)\n");
    fprintf(stderr,"\t-c         - Calculate CRC of Luma and Chroma decoded buffers and dump them to a local file\n");
    fprintf(stderr,"\t-w num     - Scale the decoded picture to Width num\n");
    fprintf(stderr,"\t-h num     - Scale the decoded picture to Height num\n");
    fprintf(stderr,"\t-r         - Decode a cRopped part of the original picture *** Feature still under development\n");
    fprintf(stderr,"\t-s mode    - Initiate a HoM sequence in the middle of decode mode=[HPS|CPS]\n");
    fprintf(stderr,"\t-f         - Log Performance Benchmarking results\n");
    fprintf(stderr,"\t-v         - Verbose\n");
    fprintf(stderr,"\n");
    exit(1);
}

/*************************************************************
 * Function    : decode
 * Description : starts the decode operation
 *               and blocks until the decode is done
 *************************************************************/
bool decode( int videofd, struct v4l2_buffer dstbuf)
{

    /* Queue the buffers and perform the JPEG decode */
    dstbuf.flags |= V4L2_BUF_FLAG_INPUT; /* Set the INPUT flag */

    verbose("VIDIOC_QBUF (dst) w/ buffer offset: 0x%08lx, length: %d\n",
           dstbuf.m.offset,
           dstbuf.length);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_QBUF, &dstbuf)) < 0) {
        printf("VIDIOC_QBUF (dst) failed!\n");
        return FAIL;
    }

    /* Block until decode finishes and dequeue the buffer */
    verbose("VIDIOC_DQBUF (dst)\n");
    if (LTV_CHECK (ioctl(videofd, VIDIOC_DQBUF, &dstbuf)) < 0) {
        printf("VIDIOC_DQBUF (dst) failed!\n");
        return FAIL;
    }

    return SUCCESS;
}

/*************************************************************
 * Function    : decode_for_hom
 * Description : starts the decode operation
 *               and launches the hom sequence before decode is done
 *************************************************************/
bool decode_for_hom ( int videofd, struct v4l2_buffer dstbuf, char* lowPowerMode)
{

    /* Queue the buffers and perform the JPEG decode */
    dstbuf.flags |= V4L2_BUF_FLAG_INPUT; /* Set the INPUT flag */

    verbose("VIDIOC_QBUF (dst) w/ buffer offset: 0x%08lx, length: %d\n",
           dstbuf.m.offset,
           dstbuf.length);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_QBUF, &dstbuf)) < 0) {
        printf("VIDIOC_QBUF (dst) failed!\n");
        return FAIL;
    }

    /* Sets the device fd to non-blocking mode */
    fcntl(videofd, F_SETFL, O_NONBLOCK|O_ASYNC);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_DQBUF, &dstbuf)) < 0) {
        printf("VIDIOC_DQBUF (dst) 1st Call failed!\nLaunching HoM sequence\n");
        system("echo +10 > /sys/class/rtc/rtc1/wakealarm");
        if (strcmp(lowPowerMode, "HPS")==0) {
            printf("HPS\n");
            system("echo -n mem > /sys/power/state");
        } else {
            printf("CPS\n");
            system("echo -n hom > /sys/power/state");
        }
        while (LTV_CHECK (ioctl(videofd, VIDIOC_DQBUF, &dstbuf)) < 0) {
            usleep(100);
        }
    } else {
        printf("Decode was too fast. Please choose a bigger picture.\n");
        return FAIL;
    }


    verbose("Decode done\n");
    return SUCCESS;
}

/*************************************************************
 * Function    : configure_decoding_destination
 * Descirption : configure the decoding destination
 *
 * return      : True if ALL is Ok, False if there is an error
 *************************************************************/
bool configure_decoding_destination( int videofd,
                                     void* *dstBuffer,
                                     int* dstBufSize,
                                     struct v4l2_format src_format,
                                     int target_width,
                                     int target_height,
                                     struct v4l2_buffer *dstbuf_p)
{
    int size = 0, i = 0, x = 0;
    struct v4l2_format format = src_format;
    struct v4l2_requestbuffers reqbufs;
    struct v4l2_format outputformat;


    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (resize_enabled && target_width > 0 ) {
        format.fmt.pix.width = target_width;
    }
    if (resize_enabled && target_height > 0 ) {
        format.fmt.pix.height = target_height;
    }

    outputformat = format;

    printf("VIDIOC_TRY_FMT (dst) dimensions %dx%d, fourcc: %d\n",
           outputformat.fmt.pix.width,
           outputformat.fmt.pix.height,
           outputformat.fmt.pix.pixelformat);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_TRY_FMT, &outputformat)) < 0) {
        printf("VIDIOC_TRY_FMT (dst) failed!\n");
        stream_off(videofd);
        return FAIL;
    }

    printf("    driver suggested dimensions %dx%d, imagesize: %d\n",
           outputformat.fmt.pix.width,
           outputformat.fmt.pix.height,
           outputformat.fmt.pix.sizeimage);

    verbose("VIDIOC_S_FMT (dst) w/ %d, %d\n",
           format.fmt.pix.width,
           format.fmt.pix.height);

    format.fmt.pix.sizeimage = outputformat.fmt.pix.sizeimage;/* NOTE: format.fmt.pix.sizeimage must be set before a VIDIOC_S_FMT */
    format.fmt.pix.pixelformat = outputformat.fmt.pix.pixelformat;/* Set the pixelformat suggested by VIDIOC_TRY_FMT (dst) */
    if (LTV_CHECK (ioctl(videofd, VIDIOC_S_FMT, &format)) < 0) { /* NOTE: VIDIOC_S_FMT takes the original format (desired dimensions), not outputformat! */
        printf("VIDIOC_S_FMT (dst) failed!\n");
        stream_off(videofd);
        return FAIL;
    }

    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (LTV_CHECK (ioctl(videofd, VIDIOC_REQBUFS, &reqbufs)) < 0) {
        printf("VIDIOC_REQBUFS (dst) failed!\n");
        return FAIL;
    }

    verbose("VIDIOC_REQBUFS (dst) returned %d output buffers\n", reqbufs.count);

    verbose("VIDIOC_STREAMON (dst)\n");
    i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (LTV_CHECK (ioctl(videofd, VIDIOC_STREAMON, &i)) < 0) {
        printf("VIDIOC_STREAMON (dst) failed!\n");
    }
    /* Allocate a buffer for the output YCbCr luma and Chroma data */
    memset(dstbuf_p, 0, sizeof(struct v4l2_buffer));
    dstbuf_p->index = 0;
    dstbuf_p->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dstbuf_p->memory = V4L2_MEMORY_MMAP;

    if (LTV_CHECK (ioctl(videofd, VIDIOC_QUERYBUF, dstbuf_p)) < 0) {
        printf("VIDIOC_QUERYBUF (src) failed!\n");
        return FAIL;
    }

    *dstBuffer = mmap(NULL, dstbuf_p->length, PROT_READ | PROT_WRITE,
                  MAP_SHARED, videofd, dstbuf_p->m.offset);

    if (*dstBuffer == MAP_FAILED) {
        return FAIL;
    }

    *dstBufSize = dstbuf_p->length;
    verbose("mmap()'ed (dst) buffer at: 0x%08x, size: %d\n",*dstBuffer, *dstBufSize);

    return SUCCESS;
}
/*************************************************************
 * Function    : configure_decoding_source
 * Description : configure the decoding source
 *
 * return      : True if ALL is Ok, False if there is an error
 ************************************************************/
bool configure_decoding_source( char* JpegFileName,
                                int videofd,
                                void* *srcBuffer,
                                int* srcBufSize ,
                                struct v4l2_format *srcFormat_p)
{
    FILE *jpeg = NULL;
    int jpegfd = 0, i = 0, x = 0, size = 0;
    struct v4l2_format format;
    struct v4l2_requestbuffers reqbufs;
    struct v4l2_buffer srcbuf;

    jpeg = fopen(JpegFileName, "rb");
    if (!jpeg) {
        printf("couldn't open input JPEG file (%s)!\n", JpegFileName);
        return FAIL;
    }

    printf("Decoding: %s\n", JpegFileName);

    /* Retrieve the JPEG file size */
    fseek(jpeg, 0, SEEK_END);
    size = ftell(jpeg);
    fseek(jpeg, 0, SEEK_SET);

    fclose(jpeg);

    /* Prepare the buffer to hold JPEG data */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    format.fmt.pix.sizeimage = size;

    verbose("VIDIOC_S_FMT (src) -> %d bytes ..\n", size);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_S_FMT, &format)) < 0) {
        printf("VIDIOC_S_FMT (src) failed!\n");
        return FAIL;
    }

    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbufs.memory = V4L2_MEMORY_MMAP;

    if (LTV_CHECK (ioctl(videofd, VIDIOC_REQBUFS, &reqbufs)) < 0) {
        printf("VIDIOC_REQBUFS (src) failed!\n");
        return FAIL;
    }

    if (reqbufs.count == 0) {
        printf("VIDIOC_REQBUFS (src) did not return any buffers !! \n");
        return FAIL;
    } else {
        verbose("VIDIOC_REQBUFS (src) returned %d input buffers\n", reqbufs.count);
    }
    verbose("VIDIOC_STREAMON (src)\n");
    i = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (LTV_CHECK (ioctl(videofd, VIDIOC_STREAMON, &i)) < 0) {
        printf("VIDIOC_STREAMON (src) failed!\n");
    }
    memset(&srcbuf, 0, sizeof(srcbuf));
    srcbuf.index = 0;
    srcbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    srcbuf.memory = V4L2_MEMORY_MMAP;

    if (LTV_CHECK (ioctl(videofd, VIDIOC_QUERYBUF, &srcbuf)) < 0) {
        printf("VIDIOC_QUERYBUF (src) failed!\n");
        return FAIL;
    }

    *srcBuffer = mmap(NULL, srcbuf.length, PROT_READ | PROT_WRITE,
                 MAP_SHARED, videofd, srcbuf.m.offset);

    if (*srcBuffer == MAP_FAILED) {
        return FAIL;
    }

    *srcBufSize = srcbuf.length ;

    verbose("mmap()'ed (src) buffer at: 0x%08x, size: %d\n", *srcBuffer, *srcBufSize);

    /* Load the JPEG to the buffer */
    jpegfd = open(JpegFileName, O_RDONLY);

    if ((x = read(jpegfd, *srcBuffer, size)) != size) {
        printf("couldn't load JPEG data! (read %d bytes, ret: %d)\n", size, x);
        perror("read");
        return FAIL;
    }

    close(jpegfd);

    srcbuf.bytesused = size;
    srcbuf.flags &= ~V4L2_BUF_FLAG_INPUT; /* Clear the INPUT flag */

    verbose("VIDIOC_QBUF (src) w/ offset: 0x%08lx, length: %d\n",
           srcbuf.m.offset, srcbuf.length);

    if (LTV_CHECK (ioctl(videofd, VIDIOC_QBUF, &srcbuf)) < 0) {
        printf("VIDIOC_QBUF (src) failed!\n");
        return FAIL;
    }

     /* Get the source format in &format */
    memset(srcFormat_p, 0, sizeof(struct v4l2_buffer));
    srcFormat_p->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (LTV_CHECK (ioctl(videofd, VIDIOC_G_FMT, srcFormat_p)) < 0) {
        printf("VIDIOC_G_FMT (src) failed!\n");
        return FAIL;
    }

    verbose("VIDIOC_G_FMT (src): picture is %dx%d\n",
           srcFormat_p->fmt.pix.width,
           srcFormat_p->fmt.pix.height);

    return SUCCESS;
}

/***************************************************************************
 * Function    : parse_arguments
 * Description : Parse the command line arguments and configure the relevant
 *                parameters
 ***************************************************************************/
void parse_arguments( int argc, char** argv, char* fileName, char* destFileName, 
                      int* target_width, int* target_height, char* lowPowerMode)
{

    int opt;
    char *option_list = "fvcrpdw:h:n:s:";

    while ((opt = getopt (argc, argv, option_list)) != -1) {
        switch(opt) {
            case 's':
                if (optarg!= NULL) {
                    hom_enabled = true;
                    strncpy(lowPowerMode, optarg, strlen(optarg) * sizeof(char) );
                    lowPowerMode[strlen(optarg)] = '\0';
                } else {
                    printf(" You need to specify the HoM mode [CPS|HPS] !\n");
                    usage();
                }
                break;
            case 'd':
                dump_enabled = true;
                break;
            case 'p':
                display_enabled = true;
                break;
            case 'c':
                crc_enabled = true;
                break;
            case 'v':
                verbose_enabled = true;
                break;
            case 'r':
                cropping_enabled = true;
                printf(" Feature still not supported !\n");
                break;
            case 'w':
                *target_width = atoi(optarg);
                if (*target_width > 0) {
                    resize_enabled = true;
                } else {
                    printf(" You need to specify the width !\n");
                    usage();
                }
                break;
            case 'h':
                *target_height = atoi(optarg);
                if (*target_height > 0) {
                    resize_enabled = true;
                } else {
                    printf(" You need to specify the height !\n");
                    usage();
                }
                break;
            case 'n':
                if (optarg!= NULL) {
                    custom_filename = true;
                    strncpy(destFileName, optarg, strlen(optarg) * sizeof(char) );
                    destFileName[strlen(optarg)] = '\0'; /* Extra charaters are copied in fileName */
                } else {
                    printf(" You need to specify the custom dump filename !\n");
                    usage();
                }
                break;
            case 'f':
                perf_bench_required = true;
                break;
            default:
                usage();
                break;
        }
    }

    /* Get the JPEG File Name */
    if (optind == (argc-1)) {
        strncpy(fileName, argv[optind], strlen(argv[optind]) * sizeof(char) );
        fileName[strlen(argv[optind])] = '\0'; /* Extra charaters are copied in fileName */
    } else {
        usage();
    }

    return;
}

/**********************************
 * Function : main()
 **********************************/
int main(int argc, char** argv)
{
    int    src_width = -1, src_height = -1, target_width = -1, target_height = -1;
    int    videofd;
    int    srcBufSize = 0, dstBufSize = 0;
    void  *srcBuffer_p = NULL, *dstBuffer_p = NULL;

    struct v4l2_buffer dstbuf;
    struct v4l2_format dstFormat, srcFormat;

    char   JpegFileName[250], destFileName[250], lowPowerMode[4];


    /* Parse the arguments if any */
    parse_arguments(argc, argv, JpegFileName, destFileName, &target_width, &target_height,
                    lowPowerMode);

    /* Open the JPEG video device */
    videofd = v4l2_open_by_name(V4L2_HWJPEG_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);
    if (videofd < 0) {
        perror("Couldn't open jpeg video device\n");
        v4l2_clean_up_and_exit( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
    }

    /* Get supported Input/Output formats from the hw-jpeg-decoder */
    get_supported_formats( videofd);

    /* Configure the source of the decode operation */
    if (!configure_decoding_source (JpegFileName, videofd, &srcBuffer_p, &srcBufSize, &srcFormat)) {
        v4l2_clean_up_and_exit( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
    }

    /* Configure the destination of the decode operation */
    if (!configure_decoding_destination(videofd, &dstBuffer_p, &dstBufSize, srcFormat, target_width, target_height, &dstbuf)) {
        v4l2_clean_up_and_exit( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
    }

    /* Perform the decode */
    if (hom_enabled) {
        if (!decode_for_hom(videofd, dstbuf, lowPowerMode)) {
            v4l2_clean_up_and_exit( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
        }
    } else {
        if (!decode(videofd, dstbuf)) {
            v4l2_clean_up_and_exit( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
        }
    }

    /**************** The Decode Operation Is Done ****************/
    /**************************************************************/

    /* Load the destination format to use it when writing the decode operation report,
       dumping or displaying the resulting GAM buffer */
    memset(&dstFormat, 0, sizeof(dstFormat));
    dstFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (LTV_CHECK (ioctl( videofd, VIDIOC_G_FMT, &dstFormat)) < 0) {
        printf("VIDIOC_G_FMT (dst) failed!\n");
        printf("It won't be possible to dump|display the decoded buffer!\n");
        stream_off(videofd);
        v4l2_clean_up_and_exit ( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
    }
    verbose("Real Dimensions: %d x %d - %dBytes/Line \n",
            dstFormat.fmt.pix.width, dstFormat.fmt.pix.height,
            dstFormat.fmt.pix.bytesperline);

    stream_off(videofd);

    /* Write the decoding report in the log file */
    dump_crc ( JpegFileName, videofd, srcFormat, dstFormat);

    /* Dump the decoded buffer in a GAM file */
    dump_GAM ( JpegFileName, destFileName, dstBuffer_p, dstFormat);

    /* Display the decoded buffer on Main-Vid */
    v4l2_display_GAM ( srcFormat, dstFormat, dstBuffer_p);

    /* Clean up reserved memory and Exit application */
    v4l2_clean_up_and_exit ( videofd, srcBuffer_p, srcBufSize, dstBuffer_p, dstBufSize);
}
