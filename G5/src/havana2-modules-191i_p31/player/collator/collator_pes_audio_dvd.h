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

Source file name : collator_pes_audio_dvd.h
Author :           Daniel

Add DVD specific features to the audio bass class.


Date        Modification                                    Name
----        ------------                                    --------
08-Jan-09   Created.                                        Daniel

************************************************************************/

#ifndef H_COLLATOR_PES_AUDIO_DVD
#define H_COLLATOR_PES_AUDIO_DVD

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "collator_pes_audio.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_PesAudioDvd_c : public Collator_PesAudio_c
{
private:
    int SyncWordPrediction; ///< Expected location of the sync word (proves we are a DVD-style PES stream)
    int RemainingWildcardsPermitted; ///< Used to avoid getting stuck in *really* extreme cases.

    /// Used to force streams with broken first access unit pointers to work.
    int RemainingMispredictionsBeforeAutomaticWildcard;
    
protected:
    enum
    {
	WILDCARD_PREDICTION = -1,
	INVALID_PREDICTION = -2,
    };

    void AdjustDvdSyncWordPredictionAfterConsumingData(int Adjustment);
    void MakeDvdSyncWordPrediction(int Prediction);
    void ResetDvdSyncWordHeuristics();
    void VerifyDvdSyncWordPrediction(int Offset);

public:
    
    Collator_PesAudioDvd_c();
    CollatorStatus_t Reset();
};

#endif // H_COLLATOR_PES_AUDIO_DVD

