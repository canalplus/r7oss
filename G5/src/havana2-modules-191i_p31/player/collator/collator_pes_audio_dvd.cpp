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

Source file name : collator_pes_audio_dvd.cpp
Author :           Daniel

Implementation of the pes collator class for player 2.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-09   Created.                                        Daniel

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

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_audio_dvd.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

static const int MaxWildcardsPermitted = 2;
static const int MaxMispredictionsBeforeAutomaticWildcard = 3;

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//


////////////////////////////////////////////////////////////////////////////
///
/// Exists solely to ensure Collator_PesAudioDvd::Reset() is called.
///
Collator_PesAudioDvd_c::Collator_PesAudioDvd_c()
{
    (void) Reset();
}

////////////////////////////////////////////////////////////////////////////
///
/// Iniitialize members.
///
CollatorStatus_t   Collator_PesAudioDvd_c::Reset(			void )
{
    CollatorStatus_t Status = Collator_PesAudio_c::Reset();

    MakeDvdSyncWordPrediction( INVALID_PREDICTION );
    ResetDvdSyncWordHeuristics();

    // reprocessing it typically unsafe for DVD-style streams (unless the sub-class carefully
    // toggles this variable during PES private header handling)
    ReprocessAccumulatedDataDuringErrorRecovery = false;

    return Status;
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
    if( Adjustment > 0)
	COLLATOR_ERROR( "Probably implementation error - positive adjustment requested (%d)\n",
		        Adjustment);

    if( -Adjustment > SyncWordPrediction )
	SyncWordPrediction = INVALID_PREDICTION;

    if( SyncWordPrediction != WILDCARD_PREDICTION && SyncWordPrediction != INVALID_PREDICTION )
    {
	COLLATOR_DEBUG( "Adjusting prediction from %d to %d\n",
			SyncWordPrediction, SyncWordPrediction + Adjustment );
	SyncWordPrediction += Adjustment;
    }
    else
	COLLATOR_DEBUG( "Prediciton is %s - no adjustment made\n",
			(SyncWordPrediction == WILDCARD_PREDICTION ? "wildcarded" : "invalid") );
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
    if( 0 == RemainingMispredictionsBeforeAutomaticWildcard )
    {
	COLLATOR_TRACE( "Deploying bad first access unit pointer workaround.\n" );

	Prediction = WILDCARD_PREDICTION;
	PassPesPrivateDataToElementaryStreamHandler = false;
	RemainingMispredictionsBeforeAutomaticWildcard = MaxMispredictionsBeforeAutomaticWildcard;
    }

    COLLATOR_DEBUG( "Making prediction of %d\n", Prediction );
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
    if( SyncWordPrediction != WILDCARD_PREDICTION )
    {
	// If SyncWordPrediction != Offset then we are *not* a DVD-style PES stream
	// and need to ensure the DVD PES private data *is* processed as elementary
        // stream.
	PassPesPrivateDataToElementaryStreamHandler = (SyncWordPrediction != Offset);

	// If we badly predicted the location of the sync word then record this. This
	// counter is used as part of a heuristic workaround in other parts of the code.
	if( (SyncWordPrediction != Offset) &&
            (SyncWordPrediction != INVALID_PREDICTION) &&
            (RemainingMispredictionsBeforeAutomaticWildcard > 0) )
	    RemainingMispredictionsBeforeAutomaticWildcard--;

	// Having consumed the prediction enter a wildcard mode. This causes
	// PassPesPrivateDataToElementaryStreamHandler to hold the same
	// value until we process a PES private data area whilst SeekingFrameHeader.
	// This is very important for DVD streams which can have sync words located
	// such that we never process a PES private data area in this mode. This this
        // case we would never regain lock on the stream.
	MakeDvdSyncWordPrediction( WILDCARD_PREDICTION );
    }
    else
    {
	// Apply a wildcard (this is not a 'match all'; it means 'do not change state').
	// However we can only apply a wildcard a limited number of times. After this
	// point we assume the current state (PassPesPrivateDataToElementaryStreamHandler) is
	// impeeding regain of sync and force a change of state.
	if( --RemainingWildcardsPermitted < 0 )
	{
	    COLLATOR_TRACE( "Having trouble re-locking, %s DVD wildcard mode.\n",
                            (PassPesPrivateDataToElementaryStreamHandler ? "entering" : "leaving") );
	    PassPesPrivateDataToElementaryStreamHandler = !PassPesPrivateDataToElementaryStreamHandler;

	    // having switched modes we can reset our counter.
	    RemainingWildcardsPermitted = MaxWildcardsPermitted;
	}
    }
}
