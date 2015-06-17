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

Source file name : frame_parser_base.h
Author :           Nick

Definition of the frame parser base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
28-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_FRAME_PARSER_BASE
#define H_FRAME_PARSER_BASE

// /////////////////////////////////////////////////////////////////////
//
//	Include any component headers

#include "bitstream_class.h"
#include "player.h"

// /////////////////////////////////////////////////////////////////////////
//
// Frequently used macros
//

#define MarkerBit( v )		if( Bits.Get(1) != (v) )									\
				{												\
				    report( severity_error, "%s(%d) - Invalid marker bit value.\n", __FUNCTION__, __LINE__ );	\
				    return FrameParserHeaderSyntaxError;							\
				}

#define MarkerBits( n, v )	if( Bits.Get((n)) != (v) )									\
				{												\
				    report( severity_error, "%s(%d) - Invalid marker bits value.\n", __FUNCTION__, __LINE__ );	\
				    return FrameParserHeaderSyntaxError;							\
				}

//

#ifndef ENABLE_FRAME_DEBUG
#define ENABLE_FRAME_DEBUG 0
#endif

#define FRAME_TAG      "Frame parser"
#define FRAME_FUNCTION (ENABLE_FRAME_DEBUG ? __PRETTY_FUNCTION__ : FRAME_TAG )

/* Output debug information (which may be on the critical path) but is usually turned off */
#define FRAME_DEBUG(fmt, args...) ((void)(ENABLE_FRAME_DEBUG && \
                                          (report(severity_note, "%s: " fmt, FRAME_FUNCTION, ##args), 0)))

/* Output trace information off the critical path */
#define FRAME_TRACE(fmt, args...) (report(severity_note, "%s: " fmt, FRAME_FUNCTION, ##args))
/* Output errors, should never be output in 'normal' operation */
#define FRAME_ERROR(fmt, args...) (report(severity_error, "%s: " fmt, FRAME_FUNCTION, ##args))

#define FRAME_ASSERT(x) do if(!(x)) report(severity_error, "%s: Assertion '%s' failed at %s:%d\n", \
                                               FRAME_FUNCTION, #x, __FILE__, __LINE__); while(0)


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef struct FrameParserConfiguration_s
{
    const char			 *FrameParserName;

    unsigned int		  StreamParametersCount;
    BufferDataDescriptor_t	 *StreamParametersDescriptor;

    unsigned int		  FrameParametersCount;
    BufferDataDescriptor_t	 *FrameParametersDescriptor;

    unsigned int		  MaxReferenceFrameCount;		// Used in reverse play resource management

    bool			  SupportSmoothReversePlay;		// Can we support smooth reverse on this stream

    bool			  InitializeStartCodeList;		// Do we need to extract the start code list from input
} FrameParserConfiguration_t;


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_Base_c : public FrameParser_c
{
protected:

    // Data

    OS_Mutex_t			  Lock;

    FrameParserConfiguration_t	  Configuration;

    BufferPool_t		  CodedFrameBufferPool;
    unsigned int		  FrameBufferCount;
    BufferPool_t		  DecodeBufferPool;
    unsigned int		  DecodeBufferCount;
    Ring_t			  OutputRing;

    BufferManager_t		  BufferManager;

    BufferDataDescriptor_t	 *StreamParametersDescriptor;
    BufferType_t		  StreamParametersType;
    BufferPool_t		  StreamParametersPool;
    Buffer_t			  StreamParametersBuffer;

    BufferDataDescriptor_t	 *FrameParametersDescriptor;
    BufferType_t		  FrameParametersType;
    BufferPool_t		  FrameParametersPool;
    Buffer_t			  FrameParametersBuffer;

    Buffer_t			  Buffer;
    unsigned int		  BufferLength;
    unsigned char		 *BufferData;
    CodedFrameParameters_t	 *CodedFrameParameters;
    ParsedFrameParameters_t	 *ParsedFrameParameters;
    StartCodeList_t		 *StartCodeList;

    bool			  FirstDecodeAfterInputJump;
    bool			  SurplusDataInjected;
    bool			  ContinuousReverseJump;

    unsigned int		  NextDecodeFrameIndex;
    unsigned int		  NextDisplayFrameIndex;

    unsigned long long		  NativeTimeBaseLine;
    unsigned long long		  LastNativeTimeUsedInBaseline;

    BitStreamClass_c		  Bits;
    bool			  FrameToDecode;

    Rational_t			  PlaybackSpeed;
    PlayDirection_t		  PlaybackDirection;

    // Functions

    FrameParserStatus_t   RegisterStreamAndFrameDescriptors(	void );

public:

    //
    // Constructor/Destructor methods
    //

    FrameParser_Base_c( 	void );
    ~FrameParser_Base_c( 	void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Halt(			void );

    FrameParserStatus_t   Reset(		void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing(	Ring_t			  Ring );

    FrameParserStatus_t   Input(			Buffer_t		  CodedBuffer );

    FrameParserStatus_t   TranslatePlaybackTimeNativeToNormalized(
							unsigned long long	  NativeTime,
							unsigned long long	 *NormalizedTime );

    FrameParserStatus_t   TranslatePlaybackTimeNormalizedToNative(
							unsigned long long	  NormalizedTime, 
							unsigned long long	 *NativeTime );

    FrameParserStatus_t   ApplyCorrectiveNativeTimeWrap( void );

    // Class function for default non-implemented functionality

    FrameParserStatus_t   ResetCollatedHeaderState(	void )					{ return PlayerNotSupported; }
    unsigned int	  RequiredPresentationLength(	unsigned char		  StartCode )	{ return 0; }
    FrameParserStatus_t   PresentCollatedHeader(	unsigned char		  StartCode,
							unsigned char		 *HeaderBytes,
							FrameParserHeaderFlag_t	 *Flags )	{ return PlayerNotSupported; }
    //
    // Extensions to the class to support my inheritors
    //

protected:

    FrameParserStatus_t   GetNewStreamParameters(	void			**Pointer );
    FrameParserStatus_t   GetNewFrameParameters(	void			**Pointer );

    //
    // Extensions to the class that may be overridden by my inheritors
    //

    virtual FrameParserStatus_t   ProcessBuffer(	void );
    virtual FrameParserStatus_t   QueueFrameForDecode(	void );

    //
    // Extensions to the class to be fulfilled by my inheritors, all defaulted to null functions
    //
    // Note: This functions are doxygenated at the end of frame_parser_base.cpp, please keep the
    // doxygen comments up to date.
    //

    virtual FrameParserStatus_t   ReadHeaders(					void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   ProcessQueuedPostDecodeParameterSettings(	void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   PurgeQueuedPostDecodeParameterSettings(	void ) {return FrameParserNoError;}
    virtual FrameParserStatus_t   GeneratePostDecodeParameterSettings(		void ) {return FrameParserNoError;}
};

#endif

