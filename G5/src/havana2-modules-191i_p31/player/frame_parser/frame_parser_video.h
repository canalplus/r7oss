/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : frame_parser_video.h
Author :           Nick

Definition of the frame parser video base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
29-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO
#define H_FRAME_PARSER_VIDEO

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "frame_parser_base.h"

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#define ANTI_EMULATION_BUFFER_SIZE		1024

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

/// Framework to unify the approach to video frame parsing.
class FrameParser_Video_c : public FrameParser_Base_c
{
protected:

    // Data

    ParsedVideoParameters_t		 *ParsedVideoParameters;

    int					  NextDisplayFieldIndex;		// Can be negative in reverse play
    int					  NextDecodeFieldIndex;
    bool				  CollapseHolesInDisplayIndices;

    bool				  NewStreamParametersSeenButNotQueued;

    Stack_t				  ReverseDecodeUnsatisfiedReferenceStack;
    Stack_t				  ReverseDecodeSingleFrameStack;
    Stack_t				  ReverseDecodeStack;

    bool				  CodedFramePlaybackTimeValid;
    unsigned long long			  CodedFramePlaybackTime;
    bool				  CodedFrameDecodeTimeValid;
    unsigned long long			  CodedFrameDecodeTime;

    unsigned int			  LastRecordedPlaybackTimeDisplayFieldIndex;
    unsigned long long			  LastRecordedNormalizedPlaybackTime;
    unsigned int			  LastRecordedDecodeTimeFieldIndex;
    unsigned long long			  LastRecordedNormalizedDecodeTime;

    ReferenceFrameList_t                  ReferenceFrameList;
    Ring_t                                ReverseQueuedPostDecodeSettingsRing;	
    bool                                  FirstDecodeOfFrame;
    PictureStructure_t			  AccumulatedPictureStructure;

    Buffer_t				  DeferredCodedFrameBuffer;
    ParsedFrameParameters_t              *DeferredParsedFrameParameters;
    ParsedVideoParameters_t              *DeferredParsedVideoParameters;
    Buffer_t				  DeferredCodedFrameBufferSecondField;
    ParsedFrameParameters_t              *DeferredParsedFrameParametersSecondField;
    ParsedVideoParameters_t              *DeferredParsedVideoParametersSecondField;
	
    Rational_t				  LastFieldRate;

    unsigned int			  NumberOfUtilizedFrameParameters;		// Record of utilized resources for reverse play
    unsigned int			  NumberOfUtilizedStreamParameters;
    unsigned int			  NumberOfUtilizedDecodeBuffers;
    bool				  RevPlayDiscardingState;
    unsigned int			  RevPlayAccumulatedFrameCount;
    unsigned int			  RevPlayDiscardedFrameCount;
    unsigned int			  RevPlaySmoothReverseFailureCount;

    unsigned int                          AntiEmulationContent;
    unsigned char                         AntiEmulationBuffer[ANTI_EMULATION_BUFFER_SIZE];
    unsigned char                        *AntiEmulationSource;

    Rational_t				  LastSeenContentFrameRate;
    unsigned int			  LastSeenContentWidth;
    unsigned int			  LastSeenContentHeight;

    bool				  ValidPTSDeducedFrameRate;
    bool				  StandardPTSDeducedFrameRate;
    Rational_t				  PTSDeducedFrameRate;
    Rational_t				  LastStandardPTSDeducedFrameRate;

    // Functions

    void                  LoadAntiEmulationBuffer(      	unsigned char    *Pointer );
    void                  CheckAntiEmulationBuffer(     	unsigned int      Size );
    void		  DeduceFrameRateFromPresentationTime( 	long long int	  MicroSecondsPerFrame );
    FrameParserStatus_t   TestAntiEmulationBuffer(      	void );

public:

    //
    // Constructor/Destructor methods
    //

    FrameParser_Video_c( 	void );
    ~FrameParser_Video_c( 	void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Halt(			void );
    FrameParserStatus_t   Reset(		void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t		Ring );
    FrameParserStatus_t   Input(			Buffer_t	CodedBuffer );

    //
    // Extensions to the class overriding the base implementations
    // NOTE in order to keep the names reasonably short, in the following
    // functions specifically for forward playback will be prefixed with 
    // ForPlay and functions specific to reverse playback will be prefixed with 
    // RevPlay.
    //

    virtual FrameParserStatus_t   ProcessBuffer(			void );

    virtual FrameParserStatus_t   ForPlayProcessFrame(			void );
    virtual FrameParserStatus_t   ForPlayQueueFrameForDecode(		void );

    virtual FrameParserStatus_t   RevPlayProcessFrame(			void );
    virtual FrameParserStatus_t   RevPlayQueueFrameForDecode(		void );
    virtual FrameParserStatus_t   RevPlayProcessDecodeStacks(		void );
    virtual FrameParserStatus_t   RevPlayPurgeDecodeStacks(		void );
    virtual FrameParserStatus_t   RevPlayClearResourceUtilization( 	void );
    virtual FrameParserStatus_t   RevPlayCheckResourceUtilization( 	void );
    virtual FrameParserStatus_t   RevPlayPurgeUnsatisfiedReferenceStack(void );

    virtual FrameParserStatus_t   InitializePostDecodeParameterSettings(void );
    virtual void		  CalculateFrameIndexAndPts(		ParsedFrameParameters_t		*ParsedFrame,
									ParsedVideoParameters_t		*ParsedVideo );
    virtual void		  CalculateDts(				ParsedFrameParameters_t		*ParsedFrame,
									ParsedVideoParameters_t		*ParsedVideo );


    //
    // Extensions to the class to be fulfilled by my inheritors, 
    // these are required to support the process buffer override
    //

    virtual FrameParserStatus_t   ReadHeaders(						void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   PrepareReferenceFrameList(				void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ResetReferenceFrameList(				void );

    virtual FrameParserStatus_t   ForPlayUpdateReferenceFrameList(	 		void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ForPlayProcessQueuedPostDecodeParameterSettings(	void );
    virtual FrameParserStatus_t   ForPlayGeneratePostDecodeParameterSettings(		void );
    virtual FrameParserStatus_t   ForPlayPurgeQueuedPostDecodeParameterSettings(	void );

    virtual FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(		void );
    virtual FrameParserStatus_t   RevPlayPurgeQueuedPostDecodeParameterSettings(	void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   RevPlayAppendToReferenceFrameList(			void ); 
    virtual FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(			void );
    virtual FrameParserStatus_t   RevPlayJunkReferenceFrameList(			void ); 
    virtual FrameParserStatus_t   RevPlayNextSequenceFrameProcess(			void ) {return FrameParserNoError;}
};

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Video_c::ResetReferenceFrameList(                  void )
///     \brief Reset the reference frame list after stream discontinuity.
///     
///     Called by FrameParser_Base_c::ProcessBuffer whenever a stream discontinuity occurs.
///     This implies that all deferred post decode parameter settings will be deferred.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///                 

// /////////////////////////////////////////////////////////////////////////
///     \fn FrameParserStatus_t   FrameParser_Video_c::PrepareReferenceFrameList(                        void )
///     \brief Prepare the reference frame list.
///
///     \return Frame parser status code, FrameParserNoError indicates success.
///             

#endif

