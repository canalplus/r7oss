 /************************************************************************
 *  Copyright (C) 2013, MathEmbedded. All Rights Reserved.
 *
 * This application is a test application that will configure the capture
 * pipeline to down scale and resize the main output and write it back
 * to the graphics plane of the auxiliary input.
 *
 *************************************************************************/

/*!
 * \file scart_path_configure.c
 * \brief Test application for the dvb_capture_pipe V4L2 driver
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include "linux/drivers/video/stmfb.h"
#include "linux/drivers/media/video/stmvout.h"

/* hard copy from player2 which is not yet compiled .. */
#include "dvb_cap_export.h"

#define DISPLAY "/sys/class/stmcoredisplay/display%d/"
#define HDMI "hdmi%d.%d/"

/*!
 * Determine the presence of an HDMI monitor
 *
 * \return 1 if present, 0 if not present or failure
*/
static int is_hdmi_monitor_present( void )
{
    char attrname[256];
    const int device_number = 0;
    const int subdevice_number = 0;
    char p[3];
    int fd;

    snprintf( attrname, 256, DISPLAY HDMI "hotplug", device_number,
        device_number, subdevice_number );

    fd = open( attrname, O_RDONLY );
    if ( fd < 0 )
        perror("Failed to open HDMI device\n");
    else
    {
        ssize_t sz = read( fd, p, 2 );
        close(fd);

        if ( sz < 0 )
            perror( "Failed to determine HDMI status\n" );
        else
        {
            p[2] = 0;
            if ( !strcmp( p, "y\n" ) )
                return 1;
        }
    }

    return 0;
}

/*!
 * Configure the auxiliary graphics plane. Set the OSD transparency and 
 * set the mixer background colour to black.
 *
 * \param transparency_on Set to 1 to make OSD transparent and 0 to make
 * it opaque.
 * \return 0 on success, 1 on failure
 */
static int configure_auxiliary_framebuffer( unsigned transparency_on )
{
    const int fd = open( "/dev/fb1", O_RDWR );
    struct stmfbio_var_screeninfo_ex screeninfo;
    struct stmfbio_output_configuration outputConfig;
    const void *buf_address; 
    const unsigned long mixer_background = 0x00000000; /* Format AARRGGBB */

    if( fd < 0 )
    {
        perror ( "Unable to open frame buffer\n" );
        return 1;
    }

    /* Set the transparency */
    screeninfo.layerid = 0;
    screeninfo.opacity = transparency_on ? 255L : 0;
    screeninfo.caps = STMFBIO_VAR_CAPS_OPACITY;
    screeninfo.activate = STMFBIO_ACTIVATE_IMMEDIATE;
    if ( ioctl( fd, STMFBIO_SET_VAR_SCREENINFO_EX, &screeninfo) < 0 )
    {
        perror( "Unable to set frame buffer transparency\n" );
        close( fd );
        return 1;
    }

    /* Set the mixer background */
    outputConfig.caps = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
    outputConfig.mixer_background = mixer_background;
    outputConfig.outputid = STMFBIO_OUTPUTID_MAIN;
    outputConfig.activate = STMFBIO_ACTIVATE_IMMEDIATE;
    if ( ioctl( fd, STMFBIO_SET_OUTPUT_CONFIG, &outputConfig ) < 0 )
    {
        perror( "Failed to set mixer background\n" );
        close( fd );
        return 1;
    }

    close( fd );
    return 0;
}

/*!
 * Locate the capture device. Scan a maximum of 10 devices.
 *
 * \param videofd The video device handle
 * \return 0 on success, 1 on failure
 */
static int select_capture_device( int videofd )
{
    struct v4l2_input input;
    const char *const name = "CAPDVB"; /* Capture device identification */
    const unsigned int max_devices_to_scan = 10;

    memset( &input, 0, sizeof(input) );
    input.index = 0;
    input.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Scan through devices until we find the capture device */
    while ( !ioctl ( videofd, VIDIOC_ENUMINPUT, &input) &&
            ( input.index < max_devices_to_scan ) )
    {
        if ( !strncasecmp(name,(char *)input.name,strlen(name)) )
        {
            if ( ( ioctl( videofd, VIDIOC_S_INPUT, &input.index ) ) < 0 )
            {
                perror( "Cannot select device\n" );
                return 1;
            }

            return 0;
        }

        ++input.index; 
    }

    perror("Failed to find capture device\n");
    return 1;
}

/*!
 * Read the screen info from the driver
 *
 * \param dev_name The device to open
 * \param var The structure to populate
 * \return 0 on success, 1 on failure
 *
 *  Notes:
 *     This function does not return a handle. The device selected is stored
 *     inside the driver.
 */
static int read_framebuffer_info( const char *const dev_name,
                                  struct fb_var_screeninfo *const var )
{
    const int fd = open( dev_name,  O_RDONLY );
    int status;
 
    if ( fd < 0 )
    {
        perror("Failed to open frame buffer\n");
        return 1;
    }

    status = ioctl( fd, FBIOGET_VSCREENINFO, var );
    if ( status  < 0 )
        perror( "Failed to get frame buffer info\n" );

    close( fd );
    return status;
}

/*!
 * Locate the frame buffer base address and confirm its size
 *
 * \param dev_name The frame buffer device to open
 * \param buf_length The size of buffer (in bytes) required
 * \return buffer base address on success, NULL on failure
 */
static void *get_buffer_address( const char *const dev_name,
                                 const size_t buf_length )
{
    void *buf_address;
    int fd = open( dev_name,  O_RDONLY );
 
    if ( fd < 0 )
    {
        perror(" Failed to open frame buffer device\n" );
        return NULL;
    }

    buf_address = mmap(NULL, buf_length, PROT_READ, MAP_SHARED, fd, 0 );
    if ( buf_address == MAP_FAILED )
    {
        perror( "Failed to read plane address\n" );
        return NULL;
    }

    return buf_address;
}

/*!
 * Configure the capture pipeline
 *
 * \param videofd The video device handle
 * \param pixel_format The pixel format to use for capture, Any of:
 *                             SURF_RGB565,
 *                             SURF_RGB888,
 *                             SURF_ARGB8565,
 *                             SURF_ARGB8888,
 *                             SURF_ARGB1555,
 *                             SURF_ARGB4444,
 *                             SURF_CRYCB888,
 *                             SURF_YCBCR422R,
 *                             SURF_ACRYCB8888
 * \param colorspace The colorimetry to use for capture. Any of:
 *                       V4L2_COLORSPACE_SMPTE170M, (ITU 601)
 *                       V4L2_COLORSPACE_REC709,    (ITU 709)
 * \param capture_source Which point in the main video path to capture 
 * data. Any of: CAP_PT_VID1,         (Internal VID1 output in ARGB8888 format)
 *               CAP_PT_VID2,         (Internal VID2 output in ARGB8888 format)
 *               CAP_PT_EXT_YCBCR888, (External AYCbCr port in AYCbCr8888 format)
 *               CAP_PT_MIX2_YCBCR,   (MIX2 YCbCr output)
 *               CAP_PT_MIX2_RGB,     (MIX2 RGB output)
 *               CAP_PT_MIX1_YCBCR,   (MIX1 YCbCr output)
 *               CAP_PT_MIX1_RGB,     (MIX1 RGB output)
 * \return 0 on success, 1 on failure 
 */
static int configure_capture_parameters( const int videofd,
                                         const SURF_FMT pixel_format,
                                         const enum v4l2_colorspace colorspace,
                                         const cap_v4l2_capture_source_t capture_source )
{
    struct v4l2_crop crop;
    struct v4l2_buffer buf;
    struct v4l2_format fmt;
    struct fb_var_screeninfo var_main;
    struct fb_var_screeninfo var_aux;
    struct stmfbio_var_screeninfo_ex screeninfo;
    struct stmfbio_output_configuration outputConfig;
    const *buf_address;

    /* Read the main display info (for width and height) */
    if ( read_framebuffer_info( "/dev/fb0", &var_main ) )
        return 1;

    /* Read the auxiliary display info */
    if ( read_framebuffer_info( "/dev/fb1", &var_aux ) )
        return 1;

    /***************************************************************************
     * Write main display size and capture parameters
     **************************************************************************/
    printf( "Main Display parameters read %ldx%ld%s\n", var_main.xres,
        var_main.yres, var_main.vmode & FB_VMODE_MASK ? "-i" : "" );

    memset( &fmt, 0, sizeof(fmt) );
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = var_main.xres;
    fmt.fmt.pix.height = var_main.yres;
    fmt.fmt.pix.field = var_main.vmode & FB_VMODE_MASK ?
        V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
    fmt.fmt.pix.priv = capture_source;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.colorspace = colorspace;

    if ( ioctl( videofd, VIDIOC_S_FMT, &fmt ) < 0 )
    {
        perror( "Couldn't set capture params\n" );
        return 1;
    }

    /***************************************************************************
     * Write auxiliary display size
     **************************************************************************/
    printf( "Aux Display parameters read %ldx%ld\n", var_aux.xres,
        var_aux.yres );

    memset( &crop, 0, sizeof(crop) );
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.width = var_aux.xres;
    crop.c.height = var_aux.yres;

    if ( ioctl( videofd, VIDIOC_S_CROP, &crop ) < 0 )
    {
        perror( "Couldn't set display params\n" );
        return 1;
    }

    /***************************************************************************
     * Locate the auxiliary graphics plane frame buffer and request the
     * capture pipeline to use this buffer
     **************************************************************************/

    memset( &buf, 0, sizeof(buf) );
    
    /* Set the buffer length assuming a storage of 2 bytes per pixel */
    buf.length = var_aux.xres * var_aux.yres * 2;
    buf_address = get_buffer_address( "/dev/fb1", buf.length );
    if ( !buf_address )
        return 1;

    /* Specify the location to capture video data to */
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.m.userptr = (unsigned long)buf_address;

    if ( ioctl ( videofd, VIDIOC_QBUF, &buf ) < 0 )
    {
        perror( "Unable to queue buffer\n" );
        return 1;
    }

    return 0;
}

/*!
 * Start capture
 *
 * \param videofd The video device handle
 * \return 0 on success, 1 on failure 
 */
static int capture_on( int fd )
{
    int arg = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if ( ioctl ( fd, VIDIOC_STREAMON, &arg ) < 0 )
    {
        perror("Unable to start capture\n");
        return 1;
    }

    return 0;
}

/*!
 * Stop capture
 *
 * \param videofd The video device handle
 * \return 0 on success, 1 on failure 
 */
static int capture_off( int fd )
{
    int arg = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if ( ioctl ( fd, VIDIOC_STREAMOFF, &arg ) < 0 )
    {
        perror( "Unable to stop capture\n" );
        return 1;
    }

    return 0;
}

/*!
 * Help menu
 */
static void print_help( void )
{
    printf("Help menu\n"
           "---------\n\n"
           "    ./scart_path_configure [option] <command>\n\n"
           "Options:\n\n"
           "    -h                       - Print out this message\n"
           "Commands\n\n"
           "    start <capture point>    - Start capture pipeline\n"
           "    stop                     - Stop capture pipeline\n\n"
           "Capture points:\n"
           "    pre_osd                  - Capture point pre OSD\n"
           "    post_osd                 - Capture point post OSD\n\n");
}

/*!
 * Main application
 * \return 0 on success, 1 on failure 
 */
int main( int argc, char *argv[] )
{
    int videofd;
    int start_capture = 0;
    int status = 0;
    
    /* Provide capture defaults */
    const enum v4l2_colorspace colorspace = V4L2_COLORSPACE_SMPTE170M;
    const SURF_FMT pixel_format = SURF_RGB565 ;
    cap_v4l2_capture_source_t capture_source;

    /* Process command line arguments */
    if ( argc < 2 )
        return 0;

    if ( !strcasecmp( argv[1], "-h" ) )
    {
        print_help( );
        return 0;
    }

    if ( !strcasecmp( argv[1], "start" ) )
    {
        start_capture = 1;

        if ( argc < 3 )
        {
            printf( "Specify a capture point\n" );
            return 1;
        }

        if ( !strcasecmp( argv[2], "pre_osd" ) )
            capture_source = CAP_PT_VID1;
        else if ( !strcasecmp( argv[2], "post_osd" ) )
            capture_source = CAP_PT_MIX1_RGB;
        else
        {
            printf( "Unrecognised capture point\n" );
            return 1;
        }
    }
    else if ( strcasecmp( argv[1], "stop" ) )
    {
        printf("Unrecognised instruction\n");
        return 1;
    }

    videofd = open( "/dev/video0", O_RDWR );
    if ( videofd < 0 )
    {
        perror( "Can't open video device\n" );
        return 1;
    }

    if ( start_capture )
    {
        /* Determine the presence of an HDMI monitor */
        status = is_hdmi_monitor_present( );
        if ( status )
        {
            printf("HDMI display is connected\n");
            status = 0;
        }
        else
            printf("HDMI display is not connected\n");

        /* Select the capture device */
        status = select_capture_device( videofd );
        if ( !status )
            status = configure_capture_parameters( videofd, pixel_format,
                colorspace, capture_source );

        if ( !status )
            status = configure_auxiliary_framebuffer( 1 );

        if ( !status )
            status = capture_on( videofd );
    }
    else
    {
        /* Don't stop the capture hardware - instead make the auxiliary display opaque */
        status = configure_auxiliary_framebuffer( 0 );
    }
 
    close( videofd );

    return 0;
}
