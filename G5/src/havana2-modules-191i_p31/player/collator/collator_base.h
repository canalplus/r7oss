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

Source file name : collator_base.h
Author :           Nick

Definition of the base collator base class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
16-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_COLLATOR_BASE
#define H_COLLATOR_BASE

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "player.h"

// /////////////////////////////////////////////////////////////////////////
//
// Frequently used macros
//

#ifndef ENABLE_COLLATOR_DEBUG
#define ENABLE_COLLATOR_DEBUG 0
#endif

#define COLLATOR_TAG      "Collator"
#define COLLATOR_FUNCTION (ENABLE_COLLATOR_DEBUG ? __PRETTY_FUNCTION__ : COLLATOR_TAG)

/* Output debug information (which may be on the critical path) but is usually turned off */
#define COLLATOR_DEBUG(fmt, args...) ((void)(ENABLE_COLLATOR_DEBUG && \
					  (report(severity_note, "%s: " fmt, COLLATOR_FUNCTION, ##args), 0)))

/* Output trace information off the critical path */
#define COLLATOR_TRACE(fmt, args...) (report(severity_note, "%s: " fmt, COLLATOR_FUNCTION, ##args))
/* Output errors, should never be output in 'normal' operation */
#define COLLATOR_ERROR(fmt, args...) (report(severity_error, "%s: " fmt, COLLATOR_FUNCTION, ##args))

#define COLLATOR_ASSERT(x) do if(!(x)) report(severity_error, "%s: Assertion '%s' failed at %s:%d\n", \
					       COLLATOR_FUNCTION, #x, __FILE__, __LINE__); while(0)


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

typedef enum
{
    CollatorConfiguration       = BASE_COLLATOR
} CollatorParameterBlockType_t;

//

typedef struct CollatorConfiguration_s
{
    bool                  GenerateStartCodeList;        // Start code list control
    unsigned int          MaxStartCodes;

    unsigned int          StreamIdentifierMask;         ///< For PES indicates the pes stream identifier
    unsigned int          StreamIdentifierCode;
    
    unsigned int          SubStreamIdentifierMask;      ///< For PES of type extended_stream_id indicates the pes sub stream identifier (set to zero if no filtering is to be done)
    unsigned int          SubStreamIdentifierCode;

    unsigned char         BlockTerminateMask;           ///< Which Start codes indicate frame complete
    unsigned char         BlockTerminateCode;

    unsigned char         StreamTerminateFlushesFrame;	// Use a stream termination code to force a frame flush (helps display of last frame)
    unsigned char         StreamTerminationCode;

    unsigned char         IgnoreCodesRangeStart;        // Start codes to ignore, IE all but first slice header 
    unsigned char         IgnoreCodesRangeEnd;          // start > end ignores none

    bool                  InsertFrameTerminateCode;     // if set causes a terminal code to be inserted into the 
    unsigned char         TerminalCode;                 // buffer, but not recorded in the start code list.

    unsigned int          ExtendedHeaderLength;         ///< Number of bytes of extended PES header (to be skipped)

    bool                  DeferredTerminateFlag;        // Terminate after finding Terminal Code

    bool		  DetermineFrameBoundariesByPresentationToFrameParser;	// Ask frame parser about frame boundaries
} CollatorConfiguration_t;

//

typedef struct CollatorParameterBlock_s
{
    CollatorParameterBlockType_t        ParameterType;

    union
    {
	CollatorConfiguration_t         Configuration;
    };
} CollatorParameterBlock_t;


// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class Collator_Base_c : public Collator_c
{
protected:

    // Data

    OS_Mutex_t                    Lock;

    CollatorConfiguration_t       Configuration;

    BufferPool_t                  CodedFrameBufferPool;
    Buffer_t                      CodedFrameBuffer;
    unsigned int                  MaximumCodedFrameSize;

    unsigned int                  AccumulatedDataSize;
    unsigned char                *BufferBase;
    CodedFrameParameters_t       *CodedFrameParameters;
    StartCodeList_t              *StartCodeList;

    Ring_t                        OutputRing;

    unsigned long long		  LimitHandlingLastPTS;
    bool			  LimitHandlingJumpInEffect;
    unsigned long long		  LimitHandlingJumpAt;

    bool			  Glitch;
    unsigned long long		  LastFramePreGlitchPTS;
    unsigned int		  FrameSinceLastPTS;

    unsigned int		  InputEntryDepth;
    bool			  InputExitPerformFrameFlush;

    // Functions

    CollatorStatus_t   GetNewBuffer(            void );

    CollatorStatus_t   AccumulateData(          unsigned int              Length,
						unsigned char            *Data );

    CollatorStatus_t   AccumulateStartCode(     PackedStartCode_t         Code );

    void               ActOnInputDescriptor(    PlayerInputDescriptor_t  *Input);

    void               DelayForInjectionThrottling( void );

    void               CheckForGlitchPromotion( void );

    CollatorStatus_t   InputEntry(		PlayerInputDescriptor_t	 *Input,
						unsigned int		  DataLength,
						void			 *Data,
						bool			  NonBlocking );
    CollatorStatus_t   InputExit(		void );

public:

    //
    // Constructor/Destructor methods
    //

    Collator_Base_c(    void );
    ~Collator_Base_c(   void );

    //
    // Override for component base class set module parameters function
    //

    CollatorStatus_t   Halt(                    void );

    CollatorStatus_t   Reset(                   void );

    CollatorStatus_t   SetModuleParameters(     unsigned int              ParameterBlockSize,
						void                     *ParameterBlock );

    //
    // Collator class functions
    //

    CollatorStatus_t   RegisterOutputBufferRing(Ring_t                    Ring );

    CollatorStatus_t   FrameFlush(              void );

    CollatorStatus_t   DiscardAccumulatedData(  void );

    CollatorStatus_t   InputJump(               bool                      SurplusDataInjected,
						bool                      ContinuousReverseJump );

    CollatorStatus_t   InputGlitch(		void );

    //
    // Internal function that may well be overridden (or supplemented)
    //

    virtual CollatorStatus_t   InternalFrameFlush(	void );
};

#endif

