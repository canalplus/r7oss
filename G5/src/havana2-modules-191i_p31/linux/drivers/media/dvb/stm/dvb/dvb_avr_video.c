/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.
 * V4L2 dvp output device driver for ST SoC display subsystems.
************************************************************************/

// #define OFFSET_THE_IMAGE

/******************************
 * INCLUDES
 ******************************/
#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/videodev.h>
#include <linux/dvb/video.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "dvb_module.h"
#include "backend.h"
#include "dvb_video.h"
#include "osdev_device.h"
#include "ksound.h"
#include "linux/dvb/stm_ioctls.h"
#include "pseudo_mixer.h"
#include "monitor_inline.h"

#include "dvb_avr_video.h"

/******************************
 * FUNCTION PROTOTYPES
 ******************************/

extern struct class_device* player_sysfs_get_class_device(void *playback, void* stream);
extern int player_sysfs_get_stream_id(struct class_device* stream_dev);
extern void player_sysfs_new_attribute_notification(struct class_device *stream_dev);

/******************************
 * LOCAL VARIABLES
 ******************************/

#define inrange(v,l,u) 			(((v) >= (l)) && ((v) <= (u)))
#define Clamp(v,l,u)			{ if( (v) < (l) ) v = (l); else if ( (v) > (u) ) v = (u); }
#define min( a, b )			(((a)<(b)) ? (a) : (b))
#define max( a, b )			(((a)>(b)) ? (a) : (b))
#define Abs( a )			(((a)<0) ? (-a) : (a))

#define DvpTimeForNFrames(N)		((((N) * Context->StreamInfo.FrameRateDenominator * 1000000ULL) + (Context->StreamInfo.FrameRateNumerator/2)) / Context->StreamInfo.FrameRateNumerator)
#define DvpCorrectedTimeForNFrames(N)	((DvpTimeForNFrames((N)) * Context->DvpFrameDurationCorrection) >> DVP_CORRECTION_FIXED_POINT_BITS);

#define DvpRange(T)			(((T * DVP_MAX_SUPPORTED_PPM_CLOCK_ERROR) / 1000000) + 1)
#define DvpValueMatchesFrameTime(V,T)	inrange( V, T - DvpRange(T), T + DvpRange(T) )

#define ControlValue(s)			((Context->DvpControl##s == DVP_USE_DEFAULT) ? Context->DvpControlDefault##s : Context->DvpControl##s)

#define GetValue( s, v )		printk( "%s\n", s ); *Value = v;
#define Check( s, l, u )		printk( "%s\n", s ); if( (Value != DVP_USE_DEFAULT) && !inrange((int)Value,l,u) ) {printk( "Set Control - Value out of range (%d not in [%d..%d])\n", Value, l, u ); return -1; }
#define CheckAndSet( s, l, u, v )	Check( s, l, u ); v = Value;

/******************************
 * GLOBAL VARIABLES
 ******************************/


/**
 * Lookup table to help the sysfs callbacks locate the context structure.
 *
 * It really is rather difficult to get hold of a context variable from a sysfs callback. The sysfs module
 * does however provide a means for us to discover the stream id. Since the sysfs attribute is unique to
 * each stream we can use the id to index into a table and thus grab hold of a context variable.
 *
 * Thus we are pretty much forced to use a global variable but at least it doesn't stop us being
 * multi-instance.
 */
static dvp_v4l2_video_handle_t *VideoContextLookupTable[16];

/******************************
 * SUPPORTED VIDEO MODES
 ******************************/

/* 
 * Capture Interface version of ModeParamsTable in the stmfb driver
 */
static const dvp_v4l2_video_mode_line_t DvpModeParamsTable[] =
{
  /* 
   * SD/ED modes 
   */
  { DVP_720_480_I60000, { 60000, SCAN_I, 720, 480, 119, 36 }, { false, false } },
  { DVP_720_480_P60000, { 60000, SCAN_P, 720, 480, 122, 36 }, { false, false } },
  { DVP_720_480_I59940, { 59940, SCAN_I, 720, 480, 119, 36 }, { false, false } },  
  { DVP_720_480_P59940, { 59940, SCAN_P, 720, 480, 122, 36 }, { false, false } },
  { DVP_640_480_I59940, { 59940, SCAN_I, 640, 480, 118, 38 }, { false, false } },  
  { DVP_720_576_I50000, { 50000, SCAN_I, 720, 576, 132, 44 }, { false, false } },
  { DVP_720_576_P50000, { 50000, SCAN_P, 720, 576, 132, 44 }, { false, false } },
  { DVP_768_576_I50000, { 50000, SCAN_I, 768, 576, 155, 44 }, { false, false } },

  /*
   * Modes used in VCR trick modes
   */
  { DVP_720_240_P59940, { 59940, SCAN_P, 720, 240, 119, 18 }, { false, false } },
  { DVP_720_240_P60000, { 60000, SCAN_P, 720, 240, 119, 18 }, { false, false } },
  { DVP_720_288_P50000, { 50000, SCAN_P, 720, 288, 132, 22 }, { false, false } },

  /*
   * 1080p modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { DVP_1920_1080_P60000, { 60000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { DVP_1920_1080_P59940, { 59940, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { DVP_1920_1080_P50000, { 50000, SCAN_P, 1920, 1080, 192, 41}, { true, true } },
  { DVP_1920_1080_P30000, { 30000, SCAN_P, 1920, 1080, 192, 41}, { true, true } },
  { DVP_1920_1080_P29970, { 29970, SCAN_P, 1920, 1080, 192, 41}, { true, true } },
  { DVP_1920_1080_P25000, { 25000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { DVP_1920_1080_P24000, { 24000, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },
  { DVP_1920_1080_P23976, { 23976, SCAN_P, 1920, 1080, 192, 41 }, { true, true } },

  /*
   * 1080i modes, SMPTE 274M (analogue) and CEA-861-C (HDMI)
   */
  { DVP_1920_1080_I60000, { 60000, SCAN_I, 1920, 1080, 192, 40 }, { true, true } },
  { DVP_1920_1080_I59940, { 59940, SCAN_I, 1920, 1080, 192, 40 }, { true, true } },
  { DVP_1920_1080_I50000_274M, { 50000, SCAN_I, 1920, 1080, 192, 40 }, { true, true} },
  /* Australian 1080i. */
  { DVP_1920_1080_I50000_AS4933, { 50000, SCAN_I, 1920, 1080, 352, 124 }, { true, true } },

  /*
   * 720p modes, SMPTE 296M (analogue) and CEA-861-C (HDMI)
   */
  { DVP_1280_720_P60000, { 60000, SCAN_P, 1280, 720, 260, 25 }, { true, true } },
  { DVP_1280_720_P59940, { 59940, SCAN_P, 1280, 720, 260, 25 }, { true, true } },
  { DVP_1280_720_P50000, { 50000, SCAN_P, 1280, 720, 260, 25 }, { true, true } },

  /*
   * A 1280x1152@50Hz Australian analogue HD mode.
   */
  { DVP_1280_1152_I50000, { 50000, SCAN_I, 1280, 1152, 235, 178 }, { true, true } },

  /*
   * good old VGA, the default fallback mode for HDMI displays, note that
   * the same video code is used for 4x3 and 16x9 displays and it is up to
   * the display to determine how to present it (see CEA-861-C). Also note
   * that CEA specifies the pixel aspect ratio is 1:1 .
   */
  { DVP_640_480_P59940, {59940, SCAN_P, 640, 480, 144, 35}, { false, false } },
  { DVP_640_480_P60000, {60000, SCAN_P, 640, 480, 144, 35}, { false, false } }
};

/******************************
 * Horizontal sample rate 
 * converter filter constants
 ******************************/

typedef struct ResizingFilter_s
{
    unsigned int	ForScalingFactorLessThan;	// 0x100 is x 1, 0x200 means downscaling, halving the dimension
    unsigned char	Coefficients[40];		// 40 for horizontal, 24 for vertical
} ResizingFilter_t;

//

#define ScalingFactor(N)		((1000 * 256)/N)		// Scaling factor in 1000s
#define WORDS_PER_VERTICAL_FILTER	6
#define WORDS_PER_HORIZONTAL_FILTER	10

static ResizingFilter_t VerticalResizingFilters[] =
{

    { ScalingFactor(850),
		{ 0x00, 0x40, 0x00,
		  0x09, 0x3d, 0xfa,
		  0x13, 0x37, 0xf6,
		  0x1d, 0x2f, 0xf4,
		  0x26, 0x26, 0xf4,
		  0x2f, 0x1b, 0xf6,
		  0x36, 0x11, 0xf9,
		  0x3b, 0x08, 0xfd } },

    { ScalingFactor(600),
		{ 0x09, 0x2e, 0x09,
		  0x0f, 0x2e, 0x03,
		  0x16, 0x2b, 0xff,
		  0x1d, 0x28, 0xfb,
		  0x25, 0x23, 0xf8,
		  0x2c, 0x1e, 0xf6,
		  0x33, 0x18, 0xf5,
		  0x39, 0x11, 0xf6 } },

    { ScalingFactor(375),
		{ 0x10, 0x20, 0x10,
		  0x13, 0x20, 0x0d,
		  0x17, 0x1f, 0x0a,
		  0x1a, 0x1f, 0x07,
		  0x1e, 0x1e, 0x04,
		  0x22, 0x1d, 0x01,
		  0x26, 0x1b, 0xff,
		  0x2b, 0x19, 0xfc } },

    { ScalingFactor(175),
		{ 0x14, 0x18, 0x14,
		  0x15, 0x18, 0x13,
		  0x16, 0x17, 0x13,
		  0x17, 0x17, 0x12,
		  0x17, 0x18, 0x11,
		  0x18, 0x18, 0x10,
		  0x19, 0x18, 0x0f,
		  0x1a, 0x17, 0x0f } },

    { 0xffffffff,
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

    { ScalingFactor(850),
		{ 0x00, 0x00, 0x40, 0x00, 0x00,
		  0xfd, 0x09, 0x3c, 0xfa, 0x04,
		  0xf9, 0x13, 0x39, 0xf5, 0x06,
		  0xf5, 0x1f, 0x31, 0xf3, 0x08,
		  0xf3, 0x2a, 0x28, 0xf3, 0x08,
		  0xf3, 0x34, 0x1d, 0xf5, 0x07,
		  0xf5, 0x3b, 0x12, 0xf9, 0x05,
		  0xfa, 0x3f, 0x07, 0xfd, 0x03 } },

    { ScalingFactor(600),
		{ 0xf7, 0x0b, 0x3c, 0x0b, 0xf7,
		  0xf5, 0x13, 0x3a, 0x04, 0xfa,
		  0xf4, 0x1c, 0x34, 0xfe, 0xfe,
		  0xf5, 0x23, 0x2e, 0xfa, 0x00,
		  0xf7, 0x29, 0x26, 0xf7, 0x03,
		  0xfa, 0x2d, 0x1e, 0xf6, 0x05,
		  0xff, 0x30, 0x15, 0xf6, 0x06,
		  0x03, 0x31, 0x0f, 0xf7, 0x06 } },

    { ScalingFactor(375),
		{ 0xfb, 0x13, 0x24, 0x13, 0xfb,
		  0xfd, 0x17, 0x23, 0x0f, 0xfa,
		  0xff, 0x1a, 0x23, 0x0b, 0xf9,
		  0x01, 0x1d, 0x21, 0x08, 0xf9,
		  0x04, 0x20, 0x1f, 0x04, 0xf9,
		  0x07, 0x22, 0x1c, 0x01, 0xfa,
		  0x0b, 0x23, 0x18, 0xff, 0xfb,
		  0x0f, 0x24, 0x14, 0xfd, 0xfc } },

    { ScalingFactor(175),
		{ 0x09, 0x0f, 0x10, 0x0f, 0x09,
		  0x0a, 0x0f, 0x11, 0x0e, 0x08,
		  0x0a, 0x10, 0x11, 0x0e, 0x07,
		  0x0b, 0x10, 0x12, 0x0d, 0x06,
		  0x0c, 0x11, 0x12, 0x0c, 0x05,
		  0x0d, 0x12, 0x11, 0x0c, 0x04,
		  0x0e, 0x12, 0x11, 0x0b, 0x04,
		  0x0f, 0x13, 0x11, 0x0a, 0x03 } },

    { 0xffffffff,
		{ 0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
		  0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
		  0x0c, 0x0d, 0x0e, 0x0d, 0x0c,
		  0x0d, 0x0d, 0x0d, 0x0d, 0x0c,
		  0x0d, 0x0d, 0x0d, 0x0d, 0x0c,
		  0x0d, 0x0d, 0x0e, 0x0d, 0x0b,
		  0x0d, 0x0e, 0x0d, 0x0d, 0x0b,
		  0x0d, 0x0e, 0x0d, 0x0d, 0x0b } }
};

/******************************
 * SYSFS TOOLS
 ******************************/

static dvp_v4l2_video_handle_t *DvpVideoSysfsLookupContext (struct class_device *class_dev)
{
    int streamid = player_sysfs_get_stream_id(class_dev);
    BUG_ON(streamid < 0 ||
	   streamid >= ARRAY_SIZE(VideoContextLookupTable) ||
	   NULL == VideoContextLookupTable[streamid]);
    return VideoContextLookupTable[streamid];
}

// Define the macro for creating fns and declaring the attribute

// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE( DvpName, LinuxName )							\
static ssize_t DvpVideoSysfsShow##DvpName( struct class_device *class_dev, char *buf )		\
{												\
    int v = atomic_read(&DvpVideoSysfsLookupContext(class_dev)->Dvp##DvpName);			\
    return sprintf(buf, "%d\n", v);								\
}												\
												\
static ssize_t DvpVideoSysfsStore##DvpName( struct class_device *class_dev, const char *buf, size_t count )	\
{												\
    int v;											\
												\
    if (1 != sscanf(buf, "%i", &v))								\
       return -EINVAL;										\
												\
    if (v < 0)											\
       return -EINVAL;										\
												\
    atomic_set(&DvpVideoSysfsLookupContext(class_dev)->Dvp##DvpName, v);			\
    return count;										\
}												\
												\
static CLASS_DEVICE_ATTR(LinuxName, 0600, DvpVideoSysfsShow##DvpName, DvpVideoSysfsStore##DvpName)
// ------------------------------------------------------------------------------------------------
#define SYSFS_CREATE( DvpName, LinuxName )							\
{												\
int Result;											\
												\
    Result  = class_device_create_file(VideoContext->DvpSysfsClassDevice,			\
                                      &class_device_attr_##LinuxName);				\
    if (Result) {										\
       DVB_ERROR("class_device_create_file failed (%d)\n", Result);				\
        return -1;										\
    }												\
												\
    player_sysfs_new_attribute_notification(VideoContext->DvpSysfsClassDevice);			\
    atomic_set(&VideoContext->Dvp##DvpName, 0);							\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_PROCESS_DECREMENT( DvpName, LinuxName )				\
SYSFS_DECLARE( DvpName, LinuxName );								\
												\
static void DvpVideoSysfsProcessDecrement##DvpName( dvp_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Dvp##DvpName);					\
												\
       if (new > 0)										\
           new--;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Dvp##DvpName, old, new));	\
												\
    if (old == 1)										\
       sysfs_notify (&((*(VideoContext->DvpSysfsClassDevice)).kobj), NULL, #LinuxName );	\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_INTERRUPT_DECREMENT( DvpName, LinuxName )				\
SYSFS_DECLARE( DvpName, LinuxName );								\
												\
static void DvpVideoSysfsInterruptDecrement##DvpName( dvp_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Dvp##DvpName);					\
												\
       if (new > 0)										\
           new--;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Dvp##DvpName, old, new));	\
												\
    if (old == 1)										\
    {												\
	VideoContext->Dvp##DvpName##Notify	= true;						\
	up( &VideoContext->DvpSynchronizerWakeSem );						\
    }												\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_INTERRUPT_INCREMENT( DvpName, LinuxName )				\
SYSFS_DECLARE( DvpName, LinuxName );								\
												\
static void DvpVideoSysfsInterruptIncrement##DvpName( dvp_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Dvp##DvpName);					\
												\
       if( new == 0)										\
           new++;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Dvp##DvpName, old, new));	\
												\
    if (old == 0)										\
    {												\
	VideoContext->Dvp##DvpName##Notify	= true;						\
	up( &VideoContext->DvpSynchronizerWakeSem );						\
    }												\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_INTERRUPT_SET( DvpName, LinuxName )					\
SYSFS_DECLARE( DvpName, LinuxName );								\
												\
static void DvpVideoSysfsInterruptSet##DvpName( dvp_v4l2_video_handle_t *VideoContext, unsigned int Value )	\
{												\
    int old;											\
												\
    /* Set and notify transition. */								\
												\
    old = atomic_read(&VideoContext->Dvp##DvpName);						\
    atomic_set(&VideoContext->Dvp##DvpName, Value );						\
    												\
    if (old != Value)										\
    {												\
	VideoContext->Dvp##DvpName##Notify	= true;						\
	up( &VideoContext->DvpSynchronizerWakeSem );						\
    }												\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_TEST_NOTIFY( DvpName, LinuxName )							\
{												\
    if(	Context->Dvp##DvpName##Notify )								\
    {												\
	Context->Dvp##DvpName##Notify	= false;						\
	sysfs_notify (&((*(Context->DvpSysfsClassDevice)).kobj), NULL, #LinuxName );		\
    }												\
}
// ------------------------------------------------------------------------------------------------

SYSFS_DECLARE_WITH_INTERRUPT_SET( 	MicroSecondsPerFrame,			microseconds_per_frame );
SYSFS_DECLARE_WITH_INTERRUPT_DECREMENT( FrameCountingNotification, 		frame_counting_notification );
SYSFS_DECLARE_WITH_INTERRUPT_DECREMENT( FrameCaptureNotification,  		frame_capture_notification );
SYSFS_DECLARE_WITH_PROCESS_DECREMENT(   OutputCropTargetReachedNotification, 	output_crop_target_reached_notification );
SYSFS_DECLARE_WITH_INTERRUPT_INCREMENT(	PostMortem,				post_mortem );

/**
 * Create the sysfs attributes unique to A/VR video streams.
 *
 * There is no partnering destroy method because destruction is handled automatically by the class device
 * machinary.
 */
static int DvpVideoSysfsCreateAttributes (dvp_v4l2_video_handle_t *VideoContext)
{
    int Result 						= 0;
    playback_handle_t playerplayback 			= NULL;
    stream_handle_t playerstream 			= NULL;
    int streamid;

    Result = DvbStreamGetPlayerEnvironment (VideoContext->DeviceContext->VideoStream, &playerplayback, &playerstream);
    if (Result < 0) {
	DVB_ERROR("StreamGetPlayerEnvironment failed\n");
	return -1;
    }

    VideoContext->DvpSysfsClassDevice = player_sysfs_get_class_device(playerplayback, playerstream);
    if (VideoContext->DvpSysfsClassDevice == NULL) {
    	DVB_ERROR("get_class_device failed -> cannot create attribute \n");
        return -1;
    }

    streamid = player_sysfs_get_stream_id(VideoContext->DvpSysfsClassDevice);
    if (streamid < 0 || streamid >= ARRAY_SIZE(VideoContextLookupTable)) {
	DVB_ERROR("streamid out of range -> cannot create attribute\n");
	return -1;
    }

    VideoContextLookupTable[streamid] = VideoContext;

//

    SYSFS_CREATE( MicroSecondsPerFrame,			microseconds_per_frame );
    SYSFS_CREATE( FrameCountingNotification, 		frame_counting_notification );
    SYSFS_CREATE( FrameCaptureNotification,  		frame_capture_notification );
    SYSFS_CREATE( OutputCropTargetReachedNotification, 	output_crop_target_reached_notification );
    SYSFS_CREATE( PostMortem,				post_mortem );

//

    return 0;
}

// ////////////////////////////////////////////////////////////////////////////

int DvpVideoClose( dvp_v4l2_video_handle_t	*Context )
{

int Result;

//

    if( Context->VideoRunning )
    {
        Context->VideoRunning 		= false;

        Context->Synchronize		= false;
        Context->SynchronizeEnabled	= false;
        Context->SynchronizerRunning	= false;

        up( &Context->DvpPreInjectBufferSem );
        up( &Context->DvpVideoInterruptSem );
        up( &Context->DvpSynchronizerWakeSem );

	while( Context->DeviceContext->VideoStream != NULL )
	{
	unsigned int  Jiffies = ((HZ)/1000)+1;

	    set_current_state( TASK_INTERRUPTIBLE );
	    schedule_timeout( Jiffies );
	}

	DvpVideoIoctlAncillaryRequestBuffers( Context, 0, 0, NULL, NULL );

	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );

	Result	= DvbDisplayDelete (BACKEND_VIDEO_ID, Context->DeviceContext->Id);
	if( Result )
	{
	    printk("Error in %s: DisplayDelete failed\n",__FUNCTION__);
	    return -EINVAL;
	}

	free_irq( Context->DvpIrq, Context );
    }

    avr_set_vsync_offset( Context->SharedContext, 0 );

//

    return 0;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to configure the vertical resize coefficients
//

static int DvpConfigureVerticalResizeCoefficients( 	dvp_v4l2_video_handle_t	*Context,
							unsigned int		 ScalingFactorStep )
{
volatile int 		*DvpRegs	= Context->DvpRegs;
unsigned int		 i;
unsigned int		 Index;
int			*Table;

    //
    // Select the table of ceofficients
    //

    for( Index  = 0;  
	 (ScalingFactorStep > VerticalResizingFilters[Index].ForScalingFactorLessThan);
         Index++ );

    Table	= (int *)(&VerticalResizingFilters[Index].Coefficients);

    //
    // Program the filter
    //

    for( i=0; i<WORDS_PER_VERTICAL_FILTER; i++ )
	DvpRegs[GAM_DVP_VFC(i)]	= Table[i];
 
    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to configure the horizontal resize coefficients
//

static int DvpConfigureHorizontalResizeCoefficients( 	dvp_v4l2_video_handle_t	*Context,
							unsigned int		 ScalingFactorStep )
{
volatile int 		*DvpRegs	= Context->DvpRegs;
unsigned int		 i;
unsigned int		 Index;
int			*Table;

    //
    // Select the table of ceofficients
    //

    for( Index  = 0;  
	 (ScalingFactorStep > HorizontalResizingFilters[Index].ForScalingFactorLessThan);
         Index++ );

    Table	= (int *)(&HorizontalResizingFilters[Index].Coefficients);

    //
    // Program the filter
    //

    for( i=0; i<WORDS_PER_HORIZONTAL_FILTER; i++ )
	DvpRegs[GAM_DVP_HFC(i)]	= Table[i];
 
    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to calculate what scalings should be applied to 
//	input data, based on the crop values and the current input mode.
//

static int DvpRecalculateScaling( dvp_v4l2_video_handle_t	*Context )
{
unsigned int		 ScaledInputWidth;		// Uncropped values
unsigned int		 ScaledInputHeight;
unsigned int		 ScalingInputWidth;		// Cropped values
unsigned int		 ScalingInputHeight;
unsigned int		 ScalingOutputWidth;
unsigned int		 ScalingOutputHeight;
unsigned int		 NewHSRC;
unsigned int		 NewVSRC;
unsigned int		 BytesPerLine;

//

    if( Context == NULL )
        return -1;			// Too early to do anything

    //
    // Enter the lock, and initialize the scaled input crop
    //

    down_interruptible( &Context->DvpScalingStateLock );
    Context->ScaledInputCrop	= Context->InputCrop;

    ScalingInputWidth	= (Context->ScaledInputCrop.Width != 0) ? Context->ScaledInputCrop.Width : Context->ModeWidth;
    ScalingInputHeight	= (Context->ScaledInputCrop.Height != 0) ? Context->ScaledInputCrop.Height : Context->ModeHeight;
    ScalingOutputWidth	= (Context->OutputCrop.Width != 0) ? Context->OutputCrop.Width : Context->ModeWidth;
    ScalingOutputHeight	= (Context->OutputCrop.Height != 0) ? Context->OutputCrop.Height : Context->ModeHeight;

    if( (ScalingInputWidth == 0) || (ScalingOutputWidth == 0) )
    {
	up( &Context->DvpScalingStateLock );
        return -1;			// Too early to do anything
    }

    //
    // We limit any downscaling to 1/8th
    //

    ScalingOutputWidth	= max( ScalingOutputWidth, ScalingInputWidth/8 );
    ScalingOutputHeight	= max( ScalingOutputHeight, ScalingInputHeight/8 );

    //
    // Now calculate the step values, note we only resize if the input is larger than the output.
    //

    NewHSRC		= DVP_SRC_OFF_VALUE;
    NewVSRC		= DVP_SRC_OFF_VALUE;
    ScaledInputWidth	= Context->ModeWidth;		// Not cropped version
    ScaledInputHeight	= Context->ModeHeight;		// Not cropped version

    if( ControlValue(HorizontalResizeEnable) &&
	(ScalingInputWidth > ScalingOutputWidth) )
    {
	NewHSRC				= 0x01000000 | (((ScalingInputWidth -1) * 256) / (ScalingOutputWidth-1));

	Context->ScaledInputCrop.X	= ((Context->ScaledInputCrop.X     * ScalingOutputWidth) / ScalingInputWidth);
	Context->ScaledInputCrop.Width	= ((Context->ScaledInputCrop.Width * ScalingOutputWidth) / ScalingInputWidth);

	ScaledInputWidth		= ((Context->ModeWidth * ScalingOutputWidth) / ScalingInputWidth);
    }

//

    if( ControlValue(VerticalResizeEnable) &&
	(ScalingInputHeight > ScalingOutputHeight) )
    {
	NewVSRC				= 0x01000000 | (((ScalingInputHeight -1) * 256) / (ScalingOutputHeight-1));

	Context->ScaledInputCrop.Y	= ((Context->ScaledInputCrop.Y      * ScalingOutputHeight) / ScalingInputHeight);
	Context->ScaledInputCrop.Height	= ((Context->ScaledInputCrop.Height * ScalingOutputHeight) / ScalingInputHeight);

	ScaledInputHeight		= ((Context->ModeHeight * ScalingOutputHeight) / ScalingInputHeight);
    }

    //
    // Calculate the other new register values.
    // This unfortunately is where I need to know the bytes per line
    // value. I import here, the knowledge that the display hardware
    // requires a 64 byte (32 pixel) allignment on a line.
    //

    BytesPerLine			= Context->BufferBytesPerPixel * ((ScaledInputWidth + 31) & 0xffffffe0);

    Context->NextWidth			= ScaledInputWidth;
    Context->NextHeight			= ScaledInputHeight;
    Context->NextRegisterCVS		= ScaledInputWidth | ((ScaledInputHeight / (Context->StreamInfo.interlaced ? 2 : 1)) << 16);;
    Context->NextRegisterVMP		= BytesPerLine * (Context->StreamInfo.interlaced ? 2 : 1);
    Context->NextRegisterVBPminusVTP	= (Context->StreamInfo.interlaced ? BytesPerLine : 0);
    Context->NextRegisterHSRC		= NewHSRC;
    Context->NextRegisterVSRC		= NewVSRC;
    Context->NextInputWindow		= Context->ScaledInputCrop;
    Context->NextOutputWindow		= Context->OutputCrop;

    up( &Context->DvpScalingStateLock );

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to get a buffer
//

static int DvpGetVideoBuffer( dvp_v4l2_video_handle_t	*Context )
{
int			 Result;
unsigned int		 Dimensions[2];
unsigned int		 BufferIndex;
unsigned int         SurfaceFormat;
DvpBufferStack_t	*Record;

//

    Record					= &Context->DvpBufferStack[Context->DvpNextBufferToGet % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];
    memset( Record, 0x00, sizeof(DvpBufferStack_t) );

//

    down_interruptible( &Context->DvpScalingStateLock );

    Record->Width				= Context->NextWidth;
    Record->Height				= Context->NextHeight;
    Record->RegisterCVS				= Context->NextRegisterCVS;
    Record->RegisterVMP				= Context->NextRegisterVMP;
    Record->RegisterVBPminusVTP			= Context->NextRegisterVBPminusVTP;
    Record->RegisterHSRC			= Context->NextRegisterHSRC;
    Record->RegisterVSRC			= Context->NextRegisterVSRC;

    Record->InputWindow				= Context->NextInputWindow;
    Record->OutputWindow			= Context->NextOutputWindow;

    Dimensions[0]				= Context->NextWidth;
    Dimensions[1]				= Context->NextHeight;

    SurfaceFormat                               = SURF_YCBCR422R;
   
    switch (SurfaceFormat)
    { 
       case SURF_RGB565:
           Context->BufferBytesPerPixel  = 0x2;
           break;
       case SURF_RGB888:
           Context->BufferBytesPerPixel  = 0x3;
           break;
       case SURF_ARGB8888:
           Context->BufferBytesPerPixel  = 0x4;
           break;       
       case SURF_YCBCR422R:
           Context->BufferBytesPerPixel  = 0x2;
    }

    up(  &Context->DvpScalingStateLock );

//

    Result 	= DvbStreamGetDecodeBuffer(	Context->DeviceContext->VideoStream,
						&Record->Buffer,
						&Record->Data,
						SurfaceFormat,
						2, Dimensions,
						&BufferIndex,
						&Context->BytesPerLine );

    if (Result != 0)
    {
	printk( "Error in %s: StreamGetDecodeBuffer failed\n", __FUNCTION__ );
	return -1;
    }

    if( Context->BytesPerLine != (Dimensions[0] * Context->BufferBytesPerPixel) )
    {
	printk( "Error in %s: StreamGetDecodeBuffer returned an unexpected bytes per line value (%d instead of %d)\n", __FUNCTION__, Context->BytesPerLine, (Dimensions[0] * Context->BufferBytesPerPixel) );
    }

//

    Context->DvpNextBufferToGet++;

    return 0;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to release currently held, but not injected buffers
//

static int DvpReleaseBuffers( dvp_v4l2_video_handle_t	*Context )
{
int			 Result;
DvpBufferStack_t	*Record;

//

    while( Context->DvpNextBufferToInject < Context->DvpNextBufferToGet )
    {
	Record	= &Context->DvpBufferStack[Context->DvpNextBufferToInject % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];

	Result	= DvbStreamReturnDecodeBuffer( Context->DeviceContext->VideoStream, Record->Buffer );
	if( Result < 0 )
	{
	    printk("Error in %s: StreamReturnDecodeBuffer failed\n", __FUNCTION__);
	    // No point returning, we may as well try and release the rest
	}

	Context->DvpNextBufferToInject++;
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to inject a buffer to the player
//

static int DvpInjectVideoBuffer( dvp_v4l2_video_handle_t	*Context )
{
int			 Result;
DvpBufferStack_t	*Record;
unsigned long long	 ElapsedFrameTime;
unsigned long long	 PresentationTime;
unsigned long long	 Pts;
StreamInfo_t		 Packet;

//

    if( Context->DvpNextBufferToInject >= Context->DvpNextBufferToGet )
    {
	printk( "DvpInjectVideoBuffer - No buffer yet to inject.\n" );
	return -1;
    }

    //
    // Make the call to set the time mapping.
    //

    Pts	= (((Context->DvpBaseTime * 27) + 150) / 300) & 0x00000001ffffffffull;

    Result = avr_set_external_time_mapping (Context->SharedContext, Context->DeviceContext->VideoStream, Pts, Context->DvpBaseTime );
    if( Result < 0 )
    {
	printk("DvpInjectVideoBuffer - dvp_enable_external_time_mapping failed\n" );
	return Result;
    }

//

    Record				= &Context->DvpBufferStack[Context->DvpNextBufferToInject % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    //
    // Calculate the expected fill time, Note the correction factor on the incoming values has 1 as 2^DVP_CORRECTION_FIXED_POINT_BITS.
    //

    Context->DvpCalculatingFrameTime	= true;

    ElapsedFrameTime			= DvpCorrectedTimeForNFrames(Context->DvpFrameCount + Context->DvpLeadInVideoFrames);

    Context->DvpDriftFrameCount++;
    Context->DvpLastDriftCorrection	= -(Context->DvpCurrentDriftError * Context->DvpDriftFrameCount) / (long long)(2 * DVP_MAXIMUM_TIME_INTEGRATION_FRAMES);

    Record->ExpectedFillTime		= Context->DvpBaseTime + ElapsedFrameTime + Context->DvpLastDriftCorrection;

    //
    // Rebase our calculation values
    //

    if( ElapsedFrameTime >= (1ULL << 31) )
    {
	Context->DvpDriftFrameCount	= 0;				// We zero the drift data, because it is encapsulated in the ExpectedFillTime
	Context->DvpLastDriftCorrection	= 0;
	Context->DvpBaseTime		= Record->ExpectedFillTime;
	Context->DvpFrameCount		= -Context->DvpLeadInVideoFrames;
    }

    Context->DvpCalculatingFrameTime	= false;

    //
    // Construct a packet to inject the information - NOTE we adjust the time to cope for a specific video latency
    //

    PresentationTime		= Record->ExpectedFillTime + Context->AppliedLatency;
    Pts				 = (((PresentationTime * 27) + 150) / 300) & 0x00000001ffffffffull;

    memcpy( &Packet, &Context->StreamInfo, sizeof(StreamInfo_t) );

    Packet.buffer 		= phys_to_virt( (unsigned int)Record->Data );
    Packet.buffer_class		= Record->Buffer;
    Packet.width		= Record->Width;
    Packet.height		= Record->Height;
    Packet.top_field_first	= ControlValue(TopFieldFirst);
    Packet.h_offset		= 0;
    Packet.InputWindow		= Record->InputWindow;
    Packet.OutputWindow		= Record->OutputWindow;
    Packet.pixel_aspect_ratio.Numerator	= Context->DeviceContext->PixelAspectRatio.Numerator;
    Packet.pixel_aspect_ratio.Denominator = Context->DeviceContext->PixelAspectRatio.Denominator;

//printk( "ElapsedFrameTime = %12lld - %016llx - %12lld, %12lld - %016llx\n", ElapsedFrameTime, Pts, StreamInfo.FrameRateNumerator, StreamInfo.FrameRateDenominator, DvpFrameDurationCorrection );

    Result 	= DvbStreamInjectPacket( Context->DeviceContext->VideoStream, (const unsigned char*)(&Packet), sizeof(StreamInfo_t), true, Pts );
    if (Result < 0)
    {
	printk("Error in %s: StreamInjectDataPacket failed\n", __FUNCTION__);
	return Result;
    }

    //
    // The ownership of the buffer has now been passed to the player, so we release our hold
    //

    Result	= DvbStreamReturnDecodeBuffer( Context->DeviceContext->VideoStream, Record->Buffer );
    if( Result < 0 )
    {
	printk("Error in %s: StreamReturnDecodeBuffer failed\n", __FUNCTION__);
	// No point in returning
    }

    //
    // Move on to next buffer
    //

    Context->DvpFrameCount++;
    Context->DvpNextBufferToInject++;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to configure the capture buffer pointers
//
unsigned long capture_address;
static int DvpConfigureNextCaptureBuffer( dvp_v4l2_video_handle_t	*Context,
					  unsigned long long		 Now )
{
int			 i;
volatile int 		*DvpRegs	= Context->DvpRegs;
DvpBufferStack_t        *Record;
bool			 DroppedAFrame;
unsigned long long	 ShouldSkip;

    //
    // Try to move on a buffer
    //	    We ensure that the timing for capture is right to move on.
    //	    in error, we stay with the current buffer
    //

    DroppedAFrame		= false;
    Record			= &Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    if( (Context->DvpNextBufferToFill+1) >= Context->DvpNextBufferToGet )
    {
	// Is there a buffer to move onto
//	printk( "DVP Video - No buffer to move onto - We dropped a frame (%d, %d) %d\n", Context->DvpNextBufferToFill, Context->DvpNextBufferToGet, Context->DvpPreInjectBufferSem.count );
	printk( "DVP DF\n" );					// Drasticaly shortened message we still need to see this, but the long message forces the condition to continue rather than fixing it
	DroppedAFrame	= true;
    }
    else if (inrange((Now - Record->ExpectedFillTime), ((7 * DvpTimeForNFrames(1)) / 8), 0x8000000000000000ULL) )
    {
	// is it time to move onto it
	Context->DvpNextBufferToFill++;
	Record			= &Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];
        up( &Context->DvpPreInjectBufferSem );
    }
    else
    {
	// Not time, inform the user
	printk( "DVP Video - Too early to fill buffer(%d), Discarding a frame (%lld)\n", Context->DvpNextBufferToFill, (Record->ExpectedFillTime - Now) );
	DroppedAFrame	= true;
    }

    Context->DvpMissedFramesInARow	= DroppedAFrame ? (Context->DvpMissedFramesInARow + 1) : 0;

    //
    // If we moved on, check that we are not late for the new buffer, 
    // if we are then stick with it, but adjust the times to compensate
    //

    if( !DroppedAFrame && inrange((Now - Record->ExpectedFillTime), DvpTimeForNFrames(1), 0x8000000000000000ULL) )
    {
	ShouldSkip	= ((Now - Record->ExpectedFillTime) + (DvpTimeForNFrames(1) / 8) - 1) / DvpTimeForNFrames(1);

	printk( "DVP Video - Too late to fill buffer(%d), should have skipped %lld buffers (Late by %lld)\n", Context->DvpNextBufferToFill, ShouldSkip, (Now - Record->ExpectedFillTime) );

	//
	// Now fudge up the time
	//

	ShouldSkip		 = DvpTimeForNFrames(1) * ShouldSkip;

	Context->DvpBaseTime	+= ShouldSkip;

	for( i = Context->DvpNextBufferToFill;
	     i < Context->DvpNextBufferToGet;
	     i++ )
	    Context->DvpBufferStack[i % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime	+= ShouldSkip;
    }

    //
    // Update the Horizontal and Vertical re-sizing 
    //

    if( Record->RegisterHSRC != Context->LastRegisterHSRC )
    {
	Context->RegisterCTL		&= ~(1 << 10);			// HRESIZE_EN           - Clear out the resize enable bit

	if( Record->RegisterHSRC != DVP_SRC_OFF_VALUE )
	{
	    DvpConfigureHorizontalResizeCoefficients( Context, (Record->RegisterHSRC & 0x00000fff) );
	    Context->RegisterCTL		|= (1 << 10);		// HRESIZE_EN           - Enable horizontal resize
	}

	Context->LastRegisterHSRC	= Record->RegisterHSRC;
    }

//

    if( Record->RegisterVSRC != Context->LastRegisterVSRC )
    {
	Context->RegisterCTL		&= ~(1 << 11);			// VRESIZE_EN           - Clear out the resize enable bit

	if( Record->RegisterVSRC != DVP_SRC_OFF_VALUE )
	{
	    DvpConfigureVerticalResizeCoefficients( Context, (Record->RegisterVSRC & 0x00000fff) );
	    Context->RegisterCTL		|= (1 << 11);		// VRESIZE_EN           - Enable vertical resize
	}

	Context->LastRegisterVSRC	= Record->RegisterVSRC;
    }

    //
    // Move onto the new buffer
    //

#ifdef OFFSET_THE_IMAGE
    DvpRegs[GAM_DVP_VTP]	= (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP);
    DvpRegs[GAM_DVP_VBP]	= (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP) + Record->RegisterVBPminusVTP;
    capture_address = (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP);
#else
    DvpRegs[GAM_DVP_VTP]	= (unsigned int)Record->Data;
    DvpRegs[GAM_DVP_VBP]	= (unsigned int)Record->Data + Record->RegisterVBPminusVTP;
    capture_address = (unsigned int)Record->Data;
#endif

    DvpRegs[GAM_DVP_HSRC]	= Record->RegisterHSRC;
    DvpRegs[GAM_DVP_VSRC]	= Record->RegisterVSRC;
    DvpRegs[GAM_DVP_VMP]	= Record->RegisterVMP;
    DvpRegs[GAM_DVP_CVS]	= Record->RegisterCVS;

    DvpRegs[GAM_DVP_CTL]	= Context->RegisterCTL;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to halt the capture hardware
//

static int DvpHaltCapture( dvp_v4l2_video_handle_t	*Context )
{
unsigned int		 Tmp;
volatile int 		*DvpRegs	= Context->DvpRegs;

    //
    // Mark state
    //

    Context->DvpState		= DvpMovingToInactive;

    //
    // make sure nothing is going on
    //

    DvpRegs[GAM_DVP_CTL]	= 0x80080000;
    DvpRegs[GAM_DVP_ITM]	= 0x00;
    Tmp				= DvpRegs[GAM_DVP_ITS];

    //
    // Mark state
    //

    Context->DvpState		= DvpInactive;

    //
    // Make sure no one thinks an ancillary capture is in progress
    //

    Context->AncillaryCaptureInProgress	= false;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to configure the capture hardware
//

static int DvpParseModeValues( dvp_v4l2_video_handle_t	*Context )
{
const dvp_v4l2_video_mode_params_t	*ModeParams;
const dvp_v4l2_video_timing_params_t	*TimingParams;
dvp_v4l2_video_mode_t		 	 Mode;
bool					 Interlaced;
unsigned int				 SixteenBit;
unsigned int				 HOffset;
unsigned int				 Width;
unsigned int				 VOffset;
unsigned int				 Height;
unsigned int				 TopVoffset;
unsigned int				 BottomVoffset;
unsigned int				 TopHeight;
unsigned int				 BottomHeight;

//

    ModeParams		= &Context->DvpCaptureMode->ModeParams;
    TimingParams	= &Context->DvpCaptureMode->TimingParams;
    Mode 		= Context->DvpCaptureMode->Mode;

    //
    // Modify default control values based on current mode
    //     NOTE because some adjustments have an incestuous relationship,
    //		we need to calculate some defaults twice.
    //

    Context->DvpControlDefault16Bit				= ModeParams->ActiveAreaWidth > 732;
    Context->DvpControlDefaultOddPixelCount			= ((ModeParams->ActiveAreaWidth + ControlValue(ActiveAreaAdjustWidth)) & 1);
    Context->DvpControlDefaultExternalVRefPolarityPositive	= TimingParams->VSyncPolarity;
    Context->DvpControlDefaultHRefPolarityPositive		= TimingParams->HSyncPolarity;
    Context->DvpControlDefaultVideoLatency			= (int)Context->DvpLatency;

    //
    // Setup the stream info based on the current capture mode
    //

    Context->StandardFrameRate				= true;

    if( ModeParams->FrameRate == 59940 )
    {
	Context->StreamInfo.FrameRateNumerator		= 60000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( ModeParams->FrameRate == 29970 )
    {
	Context->StreamInfo.FrameRateNumerator		= 30000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( ModeParams->FrameRate == 23976 )
    {
	Context->StreamInfo.FrameRateNumerator		= 24000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else
    {
	Context->StreamInfo.FrameRateNumerator		= ModeParams->FrameRate;
	Context->StreamInfo.FrameRateDenominator	= 1000;
    }

    if (ModeParams->ScanType == SCAN_I)
	Context->StreamInfo.FrameRateNumerator		/= 2;


    DvpVideoSysfsInterruptSetMicroSecondsPerFrame(Context, DvpTimeForNFrames(1) );

//

    if( DvpTimeForNFrames(1) > 20000 )				// If frame time is more than 20ms then use counts appropriate to 30 or less fps
    {
	Context->DvpWarmUpVideoFrames	= DVP_WARM_UP_VIDEO_FRAMES_30;
	Context->DvpLeadInVideoFrames	= DVP_LEAD_IN_VIDEO_FRAMES_30;
    }
    else
    {
	Context->DvpWarmUpVideoFrames	= DVP_WARM_UP_VIDEO_FRAMES_60;
	Context->DvpLeadInVideoFrames	= DVP_LEAD_IN_VIDEO_FRAMES_60;
    }

    //
    // Calculate how many buffers we will need to pre-inject.
    // That is the buffers injected ahead of filling.
    //

    Context->DvpBuffersRequiredToInjectAhead	= DVP_MAXIMUM_PLAYER_TRANSIT_TIME / DvpTimeForNFrames(1) + 1;

    //
    // Fill in the stream info fields
    //

    Context->ModeWidth			= ModeParams->ActiveAreaWidth  + ControlValue(ActiveAreaAdjustWidth);
    Context->ModeHeight			= ModeParams->ActiveAreaHeight + ControlValue(ActiveAreaAdjustHeight);

    Context->StreamInfo.interlaced	= ModeParams->ScanType == SCAN_I;
    Context->StreamInfo.h_offset	= 0;
    Context->StreamInfo.v_offset	= 0;
    Context->StreamInfo.VideoFullRange	= ControlValue(FullRange);
    Context->StreamInfo.ColourMode	= ControlValue(ColourMode);

    //
    // Lock in the state of control values
    //

    Context->SynchronizeEnabled		= ControlValue(VsyncLockEnable);
    Context->AppliedLatency		= ControlValue(VideoLatency) - Context->DvpLatency;

    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED, 
			Context->SynchronizeEnabled ? PLAY_OPTION_VALUE_ENABLE : PLAY_OPTION_VALUE_DISABLE );

    //
    // Based on the mode<dimension> values, recalculate the scaling values.
    // clearing the input crop as a matter of course (since the crop may well 
    // be invalid for the specified mode).
    //

    memset( &Context->InputCrop, 0x00, sizeof(DvpRectangle_t) );
    DvpRecalculateScaling( Context );

    //
    // Precalculate the register values
    //

    Interlaced			= Context->StreamInfo.interlaced;
    SixteenBit			= ControlValue(16Bit);

    HOffset			= ModeParams->ActiveAreaXStart + ControlValue(ActiveAreaAdjustHorizontalOffset);
    Width			= ModeParams->ActiveAreaWidth  + ControlValue(ActiveAreaAdjustWidth);
    VOffset			= ModeParams->FullVBIHeight    + ControlValue(ActiveAreaAdjustVerticalOffset);
    Height			= ModeParams->ActiveAreaHeight + ControlValue(ActiveAreaAdjustHeight);

#ifdef OFFSET_THE_IMAGE
// NAUGHTY - Since we are offseting the image, we need to crop it, or it will write past the buffer end
Width /= 2;
Height /= 2;
#endif

#if 0
    TopVoffset			= Context->StreamInfo.interlaced ? ((VOffset + 1) / 2) : VOffset;
    BottomVoffset		= Context->StreamInfo.interlaced ? (VOffset / 2) : VOffset;
#else
    TopVoffset			= Context->StreamInfo.interlaced ? (VOffset / 2) : VOffset;
    BottomVoffset		= Context->StreamInfo.interlaced ? (VOffset / 2) : VOffset;
#endif
    TopHeight			= Context->StreamInfo.interlaced ? ((Height + 1) / 2) : Height;
    BottomHeight		= Context->StreamInfo.interlaced ? (Height / 2) : Height;

    Context->RegisterTFO	= ((HOffset     * (SixteenBit ? 1 : 2)) | (TopVoffset          << 16));
    Context->RegisterTFS	= (((Width - 1) * (SixteenBit ? 1 : 2)) | ((TopHeight - 1)     << 16)) + Context->RegisterTFO;
    Context->RegisterBFO	= ((HOffset     * (SixteenBit ? 1 : 2)) | (BottomVoffset       << 16));
    Context->RegisterBFS	= (((Width -1)  * (SixteenBit ? 1 : 2)) | ((BottomHeight - 1)  << 16)) + Context->RegisterBFO;
    Context->RegisterHLL	= Width / (SixteenBit ? 2 : 1);

    //
    // Setup the control register values (No constants, so I commented each field)
    //

    Context->RegisterCTL	= (ControlValue(16Bit)				<< 30) |	// HD_EN		- 16 bit capture
				  (ControlValue(BigEndian) 			<< 23) |	// BIG_NOT_LITTLE	- Big endian format
				  (ControlValue(FullRange)			<< 16) |	// EXTENDED_1_254	- Clip input to 1..254 rather than 16..235 luma and 16..240 chroma
				  (ControlValue(ExternalSynchroOutOfPhase)	<< 15) |	// SYNCHRO_PHASE_NOTOK	- External H and V signals not in phase
				  (ControlValue(ExternalVRefOddEven)		<<  9) |	// ODDEVEN_NOT_VSYNC	- External vertical reference is an odd/even signal
				  (ControlValue(OddPixelCount)			<<  8) |	// PHASE[1]		- Number of pixels to capture is odd
				  (ControlValue(IncompleteFirstPixel)		<<  7) |	// PHASE[0]		- First pixel is incomplete (Y1 not CB0_Y0_CR0)
				  (ControlValue(ExternalVRefPolarityPositive) 	<<  6) |	// V_REF_POL		- External Vertical counter reset on +ve edge of VREF
				  (ControlValue(HRefPolarityPositive)		<<  5) |	// H_REF_POL		- Horizontal counter reset on +ve edge of HREF
				  (ControlValue(ExternalSync)			<<  4) |	// EXT_SYNC		- Use external HSI/VSI
				  (ControlValue(VsyncBottomHalfLineEnable)	<<  3) |	// VSYNC_BOT_HALF_LINE_EN - VSOUT starts at the middle of the last top field line
				  (ControlValue(ExternalSyncPolarity)		<<  2) |	// EXT_SYNC_POL		- external sync polarity
				  (0                                            <<  1) |	// ANCILLARY_DATA_EN	- ANC/VBI data collection enable managed throughout
				  (1						<<  0);		// VID_EN		- Enable capture

    if( Context->StreamInfo.interlaced )
	Context->RegisterCTL	 	|= (1 << 29);		// Reserved		- I am guessing this is an interlaced flag

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to configure the capture hardware
//

static int DvpConfigureCapture( dvp_v4l2_video_handle_t	*Context )
{
volatile int 		*DvpRegs	= Context->DvpRegs;
DvpBufferStack_t        *Record;
unsigned int		 Tmp;

//

    Record			= &Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    //
    // make sure nothing is going on
    //

    DvpRegs[GAM_DVP_CTL]	= 0x80080000;
    DvpRegs[GAM_DVP_ITM]	= 0x00;

    //
    // Clean up the ancillary data - match the consequences of the following writes
    // also capture the control variable that we do not allow to change during streams
    //

    Context->AncillaryInputBufferInputPointer		= Context->AncillaryInputBufferUnCachedAddress;
    memset( Context->AncillaryInputBufferUnCachedAddress, 0x00, Context->AncillaryInputBufferSize );

    Context->AncillaryPageSizeSpecified			= ControlValue(AncPageSizeSpecified);
    Context->AncillaryPageSize				= ControlValue(AncPageSize);

    //
    // Update to incorporate the resizing
    //

    if( Record->RegisterHSRC != DVP_SRC_OFF_VALUE )
    {
	DvpConfigureHorizontalResizeCoefficients( Context, (Record->RegisterHSRC & 0x00000fff) );
	Context->RegisterCTL		|= (1 << 10);		// HRESIZE_EN           - Enable horizontal resize
    }

    Context->LastRegisterHSRC	= Record->RegisterHSRC;

//

    if( Record->RegisterVSRC != DVP_SRC_OFF_VALUE )
    {
	DvpConfigureVerticalResizeCoefficients( Context, (Record->RegisterVSRC & 0x00000fff) );
	Context->RegisterCTL		|= (1 << 11);		// VRESIZE_EN           - Enable vertical resize
    }

    Context->LastRegisterVSRC	= Record->RegisterVSRC;

    //
    // Program the structure registers
    //

    DvpRegs[GAM_DVP_TFO]	= Context->RegisterTFO;
    DvpRegs[GAM_DVP_TFS]	= Context->RegisterTFS;
    DvpRegs[GAM_DVP_BFO]	= Context->RegisterBFO;
    DvpRegs[GAM_DVP_BFS]	= Context->RegisterBFS;
    DvpRegs[GAM_DVP_VTP]	= (unsigned int)Record->Data;
    DvpRegs[GAM_DVP_VBP]	= (unsigned int)Record->Data + Record->RegisterVBPminusVTP;
    DvpRegs[GAM_DVP_VMP]	= Record->RegisterVMP;
    DvpRegs[GAM_DVP_CVS]	= Record->RegisterCVS;
    DvpRegs[GAM_DVP_VSD]	= 0;					// Set synchronization delays to zero
    DvpRegs[GAM_DVP_HSD]	= 0;
    DvpRegs[GAM_DVP_HLL]	= Context->RegisterHLL;
    DvpRegs[GAM_DVP_HSRC]	= Record->RegisterHSRC;
    DvpRegs[GAM_DVP_VSRC]	= Record->RegisterVSRC;
    DvpRegs[GAM_DVP_PKZ]	= 0x2;					// Set packet size to 4 ST bus words

    DvpRegs[GAM_DVP_ABA]	= (unsigned int)Context->AncillaryInputBufferPhysicalAddress;
    DvpRegs[GAM_DVP_AEA]	= (unsigned int)Context->AncillaryInputBufferPhysicalAddress + Context->AncillaryInputBufferSize - DVP_ANCILLARY_BUFFER_CHUNK_SIZE;	// Note address of last 128bit word
    DvpRegs[GAM_DVP_APS]	= Context->AncillaryPageSize;

    Tmp				= DvpRegs[GAM_DVP_ITS];			// Clear interrupts before enabling
    DvpRegs[GAM_DVP_ITM]	= (1 << 4);				// Interested in Vsync top only

    DvpRegs[GAM_DVP_CTL]	= Context->RegisterCTL;			// Let er rip

//

#if 0
printk( "%08x %08x %08x %08x - %08x %08x %08x %08x - %08x %08x %08x %08x - %08x %08x\n",
		DvpRegs[GAM_DVP_TFO], DvpRegs[GAM_DVP_TFS], DvpRegs[GAM_DVP_BFO], DvpRegs[GAM_DVP_BFS],
		DvpRegs[GAM_DVP_VTP], DvpRegs[GAM_DVP_VBP], DvpRegs[GAM_DVP_VMP], DvpRegs[GAM_DVP_CVS],
		DvpRegs[GAM_DVP_VSD], DvpRegs[GAM_DVP_HSD], DvpRegs[GAM_DVP_HLL], DvpRegs[GAM_DVP_HSRC],
		DvpRegs[GAM_DVP_VSRC], DvpRegs[GAM_DVP_CTL] );
#endif

//

    return 0;

}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to start capturing data
//

static int DvpStartup( dvp_v4l2_video_handle_t	*Context )
{
int			 Result;

    //
    // Configure the capture
    //

    Context->DvpCalculatingFrameTime	= false;
    Context->DvpBaseTime		= INVALID_TIME;
    Context->DvpFrameCount		= 0;
    Context->DvpFrameDurationCorrection	= DVP_CORRECTION_FACTOR_ONE;
    Context->DvpTotalElapsedCaptureTime	= 0;
    Context->DvpTotalCapturedFrameCount	= 0;
    Context->DvpCurrentDriftError	= 0;
    Context->DvpLastDriftCorrection	= 0;
    Context->DvpDriftFrameCount		= 0;
    Context->DvpState			= DvpStarting;

    avr_invalidate_external_time_mapping(Context->SharedContext);

    sema_init( &Context->DvpVideoInterruptSem, 0 );
    sema_init( &Context->DvpPreInjectBufferSem, Context->DvpBuffersRequiredToInjectAhead-1 );

    Result				= DvpConfigureCapture( Context );
    if( Result < 0 )
    {
	printk( "DvpStartup - Failed to configure capture hardware.\n" );
	return -EINVAL;
    }

//

    while( (Context->DvpState != DvpStarted) && Context->VideoRunning && !Context->FastModeSwitch )
	down_interruptible( &Context->DvpVideoInterruptSem );

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to enter the run state, where we capture and move onto the next buffer
//

static int DvpRun( dvp_v4l2_video_handle_t	*Context )
{
unsigned long long	ElapsedFrameTime;

    //
    // Set the start time, and switch to moving to run state
    //

    ElapsedFrameTime			= DvpTimeForNFrames(Context->DvpLeadInVideoFrames + 1);
    Context->DvpRunFromTime		= Context->DvpBaseTime + ElapsedFrameTime - 8000;	// Allow for jitter by subtracting 8ms

    Context->DvpState			= DvpMovingToRun;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to stop capturing data
//

static int DvpStop( dvp_v4l2_video_handle_t	*Context )
{
int			 Result;

    //
    // Halt the capture
    //

    Result			= DvpHaltCapture( Context );
    if( Result < 0 )
    {
	printk( "Error in %s: Failed to halt capture hardware.\n", __FUNCTION__ );
	return -EINVAL;
    }

    //
    // And adjust the state accordingly
    //

    Context->DvpBaseTime	= INVALID_TIME;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to handle a warm up failure
//	    What warmup failure usually means, is that the
//	    frame rate is either wrong, or the source is
//	    way way out of spec.
//

static int DvpFrameRate(	dvp_v4l2_video_handle_t	*Context,
				unsigned long long 	 MicroSeconds,
				unsigned int		 Frames )
{
unsigned int		i;
unsigned long long 	MicroSecondsPerFrame;
unsigned int		NewDvpBuffersRequiredToInjectAhead;

    //
    // First convert the us int microseconds per frame
    //

    MicroSecondsPerFrame	= (MicroSeconds + (Frames/2)) / Frames;

    printk( "DvpFrameRate - MicroSecondsPerFrame = %5lld (%6lld, %4d)\n", MicroSecondsPerFrame, MicroSeconds, Frames );

//

    Context->StandardFrameRate	= true;

    if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 16667) )				// 60fps = 16666.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 60;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 16683) )			// 59fps = 16683.33 us
    {
	Context->StreamInfo.FrameRateNumerator		= 60000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 20000) )			// 50fps = 20000.00 us
    {
	Context->StreamInfo.FrameRateNumerator		= 50;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 33333) )			// 30fps = 33333.33 us
    {
	Context->StreamInfo.FrameRateNumerator		= 30;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 33367) )			// 29fps = 33366.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 30000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 40000) )			// 25fps = 40000.00 us
    {
	Context->StreamInfo.FrameRateNumerator		= 25;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 41667) )			// 24fps = 41666.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 24;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( DvpValueMatchesFrameTime(MicroSecondsPerFrame, 41708) )			// 23fps = 41708.33 us
    {
	Context->StreamInfo.FrameRateNumerator		= 24000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( MicroSecondsPerFrame != Context->StreamInfo.FrameRateDenominator )	// If it has changed since the last time
    {
	Context->StandardFrameRate			= false;
	Context->StreamInfo.FrameRateNumerator		= 1000000;
	Context->StreamInfo.FrameRateDenominator	= MicroSecondsPerFrame;
    }
										// If it was non standard, but has not changed since
										// last integration, we let it become the new standard.

//

    if( MicroSecondsPerFrame < 32000 )						// Has the interlaced flag been set incorrectly
	Context->StreamInfo.interlaced 			= false;

    DvpVideoSysfsInterruptSetMicroSecondsPerFrame(Context, DvpTimeForNFrames(1));

//

    if( Context->StandardFrameRate )
	printk( "DvpFrameRate - Framerate = %lld/%lld (%s)\n", Context->StreamInfo.FrameRateNumerator, Context->StreamInfo.FrameRateDenominator,
								(Context->StreamInfo.interlaced ? "Interlaced":"Progressive") );

    //
    // Recalculate how many buffers we will need to pre-inject.
    // If this has increased, then allow more injections by performing up
    // on the appropriate semaphore.
    //

    NewDvpBuffersRequiredToInjectAhead		= DVP_MAXIMUM_PLAYER_TRANSIT_TIME / DvpTimeForNFrames(1) + 1;

    if( NewDvpBuffersRequiredToInjectAhead > Context->DvpBuffersRequiredToInjectAhead )
    {
	for( i = Context->DvpBuffersRequiredToInjectAhead; i<NewDvpBuffersRequiredToInjectAhead; i++ )
	    up( &Context->DvpPreInjectBufferSem );

	Context->DvpBuffersRequiredToInjectAhead = NewDvpBuffersRequiredToInjectAhead;
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks function to handle a warm up failure
//	    What warmup failure usually means, is that the
//	    frame rate is either wrong, or the source is
//	    way way out of spec.
//

static int DvpWarmUpFailure( dvp_v4l2_video_handle_t	*Context,
			     unsigned long long 	 MicroSeconds )
{
//    printk( "$$$ DvpWarmUpFailure %5d => %5lld %5lld $$$\n", DvpTimeForNFrames(1), DvpTimeForNFrames(Context->DvpInterruptFrameCount), MicroSeconds );

    //
    // Select an appropriate frame rate
    //

    DvpFrameRate( Context, MicroSeconds, Context->DvpInterruptFrameCount );

    //
    // Automatically switch to a slow startup
    //

    Context->DvpWarmUpVideoFrames	= DVP_WARM_UP_VIDEO_FRAMES_60;
    Context->DvpLeadInVideoFrames	= DVP_LEAD_IN_VIDEO_FRAMES_60;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks dvp ancillary data capture interrupt handler
//

static int DvpAncillaryCaptureInterrupt( dvp_v4l2_video_handle_t	*Context )
{
bool			 AncillaryCaptureWasInProgress;
unsigned int		 Index;
volatile int 		*DvpRegs;
unsigned int		 Size;
unsigned char		*Capturestart;
unsigned int		 Transfer0;
unsigned int		 Transfer1;

//

    DvpRegs				= Context->DvpRegs;
    AncillaryCaptureWasInProgress	= Context->AncillaryCaptureInProgress;

    Capturestart			= Context->AncillaryInputBufferInputPointer;
    Transfer0				= 0;
    Transfer1				= 0;

    //
    // Is there any ongoing captured data
    //

    if( Context->AncillaryCaptureInProgress )
    {
	//
	// Capture the message packets
	//

	while( Context->AncillaryInputBufferInputPointer[0] )
	{
	    if( Context->AncillaryPageSizeSpecified )
	    {
		Context->AncillaryInputBufferInputPointer	+= Context->AncillaryPageSize;
	    }
	    else
	    {
		// Length of packet is in byte 6 (but the hardware only captures from the third onwards)
		// The length is in 32 bit words, and is in the bottom 6 bits of the word. 
		// There are 6 header bytes captured in total. 
		// The hardware will write in 16 byte chunks (I see though doc says 8)

		Size						 = (Context->AncillaryInputBufferInputPointer[3] & 0x3f) * 4;
		Size						+= 6;
		Size						 = (Size + 15) & 0xfffffff0;
		Context->AncillaryInputBufferInputPointer	+= Size;
	    }

	    if( (Context->AncillaryInputBufferInputPointer - Context->AncillaryInputBufferUnCachedAddress) >= Context->AncillaryInputBufferSize )
		Context->AncillaryInputBufferInputPointer	-= Context->AncillaryInputBufferSize;
	}

	//
	// Calculate transfer sizes ( 0 before end of circular buffer, 1 after wrap point of circular buffer)
	//

	if( Context->AncillaryInputBufferInputPointer < Capturestart )
	{
	    Transfer0	= (Context->AncillaryInputBufferUnCachedAddress + Context->AncillaryInputBufferSize) - Capturestart;
	    Transfer1	= Context->AncillaryInputBufferInputPointer - Context->AncillaryInputBufferUnCachedAddress;
	}
	else
	{
	    Transfer0	= Context->AncillaryInputBufferInputPointer - Capturestart;
	    Transfer1	= 0;
	}
    }

    //
    // If any data was captured handle it
    //

    if( (Transfer0 + Transfer1) > 0 ) 
    {
	//
	// Copy into any user buffer
	//

	if( Context->AncillaryStreamOn )
	{
	    if( Context->AncillaryBufferNextFillIndex == Context->AncillaryBufferNextQueueIndex )
	    {
		// printk( "DvpAncillaryCaptureInterrupt - No buffer queued to capture into.\n" );
	    }
	    else if( (Transfer0 + Transfer1) > Context->AncillaryBufferSize )
	    {
		printk( "DvpAncillaryCaptureInterrupt - Captured data too large for buffer (%d bytes), discarded.\n", (Transfer0 + Transfer1) );
	    }
	    else
	    {
		Index		= Context->AncillaryBufferQueue[Context->AncillaryBufferNextFillIndex % DVP_MAX_ANCILLARY_BUFFERS];
		Context->AncillaryBufferState[Index].Queued	= false;
		Context->AncillaryBufferState[Index].Done	= true;
		Context->AncillaryBufferState[Index].Bytes	= (Transfer0 + Transfer1);
		Context->AncillaryBufferState[Index].FillTime	= Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime;

		memcpy( Context->AncillaryBufferState[Index].UnCachedAddress, Capturestart, Transfer0 );
		if( Transfer1 != 0 )
		    memcpy( Context->AncillaryBufferState[Index].UnCachedAddress + Transfer0, Context->AncillaryInputBufferUnCachedAddress, Transfer1 );

		Context->AncillaryBufferNextFillIndex++;

		up(  &Context->DvpAncillaryBufferDoneSem );
	    }
	}

	//
	// Clear out buffer (need zeroes to recognize data written, a pointer register would have been way too useful)
	//

	memset( Capturestart, 0x00, Transfer0 );
	if( Transfer1 != 0 )
	    memset( Context->AncillaryInputBufferUnCachedAddress, 0x00, Transfer1 );
    }

    //
    // Do we wish to continue capturing
    //

    Context->AncillaryCaptureInProgress	= Context->AncillaryStreamOn;

    if( Context->AncillaryCaptureInProgress != AncillaryCaptureWasInProgress )
    {
	Context->RegisterCTL	= Context->RegisterCTL					|
				  (Context->AncillaryPageSizeSpecified		<< 13)	|	// VALID_ANC_PAGE_SIZE_EXT - During ANC (VBI) collection size will be specified in APS register
				  (Context->AncillaryCaptureInProgress ? (1 << 1) : 0);		// ANCILLARY_DATA_EN       - Capture enable

	DvpRegs[GAM_DVP_CTL]	= Context->RegisterCTL;
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks dvp interrupt handler
//

#if defined (CONFIG_KERNELVERSION)  // STLinux 2.3
int DvpInterrupt(int irq, void* data)
#else
int DvpInterrupt(int irq, void* data, struct pt_regs* pRegs)
#endif
{
int			 i;
dvp_v4l2_video_handle_t	*Context;
volatile int 		*DvpRegs;
unsigned int		 DvpIts;
ktime_t			 Ktime;
unsigned long long	 Now;
unsigned long long	 EstimatedBaseTime;
unsigned long long	 EstimatedBaseTimeRange;
bool			 ThrowAnIntegration;
bool			 ArithmeticOverflowLikely;
bool			 UpdateCorrectionFactor;
bool			 HistoricDifference0Sign;
unsigned long long	 HistoricDifference0;
bool			 HistoricDifference1Sign;
unsigned long long	 HistoricDifference1;
unsigned long long	 NewTotalElapsedTime;
unsigned int		 NewTotalFrameCount;
unsigned long long	 NewCorrectionFactor;
long long 		 CorrectionFactorChange;
long long		 AffectOfChangeOnPreviousFrameTimes;
long long		 DriftError;
long long 		 DriftLimit;
unsigned int		 Interrupts;

//

    Context		= (dvp_v4l2_video_handle_t *)data;
    DvpRegs		= Context->DvpRegs;

    DvpIts		= DvpRegs[GAM_DVP_ITS];

    Ktime 		= ktime_get();
    Now			= ktime_to_us( Ktime );

    Context->DvpInterruptFrameCount++;

//printk( "DvpInterrupt - %d, %08x, %016llx - %lld, %d\n", Context->DvpState, DvpIts, Now, DvpTimeForNFrames(1), Context->DvpInterruptFrameCount );

    switch( Context->DvpState )
    {
	case DvpInactive:
			printk( "DvpInterrupt - Dvp inactive - possible implementation error.\n" );
			DvpHaltCapture( Context );				// Try and halt it
			break;

	case DvpStarting:
			Context->DvpBaseTime				= Now + DvpTimeForNFrames(1);	// Force trigger
			Context->DvpInterruptFrameCount			= 0;
			Context->DvpwarmUpSynchronizationAttempts	= 0;
			Context->DvpMissedFramesInARow			= 0;
			Context->DvpState				= DvpWarmingUp;

	case DvpWarmingUp:
			EstimatedBaseTime				= Now - DvpCorrectedTimeForNFrames(Context->DvpInterruptFrameCount);
			EstimatedBaseTimeRange				= 1 + (DvpTimeForNFrames(Context->DvpInterruptFrameCount) * DVP_MAX_SUPPORTED_PPM_CLOCK_ERROR)/1000000;

			if( !inrange( Context->DvpBaseTime, (EstimatedBaseTime - EstimatedBaseTimeRange) , (EstimatedBaseTime + EstimatedBaseTimeRange)) &&
			    (Context->DvpwarmUpSynchronizationAttempts < DVP_WARM_UP_TRIES) )
			{
//			    printk( "DvpInterrupt - Base adjustment %4lld(%5lld) (%016llx[%d] - %016llx)\n", EstimatedBaseTime - Context->DvpBaseTime, DvpTimeForNFrames(1), EstimatedBaseTime, Context->DvpInterruptFrameCount, Context->DvpBaseTime );
			    Context->DvpBaseTime			= Now;
			    Context->DvpInterruptFrameCount		= 0;
			    Context->DvpTimeAtZeroInterruptFrameCount	= Context->DvpBaseTime;
			    Context->Synchronize			= Context->SynchronizeEnabled;		// Trigger vsync lock
			    Context->DvpwarmUpSynchronizationAttempts++;
			    up( &Context->DvpSynchronizerWakeSem );
			}

			if( Context->DvpInterruptFrameCount < Context->DvpWarmUpVideoFrames )
			    break;

//printk( "Warm up tries was %d\n", Context->DvpwarmUpSynchronizationAttempts );

			if( Context->DvpwarmUpSynchronizationAttempts >= DVP_WARM_UP_TRIES )
			    DvpWarmUpFailure( Context, (Now - Context->DvpBaseTime) );

			up( &Context->DvpVideoInterruptSem );

			Context->DvpState				= DvpStarted;
			Context->DvpIntegrateForAtLeastNFrames		= DVP_MINIMUM_TIME_INTEGRATION_FRAMES;

	case DvpStarted:
                        MonitorSignalEvent( MONITOR_EVENT_VIDEO_FIRST_FIELD_ACQUIRED, NULL, "DvpInterrupt: First field acquired" );
			break;


	case DvpMovingToRun:

//printk( "Moving %12lld  %12lld - %016llx %016llx\n", (Now - Context->DvpRunFromTime), (Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime - Now), Context->DvpRunFromTime, Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime );

			if( Now < Context->DvpRunFromTime )
			    break;

			Context->DvpState				= DvpRunning;

	case DvpRunning:
			//
			// Monitor for missed interrupts, this can throw our calculations out dramatically
			//

			Interrupts	= ((Now - Context->DvpTimeOfLastFrameInterrupt + (DvpTimeForNFrames(1) / 2)) / DvpTimeForNFrames(1));
			if( Interrupts > 1 )
			{
			    Context->DvpInterruptFrameCount	+= Interrupts - 1;		// Compensate for the missed interrupts
			    printk( "$$$$$$$$$$ Missed an interrupt %d\n", Interrupts - 1 );
			}

			//
			// Switch to next capture buffers.
			//

			DvpAncillaryCaptureInterrupt( Context );
			DvpConfigureNextCaptureBuffer( Context, Now );

			//
			// Keep the user up to date w.r.t. their frame timer/
			//

			DvpVideoSysfsInterruptDecrementFrameCountingNotification(Context);
			DvpVideoSysfsInterruptDecrementFrameCaptureNotification(Context);

			if( Context->DvpMissedFramesInARow > DVP_MAX_MISSED_FRAMES_BEFORE_POST_MORTEM )
			    DvpVideoSysfsInterruptIncrementPostMortem(Context);

			//
			// Do we wish to re-calculate the correction value,
			// have we integrated for long enough, and is this value
			// Unjittered.
			//

			DriftError	= Context->DvpBufferStack[Context->DvpNextBufferToFill % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime - Now;

			if( Context->DvpCalculatingFrameTime ||
			    (Context->DvpInterruptFrameCount < Context->DvpIntegrateForAtLeastNFrames) ||
			    ((Context->DvpInterruptFrameCount < (2 * Context->DvpIntegrateForAtLeastNFrames)) &&
			        ((Now - Context->DvpTimeOfLastFrameInterrupt) > (DvpTimeForNFrames(1) + DVP_MAXIMUM_FRAME_JITTER))) )
			{
			    if( !((Now - Context->DvpTimeOfLastFrameInterrupt) > (DvpTimeForNFrames(1) + DVP_MAXIMUM_FRAME_JITTER)) )
				Context->DvpLastFrameDriftError	= DriftError;
			    break;
			}

			//
			// Re-calculate applying a clamp to the change
			//

			UpdateCorrectionFactor			= Context->StandardFrameRate;

			NewTotalElapsedTime			= 0;		// Initialized to keep compiler happy
			NewTotalFrameCount			= 0;
			HistoricDifference0			= 0;
			HistoricDifference1			= 0;
			HistoricDifference0Sign			= false;
			HistoricDifference1Sign			= false;

			AffectOfChangeOnPreviousFrameTimes	= 0;		// Initialize for print purposes

			if( UpdateCorrectionFactor )
			{
			    //
			    // Check for discontinuous change in the correction factor (associated with jump in input clock).
			    // Do fgirst level check if we have some history, second level (detects a lower step but over a 
                            // sustained period) if we have slightly longer history.
			    //

			    if( (Context->DvpNextIntegrationRecord - Context->DvpFirstIntegrationRecord) > 0 )
			    {
				HistoricDifference0	= ((Now - Context->DvpTimeAtZeroInterruptFrameCount) <<DVP_CORRECTION_FIXED_POINT_BITS) / DvpTimeForNFrames(Context->DvpInterruptFrameCount);
				HistoricDifference0Sign	= HistoricDifference0 < Context->DvpFrameDurationCorrection;
				HistoricDifference0	= HistoricDifference0Sign ? 
										(Context->DvpFrameDurationCorrection - HistoricDifference0) :
										(HistoricDifference0 - Context->DvpFrameDurationCorrection);

				if( HistoricDifference0 > (8*DVP_ONE_PPM) )
				{
				    Context->DvpTotalElapsedCaptureTime	= 0;
				    Context->DvpTotalCapturedFrameCount	= 0;

				    Context->DvpNextIntegrationRecord	= 0;
				    Context->DvpFirstIntegrationRecord	= 0;
				}
			    }

			    if( (Context->DvpNextIntegrationRecord - Context->DvpFirstIntegrationRecord) > 1 )
			    {
				HistoricDifference1	= (Context->DvpElapsedCaptureTimeRecord[(Context->DvpNextIntegrationRecord-1) % DVP_MAX_RECORDED_INTEGRATIONS] << DVP_CORRECTION_FIXED_POINT_BITS) / DvpTimeForNFrames(Context->DvpCapturedFrameCountRecord[(Context->DvpNextIntegrationRecord-1) % DVP_MAX_RECORDED_INTEGRATIONS]);
				HistoricDifference1Sign	= HistoricDifference1 < Context->DvpFrameDurationCorrection;
				HistoricDifference1	= HistoricDifference1Sign ? 
										(Context->DvpFrameDurationCorrection - HistoricDifference1) :
										(HistoricDifference1 - Context->DvpFrameDurationCorrection);

				if( (HistoricDifference0 > DVP_ONE_PPM) && 
				    (HistoricDifference1 > DVP_ONE_PPM) &&
				    (HistoricDifference0Sign == HistoricDifference1Sign) )
				{
				    Context->DvpTotalElapsedCaptureTime	= Context->DvpElapsedCaptureTimeRecord[(Context->DvpNextIntegrationRecord-1) % DVP_MAX_RECORDED_INTEGRATIONS];
				    Context->DvpTotalCapturedFrameCount	= Context->DvpCapturedFrameCountRecord[(Context->DvpNextIntegrationRecord-1) % DVP_MAX_RECORDED_INTEGRATIONS];

				    Context->DvpFirstIntegrationRecord	= Context->DvpNextIntegrationRecord - 1;
				}
			    }

			    //
			    // Update Elapsed time and count, discarding old values when we have too many, or we risk an overflow
			    //

			    do
			    {
				NewTotalElapsedTime			= Context->DvpTotalElapsedCaptureTime + (Now - Context->DvpTimeAtZeroInterruptFrameCount);
				NewTotalFrameCount			= Context->DvpTotalCapturedFrameCount + Context->DvpInterruptFrameCount;
				ArithmeticOverflowLikely		= (NewTotalElapsedTime != ((NewTotalElapsedTime << DVP_CORRECTION_FIXED_POINT_BITS) >> DVP_CORRECTION_FIXED_POINT_BITS)) ||
									  ((DvpTimeForNFrames( NewTotalFrameCount ) & 0xffffffff00000000ull) != 0);

				ThrowAnIntegration			= ArithmeticOverflowLikely || 
									  ((Context->DvpNextIntegrationRecord - Context->DvpFirstIntegrationRecord) >= (DVP_MAX_RECORDED_INTEGRATIONS-1));
				if( ThrowAnIntegration )
				{
				    Context->DvpTotalElapsedCaptureTime 	-= Context->DvpElapsedCaptureTimeRecord[Context->DvpFirstIntegrationRecord % DVP_MAX_RECORDED_INTEGRATIONS];
				    Context->DvpTotalCapturedFrameCount 	-= Context->DvpCapturedFrameCountRecord[Context->DvpFirstIntegrationRecord % DVP_MAX_RECORDED_INTEGRATIONS];
				    Context->DvpFirstIntegrationRecord++;
				}
			    } while( ThrowAnIntegration );

			    //
			    // Calculate new factor, and the change
			    //

			    NewCorrectionFactor				= (NewTotalElapsedTime << DVP_CORRECTION_FIXED_POINT_BITS) / DvpTimeForNFrames(NewTotalFrameCount);
			    CorrectionFactorChange			= NewCorrectionFactor - Context->DvpFrameDurationCorrection;

			    Context->DvpFrameDurationCorrection		= NewCorrectionFactor;

			    //
			    // Adjust the base time so that the change only affects frames
			    // after those already calculated.
			    //

			    AffectOfChangeOnPreviousFrameTimes		= DvpTimeForNFrames(Context->DvpFrameCount + Context->DvpLeadInVideoFrames);
			    AffectOfChangeOnPreviousFrameTimes		= (AffectOfChangeOnPreviousFrameTimes * Abs(CorrectionFactorChange)) >> DVP_CORRECTION_FIXED_POINT_BITS;
			    Context->DvpBaseTime			+= (CorrectionFactorChange < 0) ? AffectOfChangeOnPreviousFrameTimes : -AffectOfChangeOnPreviousFrameTimes;
			}

			//
			// Detect an out of range correction factor, and mark our frame rate as invalid
			//

			if( !inrange(Context->DvpFrameDurationCorrection, (DVP_CORRECTION_FACTOR_ONE - (DVP_MAX_SUPPORTED_PPM_CLOCK_ERROR * DVP_ONE_PPM)),
									  (DVP_CORRECTION_FACTOR_ONE + (DVP_MAX_SUPPORTED_PPM_CLOCK_ERROR * DVP_ONE_PPM))) )
			{
			    printk( "DvpInterrupt - Correction factor %lld.%09lld outside reasonable rate, marking frame rate as invalid.\n",
                                    (Context->DvpFrameDurationCorrection >> DVP_CORRECTION_FIXED_POINT_BITS),
                                    (((Context->DvpFrameDurationCorrection & 0x3fffffff) * 1000000000) >> DVP_CORRECTION_FIXED_POINT_BITS) );

			    Context->StandardFrameRate	= false;
			}

			//
			// If running at a non-standard framerate, then try for an
			// update, keep the integration period at the minimum number of frames
			//

			if( !Context->StandardFrameRate )
			{
			    DvpFrameRate( Context, (Now - Context->DvpTimeAtZeroInterruptFrameCount), Context->DvpInterruptFrameCount );

			    //
			    // Reset the correction factor, and all integration data
			    // Note we do not adjust the base time, the change in framerate
			    // has equivalent effect to the correction factor change.
			    //

			    Context->DvpFrameDurationCorrection		= DVP_CORRECTION_FACTOR_ONE;

			    Context->DvpTotalElapsedCaptureTime		= 0;
			    Context->DvpTotalCapturedFrameCount		= 0;

			    Context->DvpNextIntegrationRecord		= 0;
			    Context->DvpFirstIntegrationRecord		= 0;

			    Context->DvpInterruptFrameCount		= 0;
			    Context->DvpTimeAtZeroInterruptFrameCount	= Now;
			    Context->DvpIntegrateForAtLeastNFrames	= DVP_MINIMUM_TIME_INTEGRATION_FRAMES/2;

			    //
			    // If we have had a step change in frame rate, we wish recalculate the base time to 
			    // to make the current expected arrival time of frames as based on now.
			    //

			    for( i = Context->DvpNextBufferToFill;
				 i < Context->DvpNextBufferToGet;
				 i++ )
			        Context->DvpBufferStack[i % DVP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime	= Now + DvpTimeForNFrames(i - Context->DvpNextBufferToFill);

			    Context->DvpFrameCount			= i - Context->DvpNextBufferToFill - Context->DvpLeadInVideoFrames;
			    Context->DvpBaseTime			= Now;
			}

			//
			// Now calculate the drift error - limitting to 2ppm
			//

			Context->DvpCurrentDriftError		= (Abs(DriftError) < Abs(Context->DvpLastFrameDriftError)) ? DriftError : Context->DvpLastFrameDriftError;
			Context->DvpBaseTime			= Context->DvpBaseTime + Context->DvpLastDriftCorrection;
			Context->DvpDriftFrameCount		= 0;
			Context->DvpLastDriftCorrection		= 0;

			DriftLimit				= (1 * DvpTimeForNFrames(DVP_MAXIMUM_TIME_INTEGRATION_FRAMES)) / 500000;
			Clamp( Context->DvpCurrentDriftError, -DriftLimit, DriftLimit );

			//
			// Print out the new state
			//

                        printk( "New Correction factor %lld.%09lld(%d - %3d) [%3lld] - (%5d %6d) %5lld (%lld,%lld)(%s%lld.%09lld %s%lld.%09lld) \n",
                                    (Context->DvpFrameDurationCorrection >> DVP_CORRECTION_FIXED_POINT_BITS),
                                    (((Context->DvpFrameDurationCorrection & 0x3fffffff) * 1000000000) >> DVP_CORRECTION_FIXED_POINT_BITS),
				    UpdateCorrectionFactor, (Context->DvpNextIntegrationRecord - Context->DvpFirstIntegrationRecord),
				    AffectOfChangeOnPreviousFrameTimes,
				    Context->DvpInterruptFrameCount, Context->DvpTotalCapturedFrameCount,
				    Context->DvpCurrentDriftError, DriftError, Context->DvpLastFrameDriftError,
				    (HistoricDifference0Sign  ? "-" : " "),
                                    (HistoricDifference0 >> DVP_CORRECTION_FIXED_POINT_BITS),
                                    (((HistoricDifference0 & 0x3fffffff) * 1000000000) >> DVP_CORRECTION_FIXED_POINT_BITS),
				    (HistoricDifference1Sign  ? "-" : " "),
                                    (HistoricDifference1 >> DVP_CORRECTION_FIXED_POINT_BITS),
                                    (((HistoricDifference1 & 0x3fffffff) * 1000000000) >> DVP_CORRECTION_FIXED_POINT_BITS) );

			//
			// Initialize for next integration
			//

			if( Context->DvpIntegrateForAtLeastNFrames >= DVP_MAXIMUM_TIME_INTEGRATION_FRAMES )
			{
			    if( UpdateCorrectionFactor )
			    {
			    	Context->DvpTotalElapsedCaptureTime	= NewTotalElapsedTime;
			    	Context->DvpTotalCapturedFrameCount	= NewTotalFrameCount;

				Context->DvpElapsedCaptureTimeRecord[Context->DvpNextIntegrationRecord % DVP_MAX_RECORDED_INTEGRATIONS]	= (Now - Context->DvpTimeAtZeroInterruptFrameCount);
				Context->DvpCapturedFrameCountRecord[Context->DvpNextIntegrationRecord % DVP_MAX_RECORDED_INTEGRATIONS]	= Context->DvpInterruptFrameCount;
				Context->DvpNextIntegrationRecord++;
			    }

			    Context->DvpInterruptFrameCount		= 0;
			    Context->DvpTimeAtZeroInterruptFrameCount	= Now;
			    Context->DvpIntegrateForAtLeastNFrames	= DVP_MAXIMUM_TIME_INTEGRATION_FRAMES;
			}
			else
			{
			    Context->DvpIntegrateForAtLeastNFrames	= 2 * Context->DvpIntegrateForAtLeastNFrames;
			}
			break;

	case DvpMovingToInactive:
			break;
    }

    //
    // Update our history
    //

    Context->DvpTimeOfLastFrameInterrupt		= Now;

//

    return IRQ_HANDLED;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks version of the stream event handler
//

static void DvpEventHandler( context_handle_t		 EventContext,
			     struct stream_event_s	*Event )
{
dvp_v4l2_video_handle_t	*Context;

    //
    // Initialize context variables
    //

    Context			= (dvp_v4l2_video_handle_t *)EventContext;

    //
    // Is the event for us - if not just lose it
    //

    switch( Event->code )
    {
	case STREAM_EVENT_VSYNC_OFFSET_MEASURED:
		avr_set_vsync_offset( Context->SharedContext, Event->u.longlong );
		break;

	case STREAM_EVENT_OUTPUT_SIZE_CHANGED:
		if( !Context->OutputCropTargetReached &&
		    (Event->u.rectangle.x == Context->OutputCropTarget.X) &&
		    (Event->u.rectangle.y == Context->OutputCropTarget.Y) &&
		    (Event->u.rectangle.width == Context->OutputCropTarget.Width) &&
		    (Event->u.rectangle.height == Context->OutputCropTarget.Height) )
		{
		    DvpVideoSysfsProcessDecrementOutputCropTargetReachedNotification(Context);
		    Context->OutputCropTargetReached	= true;
		}
		break;

	case STREAM_EVENT_FATAL_HARDWARE_FAILURE:
		DvpVideoSysfsInterruptIncrementPostMortem(Context);
		break;

	default:
		break;
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks version of the function to handle a step in the output crop
//

static void   DvpPerformOutputCropStep( dvp_v4l2_video_handle_t	*Context )
{
unsigned int	Steps;
unsigned int	Step;

    //
    // If no previous crop has been specified, then we 
    // take the mode dimensions as our starting point.
    //

    if( Context->OutputCropStart.Width == 0 )
    {
	Context->OutputCropStart.X	= 0;
	Context->OutputCropStart.Y	= 0;
	Context->OutputCropStart.Width	= Context->ModeWidth;
	Context->OutputCropStart.Height	= Context->ModeHeight;
    }

    //
    // Obtain a set of output crop values
    //

    Step			= ++Context->OutputCropCurrentStep;
    Steps			= Context->OutputCropSteps;

    Context->OutputCrop.X	= ((Context->OutputCropStart.X      * (Steps - Step)) + (Context->OutputCropTarget.X      * Step)) / Steps;
    Context->OutputCrop.Y	= ((Context->OutputCropStart.Y      * (Steps - Step)) + (Context->OutputCropTarget.Y      * Step)) / Steps;
    Context->OutputCrop.Width	= ((Context->OutputCropStart.Width  * (Steps - Step)) + (Context->OutputCropTarget.Width  * Step)) / Steps;
    Context->OutputCrop.Height	= ((Context->OutputCropStart.Height * (Steps - Step)) + (Context->OutputCropTarget.Height * Step)) / Steps;

    if( Step == Steps )
	Context->OutputCropStepping	= false;

    //
    // Round them to even values on all but the last step
    //

    if( Context->OutputCropStepping )
    {
	Context->OutputCrop.X		= (Context->OutputCrop.X      + 1) & 0xfffffffe;
	Context->OutputCrop.Y		= (Context->OutputCrop.Y      + 1) & 0xfffffffe;
	Context->OutputCrop.Width	= (Context->OutputCrop.Width  + 1) & 0xfffffffe;
	Context->OutputCrop.Height	= (Context->OutputCrop.Height + 1) & 0xfffffffe;
    }

    //
    // Now use the values
    //

    DvpRecalculateScaling( Context );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks version of the video dvp thread, re-written to handle low latency
//

static int DvpVideoThread( void *data )
{
dvp_v4l2_video_handle_t	*Context;
int			 Result;

    //
    // Initialize context variables
    //

    Context			= (dvp_v4l2_video_handle_t *)data;

    //
    // Initialize then player with a stream
    //

    Result = DvbPlaybackAddStream( Context->DeviceContext->Playback,
				BACKEND_VIDEO_ID,
				BACKEND_PES_ID,
				BACKEND_DVP_ID,
				DEMUX_INVALID_ID,
				Context->DeviceContext->Id,			// SurfaceId .....
				&Context->DeviceContext->VideoStream );
    if( Result < 0 )
    {
	printk( "DvpVideoThread - PlaybackAddStream failed with %d\n", Result );
	return -EINVAL;
    }

    Result = DvpVideoSysfsCreateAttributes(Context);
    if (Result) {
	printk("DvpVideoThread - Failed to create sysfs attributes\n");
	// non-fatal
    }

    Context->DeviceContext->VideoState.play_state = VIDEO_PLAYING;

    //
    // Interject our event handler
    //

    DvbStreamRegisterEventSignalCallback( 	Context->DeviceContext->VideoStream, (context_handle_t)Context, 
					(stream_event_signal_callback)DvpEventHandler );

    //
    // Set the appropriate policies
    //

    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_VIDEO_ASPECT_RATIO,	VIDEO_FORMAT_16_9 );
    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_VIDEO_DISPLAY_FORMAT,	VIDEO_FULL_SCREEN );
    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_AV_SYNC, 			PLAY_OPTION_VALUE_ENABLE );
    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_DECIMATE_DECODER_OUTPUT,  PLAY_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED );
    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_DISPLAY_FIRST_FRAME_EARLY,PLAY_OPTION_VALUE_DISABLE );

    //
    // Main loop
    //

    Context->VideoRunning		= true;
    while( Context->VideoRunning )
    {
	//
	// Handle setting up and executing a sequence
	//

	Context->DvpNextBufferToGet	= 0;
	Context->DvpNextBufferToInject	= 0;
	Context->DvpNextBufferToFill	= 0;

	Context->FastModeSwitch		= false;

	//
	// Insert check for no standard set up
	//

	while( (Context->DvpCaptureMode == NULL) && Context->VideoRunning )
	{
	    printk( "DvpVideoThread - Attempting to run without a video standard specified, waiting for a VIDIOC_S_STD ioctl call.\n" );
	    down_interruptible( &Context->DvpVideoInterruptSem );

	    Context->FastModeSwitch	= !Context->VideoRunning;
	}

	//

	if( Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= DvpParseModeValues( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= DvpGetVideoBuffer( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= DvpStartup( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= DvpInjectVideoBuffer( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= DvpRun( Context );

	//
	// Enter the main loop for a running sequence
	//

	while( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	{
	    down_interruptible( &Context->DvpPreInjectBufferSem );

	    if( Context->OutputCropStepping )
		DvpPerformOutputCropStep( Context );

	    if( Context->VideoRunning && !Context->FastModeSwitch )
	        Result	= DvpGetVideoBuffer( Context );

	    if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
		Result	= DvpInjectVideoBuffer( Context );
	}

	if( Result < 0 )
	    break;

	//
	// Shutdown the running sequence, either before exiting, or for a format switch
	//

	DvpStop( Context );
	DvbStreamDrain( Context->DeviceContext->VideoStream, 1 );
	DvpReleaseBuffers( Context );
    }

    //
    // Remove our event handler
    //

    DvbStreamRegisterEventSignalCallback( Context->DeviceContext->VideoStream, NULL, NULL );

    //
    // Nothing should be happening, remove the stream from the playback
    //

    Result 				= DvbPlaybackRemoveStream( Context->DeviceContext->Playback, Context->DeviceContext->VideoStream );
    Context->DeviceContext->VideoStream = NULL;

    if( Result < 0 )
    {
	printk("Error in %s: PlaybackRemoveStream failed\n",__FUNCTION__);
	return -EINVAL;
    }
//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Nicks version of the dvp synchronizer thread, to start the output at a specified time
//

static int DvpSynchronizerThread( void *data )
{
dvp_v4l2_video_handle_t	*Context;
unsigned long long	 TriggerPoint;
long long		 FrameDuration;
long long		 SleepTime;
long long		 Frames;
ktime_t			 Ktime;
unsigned long long	 Now;
unsigned long long	 ExpectedWake;
unsigned int		 Jiffies;

    //
    // Initialize context variables
    //

    Context				= (dvp_v4l2_video_handle_t *)data;
    Context->SynchronizerRunning	= true;

    //
    // Enter the main loop waiting for something to happen
    //

    while( Context->SynchronizerRunning )
    {
	down_interruptible( &Context->DvpSynchronizerWakeSem );

	//
	// Do we want to do something
	//

	SYSFS_TEST_NOTIFY( MicroSecondsPerFrame,		microseconds_per_frame );
	SYSFS_TEST_NOTIFY( FrameCountingNotification, 		frame_counting_notification );
	SYSFS_TEST_NOTIFY( FrameCaptureNotification,  		frame_capture_notification );
	SYSFS_TEST_NOTIFY( PostMortem,				post_mortem );

	while( Context->SynchronizerRunning && Context->SynchronizeEnabled && Context->Synchronize )
	{
	    //
	    // When do we want to trigger (try for 32 us early to allow time
	    // for the procedure to execute, and to shake the sleep from my eyes).
	    //

	    TriggerPoint		= Context->DvpBaseTime + ControlValue(VideoLatency) - 32;
	    FrameDuration		= DvpTimeForNFrames(1);

	    if( Context->StreamInfo.interlaced &&
		!ControlValue(TopFieldFirst) )
		TriggerPoint		+= FrameDuration/2;

	    Context->Synchronize	= false;

	    Ktime 			= ktime_get();
	    Now				= ktime_to_us( Ktime );

	    SleepTime			= (long long)TriggerPoint - (long long)Now;
	    if( SleepTime < 0 )
		Frames			= ((-SleepTime) / FrameDuration) + 1;
	    else
		Frames			= -(SleepTime / FrameDuration);
	    SleepTime			= SleepTime + (Frames * FrameDuration);

	    //
	    // Now sleep
	    //

	    Jiffies		= (unsigned int)(((long long)HZ * SleepTime) / 1000000);
	    ExpectedWake	= Now + SleepTime;

	    set_current_state( TASK_INTERRUPTIBLE );
	    schedule_timeout( Jiffies );

	    //
	    // Now livelock until we can do the synchronize
	    //

	    do
	    {
		if( !Context->SynchronizerRunning || Context->Synchronize )
		    break;

		Ktime 		= ktime_get();
	    	Now		= ktime_to_us( Ktime );
	    } while( inrange( (ExpectedWake - Now), 0, 0x7fffffffffffffffull) &&
		     Context->SynchronizerRunning && Context->SynchronizeEnabled && !Context->Synchronize );

	    if( !Context->SynchronizerRunning || !Context->SynchronizeEnabled || Context->Synchronize )
		break;

	    //
	    // Finally do the synchronize
	    //

	    DvbDisplaySynchronize( BACKEND_VIDEO_ID, Context->DeviceContext->Id );			// Trigger vsync lock

	    Ktime 		= ktime_get();
	    Now			= ktime_to_us( Ktime );

	    //
	    // If we check to see if we were interrupted at any critical point, and retry the sync if so.
	    // We assume an interrupt occurred if we took more than 128us to perform the sync trigger.

	    if( (Now - ExpectedWake) > 128 )
		Context->Synchronize	= true;
	}
//printk( "Synchronize Vsync - Now-Expected = %lld\n", (Now - ExpectedWake) );
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	The ioctl implementations for video
//

int DvpVideoIoctlSetFramebuffer(        dvp_v4l2_video_handle_t	*Context,
					unsigned int   		 Width,
                                        unsigned int   		 Height,
                                        unsigned int   		 BytesPerLine,
                                        unsigned int   		 Control )
{
printk( "VIDIOC_S_FBUF: DOING NOTHING !!!! - W = %4d, H = %4d, BPL = %4d, Crtl = %08x\n", Width, Height, BytesPerLine, Control );

    return 0;
}

// -----------------------------

int DvpVideoIoctlSetStandard(		dvp_v4l2_video_handle_t	*Context,
					v4l2_std_id		 Id )
{
int 				 	 i;
const dvp_v4l2_video_mode_line_t	*NewCaptureMode		= NULL;

//

    printk( "VIDIOC_S_STD: %08x\n", (unsigned int) Id );

//

    for( i = 0; i < N_ELEMENTS (DvpModeParamsTable); i++ )
	if( DvpModeParamsTable[i].Mode == Id )
	{
	    NewCaptureMode = &DvpModeParamsTable[i];
	    break;
	}

    if (!NewCaptureMode)
    {
	printk( "DvpVideoIoctlSetStandard - Mode not supported\n" );
	return -EINVAL;
    }

    //
    // we have a new capture mode, set the master copy, and force
    // a mode switch if any video sequence is already running.
    //

    DvpHaltCapture( Context );

    Context->DvpCaptureMode		= NewCaptureMode;
    Context->FastModeSwitch		= true;

    up( &Context->DvpPreInjectBufferSem );
    up( &Context->DvpVideoInterruptSem );

    return 0;
}

// -----------------------------

int DvpVideoIoctlOverlayStart(	dvp_v4l2_video_handle_t	*Context )
{
unsigned int		 i;
int			 Result;
char 			 TaskName[32];
struct task_struct	*Task;
struct sched_param 	 Param;

//

printk( "VIDIOC_OVERLAY:\n" );

    if( Context == NULL )
    {
	printk( "DvpVideoIoctlOverlayStart - Video context NULL\n" );
	return -EINVAL;
    }

//

    Context->DvpLatency		= (Context->SharedContext->target_latency * 1000);

//

    if( Context->VideoRunning )
    {
//	printk("DvpVideoIoctlOverlayStart - Capture Thread already started\n" );
	return 0;
    }

//

    Context->DvpLatency         = (Context->SharedContext->target_latency * 1000);
    Context->VideoRunning		= false;
    Context->DvpState			= DvpInactive;

    Context->SynchronizerRunning	= false;
    Context->Synchronize		= false;

//

    sema_init( &Context->DvpVideoInterruptSem, 0 );
    sema_init( &Context->DvpSynchronizerWakeSem, 0 );
    sema_init( &Context->DvpPreInjectBufferSem, 0 );

    sema_init( &Context->DvpScalingStateLock, 1 );

#if defined (CONFIG_KERNELVERSION) // STLinux 2.3
    Result	= request_irq( Context->DvpIrq, DvpInterrupt, IRQF_DISABLED, "dvp", Context );
#else
    Result	= request_irq( Context->DvpIrq, DvpInterrupt, SA_INTERRUPT, "dvp", Context );
#endif

    if( Result != 0 )
    {
	printk("DvpVideoIoctlOverlayStart - Cannot get irq %d (result = %d)\n", Context->DvpIrq, Result );
	return -EBUSY;
    }

    //
    // Create the ancillary input data buffer - Note we capture the control value for the size just in case the user should try to change it after allocation.
    //

    Context->AncillaryInputBufferSize			= ControlValue(AncInputBufferSize);
    Context->AncillaryInputBufferPhysicalAddress	= OSDEV_MallocPartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferSize );

    if( Context->AncillaryInputBufferPhysicalAddress == NULL )
    {
	printk( "DvpVideoIoctlOverlayStart - Unable to allocate ANC input buffer.\n" );
	free_irq( Context->DvpIrq, Context );
	return -ENOMEM;
    }

    Context->AncillaryInputBufferUnCachedAddress	= (unsigned char *)OSDEV_IOReMap( (unsigned int)Context->AncillaryInputBufferPhysicalAddress, Context->AncillaryInputBufferSize );
    Context->AncillaryInputBufferInputPointer		= Context->AncillaryInputBufferUnCachedAddress;

    memset( Context->AncillaryInputBufferUnCachedAddress, 0x00, Context->AncillaryInputBufferSize );

    //
    // Create the video tasks
    //

    strcpy( TaskName, "dvp video task" );
    Task 		= kthread_create( DvpVideoThread, Context, "%s", TaskName );
    if( IS_ERR(Task) )
    {
	printk( "DvpVideoIoctlOverlayStart - Unable to start video thread\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );
	free_irq( Context->DvpIrq, Context );
	return -EINVAL;
    }

    Param.sched_priority 	= DVP_VIDEO_THREAD_PRIORITY;
    if( sched_setscheduler( Task, SCHED_RR, &Param) )
    {
	printk("DvpVideoIoctlOverlayStart - FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Param.sched_priority);
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );
	free_irq( Context->DvpIrq, Context );
	return -EINVAL;
    }

    wake_up_process( Task );

//

    strcpy( TaskName, "dvp synchronizer task" );
    Task 		= kthread_create( DvpSynchronizerThread, Context, "%s", TaskName );
    if( IS_ERR(Task) )
    {
	printk( "DvpVideoIoctlOverlayStart - Unable to start synchronizer thread\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );
	free_irq( Context->DvpIrq, Context );
	return -EINVAL;
    }

    Param.sched_priority 	= DVP_SYNCHRONIZER_THREAD_PRIORITY;
    if( sched_setscheduler( Task, SCHED_RR, &Param) )
    {
	printk("DvpVideoIoctlOverlayStart - FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Param.sched_priority);
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );
	free_irq( Context->DvpIrq, Context );
	return -EINVAL;
    }

    wake_up_process( Task );

    //
    // Guarantee both tasks are started
    //

    for( i=0; (i<100) && (!Context->VideoRunning || !Context->SynchronizerRunning); i++ )
    {
	unsigned int  Jiffies = ((HZ)/1000)+1;

	set_current_state( TASK_INTERRUPTIBLE );
	schedule_timeout( Jiffies );
    }

    if( !Context->VideoRunning || !Context->SynchronizerRunning )
    {
	printk( "DvpVideoIoctlOverlayStart - FAILED to set start processes\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryInputBufferUnCachedAddress );
        OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryInputBufferPhysicalAddress );
	free_irq( Context->DvpIrq, Context );
	return -EINVAL;
    }

//

    return 0;
}


// -----------------------------


int DvpVideoIoctlCrop(	dvp_v4l2_video_handle_t		*Context,
			struct v4l2_crop		*Crop )
{
#if 0
printk( "VIDIOC_S_CROP:\n" );
printk( "	%s %3dx%3d\n", 
	((Crop->type == V4L2_BUF_TYPE_VIDEO_OVERLAY) ? "OutputWindow" : "InputWindow "),
	Crop->c.width, Crop->c.height );
#endif

    if( (Context == NULL) || (Context->DeviceContext->VideoStream == NULL) )
	return -EINVAL;

    if (Crop->type == V4L2_BUF_TYPE_VIDEO_OVERLAY) 
    {
	if( !Context->OutputCropTargetReached && (Context->OutputCrop.X != 0) ) 
	{
	    printk( "DvpVideoIoctlCrop: Setting output crop before previous crop has reached display\n" );
	}

        atomic_set(&Context->DvpOutputCropTargetReachedNotification, 1);

	Context->OutputCropStepping		= false;
	Context->OutputCropTargetReached	= false;

	Context->OutputCropTarget.X		= Crop->c.left;
	Context->OutputCropTarget.Y		= Crop->c.top;
	Context->OutputCropTarget.Width		= Crop->c.width;
	Context->OutputCropTarget.Height	= Crop->c.height;
	Context->OutputCropStart		= Context->OutputCrop;

	if( (Context->OutputCropTarget.X      == Context->OutputCropStart.X) &&
	    (Context->OutputCropTarget.Y      == Context->OutputCropStart.Y) &&
	    (Context->OutputCropTarget.Width  == Context->OutputCropStart.Width) &&
	    (Context->OutputCropTarget.Height == Context->OutputCropStart.Height) )

	{
	    printk( "DvpVideoIoctlCrop: Setting output crop to be unchanged - marking as target on display immediately.\n" );
	    DvpVideoSysfsProcessDecrementOutputCropTargetReachedNotification(Context);
	    Context->OutputCropTargetReached	= true;
	}
	else
	{
	    switch( ControlValue(OutputCropTransitionMode) )
	    {
	    	default:
			printk( "DvpVideoIoctlCrop: Unsupported output crop transition mode (%d) defaulting to single step. \n ", ControlValue(OutputCropTransitionMode) );

	    	case DVP_TRANSITION_MODE_SINGLE_STEP:
			Context->OutputCropSteps		= 1;
			Context->OutputCropCurrentStep		= 0;
			Context->OutputCropStepping		= true;
			break;

	    	case DVP_TRANSITION_MODE_STEP_OVER_N_VSYNCS:
			Context->OutputCropSteps		= ControlValue(OutputCropTransitionModeParameter0);
			Context->OutputCropCurrentStep		= 0;
			Context->OutputCropStepping		= true;
			break;
	    }
	}
    }
    else if (Crop->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	if( ((Crop->c.left + Crop->c.width) > Context->ModeWidth) ||
	    ((Crop->c.top + Crop->c.height) > Context->ModeHeight) )
	{
	    printk( "DvpVideoIoctlCrop: Input crop invalid for current mode, (%d + %d > %d) Or (%d + %d > %d).\n",
		Crop->c.left, Crop->c.width, Context->ModeWidth,
		Crop->c.top, Crop->c.height, Context->ModeHeight );
	    return -EINVAL;
	}

	Context->InputCrop.X		= Crop->c.left;
	Context->InputCrop.Y		= Crop->c.top;
	Context->InputCrop.Width	= Crop->c.width;
	Context->InputCrop.Height	= Crop->c.height;

	if( !Context->OutputCropStepping )
	    DvpRecalculateScaling( Context );
    }
    else {
	printk( "DvpVideoIoctlCrop: Unknown type %d. \n ", Crop->type );
	return -EINVAL;
    }

//

    return 0;
}


// -----------------------------

int DvpVideoIoctlSetControl( 		dvp_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		 Value )
{
int	ret;

printk( "VIDIOC_S_CTRL->V4L2_CID_STM_DVPIF_" );

    switch( Control )
    {
	case V4L2_CID_STM_DVPIF_RESTORE_DEFAULT:
		printk( "RESTORE_DEFAULT\n" );
		Context->DvpControl16Bit					= DVP_USE_DEFAULT;
		Context->DvpControlBigEndian					= DVP_USE_DEFAULT;
		Context->DvpControlFullRange					= DVP_USE_DEFAULT;
		Context->DvpControlIncompleteFirstPixel				= DVP_USE_DEFAULT;
		Context->DvpControlOddPixelCount				= DVP_USE_DEFAULT;
		Context->DvpControlVsyncBottomHalfLineEnable			= DVP_USE_DEFAULT;
		Context->DvpControlExternalSync					= DVP_USE_DEFAULT;
		Context->DvpControlExternalSyncPolarity				= DVP_USE_DEFAULT;
		Context->DvpControlExternalSynchroOutOfPhase			= DVP_USE_DEFAULT;
		Context->DvpControlExternalVRefOddEven				= DVP_USE_DEFAULT;
		Context->DvpControlExternalVRefPolarityPositive			= DVP_USE_DEFAULT;
		Context->DvpControlHRefPolarityPositive				= DVP_USE_DEFAULT;
		Context->DvpControlActiveAreaAdjustHorizontalOffset		= DVP_USE_DEFAULT;
		Context->DvpControlActiveAreaAdjustVerticalOffset		= DVP_USE_DEFAULT;
		Context->DvpControlActiveAreaAdjustWidth			= DVP_USE_DEFAULT;
		Context->DvpControlActiveAreaAdjustHeight			= DVP_USE_DEFAULT;
		Context->DvpControlColourMode					= DVP_USE_DEFAULT;
		Context->DvpControlVideoLatency					= DVP_USE_DEFAULT;
		Context->DvpControlBlank					= DVP_USE_DEFAULT;
		Context->DvpControlAncPageSizeSpecified				= DVP_USE_DEFAULT;
		Context->DvpControlAncPageSize					= DVP_USE_DEFAULT;
		Context->DvpControlAncInputBufferSize				= DVP_USE_DEFAULT;
		Context->DvpControlHorizontalResizeEnable			= DVP_USE_DEFAULT;
		Context->DvpControlVerticalResizeEnable				= DVP_USE_DEFAULT;
		Context->DvpControlTopFieldFirst				= DVP_USE_DEFAULT;
		Context->DvpControlVsyncLockEnable				= DVP_USE_DEFAULT;
		Context->DvpControlPixelAspectRatioCorrection			= DVP_USE_DEFAULT;
		Context->DvpControlPictureAspectRatio				= DVP_USE_DEFAULT;
		Context->DvpControlOutputCropTransitionMode			= DVP_USE_DEFAULT;
		Context->DvpControlOutputCropTransitionModeParameter0		= DVP_USE_DEFAULT;

		Context->DvpControlDefault16Bit					= 0;
		Context->DvpControlDefaultBigEndian				= 0;
		Context->DvpControlDefaultFullRange				= 0;
		Context->DvpControlDefaultIncompleteFirstPixel			= 0;
		Context->DvpControlDefaultOddPixelCount				= 0;
		Context->DvpControlDefaultVsyncBottomHalfLineEnable		= 0;
		Context->DvpControlDefaultExternalSync				= 1;
		Context->DvpControlDefaultExternalSyncPolarity			= 1;
		Context->DvpControlDefaultExternalSynchroOutOfPhase		= 0;
		Context->DvpControlDefaultExternalVRefOddEven			= 0;
		Context->DvpControlDefaultExternalVRefPolarityPositive		= 0;
		Context->DvpControlDefaultHRefPolarityPositive			= 0;
		Context->DvpControlDefaultActiveAreaAdjustHorizontalOffset	= 0;
		Context->DvpControlDefaultActiveAreaAdjustVerticalOffset	= 0;
		Context->DvpControlDefaultActiveAreaAdjustWidth			= 0;
		Context->DvpControlDefaultActiveAreaAdjustHeight		= 0;
		Context->DvpControlDefaultColourMode				= 0;
		Context->DvpControlDefaultVideoLatency				= 0xffffffff;
		Context->DvpControlDefaultBlank					= 0;
		Context->DvpControlDefaultAncPageSizeSpecified			= 0;
		Context->DvpControlDefaultAncPageSize				= DVP_DEFAULT_ANCILLARY_PAGE_SIZE;
		Context->DvpControlDefaultAncInputBufferSize			= DVP_DEFAULT_ANCILLARY_INPUT_BUFFER_SIZE;
		Context->DvpControlDefaultHorizontalResizeEnable		= 1;
		Context->DvpControlDefaultVerticalResizeEnable			= 0;
		Context->DvpControlDefaultTopFieldFirst				= 0;
		Context->DvpControlDefaultVsyncLockEnable			= 0;
		Context->DvpControlDefaultOutputCropTransitionMode		= DVP_TRANSITION_MODE_SINGLE_STEP;
		Context->DvpControlDefaultOutputCropTransitionModeParameter0	= 8;
		Context->DvpControlDefaultPixelAspectRatioCorrection		= DVP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE;
		Context->DvpControlDefaultPictureAspectRatio			= DVP_PICTURE_ASPECT_RATIO_4_3;

#if 0
Context->DvpControlOutputCropTransitionMode			= DVP_TRANSITION_MODE_STEP_OVER_N_VSYNCS;
Context->DvpControlOutputCropTransitionModeParameter0		= 60;
#endif
		break;

	case  V4L2_CID_STM_DVPIF_BLANK:
		Check( "DVPIF_BLANK", 0, 1 );
		if( Context->DeviceContext->VideoStream == NULL )
		    return -EINVAL;

		ret = DvbStreamEnable( Context->DeviceContext->VideoStream, Value );
		if( ret < 0 )
		{
		    printk( "DvpVideoIoctlSetControl - StreamEnable failed (%d)\n", Value );
		    return -EINVAL;
		}

		Context->DvpControlBlank	= Value;
		break;


	case  V4L2_CID_STM_DVPIF_16_BIT:
		CheckAndSet( "16_BIT", 0, 1, Context->DvpControl16Bit );
		break;
	case  V4L2_CID_STM_DVPIF_BIG_ENDIAN:
		CheckAndSet( "BIG_ENDIAN", 0, 1, Context->DvpControlBigEndian );
		break;
	case  V4L2_CID_STM_DVPIF_FULL_RANGE:
		CheckAndSet( "FULL_RANGE", 0, 1, Context->DvpControlFullRange );
		break;
	case  V4L2_CID_STM_DVPIF_INCOMPLETE_FIRST_PIXEL:
		CheckAndSet( "INCOMPLETE_FIRST_PIXEL", 0, 1, Context->DvpControlIncompleteFirstPixel );
		break;
	case  V4L2_CID_STM_DVPIF_ODD_PIXEL_COUNT:
		CheckAndSet( "ODD_PIXEL_COUNT", 0, 1, Context->DvpControlOddPixelCount );
		break;
	case  V4L2_CID_STM_DVPIF_VSYNC_BOTTOM_HALF_LINE_ENABLE:
		CheckAndSet( "VSYNC_BOTTOM_HALF_LINE_ENABLE", 0, 1, Context->DvpControlVsyncBottomHalfLineEnable );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNC:
		CheckAndSet( "EXTERNAL_SYNC", 0, 1, Context->DvpControlExternalSync );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNC_POLARITY:
		CheckAndSet( "EXTERNAL_SYNC_POLARITY", 0, 1, Context->DvpControlExternalSyncPolarity );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNCHRO_OUT_OF_PHASE:
		CheckAndSet( "EXTERNAL_SYNCHRO_OUT_OF_PHASE", 0, 1, Context->DvpControlExternalSynchroOutOfPhase );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_VREF_ODD_EVEN:
		CheckAndSet( "EXTERNAL_VREF_ODD_EVEN", 0, 1, Context->DvpControlExternalVRefOddEven );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_VREF_POLARITY_POSITIVE:
		CheckAndSet( "EXTERNAL_VREF_POLARITY_POSITIVE", 0, 1, Context->DvpControlExternalVRefPolarityPositive );
		break;
	case  V4L2_CID_STM_DVPIF_HREF_POLARITY_POSITIVE:
		CheckAndSet( "HREF_POLARITY_POSITIVE", 0, 1, Context->DvpControlHRefPolarityPositive );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET:
		CheckAndSet( "ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET", -260, 260, Context->DvpControlActiveAreaAdjustHorizontalOffset );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_VERTICAL_OFFSET:
		CheckAndSet( "ACTIVE_AREA_ADJUST_VERTICAL_OFFSET", -256, 256, Context->DvpControlActiveAreaAdjustVerticalOffset );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_WIDTH:
		CheckAndSet( "ACTIVE_AREA_ADJUST_WIDTH", -2048, 2048, Context->DvpControlActiveAreaAdjustWidth );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_HEIGHT:
		CheckAndSet( "ACTIVE_AREA_ADJUST_HEIGHT", -2048, 2048, Context->DvpControlActiveAreaAdjustHeight );
		break;
	case  V4L2_CID_STM_DVPIF_COLOUR_MODE:
		CheckAndSet( "COLOUR_MODE", 0, 2, Context->DvpControlColourMode );
		break;
	case  V4L2_CID_STM_DVPIF_VIDEO_LATENCY:
		CheckAndSet( "VIDEO_LATENCY", 0, 1000000, Context->DvpControlVideoLatency );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_PAGE_SIZE_SPECIFIED:
		CheckAndSet( "ANC_PAGE_SIZE_SPECIFIED", 0, 1, Context->DvpControlAncPageSizeSpecified );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_PAGE_SIZE:
		CheckAndSet( "ANC_PAGE_SIZE", 0, 4096, Context->DvpControlAncPageSize );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_INPUT_BUFFER_SIZE:
		Value		&= 0xfffffff0;
		CheckAndSet( "ANC_INPUT_BUFFER_SIZE", 0, 0x100000, Context->DvpControlAncInputBufferSize );
		break;
	case  V4L2_CID_STM_DVPIF_HORIZONTAL_RESIZE_ENABLE:
		CheckAndSet( "HORIZONTAL_RESIZE_ENABLE", 0, 1, Context->DvpControlHorizontalResizeEnable );
		DvpRecalculateScaling( Context );
		break;
	case  V4L2_CID_STM_DVPIF_VERTICAL_RESIZE_ENABLE:
		CheckAndSet( "VERTICAL_RESIZE_ENABLE", 0, 0, Context->DvpControlVerticalResizeEnable );
		DvpRecalculateScaling( Context );
		break;
	case  V4L2_CID_STM_DVPIF_TOP_FIELD_FIRST:
		CheckAndSet( "TOP_FIELD_FIRST", 0, 1, Context->DvpControlTopFieldFirst );
		break;
	case  V4L2_CID_STM_DVPIF_VSYNC_LOCK_ENABLE:
		CheckAndSet( "VSYNC_LOCK_ENABLE", 0, 1, Context->DvpControlVsyncLockEnable );
		break;
	case  V4L2_CID_STM_DVPIF_OUTPUT_CROP_TRANSITION_MODE:
		CheckAndSet( "OUTPUT_CROP_TRANSITION_MODE", 0, 1, Context->DvpControlOutputCropTransitionMode );
		break;
	case  V4L2_CID_STM_DVPIF_OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0:
		CheckAndSet( "OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0", 0, 0x7fffffff, Context->DvpControlOutputCropTransitionModeParameter0 );
		break;
	case V4L2_CID_STM_DVPIF_PIXEL_ASPECT_RATIO_CORRECTION:
	    	CheckAndSet( "PIXEL_ASPECT_RATIO_CORRECTION", DVP_PIXEL_ASPECT_RATIO_CORRECTION_MIN_VALUE, 
	    		DVP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE, Context->DvpControlPixelAspectRatioCorrection );

	    	// Set appropriate policy to manage non progressive zoom format
	        DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_PIXEL_ASPECT_RATIO_CORRECTION, 
	    	    Context->DvpControlPixelAspectRatioCorrection );
		break;
	case V4L2_CID_STM_DVPIF_PICTURE_ASPECT_RATIO:
        {
	        dvp_v4l2_video_mode_t 	 Mode;

		CheckAndSet ( "PICTURE_ASPECT_RATIO", DVP_PICTURE_ASPECT_RATIO_4_3, 
			DVP_PICTURE_ASPECT_RATIO_16_9, Context->DvpControlPictureAspectRatio );
	        
		// Calculate the pixel aspect ratio coefficients corresponding to the picture aspect ratio.
		// They are used to set the non progressive zoom format. Only for NTSC and PAL.
	        Mode = Context->DvpCaptureMode->Mode;
	        if ( Context->DvpControlPictureAspectRatio == DVP_PICTURE_ASPECT_RATIO_4_3 )
	        {
		    	switch ( Mode )
		    	{
		    	    case DVP_720_480_I60000:
		    	    case DVP_720_480_P60000:
		    	    case DVP_720_480_I59940:
		    	    case DVP_720_480_P59940:
		    		Context->DeviceContext->PixelAspectRatio.Numerator = 8;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 9;
		    	    	break;
		    	    case DVP_720_576_I50000:
		    	    case DVP_720_576_P50000:
		    	        Context->DeviceContext->PixelAspectRatio.Numerator = 16;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 15;
		    	    	break;
		    	    default:
		    	        Context->DeviceContext->PixelAspectRatio.Numerator = 1;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 1;
		    	    	break;
		    	}
		 }
		 else if ( Context->DvpControlPictureAspectRatio == DVP_PICTURE_ASPECT_RATIO_16_9 )
		 {
		    	switch ( Mode )
		    	{
		    	    case DVP_720_480_I60000:
		    	    case DVP_720_480_P60000:
		    	    case DVP_720_480_I59940:
		    	    case DVP_720_480_P59940:
		    	        Context->DeviceContext->PixelAspectRatio.Numerator = 32;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 27;
		    	    	break;
		    	    case DVP_720_576_I50000:
		    	    case DVP_720_576_P50000:
		    	        Context->DeviceContext->PixelAspectRatio.Numerator = 64;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 45;
		    	    	break;
		    	    default:
		    	    	Context->DeviceContext->PixelAspectRatio.Numerator = 1;
		    		Context->DeviceContext->PixelAspectRatio.Denominator = 1;
		    	    	break;
		    	}
	        }
		break;
	}
	default:
		printk( "Unknown %08x\n", Control );
		return -1;
    }

    return 0;
}

// --------------------------------

int DvpVideoIoctlGetControl( 		dvp_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		*Value )
{
printk( "VIDIOC_G_CTRL->V4L2_CID_STM_DVPIF_" );

    switch( Control )
    {
	case V4L2_CID_STM_DVPIF_RESTORE_DEFAULT:
		printk( "RESTORE_DEFAULT - Not a readable control\n" );
		*Value		= 0;
		break;


	case  V4L2_CID_STM_DVPIF_BLANK:
		GetValue( "DVPIF_BLANK", Context->DvpControlBlank );
		break;
	case  V4L2_CID_STM_DVPIF_16_BIT:
		GetValue( "16_BIT", Context->DvpControl16Bit );
		break;
	case  V4L2_CID_STM_DVPIF_BIG_ENDIAN:
		GetValue( "BIG_ENDIAN", Context->DvpControlBigEndian );
		break;
	case  V4L2_CID_STM_DVPIF_FULL_RANGE:
		GetValue( "FULL_RANGE", Context->DvpControlFullRange );
		break;
	case  V4L2_CID_STM_DVPIF_INCOMPLETE_FIRST_PIXEL:
		GetValue( "INCOMPLETE_FIRST_PIXEL", Context->DvpControlIncompleteFirstPixel );
		break;
	case  V4L2_CID_STM_DVPIF_ODD_PIXEL_COUNT:
		GetValue( "ODD_PIXEL_COUNT", Context->DvpControlOddPixelCount );
		break;
	case  V4L2_CID_STM_DVPIF_VSYNC_BOTTOM_HALF_LINE_ENABLE:
		GetValue( "VSYNC_BOTTOM_HALF_LINE_ENABLE", Context->DvpControlVsyncBottomHalfLineEnable );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNC:
		GetValue( "EXTERNAL_SYNC", Context->DvpControlExternalSync );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNC_POLARITY:
		GetValue( "EXTERNAL_SYNC_POLARITY", Context->DvpControlExternalSyncPolarity );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_SYNCHRO_OUT_OF_PHASE:
		GetValue( "EXTERNAL_SYNCHRO_OUT_OF_PHASE", Context->DvpControlExternalSynchroOutOfPhase );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_VREF_ODD_EVEN:
		GetValue( "EXTERNAL_VREF_ODD_EVEN", Context->DvpControlExternalVRefOddEven );
		break;
	case  V4L2_CID_STM_DVPIF_EXTERNAL_VREF_POLARITY_POSITIVE:
		GetValue( "EXTERNAL_VREF_POLARITY_POSITIVE", Context->DvpControlExternalVRefPolarityPositive );
		break;
	case  V4L2_CID_STM_DVPIF_HREF_POLARITY_POSITIVE:
		GetValue( "HREF_POLARITY_POSITIVE", Context->DvpControlHRefPolarityPositive );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET:
		GetValue( "ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET", Context->DvpControlActiveAreaAdjustHorizontalOffset );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_VERTICAL_OFFSET:
		GetValue( "ACTIVE_AREA_ADJUST_VERTICAL_OFFSET", Context->DvpControlActiveAreaAdjustVerticalOffset );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_WIDTH:
		GetValue( "ACTIVE_AREA_ADJUST_WIDTH", Context->DvpControlActiveAreaAdjustWidth );
		break;
	case  V4L2_CID_STM_DVPIF_ACTIVE_AREA_ADJUST_HEIGHT:
		GetValue( "ACTIVE_AREA_ADJUST_HEIGHT", Context->DvpControlActiveAreaAdjustHeight );
		break;
	case  V4L2_CID_STM_DVPIF_COLOUR_MODE:
		GetValue( "COLOUR_MODE", Context->DvpControlColourMode );
		break;
	case  V4L2_CID_STM_DVPIF_VIDEO_LATENCY:
		GetValue( "VIDEO_LATENCY", Context->DvpControlVideoLatency );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_PAGE_SIZE_SPECIFIED:
		GetValue( "ANC_PAGE_SIZE_SPECIFIED", Context->DvpControlAncPageSizeSpecified );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_PAGE_SIZE:
		GetValue( "ANC_PAGE_SIZE", Context->DvpControlAncPageSize );
		break;
	case  V4L2_CID_STM_DVPIF_ANC_INPUT_BUFFER_SIZE:
		GetValue( "ANC_INPUT_BUFFER_SIZE", Context->DvpControlAncInputBufferSize );
		break;
	case  V4L2_CID_STM_DVPIF_HORIZONTAL_RESIZE_ENABLE:
		GetValue( "HORIZONTAL_RESIZE_ENABLE", Context->DvpControlHorizontalResizeEnable );
		break;
	case  V4L2_CID_STM_DVPIF_VERTICAL_RESIZE_ENABLE:
		GetValue( "VERTICAL_RESIZE_ENABLE", Context->DvpControlVerticalResizeEnable );
		break;
	case  V4L2_CID_STM_DVPIF_TOP_FIELD_FIRST:
		GetValue( "TOP_FIELD_FIRST", Context->DvpControlTopFieldFirst );
		break;
	case  V4L2_CID_STM_DVPIF_VSYNC_LOCK_ENABLE:
		GetValue( "VSYNC_LOCK_ENABLE", Context->DvpControlVsyncLockEnable );
		break;
	case  V4L2_CID_STM_DVPIF_OUTPUT_CROP_TRANSITION_MODE:
		GetValue( "OUTPUT_CROP_TRANSITION_MODE", Context->DvpControlOutputCropTransitionMode );
		break;
	case  V4L2_CID_STM_DVPIF_OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0:
		GetValue( "OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0", Context->DvpControlOutputCropTransitionModeParameter0 );
		break;
	case V4L2_CID_STM_DVPIF_PIXEL_ASPECT_RATIO_CORRECTION:
	    	GetValue( "PIXEL_ASPECT_RATIO_CORRECTION", Context->DvpControlPixelAspectRatioCorrection );
	    	break;
	case V4L2_CID_STM_DVPIF_PICTURE_ASPECT_RATIO:
	    	GetValue( "PICTURE_ASPECT_RATIO", Context->DvpControlPictureAspectRatio );
	    	break;	    	
	default:
		printk( "Unknown\n" );
		return -1;
    }

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	The management functions for the Ancillary data capture
//
//	Function to request buffers
//

int DvpVideoIoctlAncillaryRequestBuffers(	dvp_v4l2_video_handle_t	*Context,
						unsigned int		 DesiredCount,
						unsigned int		 DesiredSize,
						unsigned int		*ActualCount,
						unsigned int		*ActualSize )
{
unsigned int	i;

    //
    // Is this a closedown call
    //

    if( DesiredCount == 0 )
    {
	if( Context->AncillaryBufferCount == 0 )
	{
	    printk( "DvpVideoIoctlAncillaryRequestBuffers - Attempt to free when No buffers allocated.\n" );
	    return -EINVAL;
	}

	DvpVideoIoctlAncillaryStreamOff( Context );

	Context->AncillaryBufferCount	= 0;
	OSDEV_IOUnMap( (unsigned int)Context->AncillaryBufferUnCachedAddress );
	OSDEV_FreePartitioned( DVP_ANCILLARY_PARTITION, Context->AncillaryBufferPhysicalAddress );

	return 0;
    }

    //
    // Check that we do not already have buffers
    //

    if( Context->AncillaryBufferCount != 0 )
    {
	printk( "DvpVideoIoctlAncillaryRequestBuffers - Attempt to allocate buffers when buffers are already mapped.\n" );
	return -EINVAL;
    }

    //
    // Adjust the buffer counts and sizes to meet our restrictions
    //

    Clamp( DesiredCount, DVP_MIN_ANCILLARY_BUFFERS, DVP_MAX_ANCILLARY_BUFFERS );

    Clamp( DesiredSize,  DVP_MIN_ANCILLARY_BUFFER_SIZE, DVP_MAX_ANCILLARY_BUFFER_SIZE );

    if( (DesiredSize % DVP_ANCILLARY_BUFFER_CHUNK_SIZE) != 0 )
	DesiredSize	+= (DVP_ANCILLARY_BUFFER_CHUNK_SIZE - (DesiredSize % DVP_ANCILLARY_BUFFER_CHUNK_SIZE));

    //
    // Create buffers
    //

    Context->AncillaryBufferPhysicalAddress	= OSDEV_MallocPartitioned( DVP_ANCILLARY_PARTITION, DesiredSize * DesiredCount );
    if( Context->AncillaryBufferPhysicalAddress == NULL )
    {
	printk( "DvpVideoIoctlAncillaryRequestBuffers - Unable to allocate buffers.\n" );
	return -ENOMEM;
    }

    Context->AncillaryBufferUnCachedAddress	= (unsigned char *)OSDEV_IOReMap( (unsigned int)Context->AncillaryBufferPhysicalAddress, DesiredSize * DesiredCount );
    memset( Context->AncillaryBufferUnCachedAddress, 0x00, DesiredCount * DesiredSize );

    //
    // Initialize the state
    //

    Context->AncillaryStreamOn			= false;
    Context->AncillaryCaptureInProgress		= false;
    Context->AncillaryBufferCount		= DesiredCount;
    Context->AncillaryBufferSize		= DesiredSize;

    memset( Context->AncillaryBufferState, 0x00, DVP_MAX_ANCILLARY_BUFFERS * sizeof(AncillaryBufferState_t) );
    for( i=0; i<Context->AncillaryBufferCount; i++ )
    {
	Context->AncillaryBufferState[i].PhysicalAddress	= Context->AncillaryBufferPhysicalAddress + (i * DesiredSize);
	Context->AncillaryBufferState[i].UnCachedAddress	= Context->AncillaryBufferUnCachedAddress + (i * DesiredSize);
	Context->AncillaryBufferState[i].Queued			= false;
	Context->AncillaryBufferState[i].Done			= false;
	Context->AncillaryBufferState[i].Bytes			= 0;
	Context->AncillaryBufferState[i].FillTime		= 0;
    }

    //
    // Initialize the queued/dequeued/fill pointers
    //

    Context->AncillaryBufferNextQueueIndex	= 0;
    Context->AncillaryBufferNextFillIndex	= 0;
    Context->AncillaryBufferNextDeQueueIndex	= 0;

    sema_init( &Context->DvpAncillaryBufferDoneSem, 0 );

//

    *ActualCount				= Context->AncillaryBufferCount;
    *ActualSize					= Context->AncillaryBufferSize;

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to query the state of a buffer
//

int DvpVideoIoctlAncillaryQueryBuffer(		dvp_v4l2_video_handle_t	 *Context,
						unsigned int		  Index,
						bool			 *Queued,
						bool			 *Done,
						unsigned char		**PhysicalAddress,
						unsigned char		**UnCachedAddress,
						unsigned long long	 *CaptureTime,
						unsigned int		 *Bytes,
						unsigned int		 *Size )
{
    if( Index >= Context->AncillaryBufferCount )
    {
	printk( "DvpVideoIoctlAncillaryQueryBuffer - Index out of bounds (%d).\n", Index );
	return -EINVAL;
    }

//

    *Queued		= Context->AncillaryBufferState[Index].Queued;
    *Done		= Context->AncillaryBufferState[Index].Done;
    *PhysicalAddress	= Context->AncillaryBufferState[Index].PhysicalAddress;
    *UnCachedAddress	= Context->AncillaryBufferState[Index].UnCachedAddress;
    *CaptureTime	= Context->AncillaryBufferState[Index].FillTime + Context->AppliedLatency;
    *Bytes		= Context->AncillaryBufferState[Index].Bytes;
    *Size		= Context->AncillaryBufferSize;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to queue a buffer
//

int DvpVideoIoctlAncillaryQueueBuffer(		dvp_v4l2_video_handle_t	*Context,
						unsigned int		 Index )
{
    if( Index >= Context->AncillaryBufferCount )
    {
	printk( "DvpVideoIoctlAncillaryQueryBuffer - Index out of bounds (%d).\n", Index );
	return -EINVAL;
    }

//

    if( Context->AncillaryBufferState[Index].Queued ||
	Context->AncillaryBufferState[Index].Done )
    {
	printk( "DvpVideoIoctlAncillaryQueryBuffer - Buffer already queued or awaiting dequeing.\n" );
	return -EINVAL;
    }

//

    memset( Context->AncillaryBufferState[Index].UnCachedAddress, 0x00, Context->AncillaryBufferSize );
    Context->AncillaryBufferState[Index].Queued			= true;

    Context->AncillaryBufferQueue[Context->AncillaryBufferNextQueueIndex % DVP_MAX_ANCILLARY_BUFFERS]	= Index;
    Context->AncillaryBufferNextQueueIndex++;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to de-queue a buffer
//

int DvpVideoIoctlAncillaryDeQueueBuffer(	dvp_v4l2_video_handle_t	*Context,
						unsigned int		*Index,
						bool			 Blocking )
{
    do
    {
	if( Blocking )
	    down_interruptible( &Context->DvpAncillaryBufferDoneSem );

//

	if( (Context->AncillaryBufferCount == 0) || !Context->AncillaryStreamOn )
	{
	    printk( "DvpVideoIoctlAncillaryDeQueueBuffer - Shutdown/StreamOff while waiting.\n" );
	    return -EINVAL;
	}

//

	if( Context->AncillaryBufferNextDeQueueIndex < Context->AncillaryBufferNextFillIndex )
	{
	    *Index	= Context->AncillaryBufferQueue[Context->AncillaryBufferNextDeQueueIndex % DVP_MAX_ANCILLARY_BUFFERS];
	    Context->AncillaryBufferNextDeQueueIndex++;

	    if( !Context->AncillaryBufferState[*Index].Done )
		printk( "DvpVideoIoctlAncillaryDeQueueBuffer - Internal state error, buffer is in done queue, but not marked as done - Implementation Error.\n" );

	    Context->AncillaryBufferState[*Index].Done	= false;

	    return 0;
	}
    } while( Blocking );

    return -EAGAIN;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to turn on stream aquisition
//

int DvpVideoIoctlAncillaryStreamOn(		dvp_v4l2_video_handle_t	*Context )
{
    Context->AncillaryStreamOn	= true;
    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to turn off stream aquisition
//

int DvpVideoIoctlAncillaryStreamOff(		dvp_v4l2_video_handle_t	*Context )
{
unsigned int		i;

    //
    // First mark us as off, and wait for any current capture activity to complete
    //

    Context->AncillaryStreamOn	= false;

    //
    // Clear the queued and done queues
    //

    Context->AncillaryBufferNextQueueIndex	= 0;
    Context->AncillaryBufferNextFillIndex	= 0;
    Context->AncillaryBufferNextDeQueueIndex	= 0;

    //
    // Tidy up the state records
    //

    for( i=0; i<Context->AncillaryBufferCount; i++ )
    {
	Context->AncillaryBufferState[i].Queued			= false;
	Context->AncillaryBufferState[i].Done			= false;
	Context->AncillaryBufferState[i].FillTime		= 0;
    }

    //
    // Make sure there is no-one waiting for a Dequeue
    //

    up(  &Context->DvpAncillaryBufferDoneSem );

//

    return 0;
}

