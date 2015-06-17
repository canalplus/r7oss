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
#ifndef AUDIO_CONVERSION_H
#define AUDIO_CONVERSION_H

#include "player.h"
#include "acc_mme.h"
#include "timestamps.h"
#include <ACC_Transformers/acc_mmedefines.h>
#include <ACC_Transformers/Pcm_PostProcessingTypes.h>
#include <ACC_Transformers/PcmProcessing_DecoderTypes.h>

#undef TRACE_TAG
#define TRACE_TAG   "AudioConversion"

typedef struct StmSeAudioChannelPlacementAnalysis_s
{
    int32_t ActiveChannelCount;         //!< Channels with content to process/present
    int32_t InterleavedStuffingCount;   //!< Channels without content, with at active channel(s) afterwards
    int32_t EndStuffingCount;       //!< Channels without content, without an active channel afterwards
    int32_t NamedChannelCount;      //!< Channels with a named positionning
    int32_t UnnamedChannelCount;    //!< Channels without a named positionning
    int32_t ParsingErrors;              //!< Unrecognised Channels
    int32_t TotalNumberOfChannels;      //!< Total Buffer width
} StmSeAudioChannelPlacementAnalysis_t;

enum eCEA861_Sfrequency
{
    CEA861_0k = 0 , //!< refer to stream frequency
    CEA861_32k,
    CEA861_44k,
    CEA861_48k,
    CEA861_88k,
    CEA861_96k,
    CEA861_176k,
    CEA861_192k,
};

#ifdef __cplusplus
extern "C" {
#endif

// stm_se_audio_channel_id_t to string conversion
const char *StmSeAudioChannelIdGetName(const stm_se_audio_channel_id_t chan);

// eAccAcMode to CEA861 SpeakerMapping code
void StmSeAudioAcModeToHdmi(const enum eAccAcMode AudioMode,
                            unsigned char *speaker_mapping,
                            unsigned char *channel_count);

// eAccAcMode to string conversion
const char *StmSeAudioAcModeGetName(const enum eAccAcMode InputAcMode);

// stm_se_audio_channel_pair to string conversion (nees pair id, as name depends on pair position
const char *StmSeAudioChannelPairGetName(const enum stm_se_audio_channel_pair Pair, const int PairId);

// stm_se_audio_channel_placement_t to eAccAcMode conversion
int StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(enum eAccAcMode *AudioMode,
                                                      bool *AudioModeIsPhysical,
                                                      stm_se_audio_channel_placement_t *SortedPlacement,
                                                      StmSeAudioChannelPlacementAnalysis_t *Analysis,
                                                      const stm_se_audio_channel_placement_t *Placement);
// eAccAcMode to stm_se_audio_channel_placement_t conversion
int StmSeAudioGetChannelPlacementAndAnalysisFromAcmode(stm_se_audio_channel_placement_t *Placement,
                                                       StmSeAudioChannelPlacementAnalysis_t *Analysis,
                                                       const enum eAccAcMode  AudioMode,
                                                       const int32_t BufferWidth);

// eAccAcMode to stm_se_audio_channel_assignment_t conversion
int StmSeAudioGetChannelAssignmentFromAcMode(stm_se_audio_channel_assignment_t *Assignment,
                                             const enum eAccAcMode AcMode);

// stm_se_audio_channel_assignment_t to eAccAcMode conversion
int StmSeAudioGetAcModeFromChannelAssignment(enum eAccAcMode *AudioMode,
                                             const stm_se_audio_channel_assignment_t *Assignment);

/**
 * stm_se_audio_pcm_format_t to enum eAccLpcmWs conversion
 * @param LpcmWs the matched LpcmWs (undefined if
 *               unmatched)
 * @param LpcmFormat
 *               the format we are trying to match
 *
 * @return 0 if no error and matched
 */
int StmSeAudioGetLpcmWsFromLPcmFormat(enum eAccLpcmWs   *LpcmWsCode, const stm_se_audio_pcm_format_t LpcmFormat);

/**
 * enum eAccLpcmWs to stm_se_audio_pcm_format_t  conversion
 * @param eAccLpcmWs The AccLpcmWs code we are trying to match
 *
 * @param LpcmFormat the matched pcm_format
 *         (undefined, if unmatched)
 *
 * @return 0 if no error and matched
 */
int StmSeAudioGetLPcmFormatFromLpcmWs(stm_se_audio_pcm_format_t *LpcmFormat, const enum eAccLpcmWs LpcmWsCode);

/**
 * stm_se_audio_pcm_format_t to enum eAccWordSizeCode conversion
 * @param LpcmFormat
 *               the lpcm format we want to convert
 *
 * @param WordsizeCode the matched eAccWordSizeCode (undefined if
 *         unmatched)
 *
 * @return int 0 if no error *and* matched
 */
int StmSeAudioGetWordsizeCodeFromLPcmFormat(enum eAccWordSizeCode *WordSizeCode, const stm_se_audio_pcm_format_t LpcmFormat);

/**
 * enum eAccWordSizeCode to stm_se_audio_pcm_format_t  conversion
 * @param eAccWordSizeCode The WordsizeCode code we are trying to match
 *
 * @param LpcmFormat the matched pcm_format
 *         (undefined, if unmatched)
 *
 * @return int 0 if no error *and* matched
 */
int StmSeAudioGetLPcmFormatFromWordsizeCode(stm_se_audio_pcm_format_t *LpcmFormat, const enum eAccWordSizeCode WordSizeCode);

/**
 * gets the number of bytes used to store stm_se_audio_pcm_format_t
 * @return int the number of bytes, 0 if format is invalid (@warning)
 *
 *
 * @param LpcmFormat
 *
 */
int StmSeAudioGetNrBytesFromLpcmFormat(const stm_se_audio_pcm_format_t LpcmFormat);

/**
 * @brief Get a TimeStamp_c from ACC PTS structures
 *
 *
 * @note the pointer indirection for const input PTS is required in order to
 *       able to convert a regular uint32_t to a uMME_BufferFlags, while
 *       keeping the caller aware of the required casting.
 *
 * @param PTSflag pointer to the Acc PTSFlag storage
 * @param PTS pointer to the Acc PTS storage
 *
 * @return TimeStamp_c converted time, value can be UNSPECIFIED_TIME if
 *         PTS is not ACC_PTS_PRESENT, INVALID_TIME in case of error.
 */
TimeStamp_c StmSeAudioTimeStampFromAccPts(const uMME_BufferFlags *const PTSflag,
                                          const uint64_t *const PTS);

/**
 * Converts Timestamp_C to ACC PTS
 *
 *
 * @param PTSflag pointer to the Acc PTSFlag storage
 * @param PTS pointer to the Acc PTS storage
 * @param TimeStamp the Timestamp to conver to Acc format
 */
void StmSeAudioAccPtsFromTimeStamp(uMME_BufferFlags *PTSflag, uint64_t *PTS,
                                   TimeStamp_c TimeStamp);

/**
 * Get the offset between a Transform's Output and Input timestamps
 * and apply the same offset to the SE Input time stamps in the original
 * format.
 *
 * Detect 33bit wrap-around between TransformInputTimeStamp and
 * TransformOutputTimeStamp during the transform by using the most-logical
 * Offset: the smallest.
 *
 * @param Offset  the found Offset in the same units as InputTimeStamp
 * @param IsDelay if true, then "output time stamp" is "input time stamp -
 *            offset", else ""input time stamp = offset"
 * @param InputTimeStamp
 *                The Original SE Input Time stamp
 * @param TransformInputTimeStamp
 *                The transform Input timestamp
 * @param TransformOutputTimeStamp
 *                The transform output timestamp
 *
 * @return The output SE Timestamp with Offset applied in the same format
 *         as Input. UNSPECIFIED_TIME in case any time stamp is invalid.
 */
TimeStamp_c StmSeAudioGetApplyOffsetFrom33BitPtsTimeStamps(TimeStamp_c &Offset,
                                                           bool &IsDelay,
                                                           TimeStamp_c InputTimeStamp,
                                                           TimeStamp_c TransformInputTimeStamp,
                                                           TimeStamp_c TransformOutputTimeStamp);

/**
 * Get the offset between a Transform's Output and Input timestamps
 *
 * Detect 33bit wrap-around between TransformInputTimeStamp and
 * TransformOutputTimeStamp during the transform by using the most-logical
 * Offset: the smallest.
 *
 * @param IsDelay if true, then "output time stamp" is "input time stamp -
 *            offset", else ""input time stamp = offset"
 * @param TransformInputTimeStamp
 *                The transform Input timestamp
 * @param TransformOutputTimeStamp
 *                The transform output timestamp
 *
 * @return Offset  the found Offset in the same units as InputTimeStamp
 */
TimeStamp_c StmSeAudioGetOffsetFrom33BitPtsTimeStamps(bool &IsDelay,
                                                      TimeStamp_c TransformInputTimeStamp,
                                                      TimeStamp_c TransformOutputTimeStamp);

/**
 *
 * Take a continuous sampling frequency and identify the nearest eAccFsCode
 * for that frequency.
 *
 * @param IntegerFrequency
 *            The ISO Sampling Frequency in Hz to convert into eAccFsCode
 *
 * @return the nearast eAccFsCode corresponding to the provided ISO Sampling frequency
 *
 */
enum eAccFsCode StmSeTranslateIntegerSamplingFrequencyToDiscrete(uint32_t IntegerFrequency);

/**
 *
 * get the CEA-861 code for a give ISO SamplingFrequency in Hz
 *
 * @param IntegerFrequency
 *            The ISO Sampling Frequency in Hz to convert into eAccFsCode
 *
 * @return the corresponding CEA861 character code (returns 0 if no code for this sfreq)
 *
 */
void  StmSeTranslateIntegerSamplingFrequencyToHdmi(uint32_t IntegerFrequency, unsigned char *HdmiFrequency);

/**
 *
 * Take a continuous sampling frequency and identify the nearest eAccFsCode
 * for that frequency.
 *
 * @param IsoFrequency
 *            The ISO Sampling Frequency in Hz to convert into eAccFsCode
 *
 * @param DescreteFreqency : the eAccFsCode corresponding to the provided ISO Sampling frequency
 *
 * @return -EINVAL if the given sampling frequency doesn't correspond to a known ISO SamplingFrequency
 */
int StmSeTranslateIsoSamplingFrequencyToDiscrete(uint32_t IntegerFrequency, enum eAccFsCode &DiscreteFrequency);

/**
 *
 * Take a discrete sampling frequency and convert that to an integer frequency.
 * Unexpected values (such as ACC_FS_ID) translate to zero.
 *
 * @param DescreteFrequency
 *            The enumerated Sampling Frequency as reported by the AudioFW
 *
 * @return the corresponding ISO Sampling frequency in Hz (returns -1 if out of bounds)
 *
 */
int32_t StmSeTranslateDiscreteSamplingFrequencyToInteger(enum eAccFsCode DiscreteFrequency);

/**
 *
 * Take a unsigned char representing value in Q8, and translate as a Percentage.
 *
 * @param q8val
 *            The input Q8 value to be converted
 *
 * @return the corresponding value in percentage
 *
 */

static inline uint32_t ConvQ8ToPercent(uint8_t q8val)
{
    return (((uint32_t) q8val * 25 + 31) >> 6);
}

/**
 *
 * Take a PlayerTimeFormat_t convert that to AudioFW enum ePtsTimeFormat.
 *
 *
 * @param NativeTimeFormat
 *            The PlayerTimeFormat_t NativeTimeFormat to be converted
 *
 * @return the corresponding enum ePtsTimeFormat
 *
 */
enum ePtsTimeFormat StmSeConvertPlayerTimeFormatToFwTimeFormat(PlayerTimeFormat_t NativeTimeFormat);

/**
 *
 * Take a enum ePtsTimeFormat convert that to PlayerTimeFormat_t.
 *
 *
 * @param FwTimeFormat
 *            The enum ePtsTimeFormat FwTimeFormat to be converted
 *
 * @return the corresponding PlayerTimeFormat_t NativeTimeFormat
 *
 */
PlayerTimeFormat_t StmSeConvertFwTimeFormatToPlayerTimeFormat(enum ePtsTimeFormat FwTimeFormat);



#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CONVERSION_H */
