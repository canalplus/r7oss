/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

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

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#include <stm_memsrc.h>
#include <stm_memsink.h>

#include "player_version.h"
#include "report.h"
#include "stm_se.h"

#include "osdev_mem.h"

#ifndef __init
#define __init
#endif
#ifndef __exit
#define __exit
#endif

static int  __init              EncoderTestLoadModule(void);
static void __exit              EncoderTestUnloadModule(void);

module_init(EncoderTestLoadModule);
module_exit(EncoderTestUnloadModule);

MODULE_DESCRIPTION("Encode STKPI DBGFS test Module");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

//#define VERBOSE
#ifdef VERBOSE
#define VERBOSE_PRINTK(fmt, args...) pr_info(fmt, ## args)
#else
#define VERBOSE_PRINTK(fmt, args...)
#endif

#define FRAME_SIZE_X 1920
#define FRAME_SIZE_Y 1088
#define BYTE_PER_PIXEL 2
#define BPA_PARTITION_NAME "BPA2_Region0"

struct dentry *d_memsrcsink;

struct dentry *d_in_size;
struct dentry *d_out_size;

struct dentry *d_in_fill;
struct dentry *d_out_fill;

struct dentry *d_in;
struct dentry *d_out_videoGrab;
struct dentry *d_out_audioGrab;

struct dentry *d_pull;
struct dentry *d_push;


#define COMPRESSED_BUFFER_SIZE (1920*1088)
#define ENCODE_CONTROL_SIZE (64)
struct pp_descriptor
{
    unsigned int   m_BufferLen;
    stm_object_h obj;
    stm_object_h objSource;
    //output buffer
    unsigned char m_Buffer[COMPRESSED_BUFFER_SIZE];
    unsigned int   m_availableLen;
    bool           m_metadata_read;
    bool           m_frame_to_read;
    // control
    unsigned int   m_control[ENCODE_CONTROL_SIZE];
};

//To handle input buffer
void *g_virtual_address;
unsigned long g_physical_address;

struct pp_descriptor g_theSinkVideo   = {COMPRESSED_BUFFER_SIZE, 0 };
struct pp_descriptor g_theSinkAudio   = {COMPRESSED_BUFFER_SIZE, 0 };


// Some locals
struct EncoderTestCtx
{
    stm_se_encode_h     encode;
    stm_se_encode_stream_h  audio_stream;
    stm_se_encode_stream_h  video_stream;
} context;

#ifndef CONFIG_DEBUG_FS
#warn "file unexpectedly compiled with no debugfs support"
#endif

/* Reading metatata will perform a memsink pull that will return the metadata + the frame
   The metadata will be stored into m_Buffer  waiting for a frame_read call
*/
static ssize_t metadata_read(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos)
{
    struct pp_descriptor *pp_desc = file->private_data;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    int nbpull = 0;
    int retval;

    if (pp_desc->m_metadata_read == true)
    {
        VERBOSE_PRINTK("metadata_read nothing more\n");
        return 0;
    }

    VERBOSE_PRINTK("metadata_read obj (%x) pull buf=%p count=%d, ppos=%d\n", (unsigned int)pp_desc->obj, (void *)buf, (int)count, (int)*ppos);
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)pp_desc->m_Buffer;
    p_memsink_pull_buffer->virtual_address = &pp_desc->m_Buffer      [sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length   =  pp_desc->m_BufferLen  - sizeof(stm_se_capture_buffer_t);
    retval = stm_memsink_pull_data(pp_desc->obj, p_memsink_pull_buffer, p_memsink_pull_buffer->buffer_length, &nbpull);

    if (retval < 0)
    {
        return retval;
    }

    nbpull = sizeof(stm_se_compressed_frame_metadata_t);

    if (copy_to_user(buf, &p_memsink_pull_buffer->u.compressed, nbpull))
    {
        return -EFAULT;
    }

    *ppos += nbpull;
    /* no more metadata to read */
    pp_desc->m_metadata_read = true;
    pp_desc->m_frame_to_read = true;
    return nbpull;
}

/*
    Writing metadata will put metadata into m_Buffer and will wait for the frame (frame_write) to
    perform encode injection
    Here we can only write uncompressed audio/videa frame
*/
static ssize_t metadata_write(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct pp_descriptor *pp_desc = file->private_data;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    ssize_t nbwrite;
    VERBOSE_PRINTK("metadata_write buf=%p count=%d, ppos=%lld pp_desc=%p\n", (void *)buf, (int)count, (long long int)*ppos, (void *)pp_desc);
    nbwrite = count;
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)pp_desc->m_Buffer;
    p_memsink_pull_buffer->virtual_address = &pp_desc->m_Buffer      [sizeof(stm_se_capture_buffer_t)];
    p_memsink_pull_buffer->buffer_length   =  pp_desc->m_BufferLen  - sizeof(stm_se_capture_buffer_t);

    if (copy_from_user(&p_memsink_pull_buffer->u.uncompressed, buf, nbwrite))
    {
        return -EFAULT;
    }

    return nbwrite;
}



/* Reading frame simply copy frame stored in the m_Buffer to the user
*/
static ssize_t frame_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct pp_descriptor *pp_desc = file->private_data;
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    int nbpull = 0;

    if (pp_desc->m_frame_to_read == false)
    {
        VERBOSE_PRINTK("frame_read nothing more\n");
        return 0;
    }

    VERBOSE_PRINTK("frame_read obj (%x) pull buf=%p count=%d, ppos=%d\n", (int)pp_desc->obj, (void *)buf, (int)count, (int)*ppos);
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *) pp_desc->m_Buffer;
    nbpull = p_memsink_pull_buffer->payload_length;

    if (copy_to_user(buf, p_memsink_pull_buffer->virtual_address, nbpull))
    {
        return -EFAULT;
    }

    *ppos += nbpull;
    /* metadata read possible */
    pp_desc->m_metadata_read = false;
    pp_desc->m_frame_to_read = false;
    return nbpull;
}

/*
    Writing frame will trigger a encode_injection
    - uncompressed metadata are copied from m_bffer
    - frame is temporarily copied in the m_Buffer
*/
static ssize_t frame_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos)
{
    stm_se_capture_buffer_t *p_memsink_pull_buffer;
    int result = 0;
    struct pp_descriptor *pp_desc = file->private_data;
    ssize_t nbwrite;
    VERBOSE_PRINTK("frame_write buf=%p count=%d, ppos=%lld pp_desc=%p\n", buf, count, *ppos, pp_desc);
    nbwrite = count;
    p_memsink_pull_buffer = (stm_se_capture_buffer_t *)pp_desc->m_Buffer;

    //if(copy_from_user(p_memsink_pull_buffer->virtual_address, buf, nbwrite)){
    //copy input buffer: from user space address 'buf' to bpa2 kernel address 'g_virtual_address'
    if (copy_from_user(g_virtual_address, buf, nbwrite))
    {
        return -EFAULT;
    }

    result = stm_se_encode_stream_inject_frame(pp_desc->objSource, g_virtual_address, g_physical_address, count, p_memsink_pull_buffer->u.uncompressed);

    if (result < 0)
    {
        pr_err("Error: %s Failed to send buffer\n", __func__);
    }

    return nbwrite;
}


/*
    Writing control to a unsigned int array
    Here we can only write audio/video control
*/
static ssize_t control_write(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos)
{
    struct pp_descriptor *pp_desc = file->private_data;
    ssize_t nbwrite;
    unsigned int i, j;
    stm_se_ctrl_t ctrl;
    int           value;
    int           result = 0;
    void          *comp_value;
    bool          bcompound_controlToDo;
    stm_se_picture_resolution_t    res;
    stm_se_framerate_t             fps;
    stm_se_video_multi_slice_t     slice;
    stm_se_audio_bitrate_control_t brc;
    stm_se_audio_core_format_t     core;
    VERBOSE_PRINTK("control_write buf=%p count=%d, ppos=%lld pp_desc=%p\n", (void *)buf, (int)count, (long long int)*ppos, (void *)pp_desc);
    nbwrite = (count / sizeof(unsigned int)); //unsigned int

    if (nbwrite >= ENCODE_CONTROL_SIZE)
    {
        return -EFAULT;
    }

    if (copy_from_user(&pp_desc->m_control[0], buf, count))
    {
        return -EFAULT;
    }

    // process controls
    i = 0;
    VERBOSE_PRINTK("%s: Writing %d controls\n", __func__, nbwrite);

    while (i < (nbwrite - 1))
    {
        ctrl  = (stm_se_ctrl_t) pp_desc->m_control[i];
        value = pp_desc->m_control[++i];
        bcompound_controlToDo = false;

        switch (ctrl)
        {
        case STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE:
        case STM_SE_CTRL_ENCODE_STREAM_BITRATE:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE:
        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO:
        case STM_SE_CTRL_AUDIO_ENCODE_STREAM_SCMS:
        case STM_SE_CTRL_AUDIO_ENCODE_CRC_ENABLE:
            result = stm_se_encode_stream_set_control(pp_desc->objSource, ctrl, value);

            if (result < 0)
            {
                pr_err("Error: %s Failed to write control %d\n", __func__, ctrl);
                return -EFAULT;
            }

            ++i;
            break;

        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION:
            if ((nbwrite - i) < (sizeof(stm_se_picture_resolution_t) / sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            if (i >= (nbwrite - 1))
            {
                break;
            }

            res.width  = value;
            res.height = pp_desc->m_control[++i];
            comp_value = (void *)&res;
            bcompound_controlToDo = true;
            ++i;
            break;

        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE:
            if ((nbwrite - i) < (sizeof(stm_se_framerate_t) / sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            if (i >= (nbwrite - 1))
            {
                break;
            }

            fps.framerate_num  = value;
            fps.framerate_den = pp_desc->m_control[++i];
            comp_value = (void *)&fps;
            bcompound_controlToDo = true;
            ++i;
            break;

        case STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE:
            if ((nbwrite - i) < (sizeof(stm_se_video_multi_slice_t) / sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            if (i >= (nbwrite - 2))
            {
                break;
            }

            slice.control = value;
            slice.slice_max_byte_size = pp_desc->m_control[++i];
            slice.slice_max_mb_size   = pp_desc->m_control[++i];
            comp_value = (void *)&slice;
            bcompound_controlToDo = true;
            ++i;
            break;

        case STM_SE_CTRL_AUDIO_ENCODE_STREAM_BITRATE_CONTROL:
            if ((nbwrite - i) < (sizeof(stm_se_audio_bitrate_control_t) / sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            if (i >= (nbwrite - 3))
            {
                break;
            }

            brc.is_vbr = value;
            brc.bitrate = pp_desc->m_control[++i];
            brc.vbr_quality_factor = pp_desc->m_control[++i];
            brc.bitrate_cap = pp_desc->m_control[++i];
            comp_value = (void *)&brc;
            bcompound_controlToDo = true;
            ++i;
            break;

        case STM_SE_CTRL_AUDIO_ENCODE_STREAM_CORE_FORMAT:
            if ((nbwrite - i) < (sizeof(stm_se_audio_core_format_t) / sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            if (i >= (nbwrite - 1))
            {
                break;
            }

            core.sample_rate = value;
            core.channel_placement.channel_count = pp_desc->m_control[++i];

            if (core.channel_placement.channel_count > STM_SE_MAX_NUMBER_OF_AUDIO_CHAN)
            {
                pr_err("Error: %s invalid channel count %d\n", __func__, core.channel_placement.channel_count);
                return -EINVAL;
            }
            if ((core.channel_placement.channel_count > nbwrite) ||
                (i >= (nbwrite - core.channel_placement.channel_count)))
            {
                break;
            }

            for (j = 0; j < core.channel_placement.channel_count; j++)
            {
                core.channel_placement.chan[j] = pp_desc->m_control[++i];
            }

            comp_value = (void *)&core;
            bcompound_controlToDo = true;
            ++i;
            break;

        default:
            pr_err("Error: %s Failed to write control %d\n", __func__, ctrl);
            return -EFAULT;
        }

        if (bcompound_controlToDo)
        {
            result = stm_se_encode_stream_set_compound_control(pp_desc->objSource, ctrl, comp_value);

            if (result < 0)
            {
                pr_err("Error: %s Failed to write compound control %d\n", __func__, ctrl);
                return -EFAULT;
            }
        }
    }

    return nbwrite;
}


static int pp_open(struct inode *inode, struct file *file)
{
    /* struct pp_descriptor *pp_desc; */

    if (inode->i_private)
    {
        file->private_data = inode->i_private;
        /* pp_desc = file->private_data; */
        return 0;
    }
    else
    {
        return -EFAULT;
    }
}


const struct file_operations metadata_ops =
{
    .write = metadata_write,
    .read = metadata_read,
    .open = pp_open
};

const struct file_operations frame_ops =
{
    .write = frame_write,
    .read = frame_read,
    .open = pp_open
};

const struct file_operations control_ops =
{
    .write = control_write,
    .open = pp_open
};


int  memsrcsink_create_dbgfs(void)
{
    VERBOSE_PRINTK("In memsrcsink_create_dbgfs\n"
                   "===========================\n");
    /* allocate input buffer */
    g_virtual_address = OSDEV_AllignedMallocHwBuffer(256, FRAME_SIZE_X * FRAME_SIZE_Y * BYTE_PER_PIXEL, BPA_PARTITION_NAME, &g_physical_address);

    if (g_virtual_address == 0)
    {
        pr_err("Error creating %s\n", BPA_PARTITION_NAME);
        return -1 ;
    }

    VERBOSE_PRINTK("allocate input Buffer\n");
    VERBOSE_PRINTK("virtual addr = 0x%x phys addr = 0x%x\n", (unsigned int)g_virtual_address, (unsigned int)g_physical_address);
    /* CREATE DBUGEFS INTERFACE */
    /* cd /sys/kernel/debug/Encoder  */
    d_memsrcsink = debugfs_create_dir("Encoder", NULL);

    if (IS_ERR(d_memsrcsink))
    {
        pr_err("debugfs_create_dir error 0x%x\n", (unsigned int)d_memsrcsink);
        return -1 ;
    }

    // Video part
    VERBOSE_PRINTK("Create Video compressed_metadata file\n"); /* read only*/
    d_pull = debugfs_create_file("video_compressed_metadata", 0444, d_memsrcsink, &g_theSinkVideo, &metadata_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating video compressed_metadata\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Video uncompressed_frame_metadata file\n");/* write only*/
    d_pull = debugfs_create_file("video_uncompressed_metadata", 0222, d_memsrcsink, &g_theSinkVideo, &metadata_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating video uncompressed_metadata\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Video compressed_frame file\n");/* read only*/
    d_pull = debugfs_create_file("video_compressed_frame", 0444, d_memsrcsink, &g_theSinkVideo, &frame_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating video compressed_frame\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Video uncompressed_frame file\n");/* write only*/
    d_pull = debugfs_create_file("video_uncompressed_frame", 0222, d_memsrcsink, &g_theSinkVideo, &frame_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating video uncompressed_frame\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Video control file\n");/* write only*/
    d_pull = debugfs_create_file("video_controls", 0222, d_memsrcsink, &g_theSinkVideo, &control_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating video control file\n");
        return -1 ;
    }

    // Audio part
    VERBOSE_PRINTK("Create Audio compressed_metadata file\n"); /* read only*/
    d_pull = debugfs_create_file("audio_compressed_metadata", 0444, d_memsrcsink, &g_theSinkAudio, &metadata_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating audio compressed_metadata\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Audio uncompressed_metadata file\n");/* write only*/
    d_pull = debugfs_create_file("audio_uncompressed_metadata", 0222, d_memsrcsink, &g_theSinkAudio, &metadata_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating audio uncompressed_metadata\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Audio compressed_frame file\n");/* read only*/
    d_pull = debugfs_create_file("audio_compressed_frame", 0444, d_memsrcsink, &g_theSinkAudio, &frame_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating audio compressed_frame\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Audio uncompressed_frame file\n");/* write only*/
    d_pull = debugfs_create_file("audio_uncompressed_frame", 0222, d_memsrcsink, &g_theSinkAudio, &frame_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating audio uncompressed_frame\n");
        return -1 ;
    }

    VERBOSE_PRINTK("Create Audio control file\n");/* write only*/
    d_pull = debugfs_create_file("audio_controls", 0222, d_memsrcsink, &g_theSinkVideo, &control_ops);

    if (IS_ERR(d_pull))
    {
        pr_err("error creating audio control file\n");
        return -1 ;
    }

    return 0;
}

void memsrcsink_delete_dbgfs(void)
{
    /*clean up*/
    if (d_memsrcsink)
    {
        debugfs_remove_recursive(d_memsrcsink);
    }

    d_memsrcsink = NULL;

    if (g_virtual_address)
    {
        OSDEV_AllignedFreeHwBuffer((void *)g_virtual_address, BPA_PARTITION_NAME);
    }

    g_virtual_address = 0;
}

static int __init EncoderTestLoadModule(void)
{
    int result = 0;
    context = (struct EncoderTestCtx)
    {
        0
    }; // Zero down the context struct
    /* stm_se_init gets called when Player2 Module is loaded */
    VERBOSE_PRINTK("%s: Encoder Test Module Loaded\n", __func__);

    /* create dbgfs env */
    if (memsrcsink_create_dbgfs() == -1)
    {
        return -1;
    }

    result = stm_se_encode_new("Encode0", &context.encode);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new Encode\n", __func__);
    }

    result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264, &context.video_stream);

    //DG: force null coder at first
    //result = stm_se_encode_stream_new("EncodeStream0", context.encode, STM_SE_ENCODE_STREAM_ENCODING_VIDEO_NULL, &context.video_stream);
    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264 EncodeStream\n", __func__);
    }

    result = stm_se_encode_stream_new("EncodeStream1", context.encode, STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC, &context.audio_stream);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC EncodeStream\n", __func__);
    }

//
// Create memsink object for both audio and video
//
    g_theSinkVideo.objSource = context.video_stream;
    result = stm_memsink_new("EncoderVideoSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&g_theSinkVideo.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink for video encoded frame grab\n", __func__);
        return -1;
    }

    g_theSinkAudio.objSource = context.audio_stream;
    result = stm_memsink_new("EncoderAudioSink", STM_IOMODE_BLOCKING_IO, KERNEL, (stm_memsink_h *)&g_theSinkAudio.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to create a new memsink for audio encoded frame grab\n", __func__);
        return -1;
    }

//
// try to attach
//
    VERBOSE_PRINTK("Video Attach src_obj=%p, snk_obj=%p\n", context.video_stream, g_theSinkVideo.obj);
    result = stm_se_encode_stream_attach(context.video_stream, g_theSinkVideo.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for video\n", __func__);
    }

    VERBOSE_PRINTK("Audio Attach src_obj=%p, snk_obj=%p\n", context.audio_stream, g_theSinkAudio.obj);
    result = stm_se_encode_stream_attach(context.audio_stream, g_theSinkAudio.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for audio\n", __func__);
    }

    return 0;
}

static void __exit EncoderTestUnloadModule(void)
{
    int result = 0;
    /* stm_se_term gets called when Player2 Module is unloaded */
    VERBOSE_PRINTK("%s: Unloading..\n", __func__);
//
// detach from memsink
//
    pr_info("Video detach src_obj=%p, snk_obj=%p\n", context.video_stream, g_theSinkVideo.obj);
    result = stm_se_encode_stream_detach(context.video_stream, g_theSinkVideo.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for audio\n", __func__);
    }

    VERBOSE_PRINTK("Audio detach src_obj=%p, snk_obj=%p\n", context.audio_stream, g_theSinkAudio.obj);
    result = stm_se_encode_stream_detach(context.audio_stream, g_theSinkAudio.obj);

    if (result < 0)
    {
        pr_err("Error: %s Failed to attach encode stream to memsink for audio\n", __func__);
    }

//
// Release memsink object
//
    if (g_theSinkVideo.obj)
        if (stm_memsink_delete((stm_memsink_h)g_theSinkVideo.obj) < 0)
        {
            pr_err("Error: %s Failed to delete memsink video object\n", __func__);
        }

    if (g_theSinkAudio.obj)
        if (stm_memsink_delete((stm_memsink_h)g_theSinkAudio.obj) < 0)
        {
            pr_err("Error: %s Failed to delete memsink video object\n", __func__);
        }

    if (context.audio_stream)
    {
        result = stm_se_encode_stream_delete(context.audio_stream);

        if (result < 0)
        {
            pr_err("Error: %s (audio_stream) stm_se_encode_stream_delete returned %d\n", __func__, result);
        }
    }

    if (context.video_stream)
    {
        result = stm_se_encode_stream_delete(context.video_stream);

        if (result < 0)
        {
            pr_err("Error: %s (video_stream) stm_se_encode_stream_delete returned %d\n", __func__, result);
        }
    }

    if (context.encode)
    {
        result = stm_se_encode_delete(context.encode);

        if (result < 0)
        {
            pr_err("Error: %s stm_se_encode_delete returned %d\n", __func__, result);
        }
    }

    /* remove dbgfs env */
    memsrcsink_delete_dbgfs();
    VERBOSE_PRINTK("%s: Encoder Test Module unloaded\n", __func__);
}
