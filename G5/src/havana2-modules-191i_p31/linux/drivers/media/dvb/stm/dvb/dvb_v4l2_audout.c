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
 * V4L2 audio output device driver for ST SoC display subsystems.
************************************************************************/

#include <linux/poll.h>
#include "stm_v4l2.h"

#include "dvb_module.h"
#include "dvb_v4l2_export.h"

/* 
 * AUDIO OUTPUT Devices:
 */
struct staout_description {
    char name[32];
    int audioId; // id associated with audio description
    int  deviceId;
    int  virtualId;
};

//< Describes the audio output surfaces to user
static struct staout_description g_aoutDevice[] = {
	{ "MIXER0_PRIMARY",    0, 0, 0},  //<! "MIXER0_PRIMARY" audio, primary audio mixer input
	{ "MIXER0_SECONDARY",  0, 0, 0},  //<! "MIXER0_SECONDARY" audio, secondary audio mixer input
	{ "MIXER1_PRIMARY",    0, 0, 0},  //<! "MIXER1_PRIMARY", audio, physically independent audio device used for second room audio output
};


static int dvb_v4l2_audout_ioctl(struct stm_v4l2_handles *handle, struct stm_v4l2_driver *driver,
		     int device, enum _stm_v4l2_driver_type type, struct file *file, unsigned int cmd, void *arg)
{
    switch(cmd)
    {
	case VIDIOC_ENUMAUDOUT:
	{
	    struct v4l2_audioout * const audioout = arg;
	    int index = audioout->index - driver->index_offset[device];

	    if (index < 0 || index >= ARRAY_SIZE (g_aoutDevice)) {
		DVB_ERROR("VIDIOC_ENUMAUDOUT: Output number out of range %d\n", index);
		return -EINVAL;
	    }

	    strcpy(audioout->name, g_aoutDevice[index].name);

	    break;
	}

	case VIDIOC_S_AUDOUT:
	{
	    const struct v4l2_audioout * const audioout = arg;
	    int index = audioout->index - driver->index_offset[device];

	    if (index < 0 || index >= ARRAY_SIZE (g_aoutDevice)) {
		DVB_ERROR("VIDIOC_S_AUDOUT: Output number out of range %d\n", index);
		return -EINVAL;
	    }

	    // allocate handle for driver registration

	    handle->v4l2type[STM_V4L2_AUDIO_OUTPUT].handle = kmalloc(sizeof(struct v4l2_audioout),GFP_KERNEL);
	    if (handle->v4l2type[STM_V4L2_AUDIO_OUTPUT].handle == NULL) {
		DVB_ERROR("VIDIOC_S_AUDOUT: kmalloc failed\n" );
		return -EINVAL;
	    }

	    g_aoutDevice[index].audioId = index;

	    break;
	}

	default:
	{
	    return -ENOTTY;
	}
    }

    return 0;
}

/* NOTE: Platform procedures to be added if necessary */

static int dvb_v4l2_audout_close(struct stm_v4l2_handles *handle, enum _stm_v4l2_driver_type type, struct file  *file)
{
    if (handle->v4l2type[STM_V4L2_AUDIO_OUTPUT].handle)
	kfree(handle->v4l2type[STM_V4L2_AUDIO_OUTPUT].handle);

    handle->v4l2type[STM_V4L2_AUDIO_OUTPUT].handle = NULL;

    return 0;
}

static struct stm_v4l2_driver dvb_v4l2_audout_driver = {
	.name           = "dvb_v4l2_audout",
	.type           = STM_V4L2_AUDIO_OUTPUT,
	.control_range  = {{ V4L2_CID_STM_AUDIO_O_FIRST, V4L2_CID_STM_AUDIO_O_LAST },
			   { V4L2_CID_STM_AUDIO_O_FIRST, V4L2_CID_STM_AUDIO_O_LAST },
			   { V4L2_CID_STM_AUDIO_O_FIRST, V4L2_CID_STM_AUDIO_O_LAST } },
	.n_indexes      = { ARRAY_SIZE (g_aoutDevice), ARRAY_SIZE (g_aoutDevice), ARRAY_SIZE (g_aoutDevice) },
	.ioctl          = dvb_v4l2_audout_ioctl,
	.close          = dvb_v4l2_audout_close,
	.poll           = NULL,
	.mmap           = NULL,
};

static int __init dvb_v4l2_audout_init(void)
{
    return stm_v4l2_register_driver(&dvb_v4l2_audout_driver);
}

static void __exit dvb_v4l2_audout_exit (void)
{
    stm_v4l2_unregister_driver (&dvb_v4l2_audout_driver);
}

//module_init (dvb_v4l2_audout_init);
//module_exit (dvb_v4l2_audout_exit);

//MODULE_LICENSE ("GPL");

