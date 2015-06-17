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

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudio_c
///
/// Audio specific code to assist operation of the AUDIO_DECODER transformer
/// provided by the audio firmware.
///

#include "codec_mme_audio.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_audio.h"
#include "manifestor_encode_audio.h"
#include "manifestor_audio_sourceGrab.h"
#include "havana_stream.h"

#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudio_c"

#define BUFFER_TRANSCODED_FRAME_BUFFER        "TranscodedFrameBuffer"
#define BUFFER_TRANSCODED_FRAME_BUFFER_TYPE   {BUFFER_TRANSCODED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t            InitialTranscodedFrameBufferDescriptor = BUFFER_TRANSCODED_FRAME_BUFFER_TYPE;

#define BUFFER_COMPRESSED_FRAME_BUFFER        "CompressedFrameBuffer"
#define BUFFER_COMPRESSED_FRAME_BUFFER_TYPE   {BUFFER_COMPRESSED_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t            InitialCompressedFrameBufferDescriptor = BUFFER_COMPRESSED_FRAME_BUFFER_TYPE;

#define BUFFER_AUX_FRAME_BUFFER        "AuxFrameBuffer"
#define BUFFER_AUX_FRAME_BUFFER_TYPE   {BUFFER_AUX_FRAME_BUFFER, BufferDataTypeBase, AllocateFromSuppliedBlock, 4, 64, false, false, 0}

static BufferDataDescriptor_t          InitialAuxFrameBufferDescriptor = BUFFER_AUX_FRAME_BUFFER_TYPE;

const int ACC_AcMode2ChannelCountLUT[] =
{
    2, //    ACC_MODE20t,           /*  0 */
    1, //    ACC_MODE10,            /*  1 */
    2, //    ACC_MODE20,            /*  2 */
    3, //    ACC_MODE30,            /*  3 */
    3, //    ACC_MODE21,            /*  4 */
    4, //    ACC_MODE31,            /*  5 */
    4, //    ACC_MODE22,            /*  6 */
    5, //    ACC_MODE32,            /*  7 */
    5, //    ACC_MODE23,            /*  8 */
    6, //    ACC_MODE33,            /*  9 */
    6, //    ACC_MODE24,            /*  A */
    7, //    ACC_MODE34,            /*  B */
    6, //    ACC_MODE42,            /*  C */
    8, //    ACC_MODE44,            /*  D */
    7, //    ACC_MODE52,            /*  E */
    8, //    ACC_MODE53,            /*  F */
};

///
Codec_MmeAudio_c::Codec_MmeAudio_c(void)
    : Codec_MmeBase_c()
    , DefaultAudioSurfaceDescriptor()
    , AudioOutputSurface(NULL)
    , ParsedAudioParameters(NULL)
    , AudioParametersEvents()
    , actualDecAudioMode(ACC_MODE_ID)
    , SelectedChannel(ChannelSelectStereo)
    , DRC()
    , OutmodeMain(ACC_MODE_ID) // output raw decoded data by default
    , OutmodeAux(ACC_MODE_ID)
    , StreamDrivenDownmix(false)
    , RawAudioSamplingFrequency(48000) //Set default sampling frequency = 48000K
    , RawAudioNbChannels(2) // Set default NbChannels = 2
    , RelayfsIndex(0)
    , CurrentTranscodeBufferIndex(0)
    , TranscodedBuffers()
    , CurrentTranscodeBuffer(NULL)
    , TranscodeEnable(false)
    , TranscodeNeeded(false)
    , CompressedFrameNeeded(false)
    , TranscodedFrameMemoryDevice(0)
    , TranscodedFramePool(NULL)
    , TranscodedFrameMemory()
    , TranscodedFrameBufferDescriptor(NULL)
    , TranscodedFrameBufferType()
    , CurrentAuxBufferIndex(0)
    , AuxBuffers()
    , CurrentAuxBuffer(NULL)
    , AuxOutputEnable(false)
    , AuxFrameMemoryDevice(0)
    , AuxFramePool(NULL)
    , AuxFrameMemory()
    , AuxFrameBufferDescriptor(NULL)
    , AuxFrameBufferType()
    , CurrentCompressedFrameBufferIndex()
    , CompressedFrameBuffers()
    , CurrentCompressedFrameBuffer()
    , NoOfCompressedFrameBuffers(0)
    , CompressedFrameEnable(false)
    , CompressedFrameMemoryDevice(0)
    , CompressedFramePool(NULL)
    , CompressedFrameMemory()
    , CompressedFrameBufferDescriptor(NULL)
    , CompressedFrameBufferType()
    , ProtectTransformName(false)
    , TransformName()
    , AudioDecoderTransformCapability()
    , AudioDecoderTransformCapabilityMask()
    , AudioDecoderInitializationParameters()
    , AudioDecoderStatus()
    , mTempoRequestedRatio(0)
    , mTempoSetRatio(0)
{
    SetGroupTrace(group_decoder_audio);

    RelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0);

    Configuration.CodecName = "unknown audio";

    strncpy(Configuration.TranscodedMemoryPartitionName, "aud-transcoded", sizeof(Configuration.TranscodedMemoryPartitionName));
    Configuration.TranscodedMemoryPartitionName[sizeof(Configuration.TranscodedMemoryPartitionName) - 1] = '\0';
    strncpy(Configuration.AncillaryMemoryPartitionName, "aud-codec-data", sizeof(Configuration.AncillaryMemoryPartitionName));
    Configuration.AncillaryMemoryPartitionName[sizeof(Configuration.AncillaryMemoryPartitionName) - 1] = '\0';

    Configuration.StreamParameterContextCount           = 0;
    Configuration.StreamParameterContextDescriptor      = 0;
    Configuration.DecodeContextCount                    = 0;
    Configuration.DecodeContextDescriptor               = 0;
    Configuration.MaxDecodeIndicesPerBuffer             = 2;
    Configuration.SliceDecodePermitted                  = false;
    Configuration.AvailableTransformers                 = AUDIO_CODEC_MAX_TRANSFORMERS;
    Configuration.TransformName[0]                      = AUDIO_DECODER_TRANSFORMER_NAME0;
    Configuration.TransformName[1]                      = AUDIO_DECODER_TRANSFORMER_NAME1;
    Configuration.SizeOfTransformCapabilityStructure    = sizeof(AudioDecoderTransformCapability);
    Configuration.TransformCapabilityStructurePointer   = (void *)(&AudioDecoderTransformCapability);
    Configuration.AddressingMode                        = CachedAddress;
    Configuration.ShrinkCodedDataBuffersAfterDecode     = false;

    // Fill in surface descriptor details with our assumed defaults (copied from ManifestorAudio_c)
    DefaultAudioSurfaceDescriptor.StreamType               = StreamTypeAudio;
    DefaultAudioSurfaceDescriptor.ClockPullingAvailable    = true;
    DefaultAudioSurfaceDescriptor.MasterCapable            = true;
    DefaultAudioSurfaceDescriptor.BitsPerSample            = 32;
    DefaultAudioSurfaceDescriptor.ChannelCount             = 8;
    DefaultAudioSurfaceDescriptor.SampleRateHz             = 0;
    DefaultAudioSurfaceDescriptor.FrameRate                = 1; // rational

    AudioParametersEvents.audio_coding_type  = STM_SE_STREAM_ENCODING_AUDIO_NONE;
}

Codec_MmeAudio_c::~Codec_MmeAudio_c()
{
    if (CurrentTranscodeBuffer)
    {
        CurrentTranscodeBuffer->DecrementReferenceCount();
    }

    //
    // Release the coded frame buffer pool
    //

    if (TranscodedFramePool != NULL)
    {
        BufferManager->DestroyPool(TranscodedFramePool);
    }

    AllocatorClose(&TranscodedFrameMemoryDevice);

    // Release the Aux buffers if they are not currectly attached to anything.

    if (CurrentAuxBuffer)
    {
        CurrentAuxBuffer->DecrementReferenceCount();
    }

    // Release the Aux frame buffer pool
    if (AuxFramePool != NULL)
    {
        BufferManager->DestroyPool(AuxFramePool);
    }

    AllocatorClose(&AuxFrameMemoryDevice);

    //
    // Release the compressed frame buffer if its not been attached to anything.
    //
    for (int32_t i = 0; i < AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES; i++)
    {
        if (CurrentCompressedFrameBuffer[i])
        {
            CurrentCompressedFrameBuffer[i]->DecrementReferenceCount();
        }
    }

    //
    // Release the compressed frame buffer pool
    //

    if (CompressedFramePool != NULL)
    {
        BufferManager->DestroyPool(CompressedFramePool);
    }

    AllocatorClose(&CompressedFrameMemoryDevice);
}

//
CodecStatus_t   Codec_MmeAudio_c::Halt(void)
{
    AudioOutputSurface          = NULL;
    ParsedAudioParameters       = NULL;
    SelectedChannel             = ChannelSelectStereo;

    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_DECODED_AUDIO_AUX_BUFFER0, RelayfsIndex);

    return Codec_MmeBase_c::Halt();
}

// /////////////////////////////////////////////////////////////////////////
//
//      SetModuleParameters function
//      Action  : Allows external user to set up important environmental parameters
//      Input   :
//      Output  :
//      Result  :
//

CodecStatus_t   Codec_MmeAudio_c::SetModuleParameters(
    unsigned int   ParameterBlockSize,
    void          *ParameterBlock)
{
    struct CodecParameterBlock_s *CodecParameterBlock = (struct CodecParameterBlock_s *)ParameterBlock;

    if (ParameterBlockSize != sizeof(struct CodecParameterBlock_s))
    {
        SE_ERROR("Invalid parameter block\n");
        return CodecError;
    }

    // CodecSpecifyDownmix: received from the mixer to specify StreamDrivenDownmix
    if (CodecParameterBlock->ParameterType == CodecSpecifyDownmix)
    {
        if (StreamDrivenDownmix != CodecParameterBlock->Downmix.StreamDrivenDownmix)
        {
            StreamDrivenDownmix  = CodecParameterBlock->Downmix.StreamDrivenDownmix;
            SE_DEBUG(group_decoder_audio, "Setting StreamDrivenDownmix to %d\n", StreamDrivenDownmix);
            ForceStreamParameterReload      = true;
        }

        return CodecNoError;
    }

    return Codec_MmeBase_c::SetModuleParameters(ParameterBlockSize, ParameterBlock);
}


////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command IOs
///  Populate DecodeContext with 1 Input and 1 output Buffer
///  Populate I/O MME_DataBuffers
//
//   This function must be mutex-locked by caller
//
void Codec_MmeAudio_c::PresetIOBuffers(void)
{
    CodecBufferState_t      *State;
    OS_AssertMutexHeld(&Lock);
    // plumbing
    DecodeContext->MMEBufferList[0] = &DecodeContext->MMEBuffers[0];
    DecodeContext->MMEBufferList[1] = &DecodeContext->MMEBuffers[1];

    for (int i = 0; i < 2; ++i)
    {
        DecodeContext->MMEBufferList[i] = &DecodeContext->MMEBuffers[i];
        memset(&DecodeContext->MMEBuffers[i], 0, sizeof(MME_DataBuffer_t));
        DecodeContext->MMEBuffers[i].StructSize           = sizeof(MME_DataBuffer_t);
        DecodeContext->MMEBuffers[i].NumberOfScatterPages = 1;
        DecodeContext->MMEBuffers[i].ScatterPages_p       = &DecodeContext->MMEPages[i];
        memset(&DecodeContext->MMEPages[i], 0, sizeof(MME_ScatterPage_t));
    }

    // input
    DecodeContext->MMEBuffers[0].TotalSize = CodedDataLength;
    DecodeContext->MMEPages[0].Page_p      = CodedData;
    DecodeContext->MMEPages[0].Size        = CodedDataLength;
    // output
    State   = &BufferState[CurrentDecodeBufferIndex];
    DecodeContext->MMEBuffers[1].TotalSize = Stream->GetDecodeBufferManager()->ComponentSize(State->Buffer, PrimaryManifestationComponent);
    DecodeContext->MMEPages[1].Page_p      = Stream->GetDecodeBufferManager()->ComponentBaseAddress(State->Buffer, PrimaryManifestationComponent);
    DecodeContext->MMEPages[1].Size        = Stream->GetDecodeBufferManager()->ComponentSize(State->Buffer, PrimaryManifestationComponent);
}

////////////////////////////////////////////////////////////////////////////
///
///  Set Default FrameBase style TRANSFORM command for AudioDecoder MT
///  with 1 Input Buffer and 1 Output Buffer.
//
//   This function must be mutex-locked by caller
//
void Codec_MmeAudio_c::SetCommandIO(void)
{
    OS_AssertMutexHeld(&Lock);
    PresetIOBuffers();
    // FrameBase Transformer :: 1 Input Buffer / 1 Output Buffer sent through same MME_TRANSFORM
    DecodeContext->MMECommand.NumberInputBuffers  = 1;
    DecodeContext->MMECommand.NumberOutputBuffers = 1;
    DecodeContext->MMECommand.DataBuffers_p = DecodeContext->MMEBufferList;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Connect output port function
//

CodecStatus_t   Codec_MmeAudio_c::Connect(Port_c *Port)
{
//OutputSurfaceDescriptor_t       **ListOfSurfaceParameterPointers;
    //
    // Define the decode buffer elements -
    // Different from video as handle capabilities is done in audio
    // before a decode buffer manager exists (during construction).
    //
    Stream->GetDecodeBufferManager()->FillOutDefaultList(FormatAudio,
                                                         PrimaryManifestationElement,
                                                         Configuration.ListOfDecodeBufferComponents);

    AudioOutputSurface  = &DefaultAudioSurfaceDescriptor;
    if (AudioOutputSurface == NULL)
    {
        SE_ERROR("(%s) - Failed to get output surface parameters\n", Configuration.CodecName);
        return CodecError;
    }

    //
    // Perform the standard operations
    //
    CodecStatus_t Status = Codec_MmeBase_c::Connect(Port);
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // To support audio with switch stream, we force reload of stream parameters
    //
    ForceStreamParameterReload  = true;
    //
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
// Set the Codec Parameters that have been updated since last frame
//

void   Codec_MmeAudio_c::UpdateConfig(unsigned int Update)
{

    // NEW_MUTE_CONFIG doesn't require params update
    // because it is checked upon every FillOutDecodeCommand
    if (Update == NEW_MUTE_CONFIG)
    {
        return;
    }

    ForceStreamParameterReload = true;

    // The following config update only require to trigger the Reload of Parameters
    // NEW_EMPHASIS_CONFIG
    // NEW_DUALMONO_CONFIG
    // NEW_STEREO_CONFIG
    // NEW_DRC_CONFIG
    //

    if (Update & NEW_NBCHAN_CONFIG)
    {
        PlayerStatus_t      PlayerStatus;
        PlayerStatus = Player->GetControl(Playback,
                                          Stream,
                                          STM_SE_CTRL_PLAY_STREAM_NBCHANNELS,
                                          &RawAudioNbChannels);
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Failed to get NB_CHANNELS control (%08x)\n", PlayerStatus);
        }
    }

    if (Update & NEW_SAMPLING_FREQUENCY_CONFIG)
    {
        PlayerStatus_t      PlayerStatus;
        PlayerStatus = Player->GetControl(Playback,
                                          Stream,
                                          STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY,
                                          &RawAudioSamplingFrequency);
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Failed to get SAMPLING_FREQ control (%08x)\n", PlayerStatus);
        }
    }

    if (Update & NEW_STEREO_CONFIG)
    {
        PlayerStatus_t      PlayerStatus;
        PlayerStatus = Player->GetControl(Playback,
                                          Stream,
                                          STM_SE_CTRL_STREAM_DRIVEN_STEREO,
                                          &StreamDrivenDownmix);
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Failed to get STREAM_DRIVEN_STEREO control (%08x)\n", PlayerStatus);
        }

        SE_DEBUG(group_decoder_audio, "Applying STREAM_DRIVEN_STEREO config\n");
    }

    if (Update & NEW_SPEAKER_CONFIG)
    {
        PlayerStatus_t                    PlayerStatus;
        stm_se_audio_channel_assignment_t channelAssignment;

        PlayerStatus = Player->GetControl(Playback,
                                          Stream,
                                          STM_SE_CTRL_SPEAKER_CONFIG,
                                          &channelAssignment);
        if (PlayerStatus != PlayerNoError)
        {
            SE_ERROR("Failed to get SPEAKER_CONFIG control (%08x)\n", PlayerStatus);
        }
        else if (0 != channelAssignment.malleable)
        {
            // we use provided channel assignment to deduce the audiomode to apply
            enum eAccAcMode outmodeMain_from_player;
            if (0 == StmSeAudioGetAcModeFromChannelAssignment(&outmodeMain_from_player, &channelAssignment))
            {
                SE_DEBUG(group_decoder_audio, "Setting codec OutmodeMain from player to %s\n", StmSeAudioAcModeGetName(outmodeMain_from_player));
                OutmodeMain = outmodeMain_from_player;
                SE_DEBUG(group_decoder_audio, "Applying new SPEAKER_CONFIG %x \n", OutmodeMain);
            }
            else
            {
                SE_ERROR("Invalid Channel Assignment\n");
            }
        }
        else
        {
            // as malleable = 0, we apply ACC_MODE_ID to audiomode
            SE_DEBUG(group_decoder_audio, "Setting OutmodeMain to ACC_MODE_ID\n");
            OutmodeMain = ACC_MODE_ID;
        }
    }

}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudio_c::Input(Buffer_t          CodedBuffer)
{
    CodecStatus_t     Status;
    //
    // Are we allowed in here
    //
    AssertComponentState(ComponentRunning);

    //
    // First perform base operations
    //
    Status      = Codec_MmeBase_c::Input(CodedBuffer);

    if (Status != CodecNoError)
    {
        return Status;
    }

    // Check if any transform will be generated and only if so evaluate the params change
    if (CodedDataLength != 0)
    {
        EvaluateImplicitDownmixPromotion();
        EvaluateTranscodeCompressedNeeded();
        EvaluateMixerDRCParameters();
    }

    SE_VERBOSE(group_decoder_audio,  "Handling frame %d\n", ParsedFrameParameters->DisplayFrameIndex);

    //
    // Do we need to issue a new set of stream parameters
    //

    if (ParsedFrameParameters->NewStreamParameters)
    {
        memset(&StreamParameterContext->MMECommand, 0, sizeof(MME_Command_t));

        Status = FillOutSetStreamParametersCommand();
        if (Status == CodecNoError)
        {
            Status = SendMMEStreamParameters();
        }
        else
        {
            SE_ERROR("(%s) - Failed to fill out, and send, a set stream parameters command\n", Configuration.CodecName);
            ForceStreamParameterReload   = true;
            StreamParameterContextBuffer->DecrementReferenceCount();
            StreamParameterContextBuffer = NULL;
            StreamParameterContext       = NULL;
            OS_LockMutex(&Lock);
            ReleaseDecodeContext(DecodeContext);
            OS_UnLockMutex(&Lock);
            return Status;
        }

        StreamParameterContextBuffer    = NULL;
        StreamParameterContext          = NULL;
    }

    //
    // If there is no frame to decode then exit now.
    //

    /* In StreamBase decoder EofMarker buffer need to be send to the FW.
       So that FW can return Eof Marker after consuming entire input */

    if ((!ParsedFrameParameters->NewFrameParameters) && (!ParsedFrameParameters->EofMarkerFrame))
    {
        return CodecNoError;
    }

    Status = FillOutDecodeContext();
    if (Status != CodecNoError)
    {
        return Status;
    }

    //
    // Load the parameters into MME command
    //
    //! set up default to MME_TRANSFORM - fine for most codecs
    DecodeContext->MMECommand.CmdCode                           = MME_TRANSFORM;
    //! will change to MME_SEND_BUFFERS in WMA/OGG over-ridden FillOutDecodeCommand(), thread to do MME_TRANSFORMs
    //
    Status = FillOutDecodeCommand();
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to fill out a decode command\n", Configuration.CodecName);
        OS_LockMutex(&Lock);
        ReleaseDecodeContext(DecodeContext);
        OS_UnLockMutex(&Lock);
        return Status;
    }

    //
    // Ensure that the coded frame will be available throughout the
    // life of the decode by attaching the coded frame to the decode
    // context prior to launching the decode.
    //
    AttachCodedFrameBuffer();

    // Attach the Aux Buffer to the decode context
    AttachAuxFrameBuffer();

    Status = SendMMEDecodeCommand();
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to send a decode command\n", Configuration.CodecName);
        OS_LockMutex(&Lock);
        ReleaseDecodeContext(DecodeContext);
        OS_UnLockMutex(&Lock);
        return Status;
    }

    //
    // We have finished decoding into this buffer
    //
    FinishedDecode();
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Attach the coded frame buffer to the decode frame.
/// In case of transcoding is required, attach also attach
/// the transcoded buffer to the original coded buffer
///
void Codec_MmeAudio_c::AttachCodedFrameBuffer(void)
{
    DecodeContextBuffer->AttachBuffer(CodedFrameBuffer);
    //DecodeContextBuffer->Dump (DumpAll);

    if (TranscodeEnable && TranscodeNeeded)
    {
        Buffer_t CodedDataBuffer;
        CurrentDecodeBuffer->ObtainAttachedBufferReference(CodedFrameBufferType, &CodedDataBuffer);
        SE_ASSERT(CodedDataBuffer != NULL);

        CodedDataBuffer->AttachBuffer(CurrentTranscodeBuffer);
        CurrentTranscodeBuffer->DecrementReferenceCount();
        CurrentTranscodeBuffer = NULL;
        // the transcoded buffer is now only referenced by its attachment to the coded buffer
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Attach the Aux frame buffer to the decode frame.
///
void Codec_MmeAudio_c::AttachAuxFrameBuffer(void)
{
    if (AuxOutputEnable)
    {
        if (CurrentAuxBuffer != NULL)
        {
            DecodeContextBuffer->AttachBuffer(CurrentAuxBuffer);
            CurrentAuxBuffer->DecrementReferenceCount();
            CurrentAuxBuffer = NULL;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the DecodeContext structure with parameters for MPEG audio.
/// This can be over-ridden by WMA to not have any output buffer, as it will
/// do an MME_SEND_BUFFERS instead of MME_TRANSFORM
///
CodecStatus_t Codec_MmeAudio_c::FillOutDecodeContext(void)
{
    OS_LockMutex(&Lock);

    //
    // Obtain a new buffer if needed
    //

    if (CurrentDecodeBufferIndex == INVALID_INDEX)
    {
        CodecStatus_t Status = GetDecodeBuffer();
        if (Status != CodecNoError)
        {
            SE_ERROR("(%s) - Failed to get decode buffer\n", Configuration.CodecName);
            ReleaseDecodeContext(DecodeContext);
            OS_UnLockMutex(&Lock);
            return Status;
        }
    }

    //
    // Record the buffer being used in the decode context
    //
    DecodeContext->BufferIndex  = CurrentDecodeBufferIndex;

    //
    // Provide default values for the input and output buffers (the sub-class can change this if it wants to).
    //
    // Happily the audio firmware is less mad than the video firmware on the subject of buffers.
    //
    memset(&DecodeContext->MMECommand, 0, sizeof(MME_Command_t));

    SetCommandIO();

    DecodeContext->DecodeInProgress     = true;
    BufferState[CurrentDecodeBufferIndex].DecodesInProgress++;
    BufferState[CurrentDecodeBufferIndex].OutputOnDecodesComplete       = true;

    OS_UnLockMutex(&Lock);

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Clear up - to be over-ridden by WMA codec to do nothing, as actual decode done elsewhere
///
void Codec_MmeAudio_c::FinishedDecode(void)
{
    OS_LockMutex(&Lock);
    //
    // We have finished decoding into this buffer
    //
    CurrentDecodeBufferIndex    = INVALID_INDEX;
    CurrentDecodeIndex          = INVALID_INDEX;
    OS_UnLockMutex(&Lock);
}


////////////////////////////////////////////////////////////////////////////
///
///     The intercept to the initialize data types function, that
///     ensures the audio specific type is recorded in the configuration
///     record.
///

CodecStatus_t   Codec_MmeAudio_c::InitializeDataTypes(void)
{
    //
    // Add audio specific types, and the address of
    // parsed audio parameters to the configuration
    // record. Then pass on down to the base class
    //
    Configuration.AudioVideoDataParsedParametersType    = Player->MetaDataParsedAudioParametersType;
    Configuration.AudioVideoDataParsedParametersPointer = (void **)&ParsedAudioParameters;
    Configuration.SizeOfAudioVideoDataParsedParameters  = sizeof(ParsedAudioParameters_t);
    return Codec_MmeBase_c::InitializeDataTypes();
}

////////////////////////////////////////////////////////////////////////////
///
/// Examine the capability structure returned by the firmware.
///
/// In addition to some diagnostic output and some trivial error checking the
/// primary purpose of this function is to ensure that the firmware is capable
/// of performing the requested decode.
///
CodecStatus_t   Codec_MmeAudio_c::ParseCapabilities(unsigned int ActualTransformer)
{
    // Locally rename the VeryLongMemberNamesUsedInTheSuperClass
    MME_LxAudioDecoderInfo_t &Capability = AudioDecoderTransformCapability;
    MME_LxAudioDecoderInfo_t &Mask = AudioDecoderTransformCapabilityMask;

    // Check for version skew
    if (Capability.StructSize != sizeof(MME_LxAudioDecoderInfo_t))
    {
        SE_ERROR("%s reports different sizeof MME_LxAudioDecoderInfo_t (version skew?)\n",
                 Configuration.TransformName[ActualTransformer]);
        return CodecError;
    }

    // Dump the transformer capability structure
    SE_DEBUG(group_decoder_audio,  "Transformer Capability: %s\n", Configuration.TransformName[ActualTransformer]);
    SE_DEBUG(group_decoder_audio,  "  DecoderCapabilityFlags = %x (requires %x)\n",
             Capability.DecoderCapabilityFlags, Mask.DecoderCapabilityFlags);
    SE_DEBUG(group_decoder_audio,  "  DecoderCapabilityExtFlags[0..1] = %x %x\n",
             Capability.DecoderCapabilityExtFlags[0],
             Capability.DecoderCapabilityExtFlags[1]);
    SE_DEBUG(group_decoder_audio,  "  PcmProcessorCapabilityFlags[0..1] = %x %x\n",
             Capability.PcmProcessorCapabilityFlags[0],
             Capability.PcmProcessorCapabilityFlags[1]);

    // Confirm that the transformer is sufficiently capable to support the required decoder
    if ((Mask.DecoderCapabilityFlags         !=
         (Mask.DecoderCapabilityFlags         & Capability.DecoderCapabilityFlags)) ||
        (Mask.DecoderCapabilityExtFlags[0]   !=
         (Mask.DecoderCapabilityExtFlags[0]   & Capability.DecoderCapabilityExtFlags[0])) ||
        (Mask.DecoderCapabilityExtFlags[1]   !=
         (Mask.DecoderCapabilityExtFlags[1]   & Capability.DecoderCapabilityExtFlags[1])) ||
        (Mask.PcmProcessorCapabilityFlags[0] !=
         (Mask.PcmProcessorCapabilityFlags[0]   & Capability.PcmProcessorCapabilityFlags[0])) ||
        (Mask.PcmProcessorCapabilityFlags[1] !=
         (Mask.PcmProcessorCapabilityFlags[1]   & Capability.PcmProcessorCapabilityFlags[1])))
    {
        SE_ERROR("%s is not capable of decoding %s\n",
                 Configuration.TransformName[ActualTransformer], Configuration.CodecName);
        return CodecError;
    }

    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for MPEG audio.
///
CodecStatus_t Codec_MmeAudio_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t &GlobalParams = *GlobalParams_p;
    MME_LxDecConfig_t &Config           = *((MME_LxDecConfig_t *) GlobalParams.DecConfig);
    unsigned char     *PcmParams_p      = ((unsigned char *) &Config) + Config.StructSize;
    unsigned char      Policy;
    int                channelSelection = Player->PolicyValue(Playback, Stream, PolicyAudioDualMonoChannelSelection);
    int                RegionType       = Player->PolicyValue(Playback, Stream, PolicyRegionType);
    //
    MME_LxPcmProcessingGlobalParams_Subset_t &PcmParams = *((MME_LxPcmProcessingGlobalParams_Subset_t *) PcmParams_p);
    PcmParams.StructSize = sizeof(PcmParams);
    PcmParams.DigSplit   = ACC_SPLIT_AUTO;
    PcmParams.AuxSplit   = ACC_SPLIT_AUTO;
    MME_DeEmphGlobalParams_t &deemph = PcmParams.DeEmphasis;
    int                        deemph_apply = Player->PolicyValue(Playback, Stream, PolicyAudioDeEmphasis);
    deemph.Id         = PCMPROCESS_SET_ID(ACC_PCM_DeEMPH_ID, ACC_MIX_MAIN);
    deemph.StructSize = sizeof(MME_DeEmphGlobalParams_t);
    deemph.Apply      = (enum eAccProcessApply) deemph_apply;
    deemph.Mode       = ACC_DEEMPH_50_15_us;
    MME_CMCGlobalParams_t &cmc = PcmParams.CMC;
    cmc.Id = PCMPROCESS_SET_ID(ACC_PCM_CMC_ID, ACC_MIX_MAIN);
    cmc.StructSize = sizeof(MME_CMCGlobalParams_t);
    cmc.Config[CMC_OUTMODE_MAIN] = OutmodeMain;
    cmc.Config[CMC_OUTMODE_AUX]  = OutmodeAux; // Specify the Aux outmode
    cmc.ExtendedParams.MixCoeffs.GlobalGainCoeff = ACC_UNITY;
    cmc.ExtendedParams.MixCoeffs.LfeMixCoeff = ACC_UNITY;
#if AUDIO_API_VERSION >= 0x110815
    Policy  = (Player->PolicyValue(Playback, Stream, PolicyAudioStreamDrivenDualMono) == PolicyValueStreamDrivenDualMono) ? ACC_DUAL_1p1_ONLY : 0;
#else
#warning PolicyValueStreamDrivenDualMono not yet supported
    Policy  = 0;
#endif

    switch (channelSelection)
    {
    case PolicyValueDualMonoStereoOut:
        SelectedChannel = ChannelSelectStereo;
        Policy |= ACC_DUAL_LR;
        break;

    case PolicyValueDualMonoLeftOut:
        SelectedChannel = ChannelSelectLeft;
        Policy |= ACC_DUAL_LEFT_MONO;
        break;

    case PolicyValueDualMonoRightOut:
        SelectedChannel = ChannelSelectRight;
        Policy |= ACC_DUAL_RIGHT_MONO;
        break;

    case PolicyValueDualMonoMixLeftRightOut:
        SelectedChannel = ChannelSelectMono;
        Policy |= ACC_DUAL_MIX_LR_MONO;
        break;

    default:
        SE_ERROR("Invalid Channel Selection\n");
        break;
    }

    // StreamDrivenDownmix
    // 1- first try to get StreamDrivenDownmix parameter at player level (stream or playback)
    // 2- if not available, the uses StreamDrivenDownmix setting from mixer (which can be modified through ALSA)
    bool streamDrivenDownmix_to_apply;

    if (PlayerNoError != Player->GetControl(Playback, Stream, STM_SE_CTRL_STREAM_DRIVEN_STEREO, (void *) &streamDrivenDownmix_to_apply))
    {
        SE_DEBUG(group_decoder_audio, "No StreamDrivenDownmix parameter available at player level, using mixer parameter\n");
        streamDrivenDownmix_to_apply  = StreamDrivenDownmix;
    }

    if (streamDrivenDownmix_to_apply)
    {
        Policy |= ACC_DUAL_AUTO;
    }

    cmc.Config[CMC_DUAL_MODE]       = Policy;
    SE_DEBUG(group_decoder_audio,  "Set DualMode to %d: [Channel Select = %d] (1p1only=%d]\n", Policy, SelectedChannel,
             ((Policy & ACC_DUAL_1p1_ONLY) != 0));
    SE_DEBUG(group_decoder_audio,  "StreamDrivenDownmix value applied =%d\n", streamDrivenDownmix_to_apply);
    cmc.Config[CMC_PCM_DOWN_SCALED] = ACC_MME_TRUE;
    cmc.CenterMixCoeff = ACC_M3DB;
    cmc.SurroundMixCoeff = ACC_M3DB;
//
    MME_DMixGlobalParams_t &dmix = PcmParams.DMix;
    dmix.Id = PCMPROCESS_SET_ID(ACC_PCM_DMIX_ID, ACC_MIX_MAIN);
    dmix.Apply = ACC_MME_AUTO;
    dmix.StructSize = sizeof(MME_DMixGlobalParams_t);
    dmix.Config[DMIX_USER_DEFINED] = ACC_MME_FALSE;
    dmix.Config[DMIX_STEREO_UPMIX] = ACC_MME_FALSE;
    dmix.Config[DMIX_MONO_UPMIX] = ACC_MME_FALSE;
    dmix.Config[DMIX_MEAN_SURROUND] = ACC_MME_FALSE;
    dmix.Config[DMIX_SECOND_STEREO] = ACC_MME_TRUE;
    if (RegionType == PolicyValueRegionARIB)
    {
        dmix.Config[DMIX_NORMALIZE] = DMIX_NORMALIZE_OFF;
    }
    else
    {
        dmix.Config[DMIX_NORMALIZE] = DMIX_NORMALIZE_ON;
    }
    dmix.Config[DMIX_NORM_IDX] = 0;
    dmix.Config[DMIX_DIALOG_ENHANCE] = ACC_MME_FALSE;
//
    MME_Resamplex2GlobalParams_t &resamplex2 = PcmParams.Resamplex2;
    resamplex2.Id = PCMPROCESS_SET_ID(ACC_PCM_RESAMPLE_ID, ACC_MIX_MAIN);
    resamplex2.StructSize = sizeof(MME_Resamplex2GlobalParams_t);
    resamplex2.Apply = ACC_MME_DISABLED;

    // WARNING : Activation of TempoControl involves a delay line
    // When we turn on the TempoControl, the PTS will be modified
    // wrt the duration of the delay line.
    // Consequently, tempo Control should not be activated for
    // PAUSE otherwise the Output timer will observe undue
    // non-linearity of timeline

    MME_TempoGlobalParams_t &tempo = PcmParams.Tempo;
    Rational_t               Speed;
    PlayDirection_t          Direction;
    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);
    tempo.Id         = PCMPROCESS_SET_ID(ACC_PCM_TEMPO_ID, ACC_MIX_MAIN);
    tempo.StructSize = sizeof(MME_TempoGlobalParams_t);

    if ((Speed > 0) && (Direction == PlayForward) && (Speed != 1))
    {
        tempo.Apply  = ACC_MME_ENABLED;
        // Speed =  80/100 ==> - 20% slower
        // Speed = 200/100 ==> +100% faster
        tempo.Ratio  = (Speed * 100).IntegerPart() - 100;
    }
    else
    {
        tempo.Apply  = ACC_MME_DISABLED;
        tempo.Ratio  = 0;
    }
    mTempoRequestedRatio = tempo.Ratio;

//
    return CodecNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Populate the AUDIO_DECODER's initialization parameters for audio.
///
/// Zero the entire initialization structure and populate those portions that
/// are generic for all AUDIO_DECODER instantiations.
///
/// WARNING: This method does not do what you think it does.
///
/// More exactly, if you think that this method will populate the
/// global parameters structure which forms the bulk of the initialization
/// parameters structure then you're wrong! Basically the codec specific
/// initialization parameters are variable size so from this method we don't
/// know what byte offset to use.
///
/// The upshot of all this is that this method \b must be overridden in
/// our sub-classes.
///
CodecStatus_t Codec_MmeAudio_c::FillOutTransformerInitializationParameters()
{
    MME_LxAudioDecoderInitParams_t &Params = AudioDecoderInitializationParameters;
    memset(&Params, 0, sizeof(Params));

    Params.StructSize = sizeof(MME_LxAudioDecoderInitParams_t);
    Params.CacheFlush = ACC_MME_ENABLED;
    // Detect changes between BL025 and BL028 (delete this code when BL025 is ancient history)
#if DRV_MULTICOM_AUDIO_DECODER_VERSION >= 0x090128
    Params.BlockWise.u32 = 0;
#else
    Params.BlockWise = ACC_MME_FALSE;
#endif
#if DRV_MULTICOM_AUDIO_DECODER_VERSION < 0x110729
    // upsampling must be enabled separately for each codec since it requires frame analyser support
    Params.SfreqRange = ACC_FSRANGE_UNDEFINED;
#else
    Params.Reserved = 0;
    Params.MemConfig.Crc.CRC_Main = Codec_c::EnableAudioCRC;
    Params.MemConfig.Crc.CRC_ALL  = Codec_c::EnableAudioCRC == 2;
#endif
    // run the audio decoder with a fixed number of main (surround) channels
    // The aux channel has been initialised to 2 the (max value for aux o/p)
    Params.NChans[ACC_MIX_MAIN] = AudioOutputSurface->ChannelCount;
    Params.NChans[ACC_MIX_AUX]  = 2;
    Params.ChanPos[ACC_MIX_MAIN] = 0;
    Params.ChanPos[ACC_MIX_AUX] = 0;
//
    return CodecNoError;
}

void Codec_MmeAudio_c::GlobalParamsCommandCompleted(CodecBaseStreamParameterContext_t *StreamParameterContext)
{
    SE_VERBOSE(group_decoder_audio, "\n");

    if (mTempoSetRatio != mTempoRequestedRatio)
    {
        SE_DEBUG(group_decoder_audio, "Tempo Speed Change:%d->%d", mTempoSetRatio, mTempoRequestedRatio);
        mTempoSetRatio = mTempoRequestedRatio;
    }

    return Codec_MmeBase_c::GlobalParamsCommandCompleted(StreamParameterContext);
}

////////////////////////////////////////////////////////////////////////////
///
/// Validate the PCM processing extended status structures and squawk loudly if problems are found.
///
CodecStatus_t Codec_MmeAudio_c::ValidatePcmProcessingExtendedStatus(
    CodecBaseDecodeContext_t *Context,
    MME_PcmProcessingFrameExtStatus_t *PcmStatus)
{
    ParsedAudioParameters_t *AudioParameters;
    //
    // provide default values for parameters that may be influenced by the extended status
    //
    OS_LockMutex(&Lock);
    AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return CodecError;
    }

    AudioParameters->MixingMetadata.IsMixingMetadataPresent = false;
    OS_UnLockMutex(&Lock);
    //
    // Get ready to process the status messages (and perform a little bounds checking
    //
    int mt, id;
    int DmixTableNo = 0;
    int BytesLeft = PcmStatus->BytesUsed;
    MME_PcmProcessingStatusTemplate_t *SpecificStatus = &(PcmStatus->PcmStatus);

    if (BytesLeft > (int)(sizeof(MME_PcmProcessingFrameExtStatus_Concrete_t) - sizeof(PcmStatus->BytesUsed) + sizeof(MME_PcmProcessingFrameExtCommonStatus_t)))
    {
        SE_ERROR("BytesLeft is too large - BytesLeft %d\n", BytesLeft);
        return CodecError;
    }

    //
    // Iterate through the status messages processing each one
    //

    while (BytesLeft > 0)
    {
        // check for sanity
        if (SpecificStatus->StructSize < 8 ||
            SpecificStatus->StructSize > (unsigned) BytesLeft)
        {
            SE_ERROR("PCM extended status is too %s - Id %x  StructSize %d",
                     (SpecificStatus->StructSize < 8 ? "small" : "large"),
                     SpecificStatus->Id, SpecificStatus->StructSize);
            return CodecError;
        }

        // handle the status report
        mt = ACC_PCMPROC_MT(SpecificStatus->Id);
        id = ACC_PCMPROC_ID(SpecificStatus->Id);

        switch (mt)
        {
        case ACC_PCMPROCESS_MAIN_MT :
            switch (id)
            {
            case ACC_PCM_CMC_ID:
                SE_DEBUG(group_decoder_audio, "Found [Output 0 (MAIN)]\n");
                // do nothing with it
                break;

            case ACC_PCM_DMIX_ID:
                SE_DEBUG(group_decoder_audio, "Found [DMIX report on (MAIN)]\n");
                HandleDownmixTable(Context, SpecificStatus, DmixTableNo);
                DmixTableNo++;
                break;

            default:
                SE_INFO(group_decoder_audio, "Ignoring unexpected PCM-MAIN [%d] extended status - Id %x  StructSize %d\n", id,
                        SpecificStatus->Id, SpecificStatus->StructSize);
                // just ignore it... don't propagate the error
            }

            break;
#if PCMPROCESSINGS_API_VERSION >= 0x101122

        case ACC_DECODER_MT:
            SE_DEBUG(group_decoder_audio, "Found Codec[%d] SpecificStatus , StructSize %d\n",
                     SpecificStatus->Id, SpecificStatus->StructSize);
            break;

        case ACC_PCMPROCESS_COMMON_MT:
#else
        case ACC_DECODER_MT:
#endif
            switch (SpecificStatus->Id)
            {
            case ACC_MIX_METADATA_ID:
                SE_DEBUG(group_decoder_audio, "Found ACC_MIX_METADATA_ID - Id %x  StructSize %d\n",
                         SpecificStatus->Id, SpecificStatus->StructSize);
                HandleMixingMetadata(Context, SpecificStatus);
                break;
#if PCMPROCESSINGS_API_VERSION >= 0x101122

            case ACC_SA_ID:
                SE_DEBUG(group_decoder_audio, "Found SupplementaryAudio status :: ACC_SA_ID - Id %x  StructSize %d\n",
                         SpecificStatus->Id, SpecificStatus->StructSize);
                //      HandleMixingMetadata(Context, SpecificStatus);
                break;
#endif

            case ACC_CRC_ID:
                SE_DEBUG(group_decoder_audio, "Found ACC_CRC_ID - Id %x  StructSize %d\n",
                         SpecificStatus->Id, SpecificStatus->StructSize);
                ReportDecodedCRC(SpecificStatus);
                break;

            default:
                SE_INFO(group_decoder_audio, "Ignoring unexpected PCM-COMMON [%d] extended status - Id %x  StructSize %d\n", id,
                        SpecificStatus->Id, SpecificStatus->StructSize);
                // just ignore it... don't propagate the error
            }

            break;

        case ACC_PARSER_MT:
            SE_DEBUG(group_decoder_audio, "Found Parser status :: - Id %x  StructSize %d\n",
                     SpecificStatus->Id, SpecificStatus->StructSize);
            break;

        default:
            SE_INFO(group_decoder_audio, "Ignoring unexpected MT[%d] extended status - Id %x  StructSize %d\n", mt,
                    SpecificStatus->Id, SpecificStatus->StructSize);
            // just ignore it... don't propagate the error
        }

        // move to the next status field. we checked for sanity above so we shouldn't need any bounds checks.
        BytesLeft -= SpecificStatus->StructSize;
        SpecificStatus = (MME_PcmProcessingStatusTemplate_t *)
                         (((char *) SpecificStatus) + SpecificStatus->StructSize);
    }

    return CodecNoError;
}




////////////////////////////////////////////////////////////////////////////
///
/// Error generating stub function. Should be unreachable.
///
/// This is method is deliberately not abstract since not all sub-classes are
/// required to override it.
///
void Codec_MmeAudio_c::HandleMixingMetadata(CodecBaseDecodeContext_t *Context,
                                            MME_PcmProcessingStatusTemplate_t *PcmStatus)
{
    SE_ERROR("Found mixing metadata but sub-class didn't provide a mixing metadata handler\n");
}

// This function simply uses the table defined above
unsigned char Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration(enum eAccAcMode Mode)
{
    int nLUTs = sizeof(ACC_AcMode2ChannelCountLUT) / sizeof(ACC_AcMode2ChannelCountLUT[0]);
    return (Mode >= nLUTs ? 0 : ACC_AcMode2ChannelCountLUT[Mode]);
}

// This function reports Downmix table reported by FW
void Codec_MmeAudio_c::HandleDownmixTable(CodecBaseDecodeContext_t *Context,
                                          MME_PcmProcessingStatusTemplate_t *PcmStatus, int DmixTableNo)
{
    ParsedAudioParameters_t *AudioParameters;
    MME_DmixStatus_t *DmixTable = (MME_DmixStatus_t *) PcmStatus;
    int nch_out, nch_in, i, j;
    unsigned int dmix_table_size;
    nch_out = DmixTable->Features.NChOut;
    nch_in  = DmixTable->Features.NChIn;
    dmix_table_size = SIZEOF_DMIX_STATUS(nch_out);

    //
    // Validation
    //
    if (DmixTableNo >= MAX_SUPPORTED_DMIX_TABLE)
    {
        SE_ERROR("No space to store reported Dmix table. Space supported for %d but reported for %d\n", MAX_SUPPORTED_DMIX_TABLE, DmixTableNo + 1);
        return;
    }

    if (DmixTable->StructSize < dmix_table_size)
    {
        SE_ERROR("Structure size of Dmix table is too small. got = %d expected = %d\n", DmixTable->StructSize, dmix_table_size);
        return;
    }

    if ((nch_out > MAX_NB_CHANNEL_COEFF) || nch_in > MAX_NB_CHANNEL_COEFF)
    {
        SE_ERROR("Unexpected Nch IN / Nch OUT\n");
        return;
    }

    OS_LockMutex(&Lock);
    AudioParameters = BufferState[Context->BufferIndex].ParsedAudioParameters;

    if (AudioParameters == NULL)
    {
        SE_ERROR("(%s) - AudioParameters are NULL\n", Configuration.CodecName);
        OS_UnLockMutex(&Lock);
        return;
    }

    if (!DmixTable->Features.TablePresent)
    {
        AudioParameters->DownMixTable[DmixTableNo].IsTablePresent = 0;
        SE_DEBUG(group_decoder_audio, "Downmix table reported but not present\n");
        OS_UnLockMutex(&Lock);
        return;
    }

    AudioParameters->DownMixTable[DmixTableNo].IsTablePresent = DmixTable->Features.TablePresent;
    AudioParameters->DownMixTable[DmixTableNo].NChIn        = DmixTable->Features.NChIn;
    AudioParameters->DownMixTable[DmixTableNo].NChOut       = DmixTable->Features.NChOut;
    AudioParameters->DownMixTable[DmixTableNo].InMode       = DmixTable->Features.InMode;
    AudioParameters->DownMixTable[DmixTableNo].OutMode      = DmixTable->Features.OutMode;

    for (i = 0; i < nch_out; i++)
    {
        for (j = 0; j < nch_in; j++)
        {
            AudioParameters->DownMixTable[DmixTableNo].DownMixTableCoeff[i][j] = DmixTable->DownMixTable[i][j];
        }
    }

    OS_UnLockMutex(&Lock);
}

// This function reports CRC of the decoded output by reading FW reported status
void Codec_MmeAudio_c::ReportDecodedCRC(MME_PcmProcessingStatusTemplate_t *PcmStatus)
{
    MME_CrcStatus_t *CRCStatus = (MME_CrcStatus_t *) PcmStatus;
    st_relayfs_write_se(ST_RELAY_TYPE_AUDIO_DEC_CRC, ST_RELAY_SOURCE_SE,
                        (unsigned char *) &CRCStatus->Crc[0], sizeof(CRCStatus->Crc), 0);
}

// /////////////////////////////////////////////////////////////////////////
//
//      The generic audio function used to fill out a buffer structure
//      request.
//

CodecStatus_t   Codec_MmeAudio_c::FillOutDecodeBufferRequest(DecodeBufferRequest_t     *Request)
{
    memset(Request, 0, sizeof(DecodeBufferRequest_t));

    if (ParsedAudioParameters == NULL)
    {
        SE_ERROR("(%s) - ParsedAudioParameters are NULL\n", Configuration.CodecName);
        return CodecError;
    }

    ParsedAudioParameters->Source.BitsPerSample = 32;
    ParsedAudioParameters->Source.ChannelCount  = 8;

    Request->DimensionCount     = 3;
    Request->Dimension[0]       = ParsedAudioParameters->Source.BitsPerSample;
    Request->Dimension[1]       = ParsedAudioParameters->Source.ChannelCount;
    Request->Dimension[2]       = ParsedAudioParameters->SampleCount ? ParsedAudioParameters->SampleCount : Configuration.MaximumSampleCount;

    if ((Request->Dimension[0] != 32) ||
        (Request->Dimension[1] != 8) ||
        (Request->Dimension[2] == 0))
        SE_INFO(group_decoder_audio, "Non-standard parameters (%2d %d %d)\n",
                Request->Dimension[0], Request->Dimension[1], Request->Dimension[2]);

    return CodecNoError;
}

CodecStatus_t Codec_MmeAudio_c::GetAttribute(const char *Attribute, PlayerAttributeDescriptor_t *Value)
{
    if (0 == strcmp(Attribute, "sample_frequency"))
    {
        enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) AudioDecoderStatus.SamplingFreq;
        Value->Id                        = SYSFS_ATTRIBUTE_ID_INTEGER;

        if (SamplingFreqCode < ACC_FS_reserved)
        {
            Value->u.Int = (int) StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
        }
        else
        {
            Value->u.Int = 0;
        }

        return CodecNoError;
    }
    else if (0 == strcmp(Attribute, "number_channels"))
    {
#if 0
        int lfe_mask = ((int) ACC_MODE20t_LFE - (int) ACC_MODE20t);
        int low_channel = 0;
        int number_channels = 0;
        Value->Id                        = SYSFS_ATTRIBUTE_ID_INTEGER;

        if (AudioDecoderStatus.DecAudioMode & lfe_mask) { low_channel = 1; }

        number_channels = GetNumberOfChannelsFromAudioConfiguration((enum eAccAcMode)(AudioDecoderStatus.DecAudioMode & ~lfe_mask));
        Value->u.Int = number_channels + low_channel;
#else
#define C(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## _LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break
#define Cn(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break
#define Ct(f,b) case ACC_MODE ## f ## b ## t: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## t_LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break
        Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;

        switch ((enum eAccAcMode)(AudioDecoderStatus.DecAudioMode))
        {
            Ct(2, 0);
            C(1, 0);
            C(2, 0);
            C(3, 0);
            C(2, 1);
            C(3, 1);
            C(2, 2);
            C(3, 2);
            C(2, 3);
            C(3, 3);
            C(2, 4);
            C(3, 4);
            Cn(4, 2);
            Cn(4, 4);
            Cn(5, 2);
            Cn(5, 3);

        default:
            Value->u.ConstCharPointer = "UNKNOWN";
        }

#endif
        return CodecNoError;
    }
    else
    {
        SE_ERROR("This attribute does not exist\n");
        return CodecError;
    }

    return CodecNoError;
}

/// /////////////////////////////////////////////////////////////////////////
//
//      Get the aux frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudio_c::GetAuxFrameBufferPool(BufferPool_t *Afp)
{
    allocator_status_t      AStatus;

    //
    // Create the buffer pool if not done already
    //

    if (AuxFramePool == NULL)
    {

        //
        // Coded frame buffer type
        //
        CodecStatus_t Status = InitializeDataType(&InitialAuxFrameBufferDescriptor, &AuxFrameBufferType, &AuxFrameBufferDescriptor);
        if (Status != CodecNoError)
        {
            return Status;
        }

        //
        // Get the memory and Create the pool with it
        //

        Player->GetBufferManager(&BufferManager);

        AStatus = PartitionAllocatorOpen(&AuxFrameMemoryDevice,
                                         Configuration.AncillaryMemoryPartitionName,
                                         AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT * Configuration.MaximumSampleCount,
                                         MEMORY_DEFAULT_ACCESS);
        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate memory for %s\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        AuxFrameMemory[CachedAddress]         = AllocatorUserAddress(AuxFrameMemoryDevice);
        AuxFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress(AuxFrameMemoryDevice);

        BufferStatus_t BufferStatus = BufferManager->CreatePool(&AuxFramePool,
                                                                AuxFrameBufferType,
                                                                AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT,
                                                                AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT * Configuration.MaximumSampleCount,
                                                                AuxFrameMemory);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("GetAuxFrameBufferPool(%s) - Failed to create the pool\n", Configuration.CodecName);
            return CodecError;
        }

        ((PlayerStream_c *)Stream)->AuxFrameBufferType = AuxFrameBufferType;
    }

    *Afp = AuxFramePool;

    return CodecNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new  aux buffer
//

CodecStatus_t   Codec_MmeAudio_c::GetAuxBuffer(void)
{
    BufferPool_t  Afp;
    unsigned int  BytesPerSample, NumberOfSamples;

    BytesPerSample  = ParsedAudioParameters->Source.BitsPerSample >> 3;
    NumberOfSamples = ParsedAudioParameters->SampleCount ? ParsedAudioParameters->SampleCount : Configuration.MaximumSampleCount;

    //
    // Get a buffer
    //

    CodecStatus_t Status = GetAuxFrameBufferPool(&Afp);
    if (Status != CodecNoError)
    {
        SE_ERROR("GetAuxBuffer(%s) - Failed to obtain the Aux buffer pool instance\n", Configuration.CodecName);
        return Status;
    }

    if (CurrentAuxBuffer)
    {
        CurrentAuxBuffer->DecrementReferenceCount();
        CurrentAuxBuffer = NULL;
    }

    BufferStatus_t BufferStatus = Afp->GetBuffer(&CurrentAuxBuffer, IdentifierCodec, NumberOfSamples * BytesPerSample * AUDIO_DECODER_MAX_AUX_CHANNEL_COUNT, false);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("GetAuxBuffer(%s) - Failed to obtain a Aux buffer from the Aux buffer pool\n", Configuration.CodecName);
        Afp->Dump(DumpPoolStates | DumpBufferStates);

        return CodecError;
    }

    //
    // Map it and initialize the mapped entry.
    //

    CurrentAuxBuffer->GetIndex(&CurrentAuxBufferIndex);

    if (CurrentAuxBufferIndex >= AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT)
    {
        SE_ERROR("GetAuxBuffer(%s) - Aux buffer index >= AUDIO_DECODER_AUX_FRAME_BUFFER_COUNT - Implementation error\n", Configuration.CodecName);
        return CodecError;
    }

    memset(&AuxBuffers[CurrentAuxBufferIndex], 0, sizeof(AdditionalBufferState_t));

    AuxBuffers[CurrentAuxBufferIndex].Buffer = CurrentAuxBuffer;

    //
    // Obtain the interesting references to the buffer
    //

    CurrentAuxBuffer->ObtainDataReference(&AuxBuffers[CurrentAuxBufferIndex].BufferLength,
                                          NULL,
                                          (void **)(&AuxBuffers[CurrentAuxBufferIndex].BufferPointer),
                                          Configuration.AddressingMode);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new decode buffer.
//

CodecStatus_t   Codec_MmeAudio_c::GetTranscodeBuffer(void)
{
    BufferPool_t   Tfp;
    //Buffer        Structure_t        BufferStructure;
    //
    // Get a buffer
    //
    CodecStatus_t Status = GetTranscodedFrameBufferPool(&Tfp);
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to obtain the transcoded buffer pool instance\n", Configuration.CodecName);
        return Status;
    }

    if (CurrentTranscodeBuffer)
    {
        CurrentTranscodeBuffer->DecrementReferenceCount();
        CurrentTranscodeBuffer = NULL;
    }

    BufferStatus_t BufferStatus = Tfp->GetBuffer(&CurrentTranscodeBuffer, IdentifierCodec,
                                                 Configuration.TranscodedFrameMaxSize, false);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("(%s) - Failed to obtain a transcode buffer from the transcoded buffer pool\n", Configuration.CodecName);
        Tfp->Dump(DumpPoolStates | DumpBufferStates);
        return CodecError;
    }

    //
    // Map it and initialize the mapped entry.
    //
    CurrentTranscodeBuffer->GetIndex(&CurrentTranscodeBufferIndex);

    if (CurrentTranscodeBufferIndex >= AUDIO_DECODER_TRANSCODE_BUFFER_COUNT)
    {
        SE_ERROR("(%s) - Transcode buffer index >= DTSHD_TRANSCODE_BUFFER_COUNT - Implementation error\n", Configuration.CodecName);
        return CodecError;
    }

    memset(&TranscodedBuffers[CurrentTranscodeBufferIndex], 0, sizeof(AdditionalBufferState_t));
    TranscodedBuffers[CurrentTranscodeBufferIndex].Buffer = CurrentTranscodeBuffer;
    //
    // Obtain the interesting references to the buffer
    //
    CurrentTranscodeBuffer->ObtainDataReference(&TranscodedBuffers[CurrentTranscodeBufferIndex].BufferLength,
                                                NULL,
                                                (void **)(&TranscodedBuffers[CurrentTranscodeBufferIndex].BufferPointer),
                                                Configuration.AddressingMode);

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get coded frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudio_c::GetTranscodedFrameBufferPool(BufferPool_t *Tfp)
{
    allocator_status_t      AStatus;

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if (TranscodedFramePool == NULL)
    {
        //
        // Coded frame buffer type
        //
        CodecStatus_t Status = InitializeDataType(&InitialTranscodedFrameBufferDescriptor, &TranscodedFrameBufferType, &TranscodedFrameBufferDescriptor);
        if (Status != CodecNoError)
        {
            return Status;
        }

        //
        // Get the memory and Create the pool with it
        //
        Player->GetBufferManager(&BufferManager);
        AStatus = PartitionAllocatorOpen(&TranscodedFrameMemoryDevice,
                                         Configuration.TranscodedMemoryPartitionName,
                                         AUDIO_DECODER_TRANSCODE_BUFFER_COUNT *
                                         Configuration.TranscodedFrameMaxSize,
                                         MEMORY_DEFAULT_ACCESS);

        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate memory for %s\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        TranscodedFrameMemory[CachedAddress]         = AllocatorUserAddress(TranscodedFrameMemoryDevice);
        TranscodedFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress(TranscodedFrameMemoryDevice);

        //
        BufferStatus_t BufferStatus = BufferManager->CreatePool(&TranscodedFramePool,
                                                                TranscodedFrameBufferType,
                                                                AUDIO_DECODER_TRANSCODE_BUFFER_COUNT,
                                                                AUDIO_DECODER_TRANSCODE_BUFFER_COUNT *
                                                                Configuration.TranscodedFrameMaxSize,
                                                                TranscodedFrameMemory);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to create the pool\n", Configuration.CodecName);
            return CodecError;
        }

        ((PlayerStream_c *)Stream)->TranscodedFrameBufferType = TranscodedFrameBufferType;
    }

    *Tfp = TranscodedFramePool;
    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to obtain a new compressed frame buffers for N Scatter Pages.
//

CodecStatus_t   Codec_MmeAudio_c::GetCompressedFrameBuffer(int32_t NoOfCompressedBufferToGet)
{
    BufferPool_t             Cfp; // Compressed Frame Pool
    NoOfCompressedFrameBuffers = 0;
    //Buffer        Structure_t        BufferStructure;

    //
    // Get a buffer
    //
    if (NoOfCompressedBufferToGet > AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_SCATTER_PAGES)
    {
        SE_ERROR("(%s) - NoOfCompressedBufferToGet is more than Pages allocated for Compressed Frame\n", Configuration.CodecName);
        return CodecError;
    }

    CodecStatus_t Status = GetCompressedFrameBufferPool(&Cfp);
    if (Status != CodecNoError)
    {
        SE_ERROR("(%s) - Failed to obtain the CompressedFrame buffer pool instance\n", Configuration.CodecName);
        return Status;
    }

    for (int32_t i = 0; i < NoOfCompressedBufferToGet; i++)
    {
        if (CurrentCompressedFrameBuffer[i])
        {
            CurrentCompressedFrameBuffer[i]->DecrementReferenceCount();
            CurrentCompressedFrameBuffer[i] = NULL;
        }


        BufferStatus_t BufferStatus = Cfp->GetBuffer(&CurrentCompressedFrameBuffer[i], IdentifierCodec, Configuration.CompressedFrameMaxSize, false);
        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to obtain a CompressedFrame buffer from the CompressedFramed buffer pool\n", Configuration.CodecName);
            Cfp->Dump(DumpPoolStates | DumpBufferStates);
            return CodecError;
        }

        //
        // Map and initialize the 1st Compressed Frame buffer mapped entry.
        //
        CurrentCompressedFrameBuffer[i]->GetIndex(&CurrentCompressedFrameBufferIndex[i]);

        if (CurrentCompressedFrameBufferIndex[i] >= AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT)
        {
            SE_ERROR("(%s) - buffer index >= AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT - Implementation error\n", Configuration.CodecName);
        }

        memset(&CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]], 0, sizeof(AdditionalBufferState_t));
        CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].Buffer = CurrentCompressedFrameBuffer[i];
        //
        // Obtain the interesting references to the buffer
        //
        CurrentCompressedFrameBuffer[i]->ObtainDataReference(&CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferLength,
                                                             NULL,
                                                             (void **)(&CompressedFrameBuffers[i][CurrentCompressedFrameBufferIndex[i]].BufferPointer),
                                                             Configuration.AddressingMode);
        NoOfCompressedFrameBuffers++;
    }

    return CodecNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The get compressed frame buffer pool fn
//

CodecStatus_t   Codec_MmeAudio_c::GetCompressedFrameBufferPool(BufferPool_t *Cfp)
{
    allocator_status_t      AStatus;

    //
    // If we haven't already created the buffer pool, do it now.
    //

    if (CompressedFramePool == NULL)
    {
        //
        // compressed frame buffer type
        //
        CodecStatus_t Status = InitializeDataType(&InitialCompressedFrameBufferDescriptor, &CompressedFrameBufferType, &CompressedFrameBufferDescriptor);
        if (Status != CodecNoError)
        {
            return Status;
        }

        //
        // Get the memory and Create the pool with it
        //
        Player->GetBufferManager(&BufferManager);

        AStatus = PartitionAllocatorOpen(&CompressedFrameMemoryDevice,
                                         Configuration.TranscodedMemoryPartitionName,
                                         AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT *
                                         Configuration.CompressedFrameMaxSize,
                                         MEMORY_DEFAULT_ACCESS);

        if (AStatus != allocator_ok)
        {
            SE_ERROR("Failed to allocate memory for %s\n", Configuration.CodecName);
            return PlayerInsufficientMemory;
        }

        CompressedFrameMemory[CachedAddress]         = AllocatorUserAddress(CompressedFrameMemoryDevice);
        CompressedFrameMemory[PhysicalAddress]       = AllocatorPhysicalAddress(CompressedFrameMemoryDevice);

        BufferStatus_t BufferStatus = BufferManager->CreatePool(&CompressedFramePool,
                                                                CompressedFrameBufferType,
                                                                AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT,
                                                                AUDIO_DECODER_COMPRESSED_FRAME_BUFFER_COUNT *
                                                                Configuration.CompressedFrameMaxSize,
                                                                CompressedFrameMemory);

        if (BufferStatus != BufferNoError)
        {
            SE_ERROR("(%s) - Failed to create the pool\n", Configuration.CodecName);
            return CodecError;
        }

        ((PlayerStream_c *)Stream)->CompressedFrameBufferType = CompressedFrameBufferType;
    }

    *Cfp = CompressedFramePool;
    return CodecNoError;
}

void  Codec_MmeAudio_c::SetAudioCodecDecStatistics()
{
    // Report the exact AudioMode from the coded stream instead of the AudioMode after pcm-processings
    // but report the SamplingFrequency of the transform output so that it relates to the NbOutSamples,
    // since there is no DecNbOutSamples
    Stream->Statistics().CodecAudioNumOfOutputSamples = AudioDecoderStatus.NbOutSamples;
    Stream->Statistics().CodecAudioCodingMode = AudioDecoderStatus.DecAudioMode;
    Stream->Statistics().CodecAudioSamplingFrequency = StmSeTranslateDiscreteSamplingFrequencyToInteger(AudioDecoderStatus.SamplingFreq);

    // generic handle of status for all codecs
    // Incase of EOF we amy receive muted frame with error status in callback of transfrom command when no Input is available so don't report this error
    if ((AudioDecoderStatus.DecStatus) && (AudioDecoderStatus.PTSflag.Bits.FrameType != STREAMING_DEC_EOF))
    {
        Stream->Statistics().FrameDecodeError++;
    }
}
///////////////////////////////////////////////////////////////////////////
///
///
///
CodecStatus_t   Codec_MmeAudio_c::ConvertFreqToAccCode(int aFrequency, enum eAccFsCode &aAccFreqCode)
{
    // Local variables.
    int retValue = StmSeTranslateIsoSamplingFrequencyToDiscrete((uint32_t) aFrequency, aAccFreqCode);

    if (retValue == -EINVAL)
    {
        // Convert aFrequency to ACC id frequency.
        aAccFreqCode = ACC_FS44k; // Default value.
        SE_ERROR("Bad frequency value. There is no ACC frequency id that corresponds to %d\n", aFrequency);
        return CodecError;
    }

    return CodecNoError;
}

void  Codec_MmeAudio_c::SetAudioCodecDecAttributes()
{
    PlayerAttributeDescriptor_t *Value;
    /* set "sample_frequency" */
    Value = &Stream->Attributes().sample_frequency;
    enum eAccFsCode SamplingFreqCode = (enum eAccFsCode) AudioDecoderStatus.SamplingFreq;
    Value->Id        = SYSFS_ATTRIBUTE_ID_INTEGER;

    if (SamplingFreqCode < ACC_FS_reserved)
    {
        Value->u.Int = (int) StmSeTranslateDiscreteSamplingFrequencyToInteger(SamplingFreqCode);
    }
    else
    {
        Value->u.Int = 0;
    }

    /* set "number_channels" */
    Value = &Stream->Attributes().number_channels;
#define C(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## _LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break
#define Cn(f,b) case ACC_MODE ## f ## b: Value->u.ConstCharPointer = #f "/" #b ".0"; break
#define Ct(f,b) case ACC_MODE ## f ## b ## t: Value->u.ConstCharPointer = #f "/" #b ".0"; break; case ACC_MODE ## f ## b ## t_LFE: Value->u.ConstCharPointer = #f "/" #b ".1"; break
    Value->Id = SYSFS_ATTRIBUTE_ID_CONSTCHARPOINTER;

    switch ((enum eAccAcMode)(AudioDecoderStatus.DecAudioMode))
    {
        Ct(2, 0);
        C(1, 0);
        C(2, 0);
        C(3, 0);
        C(2, 1);
        C(3, 1);
        C(2, 2);
        C(3, 2);
        C(2, 3);
        C(3, 3);
        C(2, 4);
        C(3, 4);
        Cn(4, 2);
        Cn(4, 4);
        Cn(5, 2);
        Cn(5, 3);

    default:
        Value->u.ConstCharPointer = "UNKNOWN";
    }
}

void Codec_MmeAudio_c::CheckAudioParameterEvent(MME_LxAudioDecoderFrameStatus_t *DecFrameStatus, MME_PcmProcessingFrameExtStatus_t *PcmExtStatus,
                                                stm_se_play_stream_audio_parameters_t *newAudioParametersValues, PlayerEventRecord_t  *myEvent)
{
    PlayerStatus_t            myStatus;
    enum eAccAcMode receivedDecAudioMode;
    enum eAccFsCode receivedDecSamplingFreq;
    enum eAccBoolean receivedEmphasis;
    receivedDecAudioMode     = (enum eAccAcMode) DecFrameStatus->DecAudioMode;
    receivedDecSamplingFreq  = (enum eAccFsCode) DecFrameStatus->DecSamplingFreq;
    receivedEmphasis         = (enum eAccBoolean) DecFrameStatus->Emphasis;
    // Update newAudioParametersValues for common codec parameters (specific codec parameters have been already updated in the calling function)
    // audio_coding_type parameter initialized with codec name in each codec constructor
    // we just need to retrieve the previous value
    // special case for spdfin codec
    newAudioParametersValues->audio_coding_type = AudioParametersEvents.audio_coding_type;

    // get & convert sampling_freq parameter
    if (receivedDecSamplingFreq < ACC_FS_reserved)
    {
        newAudioParametersValues->sampling_freq = StmSeTranslateDiscreteSamplingFrequencyToInteger(receivedDecSamplingFreq);
    }
    else
    {
        newAudioParametersValues->sampling_freq = 0;
        SE_ERROR("Invalid DecSamplingFreq value received\n");
    }

    // get & convert num_channels parameter
    newAudioParametersValues->num_channels = Codec_MmeAudio_c::GetNumberOfChannelsFromAudioConfiguration(receivedDecAudioMode);
    // get & convert channel_assignment parameter
    actualDecAudioMode = receivedDecAudioMode;
    newAudioParametersValues->channel_assignment = TranslateAudioModeToChannelAssignment(receivedDecAudioMode);
    // get & convert dual_mono parameter
    newAudioParametersValues->dual_mono = (ACC_MODE_1p1 == receivedDecAudioMode);
    // get bitrate and copyright parameters
    GetAudioDecodedStreamInfo(PcmExtStatus, newAudioParametersValues);
    // get emphasis parameter
    newAudioParametersValues->emphasis = (ACC_MME_TRUE == receivedEmphasis) ? 1 : 0;

    // check if there was an update
    if (0 != memcmp(&AudioParametersEvents, newAudioParametersValues, sizeof(stm_se_play_stream_audio_parameters_t)))
    {
        SE_DEBUG(group_decoder_audio, "AudioParametersEvents updated\n");
        // update AudioParametersEvents with new values
        AudioParametersEvents = *newAudioParametersValues;
        SE_DEBUG(group_decoder_audio,
                 "<New EventSourceAudioParameters>: AudioCodingType=%d SamplingFreq=%d NumChannels=%d DualMono=%d ChannelAssign[%d-%d-%d-%d-%d malleable(%d)] Bitrate=%d Copyright=%d Emphasis=%d\n",
                 AudioParametersEvents.audio_coding_type,
                 AudioParametersEvents.sampling_freq,
                 AudioParametersEvents.num_channels,
                 AudioParametersEvents.dual_mono,
                 AudioParametersEvents.channel_assignment.pair0,
                 AudioParametersEvents.channel_assignment.pair1,
                 AudioParametersEvents.channel_assignment.pair2,
                 AudioParametersEvents.channel_assignment.pair3,
                 AudioParametersEvents.channel_assignment.pair4,
                 AudioParametersEvents.channel_assignment.malleable,
                 AudioParametersEvents.bitrate,
                 AudioParametersEvents.copyright,
                 AudioParametersEvents.emphasis
                );
        // identify the event
        myEvent->Code      = EventSourceAudioParametersChange;
        myEvent->Playback  = Playback;
        myEvent->Stream    = Stream;
        // set event common codec parameters
        // audio_coding_type, sampling_freq, num_channels, dual_mono
        myEvent->Value[0].UnsignedInt = AudioParametersEvents.audio_coding_type;
        myEvent->Value[1].UnsignedInt = AudioParametersEvents.sampling_freq;
        myEvent->Value[2].UnsignedInt = AudioParametersEvents.num_channels;
        myEvent->Value[3].UnsignedInt = AudioParametersEvents.dual_mono;
        //channel_assignment
        myEvent->Value[4].UnsignedInt = AudioParametersEvents.channel_assignment.pair0;
        myEvent->Value[5].UnsignedInt = AudioParametersEvents.channel_assignment.pair1;
        myEvent->Value[6].UnsignedInt = AudioParametersEvents.channel_assignment.pair2;
        myEvent->Value[7].UnsignedInt = AudioParametersEvents.channel_assignment.pair3;
        myEvent->Value[8].UnsignedInt = AudioParametersEvents.channel_assignment.pair4;
        myEvent->Value[9].UnsignedInt = AudioParametersEvents.channel_assignment.malleable;
        // birate, copyright, emphasis
        myEvent->Value[10].UnsignedInt = AudioParametersEvents.bitrate;
        myEvent->Value[11].UnsignedInt = AudioParametersEvents.copyright;
        myEvent->Value[12].UnsignedInt = AudioParametersEvents.emphasis;
        // codec specific parameters have been already updated in the calling function : myEvent->ExtValue[]
        myStatus            = Stream->SignalEvent(myEvent);

        if (myStatus != PlayerNoError)
        {
            SE_ERROR("bug during SignalEvent treatment\n");
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Decoding of the structure MME_PcmProcessingFrameExtStatus_t to get MME_LxAudioProcessInfo_t
/// Used to get bitrate & copyright info
/// (MME_LxAudioProcessInfo_t is provided for all codecs)
///
void Codec_MmeAudio_c::GetAudioDecodedStreamInfo(MME_PcmProcessingFrameExtStatus_t *PcmExtStatus, stm_se_play_stream_audio_parameters_t *audioparams)
{
    MME_PcmProcessingStatusTemplate_t *Pcmstatus;
    MME_LxAudioProcessInfo_t *AudDecStreamInfo_p;
    U32 SizeofPCMStatus = PcmExtStatus->BytesUsed;
    U32 ByteUsed;

    for (ByteUsed = 0; ByteUsed < SizeofPCMStatus;)
    {
        Pcmstatus = (MME_PcmProcessingStatusTemplate_t *)((U8 *) & (PcmExtStatus->PcmStatus) + ByteUsed);

        if ((Pcmstatus->Id >> 8)  == ACC_PARSER_MT)
        {
            if ((Pcmstatus->Id == 0) || (Pcmstatus->StructSize > (SizeofPCMStatus - ByteUsed)))
            {
                ByteUsed = SizeofPCMStatus;
            }
            else
            {
                ByteUsed += Pcmstatus->StructSize;
                AudDecStreamInfo_p = (MME_LxAudioProcessInfo_t *)Pcmstatus;
                audioparams->bitrate   = (int) AudDecStreamInfo_p->BitRate;
                audioparams->copyright = (bool) AudDecStreamInfo_p->IsCopyright;
            }
        }
        else
        {
            ByteUsed += Pcmstatus->StructSize;
        }
    }
}

////////////////////////////////////////////////////////////////////////////
///
/// Lookup the 'natural' channel assignment for an audio mode.
///
/// This method and Mixer_Mme_c::TranslateChannelAssignmentToAudioMode are *not* reversible for
/// audio modes that are not marked as suitable for output.
///
struct stm_se_audio_channel_assignment Codec_MmeAudio_c::TranslateAudioModeToChannelAssignment(
    enum eAccAcMode AudioMode)
{
    stm_se_audio_channel_assignment TmpAssignment;
    struct stm_se_audio_channel_assignment Zeros = { 0 };
    TmpAssignment = Zeros;

    if (ACC_MODE_ID == AudioMode)
    {
        Zeros.malleable = 1;
        return Zeros;
    }

    if (0 != StmSeAudioGetChannelAssignmentFromAcMode(&TmpAssignment, AudioMode))
    {
        SE_ERROR("StmSeAudioGetAcModeFromChannelAssignment Failed\n");
        return Zeros;
    }

    if ((STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == TmpAssignment.pair0)
        && (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == TmpAssignment.pair1)
        && (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == TmpAssignment.pair2)
        && (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == TmpAssignment.pair3)
        && (STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED == TmpAssignment.pair4))
    {
        // no point in stringizing the mode - we know its not in the table
        SE_ERROR("Cannot find matching audio mode (%d)\n", AudioMode);
        return Zeros;
    }

    // Match Found
    return TmpAssignment;
}

///////////////////////////////////////////////////////////////////////////
///
/// Disable the transcoding in case of dual decode for MS11/MS12 mode.
/// The enabling will depend on the specfic codec type
///
void   Codec_MmeAudio_c::DisableTranscodingBasedOnProfile(void)
{
    unsigned int AudioServiceType = Player->PolicyValue(Playback, Stream, PolicyAudioServiceType);
    unsigned int AudioApplicationType = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);
    PlayerStream_t SecondaryStream = Playback->GetSecondaryStream();

    if ((NULL != SecondaryStream) || ((NULL == SecondaryStream) && (PolicyValueAudioServiceMainAndAudioDescription == AudioServiceType)))
    {
        // We have a secondary stream and the current stream also doesn't do main and AD decoding
        // Or we have only a single stream but it does dual decode in case of policy set to
        //    PolicyValueAudioServiceMainAndAudioDescription
        // So we have a dual decode scenario
        // do we have an MS11/MS12 use case?
        switch (AudioApplicationType)
        {
        case PolicyValueAudioApplicationMS11:
        case PolicyValueAudioApplicationMS12:
            // We don't do transcoding in this case
            TranscodeEnable = false;
            break;

        default:
            break;
        }
    }

    // If the surrent stream is secondary, then disable transcoding anyway
    switch (AudioServiceType)
    {
    case STM_SE_CTRL_VALUE_AUDIO_SERVICE_SECONDARY:
    case STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_DESCRIPTION:
        TranscodeEnable = false;
        break;

    default:
        break;
    }

    SE_DEBUG(group_decoder_audio, "Stream[%p]: transcode= %d, serv:%u,app:%u\n", Stream, TranscodeEnable, AudioServiceType, AudioApplicationType);
}

////////////////////////////////////////////////////////////////////////////
///
/// Enable the AuxBuffer in case of  MS11 single decode mode.
/// The enabling will depend on the specfic codec type
///
void   Codec_MmeAudio_c::EnableAuxBufferBasedOnProfile(void)
{
    unsigned int AudioServiceType = Player->PolicyValue(Playback, Stream, PolicyAudioServiceType);
    unsigned int AudioApplicationType = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);
    PlayerStream_t SecondaryStream = Playback->GetSecondaryStream();
    // Reset the AuxOutputEnable flag for runtime handling of the AuxBuffer (eg: When playback mode changes from
    // Single Decode to Dual Decode)
    AuxOutputEnable = false;

    if ((NULL == SecondaryStream) && (PolicyValueAudioServiceMainAndAudioDescription != AudioServiceType))
    {
        switch (AudioApplicationType)
        {
        case PolicyValueAudioApplicationMS11:
        case PolicyValueAudioApplicationMS12:
            // Enable the Aux Output in this case
            AuxOutputEnable = true;
            break;
        default:
            break;
        }
    }

    SE_DEBUG(group_decoder_audio, "Stream[%p]: auxbufferenable:%d, serv:%u,app:%u\n", Stream, TranscodeEnable, AudioServiceType, AudioApplicationType);
}

///////////////////////////////////////////////////////////////////////////
///
/// Downmix promotion evaluation
///
void Codec_MmeAudio_c::EvaluateImplicitDownmixPromotion(void)
{
    stm_se_audio_channel_assignment_t channelAssignment;
    PlayerStatus_t playerResult = Player->GetControl(Playback, Stream, STM_SE_CTRL_SPEAKER_CONFIG, (void *) &channelAssignment);
    unsigned int AudioServiceType = Player->PolicyValue(Playback, Stream, PolicyAudioServiceType);
    unsigned int AudioApplicationType = Player->PolicyValue(Playback, Stream, PolicyAudioApplicationType);
    PlayerStream_t SecondaryStream = Playback->GetSecondaryStream();
    bool IsSingleDecode = (NULL == SecondaryStream) && (PolicyValueAudioServiceMainAndAudioDescription != AudioServiceType) ? true : false;

    if (PlayerMatchNotFound == playerResult)
    {
        // no STM_SE_CTRL_SPEAKER_CONFIG control applied to this playstream,
        // Query all connected Manifestors for their expected ChannelConfiguration
        // If all Manifestors ChannelConfiguration are identical,
        // we can perform the downmix/upmix operation at codec level
        enum eAccAcMode AcMode, FinalAcMode = ACC_MODE_ID, FinalAuxAcMode = ACC_MODE_ID;
        bool FirstManifestor = true;

        Stream->GetHavanaStream()->LockManifestors();

        Manifestor_t *manifestorArray = Stream->GetHavanaStream()->GetManifestors();

        for (int i = 0; i < MAX_MANIFESTORS; i++)
        {
            if (manifestorArray[i] != NULL)
            {
                unsigned int capabilities;
                manifestorArray[i]->GetCapabilities(&capabilities);

                if (capabilities & MANIFESTOR_CAPABILITY_ENCODE)
                {
                    class Manifestor_EncodeAudio_c *manifestorEncAudio = NULL;
                    manifestorEncAudio = (class Manifestor_EncodeAudio_c *) manifestorArray[i];
                    manifestorEncAudio->GetChannelConfiguration(&AcMode);
                }
                else if ((capabilities & MANIFESTOR_CAPABILITY_DISPLAY) || (capabilities & MANIFESTOR_CAPABILITY_GRAB))
                {
                    class Manifestor_Audio_c *manifestorAudio = NULL;
                    manifestorAudio = (class Manifestor_Audio_c *) manifestorArray[i];
                    manifestorAudio->GetChannelConfiguration(&AcMode);
                }
                else if (capabilities & MANIFESTOR_CAPABILITY_SOURCE)
                {
                    class Manifestor_AudioSrc_c *manifestorAudioSrc;
                    manifestorAudioSrc = (class Manifestor_AudioSrc_c *) manifestorArray[i];
                    manifestorAudioSrc->GetChannelConfiguration(&AcMode);
                }
                else
                {
                    // This is an unknown manifestor
                    // So lets use ACC_MODE_ID as AcMode for this manifest.
                    AcMode = ACC_MODE_ID;
                    SE_DEBUG(group_decoder_audio, "Unknown capability[%d] for manifestor[%d]. Setting AcMode:%s\n", capabilities, i, StmSeAudioAcModeGetName(AcMode));
                }

                SE_DEBUG(group_decoder_audio, "manifestor[%d] AccAcMode:%s\n", i, StmSeAudioAcModeGetName(AcMode));

                if (FirstManifestor)
                {
                    FirstManifestor = false;
                    FinalAcMode = AcMode;
                }
                else if (FinalAcMode != AcMode)
                {
                    // The following condition populates the OutmodeAux variable with the acmode of that manifestor which has stereo output
                    // channel configuration in case there are multiple manifestors with different audio coding modes
                    if (FinalAuxAcMode == ACC_MODE_ID)
                    {
                        if (FinalAcMode == ACC_MODE20 || FinalAcMode == ACC_MODE20t)
                        {
                            FinalAuxAcMode = FinalAcMode;
                        }
                        else if (AcMode == ACC_MODE20 || AcMode == ACC_MODE20t)
                        {
                            FinalAuxAcMode = AcMode;
                        }
                    }
                    FinalAcMode = ACC_MODE_ID;
                    if (FinalAuxAcMode != ACC_MODE_ID)
                    {
                        // Atleast one stereo manifestor found
                        if (AcMode == ACC_MODE20 || AcMode == ACC_MODE20t)
                        {
                            if (FinalAuxAcMode != AcMode)
                            {
                                // Atleast one manifestor has a different stereo mode than the first stereo manifestor
                                FinalAuxAcMode = ACC_MODE_ID;
                                break;
                            }
                        }
                    }
                }
            }
        }

        Stream->GetHavanaStream()->UnlockManifestors();

        FinalAuxAcMode = IsSingleDecode && (PolicyValueAudioApplicationMS11 == AudioApplicationType) ? FinalAuxAcMode : ACC_MODE_ID;

        if (FinalAcMode != OutmodeMain)
        {
            SE_DEBUG(group_decoder_audio, "Change Channel Config to:%s\n", StmSeAudioAcModeGetName(FinalAcMode));
            OutmodeMain = FinalAcMode;
            ForceStreamParameterReload = true;
        }

        if (FinalAuxAcMode != OutmodeAux)
        {
            SE_DEBUG(group_decoder_audio, "Change Aux Channel Config to:%s\n", StmSeAudioAcModeGetName(FinalAuxAcMode));
            OutmodeAux = FinalAuxAcMode;
            ForceStreamParameterReload = true;
        }
    }
    else
    {
        SE_DEBUG(group_decoder_audio,  "codec OutmodeMain already set by playstream\n");
    }
}

///////////////////////////////////////////////////////////////////////////
///
/// Evaluate the need to generate a Transcoded Buffer and Compressed Buffer
/// This method is responsible to update TranscodeNeeded and CompressedFrameNeeded members.
///
void Codec_MmeAudio_c::EvaluateTranscodeCompressedNeeded(void)
{
    bool IsTranscodeNeeded       = false;
    bool IsCompressedFrameNeeded = false;

    Stream->GetHavanaStream()->LockManifestors();

    Manifestor_t *manifestorArray = Stream->GetHavanaStream()->GetManifestors();
    for (int i = 0; i < MAX_MANIFESTORS; i++)
    {
        if (manifestorArray[i] != NULL)
        {
            unsigned int capabilities;
            manifestorArray[i]->GetCapabilities(&capabilities);

            if (capabilities & MANIFESTOR_CAPABILITY_DISPLAY)
            {
                class Manifestor_Audio_c *manifestorAudio = NULL;
                manifestorAudio = (class Manifestor_Audio_c *) manifestorArray[i];
                IsTranscodeNeeded |= manifestorAudio->IsTranscodeNeeded();
                IsCompressedFrameNeeded |=  manifestorAudio->IsCompressedFrameNeeded();
            }
        }
    }

    Stream->GetHavanaStream()->UnlockManifestors();

    if (TranscodeNeeded != IsTranscodeNeeded)
    {
        SE_DEBUG(group_decoder_audio, "Change to TranscodeNeeded=%s\n", IsTranscodeNeeded ? "true" : "false");
        TranscodeNeeded = IsTranscodeNeeded;
        ForceStreamParameterReload = true;
    }
    if (CompressedFrameNeeded != IsCompressedFrameNeeded)
    {
        SE_DEBUG(group_decoder_audio, "Change to CompressedFrameNeeded=%s\n", IsCompressedFrameNeeded ? "true" : "false");
        CompressedFrameNeeded = IsCompressedFrameNeeded;
        ForceStreamParameterReload = true;
    }

}

///////////////////////////////////////////////////////////////////////////
///
/// DRC parameters evaluation
/// Query all connected ksound_manifestors for their Mixer DRC paramaters
/// in order to apply them at codec level
///
void Codec_MmeAudio_c::EvaluateMixerDRCParameters(void)
{
    // Query all connected Manifestors for their expected DRC parameters
    unsigned int capabilities;
    Stream->GetHavanaStream()->LockManifestors();

    Manifestor_t *manifestorArray = Stream->GetHavanaStream()->GetManifestors();

    for (int i = 0; i < MAX_MANIFESTORS; i++)
    {
        if (manifestorArray[i] != NULL)
        {
            manifestorArray[i]->GetCapabilities(&capabilities);

            if (capabilities & MANIFESTOR_CAPABILITY_DISPLAY)
            {
                DRCParams_t MixerDRCparams = DRC;
                class Manifestor_Audio_c *manifestorAudio = NULL;
                manifestorAudio = (class Manifestor_Audio_c *) manifestorArray[i];
                manifestorAudio->GetDRCParams(&MixerDRCparams);
                if (memcmp(&DRC, &MixerDRCparams, sizeof(DRC)))
                {
                    DRC = MixerDRCparams;
                    SE_DEBUG(group_decoder_audio, "Setting DRC to MAIN{Enable:%d Type:%d HDR:%d LDR:%d}\n",
                             DRC.DRC_Enable, DRC.DRC_Type, DRC.DRC_HDR, DRC.DRC_LDR);
                    ForceStreamParameterReload = true;
                }
            }
            else
            {
                SE_VERBOSE(group_decoder_audio, "Out of interests manifestor\n");
            }
        }
    }
    Stream->GetHavanaStream()->UnlockManifestors();
}
