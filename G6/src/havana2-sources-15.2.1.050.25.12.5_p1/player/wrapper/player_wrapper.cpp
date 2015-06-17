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

// Standard C headers and substitutions
#include <stdarg.h>
extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

#include "osinline.h"
#include <stm_data_interface.h>
#include <stm_se.h>
#include "st_relayfs_se.h"
#include "player.h"
#include "player_policy.h"
#include "player_sysfs.h"
#include "havana_player.h"
#include "player_factory.h"
#include "havana_playback.h"
#include "havana_stream.h"
#include "havana_user_data.h"
#include "mixer_mme.h"
#include "codec_capabilities.h"
#include "coder_capabilities.h"
#include "manifestor_sourceGrab.h"
#include "manifestor_encode.h"

#include "encoder_generic.h"
#include "audio_reader.h"
#include "audio_generator.h"
#include "fatal_error.h"

#include "stm_pixel_capture.h"
#include "capture_device_priv.h"
#include "uncompressed.h"
#include "message.h"

#undef TRACE_TAG
#define TRACE_TAG   "Player"

//
// This is a lookup function to convert the error reporting into
// the negative errno form used by most Unix-like kernels.
//
// This code is called with both variable and constant arguments.
// Calls with constant arguments should be regarded as documentation.
// We make this function inline so the compiler can directly replace
// the constants.
//
static inline int HavanaStatusToErrno(HavanaStatus_t Result)
{
    switch (Result)
    {
    case HavanaNoError:
        return 0;

    case HavanaNotOpen:
    case HavanaNoDevice:
    case HavanaNoFactory:
        return -ENODEV;

    case HavanaNoMemory:
    case HavanaTooManyPlaybacks:
    case HavanaTooManyStreams:
    case HavanaTooManyDisplays:
    case HavanaTooManyMixers:
    case HavanaTooManyGenerators:
        return -ENOMEM;

    case HavanaBusy:
        return -EAGAIN;

    case HavanaError:
    case HavanaPlaybackInvalid:
    case HavanaStreamInvalid:
    case HavanaComponentInvalid:
    case HavanaDisplayInvalid:
    case HavanaMixerAlreadyExists:
    case HavanaGeneratorAlreadyExists:
    default:
        return -EINVAL;
    };
}
static inline int EncoderStatusToErrno(EncoderStatus_t Result)
{
    switch (Result)
    {
    case EncoderNoError:
        return 0;

    case EncoderNotOpen:
    case EncoderNoDevice:
        return -ENODEV;

    case EncoderNoMemory:
    case EncoderTooManyStreams:
        return -ENOMEM;

    case EncoderBusy:
        return -EAGAIN;

    case EncoderUnknownStream:
    case EncoderMatchNotFound:
        return -ENOENT;

    default:
        return -EINVAL;
    };
}

static class HavanaPlayer_c    *HavanaPlayer;
static class Encoder_Generic_c *Encoder;
static bool IsLowPowerState = false;

//{{{  Streaming engine types
//
// No streaming engine type has any private static data so the choice
// of type for the _type variables it totally arbitrary.
static int stm_se_playback_type;
static int stm_se_play_stream_type;
static int stm_se_audio_generator_type;
static int stm_se_audio_mixer_type;
static int stm_se_audio_player_type;
static int stm_se_audio_reader_type;
static int stm_se_audio_card_type;
static int stm_se_video_source_type;
static int stm_se_video_grab_type;
static int stm_se_video_crc_type;

// Encoder types
static int stm_se_encode_type;
static int stm_se_encode_stream_type;

static struct
{
    const char *tag;
    stm_object_h type;
} stm_se_types[] =
{
#define T(x) { #x, (stm_object_h) &x ## _type }
    T(stm_se_playback),
    T(stm_se_play_stream),
    T(stm_se_audio_generator),
    T(stm_se_audio_mixer),
    T(stm_se_audio_player),
    T(stm_se_audio_reader),
    T(stm_se_audio_card),
    T(stm_se_video_source),
    T(stm_se_video_grab),
    T(stm_se_video_crc),

    T(stm_se_encode),
    T(stm_se_encode_stream),

#undef T
};
//}}}

//{{{  registry_init
//
// Helper function to keep all the registry code separated from
// code that actually does something.
static void registry_init()
{
    for (int i = 0; i < lengthof(stm_se_types); i++)
    {
        int res = stm_registry_add_object(STM_REGISTRY_TYPES,
                                          stm_se_types[i].tag,
                                          stm_se_types[i].type);
        if (0 != res)
        {
            SE_ERROR("Cannot register '%s' type (%d)\n",
                     stm_se_types[i].tag, res);
            // registry is largely informative for streaming engine
            // so errors are non-fatal and not propagated.
        }
    }
}
//}}}
//{{{  registry_term
static void registry_term()
{
    for (int i = 0; i < lengthof(stm_se_types); i++)
    {
        int res = stm_registry_remove_object(stm_se_types[i].type);
        if (0 != res)
        {
            SE_ERROR("Cannot unregister '%s' type (%d)\n",
                     stm_se_types[i].tag, res);
        }
    }
}
//}}}
//{{{  get_tag
static char *get_tag(char *tag, void *object)
{
    int res = stm_registry_get_object_tag(object, tag);
    if (0 != res)
    {
        snprintf(tag, STM_REGISTRY_MAX_TAG_SIZE, "NoObject@%p:%d", object, res);
        tag[STM_REGISTRY_MAX_TAG_SIZE - 1] = '\0';
    }

    return tag;
}
//}}}
//{{{  attach
static int attach(void *Source, void *Sink)
{
    char SourceTag[STM_REGISTRY_MAX_TAG_SIZE];
    char SinkTag[STM_REGISTRY_MAX_TAG_SIZE];
    int Result;
    (void) get_tag(SourceTag, Source);
    (void) get_tag(SinkTag, Sink);
    Result = stm_registry_add_connection(Source, SinkTag, Sink);
    if (0 != Result)
        SE_ERROR("Cannot add connection from %s to %s (%d)\n",
                 SourceTag, SinkTag, Result);

    return Result;
}
//}}}
//{{{  detach
static int detach(void *Source, void *Sink)
{
    char SourceTag[STM_REGISTRY_MAX_TAG_SIZE];
    char SinkTag[STM_REGISTRY_MAX_TAG_SIZE];
    int Result;
    (void) get_tag(SourceTag, Source);
    (void) get_tag(SinkTag, Sink);
    Result = stm_registry_remove_connection(Source, SinkTag);
    if (0 != Result)
        SE_ERROR("Cannot remove connection from %s to %s (%d)\n",
                 SourceTag, SinkTag, Result);

    return Result;
}
//}}}
//{{{  isConnected
static bool isConnected(void *Source, void *Sink)
{
    char SinkTag[STM_REGISTRY_MAX_TAG_SIZE];
    void *GetSink;
    int Result;
    (void) get_tag(SinkTag, Sink);
    //
    // Retrieves the object handle of the object connected with the specified tag
    //
    Result = stm_registry_get_connection(Source, SinkTag, &GetSink);
    if (Result != 0)
    {
        return false;
    }

    //
    // A connection with the specified tag found
    // check if the get sink object is the same
    //
    if (GetSink == Sink)
    {
        return true;
    }

    return false;
}
//}}}

//{{{ push_inject_data_se
//
// Register memory push data function
static int push_inject_data_se(stm_object_h sink_object,
                               struct stm_data_block *block_list,
                               uint32_t block_count,
                               uint32_t *data_blocks)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)sink_object;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return -1;
    }

    SE_EXTRAVERB(group_api, "Stream 0x%p block_count %d\n", HavanaStream->GetStream(), block_count);
    unsigned int index;
    PlayerInputDescriptor_t   InputDescriptor;
    InputDescriptor.PlaybackTimeValid          = false;
    InputDescriptor.DecodeTimeValid            = false;
    InputDescriptor.DataSpecificFlags          = 0;

    for (index = 0; index < block_count; index++)
    {
        SE_VERBOSE(group_api, ">Stream 0x%p len %d\n", HavanaStream->GetStream(), block_list[index].len);
        HavanaStream->InjectData(block_list[index].data_addr, block_list[index].len, InputDescriptor);
        SE_VERBOSE(group_api, "<Stream 0x%p\n", HavanaStream->GetStream());
    }

    *data_blocks = index;
    return 0;
}
//}}}
//{{{ se_data_push_interface
//
//
static stm_data_interface_push_sink_t se_data_push_interface =
{
    attach, // connect field
    detach, // disconnect field
    push_inject_data_se, // push_data field
    KERNEL, // mem_type field
    STM_IOMODE_BLOCKING_IO, // mode field
    0, // alignement field
    0, // max_transfer field
    0, // paketized field
};
//}}}
//{{{ se_clock_data_point_push_interface
//
//
static stm_se_clock_data_point_interface_push_sink_t se_clock_data_point_push_interface =
{
    attach, // connect field
    detach, // disconnect field
    stm_se_playback_set_clock_data_point // set_clock_data_point_data field
};
//}}}

void FillUncompressedBufferDescriptor(UncompressedBufferDesc_t *UncompressedBufferDesc, stm_i_push_get_sink_push_desc *PushBufferDesc)
{
    memset(UncompressedBufferDesc, 0, sizeof(UncompressedBufferDesc_t));

    UncompressedBufferDesc->Content.Width            = PushBufferDesc->buffer_desc.width;
    UncompressedBufferDesc->Content.Height           = PushBufferDesc->buffer_desc.height;
    UncompressedBufferDesc->Content.PixelAspectRatio = Rational_t(PushBufferDesc->pixel_aspect_ratio.numerator, PushBufferDesc->pixel_aspect_ratio.denominator);
    UncompressedBufferDesc->Content.FrameRate        = Rational_t(PushBufferDesc->src_frame_rate.numerator, PushBufferDesc->src_frame_rate.denominator);
    UncompressedBufferDesc->Content.Interlaced       = PushBufferDesc->flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED ? 1 : 0;
    UncompressedBufferDesc->Content.TopFieldFirst    = PushBufferDesc->flags & STM_PIXEL_CAPTURE_BUFFER_TOP_ONLY ? 1 : 0;

    switch (PushBufferDesc->color_space)
    {
    case STM_PIXEL_CAPTURE_RGB:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_DEFAULT;
        UncompressedBufferDesc->Content.VideoFullRange  = 0;
        break;

    case STM_PIXEL_CAPTURE_RGB_VIDEORANGE:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_DEFAULT;
        UncompressedBufferDesc->Content.VideoFullRange  = 1;
        break;

    case STM_PIXEL_CAPTURE_BT601:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_601;
        UncompressedBufferDesc->Content.VideoFullRange  = 0;
        break;

    case STM_PIXEL_CAPTURE_BT601_FULLRANGE:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_601;
        UncompressedBufferDesc->Content.VideoFullRange  = 1;
        break;

    case STM_PIXEL_CAPTURE_BT709:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_709;
        UncompressedBufferDesc->Content.VideoFullRange  = 0;
        break;

    case STM_PIXEL_CAPTURE_BT709_FULLRANGE:
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_709;
        UncompressedBufferDesc->Content.VideoFullRange  = 1;
        break;

    default:
        SE_WARNING("Unknown color range. Defaulting to BT.601\n");
        UncompressedBufferDesc->Content.ColourMode      = UNCOMPRESSED_COLOUR_MODE_601;
        UncompressedBufferDesc->Content.VideoFullRange  = 1;
        break;
    }

    UncompressedBufferDesc->Buffer      = (void *) PushBufferDesc->buffer_desc.video_buffer_addr;
    UncompressedBufferDesc->BufferClass = (Buffer_t) PushBufferDesc->buffer_desc.allocator_data;
}

//{{{ get_buffer_se
//
static int get_buffer_se(stm_object_h sink_object, void *data)
{
    class HavanaStream_c               *HavanaStream    = (class HavanaStream_c *)sink_object;
    stm_i_push_get_sink_get_desc_t     *BufferDesc      = (stm_i_push_get_sink_get_desc *)data;
    HavanaStatus_t                      Status          = HavanaNoError;
    Buffer_t                            DecodeBuffer;
    uint32_t                            LumaAddress, ChromaOffset;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream %p Data %p\n", HavanaStream->GetStream(), BufferDesc);

    if (BufferDesc == NULL)
    {
        SE_ERROR("Invalid Data\n");
        return HavanaStatusToErrno(HavanaError);
    }

    //
    // TODO Add check to forbid compressed data buffer
    //

    SE_VERBOSE(group_api, "Width %d Height %d Format %d\n", BufferDesc->width, BufferDesc->height, BufferDesc->format);

    //
    // Get a decode buffer, components' physical addresses and stride
    //
    Status = HavanaStream->GetDecodeBuffer(BufferDesc->format, BufferDesc->width, BufferDesc->height,
                                           (Buffer_t *)&DecodeBuffer, &LumaAddress, &ChromaOffset,
                                           &(BufferDesc->pitch), true /* NonBlockingInCaseOfFailure */);

    if (Status != HavanaNoError)
    {
        SE_ERROR("Unable to allocate decode buffer\n");
        return HavanaStatusToErrno(Status);
    }

    //
    // To keep track of the Buffer_t object returned above, we use the *allocator_data to store this object.
    // We then retrieve it later just before injection
    //
    BufferDesc->allocator_data           = (void *)DecodeBuffer;
    BufferDesc->video_buffer_addr        = LumaAddress;
    BufferDesc->chroma_buffer_offset     = ChromaOffset;

    return HavanaStatusToErrno(Status);
}
//}}}

//{{{ push_buffer_se
//
static int push_buffer_se(stm_object_h sink_object, void *data)
{
    class HavanaStream_c                *HavanaStream     = (class HavanaStream_c *)sink_object;
    stm_i_push_get_sink_push_desc       *PushBufferDesc   = (stm_i_push_get_sink_push_desc *)data;
    HavanaStatus_t                      Status            = HavanaNoError;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream %p Data %p\n", HavanaStream->GetStream(), PushBufferDesc);

    if (PushBufferDesc == NULL)
    {
        SE_ERROR("Invalid data descriptor\n");
        return HavanaStatusToErrno(HavanaError);
    }

    //
    // TODO Add check to forbid compressed data buffer
    //

    stm_i_push_get_sink_get_desc *BufferDesc = (stm_i_push_get_sink_get_desc *) &PushBufferDesc->buffer_desc;

    SE_VERBOSE(group_api, "GetBufferDesc %p\n", BufferDesc);

    //
    // Retrieve the Buffer_t object from *allocator_data
    //
    Buffer_t DecodeBuffer = (Buffer_t)BufferDesc->allocator_data;

    if (DecodeBuffer == NULL)
    {
        SE_ERROR("Buffer is NULL\n");
        return HavanaStatusToErrno(HavanaError);
    }

    //
    // content_is_valid 0 indicates that this buffer is to be flushed, not displayed
    //
    if (PushBufferDesc->content_is_valid == 0)
    {
        Status = HavanaStream->ReturnDecodeBuffer(DecodeBuffer);
        return HavanaStatusToErrno(Status);
    }

    UncompressedBufferDesc_t UncompressedBufferDesc;
    FillUncompressedBufferDescriptor(&UncompressedBufferDesc, PushBufferDesc);

    PlayerInputDescriptor_t   InputDescriptor;
    memset(&InputDescriptor, 0, sizeof(InputDescriptor));
    if (ValidTime(PushBufferDesc->captured_time))
    {
        InputDescriptor.PlaybackTimeValid    = true;
        InputDescriptor.PlaybackTime         = PushBufferDesc->captured_time;
    }
    else
    {
        InputDescriptor.PlaybackTimeValid    = false;
    }
    InputDescriptor.DecodeTimeValid            = false;
    InputDescriptor.SourceTimeFormat           = TimeFormatUs;
    InputDescriptor.DataSpecificFlags          = 0;

    // Pass it on
    Status = HavanaStream->InjectData((void *)&UncompressedBufferDesc, sizeof(UncompressedBufferDesc), InputDescriptor);

    return HavanaStatusToErrno(Status);
}
//}}}

//{{{ notify_flush_start
//
static void notify_flush_start_se(stm_object_h sink_object)
{
    class HavanaStream_c    *HavanaStream = (class HavanaStream_c *)sink_object;

    if (HavanaStream != NULL)
    {
        int32_t    PolicyLastFrameBehaviourValue;

        SE_VERBOSE(group_api, "Stream %p\n", HavanaStream->GetStream());

        // Get present value of policy
        stm_se_play_stream_get_control(HavanaStream, STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR, &PolicyLastFrameBehaviourValue);

        // Set policy to allow flush of all queued display buffers, including the current one
        stm_se_play_stream_set_control(HavanaStream, STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR, STM_SE_CTRL_VALUE_BLANK_SCREEN);

        // Blank the display plane
        stm_se_play_stream_set_enable(HavanaStream, 0);

        // Drain by discarding all buffers
        stm_se_play_stream_drain(HavanaStream, 1);

        // Reset control to original value
        stm_se_play_stream_set_control(HavanaStream, STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR, PolicyLastFrameBehaviourValue);
    }
    else
    {
        // If we reach here, it is an issue with the caller and so it must be fixed. No need to signal the caller...
        SE_ERROR("Invalid Stream\n");
    }
}
//}}}

//{{{ notify_flush_end_se
//
static void notify_flush_end_se(stm_object_h sink_object)
{
    class HavanaStream_c    *HavanaStream = (class HavanaStream_c *)sink_object;

    if (HavanaStream != NULL)
    {
        // Renable the display plane
        stm_se_play_stream_set_enable(HavanaStream, 1);
    }
    else
    {
        // If we reach here, it is an issue with the caller and so it must be fixed. No need to signal the caller..
        SE_ERROR("Invalid Stream\n");
    }
}
//}}}

//{{{ se_data_push_get_interface
//
//
static stm_data_interface_push_get_sink_t se_data_push_get_interface =
{
    .get_buffer         = get_buffer_se,
    .push_buffer        = push_buffer_se,
    .notify_flush_start = notify_flush_start_se,
    .notify_flush_end   = notify_flush_end_se
};
//}}}

static stm_se_media_t stm_se_play_encoding2media(stm_se_stream_encoding_t encoding)
{
    if (STM_SE_STREAM_ENCODING_AUDIO_FIRST <= encoding &&
        encoding <= STM_SE_STREAM_ENCODING_AUDIO_LAST)
    {
        return STM_SE_MEDIA_AUDIO;
    }

    if (STM_SE_STREAM_ENCODING_VIDEO_FIRST <= encoding &&
        encoding <= STM_SE_STREAM_ENCODING_VIDEO_LAST)
    {
        return STM_SE_MEDIA_VIDEO;
    }

    SE_ERROR("unknown encoding %d\n", encoding);
    return STM_SE_MEDIA_AUDIO; // we have to return something
}

#ifdef __cplusplus
extern "C" {
#endif
//{{{  stm_se_init
int             stm_se_init(void)
{
    HavanaStatus_t              Status;

    if ((Encoder != NULL) || (HavanaPlayer != NULL))
    {
        SE_ERROR("init called on non terminated state\n");
        return HavanaStatusToErrno(HavanaError);
    }

    SE_INFO(group_api, "\n");
    HavanaPlayer = new HavanaPlayer_c;
    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Unable to create player\n");
        return HavanaStatusToErrno(HavanaNoMemory);
    }

    Status = HavanaPlayer->Init();
    if (Status != HavanaNoError)
    {
        SE_ERROR("Unable to init player\n");
        stm_se_term(); // to delete HavanaPlayer
        return HavanaStatusToErrno(Status);
    }

    Status = RegisterBuiltInFactories(HavanaPlayer);
    if (Status != HavanaNoError)
    {
        SE_ERROR("Unable to register builtinfactories\n");
        stm_se_term(); // to delete HavanaPlayer
        return HavanaStatusToErrno(Status);
    }

    // Create the Encoder Object
    Encoder = new Encoder_Generic_c;
    if (Encoder == NULL)
    {
        SE_ERROR("Unable to create Encoder\n");
        stm_se_term(); // to delete HavanaPlayer
        return EncoderStatusToErrno(EncoderNoMemory);
    }

    // Register the Player Buffer Manager with the Encoder
    BufferManager_t BufferManager;
    HavanaPlayer->GetBufferManager(&BufferManager);
    EncoderStatus_t EncoderStatus = Encoder->RegisterBufferManager(BufferManager);
    if (EncoderStatus != EncoderNoError)
    {
        SE_ERROR("Failed to register BufferManager with the Encoder\n");
        stm_se_term(); // to delete HavanaPlayer and Encoder
        return EncoderStatusToErrno(EncoderStatus);
    }

    registry_init();

    // add the sink interface to the play stream object type
    int error;
    error = stm_registry_add_attribute(
                (stm_object_h)&stm_se_play_stream_type,
                STM_DATA_INTERFACE_PUSH,
                STM_REGISTRY_ADDRESS,
                &se_data_push_interface,
                sizeof(se_data_push_interface));
    if (0 != error)
    {
        SE_ERROR("Failed to add attribute (%d)\n", error);
        stm_se_term(); // to delete HavanaPlayer and Encoder
        return error;
    }

    // add the sink interface to the play stream object type
    error = stm_registry_add_attribute(
                (stm_object_h)&stm_se_play_stream_type,
                STM_DATA_INTERFACE_PUSH_GET,
                STM_REGISTRY_ADDRESS,
                &se_data_push_get_interface,
                sizeof(se_data_push_get_interface));
    if (0 != error)
    {
        SE_ERROR("Failed to add attribute (%d)\n", error);
        stm_se_term(); // to delete HavanaPlayer and Encoder
        return error;
    }

    // add the sink interface to the playback object type
    error = stm_registry_add_attribute(
                (stm_object_h)&stm_se_playback_type,
                STM_SE_CLOCK_DATA_POINT_INTERFACE_PUSH,
                STM_REGISTRY_ADDRESS,
                &se_clock_data_point_push_interface,
                sizeof(se_clock_data_point_push_interface));
    if (0 != error)
    {
        SE_ERROR("Failed to add attribute %d\n", error);
        stm_se_term(); // to delete HavanaPlayer and Encoder
        return error;
    }

    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_term
int             stm_se_term(void)
{
    SE_INFO(group_api, "\n");
    delete Encoder;
    Encoder             = NULL;
    delete HavanaPlayer;
    HavanaPlayer        = NULL;
    registry_term();
    return 0;
}
//}}}

//{{{  stm_se_set_control
int             stm_se_set_control(stm_se_ctrl_t           Option,
                                   int32_t                 Value)
{
    PlayerPolicy_t PlayerPolicy;
    SE_DEBUG(group_api, "Option %d Value %d\n", Option, Value);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    return HavanaStatusToErrno(HavanaPlayer->SetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_get_control
int             stm_se_get_control(stm_se_ctrl_t           Option,
                                   int32_t                *Value)
{
    PlayerPolicy_t PlayerPolicy;
    SE_VERBOSE(group_api, "Option %d\n", Option);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    if (Value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    return HavanaStatusToErrno(HavanaPlayer->GetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_get_compound_control
int stm_se_get_compound_control(stm_se_ctrl_t               ctrl,
                                void                        *value)
{
    int err = 0;
    SE_VERBOSE(group_api, "ctrl %d\n", ctrl);

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return -EINVAL;
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return -EINVAL;
    }

    switch (ctrl)
    {
    case STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE:
        err = Codec_Capabilities_c::GetAudioDecodeCapabilities((stm_se_audio_dec_capability_t *) value);
        break;

    case STM_SE_CTRL_GET_CAPABILITY_VIDEO_DECODE:
        err = Codec_Capabilities_c::GetVideoDecodeCapabilities((stm_se_video_dec_capability_t *) value);
        break;

    case STM_SE_CTRL_GET_CAPABILITY_AUDIO_ENCODE:
        err = Coder_Capabilities_c::GetAudioEncodeCapabilities((stm_se_audio_enc_capability_t *) value);
        break;

    case STM_SE_CTRL_GET_CAPABILITY_VIDEO_ENCODE:
        err = Coder_Capabilities_c::GetVideoEncodeCapabilities((stm_se_video_enc_capability_t *) value);
        break;

    default:
        SE_ERROR("Unsupported ctrl %d\n", ctrl);
        err = -EINVAL;
        break;
    }

    return err;
}
//}}}
//{{{  stm_se_set_error_handler
int stm_se_set_error_handler(void *ctx, stm_error_handler handler)
{
    set_fatal_error_handler(ctx, handler);
    return 0;
}
//}}}
//{{{  stm_se_playback_new
int             stm_se_playback_new(const char *name, stm_se_playback_h      *Playback)
{
    HavanaStatus_t              Status;
    class HavanaPlayback_c     *HavanaPlayback = NULL;
    int                         Result          = 0;
    SE_DEBUG(group_api, ">Playback: %s\n", name);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Status      = HavanaPlayer->CreatePlayback(&HavanaPlayback);

    if (Status == HavanaNoError)
    {
        *Playback       = (stm_se_playback_h)HavanaPlayback;
    }

    if (Status == HavanaNoError)
    {
        Result = stm_registry_add_instance(
                     STM_REGISTRY_INSTANCES,
                     &stm_se_playback_type,
                     name,
                     HavanaPlayback);

        if (0 != Result)
        {
            SE_ERROR("Cannot register playback instance (%d)\n", Result);
            // no propagation (see registry_init)
        }

        Result = PlaybackCreateSysfs(name, *Playback);

        if (Result != 0)
        {
            SE_ERROR("PlaybackCreateSysfs() failed\n");
        }
    }

    SE_INFO(group_api, "<Playback %s 0x%p %s\n", name, HavanaPlayback, (Status == HavanaNoError) ? "created" : "not created");
    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_playback_delete
int             stm_se_playback_delete(stm_se_playback_h       Playback)
{
    HavanaStatus_t              Status;
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    int                         Result          = 0;
    SE_DEBUG(group_api, ">Playback 0x%p\n", Playback);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    /* Try to remove the object from registry */
    Result = stm_registry_remove_object(HavanaPlayback);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister playback instance 0x%p (%d)\n", HavanaPlayback, Result);
        // no propagation (see registry_init)
        return HavanaStatusToErrno(HavanaError);
    }

    Result = PlaybackTerminateSysfs(Playback);

    if (Result != 0)
    {
        SE_ERROR("PlaybackTerminateSysfs() failed\n");
    }

    Status = HavanaPlayer->DeletePlayback(HavanaPlayback);

    SE_INFO(group_api, "<Playback 0x%p status %d\n", HavanaPlayback, Status);
    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_play_stream_new
int             stm_se_play_stream_new(const char             *name,
                                       stm_se_playback_h          Playback,
                                       stm_se_stream_encoding_t   Encoding,
                                       stm_se_play_stream_h      *Stream)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    class HavanaStream_c       *HavanaStream    = NULL;
    HavanaStatus_t              Status          = HavanaNoError;
    int                         Result          = 0;
    SE_INFO(group_api, ">Stream %s Playback 0x%p Encoding %d\n", name, HavanaPlayback, Encoding);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    Status      = HavanaPlayback->AddStream(stm_se_play_encoding2media(Encoding), Encoding, &HavanaStream);

    if (Status == HavanaNoError)
    {
        *Stream = (stm_se_play_stream_h)HavanaStream;
        Result = stm_registry_add_instance(
                     HavanaPlayback,
                     &stm_se_play_stream_type,
                     name,
                     HavanaStream);
        if (0 != Result)
        {
            SE_ERROR("Cannot register play_stream instance 0x%p (%d)\n", HavanaStream->GetStream(), Result);
            // no propagation (see registry_init)
        }

        Result = StreamCreateSysfs(name, Playback, stm_se_play_encoding2media(Encoding), *Stream);

        if (Result != 0)
        {
            SE_ERROR("StreamCreateSysfs() failed\n");
        }
    }

    SE_INFO(group_api, "<Stream 0x%p Encoding %d %s on Playback 0x%p\n",
            (Status == HavanaNoError) ? HavanaStream->GetStream() : NULL, Encoding, (Status == HavanaNoError) ? "created" : "not created", HavanaPlayback);

    OS_Dump_MemCheckCounters(__func__);
    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_play_stream_delete
int             stm_se_play_stream_delete(stm_se_play_stream_h    Stream)
{
    class HavanaStream_c   *HavanaStream   = (class HavanaStream_c *)Stream;
    class HavanaPlayback_c *HavanaPlayback = NULL;
    HavanaStatus_t          Status;
    int                     Result = 0;

    if (HavanaStream)
    {
        HavanaPlayback = HavanaStream->GetHavanaPlayback();
        SE_INFO(group_api, ">Playback 0x%p Stream 0x%p\n", HavanaPlayback, HavanaStream->GetStream());
    }
    else
    {
        SE_ERROR("Invalid Stream/Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    /* Try to remove the object from registry */
    Result = stm_registry_remove_object(HavanaStream);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister play_stream instance 0x%p (%d)\n", HavanaStream->GetStream(), Result);

        /* Make sure the object is not connected with another object */
        if (-EBUSY == Result)
        {
            SE_ERROR("Play stream 0x%p still connected to other object\n", HavanaStream->GetStream());
            return HavanaStatusToErrno(HavanaBusy);
        }

        return HavanaStatusToErrno(HavanaNoDevice);
    }

    /* get statistics before removing the stream */
    Result = WrapperGetPlaybackStatisticsForSysfs(HavanaPlayback, Stream);

    if (Result != 0)
    {
        SE_ERROR("WrapperGetPlaybackStatisticsForSysfs() failed\n");
    }

    Result = StreamTerminateSysfs(HavanaPlayback, Stream);

    if (Result != 0)
    {
        SE_ERROR("StreamTerminateSysfs() failed\n");
    }

    Status = HavanaPlayback->RemoveStream(HavanaStream);

    SE_DEBUG(group_api, "<Playback 0x%p Stream deleted status %d\n", HavanaPlayback, Status);
    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_playback_set_speed
int             stm_se_playback_set_speed(stm_se_playback_h       Playback,
                                          int32_t                 Speed)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    SE_INFO(group_api, "Playback 0x%p Speed %d\n", Playback, Speed);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    return HavanaStatusToErrno(HavanaPlayback->SetSpeed(Speed));
}
//}}}
//{{{  stm_se_playback_get_speed
int             stm_se_playback_get_speed(stm_se_playback_h       Playback,
                                          int32_t                *PlaySpeed)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    SE_VERBOSE(group_api, "Playback 0x%p\n", Playback);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    if (PlaySpeed == NULL)
    {
        SE_ERROR("Invalid PlaySpeed pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaPlayback->GetSpeed(PlaySpeed));
}
//}}}
//{{{  stm_se_playback_set_control
int             stm_se_playback_set_control(stm_se_playback_h       Playback,
                                            stm_se_ctrl_t           Option,
                                            int32_t                 Value)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    PlayerPolicy_t PlayerPolicy;
    SE_DEBUG(group_api, "Playback 0x%p Option %d Value %d\n", Playback, Option, Value);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    return HavanaStatusToErrno(HavanaPlayback->SetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_playback_get_control
int             stm_se_playback_get_control(stm_se_playback_h       Playback,
                                            stm_se_ctrl_t           Option,
                                            int32_t                *Value)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    PlayerPolicy_t PlayerPolicy;
    SE_VERBOSE(group_api, "Playback 0x%p Option %d\n", Playback, Option);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    if (Value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    return HavanaStatusToErrno(HavanaPlayback->GetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_playback_set_native_time
int             stm_se_playback_set_native_time(stm_se_playback_h    Playback,
                                                unsigned long long   NativeTime,
                                                unsigned long long   SystemTime)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    SE_INFO(group_api, "Playback 0x%p NativeTime %llu SystemTime %llu\n",
            Playback, NativeTime, SystemTime);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    return HavanaStatusToErrno(HavanaPlayback->SetNativePlaybackTime(NativeTime, SystemTime));
}
//}}}
//{{{  stm_se_playback_set_clock_data_point
int       stm_se_playback_set_clock_data_point(stm_se_playback_h       Playback,
                                               stm_se_time_format_t    TimeFormat,
                                               bool                    NewSequence,
                                               unsigned long long      SourceTime,
                                               unsigned long long      SystemTime)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    SE_VERBOSE(group_api, "Playback 0x%p TimeFormat %d NewSequence %d SourceTime %llu SystemTime %llu\n",
               Playback, TimeFormat, NewSequence, SourceTime, SystemTime);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    // Enable the capture of clock-data points through strelay to ease debugging of the clock-recovery algorithm
    // It provides a mean to record all the data-points and intermediate gradient computation that can then
    // be reviewed later by loading the data in a spreadsheet and post-process them
    {
        unsigned long long Now = OS_GetTimeInMicroSeconds();
        struct data_point_s
        {
            stm_se_playback_h  Playback;
            unsigned long long SourceTime;
            unsigned long long SystemTime;
            bool               NewSequence;
        } DataPoint = {Playback, SourceTime, SystemTime, NewSequence};
        st_relayfs_write_se(ST_RELAY_TYPE_CLOCK_RECOV_DATAPOINT, ST_RELAY_SOURCE_SE,
                            (unsigned char *)&DataPoint, sizeof(DataPoint), &Now);
    }
    return HavanaStatusToErrno(HavanaPlayback->SetClockDataPoint(TimeFormat, NewSequence, SourceTime, SystemTime));
}
//}}}

//{{{  stm_se_playback_get_clock_data_point
int       stm_se_playback_get_clock_data_point(stm_se_playback_h       Playback,
                                               unsigned long long     *SourceTime,
                                               unsigned long long     *SystemTime)
{
    class HavanaPlayback_c     *HavanaPlayback  = (class HavanaPlayback_c *)Playback;
    SE_VERBOSE(group_api, "Playback 0x%p\n", Playback);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    if ((SourceTime == NULL) || (SystemTime == NULL))
    {
        SE_ERROR("Invalid pointers\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaPlayback->GetClockDataPoint(SourceTime, SystemTime));
}
//}}}
//{{{  __stm_se_playback_reset_statistics
int             __stm_se_playback_reset_statistics(stm_se_playback_h         Playback)
{
    class HavanaPlayback_c       *HavanaPlayback    = (class HavanaPlayback_c *)Playback;
    SE_DEBUG(group_api, "Playback 0x%p\n", Playback);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    return HavanaStatusToErrno(HavanaPlayback->ResetStatistics());
}
//}}}
//{{{  __stm_se_playback_get_statistics
int             __stm_se_playback_get_statistics(stm_se_playback_h Playback,
                                                 struct __stm_se_playback_statistics_s     *Statistics)
{
    class HavanaPlayback_c       *HavanaPlayback    = (class HavanaPlayback_c *)Playback;
    SE_VERBOSE(group_api, "Playback 0x%p\n", Playback);

    if (HavanaPlayback == NULL)
    {
        SE_ERROR("Invalid Playback\n");
        return HavanaStatusToErrno(HavanaPlaybackInvalid);
    }

    if (Statistics == NULL)
    {
        SE_ERROR("Invalid playerplayback pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaPlayback->GetStatistics(Statistics));
}
//}}}

//{{{  stm_se_play_stream_inject_data
int stm_se_play_stream_inject_data(stm_se_play_stream_h play_stream,
                                   const void             *data,
                                   uint32_t                data_length)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, ">Stream 0x%p length %d\n", HavanaStream->GetStream(), data_length);

    PlayerInputDescriptor_t   InputDescriptor;
    InputDescriptor.PlaybackTimeValid          = false;
    InputDescriptor.DecodeTimeValid            = false;
    InputDescriptor.DataSpecificFlags          = 0;

    HavanaStream->InjectData(data, data_length, InputDescriptor);

    SE_VERBOSE(group_api, "<Stream 0x%p\n", HavanaStream->GetStream());

    return data_length;
}
//}}}

//{{{  stm_se_play_stream_set_control
int             stm_se_play_stream_set_control(stm_se_play_stream_h    Stream,
                                               stm_se_ctrl_t           Option,
                                               int32_t                 Value)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    PlayerPolicy_t PlayerPolicy;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p Option %d Value %d\n", HavanaStream->GetStream(), Option, Value);

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    // if returned value == NonCascadedControls, it is a non-policy control
    if (PlayerPolicy == NonCascadedControls)
    {
        return HavanaStatusToErrno(HavanaStream->SetControl(Option, Value));
    }

    // otherwise it is a policy control
    return HavanaStatusToErrno(HavanaStream->SetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_play_stream_get_control
int             stm_se_play_stream_get_control(stm_se_play_stream_h    Stream,
                                               stm_se_ctrl_t           Option,
                                               int32_t                *Value)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    PlayerPolicy_t PlayerPolicy;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p Option %d\n", HavanaStream->GetStream(), Option);

    if (Value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (Option >= StmMapOptionMax)
    {
        SE_ERROR("Unknown option %d\n", Option);
        return -EINVAL;
    }

    PlayerPolicy = StmMapOption [Option];

    if (PlayerPolicy == PolicyNotImplemented)
    {
        SE_ERROR("Unsupported option %d\n", Option);
        return -EOPNOTSUPP;
    }

    // if returned value == NonCascadedControls, it is a non-policy control
    if (PlayerPolicy == NonCascadedControls)
    {
        return HavanaStatusToErrno(HavanaStream->GetControl(Option, Value));
    }

    // otherwise it is a policy control
    return HavanaStatusToErrno(HavanaStream->GetOption(PlayerPolicy, Value));
}
//}}}
//{{{  stm_se_play_stream_set_enable
int             stm_se_play_stream_set_enable(stm_se_play_stream_h    Stream,
                                              bool                    Enable)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, "Stream 0x%p Enable %d\n", HavanaStream->GetStream(), Enable);

    return HavanaStatusToErrno(HavanaStream->Enable(Enable));
}
//}}}
//{{{  stm_se_play_stream_set_enable
int             stm_se_play_stream_get_enable(stm_se_play_stream_h    play_stream,
                                              bool                   *enable)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    if (enable == NULL)
    {
        SE_ERROR("Null pointer given\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaStream->GetEnable(enable));
}
//}}}
//{{{  stm_se_play_stream_drain
int             stm_se_play_stream_drain(stm_se_play_stream_h    Stream,
                                         unsigned int            Discard)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, "Stream 0x%p Discard %d\n", HavanaStream->GetStream(), Discard);

    return HavanaStatusToErrno(HavanaStream->Drain(Discard));
}
//}}}

static int __stm_se_play_stream_attach_to_pad(stm_se_play_stream_h              play_stream,
                                              stm_object_h                      sink,
                                              stm_se_play_stream_output_port_t  data_type,
                                              stm_se_sink_input_port_t          input_port)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;
    stm_object_h                DisplayType;
    unsigned int                ManifestorCapability;
    class Manifestor_c         *Manifestor;
    HavanaStatus_t              Status          = HavanaNoError;
    int                         Result;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (sink == NULL)
    {
        SE_ERROR("Sink is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    //
    // Make sure the play stream is not already connected to this sink object
    //
    if (isConnected(play_stream, sink) == true)
    {
        return -EALREADY;
    }

    if (data_type == STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY)
    {
        // User data connection to memorySink
        Status = HavanaStream->UserDataSender.Connect(sink);
    }
    else
    {
        stm_object_h  sinkType;
        char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
        int32_t       returnedSize;
        //
        // Audio and Video Grab is asked when Sink object get STM_DATA_INTERFACE_PULL
        // or Video zero copy Brab when Sink obkect get STM_DATA_INTERFACE_PUSH_RELEASE
        //
        ManifestorCapability = 0;  // assuming no MANIFESTOR_CAPABILITY_x takes 0 value
        //
        // Try to retrieve the type of the sink object
        //
        Result = stm_registry_get_object_type(sink, &sinkType);
        if (Result == 0)
        {
            //
            // Check sink object support STM_DATA_INTERFACE_PULL interface
            //
            Result = stm_registry_get_attribute(sink,
                                                STM_DATA_INTERFACE_PULL,
                                                tagTypeName,
                                                0,
                                                NULL,
                                                (int *)&returnedSize);
            if (Result == 0)
            {
                ManifestorCapability = MANIFESTOR_CAPABILITY_SOURCE;
            }

            //
            // Check sink object support STM_DATA_INTERFACE_PUSH_RELEASE interface
            //
            if (ManifestorCapability == 0)
            {
                Result = stm_registry_get_attribute(sink,
                                                    STM_DATA_INTERFACE_PUSH_RELEASE,
                                                    tagTypeName,
                                                    0,
                                                    NULL,
                                                    (int *)&returnedSize);
                if (Result == 0)
                {
                    ManifestorCapability = MANIFESTOR_CAPABILITY_PUSH_RELEASE;
                }
            }
        }

        if (ManifestorCapability == 0)
        {
            //
            // Following code is to offer downward compatibility
            //
            // At this point the sink is an object which is understood by STMFB, the audio gubbins or V4L2
            // This function needs to translate this into something to enable it to determine which type
            // of manifestor to create and how to initialise it.  I assume that this is done by some sort of
            // registry fiddling.
            Result = stm_registry_get_object_type(sink, &DisplayType);
            if (Result != 0)
            {
                SE_ERROR("Unable to determine object type, determining manifestor capabilities not possible (%d)\n", Result);
                return Result;
            }

            ManifestorCapability        = (DisplayType == &stm_se_video_grab_type)    ? MANIFESTOR_CAPABILITY_GRAB   :
                                          (DisplayType == &stm_se_video_crc_type)     ? MANIFESTOR_CAPABILITY_CRC    :
                                          (DisplayType == &stm_se_encode_stream_type) ? MANIFESTOR_CAPABILITY_ENCODE :
                                          MANIFESTOR_CAPABILITY_DISPLAY;
            // ManifestorCapability will be set to a MANIFESTOR_CAPABILITY_x value at this point
        }

        if (sinkType == &stm_se_audio_player_type)
        {
            // AudioPlayers can only be attached to the PRIMARY PORT.
            if (input_port != STM_SE_SINK_INPUT_PORT_PRIMARY)
            {
                return HavanaStatusToErrno(HavanaError);
            }

            // multiple audio-player could be attached to the stream simultaneously but they all share the same "manifestor"
            Status  = HavanaStream->AddAudioPlayer(sink);
            if (Status == HavanaNoError)
            {
                Status  = HavanaPlayer->GetManifestor(HavanaStream, ManifestorCapability, HavanaStream->GetAudioPassThrough(), &Manifestor, input_port);
            }
        }
        else
        {
            Status  = HavanaPlayer->GetManifestor(HavanaStream, ManifestorCapability, sink, &Manifestor, input_port);
        }

        if (Status == HavanaMixerAlreadyExists)
        {
            // every AudioPlayer attached as pass-through of a
            // playstream share the same manifestor (they shall
            // all be manifested at the sfreq of the play-stream).

            // The manifestor is already managed by the manifestor-coordinator.
            // No need to add it.
            Status = HavanaNoError;
        }
        else
        {
            if (Status != HavanaNoError)
            {
                return HavanaStatusToErrno(Status);
            }

            Status      = HavanaStream->AddManifestor(Manifestor);

            if (Status != HavanaNoError)
            {
                return HavanaStatusToErrno(Status);
            }
        }
    }

    attach(play_stream, sink);
    return HavanaStatusToErrno(Status);

}
//{{{  stm_se_play_stream_attach
int        stm_se_play_stream_attach(stm_se_play_stream_h              play_stream,
                                     stm_object_h                      sink,
                                     stm_se_play_stream_output_port_t  data_type)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (play_stream == NULL)
    {
        SE_ERROR("Failed to attach: play_stream is NULL\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, ">Stream 0x%p Sink 0x%p\n", HavanaStream->GetStream(), sink);

    int Status = __stm_se_play_stream_attach_to_pad(play_stream, sink, data_type, STM_SE_SINK_INPUT_PORT_PRIMARY);

    SE_INFO(group_api, "<Stream 0x%p Sink 0x%p status %d\n", HavanaStream->GetStream(), sink, Status);

    return Status;
}
//}}}

//{{{  stm_se_play_stream_attach_to_pad
int        stm_se_play_stream_attach_to_pad(stm_se_play_stream_h             play_stream,
                                            stm_object_h                     sink,
                                            stm_se_play_stream_output_port_t data_type,
                                            stm_se_sink_input_port_t         input_port)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (play_stream == NULL)
    {
        SE_ERROR("Failed to attach: play_stream is NULL\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, ">Stream 0x%p Sink 0x%p InputPort:%d\n", HavanaStream->GetStream(), sink, input_port);

    int Status = __stm_se_play_stream_attach_to_pad(play_stream, sink, data_type, input_port);

    SE_INFO(group_api, "<Stream 0x%p Sink 0x%p status %d\n", HavanaStream->GetStream(), sink, Status);

    return Status;
}
//}}}

//{{{  stm_se_play_stream_detach
int        stm_se_play_stream_detach(stm_se_play_stream_h            play_stream,
                                     stm_object_h                    sink)
{
    HavanaStatus_t              Status          = HavanaNoError;
    HavanaStatus_t              StatusUserData  = HavanaNoError;
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (play_stream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, ">Stream 0x%p Sink 0x%p\n", HavanaStream->GetStream(), sink);

    if (sink == NULL)
    {
        SE_ERROR("Invalid Sink\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    //
    // Make sure the play stream is connected to the sink object
    //
    if (isConnected(play_stream, sink) == false)
    {
        return -ENOTCONN;
    }

    //
    // Try to disconnect UserData attachment in case Sink object
    // is the memorySink object passed in stm_se_play_stream_attach
    // with DataType=STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY
    //
    StatusUserData = HavanaStream->UserDataSender.Disconnect(sink);

    bool                          MustDeleteManifestor = true;
    int                           Result;
    stm_object_h                  sinkType;
    //
    // Try to retrieve the type of the sink object
    //
    Result = stm_registry_get_object_type(sink, &sinkType);

    //
    // try to detach memorySink attachment only if it's not a UserData sink
    //
    if (Result == 0)
    {
        ManifestorStatus_t  StatusManifestor     = ManifestorNoError;
        unsigned int        ManifestorCapability = 0;
        if (StatusUserData == HavanaError)
        {
            char                          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
            int32_t                       returnedSize;
            //
            // Check sink object support STM_DATA_INTERFACE_PULL interface
            //
            Result = stm_registry_get_attribute(sink,
                                                STM_DATA_INTERFACE_PULL,
                                                tagTypeName,
                                                0,
                                                NULL,
                                                (int *)&returnedSize);

            if (Result == 0)
            {
                ManifestorCapability = MANIFESTOR_CAPABILITY_SOURCE;
            }

            //
            // Check sink object support STM_DATA_INTERFACE_PUSH_RELEASE interface
            //
            if (ManifestorCapability == 0)
            {
                Result = stm_registry_get_attribute(sink,
                                                    STM_DATA_INTERFACE_PUSH_RELEASE,
                                                    tagTypeName,
                                                    0,
                                                    NULL,
                                                    (int *)&returnedSize);
                if (Result == 0)
                {
                    ManifestorCapability = MANIFESTOR_CAPABILITY_PUSH_RELEASE;
                }
            }

            if (ManifestorCapability == 0)
            {
                ManifestorCapability        = (sinkType == &stm_se_video_grab_type)    ? MANIFESTOR_CAPABILITY_GRAB   :
                                              (sinkType == &stm_se_video_crc_type)     ? MANIFESTOR_CAPABILITY_CRC    :
                                              (sinkType == &stm_se_encode_stream_type) ? MANIFESTOR_CAPABILITY_ENCODE :
                                              MANIFESTOR_CAPABILITY_DISPLAY;
            }
        }

        //
        // If Sink object has PULL or PUSH_REALSE interface then try to disconnect it from existing Manifestor
        // We assume there is only one manisfestor Source by stream
        //
        if ((ManifestorCapability == MANIFESTOR_CAPABILITY_SOURCE)
            || (ManifestorCapability == MANIFESTOR_CAPABILITY_PUSH_RELEASE))
        {
            class Manifestor_Source_Base_c  *SourceManifestor;
            //
            // Try to find SourceManifestor
            //
            Status   = HavanaStream->FindManifestorByCapability(ManifestorCapability,
                                                                (class Manifestor_c **)&SourceManifestor);

            if (Status == HavanaNoError)
            {
                //
                // Disconnect source from memorySink or STLinuxTv
                //
                StatusManifestor = SourceManifestor->Disconnect(sink);

                if (StatusManifestor == ManifestorError)
                {
                    SE_ERROR("Failed to disconnect Sink object\n");
                    return HavanaStatusToErrno(HavanaError);
                }
            }
        }
        else if (ManifestorCapability == MANIFESTOR_CAPABILITY_ENCODE)
        {
            HavanaStream->LockManifestors();
            Manifestor_t *manifestorArray = HavanaStream->GetManifestors();
            bool found = false;

            for (int i = 0; i < MAX_MANIFESTORS; i++)
            {
                if (manifestorArray[i] != NULL)
                {
                    unsigned int capabilities;
                    manifestorArray[i]->GetCapabilities(&capabilities);

                    if (capabilities & MANIFESTOR_CAPABILITY_ENCODE)
                    {
                        // Try to find SourceManifestor
                        class Manifestor_Encoder_c *EncodeManifestor = (class Manifestor_Encoder_c *) manifestorArray[i];

                        // Disconnect source from memorySink
                        if (EncodeManifestor->IsConnected(sink) == true)
                        {
                            StatusManifestor = EncodeManifestor->Disconnect(sink);
                            if (StatusManifestor == ManifestorError)
                            {
                                SE_ERROR("Failed to disconnect EncodeStream from the EncodeManifestor object\n");
                                HavanaStream->UnlockManifestors();
                                return HavanaStatusToErrno(HavanaError);
                            }
                            else
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (found == false)
            {
                SE_WARNING("no connected EncodeStream found in the EncodeManifestor object\n");
            }
            HavanaStream->UnlockManifestors();
        }

        if (sinkType == &stm_se_audio_player_type)
        {
            HavanaStatus_t Status = HavanaStream->RemoveAudioPlayer(sink);

            if (Status != HavanaNoError)
            {
                SE_ERROR("Failed to detach PlayStream from AudioPlayer 0x%08x\n", (int)sink);
            }

            // either there is still a player attached to the passthrough
            // mixer in which case we should definitely not delete the
            // manifestor or it was the last player attached to the  mixer
            // and in that case the manifestor had to be deleted before
            // detaching the player (so it is already done).
            MustDeleteManifestor = false;
        }
    }

    if (MustDeleteManifestor)
    {
        // DeleteManifestor now takes care of removing the link with ManifestCoordinator
        // Delete corresponding Manifestor/Display in case of audio
        HavanaPlayer->DeleteManifestor(HavanaStream, sink);
    }

    // remove connection in registry
    detach(play_stream, sink);
    SE_INFO(group_api, "<Stream 0x%p Sink 0x%p\n", HavanaStream->GetStream(), sink);
    return HavanaStatusToErrno(HavanaNoError);
}
//}}}

//{{{  __stm_se_play_stream_reset_statistics
int             __stm_se_play_stream_reset_statistics(stm_se_play_stream_h         Stream)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_WARNING("Invalid Stream\n"); // W.A. for wrong call by test app: dont use SE_ERROR
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStatusToErrno(HavanaStream->ResetStatistics());
}
//}}}
//{{{  __stm_se_play_stream_get_statistics
int             __stm_se_play_stream_get_statistics(stm_se_play_stream_h Stream,
                                                    struct statistics_s     *Statistics)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    if (Statistics == NULL)
    {
        SE_ERROR("Invalid Statistics pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaStream->GetStatistics(Statistics));
}
//}}}
//{{{  __stm_se_play_stream_reset_attributs
int             __stm_se_play_stream_reset_attributes(stm_se_play_stream_h         Stream)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStatusToErrno(HavanaStream->ResetAttributes());
}
//}}}


//{{{  __stm_se_play_stream_get_attributs
int             __stm_se_play_stream_get_attributes(stm_se_play_stream_h Stream,
                                                    struct attributes_s     *Attributes)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    if (Attributes == NULL)
    {
        SE_ERROR("Invalid Attributes pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return HavanaStatusToErrno(HavanaStream->GetAttributes(Attributes));
}
//}}}

//{{{  stm_se_play_stream_get_info
int             stm_se_play_stream_get_info(stm_se_play_stream_h play_stream,
                                            stm_se_play_stream_info_t       *info
                                           )
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (info == NULL)
    {
        SE_ERROR("Invalid info pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    //SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStatusToErrno(HavanaStream->GetPlayInfo(info));
}
//}}}
//{{{  stm_se_play_stream_set_interval
int             stm_se_play_stream_set_interval(stm_se_play_stream_h    Stream,
                                                unsigned long long      Start,
                                                unsigned long long      End)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p Start %llu End %llu\n", HavanaStream->GetStream(), Start, End);

    return HavanaStatusToErrno(HavanaStream->SetPlayInterval(Start, End));
}
//}}}
//{{{  stm_se_play_stream_step
int             stm_se_play_stream_step(stm_se_play_stream_h    Stream)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStatusToErrno(HavanaStream->Step());
}
//}}}
//{{{  stm_se_play_stream_inject_discontinuity
int             stm_se_play_stream_inject_discontinuity(stm_se_play_stream_h    play_stream,
                                                        bool          smooth_reverse,
                                                        bool          surplus_data,
                                                        bool          end_of_stream)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, "Stream 0x%p smooth_reverse %d surplus_data %d eos %d\n",
            HavanaStream->GetStream(), smooth_reverse, surplus_data, end_of_stream);

    return HavanaStatusToErrno(HavanaStream->Discontinuity(smooth_reverse, surplus_data, end_of_stream));
}
//}}}
//{{{  stm_se_play_stream_switch
int             stm_se_play_stream_switch(stm_se_play_stream_h    Stream,
                                          stm_se_stream_encoding_t Encoding)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_INFO(group_api, "Stream 0x%p Encoding %d\n", HavanaStream->GetStream(), Encoding);

    OS_Dump_MemCheckCounters("<stm_se_play_stream_switch");
    int status = HavanaStatusToErrno(HavanaStream->Switch(Encoding));
    OS_Dump_MemCheckCounters(">stm_se_play_stream_switch");

    return status;
}
//}}}

//{{{  stm_se_play_stream_poll_message
int             stm_se_play_stream_poll_message(stm_se_play_stream_h       Stream,
                                                stm_se_play_stream_msg_id_t id,
                                                stm_se_play_stream_msg_t   *message)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    MessageStatus_t             MessageStatus;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (HavanaStream->GetStream() == NULL)
    {
        SE_ERROR("Invalid PlayerStream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p id %d\n", HavanaStream->GetStream(), id);

    MessageStatus = HavanaStream->GetStream()->GetMessenger()->GetPollMessage(id, message);

    if (MessageStatus != MessageNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    if (message->msg_id == STM_SE_PLAY_STREAM_MSG_INVALID)
    {
        return -EAGAIN;
    }

    return 0;
}
//}}}
//{{{  stm_se_play_stream_get_message
int             stm_se_play_stream_get_message(stm_se_play_stream_h              Stream,
                                               stm_se_play_stream_subscription_h subscription,
                                               stm_se_play_stream_msg_t         *message)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    MessageStatus_t             MessageStatus;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (HavanaStream->GetStream() == NULL)
    {
        SE_ERROR("Invalid PlayerStream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_VERBOSE(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    MessageStatus = HavanaStream->GetStream()->GetMessenger()->GetMessage(subscription, message);

    if (MessageStatus != MessageNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    if (message->msg_id == STM_SE_PLAY_STREAM_MSG_INVALID)
    {
        return -EAGAIN;
    }

    return 0;
}
//}}}
//{{{  stm_se_play_stream_subscribe
int             stm_se_play_stream_subscribe(stm_se_play_stream_h                  Stream,
                                             uint32_t                           messageMask,
                                             uint32_t                           depth,
                                             stm_se_play_stream_subscription_h *subscription)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    MessageStatus_t             MessageStatus;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (HavanaStream->GetStream() == NULL)
    {
        SE_ERROR("Invalid PlayerStream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    MessageStatus = HavanaStream->GetStream()->GetMessenger()->CreateSubscription(messageMask, depth, subscription);
    if (MessageStatus != MessageNoError)
    {
        SE_ERROR("Unable to subcribe\n");
        return HavanaStatusToErrno(HavanaError);
    }
    return 0;
}
//}}}

//{{{  stm_se_play_stream_unsubscribe
int             stm_se_play_stream_unsubscribe(stm_se_play_stream_h               Stream,
                                               stm_se_play_stream_subscription_h  subscription)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;
    MessageStatus_t             MessageStatus;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (HavanaStream->GetStream() == NULL)
    {
        SE_ERROR("Invalid PlayerStream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    MessageStatus = HavanaStream->GetStream()->GetMessenger()->DeleteSubscription(subscription);
    if (MessageStatus != MessageNoError)
    {
        SE_ERROR("Unable to unsubcribe\n");
        return HavanaStatusToErrno(HavanaError);
    }
    return 0;
}
//}}}

//{{{  stm_se_play_stream_set_alarm
int             stm_se_play_stream_set_alarm(stm_se_play_stream_h         Stream,
                                             stm_se_play_stream_alarm_t   alarm,
                                             bool                         enable,
                                             void                        *value)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p alarm %d enable %d\n",
             HavanaStream->GetStream(), alarm , enable);
    return HavanaStream->SetAlarm(alarm, enable, value);
}
//}}}

//{{{  stm_se_play_stream_set_discard_trigger
int             stm_se_play_stream_set_discard_trigger(stm_se_play_stream_h                  Stream,
                                                       stm_se_play_stream_discard_trigger_t *trigger)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (trigger == NULL)
    {
        SE_ERROR("Invalid trigger\n");
        return -EINVAL;
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStream->SetDiscardTrigger(*trigger);
}
//}}}

//{{{  stm_se_play_stream_reset_discard_triggers
int             stm_se_play_stream_reset_discard_triggers(stm_se_play_stream_h         Stream)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    SE_DEBUG(group_api, "Stream 0x%p\n", HavanaStream->GetStream());

    return HavanaStream->ResetDiscardTrigger();
}
//}}}

//{{{  __stm_se_advanced_audio_mixer_new ( internal implementation )
static int      __stm_se_advanced_audio_mixer_new(const char                 *name,
                                                  const stm_se_mixer_spec_t   topology,
                                                  stm_se_audio_mixer_h       *audio_mixer)
{
    Mixer_Mme_c                  *Mixer;
    HavanaStatus_t                Status;
    int                           Result;
    SE_DEBUG(group_api, "> name %s\n", name);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    // Check Topology boundaries
    switch (topology.type)
    {
    case STM_SE_MIXER_SINGLE_STAGE_MIXING:
        if (topology.nb_max_decoded_audio > STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS)
        {
            SE_ERROR("SINGLE_STAGE_MIXER doesn't support so many decoded input (specified %d, max = %d) \n",
                     topology.nb_max_decoded_audio, STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS);
            return HavanaStatusToErrno(HavanaNotOpen);
        }
        if (topology.nb_max_application_audio > STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS)
        {
            SE_ERROR("SINGLE_STAGE_MIXER doesn't support so many application input (specified %d, max = %d) \n",
                     topology.nb_max_application_audio, STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS);
            return HavanaStatusToErrno(HavanaNotOpen);
        }
        if (topology.nb_max_interactive_audio > STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS)
        {
            SE_ERROR("SINGLE_STAGE_MIXER doesn't support so many interactive input (specified %d, max = %d) \n",
                     topology.nb_max_interactive_audio, STM_SE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS);
            return HavanaStatusToErrno(HavanaNotOpen);
        }
        break;

    case STM_SE_MIXER_DUAL_STAGE_MIXING:
        if (topology.nb_max_interactive_audio > STM_SE_DUAL_STAGE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS)
        {
            SE_ERROR("DUAL_STAGE_MIXER doesn't support so many Interactive Audio (%d, Limit = %d) \n",
                     topology.nb_max_interactive_audio, STM_SE_DUAL_STAGE_MIXER_NB_MAX_INTERACTIVE_AUDIO_INPUTS);
            return HavanaStatusToErrno(HavanaNotOpen);
        }
        if (topology.nb_max_application_audio > STM_SE_DUAL_STAGE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS)
        {
            SE_ERROR("DUAL_STAGE_MIXER doesn't support so many application input (specified %d, max = %d) \n",
                     topology.nb_max_application_audio, STM_SE_DUAL_STAGE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS);
            return HavanaStatusToErrno(HavanaNotOpen);
        }
        break;

    default:
        SE_ERROR("Mixer type %d not supported\n", topology.type);
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Status = HavanaPlayer->CreateMixer(name, topology, &Mixer);

    if (Status == HavanaNoError)
    {
        *audio_mixer = (stm_se_audio_mixer_h)Mixer;
    }

    if (Status == HavanaNoError)
    {
        Result = stm_registry_add_instance(
                     STM_REGISTRY_INSTANCES,
                     &stm_se_audio_mixer_type,
                     name,
                     *audio_mixer);
        if (0 != Result && -EEXIST != Result)
        {
            SE_ERROR("Cannot register stm_se_audio_mixer instance %s (%d)\n", name, Result);
            // no propagation (see registry_init)
        }
    }

    SE_INFO(group_api, "< name %s Mixer 0x%p\n", name, Mixer);
    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(Status);
}
//}}}

//{{{  stm_se_advanced_audio_mixer_new
//
// This function currently has not _delete partner because the mixer is
// instantiated once and live for the entire system. This function is
// actually a mere accessor to the single global instance (so -EEXISTS is
// ignored if the registry reports this!
int             stm_se_advanced_audio_mixer_new(const char                 *name,
                                                const stm_se_mixer_spec_t   topology,
                                                stm_se_audio_mixer_h       *audio_mixer)
{
    return __stm_se_advanced_audio_mixer_new(name, topology, audio_mixer);
}
//}}}

//{{{  stm_se_audio_mixer_new
int             stm_se_audio_mixer_new(const char             *name,
                                       stm_se_audio_mixer_h   *audio_mixer)
{
    const stm_se_mixer_spec_t DefaultTopology =
    {
        STM_SE_MIXER_SINGLE_STAGE_MIXING,
        STM_SE_MIXER_NB_MAX_DECODED_AUDIO_INPUTS,
        STM_SE_MIXER_NB_MAX_APPLICATION_AUDIO_INPUTS,
        4, // up to 4 interactive audio-generators
        STM_SE_MIXER_NB_MAX_OUTPUTS
    };

    return __stm_se_advanced_audio_mixer_new(name, DefaultTopology, audio_mixer);
}
//}}}

//{{{  stm_se_audio_mixer_delete
int             stm_se_audio_mixer_delete(stm_se_audio_mixer_h audio_mixer)
{
    Mixer_Mme_c                  *Mixer;
    int                           Result;
    SE_DEBUG(group_api, ">Mixer 0x%p\n", audio_mixer);

    if (audio_mixer == NULL)
    {
        SE_ERROR("Invalid Mixer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Mixer = (Mixer_Mme_c *)audio_mixer;

    if (HavanaPlayer->DeleteMixer(Mixer) == HavanaError)
    {
        return -EBUSY;
    }

    Result = stm_registry_remove_object(audio_mixer);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister audio_mixer 0x%p (%d)\n", audio_mixer, Result);
        // no propagation (see registry_init)
    }

    SE_INFO(group_api, "<Mixer 0x%p\n", audio_mixer);
    OS_Dump_MemCheckCounters(__func__);

    return 0;
}
//{{{  stm_se_audio_mixer_set_control
int             stm_se_audio_mixer_set_control(stm_se_audio_mixer_h           audio_mixer,
                                               stm_se_ctrl_t                  ctrl,
                                               int32_t                        value)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) audio_mixer;
    PlayerStatus_t Status;
    SE_DEBUG(group_api, "Mixer 0x%p ctrl %d value %d\n", audio_mixer, ctrl, value);

    if (Mixer == NULL)
    {
        SE_ERROR("Invalid Mixer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = Mixer->SetOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
    // alternative:
    //return HavanaStatusToErrno(Mixer->SetOption (ctrl, value));
}
//}}}
//{{{  stm_se_audio_mixer_get_control
int             stm_se_audio_mixer_get_control(stm_se_audio_mixer_h           audio_mixer,
                                               stm_se_ctrl_t                  ctrl,
                                               int32_t                       *value)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) audio_mixer;
    PlayerStatus_t Status;
    SE_VERBOSE(group_api, "Mixer 0x%p ctrl %d\n", audio_mixer, ctrl);

    if (NULL == Mixer)
    {
        SE_ERROR("Invalid Mixer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = Mixer->GetOption(ctrl, *value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_mixer_set_compound_control
int             stm_se_audio_mixer_set_compound_control(stm_se_audio_mixer_h    audio_mixer,
                                                        stm_se_ctrl_t                    ctrl,
                                                        const void                       *value)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) audio_mixer;
    PlayerStatus_t Status;
    SE_DEBUG(group_api, "Mixer 0x%p ctrl %d\n", audio_mixer, ctrl);

    if (Mixer == NULL)
    {
        SE_ERROR("Invalid Mixer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = Mixer->SetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_mixer_get_compound_control
int             stm_se_audio_mixer_get_compound_control(stm_se_audio_mixer_h    audio_mixer,
                                                        stm_se_ctrl_t                    ctrl,
                                                        void                             *value)
{
    Mixer_Mme_c *Mixer = (Mixer_Mme_c *) audio_mixer;
    PlayerStatus_t Status;
    SE_VERBOSE(group_api, "Mixer 0x%p ctrl %d\n", audio_mixer, ctrl);

    if (Mixer == NULL)
    {
        SE_ERROR("Invalid Mixer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = Mixer->GetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}

int __stm_se_audio_mixer_update_transformer_id(unsigned int mixerId,
                                               const char *transformerName)
{
    HavanaStatus_t                Status;
    SE_VERBOSE(group_api, "Mixer%d Transformer%s\n", mixerId, transformerName);
    if (mixerId >= MAX_MIXERS)
    {
        return -EINVAL;
    }

    Status = HavanaPlayer->UpdateMixerTransformerId(mixerId, transformerName);

    return HavanaStatusToErrno(Status);
}

int stm_se_audio_generator_new(const char *name,
                               stm_se_audio_generator_h *audio_generator)
{
    Audio_Generator_c            *Generator;
    HavanaStatus_t                Status;
    int                           Result;
    SE_INFO(group_api, "audio_generator %s\n", name);

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Status = HavanaPlayer->CreateAudioGenerator(name, &Generator);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(Status);
    }

    *audio_generator   = (stm_se_audio_generator_h)Generator;
    Result = stm_registry_add_instance(
                 STM_REGISTRY_INSTANCES,
                 &stm_se_audio_generator_type,
                 name,
                 *audio_generator);
    if (0 != Result)
    {
        SE_ERROR("Cannot register audio_generator instance %s (%d)\n", name, Result);
        // no propagation (see registry_init)
    }

    OS_Dump_MemCheckCounters(__func__);

    return Result;
}

int stm_se_audio_generator_delete(stm_se_audio_generator_h audio_generator)
{
    Audio_Generator_c        *Generator;
    HavanaStatus_t                Status;
    int                           Result;
    SE_INFO(group_api, "audio_generator 0x%p\n", audio_generator);

    if (audio_generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Generator = (Audio_Generator_c *)audio_generator;
    Status = HavanaPlayer->DeleteAudioGenerator(Generator);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(Status);
    }

    Result = stm_registry_remove_object(audio_generator);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister audio_generator 0x%p (%d)\n", audio_generator, Result);
    }

    OS_Dump_MemCheckCounters(__func__);

    return Result;
}
///{{{ audio generator attach to mixer.
int stm_se_audio_generator_attach(stm_se_audio_generator_h audio_generator,
                                  stm_se_audio_mixer_h audio_mixer)
{
    Audio_Generator_c           *Generator = (Audio_Generator_c *) audio_generator;
    Mixer_Mme_c                 *Mixer = (Mixer_Mme_c *) audio_mixer;
    stm_object_h                audio_mixer_type, audio_generator_type;
    int                         Result;
    PlayerStatus_t              Status;
    SE_INFO(group_api, "audio_generator 0x%p, audio_mixer 0x%p\n", Generator, Mixer);

    if (audio_generator == NULL)
    {
        SE_ERROR("audio_generator is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (Mixer == NULL)
    {
        SE_ERROR("audio_mixer is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Result = stm_registry_get_object_type(Mixer, &audio_mixer_type);
    if (Result != 0)
    {
        SE_ERROR("Unable to determine audio mixer object type (%d)\n", Result);
        return Result;
    }
    else if (audio_mixer_type != &stm_se_audio_mixer_type)
    {
        SE_ERROR("error audio_mixer_type\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Result = stm_registry_get_object_type(Generator, &audio_generator_type);
    if (Result != 0)
    {
        SE_ERROR("Unable to determine audio generator object type (%d)\n", Result);
        return Result;
    }
    else if (audio_generator_type != &stm_se_audio_generator_type)
    {
        SE_ERROR("error audio_generator_type\n");
        return HavanaStatusToErrno(HavanaError);
    }

    attach(Generator, Mixer);
    Status = HavanaPlayer->AttachGeneratorToMixer(Generator, Mixer);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}


//{{{  stm_se_audio_generator_detach
int stm_se_audio_generator_detach(stm_se_audio_generator_h audio_generator,
                                  stm_se_audio_mixer_h audio_mixer)
{
    Audio_Generator_c   *Generator = (Audio_Generator_c *) audio_generator;
    Mixer_Mme_c     *Mixer = (Mixer_Mme_c *) audio_mixer;
    stm_object_h        audio_mixer_type;
    PlayerStatus_t      Status;
    int         Result;
    SE_INFO(group_api, "audio_generator 0x%p, audio_mixer 0x%p\n", Generator, Mixer);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (Mixer == NULL)
    {
        SE_ERROR("audio_mixer is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Result = stm_registry_get_object_type(Mixer, &audio_mixer_type);
    if (Result != 0)
    {
        SE_ERROR("Unable to determine audio mixer object type (%d)\n", Result);
        return Result;
    }
    else if (audio_mixer_type != &stm_se_audio_mixer_type)
    {
        SE_ERROR("error audio_mixer_type\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = HavanaPlayer->DetachGeneratorFromMixer(Generator, Mixer);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    // remove connection in registry
    detach(Generator, Mixer);
    return 0;
}

//}}}
//{{{  stm_se_audio_generator_set_compound_control
int             stm_se_audio_generator_set_compound_control(stm_se_audio_generator_h audio_generator,
                                                            stm_se_ctrl_t                    ctrl,
                                                            const void                       *value)
{
    Audio_Generator_c   *Generator = (Audio_Generator_c *) audio_generator;
    SE_DEBUG(group_api, "audio_generator 0x%p ctrl %d\n", audio_generator, ctrl);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->SetCompoundOption(ctrl, value);
}
//}}}
//{{{  stm_se_audio_generator_get_compound_control
int             stm_se_audio_generator_get_compound_control(stm_se_audio_generator_h audio_generator,
                                                            stm_se_ctrl_t                    ctrl,
                                                            void                            *value)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_VERBOSE(group_api, "audio_generator 0x%p ctrl %d\n", audio_generator, ctrl);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->GetCompoundOption(ctrl, value);
}

int             stm_se_audio_generator_set_control(stm_se_audio_generator_h audio_generator,
                                                   stm_se_ctrl_t           ctrl,
                                                   int32_t                 value)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_DEBUG(group_api, "audio_generator 0x%p ctrl %d value %d\n", audio_generator, ctrl, value);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->SetOption(ctrl, value);
}

int             stm_se_audio_generator_get_control(stm_se_audio_generator_h audio_generator,
                                                   stm_se_ctrl_t            ctrl,
                                                   int32_t                 *value)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_VERBOSE(group_api, "audio_generator 0x%p ctrl %d\n", audio_generator, ctrl);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->GetOption(ctrl, value);
}


int             stm_se_audio_generator_get_info(stm_se_audio_generator_h    audio_generator,
                                                stm_se_audio_generator_info_t *info)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_VERBOSE(group_api, "audio_generator 0x%p\n", audio_generator);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (info == NULL)
    {
        SE_ERROR("Invalid info pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Generator->GetInfo(info);
    return 0;
}

int             stm_se_audio_generator_commit(stm_se_audio_generator_h    audio_generator,
                                              uint32_t number_of_samples)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_VERBOSE(group_api, "audio_generator 0x%p number_of_samples %d\n", audio_generator, number_of_samples);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->Commit(number_of_samples);
}


//{{{  stm_se_audio_generator_start
int             stm_se_audio_generator_start(stm_se_audio_generator_h audio_generator)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_DEBUG(group_api, "audio_generator 0x%p\n", audio_generator);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->Start();
}

//{{{  stm_se_audio_generator_stop
int             stm_se_audio_generator_stop(stm_se_audio_generator_h audio_generator)
{
    Audio_Generator_c *Generator = (Audio_Generator_c *) audio_generator;
    SE_DEBUG(group_api, "audio_generator 0x%p\n", audio_generator);

    if (Generator == NULL)
    {
        SE_ERROR("audio_generator is null - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    return Generator->Stop();
}

//}}}
//{{{  stm_se_audio_reader_new
int             stm_se_audio_reader_new(const char            *name,
                                        const char            *hw_name,
                                        stm_se_audio_reader_h *audio_reader)
{
    char              AudioReaderName[STM_REGISTRY_MAX_TAG_SIZE];
    Audio_Reader_c   *AudioReader;
    HavanaStatus_t    Status = HavanaNoError;
    int               Result;
    SE_INFO(group_api, "AudioReader: %s hw_name: %s\n", name, hw_name);

    if ((name == NULL) || (hw_name == NULL) || (audio_reader == NULL))
    {
        SE_ERROR("Invalid Parameters\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Status = HavanaPlayer->CreateAudioReader(name, hw_name, &AudioReader);
    if (Status != HavanaNoError)
    {
        SE_ERROR("Cannot create audio reader device '%s'\n", hw_name);
        return HavanaError;
    }

    *audio_reader = (stm_se_audio_reader_h) AudioReader;
    snprintf(AudioReaderName, sizeof(AudioReaderName), "Object@%s:%s", name, hw_name);
    AudioReaderName[sizeof(AudioReaderName) - 1] = '\0';

    Result = stm_registry_add_instance(
                 STM_REGISTRY_INSTANCES,
                 &stm_se_audio_reader_type,
                 AudioReaderName,
                 *audio_reader);
    if (0 != Result && -EEXIST != Result)
    {
        SE_ERROR("Cannot register stm_se_audio_reader_h instance %s (%d)\n", name, Result);
        // no propagation (see registry_init)
    }
    else
    {
        SE_DEBUG(group_api, "AudioReader:%s 0x%p created\n", name, audio_reader);
    }

    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(HavanaNoError);
}
//}}}
//{{{  stm_se_audio_reader_delete
int             stm_se_audio_reader_delete(stm_se_audio_reader_h          audio_reader)
{
    Audio_Reader_c   *AudioReader;
    int               Result;
    SE_INFO(group_api, "AudioReader 0x%p\n", audio_reader);

    if (audio_reader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    AudioReader = (Audio_Reader_c *)audio_reader;

    if (HavanaPlayer->DeleteAudioReader(AudioReader) == HavanaError)
    {
        return -EBUSY;
    }

    Result = stm_registry_remove_object(audio_reader);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister stm_se_audio_reader (%d)\n", Result);
        // no propagation (see registry_init)
    }
    else
    {
        SE_DEBUG(group_api, "AudioReader 0x%p deleted\n", audio_reader);
    }

    OS_Dump_MemCheckCounters(__func__);

    return HavanaStatusToErrno(HavanaNoError);
}
//}}}
//{{{  stm_se_audio_reader_attach
int             stm_se_audio_reader_attach(stm_se_audio_reader_h              audio_reader,
                                           stm_se_play_stream_h               play_stream)
{
    class HavanaStream_c *HavanaStream = (class HavanaStream_c *) play_stream;
    class Audio_Reader_c *AudioReader  = (class Audio_Reader_c *) audio_reader;
    HavanaStatus_t        Status = HavanaNoError;

    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (play_stream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    SE_INFO(group_api, "Stream 0x%p, AudioReader 0x%p\n", HavanaStream->GetStream(), audio_reader);

    Status = HavanaPlayer->AttachAudioReader(AudioReader, HavanaStream);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(Status);
    }

    attach(audio_reader, play_stream);
    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_audio_reader_detach
int             stm_se_audio_reader_detach(stm_se_audio_reader_h              audio_reader,
                                           stm_se_play_stream_h               play_stream)
{
    class HavanaStream_c *HavanaStream = (class HavanaStream_c *) play_stream;
    class Audio_Reader_c *AudioReader = (class Audio_Reader_c *) audio_reader;
    HavanaStatus_t        Status = HavanaNoError;

    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (play_stream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    SE_INFO(group_api, "Stream 0x%p, AudioReader 0x%p\n", HavanaStream->GetStream(), audio_reader);

    Status = HavanaPlayer->DetachAudioReader(AudioReader, HavanaStream);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(Status);
    }

    detach(audio_reader, play_stream);
    return HavanaStatusToErrno(Status);
}
//}}}
//{{{  stm_se_audio_reader_get_compound_control
int             stm_se_audio_reader_get_compound_control(stm_se_audio_reader_h    audio_reader,
                                                         stm_se_ctrl_t             ctrl,
                                                         void                      *value)
{
    Audio_Reader_c *AudioReader = (Audio_Reader_c *) audio_reader;
    PlayerStatus_t Status;
    SE_VERBOSE(group_api, "AudioReader 0x%p ctrl %d\n", audio_reader, ctrl);

    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = AudioReader->GetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_reader_set_compound_control
int             stm_se_audio_reader_set_compound_control(stm_se_audio_reader_h    audio_reader,
                                                         stm_se_ctrl_t             ctrl,
                                                         const void                *value)
{
    Audio_Reader_c *AudioReader = (Audio_Reader_c *) audio_reader;
    PlayerStatus_t Status;
    SE_DEBUG(group_api, "AudioReader 0x%p ctrl %d\n", audio_reader, ctrl);

    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = AudioReader->SetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_reader_get_control
int             stm_se_audio_reader_get_control(stm_se_audio_reader_h     audio_reader,
                                                stm_se_ctrl_t             ctrl,
                                                int32_t                  *value)
{
    Audio_Reader_c *AudioReader = (Audio_Reader_c *) audio_reader;
    SE_VERBOSE(group_api, "AudioReader 0x%p ctrl %d\n", audio_reader, ctrl);
    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }
    PlayerStatus_t Status = AudioReader->GetOption(ctrl, value);
    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }
    return 0;
}
//}}}
//{{{  stm_se_audio_reader_set_control
int             stm_se_audio_reader_set_control(stm_se_audio_reader_h     audio_reader,
                                                stm_se_ctrl_t             ctrl,
                                                const int32_t             value)
{
    Audio_Reader_c *AudioReader = (Audio_Reader_c *) audio_reader;
    SE_VERBOSE(group_api, "AudioReader 0x%p ctrl %d\n", audio_reader, ctrl);
    if (AudioReader == NULL)
    {
        SE_ERROR("Invalid audio_reader\n");
        return HavanaStatusToErrno(HavanaError);
    }
    PlayerStatus_t Status = AudioReader->SetOption(ctrl, value);
    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }
    return 0;
}
//}}}
//}}}
//{{{  stm_se_audio_player_new
int             stm_se_audio_player_new(const char           *name,
                                        const char              *hw_name,
                                        stm_se_audio_player_h   *audio_player)
{
    int Result;
    char audio_player_name[STM_REGISTRY_MAX_TAG_SIZE];
    PlayerStatus_t Status;
    Audio_Player_c *AudioPlayer;
    SE_INFO(group_api, "AudioPlayer: %s hw_name: %s\n", name, hw_name);

    if ((name == NULL) || (hw_name == NULL) || (audio_player == NULL))
    {
        SE_ERROR("Invalid Parameters\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    snprintf(audio_player_name, sizeof(audio_player_name), "Object@%s:%s", name, hw_name);
    audio_player_name[sizeof(audio_player_name) - 1] = '\0';

    Status = HavanaPlayer->CreateAudioPlayer(name, hw_name, &AudioPlayer);
    if (Status == HavanaNoError)
    {
        *audio_player       = (stm_se_audio_player_h)AudioPlayer;
    }

    Result = stm_registry_add_instance(
                 STM_REGISTRY_INSTANCES,
                 &stm_se_audio_player_type,
                 audio_player_name,
                 *audio_player);
    if (0 != Result)
    {
        SE_ERROR("Cannot register audio_player instance %s (%d)\n", name, Result);
        // no propagation (see registry_init)
    }
    else
    {
        SE_DEBUG(group_api, "AudioPlayer: %s 0x%p\n", name, AudioPlayer);
    }

    OS_Dump_MemCheckCounters(__func__);

    return 0;
}
//}}}
//{{{  stm_se_audio_player_delete
int             stm_se_audio_player_delete(stm_se_audio_player_h  audio_player)
{
    Audio_Player_c               *AudioPlayer;
    int                           Result;
    SE_INFO(group_api, "AudioPlayer 0x%p\n", audio_player);

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    AudioPlayer = (Audio_Player_c *)audio_player;

    if (HavanaPlayer->DeleteAudioPlayer(AudioPlayer) == HavanaError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    Result = stm_registry_remove_object(audio_player);
    if (0 != Result)
    {
        SE_ERROR("Cannot unregister stm_se_audio_player (%d)\n", Result);
        // no propagation (see registry_init)
    }
    else
    {
        SE_DEBUG(group_api, "AudioPlayer 0x%p deleted\n", audio_player);
    }

    OS_Dump_MemCheckCounters(__func__);

    return 0;
}
//}}}
//{{{  stm_se_audio_player_get_compound_control
int             stm_se_audio_player_get_compound_control(stm_se_audio_player_h    audio_player,
                                                         stm_se_ctrl_t             ctrl,
                                                         void                      *value)
{
    Audio_Player_c *aplayer = (Audio_Player_c *) audio_player;
    PlayerStatus_t Status;
    SE_VERBOSE(group_api, "AudioPlayer 0x%p ctrl %d\n", audio_player, ctrl);

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = aplayer->GetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_player_set_compound_control
int             stm_se_audio_player_set_compound_control(stm_se_audio_player_h    audio_player,
                                                         stm_se_ctrl_t             ctrl,
                                                         const void                *value)
{
    Audio_Player_c *aplayer = (Audio_Player_c *) audio_player;
    PlayerStatus_t Status;
    SE_DEBUG(group_api, "AudioPlayer 0x%p ctrl %d\n", audio_player, ctrl);

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = aplayer->SetCompoundOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_player_get_control
int             stm_se_audio_player_get_control(stm_se_audio_player_h    audio_player,
                                                stm_se_ctrl_t              ctrl,
                                                int32_t                   *value)
{
    Audio_Player_c *aplayer = (Audio_Player_c *) audio_player;
    PlayerStatus_t Status;
    SE_VERBOSE(group_api, "AudioPlayer 0x%p ctrl %d\n", audio_player, ctrl);

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = aplayer->GetOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_player_set_control
int             stm_se_audio_player_set_control(stm_se_audio_player_h    audio_player,
                                                stm_se_ctrl_t             ctrl,
                                                const int32_t             value)
{
    Audio_Player_c *aplayer = (Audio_Player_c *) audio_player;
    PlayerStatus_t Status;
    SE_DEBUG(group_api, "AudioPlayer 0x%p ctrl %d\n", audio_player, ctrl);

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Status = aplayer->SetOption(ctrl, value);

    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_mixer_attach
int             stm_se_audio_mixer_attach(stm_se_audio_mixer_h               audio_mixer,
                                          stm_se_audio_player_h               audio_player)
{
    Mixer_Mme_c                 *Mixer = (Mixer_Mme_c *) audio_mixer;
    Audio_Player_c              *Audio_Player = (Audio_Player_c *) audio_player;
    stm_object_h                audio_mixer_type, audio_player_type;
    int                         Result;
    PlayerStatus_t              Status;
    SE_INFO(group_api, "audio_mixer 0x%p, audio_player 0x%p\n", audio_mixer, audio_player);

    if (audio_mixer == NULL)
    {
        SE_ERROR("audio_mixer is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    Result = stm_registry_get_object_type(audio_mixer, &audio_mixer_type);
    if (Result != 0)
    {
        SE_ERROR("Unable to determine audio mixer object type (%d)\n", Result);
        return Result;
    }
    else if (audio_mixer_type != &stm_se_audio_mixer_type)
    {
        SE_ERROR("error audio_mixer_type\n");
        return HavanaStatusToErrno(HavanaError);
    }

    Result = stm_registry_get_object_type(audio_player, &audio_player_type);
    if (Result != 0)
    {
        SE_ERROR("Unable to determine audio player object type (%d)\n", Result);
        return Result;
    }
    else if (audio_player_type != &stm_se_audio_player_type)
    {
        SE_ERROR("error audio_player_type\n");
        return HavanaStatusToErrno(HavanaError);
    }

    attach(audio_mixer, audio_player);
    Status = HavanaPlayer->AttachPlayerToMixer(Mixer, Audio_Player);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_audio_mixer_detach
int             stm_se_audio_mixer_detach(stm_se_audio_mixer_h               audio_mixer,
                                          stm_se_audio_player_h               audio_player)
{
    Mixer_Mme_c                 *Mixer = (Mixer_Mme_c *) audio_mixer;
    Audio_Player_c              *Audio_Player = (Audio_Player_c *) audio_player;
    PlayerStatus_t              Status;
    SE_INFO(group_api, "audio_mixer 0x%p, audio_player 0x%p\n", audio_mixer, audio_player);

    if (audio_mixer == NULL)
    {
        SE_ERROR("audio_mixer is NULL - no action taken\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (audio_player == NULL)
    {
        SE_ERROR("Invalid audio_player\n");
        return HavanaStatusToErrno(HavanaError);
    }

    if (HavanaPlayer == NULL)
    {
        SE_ERROR("Havana player does not exist\n");
        return HavanaStatusToErrno(HavanaNotOpen);
    }

    // remove connection in registry
    detach(audio_mixer, audio_player);
    Status = HavanaPlayer->DetachPlayerFromMixer(Mixer, Audio_Player);

    if (Status != HavanaNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}
//{{{  stm_se_component_set_module_parameters
int             stm_se_component_set_module_parameters(component_handle_t  Component,
                                                       void               *Data,
                                                       unsigned int        Size)
{
    // this API shall eventually be removed: backdoor API to configure mixer from pseudo mixer
    // shall be replaced by use of stm_se_audio_mixer_set_control */
    Mixer_Mme_c *MixerComponent = (Mixer_Mme_c *)Component;

    SE_DEBUG(group_api, "\n");

    if (MixerComponent == NULL)
    {
        SE_ERROR("Invalid Component\n");
        return HavanaStatusToErrno(HavanaComponentInvalid);
    }

    PlayerStatus_t Status = MixerComponent->SetModuleParameters(Size, Data);
    if (Status != PlayerNoError)
    {
        return HavanaStatusToErrno(HavanaError);
    }

    return 0;
}
//}}}

//{{{  stm_se_play_stream_register_buffer_capture_callback
stream_buffer_capture_callback stm_se_play_stream_register_buffer_capture_callback(stm_se_play_stream_h            Stream,
                                                                                   stm_se_event_context_h          Context,
                                                                                   stream_buffer_capture_callback  Callback)
{
    // this api is still in used by non stlinuxtv users => using manifestor_video_grab
    // for use of in kernel cbk; => would be better to modify manifestor
    // with removal of this cbk, HavanaCapture, manifestor_audio_grab, manifestor_video_grab could be removed
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)Stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("[!!!DEPRECATED!!!] Invalid Stream\n");
        return NULL;
    }

    SE_INFO(group_api, "[!!!DEPRECATED!!!] Stream 0x%p Callback 0x%p\n", HavanaStream->GetStream(), Callback);

    return HavanaStream->RegisterBufferCaptureCallback(Context, Callback);
}
//}}}

//{{{  stm_se_play_stream_get_compound_control
int           stm_se_play_stream_get_compound_control(stm_se_play_stream_h      play_stream,
                                                      stm_se_ctrl_t               ctrl,
                                                      void                        *value)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    SE_VERBOSE(group_api, "Stream 0x%p ctrl %d\n", HavanaStream->GetStream(), ctrl);

    return HavanaStatusToErrno(HavanaStream->GetCompoundControl(ctrl, value));
}
//}}}
//{{{  stm_se_play_stream_set_compound_control
int           stm_se_play_stream_set_compound_control(stm_se_play_stream_h      play_stream,
                                                      stm_se_ctrl_t               ctrl,
                                                      void                        *value)
{
    class HavanaStream_c       *HavanaStream    = (class HavanaStream_c *)play_stream;

    if (HavanaStream == NULL)
    {
        SE_ERROR("Invalid Stream\n");
        return HavanaStatusToErrno(HavanaStreamInvalid);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return HavanaStatusToErrno(HavanaError);
    }

    SE_DEBUG(group_api, "Stream 0x%p ctrl %d\n", HavanaStream->GetStream(), ctrl);

    return HavanaStatusToErrno(HavanaStream->SetCompoundControl(ctrl, value));
}
//}}}

// Encoder Utility Functions
//{{{ stm_se_encode_encoding2media
static stm_se_encode_stream_media_t stm_se_encode_encoding2media(stm_se_encode_stream_encoding_t encoding)
{
    if (STM_SE_ENCODE_STREAM_ENCODING_AUDIO_FIRST <= encoding &&
        encoding <= STM_SE_ENCODE_STREAM_ENCODING_AUDIO_LAST)
    {
        return STM_SE_ENCODE_STREAM_MEDIA_AUDIO;
    }

    if (STM_SE_ENCODE_STREAM_ENCODING_VIDEO_FIRST <= encoding &&
        encoding <= STM_SE_ENCODE_STREAM_ENCODING_VIDEO_LAST)
    {
        return STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    }

    SE_ERROR("unknown encoding %d\n", encoding);
    return STM_SE_ENCODE_STREAM_MEDIA_AUDIO; // we have to return something
}
//}}}
// Encode
//{{{ stm_se_encode_new
int             stm_se_encode_new(const char                      *name,
                                  stm_se_encode_h                 *encode)
{
    EncoderStatus_t             Status;
    Encode_t                    Encode = NULL;
    SE_DEBUG(group_api, ">Encode:%s\n", name);

    if ((name == NULL) || (encode == NULL))
    {
        SE_ERROR("Invalid Parameters\n");
        return EncoderStatusToErrno(EncoderError);
    }

    if (Encoder == NULL)
    {
        SE_ERROR("Encoder does not exist\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    Status      = Encoder->CreateEncode(&Encode);

    if (Status == EncoderNoError)
    {
        int                     Result = 0;
        // Return the Encode object in the handle provided
        *encode       = (stm_se_encode_h)Encode;
        Result = stm_registry_add_instance(STM_REGISTRY_INSTANCES, &stm_se_encode_type, name, Encode);
        if (0 != Result)
        {
            SE_ERROR("Cannot register stm_se_encoding instance (%d)\n", Result);
            // no propagation (see registry_init)
        }

        Result = EncodeCreateSysfs(name, Encode);
        if (0 != Result)
        {
            SE_ERROR("EncodeCreateSysFs() failed\n");
        }
    }

    SE_INFO(group_api, "<Encode:%s 0x%p status %d\n", name, Encode, Status);
    OS_Dump_MemCheckCounters(__func__);

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_delete(stm_se_encode_h                  encode)
{
    EncoderStatus_t             Status;
    int                         Result = 0;
    Encode_t                    Encode = (Encode_t) encode;
    SE_DEBUG(group_api, ">Encode 0x%p\n", encode);

    if (Encode == NULL)
    {
        SE_ERROR("Invalid Encode Handle\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    if (Encoder == NULL)
    {
        SE_ERROR("Encoder does not exist\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    Result = EncodeTerminateSysfs(Encode);

    if (0 != Result)
    {
        SE_ERROR("Failed to remove Encode structures from Sysfs\n");
    }

    Status = Encoder->TerminateEncode(Encode);

    if (Status == EncoderNoError)
    {
        Result = stm_registry_remove_object(Encode);
        if (0 != Result)
        {
            SE_ERROR("Failed to remove Encode from the registry (%d)\n", Result);
        }
    }
    else
    {
        SE_ERROR("Failed to Terminate Encode (%d)\n", Status);
    }

    SE_INFO(group_api, "<Encode 0x%p status %d\n", encode, Status);
    OS_Dump_MemCheckCounters(__func__);

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_set_control(stm_se_encode_h                  encode,
                                          stm_se_ctrl_t                    ctrl,
                                          int32_t                          value)
{
    EncoderStatus_t            Status;
    Encode_t                   Encode = (Encode_t)encode;

    SE_DEBUG(group_api, ">Encode 0x%p ctrl %d value %d\n", encode, ctrl, value);

    if (Encode == NULL)
    {
        SE_ERROR("Invalid Encode Handle\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    Status = Encode->SetControl(ctrl, &value);

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_get_control(stm_se_encode_h                  encode,
                                          stm_se_ctrl_t                    ctrl,
                                          int32_t                         *value)
{
    EncoderStatus_t            Status;
    Encode_t                   Encode = (Encode_t)encode;

    SE_VERBOSE(group_api, "Encode 0x%p ctrl %d\n", encode, ctrl);

    if (Encode == NULL)
    {
        SE_ERROR("Invalid Encode Handle\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return EncoderStatusToErrno(EncoderError);
    }

    Status = Encode->GetControl(ctrl, (void *) value);

    return EncoderStatusToErrno(Status);
}
//}}}
// Encode Stream
//{{{
int             stm_se_encode_stream_new(const char                      *name,
                                         stm_se_encode_h                  encode,
                                         stm_se_encode_stream_encoding_t  encoding,
                                         stm_se_encode_stream_h          *encode_stream)
{
    EncoderStatus_t             Status;
    class Encode_c             *Encode = (class Encode_c *)encode;
    class EncodeStream_c       *EncodeStream;
    SE_DEBUG(group_api, ">Encode 0x%p Stream %s encoding %d\n", encode, name, encoding);

    if ((name == NULL) || (encode == NULL) || (encode_stream == NULL))
    {
        SE_ERROR("Invalid Parameters\n");
        return EncoderStatusToErrno(EncoderError);
    }

    if (Encoder == NULL)
    {
        SE_ERROR("Encoder does not exist\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    Status      = Encoder->CreateEncodeStream(&EncodeStream, Encode, stm_se_encode_encoding2media(encoding), encoding);

    if (Status == EncoderNoError)
    {
        int                     Result = 0;
        // Return the Encode Stream object in the handle provided
        *encode_stream = (stm_se_encode_stream_h)EncodeStream;
        // Add the object to the registry.
        Result = stm_registry_add_instance(Encode, &stm_se_encode_stream_type, name, EncodeStream);
        if (0 != Result)
        {
            SE_ERROR("Cannot register stm_se_encode_stream instance (%d)\n", Result);
            // no propagation (see registry_init)
        }

        Result = EncodeStreamCreateSysfs(name, Encode, stm_se_encode_encoding2media(encoding), EncodeStream);

        if (0 != Result)
        {
            SE_ERROR("EncodeCreateSysFs() failed\n");
        }
    }

    SE_INFO(group_api, "<Encode 0x%p Stream %s 0x%p status %d\n", encode, name, EncodeStream, Status);
    OS_Dump_MemCheckCounters(__func__);

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_delete(stm_se_encode_stream_h           encode_stream)
{
    EncoderStatus_t             Status;
    int                         Result = 0;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    Encode_t                    Encode;
    SE_INFO(group_api, ">Stream 0x%p\n", encode_stream);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (Encoder == NULL)
    {
        SE_ERROR("Encoder does not exist\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    Result = stm_registry_remove_object(EncodeStream);
    if (0 != Result)
    {
        SE_ERROR("Failed to remove Encode Stream from the registry (%d)\n", Result);
        return EncoderStatusToErrno(EncoderBusy);
    }

    // Obtain the handle for the Encode Object that this stream belongs to.
    EncodeStream->GetClassList(&Encode);

    /* get statistics before removing the stream */
    Result = WrapperGetEncodeStatisticsForSysfs(Encode, EncodeStream);
    if (0 != Result)
    {
        SE_ERROR("WrapperGetEncodeStatisticsForSysfs() failed\n");
    }

    Result = EncodeStreamTerminateSysfs(Encode, EncodeStream);
    if (0 != Result)
    {
        SE_ERROR("Failed to remove Encode Stream structures from Sysfs (%d)\n", Result);
    }

    Status = Encoder->TerminateEncodeStream(Encode, EncodeStream);
    if (Status != EncoderNoError)
    {
        SE_ERROR("Failed to terminate Encode Stream (%d)\n", Status);
    }

    SE_INFO(group_api, "<Encode 0x%p Stream 0x%p status %d\n", Encode, encode_stream, Status);
    OS_Dump_MemCheckCounters(__func__);

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_attach(stm_se_encode_stream_h           encode_stream,
                                            stm_object_h                     sink)
{
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_INFO(group_api, "Stream 0x%p sink 0x%p\n", encode_stream, sink);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    //
    // Stream can only be connected once
    //
    if (EncodeStream->AddTransport(sink) != EncoderNoError)
    {
        SE_ERROR("Failed to attach Encode Stream 0x%p to sink 0x%p\n",
                 encode_stream, sink);
        return EncoderStatusToErrno(EncoderError);
    }

    // Add connection in the registry
    attach(encode_stream, sink);
    return EncoderStatusToErrno(EncoderNoError);
}
//}}}
//{{{
int             stm_se_encode_stream_detach(stm_se_encode_stream_h           encode_stream,
                                            stm_object_h                     sink)
{
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_INFO(group_api, "Stream 0x%p sink 0x%p\n", encode_stream, sink);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (EncodeStream->RemoveTransport(sink) != EncoderNoError)
    {
        SE_ERROR("encode stream 0x%p not attached to sink 0x%p\n",
                 encode_stream, sink);
        return EncoderStatusToErrno(EncoderError);
    }

    // Remove connection from the registry
    detach(encode_stream, sink);
    return EncoderStatusToErrno(EncoderNoError);
}
//}}}
//{{{
int             stm_se_encode_stream_get_control(stm_se_encode_stream_h           encode_stream,
                                                 stm_se_ctrl_t                    ctrl,
                                                 int32_t                         *value)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_VERBOSE(group_api, "Stream 0x%p ctrl %d\n", encode_stream, ctrl);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return EncoderStatusToErrno(EncoderError);
    }

    Status = EncodeStream->GetControl(ctrl, value);
    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_set_control(stm_se_encode_stream_h           encode_stream,
                                                 stm_se_ctrl_t                    ctrl,
                                                 int32_t                          value)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_DEBUG(group_api, "Stream 0x%p ctrl %d value %d\n", encode_stream, ctrl, value);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    Status = EncodeStream->SetControl(ctrl, &value);
    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_get_compound_control(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_ctrl_t                    ctrl,
                                                          void                            *value)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_VERBOSE(group_api, "Stream 0x%p ctrl %d\n", encode_stream, ctrl);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return EncoderStatusToErrno(EncoderError);
    }

    Status = EncodeStream->GetCompoundControl(ctrl, value);
    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_set_compound_control(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_ctrl_t                    ctrl,
                                                          const void                      *value)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_DEBUG(group_api, "Stream 0x%p ctrl %d\n", encode_stream, ctrl);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (value == NULL)
    {
        SE_ERROR("Invalid Value pointer\n");
        return EncoderStatusToErrno(EncoderError);
    }

    Status = EncodeStream->SetCompoundControl(ctrl, value);
    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_drain(stm_se_encode_stream_h           encode_stream,
                                           bool                             discard)
{
    return EncoderStatusToErrno(EncoderImplementationError);
}
//}}}
//{{{
int             stm_se_encode_stream_inject_frame(stm_se_encode_stream_h           encode_stream,
                                                  const void                      *frame_virtual_address,
                                                  unsigned long                    frame_physical_address,
                                                  uint32_t                         frame_length,
                                                  const stm_se_uncompressed_frame_metadata_t metadata)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_VERBOSE(group_api, "Stream 0x%p frame_length %d\n", encode_stream, frame_length);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (Encoder == NULL)
    {
        SE_ERROR("Encoder does not exist\n");
        return EncoderStatusToErrno(EncoderNotOpen);
    }

    if (IsLowPowerState)
    {
        SE_ERROR("SE device is in low power\n");
        return EncoderStatusToErrno(EncoderBusy);
    }

    Status = Encoder->InputData(EncodeStream, frame_virtual_address, frame_physical_address, frame_length, &metadata);

    if (Status != EncoderNoError)
    {
        SE_ERROR("Failed to inject frame into Encode Stream 0x%p\n", EncodeStream);
    }

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{
int             stm_se_encode_stream_inject_discontinuity(stm_se_encode_stream_h    encode_stream,
                                                          stm_se_discontinuity_t    discontinuity)
{
    EncoderStatus_t             Status;
    EncodeStream_t              EncodeStream = (EncodeStream_t)encode_stream;
    SE_INFO(group_api, "Stream 0x%p discontinuity %d\n", encode_stream, discontinuity);

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (IsLowPowerState)
    {
        SE_ERROR("SE device is in low power\n");
        return EncoderStatusToErrno(EncoderBusy);
    }

    Status = EncodeStream->InjectDiscontinuity(discontinuity);

    if (Status != EncoderNoError)
    {
        SE_ERROR("Failed to inject discontinuity into Encode Stream 0x%p\n", EncodeStream);
    }

    return EncoderStatusToErrno(Status);
}
//}}}
//{{{  __stm_se_encode_stream_reset_statistics
int             __stm_se_encode_stream_reset_statistics(stm_se_encode_stream_h    encode_stream)
{
    EncodeStream_t EncodeStream = (EncodeStream_t)encode_stream;

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    return EncoderStatusToErrno(EncodeStream->ResetStatistics());
}
//}}}
//{{{  __stm_se_encode_stream_get_statistics
int             __stm_se_encode_stream_get_statistics(stm_se_encode_stream_h encode_stream,
                                                      encode_stream_statistics_t *Statistics)
{
    EncodeStream_t EncodeStream = (EncodeStream_t)encode_stream;

    if (EncodeStream == NULL)
    {
        SE_ERROR("Invalid Stream Handle\n");
        return EncoderStatusToErrno(EncoderUnknownStream);
    }

    if (Statistics == NULL)
    {
        SE_ERROR("Invalid Statistics pointer\n");
        return EncoderStatusToErrno(EncoderError);
    }

    return EncoderStatusToErrno(EncodeStream->GetStatistics(Statistics));
}
//}}}
#ifdef __cplusplus
}
#endif
//{{{  C++ operators
#if __KERNEL__
#include "osinline.h"

////////////////////////////////////////////////////////////////////////////
// operator new
////////////////////////////////////////////////////////////////////////////

void *operator new(unsigned int size)
{
    return __builtin_new(size);
}

////////////////////////////////////////////////////////////////////////////
// operator delete
////////////////////////////////////////////////////////////////////////////

void operator delete(void *mem)
{
    __builtin_delete(mem);
}

////////////////////////////////////////////////////////////////////////////
// operator new
////////////////////////////////////////////////////////////////////////////

void *operator new[](unsigned int size)
{
    return __builtin_vec_new(size);
}

////////////////////////////////////////////////////////////////////////////
//   operator delete
////////////////////////////////////////////////////////////////////////////

void operator delete[](void *mem)
{
    __builtin_vec_delete(mem);
}
#endif
//}}}

////////////////////////////////////////////////////////////////////////////
//   power management internals
////////////////////////////////////////////////////////////////////////////

// API function to be used for HPS/CPS enter
// low_power_mode: indicates the standby mode (HPS or CPS)
//                 => for CPS, all active MME transformers must be terminated
int __stm_se_pm_low_power_enter(__stm_se_low_power_mode_t low_power_mode)
{
    HavanaStatus_t  HavanaStatus;
    EncoderStatus_t EncoderStatus;
    SE_INFO(group_api, "low_power_mode %d\n", low_power_mode);

    if (IsLowPowerState)
    {
        // Nothing to do: SE is already in low power state !
        return HavanaNoError;
    }

    // Call HavanaPlayer method
    if (HavanaPlayer != NULL)
    {
        HavanaStatus = HavanaPlayer->LowPowerEnter(low_power_mode);

        if (HavanaStatus != HavanaNoError)
        {
            return HavanaStatusToErrno(HavanaStatus);
        }
    }

    // Call Encoder method
    if (Encoder != NULL)
    {
        EncoderStatus = Encoder->LowPowerEnter(low_power_mode);

        if (EncoderStatus != EncoderNoError)
        {
            return EncoderStatusToErrno(EncoderStatus);
        }
    }

    // Save new low power state
    IsLowPowerState = true;
    return HavanaNoError;
}

// API function to be used for HPS/CPS exit
int __stm_se_pm_low_power_exit(void)
{
    HavanaStatus_t  HavanaStatus;
    EncoderStatus_t EncoderStatus;
    SE_INFO(group_api, "\n");

    if (!IsLowPowerState)
    {
        // Nothing to do: SE is not in low power state !
        return HavanaNoError;
    }

    // Call HavanaPlayer method
    if (HavanaPlayer != NULL)
    {
        HavanaStatus = HavanaPlayer->LowPowerExit();

        if (HavanaStatus != HavanaNoError)
        {
            return HavanaStatusToErrno(HavanaStatus);
        }
    }

    // Call Encoder method
    if (Encoder != NULL)
    {
        EncoderStatus = Encoder->LowPowerExit();

        if (EncoderStatus != EncoderNoError)
        {
            return EncoderStatusToErrno(EncoderStatus);
        }
    }

    // Save new low power state
    IsLowPowerState = false;
    return HavanaNoError;
}
