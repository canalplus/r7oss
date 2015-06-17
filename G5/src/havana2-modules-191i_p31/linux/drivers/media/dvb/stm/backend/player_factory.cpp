/// ////////////////////////////////////////////////////////////////////
/// COPYRIGHT (C) STMicroelectronics 2003
///
/// Source file name : player_factory.cpp - default player class factory
/// Author :           Julian
///
///
/// Date        Modification                                    Name
/// ----        ------------                                    --------
/// 09-Feb-07   Created                                         Julian
///
/// ////////////////////////////////////////////////////////////////////

#include "backend_ops.h"
#include "player.h"
#include "collator_pes_video_mpeg2.h"
#include "collator_pes_video_h264.h"
#include "collator_pes_video_vc1.h"
#include "collator_pes_video_wmv.h"
#include "collator_pes_video_divx.h"
#include "collator_packet_dvp.h"
#include "collator_pes_video_h263.h"
#include "collator_pes_video_flv1.h"
#include "collator_pes_video_vp6.h"
#include "collator_pes_video_rmv.h"
#include "collator_pes_video_theora.h"
//#include "collator_pes_video_avs.h"
#include "collator_pes_video_mjpeg.h"
#include "collator_pes_video_raw.h"
#include "collator_pes_audio_aac.h"
#include "collator_pes_audio_mpeg.h"
#include "collator_pes_audio_eac3.h"
#include "collator_pes_audio_dtshd.h"
#include "collator_pes_audio_lpcm.h"
#include "collator_pes_audio_wma.h"
#include "collator_pes_audio_mlp.h"
#include "collator_pes_audio_rma.h"
//#include "collator_pes_audio_avs.h"
#include "collator_pes_audio_vorbis.h"
#include "collator_pes_audio_pcm.h"

#include "frame_parser_video_mpeg2.h"
#include "frame_parser_video_h264.h"
#include "frame_parser_video_vc1.h"
#include "frame_parser_video_wmv.h"
#include "frame_parser_video_divx.h"
#include "frame_parser_video_divx_hd.h"
#include "frame_parser_video_dvp.h"
#include "frame_parser_video_h263.h"
#include "frame_parser_video_flv1.h"
#include "frame_parser_video_vp6.h"
#include "frame_parser_video_rmv.h"
#include "frame_parser_video_theora.h"
//#include "frame_parser_video_avs.h"
#include "frame_parser_video_mjpeg.h"
#include "frame_parser_audio_aac.h"
#include "frame_parser_audio_mpeg.h"
#include "frame_parser_audio_eac3.h"
#include "frame_parser_audio_dtshd.h"
#include "frame_parser_audio_lpcm.h"
#include "frame_parser_audio_wma.h"
#include "frame_parser_audio_mlp.h"
#include "frame_parser_audio_rma.h"
//#include "frame_parser_audio_avs.h"
#include "frame_parser_audio_vorbis.h"
#include "frame_parser_audio_pcm.h"

#include "codec_mme_video_mpeg2.h"
#include "codec_mme_video_h264.h"
#include "codec_mme_video_vc1.h"
#include "codec_mme_video_divx.h"
#include "codec_mme_video_divx_hd.h"
#include "codec_mme_video_h263.h"
#include "codec_mme_video_flv1.h"
#include "codec_mme_video_vp6.h"
#include "codec_mme_video_rmv.h"
#include "codec_mme_video_theora.h"
//#include "codec_mme_video_avs.h"
#include "codec_mme_video_mjpeg.h"
#include "codec_dvp_video.h"
#include "codec_mme_audio_aac.h"
#include "codec_mme_audio_mpeg.h"
#include "codec_mme_audio_eac3.h"
#include "codec_mme_audio_dtshd.h"
#include "codec_mme_audio_lpcm.h"
#include "codec_mme_audio_wma.h"
#include "codec_mme_audio_mlp.h"
#include "codec_mme_audio_rma.h"
//#include "codec_mme_audio_avs.h"
#include "codec_mme_audio_vorbis.h"
#include "codec_mme_audio_spdifin.h"
#include "codec_mme_audio_silence.h"
#include "codec_mme_audio_pcm.h"

#include "output_timer_video.h"
#include "output_timer_audio.h"
#include "manifestor_video_stmfb.h"
#include "manifestor_audio_dummy.h"
#include "manifestor_audio_ksound.h"
#include "mixer_mme.h"
#include "player_module.h"
#include "havana_player.h"
#include "player_factory.h"

//{{{  Collator factories
static void* CollatorPesVideoMpeg2Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoMpeg2_c ();
}

static void* CollatorPesVideoH264Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoH264_c ();
}

static void* CollatorPesVideoVc1Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoVc1_c ();
}

static void* CollatorPesVideoWmvFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoWmv_c ();
}

static void* CollatorPesVideoDivxFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoDivx_c ();
}

static void* CollatorPacketDvpFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PacketDvp_c ();
}

static void* CollatorPesVideoH263Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoH263_c ();
}

static void* CollatorPesVideoFlv1Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoFlv1_c ();
}

static void* CollatorPesVideoVp6Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoVp6_c ();
}

static void* CollatorPesVideoRmvFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoRmv_c ();
}

static void* CollatorPesVideoTheoraFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoTheora_c ();
}

// static void* CollatorPesVideoAvsFactory  (void)
// {
//     PLAYER_DEBUG("\n");
//     return new Collator_PesVideoAvs_c ();
// }

static void* CollatorPesVideoMjpegFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoMjpeg_c ();
}

static void* CollatorPesVideoRawFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesVideoRaw_c ();
}

static void* CollatorPesAudioAacFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioAac_c ();
}

static void* CollatorPesAudioMpegFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioMpeg_c ();
}

static void* CollatorPesAudioEAc3Factory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioEAc3_c ();
}

static void* CollatorPesAudioDtshdFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioDtshd_c ();
}

static void* CollatorPesAudioLpcmFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioLpcm_c ( TypeLpcmDVDVideo );
}

static void* CollatorPesAudioLpcmAFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioLpcm_c ( TypeLpcmDVDAudio );
}

static void* CollatorPesAudioLpcmHFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioLpcm_c ( TypeLpcmDVDHD );
}

static void* CollatorPesAudioLpcmBFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioLpcm_c ( TypeLpcmDVDBD );
}

static void* CollatorPesAudioWmaFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioWma_c ();
}

static void* CollatorPesAudioLpcmSFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioLpcm_c ( TypeLpcmSPDIFIN );
}

static void* CollatorPesAudioMlpFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioMlp_c ();
}
static void* CollatorPesAudioRmaFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioRma_c ();
}
static void* CollatorPesAudioVorbisFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioVorbis_c ();
}
#if 0
static void* CollatorPesAudioAvsFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioAvs_c ();
}
#endif

static void* CollatorPesAudioPcmFactory  (void)
{
    PLAYER_DEBUG("\n");
    return new Collator_PesAudioPcm_c ();
}

//}}}
//{{{  Frame parser factories
static void* FrameParserVideoMpeg2Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoMpeg2_c ();
}

static void* FrameParserVideoH264Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoH264_c ();
}

static void* FrameParserVideoVc1Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoVc1_c ();
}

static void* FrameParserVideoWmvFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoWmv_c ();
}

static void* FrameParserVideoDivxFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoDivx_c ();
}

static void* FrameParserVideoDivxHdFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoDivxHd_c ();
}

static void* FrameParserVideoDvpFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoDvp_c ();
}

static void* FrameParserVideoH263Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoH263_c ();
}

static void* FrameParserVideoFlv1Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoFlv1_c ();
}

static void* FrameParserVideoVp6Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoVp6_c ();
}

static void* FrameParserVideoRmvFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoRmv_c ();
}

static void* FrameParserVideoTheoraFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoTheora_c ();
}

// static void* FrameParserVideoAvsFactory (void)
// {
//     PLAYER_DEBUG("\n");
//     return new FrameParser_VideoAvs_c ();
// }

static void* FrameParserVideoMjpegFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_VideoMjpeg_c ();
}

static void* FrameParserAudioAacFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioAac_c ();
}

static void* FrameParserAudioMpegFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioMpeg_c ();
}

static void* FrameParserAudioEAc3Factory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioEAc3_c ();
}

static void* FrameParserAudioDtshdFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioDtshd_c ();
}

static void* FrameParserAudioLpcmFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioLpcm_c ();
}

static void* FrameParserAudioLpcmAFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioLpcm_c ();
}

static void* FrameParserAudioLpcmHFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioLpcm_c ();
}

static void* FrameParserAudioLpcmBFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioLpcm_c ();
}

static void* FrameParserAudioWmaFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioWma_c ();
}

/// \todo Magic constant for SPDIF latency.
///       SPDIFin has an instrinsic 8192 sample latency. Ideally the codec would configure the frame parser with the
//        latency using SetModuleParameters() but this is good enough for now.
static void* FrameParserAudioSpdifinFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioLpcm_c (8192);
}

static void* FrameParserAudioMlpFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioMlp_c ();
}

static void* FrameParserAudioRmaFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioRma_c ();
}
static void* FrameParserAudioVorbisFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioVorbis_c ();
}
#if 0
static void* FrameParserAudioAvsFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioAvs_c ();
}
#endif

static void* FrameParserAudioPcmFactory (void)
{
    PLAYER_DEBUG("\n");
    return new FrameParser_AudioPcm_c ();
}

//}}}
//{{{  Codec factories
static void* CodecMMEVideoMpeg2Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoMpeg2_c ();
}

static void* CodecMMEVideoH264Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoH264_c ();
}

static void* CodecMMEVideoDivxFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoDivx_c ();
}

static void* CodecMMEVideoDivxHdFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoDivxHd_c ();
}

static void* CodecDvpVideoFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_DvpVideo_c ();
}

static void* CodecMMEVideoVc1Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoVc1_c ();
}

static void* CodecMMEVideoH263Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoH263_c ();
}

static void* CodecMMEVideoFlv1Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoFlv1_c ();
}

static void* CodecMMEVideoVp6Factory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoVp6_c ();
}

static void* CodecMMEVideoRmvFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoRmv_c ();
}

static void* CodecMMEVideoTheoraFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoTheora_c ();
}

// static void* CodecMMEVideoAvsFactory (void)
// {
//     PLAYER_DEBUG("\n");
//     return new Codec_MmeVideoAvs_c ();
// }

static void* CodecMMEVideoMjpegFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeVideoMjpeg_c ();
}

static void* CodecMMEAudioSilenceConditionalFactory (Codec_c *Codec)
{
    if (CodecNoError != Codec->InitializationStatus)
    {
        delete Codec;
        Codec = new Codec_MmeAudioSilence_c ();
    }

    return Codec;
}

static void* CodecMMEAudioAacFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioAac_c ();
}

static void* CodecMMEAudioMpegFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioMpeg_c ();
}

static void* CodecMMEAudioEAc3Factory (void)
{
    PLAYER_DEBUG("\n");
    return CodecMMEAudioSilenceConditionalFactory( new Codec_MmeAudioEAc3_c () );
}

static void* CodecMMEAudioDtshdFactory (void)
{
    PLAYER_DEBUG("\n");
    return CodecMMEAudioSilenceConditionalFactory( new Codec_MmeAudioDtshd_c (false) );
}

static void* CodecMMEAudioLpcmFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioLpcm_c ();
}

static void* CodecMMEAudioLpcmAFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioLpcm_c ();
}

static void* CodecMMEAudioLpcmHFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioLpcm_c ();
}

static void* CodecMMEAudioLpcmBFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioLpcm_c ();
}

static void* CodecMMEAudioWmaFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioWma_c ();
}

static void* CodecMMEAudioMlpFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioMlp_c ();
}
static void* CodecMMEAudioSpdifinFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioSpdifin_c ();
}

static void* CodecMMEAudioDtshdLbrFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioDtshd_c (true);
}
static void* CodecMMEAudioRmaFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioRma_c ();
}
static void* CodecMMEAudioVorbisFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioVorbis_c ();
}
#if 0
static void* CodecMMEAudioAvsFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioAvs_c ();
}
#endif
static void* CodecMMEAudioPcmFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Codec_MmeAudioPcm_c ();
}

//}}}
//{{{  Manifestor factories
static void* ManifestorVideoStmfbFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Manifestor_VideoStmfb_c ();
}

static void* ManifestorAudioFactory (void)
{
    PLAYER_DEBUG("\n");
    return new Manifestor_AudioKsound_c();
}
//}}}
//{{{  Output timer factories
static void* OutputTimerVideoFactory (void)
{
    PLAYER_DEBUG("\n");
    return new OutputTimer_Video_c ();
}

static void* OutputTimerAudioFactory (void)
{
    PLAYER_DEBUG("\n");
    return new OutputTimer_Audio_c ();
}
//}}}
//{{{  'External' component factories
static void* Mixer0AudioFactory (void)
{
    static Mixer_Mme_c *Mixer = NULL;

    PLAYER_DEBUG("\n");

    if (!Mixer)
    {
        // TODO: Probably need to make the mixer a first class component...
        PLAYER_TRACE("Performing JIT mixer0 (main room) instanciation.\n");
        Mixer = new Mixer_Mme_c();
    }

    return Mixer;
}
//}}}
//{{{  'External' component factories
static void* Mixer1AudioFactory (void)
{
    static Mixer_Mme_c *Mixer = NULL;

    PLAYER_DEBUG("\n");

    if (!Mixer)
    {
        // TODO: Probably need to make the mixer a first class component...
        PLAYER_TRACE("Performing JIT mixer1 (second room) instanciation.\n");
        Mixer = new Mixer_Mme_c();
    }

    return Mixer;
}
//}}}

static void* DefaultFactory (void)
{
    PLAYER_DEBUG("No factory available\n");
    return NULL;
}

//{{{  RegisterBuiltInFactories
HavanaStatus_t RegisterBuiltInFactories (class  HavanaPlayer_c* HavanaPlayer)
{
    HavanaStatus_t      Status = HavanaNoError;

    PLAYER_DEBUG("\n");

    // generic video components
    HavanaPlayer->RegisterFactory (BACKEND_VIDEO_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentOutputTimer, 0, false, OutputTimerVideoFactory);
    HavanaPlayer->RegisterFactory (BACKEND_VIDEO_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentManifestor,  0, false, ManifestorVideoStmfbFactory);

    // MPEG1 video
    HavanaPlayer->RegisterFactory (BACKEND_MPEG1_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory (BACKEND_MPEG1_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_MPEG1_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoMpeg2Factory);

    // MPEG2 video
    HavanaPlayer->RegisterFactory (BACKEND_MPEG2_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory (BACKEND_MPEG2_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoMpeg2Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_MPEG2_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoMpeg2Factory);

    // H264 video
    HavanaPlayer->RegisterFactory (BACKEND_H264_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoH264Factory);
    HavanaPlayer->RegisterFactory (BACKEND_H264_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoH264Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_H264_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoH264Factory);

    // VC1 video
    HavanaPlayer->RegisterFactory (BACKEND_VC1_ID,   FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoVc1Factory);
    HavanaPlayer->RegisterFactory (BACKEND_VC1_ID,   FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoVc1Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_VC1_ID,   StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoVc1Factory);

    // WMV video
    HavanaPlayer->RegisterFactory (BACKEND_WMV_ID,   FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoVc1Factory);
    HavanaPlayer->RegisterFactory (BACKEND_WMV_ID,   FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoWmvFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_WMV_ID,   StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoWmvFactory);

    // DivX video       
    HavanaPlayer->RegisterFactory (BACKEND_MPEG4P2_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoDivxFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MPEG4P2_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoDivxFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,    BACKEND_MPEG4P2_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoDivxFactory);

    // DivX HD video       
    HavanaPlayer->RegisterFactory (BACKEND_DIVXHD_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory (BACKEND_DIVXHD_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoDivxHdFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,    BACKEND_DIVXHD_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoDivxFactory); 

    // H263 video
    HavanaPlayer->RegisterFactory (BACKEND_H263_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoH263Factory);
    HavanaPlayer->RegisterFactory (BACKEND_H263_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoH263Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_H263_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoH263Factory);

    // Flv1 video
    HavanaPlayer->RegisterFactory (BACKEND_FLV1_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoFlv1Factory);
    HavanaPlayer->RegisterFactory (BACKEND_FLV1_ID, FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoFlv1Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_FLV1_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoFlv1Factory);

    // Vp6 video
    HavanaPlayer->RegisterFactory (BACKEND_VP6_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoVp6Factory);
    HavanaPlayer->RegisterFactory (BACKEND_VP6_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoVp6Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_VP6_ID,   StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoVp6Factory);

    // Rmv video
    HavanaPlayer->RegisterFactory (BACKEND_RMV_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoRmvFactory);
    HavanaPlayer->RegisterFactory (BACKEND_RMV_ID,  FACTORY_ANY_ID,   StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoRmvFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_RMV_ID,   StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoRmvFactory);

    // Theora video
    HavanaPlayer->RegisterFactory (BACKEND_THEORA_ID, FACTORY_ANY_ID,    StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoTheoraFactory);
    HavanaPlayer->RegisterFactory (BACKEND_THEORA_ID, FACTORY_ANY_ID,    StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoTheoraFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,    BACKEND_THEORA_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoTheoraFactory);

    // Vp3 video
    HavanaPlayer->RegisterFactory (BACKEND_VP3_ID, FACTORY_ANY_ID,    StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoTheoraFactory);
    HavanaPlayer->RegisterFactory (BACKEND_VP3_ID, FACTORY_ANY_ID,    StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoTheoraFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID, BACKEND_VP3_ID, StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoTheoraFactory);

    // Dvp video
    HavanaPlayer->RegisterFactory (BACKEND_DVP_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentCodec,       0, false, CodecDvpVideoFactory);
    HavanaPlayer->RegisterFactory (BACKEND_DVP_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoDvpFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_DVP_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPacketDvpFactory);

//     // Avs video
//     HavanaPlayer->RegisterFactory (BACKEND_AVS_ID,  FACTORY_ANY_ID,    StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoAvsFactory);
//     HavanaPlayer->RegisterFactory (BACKEND_AVS_ID,  FACTORY_ANY_ID,    StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoAvsFactory);
//     HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_AVS_ID,    StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoAvsFactory);

    // Mjpeg video
    HavanaPlayer->RegisterFactory (BACKEND_MJPEG_ID,  FACTORY_ANY_ID,    StreamTypeVideo, ComponentCodec,       0, false, CodecMMEVideoMjpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MJPEG_ID,  FACTORY_ANY_ID,    StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoMjpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_MJPEG_ID,    StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoMjpegFactory);

    // Cap video
    HavanaPlayer->RegisterFactory (BACKEND_CAP_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentCodec,       0, false, CodecDvpVideoFactory);
    HavanaPlayer->RegisterFactory (BACKEND_CAP_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoDvpFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_CAP_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPacketDvpFactory);

    // Raw video
    HavanaPlayer->RegisterFactory (BACKEND_RAW_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentCodec,       0, false, CodecDvpVideoFactory);
    HavanaPlayer->RegisterFactory (BACKEND_RAW_ID,  FACTORY_ANY_ID,  StreamTypeVideo, ComponentFrameParser, 0, false, FrameParserVideoDvpFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,  BACKEND_RAW_ID,  StreamTypeVideo, ComponentCollator,    0, false, CollatorPesVideoRawFactory);

    // generic audio components
    HavanaPlayer->RegisterFactory (BACKEND_AUDIO_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentOutputTimer, 0, false, OutputTimerAudioFactory);
    HavanaPlayer->RegisterFactory (BACKEND_AUDIO_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentManifestor,  0, false, ManifestorAudioFactory);
    HavanaPlayer->RegisterFactory (BACKEND_AUDIO_ID, BACKEND_MIXER0_ID,StreamTypeAudio, ComponentExternal,    0, false, Mixer0AudioFactory);  
    HavanaPlayer->RegisterFactory (BACKEND_AUDIO_ID, BACKEND_MIXER1_ID,StreamTypeAudio, ComponentExternal,    0, false, Mixer1AudioFactory);  

    // AAC audio
    HavanaPlayer->RegisterFactory (BACKEND_AAC_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioAacFactory);
    HavanaPlayer->RegisterFactory (BACKEND_AAC_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioAacFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_AAC_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioAacFactory);

    // AC3 audio
    HavanaPlayer->RegisterFactory (BACKEND_AC3_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioEAc3Factory);
    HavanaPlayer->RegisterFactory (BACKEND_AC3_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioEAc3Factory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_AC3_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioEAc3Factory);

    // MPEG Layer I audio
    HavanaPlayer->RegisterFactory (BACKEND_MPEG1_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MPEG1_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_MPEG1_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioMpegFactory);

    // MPEG Layer II audio
    HavanaPlayer->RegisterFactory (BACKEND_MPEG2_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MPEG2_ID, FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_MPEG2_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioMpegFactory);

    // MPEG Layer III audio
    HavanaPlayer->RegisterFactory (BACKEND_MP3_ID, FACTORY_ANY_ID,     StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MP3_ID, FACTORY_ANY_ID,     StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioMpegFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID, BACKEND_MP3_ID,     StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioMpegFactory);

    // DTS audio
    HavanaPlayer->RegisterFactory (BACKEND_DTS_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioDtshdFactory);
    HavanaPlayer->RegisterFactory (BACKEND_DTS_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioDtshdFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_DTS_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioDtshdFactory);

    // LPCM audio (DVD-Video)
    HavanaPlayer->RegisterFactory (BACKEND_LPCM_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioLpcmFactory);
    HavanaPlayer->RegisterFactory (BACKEND_LPCM_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioLpcmFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,    BACKEND_LPCM_ID,  StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioLpcmFactory);

    // LPCM audio (DVD-Audio)
    HavanaPlayer->RegisterFactory (BACKEND_LPCMA_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioLpcmAFactory);
    HavanaPlayer->RegisterFactory (BACKEND_LPCMA_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioLpcmAFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,     BACKEND_LPCMA_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioLpcmAFactory);

    // LPCM audio (HD-DVD)
    HavanaPlayer->RegisterFactory (BACKEND_LPCMH_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioLpcmHFactory);
    HavanaPlayer->RegisterFactory (BACKEND_LPCMH_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioLpcmHFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,     BACKEND_LPCMH_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioLpcmHFactory);

    // LPCM audio (BluRay, also known as HDMV LPCM audio)
    HavanaPlayer->RegisterFactory (BACKEND_LPCMB_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioLpcmBFactory);
    HavanaPlayer->RegisterFactory (BACKEND_LPCMB_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioLpcmBFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,     BACKEND_LPCMB_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioLpcmBFactory);

    // WMA audio
    HavanaPlayer->RegisterFactory (BACKEND_WMA_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioWmaFactory);
    HavanaPlayer->RegisterFactory (BACKEND_WMA_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioWmaFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_WMA_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioWmaFactory);

    // AVR Support :: SPDIF-IN
    HavanaPlayer->RegisterFactory (BACKEND_SPDIFIN_ID, FACTORY_ANY_ID    , StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioSpdifinFactory); // SPDIFIN 
    HavanaPlayer->RegisterFactory (BACKEND_SPDIFIN_ID, FACTORY_ANY_ID    , StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioSpdifinFactory);// Reuse BD Lpcm
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID    , BACKEND_SPDIFIN_ID, StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioLpcmSFactory);// Set Spdifin type

    // MLP audio
    HavanaPlayer->RegisterFactory (BACKEND_MLP_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioMlpFactory);
    HavanaPlayer->RegisterFactory (BACKEND_MLP_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioMlpFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_MLP_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioMlpFactory);

    // DTS-LBR audio
    HavanaPlayer->RegisterFactory (BACKEND_DTS_LBR_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioDtshdLbrFactory);
    HavanaPlayer->RegisterFactory (BACKEND_DTS_LBR_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioDtshdFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_DTS_LBR_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioDtshdFactory);

    // Rma audio
    HavanaPlayer->RegisterFactory (BACKEND_RMA_ID,  FACTORY_ANY_ID,    StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioRmaFactory);
    HavanaPlayer->RegisterFactory (BACKEND_RMA_ID,  FACTORY_ANY_ID,    StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioRmaFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_RMA_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioRmaFactory);

    // Vorbis audio
    HavanaPlayer->RegisterFactory (BACKEND_VORBIS_ID,  FACTORY_ANY_ID,    StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioVorbisFactory);
    HavanaPlayer->RegisterFactory (BACKEND_VORBIS_ID,  FACTORY_ANY_ID,    StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioVorbisFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_VORBIS_ID,   StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioVorbisFactory);

#if 0
    // Avs audio
    HavanaPlayer->RegisterFactory (BACKEND_AVS_ID,   FACTORY_ANY_ID,    StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioAvsFactory);
    HavanaPlayer->RegisterFactory (BACKEND_AVS_ID,   FACTORY_ANY_ID,    StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioAvsFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,   BACKEND_AVS_ID,    StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioAvsFactory);
#endif

    // PCM audio (raw)
    HavanaPlayer->RegisterFactory (BACKEND_PCM_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentCodec,       0, false, CodecMMEAudioPcmFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PCM_ID,   FACTORY_ANY_ID,   StreamTypeAudio, ComponentFrameParser, 0, false, FrameParserAudioPcmFactory);
    HavanaPlayer->RegisterFactory (BACKEND_PES_ID,    BACKEND_PCM_ID,  StreamTypeAudio, ComponentCollator,    0, false, CollatorPesAudioPcmFactory);


    return Status;
}
//}}}
