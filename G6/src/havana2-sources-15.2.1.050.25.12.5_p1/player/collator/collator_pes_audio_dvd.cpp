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
/// \class Collator_PesAudioDvd_c
///
/// Implement helper routines to assist in the handling of DVD-style PES private data.
///
/// This helper routines are used to help the sub-class manage predictions regarding
/// the location of the first access unit. When the predictions are correct we assume
/// DVD-style PES private data areas exist when the prediction is incorrect we assume
/// broadcast-style PES packets.
///
/// This is a rather complex approach since we could just validate the PES private data
/// area and if it fails assume a broadcast-style stream. The approach taken here is
/// motivated largely by the assuption that the broadcasters are 'out to get us' (so we
/// want to be very careful before entering DVD mode). This is rather paranoid.
///

#include "collator_pes_audio_dvd.h"

static const int MaxWildcardsPermitted = 2;
static const int MaxMispredictionsBeforeAutomaticWildcard = 3;

////////////////////////////////////////////////////////////////////////////
///
///
Collator_PesAudioDvd_c::Collator_PesAudioDvd_c(unsigned int frameheaderlength)
    : Collator_PesAudio_c(frameheaderlength)
    , SyncWordPrediction(0)
    , RemainingWildcardsPermitted(0)
    , RemainingMispredictionsBeforeAutomaticWildcard(0)
{
    MakeDvdSyncWordPrediction(INVALID_PREDICTION);
    ResetDvdSyncWordHeuristics();
    // reprocessing it typically unsafe for DVD-style streams (unless the sub-class carefully
    // toggles this variable during PES private header handling)
    ReprocessAccumulatedDataDuringErrorRecovery = false;
}

////////////////////////////////////////////////////////////////////////////
///
/// Update the prediction after consuming data.
///
/// \param Adjustment Number of bytes to adjust the prediciton by, this is
///                   expected to be negative.
///
void Collator_PesAudioDvd_c::AdjustDvdSyncWordPredictionAfterConsumingData(int Adjustment)
{
    if (Adjustment > 0)
        SE_ERROR("Probably implementation error - positive adjustment requested (%d)\n",
                 Adjustment);

    if (-Adjustment > SyncWordPrediction)
    {
        SyncWordPrediction = INVALID_PREDICTION;
    }

    if (SyncWordPrediction != WILDCARD_PREDICTION && SyncWordPrediction != INVALID_PREDICTION)
    {
        SE_DEBUG(group_collator_audio, "Adjusting prediction from %d to %d\n",
                 SyncWordPrediction, SyncWordPrediction + Adjustment);
        SyncWordPrediction += Adjustment;
    }
    else
        SE_DEBUG(group_collator_audio, "Prediciton is %s - no adjustment made\n",
                 (SyncWordPrediction == WILDCARD_PREDICTION ? "wildcarded" : "invalid"));
}

////////////////////////////////////////////////////////////////////////////
///
/// Record a prediction about the location of the next sync word.
///
void Collator_PesAudioDvd_c::MakeDvdSyncWordPrediction(int Prediction)
{
    // Every time we make a bad prediction then we decrement the mis-prediction counter.
    // When it reaches zero we force a wildcard prediction and enter DVD mode. This is a hack
    // to allow us to play DVD-style streams with broken first access unit pointers where the
    // pointers don't, in fact, point to the an access unit at all. Such streams have been
    // observed in the wild - see https://bugzilla.stlinux.com/show_bug.cgi?id=5051
    if (0 == RemainingMispredictionsBeforeAutomaticWildcard)
    {
        SE_INFO(group_collator_audio, "Deploying bad first access unit pointer workaround\n");
        Prediction = WILDCARD_PREDICTION;
        PassPesPrivateDataToElementaryStreamHandler = false;
        RemainingMispredictionsBeforeAutomaticWildcard = MaxMispredictionsBeforeAutomaticWildcard;
    }

    SE_DEBUG(group_collator_audio, "Making prediction of %d\n", Prediction);
    SyncWordPrediction = Prediction;
}

////////////////////////////////////////////////////////////////////////////
///
/// Reset the heuristic counters.
///
/// This is called when the sub-class is happily locked to the stream.
///
void Collator_PesAudioDvd_c::ResetDvdSyncWordHeuristics()
{
    RemainingWildcardsPermitted = MaxWildcardsPermitted;
    RemainingMispredictionsBeforeAutomaticWildcard = MaxMispredictionsBeforeAutomaticWildcard;
}

////////////////////////////////////////////////////////////////////////////
///
/// Check a previous prediction regarding the location of the sync word.
///
/// This method does not report the result of the prediction. Instead it
/// updates Collator_PesAudio_c::PassPesPrivateDataToElementaryStreamHandler
/// accordingly.
///
void Collator_PesAudioDvd_c::VerifyDvdSyncWordPrediction(int Offset)
{
    if (SyncWordPrediction != WILDCARD_PREDICTION)
    {
        // If SyncWordPrediction != Offset then we are *not* a DVD-style PES stream
        // and need to ensure the DVD PES private data *is* processed as elementary
        // stream.
        PassPesPrivateDataToElementaryStreamHandler = (SyncWordPrediction != Offset);

        // If we badly predicted the location of the sync word then record this. This
        // counter is used as part of a heuristic workaround in other parts of the code.
        if ((SyncWordPrediction != Offset) &&
            (SyncWordPrediction != INVALID_PREDICTION) &&
            (RemainingMispredictionsBeforeAutomaticWildcard > 0))
        {
            RemainingMispredictionsBeforeAutomaticWildcard--;
        }

        // Having consumed the prediction enter a wildcard mode. This causes
        // PassPesPrivateDataToElementaryStreamHandler to hold the same
        // value until we process a PES private data area whilst SeekingFrameHeader.
        // This is very important for DVD streams which can have sync words located
        // such that we never process a PES private data area in this mode. This this
        // case we would never regain lock on the stream.
        MakeDvdSyncWordPrediction(WILDCARD_PREDICTION);
    }
    else
    {
        // Apply a wildcard (this is not a 'match all'; it means 'do not change state').
        // However we can only apply a wildcard a limited number of times. After this
        // point we assume the current state (PassPesPrivateDataToElementaryStreamHandler) is
        // impeeding regain of sync and force a change of state.
        if (--RemainingWildcardsPermitted < 0)
        {
            SE_INFO(group_collator_audio, "Having trouble re-locking, %s DVD wildcard mode\n",
                    (PassPesPrivateDataToElementaryStreamHandler ? "entering" : "leaving"));
            PassPesPrivateDataToElementaryStreamHandler = !PassPesPrivateDataToElementaryStreamHandler;
            // having switched modes we can reset our counter.
            RemainingWildcardsPermitted = MaxWildcardsPermitted;
        }
    }
}
