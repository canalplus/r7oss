/************************************************************************
Copyright (C) 2009 STMicroelectronics. All Rights Reserved.

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
 * V4L2 compositor capture output device driver for ST SoC display subsystems.
************************************************************************/

// #define OFFSET_THE_IMAGE
// #define DISABLE_DECIMATION       


/******************************
 * INCLUDES
 ******************************/
#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>

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
#include <linux/kthread.h>
#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "dvb_module.h"
#include "stmdisplayoutput.h"
#include "backend.h"
#include "dvb_video.h"
#include "osdev_device.h"
#include "ksound.h"
#include "linux/dvb/stm_ioctls.h"
#include "pseudo_mixer.h"
#include "monitor_inline.h"

#include "dvb_cap_video.h"

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

#define CapTimeForNFrames(N)		(((N) * Context->StreamInfo.FrameRateDenominator * 1000000ULL) / Context->StreamInfo.FrameRateNumerator)
#define CapCorrectedTimeForNFrames(N)	((CapTimeForNFrames((N)) * Context->CapFrameDurationCorrection) >> CAP_CORRECTION_FIXED_POINT_BITS);

#define CapRange(T)			(((T * CAP_MAX_SUPPORTED_PPM_CLOCK_ERROR) / 1000000) + 1)
#define CapValueMatchesFrameTime(V,T)	inrange( V, T - CapRange(T), T + CapRange(T) )

#define ControlValue(s)			((Context->CapControl##s == CAP_USE_DEFAULT) ? Context->CapControlDefault##s : Context->CapControl##s)

#define GetValue( s, v )		printk( "%s\n", s ); *Value = v;
#define Check( s, l, u )		printk( "%s\n", s ); if( (Value != CAP_USE_DEFAULT) && !inrange((int)Value,l,u) ) {printk( "Set Control - Value out of range (%d not in [%d..%d])\n", Value, l, u ); return -1; }
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
static cap_v4l2_video_handle_t *VideoContextLookupTable[16];

/******************************
 * SUPPORTED VIDEO MODES
 ******************************/

/* 
 * Capture Interface version of ModeParamsTable in the stmfb driver
 */
static const cap_v4l2_video_mode_line_t CapModeParamsTable[] =
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

    { ScalingFactor(800),
	        { 0x02, 0x3c, 0x02,
	          0x0b, 0x38, 0xfd,
	          0x14, 0x34, 0xf8,
	          0x1d, 0x2e, 0xf5,
	          0x26, 0x26, 0xf4,
	          0x2e, 0x1d, 0xf5,
	          0x35, 0x13, 0xf8,
	          0x3b, 0x0a, 0xfb } },       
   
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

static cap_v4l2_video_handle_t *CapVideoSysfsLookupContext (struct class_device *class_dev)
{
    int streamid = player_sysfs_get_stream_id(class_dev);
    BUG_ON(streamid < 0 ||
	   streamid >= ARRAY_SIZE(VideoContextLookupTable) ||
	   NULL == VideoContextLookupTable[streamid]);
    return VideoContextLookupTable[streamid];
}

// Define the macro for creating fns and declaring the attribute

// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE( CapName, LinuxName )							\
static ssize_t CapVideoSysfsShow##CapName( struct class_device *class_dev, char *buf )		\
{												\
    int v = atomic_read(&CapVideoSysfsLookupContext(class_dev)->Cap##CapName);			\
    return sprintf(buf, "%d\n", v);								\
}												\
												\
static ssize_t CapVideoSysfsStore##CapName( struct class_device *class_dev, const char *buf, size_t count )	\
{												\
    int v;											\
												\
    if (1 != sscanf(buf, "%i", &v))								\
       return -EINVAL;										\
												\
    if (v < 0)											\
       return -EINVAL;										\
												\
    atomic_set(&CapVideoSysfsLookupContext(class_dev)->Cap##CapName, v);			\
    return count;										\
}												\
												\
static CLASS_DEVICE_ATTR(LinuxName, 0600, CapVideoSysfsShow##CapName, CapVideoSysfsStore##CapName)
// ------------------------------------------------------------------------------------------------
#define SYSFS_CREATE( CapName, LinuxName )							\
{												\
int Result;											\
												\
    Result  = class_device_create_file(VideoContext->CapSysfsClassDevice,			\
                                      &class_device_attr_##LinuxName);				\
    if (Result) {										\
       DVB_ERROR("class_device_create_file failed (%d)\n", Result);				\
        return -1;										\
    }												\
												\
    player_sysfs_new_attribute_notification(VideoContext->CapSysfsClassDevice);			\
    atomic_set(&VideoContext->Cap##CapName, 0);							\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_PROCESS_DECREMENT( CapName, LinuxName )				\
SYSFS_DECLARE( CapName, LinuxName );								\
												\
static void CapVideoSysfsProcessDecrement##CapName( cap_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Cap##CapName);					\
												\
       if (new > 0)										\
           new--;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Cap##CapName, old, new));	\
												\
    if (old == 1)										\
       sysfs_notify (&((*(VideoContext->CapSysfsClassDevice)).kobj), NULL, #LinuxName );	\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_INTERRUPT_DECREMENT( CapName, LinuxName )				\
SYSFS_DECLARE( CapName, LinuxName );								\
												\
static void CapVideoSysfsInterruptDecrement##CapName( cap_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Cap##CapName);					\
												\
       if (new > 0)										\
           new--;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Cap##CapName, old, new));	\
												\
    if (old == 1)										\
    {												\
	VideoContext->Cap##CapName##Notify	= true;						\
	up( &VideoContext->CapSynchronizerWakeSem );						\
    }												\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_DECLARE_WITH_INTERRUPT_INCREMENT( CapName, LinuxName )				\
SYSFS_DECLARE( CapName, LinuxName );								\
												\
static void CapVideoSysfsInterruptIncrement##CapName( cap_v4l2_video_handle_t *VideoContext )	\
{												\
    int old, new;										\
												\
    /* conditionally decrement and notify one to zero transition. */				\
    do {											\
       old = new = atomic_read(&VideoContext->Cap##CapName);					\
												\
       if( new == 0)										\
           new++;										\
    } while (old != new && old != atomic_cmpxchg(&VideoContext->Cap##CapName, old, new));	\
												\
    if (old == 0)										\
    {												\
	VideoContext->Cap##CapName##Notify	= true;						\
	up( &VideoContext->CapSynchronizerWakeSem );						\
    }												\
}
// ------------------------------------------------------------------------------------------------
#define SYSFS_TEST_NOTIFY( CapName, LinuxName )							\
{												\
    if(	Context->Cap##CapName##Notify )								\
    {												\
	Context->Cap##CapName##Notify	= false;						\
	sysfs_notify (&((*(Context->CapSysfsClassDevice)).kobj), NULL, #LinuxName );		\
    }												\
}
// ------------------------------------------------------------------------------------------------

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
static int CapVideoSysfsCreateAttributes (cap_v4l2_video_handle_t *VideoContext)
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

    VideoContext->CapSysfsClassDevice = player_sysfs_get_class_device(playerplayback, playerstream);
    if (VideoContext->CapSysfsClassDevice == NULL) {
    	DVB_ERROR("get_class_device failed -> cannot create attribute \n");
        return -1;
    }

    streamid = player_sysfs_get_stream_id(VideoContext->CapSysfsClassDevice);
    if (streamid < 0 || streamid >= ARRAY_SIZE(VideoContextLookupTable)) {
	DVB_ERROR("streamid out of range -> cannot create attribute\n");
	return -1;
    }

    VideoContextLookupTable[streamid] = VideoContext;

//

    SYSFS_CREATE( FrameCountingNotification, 		frame_counting_notification );
    SYSFS_CREATE( FrameCaptureNotification,  		frame_capture_notification );
    SYSFS_CREATE( OutputCropTargetReachedNotification, 	output_crop_target_reached_notification );
    SYSFS_CREATE( PostMortem,				post_mortem );

//

    return 0;
}

// ////////////////////////////////////////////////////////////////////////////


static struct stmcore_vsync_cb               vsync_cb_info;

int CapVideoClose( cap_v4l2_video_handle_t	*Context )
{

int Result;
struct stmcore_display_pipeline_data  pipeline_data;

//

    if( Context->VideoRunning )
    {
        Context->VideoRunning 		= false;

        Context->Synchronize		= false;
        Context->SynchronizeEnabled	= false;
        Context->SynchronizerRunning	= false;

        up( &Context->CapPreInjectBufferSem );
        up( &Context->CapVideoInterruptSem );
        up( &Context->CapSynchronizerWakeSem );

	while( Context->DeviceContext->VideoStream != NULL )
	{
	   unsigned int  Jiffies = ((HZ)/1000)+1;

	    set_current_state( TASK_INTERRUPTIBLE );
	    schedule_timeout( Jiffies );
	}

        stmcore_get_display_pipeline(0,&pipeline_data);       
       
        Result = stmcore_unregister_vsync_callback(pipeline_data.display_runtime,
						      &vsync_cb_info);
        if ( Result )
        { 	    
	   printk("Error in %s: failed to deregister vsync callback\n",__FUNCTION__);
	   return -EINVAL;
	}
       
        Result	= DvbDisplayDelete (BACKEND_VIDEO_ID, Context->DeviceContext->Id);
        if( Result )
	{
	    printk("Error in %s: DisplayDelete failed\n",__FUNCTION__);
	    return -EINVAL;
	}


    }

    return 0;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to configure the vertical resize coefficients
//

static int CapConfigureVerticalResizeCoefficients( 	cap_v4l2_video_handle_t	*Context,
							unsigned int		 ScalingFactorStep )
{
volatile int 		*CapRegs	= Context->CapRegs;
unsigned int		 i;
unsigned int		 Index;
int			*Table;

    //
    // Select the table of coefficients
    //

    for( Index  = 0;  
	 (ScalingFactorStep > VerticalResizingFilters[Index].ForScalingFactorLessThan);
         Index++ );

    Table	= (int *)(&VerticalResizingFilters[Index].Coefficients);

    //
    // Program the filter
    //

    for( i=0; i<WORDS_PER_VERTICAL_FILTER; i++ )
	CapRegs[GAM_CAP_VFC(i)]	= Table[i];
   
    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to configure the horizontal resize coefficients
//

static int CapConfigureHorizontalResizeCoefficients( 	cap_v4l2_video_handle_t	*Context,
							unsigned int		 ScalingFactorStep )
{
volatile int 		*CapRegs	= Context->CapRegs;
unsigned int		 i;
unsigned int		 Index;
int			*Table;

    //
    // Select the table of coefficients
    //

    for( Index  = 0;  
	 (ScalingFactorStep > HorizontalResizingFilters[Index].ForScalingFactorLessThan);
         Index++ );

    Table	= (int *)(&HorizontalResizingFilters[Index].Coefficients);

    //
    // Program the filter
    //

    for( i=0; i<WORDS_PER_HORIZONTAL_FILTER; i++ )
	CapRegs[GAM_CAP_HFC(i)]	= Table[i];
 
    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to calculate what scalings should be applied to 
//	input data, based on the crop values and the current input mode.
//

static int CapRecalculateScaling( cap_v4l2_video_handle_t	*Context )
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

    Context->ScaledInputCrop	= Context->InputCrop;

//

//    printk("Hoz resize %d Vert resize %d\n",ControlValue(EnHsRc),ControlValue(EnVsRc));
	
    ScalingInputWidth	= (Context->InputCrop.Width != 0) ? Context->InputCrop.Width : Context->ModeWidth;
    ScalingInputHeight	= (Context->InputCrop.Height != 0) ? Context->InputCrop.Height : Context->ModeHeight;
    ScalingOutputWidth	= (Context->OutputCrop.Width != 0) ? Context->OutputCrop.Width : Context->ModeWidth;
    ScalingOutputHeight	= (Context->OutputCrop.Height != 0) ? Context->OutputCrop.Height : Context->ModeHeight;

//    printk("Scaled Output width %d height %d\n",ScalingOutputWidth,ScalingOutputHeight);
	
    if( (ScalingInputWidth == 0) || (ScalingOutputWidth == 0) )
    {
	printk("Scaling inputs not valid %d x %d\n",ScalingInputWidth,ScalingOutputWidth);
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

    NewHSRC		= CAP_SRC_OFF_VALUE;
    NewVSRC		= CAP_SRC_OFF_VALUE;
    ScaledInputWidth	= Context->ModeWidth;		// Not cropped version
    ScaledInputHeight	= Context->ModeHeight;		// Not cropped version

    if( ControlValue(EnHsRc) &&
	(ScalingInputWidth > ScalingOutputWidth) )
    {
	NewHSRC				= 0x01000000 | (((ScalingInputWidth -1) * 256) / (ScalingOutputWidth-1));

	Context->ScaledInputCrop.X	= ((Context->InputCrop.X     * ScalingOutputWidth) / ScalingInputWidth);
	Context->ScaledInputCrop.Width	= ((Context->InputCrop.Width * ScalingOutputWidth) / ScalingInputWidth);

	ScaledInputWidth		= ((Context->ModeWidth * ScalingOutputWidth) / ScalingInputWidth);
    }

//

    if( ControlValue(EnVsRc) &&
	(ScalingInputHeight > ScalingOutputHeight) )
    {
	NewVSRC				= 0x01000000 | (((ScalingInputHeight -1) * 256) / (ScalingOutputHeight-1));

	Context->ScaledInputCrop.Y	= ((Context->InputCrop.Y      * ScalingOutputHeight) / ScalingInputHeight);
	Context->ScaledInputCrop.Height	= ((Context->InputCrop.Height * ScalingOutputHeight) / ScalingInputHeight);

	ScaledInputHeight		= ((Context->ModeHeight * ScalingOutputHeight) / ScalingInputHeight);

       printk("New VSRC 0x%08x - Scaled input height %d Crop Y %d Crop Height %d\n",
	   NewVSRC,ScaledInputHeight,Context->ScaledInputCrop.Y,Context->ScaledInputCrop.Height);
    }

#if 0
NICK
    ScaledInputWidth = (ScaledInputWidth + 0x1f) & 0xffffffe0;
    ScaledInputHeight = (ScaledInputHeight + 0x1f) & 0xffffffe0;
#endif
   
    //
    // Calculate the other new register values.
    // This unfortunately is where I need to know the bytes per line
    // value. I import here, the knowledge that the display hardware
    // requires a 64 byte (32 pixel) allignment on a line.
    //

    printk("BufferBytesPerPixel %d - ScaledInputWidth %d\n",Context->BufferBytesPerPixel,ScaledInputWidth);
    if (Context->BufferBytesPerPixel == 0) Context->BufferBytesPerPixel = 2;
    BytesPerLine			= Context->BufferBytesPerPixel * ((ScaledInputWidth + 31) & 0xffffffe0);

    down_interruptible( &Context->CapScalingStateLock );
    
    Context->NextWidth			= ScaledInputWidth;
    Context->NextHeight			= ScaledInputHeight;
    Context->NextRegisterCMW		= ScaledInputWidth | ((ScaledInputHeight / (Context->StreamInfo.interlaced ? 2 : 1)) << 16);;
    Context->NextRegisterVMP		= BytesPerLine * (Context->StreamInfo.interlaced ? 2 : 1);
//    Context->NextRegisterVBPminusVTP	= (Context->StreamInfo.interlaced ? BytesPerLine : 0);
    Context->NextRegisterVBPminusVTP	= BytesPerLine;
    Context->NextRegisterHSRC		= NewHSRC;
    Context->NextRegisterVSRC		= NewVSRC;
    Context->NextInputWindow		= Context->ScaledInputCrop;
    Context->NextOutputWindow		= Context->OutputCrop;

    up( &Context->CapScalingStateLock );
   
    printk("Scaling input height %d scaling output height %d (%s)\n",
	   ScalingInputHeight,ScalingOutputHeight,
	   Context->StreamInfo.interlaced ? "interlaced":"progressive");
   
    //printk("NextRegisterVBPminusVTP 0x%x\n",Context->NextRegisterVBPminusVTP);
   
//

    return	0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to get a buffer
//

static int CapGetVideoBuffer( cap_v4l2_video_handle_t	*Context )
{
int			 Result;
unsigned int		 Dimensions[2];
unsigned int		 BufferIndex;
unsigned int         SurfaceFormat;
CapBufferStack_t	*Record;

//

    Record					= &Context->CapBufferStack[Context->CapNextBufferToGet % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];
    memset( Record, 0x00, sizeof(CapBufferStack_t) );

//

    down_interruptible( &Context->CapScalingStateLock );

    Dimensions[0]				= Context->NextWidth;
    Dimensions[0]                               = (Dimensions[0] + 0x1f) & 0xffffffe0;
   
    Dimensions[1]				= Context->NextHeight;
    Dimensions[1]                               = (Dimensions[1] + 0x1f) & 0xffffffe0;

    Record->RegisterCMW				= Context->NextRegisterCMW;
    Record->RegisterVMP				= Context->NextRegisterVMP;
    Record->RegisterVBPminusVTP			= Context->NextRegisterVBPminusVTP;
    Record->RegisterHSRC			= Context->NextRegisterHSRC;
    Record->RegisterVSRC			= Context->NextRegisterVSRC;

    Record->InputWindow				= Context->NextInputWindow;
    Record->OutputWindow			= Context->NextOutputWindow;

//    SurfaceFormat                               = SURF_ARGB8888;
    SurfaceFormat                               = SURF_RGB565;
//    SurfaceFormat                               = SURF_YCBCR422R;
   
    switch (SurfaceFormat)
    { 
       case SURF_RGB565:
           Context->CapControlCapFormat  = 0x0;
           Context->BufferBytesPerPixel  = 0x2;
           if (Context->CapControlSource == 0x0e) 
               Context->CapControlYCbCr2RGB = 0x1;
           break;
       case SURF_RGB888:
           Context->CapControlCapFormat  = 0x1;
           Context->BufferBytesPerPixel  = 0x3;
           if (Context->CapControlSource == 0x0e) 
               Context->CapControlYCbCr2RGB = 0x1;
           break;
       case SURF_ARGB8888:
           Context->CapControlCapFormat  = 0x5;
           Context->BufferBytesPerPixel  = 0x4;
           if (Context->CapControlSource == 0x0e) 
               Context->CapControlYCbCr2RGB = 0x1;
           break;       
       case SURF_YCBCR422R:
           Context->CapControlCapFormat  = 0x12;
           Context->BufferBytesPerPixel  = 0x2;
           Context->CapControlYCbCr2RGB  = 0;
           break;       
    }   
   
    up(  &Context->CapScalingStateLock );   

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

    Context->CapNextBufferToGet++;

    return 0;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to release currently held, but not injected buffers
//

static int CapReleaseBuffers( cap_v4l2_video_handle_t	*Context )
{
int			 Result;
CapBufferStack_t	*Record;

//

    while( Context->CapNextBufferToInject < Context->CapNextBufferToGet )
    {
	Record	= &Context->CapBufferStack[Context->CapNextBufferToInject % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];

	Result	= DvbStreamReturnDecodeBuffer( Context->DeviceContext->VideoStream, Record->Buffer );
	if( Result < 0 )
	{
	    printk("Error in %s: StreamReturnDecodeBuffer failed\n", __FUNCTION__);
	    // No point returning, we may as well try and release the rest
	}

	Context->CapNextBufferToInject++;
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to inject a buffer to the player
//

static int FrameCount = 0;

static int CapInjectVideoBuffer( cap_v4l2_video_handle_t	*Context )
{
int			 Result;
CapBufferStack_t	*Record;
unsigned long long	 ElapsedFrameTime;
unsigned long long	 PresentationTime;
unsigned long long	 Pts;
StreamInfo_t		 Packet;

//

    if( Context->CapNextBufferToInject >= Context->CapNextBufferToGet )
    {
	printk( "CapInjectVideoBuffer - No buffer yet to inject.\n" );
	return -1;
    }

    //
    // Make the call to set the time mapping.
    //

    Pts	= (((Context->CapBaseTime * 27) + 150) / 300) & 0x00000001ffffffffull;

    Result = cap_set_external_time_mapping (Context->SharedContext, Context->DeviceContext->VideoStream, Pts, Context->CapBaseTime );
    if( Result < 0 )
    {
	printk("CapInjectVideoBuffer - cap_enable_external_time_mapping failed\n" );
	return Result;
    }

//

    Record				= &Context->CapBufferStack[Context->CapNextBufferToInject % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    //
    // Calculate the expected fill time, Note the correction factor on the incoming values has 1 as 2^CAP_CORRECTION_FIXED_POINT_BITS.
    //

    Context->CapCalculatingFrameTime	= true;

    ElapsedFrameTime			= CapCorrectedTimeForNFrames(Context->CapFrameCount + Context->CapLeadInVideoFrames);

    Context->CapDriftFrameCount++;
    Context->CapLastDriftCorrection	= -(Context->CapCurrentDriftError * Context->CapDriftFrameCount) / (long long)(2 * CAP_MAXIMUM_TIME_INTEGRATION_FRAMES);

    Record->ExpectedFillTime		= Context->CapBaseTime + ElapsedFrameTime + Context->CapLastDriftCorrection;

    //
    // Rebase our calculation values
    //

    if( ElapsedFrameTime >= (1ULL << 31) )
    {
	Context->CapDriftFrameCount	= 0;				// We zero the drift data, because it is encapsulated in the ExpectedFillTime
	Context->CapLastDriftCorrection	= 0;
	Context->CapBaseTime		= Record->ExpectedFillTime;
	Context->CapFrameCount		= -Context->CapLeadInVideoFrames;
    }

    Context->CapCalculatingFrameTime	= false;

    //
    // Construct a packet to inject the information - NOTE we adjust the time to cope for a specific video latency
    //

    PresentationTime		= Record->ExpectedFillTime + Context->AppliedLatency;
    Pts				 = (((PresentationTime * 27) + 150) / 300) & 0x00000001ffffffffull;

    memcpy( &Packet, &Context->StreamInfo, sizeof(StreamInfo_t) );

    Packet.buffer 		= phys_to_virt( (unsigned int)Record->Data );
    Packet.buffer_class		= Record->Buffer;
    Packet.width		= Context->NextWidth;
    Packet.height		= Context->NextHeight;
    Packet.top_field_first	= ControlValue(TopFieldFirst);
    Packet.h_offset		= 0;
    Packet.InputWindow		= Record->InputWindow;
    Packet.OutputWindow		= Record->OutputWindow;
    Packet.pixel_aspect_ratio.Numerator	= Context->DeviceContext->PixelAspectRatio.Numerator;
    Packet.pixel_aspect_ratio.Denominator = Context->DeviceContext->PixelAspectRatio.Denominator;
    
#if 1    
    if (FrameCount++ == 128)
		printk("Buffer Address 0x%08x\n",(unsigned int)Packet.buffer);
#endif

//printk( "ElapsedFrameTime = %12lld - %016llx - %12lld, %12lld - %016llx\n", ElapsedFrameTime, Pts, StreamInfo.FrameRateNumerator, StreamInfo.FrameRateDenominator, CapFrameDurationCorrection );

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

    Context->CapFrameCount++;
    Context->CapNextBufferToInject++;

//

    return 0;
}



// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to configure the capture buffer pointers
//
unsigned long cap_address;
static int CapConfigureNextCaptureBuffer( cap_v4l2_video_handle_t	*Context,
					  unsigned long long		 Now )
{
volatile int 		*CapRegs	= Context->CapRegs;
CapBufferStack_t        *Record;
unsigned int		 BufferAdvance;
bool			 DroppedAFrame;

    //
    // select one to move onto
    //	    We ensure that the timing for capture is right,
    //      this may involve not filling buffers or refilling
    //	    the current buffer in an error condition
    //

    DroppedAFrame		= false;
    BufferAdvance		= 0;
    Record			= &Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    while( inrange((Now - Record->ExpectedFillTime), ((7 * CapTimeForNFrames(1)) / 8), 0x8000000000000000ULL) )
    {
	Context->CapNextBufferToFill++;
	if( Context->CapNextBufferToFill >= Context->CapNextBufferToGet )
	{
//	    printk( "CAP Video - No buffer to move onto - We dropped a frame (%d, %d) %d\n", Context->CapNextBufferToFill, Context->CapNextBufferToGet, Context->CapPreInjectBufferSem.count );
	    printk( "CAP DF\n" );					// Drasticaly shortened message we still need to see this, but the long message forces the condition to continue rather than fixing it
		DroppedAFrame	= true;

	    if( Context->StandardFrameRate )
		Context->CapInterruptFrameCount--;			// Pretend this interrupt never happened

	    Context->CapNextBufferToFill--;
	    break;
	}

	Record			= &Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];
	BufferAdvance++;
        up( &Context->CapPreInjectBufferSem );
    }

    if( (BufferAdvance == 0) && ((Context->CapNextBufferToFill + 1) != Context->CapNextBufferToGet) )
    {
	printk( "CAP Video - Too early to fill buffer(%d), Discarding a frame (%lld)\n", Context->CapNextBufferToFill, (Record->ExpectedFillTime - Now) );
	printk( "            %016llx %016llx %016llx %016llx\n",
		Context->CapBufferStack[(Context->CapNextBufferToFill -3) % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime,
		Context->CapBufferStack[(Context->CapNextBufferToFill -2) % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime,
		Context->CapBufferStack[(Context->CapNextBufferToFill -1) % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime,
		Context->CapBufferStack[(Context->CapNextBufferToFill -0) % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime );
    }
    else if( BufferAdvance > 1 )
    {
	printk( "CAP Video - Too late to fill buffer(%d), skipped %d buffers (%lld)\n", Context->CapNextBufferToFill, (BufferAdvance - 1), (Record->ExpectedFillTime - Now) );
	if( Context->StandardFrameRate )
	    Context->CapInterruptFrameCount	+= (BufferAdvance - 1);		// Compensate for the missed interrupts
    }

	Context->CapMissedFramesInARow	= DroppedAFrame ? (Context->CapMissedFramesInARow + 1) : 0;

    //
    // Update the Horizontal and Vertical re-sizing 
    //

    if( Record->RegisterHSRC != Context->LastRegisterHSRC )
    {
	Context->RegisterCTL		&= ~(1 << 10);			// HRESIZE_EN           - Clear out the resize enable bit

	if( Record->RegisterHSRC != CAP_SRC_OFF_VALUE )
	{
	    CapConfigureHorizontalResizeCoefficients( Context, (Record->RegisterHSRC & 0x00000fff) );
	    Context->RegisterCTL		|= (1 << 10);		// HRESIZE_EN           - Enable horizontal resize
	}

	Context->LastRegisterHSRC	= Record->RegisterHSRC;
    }

//

    if( Record->RegisterVSRC != Context->LastRegisterVSRC )
    {
	Context->RegisterCTL		&= ~(1 << 9);			// VRESIZE_EN           - Clear out the resize enable bit

	if( Record->RegisterVSRC != CAP_SRC_OFF_VALUE )
	{
	    CapConfigureVerticalResizeCoefficients( Context, (Record->RegisterVSRC & 0x00000fff) );
	    Context->RegisterCTL		|= (1 << 9);		// VRESIZE_EN           - Enable vertical resize
	}

	Context->LastRegisterVSRC	= Record->RegisterVSRC;
    }

    //
    // Move onto the new buffer
    //

#ifdef OFFSET_THE_IMAGE
    CapRegs[GAM_CAP_VTP]	= (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP);
    CapRegs[GAM_CAP_VBP]	= (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP) + Record->RegisterVBPminusVTP;
    cap_address = (unsigned int)Record->Data + 128 + (64 * Record->RegisterVMP);
#else
    CapRegs[GAM_CAP_VTP]	= (unsigned int)Record->Data;
    CapRegs[GAM_CAP_VBP]	= (unsigned int)Record->Data + Record->RegisterVBPminusVTP;
    cap_address = (unsigned int)Record->Data;
#endif

    CapRegs[GAM_CAP_HSRC]	= Record->RegisterHSRC;
    CapRegs[GAM_CAP_VSRC]	= Record->RegisterVSRC;
    CapRegs[GAM_CAP_PMP]	= Record->RegisterVMP;
    CapRegs[GAM_CAP_CMW]	= Record->RegisterCMW;		// Nick re-ordered, all registers should be configured before we hit CTL
    CapRegs[GAM_CAP_CTL]	= Context->RegisterCTL;
   
#if 0   
   if (frame_count++ == 100)
   {  	
      printk("Stored pitch %d ",CapRegs[GAM_CAP_PMP]);
      printk("Pix Width %d - Pix Height %d (%d)\n",
	     CapRegs[GAM_CAP_CMW] & 0x7ff,
	     (CapRegs[GAM_CAP_CMW] >> 16 ) & 0x7ff,
	     CapRegs[GAM_CAP_CMW] >> 30);
   }
#endif
//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//     Function to halt the capture hardware
//

static int CapHaltCapture( cap_v4l2_video_handle_t	*Context )
{
//unsigned int		 Tmp;
volatile int 		*CapRegs	= Context->CapRegs;

    //
    // Mark state
    //

    printk("%s\n",__PRETTY_FUNCTION__);
   
    Context->CapState		= CapMovingToInactive;

    //
    // make sure nothing is going on
    //

//    CapRegs[GAM_CAP_CTL]	= CapRegs[GAM_CAP_CTL] & ~(0x00000060);
    CapRegs[GAM_CAP_CTL]	= CapRegs[GAM_CAP_CTL] | (1 << 7);
    printk( "CTL = %08x\n", CapRegs[GAM_CAP_CTL] );

    //
    // Mark state
    //

    Context->CapState		= CapInactive;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to configure the capture hardware
//

static int CapParseModeValues( cap_v4l2_video_handle_t	*Context )
{
const stm_mode_params_t		*ModeParams;
const stm_timing_params_t	*TimingParams;
cap_v4l2_video_mode_t		 Mode;
bool				 Interlaced;
unsigned int			 HOffset;
unsigned int			 Width;
unsigned int			 VOffset;
unsigned int			 Height;

//

    ModeParams		= &Context->CapCaptureMode->ModeParams;
    TimingParams	= &Context->CapCaptureMode->TimingParams;
    Mode 		= Context->CapCaptureMode->Mode;

    //
    // Modify default control values based on current mode
    //     NOTE because some adjustments have an incestuous relationship,
    //		we need to calculate some defaults twice.
    //

    Context->CapControlDefaultOddPixelCount			= ((ModeParams->ActiveAreaWidth + ControlValue(ActiveAreaAdjustWidth)) & 1);
    Context->CapControlDefaultExternalVRefPolarityPositive	= TimingParams->VSyncPolarity;
    Context->CapControlDefaultHRefPolarityPositive		= TimingParams->HSyncPolarity;
    Context->CapControlDefaultVideoLatency			= (int)Context->CapLatency;

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

//

    if( CapTimeForNFrames(1) > 20000 )				// If frame time is more than 20ms then use counts appropriate to 30 or less fps
    {
	Context->CapWarmUpVideoFrames	= CAP_WARM_UP_VIDEO_FRAMES_30;
	Context->CapLeadInVideoFrames	= CAP_LEAD_IN_VIDEO_FRAMES_30;
    }
    else
    {
	Context->CapWarmUpVideoFrames	= CAP_WARM_UP_VIDEO_FRAMES_60;
	Context->CapLeadInVideoFrames	= CAP_LEAD_IN_VIDEO_FRAMES_60;
    }

    // Evaluate the picture aspect ratio. Only for NTSC and PAL I have the need to do that. 1:1 is the default.

    if ( Context->CapControlPictureAspectRatio == CAP_PICTURE_ASPECT_RATIO_4_3 )
    {
	switch ( Mode )
	{
	    case CAP_720_480_I60000:
	    case CAP_720_480_P60000:
	    case CAP_720_480_I59940:
	    case CAP_720_480_P59940:
		Context->DeviceContext->PixelAspectRatio.Numerator = 8;
		Context->DeviceContext->PixelAspectRatio.Denominator = 9;
	    	break;
	    case CAP_720_576_I50000:
	    case CAP_720_576_P50000:
	        Context->DeviceContext->PixelAspectRatio.Numerator = 16;
		Context->DeviceContext->PixelAspectRatio.Denominator = 15;
	    	break;
	    default:
	        Context->DeviceContext->PixelAspectRatio.Numerator = 1;
		Context->DeviceContext->PixelAspectRatio.Denominator = 1;
	    	break;
	}
    }
    else if ( Context->CapControlPictureAspectRatio == CAP_PICTURE_ASPECT_RATIO_16_9 )
    {
	switch ( Mode )
	{
	    case CAP_720_480_I60000:
	    case CAP_720_480_P60000:
	    case CAP_720_480_I59940:
	    case CAP_720_480_P59940:
	        Context->DeviceContext->PixelAspectRatio.Numerator = 32;
		Context->DeviceContext->PixelAspectRatio.Denominator = 27;
	    	break;
	    case CAP_720_576_I50000:
	    case CAP_720_576_P50000:
	        Context->DeviceContext->PixelAspectRatio.Numerator = 64;
		Context->DeviceContext->PixelAspectRatio.Denominator = 45;
	    	break;
	    default:
	    	Context->DeviceContext->PixelAspectRatio.Numerator = 1;
		Context->DeviceContext->PixelAspectRatio.Denominator = 1;
	    	break;
	}
    }
	
    //
    // Set appropriate policy to manage non progressive zoom format
    //
    
   Context->CapControlPixelAspectRatioCorrection = CAP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE; // cjt hack
   
    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_PIXEL_ASPECT_RATIO_CORRECTION,
	    Context->CapControlPixelAspectRatioCorrection );


    //
    // Calculate how many buffers we will need to pre-inject.
    // That is the buffers injected ahead of filling.
    //

    Context->CapBuffersRequiredToInjectAhead	= CAP_MAXIMUM_PLAYER_TRANSIT_TIME / CapTimeForNFrames(1) + 1;

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
    Context->AppliedLatency		= ControlValue(VideoLatency) - Context->CapLatency;

    DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED, 
			Context->SynchronizeEnabled ? PLAY_OPTION_VALUE_ENABLE : PLAY_OPTION_VALUE_DISABLE );

    //
    // Based on the mode<dimension> values, recalculate the scaling values.
    //

    CapRecalculateScaling( Context );

    //
    // Precalculate the register values
    //

    Interlaced			= Context->StreamInfo.interlaced;

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
    VOffset			= Context->StreamInfo.interlaced ? ((VOffset + 1) / 2) : VOffset;
#else
    VOffset			= Context->StreamInfo.interlaced ? (VOffset / 2) : VOffset;
#endif
   
//   Height			= Context->StreamInfo.interlaced ? ((Height + 1) / 2) : Height;
    Height			= Height;

   printk("Hoff %d Voff %d Width %d Height %d - %s\n",HOffset, VOffset, Width, Height,Interlaced?"interlaced":"progressive");
   
//    Context->RegisterCWO	= ((HOffset     ) | (TopVoffset          << 16));
    
    Context->RegisterCWO	= ( (VOffset          << 16)); // this works kinda
    Context->RegisterCWS	= (((Width - 1) ) | ((Height -1)     << 16)) + Context->RegisterCWO;
    Context->RegisterHLL	= Width ;
   
     {	
	int print_width  = (Context->RegisterCWS - Context->RegisterCWO) & 0x7ff;
	int print_height  = (Context->RegisterCWS - Context->RegisterCWO) >> 16;
	printk("Capture window aperture w - %d - h %d\n",print_width,print_height);
     }
	
   
    //
    // Setup the control register values (No constants, so I commented each field)
    //

    Context->RegisterCTL	= (ControlValue(CSignOut)	<< 27) | // Change chroma sign when capturing YCbCr888 or YCbCr422R
				  (ControlValue(CSignIn) 	<< 26) | // Color space converter, input chrominance components
				  (ControlValue(BF709Not601) 	<< 25) | // Color space converter, colorimetry selection
				  (ControlValue(YCbCr2RGB) 	<< 24) | // Enable YCbCr to RGB colour space conversion
				  (ControlValue(BigNotLittle) 	<< 23) | // Big or little endian bitmap
				  (ControlValue(CapFormat) 	<< 16) | // Capture buffer format
				  (ControlValue(EnHsRc) 	<< 10) | // Enable horizontal resize
				  (ControlValue(EnVsRc) 	<<  9) | // Enable vertical resize
				  (1                      	<<  8) | // Start capture !!
				  (ControlValue(SSCap) 		<<  7) | // Single shot capture
				  (ControlValue(TFCap) 		<<  6) | // Top field capture
				  (ControlValue(BFCap) 		<<  5) | // Back field capture
				  (ControlValue(VTGSel) 	<<  4) | // VTG 1 / 2 select
				  (ControlValue(Source) 	<<  0) ; // Source selection
     
//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to configure the capture hardware
//

static int CapConfigureCapture( cap_v4l2_video_handle_t	*Context )
{
volatile int 		*CapRegs	= Context->CapRegs;
CapBufferStack_t        *Record;

//

    Record			= &Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE];

    //
    // make sure nothing is going on
    //

//    CapRegs[GAM_CAP_CTL]	= CapRegs[GAM_CAP_CTL] & ~(0x00000060);	// Just disable capture
    CapRegs[GAM_CAP_CTL]	= CapRegs[GAM_CAP_CTL] | (1 << 7);
    printk( "CTL = %08x\n", CapRegs[GAM_CAP_CTL] );

    //
    // Update to incorporate the resizing
    //

    if( Record->RegisterHSRC != CAP_SRC_OFF_VALUE )
    {
	CapConfigureHorizontalResizeCoefficients( Context, (Record->RegisterHSRC & 0x00000fff) );
	Context->RegisterCTL		|= (1 << 10);		// HRESIZE_EN           - Enable horizontal resize
    }

    Context->LastRegisterHSRC	= Record->RegisterHSRC;

//

    if( Record->RegisterVSRC != CAP_SRC_OFF_VALUE )
    {
	CapConfigureVerticalResizeCoefficients( Context, (Record->RegisterVSRC & 0x00000fff) );
	Context->RegisterCTL		|= (1 << 9);		// VRESIZE_EN           - Enable vertical resize
    }

    Context->LastRegisterVSRC	= Record->RegisterVSRC;

    //
    // Program the structure registers
    //

    printk("Buffer start 0x%08x - Field offset 0x%x\n",(unsigned int)Record->Data,Record->RegisterVBPminusVTP);
   
   
    CapRegs[GAM_CAP_VTP]	= (unsigned int)Record->Data;
    CapRegs[GAM_CAP_VBP]	= (unsigned int)(Record->Data + Record->RegisterVBPminusVTP);
    CapRegs[GAM_CAP_HSRC]	= Record->RegisterHSRC;
    CapRegs[GAM_CAP_VSRC]	= Record->RegisterVSRC;
    CapRegs[GAM_CAP_PKZ]	= 0x5;

    CapRegs[GAM_CAP_CWO]	= Context->RegisterCWO; // capture window offset
    CapRegs[GAM_CAP_CWS]	= Context->RegisterCWS; // CWS cap window def
    CapRegs[GAM_CAP_CMW]	= Record->RegisterCMW;
    CapRegs[GAM_CAP_PMP]	= Record->RegisterVMP;  // pitch in bytes
    CapRegs[GAM_CAP_CTL]	= Context->RegisterCTL;	// Let er rip

   printk("CTL 0x%08x - CWO 0x%08x - CWS 0x%08x - VTP 0x%08x\nVBP 0x%08x - PMP 0x%08x - CMW 0x%08x - HSRC 0x%08x\nVSRC 0x%08x - PKZ 0x%08x\n",
	  CapRegs[GAM_CAP_CTL], CapRegs[GAM_CAP_CWO], CapRegs[GAM_CAP_CWS], CapRegs[GAM_CAP_VTP],
	  CapRegs[GAM_CAP_VBP], CapRegs[GAM_CAP_PMP], CapRegs[GAM_CAP_CMW], CapRegs[GAM_CAP_HSRC],
	  CapRegs[GAM_CAP_VSRC], CapRegs[GAM_CAP_PKZ] );
   
   {
      unsigned int i;
      for (i=0 ; i < WORDS_PER_HORIZONTAL_FILTER; ++i)
      {	   
	 printk("HFC%d 0x%08x\t",i,CapRegs[GAM_CAP_HFC(i)]);
	 if (i == 5) printk("\n");
      }      
      printk("\n");
      
      for (i=0 ; i < WORDS_PER_VERTICAL_FILTER; ++i)
	printk("VFC%d 0x%08x\t",i,CapRegs[GAM_CAP_VFC(i)]);	
      printk("\n");      
   }
	
    return 0;

}
// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to start capturing data
//

static int CapStartup( cap_v4l2_video_handle_t	*Context )
{
int			 Result;

    //
    // Configure the capture
    //

    Context->CapCalculatingFrameTime	= false;
    Context->CapBaseTime		= INVALID_TIME;
    Context->CapFrameCount		= 0;
#if 0
    Context->CapFrameDurationCorrection	= 1 << CAP_CORRECTION_FIXED_POINT_BITS;			// Fixed point 2^CAP_CORRECTION_FIXED_POINT_BITS is one.
#else
/*
{
static bool First = true;
    if( First )
        printk( "$$$$$$ Nick matches the CB101's duff clock $$$$$$\n" );
    First = false;
}
*/ 
    Context->CapFrameDurationCorrection	= 0xfff8551;						// Fixed point 0.999883
#endif
    Context->CapCurrentDriftError	= 0;
    Context->CapLastDriftCorrection	= 0;
    Context->CapDriftFrameCount		= 0;
    Context->CapState			= CapStarting;

    cap_invalidate_external_time_mapping(Context->SharedContext);

    sema_init( &Context->CapVideoInterruptSem, 0 );
    sema_init( &Context->CapPreInjectBufferSem, Context->CapBuffersRequiredToInjectAhead-1 );

    Result				= CapConfigureCapture( Context );
    if( Result < 0 )
    {
	printk( "CapStartup - Failed to configure capture hardware.\n" );
	return -EINVAL;
    }

//

    while( (Context->CapState != CapStarted) && Context->VideoRunning && !Context->FastModeSwitch )
	down_interruptible( &Context->CapVideoInterruptSem );

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to enter the run state, where we capture and move onto the next buffer
//

static int CapRun( cap_v4l2_video_handle_t	*Context )
{
unsigned long long	ElapsedFrameTime;
   
    //
    // Set the start time, and switch to moving to run state
    //

    ElapsedFrameTime			= CapTimeForNFrames(Context->CapLeadInVideoFrames + 1);
    Context->CapRunFromTime		= Context->CapBaseTime + ElapsedFrameTime - 8000;	// Allow for jitter by subtracting 8ms

    Context->CapState			= CapMovingToRun;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to stop capturing data
//

static int CapStop( cap_v4l2_video_handle_t	*Context )
{
int			 Result;

    //
    // Halt the capture
    //

    Result			= CapHaltCapture( Context );
    if( Result < 0 )
    {
	printk( "Error in %s: Failed to halt capture hardware.\n", __FUNCTION__ );
	return -EINVAL;
    }

    //
    // And adjust the state accordingly
    //

    Context->CapBaseTime	= INVALID_TIME;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	    Function to handle a warm up failure
//	    What warmup failure usually means, is that the
//	    frame rate is either wrong, or the source is
//	    way way out of spec.
//

static int CapFrameRate(	cap_v4l2_video_handle_t	*Context,
				unsigned long long 	 MicroSeconds,
				unsigned int		 Frames )
{
unsigned int		i;
unsigned long long 	MicroSecondsPerFrame;
unsigned int		NewCapBuffersRequiredToInjectAhead;

    //
    // First convert the us per frame into an idealized
    // number (one where the clock is perfectish).
    //

    MicroSecondsPerFrame	= ((MicroSeconds << CAP_CORRECTION_FIXED_POINT_BITS) + (Context->CapFrameDurationCorrection >> 1)) / Context->CapFrameDurationCorrection;
    MicroSecondsPerFrame	= (MicroSecondsPerFrame + (Frames/2)) / Frames;

    printk( "CapFrameRate - Idealized MicroSecondsPerFrame = %5lld (%6lld, %4d, %08llx)\n", MicroSecondsPerFrame, MicroSeconds, Frames, Context->CapFrameDurationCorrection );

//

    Context->StandardFrameRate	= true;

    if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 16667) )				// 60fps = 16666.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 60;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 16683) )			// 59fps = 16683.33 us
    {
	Context->StreamInfo.FrameRateNumerator		= 60000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 20000) )			// 50fps = 20000.00 us
    {
	Context->StreamInfo.FrameRateNumerator		= 50;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 33333) )			// 30fps = 33333.33 us
    {
	Context->StreamInfo.FrameRateNumerator		= 30;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 33367) )			// 29fps = 33366.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 30000;
	Context->StreamInfo.FrameRateDenominator	= 1001;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 40000) )			// 25fps = 40000.00 us
    {
	Context->StreamInfo.FrameRateNumerator		= 25;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 41667) )			// 24fps = 41666.67 us
    {
	Context->StreamInfo.FrameRateNumerator		= 25;
	Context->StreamInfo.FrameRateDenominator	= 1;
    }
    else if( CapValueMatchesFrameTime(MicroSecondsPerFrame, 41708) )			// 23fps = 41708.33 us
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

//

    if( Context->StandardFrameRate )
	printk( "CapFrameRate - Framerate = %lld/%lld (%s)\n", Context->StreamInfo.FrameRateNumerator, Context->StreamInfo.FrameRateDenominator,
								(Context->StreamInfo.interlaced ? "Interlaced":"Progressive") );

    //
    // Recalculate how many buffers we will need to pre-inject.
    // If this has increased, then allow more injections by performing up
    // on the appropriate semaphore.
    //

    NewCapBuffersRequiredToInjectAhead		= CAP_MAXIMUM_PLAYER_TRANSIT_TIME / CapTimeForNFrames(1) + 1;

    if( NewCapBuffersRequiredToInjectAhead > Context->CapBuffersRequiredToInjectAhead )
    {
	for( i = Context->CapBuffersRequiredToInjectAhead; i<NewCapBuffersRequiredToInjectAhead; i++ )
	    up( &Context->CapPreInjectBufferSem );

	Context->CapBuffersRequiredToInjectAhead = NewCapBuffersRequiredToInjectAhead;
    }

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	    Function to handle a warm up failure
//	    What warmup failure usually means, is that the
//	    frame rate is either wrong, or the source is
//	    way way out of spec.
//

static int CapWarmUpFailure( cap_v4l2_video_handle_t	*Context,
			     unsigned long long 	 MicroSeconds )
{
//    printk( "$$$ CapWarmUpFailure %5d => %5lld %5lld $$$\n", CapTimeForNFrames(1), CapTimeForNFrames(Context->CapInterruptFrameCount), MicroSeconds );

    //
    // Select an appropriate frame rate
    //

    CapFrameRate( Context, MicroSeconds, Context->CapInterruptFrameCount );

    //
    // Automatically switch to a slow startup
    //

    Context->CapWarmUpVideoFrames	= CAP_WARM_UP_VIDEO_FRAMES_60;
    Context->CapLeadInVideoFrames	= CAP_LEAD_IN_VIDEO_FRAMES_60;

//

    return 0;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//     Cap interrupt handler
//
//

void CapInterrupt(void* data, stm_field_t vsync)
{
cap_v4l2_video_handle_t	*Context;
volatile int 		*CapRegs;
ktime_t			 Ktime;
unsigned long long	 Now;
unsigned long long	 EstimatedBaseTime;
unsigned long long	 EstimatedBaseTimeRange;
unsigned long long 	 CorrectionFactor;
long long 		 CorrectionFactorChange;
long long		 AffectOfChangeOnPreviousFrameTimes;
long long		 ClampChange;
long long		 DriftError;
long long 		 DriftLimit;

//

    Context		= (cap_v4l2_video_handle_t *)data;
    CapRegs		= Context->CapRegs;

//

    if( Context->StreamInfo.interlaced &&
	(ControlValue(TopFieldFirst) == (vsync == STM_TOP_FIELD)) )
	return;

//

    Ktime 		= ktime_get();
    Now			= ktime_to_us( Ktime );

    Context->CapInterruptFrameCount++;

#if 0   
    if( (Context->CapState != CapInactive) && (Context->CapState != CapMovingToInactive) )
	printk( "CapI - %d, %d - %016llx - %d\n", Context->CapState, Context->CapInterruptFrameCount, Now, vsync );
#endif
   
    switch( Context->CapState )
    {
	case CapInactive:
//			printk( "CapInterrupt - Cap inactive - possible implementation error.\n" );
// Nick removed next line, primary use in DVP was to ensure interrupts turned off, not relevent here
//			CapHaltCapture( Context );				// Try and halt it
			break;

	case CapStarting:
			Context->CapBaseTime				= Now + CapTimeForNFrames(1);	// Force trigger
			Context->CapInterruptFrameCount			= 0;
			Context->CapwarmUpSynchronizationAttempts	= 0;
			Context->CapState				= CapWarmingUp;

	case CapWarmingUp:
			EstimatedBaseTime				= Now - CapCorrectedTimeForNFrames(Context->CapInterruptFrameCount);
			EstimatedBaseTimeRange				= 1 + (CapTimeForNFrames(Context->CapInterruptFrameCount) * CAP_MAX_SUPPORTED_PPM_CLOCK_ERROR)/1000000;

			if( !inrange( Context->CapBaseTime, (EstimatedBaseTime - EstimatedBaseTimeRange) , (EstimatedBaseTime + EstimatedBaseTimeRange)) &&
			    (Context->CapwarmUpSynchronizationAttempts < CAP_WARM_UP_TRIES) )
			{
//			    printk( "CapInterrupt - Base adjustment %4lld(%5lld) (%016llx[%d] - %016llx)\n", EstimatedBaseTime - Context->CapBaseTime, CapTimeForNFrames(1), EstimatedBaseTime, Context->CapInterruptFrameCount, Context->CapBaseTime );
			    Context->CapBaseTime			= Now;
			    Context->CapInterruptFrameCount		= 0;
			    Context->CapTimeAtZeroInterruptFrameCount	= Context->CapBaseTime;
			    Context->Synchronize			= Context->SynchronizeEnabled;		// Trigger vsync lock
			    Context->CapwarmUpSynchronizationAttempts++;
			    up( &Context->CapSynchronizerWakeSem );
			}

			if( Context->CapInterruptFrameCount < Context->CapWarmUpVideoFrames )
			    break;

//printk( "Warm up tries was %d\n", Context->CapwarmUpSynchronizationAttempts );

			if( Context->CapwarmUpSynchronizationAttempts >= CAP_WARM_UP_TRIES )
			    CapWarmUpFailure( Context, (Now - Context->CapBaseTime) );

			up( &Context->CapVideoInterruptSem );

			Context->CapState				= CapStarted;
			Context->CapIntegrateForAtLeastNFrames		= CAP_MINIMUM_TIME_INTEGRATION_FRAMES;

	case CapStarted:
                        MonitorSignalEvent( MONITOR_EVENT_VIDEO_FIRST_FIELD_ACQUIRED, NULL, "CapInterrupt: First field acquired" );
			break;


	case CapMovingToRun:

//printk( "Moving %12lld  %12lld - %016llx %016llx\n", (Now - Context->CapRunFromTime), (Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime - Now), Context->CapRunFromTime, Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime );

			if( Now < Context->CapRunFromTime )
			    break;

			Context->CapState				= CapRunning;

	case CapRunning:
			//
			// Switch to next capture buffers.
			//

			    CapConfigureNextCaptureBuffer( Context, Now );

			//
			// Keep the user up to date w.r.t. their frame timer/
			//

			CapVideoSysfsInterruptDecrementFrameCountingNotification(Context);
			CapVideoSysfsInterruptDecrementFrameCaptureNotification(Context);

			if( Context->CapMissedFramesInARow > CAP_MAX_MISSED_FRAMES_BEFORE_POST_MORTEM )
			    CapVideoSysfsInterruptIncrementPostMortem(Context);

			//
			// Do we wish to re-calculate the correction value,
			// have we integrated for long enough, and is this value
			// Unjittered.
			//

			DriftError	= Context->CapBufferStack[Context->CapNextBufferToFill % CAP_VIDEO_DECODE_BUFFER_STACK_SIZE].ExpectedFillTime - Now;

			if( Context->CapCalculatingFrameTime ||
			    (Context->CapInterruptFrameCount < (2 * Context->CapIntegrateForAtLeastNFrames)) ||
			    ((Context->CapInterruptFrameCount < Context->CapIntegrateForAtLeastNFrames) &&
			        ((Now - Context->CapTimeOfLastFrameInterrupt) > (CapTimeForNFrames(1) + CAP_MAXIMUM_FRAME_JITTER))) )
			{
			    if( !((Now - Context->CapTimeOfLastFrameInterrupt) > (CapTimeForNFrames(1) + CAP_MAXIMUM_FRAME_JITTER)) )
				Context->CapLastFrameDriftError	= DriftError;
			    break;
			}

			//
			// If running at a non-standard framerate, then try for an
			// update, keep the integration period at the minimum number of frames
			//

			if( !Context->StandardFrameRate )
			{
			    CapFrameRate( Context, (Now - Context->CapTimeAtZeroInterruptFrameCount), Context->CapInterruptFrameCount );

			    Context->CapIntegrateForAtLeastNFrames	= CAP_MINIMUM_TIME_INTEGRATION_FRAMES/2;
			}

			//
			// Re-calculate applying a clamp to the change
			//

			CorrectionFactor			= 0;		// Initialize for print purposes
			AffectOfChangeOnPreviousFrameTimes	= 0;

			if( Context->StandardFrameRate )
			{
			    CorrectionFactor			= ((Now - Context->CapTimeAtZeroInterruptFrameCount) << CAP_CORRECTION_FIXED_POINT_BITS) / CapTimeForNFrames(Context->CapInterruptFrameCount);
			    CorrectionFactorChange		= CorrectionFactor - Context->CapFrameDurationCorrection;

			    ClampChange				= CAP_MAXIMUM_TIME_INTEGRATION_FRAMES / Context->CapIntegrateForAtLeastNFrames;
			    ClampChange				= min( (128 * CAP_ONE_PPM), ((CAP_ONE_PPM * ClampChange * ClampChange) / 16) );
			    Clamp( CorrectionFactorChange, -ClampChange, ClampChange );

			    Context->CapFrameDurationCorrection	+= CorrectionFactorChange;

			    //
			    // Adjust the base time so that the change only affects frames
			    // after those already calculated.
			    //

			    AffectOfChangeOnPreviousFrameTimes	= CapTimeForNFrames(Context->CapFrameCount + Context->CapLeadInVideoFrames);
			    AffectOfChangeOnPreviousFrameTimes	= (AffectOfChangeOnPreviousFrameTimes * Abs(CorrectionFactorChange)) >> CAP_CORRECTION_FIXED_POINT_BITS;
			    Context->CapBaseTime		+= (CorrectionFactorChange < 0) ? AffectOfChangeOnPreviousFrameTimes : -AffectOfChangeOnPreviousFrameTimes;
			}

			//
			// Now calculate the drift error - limitting to 2ppm
			//

			Context->CapCurrentDriftError		= (Abs(DriftError) < Abs(Context->CapLastFrameDriftError)) ? DriftError : Context->CapLastFrameDriftError;

//printk( "oooh Last correction %4lld - Next correction %4lld (%4lld %4lld)\n", Context->CapLastDriftCorrection, Context->CapCurrentDriftError, DriftError, Context->CapLastFrameDriftError );

			Context->CapBaseTime			= Context->CapBaseTime + Context->CapLastDriftCorrection;
			Context->CapDriftFrameCount		= 0;
			Context->CapLastDriftCorrection		= 0;

			DriftLimit				= (1 * CapTimeForNFrames(CAP_MAXIMUM_TIME_INTEGRATION_FRAMES)) / 1000000;
			Clamp( Context->CapCurrentDriftError, -DriftLimit, DriftLimit );

                        printk( "New Correction factor %lld.%09lld [%3lld] (%lld.%09lld - %5d) - %5lld\n",
                                    (Context->CapFrameDurationCorrection >> CAP_CORRECTION_FIXED_POINT_BITS),
                                    (((Context->CapFrameDurationCorrection & 0xfffffff) * 1000000000) >> CAP_CORRECTION_FIXED_POINT_BITS),
				    AffectOfChangeOnPreviousFrameTimes,
				    (CorrectionFactor >> CAP_CORRECTION_FIXED_POINT_BITS),
                                    (((CorrectionFactor & 0xfffffff) * 1000000000) >> CAP_CORRECTION_FIXED_POINT_BITS),
				    Context->CapInterruptFrameCount, Context->CapCurrentDriftError );

			//
			// Initialize for next integration
			//

			Context->CapInterruptFrameCount			= 0;
			Context->CapTimeAtZeroInterruptFrameCount	= Now;
			Context->CapIntegrateForAtLeastNFrames		= (Context->CapIntegrateForAtLeastNFrames >= CAP_MAXIMUM_TIME_INTEGRATION_FRAMES) ?
										CAP_MAXIMUM_TIME_INTEGRATION_FRAMES :
										(Context->CapIntegrateForAtLeastNFrames * 2);
			break;

	case CapMovingToInactive:
			break;
    }

    //
    // Update our history
    //

    Context->CapTimeOfLastFrameInterrupt		= Now;

}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Cap version of the stream event handler
//

static void CapEventHandler( context_handle_t		 EventContext,
			     struct stream_event_s	*Event )
{
cap_v4l2_video_handle_t	*Context;

    //
    // Initialize context variables
    //

    Context			= (cap_v4l2_video_handle_t *)EventContext;

    //
    // Is the event for us - if not just lose it
    //

    switch( Event->code )
    {
	case STREAM_EVENT_VSYNC_OFFSET_MEASURED:
		cap_set_vsync_offset( Context->SharedContext, Event->u.longlong );
		break;

	case STREAM_EVENT_OUTPUT_SIZE_CHANGED:
		if( !Context->OutputCropTargetReached &&
		    (Event->u.rectangle.x == Context->OutputCropTarget.X) &&
		    (Event->u.rectangle.y == Context->OutputCropTarget.Y) &&
		    (Event->u.rectangle.width == Context->OutputCropTarget.Width) &&
		    (Event->u.rectangle.height == Context->OutputCropTarget.Height) )
		{
		    CapVideoSysfsProcessDecrementOutputCropTargetReachedNotification(Context);
		    Context->OutputCropTargetReached	= true;
		}
		break;

	default:
		break;
    }
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Function to handle a step in the output crop
//

static void   CapPerformOutputCropStep( cap_v4l2_video_handle_t	*Context )
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

    CapRecalculateScaling( Context );
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Video cap thread, re-written to handle low latency
//

static int CapVideoThread( void *data )
{
cap_v4l2_video_handle_t	*Context;
int			 Result;

const stm_mode_line_t*                CaptureCurrentDisplayMode = NULL;
const stm_mode_line_t*                DisplayCurrentDisplayMode = NULL;
const stm_mode_line_t*                PreviousDisplayMode = (stm_mode_line_t*)&CapModeParamsTable[CAP_640_480_P59940];
struct stmcore_display_pipeline_data  PipelineData;
//struct v4l2_crop                      Crop;

//struct v4l2_crop newCrop;

	
    //
    // Initialize context variables
    //

    Context			= (cap_v4l2_video_handle_t *)data;

    //
    // Initialize then player with a stream
    //

    Result = DvbPlaybackAddStream(	Context->DeviceContext->Playback,
				BACKEND_VIDEO_ID,
				BACKEND_PES_ID,
				BACKEND_DVP_ID,
				DEMUX_INVALID_ID,
				Context->DeviceContext->Id,			// SurfaceId .....
				&Context->DeviceContext->VideoStream );
    if( Result < 0 )
    {
	printk( "CapVideoThread - PlaybackAddStream failed with %d\n", Result );
	return -EINVAL;
    }

    Result = CapVideoSysfsCreateAttributes(Context);
    if (Result) {
	printk("CapVideoThread - Failed to create sysfs attributes\n");
	// non-fatal
    }

    Context->DeviceContext->VideoState.play_state = VIDEO_PLAYING;

    //
    // Interject our event handler
    //

    DvbStreamRegisterEventSignalCallback( 	Context->DeviceContext->VideoStream, (context_handle_t)Context, 
					(stream_event_signal_callback)CapEventHandler );

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

	Context->CapNextBufferToGet	= 0;
	Context->CapNextBufferToInject	= 0;
	Context->CapNextBufferToFill	= 0;

	Context->FastModeSwitch		= false;
	    

	//
	// Check to see that the display mode hasn't changed underneath us.
	// cjt is this the correct place for this check ?
	// 
	    
       stmcore_get_display_pipeline(0,&PipelineData);		     
       CaptureCurrentDisplayMode = stm_display_output_get_current_display_mode(PipelineData.display_runtime->main_output);

       stmcore_get_display_pipeline(1,&PipelineData);		     
       DisplayCurrentDisplayMode = stm_display_output_get_current_display_mode(PipelineData.display_runtime->main_output);
       
        if ((CaptureCurrentDisplayMode != NULL) && (CaptureCurrentDisplayMode->Mode != PreviousDisplayMode->Mode))
	{
	    printk("/\\/\\/\\ Updating Capture Display Settings w %4ld h %4ld %s /\\/\\/\\\n",
		   CaptureCurrentDisplayMode->ModeParams.ActiveAreaWidth,
		   CaptureCurrentDisplayMode->ModeParams.ActiveAreaHeight,
		   (CaptureCurrentDisplayMode->ModeParams.ScanType == SCAN_P) ? "progressive":"interlaced");

	   
	    printk("/\\/\\/\\ Updating Output  Display Settings w %4ld h %4ld %s /\\/\\/\\\n",
		   DisplayCurrentDisplayMode->ModeParams.ActiveAreaWidth,
		   DisplayCurrentDisplayMode->ModeParams.ActiveAreaHeight,
		   (DisplayCurrentDisplayMode->ModeParams.ScanType == SCAN_P) ? "progressive":"interlaced");
	   
	   
	    if (CapVideoIoctlSetStandard(Context,CaptureCurrentDisplayMode->Mode) == 0)
	        PreviousDisplayMode = CaptureCurrentDisplayMode;

	    if (Context->OutputCrop.Width > 720 || Context->OutputCrop.Width == 0)
	    {	
	    
	        down_interruptible( &Context->CapScalingStateLock );	       
	       
		Context->InputCrop.Width  = CaptureCurrentDisplayMode->ModeParams.ActiveAreaWidth;
		Context->InputCrop.Height = CaptureCurrentDisplayMode->ModeParams.ActiveAreaHeight;
		Context->InputCrop.X = 0;
		Context->InputCrop.Y = 0;		    		    
		    
		Context->OutputCrop.Width  = DisplayCurrentDisplayMode->ModeParams.ActiveAreaWidth;
		Context->OutputCrop.Height = DisplayCurrentDisplayMode->ModeParams.ActiveAreaHeight;
		Context->OutputCrop.X = 0;
		Context->OutputCrop.Y = 0;

	        up( &Context->CapScalingStateLock );	   
	       
	        CapParseModeValues(Context);
		CapRecalculateScaling(Context);
		
/*	       
	       Crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	       Crop.c.left   = 0;
	       Crop.c.top    = 0;
	       Crop.c.width  = CaptureCurrentDisplayMode->ModeParams.ActiveAreaWidth;;
	       Crop.c.height = CaptureCurrentDisplayMode->ModeParams.ActiveAreaHeight;
	       
	       CapVideoIoctlCrop(Context,&Crop);
	       
	       
	       Crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	       Crop.c.left   = 0;
	       Crop.c.top    = 0;
	       Crop.c.width  = DisplayCurrentDisplayMode->ModeParams.ActiveAreaWidth;
	       Crop.c.height = DisplayCurrentDisplayMode->ModeParams.ActiveAreaHeight;
	       
	       CapVideoIoctlCrop(Context,&Crop);
*/	    
	    }
		
	}
	
	    
	//
	// Insert check for no standard set up
	//

	while( Context->CapCaptureMode == NULL )
	{
	    printk( "CapVideoThread - Attempting to run without a video standard specified, waiting for a VIDIOC_S_STD ioctl call.\n" );
	    down_interruptible( &Context->CapVideoInterruptSem );

	    Context->FastModeSwitch	= !Context->VideoRunning;
	}

	//

	if( Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= CapParseModeValues( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= CapGetVideoBuffer( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= CapStartup( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= CapInjectVideoBuffer( Context );
	if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	    Result	= CapRun( Context );

	//
	// Enter the main loop for a running sequence
	//

	while( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
	{
	    down_interruptible( &Context->CapPreInjectBufferSem );

	    if( Context->OutputCropStepping )
		CapPerformOutputCropStep( Context );

	    if( Context->VideoRunning && !Context->FastModeSwitch )
	        Result	= CapGetVideoBuffer( Context );

	    if( (Result == 0) && Context->VideoRunning && !Context->FastModeSwitch )
		Result	= CapInjectVideoBuffer( Context );
	}

	if( Result < 0 )
	    break;

	//
	// Shutdown the running sequence, either before exiting, or for a format switch
	//

	CapStop( Context );
	DvbStreamDrain( Context->DeviceContext->VideoStream, 1 );
	CapReleaseBuffers( Context );
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
//	Cap synchronizer thread, to start the output at a specified time
//

static int CapSynchronizerThread( void *data )
{
cap_v4l2_video_handle_t	*Context;
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

    Context				= (cap_v4l2_video_handle_t *)data;
    Context->SynchronizerRunning	= true;

    //
    // Enter the main loop waiting for something to happen
    //

    while( Context->SynchronizerRunning )
    {
	down_interruptible( &Context->CapSynchronizerWakeSem );

	//
	// Do we want to do something
	//

	SYSFS_TEST_NOTIFY( FrameCountingNotification, 		frame_counting_notification );
	SYSFS_TEST_NOTIFY( FrameCaptureNotification,  		frame_capture_notification );
	SYSFS_TEST_NOTIFY( PostMortem,				post_mortem );

	while( Context->SynchronizerRunning && Context->SynchronizeEnabled && Context->Synchronize )
	{
	    //
	    // When do we want to trigger (try for 32 us early to allow time
	    // for the procedure to execute, and to shake the sleep from my eyes).
	    //

	    TriggerPoint		= Context->CapBaseTime + ControlValue(VideoLatency) - 32;
	    FrameDuration		= CapTimeForNFrames(1);

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

int CapVideoIoctlSetFramebuffer(        cap_v4l2_video_handle_t	*Context,
					unsigned int   		 Width,
                                        unsigned int   		 Height,
                                        unsigned int   		 BytesPerLine,
                                        unsigned int   		 Control )
{
printk( "VIDIOC_S_FBUF: DOING NOTHING !!!! - W = %4d, H = %4d, BPL = %4d, Crtl = %08x\n", Width, Height, BytesPerLine, Control );

    return 0;
}

// -----------------------------

int CapVideoIoctlSetStandard(		cap_v4l2_video_handle_t	*Context,
					v4l2_std_id		 Id )
{
int 			 i;
const stm_mode_line_t   *NewCaptureMode		= NULL;

//

    printk( "VIDIOC_S_STD: %08x\n", (unsigned int) Id );

//

    for( i = 0; i < N_ELEMENTS (CapModeParamsTable); i++ )
     { 
	
	if( (unsigned int)CapModeParamsTable[i].Mode == (unsigned int)Id )
	{
	    NewCaptureMode = (stm_mode_line_t*)&CapModeParamsTable[i];
	    break;
	}
     }
   
    if (!NewCaptureMode)
    {
	printk( "CapVideoIoctlSetStandard - Mode not supported\n" );
	return -EINVAL;
    }

    //
    // we have a new capture mode, set the master copy, and force
    // a mode switch if any video sequence is already running.
    //

    CapHaltCapture( Context );

    Context->CapCaptureMode		= NewCaptureMode;
    Context->FastModeSwitch		= true;

    up( &Context->CapPreInjectBufferSem );
    up( &Context->CapVideoInterruptSem );

    return 0;
}

// -----------------------------


int CapVideoIoctlOverlayStart(	cap_v4l2_video_handle_t	*Context )
{
unsigned int		 i;
char 			 TaskName[32];
struct task_struct	*Task;
struct sched_param 	 Param;
	
struct stmcore_display_pipeline_data  pipeline_data;

//

   printk( "*** VIDIOC_OVERLAY: ****\n" );

    if( Context == NULL )
    {
	printk( "CapVideoIoctlOverlayStart - Video context NULL\n" );
	return -EINVAL;
    }

//

    Context->CapLatency		= (Context->SharedContext->target_latency * 1000);

//

    if( Context->VideoRunning )
    {
	printk("CapVideoIoctlOverlayStart - Capture Thread already started\n" );
	return 0;
    }

//

    Context->CapLatency         = (Context->SharedContext->target_latency * 1000);
    Context->VideoRunning		= false;
    Context->CapState			= CapInactive;

    Context->SynchronizerRunning	= false;
    Context->Synchronize		= false;

//

    sema_init( &Context->CapVideoInterruptSem, 0 );
    sema_init( &Context->CapSynchronizerWakeSem, 0 );
    sema_init( &Context->CapPreInjectBufferSem, 0 );

    sema_init( &Context->CapScalingStateLock, 1 );
	
    stmcore_get_display_pipeline(0,&pipeline_data);	
	
    INIT_LIST_HEAD(&(vsync_cb_info.node));
    vsync_cb_info.owner   = THIS_MODULE;
    vsync_cb_info.context = (void*)Context;
    vsync_cb_info.cb      = CapInterrupt;
	
    if(stmcore_register_vsync_callback(pipeline_data.display_runtime,
				       &vsync_cb_info)<0)
    {
	    printk(KERN_ERR "********** Cannot register vsync callback ***********\n");
	    return -ENODEV;
    }	
   
    //
    // Create the video tasks
    //

    strcpy( TaskName, "cap video task" );
    Task 		= kthread_create( CapVideoThread, Context, "%s", TaskName );
    if( IS_ERR(Task) )
    {
	printk( "CapVideoIoctlOverlayStart - Unable to start video thread\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	return -EINVAL;
    }

    Param.sched_priority 	= CAP_VIDEO_THREAD_PRIORITY;
    if( sched_setscheduler( Task, SCHED_RR, &Param) )
    {
	printk("CapVideoIoctlOverlayStart - FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Param.sched_priority);
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	return -EINVAL;
    }

    wake_up_process( Task );

//

    strcpy( TaskName, "cap synchronizer task" );
    Task 		= kthread_create( CapSynchronizerThread, Context, "%s", TaskName );
    if( IS_ERR(Task) )
    {
	printk( "CapVideoIoctlOverlayStart - Unable to start synchronizer thread\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	return -EINVAL;
    }

    Param.sched_priority 	= CAP_SYNCHRONIZER_THREAD_PRIORITY;
    if( sched_setscheduler( Task, SCHED_RR, &Param) )
    {
	printk("CapVideoIoctlOverlayStart - FAILED to set scheduling parameters to priority %d (SCHED_RR)\n", Param.sched_priority);
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
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
	printk( "CapVideoIoctlOverlayStart - FAILED to set start processes\n" );
	Context->VideoRunning		= false;
	Context->SynchronizerRunning	= false;
	return -EINVAL;
    }

//

    return 0;
}


// -----------------------------


int CapVideoIoctlCrop(	cap_v4l2_video_handle_t		*Context,
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
	    printk( "CapVideoIoctlCrop: Setting output crop before previous crop has reached display\n" );
	}

        atomic_set(&Context->CapOutputCropTargetReachedNotification, 1);

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
	    printk( "CapVideoIoctlCrop: Setting output crop to be unchanged - marking as target on display immediately.\n" );
	    CapVideoSysfsProcessDecrementOutputCropTargetReachedNotification(Context);
	    Context->OutputCropTargetReached	= true;
	}
	else
	{
	    switch( ControlValue(OutputCropTransitionMode) )
	    {
	    	default:
			printk( "CapVideoIoctlCrop: Unsupported output crop transition mode (%d) defaulting to single step. \n ", ControlValue(OutputCropTransitionMode) );

	    	case CAP_TRANSITION_MODE_SINGLE_STEP:
			Context->OutputCrop.X			= Context->OutputCropTarget.X;
			Context->OutputCrop.Y			= Context->OutputCropTarget.Y;
			Context->OutputCrop.Width		= Context->OutputCropTarget.Width;
			Context->OutputCrop.Height		= Context->OutputCropTarget.Height;
			CapRecalculateScaling( Context );
			break;

	    	case CAP_TRANSITION_MODE_STEP_OVER_N_VSYNCS:
			Context->OutputCropSteps		= ControlValue(OutputCropTransitionModeParameter0);
			Context->OutputCropCurrentStep		= 0;

			Context->OutputCropStepping		= true;
			break;
	    }
	}
    }
    else if (Crop->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
	Context->InputCrop.X		= Crop->c.left;
	Context->InputCrop.Y		= Crop->c.top;
	Context->InputCrop.Width	= Crop->c.width;
	Context->InputCrop.Height	= Crop->c.height;
	CapRecalculateScaling( Context );
    }
    else {
	printk( "CapVideoIoctlCrop: Unknown type %d. \n ", Crop->type );
	return -EINVAL;
    }

//

    return 0;
}


// -----------------------------


int CapVideoIoctlSetControl( 		cap_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		 Value )
{
int	ret;

printk( "VIDIOC_S_CTRL->V4L2_CID_STM_CAPIF_" );

    switch( Control )
    {
	case V4L2_CID_STM_CAPIF_RESTORE_DEFAULT:
		printk( "RESTORE_DEFAULT\n" );

		Context->CapControlCSignOut					= CAP_USE_DEFAULT;
		Context->CapControlCSignIn					= CAP_USE_DEFAULT;
	 	Context->CapControlBF709Not601					= CAP_USE_DEFAULT;
	 	Context->CapControlYCbCr2RGB					= CAP_USE_DEFAULT;
	 	Context->CapControlBigNotLittle					= CAP_USE_DEFAULT;
	 	Context->CapControlCapFormat					= CAP_USE_DEFAULT;
	 	Context->CapControlEnHsRc					= CAP_USE_DEFAULT;
	 	Context->CapControlEnVsRc					= CAP_USE_DEFAULT;
		Context->CapControlSSCap					= CAP_USE_DEFAULT;
	 	Context->CapControlTFCap					= CAP_USE_DEFAULT;
	 	Context->CapControlBFCap					= CAP_USE_DEFAULT;
	 	Context->CapControlVTGSel					= CAP_USE_DEFAULT;
	 	Context->CapControlSource					= CAP_USE_DEFAULT;
       
		Context->CapControlBigEndian					= CAP_USE_DEFAULT;
		Context->CapControlFullRange					= CAP_USE_DEFAULT;
		Context->CapControlIncompleteFirstPixel				= CAP_USE_DEFAULT;
		Context->CapControlOddPixelCount				= CAP_USE_DEFAULT;
		Context->CapControlVsyncBottomHalfLineEnable			= CAP_USE_DEFAULT;
		Context->CapControlExternalVRefPolarityPositive			= CAP_USE_DEFAULT;
		Context->CapControlHRefPolarityPositive				= CAP_USE_DEFAULT;
		Context->CapControlActiveAreaAdjustHorizontalOffset		= CAP_USE_DEFAULT;
		Context->CapControlActiveAreaAdjustVerticalOffset		= CAP_USE_DEFAULT;
		Context->CapControlActiveAreaAdjustWidth			= CAP_USE_DEFAULT;
		Context->CapControlActiveAreaAdjustHeight			= CAP_USE_DEFAULT;
		Context->CapControlColourMode					= CAP_USE_DEFAULT;
		Context->CapControlVideoLatency					= CAP_USE_DEFAULT;
		Context->CapControlBlank					= CAP_USE_DEFAULT;
		Context->CapControlTopFieldFirst				= CAP_USE_DEFAULT;
		Context->CapControlVsyncLockEnable				= CAP_USE_DEFAULT;
		Context->CapControlPixelAspectRatioCorrection			= CAP_USE_DEFAULT;
		Context->CapControlOutputCropTransitionMode			= CAP_USE_DEFAULT;
		Context->CapControlOutputCropTransitionModeParameter0		= CAP_USE_DEFAULT;

       
		Context->CapControlDefaultCSignOut				= 0;
		Context->CapControlDefaultCSignIn				= 0;
	 	Context->CapControlDefaultBF709Not601				= 0;
	 	Context->CapControlDefaultYCbCr2RGB				= 0;
	 	Context->CapControlDefaultBigNotLittle				= 0;
	 	Context->CapControlDefaultCapFormat				= 0x0;  // RGB565
//	 	Context->CapControlDefaultCapFormat				= 0x5;  // ARGB8888
//	 	Context->CapControlDefaultCapFormat				= 0x12; // YCbCr4:2:2R

#ifdef DISABLE_DECIMATION       
	 	Context->CapControlDefaultEnHsRc				= 0;
	 	Context->CapControlDefaultEnVsRc				= 0;
#else       
	 	Context->CapControlDefaultEnHsRc				= 1;
	 	Context->CapControlDefaultEnVsRc				= 1;       
#endif       
		Context->CapControlDefaultSSCap 	    			= 0;
	 	Context->CapControlDefaultTFCap 				= 1;
	 	Context->CapControlDefaultBFCap 				= 1;
	 	Context->CapControlDefaultVTGSel				= 0;
//	 	Context->CapControlDefaultSource				= 0x1; // Video output 
//	 	Context->CapControlDefaultSource				= 0xe; // mixer output 422R
	 	Context->CapControlDefaultSource				= 0xf; // mixer output RGB

		Context->CapControlDefaultBigEndian				= 0;
		Context->CapControlDefaultFullRange				= 0;
		Context->CapControlDefaultIncompleteFirstPixel			= 0;
		Context->CapControlDefaultOddPixelCount				= 0;
		Context->CapControlDefaultVsyncBottomHalfLineEnable		= 0;
		Context->CapControlDefaultHRefPolarityPositive			= 0;
		Context->CapControlDefaultActiveAreaAdjustHorizontalOffset	= 0;
		Context->CapControlDefaultActiveAreaAdjustVerticalOffset	= 0;
		Context->CapControlDefaultActiveAreaAdjustWidth			= 0;
		Context->CapControlDefaultActiveAreaAdjustHeight		= 0;
		Context->CapControlDefaultColourMode				= 0;
		Context->CapControlDefaultVideoLatency				= 0xffffffff;
		Context->CapControlDefaultBlank					= 0;
		Context->CapControlDefaultTopFieldFirst				= 1;
		Context->CapControlDefaultVsyncLockEnable			= 0;
		Context->CapControlDefaultOutputCropTransitionMode		= CAP_TRANSITION_MODE_SINGLE_STEP;
		Context->CapControlDefaultOutputCropTransitionModeParameter0	= 8;
		Context->CapControlDefaultPixelAspectRatioCorrection		= CAP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE;

#if 0
Context->CapControlOutputCropTransitionMode			= CAP_TRANSITION_MODE_STEP_OVER_N_VSYNCS;
Context->CapControlOutputCropTransitionModeParameter0		= 50;
#endif
		break;

	case  V4L2_CID_STM_CAPIF_BLANK:
		Check( "CAPIF_BLANK", 0, 1 );
		if( Context->DeviceContext->VideoStream == NULL )
		    return -EINVAL;

		ret = DvbStreamEnable( Context->DeviceContext->VideoStream, Value );
		if( ret < 0 )
		{
		    printk( "CapVideoIoctlSetControl - StreamEnable failed (%d)\n", Value );
		    return -EINVAL;
		}

		Context->CapControlBlank	= Value;
		break;

       
	case  V4L2_CID_STM_CAPIF_MIXER_INPUT:
		Check( "MIXER_INPUT", 0, 0xf);
                Context->CapControlSource = 0xf;
//                printk("Capture Source now 0x%0x\n",Context->CapControlSource );
		//
		// we have a new capture source, force a mode 
		// switch if any video sequence is already running.
		//

		CapHaltCapture( Context );
		Context->FastModeSwitch		= true;

		up( &Context->CapPreInjectBufferSem );
		up( &Context->CapVideoInterruptSem );
		break;
	case  V4L2_CID_STM_CAPIF_VIDEO_INPUT:
		CheckAndSet( "VIDEO_INPUT", 0, 0x1, Context->CapControlSource );
//                printk("Capture Source now 0x%0x\n",Context->CapControlSource );
		//
		// we have a new capture source, force a mode 
		// switch if any video sequence is already running.
		//

		CapHaltCapture( Context );
		Context->FastModeSwitch		= true;

		up( &Context->CapPreInjectBufferSem );
		up( &Context->CapVideoInterruptSem );
		break;              
	case  V4L2_CID_STM_CAPIF_BIG_ENDIAN:
		CheckAndSet( "BIG_ENDIAN", 0, 1, Context->CapControlBigEndian );
		break;
	case  V4L2_CID_STM_CAPIF_FULL_RANGE:
		CheckAndSet( "FULL_RANGE", 0, 1, Context->CapControlFullRange );
		break;
	case  V4L2_CID_STM_CAPIF_INCOMPLETE_FIRST_PIXEL:
		CheckAndSet( "INCOMPLETE_FIRST_PIXEL", 0, 1, Context->CapControlIncompleteFirstPixel );
		break;
	case  V4L2_CID_STM_CAPIF_ODD_PIXEL_COUNT:
		CheckAndSet( "ODD_PIXEL_COUNT", 0, 1, Context->CapControlOddPixelCount );
		break;
	case  V4L2_CID_STM_CAPIF_VSYNC_BOTTOM_HALF_LINE_ENABLE:
		CheckAndSet( "VSYNC_BOTTOM_HALF_LINE_ENABLE", 0, 1, Context->CapControlVsyncBottomHalfLineEnable );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET:
		CheckAndSet( "ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET", -260, 260, Context->CapControlActiveAreaAdjustHorizontalOffset );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_VERTICAL_OFFSET:
		CheckAndSet( "ACTIVE_AREA_ADJUST_VERTICAL_OFFSET", -256, 256, Context->CapControlActiveAreaAdjustVerticalOffset );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_WIDTH:
		CheckAndSet( "ACTIVE_AREA_ADJUST_WIDTH", -2048, 2048, Context->CapControlActiveAreaAdjustWidth );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HEIGHT:
		CheckAndSet( "ACTIVE_AREA_ADJUST_HEIGHT", -2048, 2048, Context->CapControlActiveAreaAdjustHeight );
		break;
	case  V4L2_CID_STM_CAPIF_COLOUR_MODE:
		CheckAndSet( "COLOUR_MODE", 0, 2, Context->CapControlColourMode );
		break;
	case  V4L2_CID_STM_CAPIF_VIDEO_LATENCY:
		CheckAndSet( "VIDEO_LATENCY", 0, 1000000, Context->CapControlVideoLatency );
		break;
	case  V4L2_CID_STM_CAPIF_HORIZONTAL_RESIZE_ENABLE:
		CheckAndSet( "HORIZONTAL_RESIZE_ENABLE", 0, 1, Context->CapControlEnHsRc );
		CapRecalculateScaling( Context );
		break;
	case  V4L2_CID_STM_CAPIF_VERTICAL_RESIZE_ENABLE:
		CheckAndSet( "VERTICAL_RESIZE_ENABLE", 0, 1, Context->CapControlEnVsRc );
		CapRecalculateScaling( Context );
		break;
	case  V4L2_CID_STM_CAPIF_TOP_FIELD_FIRST:
		CheckAndSet( "TOP_FIELD_FIRST", 0, 1, Context->CapControlTopFieldFirst );
		break;
	case  V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE:
		CheckAndSet( "OUTPUT_CROP_TRANSITION_MODE", 0, 1, Context->CapControlOutputCropTransitionMode );
		break;
	case  V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0:
		CheckAndSet( "OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0", 0, 0x7fffffff, Context->CapControlOutputCropTransitionModeParameter0 );
		break;
	case V4L2_CID_STM_CAPIF_PIXEL_ASPECT_RATIO_CORRECTION:
	    	CheckAndSet( "PIXEL_ASPECT_RATIO_CORRECTION", CAP_PIXEL_ASPECT_RATIO_CORRECTION_MIN_VALUE, 
	    		CAP_PIXEL_ASPECT_RATIO_CORRECTION_MAX_VALUE, Context->CapControlPixelAspectRatioCorrection );
		
	    	if( Context->DeviceContext->VideoStream == NULL ) 
	    	    return -EINVAL;
		
	    	ret = DvbStreamSetOption( Context->DeviceContext->VideoStream, PLAY_OPTION_PIXEL_ASPECT_RATIO_CORRECTION, Value );
		if( ret < 0 ) {
		    printk( "CapVideoIoctlSetControl - StreamSetOption failed (%d)\n", Value );
		    return -EINVAL;
		}
		break;

	default:
		printk( "Unknown %08x\n", Control );
		return -1;
    }

    return 0;
}

// --------------------------------

int CapVideoIoctlGetControl( 		cap_v4l2_video_handle_t	*Context,
					unsigned int		 Control,
					unsigned int		*Value )
{
printk( "VIDIOC_G_CTRL->V4L2_CID_STM_CAPIF_" );

    switch( Control )
    {
	case V4L2_CID_STM_CAPIF_RESTORE_DEFAULT:
		printk( "RESTORE_DEFAULT - Not a readable control\n" );
		*Value		= 0;
		break;
       
	case  V4L2_CID_STM_CAPIF_MIXER_INPUT:
		GetValue( "MIXER_INPUT",Context->CapControlDefaultSource );
                if (*Value == 0xf) *Value = 1;
                break;
	case  V4L2_CID_STM_CAPIF_VIDEO_INPUT:
		GetValue( "VIDEO_INPUT",Context->CapControlDefaultSource );
                if (*Value == 0x1) *Value = 1;
		break;       
	case  V4L2_CID_STM_CAPIF_BIG_ENDIAN:
		GetValue( "BIG_ENDIAN", Context->CapControlBigEndian );
		break;
	case  V4L2_CID_STM_CAPIF_FULL_RANGE:
		GetValue( "FULL_RANGE", Context->CapControlFullRange );
		break;
	case  V4L2_CID_STM_CAPIF_INCOMPLETE_FIRST_PIXEL:
		GetValue( "INCOMPLETE_FIRST_PIXEL", Context->CapControlIncompleteFirstPixel );
		break;
	case  V4L2_CID_STM_CAPIF_ODD_PIXEL_COUNT:
		GetValue( "ODD_PIXEL_COUNT", Context->CapControlOddPixelCount );
		break;
	case  V4L2_CID_STM_CAPIF_VSYNC_BOTTOM_HALF_LINE_ENABLE:
		GetValue( "VSYNC_BOTTOM_HALF_LINE_ENABLE", Context->CapControlVsyncBottomHalfLineEnable );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET:
		GetValue( "ACTIVE_AREA_ADJUST_HORIZONTAL_OFFSET", Context->CapControlActiveAreaAdjustHorizontalOffset );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_VERTICAL_OFFSET:
		GetValue( "ACTIVE_AREA_ADJUST_VERTICAL_OFFSET", Context->CapControlActiveAreaAdjustVerticalOffset );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_WIDTH:
		GetValue( "ACTIVE_AREA_ADJUST_WIDTH", Context->CapControlActiveAreaAdjustWidth );
		break;
	case  V4L2_CID_STM_CAPIF_ACTIVE_AREA_ADJUST_HEIGHT:
		GetValue( "ACTIVE_AREA_ADJUST_HEIGHT", Context->CapControlActiveAreaAdjustHeight );
		break;
	case  V4L2_CID_STM_CAPIF_COLOUR_MODE:
		GetValue( "COLOUR_MODE", Context->CapControlColourMode );
		break;
	case  V4L2_CID_STM_CAPIF_VIDEO_LATENCY:
		GetValue( "VIDEO_LATENCY", Context->CapControlVideoLatency );
		break;
	case  V4L2_CID_STM_CAPIF_HORIZONTAL_RESIZE_ENABLE:
		GetValue( "HORIZONTAL_RESIZE_ENABLE", Context->CapControlEnHsRc );
		break;
	case  V4L2_CID_STM_CAPIF_VERTICAL_RESIZE_ENABLE:
		GetValue( "VERTICAL_RESIZE_ENABLE", Context->CapControlEnVsRc );
		break;
	case  V4L2_CID_STM_CAPIF_TOP_FIELD_FIRST:
		GetValue( "TOP_FIELD_FIRST", Context->CapControlTopFieldFirst );
		break;
	case  V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE:
		GetValue( "OUTPUT_CROP_TRANSITION_MODE", Context->CapControlOutputCropTransitionMode );
		break;
	case  V4L2_CID_STM_CAPIF_OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0:
		GetValue( "OUTPUT_CROP_TRANSITION_MODE_PARAMETER_0", Context->CapControlOutputCropTransitionModeParameter0 );
		break;

	case V4L2_CID_STM_CAPIF_PIXEL_ASPECT_RATIO_CORRECTION:
	    	GetValue( "PIXEL_ASPECT_RATIO_CORRECTION", Context->CapControlPixelAspectRatioCorrection );
	    	break;

	default:
		printk( "Unknown\n" );
		return -1;
    }

    return 0;
}
