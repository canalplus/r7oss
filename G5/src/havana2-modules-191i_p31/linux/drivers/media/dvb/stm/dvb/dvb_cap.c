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

//#define NICK_TEST_HACK

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
#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/device.h>

#include "stm_v4l2.h"

#include "dvb_module.h"
#include "stmdisplayoutput.h"
#include "backend.h"
#include "dvb_video.h"
#include "osdev_device.h"
#include "ksound.h"
#include "pseudo_mixer.h"

#include "dvb_cap_video.h"

/******************************
 * Private CONSTANTS and MACROS
 ******************************/

#define CAP_ID 1
#define AVR_DEFAULT_TARGET_LATENCY  		110   // milliseconds

/*
 * ALSA mixer controls
 */
#define STM_CTRL_AUDIO_MUTE               0
#define STM_CTRL_AUDIO_SAMPLE_RATE        STM_CTRL_AUDIO_MUTE + 1
#define STM_CTRL_AUDIO_MASTER_LATENCY     STM_CTRL_AUDIO_SAMPLE_RATE + 1
#define STM_CTRL_AUDIO_HDMI_LATENCY       STM_CTRL_AUDIO_MASTER_LATENCY + 1
#define STM_CTRL_AUDIO_SPDIF_LATENCY      STM_CTRL_AUDIO_HDMI_LATENCY + 1
#define STM_CTRL_AUDIO_ANALOG_LATENCY     STM_CTRL_AUDIO_SPDIF_LATENCY + 1

#define FAIL_IF_NULL(x) do { if (!(x)) return -EINVAL; } while(0)

#define CAP_CAPTURE_OFF 0
#define CAP_CAPTURE_ON  1

/******************************
 * FUNCTION PROTOTYPES
 ******************************/

extern struct class*        player_sysfs_get_player_class(void);
extern struct class_device* player_sysfs_get_class_device(void *playback, void*stream);

/******************************
 * GLOBAL VARIABLES
 ******************************/

/*  Video Capture Devices
    * "D1-DVP0", video input taken from the DVP
    * "VIDEO0", decoded video produced by the primary video pipeline (/dev/dvb/adapter0/video0)
    * "VIDEO1", decoded video produced by the secondary video pipeline (/dev/dvb/adapter0/video1)
    * "VIDEO2", decoded video produced by the BD-J MPEG drip pipeline (/dev/dvb/adapter0/video2)
*/
struct stvin_description {
  char name[32];
  int  deviceId;
  int  virtualId;
};

//< Describes the input surfaces to user
static struct stvin_description g_vinDevice[] = {
  { "NULL", 0, 0 },
  { "D1-CAP0", 0, 0 }
//  { "VIDEO0",  0, 0 },
//  { "VIDEO1",  0, 0 },
//  { "VIDEO2",  0, 0 }
};

/******************************
 * EXTERNAL VISIBLE INTERFACES
 ******************************/

/* avr_set_external_time_mapping ()
 * This function enables the external time mapping required for AVsynchronization. In this way, the system time is used to
 * sync audio and video. This function needs to be called always (even for audion only and video only) but only once,
 * as the second value would overwrite the first one and create an indesired activity into the player.
 *
 * NOTE1: SystemTime, NativeTime and the Pts values must be max 33 bits long ->  &= 0x1ffffffffULL
 * NOTE2: all the timings values are expressed in usecs in these functions */
int cap_set_external_time_mapping (cap_v4l2_shared_handle_t *shared_context, struct StreamContext_s* stream,
				     unsigned long long nativetime, unsigned long long systemtime)
{
	int result = 0;
	struct PlaybackContext_s* playback = shared_context->cap_device_context.Playback;

	//
	// If already set then just return
	//

	if( !shared_context->update_player2_time_mapping )
		return 0;

	shared_context->update_player2_time_mapping	= false;

	//

	result  = DvbStreamSetOption (stream, PLAY_OPTION_EXTERNAL_TIME_MAPPING, 1);
	if (result < 0) {
		DVB_ERROR("PLAY_OPTION_EXTERNAL_TIME_MAPPING set failed\n");
		return -EINVAL;
	}

	//
	// Modify the system time with the specified latency,
	// this is contained within a glbal variable, I anticipate
	// that a control will be implemented to set it before
	// starting (or switching the mode of) a capture.
	//

	systemtime		+= (shared_context->target_latency * 1000);

	//

	/* compensate the mapping if audio_video_latency_offset is negative. -ve values mean that the player
	 * will make audio appear earlier which would cause the minimum system latency value to be exceeded.
	 * by adjusting the systemtime by the audio_video_latency_offset we are effectively keeping the
	 * audio latency the same and changing increasing the video latency instead.
	 *
	 * this is correct for both the high and low latency audio implementations. for high latency the
	 * offset is applied (as a -ve) on the output timer. as a result we must apply additional latency
	 * to the audio. for low latency the value is applied by the code only of the value is positive
	 * (meaning compensating here does do the right thing).
	 *
	 * TODO: we should unconditionally apply the offset, at present it is harmless to shorten the video
	 *       latency. however we intend to tune the audio latency (mostly by being less rigorous in our
	 *       handling of DTS). at this point we run the risk of overshortening the video latency so for
	 *       now we handle audio-trails-video by extending the audio latency.
	 */
	if (shared_context->audio_video_latency_offset < 0) {
#if 0
		DVB_DEBUG("Munging nativetime (%lld -> %lld)\n",
				nativetime, nativetime + audio_video_latency_offset);
#endif
		nativetime += shared_context->audio_video_latency_offset;
	}

	result = DvbPlaybackSetNativePlaybackTime(playback, nativetime, systemtime);
	if (result < 0) {
		DVB_ERROR("PlaybackSetNativePlaybackTime failed with %d\n", result);
		return -EINVAL;
	}

	return result;
}

/*
 * Invalidate the mapping we already have
 */
void cap_invalidate_external_time_mapping (cap_v4l2_shared_handle_t *shared_context)
{
	shared_context->update_player2_time_mapping	= true;
}

/*
 * Notify the capture code that the mixer settings have been changed.
 *
 * This function is registered with the pseudocard and is called whenever the mixer settings are changed.
 * In reaction we must update audio_video_latency_offset and ensure that the player's external time mapping
 * is updated to account for the new latency values.
 
static void cap_update_external_time_mapping (void *ctx, const struct snd_pseudo_mixer_settings *mixer_settings)
{
	cap_v4l2_shared_handle_t *shared_context = ctx;
	long long old_offset = shared_context->audio_video_latency_offset;

	// record the current A/V mapping
	shared_context->audio_video_latency_offset = mixer_settings->master_latency * 90; // convert ms to PES time units

	// provoke an update if required
	if (old_offset != shared_context->audio_video_latency_offset)
		shared_context->update_player2_time_mapping = true;
}
*/
/*
 * Inform everyone of the vsync offset when running without vsync locking.
 */

void cap_set_vsync_offset(cap_v4l2_shared_handle_t *shared_context, long long vsync_offset )
{
    printk( "cap_set_vsync_offset - vsync offset = %lldus\n", vsync_offset );
    printk( "\t\t\tNeed to add code to call AvrAudioSetCompensatoryLatency() when it is written\n" );
}

/******************************
 * IOCTL MANAGEMENT
 ******************************/


static int cap_ioctl_overlay_start(cap_v4l2_shared_handle_t *shared_context,
				   struct stm_v4l2_handles *handle)
{
	int ret = 0;
	playback_handle_t     playerplayback;
	struct class_device * playerclassdev;

	// Always setup a new mapping when we start a new overlay
	cap_invalidate_external_time_mapping(shared_context);

	if (shared_context->cap_device_context.Playback == NULL)
	{
		ret = DvbPlaybackCreate (&shared_context->cap_device_context.Playback);
		if (ret < 0)
		{
			DVB_ERROR("PlaybackCreate failed\n" );
			return -EINVAL;
		}
	}

	// apply any 'sticky' policies (ones that can't be changes after startup) before going any further

	/* select the system clock as master. we are timestamping incoming data using the system clock
	 * so we must ensure that the output system uses an identical clock or the input and output will
	 * drift appart.
	 */
	ret = DvbPlaybackSetOption (shared_context->cap_device_context.Playback,
			         PLAY_OPTION_MASTER_CLOCK, PLAY_OPTION_VALUE_SYSTEM_CLOCK_MASTER);
	if (ret < 0) {
		DVB_ERROR("PLAY_OPTION_VALUE_SYSTEM_CLOCK_MASTER coult not be set\n" );
		return -EINVAL;
	}

	// Wait for the playback's creational event to be acted upon
	ret = DvbPlaybackGetPlayerEnvironment (shared_context->cap_device_context.Playback,
                                            &playerplayback);
	    if (ret < 0) {
		DVB_ERROR("Cannot get internal player handle\n");
		return -EINVAL;
	}

	playerclassdev = player_sysfs_get_class_device(playerplayback, NULL);
	while (NULL == playerclassdev) {
		DVB_TRACE("Spinning waiting for creational event to be delivered\n");
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout (10);

		playerclassdev = player_sysfs_get_class_device(playerplayback, NULL);
	}

	// Video Start - if and only if a video input has been selected (handle is non-NULL)

	if (shared_context->cap_device_context.VideoCaptureStatus == CAP_CAPTURE_ON)
	{
		if (handle->v4l2type[STM_V4L2_VIDEO_INPUT].driver && handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle) 
	        {
		         ret = CapVideoIoctlOverlayStart( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle );
		         if( ret != 0 ) 
		         {
			    DVB_ERROR("DvpVideoIoctlOverlayStart failed\n" );
			    return ret;
			 }
		}
	}

	return 0;
}


static int cap_ioctl_overlay_stop(cap_v4l2_shared_handle_t *shared_context,
				  struct stm_v4l2_handles *handle)
{
	int ret = 0;

	if (shared_context->cap_device_context.Playback == NULL) {
		//DVB_DEBUG("Playback already NULL\n");
		return 0;
	}

	if(handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle) {
	    ret = CapVideoClose( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle );
	    if (ret < 0) {
		    DVB_ERROR("CapVideoClose failed \n"); // no recovery possible
		    return -EINVAL;
		}
	}

	ret = DvbPlaybackDelete(shared_context->cap_device_context.Playback);
	if (ret < 0) {
            DVB_ERROR("PlaybackDelete failed\n");
            return -EINVAL;
	}

	shared_context->cap_device_context.Playback = NULL;

	return 0;
}

/**
 * Select a the video input specified by id.
 *
 * Currently we assume we know what id was (because we only support one)
 *
 * \todo Most of this initialization ought to migrate into the video driver.
 */
static int cap_ioctl_video_set_input(cap_v4l2_shared_handle_t *shared_context,
				     struct stm_v4l2_handles *handle,
				     int id)
{
	int ret = 0;

#ifdef NICK_TEST_HACK
static dvp_v4l2_video_handle_t *video_context = NULL;

    if( video_context != NULL )
    {
	handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle = video_context;
	return 0;
    }
#else
   	cap_v4l2_video_handle_t *video_context;
#endif

   	if (handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle == NULL)
   	{
   		// allocate video handle for driver registration

   		video_context = kzalloc(sizeof(cap_v4l2_video_handle_t),GFP_KERNEL);
   		if (NULL == video_context) {
   			DVB_ERROR("Handle allocation failed\n");
   			return -ENOMEM;
   		}

   		video_context->SharedContext = shared_context;
   		video_context->DeviceContext = &shared_context->cap_device_context;
   		video_context->CapRegs = shared_context->mapped_cap_registers;
   		video_context->VtgRegs = shared_context->mapped_vtg_registers;
   		video_context->CapIrq = shared_context->cap_irq;
   		video_context->CapIrq2 = shared_context->cap_irq2;
   		video_context->CapLatency = (shared_context->target_latency * 1000);
   		CapVideoIoctlSetControl( video_context, V4L2_CID_STM_CAPIF_RESTORE_DEFAULT, 0 );

   		handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle = video_context;
   	}
   	else
   		DVB_DEBUG ("Video Handle already allocated\n");

   	// Check if playback exists

   	if (shared_context->cap_device_context.Playback == NULL) {
		DVB_DEBUG("Overlay Interface has not started yet, nothing to do.\n");
		return 0;
	}

	// Video Start - if and only if a video input has been selected (handle is non-NULL)

	if (shared_context->cap_device_context.VideoCaptureStatus == CAP_CAPTURE_ON)
	{
		if (handle->v4l2type[STM_V4L2_VIDEO_INPUT].driver && handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle) {
			ret = CapVideoIoctlOverlayStart( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle );
			if( ret != 0 ) {
				DVB_ERROR("CapVideoIoctlOverlayStart failed\n" );
				return ret;
			}
		}
	}

	// Video Stop

	if (shared_context->cap_device_context.VideoCaptureStatus == CAP_CAPTURE_OFF)
	{
		if(handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle) {
		    ret = CapVideoClose( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle );
			if (ret < 0) {
				DVB_ERROR("CapVideoClose failed \n"); // no recovery possible
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int cap_ioctl(struct stm_v4l2_handles *handle, struct stm_v4l2_driver *driver,
		     int device, enum _stm_v4l2_driver_type type, struct file *file, unsigned int cmd, void *arg)
{
    cap_v4l2_shared_handle_t *shared_context = driver->priv;
    int res;

#define CASE(x) case x: DVB_DEBUG( #x "\n" );

//    printk("%s :: cmd 0x%x\n",__FUNCTION__,cmd);
	
    switch(cmd)
    {

   	/**********************************************************/
   	/*  		VIDEO IOCTL CALLS				  			  */
	/**********************************************************/


        CASE(VIDIOC_S_FBUF)
	{
	    struct v4l2_framebuffer *argp = arg;

	    CapVideoIoctlSetFramebuffer( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle,
					 argp->fmt.width, argp->fmt.height, argp->fmt.bytesperline, argp->fmt.priv );
	    break;
	}

	CASE(VIDIOC_S_STD)
	{
	    v4l2_std_id id = *((v4l2_std_id *)arg);

	    res	= CapVideoIoctlSetStandard( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle, id );
	    if( res != 0 )
		return res;

	    break;
	}

        CASE(VIDIOC_ENUMINPUT)
	{
		struct v4l2_input * const input = arg;
		int index = input->index - driver->index_offset[device];

		if ( index < 0  || index >= ARRAY_SIZE (g_vinDevice) ){
			//DVB_ERROR("VIDIOC_ENUMINPUT: Input number (%d) out of range\n", index );
			return -EINVAL;
		}

		strcpy(input->name, g_vinDevice[index].name);

		break;
	}

	CASE(VIDIOC_S_INPUT)
	{
		int input_id = (*(int *) arg) - driver->index_offset[device];

		// Note: an invalid value is not ERROR unless the Registration Driver Interface tells so. 

		if (input_id < 0  || input_id >= ARRAY_SIZE (g_vinDevice) )
			return -EINVAL;
		
		// the only values I can get are 0 and 1 for now

		if (input_id == 0)
			shared_context->cap_device_context.VideoCaptureStatus = CAP_CAPTURE_OFF;
		else
			shared_context->cap_device_context.VideoCaptureStatus = CAP_CAPTURE_ON;

		res = cap_ioctl_video_set_input(shared_context, handle, input_id);
		if (res) return res;

    	break;
	}

	// this probably needs to call stream on when finished however for now
	// we make this to mean we will start capturing into our buffer
	CASE(VIDIOC_OVERLAY)
	{
		int *start = arg;

		if (start == NULL) return -EINVAL;
		if (!handle) return -EINVAL;

		if (*start == 1) // START
		{
			res = cap_ioctl_overlay_start(shared_context, handle);
			if (0 != res) return res;
		}
		else if (*start == 0) // STOP
		{
			res = cap_ioctl_overlay_stop(shared_context, handle);
			if (0 != res) return res;
		}
		else
		{
		    DVB_ERROR("Option not supported, I'm sorry.\n" );
		    return -EINVAL;
		}

	    break;
	}

	CASE(VIDIOC_S_CROP)
	{
		struct v4l2_crop *crop = (struct v4l2_crop*)arg;

		res = CapVideoIoctlCrop( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle, crop );
		if( res != 0 )
		    return res;

		break;
	}

   	/**********************************************************/
   	/*  		VIDEO AND AUDIO CONTROLS					  */
	/**********************************************************/


	CASE(VIDIOC_S_CTRL)
	{
	    ULONG  ctrlid 		= 0;
	    ULONG  ctrlvalue 		= 0;
	    struct v4l2_control* pctrl 	= arg;

	    ctrlid 		= pctrl->id;
	    ctrlvalue 		= pctrl->value;

	    switch (ctrlid)
	    {
/*		    
	        case V4L2_CID_STM_AUDIO_FORMAT_RECOGNIZER:
		    if ((ctrlvalue != 0) && (ctrlvalue != 1))
			 return -ERANGE;
		    AvrAudioSetFormatRecogniserEnable(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle, ctrlvalue);
	            break;

	        case V4L2_CID_STM_AUDIO_AAC_DECODE:
		    if ((ctrlvalue != 0) && (ctrlvalue != 1))
			return -ERANGE;
		    AvrAudioSetAacDecodeEnable(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle, ctrlvalue);
	            break;

	        case V4L2_CID_STM_AUDIO_MUTE: // 1 = mute on; 0 = mute off
	        {
	            int ret = 0;

	            if ((ctrlvalue != 0) && (ctrlvalue != 1)) return -ERANGE;

	            ret = AvrAudioSetEmergencyMuteReason(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle, ctrlvalue);
	            if (ret < 0) {
	        	DVB_ERROR("dvp_deploy_emergency_mute failed\n" );
			return -EINVAL;
	            }
	            break;
	        }
*/
	        default:
	        {
	            int ret = 0;

		    ret = CapVideoIoctlSetControl( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle, ctrlid, ctrlvalue );
		    if( ret < 0 )
		    {
		        DVB_ERROR("Set control invalid\n" );
	                return -EINVAL;
		    }
		}
	    }

	    break;
	}

	CASE(VIDIOC_G_CTRL)
	{
	    int ret;
	    struct v4l2_control* pctrl = arg;

	    switch (pctrl->id)
	    {
/*		    
	        case V4L2_CID_STM_AUDIO_FORMAT_RECOGNIZER:
	            pctrl->value = AvrAudioGetFormatRecogniserEnable(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle);
	            break;

	        case V4L2_CID_STM_AUDIO_AAC_DECODE:
	            pctrl->value = AvrAudioGetAacDecodeEnable(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle);
	            break;

	        case V4L2_CID_STM_AUDIO_MUTE:
	            pctrl->value = AvrAudioGetEmergencyMuteReason(handle->v4l2type[STM_V4L2_AUDIO_INPUT].handle);
	            break;
*/
	        default:
		    ret = CapVideoIoctlGetControl( handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle, pctrl->id, &pctrl->value );
		    if( ret < 0 )
		    {
		        DVB_ERROR("Get control invalid\n" );
	                return -EINVAL;
		    }
	    }

	    break;
	}

	default:
	{
	    return -ENOTTY;
	}

    } // end switch (cmd)

    return 0;
}

static int cap_close(struct stm_v4l2_handles *handle, enum _stm_v4l2_driver_type type, struct file  *file)
{
	int ret = 0;
	cap_v4l2_shared_handle_t *shared_context = NULL;

	// all the v4l2 drivers with the same handle share the same private data
	// --> fetch just one

	if (!handle->v4l2type[STM_V4L2_VIDEO_INPUT].driver) {
	}
	else {
		shared_context = handle->v4l2type[STM_V4L2_VIDEO_INPUT].driver->priv;
	}

	shared_context->cap_device_context.VideoCaptureStatus = CAP_CAPTURE_OFF;

	ret = cap_ioctl_overlay_stop(shared_context, handle);
	if (ret) {
		DVB_ERROR("cap_ioctl_overlay_stop failed\n");
		return -EINVAL;
	}

	// free the handles

	if (handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle)
		kfree(handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle);

	handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle = NULL;

	return 0;
}

// ////////////////////////////////////////////////////////////////////////////////////
//
// MMap code for ancillary buffers
//
//    This is almost entirely stolen from Pete's code in dvb_v4l2.c
//    except for the range check. Since my buffers are likely to be
//    only 32/64 bytes long, I will only allow the mapping of all of them
//    in one lump.
//

static struct page* cap_vm_nopage(struct vm_area_struct *vma, unsigned long vaddr, int *type)
{

// I hope this code isn't used, the result certainly isn't useful!

struct page	*page;
/*
void		*page_addr;
unsigned long	 page_frame;
*/
    if (vaddr > vma->vm_end)
	return NOPAGE_SIGBUS;

    /*
     * Note that this assumes an identity mapping between the page offset and
     * the pfn of the physical address to be mapped. This will get more complex
     * when the 32bit SH4 address space becomes available.
     */
/*
    page_addr	= (void*)((vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT) +
			  ((cap_v4l2_video_handle_t *)(vma->vm_private_data))->AncillaryBufferPhysicalAddress);
    page_frame 	= ((unsigned long)page_addr >> PAGE_SHIFT);

    if(!pfn_valid(page_frame))
	return NOPAGE_SIGBUS;

    page = virt_to_page(__va(page_addr));

    get_page(page);

    if (type)
	*type = VM_FAULT_MINOR;
*/
    return page;
}

static void cap_vm_open(struct vm_area_struct *vma)
{
	//DVB_DEBUG("/n");
}

static void cap_vm_close(struct vm_area_struct *vma)
{
	//DVB_DEBUG("/n");
}

static struct vm_operations_struct cap_vm_ops_memory =
{
	.open     = cap_vm_open,
	.close    = cap_vm_close,
	.nopage   = cap_vm_nopage,
};

static int cap_mmap(struct stm_v4l2_handles *handle, enum _stm_v4l2_driver_type type, struct file *file, struct vm_area_struct*  vma)
{
/*
unsigned int		 i;
unsigned int		 ValidMapSize;
cap_v4l2_video_handle_t	*VideoContext = (cap_v4l2_video_handle_t *)handle->v4l2type[STM_V4L2_VIDEO_INPUT].handle;
*/
//

    if (!(vma->vm_flags & VM_WRITE))
	return -EINVAL;
/*
    ValidMapSize	= (VideoContext->AncillaryBufferCount * VideoContext->AncillaryBufferSize) + (1 << PAGE_SHIFT) - 1;
    ValidMapSize	= ValidMapSize & (~((1 << PAGE_SHIFT) - 1));

    if( (vma->vm_pgoff != 0) || ((vma->vm_end - vma->vm_start) != ValidMapSize) )
    {
	printk( "cap_mmap - Only whole buffer memory can be mapped - Invalid address range should be Offset = 0, length = Count * Size(%d*%d)[%08lx %04lx]\n", VideoContext->AncillaryBufferCount, VideoContext->AncillaryBufferSize, vma->vm_pgoff, (vma->vm_end - vma->vm_start) );
	return -EINVAL;
    }

    vma->vm_flags       |= VM_DONTEXPAND | VM_LOCKED;
    vma->vm_page_prot    = pgprot_noncached(vma->vm_page_prot);
    vma->vm_private_data = VideoContext;
    vma->vm_ops 	 = &cap_vm_ops_memory;

//

    for( i=0; i<VideoContext->AncillaryBufferCount; i++ )
	VideoContext->AncillaryBufferState[i].Mapped	= true;

//
*/
    return 0;
}

//
// End of MMap code for ancillary buffers
//
// ////////////////////////////////////////////////////////////////////////////////////

static struct stm_v4l2_driver cap_video_overlay = {
	.type          = STM_V4L2_VIDEO_INPUT,
	.control_range = {
		{ V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST },
		{ V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST },
		{ V4L2_CID_STM_CAP_VIDEO_FIRST, V4L2_CID_STM_CAP_VIDEO_LAST } },
	.n_indexes     = { ARRAY_SIZE (g_vinDevice), ARRAY_SIZE (g_vinDevice), ARRAY_SIZE (g_vinDevice) },
	.ioctl         = cap_ioctl,
	.close         = cap_close,
	.poll          = NULL,
	.mmap          = cap_mmap,
	.name          = "Compositor Capture Device",
};

/* Platform procedures */

static cap_v4l2_shared_handle_t *cap_v4l2_shared_context_new(void)
{
	cap_v4l2_shared_handle_t *ctx = kzalloc(sizeof(cap_v4l2_shared_handle_t), GFP_KERNEL);

	if (!ctx)
		return NULL;

	// Initialize cap_device_context
	ctx->cap_device_context.Id                       = CAP_ID;
	ctx->cap_device_context.DemuxContext             = &ctx->cap_device_context;        /* wire directly to own demux by default */
	ctx->cap_device_context.Playback                 = NULL;
	ctx->cap_device_context.StreamType               = STREAM_TYPE_TRANSPORT;
	ctx->cap_device_context.DvbContext               = NULL; // DvbContext;
	ctx->cap_device_context.DemuxStream              = NULL;
	ctx->cap_device_context.VideoStream              = NULL;
	ctx->cap_device_context.AudioStream              = NULL;

	ctx->cap_device_context.VideoState.video_blank             = 0;
	ctx->cap_device_context.VideoState.play_state              = VIDEO_STOPPED;
	ctx->cap_device_context.VideoState.stream_source           = VIDEO_SOURCE_MEMORY;
	ctx->cap_device_context.VideoState.video_format            = VIDEO_FORMAT_16_9;
	ctx->cap_device_context.VideoState.display_format          = VIDEO_FULL_SCREEN;
	ctx->cap_device_context.VideoSize.w                        = 0;
	ctx->cap_device_context.VideoSize.h                        = 0;
	ctx->cap_device_context.VideoSize.aspect_ratio             = 0;
	ctx->cap_device_context.FrameRate                          = 0;
	ctx->cap_device_context.PlaySpeed                          = DVB_SPEED_NORMAL_PLAY;
	ctx->cap_device_context.VideoId                            = DEMUX_INVALID_ID;     /* CodeToInteger('v','i','d','s'); */
	ctx->cap_device_context.VideoEncoding                      = VIDEO_ENCODING_AUTO;
	ctx->cap_device_context.VideoEvents.Write                  = 0;
	ctx->cap_device_context.VideoEvents.Read                   = 0;
	ctx->cap_device_context.VideoEvents.Overflow               = 0;
	ctx->cap_device_context.VideoCaptureStatus                 = CAP_CAPTURE_OFF;

	// Is this needed Julian W????
	init_waitqueue_head (&ctx->cap_device_context.VideoEvents.WaitQueue);
	// End is this needed

	mutex_init   (&(ctx->cap_device_context.VideoWriteLock));

	ctx->target_latency		= AVR_DEFAULT_TARGET_LATENCY;
	ctx->audio_video_latency_offset	= 0;

	return ctx;
}

static int cap_probe(struct device *dev)
{
	unsigned int base, size;
	struct platform_device *cap_device_data = to_platform_device(dev);
	cap_v4l2_shared_handle_t *shared_context;

	if (!cap_device_data || !cap_device_data->name) {
		DVB_ERROR("Device probe failed.  Check your kernel SoC config!!\n" );
		return -ENODEV;
	}

	shared_context = cap_v4l2_shared_context_new();
	if (!shared_context) {
		DVB_ERROR("Cannot allocate memory for shared Capture context\n");
		return -ENOMEM;
	}

	// all the v4l2 drivers registered below share the same private data
	cap_video_overlay.priv = shared_context;

	stm_v4l2_register_driver(&cap_video_overlay);

	// discover the platform resources and record this in the shared context
	base = platform_get_resource(cap_device_data, IORESOURCE_MEM, 0)->start;
	size = (((unsigned int)(platform_get_resource(cap_device_data, IORESOURCE_MEM, 0)->end)) - base) + 1;
//	shared_context->cap_irq = platform_get_resource(cap_device_data, IORESOURCE_IRQ, 0)->start;
//	shared_context->cap_irq2 = platform_get_resource(cap_device_data, IORESOURCE_IRQ, 0)->end;
	shared_context->mapped_cap_registers = ioremap(base, size);
	shared_context->mapped_vtg_registers = ioremap(0xfe030200, 32*1024);

	DVB_DEBUG("Compositor Capture Settings initialised\n" );

	return 0;
}

static int cap_remove(struct device *dev)
{
	DVB_ERROR("Removal of CAP driver is not yet supported\n");

	//kfree(shared_context);

	return -EINVAL;
}

static struct device_driver cap_driver = {
	.name = "cap",
	.bus = &platform_bus_type,
	.probe = cap_probe,
	.remove = cap_remove,
};

__init int cap_init(void)
{
	return driver_register(&cap_driver);
}

#if 0
static void cap_cleanup(void)
{
	driver_unregister(&cap_driver);
}
#endif
