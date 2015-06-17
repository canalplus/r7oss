/************************************************************************
COPYRIGHT (C) SGS-THOMSON Microelectronics 2010

Source file name : collator_pes_video_raw.h
Author :           Julian

Definition of the raw collator pes class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
23-Feb-10   Created from existing collator_pes_video_wmv.h  Julian

************************************************************************/

#ifndef H_COLLATOR_PES_VIDEO_RAW
#define H_COLLATOR_PES_VIDEO_RAW

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "collator_pes_video.h"
#include "dvp.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/*
The raw collator is a frame collator as raw streams
do not contain start codes so frame length is passed from the container parser
in a private structure. This is passed into the player using the privade data
structure in the pes header.
*/

class Collator_PesVideoRaw_c : public Collator_PesVideo_c
{
private:
    int                         DataRemaining;
    unsigned int                DataCopied;
    unsigned int*               DecodeBuffer;
    StreamInfo_t                StreamInfo;
protected:

public:

    // Constructor/Destructor methods
    Collator_PesVideoRaw_c(  void );

    // Base class overrides
    CollatorStatus_t    Reset(  void );

    // Collator class functions
    CollatorStatus_t    Input(  PlayerInputDescriptor_t  *Input,
                                unsigned int              DataLength,
                                void                     *Data,
                                bool                      NonBlocking = false,
                                unsigned int             *DataLengthRemaining = NULL );

};

#endif /* H_COLLATOR_PES_VIDEO_RAW */

