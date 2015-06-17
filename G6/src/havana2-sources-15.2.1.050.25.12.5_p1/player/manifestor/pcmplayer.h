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

#ifndef H_PCMPLAYER_CLASS
#define H_PCMPLAYER_CLASS

#include "player_types.h"
#include "predefined_metadata_types.h"

#undef TRACE_TAG
#define TRACE_TAG   "PcmPlayer_c"

typedef struct PcmPlayerSurfaceParameters_s
{
    AudioSurfaceParameters_t    PeriodParameters;

    unsigned int                ActualSampleRateHz;
    unsigned int                NumPeriods;
    unsigned int                PeriodSize;
} PcmPlayerSurfaceParameters_t;

enum eIecValidity
{
    IEC_VALID_SAMPLES   = 0,
    IEC_INVALID_SAMPLES = 1
};

////////////////////////////////////////////////////////////////////////////
///
/// Abstract PCM player interface definition.
///
/// This is not a pure virtual interface largely due to a small number of
/// inline utilities methods.
///
class PcmPlayer_c
{
public:
    enum OutputEncoding
    {
        OUTPUT_DISABLED, //< Don't configure this output (will be sent via separate coded data formatter)
        OUTPUT_PCM,      //< Raw PCM
        OUTPUT_IEC60958, //< PCM formatted for SPDIF output
        ENCODED_LOWEST,
        OUTPUT_AC3 = ENCODED_LOWEST,//< AC3 with IEC61937 formatting for SPDIF output
        OUTPUT_DTS,      //< DTS with IEC61937 formatting for SPDIF output
        OUTPUT_FATPIPE,  //< FatPipe encoding
        ENCODED_HIGHEST,
        BYPASS_LOWEST,   //
        BYPASS_AC3 = BYPASS_LOWEST, //< Bypassed AC3 with IEC61937 formatting for SPDIF output
        BYPASS_AAC,       //< Bypass MP2-AAC with IEC61937 formating for SPDIF output
        BYPASS_HEAAC_960, //< Bypass MP4-HEAAC with IEC61937 formating for HDMI output (HDMI Layout 0)
        BYPASS_HEAAC_1024,//< Bypass MP4-HEAAC with IEC61937 formating for HDMI output (HDMI Layout 0)
        BYPASS_HEAAC_1920,//< Bypass MP4-HEAAC with IEC61937 formating for HDMI output (HDMI Layout 0)
        BYPASS_HEAAC_2048,//< Bypass MP4-HEAAC with IEC61937 formating for HDMI output (HDMI Layout 0)
        BYPASS_SPDIFIN_COMPRESSED, //< Bypass already formatted compressed data comming from SPDIFin
        BYPASS_SPDIFIN_PCM, //< Bypass already PCM data comming from SPDIFin
        BYPASS_DTS_512,  //< Bypassed DTS/512 with IEC61937 formatting for SPDIF output
        BYPASS_DTS_1024, //< Bypassed DTS/1024 with IEC61937 formatting for SPDIF output
        BYPASS_DTS_2048, //< Bypassed DTS/2048 with IEC61937 formatting for SPDIF output
        BYPASS_DTS_CDDA, //< Bypassed DTS with IEC60958 formatting (coded frames are too large for preable)
        BYPASS_DTSHD_LBR,//< Bypassed DTS-HD LBR with IEC61937 formatting for hdmi output (HDMI Layout 0)
        BYPASS_DTSHD_HR, //< Bypassed DTS-HD HR or DTS with IEC61937 formatting for hdmi output (HDMI Layout 0)
        BYPASS_DTSHD_DTS_4096,//< Bypassed DTS with IEC61937 formatting for hdmi output (HDMI Layout 0)
        BYPASS_DTSHD_DTS_8192,//< Bypassed DTS with IEC61937 formatting for hdmi output (HDMI Layout 0)
        BYPASS_DDPLUS,   //< Bypassed DDPlus with IEC61937 formatting for hdmi output (HDMI Layout 0)
        BYPASS_HBRA_LOWEST,
        BYPASS_DTSHD_MA = BYPASS_HBRA_LOWEST, //< Bypassed DTS-HD MA with IEC61937 formatting for hdmi output (HDMI HBR_audio)
        BYPASS_TRUEHD,   //< Bypassed Dolby TrueHD with IEC61937 formatting for hdmi output (HDMI HBR_audio)
        BYPASS_HBRA_HIGHEST = BYPASS_TRUEHD,
        BYPASS_HIGHEST,
        BYPASS_MUTE
    };

    PlayerStatus_t  InitializationStatus;

    PcmPlayer_c(void)
        : InitializationStatus(PlayerNoError), SurfaceParameters()
    {}
    virtual ~PcmPlayer_c(void) {}

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// restart a PCM player when an underrun has been reported while Mapping or Commiting buffer
    ///
    virtual PlayerStatus_t DeployUnderrunRecovery() = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Get direct access to the PCM player's hardware buffer.
    ///
    virtual PlayerStatus_t MapSamples(unsigned int SampleCount, bool NonBlock, void **MappedSamplesPP) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Notify the PCM player that the mapped samples have been filled.
    ///
    virtual PlayerStatus_t CommitMappedSamples() = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Determine the system time at which the next commit will be displayed.
    ///
    virtual PlayerStatus_t GetTimeOfNextCommit(unsigned long long *TimeP) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Reconfigure the player's nominal settings.
    ///
    /// This call may alter the parameters structure it is supplied with to
    /// reflect the actual settings the PCM player has adopted. The caller must
    /// validate the structure still meets their requirements after a successful
    /// call.
    ///
    virtual PlayerStatus_t SetParameters(PcmPlayerSurfaceParameters_t *ParamsP) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Specify the ratio by which the output rate should deviate from the nominal value.
    ///
    virtual PlayerStatus_t SetOutputRateAdjustment(int Rate, int *ActualRateP) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Set a few SPDIF ChannelStatus bits to specify that the stream is of encoded type.
    /// Also Specify the ratio by which the output rate should deviate from the nominal value.
    ///
    virtual PlayerStatus_t SetIec60958StatusBits(struct snd_pseudo_mixer_aes_iec958 *Iec958Status) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Set a few SPDIF ChannelStatus bits to specify that the stream is of encoded type.
    ///
    virtual PlayerStatus_t SetIec61937StatusBits(struct snd_pseudo_mixer_aes_iec958 *Iec958Status) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Set a few SPDIF Validity bits
    ///
    virtual PlayerStatus_t SetIec60958ValidityBits(enum eIecValidity                 Validity) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Calculate the length in bytes of a number of samples.
    ///
    virtual unsigned int SamplesToBytes(unsigned int SampleCount) = 0;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Calculate the length in samples of a number of bytes.
    ///
    virtual unsigned int BytesToSamples(unsigned int ByteCount) = 0;

    inline static bool IsOutputBypassed(OutputEncoding Encoding)
    {
        return (BYPASS_LOWEST <= Encoding && Encoding < BYPASS_HIGHEST);
    }

    inline static bool IsOutputEncoded(OutputEncoding Encoding)
    {
        return (ENCODED_LOWEST <= Encoding && Encoding < ENCODED_HIGHEST);
    }

    static const char *LookupOutputEncoding(OutputEncoding Encoding)
    {
        switch (Encoding)
        {
#define L(x) case x: return #x
            L(OUTPUT_DISABLED);
            L(OUTPUT_PCM);
            L(OUTPUT_IEC60958);
            L(OUTPUT_AC3);
            L(OUTPUT_DTS);
            L(OUTPUT_FATPIPE);
            L(BYPASS_AC3);
            L(BYPASS_AAC);
            L(BYPASS_HEAAC_960);
            L(BYPASS_HEAAC_1024);
            L(BYPASS_HEAAC_1920);
            L(BYPASS_HEAAC_2048);
            L(BYPASS_DTS_512);
            L(BYPASS_DTS_1024);
            L(BYPASS_DTS_2048);
            L(BYPASS_DTS_CDDA);
            L(BYPASS_DTSHD_LBR);
            L(BYPASS_DTSHD_HR);
            L(BYPASS_DTSHD_DTS_4096);
            L(BYPASS_DTSHD_DTS_8192);
            L(BYPASS_DDPLUS);
            L(BYPASS_DTSHD_MA);
            L(BYPASS_TRUEHD);
            L(BYPASS_SPDIFIN_COMPRESSED);
            L(BYPASS_SPDIFIN_PCM);
#undef L

        default:
            return "UNKNOWN";
        }
    }

protected:
    PcmPlayerSurfaceParameters_t SurfaceParameters;
};

#endif // H_PCMPLAYER_CLASS
