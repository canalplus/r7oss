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

#include "player.h"
#include "collator_pes_video_wmv.h"
#include "collator_pes_video_vc1.h"
#include "collator2_pes_video_mvc.h"
#include "collator2_pes_video_mpeg2.h"
#include "collator2_pes_video_h264.h"
#include "collator2_pes_video_hevc.h"
#include "collator_pes_video_divx.h"
#include "collator_video_uncompressed.h"
#include "collator_pes_video_h263.h"
#include "collator_pes_video_flv1.h"
#include "collator_pes_video_vp6.h"
#include "collator_pes_video_rmv.h"
#include "collator_pes_video_theora.h"
#include "collator_pes_video_avs.h"
#include "collator_pes_video_mjpeg.h"
#include "collator_pes_video_vp8.h"
#include "collator_pes_audio_aac.h"
#include "collator_pes_audio_mpeg.h"
#include "collator_pes_audio_eac3.h"
#include "collator_pes_audio_dtshd.h"
#include "collator_pes_audio_lpcm.h"
#include "collator_pes_audio_adpcm.h"
#include "collator_pes_audio_wma.h"
#include "collator_pes_audio_mlp.h"
#include "collator_pes_audio_rma.h"
#include "collator_pes_audio_vorbis.h"
#include "collator_pes_audio_pcm.h"
#include "collator_pes_audio_dra.h"

#include "frame_parser_video_mpeg2.h"
#include "frame_parser_video_h264.h"
#include "frame_parser_video_h264_mvc.h"
#include "frame_parser_video_hevc.h"
#include "frame_parser_video_vc1.h"
#include "frame_parser_video_wmv.h"
#include "frame_parser_video_vc1_rp227spmp.h"
#include "frame_parser_video_divx.h"
#include "frame_parser_video_divx_hd.h"
#include "frame_parser_video_uncompressed.h"
#include "frame_parser_video_h263.h"
#include "frame_parser_video_flv1.h"
#include "frame_parser_video_vp6.h"
#include "frame_parser_video_rmv.h"
#include "frame_parser_video_theora.h"
#include "frame_parser_video_avs.h"
#include "frame_parser_video_mjpeg.h"
#include "frame_parser_video_vp8.h"
#include "frame_parser_audio_aac.h"
#include "frame_parser_audio_mpeg.h"
#include "frame_parser_audio_eac3.h"
#include "frame_parser_audio_dtshd.h"
#include "frame_parser_audio_lpcm.h"
#include "frame_parser_audio_adpcm.h"
#include "frame_parser_audio_wma.h"
#include "frame_parser_audio_mlp.h"
#include "frame_parser_audio_rma.h"
#include "frame_parser_audio_vorbis.h"
#include "frame_parser_audio_pcm.h"
#include "frame_parser_audio_dra.h"

#include "codec_mme_video_mpeg2.h"
#include "codec_mme_video_h264.h"
#include "codec_mme_video_h264_mvc.h"
#include "codec_mme_video_hevc.h"
#include "codec_mme_video_vc1.h"
#include "codec_mme_video_divx_hd.h"
#include "codec_mme_video_h263.h"
#include "codec_mme_video_flv1.h"
#include "codec_mme_video_vp6.h"
#include "codec_mme_video_rmv.h"
#include "codec_mme_video_theora.h"
#include "codec_mme_video_avs.h"
#include "codec_mme_video_mjpeg.h"
#include "codec_mme_video_vp8.h"
#include "codec_uncompressed_video.h"
#include "codec_mme_audio_aac.h"
#include "codec_mme_audio_mpeg.h"
#include "codec_mme_audio_eac3.h"
#include "codec_mme_audio_dtshd.h"
#include "codec_mme_audio_lpcm.h"
#include "codec_mme_audio_adpcm.h"
#include "codec_mme_audio_wma.h"
#include "codec_mme_audio_mlp.h"
#include "codec_mme_audio_rma.h"
#include "codec_mme_audio_vorbis.h"
#include "codec_mme_audio_spdifin.h"
#include "codec_mme_audio_silence.h"
#include "codec_mme_audio_pcm.h"
#include "codec_mme_audio_dra.h"

#include "output_timer_video.h"
#include "output_timer_audio.h"

#include "decode_buffer_manager_base.h"
#include "manifestation_coordinator_base.h"

#include "manifestor_video_stmfb.h"
#include "manifestor_audio_ksound.h"


#include "havana_player.h"
#include "player_factory.h"
#undef TRACE_TAG
#define TRACE_TAG   "PlayerFactory"

// TODO(pht) move to new + FinalizeInit() pattern

//{{{  Collator factories
static void *CollatorPesVideoMpeg2Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator2_PesVideoMpeg2_c();
}

static void *CollatorPesVideoH264Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator2_PesVideoH264_c();
}

static void *CollatorPesVideoH264_MVCFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator2_PesVideoMVC_c();
}

static void *CollatorPesVideoHevcFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator2_PesVideoHevc_c();
}

static void *CollatorPesVideoVc1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoVc1_c();
}

static void *CollatorPesVideoWmvFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoWmv_c();
}

static void *CollatorPesVideoDivxFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoDivx_c();
}

static void *CollatorVideoUncompressedFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_VideoUncompressed_c();
}

static void *CollatorPesVideoH263Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoH263_c();
}

static void *CollatorPesVideoFlv1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoFlv1_c();
}

static void *CollatorPesVideoVp6Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoVp6_c();
}

static void *CollatorPesVideoRmvFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoRmv_c();
}

static void *CollatorPesVideoTheoraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoTheora_c();
}

static void *CollatorPesVideoAvsFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoAvs_c();
}

static void *CollatorPesVideoMjpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoMjpeg_c();
}

static void *CollatorPesVideoVp8Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesVideoVp8_c();
}

static void *CollatorPesAudioAacFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioAac_c();
}

static void *CollatorPesAudioMpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioMpeg_c();
}

static void *CollatorPesAudioEAc3Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioEAc3_c();
}

static void *CollatorPesAudioDtshdFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioDtshd_c();
}

static void *CollatorPesAudioLpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioLpcm_c(TypeLpcmDVDVideo);
}

static void *CollatorPesAudioLpcmAFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioLpcm_c(TypeLpcmDVDAudio);
}

static void *CollatorPesAudioMsAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioAdpcm_c(TypeMsAdpcm);
}

static void *CollatorPesAudioImaAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioAdpcm_c(TypeImaAdpcm);
}


static void *CollatorPesAudioLpcmHFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioLpcm_c(TypeLpcmDVDHD);
}

static void *CollatorPesAudioLpcmBFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioLpcm_c(TypeLpcmDVDBD);
}

static void *CollatorPesAudioWmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioWma_c();
}

static void *CollatorPesAudioLpcmSFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioLpcm_c(TypeLpcmSPDIFIN);
}

static void *CollatorPesAudioMlpFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioMlp_c();
}
static void *CollatorPesAudioRmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioRma_c();
}
static void *CollatorPesAudioVorbisFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioVorbis_c();
}

static void *CollatorPesAudioPcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioPcm_c();
}

static void *CollatorPesAudioDraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Collator_PesAudioDra_c();
}

//}}}
//{{{  Frame parser factories
static void *FrameParserVideoMpeg2Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoMpeg2_c();
}

static void *FrameParserVideoH264Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoH264_c();
}

static void *FrameParserVideoH264_MVCFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoH264_MVC_c();
}

static void *FrameParserVideoHevcFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoHevc_c();
}

static void *FrameParserVideoVc1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoVc1_c();
}

static void *FrameParserVideoWmvFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoWmv_c();
}

static void *FrameParserVideoVc1Rp227SpMpFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoVc1_Rp227SpMp_c();
}

static void *FrameParserVideoDivxHdFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoDivxHd_c();
}

static void *FrameParserVideoUncompressedFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoUncompressed_c();
}

static void *FrameParserVideoH263Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoH263_c();
}

static void *FrameParserVideoFlv1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoFlv1_c();
}

static void *FrameParserVideoVp6Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoVp6_c();
}

static void *FrameParserVideoRmvFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoRmv_c();
}

static void *FrameParserVideoTheoraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoTheora_c();
}

static void *FrameParserVideoAvsFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoAvs_c();
}

static void *FrameParserVideoMjpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoMjpeg_c();
}

static void *FrameParserVideoVp8Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_VideoVp8_c();
}

static void *FrameParserAudioAacFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioAac_c();
}

static void *FrameParserAudioMpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioMpeg_c();
}

static void *FrameParserAudioEAc3Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioEAc3_c();
}

static void *FrameParserAudioDtshdFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioDtshd_c();
}

static void *FrameParserAudioLpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioLpcm_c();
}

static void *FrameParserAudioLpcmAFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioLpcm_c();
}
static void *FrameParserAudioMsAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioAdpcm_c();
}

static void *FrameParserAudioImaAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioAdpcm_c();
}

static void *FrameParserAudioLpcmHFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioLpcm_c();
}

static void *FrameParserAudioLpcmBFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioLpcm_c();
}

static void *FrameParserAudioWmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioWma_c();
}

/// \todo Magic constant for SPDIF latency.
///       SPDIFin has an instrinsic 8192 sample latency. Ideally the codec would configure the frame parser with the
//        latency using SetModuleParameters() but this is good enough for now.
static void *FrameParserAudioSpdifinFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioLpcm_c(8192);
}

static void *FrameParserAudioMlpFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioMlp_c();
}

static void *FrameParserAudioRmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioRma_c();
}
static void *FrameParserAudioVorbisFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioVorbis_c();
}

static void *FrameParserAudioPcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioPcm_c();
}

static void *FrameParserAudioDraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new FrameParser_AudioDra_c();
}

//}}}
//{{{  Codec factories
static void *CodecMMEVideoMpeg2Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoMpeg2_c();
}

static void *CodecMMEVideoH264Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoH264_c();
}

static void *CodecMMEVideoH264_MVCFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoH264_MVC_c();
}

static void *CodecMMEVideoHevcFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoHevc_c();
}

#if 0
static void *CodecMMEVideoDivxFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoDivx_c();
}
#endif

static void *CodecMMEVideoDivxHdFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoDivxHd_c();
}

static void *CodecUncompressedVideoFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_UncompressedVideo_c();
}

static void *CodecMMEVideoVc1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoVc1_c();
}

static void *CodecMMEVideoH263Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoH263_c();
}

static void *CodecMMEVideoFlv1Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoFlv1_c();
}

static void *CodecMMEVideoVp6Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoVp6_c();
}

static void *CodecMMEVideoRmvFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoRmv_c();
}

static void *CodecMMEVideoTheoraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoTheora_c();
}

static void *CodecMMEVideoAvsFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoAvs_c();
}

static void *CodecMMEVideoMjpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoMjpeg_c();
}

static void *CodecMMEVideoVp8Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeVideoVp8_c();
}

static void *CodecMMEAudioSilenceConditionalFactory(Codec_c *Codec)
{
    if (CodecNoError != Codec->InitializationStatus)
    {
        delete Codec;
        Codec = new Codec_MmeAudioSilence_c();
    }

    return Codec;
}

static void *CodecMMEAudioAacFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioAac_c();
}

static void *CodecMMEAudioMpegFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioMpeg_c();
}

static void *CodecMMEAudioEAc3Factory(void)
{
    SE_DEBUG(group_player, "\n");
    return CodecMMEAudioSilenceConditionalFactory(new Codec_MmeAudioEAc3_c());
}

static void *CodecMMEAudioDtshdFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return CodecMMEAudioSilenceConditionalFactory(new Codec_MmeAudioDtshd_c(false));
}

static void *CodecMMEAudioLpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioLpcm_c();
}

static void *CodecMMEAudioLpcmAFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioLpcm_c();
}
static void *CodecMMEAudioMsAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioAdpcm_c();
}


static void *CodecMMEAudioImaAdpcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioAdpcm_c();
}

static void *CodecMMEAudioLpcmHFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioLpcm_c();
}

static void *CodecMMEAudioLpcmBFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioLpcm_c();
}

static void *CodecMMEAudioWmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioWma_c();
}

static void *CodecMMEAudioMlpFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioMlp_c();
}
static void *CodecMMEAudioSpdifinFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioSpdifin_c();
}

static void *CodecMMEAudioDtshdLbrFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioDtshd_c(true);
}
static void *CodecMMEAudioRmaFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioRma_c();
}
static void *CodecMMEAudioVorbisFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioVorbis_c();
}
#if 0
static void *CodecMMEAudioAvsFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioAvs_c();
}
#endif
static void *CodecMMEAudioPcmFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioPcm_c();
}

static void *CodecMMEAudioDraFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Codec_MmeAudioDra_c();
}

//}}}
//{{{  Manifestor factories
static void *ManifestorVideoStmfbFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Manifestor_VideoStmfb_c();
}

static void *ManifestorAudioFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new Manifestor_AudioKsound_c();
}
//}}}
//{{{  Output timer factories
static void *OutputTimerVideoFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new OutputTimer_Video_c();
}

static void *OutputTimerAudioFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new OutputTimer_Audio_c();
}
//}}}
//{{{  Decode buffer manager factory
static void *DecodeBufferManagerFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new DecodeBufferManager_Base_c();
}
//}}}
//{{{  Decode buffer manager factory
static void *ManifestationCoordinatorFactory(void)
{
    SE_DEBUG(group_player, "\n");
    return new ManifestationCoordinator_Base_c();
}

static inline void *DefaultFactory(void)
{
    SE_ERROR("No factory available\n");
    return NULL;
}

//{{{  RegisterBuiltInFactories
HavanaStatus_t RegisterBuiltInFactories(class  HavanaPlayer_c *HavanaPlayer)
{
    HavanaStatus_t      Status = HavanaNoError;
    SE_DEBUG(group_player, "\n");
    // generic video components
    HavanaPlayer->RegisterFactory(ComponentOutputTimer, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_NONE,              0, false, OutputTimerVideoFactory);
    HavanaPlayer->RegisterFactory(ComponentDecodeBufferManager, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_NONE,      0, false, DecodeBufferManagerFactory);
    HavanaPlayer->RegisterFactory(ComponentManifestationCoordinator, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_NONE, 0, false, ManifestationCoordinatorFactory);
    HavanaPlayer->RegisterFactory(ComponentManifestor,  StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_NONE,              0, false, ManifestorVideoStmfbFactory);
    // MPEG1 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG1,             0, false, CodecMMEVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG1,             0, false, FrameParserVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG1,             0, false, CollatorPesVideoMpeg2Factory);
    // MPEG2 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG2,             0, false, CodecMMEVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG2,             0, false, FrameParserVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG2,             0, false, CollatorPesVideoMpeg2Factory);
    // H264 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H264,              0, false, CodecMMEVideoH264Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H264,              0, false, FrameParserVideoH264Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H264,              0, false, CollatorPesVideoH264Factory);
    // MVC video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MVC,               0, false, CodecMMEVideoH264_MVCFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MVC,               0, false, FrameParserVideoH264_MVCFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MVC,               0, false, CollatorPesVideoH264_MVCFactory);
    // Hevc video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_HEVC,              0, false, CodecMMEVideoHevcFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_HEVC,              0, false, FrameParserVideoHevcFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_HEVC,              0, false, CollatorPesVideoHevcFactory);
    // VC1 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1,               0, false, CodecMMEVideoVc1Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1,               0, false, FrameParserVideoVc1Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1,               0, false, CollatorPesVideoVc1Factory);
    // WMV video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_WMV,               0, false, CodecMMEVideoVc1Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_WMV,               0, false, FrameParserVideoWmvFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_WMV,               0, false, CollatorPesVideoWmvFactory);
    // VC1 RP227 SP/MP video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP,     0, false, CodecMMEVideoVc1Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP,     0, false, CollatorPesVideoVc1Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP,     0, false, FrameParserVideoVc1Rp227SpMpFactory);
    // DivX video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG4P2,           0, false, CodecMMEVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG4P2,           0, false, FrameParserVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MPEG4P2,           0, false, CollatorPesVideoDivxFactory);
    // DivX HD video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DIVXHD,            0, false, CodecMMEVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DIVXHD,            0, false, FrameParserVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DIVXHD,            0, false, CollatorPesVideoDivxFactory);
    // H263 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H263,              0, false, CodecMMEVideoH263Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H263,              0, false, FrameParserVideoH263Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_H263,              0, false, CollatorPesVideoH263Factory);
    // Flv1 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_FLV1,              0, false, CodecMMEVideoFlv1Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_FLV1,              0, false, FrameParserVideoFlv1Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_FLV1,              0, false, CollatorPesVideoFlv1Factory);
    // Vp6 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP6,               0, false, CodecMMEVideoVp6Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP6,               0, false, FrameParserVideoVp6Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP6,               0, false, CollatorPesVideoVp6Factory);
    // Rmv video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RMV,               0, false, CodecMMEVideoRmvFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RMV,               0, false, FrameParserVideoRmvFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RMV,               0, false, CollatorPesVideoRmvFactory);
    // Theora video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_THEORA,            0, false, CodecMMEVideoTheoraFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_THEORA,            0, false, FrameParserVideoTheoraFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_THEORA,            0, false, CollatorPesVideoTheoraFactory);
    // Vp3 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP3,               0, false, CodecMMEVideoTheoraFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP3,               0, false, FrameParserVideoTheoraFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP3,               0, false, CollatorPesVideoTheoraFactory);
    // Dvp video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DVP,               0, false, CodecUncompressedVideoFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DVP,               0, false, FrameParserVideoUncompressedFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_DVP,               0, false, CollatorVideoUncompressedFactory);
    // Avs video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_AVS,               0, false, CodecMMEVideoAvsFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_AVS,               0, false, FrameParserVideoAvsFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_AVS,               0, false, CollatorPesVideoAvsFactory);
    // Mjpeg video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MJPEG,             0, false, CodecMMEVideoMjpegFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MJPEG,             0, false, FrameParserVideoMjpegFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_MJPEG,             0, false, CollatorPesVideoMjpegFactory);
    // Cap video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_CAP,               0, false, CodecUncompressedVideoFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_CAP,               0, false, FrameParserVideoUncompressedFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_CAP,               0, false, CollatorVideoUncompressedFactory);
    // Raw video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RAW,               0, false, CodecUncompressedVideoFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RAW,               0, false, FrameParserVideoUncompressedFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_RAW,               0, false, CollatorVideoUncompressedFactory);
    // Uncompressed video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,      0, false, CodecUncompressedVideoFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,      0, false, FrameParserVideoUncompressedFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_UNCOMPRESSED,      0, false, CollatorVideoUncompressedFactory);
    // Vp8 video
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP8,               0, false, CodecMMEVideoVp8Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP8,               0, false, FrameParserVideoVp8Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeVideo, STM_SE_STREAM_ENCODING_VIDEO_VP8,               0, false, CollatorPesVideoVp8Factory);
    // generic audio components
    HavanaPlayer->RegisterFactory(ComponentOutputTimer, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_NONE,              0, false, OutputTimerAudioFactory);
    HavanaPlayer->RegisterFactory(ComponentDecodeBufferManager, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_NONE,      0, false, DecodeBufferManagerFactory);
    HavanaPlayer->RegisterFactory(ComponentManifestationCoordinator, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_NONE, 0, false, ManifestationCoordinatorFactory);
    HavanaPlayer->RegisterFactory(ComponentManifestor,  StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_NONE,              0, false, ManifestorAudioFactory);
    // AAC audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AAC,               0, false, CodecMMEAudioAacFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AAC,               0, false, FrameParserAudioAacFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AAC,               0, false, CollatorPesAudioAacFactory);
    // AC3 audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AC3,               0, false, CodecMMEAudioEAc3Factory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AC3,               0, false, FrameParserAudioEAc3Factory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_AC3,               0, false, CollatorPesAudioEAc3Factory);
    // MPEG Layer I audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG1,             0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG1,             0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG1,             0, false, CollatorPesAudioMpegFactory);
    // MPEG Layer II audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG2,             0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG2,             0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MPEG2,             0, false, CollatorPesAudioMpegFactory);
    // MPEG Layer III audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MP3,               0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MP3,               0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MP3,               0, false, CollatorPesAudioMpegFactory);
    // DTS audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS,               0, false, CodecMMEAudioDtshdFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS,               0, false, FrameParserAudioDtshdFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS,               0, false, CollatorPesAudioDtshdFactory);
    // LPCM audio (DVD-Video)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCM,              0, false, CodecMMEAudioLpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCM,              0, false, FrameParserAudioLpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCM,              0, false, CollatorPesAudioLpcmFactory);
    // LPCM audio (DVD-Audio)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMA,             0, false, CodecMMEAudioLpcmAFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMA,             0, false, FrameParserAudioLpcmAFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMA,             0, false, CollatorPesAudioLpcmAFactory);
    // ADPCM audio (?)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM,          0, false, CodecMMEAudioMsAdpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM,          0, false, FrameParserAudioMsAdpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM,          0, false, CollatorPesAudioMsAdpcmFactory);
    // ADPCM audio (?)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM,         0, false, CodecMMEAudioImaAdpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM,         0, false, FrameParserAudioImaAdpcmFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM,         0, false, CollatorPesAudioImaAdpcmFactory);
    // LPCM audio (HD-DVD)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMH,             0, false, CodecMMEAudioLpcmHFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMH,             0, false, FrameParserAudioLpcmHFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMH,             0, false, CollatorPesAudioLpcmHFactory);
    // LPCM audio (BluRay, also known as HDMV LPCM audio)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMB,             0, false, CodecMMEAudioLpcmBFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMB,             0, false, FrameParserAudioLpcmBFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_LPCMB,             0, false, CollatorPesAudioLpcmBFactory);
    // WMA audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_WMA,               0, false, CodecMMEAudioWmaFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_WMA,               0, false, FrameParserAudioWmaFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_WMA,               0, false, CollatorPesAudioWmaFactory);
    // AVR Support :: SPDIF-IN
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,           0, false, CodecMMEAudioSpdifinFactory);  // SPDIFIN
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,           0, false, FrameParserAudioSpdifinFactory); // Reuse BD Lpcm
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,           0, false, CollatorPesAudioLpcmSFactory); // Set Spdifin type
    // MLP audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MLP,               0, false, CodecMMEAudioMlpFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MLP,               0, false, FrameParserAudioMlpFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_MLP,               0, false, CollatorPesAudioMlpFactory);
    // DTS-LBR audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR,           0, false, CodecMMEAudioDtshdLbrFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR,           0, false, FrameParserAudioDtshdFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR,           0, false, CollatorPesAudioDtshdFactory);
    // Rma audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_RMA,               0, false, CodecMMEAudioRmaFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_RMA,               0, false, FrameParserAudioRmaFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_RMA,               0, false, CollatorPesAudioRmaFactory);
    // Vorbis audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_VORBIS,            0, false, CodecMMEAudioVorbisFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_VORBIS,            0, false, FrameParserAudioVorbisFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_VORBIS,            0, false, CollatorPesAudioVorbisFactory);
    // PCM audio (raw)
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_PCM,               0, false, CodecMMEAudioPcmFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_PCM,               0, false, FrameParserAudioPcmFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_PCM,               0, false, CollatorPesAudioPcmFactory);
    // DRA audio
    HavanaPlayer->RegisterFactory(ComponentCodec,       StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DRA,               0, false, CodecMMEAudioDraFactory);
    HavanaPlayer->RegisterFactory(ComponentFrameParser, StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DRA,               0, false, FrameParserAudioDraFactory);
    HavanaPlayer->RegisterFactory(ComponentCollator,    StreamTypeAudio, STM_SE_STREAM_ENCODING_AUDIO_DRA,               0, false, CollatorPesAudioDraFactory);
    return Status;
}
//}}}
