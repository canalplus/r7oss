/*
 * dfbjpegdecode.c, A test application for decoding JPEGs using V4L2 and DirectFB
 *
 * Copyright (C) 2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/videodev2.h>

#include <directfb.h>

#include "common.h"

bool dump_enabled = false,
    display_enabled = false,
    crc_enabled = false,
    verbose_enabled = false,
    cropping_enabled = false,
    resize_enabled = false,
    custom_filename = false,
    perf_bench_required = false;

/************************************************************
 * Function    : usage
 * Description : Display the help of the application
 ************************************************************/
static void usage( void)
{
    fprintf(stderr,"Usage: dfb-jpegdecode file_name [options]\n");
    fprintf(stderr,"\t-d         - Dump the decoded picture in a GAM file\n");
    fprintf(stderr,"\t-n name    - If dump enabled, use this filename for the dumped GAM\n");
    fprintf(stderr,"\t-p         - disPlay the decoded picture (on Main-Vid plane)\n");
    fprintf(stderr,"\t-c         - Calculate CRC of Luma and Chroma decoded buffers and dump them to a local file\n");
    fprintf(stderr,"\t-w num     - Scale the decoded picture to Width num\n");
    fprintf(stderr,"\t-h num     - Scale the decoded picture to Height num\n");
    fprintf(stderr,"\t-r         - Decode a cRopped part of the original picture *** Feature still under development\n");
    fprintf(stderr,"\t-f         - Log Performance Benchmarking results\n");
    fprintf(stderr,"\t-v         - Verbose\n");
    fprintf(stderr,"\n");
    exit(1);
}

/***************************************************************************
 * Function    : parse_arguments
 * Description : Parse the command line arguments and configure the relevant
 *                parameters
 ***************************************************************************/
void parse_arguments( int argc, char** argv, char* fileName, char* destFileName)
{

    int opt;
    char *option_list = "fvcpdrn:";

    while ((opt = getopt (argc, argv, option_list)) != -1)
    {
        switch(opt)
        {
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

/***********************************************************************
 * Function    : dfb_clean_up_and_exit
 * Description : release the DFB interfaces and surfaces created
 ***********************************************************************/
void dfb_clean_up_and_exit( IDirectFB* dfb, IDirectFBSurface* pSource, IDirectFBSurface* pDestination, IDirectFBSurface* pSurface)
{
    if (pSource) {
        pSource->Unlock( pSource );
        pSource->Release( pSource );
    }

    if (pDestination) {
        pDestination->Unlock( pDestination );
        pDestination->Release( pDestination );
    }
    if (pSurface) {
        pSurface->Release( pSurface );
    }
    dfb->Release( dfb );

    exit (0);
}

/***************************************************************
 * Function    : display_GAM
 * Description : blit the surface with decoded GAM data to screen
 ***************************************************************/
void dfb_display_GAM ( IDirectFBSurface* pSurface,
                            IDirectFBSurface* pDestination,
                            struct v4l2_format dst_format,
                            struct v4l2_format src_format)
{
    struct timespec            currenttime;
    struct timeval             start_time, end_time;
    int    delay = 1;
    unsigned long long display_time = 0; /* Display time in uSeconds */

    if (display_enabled) {
        /* Clip to the valid destination rect */
        DFBRectangle rect = {
            .x = 0,
            .y = 0,
            .w = dst_format.fmt.pix.width,
            .h = dst_format.fmt.pix.height
        };
        if (perf_bench_required == true) {
        /* Get the current "system" time */
            clock_gettime( CLOCK_MONOTONIC,&currenttime);
            start_time.tv_sec  = currenttime.tv_sec + delay;
            start_time.tv_usec = currenttime.tv_nsec / 1000;
        }
        pSurface->StretchBlit( pSurface, pDestination, &rect, NULL );
        pSurface->Flip( pSurface, NULL, DSFLIP_NONE );
        if (perf_bench_required == true) {
            /* Get the current "system" time */
            clock_gettime( CLOCK_MONOTONIC,&currenttime);
            end_time.tv_sec  = currenttime.tv_sec + delay;
            end_time.tv_usec = currenttime.tv_nsec / 1000;
            /* Calculate the difference between start_time and end_time */
            display_time = (end_time.tv_sec * 1000000LL + end_time.tv_usec);
            display_time -= (start_time.tv_sec * 1000000LL + start_time.tv_usec);
            log_fill_rate( display_time,
                           src_format.fmt.pix.width,
                           src_format.fmt.pix.height,
                           dst_format.fmt.pix.width,
                           dst_format.fmt.pix.height,
                           dst_format.fmt.pix.pixelformat);
        }
        sleep(4);
    }
    return;
}

/**********************************
 * Function : main()
 **********************************/
int main( int argc, char *argv[] )
{
    DFBResult               ret;
    DFBSurfaceDescription   desc;
    IDirectFB              *dfb;
    IDirectFBSurface       *pSurface = NULL;
    IDirectFBSurface       *pSource = NULL;
    IDirectFBSurface       *pDestination = NULL;
    DFBSurfacePixelFormat   fmt;
    void                   *ret_ptr;
    int                     ret_pitch;

    unsigned int size;
    FILE *jpeg = NULL;
    int jpegfd;

    struct fmt *outputfmt;
    struct v4l2_requestbuffers reqbufs;
    struct v4l2_format src_format;
    struct v4l2_format format, dst_format;
    struct v4l2_buffer srcbuf;
    struct v4l2_buffer dstbuf;

    int fd;
    int type;

    char   JpegFileName[250], destFileName[250];


    ret = DirectFBInit( &argc, &argv ); /* In case we want to pass special parameters*/
    if (ret) {                          /* to DirectFB from within command line */
        printf("Error initializing DFB\n");
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    parse_arguments(argc, argv, JpegFileName, destFileName);

    ret = DirectFBCreate( &dfb ); /* Create an interface with DirectFB */
    if (ret){
        printf("Error Creating DFB interface\n");
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    jpeg = fopen( JpegFileName, "rb" );
    if (!jpeg) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    fseek( jpeg, 0, SEEK_END );
    size = ftell( jpeg );

    dfb->SetCooperativeLevel( dfb, DFSCL_EXCLUSIVE ); /* Take full control over layers */

    memset( &desc, 0, sizeof(desc) );

    desc.flags = DSDESC_CAPS;
    desc.caps = DSCAPS_PRIMARY|DSCAPS_DOUBLE;

    ret = dfb->CreateSurface( dfb, &desc, &pSurface );
    if (ret) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    pSurface->Clear( pSurface, 0, 0, 0, 255 );
    pSurface->Flip( pSurface, NULL, 0 );
    pSurface->Clear( pSurface, 0, 0, 0, 255 );

    fd = v4l2_open_by_name( V4L2_HWJPEG_DRIVER_NAME, V4L2_DISPLAY_CARD_NAME, O_RDWR);

    memset(&desc, 0, sizeof(desc));

    int w = 4 * 4096;
    int h = size / w + 1;

    desc.flags = DSDESC_PIXELFORMAT|DSDESC_WIDTH|DSDESC_HEIGHT|DSDESC_CAPS;
    desc.caps = DSCAPS_VIDEOONLY;
    desc.pixelformat = DSPF_A8;
    desc.width = w;
    desc.height = h;

    ret = dfb->CreateSurface( dfb, &desc, &pSource );
    if (ret) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    ret = pSource->Lock( pSource, DSLF_WRITE, &ret_ptr, &ret_pitch );
    if (ret) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    printf("Loading JPEG picture (%d bytes) ..\n", size);

    fseek( jpeg, 0, SEEK_SET );
    fread( ret_ptr, 1, size, jpeg );
    fclose( jpeg );
    jpeg = NULL;

    verbose("JPEG picture loaded.\n");

    pSource->Unlock( pSource );

    /* Request capture buffer */
    memset( &reqbufs, 0, sizeof( reqbufs ) );
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_USERPTR;

    if (LTV_CHECK (ioctl( fd, VIDIOC_REQBUFS, &reqbufs )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    /* Request source buffer */
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbufs.memory = V4L2_MEMORY_USERPTR;

    if (LTV_CHECK (ioctl( fd, VIDIOC_REQBUFS, &reqbufs )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    /* Toggle streaming on */
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (LTV_CHECK (ioctl( fd, VIDIOC_STREAMON, &type )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (LTV_CHECK (ioctl( fd, VIDIOC_STREAMON, &type )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    /* Set the source format as JPEG */
    memset( &src_format, 0, sizeof( src_format ) );
    src_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    src_format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;

    if (LTV_CHECK (ioctl( fd, VIDIOC_S_FMT, &src_format )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    /* Queue the source buffer */
    memset( &srcbuf, 0, sizeof( srcbuf ) );
    srcbuf.index = 0;
    srcbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    srcbuf.memory = V4L2_MEMORY_USERPTR;
    srcbuf.m.userptr = (unsigned long)ret_ptr;
    srcbuf.length = size;

    if (LTV_CHECK (ioctl( fd, VIDIOC_QBUF, &srcbuf )) < 0) {
        stream_off(fd);
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    /* Retrieve the JPEG format */
    memset( &format, 0, sizeof( format ) );
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (LTV_CHECK (ioctl( fd, VIDIOC_G_FMT, &format )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    verbose( "JPEG is %dx%d\n",
            format.fmt.pix.width, format.fmt.pix.height );

    /* Save a copy of the original dimensions */
    src_format = format;

    /* We want to decode to the Original width and height of the JPEG */
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    format.fmt.pix.width = 1920;
    format.fmt.pix.height = 1080;

    dst_format = format;

    /* Ask the decoder for its preferred format */
    if (LTV_CHECK (ioctl( fd, VIDIOC_TRY_FMT, &dst_format )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    switch (dst_format.fmt.pix.pixelformat) {
        case V4L2_PIX_FMT_NV12:
            fmt = DSPF_NV12;
            break;
        case V4L2_PIX_FMT_NV16:
            fmt = DSPF_NV16;
            break;
        case V4L2_PIX_FMT_NV24:
            fmt = DSPF_NV24;
            break;
        default:
            stream_off(fd);
            dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    desc.flags = DSDESC_PIXELFORMAT|DSDESC_WIDTH|DSDESC_HEIGHT|DSDESC_CAPS;
    desc.caps = DSCAPS_VIDEOONLY;
    desc.pixelformat = fmt;
    desc.width = dst_format.fmt.pix.bytesperline; /* h/w pitch */
    desc.height = dst_format.fmt.pix.height;

    printf( "Decoding to %dx%d\n",
            dst_format.fmt.pix.width, dst_format.fmt.pix.height );

    ret = dfb->CreateSurface( dfb, &desc, &pDestination );
    if (ret) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    format.fmt.pix.sizeimage =
        desc.width * DFB_PLANE_MULTIPLY( fmt, desc.height );
    format.fmt.pix.pixelformat = dst_format.fmt.pix.pixelformat;

    /* Note: VIDIOC_S_FMT takes the original requested dimensions */
    if (LTV_CHECK (ioctl( fd, VIDIOC_S_FMT, &format )) < 0) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    ret = pDestination->Lock( pDestination, DSLF_WRITE, &ret_ptr, &ret_pitch );
    if (ret) {
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }
    /* Queue the destination buffer */
    memset( &dstbuf, 0, sizeof( dstbuf ) );
    dstbuf.index = 0;
    dstbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dstbuf.memory = V4L2_MEMORY_USERPTR;
    dstbuf.m.userptr = (unsigned long)ret_ptr;
    dstbuf.length = dst_format.fmt.pix.sizeimage;

    if (LTV_CHECK (ioctl( fd, VIDIOC_QBUF, &dstbuf )) < 0){
        stream_off(fd);
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    /* Block until decoded picture is ready */
    if (LTV_CHECK (ioctl( fd, VIDIOC_DQBUF, &dstbuf )) < 0){
        stream_off(fd);
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);
    }

    memset(&dst_format, 0, sizeof(dst_format));
    dst_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (LTV_CHECK (ioctl( fd, VIDIOC_G_FMT, &dst_format)) < 0)
        dfb_clean_up_and_exit( dfb, pSource, pDestination, pSurface);

    /* Turn off streaming on jpeg video device */
    stream_off(fd);

    /* Write the decoding report in the log file */
    dump_crc ( JpegFileName, fd, src_format, dst_format);

    /* Dump the decoded buffer in a GAM file */
    dump_GAM ( JpegFileName, destFileName, ret_ptr, format);

    pDestination->Unlock( pDestination );

    /* Display the decoded buffer */
    dfb_display_GAM ( pSurface, pDestination, dst_format, src_format);

    /* Clean up reserved memory and Exit application */
    dfb_clean_up_and_exit ( dfb, pSource, pDestination, pSurface);
}
